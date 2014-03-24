/**
 * @file gst.c  Gstreamer video pipeline
 *
 * Copyright (C) 2014 Fadeev Alexander
 */
#include <re.h>
#include <stdlib.h>
#include <string.h>
#define __USE_POSIX199309
#include <sys/time.h>
#include <pthread.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include "gst_video.h"

#define DEBUG_MODULE "gst"
#define DEBUG_LEVEL 5
#include <re_dbg.h>

// For usleep
#define __USE_BSD
#include <unistd.h>



/**
 * Defines the Gstreamer state
 */
struct gst_video_state {
	/* Gstreamer */
	GstElement *pipeline, *source, *sink;
	GstBus *bus;
	gulong need_data_handler;
	gulong enough_data_handler;
	gulong new_buffer_handler;

	/* Main loop thread. */
	int run;
	pthread_t tid;

	/* Thread synchronization. */
	pthread_mutex_t mutex;
	pthread_cond_t wait;
	int bwait;

	/* Frame buffer pull callback (send method). */
	void (*pull)(void *dst, int size, void *arg);
	void *arg;
};


static void internal_bus_watch_handler(struct gst_video_state *st)
{
	GstTagList *tag_list;
	gchar *title;
	GError *err;
	gchar *d;

	GstMessage *msg = gst_bus_pop(st->bus);

	if (!msg) {
		// take a nap (300ms)
		usleep(300 * 1000);
		return;
	}

	switch (GST_MESSAGE_TYPE(msg)) {

	case GST_MESSAGE_EOS:

		/* XXX decrementing repeat count? */

		/* Re-start stream */
		gst_element_set_state(st->pipeline, GST_STATE_NULL);
		gst_element_set_state(st->pipeline, GST_STATE_PLAYING);
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &d);

		DEBUG_WARNING("Error: %d(%m) message=%s\n", err->code,
			      err->code, err->message);
		DEBUG_WARNING("Debug: %s\n", d);

		g_free(d);
		g_error_free(err);

		st->run = FALSE;
		break;

	case GST_MESSAGE_TAG:
		gst_message_parse_tag(msg, &tag_list);

		if (gst_tag_list_get_string(tag_list, GST_TAG_TITLE, &title)) {
			DEBUG_NOTICE("Title: %s\n", title);
			g_free(title);
		}
		break;

	default:
		break;
	}

	gst_message_unref(msg);
}


static void *internal_thread(void *arg)
{
	struct gst_video_state *st = arg;

	/* Now set to playing and iterate. */
	DEBUG_NOTICE("Setting pipeline to PLAYING\n");

	gst_element_set_state(st->pipeline, GST_STATE_PLAYING);

	while (st->run) {
		internal_bus_watch_handler(st);
	}

	DEBUG_NOTICE("Pipeline thread was stopped.\n");

	return NULL;
}


static void internal_appsrc_start_feed(GstElement * pipeline, guint size, struct gst_video_state *st)
{
	(void)pipeline;
	(void)size;

	pthread_mutex_lock(&st->mutex);
	st->bwait = FALSE;
	pthread_cond_signal(&st->wait);
	pthread_mutex_unlock(&st->mutex);
}

static void internal_appsrc_stop_feed(GstElement * pipeline, struct gst_video_state *st)
{
	(void)pipeline;

	pthread_mutex_lock(&st->mutex);
	st->bwait = TRUE;
	pthread_mutex_unlock(&st->mutex);
}

/* The appsink has received a buffer */
static void internal_appsink_new_buffer(GstElement *sink, struct gst_video_state *st)
{
	GstBuffer *buffer;

	/* Retrieve the buffer */
	g_signal_emit_by_name(sink, "pull-buffer", &buffer);

	if (buffer) {
		guint8 *data = GST_BUFFER_DATA(buffer);
		guint size = GST_BUFFER_SIZE(buffer);

		if (st->pull) {
			st->pull(data, size, st->arg);
		}

		gst_buffer_unref(buffer);
	}
}

/**
 * Set up the Gstreamer pipeline. Appsrc gets raw frames, and appsink takes
 * encoded frames.
 *
 * The pipeline looks like this:
 *
 * <pre>
 *  .--------.   .-----------.   .----------.
 *  | appsrc |   |  x264enc  |   | appsink  |
 *  |   .----|   |----.  .---|   |----.     |
 *  |   |src |-->|sink|  |src|-->|sink|-----+-->handoff
 *  |   '----|   |----'  '---|   |----'     |   handler
 *  '--------'   '-----------'   '----------'
 * </pre>
 */
gst_video_t *gst_video_alloc(int width, int height, int framerate, int bitrate, void (*f)(void *dst, int size, void *arg), void *arg)
{
	GstElement *source, *sink;
	GError* gerror = NULL;
	gchar *version;
	char pipeline[1024];
	int err = 0;

	struct gst_video_state *st = mem_zalloc(sizeof(struct gst_video_state), NULL);

	gst_init(NULL, NULL);

	/* Print gstreamer version. */
	version = gst_version_string();
	DEBUG_NOTICE("init: %s\n", version);
	g_free(version);

#ifndef TARGET_BRANTO_BALL
	snprintf(pipeline, sizeof(pipeline), "appsrc name=source is-live=TRUE block=TRUE do-timestamp=TRUE ! "
	                                     "videoparse width=%d height=%d format=i420 framerate=%d/1 ! "
	                                     "x264enc byte-stream=TRUE rc-lookahead=0 sync-lookahead=0 bitrate=%d ! "
	                                     "appsink name=sink emit-signals=TRUE drop=TRUE", width, height, framerate, bitrate / 1024 /* kbit/s */);

	DEBUG_NOTICE("format: yu12 = yuv420p = i420\n");
#else
/* BeagleBoard-xM with camera*/
/*	snprintf(pipeline, sizeof(pipeline), "v4l2src name=source device=/dev/video2 always-copy=false ! "*/
	snprintf(pipeline, sizeof(pipeline), "appsrc name=source ! "
	                                     "video/x-raw-yuv,width=%d,height=%d,format=(fourcc)UYVY,framerate=%d/1 ! "
	                                     "TIVidenc1 codecName=h264enc engineName=codecServer bitRate=%d ! "
	                                     "appsink name=sink emit-signals=TRUE drop=TRUE", width, height, framerate, bitrate);

	DEBUG_NOTICE("format: uyvy\n");
#endif

	DEBUG_NOTICE("width: %d\n", width);
	DEBUG_NOTICE("height: %d\n", height);
	DEBUG_NOTICE("framerate: %d\n", framerate);
	DEBUG_NOTICE("bitrate: %d\n", bitrate);

	/* Initialize pipeline. */
	st->pipeline = gst_parse_launch(pipeline, &gerror);
	if (gerror) {
		DEBUG_WARNING("%s: %s\n", gerror->message, pipeline);
		err = gerror->code;
		g_error_free(gerror);
		goto out;
	}

	source = gst_bin_get_by_name(GST_BIN(st->pipeline), "source");
	sink = gst_bin_get_by_name(GST_BIN(st->pipeline), "sink");

	if (!source || !sink) {
		DEBUG_WARNING("failed to get source or sink pipeline elements");
		err = ENOMEM;
		goto out;
	}

	st->source = source;
	st->sink = sink;

#if 1
	/* Configure appsource*/
	st->need_data_handler = g_signal_connect(source, "need-data", G_CALLBACK(internal_appsrc_start_feed), st);
	st->enough_data_handler = g_signal_connect(source, "enough-data", G_CALLBACK(internal_appsrc_stop_feed), st);
#endif

	/* Configure appsink. */
	st->new_buffer_handler = g_signal_connect(sink, "new-buffer", G_CALLBACK(internal_appsink_new_buffer), st);

	/********************* Misc **************************/

	/* Bus watch */
	st->bus = gst_pipeline_get_bus(GST_PIPELINE(st->pipeline));

	/********************* Thread **************************/

	/* Synchronization primitives. */
	pthread_mutex_init(&st->mutex, NULL);
	pthread_cond_init(&st->wait, NULL);
	st->bwait = FALSE;

	err = gst_element_set_state(st->pipeline, GST_STATE_PLAYING);
	if (GST_STATE_CHANGE_FAILURE == err) {
		g_warning("set state returned GST_STATE_CHANGE_FAILUER\n");
	}

	/* Launch thread with gstreamer loop. */
	st->run = TRUE;
	err = pthread_create(&st->tid, NULL, internal_thread, st);
	if (err) {
		st->run = FALSE;
		goto out;
	}

	/* Set callback to context. */
	st->pull = f;
	st->arg = arg;

out:
	if (err) {
		gst_video_free((gst_video_t *)st);
		st = NULL;
	}

	return (gst_video_t *)st;
}

void gst_video_free(gst_video_t *ctx)
{
	struct gst_video_state *st = (struct gst_video_state *)ctx;

	if (st) {
		/* Remove asynchronous callbacks to prevent using gst_video_t context ("st") after releasing. */
		g_signal_handler_disconnect(st->source, st->need_data_handler);
		g_signal_handler_disconnect(st->source, st->enough_data_handler);
		g_signal_handler_disconnect(st->sink, st->new_buffer_handler);

		/* Stop thread. */
		if (st->run) {
			st->run = FALSE;
			pthread_join(st->tid, NULL);
		}

		gst_object_unref(GST_OBJECT(st->source));
		gst_object_unref(GST_OBJECT(st->sink));
		gst_object_unref(GST_OBJECT(st->bus));

		gst_element_set_state(st->pipeline, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(st->pipeline));

		mem_deref(st);

		DEBUG_NOTICE("deinitialized\n");
	}
}

int gst_video_push(gst_video_t *ctx, const void *src, int size)
{
	struct gst_video_state *st = (struct gst_video_state *)ctx;

	GstBuffer *buffer;
	int ret = 0;

	if (!st) {
		return EINVAL;
	}

	if (!size) {
		DEBUG_WARNING("eos returned %d at %d\n", ret, __LINE__);
		gst_app_src_end_of_stream((GstAppSrc *)st->source);
		return ret;
	}

	/* Wait "start feed". */
	pthread_mutex_lock(&st->mutex);
	if (st->bwait) {
#define WAIT_TIME_SECONDS 5
		struct timespec ts;
		struct timeval tp;
		gettimeofday(&tp, NULL);
		ts.tv_sec  = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		ts.tv_sec += WAIT_TIME_SECONDS;
		/* Wait. */
		ret = pthread_cond_timedwait(&st->wait, &st->mutex, &ts);
		if (ETIMEDOUT == ret) {
			DEBUG_WARNING("Raw frame is lost because of timeout\n");
			return ret;
		}
	}
	pthread_mutex_unlock(&st->mutex);

	/* Create a new empty buffer */
	buffer = gst_buffer_new();
	GST_BUFFER_MALLOCDATA(buffer) = (guint8 *)src;
	GST_BUFFER_SIZE(buffer) = (guint)size;
	GST_BUFFER_DATA(buffer) = GST_BUFFER_MALLOCDATA(buffer);

	ret = gst_app_src_push_buffer((GstAppSrc *)st->source, buffer);

	if (ret != GST_FLOW_OK) {
		DEBUG_WARNING("push buffer returned %d for %d bytes \n", ret, size);
		return ret;
	}

	return ret;
}
