/**
 * @file jpg_vf.c  Write vidframe to a JPG-file
 *
 * Author: Doug Blewett
 * Review: Alfred E. Heggestad
 */
#define _BSD_SOURCE 1
#include <string.h>
#include <time.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <jpeglib.h>
#include "jpg_vf.h"

static char *jpg_filename(const struct tm *tmx, const char *name,
			  char *buf, unsigned int length);


int jpg_save_vidframe(const struct vidframe *vf, const char *path, const char *preview_path)
{

	struct		jpeg_compress_struct cinfo;
	struct		jpeg_error_mgr jerr;
  	JSAMPROW	row_pointer[1];
	
	unsigned char *imgdata,*src,*dst;
	int row_stride,pixs;
	
	FILE * fp;
	char filename_buf[64];

	struct vidframe *f2 = NULL;
	int err = 0;

	unsigned int width = vf->size.w & ~1;
	unsigned int height = vf->size.h & ~1;
	
	time_t tnow;
	struct tm *tmx;
	
	// 0
	tnow = time(NULL);
	tmx = localtime(&tnow);
	imgdata = vf->data[0];

	if (vf->fmt != VID_FMT_RGB32)
	{
		err = vidframe_alloc(&f2, VID_FMT_RGB32, &vf->size);
		if (err) goto out;
		vidconv(f2, vf, NULL);
		imgdata = f2->data[0];
	}

	fp = fopen(jpg_filename(tmx, path, filename_buf, sizeof(filename_buf)), "wb");
	if (fp == NULL) 
	{
		err = errno;
		goto out;
	}

	// 32bpp -> 24bpp
	pixs = width*height;
	src = imgdata; 
	dst = imgdata; 
	while (pixs--)
	{
		*dst++=*src++; //R
		*dst++=*src++; //G
		*dst++=*src++; //B
		src++; //A
	}

	// create jpg structures
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);
	
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3; // 24 bpp
	// I wonder if this will make double conversion.
	cinfo.in_color_space = JCS_EXT_BGR;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 85 , TRUE); // quality 85%

	// compress
	jpeg_start_compress(&cinfo, TRUE);

	row_stride = width * cinfo.input_components;		
	//buffer = (JSAMPARRAY) imgdata;

	while (cinfo.next_scanline < cinfo.image_height) 
	{
		row_pointer[0] = & imgdata[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);

	
	/* Finish writing. */
out:
	jpeg_destroy_compress(&cinfo);
	mem_deref(f2);
	if (fp) fclose(fp);
	return 0;
}


static char *jpg_filename(const struct tm *tmx, const char *name,
			  char *buf, unsigned int length)
{
	/*
	 * -2013-03-03-15-22-56.png - 24 chars
	 */
	if (strlen(name) + 24 >= length) {
		buf[0] = '\0';
		return buf;
	}

	sprintf(buf, (tmx->tm_mon < 9 ? "%s-%d-0%d" : "%s-%d-%d"), name,
		1900 + tmx->tm_year, tmx->tm_mon + 1);

	sprintf(buf + strlen(buf), (tmx->tm_mday < 10 ? "-0%d" : "-%d"),
		tmx->tm_mday);

	sprintf(buf + strlen(buf), (tmx->tm_hour < 10 ? "-0%d" : "-%d"),
		tmx->tm_hour);

	sprintf(buf + strlen(buf), (tmx->tm_min < 10 ? "-0%d" : "-%d"),
		tmx->tm_min);

	sprintf(buf + strlen(buf), (tmx->tm_sec < 10 ? "-0%d.jpg" : "-%d.jpg"),
		tmx->tm_sec);

	return buf;
}
