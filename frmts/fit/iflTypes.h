#if 0 

    Copyright (c) 1991 SGI   All Rights Reserved
    THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
    The copyright notice above does not evidence any
    actual or intended publication of such source code,
    and is an unpublished work by Silicon Graphics, Inc.
    This material contains CONFIDENTIAL INFORMATION that
    is the property of Silicon Graphics, Inc. Any use,
    duplication or disclosure not specifically authorized
    by Silicon Graphics is strictly prohibited.
    
    RESTRICTED RIGHTS LEGEND:
    
    Use, duplication or disclosure by the Government is
    subject to restrictions as set forth in subdivision
    (c)(1)(ii) of the Rights in Technical Data and Computer
    Software clause at DFARS 52.227-7013, and/or in similar
    or successor clauses in the FAR, DOD or NASA FAR
    Supplement.  Unpublished- rights reserved under the
    Copyright Laws of the United States.  Contractor is
    SILICON GRAPHICS, INC., 2011 N. Shoreline Blvd.,
    Mountain View, CA 94039-7311

#endif
#ifndef _iflTypes_h_
#define _iflTypes_h_

#include <ifl/iflDefs.h>
#include <sys/types.h>


/* Define the image data types */

enum iflDataType {
    iflBit 	= 1,	/* single-bit */
    iflUChar 	= 2,	/* unsigned character (byte) */
    iflChar 	= 4,	/* signed character (byte) */
    iflUShort	= 8,	/* unsigned short integer (nominally 16 bits) */
    iflShort	= 16,	/* signed short integer */
    iflUInt	= 32,	/* unsigned integer (nominally 32 bits) */
    iflInt	= 64,	/* integer */
    iflULong	= 32,	/* deprecated, same as iflUInt */
    iflLong	= 64,	/* deprecated, same as iflULong */
    iflFloat	= 128,	/* floating point */
    iflDouble	= 256	/* double precision floating point */
};
#ifndef __cplusplus
typedef enum iflDataType iflDataType;
#endif


/* Define the pixel component ordering */

enum iflOrder {
    iflInterleaved = 1,	/* store as RGBRGBRGBRGB... */
    iflSequential  = 2,	/* store as RRR..GGG..BBB.. per line */
    iflSeparate    = 4	/* channels are stored in separate tiles */
};
#ifndef __cplusplus
typedef enum iflOrder iflOrder;
#endif


/* Define supported Color Models */

enum iflColorModel {
    iflNegative = 1, 	    /* inverted luminance (min value is white) */
    iflLuminance = 2,	    /* luminamce */
    iflRGB = 3,	    	    /* full color (Red, Green, Blue triplets) */
    iflRGBPalette = 4,	    /* color mapped values */
    iflRGBA = 5,	    /* full color with transparency (alpha channel) */
    iflHSV = 6,	    	    /* Hue, Saturation, Value */
    iflCMY = 7,	    	    /* Cyan, Magenta, Yellow */
    iflCMYK = 8,	    /* Cyan, Magenta, Yellow */
    iflBGR = 9,	    	    /* full color (ordered Blue, Green, Red) */
    iflABGR = 10,	    /* Alpha, Blue, Green, Red (SGI frame buffers) */
    iflMultiSpectral = 11,  /* multi-spectral data, arbitrary number of chans */
    iflYCC = 12, 	    /* PhotoCD color model (Luminance, Chrominance) */
    iflLuminanceAlpha = 13  /* Luminance plus alpha */
};
#ifndef __cplusplus
typedef enum iflColorModel iflColorModel;
#endif


/* Define supported orientations */

enum iflOrientation {
    /* NOTE: These values cannot be changed.  Various tables (and the TIFF
             library interface) depend on their ordering */
    iflUpperLeftOrigin = 1,  /* from upper left corner, scan right then down */
    iflUpperRightOrigin = 2, /* from upper right corner, scan left then down */
    iflLowerRightOrigin = 3, /* from lower right corner, scan left then up */
    iflLowerLeftOrigin = 4,  /* from lower left corner, scan right then up */
    iflLeftUpperOrigin = 5,  /* from upper left corner, scan down then right */
    iflRightUpperOrigin = 6, /* from upper right corner, scan down then left */
    iflRightLowerOrigin = 7, /* from lower right corner, scan up then left */
    iflLeftLowerOrigin = 8   /* from lower left corner, scan up then right */
};
#ifndef __cplusplus
typedef enum iflOrientation iflOrientation;
#endif


/* Define supported compression schemes */

enum iflCompression {
    iflUnknownCompression = -1, /* Compressed, but not in the list below */
    iflNoCompression = 1,   	/* Not compressed */
    iflSGIRLE = 2,	    	/* SGI's RLE compression */
    iflCCITTFAX3 = 3,	    	/* CCITT Group 3 fax encoding */
    iflCCITTFAX4 = 4,	    	/* CCITT Group 4 fax encoding */
    iflLZW = 5,		    	/* Lempel-Ziv  & Welch */
    iflJPEG = 6,	    	/* Joint Photographics Expert Group */
    iflPACKBITS = 7,	    	/* Macintosh RLE */
    iflZIP = 8		    	/* ZIP deflate/inflate */
};
#ifndef __cplusplus
typedef enum iflCompression iflCompression;
#endif


/* Define flip modes */

enum iflFlip {
    iflNoFlip = 0,	    /* No Flip */
    iflXFlip = 1,	    /* Flip about X axis of input */
    iflYFlip = 2	    /* Flip about Y axis of input */
};
#ifndef __cplusplus
typedef enum iflFlip iflFlip;
#endif

struct iflClassList;
/* 
 * class ID is really the address of 
 * a class's iflClassList member (see iflClassList.h) 
 */
typedef const struct iflClassList* iflClassId;

/* use iflClassID macro to construct parameter to derivesFrom method */
#ifdef __cplusplus
#define iflClassID(class) &class::classList
#else
#define iflClassID(class) /* not implemented */
#endif
#define iflClassIdNone NULL

enum iflAxis {
    iflX = 0, iflY = 1, iflZ = 2, iflC = 3
};
#ifndef __cplusplus
typedef enum iflAxis iflAxis;
#endif

#endif
