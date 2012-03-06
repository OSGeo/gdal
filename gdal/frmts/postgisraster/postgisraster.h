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

#define DEFAULT_HOST			"localhost"
#define DEFAULT_PORT			"5432"
#define DEFAULT_SCHEMA          "public"
#define DEFAULT_COLUMN			"rast"
#define DEFAULT_USER			"postgres"
#define DEFAULT_PASSWORD		"postgres"

#define POSTGIS_RASTER_VERSION         (GUInt16)0
#define RASTER_HEADER_SIZE              61
#define RASTER_BAND_HEADER_FIXED_SIZE   1

#define BAND_SIZE(nodatasize, datasize) \
        (RASTER_BAND_HEADER_FIXED_SIZE + nodatasize + datasize)

#define GET_BAND_DATA(raster, nband, nodatasize, datasize) \
    (raster + RASTER_HEADER_SIZE + nband * BAND_SIZE(nodatasize, datasize) - datasize)

#define FLT_NEQ(x, y) (fabs(x - y) > FLT_EPSILON)


/* Working modes */
#define NO_MODE -999
#define ONE_RASTER_PER_ROW 1
#define ONE_RASTER_PER_TABLE 2
#define BROWSE_SCHEMA 3
#define BROWSE_DATABASE 4

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
    char** papszSubdatasets;
    double adfGeoTransform[6];
    int nSrid;
    PGconn* poConn;
    GBool bRegularBlocking;
    GBool bAllTilesSnapToSameGrid;
    GBool bRegisteredInRasterColumns;
    char* pszSchema;
    char* pszTable;
    char* pszColumn;
    char* pszWhere;
    char* pszProjection;
    int nMode;
    int nBlockXSize;
    int nBlockYSize;
    GBool bBlocksCached;
    GBool SetRasterProperties(const char *);
    GBool BrowseDatabase(const char *, char *);
    GBool SetRasterBands();
    GBool SetOverviewCount();

public:
    PostGISRasterDataset();
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

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
        void *, int, int, GDALDataType, int, int *, int, int, int );
};

/******************************************************************************
 * PostGISRasterRasterBand: extends GDALRasterBand to support PostGIS Raster bands
 ******************************************************************************/
class PostGISRasterRasterBand : public GDALRasterBand {
    friend class PostGISRasterDataset;
private:
    double dfNoDataValue;
    int nOverviewFactor;
    GBool bIsOffline;
    int nOverviewCount;
    char* pszSchema;
    char* pszTable;
    char* pszColumn;
    char* pszWhere;
    PostGISRasterRasterBand ** papoOverviews;
    void NullBlock(void *);

public:

    PostGISRasterRasterBand(PostGISRasterDataset *poDS, int nBand, GDALDataType hDataType,
            double dfNodata, GBool bSignedByte, int nBitDepth, int nFactor,
            GBool bIsOffline, char * pszSchema = NULL, char * pszTable = NULL,
            char * pszColumn = NULL);

    virtual ~PostGISRasterRasterBand();

    virtual double GetNoDataValue(int *pbSuccess = NULL);
    virtual CPLErr SetNoDataValue(double);
    virtual void GetBlockSize(int *, int *);
    virtual CPLErr IReadBlock(int, int, void *);
    int GetBand();
    GDALDataset* GetDataset();
    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand * GetOverview(int);
};

