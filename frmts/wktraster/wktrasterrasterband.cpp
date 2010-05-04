
#include <string>


#include "cpl_string.h"

/******************************************************************************
 * File :    wktrasterrasterband.cpp
 * Project:  WKT Raster driver
 * Purpose:  GDAL Dataset code for WKTRaster driver 
 * Author:   Jorge Arevalo, jorgearevalo@gis4free.org
 * 
 * Last changes:
 * $Id$
 *
 * TODO:
 *  - Block caching, to avoid fetching the whole raster from database each time
 *    IReadBlock is called.
 *  - Read outdb rasters. Take into account that the outdb raster may don't have
 *    the same block structure...
 *  - Update raster_columns table if values read from IReadBlock are different
 *
 ******************************************************************************
 * Copyright (c) 2009, Jorge Arevalo, jorgearevalo@gis4free.org
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
 ******************************************************************************/
#include "wktraster.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include <gdal_priv.h>

CPL_CVSID("$Id$");

/**
 * Constructor.
 * Parameters:
 *  - WKTRasterDataset *: The Dataset this band belongs to
 *  - int: the band number
 *  - GDALDataType: The data type for this band
 *  - double: The nodata value.  Could be any kind of data (GByte, GUInt16,
 *          GInt32...) but the variable has the bigger type.
 *  - GBool: if the data type is signed byte or not. If yes, the SIGNEDBYTE
 *          metadata value is added to IMAGE_STRUCTURE domain
 *  - int: The bit depth, to add NBITS as metadata value in IMAGE_STRUCTURE
 *          domain.
 */
WKTRasterRasterBand::WKTRasterRasterBand(WKTRasterDataset *poDS,
        int nBand, GDALDataType hDataType, double dfNodata, GBool bSignedByte,
        int nBitD) {
    
    VALIDATE_POINTER0(poDS, "WKTRasterRasterBand");

    poDS = poDS;
    nBand = nBand;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();

    nBlockXSize = ((WKTRasterDataset *) poDS)->nBlockSizeX;
    nBlockYSize = ((WKTRasterDataset *) poDS)->nBlockSizeY;

     // Check Irregular blocking
    if (nBlockXSize == 0 || nBlockYSize == 0) {
        CPLError(CE_Warning, CPLE_NotSupported,
                "This band has irregular blocking, but is not supported yet");
    }


    eAccess = ((WKTRasterDataset *) poDS)->GetAccess();

    // Get no data value and pixel type from dataset too
    eDataType = hDataType;
    dfNoDataValue = dfNodata;

    nBitDepth = nBitD;

    // Add pixeltype to image structure domain
    if (bSignedByte == TRUE) {
        SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
        bIsSignedByte = bSignedByte;
    }

    // Add NBITS to metadata only for sub-byte types
    if (nBitDepth < 8)
        SetMetadataItem("NBITS", CPLString().Printf( "%d", nBitDepth ),
            "IMAGE_STRUCTURE" );
}

/**
 * Says if the datatype for this band is signedbyte.
 * Parameters: none
 * Returns:
 *  - TRUE if the datatype for this band is signedbyte, FALSE otherwise
 */
 GBool WKTRasterRasterBand::IsSignedByteDataType()
 {
     return bIsSignedByte;
}

/**
 * Get the bit depth for this raster band
 * Parameters: none.
 * Returns: the bit depth
 */
 int WKTRasterRasterBand::GetNBitDepth()
 {
     return nBitDepth;
 }

/**
 * Write a block of data to the raster band. First establish if a corresponding
 * row to the block already exists with a SELECT. If so, update the raster 
 * column contents. If it does not exist create a new row for the block.
 * Inputs:
 *  int: horizontal block offset
 *  int: vertical block offset
 *  void *: The buffer wit the data to be write
 * Output:
 *  CE_None on success, CE_Failure on error
 */
CPLErr WKTRasterRasterBand::IWriteBlock(int nBlockXOff,
        int nBlockYOff, void * pImage) {

    // Check parameters
    if (pImage == NULL || nBlockXOff < 0 || nBlockYOff < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Unsupported block size or NULL buffer");
        return CE_Failure;
    }

    WKTRasterDataset * poWKTRasterDS = (WKTRasterDataset *) poDS;
    CPLString osCommand;
    PGresult * hPGresult;
    int nPixelSize;
    int nPixelXInitPosition;
    int nPixelYInitPosition;
    int nPixelXEndPosition;
    int nPixelYEndPosition;
    double adfTransform[6];
    double fProjXInit = 0.0;
    double fProjYInit = 0.0;
    double fProjXEnd = 0.0;
    double fProjYEnd = 0.0;
    double fProjLowerLeftX = 0.0;
    double fProjLowerLeftY = 0.0;
    double fProjUpperRightX = 0.0;
    double fProjUpperRightY = 0.0;
    GByte byMachineEndianess = NDR;
    int nTuples = 0;
    char * pszHexWkb = NULL;
    WKTRasterWrapper * poWKTRasterWrapper = NULL;
    WKTRasterBandWrapper * poWKTRasterBandWrapper = NULL;
    int nRid = 0;
    int nCurrentBandPixelSize = 0;


    // Check machine endianess
#ifdef CPL_LSB
    byMachineEndianess = NDR;
#else
    byMachineEndianess = XDR;
#endif

    /***********************************************************************
     * Get pixel size (divide by 8 because GDALGetDataTypeSize returns the
     * size in bits)
     ***********************************************************************/
    nPixelSize = GDALGetDataTypeSize(eDataType) / 8;

    /***********************************************************************
     * nBlockXOff and nBlockYOff are block offsets. So, we first have to
     * transform them in pixel/line coordinates, taking into account the
     * size of a block.
     ***********************************************************************/
    nPixelXInitPosition = nBlockXSize * nBlockXOff;
    nPixelYInitPosition = nBlockYSize * nBlockYOff;

    // Now, get the end of the block
    nPixelXEndPosition = nPixelXInitPosition + nBlockXSize;
    nPixelYEndPosition = nPixelYInitPosition + nBlockYSize;


    /**************************************************************************
     * Transform pixel/line coordinates into coordinates of the raster
     * reference system.
     * NOTE: I take the georeference information from dataset. The SQL function
     * ST_GdalGeoTransform takes the same information from the raster row, in
     * array format.
     **************************************************************************/
    poWKTRasterDS->GetGeoTransform(adfTransform);
    fProjXInit = adfTransform[0] +
            nPixelXInitPosition * adfTransform[1] +
            nPixelYInitPosition * adfTransform[2];

    fProjYInit = adfTransform[3] +
            nPixelXInitPosition * adfTransform[4] +
            nPixelYInitPosition * adfTransform[5];

    fProjXEnd = adfTransform[0] +
            nPixelXEndPosition * adfTransform[1] +
            nPixelYEndPosition * adfTransform[2];

    fProjYEnd = adfTransform[3] +
            nPixelXEndPosition * adfTransform[4] +
            nPixelYEndPosition * adfTransform[5];


    /*************************************************************************
     * Now we have the block coordinates transformed into coordinates of the
     * raster reference system. This coordinates are from:
     *  - Upper left corner
     *  - Lower right corner
     * But for ST_MakeBox2D, we'll need block's coordinates of:
     *  - Lower left corner
     *  - Upper righ corner
     *************************************************************************/
    fProjLowerLeftX = fProjXInit;
    fProjLowerLeftY = fProjYEnd;

    fProjUpperRightX = fProjXEnd;
    fProjUpperRightY = fProjYInit;


    /*************************************************************************
     * Perform a spatial query that gives the tile/block (row of raster table)
     * or tiles/blocks (in case of non-regular blocking) that should contain
     * this block
     *************************************************************************/

    if (poWKTRasterDS->pszWhereClause != NULL) {

        /**
         * Table has GIST index-
         * We could disable sequential scanning, but for versions of PostGIS
         * up to 0.8, this is no necessary. PostGIS >=0.8 is correctly
         * integrated with query planner, thus PostgreSQL will use indexes
         * whenever appropriate.
         * NOTE: We should add version checking here, and disable sequential
         * scanning when needed. See OGRDataSource::Open method.
         */
        if (poWKTRasterDS->bTableHasGISTIndex) {

            /**
             * This queries for the tiles that contains the given block. When
             * regular_blocking rasters, the blocks will have the same size of
             * a tile, so, we can use "contain" function and "~" operator.
             * But when non-regular_blocking, the only way this will work is
             * setting the block size to the smallest tile size. The problem is
             * how do we know all the tiles size?
             */
            osCommand.Printf(
                    "SELECT rid, %s FROM %s.%s WHERE %s ~ ST_SetSRID(ST_MakeBox2D\
                    (ST_Point(%f, %f),ST_Point(%f, %f)), %d) AND %s",
                    poWKTRasterDS->pszRasterColumnName,
                    poWKTRasterDS->pszSchemaName,
                    poWKTRasterDS->pszTableName,
                    poWKTRasterDS->pszRasterColumnName, fProjLowerLeftX,
                    fProjLowerLeftY, fProjUpperRightX, fProjUpperRightY,
                    poWKTRasterDS->nSrid, poWKTRasterDS->pszWhereClause);

        }            /**
             * Table hasn't a GIST index. Normal searching
             */
        else {
            osCommand.Printf(
                    "SELECT rid, %s FROM %s.%s WHERE _ST_Contains(%s, ST_SetSRID(\
                    ST_MakeBox2D(ST_Point(%f, %f), ST_Point(%f, %f)), %d)) AND \
                    %s",
                    poWKTRasterDS->pszRasterColumnName,
                    poWKTRasterDS->pszSchemaName,
                    poWKTRasterDS->pszTableName,
                    poWKTRasterDS->pszRasterColumnName, fProjLowerLeftX,
                    fProjLowerLeftY, fProjUpperRightX, fProjUpperRightY,
                    poWKTRasterDS->nSrid, poWKTRasterDS->pszWhereClause);
        }


    }        // No where clause
    else {


        /**
         * Table has a GIST index.
         */
        if (poWKTRasterDS->bTableHasGISTIndex) {

            osCommand.Printf(
                    "SELECT rid, %s FROM %s.%s WHERE %s ~ ST_SetSRID(ST_MakeBox2D\
                    (ST_Point(%f, %f), ST_Point(%f, %f)), %d)",
                    poWKTRasterDS->pszRasterColumnName,
                    poWKTRasterDS->pszSchemaName,
                    poWKTRasterDS->pszTableName,
                    poWKTRasterDS->pszRasterColumnName, fProjLowerLeftX,
                    fProjLowerLeftY, fProjUpperRightX, fProjUpperRightY,
                    poWKTRasterDS->nSrid);

        }            /**
             * Table hasn't a GIST index. Normal searching
             */
        else {
            osCommand.Printf(
                    "SELECT rid, %s FROM %s.%s WHERE _ST_Contains(%s, ST_SetSRID(\
                    ST_MakeBox2D(ST_Point(%f, %f), ST_Point(%f, %f)), %d))",
                    poWKTRasterDS->pszRasterColumnName,
                    poWKTRasterDS->pszSchemaName,
                    poWKTRasterDS->pszTableName,
                    poWKTRasterDS->pszRasterColumnName, fProjLowerLeftX, 
                    fProjLowerLeftY, fProjUpperRightX, fProjUpperRightY,
                    poWKTRasterDS->nSrid);
        }
    }


    //printf("query: %s\n", osCommand.c_str());

    hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());
    if (hPGresult == NULL || PQresultStatus(hPGresult) != PGRES_TUPLES_OK) {
        if (hPGresult)
            PQclear(hPGresult);

        CPLError(CE_Failure, CPLE_AppDefined,
                "Sorry, couldn't fetch block information from database: %s",
                PQerrorMessage(poWKTRasterDS->hPGconn));
        return CE_Failure;
    }


    nTuples = PQntuples(hPGresult);


    /*******************************************************
     * Block not found. We have to create a new one
     *******************************************************/
    if (nTuples <= 0) {

        /**
         * There is no block. We need to create a new one. We can use the
         * first block of the table and modify it.
         */
        PQclear(hPGresult);
        osCommand.Printf(
                "SELECT %s FROM %s.%s LIMIT 1 OFFSET 0",
                poWKTRasterDS->pszRasterColumnName,
                poWKTRasterDS->pszSchemaName,
                poWKTRasterDS->pszTableName);

        hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());

        /**
         * Empty table, What should we do? Is it an error?
         */
        if (hPGresult == NULL || PQresultStatus(hPGresult) != PGRES_TUPLES_OK) {
            if (hPGresult)
                PQclear(hPGresult);

            CPLError(CE_Failure, CPLE_AppDefined,
                    "Sorry, couldn't fetch block information from database: %s",
                    PQerrorMessage(poWKTRasterDS->hPGconn));

            return CE_Failure;
        }
        
        // Get HEXWKB representation of the block
        pszHexWkb = CPLStrdup(PQgetvalue(hPGresult, 0, 0));
        PQclear(hPGresult);

        // Create a wrapper object with this data
        poWKTRasterWrapper = new WKTRasterWrapper();

        // Should we try creating the raster block in other way?
        if (poWKTRasterWrapper->Initialize(pszHexWkb) == FALSE) {
            CPLFree(pszHexWkb);
            delete poWKTRasterWrapper; 
            return CE_Failure;
        }

        // We won't need this
        CPLFree(pszHexWkb);

        // Get raster band
        poWKTRasterBandWrapper = poWKTRasterWrapper->GetBand((GUInt16)nBand);

        // Should we try creating the raster block in other way?
        if (poWKTRasterBandWrapper == NULL) {
            delete poWKTRasterWrapper;
            return CE_Failure;
        }

        // Set raster data
        poWKTRasterBandWrapper->SetData((GByte *)pImage,
                (nBlockXSize * nBlockYSize) * nPixelSize * sizeof (GByte));

        // Insert new block into table. First, we need a new rid
        osCommand.Printf(
                "SELECT rid FROM %s.%s ORDER BY rid DESC LIMIT 1 OFFSET 0",
                poWKTRasterDS->pszSchemaName, poWKTRasterDS->pszTableName);

        hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());
        if (
                hPGresult == NULL ||
                PQresultStatus(hPGresult) != PGRES_TUPLES_OK ||
                PQntuples(hPGresult) <= 0) {
            // What should we do?
            if (hPGresult)
                PQclear(hPGresult);
            delete poWKTRasterWrapper;
            delete poWKTRasterBandWrapper;

            return CE_Failure;
        }

        int nRid = atoi(PQgetvalue(hPGresult, 0, 0)) + 1;
        pszHexWkb = poWKTRasterWrapper->GetHexWkbRepresentation();

        // Insert the block        
        osCommand.Printf(
                "INSERT INTO %s.%s (rid, %s) VALUES (%d, %s)",
                poWKTRasterDS->pszSchemaName,poWKTRasterDS->pszTableName,
                poWKTRasterDS->pszRasterColumnName,nRid, pszHexWkb);

        hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());
        if (hPGresult == NULL || 
                PQresultStatus(hPGresult) != PGRES_COMMAND_OK) {
            CPLError(CE_Failure, CPLE_NoWriteAccess,
                    "Couldn't add new block to database: %s",
                    PQerrorMessage(poWKTRasterDS->hPGconn));
            if (hPGresult)
                PQclear(hPGresult);
            delete poWKTRasterWrapper;
            delete poWKTRasterBandWrapper;
            CPLFree(pszHexWkb);

            return CE_Failure;
        }

        // block added
        CPLFree(pszHexWkb);
        PQclear(hPGresult);
    }

   /****************************************************************
    * One block found: We have to update the block data
    ****************************************************************/
    else if (nTuples == 1) {

        // get data
        nRid = atoi(PQgetvalue(hPGresult, 0, 0));
        pszHexWkb = CPLStrdup(PQgetvalue(hPGresult, 0, 1));
        PQclear(hPGresult);

        // Create wrapper
         // Should we try creating the raster block in other way?
        poWKTRasterWrapper = new WKTRasterWrapper();
 	if (poWKTRasterWrapper->Initialize(pszHexWkb) == FALSE) {
            CPLFree(pszHexWkb);
            delete poWKTRasterWrapper; 
            return CE_Failure;
        }

        // We won't need this
        CPLFree(pszHexWkb);

        // Get raster band
        poWKTRasterBandWrapper = poWKTRasterWrapper->GetBand((GUInt16)nBand);

        // Should we try creating the raster block in other way?

        if (poWKTRasterBandWrapper == NULL) {
            delete poWKTRasterWrapper;
            return CE_Failure;
        }

        /**
         * Swap words if needed
         */
        if (poWKTRasterWrapper->byEndianess != byMachineEndianess) {

            // Get pixel size of this band
            switch (poWKTRasterBandWrapper->byPixelType) {
                case 0: case 1: case 2: case 3: case 4:
                    nCurrentBandPixelSize = 1;
                    break;
                case 5: case 6: case 9:
                    nCurrentBandPixelSize = 2;
                    break;
                case 7: case 8: case 10:
                    nCurrentBandPixelSize = 4;
                    break;
                case 11:
                    nCurrentBandPixelSize = 8;
                    break;
                default:
                    nCurrentBandPixelSize = 1;
            }

            GDALSwapWords((GByte *)pImage, nCurrentBandPixelSize,
                    poWKTRasterBandWrapper->nDataSize / nCurrentBandPixelSize,
                    nCurrentBandPixelSize);
        }

        // Set raster data
        poWKTRasterBandWrapper->SetData((GByte *)pImage,
                (nBlockXSize * nBlockYSize) * nPixelSize * sizeof (GByte));
        
        // Get hexwkb again, with new data
        pszHexWkb = poWKTRasterWrapper->GetHexWkbRepresentation();


        // update register
        osCommand.Printf(
                "UPDATE %s.%s SET %s = %s WHERE rid = %d",
                poWKTRasterDS->pszSchemaName, poWKTRasterDS->pszTableName,
                poWKTRasterDS->pszRasterColumnName, pszHexWkb, nRid);
        hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());
        if (hPGresult == NULL ||
                PQresultStatus(hPGresult) != PGRES_COMMAND_OK) {
            if (hPGresult)
                PQclear(hPGresult);
            CPLFree(pszHexWkb);
            CPLError(CE_Failure, CPLE_NoWriteAccess,
                    "Couldn't update the raster data");
            return CE_Failure;
        }

        // ok, updated
        CPLFree(pszHexWkb);
        PQclear(hPGresult);

    }
   
    /*****************************************************************
     * More than one block found. What should we do?
     *****************************************************************/
    else {
        // Only regular_block supported, just now
        PQclear(hPGresult);
        CPLError(CE_Failure, CPLE_NotSupported,
                "Sorry, but the raster presents block overlapping. This feature\
                is under development");

        return CE_Failure;
    }


    return CE_None;
}


/**
 * Read a block of image data
 * Inputs:
 *  int: horizontal block offset
 *  int: vertical block offset
 *  void *: The buffer into the data will be read
 * Output:
 *  CE_None on success, CE_Failure on error
 */
CPLErr WKTRasterRasterBand::IReadBlock(int nBlockXOff,
        int nBlockYOff, void * pImage) {

    WKTRasterDataset * poWKTRasterDS = (WKTRasterDataset *) poDS;
    CPLString osCommand;
    PGresult * hPGresult;
    int nPixelSize;
    int nPixelXInitPosition;
    int nPixelYInitPosition;
    int nPixelXEndPosition;
    int nPixelYEndPosition;
    double adfTransform[6];
    double dfProjXInit = 0.0;
    double dfProjYInit = 0.0;
    double dfProjXEnd = 0.0;
    double dfProjYEnd = 0.0;
    double dfProjLowerLeftX = 0.0;
    double dfProjLowerLeftY = 0.0;
    double dfProjUpperRightX = 0.0;
    double dfProjUpperRightY = 0.0;
    char * pszHexWkb = NULL;
    int nTuples = 0;
    GByte * pbyRasterData = NULL;
    WKTRasterWrapper * poWKTRasterWrapper = NULL;
    WKTRasterBandWrapper * poWKTRasterBandWrapper = NULL;
    int nNaturalXBlockSize = 0;
    int nNaturalYBlockSize = 0;
    int nPadXSize = 0;
    int nPadYSize = 0;
    int nBlockXBound = 0;
    int nBlockYBound = 0;

    /* Check input parameters */
    if (pImage == NULL || nBlockXOff < 0 || nBlockYOff < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Unsupported block size or NULL buffer");
        return CE_Failure;
    }

    /*************************************************************************
     * Get pixel size (divide by 8 because GDALGetDataTypeSize returns the
     * size in bits)
     *************************************************************************/
    nPixelSize = MAX(1,GDALGetDataTypeSize(eDataType)/8);

    /*************************************************************************
     * nBlockXOff and nBlockYOff are block offsets. So, we first have to
     * transform them in pixel/line coordinates, taking into account the
     * size of a block.
     *************************************************************************/
    GetBlockSize(&nNaturalXBlockSize, &nNaturalYBlockSize);

    /**
     * The end of this block is the start of the next one
     * xxx jorgearevalo: sure?? always??
     */
    nBlockXBound = (nBlockXOff * nNaturalXBlockSize) + nNaturalXBlockSize;
    nBlockYBound = (nBlockYOff * nNaturalYBlockSize) + nNaturalYBlockSize;

    if (nBlockXBound > nRasterXSize)
        nPadXSize = nBlockXBound - nRasterXSize;            
    if (nBlockYBound > nRasterYSize)
        nPadYSize = nBlockYBound - nRasterYSize;


    nPixelXInitPosition = nBlockXOff * nNaturalXBlockSize;
    nPixelYInitPosition = nBlockYOff * nNaturalYBlockSize;

    nPixelXEndPosition = nPixelXInitPosition + (nNaturalXBlockSize - nPadXSize);
    nPixelYEndPosition = nPixelYInitPosition + (nNaturalYBlockSize - nPadYSize);

    /**************************************************************************
     * Transform pixel/line coordinates into coordinates of the raster
     * reference system.
     * NOTE: I take the georeference information from dataset. The SQL function
     * ST_GdalGeoTransform takes the same information from the raster row, in
     * array format.
     **************************************************************************/
    poWKTRasterDS->GetGeoTransform(adfTransform);
   
    dfProjXInit = adfTransform[0] +
            nPixelXInitPosition * adfTransform[1] +
            nPixelYInitPosition * adfTransform[2];

    dfProjYInit = adfTransform[3] +
            nPixelXInitPosition * adfTransform[4] +
            nPixelYInitPosition * adfTransform[5];

    dfProjXEnd = adfTransform[0] +
            nPixelXEndPosition * adfTransform[1] +
            nPixelYEndPosition * adfTransform[2];

    dfProjYEnd = adfTransform[3] +
            nPixelXEndPosition * adfTransform[4] +
            nPixelYEndPosition * adfTransform[5];


    /*************************************************************************
     * Now we have the block coordinates transformed into coordinates of the
     * raster reference system. This coordinates are from:
     *  - Upper left corner
     *  - Lower right corner
     * But for ST_MakeBox2D, we'll need block's coordinates of:
     *  - Lower left corner
     *  - Upper right corner
     *************************************************************************/
    dfProjLowerLeftX = dfProjXInit;
    dfProjLowerLeftY = dfProjYEnd;

    dfProjUpperRightX = dfProjXEnd;
    dfProjUpperRightY = dfProjYInit;


    /**************************************************************************
     * Perform a spatial query that gives the tile/block (row of raster table)
     * or tiles/blocks (in case of non-regular blocking) that contain this block
     **************************************************************************/
    if (poWKTRasterDS->pszWhereClause != NULL)
    {
       osCommand.Printf(
               "SELECT %s FROM %s.%s WHERE %s ~ ST_SetSRID(ST_MakeBox2D\
                (ST_Point(%f, %f), ST_Point(%f, %f)), %d) AND %s",
               poWKTRasterDS->pszRasterColumnName, poWKTRasterDS->pszSchemaName,
               poWKTRasterDS->pszTableName, poWKTRasterDS->pszRasterColumnName,
               dfProjLowerLeftX, dfProjLowerLeftY, dfProjUpperRightX,
               dfProjUpperRightY, poWKTRasterDS->nSrid,
               poWKTRasterDS->pszWhereClause); 
    }

    else
    {
       osCommand.Printf(
               "SELECT %s FROM %s.%s WHERE %s ~ ST_SetSRID(ST_MakeBox2D\
                (ST_Point(%f, %f), ST_Point(%f, %f)), %d)",
               poWKTRasterDS->pszRasterColumnName, poWKTRasterDS->pszSchemaName,
               poWKTRasterDS->pszTableName, poWKTRasterDS->pszRasterColumnName,
               dfProjLowerLeftX, dfProjLowerLeftY, dfProjUpperRightX,
               dfProjUpperRightY, poWKTRasterDS->nSrid);
 
    }

    hPGresult = PQexec(poWKTRasterDS->hPGconn, osCommand.c_str());

    if (hPGresult == NULL ||
            PQresultStatus(hPGresult) != PGRES_TUPLES_OK ||
            PQntuples(hPGresult) < 0)
    {
        if (hPGresult)
            PQclear(hPGresult);
        CPLError(CE_Failure, CPLE_AppDefined,
                "Sorry, couldn't fetch block information from database: %s",
                PQerrorMessage(poWKTRasterDS->hPGconn));
        return CE_Failure;
    }


    nTuples = PQntuples(hPGresult);

    /*****************************************************************
     * No blocks found. Fill the buffer with nodata value
     *****************************************************************/
    if (nTuples == 0) 
    {
        NullBlock(pImage);
        return CE_None;
    }

    
    /******************************************************************
     * One block found. Regular blocking arrangements, no overlaps
     ******************************************************************/
    else if (nTuples == 1) 
    {
        // Get HEXWKB representation of the block
        pszHexWkb = CPLStrdup(PQgetvalue(hPGresult, 0, 0));

        PQclear(hPGresult);

        // Raster hex must have an even number of characters
        if (pszHexWkb == NULL || strlen(pszHexWkb) % 2) 
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "The HEXWKB data fetch from database must have an even number \
                  of characters");
            return CE_Failure;
        }


        // Create a wrapper object
        poWKTRasterWrapper = new WKTRasterWrapper();
        if (poWKTRasterWrapper->Initialize(pszHexWkb) == FALSE) 
        {
            CPLFree(pszHexWkb);
            delete poWKTRasterWrapper;
            return CE_Failure;
        }
        

        // We won't need this
        CPLFree(pszHexWkb);

        // Create raster band wrapper
        poWKTRasterBandWrapper = poWKTRasterWrapper->GetBand((GUInt16)nBand);
        if (poWKTRasterBandWrapper == NULL) 
        {
            CPLError(CE_Failure, CPLE_ObjectNull,"Couldn't fetch band data");
            delete poWKTRasterWrapper;
            return CE_Failure;
        }

        // Get raster data
        pbyRasterData = poWKTRasterBandWrapper->GetData();

       
        
        //printf("IReadBlock with offset: %d, %d\n", nBlockXOff, nBlockYOff);

        /**********************************************************
         * Check if the raster is offline
         **********************************************************/
        if (poWKTRasterBandWrapper->bIsOffline == TRUE) 
        {
            // The raster data in this case is a path to the raster file
            int nBandToRead = poWKTRasterBandWrapper->nOutDbBandNumber;


            // Open dataset, if needed
            if  (poWKTRasterDS->poOutdbRasterDS == NULL) 
            {
                poWKTRasterDS->poOutdbRasterDS = (GDALDataset *)
                        GDALOpen((char *)pbyRasterData, GA_ReadOnly);
            }

            if (poWKTRasterDS->poOutdbRasterDS != NULL)

                // Read data from band
                /**
                 * NOT SO SIMPLE!!!!
                 * The outdb file may don't have the same block structure...
                 */

                
                poWKTRasterDS->poOutdbRasterDS->GetRasterBand(nBandToRead)->ReadBlock(
                    nBlockXOff, nBlockYOff, pImage);                             
            else 
            {
                CPLError(CE_Failure, CPLE_ObjectNull,
                    "Couldn't read band data from out-db raster");
                delete poWKTRasterWrapper;
                return CE_Failure;
            }
            
        }


        /****************************
         * Indb raster
         ****************************/
        else 
        {
            /**
             * Copy the data buffer into pImage.
             * nBlockXSize * nBlockYSize should be equal to nDataSize/2
             */
            memcpy(pImage, pbyRasterData,
                    (nNaturalXBlockSize * nNaturalYBlockSize) *
                    nPixelSize * sizeof (GByte));
        }
       


        // Free resources and exit
        delete poWKTRasterWrapper;

        return CE_None;

    }// end if ntuples == 1

        /*********************************************************************
         * More than one block found. Non regular blocking arrangements
         *********************************************************************/
    else 
    {
        // Only regular_block supported, just now
        PQclear(hPGresult);
        CPLError(CE_Failure, CPLE_NotSupported,
                "Sorry, but the raster presents block overlapping. This feature \
                is under development");

        return CE_Failure;
    }
}

/**
 * Set the block data to the null value if it is set, or zero if there is
 * no null data value.
 * Parameters:
 *  - void *: the block data
 * Returns: nothing
 */
void WKTRasterRasterBand::NullBlock(void *pData) 
{
    VALIDATE_POINTER0(pData, "NullBlock");

    int nNaturalBlockXSize = 0;
    int nNaturalBlockYSize = 0;
    GetBlockSize(&nNaturalBlockXSize, &nNaturalBlockYSize);

    int nWords = nNaturalBlockXSize * nNaturalBlockYSize;
    int nChunkSize = MAX(1, GDALGetDataTypeSize(eDataType) / 8);

    int bNoDataSet;
    double dfNoData = GetNoDataValue(&bNoDataSet);
    if (!bNoDataSet) 
    {
        memset(pData, 0, nWords * nChunkSize);
    } 
    else 
    {
        int i = 0;
        for (i = 0; i < nWords; i += nChunkSize)
            memcpy((GByte *) pData + i, &dfNoData, nChunkSize);
    }
}

/**
 * Set the no data value for this band.
 * Parameters:
 *  - double: The nodata value
 * Returns:
 *  - CE_None.
 */
CPLErr WKTRasterRasterBand::SetNoDataValue(double dfNewValue) {
    dfNoDataValue = dfNewValue;

    return CE_None;
}

/**
 * Fetch the no data value for this band.
 * Parameters:
 *  - int *: pointer to a boolean to use to indicate if a value is actually
 *          associated with this layer. May be NULL (default).
 *  Returns:
 *  - double: the nodata value for this band.
 */
double WKTRasterRasterBand::GetNoDataValue(int *pbSuccess) {
    if (pbSuccess != NULL)
        *pbSuccess = TRUE;

    return dfNoDataValue;
}

/**
 * Returns the number of overview layers available.
 * Parameters: none
 * Returns:
 *  int: the number of overviews layers available
 */
int WKTRasterRasterBand::GetOverviewCount() {
    WKTRasterDataset * poWKTRasterDS = (WKTRasterDataset *) poDS;

    return (poWKTRasterDS->nOverviews > 0) ?
            poWKTRasterDS->nOverviews :
            GDALRasterBand::GetOverviewCount();
}

/**
 * Fetch overview raster band object.
 * Parameters:
 *  - int: overview index between 0 and GetOverviewCount()-1
 * Returns:
 *  - GDALRasterBand *: overview GDALRasterBand.
 */
GDALRasterBand * WKTRasterRasterBand::GetOverview(int nOverview) {
    WKTRasterDataset * poWKTRasterDS = (WKTRasterDataset *) poDS;

    if (poWKTRasterDS->nOverviews > 0) {
        if (nOverview < 0 || nOverview >= poWKTRasterDS->nOverviews)
            return NULL;
        else
            return
            poWKTRasterDS->papoWKTRasterOv[nOverview]->GetRasterBand(nBand);
    } else
        return GDALRasterBand::GetOverview(nOverview);
}

/**
 * Get the natural block size for this band.
 * Parameters:
 *  - int *: pointer to int to store the natural X block size
 *  - int *: pointer to int to store the natural Y block size
 * Returns: nothing
 */
void WKTRasterRasterBand::GetBlockSize(int * pnXSize, int *pnYSize)
 {
    if (nBlockXSize == 0 || nBlockYSize == 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "This WKT Raster band has non regular blocking arrangement. \
                This feature is under development");

        if (pnXSize != NULL)
            *pnXSize = 0;
        if (pnYSize != NULL)
            *pnYSize = 0;

    }
    else {
        GDALRasterBand::GetBlockSize(pnXSize, pnYSize);
    }
}





