/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Partial implementation of the ISO/IEC 15444-1 standard
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@at1895.spb.edu>
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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.1  2002/09/18 16:49:01  dron
 * Initial release
 *
 *
 *
 */

#include "gdal_priv.h"

#define uchar unsigned char
#define longlong long long
#define ulonglong unsigned long long
#include <jasper/jasper.h>

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_JPEG2000(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				JPEG2000Dataset				*/
/* ==================================================================== */
/************************************************************************/

class JPEG2000Dataset : public GDALDataset
{
    friend class JPEG2000RasterBand;

//    double      adfGeoTransform[6];
    char        *pszProjection;

    FILE	*fp;
    jas_stream_t *sStream;
    jas_image_t	*sImage;

  public:
                JPEG2000Dataset();
		~JPEG2000Dataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

//    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            JPEG2000RasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JPEG2000RasterBand : public GDALRasterBand
{
    friend class JPEG2000Dataset;

    jas_matrix_t	*sMatrix;

  public:

    		JPEG2000RasterBand( JPEG2000Dataset *, int, int, int );
    		~JPEG2000RasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           JPEG2000RasterBand()                       */
/************************************************************************/

JPEG2000RasterBand::JPEG2000RasterBand( JPEG2000Dataset *poDS, int nBand,
		int iDepth, int bSignedness )

{
    this->poDS = poDS;
    this->nBand = nBand;

    // XXX: JasPer can't handle data with depth > 32 bits
    // Maximum possible depth for JPEG2000 is 38!
    switch ( bSignedness )
    {
	case 1:		// Signed component
	if (iDepth <= 8)
	    this->eDataType = GDT_Byte; // FIXME: should be signed
	else if (iDepth <= 16)
            this->eDataType = GDT_Int16;
	else if (iDepth <= 32)
            this->eDataType = GDT_Int32;
	break;
	case 0:		// Unsigned component
	default:
	if (iDepth <= 8)
	    this->eDataType = GDT_Byte;
	else if (iDepth <= 16)
            this->eDataType = GDT_UInt16;
	else if (iDepth <= 32)
            this->eDataType = GDT_UInt32;
	break;
    }

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->GetRasterYSize();
    sMatrix = jas_matrix_create(nBlockYSize, nBlockXSize);
    
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEG2000RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    int i, j;
    JPEG2000Dataset *poGDS = (JPEG2000Dataset *) poDS;
    
    jas_image_readcmpt( poGDS->sImage, nBand - 1, nBlockXOff, nBlockYOff,
	nBlockXSize, nBlockYSize, sMatrix );

    for( i = 0; i < jas_matrix_numrows(sMatrix); i++ )
	for( j = 0; j < jas_matrix_numcols(sMatrix); j++ )
	    // XXX: We need casting because matrix element always
	    // has 32 bit depth in JasPer
	    // FIXME: what about float values?
	    switch( eDataType )
	    {
	        case GDT_Int16:
                *((GInt16*)pImage)++ = (GInt16)jas_matrix_get(sMatrix, i, j);
		break;
	        case GDT_Int32:
                *((GInt32*)pImage)++ = (GInt32)jas_matrix_get(sMatrix, i, j);
		break;
	        case GDT_UInt16:
                *((GUInt16*)pImage)++ = (GUInt16)jas_matrix_get(sMatrix, i, j);
		break;
	        case GDT_UInt32:
                *((GUInt32*)pImage)++ = (GUInt32)jas_matrix_get(sMatrix, i, j);
		break;
	        case GDT_Byte:
	        default:
                *((GByte*)pImage)++ = (GByte)jas_matrix_get(sMatrix, i, j);
		break;
	    }

    return CE_None;
}

/************************************************************************/
/*                         ~JPEG2000RasterBand()                        */
/************************************************************************/

JPEG2000RasterBand::~JPEG2000RasterBand()

{
    jas_matrix_destroy( sMatrix );
}


/************************************************************************/
/* ==================================================================== */
/*				JPEG2000Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JPEG2000Dataset()                           */
/************************************************************************/

JPEG2000Dataset::JPEG2000Dataset()

{
    fp = NULL;
    pszProjection = "";
    nBands = 0;
}

/************************************************************************/
/*                            ~JPEG2000Dataset()                         */
/************************************************************************/

JPEG2000Dataset::~JPEG2000Dataset()

{
    jas_stream_close( sStream );
    jas_image_destroy(sImage);
    jas_image_clearfmts();
    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/*CPLErr JPEG2000Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}*/

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JPEG2000Dataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPEG2000Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		iFormat;
    char	*pszFormatName = NULL;
    jas_stream_t *sS;

    if( poOpenInfo->fp == NULL )
        return NULL;
   
    jas_init();
    if( !(sS = jas_stream_fopen( poOpenInfo->pszFilename, "rb" )) )
	return NULL;
    iFormat = jas_image_getfmt( sS );
    if ( !(pszFormatName = jas_image_fmttostr( iFormat )) )
	return NULL;
    if ( strlen( pszFormatName ) < 3 ||
        (!EQUALN( pszFormatName, "jp2", 3 ) &&
	 !EQUALN( pszFormatName, "jpc", 3 ) &&
	 !EQUALN( pszFormatName, "pgx", 3 )) )
    {
        jas_stream_close( sS );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPEG2000Dataset 	*poDS;

    poDS = new JPEG2000Dataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    poDS->sStream = sS;

    if ( !(poDS->sImage = jas_image_decode(poDS->sStream, iFormat, 0)) )
        return NULL;

    poDS->nBands = jas_image_numcmpts( poDS->sImage );
    poDS->nRasterXSize = jas_image_cmptwidth( poDS->sImage, 0 );
    poDS->nRasterYSize = jas_image_cmptheight( poDS->sImage, 0 );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int iBand;
    
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JPEG2000RasterBand( poDS, iBand,
	    jas_image_cmptprec(poDS->sImage, iBand - 1),
	    jas_image_cmptsgnd(poDS->sImage, iBand - 1) ) );
        
    }

    return( poDS );
}

#if 0
/************************************************************************/
/*                      JPEG2000CreateCopy()                            */
/************************************************************************/

static GDALDataset *
JPEG2000CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                    int bStrict, char ** papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    int			iBand;
    jas_stream_t	*sStream;
    jas_image_t		*sImage;
    jas_image_cmptparm_t *sComps; // Array of pointers to image components

    if( !(sStream = jas_stream_fopen( pszFilename, "w+b" )) )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }
    
    sComps = (jas_image_cmptparm_t*)
	CPLMalloc( nBands * sizeof(jas_image_cmptparm_t) );
    for ( iBand = 0; iBand < nBands; iBand++ )
    {
        sComps[iBand].tlx = sComps[iBand].tly = 0;
	sComps[iBand].hstep = sComps[iBand].vstep = 1;
	sComps[iBand].width = nXSize;
	sComps[iBand].height = nYSize;
	sComps[iBand].prec = 8; // FIXME
	sComps[iBand].sgnd = 0; // FIXME
    }
    if ( !(sImage = /*jas_image_create0()*/jas_image_create( nBands, sComps, 0 )) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to create image %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GDALRasterBand	*poBand;
    GUInt32		*paiScanline;
    int         	iLine, iPixel/*, bSuccess */;
    CPLErr      	eErr = CE_None;
    jas_matrix_t	*sMatrix;
    
    if ( !(sMatrix = jas_matrix_create( nYSize, nXSize )) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to create matrix with size %dx%d.\n", 
                   nXSize, nYSize );
        return NULL;
    }
    paiScanline = (GUInt32 *) CPLMalloc( nXSize *
                            GDALGetDataTypeSize(GDT_UInt32) / 8 );
    /*if ( !(jas_image_encode( sImage, sStream, jas_image_strtofmt("jp2"), 0 )) )
    -of JPEG2000 test.tif test.jp2{
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to encode image %s.\n", 
                  pszFilename );
        return NULL;
    }*/
    for ( iBand = 1; iBand <= nBands; iBand++ )
    {
        poBand = poSrcDS->GetRasterBand( iBand );
        for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                              paiScanline, nXSize, 1, GDT_UInt32,
                              sizeof(GUInt32), sizeof(GUInt32) * nXSize );
            for ( iPixel = 0; iPixel < nXSize; iPixel++ )
                jas_matrix_set( sMatrix, iPixel, iLine,
		                (jas_seqent_t)paiScanline[iPixel] );
            /*jas_image_writecmpt(sImage, iBand - 1, 0, iLine,
			    nXSize, 1, sMatrix);*/
            if( eErr == CE_None &&
            !pfnProgress((iLine + 1) / ((double) nYSize), NULL, pProgressData))
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt, 
                      "User terminated CreateCopy()" );
            }
        }
        if( !(jas_image_addcmpt(sImage, iBand - 1, &sComps[iBand - 1])) );
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Unable to add component %d to image %s.\n", 
                iBand - 1, pszFilename );
            return NULL;
        }
        if( !(jas_image_writecmpt(sImage, iBand - 1, 0, 0,
    		    nXSize, nYSize, sMatrix)) );
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Unable to write component %d of image %s.\n", 
                iBand - 1, pszFilename );
            return NULL;
        }
    }
    if ( !(jas_image_encode( sImage, sStream, jas_image_strtofmt("jp2"), 0 )) )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Unable to encode image %s.\n", 
                  pszFilename );
        return NULL;
    }
    jas_stream_flush( sStream );
    
    jas_matrix_destroy( sMatrix );
    CPLFree( paiScanline );
    jas_image_destroy( sImage );
    CPLFree( sComps );
    jas_image_clearfmts();
    if ( jas_stream_close( sStream ) )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Unable to close file %s.\n", 
                  pszFilename );
        return NULL;
    }

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}
#endif

/************************************************************************/
/*                        GDALRegister_JPEG2000()				*/
/************************************************************************/

void GDALRegister_JPEG2000()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JPEG2000" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JPEG2000" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG-2000 part 1 (ISO/IEC 15444-1)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpeg2000.html" );

        poDriver->pfnOpen = JPEG2000Dataset::Open;
//        poDriver->pfnCreateCopy = JPEG2000CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

