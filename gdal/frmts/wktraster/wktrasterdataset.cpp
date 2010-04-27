/*************************************************************************
 * File :    wktrasterdataset.cpp
 * Project:  WKT Raster driver
 * Purpose:  GDAL Dataset code for WKTRaster driver 
 * Author:   Jorge Arevalo, jorgearevalo@gis4free.org
 * 
 * Last changes:
 * $Id$
 *
 * This class represents a dataset of the PostGIS Raster type, formally
 * known as "WKT Raster" type. This type implements a complete raster
 * structure for the PostGIS extension. A single attribute of type 
 * "raster" can store:
 *  - a complete image
 *  - an image tile
 *  - a "raster object". A object result from the rasterization of a
 *    vector structure
 * Hence, a table with a column of type "raster", can store:
 *  a) an image warehouse of untiled and (possibly) unrelated images. These
 *     images may or may not overlap since every image has its own 
 *     georeference. They may  also have no georeference at all.
 *  b) an irregularly tiled raster coverage. It might not necessarily be
 *     rectangular, there might be some missing tiles, and they might be 
 *     different sizes. Tiles should not overlap.
 *  c) a regularly tiled raster coverage. It might not necessarily be 
 *     rectangular, there might be some missing tiles, and the tiles should
 *     be the same size. Tiles should not overlap.
 *  d) a rectangular regularly tiled raster coverage. It is necessarily
 *     rectangular, there are no missing tiles, they are all the same size
 *     and they do not overlap.
 *  e) a tiled image. It is necessarily rectangular, there are no missing
 *     tiles they are all the same size, and they do not overlap. This 
 *     type is different from type d) in that it does not represent a 
 *     complete coverage; other images forming the rest of the coverage 
 *     are stored as other tables of tiled images. This structure is not 
 *     very practical from a GIS analytical point of view since any 
 *     operations applied to the coverage must also be applied to every
 *     table.
 *  f) a raster object coverage resulting from the rasterization of a 
 *     vector coverage. Each vector feature becomes a small raster with
 *     the same extent as the original vector feature. This type of 
 *     coverage is not necessarily complete, nor rectangular; tiles should
 *     be of different sizes and might overlap. It all depends on the 
 *     characteristics of the vector layer being rasterized. An exhaustive
 *     (or continuous) layer of mutually exclusive geometries (without 
 *     gaps or holes like a forest cover) would result in a raster object
 *     coverage in which significant pixels (withdata values) would not
 *     overlap, but non-significant pixels (nodata values) would. On the 
 *     other end of the spectrum, a discontinuous layer of mutually 
 *     exclusive geometries (like a lake or building layer) would result
 *     in a coverage of mostly disjoint raster objects.
 * 
 * A table with c), d) or e) arrangements is said to have "regular
 * blocking arrangement". This is:
 *  - All raster tiles (table rows) have the same width and height.
 *  - All tiles do not overlap and their upper left corners follow a 
 *    regular block grid.
 *  - The global extent of the raster is rectangular and not rotated.
 *
 * To know if a given raster table is expected to have regular blocking
 * arrangement, there is a special table called RASTER_COLUMNS table,
 * similar in concept to GEOMETRY_COLUMNS table for PostGIS, with a
 * boolean field "regular_blocking". There is, however, no mechanism to 
 * enforce (constrain) these criteria and adding, modifying or deleting a 
 * row from the table might break this regular blocking, and the 
 * regular_blocking attribute will not be automatically updated.
 *
 * NOTE:
 * 	There are many small functions in this class. Some of these functions
 * 	perform database queries. From the point of view of performance, this 
 *  may be a bad idea, but from point of view of code manintenance, I think
 *  is better. Anyway, the WKT Raster project is being developed at same 
 *  time that this driver,and I think that is better to allow fast changes
 *  in the code (easy to understand and to change) than performance, at 
 *  this moment (August 2009)
 *
 * TODO:
 *  - Eliminate working mode. We can determine if the raster has regular
 *    blocking or not by querying RASTER_COLUMNS table.
 *	- Allow non-regular blocking table arrangements (in general)
 *  - Allow PQ connection string parameters in any order
 *  - Update data blocks in SetProjection
 *  - Update data blocks in SetGeoTransform
 *  - Disable sequential scanning in OpenConnection in the database 
 *    instance
 *    has support for GEOMETRY type (is a good idea?)
 *  - In SetRasterProperties, if we can't create OGRGeometry from database,
 *    we should "attack" directly the WKT representation of geometry, not
 *    abort the program. For example, when pszProjectionRef is NULL
 *   (SRID = -1), when poSR or prGeom are NULL...
 *
 *    Low priority:
 *      - Add support for rotated images in SetRasterProperties
 *      - Check if the tiles of a table REALLY have regular blocking 
 *        arrangement.The fact that "regular_blocking" field in 
 *        RASTER_COLUMNS table is set to TRUE doesn't proof it. Method 
 *        TableHasRegularBlocking.
 *
 ************************************************************************
 * Copyright (c) 2009, Jorge Arevalo, jorgearevalo@gis4free.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject to 
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ************************************************************************/

#include "wktraster.h"
#include <stdlib.h>
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include <math.h>
#include <cpl_error.h>

#ifdef _WIN32
#define rint(x) floor((x) + 0.5)
#endif

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_WKTRaster(void);

CPL_C_END


    /**
     * Constructor. Init the class properties with default values
     */
WKTRasterDataset::WKTRasterDataset() {
    hPGconn = NULL;
    bCloseConnection = FALSE;
    pszSchemaName = NULL;
    pszTableName = NULL;
    pszRasterColumnName = NULL;
    pszWhereClause = NULL;
    bTableHasGISTIndex = FALSE;
    nVersion = 0;
    nBlockSizeX = 0;
    nBlockSizeY = 0;
    dfPixelSizeX = 0.0;
    dfPixelSizeY = 0.0;
    dfUpperLeftX = 0.0;
    dfUpperLeftY = 0.0;
    dfLowerRightX = 0.0;
    dfLowerRightY = 0.0;
    dfSkewX = 0.0;
    dfSkewY = 0.0;
    nSrid = -1;
    nOverviews = 0;
    papoWKTRasterOv = NULL;
    poOutdbRasterDS = NULL;

    // TO BE DELETED
    papoBlocks = NULL;
    nBlocks = 0;
    
}

    /**
     * Destructor. Frees allocated memory.
     */
WKTRasterDataset::~WKTRasterDataset() {
    CPLFree(pszSchemaName);
    CPLFree(pszTableName);
    CPLFree(pszWhereClause);
    CPLFree(pszRasterColumnName);

    if (nOverviews > 0) {
        for (int i = 0; i < nOverviews; i++) {
            delete papoWKTRasterOv[i];
            papoWKTRasterOv[i] = NULL;
        }

        CPLFree(papoWKTRasterOv);
    }


    if (nBlocks > 0) {
        for(int i = 0; i < nBlocks; i++) {
            delete papoBlocks[i];
            papoBlocks[i] = NULL;
        }

        CPLFree(papoBlocks);
    }

    /**
     * bCloseConnection = TRUE for Dataset, FALSE for Overviews
     */
    if (bCloseConnection == TRUE && hPGconn != NULL) {
        PQfinish(hPGconn);
        hPGconn = NULL;
    }

    if (poOutdbRasterDS != NULL) {
        GDALClose((GDALDatasetH)poOutdbRasterDS);
    }
}

/**
 * Check if the table has an index
 * Parameters:
 *  - PGconn *: pointer to connection
 *  - const char *: table name
 *  - const char *: table schema
 * Output:
 *  - GBool: TRUE if the table has an index, FALSE otherwise
 */
static
GBool TableHasGISTIndex(PGconn * hPGconn, const char * pszTable,
        const char * pszSchema) {

    // Security checkings
    VALIDATE_POINTER1(hPGconn, "TableHasGISTIndex", FALSE);
    VALIDATE_POINTER1(pszTable, "TableHasGISTIndex", FALSE);


    CPLString osCommand;
    PGresult * hPGresult = NULL;
    char * pszResultString;
    GBool bTableHasIndex = FALSE;
    
    /*****************************************************************
     * Check if the table has an index in PostgreSQL system tables
     *****************************************************************/
    osCommand.Printf(
            "SELECT relhasindex FROM pg_class, pg_attribute, pg_type, \
            pg_namespace WHERE pg_namespace.nspname = '%s' and \
            pg_namespace.oid = pg_class.relnamespace and \
            pg_class.relname = '%s' and \
            pg_class.oid = pg_attribute.attrelid and \
		    pg_attribute.atttypid = pg_type.oid and \
            pg_type.typname = 'raster'",
           pszTable, pszSchema);

    hPGresult = PQexec(hPGconn, osCommand.c_str());
    if (
            hPGresult != NULL &&
            PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
            PQntuples(hPGresult) > 0
            ) 
    {
        pszResultString = PQgetvalue(hPGresult, 0, 0);
        if (pszResultString != NULL && CSLTestBoolean(pszResultString))
        {
            bTableHasIndex = TRUE;
        } 
        else 
        {
            bTableHasIndex = FALSE;
        }

    } 
    else 
    {
       bTableHasIndex = FALSE;
    }  

    if (hPGresult)
        PQclear(hPGresult);

    return bTableHasIndex;
}


/**
 * Check if the RASTER_COLUMNS table exists
 * Parameters:
 *  - PGconn *: pointer to connection
 * Output:
 *  - GBool: TRUE if the table RASTER_COLUMNS does exist, FALSE otherwise
 */
static
GBool ExistsRasterColumnsTable(PGconn * hPGconn)
{
    VALIDATE_POINTER1(hPGconn, "ExistsRasterColumnsTable", FALSE);
    PGresult * hPGresult = NULL;
    int nCount = 0;
    GBool bExistsRasterColumnsTable = FALSE;
    CPLString osCommand;

    /*********************************************************************
     * Look for RASTER_COLUMNS table
     *********************************************************************/
    osCommand.Printf(
            "select count(pg_class.relname) from pg_class, pg_namespace \
            where pg_class.relnamespace = pg_namespace.oid and \
            pg_class.relname='raster_columns'");
    hPGresult = PQexec(hPGconn, osCommand.c_str());
    if (
            hPGresult != NULL &&
            PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
            PQntuples(hPGresult) > 0
            ) 
    {
        nCount = atoi(PQgetvalue(hPGresult, 0, 0));
        if (nCount == 1)
        {
            bExistsRasterColumnsTable = TRUE;
        } 
        else 
        {
            bExistsRasterColumnsTable = FALSE;
        }

    } 
    else 
    {
        bExistsRasterColumnsTable = FALSE;
    }

    if (hPGresult);
        PQclear(hPGresult);
        
    return bExistsRasterColumnsTable;
}


/**
 * Check if exists an overviews table
 * Parameters:
 *  - PGconn *: pointer to connection
 * Output:
 *  - GBool: TRUE if the table RASTER_OVERVIEWS does exist, FALSE otherwise
 */
static
GBool ExistsOverviewsTable(PGconn * hPGconn) 
{
    VALIDATE_POINTER1(hPGconn, "ExistsRasterOverviewsTable", FALSE);
    PGresult * hPGresult = NULL;
    int nCount = 0;
    GBool bExistsRasterOverviewsTable = FALSE;
    CPLString osCommand;

    /*********************************************************************
     * Look for RASTER_COLUMNS table
     *********************************************************************/
    osCommand.Printf(
            "select count(pg_class.relname) from pg_class, pg_namespace \
            where pg_class.relnamespace = pg_namespace.oid and \
            pg_class.relname='raster_overviews'");
    hPGresult = PQexec(hPGconn, osCommand.c_str());
    if (
            hPGresult != NULL &&
            PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
            PQntuples(hPGresult) > 0
            ) 
    {
        nCount = atoi(PQgetvalue(hPGresult, 0, 0));
        if (nCount == 1)
        {
            bExistsRasterOverviewsTable = TRUE;
        } 
        else 
        {
            bExistsRasterOverviewsTable = FALSE;
        }

    } 
    else 
    {
        bExistsRasterOverviewsTable = FALSE;
    }

    if (hPGresult);
        PQclear(hPGresult);
        
    return bExistsRasterOverviewsTable;
}


/**
 * Check if the table has regular_blocking constraint.
 * NOTE: From now, only tables with regular_blocking are allowed
 * Parameters:
 *  - PGconn *: pointer to connection
 *  - const char *: table name
 *  - const char *: raster column name
 *  - const char *: schema name
 * Returns:
 *  - GBool: TRUE if the table has regular_blocking constraint, FALSE 
 *          otherwise
 */
static
GBool TableHasRegularBlocking(PGconn * hPGconn, const char * pszTable,
        const char * pszColumn, const char * pszSchema) 
{

    // Security checkings
    VALIDATE_POINTER1(hPGconn, "TableHasRegularBlocking", FALSE);
    VALIDATE_POINTER1(pszTable, "TableHasRegularBlocking", FALSE);
    VALIDATE_POINTER1(pszColumn, "TableHasRegularBlocking", FALSE);

    CPLString osCommand;
    PGresult * hPGresult = NULL;
    char * pszResultString;
    GBool bTableHasRegularBlocking = FALSE;


    /**********************************************************************
     * Look for regular_blocking in RASTER_COLUMNS table
     *********************************************************************/
    osCommand.Printf(
            "SELECT	regular_blocking FROM raster_columns WHERE \
            r_table_name = '%s' and r_column = '%s' and \
            r_table_schema = '%s'",
            pszTable, pszColumn, pszSchema);

    hPGresult = PQexec(hPGconn, osCommand.c_str());
    if (hPGresult != NULL && PQresultStatus(hPGresult) == PGRES_TUPLES_OK 
            && PQntuples(hPGresult) > 0) 
    {
        pszResultString = PQgetvalue(hPGresult, 0, 0);
        if (pszResultString != NULL && CSLTestBoolean(pszResultString))
        {
            bTableHasRegularBlocking = TRUE;
        } 
        else 
        {
            bTableHasRegularBlocking = FALSE;
        }


    } 
    else 
    {
        bTableHasRegularBlocking = FALSE;
    }

    if (hPGresult != NULL)
        PQclear(hPGresult);

    return bTableHasRegularBlocking;
}

/**
 * Get the name of the column of type raster
 * Parameters:
 * 	PGconn *: pointer to connection
 *      const char *: schema name
 * 	const char *: table name
 * Output:
 * 	char *: The column name, or NULL if doesn't exist
 */
static
char * GetWKTRasterColumnName(PGconn * hPGconn, const char * pszSchemaName,
        const char * pszTable) {

    // Security checkings
    VALIDATE_POINTER1(hPGconn, "GetWKTRasterColumnName", NULL);
    VALIDATE_POINTER1(pszTable, "GetWKTRasterColumnName", NULL);

    CPLString osCommand;
    PGresult * hPGresult = NULL;
    char * pszResultString;

  
    /************************************************************
     * Get the attribute name of type 'raster' of the table
     ************************************************************/
    osCommand.Printf(
            "SELECT attname \
            FROM pg_class, pg_attribute, pg_type, pg_namespace \
            WHERE \
		pg_namespace.nspname = '%s' and \
		pg_namespace.oid = pg_class.relnamespace and \
		pg_class.relname = '%s' and \
		pg_class.oid = pg_attribute.attrelid and \
		pg_attribute.atttypid = pg_type.oid and \
		pg_type.typname = 'raster'",
            pszSchemaName, pszTable);

    hPGresult = PQexec(hPGconn, osCommand.c_str());
    if (
            hPGresult != NULL &&
            PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
            PQntuples(hPGresult) > 0
            ) {
        pszResultString = CPLStrdup(PQgetvalue(hPGresult, 0, 0));
        PQclear(hPGresult);

    } else {
        if (hPGresult != NULL)
            PQclear(hPGresult);
        pszResultString = NULL;
    }


    return pszResultString;

}

/**
 * Open a database connection and perform some security checkings
 * Parameters:
 * 	const char *: Connection string
 * Output:
 * 	PGconn *: pointer to object connection, or NULL it there was a problem
 */
static
PGconn * OpenConnection(const char *pszConnectionString) {

    // Security checkings
    VALIDATE_POINTER1(pszConnectionString, "OpenConnection", NULL);
   
    PGconn * hPGconn = NULL;
    PGresult * hPGresult = NULL;


    /********************************************************
     * 		Connect with database
     ********************************************************/
    hPGconn = PQconnectdb(pszConnectionString + 3);
    if (hPGconn == NULL || PQstatus(hPGconn) == CONNECTION_BAD) {
        CPLError(CE_Failure, CPLE_AppDefined, "PGconnectcb failed.\n%s",
                PQerrorMessage(hPGconn));
        PQfinish(hPGconn);
        return NULL;
    }

    /*****************************************************************
     *  Test to see if this database instance has support for the
     *  PostGIS Geometry type.
     *  TODO: If so, disable sequential scanning so we will get the
     *  value of the gist indexes. (is it a good idea?)
     *****************************************************************/
    hPGresult = PQexec(hPGconn,
            "SELECT oid FROM pg_type WHERE typname = 'geometry'");
    if (
            hPGresult == NULL ||
            PQresultStatus(hPGresult) != PGRES_TUPLES_OK ||
            PQntuples(hPGresult) <= 0
            ) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Can't find geometry type, is Postgis correctly installed ?\n");
        if (hPGresult)
            PQclear(hPGresult);
        PQfinish(hPGconn);
        hPGconn = NULL;

    }

    return hPGconn;
}

/**
 * Extract a field from the connection string:
 * PG:[host='<host>'] [user='<user>'] [password='<password>']
 * dbname='<dbname>' table='<raster_table>' [mode='<working_mode>'
 * where='<where_clause>']
 * The idea is to extract the fields that PQconnect function doesn't accept
 * (this is, 'table', 'mode' and 'where')
 *
 * Parameters:
 * 	char **: pointer to connection string, to be modified
 * 	char *: field init (ex.: "where=", "table=")
 * Output:
 * 	char *: The field (where clause or table name) of the connection string
 * 	if everything works fine, NULL otherwise
 */
static
char * ExtractField(char ** ppszConnectionString, const char * pszFieldInit) {

    // Security checkings
    VALIDATE_POINTER1(ppszConnectionString, "ExtractField", NULL);
    VALIDATE_POINTER1(*ppszConnectionString, "ExtractField", NULL);
    VALIDATE_POINTER1(pszFieldInit, "ExtractField", NULL);

    char * pszField = NULL;
    char * pszStart = NULL;


    /************************************************************************
     * Get the value of the field to extract, and delete the whole field
     * from the connection string
     ************************************************************************/
    int nFieldInitLen = strlen(pszFieldInit);

    /*
     * Get the initial position of the field in the string
     */
    pszStart = strstr(*ppszConnectionString, pszFieldInit); 
    if (pszStart == NULL)
        return NULL;
    
    int bHasQuote = pszStart[nFieldInitLen] == '\'';

    // Copy the field of the connection string to another var
    pszField = CPLStrdup(pszStart + nFieldInitLen + bHasQuote);
    char* pszEndField = strchr(pszField, (bHasQuote) ? '\'' : ' ');
    if (pszEndField)
    {
        *pszEndField = '\0';
        char* pszEnd = pszStart + nFieldInitLen + bHasQuote +
            (pszEndField - pszField) + 1;
        memmove(pszStart, pszEnd, strlen(pszEnd) + 1);
    }
    else
        // Delete the field's part from the connection string
        *pszStart = '\0';

    return pszField;
}

/**
 * Populate the georeference information fields of dataset
 * Input:
 * Output:
 *      CPLErr: CE_None if the fields are populated, CE_Failure otherwise
 */
CPLErr WKTRasterDataset::SetRasterProperties()
{
    CPLString osCommand;
    PGresult * hPGresult = NULL;
    char * pszExtent;
    OGRSpatialReference * poSR = NULL;
    OGRGeometry * poGeom = NULL;
    OGRErr OgrErr = OGRERR_NONE;
    OGREnvelope * poE = NULL;
    char * pszProjectionRef;   

    /*********************************************************************
     * The raster table could contain several images with different
     * georeference information, or even doesn't contain georeference at
     * all. So, we first need to check if the raster table has regular
     * blocking structure or not
     *********************************************************************/
    if (bTableHasRegularBlocking)
    {
        /**
         * With a WHERE clause, we can get one or more rows, but if 
         * regular blocking is expected for this table, all the rows
         * (tiles) have the same width and height, and their upper left
         * corners follow a regular block grid. So, we can get the first
         * row to get these values.
         * xxx jorgearevalo: the same srid and pixel size too?
         */
        if (pszWhereClause != NULL)
        {
            osCommand.Printf(
                "SELECT st_srid(rast), st_skewx(rast), st_skewy(rast), \
                st_pixelsizex(rast), st_pixelsizey(rast) FROM %s.%s \
                WHERE %s", pszSchemaName, pszTableName, pszWhereClause);
        }

        /**
         * Without a WHERE clause, we don't have a row filter, but the
         * principle is the same, as regular blocking is expected for this
         * table.
         */
        else
        {
             osCommand.Printf(
                "SELECT st_srid(rast), st_skewx(rast), st_skewy(rast),\
                st_pixelsizex(rast), st_pixelsizey(rast) FROM %s.%s", 
                pszSchemaName, pszTableName);
        }

        hPGresult = PQexec(hPGconn, osCommand.c_str());
        if (hPGresult != NULL && 
                PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
                PQntuples(hPGresult) > 0)
        {

            /**
             * Get most of the raster properties
             **/
            nSrid = atoi(PQgetvalue(hPGresult, 0, 0));
            dfSkewX = atof(PQgetvalue(hPGresult, 0, 1));
            dfSkewY = atof(PQgetvalue(hPGresult, 0, 2));
            dfPixelSizeX = atof(PQgetvalue(hPGresult, 0, 3));
            dfPixelSizeY = atof(PQgetvalue(hPGresult, 0, 4));

            PQclear(hPGresult);
            
            /**
             * The rest of the properties are gotten from
             * RASTER_COLUMNS table
             * TODO: we don't need st_astext, should be enough with
             * extent. Study it.
             */
            osCommand.Printf(
                    "SELECT blocksize_x, blocksize_y, st_astext(extent) \
                    FROM raster_columns WHERE r_table_schema = '%s' AND \
                    r_table_name = '%s' AND r_column = '%s'",
                    pszSchemaName, pszTableName, pszRasterColumnName);
       
            hPGresult = PQexec(hPGconn, osCommand.c_str());
            if (hPGresult != NULL && 
                PQresultStatus(hPGresult) == PGRES_TUPLES_OK &&
                PQntuples(hPGresult) > 0)
            {
                nBlockSizeX = (PQgetvalue(hPGresult, 0, 0)) ? 
                    atoi(PQgetvalue(hPGresult, 0, 0)) :
                    0;
                nBlockSizeY = (PQgetvalue(hPGresult, 0, 1)) ?
                    atoi(PQgetvalue(hPGresult, 0, 1)) :
                    0;

                /**
                 * Be careful: The extent coordinates are projected
                 * coordinates, unless the raster covered by extent
                 * doesn't have georeference information (srid = -1)
                 **/
                pszExtent = CPLStrdup(PQgetvalue(hPGresult, 0, 2));

                PQclear(hPGresult);
                                             
                /**
                 * Construct a Geometry Object based on raster extent
                 */                    
                pszProjectionRef = (char *) GetProjectionRef();           
                poSR = new OGRSpatialReference(pszProjectionRef);
                OgrErr = OGRGeometryFactory::createFromWkt(
                        &pszExtent, poSR, &poGeom);
                if (OgrErr != OGRERR_NONE) 
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "Couldn't get WKT Raster extent from database");
                    CPLFree(pszExtent);
                    CPLFree(pszProjectionRef);

                    return CE_Failure;
                }         
                
                poE = new OGREnvelope();
                poGeom->getEnvelope(poE);

                /**
                 * Get the rest of raster properties from this object
                 */
                nRasterXSize = (int)
                    fabs(rint((poE->MaxX - poE->MinX) / dfPixelSizeX));
                nRasterYSize = (int)
                    fabs(rint((poE->MaxY - poE->MinY) / dfPixelSizeY));
               

                /**
                 * TODO: This is not correct... Fails with
                 * non-georeferenced images
                 */
                dfUpperLeftX = poE->MinX;
                dfUpperLeftY = poE->MaxY;
              
                /**
                 * TODO: pszExtent is modified by createFromWkt, so, we 
                 * can't free it. What should we do?
                 */
                //CPLFree(pszExtent);

                CPLFree(pszProjectionRef);                
            }

            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Couldn't get raster dimensions from database");
                if (hPGresult)
                    PQclear(hPGresult);
               
                return CE_Failure;
            }

        }

        else
        {
            /**
             * TODO: if there was an error getting information for raster
             * table, we could try querying RASTER_COLUMNS table
             */
            CPLError(CE_Failure, CPLE_OpenFailed,
                    "Couldn't fetch raster information.");
            return CE_Failure;
        }
    }

    /********************************************************************
     * Table with non regular blocking arrangement. Still under
     * development
     ********************************************************************/
    else
    {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Table with non regular blocking arrangement.\
                    This feature is not supported yet\n");
            return CE_Failure;

    }

    return CE_None;
}


/**
 * Explode a string representing an array into an array of strings. The input
 * string has this format: {element1,element2,element3,....,elementN}
 * Parameters:
 *  - const char *: a string representing an array
 *  - int *: pointer to an int that will contain the number of elements
 * Returns:
 *  char **: An array of strings, one per element. Must be freed with CSLDestroy
 */
char ** WKTRasterDataset::ExplodeArrayString(
        const char * pszPQarray, int * pnNumberOfElements) {

    // integrity checkings
    if (
            pszPQarray == NULL ||
            pszPQarray[0] != '{' ||
            pszPQarray[strlen(pszPQarray) - 1] != '}'
            ) {
        if (pnNumberOfElements)
            *pnNumberOfElements = 0;
        return NULL;
    }
    
    char* pszTemp = CPLStrdup(pszPQarray + 1);
    pszTemp[strlen(pszTemp) - 1] ='\0';
    char** papszRet = CSLTokenizeString2( pszTemp, ",", 0 );
    CPLFree(pszTemp);
    
    if (pnNumberOfElements)
        *pnNumberOfElements = CSLCount(papszRet);
    return papszRet;
}


/**
 * Implode an array of strings, to transform it into a PostgreSQL-style
 * array, with this format: {element1,element2,element3,....,elementN}
 * Input:
 *  char **: An array of strings, one per element
 *  int: An int that contains the number of elements
 * Output:
 *  const char *: a string representing an array
 */
char * WKTRasterDataset::ImplodeStrings(char ** papszElements, int nElements)
{
    VALIDATE_POINTER1(papszElements, "ImplodeStrings", NULL);

    
    char * pszPQarray = NULL;
    char * ptrString;
    char szTemp[1024];
    unsigned int nCharsCopied = 0;
    unsigned int nNumberOfCommas = 0;
    unsigned int nNumAvailableBytes = 0;
    int nPosOpeningBracket = 0;
    int nPosClosingBracket = 0;

    /**************************************************************************
     * Array for the string. We could allocate memory for all the elements of
     * the array, plus commas and brackets, but 1024 bytes should be enough to
     * store small values, and the method is faster in this way
     **************************************************************************/
    pszPQarray = (char *)CPLCalloc(1024, sizeof(char));
    VALIDATE_POINTER1(pszPQarray, "ImplodeStrings", NULL);


    // empty string
    if (nElements <= 0) {
        nPosOpeningBracket = 0;
        nPosClosingBracket = 1;
    }

    // Without commas
    else if (nElements == 1) {
        nPosOpeningBracket = 0;
        nCharsCopied = MIN(1024, strlen(papszElements[0]));
        memcpy(pszPQarray + sizeof(char),
                papszElements[0], nCharsCopied*sizeof(char));

        nPosClosingBracket = nCharsCopied + sizeof(char);
    }

    // loop the array and create the string
    else {
        nPosOpeningBracket = 0;

        nNumberOfCommas = nElements - 1;
        nNumAvailableBytes = 1024 - (2 + nNumberOfCommas) * sizeof(char);

        // This should NEVER happen, it's really difficult...
        if (nNumAvailableBytes < 2) {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Sorry, couldn't allocate enough space for PQ array");
            CPLFree(pszPQarray);
                   
            return NULL;
        }

        // ptrString will move over the output string
        ptrString = pszPQarray + sizeof(char);

        for(int i = 0; i < nElements; i++) {

            // Check if element really exists
            if (papszElements[i] == NULL) {
                // one less comma, more space available
                nNumAvailableBytes += sizeof(char);
                continue;
            }

            // Copy the element to output string
            else {
                memset(szTemp, '\0', 1024*sizeof(char));

                // We can copy, at most, the number of available bytes
                nCharsCopied = MIN(strlen(papszElements[i]), nNumAvailableBytes);
                memcpy(szTemp, papszElements[i], nCharsCopied * sizeof(char));

                // Copy element to end string
                memcpy(ptrString, szTemp, strlen(szTemp) * sizeof(char));
                ptrString += strlen(szTemp) * sizeof(char);
                
                // No last element, add comma
                if (i < nElements - 1) {
                    // If we don't have space for more elements, break
                    if (nNumAvailableBytes == 1) {
                        break;
                    }

                    // Add comma
                    ptrString[0] = ',';
                    ptrString += sizeof(char);
                }
            }
        }

        nPosClosingBracket = ptrString - pszPQarray;
    }

    // put brackets and return
    pszPQarray[nPosOpeningBracket] = '{';
    pszPQarray[nPosClosingBracket] = '}';
    return pszPQarray;
}

/**
 * Method: Open
 * Open a connection wiht PostgreSQL. The connection string will have this
 * format:
 * 	PG:[host='<host>]' user='<user>' [password='<password>]'
 *      dbname='<dbname>' table='<raster_table>' [mode='working_mode'
 *      where='<where_clause>'].
 * All the connection string, apart from table='table_name', where='sql_where'
 * and mode ='working_mode' is PQconnectdb-valid.
 *
 * 	NOTE: The table name can't include the schema in the form
 *      <schema>.<table_name>,
 * 	and this is a TODO task.
 * Parameters:
 *  - GDALOpenInfo *: pointer to a GDALOpenInfo structure, with useful
 *                  information for Dataset
 * Returns:
 *  - GDALDataset *: pointer to a new GDALDataset instance on success,
 *                  NULL otherwise
 */
GDALDataset * WKTRasterDataset::Open(GDALOpenInfo * poOpenInfo) {
    GBool bTableHasIndex = FALSE;
    GBool bExistsOverviewsTable = FALSE;
    char * pszRasterColumnName = NULL;
    char * pszSchemaName = NULL;
    char * pszTableName = NULL;
    char * pszWhereClause = NULL;
    char * pszConnectionString = NULL;
    WKTRasterDataset * poDS = NULL;
    PGconn * hPGconn = NULL;
    PGresult * hPGresult = NULL;
    CPLString osCommand;
    char * pszArrayPixelTypes = NULL;
    char * pszArrayNodataValues = NULL;
    char ** papszPixelTypes = NULL;
    char ** papszNodataValues = NULL;
    GDALDataType hDataType = GDT_Byte;
    double dfNoDataValue = 0.0;
    int nCountNoDataValues = 0;
    char ** papszTokenizedStr = NULL;
    int nTokens = 0;
    int i;
    int nPos;
    const char * pszTmp;
    const char * pszToken;


    /********************************************************
     *               Security checkings
     ********************************************************/
    if (
            poOpenInfo->pszFilename == NULL ||
            !EQUALN(poOpenInfo->pszFilename, "PG:", 3)
            ) {
        /**
         * Drivers must quietly return NULL if the passed file is not of 
         * their format. They should only produce an error if the file 
         * does appear to be of their supported format, but for some 
         * reason, unsupported or corrupt
         */
        return NULL;
    }


    /*************************************************
     * Check GEOS library existence
     *************************************************/
    if (OGRGeometryFactory::haveGEOS() == FALSE) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't find GEOS library installed");
        return NULL;
    }


    /***************************************************
     * Fetch schema name, table name and where clause
     ***************************************************/
    papszTokenizedStr = CSLTokenizeString(
            poOpenInfo->pszFilename + 3);

    pszSchemaName = CPLStrdup(
            CSLFetchNameValueDef(papszTokenizedStr, "schema", "public"));
    
    pszTmp = CSLFetchNameValue(papszTokenizedStr, "table");
    pszTableName = pszTmp ? CPLStrdup(pszTmp) : NULL;
    if (pszTableName == NULL)
    {        
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't find a table name. Is connection string in the \
                format PG:\"host=<host> user=<user> password=<password> \
                dbname=<dbname> table=<raster_table> [schema=<schema>] \
                [where=<where_clause>]\"?\n");
        CPLFree(pszSchemaName);
        CSLDestroy(papszTokenizedStr);
    }

    pszTmp = CSLFetchNameValue(papszTokenizedStr, "where");
    pszWhereClause = pszTmp ? CPLStrdup(pszTmp) : NULL;

    /********************************************************************
     * Reconstruct connection string without these fields
     ********************************************************************/
    CSLDestroy(papszTokenizedStr);

    pszConnectionString = CPLStrdup(poOpenInfo->pszFilename);
    ExtractField(&pszConnectionString, "schema=");
    ExtractField(&pszConnectionString, "table=");
    if (pszWhereClause)
        ExtractField(&pszConnectionString, "where=");


    /********************************************************
     * 		Open a database connection
     ********************************************************/
    hPGconn = OpenConnection(pszConnectionString);
    
    CPLFree(pszConnectionString);
    pszConnectionString = NULL;

    if (hPGconn == NULL) {

        CPLFree(pszTableName);
        CPLFree(pszSchemaName);
        CPLFree(pszWhereClause);

        return NULL;
    }

    
    /************************************************
     * Check if the RASTER_COLUMNS table does exist
     ************************************************/
    if (ExistsRasterColumnsTable(hPGconn) == FALSE)  {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't find RASTER_COLUMNS table. Please, check WKT\
                Raster extension is properly installed");

        PQfinish(hPGconn);
        CPLFree(pszTableName);
        CPLFree(pszSchemaName);
        CPLFree(pszWhereClause);

        return NULL;
    }



    /******************************************************
     * Check if the table has a column of the type raster
     ******************************************************/
    pszRasterColumnName = GetWKTRasterColumnName(hPGconn, pszSchemaName,
            pszTableName);
    if (pszRasterColumnName == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't find a WKT Raster column in %s.%s table\n",
                pszSchemaName, pszTableName);
        PQfinish(hPGconn);
        CPLFree(pszTableName);
        CPLFree(pszSchemaName);
        CPLFree(pszWhereClause);

        return NULL;
    }


    /****************************************************
     * 	Check if the table has regular_blocking
     * 	constraint enabled. Currently only regular_blocking 
     * tables are allowed.
     *
     * 	TODO: The fact that "regular_blocking"
     * 	is set to TRUE doesn't mean that all the
     * 	tiles read are regular. Need to check it
     ****************************************************/
    if (TableHasRegularBlocking(hPGconn, pszTableName, 
        pszRasterColumnName, pszSchemaName) == FALSE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Table %s.%s doesn't seem to have regular blocking \
                arrangement. Only tables with regular blocking \
                arrangement can  be read\n",
                pszSchemaName, pszTableName);
        PQfinish(hPGconn);
        CPLFree(pszTableName);
        CPLFree(pszSchemaName);
        CPLFree(pszWhereClause);
        CPLFree(pszRasterColumnName);

        return NULL;

    }

    /********************************************
     * 	Check if the table has an index
     *  Useful for spatial queries in raster band
     ********************************************/
    bTableHasIndex = 
        TableHasGISTIndex(hPGconn, pszTableName, pszSchemaName);


    /*********************************************************************
     * At this point, we can start working with the given database and
     * table. 
     *********************************************************************/

    /********************************************************
     * 	Instantiate a new Dataset object
     ********************************************************/
    poDS = new WKTRasterDataset();

    /**
     * TODO: Allow regular and irregular blocking. By default,
     * we only allow regular blocking
     */
    poDS->bTableHasRegularBlocking = TRUE;


    poDS->pszRasterColumnName = pszRasterColumnName;
    poDS->bTableHasGISTIndex = bTableHasIndex;
    poDS->pszTableName = pszTableName;
    poDS->pszSchemaName = pszSchemaName;
    poDS->pszWhereClause = pszWhereClause;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->hPGconn = hPGconn;
    poDS->bCloseConnection = TRUE;  // dataset must close connection


    /********************************************
     *  Populate georeference related fields
     ********************************************/
    if (poDS->SetRasterProperties() == CE_Failure) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Couldn't get raster properties.");
        delete poDS;
        return NULL;
    } 

    /****************************************************************
     * Now, we're going to create the raster bands. First, we need
     * the pixel_types and nodata string representations (PQ arrays)
     ****************************************************************/    
    osCommand.Printf(
            "select pixel_types, nodata_values from raster_columns \
            where r_table_schema = '%s' and r_table_name = '%s' and \
            r_column = '%s'",
            poDS->pszSchemaName, poDS->pszTableName, 
            poDS->pszRasterColumnName);
    hPGresult = PQexec(poDS->hPGconn, osCommand.c_str());
    if (hPGresult == NULL || PQresultStatus(hPGresult) != PGRES_TUPLES_OK 
            || PQntuples(hPGresult) <= 0) 
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't get raster properties.");
        delete poDS;
        return NULL;
    } 
    else 
    {
        pszArrayPixelTypes = CPLStrdup(PQgetvalue(hPGresult, 0, 0));
        pszArrayNodataValues = CPLStrdup(PQgetvalue(hPGresult, 0, 1));

        /************************************************************
         * TODO: Use CSLTokenizeString for this
         ************************************************************/
        int nBands = 1;

        // Explode PQ arrays into char **
        if (pszArrayPixelTypes != NULL)
            papszPixelTypes = poDS->ExplodeArrayString(pszArrayPixelTypes,
                    &nBands);
        if (pszArrayNodataValues != NULL) {

            /**
             * Has nBands been fetched by the previous call? If yes, 
             * we don't need to fetch it in the next call
             */
            if (nBands != 0)
                papszNodataValues =
                        poDS->ExplodeArrayString(pszArrayNodataValues,
                    &nCountNoDataValues);

            // We don't have a value for nBands, try to fetch one
            else
                papszNodataValues =
                        poDS->ExplodeArrayString(pszArrayNodataValues,
                    &nBands);
        }

        poDS->nBands = nBands;
    }

    
    /**************************************************************
     * Create the raster bands
     **************************************************************/
    GBool bSignedByte = FALSE;
    int nBitDepth = 8;
    for (int iBand = 0; iBand < poDS->nBands; iBand++) {
        if (pszArrayPixelTypes != NULL && papszPixelTypes != NULL) {
            if (EQUALN(papszPixelTypes[iBand], "1BB", 3 * sizeof (char))) 
            {
                hDataType = GDT_Byte;
                nBitDepth = 1;
            }
            
            else if(EQUALN(papszPixelTypes[iBand], "2BUI", 
                        4 * sizeof (char))) 
            {
                hDataType = GDT_Byte;
                nBitDepth = 2;
            }

            else if (EQUALN(papszPixelTypes[iBand], "4BUI",
                    4 * sizeof (char))) 
            {
                hDataType = GDT_Byte;
                nBitDepth = 4;
            }
            
            else if (EQUALN(papszPixelTypes[iBand], "8BUI",
                    4 * sizeof (char))) 
            {
                hDataType = GDT_Byte;
                nBitDepth = 8;
            }

            else if (EQUALN(papszPixelTypes[iBand], "8BSI",
                    4 * sizeof (char))) 
            {
                hDataType = GDT_Byte;

                /**
                 * To indicate the unsigned byte values between 128 and 255
                 * should be interpreted as being values between -128 and 
                 * -1 for applications that recognise the SIGNEDBYTE type.
                 */
                bSignedByte = TRUE;

                nBitDepth = 8;
            }

            else if (EQUALN(papszPixelTypes[iBand], "16BSI",
                    5 * sizeof (char))) 
            {                
                hDataType = GDT_Int16;
                nBitDepth = 16;
            }

            else if (EQUALN(papszPixelTypes[iBand], "16BUI",
                    5 * sizeof (char))) 
            {
                hDataType = GDT_UInt16;
                nBitDepth = 16;
            }

            else if (EQUALN(papszPixelTypes[iBand], "32BSI",
                    5 * sizeof (char))) 
            {
                hDataType = GDT_Int32;
                nBitDepth = 32;
            }

            else if (EQUALN(papszPixelTypes[iBand], "32BUI",
                    5 * sizeof (char))) 
            {
                hDataType = GDT_UInt32;
                nBitDepth = 32;
            }

                
            else if (EQUALN(papszPixelTypes[iBand], "32BF",
                    4 * sizeof (char))) 
            {
                hDataType = GDT_Float32;
                nBitDepth = 32;
            }

            else if (EQUALN(papszPixelTypes[iBand], "64BF",
                    4 * sizeof (char))) 
            {
                hDataType = GDT_Float64;
                nBitDepth = 64;
            }

            else 
            {
                hDataType = GDT_Byte;
                nBitDepth = 8;
            }

        }

        // array of data types doesn't exist or is an empty string
        else 
        {
            hDataType = GDT_Byte;
            nBitDepth = 8;
        }

        if (pszArrayNodataValues != NULL && papszNodataValues != NULL &&
            iBand < nCountNoDataValues) 
        {
            dfNoDataValue = atof(papszNodataValues[iBand]);        
        }

        // array of nodata values doesn't exist or is an empty string
        else 
        {
            dfNoDataValue = 0.0;
        }


        // Create raster band objects
        poDS->SetBand(iBand + 1, new WKTRasterRasterBand(poDS, iBand + 1,
                hDataType, dfNoDataValue, bSignedByte, nBitDepth));
    }


    /******************************************************
     * Create overviews datasets, if does exist
     ******************************************************/
    bExistsOverviewsTable = ExistsOverviewsTable(hPGconn);
    if (bExistsOverviewsTable) {


        // Count the number of overviews        
        osCommand.Printf(
                "select o_table_name, overview_factor, o_column, \
                o_table_schema from raster_overviews where \
                r_table_schema = '%s' and r_table_name = '%s'",
                poDS->pszSchemaName, poDS->pszTableName);
        hPGresult = PQexec(poDS->hPGconn, osCommand.c_str());

        // no overviews
        if (
                hPGresult == NULL ||
                PQresultStatus(hPGresult) != PGRES_TUPLES_OK ||
                PQntuples(hPGresult) <= 0
                ) 
        {
            poDS->nOverviews = 0;
        }
            // overviews
        else 
        {
            poDS->nOverviews = PQntuples(hPGresult);
        }


        /**
         * Create overviews's Datasets and metadata
         */
        if (poDS->nOverviews > 0) 
        {

            poDS->papoWKTRasterOv =
                    (WKTRasterDataset **) VSICalloc(poDS->nOverviews,
                    sizeof (WKTRasterDataset *));
            if (poDS->papoWKTRasterOv == NULL) 
            {
                CPLError(CE_Warning, CPLE_ObjectNull,
                        "Couldn't allocate memory for overviews. Number \
                        of overviews will be set to 0");
                poDS->nOverviews = 0;
            }
            else 
            {

                for (int i = 0; i < poDS->nOverviews; i++) 
                {
                    /**
                     * CREATE DATASETS
                     */

                    // AND RASTERXSIZE?? IT SHOULD BE REDUCED...
                    poDS->papoWKTRasterOv[i] = new WKTRasterDataset();
                    poDS->papoWKTRasterOv[i]->nBlockSizeX =
                            poDS->nBlockSizeX;
                    poDS->papoWKTRasterOv[i]->nBlockSizeY =
                            poDS->nBlockSizeY;

                    int nOverviewFactor = atoi(
                            PQgetvalue(hPGresult, i, 1));
                    char * pszOVTableName = CPLStrdup(
                            PQgetvalue(hPGresult, i, 0));
                    char * pszOVColumnName = CPLStrdup(
                            PQgetvalue(hPGresult, i, 2));
                    char * pszOVSchemaName = CPLStrdup(
                            PQgetvalue(hPGresult, i, 3));

                    /**
                     * m/px is bigger in ov, because we are more far...
                     */
                    poDS->papoWKTRasterOv[i]->dfPixelSizeX =
                            poDS->dfPixelSizeX * nOverviewFactor;

                    poDS->papoWKTRasterOv[i]->dfPixelSizeY =
                            poDS->dfPixelSizeY * nOverviewFactor;

                    /**
                     * But raster size is smaller, for the same reason
                     * (we're more far covering the same area)
                     */
                    poDS->papoWKTRasterOv[i]->nRasterXSize =
                            poDS->nRasterXSize / nOverviewFactor;

                    poDS->papoWKTRasterOv[i]->nRasterYSize =
                            poDS->nRasterYSize / nOverviewFactor;

                    // Check these values (are correct??)
                    poDS->papoWKTRasterOv[i]->dfSkewX = poDS->dfSkewX;
                    poDS->papoWKTRasterOv[i]->dfSkewY = poDS->dfSkewY;
                    poDS->papoWKTRasterOv[i]->dfUpperLeftX =
                        poDS->dfUpperLeftX;
                    poDS->papoWKTRasterOv[i]->dfUpperLeftY = 
                        poDS->dfUpperLeftY;

                    poDS->papoWKTRasterOv[i]->hPGconn = poDS->hPGconn;
                    
                    // children datasets don't close connection
                    poDS->papoWKTRasterOv[i]->bCloseConnection = FALSE;
                    poDS->papoWKTRasterOv[i]->pszTableName = 
                        pszOVTableName;
                    poDS->papoWKTRasterOv[i]->pszSchemaName = 
                        pszOVSchemaName;
                    poDS->papoWKTRasterOv[i]->pszRasterColumnName =
                            pszOVColumnName;

                    poDS->papoWKTRasterOv[i]->pszWhereClause =
                            (poDS->pszWhereClause != NULL) ?
                                CPLStrdup(poDS->pszWhereClause):
                                NULL;

                    GBool bOVTableHasIndex = 
                        TableHasGISTIndex(poDS->hPGconn, pszOVTableName, 
                                pszOVSchemaName);
                    poDS->papoWKTRasterOv[i]->bTableHasGISTIndex =
                            bOVTableHasIndex;

                    poDS->papoWKTRasterOv[i]->nSrid = poDS->nSrid;


                    /**
                     * CREATE OVERVIEWS RASTER BANDS
                     */
                    poDS->papoWKTRasterOv[i]->nBands = poDS->nBands;
                    for (int j = 0; j < poDS->nBands; j++) 
                    {
                        WKTRasterRasterBand * poWKTRB =
                                (WKTRasterRasterBand *)
                                (poDS->GetRasterBand(j + 1));

                        poDS->papoWKTRasterOv[i]->SetBand(j + 1,
                                new WKTRasterRasterBand(
                                poDS->papoWKTRasterOv[i],
                                j + 1, poWKTRB->GetRasterDataType(),
                                poWKTRB->GetNoDataValue(),
                                poWKTRB->IsSignedByteDataType(),
                                poWKTRB->GetNBitDepth()));
                    }
                   
                } // end of creating overviews

            } // end else

            // free result
            PQclear(hPGresult);

        } // end if nOverviews > 0
    } // end if exists overview table

    /****************** Overviews created **********************/

    
    /***********************************************************
     * Free memory
     ***********************************************************/
    CSLDestroy(papszPixelTypes);
    CSLDestroy(papszNodataValues);
    CPLFree(pszArrayPixelTypes);
    CPLFree(pszArrayNodataValues);

     // All WKT Raster bands are consecutives
    poDS->SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");

    return poDS;
}

/**
 * Method: GetGeoTransform
 * Fetches the coefficients for transforming between pixel/line (P,L) 
 * raster space and projection coordinates (Xp, Yp) space
 * The affine transformation performed is:
 * 	Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
 * 	Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
 * Parameters:
 *  - double *: pointer to a double array, that will contains the affine
 * 	transformation matrix coefficients
 * Returns:
 *  - CPLErr: CE_None on success, or CE_Failure if no transform can be 
 *            fetched.
 */
CPLErr WKTRasterDataset::GetGeoTransform(double * padfTransform) {
    // checking input parameters
    // NOT NEEDED (Is illegal to call GetGeoTransform with a NULL
    // argument. Thanks to Even Rouault)
    /*
    if (padfTransform == NULL)
    {
            // default matrix
            padfTransform[0] = 0.0;
            padfTransform[1] = 1.0;
            padfTransform[2] = 0.0;
            padfTransform[3] = 0.0;
            padfTransform[4] = 0.0;
            padfTransform[5] = 1.0;

            return CE_Failure;
    }
     */

    // copy necessary values in supplied buffer
    padfTransform[0] = dfUpperLeftX;
    padfTransform[1] = dfPixelSizeX;
    padfTransform[2] = dfSkewX;
    padfTransform[3] = dfUpperLeftY;
    padfTransform[4] = dfSkewY;
    padfTransform[5] = dfPixelSizeY;

    return CE_None;
}

/**
 * Fetch the projection definition string for this dataset in OpenGIS WKT
 * format. It should be suitable for use with the OGRSpatialReference class
 * Parameters: none
 * Returns:
 *  - const char *: a pointer to an internal projection reference string.
 *                  It should not be altered, freed or expected to last for
 *                  long.
 */
const char * WKTRasterDataset::GetProjectionRef() {
    CPLString osCommand;
    PGresult * hResult;
    char * pszProjection;

    if (nSrid == -1) {
        return "";
    }

    /********************************************************
     *      	Reading proj from database
     ********************************************************/    
    osCommand.Printf("SELECT srtext FROM spatial_ref_sys where SRID=%d", 
            nSrid);

    hResult = PQexec(this->hPGconn, osCommand.c_str());

    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0) {
        pszProjection = CPLStrdup(PQgetvalue(hResult, 0, 0));
    }

    if (hResult)
        PQclear(hResult);

    return pszProjection;
}

/**
 * Set the projection reference string for this dataset. The string should
 * be in OGC WKT or PROJ.4 format
 * Parameters:
 *  - const char *: projection reference string.
 * Returns:
 *  - CE_Failure if an error occurs, otherwise CE_None.
 */
CPLErr WKTRasterDataset::SetProjection(const char * pszProjectionRef)
{
    VALIDATE_POINTER1(pszProjectionRef, "SetProjection", CE_Failure);

    CPLString osCommand;
    PGresult * hResult;
    int nFetchedSrid = -1;


    /*****************************************************************
     * Check if the dataset allows updating
     *****************************************************************/
    if (GetAccess() != GA_Update) {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                "This driver doesn't allow write access");
        return CE_Failure;
    }

    /*****************************************************************
     * Look for projection with this text
     *****************************************************************/

    // First, WKT text
    osCommand.Printf("SELECT srid FROM spatial_ref_sys where srtext='%s'",
            pszProjectionRef);
    hResult = PQexec(hPGconn, osCommand.c_str());

    if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0) 
    {

        nFetchedSrid = atoi(PQgetvalue(hResult, 0, 0));

        // update class attribute
        nSrid = nFetchedSrid;


        // update raster_columns table
        osCommand.Printf("UPDATE raster_columns SET srid=%d WHERE \
                    r_table_name = '%s' AND r_column = '%s'",
                nSrid, pszTableName, pszRasterColumnName);
        hResult = PQexec(hPGconn, osCommand.c_str());
        if (hResult == NULL || PQresultStatus(hResult) != PGRES_COMMAND_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Couldn't update raster_columns table: %s",
                    PQerrorMessage(hPGconn));
            return CE_Failure;
        }

        // TODO: Update ALL blocks with the new srid...

        return CE_None;
    }

    // If not, proj4 text
    else 
    {
        osCommand.Printf(
                "SELECT srid FROM spatial_ref_sys where proj4text='%s'",
            pszProjectionRef);
        hResult = PQexec(this->hPGconn, osCommand.c_str());

        if (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) > 0) 
        {

            nFetchedSrid = atoi(PQgetvalue(hResult, 0, 0));

            // update class attribute
            nSrid = nFetchedSrid;

            // update raster_columns table            
            osCommand.Printf("UPDATE raster_columns SET srid=%d WHERE \
                    r_table_name = '%s' AND r_column = '%s'",
                    nSrid, pszTableName, pszRasterColumnName);

            hResult = PQexec(hPGconn, osCommand.c_str());
            if (hResult == NULL ||
                    PQresultStatus(hResult) != PGRES_COMMAND_OK) 
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Couldn't update raster_columns table: %s",
                        PQerrorMessage(hPGconn));
                return CE_Failure;
            }

            // TODO: Update ALL blocks with the new srid...

            return CE_None;
        }

        else 
        {
            CPLError(CE_Failure, CPLE_WrongFormat,
                    "Couldn't find WKT neither proj4 definition");
            return CE_Failure;
        }
    }
}

/**
 * Set the affine transformation coefficients.
 * Parameters:
 *  - double *:a six double buffer containing the transformation
 *             coefficients to be written with the dataset.
 *  Returns:
 *  - CE_None on success, or CE_Failure if this transform cannot be written.
 */
CPLErr WKTRasterDataset::SetGeoTransform(double * padfTransform)
{
    VALIDATE_POINTER1(padfTransform, "SetGeoTransform", CE_Failure);

    dfUpperLeftX = padfTransform[0];
    dfPixelSizeX = padfTransform[1];
    dfSkewX = padfTransform[2];
    dfUpperLeftY = padfTransform[3];
    dfSkewY = padfTransform[4];
    dfPixelSizeY = padfTransform[5];

    // TODO: Update all data blocks with these values
    
    return CE_None;
}


/************************************************************************/
/*                          GDALRegister_WKTRaster()                    */
/************************************************************************/
void GDALRegister_WKTRaster() 
{
    GDALDriver *poDriver;

    if (GDALGetDriverByName("WKTRaster") == NULL) {
        poDriver = new GDALDriver();

        poDriver->SetDescription("WKTRaster");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                "PostGIS WKT Raster driver");

        poDriver->pfnOpen = WKTRasterDataset::Open;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

