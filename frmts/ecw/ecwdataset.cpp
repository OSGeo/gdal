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
 * Revision 1.14  2003/09/23 13:47:47  warmerda
 * never return a NULL projection
 *
 * Revision 1.13  2003/06/19 18:50:20  warmerda
 * added projection reading support
 *
 * Revision 1.12  2003/06/18 17:26:35  warmerda
 * dont try ecwopen function if path isnt to a valid file
 *
 * Revision 1.11  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.10  2002/10/01 19:34:27  warmerda
 * fixed problems with supersampling by forcing through fullres blocks
 *
 * Revision 1.9  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.8  2002/07/23 14:40:41  warmerda
 * disable compress support on Unix
 *
 * Revision 1.7  2002/07/23 14:05:35  warmerda
 * Avoid warnings.
 *
 * Revision 1.6  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.5  2002/01/22 14:48:19  warmerda
 * Fixed pabyWorkBuffer memory leak in IRasterIO().
 *
 * Revision 1.4  2001/11/11 23:50:59  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.3  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.2  2001/04/20 03:06:07  warmerda
 * addec compression support
 *
 * Revision 1.1  2001/04/02 17:06:46  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include <NCSECWClient.h>
#include <NCSECWCompressClient.h>
#include <NCSErrors.h>

CPL_CVSID("$Id$");

#ifdef FRMT_ecw

/* As of July 2002 only uncompress support is available on Unix */
#ifdef WIN32
#define HAVE_COMPRESS
#endif

static int    gnTriedCSFile = FALSE;
static char **gpapszCSLookup = NULL;

typedef struct {
    int              bCancelled;
    GDALProgressFunc pfnProgress;
    void             *pProgressData;
    GDALDataset      *poSrc;
} ECWCompressInfo;

/************************************************************************/
/* ==================================================================== */
/*				ECWDataset				*/
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand;

class CPL_DLL ECWDataset : public GDALDataset
{
    friend class ECWRasterBand;

    NCSFileView *hFileView;
    NCSFileViewFileInfo *psFileInfo;

    int         nFullResWindowBand;
    int         nLastScanlineRead;
    CPLErr      ResetFullResViewWindow(int nBand, int nStartLine );

    char        *pszProjection;

    void        ECW2WKTProjection();

  public:
    		ECWDataset();
    		~ECWDataset();
                
    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            ECWRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand : public GDALRasterBand
{
    friend class ECWDataset;
    
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
    int          iBand, bDirect;
    GByte        *pabyWorkBuffer = NULL;

/* -------------------------------------------------------------------- */
/*      Supersampling is not supported by the ECW API, so just turn     */
/*      over control to the normal API.  We also drop down to the       */
/*      block oriented API if only a single scanline was requested.     */
/*      This is based on the assumption that doing lots of single       */
/*      scanline windows is expensive.                                  */
/* -------------------------------------------------------------------- */
    if( nXSize < nBufXSize || nYSize < nBufYSize || nYSize == 1 )	
    {
#ifdef notdef
        CPLDebug( "ECWRasterBand", 
                  "RasterIO(%d,%d,%d,%d -> %dx%d) - redirected.", 
                  nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );
#endif
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, 
                                         eBufType, nPixelSpace, nLineSpace );
    }
#ifdef notdef
    CPLDebug( "ECWRasterBand", 
              "RasterIO(%d,%d,%d,%d -> %dx%d)", 
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );
#endif

/* -------------------------------------------------------------------- */
/*      Default line and pixel spacing if needed.                       */
/* -------------------------------------------------------------------- */
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
    poODS->nFullResWindowBand = -1;
    eNCSErr = NCScbmSetFileView( poODS->hFileView, 
                                 1, (unsigned int *) (&iBand),
                                 nXOff, nYOff, 
                                 nXOff + nXSize - 1, 
                                 nYOff + nYSize - 1,
                                 nBufXSize, nBufYSize );
    if( eNCSErr != NCS_SUCCESS )
    {
        CPLFree( pabyWorkBuffer );
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
            CPLFree( pabyWorkBuffer );
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

    CPLFree( pabyWorkBuffer );

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ECWRasterBand::IReadBlock( int, int nBlockYOff, void * pImage )

{
    ECWDataset	*poODS = (ECWDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Do we need to reset the view window?                            */
/* -------------------------------------------------------------------- */
    if( nBand != poODS->nFullResWindowBand
        || poODS->nLastScanlineRead >= nBlockYOff )
    {
        CPLErr eErr;

        eErr = poODS->ResetFullResViewWindow( nBand, nBlockYOff );
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Load lines till we get to the desired scanline.                 */
/* -------------------------------------------------------------------- */
    while( poODS->nLastScanlineRead < nBlockYOff )
    {
        NCSEcwReadStatus  eRStatus;

        eRStatus = NCScbmReadViewLineBIL( poODS->hFileView, 
                                          (unsigned char **) &pImage );
        
        if( eRStatus != NCSECW_READ_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "NCScbmReadViewLineBIL failed." );
            return CE_Failure;
        }

        poODS->nLastScanlineRead++;
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
    nFullResWindowBand = -1;
    pszProjection = NULL;
}

/************************************************************************/
/*                           ~ECWDataset()                            */
/************************************************************************/

ECWDataset::~ECWDataset()

{
    CPLFree( pszProjection );
}

/************************************************************************/
/*                       ResetFullResViewWindow()                       */
/************************************************************************/

CPLErr ECWDataset::ResetFullResViewWindow( int nBand,
                                           int nStartLine )

{
    NCSError     eNCSErr;
    unsigned int iBand = nBand-1;
#ifdef nodef
    CPLDebug( "ECWDataset", 
              "NCScbmSetFileView( %d, %d, %d, %d, %d, %d, %d )", 
              nBand,
              0, nStartLine, 
              nRasterXSize-1, nRasterYSize-1,
              nRasterXSize, nRasterYSize-nStartLine );
#endif
                              
    eNCSErr = NCScbmSetFileView( hFileView, 1, &iBand,
                                 0, nStartLine, 
                                 nRasterXSize-1, nRasterYSize-1,
                                 nRasterXSize, nRasterYSize-nStartLine );

    if( eNCSErr != NCS_SUCCESS )
    {
        nFullResWindowBand = -1;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", NCSGetErrorText(eNCSErr) );
        return CE_Failure;
    }

    nFullResWindowBand = nBand;
    nLastScanlineRead = nStartLine-1;

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ECWDataset::Open( GDALOpenInfo * poOpenInfo )

{
    NCSFileView      *hFileView = NULL;
    NCSError         eErr;
    int              i;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"ecw")
        || poOpenInfo->fp == NULL )
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

    CPLDebug( "ECW", "FileInfo: SizeXY=%d,%d Bands=%d\n"
              "       OriginXY=%g,%g  CellIncrementXY=%g,%g\n",
              poDS->psFileInfo->nSizeX,
              poDS->psFileInfo->nSizeY,
              poDS->psFileInfo->nBands,
              poDS->psFileInfo->fOriginX,
              poDS->psFileInfo->fOriginY,
              poDS->psFileInfo->fCellIncrementX,
              poDS->psFileInfo->fCellIncrementY );

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

    poDS->ECW2WKTProjection();

    return( poDS );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ECWDataset::GetProjectionRef() 

{
    if( pszProjection == NULL )
        return "";
    else
        return pszProjection;
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

/************************************************************************/
/*                         ECW2WKTProjection()                          */
/*                                                                      */
/*      Set the dataset pszProjection string in OGC WKT format by       */
/*      looking up the ECW (GDT) coordinate system info in              */
/*      ecw_cs.dat support data file.                                   */
/*                                                                      */
/*      This code is likely still broken in some circumstances.  For    */
/*      instance, I haven't been careful about changing the linear      */
/*      projection parameters (false easting/northing) if the units     */
/*      is feet.  Lots of cases missing here, and in ecw_cs.dat.        */
/************************************************************************/

void ECWDataset::ECW2WKTProjection()

{
    if( psFileInfo == NULL )
        return;

    CPLDebug( "ECW", "projection=%s, datum=%s",
              psFileInfo->szProjection, psFileInfo->szDatum );

    if( gnTriedCSFile && gpapszCSLookup == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Load the supporting data file with the coordinate system        */
/*      translations if we don't already have it loaded.  Note,         */
/*      currently we never unload the file, even if the driver is       */
/*      destroyed ... but we should.                                    */
/* -------------------------------------------------------------------- */
    if( !gnTriedCSFile )
    {
        const char *pszFilename = CPLFindFile( "data", "ecw_cs.dat" );
        
        gnTriedCSFile = TRUE;
        if( pszFilename != NULL )
            gpapszCSLookup = CSLLoad( pszFilename );
        if( gpapszCSLookup == NULL )
            return;
    }

/* -------------------------------------------------------------------- */
/*      Set projection if we have it.                                   */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    if( EQUAL(psFileInfo->szProjection,"GEODETIC") )
    {
    }
    else
    {
        const char *pszProjWKT;

        pszProjWKT = CSLFetchNameValue( gpapszCSLookup, 
                                        psFileInfo->szProjection );

        if( pszProjWKT == NULL 
            || EQUAL(pszProjWKT,"unsupported")
            || oSRS.importFromWkt( (char **) &pszProjWKT ) != OGRERR_NONE )
        {
            oSRS.SetLocalCS( psFileInfo->szProjection );
        }

        if( psFileInfo->eCellSizeUnits == ECW_CELL_UNITS_FEET )
            oSRS.SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV));
        else
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    }

/* -------------------------------------------------------------------- */
/*      Set the geogcs.                                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oGeogCS;
    const char *pszGeogWKT;

    pszGeogWKT = CSLFetchNameValue( gpapszCSLookup, 
                                    psFileInfo->szDatum );

    if( pszGeogWKT == NULL )
        oGeogCS.SetWellKnownGeogCS( "WGS84" );
    else
    {
        oGeogCS.importFromWkt( (char **) &pszGeogWKT );
    }

    if( !oSRS.IsLocal() )
        oSRS.CopyGeogCSFrom( &oGeogCS );

/* -------------------------------------------------------------------- */
/*      Capture the resulting composite coordiante system.              */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL )
    {
        CPLFree( pszProjection );
        pszProjection = NULL;
    }
    oSRS.exportToWkt( &pszProjection );
}

/************************************************************************/
/*                         ECWCompressReadCB()                          */
/************************************************************************/

static BOOLEAN ECWCompressReadCB( struct NCSEcwCompressClient *psClient, 
                                  UINT32 nNextLine, IEEE4 **ppInputArray )

{
    ECWCompressInfo      *psCompressInfo;
    int                  iBand;

    psCompressInfo = (ECWCompressInfo *) psClient->pClientData;

    for( iBand = 0; iBand < (int) psClient->nInputBands; iBand++ )
    {
        GDALRasterBand      *poBand;


        poBand = psCompressInfo->poSrc->GetRasterBand( iBand+1 );

        if( poBand->RasterIO( GF_Read, 0, nNextLine, poBand->GetXSize(), 1, 
                              ppInputArray[iBand], poBand->GetXSize(), 1, 
                              GDT_Float32, 0, 0 ) != CE_None )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                        ECWCompressStatusCB()                         */
/************************************************************************/

static void ECWCompressStatusCB( NCSEcwCompressClient *psClient, 
                                 UINT32 nCurrentLine )

{
    ECWCompressInfo      *psCompressInfo;

    psCompressInfo = (ECWCompressInfo *) psClient->pClientData;
    
    psCompressInfo->bCancelled = 
        !psCompressInfo->pfnProgress( nCurrentLine 
                                      / (float) psClient->nInOutSizeY,
                                      NULL, psCompressInfo->pProgressData );
}

/************************************************************************/
/*                        ECWCompressCancelCB()                         */
/************************************************************************/

static BOOLEAN ECWCompressCancelCB( NCSEcwCompressClient *psClient )

{
    ECWCompressInfo      *psCompressInfo;

    psCompressInfo = (ECWCompressInfo *) psClient->pClientData;

    return (BOOLEAN) psCompressInfo->bCancelled;
}

#ifdef HAVE_COMPRESS
/************************************************************************/
/*                           ECWCreateCopy()                            */
/************************************************************************/

static GDALDataset *
ECWCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary checking in input.                          */
/* -------------------------------------------------------------------- */
    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ECW driver requires at least one band as input." );
        return NULL;
    }

    if( nXSize < 128 || nYSize < 128 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ECW driver requires image to be at least 128x128,\n"
                  "the source image is %dx%d.\n", 
                  nXSize, nYSize );
        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "CW driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse out some known options.                                   */
/* -------------------------------------------------------------------- */
    float      fTargetCompression = 75.0;

    if( CSLFetchNameValue(papszOptions, "TARGET") != NULL )
    {
        fTargetCompression = (float) 
            atof(CSLFetchNameValue(papszOptions, "TARGET"));
        
        if( fTargetCompression < 1.1 || fTargetCompression > 100.0 )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "TARGET compression of %.3f invalid, should be a\n"
                      "value between 1 and 100 percent.\n", 
                      (double) fTargetCompression );
            return NULL;
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Create and initialize compressor.                               */
/* -------------------------------------------------------------------- */
    NCSEcwCompressClient      *psClient;
    ECWCompressInfo           sCompressInfo;

    psClient = NCSEcwCompressAllocClient();
    if( psClient == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "NCSEcwCompressAllocClient() failed.\n" );
        return NULL;
    }

    psClient->nInputBands = nBands;
    psClient->nInOutSizeX = nXSize;
    psClient->nInOutSizeY = nYSize;
    psClient->fTargetCompression = fTargetCompression;

    if( nBands == 1 )
        psClient->eCompressFormat = COMPRESS_UINT8;
    else if( nBands == 3 )
        psClient->eCompressFormat = COMPRESS_RGB;
    else
        psClient->eCompressFormat = COMPRESS_MULTI;

    strcpy( psClient->szOutputFilename, pszFilename );

    sCompressInfo.bCancelled = FALSE;
    sCompressInfo.pfnProgress = pfnProgress;
    sCompressInfo.pProgressData = pProgressData;
    sCompressInfo.poSrc = poSrcDS;

    psClient->pReadCallback = ECWCompressReadCB;
    psClient->pStatusCallback = ECWCompressStatusCB;
    psClient->pCancelCallback = ECWCompressCancelCB;
    psClient->pClientData = (void *) &sCompressInfo;

/* -------------------------------------------------------------------- */
/*      Set block size if desired.                                      */
/* -------------------------------------------------------------------- */
    psClient->nBlockSizeX = 256;
    psClient->nBlockSizeY = 256;
    if( CSLFetchNameValue(papszOptions, "BLOCKSIZE") != NULL )
    {
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKSIZE"));
        psClient->nBlockSizeY = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKSIZE"));
    }
        
    if( CSLFetchNameValue(papszOptions, "BLOCKXSIZE") != NULL )
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKXSIZE"));
        
    if( CSLFetchNameValue(papszOptions, "BLOCKYSIZE") != NULL )
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKYSIZE"));
        
/* -------------------------------------------------------------------- */
/*      Georeferencing.                                                 */
/* -------------------------------------------------------------------- */
    double      adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
            CPLError( CE_Warning, CPLE_NotSupported, 
                      "Rotational coefficients ignored, georeferencing of\n"
                      "output ECW file will be incorrect.\n" );
        else
        {
            psClient->fOriginX = adfGeoTransform[0];
            psClient->fOriginY = adfGeoTransform[3];
            psClient->fCellIncrementX = adfGeoTransform[1];
            psClient->fCellIncrementY = adfGeoTransform[5];
        }
    }

/* -------------------------------------------------------------------- */
/*      Start the compression.                                          */
/* -------------------------------------------------------------------- */
    NCSError      eError;

    eError = NCSEcwCompressOpen( psClient, FALSE );
    if( eError == NCS_SUCCESS )
    {
        eError = NCSEcwCompress( psClient );
        NCSEcwCompressClose( psClient );
    }

    if( eError != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ECW compression failed.\n%s", 
                  NCSGetErrorText( eError ) );

        NCSEcwCompressFreeClient( psClient );
        return NULL;
    }

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Cleanup, and return read-only handle.                           */
/* -------------------------------------------------------------------- */
    NCSEcwCompressFreeClient( psClient );
    
    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}
#endif /* def HAVE_COMPRESS */
#endif /* def FRMT_ecw */

/************************************************************************/
/*                          GDALRegister_ECW()                        */
/************************************************************************/

void GDALRegister_ECW()

{
#ifdef FRMT_ecw 
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ECW" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ECW" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ERMapper Compressed Wavelets" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_ecw.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ecw" );
        
        poDriver->pfnOpen = ECWDataset::Open;
#ifdef HAVE_COMPRESS
        poDriver->pfnCreateCopy = ECWCreateCopy;
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );
#endif

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif /* def FRMT_ecw */
}

