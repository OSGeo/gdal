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
 * Copyright (c) 2013, Even Rouault
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

CPL_CVSID("$Id$")

/************************
 * \brief Constructor
 ************************/
PostGISRasterTileRasterBand::PostGISRasterTileRasterBand(
    PostGISRasterTileDataset * poRTDSIn, int nBandIn,
    GDALDataType eDataTypeIn, GBool bIsOfflineIn) :
    bIsOffline(bIsOfflineIn),
    poSource(NULL)
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
    if (poBlock != NULL) {
        poBlock->DropLock();
        return true;
    }

    return false;
}

/*****************************************************
 * \brief Read a natural block of raster band data
 *****************************************************/
CPLErr PostGISRasterTileRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                               CPL_UNUSED int nBlockYOff,
                                               void * pImage)
{
    CPLString osCommand;
    PGresult * poResult = NULL;
    int nWKBLength = 0;

    int nPixelSize = GDALGetDataTypeSize(eDataType)/8;

    PostGISRasterTileDataset * poRTDS =
        (PostGISRasterTileDataset *)poDS;

    // Get by PKID
    if (poRTDS->poRDS->pszPrimaryKeyName)
    {
        //osCommand.Printf("select ST_AsBinary(st_band(%s, %d),TRUE) from %s.%s where "
        osCommand.Printf("select st_band(%s, %d) from %s.%s where "
            "%s = '%s'", poRTDS->poRDS->pszColumn, nBand, poRTDS->poRDS->pszSchema, poRTDS->poRDS->pszTable,
            poRTDS->poRDS->pszPrimaryKeyName, poRTDS->pszPKID);
    }

    // Get by upperleft
    else {
        osCommand.Printf("select st_band(%s, %d) from %s.%s where "
            "abs(ST_UpperLeftX(%s) - %.8f) < 1e-8 and abs(ST_UpperLeftY(%s) - %.8f) < 1e-8",
            poRTDS->poRDS->pszColumn, nBand, poRTDS->poRDS->pszSchema, poRTDS->poRDS->pszTable, poRTDS->poRDS->pszColumn,
            poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_X], poRTDS->poRDS->pszColumn,
            poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y]);
    }

    poResult = PQexec(poRTDS->poRDS->poConn, osCommand.c_str());

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster", "PostGISRasterTileRasterBand::IReadBlock(): "
             "Query = \"%s\" --> number of rows = %d",
             osCommand.c_str(), poResult ? PQntuples(poResult) : 0 );
#endif

    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0) {

        if (poResult)
            PQclear(poResult);

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error getting block of data (upperpixel = %f, %f)",
                poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_X],
                poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y]);

        return CE_Failure;
    }

    // TODO: Check this
    if (bIsOffline) {
        CPLError(CE_Failure, CPLE_AppDefined, "This raster has outdb "
            "storage. This feature isn't still available");

        PQclear(poResult);
        return CE_Failure;
    }

    /* Copy only data size, without payload */
    int nExpectedDataSize =
        nBlockXSize * nBlockYSize * nPixelSize;

    GByte * pbyData = CPLHexToBinary(PQgetvalue(poResult, 0, 0),
        &nWKBLength);
    int nExpectedWKBLength = RASTER_HEADER_SIZE + BAND_SIZE(nPixelSize, nExpectedDataSize);
    CPLErr eRet = CE_None;
    if( nWKBLength != nExpectedWKBLength )
    {
        CPLDebug("PostGIS_Raster", "nWKBLength=%d, nExpectedWKBLength=%d", nWKBLength, nExpectedWKBLength );
        eRet = CE_Failure;
    }
    else
    {
        GByte * pbyDataToRead =
        (GByte*)GET_BAND_DATA(pbyData,1, nPixelSize,
            nExpectedDataSize);

        // Do byte-swapping if necessary */
        int bIsLittleEndian = (pbyData[0] == 1);
#ifdef CPL_LSB
        int bSwap = !bIsLittleEndian;
#else
        int bSwap = bIsLittleEndian;
#endif
        if( bSwap && nPixelSize > 1 )
        {
            GDALSwapWords( pbyDataToRead, nPixelSize,
                           nBlockXSize * nBlockYSize,
                           nPixelSize );
        }

        memcpy(pImage, pbyDataToRead, nExpectedDataSize);
    }

    CPLFree(pbyData);
    PQclear(poResult);

    return eRet;
}
