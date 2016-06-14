#include "grib2.h"

#include "cpl_port.h"

#ifndef USE_JPEG2000
int enc_jpeg2000(CPL_UNUSED unsigned char *cin,
                 CPL_UNUSED g2int width,
                 CPL_UNUSED g2int height,
                 CPL_UNUSED g2int nbits,
                 CPL_UNUSED g2int ltype,
                 CPL_UNUSED g2int ratio,
                 CPL_UNUSED g2int retry,
                 CPL_UNUSED char *outjpc,
                 CPL_UNUSED g2int jpclen) { return 0; }

#else   /* USE_JPEG2000 */

#include <stdio.h>
#include <stdlib.h>

#ifdef USE_JPEG2000_J2KSUBFILE
// J2KSUBFILE includes .. TODO!!
#else
#include <jasper/jasper.h>
#define JAS_1_700_2
#endif /* USE_JPEG2000_J2KSUBFILE */

int enc_jpeg2000(unsigned char *cin,g2int width,g2int height,g2int nbits,
                 g2int ltype, g2int ratio, g2int retry, char *outjpc, 
                 g2int jpclen)
/*$$$  SUBPROGRAM DOCUMENTATION BLOCK
*                .      .    .                                       .
* SUBPROGRAM:    enc_jpeg2000      Encodes JPEG2000 code stream
*   PRGMMR: Gilbert          ORG: W/NP11     DATE: 2002-12-02
*
* ABSTRACT: This Function encodes a grayscale image into a JPEG2000 code stream
*   specified in the JPEG2000 Part-1 standard (i.e., ISO/IEC 15444-1) 
*   using JasPer Software version 1.500.4 (or 1.700.2 ) written by the 
*   University of British Columbia, Image Power Inc, and others.
*   JasPer is available at http://www.ece.uvic.ca/~mdadams/jasper/.
*
* PROGRAM HISTORY LOG:
* 2002-12-02  Gilbert
* 2004-12-16  Gilbert - Added retry argument/option to allow option of
*                       increasing the maximum number of guard bits to the
*                       JPEG2000 algorithm.
*
* USAGE:    int enc_jpeg2000(unsigned char *cin,g2int width,g2int height,
*                            g2int nbits, g2int ltype, g2int ratio, 
*                            g2int retry, char *outjpc, g2int jpclen)
*
*   INPUT ARGUMENTS:
*      cin   - Packed matrix of Grayscale image values to encode.
*     width  - width of image
*     height - height of image
*     nbits  - depth (in bits) of image.  i.e number of bits
*              used to hold each data value
*    ltype   - indicator of lossless or lossy compression
*              = 1, for lossy compression
*              != 1, for lossless compression
*    ratio   - target compression ratio.  (ratio:1)
*              Used only when ltype == 1.
*    retry   - Pointer to option type.
*              1 = try increasing number of guard bits
*              otherwise, no additional options
*    jpclen  - Number of bytes allocated for new JPEG2000 code stream in
*              outjpc.
*
*   INPUT ARGUMENTS:
*     outjpc - Output encoded JPEG2000 code stream
*
*   RETURN VALUES :
*        > 0 = Length in bytes of encoded JPEG2000 code stream
*         -3 = Error decode jpeg2000 code stream.
*         -5 = decoded image had multiple color components.
*              Only grayscale is expected.
*
* REMARKS:
*
*      Requires JasPer Software version 1.500.4 or 1.700.2
*
* ATTRIBUTES:
*   LANGUAGE: C
*   MACHINE:  IBM SP
*
*$$$*/
{

#ifdef USE_JPEG2000_J2KSUBFILE
     
    // J2KSUBFILE method ... TODO!!
    return 0;

#else /* USE_JPEG2000_J2KSUBFILE */

    // JasPer method

    int ier,rwcnt;
    jas_image_t image;
    jas_stream_t *jpcstream,*istream;
    jas_image_cmpt_t cmpt,*pcmpt;
#define MAXOPTSSIZE 1024
    char opts[MAXOPTSSIZE];

/*
    printf(" enc_jpeg2000:width %ld\n",width);
    printf(" enc_jpeg2000:height %ld\n",height);
    printf(" enc_jpeg2000:nbits %ld\n",nbits);
    printf(" enc_jpeg2000:jpclen %ld\n",jpclen);
*/
//    jas_init();

//
//    Set lossy compression options, if requested.
//
    if ( ltype != 1 ) {
       opts[0]=(char)0;
    }
    else {
       sprintf(opts,"mode=real\nrate=%f",1.0/(float)ratio);
    }
    if ( retry == 1 ) {             // option to increase number of guard bits
       strcat(opts,"\nnumgbits=4");
    }
    //printf("SAGopts: %s\n",opts);
    
//
//     Initialize the JasPer image structure describing the grayscale
//     image to encode into the JPEG2000 code stream.
//
    image.tlx_=0;
    image.tly_=0;
#ifdef JAS_1_500_4 
    image.brx_=(uint_fast32_t)width;
    image.bry_=(uint_fast32_t)height;
#endif 
#ifdef JAS_1_700_2
    image.brx_=(jas_image_coord_t)width;
    image.bry_=(jas_image_coord_t)height;
#endif
    image.numcmpts_=1;
    image.maxcmpts_=1;
#ifdef JAS_1_500_4
    image.colormodel_=JAS_IMAGE_CM_GRAY;         /* grayscale Image */
#endif
#ifdef JAS_1_700_2
    image.clrspc_=JAS_CLRSPC_SGRAY;         /* grayscale Image */
    image.cmprof_=0; 
#endif
    image.inmem_=1;

    cmpt.tlx_=0;
    cmpt.tly_=0;
    cmpt.hstep_=1;
    cmpt.vstep_=1;
#ifdef JAS_1_500_4
    cmpt.width_=(uint_fast32_t)width;
    cmpt.height_=(uint_fast32_t)height;
#endif
#ifdef JAS_1_700_2
    cmpt.width_=(jas_image_coord_t)width;
    cmpt.height_=(jas_image_coord_t)height;
    cmpt.type_=JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_GRAY_Y);
#endif
    cmpt.prec_=nbits;
    cmpt.sgnd_=0;
    cmpt.cps_=(nbits+7)/8;

    pcmpt=&cmpt;
    image.cmpts_=&pcmpt;

//
//    Open a JasPer stream containing the input grayscale values
//
    istream=jas_stream_memopen((char *)cin,height*width*cmpt.cps_);
    cmpt.stream_=istream;

//
//    Open an output stream that will contain the encoded jpeg2000
//    code stream.
//
    jpcstream=jas_stream_memopen(outjpc,(int)jpclen);

//
//     Encode image.
//
    ier=jpc_encode(&image,jpcstream,opts);
    if ( ier != 0 ) {
       printf(" jpc_encode return = %d \n",ier);
       return -3;
    }
//
//     Clean up JasPer work structures.
//    
    rwcnt=jpcstream->rwcnt_;
    ier=jas_stream_close(istream);
    ier=jas_stream_close(jpcstream);
//
//      Return size of jpeg2000 code stream
//
    return (rwcnt);

#endif /* USE_JPEG2000_J2KSUBFILE */

}

#endif   /* USE_JPEG2000 */
