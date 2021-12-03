/***********************************************************************
 * File :    postgisrasterrasterband.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL RasterBand implementation for PostGIS Raster driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Author:       David Zwarg, dzwarg@azavea.com
 *
 * Last changes: $Id$
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo, David Zwarg
 * Copyright (c) 2013-2018, Even Rouault <even.rouault at spatialys.com>
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

/**
 * \brief Constructor.
 *
 * nBand it is just necessary for overview band creation
 */
PostGISRasterRasterBand::PostGISRasterRasterBand(
    PostGISRasterDataset * poDSIn, int nBandIn,
    GDALDataType eDataTypeIn, GBool bNoDataValueSetIn, double dfNodata) :
    VRTSourcedRasterBand(poDSIn, nBandIn),
    pszSchema(poDSIn->pszSchema),
    pszTable(poDSIn->pszTable),
    pszColumn(poDSIn->pszColumn)
{
    /* Basic properties */
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eDataTypeIn;
    m_bNoDataValueSet = bNoDataValueSetIn;
    m_dfNoDataValue = dfNodata;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();

    /*******************************************************************
     * Finally, set the block size. We apply the same logic than in VRT
     * driver.
     *
     * We limit the size of a block with MAX_BLOCK_SIZE here to prevent
     * arrangements of just one big tile.
     *
     * This value is just used in case we only have 1 tile in the
     * table. Otherwise, the reading operations are performed by the
     * sources, not the PostGISRasterBand object itself.
     ******************************************************************/
    nBlockXSize = atoi(CPLGetConfigOption("PR_BLOCKXSIZE",
                    CPLSPrintf("%d",MIN(MAX_BLOCK_SIZE, this->nRasterXSize))));
    nBlockYSize = atoi(CPLGetConfigOption("PR_BLOCKYSIZE",
                    CPLSPrintf("%d",MIN(MAX_BLOCK_SIZE, this->nRasterYSize))));

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterRasterBand constructor: Band size: (%d X %d)",
        nRasterXSize, nRasterYSize);

    CPLDebug("PostGIS_Raster", "PostGISRasterRasterBand::Constructor: "
        "Block size (%dx%d)", this->nBlockXSize, this->nBlockYSize);
#endif
}

/***********************************************
 * \brief: Band destructor
 ***********************************************/
PostGISRasterRasterBand::~PostGISRasterRasterBand() {}

/********************************************************
 * \brief Set nodata value to a buffer
 ********************************************************/
void PostGISRasterRasterBand::NullBuffer(void* pData,
                                         int nBufXSize,
                                         int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace,
                                         int nLineSpace)
{
    int j;
    for(j = 0; j < nBufYSize; j++)
    {
        double dfVal = 0.0;
        if( m_bNoDataValueSet )
            dfVal = m_dfNoDataValue;
        GDALCopyWords(&dfVal, GDT_Float64, 0,
                    static_cast<GByte*>(pData) + j * nLineSpace, eBufType, nPixelSpace,
                    nBufXSize);
    }
}

/********************************************************
 * \brief SortTilesByPKID
 ********************************************************/
static int SortTilesByPKID(const void* a, const void* b)
{
    const PostGISRasterTileDataset* pa = *static_cast<const PostGISRasterTileDataset* const*>(a);
    const PostGISRasterTileDataset* pb = *static_cast<const PostGISRasterTileDataset* const*>(b);
    return strcmp(pa->GetPKID(), pb->GetPKID());
}

/**
 * Read/write a region of image data for this band.
 *
 * This method allows reading a region of a PostGISRasterBand into a buffer.
 * The write support is still under development
 *
 * The function fetches all the raster data that intersects with the region
 * provided, and store the data in the GDAL cache.
 *
 * It automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the PostGISRasterRasterBand.
 *
 * The nPixelSpace and nLineSpace parameters allow reading into FROM various
 * organization of buffers.
 *
 * @param eRWFlag Either GF_Read to read a region of data (GF_Write, to write
 * a region of data, yet not supported)
 *
 * @param nXOff The pixel offset to the top left corner of the region of the
 * band to be accessed. This would be zero to start FROM the left side.
 *
 * @param nYOff The line offset to the top left corner of the region of the band
 * to be accessed. This would be zero to start FROM the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or FROM which it
 * should be written. This buffer must contain at least
 * nBufXSize * nBufYSize * nBandCount words of type eBufType. It is organized in
 * left to right,top to bottom pixel order. Spacing is controlled by the
 * nPixelSpace, and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or FROM which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired region
 * is to be read, or FROM which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer. The
 * pixel values will automatically be translated to/FROM the
 * PostGISRasterRasterBand data type as needed.
 *
 * @param nPixelSpace The byte offset FROM the start of one pixel value in pData
 * to the start of the next pixel value within a scanline. If defaulted (0) the
 * size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset FROM the start of one scanline in pData to
 * the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr PostGISRasterRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff,
    int nYOff, int nXSize, int nYSize, void * pData, int nBufXSize,
    int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg* psExtraArg)
{
    /**
     * TODO: Write support not implemented yet
     **/
    if (eRWFlag == GF_Write) {
        ReportError(CE_Failure, CPLE_NotSupported,
            "Writing through PostGIS Raster band not supported yet");

        return CE_Failure;
    }

    /*******************************************************************
     * Do we have overviews that would be appropriate to satisfy this
     * request?
     ******************************************************************/
    if( (nBufXSize < nXSize || nBufYSize < nYSize) &&
        GetOverviewCount() > 0 )
    {
        if(OverviewRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, nPixelSpace,
            nLineSpace, psExtraArg) == CE_None)

        return CE_None;
    }

    PostGISRasterDataset * poRDS = cpl::down_cast<PostGISRasterDataset *>(poDS);

    int bSameWindowAsOtherBand =
        (nXOff == poRDS->nXOffPrev &&
         nYOff == poRDS->nYOffPrev &&
         nXSize == poRDS->nXSizePrev &&
         nYSize == poRDS->nYSizePrev);
    poRDS->nXOffPrev = nXOff;
    poRDS->nYOffPrev = nYOff;
    poRDS->nXSizePrev = nXSize;
    poRDS->nYSizePrev = nYSize;

    /* Logic to determine if bands are read in order 1, 2, ... N */
    /* If so, then use multi-band caching, otherwise do just single band caching */
    if( poRDS->bAssumeMultiBandReadPattern )
    {
        if( nBand != poRDS->nNextExpectedBand )
        {
            CPLDebug("PostGIS_Raster",
                    "Disabling multi-band caching since band access pattern does not match");
            poRDS->bAssumeMultiBandReadPattern = false;
            poRDS->nNextExpectedBand = 1;
        }
        else
        {
            poRDS->nNextExpectedBand ++;
            if( poRDS->nNextExpectedBand > poRDS->GetRasterCount() )
                poRDS->nNextExpectedBand = 1;
        }
    }
    else
    {
        if( nBand == poRDS->nNextExpectedBand )
        {
            poRDS->nNextExpectedBand ++;
            if( poRDS->nNextExpectedBand > poRDS->GetRasterCount() )
            {
                CPLDebug("PostGIS_Raster", "Re-enabling multi-band caching");
                poRDS->bAssumeMultiBandReadPattern = true;
                poRDS->nNextExpectedBand = 1;
            }
        }
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
            "PostGISRasterRasterBand::IRasterIO: "
            "nBand = %d, nXOff = %d, nYOff = %d, nXSize = %d, nYSize = %d, nBufXSize = %d, nBufYSize = %d",
             nBand, nXOff, nYOff, nXSize,  nYSize, nBufXSize, nBufYSize);
#endif

    /*******************************************************************
     * Several tiles: we first look in all our sources caches. Missing
     * blocks are queried
     ******************************************************************/
    double adfProjWin[8];
    int nFeatureCount = 0;
    CPLRectObj sAoi;

    poRDS->PolygonFromCoords(nXOff, nYOff, nXOff + nXSize, nYOff + nYSize, adfProjWin);
    // (p[6], p[7]) is the minimum (x, y), and (p[2], p[3]) the max
    sAoi.minx = adfProjWin[6];
    sAoi.maxx = adfProjWin[2];
    if( adfProjWin[7] < adfProjWin[3] )
    {
        sAoi.miny = adfProjWin[7];
        sAoi.maxy = adfProjWin[3];
    }
    else
    {
        sAoi.maxy = adfProjWin[7];
        sAoi.miny = adfProjWin[3];
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
            "PostGISRasterRasterBand::IRasterIO: "
            "Intersection box: (%f, %f) - (%f, %f)", sAoi.minx,
            sAoi.miny, sAoi.maxx, sAoi.maxy);
#endif

    if (poRDS->hQuadTree == nullptr)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
            "Could not read metadata index.");
        return CE_Failure;
    }

    NullBuffer(pData, nBufXSize, nBufYSize, eBufType, static_cast<int>(nPixelSpace), static_cast<int>(nLineSpace));

    if( poRDS->bBuildQuadTreeDynamically && !bSameWindowAsOtherBand )
    {
        if( !(poRDS->LoadSources(nXOff, nYOff, nXSize, nYSize, nBand)) )
            return CE_Failure;
    }

    // Matching sources, to avoid a dumb for loop over the sources
    PostGISRasterTileDataset ** papsMatchingTiles =
        reinterpret_cast<PostGISRasterTileDataset **>(CPLQuadTreeSearch(poRDS->hQuadTree, &sAoi, &nFeatureCount));

    // No blocks found. This is not an error (the raster may have holes)
    if (nFeatureCount == 0) {
        CPLFree(papsMatchingTiles);

        return CE_None;
    }

    int i;

    /**
     * We need to store the max, min coords for the missing tiles in
     * any place. This is as good as any other
     **/
    sAoi.minx = 0.0;
    sAoi.miny = 0.0;
    sAoi.maxx = 0.0;
    sAoi.maxy = 0.0;

    GIntBig nMemoryRequiredForTiles = 0;
    CPLString osIDsToFetch;
    int nTilesToFetch = 0;
    int nBandDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;

    // Loop just over the intersecting sources
    for(i = 0; i < nFeatureCount; i++) {
        PostGISRasterTileDataset *poTile = papsMatchingTiles[i];
        PostGISRasterTileRasterBand* poTileBand =
            cpl::down_cast<PostGISRasterTileRasterBand *>(poTile->GetRasterBand(nBand));

        nMemoryRequiredForTiles += poTileBand->GetXSize() * poTileBand->GetYSize() *
            nBandDataTypeSize;

        // Missing tile: we'll need to query for it
        if (!poTileBand->IsCached()) {

            // If we have a PKID, add the tile PKID to the list
            if (poTile->pszPKID != nullptr)
            {
                if( !osIDsToFetch.empty() )
                    osIDsToFetch += ",";
                osIDsToFetch += "'";
                osIDsToFetch += poTile->pszPKID;
                osIDsToFetch += "'";
            }

            double dfTileMinX, dfTileMinY, dfTileMaxX, dfTileMaxY;
            poTile->GetExtent(&dfTileMinX, &dfTileMinY,
                              &dfTileMaxX, &dfTileMaxY);

            /**
             * We keep the general max and min values of all the missing
             * tiles, to raise a query that intersect just that area.
             *
             * TODO: In case of just a few tiles and very separated,
             * this strategy is clearly suboptimal. We'll get our
             * missing tiles, but with a lot of other not needed tiles.
             *
             * A possible optimization will be to simply rely on the
             * I/O method of the source (must be implemented), in case
             * we have minus than a reasonable amount of tiles missing.
             * Another criteria to decide would be how separated the
             * tiles are. Two queries for just two adjacent tiles is
             * also a dumb strategy.
             **/
            if( nTilesToFetch == 0 )
            {
                sAoi.minx = dfTileMinX;
                sAoi.miny = dfTileMinY;
                sAoi.maxx = dfTileMaxX;
                sAoi.maxy = dfTileMaxY;
            }
            else
            {
                if (dfTileMinX < sAoi.minx)
                    sAoi.minx = dfTileMinX;

                if (dfTileMinY < sAoi.miny)
                    sAoi.miny = dfTileMinY;

                if (dfTileMaxX > sAoi.maxx)
                    sAoi.maxx = dfTileMaxX;

                if (dfTileMaxY > sAoi.maxy)
                    sAoi.maxy = dfTileMaxY;
            }

            nTilesToFetch ++;
        }
    }

    /* Determine caching strategy */
    bool bAllBandCaching = false;
    if (nTilesToFetch > 0)
    {
        GIntBig nCacheMax = GDALGetCacheMax64();
        if( nMemoryRequiredForTiles > nCacheMax )
        {
            CPLDebug("PostGIS_Raster",
                    "For best performance, the block cache should be able to store " CPL_FRMT_GIB
                    " bytes for the tiles of the requested window, "
                    "but it is only " CPL_FRMT_GIB " byte large",
                    nMemoryRequiredForTiles, nCacheMax );
            nTilesToFetch = 0;
        }

        if( poRDS->GetRasterCount() > 1 && poRDS->bAssumeMultiBandReadPattern )
        {
            GIntBig nMemoryRequiredForTilesAllBands =
                nMemoryRequiredForTiles * poRDS->GetRasterCount();
            if( nMemoryRequiredForTilesAllBands <= nCacheMax )
            {
                bAllBandCaching = true;
            }
            else
            {
                CPLDebug("PostGIS_Raster", "Caching only this band, but not all bands. "
                         "Cache should be " CPL_FRMT_GIB " byte large for that",
                         nMemoryRequiredForTilesAllBands);
            }
        }
    }

    // Raise a query for missing tiles and cache them
    if (nTilesToFetch > 0) {

        /**
         * There are several options here, to raise the query.
         * - Get all the tiles which PKID is in a list of missing
         *   PKIDs.
         * - Get all the tiles that intersect a polygon constructed
         *   based on the (min - max) values calculated before.
         * - Get all the tiles with upper left pixel included in the
         *   range (min - max) calculated before.
         *
         * The first option is the most efficient one when a PKID exists.
         * After that, the second one is the most efficient one when a
         * spatial index exists.
         * The third one is the only one available when neither a PKID or spatial
         * index exist.
         **/

        CPLString osSchemaI(CPLQuotedSQLIdentifier(pszSchema));
        CPLString osTableI(CPLQuotedSQLIdentifier(pszTable));
        CPLString osColumnI(CPLQuotedSQLIdentifier(pszColumn));

        CPLString osWHERE;
        if (!osIDsToFetch.empty() && (poRDS->bIsFastPK || !(poRDS->HasSpatialIndex())) ) {
            if( nTilesToFetch < poRDS->m_nTiles || poRDS->bBuildQuadTreeDynamically )
            {
                osWHERE += poRDS->pszPrimaryKeyName;
                osWHERE += " IN (";
                osWHERE += osIDsToFetch;
                osWHERE += ")";
            }
        }
        else
        {
            if( poRDS->HasSpatialIndex() )
            {
                osWHERE += CPLSPrintf("%s && "
                        "ST_GeomFromText('POLYGON((%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f))')",
                        osColumnI.c_str(),
                        adfProjWin[0], adfProjWin[1],
                        adfProjWin[2], adfProjWin[3],
                        adfProjWin[4], adfProjWin[5],
                        adfProjWin[6], adfProjWin[7],
                        adfProjWin[0], adfProjWin[1]);
            }
            else
            {
                #define EPS 1e-5
                osWHERE += CPLSPrintf("ST_UpperLeftX(%s)"
                    " BETWEEN %f AND %f AND ST_UpperLeftY(%s) BETWEEN "
                    "%f AND %f", osColumnI.c_str(),
                    sAoi.minx-EPS, sAoi.maxx+EPS,
                    osColumnI.c_str(),
                    sAoi.miny-EPS, sAoi.maxy+EPS);
            }
        }

        if( poRDS->pszWhere != nullptr )
        {
            if( !osWHERE.empty() )
                osWHERE += " AND ";
            osWHERE += "(";
            osWHERE += poRDS->pszWhere;
            osWHERE += ")";
        }

        bool bCanUseClientSide = true;
        if( poRDS->eOutDBResolution == OutDBResolution::CLIENT_SIDE_IF_POSSIBLE )
        {
            bCanUseClientSide = poRDS->CanUseClientSideOutDB(bAllBandCaching,
                                                             nBand,
                                                             osWHERE);
        }

        CPLString osRasterToFetch;
        if (bAllBandCaching)
            osRasterToFetch = osColumnI;
        else
            osRasterToFetch.Printf("ST_Band(%s, %d)", osColumnI.c_str(), nBand);
        if( poRDS->eOutDBResolution == OutDBResolution::SERVER_SIDE ||
            !bCanUseClientSide )
        {
            osRasterToFetch = "encode(ST_AsBinary(" + osRasterToFetch + ",TRUE),'hex')";
        }

        CPLString osCommand;
        osCommand.Printf("SELECT %s, ST_Metadata(%s), %s FROM %s.%s",
                         (poRDS->GetPrimaryKeyRef()) ? poRDS->GetPrimaryKeyRef() : "NULL",
                         osColumnI.c_str(),
                         osRasterToFetch.c_str(),
                         osSchemaI.c_str(), osTableI.c_str());
        if( !osWHERE.empty() )
        {
            osCommand += " WHERE " + osWHERE;
        }

        PGresult * poResult = PQexec(poRDS->poConn, osCommand.c_str());

#ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
            "PostGISRasterRasterBand::IRasterIO(): Query = \"%s\" --> number of rows = %d",
            osCommand.c_str(), poResult ? PQntuples(poResult) : 0 );
#endif

        if (poResult == nullptr ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) < 0) {

            if (poResult)
                PQclear(poResult);

            CPLError(CE_Failure, CPLE_AppDefined,
                "PostGISRasterRasterBand::IRasterIO(): %s",
                PQerrorMessage(poRDS->poConn));

            // Free the object that holds pointers to matching tiles
            CPLFree(papsMatchingTiles);
            return CE_Failure;
        }

        /**
         * No data. Return the buffer filled with nodata values
         **/
        else if (PQntuples(poResult) == 0) {
            PQclear(poResult);

            // Free the object that holds pointers to matching tiles
            CPLFree(papsMatchingTiles);
            return CE_None;
        }

        /**
         * Ok, we loop over the results
         **/
        int nTuples = PQntuples(poResult);
        for(i = 0; i < nTuples; i++)
        {
            const char *pszPKID = PQgetvalue(poResult, i, 0);
            const char* pszMetadata = PQgetvalue(poResult, i, 1);
            const char* pszRaster = PQgetvalue(poResult, i, 2);
            poRDS->CacheTile(pszMetadata, pszRaster, pszPKID, nBand, bAllBandCaching);
        } // All tiles have been added to cache

        PQclear(poResult);
    } // End missing tiles

/* -------------------------------------------------------------------- */
/*      Overlay each source in turn over top this.                      */
/* -------------------------------------------------------------------- */

    CPLErr eErr = CE_None;
    /* Sort tiles by ascending PKID, so that the draw order is deterministic. */
    if( poRDS->GetPrimaryKeyRef() != nullptr )
    {
        qsort(papsMatchingTiles, nFeatureCount, sizeof(PostGISRasterTileDataset*),
              SortTilesByPKID);
    }

    for(i = 0; i < nFeatureCount && eErr == CE_None; i++)
    {
        PostGISRasterTileDataset *poTile = papsMatchingTiles[i];
        PostGISRasterTileRasterBand* poTileBand =
            cpl::down_cast<PostGISRasterTileRasterBand *>(poTile->GetRasterBand(nBand));
        eErr =
            poTileBand->poSource->RasterIO( eDataType,
                                            nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize,
                                            eBufType, nPixelSpace, nLineSpace, nullptr);
    }

    // Free the object that holds pointers to matching tiles
    CPLFree(papsMatchingTiles);

    return eErr;
}

/**
 * \brief Set the no data value for this band.
 * Parameters:
 *  - double: The nodata value
 * Returns:
 *  - CE_None.
 */
CPLErr PostGISRasterRasterBand::SetNoDataValue(double dfNewValue) {
    m_dfNoDataValue = dfNewValue;

    return CE_None;
}

/**
 * \brief Fetch the no data value for this band.
 * Parameters:
 *  - int *: pointer to a boolean to use to indicate if a value is actually
 *          associated with this layer. May be NULL (default).
 *  Returns:
 *  - double: the nodata value for this band.
 */
double PostGISRasterRasterBand::GetNoDataValue(int *pbSuccess) {
    if (pbSuccess != nullptr)
        *pbSuccess = m_bNoDataValueSet;

    return m_dfNoDataValue;
}

/***************************************************
 * \brief Return the number of overview layers available
 ***************************************************/
int PostGISRasterRasterBand::GetOverviewCount()
{
    PostGISRasterDataset * poRDS = cpl::down_cast<PostGISRasterDataset *>(poDS);
    return poRDS->GetOverviewCount();
}

/**********************************************************
 * \brief Fetch overview raster band object
 **********************************************************/
GDALRasterBand * PostGISRasterRasterBand::GetOverview(int i)
{
    if (i < 0 || i >= GetOverviewCount())
        return nullptr;

    PostGISRasterDataset * poRDS = cpl::down_cast<PostGISRasterDataset *>(poDS);
    PostGISRasterDataset* poOverviewDS = poRDS->GetOverviewDS(i);
    if( poOverviewDS->nBands == 0 )
    {
        if (!poOverviewDS->SetRasterProperties(nullptr) ||
             poOverviewDS->GetRasterCount() != poRDS->GetRasterCount())
        {
            CPLDebug("PostGIS_Raster",
                     "Request for overview %d of band %d failed", i, nBand);
            return nullptr;
        }
    }

    return poOverviewDS->GetRasterBand(nBand);
}

/**
 * \brief How should this band be interpreted as color?
 * GCI_Undefined is returned when the format doesn't know anything about the
 * color interpretation.
 **/
GDALColorInterp PostGISRasterRasterBand::GetColorInterpretation()
{
    if (poDS->GetRasterCount() == 1) {
        m_eColorInterp = GCI_GrayIndex;
    }

    else if (poDS->GetRasterCount() == 3) {
        if (nBand == 1)
            m_eColorInterp = GCI_RedBand;
        else if( nBand == 2 )
            m_eColorInterp = GCI_GreenBand;
        else if( nBand == 3 )
            m_eColorInterp = GCI_BlueBand;
        else
            m_eColorInterp = GCI_Undefined;
    }

    else {
        m_eColorInterp = GCI_Undefined;
    }

    return m_eColorInterp;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double PostGISRasterRasterBand::GetMinimum( int *pbSuccess )
{
    PostGISRasterDataset * poRDS = cpl::down_cast<PostGISRasterDataset *>(poDS);
    if( poRDS->bBuildQuadTreeDynamically && poRDS->m_nTiles == 0 )
    {
        if( pbSuccess )
            *pbSuccess = FALSE;
        return 0.0;
    }
    return VRTSourcedRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double PostGISRasterRasterBand::GetMaximum( int *pbSuccess )
{
    PostGISRasterDataset * poRDS = cpl::down_cast<PostGISRasterDataset *>(poDS);
    if( poRDS->bBuildQuadTreeDynamically && poRDS->m_nTiles == 0 )
    {
        if( pbSuccess )
            *pbSuccess = FALSE;
        return 0.0;
    }
    return VRTSourcedRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr PostGISRasterRasterBand::ComputeRasterMinMax( int bApproxOK, double* adfMinMax )
{
    if( nRasterXSize < 1024 && nRasterYSize < 1024 )
        return VRTSourcedRasterBand::ComputeRasterMinMax(bApproxOK, adfMinMax);

    int nOverviewCount = GetOverviewCount();
    for(int i = 0; i < nOverviewCount; i++)
    {
        auto poOverview = GetOverview(i);
        if( poOverview->GetXSize() < 1024 && poOverview->GetYSize() < 1024 )
            return poOverview->ComputeRasterMinMax(bApproxOK, adfMinMax);
    }

    return CE_Failure;
}
