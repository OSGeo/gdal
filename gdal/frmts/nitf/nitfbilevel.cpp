/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Module implement BILEVEL (C1) compressed image reading.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "nitflib.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
// #include "tiff.h"
CPL_C_START
#include "tiffio.h"
CPL_C_END
#include "tifvsi.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                       NITFUncompressBILEVEL()                        */
/************************************************************************/

int NITFUncompressBILEVEL( NITFImage *psImage,
                           GByte *pabyInputData, int nInputBytes,
                           GByte *pabyOutputImage )

{
/* -------------------------------------------------------------------- */
/*      Write memory TIFF with the bilevel data.                        */
/* -------------------------------------------------------------------- */
    const int nOutputBytes= (psImage->nBlockWidth * psImage->nBlockHeight + 7)/8;

    CPLString osFilename;

    osFilename.Printf( "/vsimem/nitf-wrk-%ld.tif", (long) CPLGetPID() );

    VSILFILE* fpL = VSIFOpenL(osFilename, "w+");
    if( fpL == NULL )
        return FALSE;
    TIFF *hTIFF = VSI_TIFFOpen( osFilename, "w+", fpL );
    if (hTIFF == NULL)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
        return FALSE;
    }

    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH,    psImage->nBlockWidth );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH,   psImage->nBlockHeight );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, 1 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT,  SAMPLEFORMAT_UINT );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG,  PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_FILLORDER,     FILLORDER_MSB2LSB );

    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP,  psImage->nBlockHeight );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX3 );

    if( psImage->szCOMRAT[0] == '2' )
        TIFFSetField( hTIFF, TIFFTAG_GROUP3OPTIONS, GROUP3OPT_2DENCODING );

    TIFFWriteRawStrip( hTIFF, 0, pabyInputData, nInputBytes );
    TIFFWriteDirectory( hTIFF );

    TIFFClose( hTIFF );

/* -------------------------------------------------------------------- */
/*      Now open and read it back.                                      */
/* -------------------------------------------------------------------- */
    bool bResult = true;

    hTIFF = VSI_TIFFOpen( osFilename, "r", fpL );
    if (hTIFF == NULL)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
        return FALSE;
    }

    if( TIFFReadEncodedStrip( hTIFF, 0, pabyOutputImage, nOutputBytes ) == -1 )
    {
        memset( pabyOutputImage, 0, nOutputBytes );
        bResult = false;
    }

    TIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));

    VSIUnlink( osFilename );

    return bResult;
}
