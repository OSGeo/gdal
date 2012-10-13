/******************************************************************************
 * File :    postgisraster.h
 * Project:  PostGIS Raster driver
 * Purpose:  Main header file for PostGIS Raster Driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *
 * Last changes: $Id: $
 *
 ******************************************************************************
 * Copyright (c) 2010, Jorge Arevalo, jorge.arevalo@deimos-space.com
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
#include "gdal_priv.h"
#include "libpq-fe.h"
#include <float.h>
//#include "liblwgeom.h"

// General defines

#define DEFAULT_HOST            "localhost"
#define DEFAULT_PORT            "5432"
#define DEFAULT_SCHEMA          "public"
#define DEFAULT_COLUMN          "rast"
#define DEFAULT_USER            "postgres"
#define DEFAULT_PASSWORD        "postgres"

#define DEFAULT_BLOCK_X_SIZE    256
#define DEFAULT_BLOCK_Y_SIZE    256


#define POSTGIS_RASTER_VERSION         (GUInt16)0
#define RASTER_HEADER_SIZE              61
#define RASTER_BAND_HEADER_FIXED_SIZE   1

#define BAND_SIZE(nodatasize, datasize) \
        (RASTER_BAND_HEADER_FIXED_SIZE + nodatasize + datasize)

#define GET_BAND_DATA(raster, nband, nodatasize, datasize) \
    (raster + RASTER_HEADER_SIZE + nband * BAND_SIZE(nodatasize, datasize) - datasize)

#define FLT_NEQ(x, y) (fabs(x - y) > FLT_EPSILON)
#define FLT_EQ(x, y) (fabs(x - y) <= FLT_EPSILON)


/* Working modes */
#define NO_MODE -999
#define ONE_RASTER_PER_ROW 1
#define ONE_RASTER_PER_TABLE 2
#define BROWSE_SCHEMA 3
#define BROWSE_DATABASE 4

/* Easily working with georef arrays (taken from gdalbuiltvrt.cpp) */
#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

/* Taken from gdalbuiltvrt.cpp */
typedef enum
{
    LOWEST_RESOLUTION,
    HIGHEST_RESOLUTION,
    AVERAGE_RESOLUTION,
    USER_RESOLUTION
} ResolutionStrategy;

/**
 * OPTIMIZATION:
 * To construct the mosaic of tiles, we should check the pixel size of all tiles, 
 * in order to determine the dataset's pixel size. This can be really heavy. So, 
 * we define this number as the number of tiles that will be taken into account to
 * do it. If set to 0, all the tiles are taken
 **/ 
#define MAX_TILES   3 

class PostGISRasterRasterBand;

/*****************************************************************************
 * PostGISRasterDriver: extends GDALDriver to support PostGIS Raster connect.
 *****************************************************************************/
class PostGISRasterDriver : public GDALDriver {
    friend class PostGISRasterDataset;

private:
    PGconn** papoConnection;
    int nRefCount;
public:
    PostGISRasterDriver();
    virtual ~PostGISRasterDriver();
    PGconn* GetConnection(const char *, const char *, const char *,
            const char *, const char *);
};

/******************************************************************************
 * PostGISRasterDataset: extends GDALDataset to support PostGIS Raster datasets
 *****************************************************************************/
class PostGISRasterDataset : public GDALDataset {
    friend class PostGISRasterRasterBand;
private:
    char* pszOriginalConnectionString;
    char** papszSubdatasets;
    double adfGeoTransform[6];
    int nSrid;
    PGconn* poConn;
    GBool bRegularBlocking;
    GBool bAllTilesSnapToSameGrid;  // TODO: future use?
    GBool bRegisteredInRasterColumns;// TODO: future use?
    char* pszSchema;
    char* pszTable;
    char* pszColumn;
    char* pszWhere;
    char* pszProjection;
    ResolutionStrategy resolutionStrategy;
    int nMode;
    int nTiles;
    double xmin, ymin, xmax, ymax;
    GBool bBlocksCached;// TODO: future use?
    GBool SetRasterProperties(const char *);
    GBool BrowseDatabase(const char *, char *);
    GBool SetOverviewCount();
    GBool GetRasterMetadata(char *, double, double, double *, double *, int *, int *);

public:
    PostGISRasterDataset(ResolutionStrategy inResolutionStrategy);
    virtual ~PostGISRasterDataset();
    static GDALDataset* Open(GDALOpenInfo *);
    static GDALDataset* CreateCopy(const char *, GDALDataset *, 
        int, char **, GDALProgressFunc, void *);
    static GBool InsertRaster(PGconn *, PostGISRasterDataset *, 
        const char *, const char *, const char *);
    static CPLErr Delete(const char*);
    char ** GetMetadata(const char *);
    const char* GetProjectionRef();
    CPLErr SetProjection(const char*);
    CPLErr SetGeoTransform(double *);
    CPLErr GetGeoTransform(double *);
};

/******************************************************************************
 * PostGISRasterRasterBand: extends GDALRasterBand to support PostGIS Raster bands
 ******************************************************************************/
class PostGISRasterRasterBand : public GDALRasterBand {
    friend class PostGISRasterDataset;
private:
    GBool bHasNoDataValue;
    double dfNoDataValue;
    int nOverviewFactor;
    GBool bIsOffline;
    int nOverviewCount; 
    char* pszSchema;
    char* pszTable;
    char* pszColumn;
    PostGISRasterRasterBand ** papoOverviews;
    GDALDataType TranslateDataType(const char *);
    GDALColorInterp eBandInterp;

public:

    PostGISRasterRasterBand(PostGISRasterDataset *poDS, int nBand, GDALDataType hDataType,
            GBool bHasNoDataValue, double dfNodata, GBool bSignedByte, int nBitDepth, int nFactor,
            int nBlockXSize, int nBlockYSize, GBool bIsOffline = false, char * inPszSchema = NULL,
            char * inPszTable = NULL, char * inPszColumn = NULL);

    virtual ~PostGISRasterRasterBand();

    virtual double GetNoDataValue(int *pbSuccess = NULL);
    virtual CPLErr SetNoDataValue(double);
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int, GDALDataType, 
        int, int);
    virtual CPLErr IReadBlock(int, int, void *);
    int GetBand();
    GDALDataset* GetDataset();
    virtual int GetOverviewCount();
    virtual GDALRasterBand * GetOverview(int);
    virtual GDALColorInterp GetColorInterpretation();
};

