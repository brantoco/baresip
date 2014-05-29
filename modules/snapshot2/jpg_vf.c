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


static int jpg_save_vidframe_onefile(const struct vidframe *vf, const char *file_path, int downscale)
{

	struct		jpeg_compress_struct cinfo;
	struct		jpeg_error_mgr jerr;
  	JSAMPROW	row_pointer[1];
	
	unsigned char *imgdata,*src,*dst;
	int row_stride,pixs;
	
	FILE * fp;

	struct vidframe *f2 = NULL;
	int err = 0;

	unsigned int width = vf->size.w & ~1;
	unsigned int height = vf->size.h & ~1;
	
	imgdata = vf->data[0];

	if (vf->fmt != VID_FMT_RGB32)
	{
		err = vidframe_alloc(&f2, VID_FMT_RGB32, &vf->size);
		if (err) goto out;

		vidconv(f2, vf, NULL);
		imgdata = f2->data[0];
	}

	fp = fopen(file_path, "wb");
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
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst += 3;
		src += 4;
	}

	// create jpg structures
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);
	
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3; // 24 bpp
	// I wonder if this will make double conversion.
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 85 , TRUE); // quality 85%

	cinfo.scale_num = 1;
	cinfo.scale_denom = downscale;

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
	if (fp) fclose(fp);
	return 0;
}


/** vf_copy must always be our copy of the frame. */
int jpg_save_vidframe(const struct vidframe *vf_copy, const char *file_path, const char *preview_file_path)
{
	if (file_path) {
		debug("Making photo: %s\n", file_path);
		jpg_save_vidframe_onefile(vf_copy, file_path, 1);

		// TODO: Calculate the best downscale denominator based based on desired preview size and frame size.
		// TODO
//		if (preview_file_path) {
//			debug("Making preview: %s\n", preview_file_path);
//			jpg_save_vidframe_onefile(vf_copy, preview_file_path, 2);
//		}

	} else {
		error("No image path given!\n");
		return EINVAL;
	}

	return 0;
}
