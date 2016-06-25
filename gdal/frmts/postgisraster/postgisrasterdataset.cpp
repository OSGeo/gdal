/***********************************************************************
 * File :    postgisrasterdataset.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL Dataset implementation for PostGIS Raster driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Author:	 David Zwarg, dzwarg@azavea.com
 *
 * Last changes: $Id: $
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo, David Zwarg
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "postgisraster.h"
#include <math.h>

#ifdef _WIN32
#define rint(x) floor((x) + 0.5)
#endif

/* PostgreSQL defaults */
#define DEFAULT_SCHEMA          "public"
#define DEFAULT_COLUMN          "rast"

/** Note on read performance on mode=2:

    There are different situations:

     1) the table is registered in the raster_columns table and number of bands, minx,miny,maxx,maxy are available
        a) no where clause, the table has a primary key and a GIST index on the raster column.
            If the raster_columns advertize a scale_x and scale_y, use it.
            Otherwise take the metadata of 10 rasters and compute and average scale_x, scale_y
            With above information, we can build the dataset definition.

            (Following logic is implemented in LoadSources())

            During a IRasterIO() query,
            i) we will do a SQL query to retrieve the PKID of tiles that intersect the query window.
            ii) If some tiles are not registered as sources, then do a SQL query to fetch their metadata
            and instantiate them and register them.
            iii) If some tiles are not cached, then determine if the query window is not too big (w.r.t. GDAL cache),
            and if not, then do a SQL query to fetch their raster column.

            Note: if raster_columns show that all tiles have same dimensions, we can merge the query ii) and iii)
            in the same one.

        b) otherwise, do a full scan of metadata to build the sources

     2) otherwise, throw a warning to the user and do a full scan of metadata is needed to build the sources

     For 1b) and 2), during a IRasterIO() query, determine which sources are needed and not cached.

        (Following logic is implemented in IRasterIO())

        If the query window is not too big,
            If there's a primary key, then do a SQL query on the IDs of uncached sources to fetch their raster column
            and cache them.
            Otherwise if there's a GIST index, do a SQL spatial query on the window to fetch their raster column,
            and cache them (identification with registered sources is done with the top-left coordinates)
            Otherwise do a SQL query based on the range of top-left coordinates of tiles that intersect the query window.

     Conclusion: best performance is achieved with: no where clause, a primary key, a GIST index, known table extent
                 and, with moderate performance advantage :
                  - same scale_x, scale_y to save an initial SQL query,
                  - same blocksize_x, blocksize_y to save one SQL query per IRasterIO()
*/

/************************
 * \brief Constructor
 ************************/
PostGISRasterDataset::PostGISRasterDataset():VRTDataset(0, 0) {

    papszSubdatasets = NULL;
    nSrid = -1;
    nOverviewFactor = 1;
    nBandsToCreate = 0;
    poConn = NULL;
    bRegularBlocking = false;
    bAllTilesSnapToSameGrid = false;

    bCheckAllTiles = CPLTestBool(
        CPLGetConfigOption("PR_ALLOW_WHOLE_TABLE_SCAN", "YES"));

    pszSchema = NULL;
    pszTable = NULL;
    pszColumn = NULL;
    pszWhere = NULL;
    pszProjection = NULL;

    adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = 0.0;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0.0;
    adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = 0.0;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0.0;
    adfGeoTransform[GEOTRSFRM_WE_RES] = 0;
    adfGeoTransform[GEOTRSFRM_NS_RES] = 0;

    adfGeoTransform[GEOTRSFRM_WE_RES] =
        CPLAtof(CPLGetConfigOption("PR_WE_RES", NO_VALID_RES));

    adfGeoTransform[GEOTRSFRM_NS_RES] =
        CPLAtof(CPLGetConfigOption("PR_NS_RES", NO_VALID_RES));

    // Default
    resolutionStrategy = AVERAGE_APPROX_RESOLUTION;

    const char * pszTmp = NULL;
    // We ignore this option if we provided the desired resolution
    if (CPLIsEqual(adfGeoTransform[GEOTRSFRM_WE_RES], CPLAtof(NO_VALID_RES)) ||
        CPLIsEqual(adfGeoTransform[GEOTRSFRM_NS_RES], CPLAtof(NO_VALID_RES))) {

        // Resolution didn't have a valid value, so, we initiate it
        adfGeoTransform[GEOTRSFRM_WE_RES] = 0.0;
        adfGeoTransform[GEOTRSFRM_NS_RES] = 0.0;

        pszTmp =
            CPLGetConfigOption("PR_RESOLUTION_STRATEGY", "AVERAGE_APPROX");

        if (EQUAL(pszTmp, "LOWEST"))
            resolutionStrategy = LOWEST_RESOLUTION;

        else if (EQUAL(pszTmp, "HIGHEST"))
            resolutionStrategy = HIGHEST_RESOLUTION;

        else if (EQUAL(pszTmp, "USER"))
            resolutionStrategy = USER_RESOLUTION;

        else if (EQUAL(pszTmp, "AVERAGE"))
            resolutionStrategy = AVERAGE_RESOLUTION;

    }

    else {
        resolutionStrategy = USER_RESOLUTION;
#ifdef DEBUG_VERBOSE
        pszTmp = "USER";
#endif
    }

#ifdef DEBUG_VERBOSE
     CPLDebug("PostGIS_Raster", "PostGISRasterDataset::Constructor:"
                "STRATEGY = %s", pszTmp);
#endif

    m_nTiles = 0;
    nMode = NO_MODE;
    poDriver = NULL;

    nRasterXSize = nRasterYSize = 0;
    pszPrimaryKeyName = NULL;
    bIsFastPK = false;
    bHasTriedFetchingPrimaryKeyName = false;

    papoSourcesHolders = NULL;
    hQuadTree = NULL;

    bHasBuiltOverviews = false;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    poParentDS = NULL;

    bAssumeMultiBandReadPattern = true;
    nNextExpectedBand = 1;
    nXOffPrev = 0;
    nYOffPrev = 0;
    nXSizePrev = 0;
    nYSizePrev = 0;

    bHasTriedHasSpatialIndex = false;
    bHasSpatialIndex = false;

    bBuildQuadTreeDynamically = false;

    bTilesSameDimension = false;
    nTileWidth = 0;
    nTileHeight = 0;

    SetWritable(false);


    /**
     * TODO: Parametrize bAllTilesSnapToSameGrid. It controls if all the
     * raster rows, in ONE_RASTER_PER_TABLE mode, must be checked to
     * test if they snap to the same grid and have the same SRID. It can
     * be the user decision, if he/she's sure all the rows pass the
     * test and want more speed.
     **/

}

/************************
 * \brief Constructor
 ************************/
PostGISRasterDataset::~PostGISRasterDataset() {

    if (pszSchema) {
        CPLFree(pszSchema);
        pszSchema = NULL;
    }

    if (pszTable) {
        CPLFree(pszTable);
        pszTable = NULL;
    }

    if (pszColumn) {
        CPLFree(pszColumn);
        pszColumn = NULL;
    }

    if (pszWhere) {
        CPLFree(pszWhere);
        pszWhere = NULL;
    }

    if (pszProjection) {
        CPLFree(pszProjection);
        pszProjection = NULL;
    }

    if (pszPrimaryKeyName) {
        CPLFree(pszPrimaryKeyName);
        pszPrimaryKeyName = NULL;
    }

    if (papszSubdatasets) {
        CSLDestroy(papszSubdatasets);
        papszSubdatasets = NULL;
    }

    if (hQuadTree) {
        CPLQuadTreeDestroy(hQuadTree);
        hQuadTree = NULL;
    }

    // Call it now so that the VRT sources
    // are deleted and that there is no longer any code
    // referencing the bands of the source holders.
    // Otherwise this would go wrong because
    // of the deleting the source holders just below.
    CloseDependentDatasets();

    if (papoSourcesHolders) {
        int i;
        for(i = 0; i < m_nTiles; i++) {
            if (papoSourcesHolders[i])
                delete papoSourcesHolders[i];

        }

        VSIFree(papoSourcesHolders);
        papoSourcesHolders = NULL;
    }
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int PostGISRasterDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = VRTDataset::CloseDependentDatasets();
    if( nOverviewCount > 0 )
    {
        int i;
        for(i = 0; i < nOverviewCount; i++)
        {
            delete papoOverviewDS[i];
        }
        CPLFree(papoOverviewDS);
        papoOverviewDS = NULL;
        nOverviewCount = 0;
        bHasDroppedRef = TRUE;
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                            HasSpatialIndex()                         */
/************************************************************************/

GBool PostGISRasterDataset::HasSpatialIndex()
{
    CPLString osCommand;
    PGresult* poResult = NULL;

    // If exists, return it
    if (bHasTriedHasSpatialIndex) {
        return bHasSpatialIndex;
    }

    bHasTriedHasSpatialIndex = true;

    /* For debugging purposes only */
    if( CPLTestBool(CPLGetConfigOption("PR_DISABLE_GIST", "FALSE") ) )
        return false;

    // Copyright dustymugs !!!
    osCommand.Printf(
    "SELECT n.nspname AS schema_name, c2.relname AS table_name, att.attname AS column_name, "
    "       c.relname AS index_name, am.amname AS index_type "
    "FROM pg_catalog.pg_class c "
    "JOIN pg_catalog.pg_index i ON i.indexrelid = c.oid "
    "JOIN pg_catalog.pg_class c2 ON i.indrelid = c2.oid "
    "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
    "JOIN pg_am am ON c.relam = am.oid "
    "JOIN pg_attribute att ON att.attrelid = c2.oid "
    "AND pg_catalog.format_type(att.atttypid, att.atttypmod) = 'raster' "
    "WHERE c.relkind IN ('i') "
    "AND am.amname = 'gist' "
    "AND strpos(split_part(pg_catalog.pg_get_indexdef(i.indexrelid, 0, true), ' gist ', 2), att.attname) > 0 "
    "AND n.nspname = '%s' "
    "AND c2.relname = '%s' "
    "AND att.attname = '%s' ", pszSchema, pszTable, pszColumn);

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::HasSpatialIndex(): Query: %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());

    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0 )
    {
        bHasSpatialIndex = false;
        CPLDebug("PostGIS_Raster", "For better performance, creating a spatial index "
                 "with 'CREATE INDEX %s_%s_%s_gist_idx ON %s.%s USING GIST (ST_ConvexHull(%s))' is advised",
                 pszSchema, pszTable, pszColumn, pszSchema, pszTable, pszColumn);
    }
    else
    {
        bHasSpatialIndex = true;
    }

    if( poResult )
        PQclear(poResult);
    return bHasSpatialIndex;
}

/***********************************************************************
 * \brief Look for a primary key in the table selected by the user
 *
 * If the table does not have a primary key, it returns NULL
 **********************************************************************/
const char * PostGISRasterDataset::GetPrimaryKeyRef()
{
    CPLString osCommand;
    PGresult* poResult = NULL;

    // If exists, return it
    if (bHasTriedFetchingPrimaryKeyName) {
        return pszPrimaryKeyName;
    }

    bHasTriedFetchingPrimaryKeyName = true;

    /* For debugging purposes only */
    if( CPLTestBool(CPLGetConfigOption("PR_DISABLE_PK", "FALSE") ) )
        return NULL;

    /* Determine the primary key/unique column on the table */
    osCommand.Printf("select d.attname from pg_catalog.pg_constraint "
        "as a join pg_catalog.pg_indexes as b on a.conname = "
        "b.indexname join pg_catalog.pg_class as c on c.relname = "
        "b.tablename join pg_catalog.pg_attribute as d on "
        "c.relfilenode = d.attrelid where b.schemaname = '%s' and "
        "b.tablename = '%s' and d.attnum = a.conkey[1] and a.contype "
        "in ('p', 'u')", pszSchema, pszTable);

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::GetPrimaryKeyRef(): Query: %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());

    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0 ) {

        PQclear(poResult);

        /**
         * Maybe there is no primary key or unique constraint; a
         * sequence will also suffice; get the first one
         **/

        osCommand.Printf("select cols.column_name from "
            "information_schema.columns as cols join "
            "information_schema.sequences as seqs on "
            "cols.column_default like '%%'||seqs.sequence_name||'%%' "
            "where cols.table_schema = '%s' and cols.table_name = '%s'",
            pszSchema, pszTable);

#ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::GetPrimaryKeyRef(): Query: %s",
            osCommand.c_str());
#endif

        poResult = PQexec(poConn, osCommand.c_str());

        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) <= 0) {

            CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::GetPrimaryKeyRef(): Could not "
            "find a primary key or unique column on the specified "
            "table %s.%s. For better performance, creating a primary key on the table is advised.",
            pszSchema, pszTable);

            pszPrimaryKeyName = NULL; // Just in case

        }

        else {
            pszPrimaryKeyName = CPLStrdup(PQgetvalue(poResult, 0, 0));
        }

    }

    // Ok, get the primary key
    else {
        pszPrimaryKeyName = CPLStrdup(PQgetvalue(poResult, 0, 0));
        bIsFastPK = true;
    }

    PQclear(poResult);

    return pszPrimaryKeyName;
}


/***********************************************************************
 * \brief Look for raster tables in database and store them as
 * subdatasets
 *
 * If no table is provided in connection string, the driver looks for
 * the existent raster tables in the schema given as argument. This
 * argument, however, is optional. If a NULL value is provided, the
 * driver looks for all raster tables in all schemas of the
 * user-provided database.
 *
 * NOTE: Permissions are managed by libpq. The driver only returns an
 * error if an error is returned when trying to access to tables not
 * allowed to the current user.
 **********************************************************************/
GBool PostGISRasterDataset::BrowseDatabase(const char* pszCurrentSchema,
        const char* pszValidConnectionString) {

    char* l_pszSchema = NULL;
    char* l_pszTable = NULL;
    char* l_pszColumn = NULL;

    int i = 0;
    int nTuples = 0;
    PGresult * poResult = NULL;
    CPLString osCommand;


    /*************************************************************
     * Fetch all the raster tables and store them as subdatasets
     *************************************************************/
    if (pszCurrentSchema == NULL) {
        osCommand.Printf("select pg_namespace.nspname as schema, "
            "pg_class.relname as table, pg_attribute.attname as column "
            "from pg_class, pg_namespace,pg_attribute, pg_type where "
            "pg_class.relnamespace = pg_namespace.oid and "
            "pg_class.oid = pg_attribute.attrelid and "
            "pg_attribute.atttypid = pg_type.oid and "
            "pg_type.typname = 'raster'");

        poResult = PQexec(poConn, osCommand.c_str());
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) <= 0) {

            ReportError(CE_Failure, CPLE_AppDefined,
                "Error browsing database for PostGIS Raster tables: %s",
                PQerrorMessage(poConn));
            if (poResult != NULL)
                PQclear(poResult);

            return false;
        }


        nTuples = PQntuples(poResult);
        for (i = 0; i < nTuples; i++) {
            l_pszSchema = PQgetvalue(poResult, i, 0);
            l_pszTable = PQgetvalue(poResult, i, 1);
            l_pszColumn = PQgetvalue(poResult, i, 2);

            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                CPLSPrintf("SUBDATASET_%d_NAME", (i + 1)),
                CPLSPrintf("PG:%s schema=%s table=%s column=%s",
                pszValidConnectionString, l_pszSchema, l_pszTable,
                l_pszColumn));

            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                CPLSPrintf("SUBDATASET_%d_DESC", (i + 1)),
                CPLSPrintf("PostGIS Raster table at %s.%s (%s)",
                l_pszSchema, l_pszTable, l_pszColumn));
        }

        PQclear(poResult);

    }
        /***************************************************************
         * Fetch all the schema's raster tables and store them as
         * subdatasets
         **************************************************************/
    else {
        osCommand.Printf("select pg_class.relname as table, "
            "pg_attribute.attname as column from pg_class, "
            "pg_namespace,pg_attribute, pg_type where "
            "pg_class.relnamespace = pg_namespace.oid and "
            "pg_class.oid = pg_attribute.attrelid and "
            "pg_attribute.atttypid = pg_type.oid and "
            "pg_type.typname = 'raster' and "
            "pg_namespace.nspname = '%s'", pszCurrentSchema);

        poResult = PQexec(poConn, osCommand.c_str());
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) <= 0) {

            ReportError(CE_Failure, CPLE_AppDefined,
                "Error browsing database for PostGIS Raster tables: %s",
                PQerrorMessage(poConn));

            if (poResult != NULL)
                PQclear(poResult);

            return false;
        }


        nTuples = PQntuples(poResult);
        for (i = 0; i < nTuples; i++) {
            l_pszTable = PQgetvalue(poResult, i, 0);
            l_pszColumn = PQgetvalue(poResult, i, 1);

            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                CPLSPrintf("SUBDATASET_%d_NAME", (i + 1)),
                CPLSPrintf("PG:%s schema=%s table=%s column=%s",
                pszValidConnectionString, pszCurrentSchema, l_pszTable,
                l_pszColumn));

            papszSubdatasets = CSLSetNameValue(papszSubdatasets,
                CPLSPrintf("SUBDATASET_%d_DESC", (i + 1)),
                CPLSPrintf("PostGIS Raster table at %s.%s (%s)",
                pszCurrentSchema, l_pszTable, l_pszColumn));
        }

        PQclear(poResult);
    }

    return true;
}


/***********************************************************************
 * \brief Look for overview tables for the bands of the current dataset
 **********************************************************************/
PROverview * PostGISRasterDataset::GetOverviewTables(int * pnOverviews)
{
    PROverview * poOV = NULL;
    CPLString osCommand;
    PGresult * poResult = NULL;

    osCommand.Printf("SELECT o_table_name, overview_factor, "
            "o_raster_column, o_table_schema FROM raster_overviews "
            "WHERE r_table_schema = '%s' AND r_table_name = '%s' AND "
            "r_raster_column = '%s' ORDER BY overview_factor", this->pszSchema, this->pszTable,
            this->pszColumn);

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::GetOverviewTables(): Query: %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());

    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) < 0) {

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error looking for overview tables: %s",
            PQerrorMessage(poConn));

        if (poResult)
            PQclear(poResult);

        return NULL;
    }

    else if (PQntuples(poResult) == 0) {
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::GetOverviewTables(): No overviews "
            "for table %s.%s", pszTable, pszSchema);

        if (poResult)
            PQclear(poResult);

        return NULL;
    }

    int nTuples = PQntuples(poResult);

    poOV = (PROverview *)VSIMalloc2(nTuples, sizeof(PROverview));
    if (poOV == NULL) {
        ReportError(CE_Failure, CPLE_AppDefined,
            "Error looking for overview tables");

        PQclear(poResult);

        return NULL;
    }

    int iOVerview = 0;
    for(iOVerview = 0; iOVerview < nTuples; iOVerview++) {
        poOV[iOVerview].pszSchema =
            CPLStrdup(PQgetvalue(poResult, iOVerview, 3));

        poOV[iOVerview].pszTable =
            CPLStrdup(PQgetvalue(poResult, iOVerview, 0));

        poOV[iOVerview].pszColumn =
            CPLStrdup(PQgetvalue(poResult, iOVerview, 2));

        poOV[iOVerview].nFactor =
            atoi(PQgetvalue(poResult, iOVerview, 1));

    }

    if (pnOverviews)
        *pnOverviews = nTuples;

    PQclear(poResult);

    return poOV;
}

/***********************************************************************
 * \brief Build overview datasets
 ***********************************************************************/
void PostGISRasterDataset::BuildOverviews()
{
    if( bHasBuiltOverviews || poParentDS != NULL )
        return;

    bHasBuiltOverviews = true;
    /*******************************************************************
     * We also get the names of the overview tables, if they exist. So,
     * we'll can use them to create the overview datasets.
     ******************************************************************/
    int nOV = 0;
    PROverview * poOV = GetOverviewTables(&nOV);

    if (poOV)
    {
        papoOverviewDS = (PostGISRasterDataset**) CPLCalloc(nOV, sizeof(PostGISRasterDataset*));
        nOverviewCount = 0;

        int iOV;
        for(iOV = 0; iOV < nOV; iOV++)
        {
            PostGISRasterDataset* poOvrDS = new PostGISRasterDataset();
            poOvrDS->nOverviewFactor = poOV[iOV].nFactor;
            poOvrDS->poConn = poConn;
            poOvrDS->eAccess = eAccess;
            poOvrDS->nMode = nMode;
            poOvrDS->pszSchema = poOV[iOV].pszSchema; // takes ownership
            poOvrDS->pszTable = poOV[iOV].pszTable; // takes ownership
            poOvrDS->pszColumn = poOV[iOV].pszColumn; // takes ownership
            poOvrDS->pszWhere = pszWhere ? CPLStrdup(pszWhere) : NULL;
            poOvrDS->poParentDS = this;

            if (!CPLTestBool(CPLGetConfigOption("PG_DEFERRED_OVERVIEWS", "YES")) &&
                (!poOvrDS->SetRasterProperties(NULL) ||
                poOvrDS->GetRasterCount() != GetRasterCount()))
            {
                delete poOvrDS;
            }
            else
            {
                papoOverviewDS[nOverviewCount ++] = poOvrDS;
            }
        }

        VSIFree(poOV);
    }
}

/***********************************************************************
 * \brief Returns overview count
 ***********************************************************************/
int PostGISRasterDataset::GetOverviewCount()
{
    BuildOverviews();
    return nOverviewCount;
}

/***********************************************************************
 * \brief Returns overview dataset
 ***********************************************************************/
PostGISRasterDataset* PostGISRasterDataset::GetOverviewDS(int iOvr)
{
    if( iOvr < 0 || iOvr > GetOverviewCount() )
        return NULL;
    return papoOverviewDS[iOvr];
}

/***********************************************************************
 * \brief Calculates the destination window for a VRT source, taking
 * into account that the source is a PostGIS Raster tile and the
 * destination is the general dataset itself
 *
 * This method is adapted from gdalbuildvrt as is in GDAL 1.10.0
***********************************************************************/
GBool PostGISRasterDataset::GetDstWin(
    PostGISRasterTileDataset * psDP,
    int* pnDstXOff, int* pnDstYOff, int* pnDstXSize, int* pnDstYSize)
{
    double we_res = this->adfGeoTransform[GEOTRSFRM_WE_RES];
    double ns_res = this->adfGeoTransform[GEOTRSFRM_NS_RES];

    double adfTileGeoTransform[6];
    psDP->GetGeoTransform(adfTileGeoTransform);

    *pnDstXOff = (int)
        (0.5 + (adfTileGeoTransform[GEOTRSFRM_TOPLEFT_X] - xmin) / we_res);

    if( ns_res < 0 )
        *pnDstYOff = (int)
            (0.5 + (ymax - adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y]) / -ns_res);
    else
        *pnDstYOff = (int)
            (0.5 + (adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y] - ymin) / ns_res);

    *pnDstXSize = (int)
        (0.5 + psDP->GetRasterXSize() *
         adfTileGeoTransform[GEOTRSFRM_WE_RES] / we_res);
    *pnDstYSize = (int)
        (0.5 + psDP->GetRasterYSize() *
         adfTileGeoTransform[GEOTRSFRM_NS_RES] / ns_res);

    return true;
}

/***********************************************************************
 * \brief Add tiles bands as complex source for raster bands.
 **********************************************************************/
GBool PostGISRasterDataset::AddComplexSource(PostGISRasterTileDataset* poRTDS)
{
    // Parameters to add the tile bands as sources
    int nDstXOff = 0;
    int nDstYOff = 0;
    int nDstXSize = 0;
    int nDstYSize = 0;

    // Get src and dst parameters
    GBool bValidTile = GetDstWin(poRTDS,
        &nDstXOff, &nDstYOff, &nDstXSize, &nDstYSize);
    if (!bValidTile)
        return false;

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::AddComplexSource: "
        "Tile bounding box from (%d, %d) of size (%d, %d) will "
        "cover raster bounding box from (%d, %d) of size "
        "(%d, %d)", 0, 0,
        poRTDS->GetRasterXSize(),
        poRTDS->GetRasterYSize(),
        nDstXOff, nDstYOff, nDstXSize, nDstYSize);
#endif

    // Add tiles bands as sources for the raster bands
    for(int iBand = 0; iBand < nBandsToCreate; iBand++)
    {
        PostGISRasterRasterBand * prb =
            (PostGISRasterRasterBand *)GetRasterBand(iBand + 1);

        int bHasNoData = FALSE;
        double dfBandNoDataValue = prb->GetNoDataValue(&bHasNoData);

        PostGISRasterTileRasterBand * prtb =
            (PostGISRasterTileRasterBand *)
                poRTDS->GetRasterBand(iBand + 1);

        prb->AddComplexSource(prtb, 0, 0,
            poRTDS->GetRasterXSize(),
            poRTDS->GetRasterYSize(),
            nDstXOff, nDstYOff, nDstXSize, nDstYSize,
            0.0, 1.0,
            (bHasNoData) ? dfBandNoDataValue : VRT_NODATA_UNSET);

        prtb->poSource = prb->papoSources[prb->nSources-1];
    }

    return true;
}

/************************************************************************/
/*                         GetMatchingSourceRef()                       */
/************************************************************************/

/**
 * \brief Get the tile dataset that matches the given upper left pixel
 **/
PostGISRasterTileDataset *
    PostGISRasterDataset::GetMatchingSourceRef(double dfUpperLeftX,
                                               double dfUpperLeftY)
{
    int i;
    PostGISRasterTileDataset * poRTDS = NULL;

    for(i = 0; i < m_nTiles; i++)
    {
        poRTDS = papoSourcesHolders[i];

        if (CPLIsEqual(poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_X],
                dfUpperLeftX) &&
            CPLIsEqual(poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y],
                dfUpperLeftY)) {

            return poRTDS;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           CacheTile()                                */
/************************************************************************/

void PostGISRasterDataset::CacheTile(const char* pszMetadata,
                                     const char* pszRaster,
                                     const char *pszPKID,
                                     int nBand,
                                     int bAllBandCaching)
{
    /**
    * Get metadata record and unpack it
    **/
    char* pszRes = CPLStrdup(pszMetadata);

    // Skip first "("
    char* pszFilteredRes = pszRes + 1;

    // Skip last ")"
    pszFilteredRes[strlen(pszFilteredRes)-1] = '\0';

    // Tokenize
    char** papszParams =
        CSLTokenizeString2(pszFilteredRes, ",",
            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS);
    CPLAssert(CSLCount(papszParams) >= ELEMENTS_OF_METADATA_RECORD);

    CPLFree(pszRes);

    double dfTilePixelSizeX = CPLAtof(papszParams[POS_UPPERLEFTX]);
    double dfTilePixelSizeY = CPLAtof(papszParams[POS_UPPERLEFTY]);
    int nTileXSize = atoi(papszParams[POS_WIDTH]);
    int nTileYSize = atoi(papszParams[POS_HEIGHT]);

    CSLDestroy(papszParams);
    papszParams = NULL;

    /**
        * Get actual raster band data
        **/
    int nBandDataTypeSize = GDALGetDataTypeSize(GetRasterBand(nBand)->GetRasterDataType()) / 8;
    int nExpectedBandDataSize =
        nTileXSize * nTileYSize * nBandDataTypeSize;

    int nWKBLength = 0;
    GByte* pbyData = CPLHexToBinary(pszRaster, &nWKBLength);

    int nExpectedBands = bAllBandCaching ? GetRasterCount() : 1;
    int nExpectedWKBLength = RASTER_HEADER_SIZE + BAND_SIZE(nBandDataTypeSize, nExpectedBandDataSize) * nExpectedBands;
    if( nWKBLength != nExpectedWKBLength )
    {
        CPLDebug("PostGIS_Raster", "nWKBLength=%d, nExpectedWKBLength=%d", nWKBLength, nExpectedWKBLength );
        CPLFree(pbyData);
        return;
    }

    // Do byte-swapping if necessary */
    int bIsLittleEndian = (pbyData[0] == 1);
#ifdef CPL_LSB
    int bSwap = !bIsLittleEndian;
#else
    int bSwap = bIsLittleEndian;
#endif

    PostGISRasterTileDataset* poRTDS = NULL;
    if( GetPrimaryKeyRef() != NULL )
        poRTDS = GetMatchingSourceRef(pszPKID);
    else
        poRTDS = GetMatchingSourceRef(dfTilePixelSizeX,
                                      dfTilePixelSizeY);
    if( poRTDS == NULL )
    {
        CPLFree(pbyData);
        return;
    }

    for(int k=1; k <= nExpectedBands; k++)
    {
        GByte* pbyDataToRead = (GByte*)GET_BAND_DATA(pbyData, k,
                nBandDataTypeSize, nExpectedBandDataSize);

        if( bSwap && nBandDataTypeSize > 1 )
        {
            GDALSwapWords( pbyDataToRead, nBandDataTypeSize,
                        nTileXSize * nTileYSize,
                        nBandDataTypeSize );
        }

        /**
        * Get the right PostGISRasterRasterBand
        **/
        int nCurBand = ( nExpectedBands > 1 ) ? k : nBand;

        /**
        * Get the right tileband
        **/
        GDALRasterBand * poRTB = poRTDS->GetRasterBand(nCurBand);

        /**
        * Manually add each tile data to the cache of the
        * matching PostGISRasterTileRasterBand.
        **/
        if (poRTB)
        {
            GDALRasterBlock* poBlock = poRTB->GetLockedBlockRef(0, 0, TRUE);
            if( poBlock != NULL )
            {
                // Point block data ref to fetched data
                memcpy(poBlock->GetDataRef(), (void *)pbyDataToRead,
                    nExpectedBandDataSize);

                poBlock->DropLock();
            }
        }
    }

    CPLFree(pbyData);
}

/************************************************************************/
/*                          LoadSources()                               */
/************************************************************************/

GBool PostGISRasterDataset::LoadSources(int nXOff, int nYOff, int nXSize, int nYSize, int nBand)
{
    if( !bBuildQuadTreeDynamically )
        return false;

    CPLString osCommand;
    CPLString osSpatialFilter;
    CPLString osIDsToFetch;
    PGresult * poResult;

    int bFetchAll = FALSE;
    if( nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize && nYSize == nRasterYSize )
    {
        bFetchAll = TRUE;
    }
    else
    {
        double adfProjWin[8];
        PolygonFromCoords(nXOff, nYOff, nXOff + nXSize, nYOff + nYSize, adfProjWin);
        osSpatialFilter.Printf("%s && "
                        "ST_GeomFromText('POLYGON((%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f))') ",
                        //"AND ST_Intersects(%s, ST_GeomFromEWKT('SRID=%d;POLYGON((%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f,%.18f %.18f))'))",
                        pszColumn,
                        adfProjWin[0], adfProjWin[1],
                        adfProjWin[2], adfProjWin[3],
                        adfProjWin[4], adfProjWin[5],
                        adfProjWin[6], adfProjWin[7],
                        adfProjWin[0], adfProjWin[1]
                        /*,pszColumn, nSrid,
                        adfProjWin[0], adfProjWin[1],
                        adfProjWin[2], adfProjWin[3],
                        adfProjWin[4], adfProjWin[5],
                        adfProjWin[6], adfProjWin[7],
                        adfProjWin[0], adfProjWin[1]*/);
    }

    int bLoadRasters = FALSE;
    int bAllBandCaching = FALSE;

    if( m_nTiles > 0 && !bFetchAll )
    {
        osCommand.Printf("SELECT %s FROM %s.%s",
                        pszPrimaryKeyName, pszSchema, pszTable);
        osCommand += " WHERE ";
        osCommand += osSpatialFilter;

        osSpatialFilter = "";

        poResult = PQexec(poConn, osCommand.c_str());

    #ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::LoadSources(): Query = \"%s\" --> number of rows = %d",
            osCommand.c_str(), poResult ? PQntuples(poResult) : 0 );
    #endif

        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) < 0) {

            if (poResult)
                PQclear(poResult);

            CPLError(CE_Failure, CPLE_AppDefined,
                "PostGISRasterDataset::LoadSources(): %s",
                PQerrorMessage(poConn));

            return false;
        }

        if( bTilesSameDimension && nBand > 0 )
        {
            GIntBig nMemoryRequiredForTiles = static_cast<GIntBig>(PQntuples(poResult)) * nTileWidth * nTileHeight *
                (GDALGetDataTypeSize(GetRasterBand(nBand)->GetRasterDataType()) / 8);
            GIntBig nCacheMax = (GIntBig) GDALGetCacheMax64();
            if( nBands * nMemoryRequiredForTiles <= nCacheMax )
            {
                bLoadRasters = TRUE;
                bAllBandCaching = TRUE;
            }
            else if( nMemoryRequiredForTiles <= nCacheMax )
            {
                bLoadRasters = TRUE;
            }
        }

        int i;
        for(i=0;i<PQntuples(poResult);i++)
        {
            const char* pszPKID = PQgetvalue(poResult, i, 0);
            PostGISRasterTileDataset *poTile = GetMatchingSourceRef(pszPKID);
            int bFetchTile = FALSE;
            if( poTile == NULL )
                bFetchTile = TRUE;
            else if( bLoadRasters )
            {
                PostGISRasterTileRasterBand* poTileBand =
                    (PostGISRasterTileRasterBand *)poTile->GetRasterBand(nBand);
                if( !poTileBand->IsCached() )
                    bFetchTile = TRUE;
            }
            if( bFetchTile )
            {
                if( osIDsToFetch.size() != 0 )
                    osIDsToFetch += ",";
                osIDsToFetch += "'";
                osIDsToFetch += pszPKID;
                osIDsToFetch += "'";
            }
        }

        PQclear(poResult);
    }

    if( bFetchAll || osIDsToFetch.size() != 0 || osSpatialFilter.size() != 0 )
    {
        osCommand.Printf("SELECT %s, ST_Metadata(%s)",
                         pszPrimaryKeyName, pszColumn);
        if( bLoadRasters )
        {
            if( bAllBandCaching )
                osCommand += CPLSPrintf(", %s", pszColumn);
            else
                osCommand += CPLSPrintf(", ST_Band(%s, %d)", pszColumn, nBand);
        }
        osCommand += CPLSPrintf(" FROM %s.%s",
                                pszSchema, pszTable);
        if( osIDsToFetch.size() != 0 )
        {
            osCommand += " WHERE ";
            osCommand += pszPrimaryKeyName;
            osCommand += " IN (";
            osCommand += osIDsToFetch;
            osCommand += ")";
        }
        else if ( osSpatialFilter.size() != 0 )
        {
            osCommand += " WHERE ";
            osCommand += osSpatialFilter;
        }

        poResult = PQexec(poConn, osCommand.c_str());

#ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::LoadSources(): Query = \"%s\" --> number of rows = %d",
            osCommand.c_str(), poResult ? PQntuples(poResult) : 0 );
#endif

        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) < 0) {

            if (poResult)
                PQclear(poResult);

            CPLError(CE_Failure, CPLE_AppDefined,
                "PostGISRasterDataset::LoadSources(): %s",
                PQerrorMessage(poConn));

            return false;
        }

        for(int i=0;i<PQntuples(poResult);i++)
        {
            const char* pszPKID = PQgetvalue(poResult, i, 0);
            const char* pszMetadata = PQgetvalue(poResult, i, 1);

            PostGISRasterTileDataset* poRTDS = GetMatchingSourceRef(pszPKID);
            if( poRTDS == NULL )
            {
                poRTDS = BuildRasterTileDataset(
                        pszMetadata, pszPKID, GetRasterCount(), NULL);
                if( poRTDS != NULL )
                {
                    if( AddComplexSource(poRTDS) )
                    {
                        oMapPKIDToRTDS[poRTDS->pszPKID] = poRTDS;
                        papoSourcesHolders = (PostGISRasterTileDataset**)
                            CPLRealloc(papoSourcesHolders, sizeof(PostGISRasterTileDataset*) * (m_nTiles + 1));
                        papoSourcesHolders[m_nTiles ++] = poRTDS;
                        CPLQuadTreeInsert(hQuadTree, poRTDS);
                    }
                    else
                    {
                        delete poRTDS;
                        poRTDS = NULL;
                    }
                }
            }

            if( bLoadRasters && poRTDS != NULL )
            {
                const char* pszRaster = PQgetvalue(poResult, i, 2);
                CacheTile(pszMetadata, pszRaster, pszPKID, nBand, bAllBandCaching);
            }
        }

        PQclear(poResult);
    }

    // If we have fetched the surface of all the dataset, then all sources have
    // been built, and we don't need to do a spatial query on following IRasterIO() calls
    if( bFetchAll )
        bBuildQuadTreeDynamically = false;

    return true;
}

/***********************************************************************
 * \brief Get some useful metadata for all bands
 *
 * The allocated memory is responsibility of the caller
 **********************************************************************/
BandMetadata * PostGISRasterDataset::GetBandsMetadata(int * pnBands)
{
    BandMetadata * poBMD = NULL;
    PGresult * poResult = NULL;
    CPLString osCommand;
    char * pszRes = NULL;
    char * pszFilteredRes = NULL;
    char ** papszParams = NULL;

    osCommand.Printf("select st_bandmetadata(%s, band) from "
      "(select %s, generate_series(1, %d) band from "
      "(select %s from %s.%s where (%s) AND st_numbands(%s)=%d limit 1) bar) foo", pszColumn,
      pszColumn, nBandsToCreate, pszColumn, pszSchema, pszTable, pszWhere ? pszWhere : "true",
      pszColumn, nBandsToCreate);

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::GetBandsMetadata(): Query: %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());
    /* Error getting info from database */
    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0) {

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error getting band metadata while creating raster "
            "bands");

        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::GetBandsMetadata(): %s",
            PQerrorMessage(poConn));

        if (poResult)
            PQclear(poResult);

        return NULL;
    }

    // Matches nBands
    int nTuples = PQntuples(poResult);

    poBMD = (BandMetadata *)VSI_MALLOC2_VERBOSE(nTuples, sizeof(BandMetadata));
    if (poBMD == NULL) {
        PQclear(poResult);

        return NULL;
    }


    int iBand = 0;

    for(iBand = 0; iBand < nTuples; iBand++) {

        // Get metadata record
        pszRes = CPLStrdup(PQgetvalue(poResult, iBand, 0));

        // Skip first "("
        pszFilteredRes = pszRes + 1;

        // Skip last ")"
        pszFilteredRes[strlen(pszFilteredRes)-1] = '\0';

        // Tokenize
        papszParams =
            CSLTokenizeString2(pszFilteredRes, ",", CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS);
        CPLAssert(CSLCount(papszParams) >= ELEMENTS_OF_BAND_METADATA_RECORD);

        CPLFree(pszRes);

        // If the band doesn't have nodata, NULL is returned as nodata
        TranslateDataType(papszParams[POS_PIXELTYPE],
            &(poBMD[iBand].eDataType), &(poBMD[iBand].nBitsDepth),
            &(poBMD[iBand].bSignedByte));

        if (papszParams[POS_NODATAVALUE] == NULL ||
            EQUAL(papszParams[POS_NODATAVALUE], "NULL") ||
            EQUAL(papszParams[POS_NODATAVALUE], "f") ||
            EQUAL(papszParams[POS_NODATAVALUE], "")) {

            poBMD[iBand].bHasNoDataValue = false;
            poBMD[iBand].dfNoDataValue = CPLAtof(NO_VALID_RES);
        }

        else {
            poBMD[iBand].bHasNoDataValue = true;
            poBMD[iBand].dfNoDataValue =
                CPLAtof(papszParams[POS_NODATAVALUE]);
        }

        // TODO: Manage outdb and get path
        poBMD[iBand].bIsOffline = (papszParams[POS_ISOUTDB] != NULL) ?
            EQUAL(papszParams[POS_ISOUTDB], "t") :
            false;

        CSLDestroy(papszParams);
    }

    if (pnBands)
        *pnBands = nTuples;

    PQclear(poResult);

    return poBMD;

}

/***********************************************************************
 * \brief Function to get the bounding box of each element inserted in
 * the QuadTree index
 **********************************************************************/
static void GetTileBoundingBox(const void *hFeature,
    CPLRectObj * pBounds)
{
    PostGISRasterTileDataset * poRTD =
        (PostGISRasterTileDataset *)hFeature;

    double adfTileGeoTransform[6];
    poRTD->GetGeoTransform(adfTileGeoTransform);

    int nTileWidth = poRTD->GetRasterXSize();
    int nTileHeight = poRTD->GetRasterYSize();

    pBounds->minx = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_X];
    pBounds->maxx = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_X] +
         nTileWidth *
         adfTileGeoTransform[GEOTRSFRM_WE_RES];

    if (adfTileGeoTransform[GEOTRSFRM_NS_RES] >= 0.0) {
        pBounds->miny = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y];
        pBounds->maxy = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
            nTileHeight *
            adfTileGeoTransform[GEOTRSFRM_NS_RES];
    }
    else {
        pBounds->maxy = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y];
        pBounds->miny = adfTileGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
            nTileHeight *
            adfTileGeoTransform[GEOTRSFRM_NS_RES];
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster", "TileBoundingBox minx=%f miny=%f maxx=%f maxy=%f adfTileGeoTransform[GEOTRSFRM_NS_RES]=%f",
             pBounds->minx, pBounds->miny, pBounds->maxx, pBounds->maxy, adfTileGeoTransform[GEOTRSFRM_NS_RES]);
#endif

    return;
}

/********************************************************
 * \brief Builds a PostGISRasterTileDataset* object from the ST_Metadata
 ********************************************************/
PostGISRasterTileDataset* PostGISRasterDataset::BuildRasterTileDataset(const char* pszMetadata,
                                                                       const char* pszPKID,
                                                                       int nBandsFetched,
                                                                       BandMetadata * poBandMetaData)
{
    // Get metadata record
    char* pszRes = CPLStrdup(pszMetadata);

    // Skip first "("
    char* pszFilteredRes = pszRes + 1;

    // Skip last ")"
    pszFilteredRes[strlen(pszFilteredRes)-1] = '\0';

    // Tokenize
    char** papszParams =
        CSLTokenizeString2(pszFilteredRes, ",",
            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS);
    CPLAssert(CSLCount(papszParams) >= ELEMENTS_OF_METADATA_RECORD);

    CPLFree(pszRes);

    double tileSkewX = CPLAtof(papszParams[POS_SKEWX]);
    double tileSkewY = CPLAtof(papszParams[POS_SKEWY]);

    // Rotated rasters are not allowed, so far
    // TODO: allow them
    if (!CPLIsEqual(tileSkewX, 0.0) ||
        !CPLIsEqual(tileSkewY, 0.0)) {

        ReportError(CE_Failure, CPLE_AppDefined,
            "GDAL PostGIS Raster driver can not work with "
            "rotated rasters yet.");

        CSLDestroy(papszParams);
        return NULL;
    }

    int l_nTileWidth = atoi(papszParams[POS_WIDTH]);
    int l_nTileHeight = atoi(papszParams[POS_HEIGHT]);

    /**
        * Now, construct a PostGISRasterTileDataset, and add
        * its bands as sources for the general raster bands
        **/
    int nTileBands = atoi(papszParams[POS_NBANDS]);

        /**
        * If the source doesn't have the same number of bands than
        * the raster band, is discarded
        **/
    if (nTileBands != nBandsFetched) {
        CPLDebug("PostGIS_Raster", "PostGISRasterDataset::"
            "BuildRasterTileDataset(): Tile has %d "
            "bands, and the raster has %d bands. Discarding "
            "this tile", nTileBands, nBandsFetched);

        CSLDestroy(papszParams);

        return NULL;
    }

    PostGISRasterTileDataset* poRTDS =
        new PostGISRasterTileDataset(this, l_nTileWidth, l_nTileHeight);

    if (GetPrimaryKeyRef() != NULL)
    {
        poRTDS->pszPKID = CPLStrdup(pszPKID);
    }

    poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_X]=
        CPLAtof(papszParams[POS_UPPERLEFTX]);

    poRTDS->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y]=
        CPLAtof(papszParams[POS_UPPERLEFTY]);

    poRTDS->adfGeoTransform[GEOTRSFRM_WE_RES]=
        CPLAtof(papszParams[POS_SCALEX]);

    poRTDS->adfGeoTransform[GEOTRSFRM_NS_RES]=
        CPLAtof(papszParams[POS_SCALEY]);

    // TODO: outdb bands should be handled. Not a priority.
    for(int j = 0; j < nTileBands; j++) {

        // Create band
        poRTDS->SetBand(j + 1,
            new PostGISRasterTileRasterBand(
                poRTDS,
                j + 1, (poBandMetaData ) ? poBandMetaData[j].eDataType : GetRasterBand(j+1)->GetRasterDataType(),
                (poBandMetaData ) ? poBandMetaData[j].bIsOffline : FALSE));
    }

    CSLDestroy(papszParams);

    return poRTDS;
}


/********************************************************
 * \brief Updates components GEOTRSFRM_WE_RES and GEOTRSFRM_NS_RES
 *        of dataset adfGeoTransform
 ********************************************************/
void PostGISRasterDataset::UpdateGlobalResolutionWithTileResolution(
        double tilePixelSizeX, double tilePixelSizeY)
{
    // Calculate pixel size
    if (resolutionStrategy == AVERAGE_RESOLUTION ||
        resolutionStrategy == AVERAGE_APPROX_RESOLUTION) {
        adfGeoTransform[GEOTRSFRM_WE_RES] += tilePixelSizeX;
        adfGeoTransform[GEOTRSFRM_NS_RES] += tilePixelSizeY;
    }

    else if (resolutionStrategy == HIGHEST_RESOLUTION)  {
        adfGeoTransform[GEOTRSFRM_WE_RES] =
            MIN(adfGeoTransform[GEOTRSFRM_WE_RES],
                tilePixelSizeX);

        /**
            * Yes : as ns_res is negative, the highest resolution
            * is the max value.
            *
            * Negative tilePixelSizeY means that the coords origin
            * is in top left corner. This is not the common
            * situation. Most image files store data from top to
            * bottom, while the projected coordinate systems
            * utilize traditional Cartesian coordinates with the
            * origin in the conventional lower-left corner (bottom
            * to top). For that reason, this parameter is normally
            * negative.
            **/
        if (tilePixelSizeY < 0.0)
            adfGeoTransform[GEOTRSFRM_NS_RES] =
                MAX(adfGeoTransform[GEOTRSFRM_NS_RES],
                tilePixelSizeY);
        else
            adfGeoTransform[GEOTRSFRM_NS_RES] =
                MIN(adfGeoTransform[GEOTRSFRM_NS_RES],
                tilePixelSizeY);
    }

    else if (resolutionStrategy == LOWEST_RESOLUTION) {
        adfGeoTransform[GEOTRSFRM_WE_RES] =
            MAX(adfGeoTransform[GEOTRSFRM_WE_RES],
                tilePixelSizeX);

        if (tilePixelSizeY < 0.0)
            adfGeoTransform[GEOTRSFRM_NS_RES] =
                MIN(adfGeoTransform[GEOTRSFRM_NS_RES],
                tilePixelSizeY);
        else
            adfGeoTransform[GEOTRSFRM_NS_RES] =
                MAX(adfGeoTransform[GEOTRSFRM_NS_RES],
                tilePixelSizeY);
    }

}

/***********************************************************************
 * \brief Build bands
 ***********************************************************************/
void PostGISRasterDataset::BuildBands(BandMetadata * poBandMetaData,
                                      int nBandsFetched)
{
#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
        "Now constructing the raster dataset bands");
#endif

    int iBand;
    for (iBand = 0; iBand < nBandsFetched; iBand++) {

        SetBand(iBand + 1, new PostGISRasterRasterBand(this, iBand + 1,
            poBandMetaData[iBand].eDataType,
            poBandMetaData[iBand].bHasNoDataValue,
            poBandMetaData[iBand].dfNoDataValue,
            poBandMetaData[iBand].bIsOffline));

        // Set some band metadata items
        GDALRasterBand * b = GetRasterBand(iBand + 1);

        if (poBandMetaData[iBand].bSignedByte) {
            b->SetMetadataItem(
                "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
        }

        if (poBandMetaData[iBand].nBitsDepth < 8) {
            b->SetMetadataItem(
            "NBITS", CPLString().Printf( "%d",
                poBandMetaData[iBand].nBitsDepth ),
            "IMAGE_STRUCTURE" );
        }

#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
            "Band %d built", iBand + 1);
#endif
    }
}

/***********************************************************************
 * \brief Construct just one dataset from all the results fetched.
 *
 * This method is not very elegant. It's strongly attached to
 * SetRasterProperties (it assumes poResult is not NULL, and the actual
 * results are stored at fixed positions). I just did it to avoid a
 * huge SetRasterProperties method.
 *
 * I know, this could be avoided in a better way. Like implementing a
 * wrapper to raise queries and get results without all the checking
 * overhead. I'd like to do it, someday...
 **********************************************************************/
GBool PostGISRasterDataset::ConstructOneDatasetFromTiles(
    PGresult * poResult)
{

    /*******************************************************************
     * We first get the band metadata. So we'll can use it as metadata
     * for all the sources.
     *
     * We just fetch the band metadata from 1 tile. So, we assume that:
     * - All the bands have the same data type
     * - All the bands have the same NODATA value
     *
     * It's user's responsibility to ensure the requested table fit in
     * this schema. He/she may use the 'where' clause to ensure this
     ******************************************************************/
    int nBandsFetched = 0;
    BandMetadata * poBandMetaData = GetBandsMetadata(&nBandsFetched);

    /*******************************************************************
     * Now, we can iterate over the input query's results (metadata
     * from all the database tiles).
     *
     * In this iteration, we will construct the dataset GeoTransform
     * array and we will add each tile's band as source for each of our
     * rasterbands.
     ******************************************************************/
    int l_nTiles = PQntuples(poResult);

    adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = xmin;

    int nField = (GetPrimaryKeyRef() != NULL) ? 1 : 0;
    /**
     * Optimization - Just one tile: The dataset's geotransform is the
     * tile's geotransform. And we don't need to construct sources for
     * the raster bands. We just read from the unique tile we have. So,
     * we avoid all the VRT stuff.
     *
     * TODO: For some reason, the implementation of IRasterIO in
     * PostGISRasterRasterBand class causes a segmentation fault when
     * tries to call GDALRasterBand::IRasterIO. This call intends to
     * delegate the I/O in the parent class, that will call
     * PostGISRasterRasterBand::IReadBlock.
     *
     * If you avoid PostGISRasterRasterBand::IRasterIO method, the
     * IReadBlock method is directly call and it works fine.
     *
     * Right now, we avoid this optimization by making the next boolean
     * variable always true.
     **/
    //GBool bNeedToConstructSourcesHolders = (l_nTiles > 1);

    // As the optimization is not working, we avoid it
    GBool bNeedToConstructSourcesHolders = true;

#ifdef notdef
    // This won't be called if the optimization is disabled
    if (!bNeedToConstructSourcesHolders)
    {
        // Get metadata record
        char* pszRes = CPLStrdup(PQgetvalue(poResult, 0, nField));

        // Skip first "("
        char* pszFilteredRes = pszRes + 1;

        // Skip last ")"
        pszFilteredRes[strlen(pszFilteredRes)-1] = '\0';

        // Tokenize
        char** papszParams =
            CSLTokenizeString2(pszFilteredRes, ",", CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS);
        CPLAssert(CSLCount(papszParams) >= ELEMENTS_OF_METADATA_RECORD);

        CPLFree(pszRes);

        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] =
            CPLAtof(papszParams[POS_SKEWX]);

        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] =
            CPLAtof(papszParams[POS_SKEWY]);

        // Rotated rasters are not allowed, so far
        // TODO: allow them
        if (!CPLIsEqual(adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1], 0.0) ||
            !CPLIsEqual(adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2], 0.0)) {

            ReportError(CE_Failure, CPLE_AppDefined,
                    "GDAL PostGIS Raster driver can not work with "
                    "rotated rasters yet.");

            VSIFree(poBandMetaData);
            CSLDestroy(papszParams);

            return false;
        }

        /**
         * We override user resolution. It only makes sense in case we
         * have several tiles with different resolutions
         **/
        adfGeoTransform[GEOTRSFRM_WE_RES] =
            CPLAtof(papszParams[POS_SCALEX]);
        adfGeoTransform[GEOTRSFRM_NS_RES] =
            CPLAtof(papszParams[POS_SCALEY]);

        CSLDestroy(papszParams);

    }

    /**
     * Several tiles: construct the dataset from metadata of all tiles,
     * and create PostGISRasterTileDataset objects, to hold the
     * PostGISRasterTileRasterBands objects that will be used as sources
     **/
    else
#endif
    {
        int i;

#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
            "Constructing one dataset from %d tiles", l_nTiles);
#endif

        papoSourcesHolders = (PostGISRasterTileDataset **)
            VSI_CALLOC_VERBOSE(l_nTiles, sizeof(PostGISRasterTileDataset *));

        if (papoSourcesHolders == NULL) {
            VSIFree(poBandMetaData);

            return false;
        }

        int nValidTiles = 0;
        for(i = 0; i < l_nTiles; i++)
        {
            PostGISRasterTileDataset* poRTDS =
                BuildRasterTileDataset(PQgetvalue(poResult, i, nField),
                                       (GetPrimaryKeyRef() != NULL) ? PQgetvalue(poResult, i, 0) : NULL,
                                       nBandsFetched,
                                       poBandMetaData);
            if( poRTDS == NULL )
                continue;

            if( nOverviewFactor == 1 && resolutionStrategy != USER_RESOLUTION )
            {
                double tilePixelSizeX = poRTDS->adfGeoTransform[GEOTRSFRM_WE_RES];
                double tilePixelSizeY = poRTDS->adfGeoTransform[GEOTRSFRM_NS_RES];

                if( nValidTiles == 0 )
                {
                    adfGeoTransform[GEOTRSFRM_WE_RES] = tilePixelSizeX;
                    adfGeoTransform[GEOTRSFRM_NS_RES] = tilePixelSizeY;
                }
                else
                {
                    UpdateGlobalResolutionWithTileResolution(tilePixelSizeX,
                                                            tilePixelSizeY);
                }
            }

            papoSourcesHolders[nValidTiles++] = poRTDS;

        } // end for

        l_nTiles = nValidTiles;

        if( nOverviewFactor > 1 )
        {
            adfGeoTransform[GEOTRSFRM_WE_RES] = poParentDS->adfGeoTransform[GEOTRSFRM_WE_RES] * nOverviewFactor;
            adfGeoTransform[GEOTRSFRM_NS_RES] = poParentDS->adfGeoTransform[GEOTRSFRM_NS_RES] * nOverviewFactor;
        }
        else if ((resolutionStrategy == AVERAGE_RESOLUTION ||
             resolutionStrategy == AVERAGE_APPROX_RESOLUTION) && l_nTiles > 0) {
            adfGeoTransform[GEOTRSFRM_WE_RES] /= l_nTiles;
            adfGeoTransform[GEOTRSFRM_NS_RES] /= l_nTiles;
        }

    } // end else

    /**
     * Complete the rest of geotransform parameters
     **/
    if (adfGeoTransform[GEOTRSFRM_NS_RES] >= 0.0)
        adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = ymin;
    else
        adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = ymax;

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
        "GeoTransform array = (%f, %f, %f, %f, %f, %f)",
        adfGeoTransform[GEOTRSFRM_TOPLEFT_X],
        adfGeoTransform[GEOTRSFRM_WE_RES],
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1],
        adfGeoTransform[GEOTRSFRM_TOPLEFT_Y],
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2],
        adfGeoTransform[GEOTRSFRM_NS_RES]);
#endif

    // Calculate the raster size from the geotransform array
    nRasterXSize = (int)
        fabs(rint((xmax - xmin) /
            adfGeoTransform[GEOTRSFRM_WE_RES]));

    nRasterYSize = (int)
        fabs(rint((ymax - ymin) /
            adfGeoTransform[GEOTRSFRM_NS_RES]));

#ifdef DEBUG_VERBOSE
    CPLDebug( "PostGIS_Raster",
        "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
        "Raster size: (%d, %d), ",nRasterXSize, nRasterYSize);
#endif

    if (nRasterXSize <= 0 || nRasterYSize <= 0) {
        ReportError(CE_Failure, CPLE_AppDefined,
            "Computed PostGIS Raster dimension is invalid. You've "
            "probably specified inappropriate resolution." );

        VSIFree(poBandMetaData);
        return false;
    }

    /*******************************************************************
     * Now construct the dataset bands
     ******************************************************************/
    BuildBands(poBandMetaData, nBandsFetched);

    // And free bandmetadata
    VSIFree(poBandMetaData);

    /*******************************************************************
     * Finally, add complex sources and create a quadtree index for them
     ******************************************************************/
    if (bNeedToConstructSourcesHolders) {
#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
            "Finally, adding sources for bands");
#endif
        for(int iSource = 0; iSource < l_nTiles; iSource++)
        {
            PostGISRasterTileDataset* poRTDS =papoSourcesHolders[iSource];
            if (!AddComplexSource(poRTDS))
            {
                CPLDebug("PostGIS_Raster",
                    "PostGISRasterDataset::ConstructOneDatasetFromTiles:"
                    "Bounding box of tile %d does not intersect the "
                    "bounding box of dataset ", iSource);

                continue;
            }
            if( poRTDS->pszPKID != NULL )
                oMapPKIDToRTDS[poRTDS->pszPKID] = poRTDS;
            CPLQuadTreeInsert(hQuadTree, poRTDS);
        }
    }

    return true;
}



/***********************************************************************
 * \brief Construct subdatasets and show them.
 *
 * This method is not very elegant. It's strongly attached to
 * SetRasterProperties (it assumes poResult is not NULL, and the actual
 * results are stored at fixed positions). I just did it to avoid a
 * huge SetRasterProperties method.
 *
 * I know, this could be avoided in a better way. Like implementing a
 * wrapper to raise queries and get results without all the checking
 * overhead. I'd like to do it, someday...
 **********************************************************************/
GBool PostGISRasterDataset::YieldSubdatasets(PGresult * poResult,
const char * pszValidConnectionString)
{
    int l_nTiles = PQntuples(poResult);
    int i = 0;
    double dfTileUpperLeftX = 0;
    double dfTileUpperLeftY = 0;

    papszSubdatasets = (char**)VSICalloc(2 * l_nTiles + 1, sizeof(char*));
    if( papszSubdatasets == NULL )
        return false;

    // Subdatasets identified by primary key
    if (GetPrimaryKeyRef() != NULL) {

        for(i = 0; i < l_nTiles; i++) {

            const char* pszId = PQgetvalue(poResult, i, 0);

            papszSubdatasets[2 * i] =
                CPLStrdup(CPLSPrintf("SUBDATASET_%d_NAME=PG:%s schema=%s table=%s column=%s "
                    "where='%s = %s'", i+ 1, pszValidConnectionString,
                    pszSchema, pszTable, pszColumn, pszPrimaryKeyName,
                    pszId));

            papszSubdatasets[2 * i + 1] =
                CPLStrdup(CPLSPrintf("SUBDATASET_%d_DESC=PostGIS Raster at %s.%s (%s), with %s = %s",
                    i + 1, pszSchema, pszTable, pszColumn, pszPrimaryKeyName,
                    pszId));
        }
    }

    // Subdatasets identified by upper left pixel
    else {

        char * pszRes;
        char * pszFilteredRes;
        char ** papszParams;

        for(i = 0; i < l_nTiles; i++) {

            pszRes = CPLStrdup(PQgetvalue(poResult, i, 0));

            // Skip first "("
            pszFilteredRes = pszRes + 1;

            // Skip last ")"
            pszFilteredRes[strlen(pszFilteredRes)-1] = '\0';

            // Tokenize
            papszParams =
                CSLTokenizeString2(pszFilteredRes, ",",
                    CSLT_HONOURSTRINGS);

            CPLFree(pszRes);

            dfTileUpperLeftX = CPLAtof(papszParams[POS_UPPERLEFTX]);
            dfTileUpperLeftY = CPLAtof(papszParams[POS_UPPERLEFTY]);

            papszSubdatasets[2 * i] =
                CPLStrdup(CPLSPrintf("SUBDATASET_%d_NAME=PG:%s schema=%s table=%s column=%s "
                    "where='abs(ST_UpperLeftX(%s) - %.8f) < 1e-8 AND "
                    "abs(ST_UpperLeftY(%s) - %.8f) < 1e-8'",
                    i + 1, pszValidConnectionString, pszSchema, pszTable,
                    pszColumn, pszColumn, dfTileUpperLeftX, pszColumn,
                    dfTileUpperLeftY));

             papszSubdatasets[2 * i + 1] =
                CPLStrdup(CPLSPrintf("SUBDATASET_%d_DESC=PostGIS Raster at %s.%s (%s), "
                "UpperLeft = %.8f, %.8f", i + 1, pszSchema, pszTable, pszColumn,
                dfTileUpperLeftX, dfTileUpperLeftY));

            CSLDestroy(papszParams);
        }
    }

    /**
     * Not a single raster fetched. Not really needed. Just to keep code clean
     **/
    nRasterXSize = 0;
    nRasterYSize = 0;
    adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = 0.0;
    adfGeoTransform[GEOTRSFRM_WE_RES] = 1.0;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0.0;
    adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = 0.0;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0.0;
    adfGeoTransform[GEOTRSFRM_NS_RES] = -1.0;

    return true;
}



/***********************************************************************
 * \brief Set the general raster properties.
 *
 * This method is called when the driver working mode is
 * ONE_RASTER_PER_ROW or ONE_RASTER_PER_TABLE.
 *
 * We must distinguish between tiled and untiled raster coverages. In
 * PostGIS Raster, there's no real difference between 'tile' and
 * 'raster'. There's only 'raster objects'. Each record of a raster
 * table is a raster object, and has its own georeference information,
 * whether if the record is a tile of a bigger raster coverage or is a
 * complete raster. So, <b>there's no a way of knowing if the rows of a
 * raster table are related or not</b>. It's user's responsibility, and
 * it's managed by 'mode' parameter in connection string, which
 * determines the driver working mode.
 *
 * The user is responsible to ensure that the raster layer meets the
 * minimum topological requirements for analysis. The ideal case is when
 * all the raster tiles of a continuous layer are the same size, snap to
 * the same grid and do not overlap.
 *
 **********************************************************************/
GBool PostGISRasterDataset::SetRasterProperties
    (const char * pszValidConnectionString)
{
    PGresult* poResult = NULL;
    CPLString osCommand;
    GBool bDataFoundInRasterColumns = false;
    GBool bNeedToCheckWholeTable = false;

    /*******************************************************************
     * Get the extent and the maximum number of bands of the requested
     * raster-
     *
     * TODO: The extent of rotated rasters could be a problem. We will
     * need a ST_RotatedExtent function in PostGIS. Without that
     * function, we should not allow rotated rasters.
     ******************************************************************/
    if (pszWhere != NULL) {
        osCommand.Printf(
            "select srid, nbband, ST_XMin(geom) as xmin, "
            "ST_XMax(geom) as xmax, ST_YMin(geom) as ymin, "
            "ST_YMax(geom) as ymax, scale_x, scale_y from (select ST_SRID(%s) srid, "
            "ST_Extent(%s::geometry) geom, max(ST_NumBands(%s)) "
            "nbband, avg(ST_ScaleX(%s)) scale_x, avg(ST_ScaleY(%s)) scale_y from %s.%s where %s group by ST_SRID(%s)) foo",
            pszColumn, pszColumn, pszColumn, pszColumn, pszColumn,
            pszSchema, pszTable, pszWhere,
            pszColumn);

#ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::SetRasterProperties(): First query: %s",
        osCommand.c_str());
#endif

        poResult = PQexec(poConn, osCommand.c_str());
    }

    else {

        /**
         * Optimization: First, check raster_columns view (it makes
         * things faster. See ticket #5046)
         *
         * This can only be applied if we don't have a 'where' clause,
         * because raster_columns view stores statistics about the whole
         * table. If the user specified 'where' clause is because is
         * just interested in a subset of the table rows.
         **/
        osCommand.Printf(
            "select srid, nbband, ST_XMin(geom) as xmin, ST_XMax(geom) "
            "as xmax, ST_YMin(geom) as ymin, ST_YMax(geom) as ymax, "
            "scale_x, scale_y, blocksize_x, blocksize_y, same_alignment, regular_blocking "
            "from (select srid, extent geom, num_bands nbband, "
            "scale_x, scale_y, blocksize_x, blocksize_y, same_alignment, regular_blocking from "
            "raster_columns where r_table_schema = '%s' and "
            "r_table_name = '%s' and r_raster_column = '%s' ) foo",
            pszSchema, pszTable, pszColumn);

#ifdef DEBUG_QUERY
        CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::SetRasterProperties(): First query: %s",
        osCommand.c_str());
#endif

        poResult = PQexec(poConn, osCommand.c_str());

        // Query execution error
        if(poResult == NULL ||
            PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) < 0) {

            bNeedToCheckWholeTable = true;

            if (poResult)
                PQclear(poResult);

        }

        /**
         * We didn't find anything in raster_columns view. Need to check
         * the whole table for metadata
         **/
        else if (PQntuples(poResult) == 0) {

            ReportError(CE_Warning, CPLE_AppDefined, "Cannot find "
            "information about %s.%s table in raster_columns view. The "
            "raster table load would take a lot of time. Please, "
            "execute AddRasterConstraints PostGIS function to register "
            "this table as raster table in raster_columns view. This "
            "will save a lot of time.", pszSchema, pszTable);

            PQclear(poResult);

            bNeedToCheckWholeTable = true;
        }

        /* There's a result but the row has empty values */
        else if (PQntuples(poResult) == 1 &&
                 (PQgetvalue(poResult, 0, 1)[0] == '\0' ||
                 (poParentDS == NULL &&
                  (PQgetvalue(poResult, 0, 2)[0] == '\0' ||
                  PQgetvalue(poResult, 0, 3)[0] == '\0' ||
                  PQgetvalue(poResult, 0, 4)[0] == '\0' ||
                  PQgetvalue(poResult, 0, 5)[0] == '\0'))) ) {
            ReportError(CE_Warning, CPLE_AppDefined, "Cannot find (valid) "
            "information about %s.%s table in raster_columns view. The "
            "raster table load would take a lot of time. Please, "
            "execute AddRasterConstraints PostGIS function to register "
            "this table as raster table in raster_columns view. This "
            "will save a lot of time.", pszSchema, pszTable);

            PQclear(poResult);

            bNeedToCheckWholeTable = true;
        }

        // We should check whole table but we can't
        if (bNeedToCheckWholeTable && !bCheckAllTiles) {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot find "
            "information about %s.%s table in raster_columns view. "
            "Please, execute AddRasterConstraints PostGIS function to "
            "register this table as raster table in raster_columns "
            "view. This will save a lot of time. As alternative, "
            "provide configuration option "
            "PR_ALLOW_WHOLE_TABLE_SCAN=YES. With this option, the "
            "driver will work even without the table information "
            "stored in raster_columns view, but it could perform "
            "really slow.", pszSchema, pszTable);

            PQclear(poResult);

            return false;

        }

        // We should check the whole table and we can
        else if (bNeedToCheckWholeTable) {
            osCommand.Printf(
                "select srid, nbband, st_xmin(geom) as xmin, "
                "st_xmax(geom) as xmax, st_ymin(geom) as ymin, "
                "st_ymax(geom) as ymax, scale_x, scale_y from (select st_srid(%s) srid, "
                "st_extent(%s::geometry) geom, max(ST_NumBands(%s)) "
                "nbband, avg(ST_ScaleX(%s)) scale_x, avg(ST_ScaleY(%s)) scale_y from %s.%s group by st_srid(%s)) foo",
                pszColumn, pszColumn, pszColumn, pszColumn, pszColumn, pszSchema, pszTable, pszColumn);

#ifdef DEBUG_QUERY
            CPLDebug("PostGIS_Raster",
                "PostGISRasterDataset::SetRasterProperties(): "
                "First query: %s", osCommand.c_str());
#endif

            poResult = PQexec(poConn, osCommand.c_str());
        }

        // We already found the data in raster_columns
        else {
            bDataFoundInRasterColumns = true;
        }
    }

    // Query execution error
    if(poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) < 0) {

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error browsing database for PostGIS Raster "
            "properties : %s",  PQerrorMessage(poConn));

        if (poResult != NULL)
            PQclear(poResult);

        return false;
    }

    else if (PQntuples(poResult) == 0) {
        ReportError(CE_Failure, CPLE_AppDefined,
            "No results found in %s.%s. Did you specify a 'where' "
            "clause too restrictive?", pszSchema, pszTable);

        PQclear(poResult);

        return false;
    }


    /**
     * Found more than one SRID value in the table. Not allowed.
     *
     * TODO: We could provide an extra parameter, to transform all the
     * tiles to the same SRID
     **/
    else if (PQntuples(poResult) > 1) {
        ReportError(CE_Failure, CPLE_AppDefined,
            "Error, the table %s.%s contains tiles with different "
            "srid. This feature is not yet supported by the PostGIS "
            "Raster driver. Please, specify a table that contains only "
            "tiles with the same srid or provide a 'where' constraint "
            "to select just the tiles with the same value for srid",
            pszSchema, pszTable);

        PQclear(poResult);

        return false;
    }

    // Get some information we will probably need further
    nSrid = atoi(PQgetvalue(poResult, 0, 0));
    nBandsToCreate = atoi(PQgetvalue(poResult, 0, 1));
    if( poParentDS != NULL )
    {
        /* If we are an overview of a parent dataset, we need to adjust */
        /* to its extent, otherwise IRasterIO() will not work properly */
        xmin = poParentDS->xmin;
        xmax = poParentDS->xmax;
        ymin = poParentDS->ymin;
        ymax = poParentDS->ymax;
    }
    else
    {
        xmin = CPLAtof(PQgetvalue(poResult, 0, 2));
        xmax = CPLAtof(PQgetvalue(poResult, 0, 3));
        ymin = CPLAtof(PQgetvalue(poResult, 0, 4));
        ymax = CPLAtof(PQgetvalue(poResult, 0, 5));
    }

    // Create the QuadTree object
    CPLRectObj sRect;
    sRect.minx = xmin;
    sRect.miny = ymin;
    sRect.maxx = xmax;
    sRect.maxy = ymax;
    hQuadTree = CPLQuadTreeCreate(&sRect, GetTileBoundingBox);

    double scale_x = CPLAtof(PQgetvalue(poResult, 0, 6));
    double scale_y = CPLAtof(PQgetvalue(poResult, 0, 7));
    if( nOverviewFactor > 1 && poParentDS != NULL )
    {
        scale_x = poParentDS->adfGeoTransform[GEOTRSFRM_WE_RES] * nOverviewFactor;
        scale_y = poParentDS->adfGeoTransform[GEOTRSFRM_NS_RES] * nOverviewFactor;
    }
    else if( resolutionStrategy == USER_RESOLUTION)
    {
        scale_x = adfGeoTransform[GEOTRSFRM_WE_RES];
        scale_y = adfGeoTransform[GEOTRSFRM_NS_RES];
    }

    // These fields can only be fetched from raster_columns view
    if (bDataFoundInRasterColumns)
    {
        nTileWidth = atoi(PQgetvalue(poResult, 0, 8));
        nTileHeight = atoi(PQgetvalue(poResult, 0, 9));
        if( nTileWidth != 0 && nTileHeight != 0 )
            bTilesSameDimension = true;

        bAllTilesSnapToSameGrid =
            EQUAL(PQgetvalue(poResult, 0, 10), "t");

        bRegularBlocking =
            EQUAL(PQgetvalue(poResult, 0, 11), "t");
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::SetRasterProperties: xmin = %f, "
        "xmax = %f, ymin = %f, ymax = %f, scale_x = %f, scale_y = %f",
             xmin, xmax, ymin, ymax, scale_x, scale_y);
#endif

    PQclear(poResult);

    /*******************************************************************
     * Now, we fetch the metadata of all the raster tiles in the
     * database, that will allow us to construct VRT sources to the
     * PostGIS Raster bands.
     *
     * TODO: Improve this. If we have a big amount of tiles, it can be
     * a problem.
     ******************************************************************/
    // We'll identify each tile for its primary key/unique id (ideal)
    if (GetPrimaryKeyRef() != NULL)
    {
        if (pszWhere == NULL)
        {
            /* If we don't know the pixel size, then guess it from averaging the metadata */
            /* of a maximum 10 rasters */
            if( bIsFastPK && nMode == ONE_RASTER_PER_TABLE && HasSpatialIndex() &&
                (scale_x == 0 || scale_y == 0) &&
                resolutionStrategy == AVERAGE_APPROX_RESOLUTION )
            {
                osCommand.Printf("SELECT avg(scale_x) avg_scale_x, avg(scale_y) avg_scale_y FROM "
                                 "(SELECT ST_ScaleX(%s) scale_x, ST_ScaleY(%s) scale_y FROM %s.%s LIMIT 10) foo",
                                 pszColumn, pszColumn, pszSchema, pszTable);
                #ifdef DEBUG_QUERY
                    CPLDebug("PostGIS_Raster",
                        "PostGISRasterDataset::SetRasterProperties(): Query: %s",
                        osCommand.c_str());
                #endif

                poResult = PQexec(poConn, osCommand.c_str());
                if (poResult == NULL ||
                    PQresultStatus(poResult) != PGRES_TUPLES_OK ||
                    PQntuples(poResult) <= 0) {

                    ReportError(CE_Failure, CPLE_AppDefined,
                        "Error retrieving raster metadata");

                    CPLDebug("PostGIS_Raster",
                        "PostGISRasterDataset::SetRasterProperties(): %s",
                        PQerrorMessage(poConn));

                    if (poResult != NULL)
                        PQclear(poResult);

                    return false;
                }

                scale_x = CPLAtof(PQgetvalue(poResult, 0, 0));
                scale_y = CPLAtof(PQgetvalue(poResult, 0, 1));
                CPLDebug("PostGIS_Raster",
                         "PostGISRasterDataset::SetRasterProperties: guessed: scale_x = %f, scale_y = %f",
                         scale_x, scale_y);

                PQclear(poResult);
            }

            /* If we build a raster for the whole table, than we have a spatial index,
               and a primary key, and we know the pixel size, we can build the dataset
               immediately, and IRasterIO() queries will be able to retrieve quickly
               the PKID of the tiles to fetch, so we don't need to scan the whole table */
            if( bIsFastPK && nMode == ONE_RASTER_PER_TABLE && HasSpatialIndex() &&
                scale_x != 0 && scale_y != 0 )
            {
                adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = xmin;
                adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0.0;
                adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = (scale_y < 0) ? ymax : ymin;
                adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0.0;
                adfGeoTransform[GEOTRSFRM_WE_RES] = scale_x;
                adfGeoTransform[GEOTRSFRM_NS_RES] = scale_y;

                // Calculate the raster size from the geotransform array
                nRasterXSize = (int)
                    fabs(rint((xmax - xmin) /
                        adfGeoTransform[GEOTRSFRM_WE_RES]));

                nRasterYSize = (int)
                    fabs(rint((ymax - ymin) /
                        adfGeoTransform[GEOTRSFRM_NS_RES]));

            #ifdef DEBUG_VERBOSE
                CPLDebug( "PostGIS_Raster",
                    "PostGISRasterDataset::ConstructOneDatasetFromTiles: "
                    "Raster size: (%d, %d), ",nRasterXSize, nRasterYSize);
            #endif

                if (nRasterXSize <= 0 || nRasterYSize <= 0) {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Computed PostGIS Raster dimension is invalid. You "
                        "have probably specified an inappropriate "
                        "resolution." );

                    return false;
                }

                bBuildQuadTreeDynamically = true;

                /**************************************************************
                * Now construct the dataset bands
                ***************************************************************/
                int nBandsFetched = 0;
                BandMetadata * poBandMetaData = GetBandsMetadata(&nBandsFetched);

                BuildBands(poBandMetaData, nBandsFetched);

                // And free bandmetadata
                VSIFree(poBandMetaData);

                return true;
            }

            osCommand.Printf("select %s, st_metadata(%s) from %s.%s",
                pszPrimaryKeyName, pszColumn, pszSchema, pszTable);

            // srid should not be necessary. It was previously checked
        }

        else {
            osCommand.Printf("select %s, st_metadata(%s) from %s.%s "
                "where %s", pszPrimaryKeyName, pszColumn, pszSchema,
                pszTable, pszWhere);
        }
    }

    // No primary key/unique id found. We rely on upper left pixel
    else {
        if (pszWhere == NULL) {
            osCommand.Printf("select st_metadata(%s) from %s.%s",
                pszColumn, pszSchema, pszTable);
        }

        else {
            osCommand.Printf("select st_metadata(%s) from %s.%s "
                "where %s", pszColumn, pszSchema, pszTable, pszWhere);
        }
    }

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::SetRasterProperties(): Query: %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());
    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_TUPLES_OK ||
        PQntuples(poResult) <= 0) {

        ReportError(CE_Failure, CPLE_AppDefined,
            "Error retrieving raster metadata");

        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::SetRasterProperties(): %s",
            PQerrorMessage(poConn));

        if (poResult != NULL)
            PQclear(poResult);

        return false;
    }

    // Now we know the number of tiles that form our dataset
    m_nTiles = PQntuples(poResult);

    /*******************************************************************
     * We are going to create a whole dataset as a mosaic with all the
     * tiles. We'll consider each tile as a VRT source for
     * PostGISRasterRasterBand. The data will be actually read by each
     * of these sources, and it will be cached in the sources' caches,
     * not in the PostGISRasterRasterBand cache
     ******************************************************************/
    if (m_nTiles == 1 || nMode == ONE_RASTER_PER_TABLE)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::SetRasterProperties(): "
            "Constructing one dataset from %d tiles", m_nTiles);
#endif

        GBool res = ConstructOneDatasetFromTiles(poResult);

        PQclear(poResult);

        return res;
    }


    /***************************************************************
     * One raster per row: collect subdatasets
     **************************************************************/
    else if (nMode == ONE_RASTER_PER_ROW) {
#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster",
            "PostGISRasterDataset::SetRasterProperties(): "
            "Reporting %d datasets", m_nTiles);
#endif

        GBool res = YieldSubdatasets(poResult,
            pszValidConnectionString);

        PQclear(poResult);

        return res;
    }

    /***************************************************************
     * Wrong mode: error
     **************************************************************/
    else {
        ReportError(CE_Failure, CPLE_AppDefined,
            "Wrong driver working mode. You must specify mode = 1 or "
            "mode = 2 in the connection string. Check PostGIS Raster "
            "documentation at "
            "http://trac.osgeo.org/gdal/wiki/frmts_wtkraster.html "
            "for further information about working modes.");

        PQclear(poResult);

        return false;
    }
}

/***********************************************************************
 * \brief Get the connection information for a filename.
 *
 * This method extracts these dataset parameters from the connection
 * string, if present:
 * - pszSchema: The schema where the table belongs
 * - pszTable: The table's name
 * - pszColumn: The raster column's name
 * - pszWhere: A where constraint to apply to the table's rows
 * - pszHost: The PostgreSQL host
 * - pszPort: The PostgreSQL port
 * - pszUser: The PostgreSQL user
 * - pszPassword: The PostgreSQL password
 * - nMode: The connection mode
 *
 * If any of there parameters is not present in the connection string,
 * default values are taken. nMode is deducted from the rest of
 * parameters if not provided.
 *
 * Apart from that, bBrowseDatabase is set to TRUE if the mode is
 * BROWSE_SCHEMA or BROWSE_DATABASE
 **********************************************************************/
static GBool
GetConnectionInfo(const char * pszFilename,
    char ** ppszConnectionString, char ** ppszDbname, char ** ppszSchema, char ** ppszTable,
    char ** ppszColumn, char ** ppszWhere, char ** ppszHost,
    char ** ppszPort, char ** ppszUser, char ** ppszPassword,
    WorkingMode * nMode, GBool * bBrowseDatabase)
{
    int nPos = -1, i;
    char * pszTmp = NULL;
    char **papszParams = ParseConnectionString(pszFilename);
    if (papszParams == NULL) {
        return false;
    }

    /*******************************************************************
     * Get mode:
     *  - 1. ONE_RASTER_PER_ROW: Each row is considered as a separate
     *      raster
     *  - 2. ONE_RASTER_PER_TABLE: All the table rows are considered as
     *      a whole raster coverage
     ******************************************************************/
    nPos = CSLFindName(papszParams, "mode");
    if (nPos != -1) {
        int tmp;
        tmp = atoi(CPLParseNameValue(papszParams[nPos], NULL));

        // default value
        *nMode = ONE_RASTER_PER_ROW;

        if (tmp == 2) {
            *nMode = ONE_RASTER_PER_TABLE;
        }


        /* Remove the mode from connection string */
        papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
    }
        /* Default mode */
    else
        *nMode = ONE_RASTER_PER_ROW;

    /**
     * Case 1: There's no database name: Error, you need, at least,
     * specify a database name (NOTE: insensitive search)
     **/
    nPos = CSLFindName(papszParams, "dbname");
    if (nPos == -1) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "You must specify at least a db name");

        CSLDestroy(papszParams);

        return false;
    }

    *ppszDbname =
        CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));

    /**
     * Case 2: There's database name, but no table name: activate a flag
     * for browsing the database, fetching all the schemas that contain
     * raster tables
     **/
    nPos = CSLFindName(papszParams, "table");
    if (nPos == -1) {
        *bBrowseDatabase = true;

        /* Get schema name, if exist */
        nPos = CSLFindName(papszParams, "schema");
        if (nPos != -1) {
            *ppszSchema =
                CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));

            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }

        /**
         * Remove the rest of the parameters, if exist (they must not be
         * present if we want a valid PQ connection string)
         **/
        nPos = CSLFindName(papszParams, "column");
        if (nPos != -1) {
            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }

        nPos = CSLFindName(papszParams, "where");
        if (nPos != -1) {
            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }
    } else {
        *ppszTable =
            CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));
        /* Delete this pair from params array */
        papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);

        /**
         * Case 3: There's database and table name, but no column
         * name: Use a default column name and use the table to create
         * the dataset
         **/
        nPos = CSLFindName(papszParams, "column");
        if (nPos == -1) {
            *ppszColumn = CPLStrdup(DEFAULT_COLUMN);
        }
        /**
         * Case 4: There's database, table and column name: Use the
         * table to create a dataset
         **/
        else {
            *ppszColumn =
                CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));

            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }

        /* Get the rest of the parameters, if exist */
        nPos = CSLFindName(papszParams, "schema");
        if (nPos == -1) {
            *ppszSchema = CPLStrdup(DEFAULT_SCHEMA);
        } else {
            *ppszSchema =
                CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));

            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }

        nPos = CSLFindName(papszParams, "where");
        if (nPos != -1) {
            *ppszWhere =
                CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));

            /* Delete this pair from params array */
            papszParams = CSLRemoveStrings(papszParams, nPos, 1, NULL);
        }
    }

    /* Parse ppszWhere, if needed */
    if (*ppszWhere) {
        pszTmp = ReplaceQuotes(*ppszWhere, static_cast<int>(strlen(*ppszWhere)));
        CPLFree(*ppszWhere);
        *ppszWhere = pszTmp;
    }

    /***************************************
     * Construct a valid connection string
     ***************************************/
    *ppszConnectionString = (char*) CPLCalloc(strlen(pszFilename),
            sizeof (char));
    for (i = 0; i < CSLCount(papszParams); i++) {
        *ppszConnectionString =
            strncat(*ppszConnectionString, papszParams[i],
                strlen(papszParams[i]));

        *ppszConnectionString =
            strncat(*ppszConnectionString, " ", strlen(" "));
    }

    nPos = CSLFindName(papszParams, "host");
    if (nPos != -1) {
        *ppszHost =
            CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));
    }
    else if (CPLGetConfigOption("PGHOST", NULL) != NULL) {
        *ppszHost = CPLStrdup(CPLGetConfigOption("PGHOST", NULL));
    }
    else
        *ppszHost = NULL;
    /*else {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Host parameter must be provided, or PGHOST environment "
            "variable must be set. Please set the host and try again.");

        CSLDestroy(papszParams);

        return false;
    }*/

    nPos = CSLFindName(papszParams, "port");
    if (nPos != -1) {
        *ppszPort =
            CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));
    }
    else if (CPLGetConfigOption("PGPORT", NULL) != NULL ) {
        *ppszPort = CPLStrdup(CPLGetConfigOption("PGPORT", NULL));
    }
    else
        *ppszPort = NULL;
    /*else {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Port parameter must be provided, or PGPORT environment "
            "variable must be set. Please set the port and try again.");

        CSLDestroy(papszParams);

        return false;
    }*/

    nPos = CSLFindName(papszParams, "user");
    if (nPos != -1) {
        *ppszUser =
            CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));
    }
    else if (CPLGetConfigOption("PGUSER", NULL) != NULL ) {
        *ppszUser = CPLStrdup(CPLGetConfigOption("PGUSER", NULL));
    }
    else
        *ppszUser = NULL;
    /*else {
        CPLError(CE_Failure, CPLE_AppDefined,
            "User parameter must be provided, or PGUSER environment "
            "variable must be set. Please set the user and try again.");

        CSLDestroy(papszParams);

        return false;
    }*/

    nPos = CSLFindName(papszParams, "password");
    if (nPos != -1) {
        *ppszPassword =
            CPLStrdup(CPLParseNameValue(papszParams[nPos], NULL));
    }
    else if (CPLGetConfigOption("PGPASSWORD", NULL) != NULL ) {
        *ppszPassword = CPLStrdup(CPLGetConfigOption("PGPASSWORD", NULL));
    }
    else
        *ppszPassword = NULL;

    CSLDestroy(papszParams);

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::GetConnectionInfo(): "
        "Mode: %d\nDbname: %s\nSchema: %s\nTable: %s\nColumn: %s\nWhere: %s\n"
        "Host: %s\nPort: %s\nUser: %s\nPassword: %s\n"
        "Connection String: %s\n",
        *nMode, *ppszDbname,
        *ppszSchema ? *ppszSchema : "(null)",
        *ppszTable ? *ppszTable : "(null)",
        *ppszColumn ? *ppszColumn : "(null)",
        *ppszWhere ? *ppszWhere : "(null)",
        *ppszHost ? *ppszHost : "(null)",
        *ppszPort ? *ppszPort : "(null)",
        *ppszUser ? *ppszUser : "(null)",
        *ppszPassword ? *ppszPassword : "(null)",
        *ppszConnectionString);
#endif

    return true;
}

/***********************************************************************
 * \brief Create a connection to a postgres database
 **********************************************************************/
static PGconn *
GetConnection(const char * pszFilename, char ** ppszConnectionString,
    char ** ppszSchema, char ** ppszTable, char ** ppszColumn,
    char ** ppszWhere, WorkingMode * nMode, GBool * bBrowseDatabase)
{
    PostGISRasterDriver * poDriver;
    PGconn * poConn = NULL;
    char * pszDbname = NULL;
    char * pszHost = NULL;
    char * pszPort = NULL;
    char * pszUser = NULL;
    char * pszPassword = NULL;

    if (GetConnectionInfo(pszFilename, ppszConnectionString, &pszDbname, ppszSchema,
        ppszTable, ppszColumn, ppszWhere, &pszHost, &pszPort, &pszUser,
        &pszPassword, nMode, bBrowseDatabase))
    {
        /**************************************************************
         * Open a new database connection
         **************************************************************/
        poDriver =
            (PostGISRasterDriver *)GDALGetDriverByName("PostGISRaster");

        poConn = poDriver->GetConnection(*ppszConnectionString,
                pszDbname, pszHost, pszPort, pszUser);

        if (poConn == NULL) {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Couldn't establish a database connection");
        }
    }

    CPLFree(pszDbname);
    CPLFree(pszHost);
    CPLFree(pszPort);
    CPLFree(pszUser);
    CPLFree(pszPassword);

    return poConn;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int PostGISRasterDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if (poOpenInfo->pszFilename == NULL ||
        poOpenInfo->fpL != NULL ||
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "PG:"))
    {
        return FALSE;
    }

    // Will avoid a OGR PostgreSQL connection string to be recognized as a
    // PostgisRaster one and later fail (#6034)
    if( strstr(poOpenInfo->pszFilename, " schemas=") ||
        strstr(poOpenInfo->pszFilename, " SCHEMAS=") )
    {
        return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 * \brief Open a connection with PostgreSQL. The connection string will
 * have the PostgreSQL accepted format, plus the next key=value pairs:
 *  schema = <schema_name>
 *  table = <table_name>
 *  column = <column_name>
 *  where = <SQL where>
 *  mode = <working mode> (1 or 2)
 *
 * These pairs are used for selecting the right raster table.
 **********************************************************************/
GDALDataset* PostGISRasterDataset::Open(GDALOpenInfo* poOpenInfo) {
    char* pszConnectionString = NULL;
    char* pszSchema = NULL;
    char* pszTable = NULL;
    char* pszColumn = NULL;
    char* pszWhere = NULL;
    WorkingMode nMode = NO_MODE;
    PGconn * poConn = NULL;
    PostGISRasterDataset* poDS = NULL;
    GBool bBrowseDatabase = false;
    CPLString osCommand;

    /**************************
     * Check input parameter
     **************************/
    if (!Identify(poOpenInfo))
        return NULL;

    poConn = GetConnection(poOpenInfo->pszFilename,
        &pszConnectionString, &pszSchema, &pszTable, &pszColumn,
        &pszWhere, &nMode, &bBrowseDatabase);
    if (poConn == NULL) {
        CPLFree(pszConnectionString);
        CPLFree(pszSchema);
        CPLFree(pszTable);
        CPLFree(pszColumn);
        CPLFree(pszWhere);
        return NULL;
    }


    /*******************************************************************
     * No table will be read. Only shows information about the existent
     * raster tables
     ******************************************************************/
    if (bBrowseDatabase) {
        /**
         * Creates empty dataset object, only for subdatasets
         **/
        poDS = new PostGISRasterDataset();
        poDS->poConn = poConn;
        poDS->eAccess = GA_ReadOnly;
        //poDS->poDriver = poDriver;
        poDS->nMode = (pszSchema) ? BROWSE_SCHEMA : BROWSE_DATABASE;

        /**
         * Look for raster tables at database and
         * store them as subdatasets
         **/
        if (!poDS->BrowseDatabase(pszSchema, pszConnectionString)) {
            CPLFree(pszConnectionString);
            delete poDS;

            if (pszSchema)
                CPLFree(pszSchema);
            if (pszTable)
                CPLFree(pszTable);
            if (pszColumn)
                CPLFree(pszColumn);
            if (pszWhere)
                CPLFree(pszWhere);

            return NULL;
        }

        if (pszSchema)
            CPLFree(pszSchema);
        if (pszTable)
            CPLFree(pszTable);
        if (pszColumn)
            CPLFree(pszColumn);
        if (pszWhere)
            CPLFree(pszWhere);
    }

    /*******************************************************************
     * A table will be read as dataset: Fetch raster properties from db.
     ******************************************************************/
    else {
        poDS = new PostGISRasterDataset();
        poDS->poConn = poConn;
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->nMode = nMode;
        //poDS->poDriver = poDriver;

        poDS->pszSchema = pszSchema;
        poDS->pszTable = pszTable;
        poDS->pszColumn = pszColumn;
        poDS->pszWhere = pszWhere;

        /**
         * Fetch basic raster metadata from db
         **/
#ifdef DEBUG_VERBOSE
        CPLDebug("PostGIS_Raster", "Open:: connection string = %s",
            pszConnectionString);
#endif

        if (!poDS->SetRasterProperties(pszConnectionString)) {
            CPLFree(pszConnectionString);
            delete poDS;
            return NULL;
        }
    }

    CPLFree(pszConnectionString);
    return poDS;

}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PostGISRasterDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/*****************************************
 * \brief Get Metadata from raster
 * TODO: Add more options (the result of
 * calling ST_Metadata, for example)
 *****************************************/
char** PostGISRasterDataset::GetMetadata(const char *pszDomain) {
    if (pszDomain != NULL && STARTS_WITH_CI(pszDomain, "SUBDATASETS"))
        return papszSubdatasets;
    else
        return GDALDataset::GetMetadata(pszDomain);
}

/*****************************************************
 * \brief Fetch the projection definition string
 * for this dataset in OpenGIS WKT format. It should
 * be suitable for use with the OGRSpatialReference
 * class.
 *****************************************************/
const char* PostGISRasterDataset::GetProjectionRef() {
    CPLString osCommand;
    PGresult* poResult;

    if (nSrid == -1)
        return "";

    if (pszProjection)
        return pszProjection;

    /********************************************************
     *          Reading proj from database
     ********************************************************/
    osCommand.Printf("SELECT srtext FROM spatial_ref_sys where SRID=%d",
            nSrid);
    poResult = PQexec(this->poConn, osCommand.c_str());
    if (poResult && PQresultStatus(poResult) == PGRES_TUPLES_OK
            && PQntuples(poResult) > 0) {
        pszProjection = CPLStrdup(PQgetvalue(poResult, 0, 0));
    }

    if (poResult)
        PQclear(poResult);

    return pszProjection;
}

/**********************************************************
 * \brief Set projection definition. The input string must
 * be in OGC WKT or PROJ.4 format
 **********************************************************/
CPLErr PostGISRasterDataset::SetProjection(const char * pszProjectionRef) {
    VALIDATE_POINTER1(pszProjectionRef, "SetProjection", CE_Failure);

    CPLString osCommand;
    PGresult * poResult;
    int nFetchedSrid = -1;


    /*****************************************************************
     * Check if the dataset allows updating
     *****************************************************************/
    if (GetAccess() != GA_Update) {
        ReportError(CE_Failure, CPLE_NoWriteAccess,
                "This driver doesn't allow write access");
        return CE_Failure;
    }

    /*****************************************************************
     * Look for projection with this text
     *****************************************************************/

    // First, WKT text
    osCommand.Printf("SELECT srid FROM spatial_ref_sys where srtext='%s'",
            pszProjectionRef);
    poResult = PQexec(poConn, osCommand.c_str());

    if (poResult && PQresultStatus(poResult) == PGRES_TUPLES_OK
            && PQntuples(poResult) > 0) {

        nFetchedSrid = atoi(PQgetvalue(poResult, 0, 0));

        // update class attribute
        nSrid = nFetchedSrid;


        // update raster_columns table
        osCommand.Printf("UPDATE raster_columns SET srid=%d WHERE \
                    r_table_name = '%s' AND r_column = '%s'",
                nSrid, pszTable, pszColumn);
        poResult = PQexec(poConn, osCommand.c_str());
        if (poResult == NULL || PQresultStatus(poResult) != PGRES_COMMAND_OK) {
            ReportError(CE_Failure, CPLE_AppDefined,
                    "Couldn't update raster_columns table: %s",
                    PQerrorMessage(poConn));
            return CE_Failure;
        }

        // TODO: Update ALL blocks with the new srid...

        return CE_None;
    }
        // If not, proj4 text
    else {
        osCommand.Printf(
                "SELECT srid FROM spatial_ref_sys where proj4text='%s'",
                pszProjectionRef);
        poResult = PQexec(poConn, osCommand.c_str());

        if (poResult && PQresultStatus(poResult) == PGRES_TUPLES_OK
                && PQntuples(poResult) > 0) {

            nFetchedSrid = atoi(PQgetvalue(poResult, 0, 0));

            // update class attribute
            nSrid = nFetchedSrid;

            // update raster_columns table
            osCommand.Printf("UPDATE raster_columns SET srid=%d WHERE \
                    r_table_name = '%s' AND r_column = '%s'",
                    nSrid, pszTable, pszColumn);

            poResult = PQexec(poConn, osCommand.c_str());
            if (poResult == NULL ||
                    PQresultStatus(poResult) != PGRES_COMMAND_OK) {
                ReportError(CE_Failure, CPLE_AppDefined,
                        "Couldn't update raster_columns table: %s",
                        PQerrorMessage(poConn));
                return CE_Failure;
            }

            // TODO: Update ALL blocks with the new srid...

            return CE_None;
        }
        else {
            ReportError(CE_Failure, CPLE_WrongFormat,
                    "Couldn't find WKT neither proj4 definition");
            return CE_Failure;
        }
    }
}

/********************************************************
 * \brief Set the affine transformation coefficients
 ********************************************************/
CPLErr PostGISRasterDataset::SetGeoTransform(double* padfGeoTransform) {
    if (!padfGeoTransform)
        return CE_Failure;

    memcpy(adfGeoTransform, padfGeoTransform, 6 * sizeof(double));

    return CE_None;
}

/********************************************************
 * \brief Get the affine transformation coefficients
 ********************************************************/
CPLErr PostGISRasterDataset::GetGeoTransform(double * padfGeoTransform) {

    // copy necessary values in supplied buffer
    memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));

    if( nRasterXSize == 0 && nRasterYSize == 0 )
        return CE_Failure;

    /* To avoid QGIS trying to create a warped VRT for what is really */
    /* an ungeoreferenced dataset */
    if( CPLIsEqual(padfGeoTransform[0], 0.0) &&
        CPLIsEqual(padfGeoTransform[1], 1.0) &&
        CPLIsEqual(padfGeoTransform[2], 0.0) &&
        CPLIsEqual(padfGeoTransform[3], 0.0) &&
        CPLIsEqual(padfGeoTransform[4], 0.0) &&
        CPLIsEqual(padfGeoTransform[5], 1.0) )
    {
        return CE_Failure;
    }

    return CE_None;
}


/*********************************************************
 * \brief Fetch files forming dataset.
 *
 * We need to define this method because the VRTDataset
 * method doesn't check for NULL FileList before trying
 * to collect the names of all sources' file list.
 *********************************************************/
char **PostGISRasterDataset::GetFileList()
{
	return NULL;
}

/********************************************************
 * \brief Create a copy of a PostGIS Raster dataset.
 ********************************************************/
GDALDataset *
PostGISRasterDataset::CreateCopy( CPL_UNUSED const char * pszFilename,
                                  GDALDataset *poGSrcDS,
                                  CPL_UNUSED int bStrict,
                                  CPL_UNUSED char ** papszOptions,
                                  CPL_UNUSED GDALProgressFunc pfnProgress,
                                  CPL_UNUSED void * pProgressData )
{
    char* pszSchema = NULL;
    char* pszTable = NULL;
    char* pszColumn = NULL;
    char* pszWhere = NULL;
    GBool bBrowseDatabase = false;
    WorkingMode nMode;
    char* pszConnectionString = NULL;
    const char* pszSubdatasetName;
    PGconn * poConn = NULL;
    PGresult * poResult = NULL;
    CPLString osCommand;
    GBool bInsertSuccess;

    if( poGSrcDS->GetDriver() != GDALGetDriverByName("PostGISRaster") )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
            "PostGISRasterDataset::CreateCopy() only works on source "
            "datasets that are PostGISRaster" );
        return NULL;
    }

    // Now we can do the cast
    PostGISRasterDataset *poSrcDS = (PostGISRasterDataset *)poGSrcDS;
    PostGISRasterDataset *poSubDS;

    // Check connection string
    if (pszFilename == NULL ||
        !STARTS_WITH_CI(pszFilename, "PG:")) {
        /**
         * The connection string provided is not a valid connection
         * string.
         */
        CPLError( CE_Failure, CPLE_NotSupported,
            "PostGIS Raster driver was unable to parse the provided "
            "connection string." );
        return NULL;
    }

    poConn = GetConnection(pszFilename, &pszConnectionString, &pszSchema,
        &pszTable, &pszColumn, &pszWhere, &nMode, &bBrowseDatabase);
    if (poConn == NULL || bBrowseDatabase || pszTable == NULL)
    {
        CPLFree(pszConnectionString);
        CPLFree(pszSchema);
        CPLFree(pszTable);
        CPLFree(pszColumn);
        CPLFree(pszWhere);

        // if connection info fails, browsing mode, or no table set
        return NULL;
    }

    // begin transaction
    poResult = PQexec(poConn, "begin");
    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_COMMAND_OK) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error beginning database transaction: %s",
            PQerrorMessage(poConn));
        if (poResult != NULL)
            PQclear(poResult);
        if (pszSchema)
            CPLFree(pszSchema);
        if (pszTable)
            CPLFree(pszTable);
        if (pszColumn)
            CPLFree(pszColumn);
        if (pszWhere)
            CPLFree(pszWhere);

        CPLFree(pszConnectionString);

        return NULL;
    }

    PQclear(poResult);

    // create table for raster (if not exists because a
    // dataset will not be reported for an empty table)

    // TODO: is 'rid' necessary?
    osCommand.Printf("create table if not exists %s.%s (rid serial, %s "
        "public.raster, constraint %s_pkey primary key (rid));",
        pszSchema, pszTable, pszColumn, pszTable);
    poResult = PQexec(poConn, osCommand.c_str());
    if (
            poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

        CPLError(CE_Failure, CPLE_AppDefined,
                "Error creating needed tables: %s",
                PQerrorMessage(poConn));
        if (poResult != NULL)
            PQclear(poResult);

        // rollback
        poResult = PQexec(poConn, "rollback");
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

            CPLError(CE_Failure, CPLE_AppDefined,
                "Error rolling back transaction: %s",
                PQerrorMessage(poConn));
        }
        if (poResult != NULL)
            PQclear(poResult);
        if (pszSchema)
            CPLFree(pszSchema);
        if (pszTable)
            CPLFree(pszTable);
        if (pszColumn)
            CPLFree(pszColumn);
        if (pszWhere)
            CPLFree(pszWhere);

        CPLFree(pszConnectionString);

        return NULL;
    }

    PQclear(poResult);

    osCommand.Printf("create index %s_%s_gist ON %s.%s USING gist "
        "(public.st_convexhull(%s));", pszTable, pszColumn,
        pszSchema, pszTable, pszColumn);
    poResult = PQexec(poConn, osCommand.c_str());
    if (
            poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

        CPLError(CE_Failure, CPLE_AppDefined,
                "Error creating needed index: %s",
                PQerrorMessage(poConn));
        if (poResult != NULL)
            PQclear(poResult);

        // rollback
        poResult = PQexec(poConn, "rollback");
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

            CPLError(CE_Failure, CPLE_AppDefined,
                "Error rolling back transaction: %s",
                PQerrorMessage(poConn));
        }
        if (poResult != NULL)
            PQclear(poResult);
        if (pszSchema)
            CPLFree(pszSchema);
        if (pszTable)
            CPLFree(pszTable);
        if (pszColumn)
            CPLFree(pszColumn);
        if (pszWhere)
            CPLFree(pszWhere);

        CPLFree(pszConnectionString);

        return NULL;
    }

    PQclear(poResult);

    if (poSrcDS->nMode == ONE_RASTER_PER_TABLE) {
        // one raster per table

        // insert one raster
        bInsertSuccess = InsertRaster(poConn, poSrcDS,
            pszSchema, pszTable, pszColumn);
        if (!bInsertSuccess) {
            // rollback
            poResult = PQexec(poConn, "rollback");
            if (poResult == NULL ||
                PQresultStatus(poResult) != PGRES_COMMAND_OK) {

                CPLError(CE_Failure, CPLE_AppDefined,
                    "Error rolling back transaction: %s",
                    PQerrorMessage(poConn));
            }
            if (poResult != NULL)
                PQclear(poResult);
            if (pszSchema)
                CPLFree(pszSchema);
            if (pszTable)
                CPLFree(pszTable);
            if (pszColumn)
                CPLFree(pszColumn);
            if (pszWhere)
                CPLFree(pszWhere);

            CPLFree(pszConnectionString);

            return NULL;
        }
    }
    else if (poSrcDS->nMode == ONE_RASTER_PER_ROW) {
        // one raster per row

        // papszSubdatasets contains name/desc for each subdataset
        for (int i = 0; i < CSLCount(poSrcDS->papszSubdatasets); i += 2) {
            pszSubdatasetName = CPLParseNameValue( poSrcDS->papszSubdatasets[i], NULL);
            if (pszSubdatasetName == NULL) {
                CPLDebug("PostGIS_Raster", "PostGISRasterDataset::CreateCopy(): "
                "Could not parse name/value out of subdataset list: "
                "%s", poSrcDS->papszSubdatasets[i]);
                continue;
            }

            // for each subdataset
            GDALOpenInfo poOpenInfo( pszSubdatasetName, GA_ReadOnly );
            // open the subdataset
            poSubDS = (PostGISRasterDataset *)Open(&poOpenInfo);

            if (poSubDS == NULL) {
                // notify!
                CPLDebug("PostGIS_Raster", "PostGISRasterDataset::CreateCopy(): "
                    "Could not open a subdataset: %s",
                    pszSubdatasetName);
                continue;
            }

            // insert one raster
            bInsertSuccess = InsertRaster(poConn, poSubDS,
                pszSchema, pszTable, pszColumn);

            if (!bInsertSuccess) {
                CPLDebug("PostGIS_Raster", "PostGISRasterDataset::CreateCopy(): "
                    "Could not copy raster subdataset to new dataset." );

                // keep trying ...
            }

            // close this dataset
            GDALClose((GDALDatasetH)poSubDS);
        }
    }

    // commit transaction
    poResult = PQexec(poConn, "commit");
    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_COMMAND_OK) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error committing database transaction: %s",
            PQerrorMessage(poConn));
        if (poResult != NULL)
            PQclear(poResult);
        if (pszSchema)
            CPLFree(pszSchema);
        if (pszTable)
            CPLFree(pszTable);
        if (pszColumn)
            CPLFree(pszColumn);
        if (pszWhere)
            CPLFree(pszWhere);

        CPLFree(pszConnectionString);

        return NULL;
    }

    PQclear(poResult);

    if (pszSchema)
        CPLFree(pszSchema);
    if (pszTable)
        CPLFree(pszTable);
    if (pszColumn)
        CPLFree(pszColumn);
    if (pszWhere)
        CPLFree(pszWhere);

    CPLFree(pszConnectionString);

    CPLDebug("PostGIS_Raster", "PostGISRasterDataset::CreateCopy(): "
        "Opening new dataset: %s", pszFilename);

    // connect to the new dataset
    GDALOpenInfo poOpenInfo( pszFilename, GA_Update );
    // open the newdataset
    poSubDS = (PostGISRasterDataset *)Open(&poOpenInfo);

    if (poSubDS == NULL) {
        CPLDebug("PostGIS_Raster", "PostGISRasterDataset::CreateCopy(): "
            "New dataset could not be opened.");
    }

    return poSubDS;
}

/********************************************************
 * \brief Helper method to insert a new raster.
 ********************************************************/
GBool
PostGISRasterDataset::InsertRaster(PGconn * poConn,
    PostGISRasterDataset * poSrcDS, const char *pszSchema,
    const char * pszTable, const char * pszColumn)
{
    CPLString osCommand;
    PGresult * poResult = NULL;

    if (poSrcDS->pszWhere == NULL) {
        osCommand.Printf("insert into %s.%s (%s) (select %s from %s.%s)",
            pszSchema, pszTable, pszColumn, poSrcDS->pszColumn,
            poSrcDS->pszSchema, poSrcDS->pszTable);
    }
    else {
        osCommand.Printf("insert into %s.%s (%s) (select %s from %s.%s where %s)",
            pszSchema, pszTable, pszColumn, poSrcDS->pszColumn,
            poSrcDS->pszSchema, poSrcDS->pszTable, poSrcDS->pszWhere);
    }

#ifdef DEBUG_QUERY
    CPLDebug("PostGIS_Raster", "PostGISRasterDataset::InsertRaster(): Query = %s",
        osCommand.c_str());
#endif

    poResult = PQexec(poConn, osCommand.c_str());
    if (
            poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

        CPLError(CE_Failure, CPLE_AppDefined,
                "Error inserting raster: %s",
                PQerrorMessage(poConn));
        if (poResult != NULL)
            PQclear(poResult);

        return false;
    }

    PQclear(poResult);

    return true;
}

/*********************************************************
 * \brief Delete a PostGIS Raster dataset.
 *********************************************************/
CPLErr
PostGISRasterDataset::Delete(const char* pszFilename)
{
    char* pszSchema = NULL;
    char* pszTable = NULL;
    char* pszColumn = NULL;
    char* pszWhere = NULL;
    GBool bBrowseDatabase;
    char* pszConnectionString = NULL;
    WorkingMode nMode;
    PGconn * poConn = NULL;
    PGresult * poResult = NULL;
    CPLString osCommand;
    CPLErr nError = CE_Failure;

    // Check connection string
    if (pszFilename == NULL ||
        !STARTS_WITH_CI(pszFilename, "PG:")) {
        /**
         * The connection string provided is not a valid connection
         * string.
         */
        CPLError( CE_Failure, CPLE_NotSupported,
            "PostGIS Raster driver was unable to parse the provided "
            "connection string. Nothing was deleted." );
        return CE_Failure;
    }

    poConn = GetConnection(pszFilename, &pszConnectionString,
        &pszSchema, &pszTable, &pszColumn, &pszWhere,
        &nMode, &bBrowseDatabase);
    if (poConn == NULL || pszSchema == NULL || pszTable == NULL) {
        CPLFree(pszConnectionString);
        CPLFree(pszSchema);
        CPLFree(pszTable);
        CPLFree(pszColumn);
        CPLFree(pszWhere);

        return CE_Failure;
    }

    // begin transaction
    poResult = PQexec(poConn, "begin");
    if (poResult == NULL ||
        PQresultStatus(poResult) != PGRES_COMMAND_OK) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error beginning database transaction: %s",
            PQerrorMessage(poConn));

        // set nMode to NO_MODE to avoid any further processing
        nMode = NO_MODE;
    }

    PQclear(poResult);

    if ( nMode == ONE_RASTER_PER_TABLE ||
        (nMode == ONE_RASTER_PER_ROW && pszWhere == NULL)) {
        // without a where clause, this delete command shall delete
        // all subdatasets, even if the mode is ONE_RASTER_PER_ROW

        // drop table <schema>.<table>;
        osCommand.Printf("drop table %s.%s", pszSchema, pszTable);
        poResult = PQexec(poConn, osCommand.c_str());
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Couldn't drop the table %s.%s: %s",
                    pszSchema, pszTable, PQerrorMessage(poConn));
        }
        else {
            PQclear(poResult);
            nError = CE_None;
        }
    }
    else if (nMode == ONE_RASTER_PER_ROW) {

        // delete from <schema>.<table> where <where>
        osCommand.Printf("delete from %s.%s where %s", pszSchema,
            pszTable, pszWhere);
        poResult = PQexec(poConn, osCommand.c_str());
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Couldn't delete records from the table %s.%s: %s",
                    pszSchema, pszTable, PQerrorMessage(poConn));
        }
        else {
            PQclear(poResult);
            nError = CE_None;
        }
    }

    // if mode == NO_MODE, the begin transaction above did not complete,
    // so no commit is necessary
    if (nMode != NO_MODE) {
        poResult = PQexec(poConn, "commit");
        if (poResult == NULL ||
            PQresultStatus(poResult) != PGRES_COMMAND_OK) {

            CPLError(CE_Failure, CPLE_AppDefined,
                "Error committing database transaction: %s",
                PQerrorMessage(poConn));

            nError = CE_Failure;
        }
    }

    if (poResult)
        PQclear(poResult);
    if (pszSchema)
        CPLFree(pszSchema);
    if (pszTable)
        CPLFree(pszTable);
    if (pszColumn)
        CPLFree(pszColumn);
    if (pszWhere)
        CPLFree(pszWhere);

    // clean up connection string
    CPLFree(pszConnectionString);

    return nError;
}

/***********************************************************************
 * \brief Create an array with all the coordinates needed to construct
 * a polygon using ST_PolygonFromText.
 **********************************************************************/
GBool PostGISRasterDataset::PolygonFromCoords(
    int nXOff, int nYOff, int nXEndOff, int nYEndOff,
    double adfProjWin[8])
{
    // We first construct a polygon to intersect with
    int ulx = nXOff;
    int uly = nYOff;
    int lrx = nXEndOff;
    int lry = nYEndOff;

    double xRes = adfGeoTransform[GEOTRSFRM_WE_RES];
    double yRes = adfGeoTransform[GEOTRSFRM_NS_RES];

    adfProjWin[0] = adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
                    ulx * xRes +
                    uly * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1];
    adfProjWin[1] = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
                    ulx * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] +
                    uly * yRes;
    adfProjWin[2] = adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
                    lrx * xRes +
                    uly * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1];
    adfProjWin[3] = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
                    lrx * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] +
                    uly * yRes;
    adfProjWin[4] = adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
                    lrx * xRes +
                    lry * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1];
    adfProjWin[5] = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
                    lrx * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] +
                    lry * yRes;
    adfProjWin[6] = adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
                    ulx * xRes +
                    lry * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1];
    adfProjWin[7] = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
                    ulx * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] +
                    lry * yRes;

#ifdef DEBUG_VERBOSE
    CPLDebug("PostGIS_Raster",
        "PostGISRasterDataset::PolygonFromCoords: constructed "
        "polygon: POLYGON((%.17f %.17f, %.17f %.17f, %.17f %.17f, "
        "%.17f %.17f, %.17f %.17f))", adfProjWin[0], adfProjWin[1],
        adfProjWin[2], adfProjWin[3], adfProjWin[4], adfProjWin[5],
        adfProjWin[6], adfProjWin[7], adfProjWin[0], adfProjWin[1]);
#endif

    return true;
}

/***********************************************************************
 * GDALRegister_PostGISRaster()
 **********************************************************************/
void GDALRegister_PostGISRaster()

{
    if( !GDAL_CHECK_VERSION( "PostGISRaster driver" ) )
        return;

    if( GDALGetDriverByName( "PostGISRaster" ) != NULL )
        return;

    GDALDriver *poDriver = new PostGISRasterDriver();

    poDriver->SetDescription("PostGISRaster");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "PostGIS Raster driver");
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->pfnOpen = PostGISRasterDataset::Open;
    poDriver->pfnIdentify = PostGISRasterDataset::Identify;
    poDriver->pfnCreateCopy = PostGISRasterDataset::CreateCopy;
    poDriver->pfnDelete = PostGISRasterDataset::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
