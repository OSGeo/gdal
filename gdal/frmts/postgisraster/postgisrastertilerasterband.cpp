/***********************************************************************
 * File :    postgisrastertilerasterband.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL Tile RasterBand implementation for PostGIS Raster
 * driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 * Last changes: $Id$
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo
 * Copyright (c) 2013-2018, Even Rouault
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **********************************************************************/
#include "postgisraster.h"
#include <memory>

CPL_CVSID("$Id$")

/************************
 * \brief Constructor
 ************************/
PostGISRasterTileRasterBand::PostGISRasterTileRasterBand(
    PostGISRasterTileDataset * poRTDSIn, int nBandIn,
    GDALDataType eDataTypeIn) :
    poSource(nullptr)
{
    // Basic properties.
    poDS = poRTDSIn;
    nBand = nBandIn;

#if 0
    CPLDebug("PostGIS_Raster",
        "PostGISRasterTileRasterBand::Constructor: Raster tile dataset "
        "of dimensions %dx%d", poRTDS->GetRasterXSize(),
        poRTDS->GetRasterYSize());
#endif

    eDataType = eDataTypeIn;

    nRasterXSize = poRTDSIn->GetRasterXSize();
    nRasterYSize = poRTDSIn->GetRasterYSize();

    nBlockXSize = nRasterXSize;
    nBlockYSize = nRasterYSize;
}

/************************
 * \brief Destructor
 ************************/
PostGISRasterTileRasterBand::~PostGISRasterTileRasterBand()
{
}

/***********************************************************************
 * \brief Returns true if the (only) block is stored in the cache
 **********************************************************************/
GBool PostGISRasterTileRasterBand::IsCached()
{
    GDALRasterBlock * poBlock = TryGetLockedBlockRef(0, 0);
    if (poBlock != nullptr) {
        poBlock->DropLock();
        return true;
    }

    return false;
}

/*****************************************************
 * \brief Read a natural block of raster band data
 *****************************************************/
CPLErr PostGISRasterTileRasterBand::IReadBlock(int /*nBlockXOff*/,
                                               int /*nBlockYOff*/,
                                               void * pImage)
{
    CPLString osCommand;
    PGresult * poResult = nullptr;
    int nWKBLength = 0;

    const int nPixelSize = GDALGetDataTypeSizeBytes(eDataType);

    PostGISRasterTileDataset * poRTDS =
        cpl::down_cast<PostGISRasterTileDataset *>(poDS);

    const double dfTileUpperLeftX = poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_X];
    const double dfTileUpperLeftY = poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
    const double dfTileResX = poRTDS->adfGeoTransform[1];
    const double dfTileResY = poRTDS->adfGeoTransform[5];
    const int nTileXSize = nBlockXSize;
    const int nTileYSize = nBlockYSize;

    CPLString osSchemaI(CPLQuotedSQLIdentifier(poRTDS->poRDS->pszSchema));
    CPLString osTableI(CPLQuotedSQLIdentifier(poRTDS->poRDS->pszTable));
    CPLString osColumnI(CPLQuotedSQLIdentifier(poRTDS->poRDS->pszColumn));

    CPLString osRasterToFetch;
    osRasterToFetch.Printf("ST_Band(%s, %d)",
                           osColumnI.c_str(), nBand);
    // We don't honour CLIENT_SIDE_IF_POSSIBLE since it would be likely too
    // costly in that context.
    if( poRTDS->poRDS->eOutDBResolution != OutDBResolution::CLIENT_SIDE )
    {
        osRasterToFetch = "encode(ST_AsBinary(" + osRasterToFetch + ",TRUE),'hex')";
    }

    osCommand.Printf("SELECT %s FROM %s.%s WHERE ",
        osRasterToFetch.c_str(), osSchemaI.c_str(), osTableI.c_str());

    // Get by PKID
    if (poRTDS->poRDS->pszPrimaryKeyName)
    {
        CPLString osPrimaryKeyNameI(CPLQuotedSQLIdentifier(poRTDS->poRDS->pszPrimaryKeyName));
        osCommand += CPLSPrintf("%s = '%s'",
                        osPrimaryKeyNameI.c_str(), poRTDS->pszPKID);
    }

    // Get by upperleft
    else {
        osCommand += CPLSPrintf(
            "abs(ST_UpperLeftX(%s) - %.8f) < 1e-8 and abs(ST_UpperLeftY(%s) - %.8f) < 1e-8",
            osColumnI.c_str(),
            dfTileUpperLeftX,
            osColumnI.c_str(),
            dfTileUpperLeftY);
    }

    poResult = PQexec(poRTDS->poRDS->poConn, osCommand.c_str());

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster", "PostGISRasterTileRasterBand::IReadBlock(): "
             "Query = \"%s\" --> number of rows = %d",
             osCommand.c_str(), poResult ? PQntuples(poResult) : 0 );
#endif

    if (poResult == nullptr ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0) {

        CPLString osError;
        if( PQresultStatus(poResult) == PGRES_FATAL_ERROR )
        {
            const char *pszError = PQerrorMessage( poRTDS->poRDS->poConn );
            if( pszError )
                osError = pszError;
        }
        if (poResult)
            PQclear(poResult);

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error getting block of data (upperpixel = %f, %f): %s",
                dfTileUpperLeftX,
                dfTileUpperLeftY,
                osError.c_str());

        return CE_Failure;
    }

    /* Copy only data size, without payload */
    int nExpectedDataSize =
        nBlockXSize * nBlockYSize * nPixelSize;

    struct CPLFreer { void operator() (GByte* x) const { CPLFree(x); } };
    std::unique_ptr<GByte, CPLFreer> pbyDataAutoFreed(
        CPLHexToBinary(PQgetvalue(poResult, 0, 0), &nWKBLength));
    GByte* pbyData = pbyDataAutoFreed.get();
    PQclear(poResult);

    const int nMinimumWKBLength = RASTER_HEADER_SIZE + BAND_SIZE(1, nPixelSize);
    if( nWKBLength < nMinimumWKBLength )
    {
        CPLDebug("PostGIS_Raster", "nWKBLength=%d. too short. Expected at least %d",
                 nWKBLength, nMinimumWKBLength );
        return CE_Failure;
    }

    // Is it indb-raster ?
    if( (pbyData[RASTER_HEADER_SIZE] & 0x80) == 0 )
    {
        int nExpectedWKBLength = RASTER_HEADER_SIZE +
                                 BAND_SIZE(nPixelSize, nExpectedDataSize);
        if( nWKBLength != nExpectedWKBLength )
        {
            CPLDebug("PostGIS_Raster",
                     "nWKBLength=%d, nExpectedWKBLength=%d",
                     nWKBLength, nExpectedWKBLength );
            return CE_Failure;
        }

        GByte * pbyDataToRead = GET_BAND_DATA(pbyData,1,
                                              nPixelSize,nExpectedDataSize);

        // Do byte-swapping if necessary */
        const bool bIsLittleEndian = (pbyData[0] == 1);
#ifdef CPL_LSB
        const bool bSwap = !bIsLittleEndian;
#else
        const bool bSwap = bIsLittleEndian;
#endif

        if( bSwap && nPixelSize > 1 )
        {
            GDALSwapWords( pbyDataToRead, nPixelSize,
                           nBlockXSize * nBlockYSize,
                           nPixelSize );
        }

        memcpy(pImage, pbyDataToRead, nExpectedDataSize);
    }
    else
    {
        int nCurOffset = RASTER_HEADER_SIZE;
        if( !poRTDS->poRDS->LoadOutdbRaster(nCurOffset, eDataType, nBand,
                                             pbyData,
                                             nWKBLength,
                                             pImage,
                                             dfTileUpperLeftX,
                                             dfTileUpperLeftY,
                                             dfTileResX,
                                             dfTileResY,
                                             nTileXSize,
                                             nTileYSize) )
        {
            return CE_Failure;
        }
    }


    return CE_None;
}
