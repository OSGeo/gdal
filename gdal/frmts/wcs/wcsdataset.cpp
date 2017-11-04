/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "gmlutils.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "gmlcoverage.h"

#include <algorithm>
#include <dirent.h>

#include "wcsdataset.h"
#include "wcsrasterband.h"
#include "wcsutils.h"

CPL_CVSID("$Id: wcsdataset.cpp 39343 2017-06-27 20:57:02Z rouault $")

/************************************************************************/
/* ==================================================================== */
/*                            WCSDataset                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             WCSDataset()                             */
/************************************************************************/

WCSDataset::WCSDataset(int version) :
    bServiceDirty(FALSE),
    psService(NULL),
    papszSDSModifiers(NULL),
    m_Version(version),
    pszProjection(NULL),
    native_crs(true),
    axis_order_swap(false),
    pabySavedDataBuffer(NULL),
    papszHttpOptions(NULL),
    nMaxCols(-1),
    nMaxRows(-1)
{
    m_Version = version;
    
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    apszCoverageOfferingMD[0] = NULL;
    apszCoverageOfferingMD[1] = NULL;
}

/************************************************************************/
/*                            ~WCSDataset()                             */
/************************************************************************/

WCSDataset::~WCSDataset()

{
    // perhaps this should be moved into a FlushCache() method.
    if( bServiceDirty && !STARTS_WITH_CI(GetDescription(), "<WCS_GDAL>") )
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
/*                           SetCRS()                                   */
/*                                                                      */
/*      Set the name and the WKT of the projection of this dataset.     */
/*      Based on the projection, sets the axis order flag.              */
/*      Also set the native flag.                                       */
/************************************************************************/

bool WCSDataset::SetCRS(CPLString crs, bool native)
{
    osCRS = crs;
    if (!CRSImpliesAxisOrderSwap(osCRS, axis_order_swap, &pszProjection)) {
        return false;
    }
    native_crs = native;
    return true;
}

/************************************************************************/
/*                           SetGeometry()                              */
/*                                                                      */
/*      Set GeoTransform and RasterSize from the coverage envelope,     */
/*      axis_order, grid size, and grid offsets.                        */
/************************************************************************/

void WCSDataset::SetGeometry(std::vector<double> envelope,
                             CPL_UNUSED std::vector<CPLString> axis_order,
                             std::vector<int> size,
                             CPL_UNUSED std::vector<std::vector<double>> offsets)
{

    // todo: handle offsets that are not ((x,0),(0,y))
    // todo: use domain_index
    
    nRasterXSize = size[0];
    nRasterYSize = size[1];
    
    adfGeoTransform[0] = envelope[0];
    adfGeoTransform[1] = (envelope[2]-envelope[0])/(double)nRasterXSize;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = envelope[3];
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = (envelope[1]-envelope[3])/(double)nRasterYSize;
    
    
}

/************************************************************************/
/*                           TestUseBlockIO()                           */
/*                                                                      */
/*      Check whether we should use blocked IO (true) or direct io      */
/*      (FALSE) for a given request configuration and environment.      */
/************************************************************************/

int WCSDataset::TestUseBlockIO( CPL_UNUSED int nXOff,
                                CPL_UNUSED int nYOff,
                                int nXSize,
                                int nYSize,
                                int nBufXSize,
                                int nBufYSize )
{
    int bUseBlockedIO = bForceCachedIO;

    if( nYSize == 1 || nXSize * ((double) nYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( nBufYSize == 1 || nBufXSize * ((double) nBufYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( bUseBlockedIO
        && CPLTestBool( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
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
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg)

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
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
    else
        return DirectRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
}

/************************************************************************/
/*                           DirectRasterIO()                           */
/*                                                                      */
/*      Make exactly one request to the server for this data.           */
/************************************************************************/

CPLErr
WCSDataset::DirectRasterIO( CPL_UNUSED GDALRWFlag eRWFlag,
                            int nXOff,
                            int nYOff,
                            int nXSize,
                            int nYSize,
                            void * pData,
                            int nBufXSize,
                            int nBufYSize,
                            GDALDataType eBufType,
                            int nBandCount,
                            int *panBandMap,
                            GSpacing nPixelSpace, GSpacing nLineSpace,
                            GSpacing nBandSpace,
                            CPL_UNUSED GDALRasterIOExtraArg* psExtraArg)
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
/*      Try and open result as a dataset.                               */
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

    if( (!osBandIdentifier.empty() && poTileDS->GetRasterCount() != nBandCount)
        || (osBandIdentifier.empty() && poTileDS->GetRasterCount() !=
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
    eErr = CE_None;

    for( int iBand = 0;
         iBand < nBandCount && eErr == CE_None;
         iBand++ )
    {
        GDALRasterBand *poTileBand = NULL;

        if( !osBandIdentifier.empty() )
            poTileBand = poTileDS->GetRasterBand( iBand + 1 );
        else
            poTileBand = poTileDS->GetRasterBand( panBandMap[iBand] );

        eErr = poTileBand->RasterIO( GF_Read,
                                     0, 0, nBufXSize, nBufYSize,
                                     ((GByte *) pData) +
                                     iBand * nBandSpace, nBufXSize, nBufYSize,
                                     eBufType, nPixelSpace, nLineSpace, NULL );
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
/* -------------------------------------------------------------------- */
/*      Figure out the georeferenced extents.                           */
/* -------------------------------------------------------------------- */
    std::vector<double> extent;

    // WCS 1.0 extents are the outer edges of outer pixels.
    extent.push_back(adfGeoTransform[0] +
                     (nXOff) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff + nYSize) * adfGeoTransform[5]);
    extent.push_back(adfGeoTransform[0] +
                     (nXOff + nXSize) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff) * adfGeoTransform[5]);
    

/* -------------------------------------------------------------------- */
/*      Build band list if we have the band identifier.                 */
/* -------------------------------------------------------------------- */
    CPLString osBandList;

    if( !osBandIdentifier.empty() && nBandCount > 0 && panBandList != NULL )
    {
        int iBand;

        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( iBand > 0 )
                osBandList += ",";
            osBandList += CPLString().Printf( "%d", panBandList[iBand] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Construct a KVP GetCoverage request.                            */
/* -------------------------------------------------------------------- */
    CPLString osRequest = GetCoverageRequest(nXOff, nYOff,
                                             nXSize, nYSize,
                                             nBufXSize, nBufYSize,
                                             extent, osBandList);

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

    CPLXMLNode *psDC = NULL;

    // if it is in cache, get it from there
    CPLString cache_dir;
    CPLString dc_filename = "";
    if (SetupCache(cache_dir, false)) {
        dc_filename = this->GetDescription(); // the WCS_GDAL file (<basename>.xml)
        dc_filename.erase(dc_filename.find(".xml"), 4);
        dc_filename += ".DC.xml";
        if (FileIsReadable(dc_filename)) {
            psDC = CPLParseXMLFile(dc_filename);
        }
    }

    if (!psDC) {
        osRequest = DescribeCoverageRequest();
        CPLErrorReset();
        CPLHTTPResult *psResult = CPLHTTPFetch( osRequest, papszHttpOptions );
        if( ProcessError( psResult ) ) {
            return FALSE;
        }
        
/* -------------------------------------------------------------------- */
/*      Parse result.                                                   */
/* -------------------------------------------------------------------- */

        psDC = CPLParseXMLString( (const char *) psResult->pabyData );
        CPLHTTPDestroyResult( psResult );
        if( psDC == NULL ) {
            return FALSE;
        }

        // if we have cache, put it there
        if (dc_filename != "") {
            CPLSerializeXMLTreeToFile(psDC, dc_filename);
        }
    }

    CPLStripXMLNamespace( psDC, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Did we get a CoverageOffering?                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCO = CoverageOffering(psDC);

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

        CPLStripXMLNamespace( psTree, NULL, TRUE );
        
        const char *pszMsg = CPLGetXMLValue(psTree, this->ExceptionNodeName(), NULL);

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

    return false;
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
        return false;

/* -------------------------------------------------------------------- */
/*      Try and open result as a dataset.                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = GDALOpenResult( psResult );

    if( poDS == NULL )
        return false;

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
        return false;
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
    if( !osResultFilename.empty() )
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
/*      Filename is WCS:URL                                             */
/*                                                                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes == 0
        && STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "WCS:") )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Is this a WCS_GDAL service description file or "in url"         */
/*      equivalent?                                                     */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes == 0
        && STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "<WCS_GDAL>") )
        return TRUE;

    else if( poOpenInfo->nHeaderBytes >= 10
             && STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader, "<WCS_GDAL>") )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Is this apparently a WCS subdataset reference?                  */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "WCS_SDS:")
             && poOpenInfo->nHeaderBytes == 0 )
        return TRUE;

    else
        return FALSE;
}

static int WCSParseVersion( const char *version )
{
    if( EQUAL(version, "2.0.1") )
        return 201;
    if( EQUAL(version, "1.1.2") )
        return 112;
    if( EQUAL(version, "1.1.1") )
        return 111;
    if( EQUAL(version, "1.1.0") )
        return 110;
    if( EQUAL(version, "1.0.0") )
        return 100;
    CPLError( CE_Failure, CPLE_AppDefined,
              "WCS Version '%s' not supported.", version );
    return 0;
}

const char *WCSDataset::Version()
{
    if( this->m_Version == 201 )
        return "2.0.1";
    if( this->m_Version == 112 )
        return "1.1.2";
    if( this->m_Version == 111 )
        return "1.1.1";
    if( this->m_Version == 110 )
        return "1.1.0";
    if( this->m_Version == 100 )
        return "1.0.0";
    return "";
}

WCSDataset *WCSDataset::CreateFromCapabilities(GDALOpenInfo * poOpenInfo, CPLString path, CPLString url)
{
    // request Capabilities, later code will write PAM to cache
    url = CPLURLAddKVP(url, "service", "WCS");
    url = CPLURLAddKVP(url, "request", "GetCapabilities");

    char **options = NULL;
    const char *keys2[] = {
        "Timeout",
        "UserPwd",
        "HttpAuth"
    };
    for (unsigned int i = 0; i < sizeof(keys2)/sizeof(keys2[0]); i++) {
        CPLString str = keys2[i];
        std::transform(str.begin(), str.end(),str.begin(), ::toupper);
        CPLString value = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, str, "");
        if (value != "") {
            options = CSLSetNameValue(options, str, value);
        }
    }
    CPLHTTPResult *psResult = CPLHTTPFetch(url.c_str(), options);
    if (psResult == NULL || psResult->nDataLen == 0) {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    CPLXMLNode *psService = CPLParseXMLString((const char*)psResult->pabyData);
    if (!psService) {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    psService = psService->psNext; // from ?XML to root
    // get version
    // it may be that the version in the URL in cache (user's request)
    // is different from the version in the XML (server's response)
    // but that is probably not a problem
    int version_from_server = 0;
    for (CPLXMLNode *node = psService->psChild; node != NULL; node = node->psNext) {
        const char *attr = node->pszValue;
        if (node->eType == CXT_Attribute && EQUAL(attr, "version")) {
            version_from_server = WCSParseVersion(CPLGetXMLValue(node, NULL, ""));
            if (version_from_server == 0) {
                return NULL;
            }
        }
    }
                              
    CPLString capabilities = path + ".xml";
    CPLSerializeXMLTreeToFile(psService, capabilities);

    WCSDataset *poDS;
    if (version_from_server == 201) {
        poDS = new WCSDataset201();
    } else if (version_from_server/10 == 11) {
        poDS = new WCSDataset110(version_from_server);
    } else {
        poDS = new WCSDataset100();
    }
    if (poDS->ParseCapabilities(psService) != CE_None) {
        poDS->ProcessError(psResult);
        delete poDS;
        return NULL;
    }
    CPLHTTPDestroyResult(psResult);
    CPLDestroyXMLNode(psService);
    poDS->SetDescription(path);
    return poDS;
}

WCSDataset *WCSDataset::CreateFromMetadata(CPLString path)
{
    WCSDataset *poDS;
    // try to read the PAM XML from path + metadata extension
    if (FileIsReadable(path + ".aux.xml")) {
        CPLXMLNode *metadata = CPLParseXMLFile((path + ".aux.xml").c_str());
        int version_from_metadata = WCSParseVersion(
            CPLGetXMLValue(metadata, "WCS_GLOBAL#version", NULL )
            );
        if (version_from_metadata == 201) {
            poDS = new WCSDataset201();
        } else if (version_from_metadata/10 == 11) {
            poDS = new WCSDataset110(version_from_metadata);
        } else {
            poDS = new WCSDataset100();
        }
        poDS->SetDescription(path);
        poDS->TryLoadXML(); // todo: avoid reload
    } else {
        // obviously there was an error
        // processing the Capabilities file
        // so we show it to the user
        GByte *pabyOut = NULL;
        path += ".xml";
        if( !VSIIngestFile( NULL, path, &pabyOut, NULL, -1 ) )
            return NULL;
        CPLString error = reinterpret_cast<char *>(pabyOut);
        if( error.size() > 2048 ) {
            error.resize( 2048 );
        }
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error:\n%s",
                  error.c_str() );
        return NULL;
    }
    return poDS;
}

static CPLXMLNode *CreateService(GDALOpenInfo * poOpenInfo,
                                 CPLString base_url,
                                 CPLString version,
                                 CPLString coverage)
{
    // construct WCS_GDAL XML into psService
    CPLString xml = "<WCS_GDAL>";
    xml += "<ServiceURL>" + base_url + "</ServiceURL>";
    xml += "<Version>" + version + "</Version>";
    xml += "<CoverageName>" + coverage + "</CoverageName>";
    const char *keys2[] = {
        "Timeout",
        "UserPwd",
        "HttpAuth",
    };
    for (unsigned int i = 0; i < sizeof(keys2)/sizeof(keys2[0]); i++) {
        CPLString str = keys2[i];
        std::transform(str.begin(), str.end(),str.begin(), ::toupper);
        CPLString value = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, str, "");
        if (value != "") {
            str = keys2[i];
            xml += "<" + str + ">" + value + "</" + str + ">";
        }
    }
    xml += "</WCS_GDAL>";
    CPLXMLNode *psService = CPLParseXMLString(xml);
    return psService;
}

static void UpdateService(CPLXMLNode *service, GDALOpenInfo * poOpenInfo, CPLString path)
{
    service = service->psChild;
    const char *keys2[] = {
        "NoGridCRS", // do not put GridCRS params into GetCoverage URL if not necessary (1.1)
        "CRS" // override native CRS, should be one of the supported ones
        "PreferredFormat", // option for format
        "Domain", // names for x and y dimensions, if not set uses the first two ("name,name")
        "Dimensions", // options for slicing/trimming ("name[,crs](begin,end);name[,crs](slice)")
                      // note that this is not for x/y
        "DimensionToBand", // to put non x/y dimension to a band (may require Range) ("name")
        "Interpolation",
        "Range" // what to put to bands, format as RANGESUBSET in GetCoverage ("name,name:name,")
    };
    for (unsigned int i = 0; i < sizeof(keys2)/sizeof(keys2[0]); i++) {
        CPLString str = keys2[i];
        std::transform(str.begin(), str.end(),str.begin(), ::toupper);
        CPLString value = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, str, "");
        if (value != "") {
            str = keys2[i];
            if (!CPLSetXMLValue(service, keys2[i], value)) {
                CPLCreateXMLElementAndValue(service, keys2[i], value);
            }
        }
    }
    // save it to cache
    // it will be updated later with data from DescribeCoverage
    CPLSerializeXMLTreeToFile(service, path);
    /*
      do not do this as it prevents fetching metadata
    CPLFree(poOpenInfo->pszFilename);
    poOpenInfo->pszFilename = CPLStrdup(path);
    */
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *WCSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    CPLXMLNode *psService = NULL;
    CPLXMLNode *metadata = NULL;
    char **papszModifiers = NULL;
    
/* -------------------------------------------------------------------- */
/*      If filename is WCS:URL                                          */
/*      We will set service and request the URL,                        */
/*      but version / acceptVersions is left for the user.              */
/*      The server *should* return the latest supported version         */
/*      but that is not dependable.                                     */
/*      If there is no coverage id/name, get capabilities.              */
/*      Otherwise, proceed to describe coverage / get coverage.         */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes == 0
        && STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "WCS:") )
    {
        CPLString url = (const char *)(poOpenInfo->pszFilename + 4);
        CPLString version = CPLURLGetValue(url, "version");
        CPLString coverage = CPLURLGetValue(url, "coverageId"); // 2.0
        if( coverage == "" )
        {
            coverage = CPLURLGetValue(url, "identifiers"); // 1.1
            if( coverage == "" )
            {
                coverage = CPLURLGetValue(url, "coverage"); // 1.0
            }
        }
        // the URL in cache is either with parameter version
        // or version and coverage name/identifier
        
        // remove all parameters, and possibly add version and coverage
        // coverage is always as 'coverageId'
        CPLString base_url = url.substr(0, url.find("?"));
        base_url += "?";
        url = base_url;
        if (version != "") {
            url += "version=" + version;
        }
        if (coverage != "") {
            if (version != "") {
                url += "&";
            }
            url += "coverage=" + coverage;
        }
        
        CPLString cache_dir = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CACHE_DIR", "");
        if (!SetupCache(cache_dir,
                        CPLFetchBool(poOpenInfo->papszOpenOptions, "CLEAR_CACHE", false)))
        {
            return NULL;
        }

        if (CPLFetchBool(poOpenInfo->papszOpenOptions, "REFRESH_CACHE", false)) {
            DeleteEntryFromCache(cache_dir, "", url);
        }

        // cache is a hash of basename => URL
        // below 'cached' means the URL is in cache
        // for basename there may be either
        // Capabilities (.xml) and PAM metadata file (.aux.xml)
        // or DescribeCoverage (.DC.xml) and WCS_GDAL (.xml)
        CPLString filename;
        int cached = FromCache(cache_dir, filename, url);
        if (cached == -1) { // error
            return NULL;
        }
        cached = cached && FileIsReadable(filename + ".xml");

        // the general policy for service documents in cache is
        // that they are not user editable
        // users should use options and possibly RECREATE_SERVICE

        // even if we have coverage we need PAM metadata
        // if the service file needs to be (re)created
        // if we don't have it, fetch it first
        bool recreate_service = CPLFetchBool(poOpenInfo->papszOpenOptions, "RECREATE_SERVICE", false);
        if (coverage != "" && (!cached || (cached && !recreate_service))) {
            // the filename of the metadata is the key of URL without coverage
            CPLString pam_url = URLRemoveKey(url, "coverageId");
            CPLString pam_filename;
            if (!FromCache(cache_dir, pam_filename, pam_url)) {
                WCSDataset *pam = CreateFromCapabilities(poOpenInfo, pam_filename, pam_url);
                if (!pam) {
                    return NULL;
                }
            }
            //metadata = CPLParseXMLFile((pam_filename + ".aux.xml").c_str());
            // metadata is put into the new dataset once we create it below
            CPLFree(poOpenInfo->pszFilename);
            poOpenInfo->pszFilename = CPLStrdup(pam_filename);
        }
        
        if (coverage == "") {
            // Open a dataset with subdataset(s)
            // Information is in PAM file (<basename>.aux.xml)
            // which is made from the Capabilities file (<basename>.xml)
            if (cached && !CPLFetchBool(poOpenInfo->papszOpenOptions, "RECREATE_META", false)) {
                return WCSDataset::CreateFromMetadata(filename);
            }
            return WCSDataset::CreateFromCapabilities(poOpenInfo, filename, url);
        } else {
            // Open a subdataset
            // Information is in WCS_GDAL file (<basename>.xml)
            // which is made from options and URL
            // and a Coverage description file (<basename>.DC.xml)
            filename += ".xml";
            if (cached && !recreate_service) {
                // read from cache
                psService = CPLParseXMLFile(filename);
                UpdateService(psService, poOpenInfo, filename);
                /*
                CPLFree(poOpenInfo->pszFilename);
                poOpenInfo->pszFilename = CPLStrdup(filename);
                */
            } else {
                psService = CreateService(poOpenInfo, base_url, version, coverage);
                UpdateService(psService, poOpenInfo, filename);
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Is this a WCS_GDAL service description file or "in url"         */
/*      equivalent?                                                     */
/* -------------------------------------------------------------------- */
    else if( poOpenInfo->nHeaderBytes == 0
             && STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "<WCS_GDAL>") )
    {
        psService = CPLParseXMLString( poOpenInfo->pszFilename );
    }
    else if( poOpenInfo->nHeaderBytes >= 10
             && STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader, "<WCS_GDAL>") )
    {
        psService = CPLParseXMLFile( poOpenInfo->pszFilename );
    }
/* -------------------------------------------------------------------- */
/*      Is this apparently a subdataset?                                */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH_CI((const char *) poOpenInfo->pszFilename, "WCS_SDS:")
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
        CPLDestroyXMLNode( psService );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Missing one or both of ServiceURL and CoverageName elements.\n"
                  "See WCS driver documentation for details on service description file format." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What version are we working with?                               */
/* -------------------------------------------------------------------- */
    const char *pszVersion = CPLGetXMLValue( psService, "Version", "1.0.0" );

    int nVersion = WCSParseVersion(pszVersion);

    if( nVersion == 0 )
    {
        CSLDestroy( papszModifiers );
        CPLDestroyXMLNode( psService );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    // todo: at this point version could be from user, is that a problem?
    WCSDataset *poDS;
    if (nVersion == 201) {
        poDS = new WCSDataset201();
    } else if (nVersion/10 == 11) {
        poDS = new WCSDataset110(nVersion);
    } else {
        poDS = new WCSDataset100();
    }

    poDS->psService = psService;
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->papszSDSModifiers = papszModifiers;
    if (metadata) {
        poDS->XMLInit(metadata, NULL);
    }
    poDS->TryLoadXML(); // we need the PAM metadata already in ExtractGridInfo

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
    if (!poDS->ExtractGridInfo()) {
        delete poDS;
        return NULL;
    }

    if( !poDS->EstablishRasterDetails() ) // todo: do this only if missing info
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
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "WCS_SDS:")
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "<WCS_GDAL>")
        && !poDS->aosTimePositions.empty() )
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
    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr WCSDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
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

    return "";
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
/*                          GDALRegister_WCS()                          */
/************************************************************************/

void GDALRegister_WCS()

{
    if( GDALGetDriverByName( "WCS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "WCS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OGC Web Coverage Service" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_wcs.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->pfnOpen = WCSDataset::Open;
    poDriver->pfnIdentify = WCSDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
