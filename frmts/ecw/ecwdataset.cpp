/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  ECW (ERMapper Wavelet Compression Format) Driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.1  2001/04/02 17:06:46  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include <NCSECWClient.h>
#include <NCSErrors.h>

static GDALDriver	*poECWDriver = NULL;

#ifdef FRMT_ecw

/************************************************************************/
/* ==================================================================== */
/*				ECWDataset				*/
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand;

class CPL_DLL ECWDataset : public GDALDataset
{
    friend	ECWRasterBand;

    NCSFileView *hFileView;
    NCSFileViewFileInfo *psFileInfo;

  public:
    		ECWDataset();
    		~ECWDataset();
                
    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
};

/************************************************************************/
/* ==================================================================== */
/*                            ECWRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand : public GDALRasterBand
{
    friend	   ECWDataset;
    
    ECWDataset     *poGDS;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

                   ECWRasterBand( ECWDataset *, int );
                   ~ECWRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual int    HasArbitraryOverviews() { return TRUE; }
};

/************************************************************************/
/*                           ECWRasterBand()                            */
/************************************************************************/

ECWRasterBand::ECWRasterBand( ECWDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          ~ECWRasterBand()                           */
/************************************************************************/

ECWRasterBand::~ECWRasterBand()

{
    FlushCache();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ECWRasterBand::IReadBlock( int, int nBlockYOff, void * pImage )

{
    return IRasterIO( GF_Read, 0, nBlockYOff, nBlockXSize, 1, 
                      pImage, nBlockXSize, 1, GDT_Byte, 0, 0 );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ECWRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace, int nLineSpace )
    
{
    ECWDataset	*poODS = (ECWDataset *) poDS;
    NCSError     eNCSErr;
    CPLErr       eErr;
    int          iBand, bDirect;
    GByte        *pabyWorkBuffer = NULL;

    CPLDebug( "ECWRasterBand", 
              "RasterIO(%d,%d,%d,%d -> %dx%d)\n", 
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

    if( nLineSpace == 0 )
        nLineSpace = nBufXSize;

    if( nPixelSpace == 0 )
        nPixelSpace = 1;

/* -------------------------------------------------------------------- */
/*      Can we perform direct loads, or must we load into a working     */
/*      buffer, and transform?                                          */
/* -------------------------------------------------------------------- */
    bDirect = nPixelSpace == 1 && eBufType == GDT_Byte;
    if( !bDirect )
        pabyWorkBuffer = (GByte *) CPLMalloc(nBufXSize);

/* -------------------------------------------------------------------- */
/*      Establish access at the desired resolution.                     */
/* -------------------------------------------------------------------- */
    iBand = nBand-1;
    eNCSErr = NCScbmSetFileView( poODS->hFileView, 
                                 1, (unsigned int *) (&iBand),
                                 nXOff, nYOff, 
                                 nXOff + nXSize - 1, 
                                 nYOff + nYSize - 1,
                                 nBufXSize, nBufYSize );
    if( eNCSErr != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", NCSGetErrorText(eNCSErr) );
        
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read back one scanline at a time, till request is satisfied.    */
/* -------------------------------------------------------------------- */
    int      iScanline;

    for( iScanline = 0; iScanline < nBufYSize; iScanline++ )
    {
        NCSEcwReadStatus  eRStatus;
        unsigned char *pabySrcBuf;

        if( bDirect )
            pabySrcBuf = ((GByte *)pData)+iScanline*nLineSpace;
        else
            pabySrcBuf = pabyWorkBuffer;

        eRStatus = NCScbmReadViewLineBIL( poODS->hFileView, &pabySrcBuf );

        if( eRStatus != NCSECW_READ_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "NCScbmReadViewLineBIL failed." );
            return CE_Failure;
        }

        if( !bDirect )
        {
            GDALCopyWords( pabyWorkBuffer, GDT_Byte, 1, 
                           ((GByte *)pData)+iScanline*nLineSpace, 
                           eBufType, nPixelSpace, nBufXSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            ECWDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            ECWDataset()                            */
/************************************************************************/

ECWDataset::ECWDataset()

{
}

/************************************************************************/
/*                           ~ECWDataset()                            */
/************************************************************************/

ECWDataset::~ECWDataset()

{
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ECWDataset::Open( GDALOpenInfo * poOpenInfo )

{
    NCSFileView      *hFileView = NULL;
    NCSError         eErr;
    int              i;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"ecw") )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    eErr = NCScbmOpenFileView( poOpenInfo->pszFilename, &hFileView, NULL );
    if( eErr != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", NCSGetErrorText(eErr) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ECWDataset 	*poDS;

    poDS = new ECWDataset();

    poDS->poDriver = poECWDriver;
    poDS->hFileView = hFileView;
    
/* -------------------------------------------------------------------- */
/*      Fetch general file information.                                 */
/* -------------------------------------------------------------------- */
    eErr = NCScbmGetViewFileInfo( hFileView, &(poDS->psFileInfo) );
    if( eErr != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", NCSGetErrorText(eErr) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->psFileInfo->nSizeX; 
    poDS->nRasterYSize = poDS->psFileInfo->nSizeY;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i=0; i < poDS->psFileInfo->nBands; i++ )
    {
        poDS->SetBand( i+1, new ECWRasterBand( poDS, i+1 ) );
    }

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ECWDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = psFileInfo->fOriginX;
    padfTransform[1] = psFileInfo->fCellIncrementX;
    padfTransform[2] = 0.0;

    padfTransform[3] = psFileInfo->fOriginY;
    padfTransform[4] = 0.0;
    padfTransform[5] = psFileInfo->fCellIncrementY;

    return( CE_None );
}

#endif /* def FRMT_ecw */
/************************************************************************/
/*                          GDALRegister_ECW()                        */
/************************************************************************/

void GDALRegister_ECW()

{
#ifdef FRMT_ecw 
    GDALDriver	*poDriver;

    if( poECWDriver == NULL )
    {
        poECWDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "ECW";
        poDriver->pszLongName = "ECW (Ermapper Compressed Wavelets)";
        
        poDriver->pfnOpen = ECWDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif /* def FRMT_ecw */
}

