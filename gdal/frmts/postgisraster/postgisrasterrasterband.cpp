/******************************************************************************
 * File :    PostGISRasterRasterBand.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL Raster Band implementation for PostGIS Raster driver 
 * Author:   Jorge Arevalo, jorgearevalo@deimos-space.com
 * 
 * Last changes:
 * $Id: $
 *
 ******************************************************************************
 * Copyright (c) 2009 - 2011, Jorge Arevalo, jorge.arevalo@deimos-space.com
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
#include "postgisraster.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "gdal_priv.h"
#include "gdal.h"
#include <string>
#include "cpl_string.h"


/**
 * \brief Constructor.
 * Parameters:
 *  - PostGISRasterDataset *: The Dataset this band belongs to
 *  - int: the band number
 *  - GDALDataType: The data type for this band
 *  - double: The nodata value.  Could be any kind of data (GByte, GUInt16,
 *          GInt32...) but the variable has the bigger type.
 *  - GBool: if the data type is signed byte or not. If yes, the SIGNEDBYTE
 *          metadata value is added to IMAGE_STRUCTURE domain
 *  - int: The bit depth, to add NBITS as metadata value in IMAGE_STRUCTURE
 *          domain.
 */
PostGISRasterRasterBand::PostGISRasterRasterBand(PostGISRasterDataset *poDS,
        int nBand, GDALDataType hDataType, double dfNodata, GBool bSignedByte,
        int nBitDepth, int nFactor, GBool bIsOffline, char * pszSchema, 
        char * pszTable, char * pszColumn)
{

    /* Basic properties */
    this->poDS = poDS;
    this->nBand = nBand;
    this->bIsOffline = bIsOffline;
    this->pszSchema = (pszSchema) ? pszSchema : CPLStrdup(poDS->pszSchema);    
    this->pszTable = (pszTable) ? pszTable: CPLStrdup(poDS->pszTable);
    this->pszColumn = (pszColumn) ? pszColumn : CPLStrdup(poDS->pszColumn);
    this->pszWhere = CPLStrdup(poDS->pszWhere);

    eAccess = poDS->GetAccess();
    eDataType = hDataType;
    dfNoDataValue = dfNodata;


    /*****************************************************
     * Check block size issue
     *****************************************************/
    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;

    if (nBlockXSize == 0 || nBlockYSize == 0) {
        CPLError(CE_Warning, CPLE_NotSupported,
                "This band has irregular blocking, but is not supported yet");
    }

        
    // Add pixeltype to image structure domain
    if (bSignedByte == true) {
        SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
    }

    // Add NBITS to metadata only for sub-byte types
    if (nBitDepth < 8)
        SetMetadataItem("NBITS", CPLString().Printf( "%d", nBitDepth ),
            "IMAGE_STRUCTURE" );


    nOverviewFactor = nFactor;

    /**********************************************************
     * Check overviews. Query RASTER_OVERVIEWS table for
     * existing overviews, only in case we are on level 0
     * TODO: can we do this without querying RASTER_OVERVIEWS?
     * How do we know the number of overviews? Is an inphinite
     * loop...
     **********************************************************/
    if (nOverviewFactor == 0) {    

        CPLString osCommand;
        PGresult * poResult = NULL;
        int i = 0;
        int nFetchOvFactor = 0;
        char * pszOvSchema = NULL;
        char * pszOvTable = NULL;
        char * pszOvColumn = NULL;

        nRasterXSize = poDS->GetRasterXSize();
        nRasterYSize = poDS->GetRasterYSize();
 
        osCommand.Printf("select o_table_name, overview_factor, o_raster_column, "
                "o_table_schema from raster_overviews where r_table_schema = "
                "'%s' and r_table_name = '%s' and r_raster_column = '%s'",
                poDS->pszSchema, poDS->pszTable, poDS->pszColumn);

        poResult = PQexec(poDS->poConn, osCommand.c_str());
        if (poResult != NULL && PQresultStatus(poResult) == PGRES_TUPLES_OK &&
                PQntuples(poResult) > 0) {
            
            /* Create overviews */
            nOverviewCount = PQntuples(poResult);           
            papoOverviews = (PostGISRasterRasterBand **)VSICalloc(nOverviewCount,
                    sizeof(PostGISRasterRasterBand *));
            if (papoOverviews == NULL) {
                CPLError(CE_Warning, CPLE_OutOfMemory, "Couldn't create "
                        "overviews for band %d\n", nBand);              
                PQclear(poResult);
                return;
            }
                       
            for(i = 0; i < nOverviewCount; i++) {

                nFetchOvFactor = atoi(PQgetvalue(poResult, i, 1));
                pszOvSchema = CPLStrdup(PQgetvalue(poResult, i, 3));
                pszOvTable = CPLStrdup(PQgetvalue(poResult, i, 0));
                pszOvColumn = CPLStrdup(PQgetvalue(poResult, i, 2));
 
                /**
                 * NOTE: Overview bands are not considered to be a part of a
                 * dataset, but we use the same dataset for all the overview
                 * bands just for simplification (we'll need to access the table
                 * and schema names). But in method GetDataset, NULL is return
                 * if we're talking about an overview band
                 */
                papoOverviews[i] = new PostGISRasterRasterBand(poDS, nBand,
                        hDataType, dfNodata, bSignedByte, nBitDepth,
                        nFetchOvFactor, bIsOffline, pszOvSchema, pszOvTable, pszOvColumn);

            }

            PQclear(poResult);

        }

        else {

            nOverviewCount = 0;
            papoOverviews = NULL;
            if (poResult)
                PQclear(poResult);
        }
    }

    /************************************
     * We are in an overview level. Set
     * raster size to its value 
     ************************************/
    else {

        /* 
         * No overviews inside an overview (all the overviews are from original
         * band
         */
        nOverviewCount = 0;
        papoOverviews = NULL;

        nRasterXSize = (int) floor((double)poDS->GetRasterXSize() / nOverviewFactor);
        nRasterYSize = (int) floor((double)poDS->GetRasterYSize() / nOverviewFactor);        
    }

    CPLDebug("PostGIS_Raster", "PostGISRasterRasterBand constructor: Band "
            "created (srid = %d)", poDS->nSrid);
}

/***********************************************
 * \brief: Band destructor
 ***********************************************/
PostGISRasterRasterBand::~PostGISRasterRasterBand()
{
    int i;

    if (pszSchema)
        CPLFree(pszSchema);
    if (pszTable)
        CPLFree(pszTable);
    if (pszColumn)
        CPLFree(pszColumn);
    if (pszWhere)
        CPLFree(pszWhere);

    if (papoOverviews) {
        for(i = 0; i < nOverviewCount; i++)
            delete papoOverviews[i];

        CPLFree(papoOverviews);
    }
}


/**
 * \brief Set the block data to the null value if it is set, or zero if there is
 * no null data value.
 * Parameters:
 *  - void *: the block data
 * Returns: nothing
 */
void PostGISRasterRasterBand::NullBlock(void *pData) 
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
 * \brief Set the no data value for this band.
 * Parameters:
 *  - double: The nodata value
 * Returns:
 *  - CE_None.
 */
CPLErr PostGISRasterRasterBand::SetNoDataValue(double dfNewValue) {
    dfNoDataValue = dfNewValue;

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
    if (pbSuccess != NULL)
        *pbSuccess = TRUE;

    return dfNoDataValue;
}


/**
 * \brief Get the natural block size for this band.
 * Parameters:
 *  - int *: pointer to int to store the natural X block size
 *  - int *: pointer to int to store the natural Y block size
 * Returns: nothing
 */
void PostGISRasterRasterBand::GetBlockSize(int * pnXSize, int *pnYSize)
 {
    if (nBlockXSize == 0 || nBlockYSize == 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "This PostGIS Raster band has non regular blocking arrangement. \
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


/*****************************************************
 * \brief Fetch the band number
 *****************************************************/
int PostGISRasterRasterBand::GetBand()
{
    return (nOverviewFactor) ? 0 : nBand;
}

/*****************************************************
 * \brief Fetch the owning dataset handle
 *****************************************************/
GDALDataset* PostGISRasterRasterBand::GetDataset()
{
    return (nOverviewFactor) ? NULL : poDS;
}

/****************************************************
 * \brief Check for arbitrary overviews
 * The datastore can compute arbitrary overviews 
 * efficiently, because the overviews are tables, 
 * like the original raster. The effort is the same.
 ****************************************************/
int PostGISRasterRasterBand::HasArbitraryOverviews()
{
    return (nOverviewFactor) ? false : true;
}

/***************************************************
 * \brief Return the number of overview layers available
 ***************************************************/
int PostGISRasterRasterBand::GetOverviewCount()
{
    return (nOverviewFactor) ?
        0 :
        nOverviewCount;
}

/**********************************************************
 * \brief Fetch overview raster band object
 **********************************************************/
GDALRasterBand * PostGISRasterRasterBand::GetOverview(int i)
{
    return (i >= 0 && i < GetOverviewCount()) ? 
        (GDALRasterBand *)papoOverviews[i] : GDALRasterBand::GetOverview(i);
}

/*****************************************************
 * \brief Read a natural block of raster band data
 *****************************************************/
CPLErr PostGISRasterRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void*
        pImage)
{
    PostGISRasterDataset * poPostGISRasterDS = (PostGISRasterDataset*)poDS;
    CPLString osCommand;
    PGresult* poResult = NULL;
    double adfTransform[6];
    int nTuples = 0;
    int nNaturalBlockXSize = 0;
    int nNaturalBlockYSize = 0;
    int nPixelSize = 0;
    int nPixelXInitPosition;
    int nPixelYInitPosition;
    int nPixelXEndPosition;
    int nPixelYEndPosition;
    double dfProjXInit = 0.0;
    double dfProjYInit = 0.0;
    double dfProjXEnd = 0.0;
    double dfProjYEnd = 0.0;
    double dfProjLowerLeftX = 0.0;
    double dfProjLowerLeftY = 0.0;
    double dfProjUpperRightX = 0.0;
    double dfProjUpperRightY = 0.0;
    GByte* pbyData = NULL;
    char* pbyDataToRead = NULL;
    int nWKBLength = 0;
    int nExpectedDataSize = 0;

    /* Get pixel and block size */
    nPixelSize = MAX(1,GDALGetDataTypeSize(eDataType)/8);
    GetBlockSize(&nNaturalBlockXSize, &nNaturalBlockYSize);
    
    /* Get pixel,line coordinates of the block */
    /**
     * TODO: What if the georaster is rotated? Following the gdal_translate
     * source code, you can't use -projwin option with rotated rasters
     **/
    nPixelXInitPosition = nNaturalBlockXSize * nBlockXOff;
    nPixelYInitPosition = nNaturalBlockYSize * nBlockYOff;
    nPixelXEndPosition = nPixelXInitPosition + nNaturalBlockXSize;
    nPixelYEndPosition = nPixelYInitPosition + nNaturalBlockYSize;

    poPostGISRasterDS->GetGeoTransform(adfTransform);

    /* Pixel size correction, in case of overviews */
    if (nOverviewFactor) {
        adfTransform[1] *= nOverviewFactor;
        adfTransform[5] *= nOverviewFactor;
    }

    /* Calculate the "query box" */
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

    /**
     * Return all raster objects from our raster table that fall reside or
     * partly reside in a coordinate bounding box.
     * NOTE: Is it necessary to use a BB optimization like:
     * select st_makebox2d(...) && geom and (rest of the query)...?
     **/
    if (poPostGISRasterDS->pszWhere != NULL)
    {
        osCommand.Printf("select rid, %s from %s.%s where %s ~ "
                "st_setsrid(st_makebox2d(st_point(%f, %f), st_point(%f,"
                "%f)),%d) and %s", pszColumn, pszSchema, pszTable, pszColumn, 
                dfProjLowerLeftX, dfProjLowerLeftY, dfProjUpperRightX,
                dfProjUpperRightY, poPostGISRasterDS->nSrid, pszWhere);
    }

    else
    {
        osCommand.Printf("select rid, %s from %s.%s where %s ~ "
                "st_setsrid(st_makebox2d(st_point(%f, %f), st_point(%f,"
                "%f)),%d)", pszColumn, pszSchema, pszTable, pszColumn, 
                dfProjLowerLeftX, dfProjLowerLeftY, dfProjUpperRightX, 
                dfProjUpperRightY, poPostGISRasterDS->nSrid);
    }

    CPLDebug("PostGIS_Raster", "PostGISRasterRasterBand::IReadBlock: "
            "The query = %s", osCommand.c_str());

    poResult = PQexec(poPostGISRasterDS->poConn, osCommand.c_str());
    if (poResult == NULL || PQresultStatus(poResult) != PGRES_TUPLES_OK ||
            PQntuples(poResult) <= 0)
    {
        if (poResult)
            PQclear(poResult);

        /* TODO: Raise an error and exit? */
        CPLDebug("PostGIS_Raster", "PostGISRasterRasterBand::IReadBlock: "
                "The block (%d, %d) is empty", nBlockXOff, nBlockYOff);
        NullBlock(pImage);
        return CE_None;
    }

    nTuples = PQntuples(poResult);

    /* No overlapping */
    if (nTuples == 1)
    {        
        /**
         * Out db band.
         * TODO: Manage this situation
         **/
        if (bIsOffline)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "This raster has outdb storage."
                    "This feature isn\'t still available");
            return CE_Failure;
        }

        int nRid = atoi(PQgetvalue(poResult, 0, 0));
        
        /* Only data size, without payload */
        nExpectedDataSize = nNaturalBlockXSize * nNaturalBlockYSize *
            nPixelSize;
        pbyData = CPLHexToBinary(PQgetvalue(poResult, 0, 1), &nWKBLength);
        pbyDataToRead = (char*)GET_BAND_DATA(pbyData,nBand, nPixelSize,
                nExpectedDataSize);

        memcpy(pImage, pbyDataToRead, nExpectedDataSize * sizeof(char));
        
        CPLDebug("PostGIS_Raster", "IReadBlock: Copied %d bytes from block "
                "(%d, %d) (rid = %d) to %p", nExpectedDataSize, nBlockXOff, 
                nBlockYOff, nRid, pImage);

        CPLFree(pbyData);
        PQclear(poResult);

        return CE_None;
    }

    /** Overlapping raster data.
     * TODO: Manage this situation. Suggestion: open several datasets, because
     * you can't manage overlapping raster data with only one dataset
     **/
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Overlapping raster data. Feature under development, not "
                "available yet");
        if (poResult)
            PQclear(poResult);

        return CE_Failure;

    }
}


