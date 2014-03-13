#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/common.h>
#include <sys/time.h>
#include "re.h"
#include "rem.h"
#include "baresip.h"
#include "vidcont.h"


#define STREAM_PIX_FMT PIX_FMT_UYVY422 /* default pix_fmt */


struct vidcont_int_t {
	AVFormatContext *av;
	AVStream *st;
	int64_t start_time;
};


static AVStream *internal_add_video_stream(AVFormatContext *oc, int width, int height, int framerate, int bitrate, enum CodecID codec_id, struct mbuf *sps, struct mbuf *pps)
{
	AVCodecContext *c;
	AVStream *st;
	char nalu_hdr[4] = {0x00, 0x00, 0x00, 0x01};

	int sps_len = sps->end;
	int pps_len = pps->end;
	int nalu_hdr_len = sizeof(nalu_hdr);

	st = avformat_new_stream(oc, NULL);
	if (!st) {
		debug("Could not alloc stream\n");
		return NULL;
	}

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_VIDEO;

	c->extradata_size = nalu_hdr_len + sps_len + nalu_hdr_len + pps_len;
	c->extradata = av_mallocz(c->extradata_size);

	memcpy(c->extradata, nalu_hdr, nalu_hdr_len);
	mbuf_set_pos(sps, 0);
	mbuf_read_mem(sps, c->extradata + nalu_hdr_len, sps_len);
	memcpy(c->extradata + nalu_hdr_len + sps_len, nalu_hdr, nalu_hdr_len);
	mbuf_set_pos(pps, 0);
	mbuf_read_mem(pps, c->extradata + nalu_hdr_len + sps_len + nalu_hdr_len, pps_len);

	/* put sample parameters */
	c->bit_rate = bitrate;
	/* resolution must be a multiple of two */
	c->width = width;
	c->height = height;
	/* time base: this is the fundamental unit of time (in seconds) in terms
	   of which frame timestamps are represented. for fixed-fps content,
	   timebase should be 1/framerate and timestamp increments should be
	   identically 1. */
	c->time_base.den = framerate;
	c->time_base.num = 1;
	c->pix_fmt = STREAM_PIX_FMT;
	// some formats want stream headers to be separate
	if(oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	return st;
}

vidcont_t *vidcont_alloc(const char *filename, int width, int height, int framerate, int bitrate, enum CodecID codec_id, struct mbuf *sps, struct mbuf *pps)
{
	struct vidcont_int_t *int_ctx = NULL;
	AVFormatContext *av = NULL;
	AVOutputFormat *fmt = NULL;
	AVStream *st = NULL;

	if (!filename || !strlen(filename) || !width || !height || !framerate || !bitrate || !codec_id || !sps || !pps) {
		debug("vidcont_alloc: Invalid argument\n");
		return NULL;
	}

	int_ctx = mem_zalloc(sizeof(struct vidcont_int_t), NULL);

	if (!int_ctx) {
		debug("vidcont_alloc: Memory allocation error\n");
		return NULL;
	}

	av_register_all();

	fmt = av_guess_format(NULL, filename, NULL);
	if (!fmt) {
		debug("Could not deduce output format from file extension: using MPEG.\n");
		fmt = av_guess_format("mp4", NULL, NULL);
	}
	if (!fmt) {
		debug("Could not find suitable output format\n");
		goto alloc_error;
	}

	av = avformat_alloc_context();
	if (!av) {
		debug("Memory error\n");
		goto alloc_error;
	}

	fmt->video_codec = codec_id;
	av->oformat = fmt;
	snprintf(av->filename, sizeof(av->filename), "%s", filename);

	st = internal_add_video_stream(av, width, height, framerate, bitrate, codec_id, sps, pps);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&av->pb, filename, AVIO_FLAG_WRITE) < 0) {
			debug("Could not open '%s'\n", filename);
			goto alloc_error;
		}
	}

	avformat_write_header(av, NULL);

	int_ctx->av = av;
	int_ctx->st = st;

	return (vidcont_t *)int_ctx;

alloc_error:
	if (!av) {
		av_free(av);
	}

	if (!int_ctx) {
		mem_deref(int_ctx);
	}

	return NULL;
}

void vidcont_free(vidcont_t *ctx)
{
	struct vidcont_int_t *int_ctx = (struct vidcont_int_t *)ctx;

	if (!int_ctx) {
		return;
	}

	av_write_trailer(int_ctx->av);

	// Only this line flushes file to disk. avio_flush doesn't work.
	avio_close(int_ctx->av->pb);

	av_free(int_ctx->st->codec->extradata);
	av_free(int_ctx->av);
	mem_deref(int_ctx);
}

int vidcont_video_write(vidcont_t *ctx, void *buf, int size)
{
	AVPacket pkt;
	int64_t now;

	struct vidcont_int_t *int_ctx = (struct vidcont_int_t *)ctx;

	if (!ctx) {
		return EINVAL;
	}

	now = av_gettime();

	if (!int_ctx->start_time) {
		int_ctx->start_time = now;
	}

	av_init_packet(&pkt);

	pkt.stream_index = int_ctx->st->index;
	pkt.flags |= AV_PKT_FLAG_KEY;
	pkt.data = buf;
	pkt.size = size;
	pkt.pts = av_rescale_q((now - int_ctx->start_time), (AVRational){1, 1000000}, int_ctx->st->time_base);

	/* write the compressed frame in the media file */
	return av_interleaved_write_frame(int_ctx->av, &pkt);
}
