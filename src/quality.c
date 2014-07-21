#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <re.h>
#include <rem.h>
#include "baresip.h"
#include "core.h"
/*#include "list_ext.h"*/
#include "quality.h"

#define QD_SHOW_INFO true
#define QD_TEST_MODE false

/*
 * #define COLLECT_VIA_METRIC
 */
#define COLLECT_VIA_RTCP


struct rtcp_stats cur_stats = { {0, 0, 0}, {0, 0, 0} };
struct rtcp_stats final_stats = { {0, 0, 0}, {0, 0, 0} };

static struct rtcp_stats * msg_q = 0;
static int                 msg_q_tx_p = 0;
static int                 msg_q_rx_p = 0;

#define msg_q_rx msg_q
#define msg_q_tx msg_q

static uint32_t pre_time_rx = 0;
static uint32_t pre_time_tx = 0;

uint32_t tick_gap_rx = 0;
uint32_t tick_gap_tx = 0;

static uint32_t cur_tick_tx = 0;
static uint32_t cur_tick_rx = 0;

// packets per second
static uint32_t rx_per_sec;
static uint32_t tx_per_sec;

static uint8_t lost_frac_rx=0;
static uint8_t lost_frac_tx=0;

uint8_t lost_fraction_rx(struct rtcp_stats *rs)
{
    uint32_t  lost_v;

    if (!rs) rs = &cur_stats;
    if (!rs->rx.sent) return lost_frac_rx;

    lost_v = (rs->rx.lost<<11)/rs->rx.sent;
    if (lost_v>255)
        lost_v=255;

    lost_frac_rx = lost_v;
    return (uint8_t) lost_v;
}

uint8_t lost_fraction_tx(struct rtcp_stats *rs)
{
    uint32_t  lost_v;

    if (!rs) rs = &cur_stats;
    if (!rs->tx.sent) return lost_frac_tx;
    lost_v = (rs->tx.lost<<11)/rs->tx.sent;
    if (lost_v>255)
        lost_v=255;

    lost_frac_tx = lost_v;
    return (uint8_t) lost_v;
}

int check_time_rx(void)
{
    time_t t;
    uint32_t time_gap;
    uint32_t cur_time;
    uint32_t time_gap_p;

    if (pre_time_rx == 0) {
        pre_time_rx = time(NULL);
        rx_per_sec=0;
    }
    time(&t);
    cur_time = t*1000;

    if (cur_time==pre_time_rx || rx_per_sec==0)
        rx_per_sec++;
    else {
        time_gap = 1000/rx_per_sec;
        time_gap_p = msg_queue_time/msg_q_l;
        tick_gap_rx = time_gap_p/time_gap;

        if (QD_TEST_MODE) {
            info("\n\nRX:\n");
            info("%u packets in msg queue length\n",msg_q_l);
            info("%u mseconds msg queue time\n",(uint32_t)msg_queue_time);
            info("(%u mseconds good packets interval)\n",time_gap_p);
            info("%u packets per second\n",rx_per_sec);
            info("=> %u packets gap  <=\n\n",tick_gap_rx);
        }
        rx_per_sec=0;
        pre_time_rx=cur_time;
    }

    if (cur_tick_rx>tick_gap_rx) {
        cur_tick_rx = 0;
        return 1;
    }
    cur_tick_rx++;
    return 0;
}

int check_time_tx(void)
{
    time_t t;
    uint32_t time_gap;
    uint32_t cur_time;
    uint32_t time_gap_p;

    if (pre_time_tx == 0) {
        pre_time_tx = time(NULL);
        tx_per_sec=0;
    }
    time(&t);
    cur_time = t*1000;

    if (cur_time==pre_time_tx || tx_per_sec==0)
        tx_per_sec++;
    else {
        time_gap = 1000/tx_per_sec;
        time_gap_p = msg_queue_time/msg_q_l;
        tick_gap_tx = time_gap_p/time_gap;
        tx_per_sec=0;
        pre_time_tx=cur_time;
    }

    if (cur_tick_tx>tick_gap_tx) {
        cur_tick_tx = 0;
        return 1;
    }
    cur_tick_tx++;
    return 0;
}

static void collect_rtp_stats(struct stream *s, struct rtcp_stats *rs)
{
    if (!s || !rs) return;
    rs->tx.sent += s->rtcp_stats.tx.sent;
    rs->tx.lost += s->rtcp_stats.tx.lost;
    rs->rx.sent += s->rtcp_stats.rx.sent;
    rs->rx.lost += s->rtcp_stats.rx.lost;
}

static void collect_cur_rtcp_stats_rx(struct rtcp_stats *rs)
{
    int i,ii,t;

    uint32_t rx_min_sent,rx_max_sent;
    float sumlost;
    int sumlosti;

    float klost = 1.5/msg_q_l;
    int   nlost = 0;

    if (!rs) return;

    rx_min_sent=INT_MAX;
    rx_max_sent=0;

    sumlost=0;
    sumlosti=0;

    if (msg_q) {
        if (msg_q_rx_p>0 && msg_q_rx_p+2<msg_q_l) {
            for (i=msg_q_rx_p+2; i<msg_q_l; i++) {
                if (msg_q_rx[i].rx.sent) {
                    ii = (i)?i-1:msg_q_l-1;
                    if (msg_q_rx[i].rx.sent>rx_max_sent) {
                        rx_max_sent = msg_q_rx[i].rx.sent;
                    }
                    if (msg_q_rx[i].rx.sent<rx_min_sent) {
                        rx_min_sent = msg_q_rx[i].rx.sent;
                    }
                    if (msg_q_rx[ii].rx.sent>0) {
                        t =(msg_q_rx[i].rx.lost-msg_q_rx[ii].rx.lost);
                        sumlosti += t;
                        nlost++;
                        sumlost += (klost*nlost)*t;
                    }
               }
            }
        }

        for (i=0;i<msg_q_rx_p;i++) {
            ii = (i)?i-1:msg_q_l-1;
            if (msg_q_rx[i].rx.sent>rx_max_sent) {
                rx_max_sent = msg_q_rx[i].rx.sent;
            }
            if (msg_q_rx[i].rx.sent<rx_min_sent) {
                rx_min_sent = msg_q_rx[i].rx.sent;
            }
            if (msg_q_rx[ii].rx.sent>0) {
                t =(msg_q_rx[i].rx.lost-msg_q_rx[ii].rx.lost);
                sumlosti += t;
                nlost++;
                sumlost += (klost*nlost)*t;
            }
        }

    }

    if (rx_min_sent!=INT_MAX && rx_max_sent!=0) {
        rs->rx.sent = rx_max_sent-rx_min_sent;
        rs->rx.lost = sumlost;
    }
    else {
        rs->rx.sent = 0;
        rs->rx.lost = 0;
    }

}

static void collect_cur_rtcp_stats_tx(struct rtcp_stats *rs)
{
    int i,ii,t;

    uint32_t tx_min_sent,tx_max_sent;
    float sumlost;
    int sumlosti;

    float klost = 1.5/msg_q_l;
    int   nlost = 0;

    if (!rs) return;

    tx_min_sent=INT_MAX;
    tx_max_sent=0;

    sumlost=0;
    sumlosti=0;

    if (msg_q) {
        if (msg_q_tx_p>0 && msg_q_tx_p+2<msg_q_l) {
            for (i=msg_q_tx_p+2; i<msg_q_l; i++) {
                if (msg_q_tx[i].tx.sent) {
                    ii = (i)?i-1:msg_q_l-1;
                    if (msg_q_tx[i].tx.sent>tx_max_sent) {
                        tx_max_sent = msg_q_tx[i].tx.sent;
                    }
                    if (msg_q_tx[i].tx.sent<tx_min_sent) {
                        tx_min_sent = msg_q_tx[i].tx.sent;
                    }
                    if (msg_q_tx[ii].tx.sent>0) {
                        t =(msg_q_tx[i].tx.lost-msg_q_tx[ii].tx.lost);
                        if (t>0) {
							sumlosti += t;
							nlost++;
							sumlost += (klost*nlost)*t;
                        }
                    }
               }
            }
        }

        for (i=0;i<msg_q_tx_p;i++) {
            ii = (i)?i-1:msg_q_l-1;
            if (msg_q_tx[i].tx.sent>tx_max_sent) {
                tx_max_sent = msg_q_tx[i].tx.sent;
            }
            if (msg_q_tx[i].tx.sent<tx_min_sent) {
                tx_min_sent = msg_q_tx[i].tx.sent;
            }
            if (msg_q_tx[ii].tx.sent>0) {
                t =(msg_q_tx[i].tx.lost-msg_q_tx[ii].tx.lost);
                if (t>0) {
					sumlosti += t;
					nlost++;
					sumlost += (klost*nlost)*t;
                }
            }
        }

    }

    if (tx_min_sent!=INT_MAX) {
        rs->tx.sent = tx_max_sent-tx_min_sent;
        rs->tx.lost = sumlost;
    }
    else {
        rs->rx.sent = 0;
        rs->rx.lost = 0;
    }
}

static void collect_cur_rtcp_stats(struct rtcp_stats *rs)
{
    collect_cur_rtcp_stats_tx(rs);
    collect_cur_rtcp_stats_rx(rs);
}

#ifdef COLLECT_VIA_METRIC

#define SOURCE_STAT_RX_PACKETS metric_rx.n_packets
#define SOURCE_STAT_TX_PACKETS metric_tx.n_packets

#define SOURCE_STAT_RX_ERRORS metric_rx.n_err
#define SOURCE_STAT_TX_ERRORS metric_tx.n_err

#elif defined(COLLECT_VIA_RTCP)

#define SOURCE_STAT_RX_PACKETS rtcp_stats.rx.sent
#define SOURCE_STAT_TX_PACKETS rtcp_stats.tx.sent

#define SOURCE_STAT_RX_ERRORS rtcp_stats.rx.lost
#define SOURCE_STAT_TX_ERRORS rtcp_stats.tx.sent

#endif

/*
  collect rx statstics all o streams into queue
  */
void rtcp_store_stat_rx(void)
{
    struct le *le, *le_u, *le_c;
    struct list *ua_l, *call_l, *l;
    struct ua *tua;
    struct call *tcall;
    struct stream *s;

    if (!msg_q_rx) {
        msg_q_rx = mem_zalloc(sizeof(*msg_q_rx)*msg_q_l,NULL);
        if (!msg_q_rx) {
            info("RTCP message queue memory allocation error.\n");
        }
    }

    if (msg_q_rx) {
        msg_q_rx[msg_q_rx_p].tx.sent = 0;
        msg_q_rx[msg_q_rx_p].tx.lost = 0;

        ua_l = uag_list();
        if (ua_l) {
			for (le_u = ua_l->head; le_u; le_u=le_u->next) {
				tua = (struct ua *) le_u->data;
				call_l = ua_calls(tua);
				if (call_l) {
					for (le_c = call_l->head; le_c; le_c=le_c->next) {
						tcall = (struct call *) le_c->data;
						l = call_streaml(tcall);
						for (le = l->head; le; le=le->next) {
							s = (struct stream *) le->data;
							msg_q_rx[msg_q_rx_p].rx.sent += s->SOURCE_STAT_RX_PACKETS;
							msg_q_rx[msg_q_rx_p].rx.lost += s->SOURCE_STAT_RX_ERRORS;
						}
					}
				}
			}
		}
        msg_q_rx_p++;
        if (msg_q_rx_p==msg_q_l) msg_q_rx_p=0;
    }

    collect_cur_rtcp_stats_rx(&cur_stats);
}

void rtcp_store_stat_tx(void)
{
    struct le *le, *le_u, *le_c;
    struct list *ua_l, *call_l, *l;
    struct ua *tua;
    struct call *tcall;
    struct stream *s;

    if (!msg_q_tx) {
        msg_q_tx = mem_zalloc(sizeof(*msg_q_tx)*msg_q_l,NULL);
        if (!msg_q_tx) {
            info("RTCP message queue memory allocation error.\n");
        }
    }

    if (msg_q_tx) {
        msg_q_tx[msg_q_tx_p].tx.sent = 0;
        msg_q_tx[msg_q_tx_p].tx.lost = 0;

        ua_l = uag_list();
        if (ua_l) {
			for (le_u = ua_l->head; le_u; le_u=le_u->next) {
				tua = (struct ua *) le_u->data;
				call_l = ua_calls(tua);
				if (call_l) {
					for (le_c = call_l->head; le_c; le_c=le_c->next) {
						tcall = (struct call *) le_c->data;
						l = call_streaml(tcall);
						for (le = l->head; le; le=le->next) {
							s = (struct stream *) le->data;
				            msg_q_tx[msg_q_tx_p].tx.sent += s->SOURCE_STAT_TX_PACKETS;
				            msg_q_tx[msg_q_tx_p].tx.lost += s->SOURCE_STAT_TX_ERRORS;
						}
					}
				}
			}
		}
        msg_q_tx_p++;
        if (msg_q_tx_p==msg_q_l) msg_q_tx_p=0;
    }

    collect_cur_rtcp_stats_tx(&cur_stats);
}

void rtcp_info(struct rtcp_stats *rs)
{
    collect_cur_rtcp_stats(rs);
    if (QD_SHOW_INFO) {
        info("Current errors buffer:\n\t\tTransmit:\tReceive:\n"
        "packets:\t%7u\t\t%7u\n"
        "errors:\t\t%7d\t\t%7d\n"
        "lost fraction:\t%7u\t\t%7u\t(/256)\n",
        rs->tx.sent, rs->rx.sent,
        rs->tx.lost, rs->rx.lost,
        lost_fraction_tx(NULL),
        lost_fraction_rx(NULL));
    }
}

void stream_rtcp_info(struct stream *s)
{
    if (QD_SHOW_INFO) {
        info("\n%-9s:\n\t\tTransmit:\tReceive:\n"
        "packets:\t%7u\t\t%7u\n"
        "errors:\t\t%7d\t\t%7d\n"
        "jitter:\t\t%7.1f\t\t%7.1f\t(ms)\n"
        "avg. bitrate:\t%7.1f\t\t%7.1f\t(kbit/s)\n"
        "lost fraction:\t%7u\t\t%7u\t(/256)\n\n",
        sdp_media_name(s->sdp),
        s->metric_tx.n_packets, s->metric_rx.n_packets,
        s->metric_tx.n_err, s->metric_rx.n_err,
        1.0*s->rtcp_stats.tx.jit/1000,
        1.0*s->rtcp_stats.rx.jit/1000,
        1.0*metric_avg_bitrate(&s->metric_tx)/1000,
        1.0*metric_avg_bitrate(&s->metric_rx)/1000,
        lost_fraction_tx(&s->rtcp_stats),
        lost_fraction_rx(&s->rtcp_stats));
    }
}

static void show_stat_queue(void)
{
    int i,ii,t,sumlosti;

    if (QD_TEST_MODE) {

        info("\nRecieve:\n");

        sumlosti = 0;

        if (msg_q_rx_p>0 && msg_q_rx_p+2<msg_q_l) {
            for (i=msg_q_rx_p+2; i<msg_q_l; i++) {
                if (msg_q_rx[i].rx.sent) {
                    ii = (i)?i-1:msg_q_l-1;
                    if (msg_q_rx[ii].rx.sent>0) {
                        t =(msg_q_rx[i].rx.lost-msg_q_rx[ii].rx.lost);
                        sumlosti +=t;
                    }
                    info("%u\t%u\t%u\t%u\t%u\n",i,msg_q_rx[i].rx.sent,msg_q_rx[i].rx.lost,t,sumlosti);
               }
            }
        }

        for (i=0;i<msg_q_rx_p;i++) {
            ii = (i)?i-1:msg_q_l-1;
            if (msg_q_rx[ii].rx.sent>0) {
                t =(msg_q_rx[i].rx.lost-msg_q_rx[ii].rx.lost);
                sumlosti +=t;
            }
            info("%u\t%u\t%u\t%u\t%u\n",i,msg_q_rx[i].rx.sent,msg_q_rx[i].rx.lost,t,sumlosti);
        }

        info("\nTransmit:\n");

        if (msg_q_tx_p>0 && msg_q_tx_p+2<msg_q_l) {
            for (i=msg_q_tx_p+2; i<msg_q_l; i++) {
                if (msg_q_tx[i].tx.sent)
                    info("%u\t%u\t%u\n",i,msg_q_tx[i].tx.sent,msg_q_tx[i].tx.lost);
            }
        }

        for (i=0;i<msg_q_tx_p;i++) {
            info("%u\t%u\t%u\n",i,msg_q_tx[i].tx.sent,msg_q_tx[i].tx.lost);
        }
    }
}

void quality_display_on_stream_destructor(struct stream *s)
{
    if (s->cfg.rtp_stats) {
        collect_rtp_stats(s,&final_stats);
        stream_rtcp_info(s);
        if (list_count(call_streaml(s->call)) == 1) {
            rtcp_info(&cur_stats);
            /* show_stat_queue(); */
            if (msg_q) {
                mem_deref(msg_q);
                msg_q = 0;
            }
            msg_q_tx_p = 0;
            msg_q_rx_p = 0;
        }
    }
}

