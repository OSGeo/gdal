#include "grib2.h"
#ifndef USE_PNG
int dec_png(unsigned char *pngbuf,g2int *width,g2int *height,char *cout){return 0;}
#else   /* USE_PNG */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>


struct png_stream {
   unsigned char *stream_ptr;     /*  location to write PNG stream  */
   g2int stream_len;               /*  number of bytes written       */
};
typedef struct png_stream png_stream;

void user_read_data(png_structp , png_bytep , png_uint_32 );

void user_read_data(png_structp png_ptr,png_bytep data, png_uint_32 length)
/*
        Custom read function used so that libpng will read a PNG stream
        from memory instead of a file on disk.
*/
{
     char *ptr;
     g2int offset;
     png_stream *mem;

     mem=(png_stream *)png_get_io_ptr(png_ptr);
     ptr=(void *)mem->stream_ptr;
     offset=mem->stream_len;
/*     printf("SAGrd %ld %ld %x\n",offset,length,ptr);  */
     memcpy(data,ptr+offset,length);
     mem->stream_len += length;
}



int dec_png(unsigned char *pngbuf,g2int *width,g2int *height,char *cout)
{
    int interlace,color,compress,filter,bit_depth;
    g2int j,k,n,bytes,clen;
    png_structp png_ptr;
    png_infop info_ptr,end_info;
    png_bytepp row_pointers;
    png_stream read_io_ptr;

/*  check if stream is a valid PNG format   */

    if ( png_sig_cmp(pngbuf,0,8) != 0) 
       return (-3);

/* create and initialize png_structs  */

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, 
                                      NULL, NULL);
    if (!png_ptr)
       return (-1);

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
       png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL);
       return (-2);
    }

    end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
       png_destroy_read_struct(&png_ptr,(png_infopp)info_ptr,(png_infopp)NULL);
       return (-2);
    }

/*     Set Error callback   */

    if (setjmp(png_jmpbuf(png_ptr)))
    {
       png_destroy_read_struct(&png_ptr, &info_ptr,&end_info);
       return (-3);
    }

/*    Initialize info for reading PNG stream from memory   */

    read_io_ptr.stream_ptr=(png_voidp)pngbuf;
    read_io_ptr.stream_len=0;

/*    Set new custom read function    */

    png_set_read_fn(png_ptr,(voidp)&read_io_ptr,(png_rw_ptr)user_read_data);
/*     png_init_io(png_ptr, fptr);   */

/*     Read and decode PNG stream   */

    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

/*     Get pointer to each row of image data   */

    row_pointers = png_get_rows(png_ptr, info_ptr);

/*     Get image info, such as size, depth, colortype, etc...   */

    /*printf("SAGT:png %d %d %d\n",info_ptr->width,info_ptr->height,info_ptr->bit_depth);*/
    (void)png_get_IHDR(png_ptr, info_ptr, (png_uint_32 *)width, (png_uint_32 *)height,
               &bit_depth, &color, &interlace, &compress, &filter);

/*     Check if image was grayscale      */

/*
    if (color != PNG_COLOR_TYPE_GRAY ) {
       fprintf(stderr,"dec_png: Grayscale image was expected. \n");
    }
*/
    if ( color == PNG_COLOR_TYPE_RGB ) {
       bit_depth=24;
    }
    else if ( color == PNG_COLOR_TYPE_RGB_ALPHA ) {
       bit_depth=32;
    }
/*     Copy image data to output string   */

    n=0;
    bytes=bit_depth/8;
    clen=(*width)*bytes;
    for (j=0;j<*height;j++) {
      for (k=0;k<clen;k++) {
        cout[n]=*(row_pointers[j]+k);
        n++;
      }
    }

/*      Clean up   */

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    return 0;

}

#endif   /* USE_PNG */
