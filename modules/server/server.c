#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>

#include <pthread.h>

#define PORT    9999
#define MAXMSG  512

#ifdef SO_REUSEPORT
#define BRANTO_SOCKET_OPTION (SO_REUSEPORT | SO_REUSEADDR)
#else
#define BRANTO_SOCKET_OPTION SO_REUSEADDR
#endif

static volatile int exclusion = 0;
static int run = 0;
static pthread_t thread;
static int sock;

static void on_command(const char* command)
{
	if (strstr(command, "photo") == command) {
		struct re_printf pf;
		pf.arg = strstr(command, " ");
		if (pf.arg && strlen(pf.arg)) {
			pf.arg = ((char *)pf.arg) + 1;
		}
		cmd_process(NULL, 'o', &pf);
	} else if (strstr(command, "video-start") == command) {
		struct re_printf pf;
		pf.arg = strstr(command, " ");
		if (pf.arg && strlen(pf.arg)) {
			pf.arg = ((char *)pf.arg) + 1;
		}
		cmd_process(NULL, 'z', &pf);
	} else if (strstr(command, "video-stop") == command) {
		cmd_process(NULL, 'Z', NULL);
	}
}

static int make_socket(uint16_t port)
{
    int sock;
    int option = 1;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);

    /* http://stackoverflow.com/questions/8330808/bind-with-so-reuseaddr-fails */

    if (setsockopt(sock, SOL_SOCKET, BRANTO_SOCKET_OPTION, (char*)&option, sizeof(option)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);

    }

    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
        perror ("bind");
        exit (EXIT_FAILURE);
    }

    printf("Server started.\n");

    return sock;
}


static int read_from_client(int filedes)
{
    char buffer[MAXMSG + 1];
    int nbytes;

    nbytes = read(filedes, buffer, MAXMSG);

    if (nbytes < 0)
    {
        /* Read error. */
        perror("read");
        exit(EXIT_FAILURE);
    }
    else if (nbytes == 0)
        /* End-of-file. */
        return -1;
    else
    {
	int i;

	/* Trim. */
	for (i = nbytes; i >=0; i--) {
		if (buffer[i] == '\r' || buffer[i] == '\n' || buffer[i] == 0) {
			buffer[i] = 0;
		} else {
			break;
		}
	}

	/* Data read. Command. */
        on_command(buffer);

        return -1; //close socket
    }
}

static void *server_thread(void *arg)
{
    fd_set active_fd_set, read_fd_set;
    int i;
    struct sockaddr_in clientname;
    size_t size;

    (void)arg;

    /* Create the socket and set it up to accept connections. */
    sock = make_socket (PORT);
    if (listen (sock, 1) < 0)
    {
        perror ("listen");
        return NULL;
    }

    /* Initialize the set of active sockets. */
    FD_ZERO (&active_fd_set);
    FD_SET (sock, &active_fd_set);

    while (run)
    {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            perror ("select");
            return NULL;
        }

        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET (i, &read_fd_set))
            {
                if (i == sock)
                {
                    /* Connection request on original socket. */
                    int new;
                    size = sizeof (clientname);
                    new = accept (sock, (struct sockaddr *) &clientname, &size);
                    if (new < 0)
                    {
                        perror ("accept");
                        return NULL;
                    }
                    fprintf (stderr,
                        "Server: connect from host %s, port %d.\n",
                         inet_ntoa (clientname.sin_addr),
                         ntohs (clientname.sin_port));
                    FD_SET (new, &active_fd_set);
                }
                else
                {
                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i) < 0)
                    {
                        fprintf (stdout, "Server: closing socket\r\n");
                        close (i);
                        FD_CLR (i, &active_fd_set);
                    }
                }
            }
    }

    return NULL;
}

static int server_init(void)
{
	int ret = 0;

	if (!run) {
		run = 1;

		ret = pthread_create(&thread, NULL, server_thread, NULL);

		if (ret) {
			run = 0;
		} else {
			info("server: port %d\n", PORT);
		}
	}

	return ret;
}


static int server_close(void)
{
	run = 0;

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(server) = {
	"server",
	"application",
	server_init,
	server_close
};
