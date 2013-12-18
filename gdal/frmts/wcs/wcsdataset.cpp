/******************************************************************************
 * $Id$
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*				WCSDataset				*/
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand;

class CPL_DLL WCSDataset : public GDALPamDataset
{
    friend class WCSRasterBand;

    int         bServiceDirty;
    CPLXMLNode *psService;

    char       *apszCoverageOfferingMD[2];

    char      **papszSDSModifiers;

    int         nVersion;  // eg 100 for 1.0.0, 110 for 1.1.0

    CPLString   osCRS;

    char        *pszProjection;
    double      adfGeoTransform[6];

    CPLString   osBandIdentifier;

    CPLString   osDefaultTime;
    std::vector<CPLString> aosTimePositions;

    int         TestUseBlockIO( int, int, int, int, int, int );
    CPLErr      DirectRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *, int, int, int );
    CPLErr      GetCoverage( int nXOff, int nYOff, int nXSize, int nYSize,
                             int nBufXSize, int nBufYSize, 
                             int nBandCount, int *panBandList,
                             CPLHTTPResult **ppsResult );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    int		DescribeCoverage();
    int         ExtractGridInfo100();
    int         ExtractGridInfo();
    int         EstablishRasterDetails();

    int         ProcessError( CPLHTTPResult *psResult );
    GDALDataset *GDALOpenResult( CPLHTTPResult *psResult );
    void        FlushMemoryResult();
    CPLString   osResultFilename;
    GByte      *pabySavedDataBuffer;

    char      **papszHttpOptions;

    int         nMaxCols;
    int         nMaxRows;
    
  public:
                WCSDataset();
                ~WCSDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual char **GetFileList(void);

    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char *pszDomain );
};

/************************************************************************/
/* ==================================================================== */
/*                            WCSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand : public GDALPamRasterBand
{
    friend class WCSDataset;

    int            iOverview;
    int            nResFactor;

    WCSDataset    *poODS;

    int            nOverviewCount;
    WCSRasterBand **papoOverviews;
    
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

                   WCSRasterBand( WCSDataset *, int nBand, int iOverview );
                  ~WCSRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = NULL );

    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           WCSRasterBand()                            */
/************************************************************************/

WCSRasterBand::WCSRasterBand( WCSDataset *poDS, int nBand, int iOverview )

{
    poODS = poDS;
    this->nBand = nBand;

    eDataType = GDALGetDataTypeByName( 
        CPLGetXMLValue( poDS->psService, "BandType", "Byte" ) );

/* -------------------------------------------------------------------- */
/*      Establish resolution reduction for this overview level.         */
/* -------------------------------------------------------------------- */
    this->iOverview = iOverview;
    nResFactor = 1 << (iOverview+1); // iOverview == -1 is base layer

/* -------------------------------------------------------------------- */
/*      Establish block size.                                           */
/* -------------------------------------------------------------------- */
    nRasterXSize = poDS->GetRasterXSize() / nResFactor;
    nRasterYSize = poDS->GetRasterYSize() / nResFactor;
    
    nBlockXSize = atoi(CPLGetXMLValue( poDS->psService, "BlockXSize", "0" ) );
    nBlockYSize = atoi(CPLGetXMLValue( poDS->psService, "BlockYSize", "0" ) );

    if( nBlockXSize < 1 )
    {
        if( nRasterXSize > 1800 )
            nBlockXSize = 1024;
        else
            nBlockXSize = nRasterXSize;
    }
    
    if( nBlockYSize < 1 )
    {
        if( nRasterYSize > 900 )
            nBlockYSize = 512;
        else
            nBlockYSize = nRasterYSize;
    }

/* -------------------------------------------------------------------- */
/*      If this is the base layer, create the overview layers.          */
/* -------------------------------------------------------------------- */
    if( iOverview == -1 )
    {
        int i;

        nOverviewCount = atoi(CPLGetXMLValue(poODS->psService,"OverviewCount",
                                             "-1"));
        if( nOverviewCount < 0 )
        {
            for( nOverviewCount = 0; 
                 (MAX(nRasterXSize,nRasterYSize) / (1 << nOverviewCount)) > 900;
                 nOverviewCount++ ) {}
        }
        else if( nOverviewCount > 30 )
        {
            /* There's no reason to have more than 30 overviews, because */
            /* 2^(30+1) overflows a int32 */
            nOverviewCount = 30;
        }

        papoOverviews = (WCSRasterBand **) 
            CPLCalloc( nOverviewCount, sizeof(void*) );
        
        for( i = 0; i < nOverviewCount; i++ )
            papoOverviews[i] = new WCSRasterBand( poODS, nBand, i );
    }
    else
    {
        nOverviewCount = 0;
        papoOverviews = NULL;
    }
}

/************************************************************************/
/*                           ~WCSRasterBand()                           */
/************************************************************************/

WCSRasterBand::~WCSRasterBand()
    
{
    FlushCache();
    
    if( nOverviewCount > 0 )
    {
        int i;
        
        for( i = 0; i < nOverviewCount; i++ )
            delete papoOverviews[i];

        CPLFree( papoOverviews );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr WCSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    CPLErr eErr;
    CPLHTTPResult *psResult = NULL;

    eErr = poODS->GetCoverage( nBlockXOff * nBlockXSize * nResFactor, 
                               nBlockYOff * nBlockYSize * nResFactor,
                               nBlockXSize * nResFactor, 
                               nBlockYSize * nResFactor, 
                               nBlockXSize, nBlockYSize, 
                               1, &nBand, &psResult );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poTileDS = poODS->GDALOpenResult( psResult );

    if( poTileDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Verify configuration.                                           */
/* -------------------------------------------------------------------- */
    if( poTileDS->GetRasterXSize() != nBlockXSize
        || poTileDS->GetRasterYSize() != nBlockYSize )
    {
        CPLDebug( "WCS", "Got size=%dx%d instead of %dx%d.", 
                  poTileDS->GetRasterXSize(), poTileDS->GetRasterYSize(),
                  nBlockXSize, nBlockYSize );

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected configuration.\n"
                  "Got %dx%d instead of %dx%d.", 
                  poTileDS->GetRasterXSize(), poTileDS->GetRasterYSize(),
                  nBlockXSize, nBlockYSize );
        delete poTileDS;
        return CE_Failure;
    }

    if( (strlen(poODS->osBandIdentifier) && poTileDS->GetRasterCount() != 1)
        || (!strlen(poODS->osBandIdentifier) 
            && poTileDS->GetRasterCount() != poODS->GetRasterCount()) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected band configuration.");
        delete poTileDS;
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process all bands of memory result, copying into pBuffer, or    */
/*      pushing into cache for other bands.                             */
/* -------------------------------------------------------------------- */
    int iBand;
    eErr = CE_None;
    
    for( iBand = 0; 
         iBand < poTileDS->GetRasterCount() && eErr == CE_None; 
         iBand++ )
    {
        GDALRasterBand *poTileBand = poTileDS->GetRasterBand( iBand+1 );

        if( iBand+1 == GetBand() || strlen(poODS->osBandIdentifier) )
        {
            eErr = poTileBand->RasterIO( GF_Read, 
                                         0, 0, nBlockXSize, nBlockYSize, 
                                         pImage, nBlockXSize, nBlockYSize, 
                                         eDataType, 0, 0 );
        }
        else
        {
            GDALRasterBand *poTargBand = poODS->GetRasterBand( iBand+1 );
            
            if( iOverview != -1 )
                poTargBand = poTargBand->GetOverview( iOverview );
            
            GDALRasterBlock *poBlock = poTargBand->GetLockedBlockRef(
                nBlockXOff, nBlockYOff, TRUE );

            if( poBlock != NULL )
            {
                eErr = poTileBand->RasterIO( GF_Read,
                                            0, 0, nBlockXSize, nBlockYSize,
                                            poBlock->GetDataRef(),
                                            nBlockXSize, nBlockYSize,
                                            eDataType, 0, 0 );
                poBlock->DropLock();
            }
            else
                eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    delete poTileDS;
    
    poODS->FlushMemoryResult();

    return eErr;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WCSRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace, int nLineSpace )
    
{
    if( (poODS->nMaxCols > 0 && poODS->nMaxCols < nBufXSize)
        ||  (poODS->nMaxRows > 0 && poODS->nMaxRows < nBufYSize) )
        return CE_Failure;

    if( poODS->TestUseBlockIO( nXOff, nYOff, nXSize, nYSize,
                               nBufXSize,nBufYSize ) )
        return GDALPamRasterBand::IRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nPixelSpace, nLineSpace );
    else
        return poODS->DirectRasterIO( 
            eRWFlag, 
            nXOff * nResFactor, nYOff * nResFactor, 
            nXSize * nResFactor, nYSize * nResFactor,
            pData, nBufXSize, nBufYSize, eBufType, 
            1, &nBand, nPixelSpace, nLineSpace, 0 );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double WCSRasterBand::GetNoDataValue( int *pbSuccess )

{
    CPLLocaleC  oLocaleEnforcer;
    const char *pszSV = CPLGetXMLValue( poODS->psService, "NoDataValue", NULL);

    if( pszSV == NULL )
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
    else
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return atof(pszSV);
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int WCSRasterBand::GetOverviewCount() 
    
{
    return nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *WCSRasterBand::GetOverview( int iOverview )

{
    if( iOverview < 0 || iOverview >= nOverviewCount )
        return NULL;
    else
        return papoOverviews[iOverview];
}

/************************************************************************/
/* ==================================================================== */
/*                            WCSDataset                                */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                             WCSDataset()                             */
/************************************************************************/

WCSDataset::WCSDataset()

{
    psService = NULL;
    bServiceDirty = FALSE;
    pszProjection = NULL;
    
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    pabySavedDataBuffer = NULL;
    papszHttpOptions = NULL;

    nMaxCols = -1;
    nMaxRows = -1;

    apszCoverageOfferingMD[0] = NULL;
    apszCoverageOfferingMD[1] = NULL;

    papszSDSModifiers = NULL;
}

/************************************************************************/
/*                            ~WCSDataset()                             */
/************************************************************************/

WCSDataset::~WCSDataset()

{
    // perhaps this should be moved into a FlushCache() method.
    if( bServiceDirty && !EQUALN(GetDescription(),"<WCS_GDAL>",10) )
    {
        CPLSerializeXMLTreeToFile( psService, GetDescription() );
        bServiceDirty = FALSE;
    }

    CPLDestroyXMLNode( psService );

    CPLFree( pszProjection );
    pszProjection = NULL;

    CSLDestroy( papszHttpOptions );
    CSLDestroy( papszSDSModifiers );

    CPLFree( apszCoverageOfferingMD[0] );

    FlushMemoryResult();
}

/************************************************************************/
/*                           TestUseBlockIO()                           */
/*                                                                      */
/*      Check whether we should use blocked IO (true) or direct io      */
/*      (FALSE) for a given request configuration and environment.      */
/************************************************************************/

int WCSDataset::TestUseBlockIO( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize )

{
    int bUseBlockedIO = bForceCachedIO;

    if( nYSize == 1 || nXSize * ((double) nYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( nBufYSize == 1 || nBufXSize * ((double) nBufYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( bUseBlockedIO
        && CSLTestBoolean( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
        bUseBlockedIO = FALSE;

    return bUseBlockedIO;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WCSDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, 
                              int nBandCount, int *panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace)

{
    if( (nMaxCols > 0 && nMaxCols < nBufXSize)
        ||  (nMaxRows > 0 && nMaxRows < nBufYSize) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      We need various criteria to skip out to block based methods.    */
/* -------------------------------------------------------------------- */
    if( TestUseBlockIO( nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize ) )
        return GDALPamDataset::IRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );
    else
        return DirectRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );
}

/************************************************************************/
/*                           DirectRasterIO()                           */
/*                                                                      */
/*      Make exactly one request to the server for this data.           */
/************************************************************************/

CPLErr 
WCSDataset::DirectRasterIO( GDALRWFlag eRWFlag,
                            int nXOff, int nYOff, int nXSize, int nYSize,
                            void * pData, int nBufXSize, int nBufYSize,
                            GDALDataType eBufType, 
                            int nBandCount, int *panBandMap,
                            int nPixelSpace, int nLineSpace, int nBandSpace)

{
    CPLDebug( "WCS", "DirectRasterIO(%d,%d,%d,%d) -> (%d,%d) (%d bands)\n", 
              nXOff, nYOff, nXSize, nYSize, 
              nBufXSize, nBufYSize, nBandCount );

/* -------------------------------------------------------------------- */
/*      Get the coverage.                                               */
/* -------------------------------------------------------------------- */
    CPLHTTPResult *psResult = NULL;
    CPLErr eErr = 
        GetCoverage( nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, 
                     nBandCount, panBandMap, &psResult );

    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poTileDS = GDALOpenResult( psResult );

    if( poTileDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Verify configuration.                                           */
/* -------------------------------------------------------------------- */
    if( poTileDS->GetRasterXSize() != nBufXSize
        || poTileDS->GetRasterYSize() != nBufYSize )
    {
        CPLDebug( "WCS", "Got size=%dx%d instead of %dx%d.", 
                  poTileDS->GetRasterXSize(), poTileDS->GetRasterYSize(),
                  nBufXSize, nBufYSize );

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected configuration.\n"
                  "Got %dx%d instead of %dx%d.", 
                  poTileDS->GetRasterXSize(), poTileDS->GetRasterYSize(),
                  nBufXSize, nBufYSize );
        delete poTileDS;
        return CE_Failure;
    }

    if( (strlen(osBandIdentifier) && poTileDS->GetRasterCount() != nBandCount)
        || (!strlen(osBandIdentifier) && poTileDS->GetRasterCount() != 
            GetRasterCount() ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected band count." );
        delete poTileDS;
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Pull requested bands from the downloaded dataset.               */
/* -------------------------------------------------------------------- */
    int iBand;

    eErr = CE_None;
    
    for( iBand = 0; 
         iBand < nBandCount && eErr == CE_None; 
         iBand++ )
    {
        GDALRasterBand *poTileBand;

        if( strlen(osBandIdentifier) )
            poTileBand = poTileDS->GetRasterBand( iBand + 1 );
        else
            poTileBand = poTileDS->GetRasterBand( panBandMap[iBand] );

        eErr = poTileBand->RasterIO( GF_Read, 
                                     0, 0, nBufXSize, nBufYSize,
                                     ((GByte *) pData) + 
                                     iBand * nBandSpace, nBufXSize, nBufYSize, 
                                     eBufType, nPixelSpace, nLineSpace );
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    delete poTileDS;
    
    FlushMemoryResult();

    return eErr;
}

/************************************************************************/
/*                            GetCoverage()                             */
/*                                                                      */
/*      Issue the appropriate version of request for a given window,    */
/*      buffer size and band list.                                      */
/************************************************************************/

CPLErr WCSDataset::GetCoverage( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                int nBandCount, int *panBandList,
                                CPLHTTPResult **ppsResult )

{
    CPLLocaleC oLocaleEnforcer;

/* -------------------------------------------------------------------- */
/*      Figure out the georeferenced extents.                           */
/* -------------------------------------------------------------------- */
    double dfMinX, dfMaxX, dfMinY, dfMaxY;
    
    // WCS 1.0 extents are the outer edges of outer pixels.
    dfMinX = adfGeoTransform[0] + 
        (nXOff) * adfGeoTransform[1];
    dfMaxX = adfGeoTransform[0] + 
        (nXOff + nXSize) * adfGeoTransform[1];
    dfMaxY = adfGeoTransform[3] + 
        (nYOff) * adfGeoTransform[5];
    dfMinY = adfGeoTransform[3] + 
        (nYOff + nYSize) * adfGeoTransform[5];

/* -------------------------------------------------------------------- */
/*      Build band list if we have the band identifier.                 */
/* -------------------------------------------------------------------- */
    CPLString osBandList;
    int       bSelectingBands = FALSE;
    
    if( strlen(osBandIdentifier) && nBandCount > 0 )
    {
        int iBand;

        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( iBand > 0 )
                osBandList += ",";
            osBandList += CPLString().Printf( "%d", panBandList[iBand] );
        }

        bSelectingBands = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      URL encode strings that could have questionable characters.     */
/* -------------------------------------------------------------------- */
    CPLString osCoverage, osFormat;
    char *pszEncoded; 

    osCoverage = CPLGetXMLValue( psService, "CoverageName", "" );

    pszEncoded = CPLEscapeString( osCoverage, -1, CPLES_URL );
    osCoverage = pszEncoded;
    CPLFree( pszEncoded );
    
    osFormat = CPLGetXMLValue( psService, "PreferredFormat", "" );

    pszEncoded = CPLEscapeString( osFormat, -1, CPLES_URL );
    osFormat = pszEncoded;
    CPLFree( pszEncoded );
    
/* -------------------------------------------------------------------- */
/*      Do we have a time we want to use?                               */
/* -------------------------------------------------------------------- */
    CPLString osTime;

    osTime = CSLFetchNameValueDef( papszSDSModifiers, "time", osDefaultTime );
    
/* -------------------------------------------------------------------- */
/*      Construct a "simple" GetCoverage request (WCS 1.0).		*/
/* -------------------------------------------------------------------- */
    CPLString osRequest;

    if( nVersion == 100 )
    {
        osRequest.Printf( 
            "%sSERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE=%s"
            "&FORMAT=%s&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=%d&HEIGHT=%d&CRS=%s%s",
            CPLGetXMLValue( psService, "ServiceURL", "" ),
            osCoverage.c_str(),
            osFormat.c_str(),
            dfMinX, dfMinY, dfMaxX, dfMaxY,
            nBufXSize, nBufYSize, 
            osCRS.c_str(),
            CPLGetXMLValue( psService, "GetCoverageExtra", "" ) );
 
        if( CPLGetXMLValue( psService, "Resample", NULL ) )
        {
            osRequest += "&INTERPOLATION=";
            osRequest += CPLGetXMLValue( psService, "Resample", "" );
        }

        if( osTime != "" )
        {
            osRequest += "&time=";
            osRequest += osTime;
        }

        if( bSelectingBands )
        {
            osRequest += CPLString().Printf( "&%s=%s", 
                                             osBandIdentifier.c_str(),
                                             osBandList.c_str() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Construct a "simple" GetCoverage request (WCS 1.1+).            */
/* -------------------------------------------------------------------- */
    else
    {
        CPLString osRangeSubset;

        osRangeSubset.Printf("&RangeSubset=%s", 
                             CPLGetXMLValue(psService,"FieldName",""));
        
        if( CPLGetXMLValue( psService, "Resample", NULL ) )
        {
            osRangeSubset += ":";
            osRangeSubset += CPLGetXMLValue( psService, "Resample", "");
        }

        if( bSelectingBands )
        {
            osRangeSubset += 
                CPLString().Printf( "[%s[%s]]",
                                    osBandIdentifier.c_str(), 
                                    osBandList.c_str() );
        }

        // WCS 1.1 extents are centers of outer pixels.
        dfMaxX -= adfGeoTransform[1] * 0.5;
        dfMinX += adfGeoTransform[1] * 0.5;
        dfMinY -= adfGeoTransform[5] * 0.5;
        dfMaxY += adfGeoTransform[5] * 0.5;

        // Carefully adjust bounds for pixel centered values at new 
        // sampling density.

        double dfXStep = adfGeoTransform[1];
        double dfYStep = adfGeoTransform[5];

        if( nBufXSize != nXSize || nBufYSize != nYSize )
        {
            dfXStep = (nXSize/(double)nBufXSize) * adfGeoTransform[1];
            dfYStep = (nYSize/(double)nBufYSize) * adfGeoTransform[5];
            
            dfMinX  = nXOff * adfGeoTransform[1] + adfGeoTransform[0] 
                    + dfXStep * 0.5;
            dfMaxX  = dfMinX + (nBufXSize - 1) * dfXStep;

            dfMaxY  = nYOff * adfGeoTransform[5] + adfGeoTransform[3] 
                    + dfYStep * 0.5;
            dfMinY  = dfMaxY + (nBufYSize - 1) * dfYStep;
        }

        osRequest.Printf( 
            "%sSERVICE=WCS&VERSION=%s&REQUEST=GetCoverage&IDENTIFIER=%s"
            "&FORMAT=%s&BOUNDINGBOX=%.15g,%.15g,%.15g,%.15g,%s%s%s",
            CPLGetXMLValue( psService, "ServiceURL", "" ),
            CPLGetXMLValue( psService, "Version", "" ),
            osCoverage.c_str(),
            osFormat.c_str(),
            dfMinX, dfMinY, dfMaxX, dfMaxY,
            osCRS.c_str(),
            osRangeSubset.c_str(),
            CPLGetXMLValue( psService, "GetCoverageExtra", "" ) );

        if( nBufXSize != nXSize || nBufYSize != nYSize )
        {
            osRequest += CPLString().Printf( 
                "&GridBaseCRS=%s"
                "&GridCS=%s"
                "&GridType=urn:ogc:def:method:WCS:1.1:2dGridIn2dCrs"
                "&GridOrigin=%.15g,%.15g"
                "&GridOffsets=%.15g,%.15g",
                osCRS.c_str(), 
                osCRS.c_str(), 
                dfMinX, dfMaxY,
                dfXStep, dfYStep );
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    *ppsResult = CPLHTTPFetch( osRequest, papszHttpOptions );

    if( ProcessError( *ppsResult ) )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          DescribeCoverage()                          */
/*                                                                      */
/*      Fetch the DescribeCoverage result and attach it to the          */
/*      service description.                                            */
/************************************************************************/

int WCSDataset::DescribeCoverage()

{
    CPLString osRequest;

/* -------------------------------------------------------------------- */
/*      Fetch coverage description for this coverage.                   */
/* -------------------------------------------------------------------- */
    if( nVersion == 100 )
        osRequest.Printf( 
            "%sSERVICE=WCS&REQUEST=DescribeCoverage&VERSION=%s&COVERAGE=%s%s", 
            CPLGetXMLValue( psService, "ServiceURL", "" ),
            CPLGetXMLValue( psService, "Version", "1.0.0" ),
            CPLGetXMLValue( psService, "CoverageName", "" ),
            CPLGetXMLValue( psService, "DescribeCoverageExtra", "" ) );
    else
        osRequest.Printf( 
            "%sSERVICE=WCS&REQUEST=DescribeCoverage&VERSION=%s&IDENTIFIERS=%s%s&FORMAT=text/xml", 
            CPLGetXMLValue( psService, "ServiceURL", "" ),
            CPLGetXMLValue( psService, "Version", "1.0.0" ),
            CPLGetXMLValue( psService, "CoverageName", "" ),
            CPLGetXMLValue( psService, "DescribeCoverageExtra", "" ) );

    CPLErrorReset();
    
    CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, papszHttpOptions );

    if( ProcessError( psResult ) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Parse result.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDC = CPLParseXMLString( (const char *) psResult->pabyData );
    CPLHTTPDestroyResult( psResult );

    if( psDC == NULL )
        return FALSE;

    CPLStripXMLNamespace( psDC, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Did we get a CoverageOffering?                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCO;

    if( nVersion == 100 )
        psCO = CPLGetXMLNode( psDC, "=CoverageDescription.CoverageOffering" );
    else
        psCO =CPLGetXMLNode( psDC,"=CoverageDescriptions.CoverageDescription");

    if( !psCO )
    {
        CPLDestroyXMLNode( psDC );

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to fetch a <CoverageOffering> back %s.", 
                  osRequest.c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Duplicate the coverage offering, and insert into                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNext = psCO->psNext;
    psCO->psNext = NULL;
    
    CPLAddXMLChild( psService, CPLCloneXMLTree( psCO ) );
    bServiceDirty = TRUE;

    psCO->psNext = psNext;

    CPLDestroyXMLNode( psDC );
    return TRUE;
}

/************************************************************************/
/*                         ExtractGridInfo100()                         */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 1.0.0    */
/*      and above.                                                      */
/************************************************************************/

int WCSDataset::ExtractGridInfo100()

{
    CPLLocaleC  oLocaleEnforcer; 
    CPLXMLNode * psCO = CPLGetXMLNode( psService, "CoverageOffering" );

    if( psCO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We need to strip off name spaces so it is easier to             */
/*      searchfor plain gml names.                                      */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psCO, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Verify we have a Rectified Grid.                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRG = 
        CPLGetXMLNode( psCO, "domainSet.spatialDomain.RectifiedGrid" );

    if( psRG == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find RectifiedGrid in CoverageOffering,\n"
                  "unable to process WCS Coverage." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract size, geotransform and coordinate system.               */
/* -------------------------------------------------------------------- */
    if( GDALParseGMLCoverage( psRG, &nRasterXSize, &nRasterYSize, 
                              adfGeoTransform, &pszProjection ) != CE_None )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Fallback to nativeCRSs declaration.                             */
/* -------------------------------------------------------------------- */
    const char *pszNativeCRSs = 
        CPLGetXMLValue( psCO, "supportedCRSs.nativeCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs = 
            CPLGetXMLValue( psCO, "supportedCRSs.requestResponseCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs = 
            CPLGetXMLValue( psCO, "supportedCRSs.requestCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs = 
            CPLGetXMLValue( psCO, "supportedCRSs.responseCRSs", NULL );

    if( pszNativeCRSs != NULL  
        && (pszProjection == NULL || strlen(pszProjection) == 0) )
    {
        OGRSpatialReference oSRS;
        
        if( oSRS.SetFromUserInput( pszNativeCRSs ) == OGRERR_NONE )
        {
            CPLFree( pszProjection );
            oSRS.exportToWkt( &pszProjection );
        }
        else
            CPLDebug( "WCS", 
                      "<nativeCRSs> element contents not parsable:\n%s", 
                      pszNativeCRSs );
    }

    // We should try to use the services name for the CRS if possible.
    if( pszNativeCRSs != NULL
        && ( EQUALN(pszNativeCRSs,"EPSG:",5)
             || EQUALN(pszNativeCRSs,"AUTO:",5)
             || EQUALN(pszNativeCRSs,"Image ",6)
             || EQUALN(pszNativeCRSs,"Engineering ",12)
             || EQUALN(pszNativeCRSs,"OGC:",4) ) )
    {
        osCRS = pszNativeCRSs;
        
        size_t nDivider = osCRS.find( " " );

        if( nDivider != std::string::npos )
            osCRS.resize( nDivider-1 );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a coordinate system override?                        */
/* -------------------------------------------------------------------- */
    const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", NULL );
    
    if( pszProjOverride )
    {
        OGRSpatialReference oSRS;
        
        if( oSRS.SetFromUserInput( pszProjOverride ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "<SRS> element contents not parsable:\n%s", 
                      pszProjOverride );
            return FALSE;
        }

        CPLFree( pszProjection );
        oSRS.exportToWkt( &pszProjection );

        if( EQUALN(pszProjOverride,"EPSG:",5)
            || EQUALN(pszProjOverride,"AUTO:",5)
            || EQUALN(pszProjOverride,"OGC:",4)
            || EQUALN(pszProjOverride,"Image ",6)
            || EQUALN(pszProjOverride,"Engineering ",12) )
            osCRS = pszProjOverride;
    }

/* -------------------------------------------------------------------- */
/*      Build CRS name to use.                                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    const char *pszAuth;

    if( pszProjection && strlen(pszProjection) > 0 && osCRS == "" )
    {
        oSRS.SetFromUserInput( pszProjection );
        pszAuth = oSRS.GetAuthorityName(NULL);
    
        if( pszAuth != NULL && EQUAL(pszAuth,"EPSG") )
        {
            pszAuth = oSRS.GetAuthorityCode(NULL);
            if( pszAuth )
            {
                osCRS = "EPSG:";
                osCRS += pszAuth;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to define CRS to use." );
                return FALSE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Pick a format type if we don't already have one selected.       */
/*                                                                      */
/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "PreferredFormat", NULL ) == NULL )
    {
        CPLXMLNode *psSF = CPLGetXMLNode( psCO, "supportedFormats" );
        CPLXMLNode *psNode;
        char **papszFormatList = NULL;
        CPLString osPreferredFormat;
        int iFormat;

        if( psSF == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No <PreferredFormat> tag in service definition file, and no\n"
                      "<supportedFormats> in coverageOffering." );
            return FALSE;
        }

        for( psNode = psSF->psChild; psNode != NULL; psNode = psNode->psNext )
        {
            if( psNode->eType == CXT_Element 
                && EQUAL(psNode->pszValue,"formats") 
                && psNode->psChild != NULL 
                && psNode->psChild->eType == CXT_Text )
            {
                // This check is looking for deprecated WCS 1.0 capabilities
                // with multiple formats space delimited in a single <formats>
                // element per GDAL ticket 1748 (done by MapServer 4.10 and
                // earlier for instance). 
                if( papszFormatList == NULL
                    && psNode->psNext == NULL
                    && strstr(psNode->psChild->pszValue," ") != NULL
                    && strstr(psNode->psChild->pszValue,";") == NULL )
                {
                    char **papszSubList = 
                        CSLTokenizeString( psNode->psChild->pszValue );
                    papszFormatList = CSLInsertStrings( papszFormatList, 
                                                        -1, papszSubList );
                    CSLDestroy( papszSubList );
                }
                else
                {
                    papszFormatList = CSLAddString( papszFormatList, 
                                                    psNode->psChild->pszValue);
                }
            }
        }
        
        for( iFormat = 0; 
             papszFormatList != NULL && papszFormatList[iFormat] != NULL;
             iFormat++ )
        {
            if( strlen(osPreferredFormat) == 0 )
                osPreferredFormat = papszFormatList[iFormat];
            
            if( strstr(papszFormatList[iFormat],"tiff") != NULL 
                    || strstr(papszFormatList[iFormat],"TIFF") != NULL
                    || strstr(papszFormatList[iFormat],"Tiff") != NULL )
            {
                osPreferredFormat = papszFormatList[iFormat];
                break;
            }
        }

        CSLDestroy( papszFormatList );

        if( strlen(osPreferredFormat) > 0 )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "PreferredFormat", 
                                         osPreferredFormat );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to identify a nodata value.  For now we only support the    */
/*      singleValue mechanism.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "NoDataValue", NULL ) == NULL )
    {
        const char *pszSV = CPLGetXMLValue( psCO, "rangeSet.RangeSet.nullValues.singleValue", NULL );
        
        if( pszSV != NULL && (atof(pszSV) != 0.0 || *pszSV == '0') )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "NoDataValue", 
                                         pszSV );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a Band range type.  For now we look for a fairly     */
/*      specific configuration.  The rangeset my have one axis named    */
/*      "Band", with a set of ascending numerical values.               */
/* -------------------------------------------------------------------- */
    osBandIdentifier = CPLGetXMLValue( psService, "BandIdentifier", "" );
    CPLXMLNode * psAD = CPLGetXMLNode( psService, 
      "CoverageOffering.rangeSet.RangeSet.axisDescription.AxisDescription" );
    CPLXMLNode *psValues;

    if( strlen(osBandIdentifier) == 0
        && psAD != NULL 
        && (EQUAL(CPLGetXMLValue(psAD,"name",""),"Band") 
            || EQUAL(CPLGetXMLValue(psAD,"name",""),"Bands"))
        && ( (psValues = CPLGetXMLNode( psAD, "values" )) != NULL ) )
    {
        CPLXMLNode *psSV;
        int iBand;

        osBandIdentifier = CPLGetXMLValue(psAD,"name","");

        for( psSV = psValues->psChild, iBand = 1; 
             psSV != NULL; 
             psSV = psSV->psNext, iBand++ )
        {
            if( psSV->eType != CXT_Element 
                || !EQUAL(psSV->pszValue,"singleValue") 
                || psSV->psChild == NULL
                || psSV->psChild->eType != CXT_Text
                || atoi(psSV->psChild->pszValue) != iBand )
            {
                osBandIdentifier = "";
                break;
            }
        }

        if( strlen(osBandIdentifier) )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "BandIdentifier", 
                                         osBandIdentifier );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a temporal domain?  If so, try to identify a         */
/*      default time value.                                             */
/* -------------------------------------------------------------------- */
    osDefaultTime = CPLGetXMLValue( psService, "DefaultTime", "" );
    CPLXMLNode * psTD = 
        CPLGetXMLNode( psService, "CoverageOffering.domainSet.temporalDomain" );
    CPLString osServiceURL = CPLGetXMLValue( psService, "ServiceURL", "" );
    CPLString osCoverageExtra = CPLGetXMLValue( psService, "GetCoverageExtra", "" );

    if( psTD != NULL )
    {
        CPLXMLNode *psTime;

        // collect all the allowed time positions.

        for( psTime = psTD->psChild; psTime != NULL; psTime = psTime->psNext )
        {
            if( psTime->eType == CXT_Element
                && EQUAL(psTime->pszValue,"timePosition")
                && psTime->psChild != NULL
                && psTime->psChild->eType == CXT_Text )
                aosTimePositions.push_back( psTime->psChild->pszValue );
        }

        // we will default to the last - likely the most recent - entry.
        
        if( aosTimePositions.size() > 0 
            && osDefaultTime == ""
            && osServiceURL.ifind("time=") == std::string::npos
            && osCoverageExtra.ifind("time=") == std::string::npos )
        {
            osDefaultTime = aosTimePositions[aosTimePositions.size()-1];

            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "DefaultTime", 
                                         osDefaultTime );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          ParseBoundingBox()                          */
/************************************************************************/

static int ParseBoundingBox( CPLXMLNode *psBoundingBox, CPLString &osCRS, 
                             double &dfLowerX, double &dfLowerY, 
                             double &dfUpperX, double &dfUpperY )

{
    CPLLocaleC  oLocaleEnforcer; 
    int nRet = TRUE;

    osCRS = CPLGetXMLValue( psBoundingBox, "crs", "" );

    char **papszLC = CSLTokenizeStringComplex( 
        CPLGetXMLValue( psBoundingBox, "LowerCorner", ""),
        " ", FALSE, FALSE );
    char **papszUC = CSLTokenizeStringComplex( 
        CPLGetXMLValue( psBoundingBox, "UpperCorner", ""),
        " ", FALSE, FALSE );

    if( CSLCount(papszLC) >= 2 && CSLCount(papszUC) >= 2 )
    {
        dfLowerX = atof(papszLC[0]);
        dfLowerY = atof(papszLC[1]);
        dfUpperX = atof(papszUC[0]);
        dfUpperY = atof(papszUC[1]);
    }
    else
        nRet = FALSE;
    
    CSLDestroy( papszUC );
    CSLDestroy( papszLC );

    return nRet;
}

/************************************************************************/
/*                          ExtractGridInfo()                           */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 1.1      */
/*      and above.                                                      */
/************************************************************************/

int WCSDataset::ExtractGridInfo()

{
    CPLLocaleC  oLocaleEnforcer; 

    if( nVersion == 100 )
        return ExtractGridInfo100();

    CPLXMLNode * psCO = CPLGetXMLNode( psService, "CoverageDescription" );

    if( psCO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We need to strip off name spaces so it is easier to             */
/*      searchfor plain gml names.                                      */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psCO, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Verify we have a SpatialDomain and GridCRS.                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psSD = 
        CPLGetXMLNode( psCO, "Domain.SpatialDomain" );
    CPLXMLNode *psGCRS = 
        CPLGetXMLNode( psSD, "GridCRS" );

    if( psSD == NULL || psGCRS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find GridCRS in CoverageDescription,\n"
                  "unable to process WCS Coverage." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Geotransform from GridCRS.                              */
/* -------------------------------------------------------------------- */
    const char *pszGridType = CPLGetXMLValue( psGCRS, "GridType",
                                              "urn:ogc:def:method:WCS::2dSimpleGrid" );

    char **papszOriginTokens = 
        CSLTokenizeStringComplex( CPLGetXMLValue( psGCRS, "GridOrigin", ""),
                                  " ", FALSE, FALSE );
    char **papszOffsetTokens = 
        CSLTokenizeStringComplex( CPLGetXMLValue( psGCRS, "GridOffsets", ""),
                                  " ", FALSE, FALSE );

    if( strstr(pszGridType,":2dGridIn2dCrs") 
        || strstr(pszGridType,":2dGridin2dCrs") )
    {
        if( CSLCount(papszOffsetTokens) == 4
            && CSLCount(papszOriginTokens) == 2 )
        {
            adfGeoTransform[0] = atof(papszOriginTokens[0]);
            adfGeoTransform[1] = atof(papszOffsetTokens[0]);
            adfGeoTransform[2] = atof(papszOffsetTokens[1]);
            adfGeoTransform[3] = atof(papszOriginTokens[1]);
            adfGeoTransform[4] = atof(papszOffsetTokens[2]);
            adfGeoTransform[5] = atof(papszOffsetTokens[3]);
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "2dGridIn2dCrs does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return FALSE;
        }
    }

    else if( strstr(pszGridType,":2dGridIn3dCrs") )
    {
        if( CSLCount(papszOffsetTokens) == 6
            && CSLCount(papszOriginTokens) == 3 )
        {
            adfGeoTransform[0] = atof(papszOriginTokens[0]);
            adfGeoTransform[1] = atof(papszOffsetTokens[0]);
            adfGeoTransform[2] = atof(papszOffsetTokens[1]);
            adfGeoTransform[3] = atof(papszOriginTokens[1]);
            adfGeoTransform[4] = atof(papszOffsetTokens[3]);
            adfGeoTransform[5] = atof(papszOffsetTokens[4]);
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "2dGridIn3dCrs does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return FALSE;
        }
    }
    
    else if( strstr(pszGridType,":2dSimpleGrid") )
    {
        if( CSLCount(papszOffsetTokens) == 2
            && CSLCount(papszOriginTokens) == 2 )
        {
            adfGeoTransform[0] = atof(papszOriginTokens[0]);
            adfGeoTransform[1] = atof(papszOffsetTokens[0]);
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = atof(papszOriginTokens[1]);
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = atof(papszOffsetTokens[1]);
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "2dSimpleGrid does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return FALSE;
        }
    }

    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognised GridCRS.GridType value '%s',\n"
                  "unable to process WCS coverage.",
                  pszGridType );
        return FALSE;
    }

    CSLDestroy( papszOffsetTokens );
    CSLDestroy( papszOriginTokens );

    // GridOrigin is center of pixel ... offset half pixel to adjust. 

    adfGeoTransform[0] -= (adfGeoTransform[1]+adfGeoTransform[2]) * 0.5; 
    adfGeoTransform[3] -= (adfGeoTransform[4]+adfGeoTransform[5]) * 0.5; 

/* -------------------------------------------------------------------- */
/*      Establish our coordinate system.                                */
/* -------------------------------------------------------------------- */
    osCRS = CPLGetXMLValue( psGCRS, "GridBaseCRS", "" );
    
    if( strlen(osCRS) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find GridCRS.GridBaseCRS" );
        return FALSE;
    }
    else if( strstr(osCRS,":imageCRS") )
    {
        // raw image.
    }
    else
    {
        OGRSpatialReference oSRS;
        if( oSRS.importFromURN( osCRS ) == OGRERR_NONE )
        {
            CPLFree( pszProjection );
            oSRS.exportToWkt( &pszProjection );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to interprete GridBaseCRS '%s'.",
                      osCRS.c_str() );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Search for an ImageCRS for raster size.                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode;

    nRasterXSize = -1;
    nRasterYSize = -1;
    for( psNode = psSD->psChild; 
         psNode != NULL && nRasterXSize == -1;
         psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"BoundingBox") )
            continue;

        double dfLX, dfLY, dfUX, dfUY;
        CPLString osBBCRS;

        if( ParseBoundingBox( psNode, osBBCRS, dfLX, dfLY, dfUX, dfUY )
            && strstr(osBBCRS,":imageCRS") 
            && dfLX == 0 && dfLY == 0 )
        {
            nRasterXSize = (int) (dfUX + 1.01);
            nRasterYSize = (int) (dfUY + 1.01);
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we search for a bounding box in our coordinate        */
/*      system and derive the size from that.                           */
/* -------------------------------------------------------------------- */
    for( psNode = psSD->psChild; 
         psNode != NULL && nRasterXSize == -1;
         psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"BoundingBox") )
            continue;

        double dfLX, dfLY, dfUX, dfUY;
        CPLString osBBCRS;

        if( ParseBoundingBox( psNode, osBBCRS, dfLX, dfLY, dfUX, dfUY )
            && osBBCRS == osCRS
            && adfGeoTransform[2] == 0.0
            && adfGeoTransform[4] == 0.0 )
        {
            nRasterXSize = 
                (int) ((dfUX - dfLX) / adfGeoTransform[1] + 1.01);
            nRasterYSize = 
                (int) ((dfUY - dfLY) / fabs(adfGeoTransform[5]) + 1.01);
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a coordinate system override?                        */
/* -------------------------------------------------------------------- */
    const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", NULL );
    
    if( pszProjOverride )
    {
        OGRSpatialReference oSRS;
        
        if( oSRS.SetFromUserInput( pszProjOverride ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "<SRS> element contents not parsable:\n%s", 
                      pszProjOverride );
            return FALSE;
        }

        CPLFree( pszProjection );
        oSRS.exportToWkt( &pszProjection );
    }

/* -------------------------------------------------------------------- */
/*      Pick a format type if we don't already have one selected.       */
/*                                                                      */
/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "PreferredFormat", NULL ) == NULL )
    {
        CPLXMLNode *psNode;
        CPLString osPreferredFormat;

        for( psNode = psCO->psChild; psNode != NULL; psNode = psNode->psNext )
        {
            if( psNode->eType == CXT_Element 
                && EQUAL(psNode->pszValue,"SupportedFormat") 
                && psNode->psChild
                && psNode->psChild->eType == CXT_Text )
            {
                if( strlen(osPreferredFormat) == 0 )
                    osPreferredFormat = psNode->psChild->pszValue;

                if( strstr(psNode->psChild->pszValue,"tiff") != NULL 
                    || strstr(psNode->psChild->pszValue,"TIFF") != NULL
                    || strstr(psNode->psChild->pszValue,"Tiff") != NULL )
                {
                    osPreferredFormat = psNode->psChild->pszValue;
                    break;
                }
            }
        }

        if( strlen(osPreferredFormat) > 0 )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "PreferredFormat", 
                                         osPreferredFormat );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to identify a nodata value.  For now we only support the    */
/*      singleValue mechanism.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "NoDataValue", NULL ) == NULL )
    {
        const char *pszSV = 
            CPLGetXMLValue( psCO, "Range.Field.NullValue", NULL );
        
        if( pszSV != NULL && (atof(pszSV) != 0.0 || *pszSV == '0') )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "NoDataValue", 
                                         pszSV );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grab the field name, if possible.                               */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "FieldName", NULL ) == NULL )
    {
        CPLString osFieldName = 
            CPLGetXMLValue( psCO, "Range.Field.Identifier", "" );
        
        if( strlen(osFieldName) > 0 )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "FieldName", 
                                         osFieldName );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to find required Identifier name %s for Range Field.",
                      osCRS.c_str() );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a "Band" axis?  If so try to grab the bandcount      */
/*      and data type from it.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode * psAxis = CPLGetXMLNode( 
        psService, "CoverageDescription.Range.Field.Axis" );

    if( (EQUAL(CPLGetXMLValue(psAxis,"Identifier",""),"Band")
         || EQUAL(CPLGetXMLValue(psAxis,"Identifier",""),"Bands"))
        && CPLGetXMLNode(psAxis,"AvailableKeys") != NULL )
    {
        osBandIdentifier = CPLGetXMLValue(psAxis,"Identifier","");
        
        // verify keys are ascending starting at 1
        CPLXMLNode *psValues = CPLGetXMLNode(psAxis,"AvailableKeys");
        CPLXMLNode *psSV;
        int iBand;
        
        for( psSV = psValues->psChild, iBand = 1; 
             psSV != NULL; 
             psSV = psSV->psNext, iBand++ )
        {
            if( psSV->eType != CXT_Element 
                || !EQUAL(psSV->pszValue,"Key") 
                || psSV->psChild == NULL
                || psSV->psChild->eType != CXT_Text
                || atoi(psSV->psChild->pszValue) != iBand )
            {
                osBandIdentifier = "";
                break;
            }
        }
        
        if( strlen(osBandIdentifier) )
        {
            bServiceDirty = TRUE;
            if( CPLGetXMLValue(psService,"BandIdentifier",NULL) == NULL )
                CPLCreateXMLElementAndValue( psService, "BandIdentifier", 
                                             osBandIdentifier );

            if( CPLGetXMLValue(psService,"BandCount",NULL) == NULL )
                CPLCreateXMLElementAndValue( psService, "BandCount", 
                                             CPLString().Printf("%d",iBand-1));
        }

        // Is this an ESRI server returning a GDAL recognised data type?
        CPLString osDataType = CPLGetXMLValue( psAxis, "DataType", "" );
        if( GDALGetDataTypeByName(osDataType) != GDT_Unknown 
            && CPLGetXMLValue(psService,"BandType",NULL) == NULL )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "BandType", osDataType );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                            ProcessError()                            */
/*                                                                      */
/*      Process an HTTP error, reporting it via CPL, and destroying     */
/*      the HTTP result object.  Returns TRUE if there was an error,    */
/*      or FALSE if the result seems ok.                                */
/************************************************************************/

int WCSDataset::ProcessError( CPLHTTPResult *psResult )

{
/* -------------------------------------------------------------------- */
/*      There isn't much we can do in this case.  Hopefully an error    */
/*      was already issued by CPLHTTPFetch()                            */
/* -------------------------------------------------------------------- */
    if( psResult == NULL || psResult->nDataLen == 0 )
    {
        CPLHTTPDestroyResult( psResult );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If we got an html document, we presume it is an error           */
/*      message and report it verbatim up to a certain size limit.      */
/* -------------------------------------------------------------------- */

    if( psResult->pszContentType != NULL 
        && strstr(psResult->pszContentType, "html") != NULL )
    {
        CPLString osErrorMsg = (char *) psResult->pabyData;

        if( osErrorMsg.size() > 2048 )
            osErrorMsg.resize( 2048 );

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Malformed Result:\n%s", 
                  osErrorMsg.c_str() );
        CPLHTTPDestroyResult( psResult );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Does this look like a service exception?  We would like to      */
/*      check based on the Content-type, but this seems quite           */
/*      undependable, even from MapServer!                              */
/* -------------------------------------------------------------------- */
    if( strstr((const char *)psResult->pabyData, "ServiceException") 
        || strstr((const char *)psResult->pabyData, "ExceptionReport") )
    {
        CPLXMLNode *psTree = CPLParseXMLString( (const char *) 
                                                psResult->pabyData );
        const char *pszMsg = NULL;

        CPLStripXMLNamespace( psTree, NULL, TRUE );

        // VERSION 1.0.0
        if( psTree != NULL )
            pszMsg = CPLGetXMLValue(psTree,
                                    "=ServiceExceptionReport.ServiceException",
                                    NULL );
        // VERSION 1.1.0
        if( pszMsg == NULL )
            pszMsg = CPLGetXMLValue(psTree,
                                    "=ExceptionReport.Exception.ExceptionText",
                                    NULL );

        if( pszMsg )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s", pszMsg );
        else
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Corrupt Service Exception:\n%s",
                      (const char *) psResult->pabyData );
        
        CPLDestroyXMLNode( psTree );
        CPLHTTPDestroyResult( psResult );
        return TRUE;
    }


/* -------------------------------------------------------------------- */
/*      Hopefully the error already issued by CPLHTTPFetch() is         */
/*      sufficient.                                                     */
/* -------------------------------------------------------------------- */
    if( CPLGetLastErrorNo() != 0 )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                       EstablishRasterDetails()                       */
/*                                                                      */
/*      Do a "test" coverage query to work out the number of bands,     */
/*      and pixel data type of the remote coverage.                     */
/************************************************************************/

int WCSDataset::EstablishRasterDetails()

{
    CPLXMLNode * psCO = CPLGetXMLNode( psService, "CoverageOffering" );

    const char* pszCols = CPLGetXMLValue( psCO, "dimensionLimit.columns", NULL );
    const char* pszRows = CPLGetXMLValue( psCO, "dimensionLimit.rows", NULL );
    if( pszCols && pszRows )
    {
        nMaxCols = atoi(pszCols);
        nMaxRows = atoi(pszRows);
        SetMetadataItem("MAXNCOLS", pszCols, "IMAGE_STRUCTURE" );
        SetMetadataItem("MAXNROWS", pszRows, "IMAGE_STRUCTURE" );
    }

/* -------------------------------------------------------------------- */
/*      Do we already have bandcount and pixel type settings?           */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "BandCount", NULL ) != NULL 
        && CPLGetXMLValue( psService, "BandType", NULL ) != NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Fetch a small block of raster data.                             */
/* -------------------------------------------------------------------- */
    CPLHTTPResult *psResult = NULL;
    CPLErr eErr;
    
    eErr = GetCoverage( 0, 0, 2, 2, 2, 2, 0, NULL, &psResult );
    if( eErr != CE_None )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = GDALOpenResult( psResult );

    if( poDS == NULL )
        return FALSE;

    const char* pszPrj = poDS->GetProjectionRef();
    if( pszPrj && strlen(pszPrj) > 0 )
    {
        if( pszProjection )
            CPLFree( pszProjection );

        pszProjection = CPLStrdup( pszPrj );
    }

/* -------------------------------------------------------------------- */
/*      Record details.                                                 */
/* -------------------------------------------------------------------- */
    if( poDS->GetRasterCount() < 1 )
    {
        delete poDS;
        return FALSE;
    }
    
    if( CPLGetXMLValue(psService,"BandCount",NULL) == NULL )
        CPLCreateXMLElementAndValue( 
            psService, "BandCount", 
            CPLString().Printf("%d",poDS->GetRasterCount()));
    
    CPLCreateXMLElementAndValue( 
        psService, "BandType", 
        GDALGetDataTypeName(poDS->GetRasterBand(1)->GetRasterDataType()) );

    bServiceDirty = TRUE;
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    delete poDS;
    
    FlushMemoryResult();

    return TRUE;
}

/************************************************************************/
/*                         FlushMemoryResult()                          */
/*                                                                      */
/*      This actually either cleans up the in memory /vsimem/           */
/*      temporary file, or the on disk temporary file.                  */
/************************************************************************/
void WCSDataset::FlushMemoryResult()    
        
{
    if( strlen(osResultFilename) > 0 )
    {
        VSIUnlink( osResultFilename );
        osResultFilename = "";
    }

    if( pabySavedDataBuffer )
    {
        CPLFree( pabySavedDataBuffer );
        pabySavedDataBuffer = NULL;
    }
}

/************************************************************************/
/*                           GDALOpenResult()                           */
/*                                                                      */
/*      Open a CPLHTTPResult as a GDALDataset (if possible).  First     */
/*      attempt is to open handle it "in memory".  Eventually we        */
/*      will add support for handling it on file if necessary.          */
/*                                                                      */
/*      This method will free CPLHTTPResult, the caller should not      */
/*      access it after the call.                                       */
/************************************************************************/

GDALDataset *WCSDataset::GDALOpenResult( CPLHTTPResult *psResult )

{
    FlushMemoryResult();

    CPLDebug( "WCS", "GDALOpenResult() on content-type: %s",
              psResult->pszContentType );

/* -------------------------------------------------------------------- */
/*      If this is multipart/related content type, we should search     */
/*      for the second part.                                            */
/* -------------------------------------------------------------------- */
    GByte *pabyData = psResult->pabyData;
    int    nDataLen = psResult->nDataLen;

    if( psResult->pszContentType 
        && strstr(psResult->pszContentType,"multipart") 
        && CPLHTTPParseMultipartMime(psResult) )
    {
        if( psResult->nMimePartCount > 1 )
        {
            pabyData = psResult->pasMimePart[1].pabyData;
            nDataLen = psResult->pasMimePart[1].nDataLen;

            if (CSLFindString(psResult->pasMimePart[1].papszHeaders,
                              "Content-Transfer-Encoding: base64") != -1)
            {
                nDataLen = CPLBase64DecodeInPlace(pabyData);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a memory file from the result.                           */
/* -------------------------------------------------------------------- */
    // Eventually we should be looking at mime info and stuff to figure
    // out an optimal filename, but for now we just use a fixed one.
    osResultFilename.Printf( "/vsimem/wcs/%p/wcsresult.dat", 
                             this );

    VSILFILE *fp = VSIFileFromMemBuffer( osResultFilename, pabyData, nDataLen,
                                     FALSE );

    if( fp == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Try opening this result as a gdaldataset.                       */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = (GDALDataset *) 
        GDALOpen( osResultFilename, GA_ReadOnly );

/* -------------------------------------------------------------------- */
/*      If opening it in memory didn't work, perhaps we need to         */
/*      write to a temp file on disk?                                   */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        CPLString osTempFilename;
        VSILFILE *fpTemp;
        
        osTempFilename.Printf( "/tmp/%p_wcs.dat", this );
                               
        fpTemp = VSIFOpenL( osTempFilename, "wb" );
        if( fpTemp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to create temporary file:%s", 
                      osTempFilename.c_str() );
        }
        else
        {
            if( VSIFWriteL( pabyData, nDataLen, 1, fpTemp )
                != 1 )
            {
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "Failed to write temporary file:%s", 
                          osTempFilename.c_str() );
                VSIFCloseL( fpTemp );
                VSIUnlink( osTempFilename );
            }
            else
            {
                VSIFCloseL( fpTemp );
                VSIUnlink( osResultFilename );
                osResultFilename = osTempFilename;

                poDS =  (GDALDataset *) 
                    GDALOpen( osResultFilename, GA_ReadOnly );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Steal the memory buffer from HTTP result.                       */
/* -------------------------------------------------------------------- */
    pabySavedDataBuffer = psResult->pabyData;
        
    psResult->pabyData = NULL;
    psResult->nDataLen = psResult->nDataAlloc = 0;
    
    if( poDS == NULL )
        FlushMemoryResult();

    CPLHTTPDestroyResult(psResult);

    return poDS;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int WCSDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a WCS_GDAL service description file or "in url"         */
/*      equivelent?                                                     */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes == 0
        && EQUALN((const char *) poOpenInfo->pszFilename,"<WCS_GDAL>",10) )
        return TRUE;

    else if( poOpenInfo->nHeaderBytes >= 10
             && EQUALN((const char *) poOpenInfo->pabyHeader,"<WCS_GDAL>",10) )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Is this apparently a WCS subdataset reference?                  */
/* -------------------------------------------------------------------- */
    else if( EQUALN((const char *) poOpenInfo->pszFilename,"WCS_SDS:",8) 
             && poOpenInfo->nHeaderBytes == 0 )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *WCSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    char **papszModifiers = NULL;

/* -------------------------------------------------------------------- */
/*      Is this a WCS_GDAL service description file or "in url"         */
/*      equivelent?                                                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psService = NULL;

    if( poOpenInfo->nHeaderBytes == 0 
        && EQUALN((const char *) poOpenInfo->pszFilename,"<WCS_GDAL>",10) )
    {
        psService = CPLParseXMLString( poOpenInfo->pszFilename );
    }
    else if( poOpenInfo->nHeaderBytes >= 10
             && EQUALN((const char *) poOpenInfo->pabyHeader,"<WCS_GDAL>",10) )
    {
        psService = CPLParseXMLFile( poOpenInfo->pszFilename );
    }
/* -------------------------------------------------------------------- */
/*      Is this apparently a subdataset?                                */
/* -------------------------------------------------------------------- */
    else if( EQUALN((const char *) poOpenInfo->pszFilename,"WCS_SDS:",8) 
             && poOpenInfo->nHeaderBytes == 0 )
    {
        int iLast;

        papszModifiers = CSLTokenizeString2( poOpenInfo->pszFilename+8, ",",
                                             CSLT_HONOURSTRINGS );

        iLast = CSLCount(papszModifiers)-1;
        if( iLast >= 0 )
        {
            psService = CPLParseXMLFile( papszModifiers[iLast] );
            CPLFree( papszModifiers[iLast] );
            papszModifiers[iLast] = NULL;
        }

    }

/* -------------------------------------------------------------------- */
/*      Success so far?                                                 */
/* -------------------------------------------------------------------- */
    if( psService == NULL )
    {
        CSLDestroy( papszModifiers );
        return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CSLDestroy( papszModifiers );
        CPLDestroyXMLNode( psService );
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The WCS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Check for required minimum fields.                              */
/* -------------------------------------------------------------------- */
    if( !CPLGetXMLValue( psService, "ServiceURL", NULL )
        || !CPLGetXMLValue( psService, "CoverageName", NULL ) )
    {
        CSLDestroy( papszModifiers );
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Missing one or both of ServiceURL and CoverageName elements.\n"
                  "See WCS driver documentation for details on service description file format." );

        CPLDestroyXMLNode( psService );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What version are we working with?                               */
/* -------------------------------------------------------------------- */
    const char *pszVersion = CPLGetXMLValue( psService, "Version", "1.0.0" );
    int nVersion;

    if (EQUAL(pszVersion, "1.1.2") )
        nVersion = 112;
    else if( EQUAL(pszVersion,"1.1.1") )
        nVersion = 111;
    else if( EQUAL(pszVersion,"1.1.0") )
        nVersion = 110;
    else if( EQUAL(pszVersion,"1.0.0") )
        nVersion = 100;
    else
    {
        CSLDestroy( papszModifiers );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "WCS Version '%s' not supported.", pszVersion );
        CPLDestroyXMLNode( psService );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    WCSDataset 	*poDS;

    poDS = new WCSDataset();

    poDS->psService = psService;
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->nVersion = nVersion;
    poDS->papszSDSModifiers = papszModifiers;

/* -------------------------------------------------------------------- */
/*      Capture HTTP parameters.                                        */
/* -------------------------------------------------------------------- */
    const char  *pszParm;

    poDS->papszHttpOptions = 
        CSLSetNameValue(poDS->papszHttpOptions,
                        "TIMEOUT",
                        CPLGetXMLValue( psService, "Timeout", "30" ) );

    pszParm = CPLGetXMLValue( psService, "HTTPAUTH", NULL );
    if( pszParm )
        poDS->papszHttpOptions = 
            CSLSetNameValue( poDS->papszHttpOptions, 
                             "HTTPAUTH", pszParm );

    pszParm = CPLGetXMLValue( psService, "USERPWD", NULL );
    if( pszParm )
        poDS->papszHttpOptions = 
            CSLSetNameValue( poDS->papszHttpOptions, 
                             "USERPWD", pszParm );

/* -------------------------------------------------------------------- */
/*      If we don't have the DescribeCoverage result for this           */
/*      coverage, fetch it now.                                         */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psService, "CoverageOffering" ) == NULL 
        && CPLGetXMLNode( psService, "CoverageDescription" ) == NULL )
    {
        if( !poDS->DescribeCoverage() )
        {
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract coordinate system, grid size, and geotransform from     */
/*      the coverage description and/or service description             */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !poDS->ExtractGridInfo() )
    {
        delete poDS;
        return NULL;
    }
    
    if( !poDS->EstablishRasterDetails() )
    {
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int nBandCount = atoi(CPLGetXMLValue(psService,"BandCount","1"));
    int iBand;

    if (!GDALCheckBandCount(nBandCount, 0))
    {
        delete poDS;
        return NULL;
    }
     
    for( iBand = 0; iBand < nBandCount; iBand++ )
        poDS->SetBand( iBand+1, new WCSRasterBand( poDS, iBand+1, -1 ) );

/* -------------------------------------------------------------------- */
/*      Set time metadata on the dataset if we are selecting a          */
/*      temporal slice.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osTime = CSLFetchNameValueDef( poDS->papszSDSModifiers, "time", 
                                             poDS->osDefaultTime );
    
    if( osTime != "" )
        poDS->GDALMajorObject::SetMetadataItem( "TIME_POSITION", 
                                                osTime.c_str() );

/* -------------------------------------------------------------------- */
/*      Do we have a band identifier to select only a subset of bands?  */
/* -------------------------------------------------------------------- */
    poDS->osBandIdentifier = CPLGetXMLValue(psService,"BandIdentifier","");

/* -------------------------------------------------------------------- */
/*      Do we have time based subdatasets?  If so, record them in       */
/*      metadata.  Note we don't do subdatasets if this is a            */
/*      subdataset or if this is an all-in-memory service.              */
/* -------------------------------------------------------------------- */
    if( !EQUALN(poOpenInfo->pszFilename,"WCS_SDS:",8) 
        && !EQUALN(poOpenInfo->pszFilename,"<WCS_GDAL>",10) 
        && poDS->aosTimePositions.size() > 0 )
    {
        char **papszSubdatasets = NULL;
        int iTime;

        for( iTime = 0; iTime < (int)poDS->aosTimePositions.size(); iTime++ )
        {
            CPLString osName;
            CPLString osValue;
            
            osName.Printf( "SUBDATASET_%d_NAME", iTime+1 );
            osValue.Printf( "WCS_SDS:time=\"%s\",%s", 
                           poDS->aosTimePositions[iTime].c_str(), 
                           poOpenInfo->pszFilename );
            papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                osName, osValue );

            CPLString osCoverage = 
                CPLGetXMLValue( poDS->psService, "CoverageName", "" );

            osName.Printf( "SUBDATASET_%d_DESC", iTime+1 );
            osValue.Printf( "Coverage %s at time %s", 
                            osCoverage.c_str(), 
                            poDS->aosTimePositions[iTime].c_str() );
            papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                osName, osValue );
        }
        
        poDS->GDALMajorObject::SetMetadata( papszSubdatasets, 
                                            "SUBDATASETS" );
        
        CSLDestroy( papszSubdatasets );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();
    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr WCSDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return( CE_None );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *WCSDataset::GetProjectionRef()

{
    const char* pszPrj = GDALPamDataset::GetProjectionRef();
    if( pszPrj && strlen(pszPrj) > 0 )
        return pszPrj;

    if ( pszProjection && strlen(pszProjection) > 0 )
        return pszProjection;

    return( "" );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **WCSDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

/* -------------------------------------------------------------------- */
/*      ESRI also wishes to include service urls in the file list       */
/*      though this is not currently part of the general definition     */
/*      of GetFileList() for GDAL.                                      */
/* -------------------------------------------------------------------- */
#ifdef ESRI_BUILD
    CPLString file;
    file.Printf( "%s%s",
                 CPLGetXMLValue( psService, "ServiceURL", "" ),
                 CPLGetXMLValue( psService, "CoverageName", "" ) );
    papszFileList = CSLAddString( papszFileList, file.c_str() );
#endif /* def ESRI_BUILD */
    
    return papszFileList;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **WCSDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:CoverageOffering", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **WCSDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain == NULL
        || !EQUAL(pszDomain,"xml:CoverageOffering") )
        return GDALPamDataset::GetMetadata( pszDomain );

    
    CPLXMLNode *psNode = CPLGetXMLNode( psService, "CoverageOffering" );

    if( psNode == NULL )
        psNode = CPLGetXMLNode( psService, "CoverageDescription" );

    if( psNode == NULL )
        return NULL;

    if( apszCoverageOfferingMD[0] == NULL )
    {
        CPLXMLNode *psNext = psNode->psNext;
        psNode->psNext = NULL;

        apszCoverageOfferingMD[0] = CPLSerializeXMLTree( psNode );

        psNode->psNext = psNext;
    }

    return apszCoverageOfferingMD;
}


/************************************************************************/
/*                          GDALRegister_WCS()                        */
/************************************************************************/

void GDALRegister_WCS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "WCS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "WCS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "OGC Web Coverage Service" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_wcs.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        
        poDriver->pfnOpen = WCSDataset::Open;
        poDriver->pfnIdentify = WCSDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
