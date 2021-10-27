/***********************************************************************
 * File :    postgisraster.h
 * Project:  PostGIS Raster driver
 * Purpose:  Main header file for PostGIS Raster Driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Author:   David Zwarg, dzwarg@azavea.com
 *
 * Last changes: $Id$
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo, David Zwarg
 * Copyright (c) 2013, Even Rouault
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"),to deal in the Software without restriction, including
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
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **********************************************************************/

#ifndef POSTGISRASTER_H_INCLUDED
#define POSTGISRASTER_H_INCLUDED

#include "gdal_priv.h"
#include "libpq-fe.h"
#include "vrtdataset.h"
#include "cpl_mem_cache.h"
#include "cpl_quad_tree.h"
#include <float.h>
#include <map>

//#define DEBUG_VERBOSE
//#define DEBUG_QUERY

#if defined(DEBUG_VERBOSE) && !defined(DEBUG_QUERY)
#define DEBUG_QUERY
#endif

/**
 * The block size for the cache will be the minimum between the tile
 * size from sources and this value. So, please keep it at 2048 or
 * lower
 **/
#define MAX_BLOCK_SIZE 2048

#define NO_VALID_RES "-1234.56"

/**
 * To move over the data return by queries
 **/
#define POSTGIS_RASTER_VERSION          static_cast<GUInt16>(0)
#define RASTER_HEADER_SIZE              61
#define RASTER_BAND_HEADER_FIXED_SIZE   1

#define BAND_SIZE(nodatasize, datasize) \
        (RASTER_BAND_HEADER_FIXED_SIZE + (nodatasize) + (datasize))

#define GET_BAND_DATA(raster, nband, nodatasize, datasize) \
    ((raster) + RASTER_HEADER_SIZE + (nband) * BAND_SIZE(nodatasize, datasize) - (datasize))

#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

// Number of results return by ST_Metadata PostGIS function
#define ELEMENTS_OF_METADATA_RECORD 10

// Positions of elements of ST_Metadata PostGIS function
#define POS_UPPERLEFTX      0
#define POS_UPPERLEFTY      1
#define POS_WIDTH           2
#define POS_HEIGHT          3
#define POS_SCALEX          4
#define POS_SCALEY          5
#define POS_SKEWX           6
#define POS_SKEWY           7
#define POS_SRID            8
#define POS_NBANDS          9

// Number of results return by ST_BandMetadata PostGIS function
#define ELEMENTS_OF_BAND_METADATA_RECORD 4

// Positions of elements of ST_BandMetadata PostGIS function
#define POS_PIXELTYPE       0
#define POS_NODATAVALUE     1
#define POS_ISOUTDB         2
#define POS_PATH            3

/**
 * The driver can work in these modes:
 * - NO_MODE: Error case
 * - ONE_RASTER_PER_ROW: Each row of the requested table is considered
 *              as a separated raster object. This is the default mode, if
 *              database and table name are provided, and no mode is specified.
 * - ONE_RASTER_PER_TABLE: All the rows of the requested table are
 *              considered as tiles of a bigger raster coverage (the whole
 *              table). If database and table name are specified and mode = 2
 *              is present in the connection string, this is the selected mode.
 * - BROWSE_SCHEMA: If no table name is specified, just database and
 *              schema names, the driver will yell of the schema's raster tables
 *              as possible datasets.
 * - BROWSE_DATABASE: If no table name is specified, just database name,
 *              the driver will yell of the database's raster tables as possible
 *              datasets.
 **/
typedef enum
{
    NO_MODE,
    ONE_RASTER_PER_ROW,
    ONE_RASTER_PER_TABLE,
    BROWSE_SCHEMA,
    BROWSE_DATABASE
} WorkingMode;


enum class OutDBResolution
{
    SERVER_SIDE,
    CLIENT_SIDE,
    CLIENT_SIDE_IF_POSSIBLE
};

/**
 * Important metadata of a PostGIS Raster band
 **/
typedef struct {
    GDALDataType eDataType;
    int nBitsDepth;
    GBool bSignedByte;
    GBool bHasNoDataValue;
    GBool bIsOffline;
    char * path;
    double dfNoDataValue;
} BandMetadata;

typedef struct {
    char * pszSchema;
    char * pszTable;
    char * pszColumn;
    int nFactor;
} PROverview;

// Some tools definitions
char * ReplaceQuotes(const char *, int);
char * ReplaceSingleQuotes(const char *, int);
char ** ParseConnectionString(const char *);
GBool TranslateDataType(const char *, GDALDataType *, int *, GBool *);

class PostGISRasterRasterBand;
class PostGISRasterTileDataset;

/***********************************************************************
 * PostGISRasterDriver: extends GDALDriver to support PostGIS Raster
 * connect.
 **********************************************************************/
class PostGISRasterDriver final: public GDALDriver {

private:
    CPLMutex* hMutex;
    std::map<CPLString, PGconn*> oMapConnection{};

    CPL_DISALLOW_COPY_ASSIGN(PostGISRasterDriver)
public:
    PostGISRasterDriver();
    virtual ~PostGISRasterDriver();
  PGconn *GetConnection(const char *pszConnectionString,
                        const char *pszServiceIn, const char *pszDbnameIn,
                        const char *pszHostIn, const char *pszPortIn,
                        const char *pszUserIn);
};

/***********************************************************************
 * PostGISRasterDataset: extends VRTDataset to support PostGIS Raster
 * datasets
 **********************************************************************/
class PostGISRasterDataset final: public VRTDataset {
    friend class PostGISRasterRasterBand;
    friend class PostGISRasterTileRasterBand;
private:

    typedef enum
    {
        LOWEST_RESOLUTION,
        HIGHEST_RESOLUTION,
        AVERAGE_RESOLUTION,
        USER_RESOLUTION,
        AVERAGE_APPROX_RESOLUTION
    } ResolutionStrategy;

    char** papszSubdatasets;
    double adfGeoTransform[6];
    int nSrid;
    int nOverviewFactor;
    int nBandsToCreate;
    PGconn* poConn;
    GBool bRegularBlocking;
    GBool bAllTilesSnapToSameGrid;
    GBool bCheckAllTiles;
    char* pszSchema;
    char* pszTable;
    char* pszColumn;
    char* pszWhere;
    char * pszPrimaryKeyName;
    GBool  bIsFastPK;
    int bHasTriedFetchingPrimaryKeyName;
    char* pszProjection;
    ResolutionStrategy resolutionStrategy;
    WorkingMode nMode;
    OutDBResolution eOutDBResolution{OutDBResolution::SERVER_SIDE};
    bool bHasStBandFileSize = false;
    int m_nTiles;
    double xmin;
    double ymin;
    double xmax;
    double ymax;
    PostGISRasterTileDataset ** papoSourcesHolders;
    CPLQuadTree * hQuadTree;

    GBool bHasBuiltOverviews;
    int nOverviewCount;
    PostGISRasterDataset* poParentDS;
    PostGISRasterDataset** papoOverviewDS;

    std::map<CPLString, PostGISRasterTileDataset*> oMapPKIDToRTDS{};

    GBool bAssumeMultiBandReadPattern;
    int nNextExpectedBand;
    int nXOffPrev;
    int nYOffPrev;
    int nXSizePrev;
    int nYSizePrev;

    GBool bHasTriedHasSpatialIndex;
    GBool bHasSpatialIndex;

    GBool bBuildQuadTreeDynamically;

    GBool bTilesSameDimension;
    int nTileWidth;
    int nTileHeight;

    int m_nLastLoadSourcesXOff = 0;
    int m_nLastLoadSourcesYOff = 0;
    int m_nLastLoadSourcesXSize = 0;
    int m_nLastLoadSourcesYSize = 0;
    int m_nLastLoadSourcesBand = 0;

    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> oOutDBDatasetCache{8,0};
    lru11::Cache<std::string, bool> oOutDBFilenameUsable{100,0};

    GBool ConstructOneDatasetFromTiles(PGresult *);
    GBool YieldSubdatasets(PGresult *, const char *);
    GBool SetRasterProperties(const char *);
    GBool BrowseDatabase(const char *, const char *);
    GBool AddComplexSource(PostGISRasterTileDataset* poRTDS);
    GBool GetDstWin(PostGISRasterTileDataset *, int *, int *, int *,
                    int *);
    BandMetadata * GetBandsMetadata(int *);
    PROverview * GetOverviewTables(int *);

    PostGISRasterTileDataset* BuildRasterTileDataset(const char* pszMetadata,
                                                     const char* pszPKID,
                                                     int nBandsFetched,
                                                     BandMetadata * poBandMetaData);
    void UpdateGlobalResolutionWithTileResolution(double tilePixelSizeX,
                                                  double tilePixelSizeY);
    void BuildOverviews();
    void BuildBands(BandMetadata * poBandMetaData, int nBandsFetched);

    PostGISRasterTileDataset * GetMatchingSourceRef(const char * pszPKID) { return oMapPKIDToRTDS[pszPKID]; }
    PostGISRasterTileDataset * GetMatchingSourceRef(double dfUpperLeftX, double dfUpperLeftY);

    bool CanUseClientSideOutDB(bool bAllBandCaching,
                               int nBand,
                               const CPLString& osWHERE);

    bool LoadOutdbRaster(int& nCurOffset,
                          GDALDataType eDT,
                          int nBand,
                          const GByte* pbyData,
                          int nWKBLength,
                          void* pImage,
                          double dfTileUpperLeftX,
                          double dfTileUpperLeftY,
                          double dfTileResX,
                          double dfTileResY,
                          int nTileXSize,
                          int nTileYSize);

    CPL_DISALLOW_COPY_ASSIGN(PostGISRasterDataset)

  protected:
    virtual int         CloseDependentDatasets() override;
    virtual void        FlushCache(bool bAtClosing) override;

public:
    PostGISRasterDataset();
    virtual ~PostGISRasterDataset();
    static GDALDataset* Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    static GDALDataset* CreateCopy(const char *, GDALDataset *,
        int, char **, GDALProgressFunc, void *);
    static GBool InsertRaster(PGconn *, PostGISRasterDataset *,
        const char *, const char *, const char *);
    static CPLErr Delete(const char*);
    virtual char      **GetMetadataDomainList() override;
    char ** GetMetadata(const char *) override;
    const char* _GetProjectionRef() override;
    CPLErr _SetProjection(const char*) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    CPLErr SetGeoTransform(double *) override;
    CPLErr GetGeoTransform(double *) override;
    char **GetFileList() override;

    int    GetOverviewCount();
    PostGISRasterDataset* GetOverviewDS(int iOvr);

    const char * GetPrimaryKeyRef();
    GBool HasSpatialIndex();
    GBool LoadSources(int nXOff, int nYOff, int nXSize, int nYSize, int nBand);
    GBool PolygonFromCoords(int nXOff, int nYOff, int nXEndOff,
        int nYEndOff,double adfProjWin[8]);
    void CacheTile(const char* pszMetadata, const char* pszRaster, const char *pszPKID, int nBand, bool bAllBandCaching);
};

/***********************************************************************
 * PostGISRasterRasterBand: extends VRTSourcedRasterBand to support
 * PostGIS Raster bands
 **********************************************************************/
class PostGISRasterTileRasterBand;

class PostGISRasterRasterBand final: public VRTSourcedRasterBand {
    friend class PostGISRasterDataset;

    CPL_DISALLOW_COPY_ASSIGN(PostGISRasterRasterBand)
protected:
    const char* pszSchema;
    const char* pszTable;
    const char* pszColumn;

    void                      NullBuffer(void* pData,
                                         int nBufXSize,
                                         int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace,
                                         int nLineSpace);
public:

    PostGISRasterRasterBand( PostGISRasterDataset * poDSIn, int nBandIn,
                             GDALDataType eDataTypeIn, GBool bNoDataValueSetIn,
                             double dfNodata );

    virtual ~PostGISRasterRasterBand();

    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
    virtual CPLErr SetNoDataValue(double) override;
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *,
                             int, int, GDALDataType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg* psExtraArg) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand * GetOverview(int) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual double GetMinimum( int *pbSuccess ) override;
    virtual double GetMaximum( int *pbSuccess ) override;
    virtual CPLErr ComputeRasterMinMax( int bApproxOK, double* adfMinMax ) override;
};

/***********************************************************************
 * PostGISRasterTileDataset: it holds just a raster tile
 **********************************************************************/
class PostGISRasterTileRasterBand;

class PostGISRasterTileDataset final: public GDALDataset {
    friend class PostGISRasterDataset;
    friend class PostGISRasterRasterBand;
    friend class PostGISRasterTileRasterBand;
private:
    PostGISRasterDataset* poRDS;
    char * pszPKID;
    double adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(PostGISRasterTileDataset)

public:
    PostGISRasterTileDataset(PostGISRasterDataset* poRDS,
                             int nXSize,
                             int nYSize);
    ~PostGISRasterTileDataset();
    CPLErr GetGeoTransform(double *) override;
    void   GetExtent(double* pdfMinX, double* pdfMinY, double* pdfMaxX, double* pdfMaxY) const;
    const char* GetPKID() const { return pszPKID; }
};

/***********************************************************************
 * PostGISRasterTileRasterBand: it holds a raster tile band, that will
 * be used as a source for PostGISRasterRasterBand
 **********************************************************************/
class PostGISRasterTileRasterBand final: public GDALRasterBand {
    friend class PostGISRasterRasterBand;
    friend class PostGISRasterDataset;
private:
    GBool IsCached();

    VRTSource* poSource;

    CPL_DISALLOW_COPY_ASSIGN(PostGISRasterTileRasterBand)

public:
    PostGISRasterTileRasterBand(
        PostGISRasterTileDataset * poRTDS, int nBand,
        GDALDataType eDataType);
    virtual ~PostGISRasterTileRasterBand();
    virtual CPLErr IReadBlock(int, int, void *) override;
};

#endif // POSTGISRASTER_H_INCLUDED
