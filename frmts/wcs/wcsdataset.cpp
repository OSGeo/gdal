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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.6  2007/01/02 05:44:23  fwarmerdam
 * Added "optimized" RasterIO() method implementations.
 *
 * Revision 1.5  2006/11/28 03:23:51  fwarmerdam
 * various improvements to error trapping
 *
 * Revision 1.4  2006/11/20 18:30:02  fwarmerdam
 * Use %.15g for BBOX values.
 *
 * Revision 1.3  2006/11/03 20:14:03  fwarmerdam
 * Support writing results to a temp file when in-memory does not work.
 *
 * Revision 1.2  2006/11/02 02:39:50  fwarmerdam
 * enable online help pointer
 *
 * Revision 1.1  2006/10/27 02:15:56  fwarmerdam
 * New
 *
 */

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

    CPLString   osCRS;

    char        *pszProjection;
    double      adfGeoTransform[6];

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    int		DescribeCoverage();
    int         ExtractGridInfo();
    int         EstablishRasterDetails();

    int         ProcessError( CPLHTTPResult *psResult );
    GDALDataset *GDALOpenResult( CPLHTTPResult *psResult );
    void        FlushMemoryResult();
    CPLString   osResultFilename;
    
  public:
                WCSDataset();
                ~WCSDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
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
    double         adfGeoTransform[6];

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
/*      Establish possibly reduced resolution geotransform.             */
/* -------------------------------------------------------------------- */
    adfGeoTransform[0] = poODS->adfGeoTransform[0];
    adfGeoTransform[1] = poODS->adfGeoTransform[1] * nResFactor;
    adfGeoTransform[2] = poODS->adfGeoTransform[2] * nResFactor;
    adfGeoTransform[3] = poODS->adfGeoTransform[3];
    adfGeoTransform[4] = poODS->adfGeoTransform[4] * nResFactor;
    adfGeoTransform[5] = poODS->adfGeoTransform[5] * nResFactor;

/* -------------------------------------------------------------------- */
/*      If this is the base layer, create the overview layers.          */
/* -------------------------------------------------------------------- */
    if( iOverview == -1 )
    {
        int i;

        nOverviewCount = atoi(CPLGetXMLValue(poODS->psService,"OverviewCount",
                                             "-1"));
        if( nOverviewCount == -1 )
        {
            for( nOverviewCount = 0; 
                 (MAX(nRasterXSize,nRasterYSize) / (1 << nOverviewCount)) > 900;
                 nOverviewCount++ ) {}
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
/* -------------------------------------------------------------------- */
/*      Figure out the georeferenced extents.                           */
/* -------------------------------------------------------------------- */
    double dfMinX, dfMaxX, dfMinY, dfMaxY;
    
    dfMinX = adfGeoTransform[0] + 
        (nBlockXOff * nBlockXSize + 0.5) * adfGeoTransform[1];
    dfMaxX = adfGeoTransform[0] + 
        ((nBlockXOff+1) * nBlockXSize + 0.5) * adfGeoTransform[1];
    dfMaxY = adfGeoTransform[3] + 
        (nBlockYOff * nBlockYSize + 0.5) * adfGeoTransform[5];
    dfMinY = adfGeoTransform[3] + 
        ((nBlockYOff+1) * nBlockYSize + 0.5) * adfGeoTransform[5];
    

/* -------------------------------------------------------------------- */
/*      Construct a simple GetCoverage request.                         */
/* -------------------------------------------------------------------- */
    CPLString osRequest;

    osRequest.Printf( 
        "%sSERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE=%s"
        "&FORMAT=%s&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=%d&HEIGHT=%d&CRS=%s",
        CPLGetXMLValue( poODS->psService, "ServiceURL", "" ),
        CPLGetXMLValue( poODS->psService, "CoverageName", "" ),
        CPLGetXMLValue( poODS->psService, "PreferredFormat", "" ),
        dfMinX, dfMinY, dfMaxX, dfMaxY,
        nBlockXSize, nBlockYSize,
        poODS->osCRS.c_str() ); // maxy

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLString osTimeout = "TIMEOUT=";
    osTimeout += CPLGetXMLValue( poODS->psService, "Timeout", "30" );
    char *apszOptions[] = { 
        (char *) osTimeout.c_str(),
        NULL 
    };

    CPLErrorReset();
    
    CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, apszOptions );

    if( poODS->ProcessError( psResult ) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poTileDS = poODS->GDALOpenResult( psResult );

    if( poTileDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Verify configuration.                                           */
/* -------------------------------------------------------------------- */
    if( poTileDS->GetRasterCount() != poODS->GetRasterCount()
        || poTileDS->GetRasterXSize() != nBlockXSize
        || poTileDS->GetRasterYSize() != nBlockYSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected configuration." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process all bands of memory result, copying into pBuffer, or    */
/*      pushing into cache for other bands.                             */
/* -------------------------------------------------------------------- */
    int iBand;
    CPLErr eErr = CE_None;
    
    for( iBand = 0; 
         iBand < poTileDS->GetRasterCount() && eErr == CE_None; 
         iBand++ )
    {
        GDALRasterBand *poTileBand = poTileDS->GetRasterBand( iBand+1 );

        if( iBand+1 == GetBand() )
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

            eErr = poTileBand->RasterIO( GF_Read, 
                                         0, 0, nBlockXSize, nBlockYSize, 
                                         poBlock->GetDataRef(), 
                                         nBlockXSize, nBlockYSize, 
                                         eDataType, 0, 0 );
            poBlock->DropLock();
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
    // Try optimized paths through dataset level rasterio.

    return poDS->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, 
                           nBufXSize, nBufYSize, eBufType, 
                           1, &nBand, nPixelSpace, nLineSpace, 0 );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double WCSRasterBand::GetNoDataValue( int *pbSuccess )

{
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
}

/************************************************************************/
/*                            ~WCSDataset()                             */
/************************************************************************/

WCSDataset::~WCSDataset()

{
    // perhaps this should be moved into a FlushCache() method.
    if( bServiceDirty )
    {
        CPLSerializeXMLTreeToFile( psService, GetDescription() );
        bServiceDirty = FALSE;
    }

    CPLDestroyXMLNode( psService );

    CPLFree( pszProjection );
    pszProjection = NULL;

    FlushMemoryResult();
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
/* -------------------------------------------------------------------- */
/*      We need various criteria to skip out to block based methods.    */
/* -------------------------------------------------------------------- */
    int bUseBlockedIO = bForceCachedIO;

    if( nYSize == 1 || nXSize * ((double) nYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( nBufYSize == 1 || nBufXSize * ((double) nBufYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( CSLTestBoolean( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
        bUseBlockedIO = FALSE;

    if( bUseBlockedIO )
        return GDALDataset::BlockBasedRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );

/* -------------------------------------------------------------------- */
/*      Figure out the georeferenced extents.                           */
/* -------------------------------------------------------------------- */
    double dfMinX, dfMaxX, dfMinY, dfMaxY;
    
    dfMinX = adfGeoTransform[0] + 
        (nXOff + 0.5) * adfGeoTransform[1];
    dfMaxX = adfGeoTransform[0] + 
        (nXOff+nXSize + 0.5) * adfGeoTransform[1];
    dfMaxY = adfGeoTransform[3] + 
        (nYOff + 0.5) * adfGeoTransform[5];
    dfMinY = adfGeoTransform[3] + 
        (nYOff + nYSize + 0.5) * adfGeoTransform[5];

/* -------------------------------------------------------------------- */
/*      Construct a simple GetCoverage request.                         */
/* -------------------------------------------------------------------- */
    CPLString osRequest;

    osRequest.Printf( 
        "%sSERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE=%s"
        "&FORMAT=%s&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=%d&HEIGHT=%d&CRS=%s",
        CPLGetXMLValue( psService, "ServiceURL", "" ),
        CPLGetXMLValue( psService, "CoverageName", "" ),
        CPLGetXMLValue( psService, "PreferredFormat", "" ),
        dfMinX, dfMinY, dfMaxX, dfMaxY,
        nBufXSize, nBufYSize,
        osCRS.c_str() ); 

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLString osTimeout = "TIMEOUT=";
    osTimeout += CPLGetXMLValue( psService, "Timeout", "30" );
    char *apszOptions[] = { 
        (char *) osTimeout.c_str(),
        NULL 
    };

    CPLErrorReset();
    
    CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, apszOptions );

    if( ProcessError( psResult ) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poTileDS = GDALOpenResult( psResult );

    if( poTileDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Verify configuration.                                           */
/* -------------------------------------------------------------------- */
    if( poTileDS->GetRasterCount() != GetRasterCount()
        || poTileDS->GetRasterXSize() != nBufXSize
        || poTileDS->GetRasterYSize() != nBufYSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Returned tile does not match expected configuration." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process all bands of memory result, copying into pBuffer, or    */
/*      pushing into cache for other bands.                             */
/* -------------------------------------------------------------------- */
    int iBand;
    CPLErr eErr = CE_None;
    
    for( iBand = 0; 
         iBand < nBandCount && eErr == CE_None; 
         iBand++ )
    {
        GDALRasterBand *poTileBand = 
            poTileDS->GetRasterBand( panBandMap[iBand] );

        eErr = poTileBand->RasterIO( GF_Read, 
                                     0, 0, nBufXSize, nBufYSize,
                                     ((GByte *) pData) + 
                                     (panBandMap[iBand]-1) * nBandSpace, 
                                     nBufXSize, nBufYSize, 
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
    osRequest.Printf( 
        "%sSERVICE=WCS&VERSION=1.0.0&REQUEST=DescribeCoverage&COVERAGE=%s", 
        CPLGetXMLValue( psService, "ServiceURL", "" ),
        CPLGetXMLValue( psService, "CoverageName", "" ) );

    CPLErrorReset();
    
    CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, NULL );

    if( ProcessError( psResult ) )
        return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Parse result.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDC = CPLParseXMLString( (const char *) psResult->pabyData );
    CPLHTTPDestroyResult( psResult );

    if( psDC == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Did we get a CoverageOffering?                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCO = 
        CPLGetXMLNode( psDC, "=CoverageDescription.CoverageOffering" );

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
/*                          ExtractGridInfo()                           */
/************************************************************************/

int WCSDataset::ExtractGridInfo()

{
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
/*      Build CRS name to use.                                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    const char *pszAuth;

    if( pszProjection && strlen(pszProjection) > 0 )
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
        CPLString osPreferredFormat;

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
        const char *pszSV = CPLGetXMLValue( psCO, "rangeSet.RangeSet.nullValues.singleValue", NULL );
        
        if( pszSV != NULL && (atof(pszSV) != 0.0 || *pszSV == '0') )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "NoDataValue", 
                                         pszSV );
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
/*      In this case we can presume the error was already issued by     */
/*      CPLHTTPFetch().                                                 */
/* -------------------------------------------------------------------- */
    if( psResult == NULL || psResult->nDataLen == 0 
        || CPLGetLastErrorNo() != 0 )
    {
        CPLHTTPDestroyResult( psResult );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Does this look like a service exception?  We would like to      */
/*      check based on the Content-type, but this seems quite           */
/*      undependable, even from MapServer!                              */
/* -------------------------------------------------------------------- */
    if( strstr((const char *)psResult->pabyData, "<ServiceException") )
    {
        CPLXMLNode *psTree = CPLParseXMLString( (const char *) 
                                                psResult->pabyData );
        const char *pszMsg = NULL;

        if( psTree != NULL )
            pszMsg = CPLGetXMLValue(psTree,
                                    "=ServiceExceptionReport.ServiceException",
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
/* -------------------------------------------------------------------- */
/*      Do we already have bandcount and pixel type settings?           */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "BandCount", NULL ) != NULL 
        && CPLGetXMLValue( psService, "BandType", NULL ) != NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Construct a simple GetCoverage request.                         */
/* -------------------------------------------------------------------- */
    CPLString osRequest;

    osRequest.Printf( 
        "%sSERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE=%s"
        "&FORMAT=%s&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=2&HEIGHT=2&CRS=%s",
        CPLGetXMLValue( psService, "ServiceURL", "" ),
        CPLGetXMLValue( psService, "CoverageName", "" ),
        CPLGetXMLValue( psService, "PreferredFormat", "" ),
        adfGeoTransform[0] + 0.5 * adfGeoTransform[1], // minx
        adfGeoTransform[3] + 1.5 * adfGeoTransform[5], // miny
        adfGeoTransform[0] + 1.5 * adfGeoTransform[1], // maxx
        adfGeoTransform[3] + 0.5 * adfGeoTransform[5],
        osCRS.c_str() ); // maxy

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, NULL );

    if( ProcessError( psResult ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataseat.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = GDALOpenResult( psResult );

    if( poDS == NULL )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Record details.                                                 */
/* -------------------------------------------------------------------- */
    if( poDS->GetRasterCount() < 1 )
        return FALSE;
    
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

/* -------------------------------------------------------------------- */
/*      Create a memory file from the result.                           */
/* -------------------------------------------------------------------- */
    // Eventually we should be looking at mime info and stuff to figure
    // out an optimal filename, but for now we just use a fixed one.
    osResultFilename.Printf( "/vsimem/wcs/%p/wcsresult.dat", 
                             this );

    FILE *fp = VSIFileFromMemBuffer( osResultFilename, 
                                     psResult->pabyData, 
                                     psResult->nDataLen, 
                                     TRUE );

    if( fp == NULL )
        return NULL;

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
        FILE *fpTemp;
        
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
            if( VSIFWriteL( psResult->pabyData, psResult->nDataLen, 1, fpTemp )
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
    psResult->pabyData = NULL;
    psResult->nDataLen = psResult->nDataAlloc = 0;

    if( poDS == NULL )
        FlushMemoryResult();

    return poDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *WCSDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a WCS_GDAL service description file?                    */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 10
        || !EQUALN((const char *) poOpenInfo->pabyHeader,"<WCS_GDAL>",10) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read and parse the service description file.                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psService = CPLParseXMLFile( poOpenInfo->pszFilename );
    if( psService == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for required minimum fields.                              */
/* -------------------------------------------------------------------- */
    if( !CPLGetXMLValue( psService, "ServiceURL", NULL )
        || !CPLGetXMLValue( psService, "CoverageName", NULL ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Missing one or both of ServiceURL and CoverageName elements.\n"
                  "See WCS driver documentation for details on service description file format." );

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

/* -------------------------------------------------------------------- */
/*      If we don't have the DescribeCoverage result for this           */
/*      coverage, fetch it now.                                         */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psService, "CoverageOffering" ) == NULL )
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
     
    for( iBand = 0; iBand < nBandCount; iBand++ )
        poDS->SetBand( iBand+1, new WCSRasterBand( poDS, iBand+1, -1 ) );

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
    if( pszProjection )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
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
        
        poDriver->pfnOpen = WCSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
