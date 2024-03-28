/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 ****************************************************************************/

#ifndef NETCDFDATASET_H_INCLUDED_
#define NETCDFDATASET_H_INCLUDED_

#include <array>
#include <ctime>
#include <cfloat>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "netcdf.h"
#include "netcdfformatenum.h"
#include "netcdfsg.h"
#include "netcdfsgwriterutil.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "netcdfuffd.h"
#include "netcdf_cf_constants.h"

#if CPL_IS_LSB
#define PLATFORM_HEADER 1
#else
#define PLATFORM_HEADER 0
#endif

/************************************************************************/
/* ==================================================================== */
/*                           defines                                    */
/* ==================================================================== */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Creation and Configuration Options                              */
/* -------------------------------------------------------------------- */

/* Creation options

   FORMAT=NC/NC2/NC4/NC4C (COMPRESS=DEFLATE sets FORMAT=NC4C)
   COMPRESS=NONE/DEFLATE (default: NONE)
   ZLEVEL=[1-9] (default: 1)
   WRITE_BOTTOMUP=YES/NO (default: YES)
   WRITE_GDAL_TAGS=YES/NO (default: YES)
   WRITE_LONLAT=YES/NO/IF_NEEDED (default: YES for geographic, NO for projected)
   TYPE_LONLAT=float/double (default: double for geographic, float for
   projected) PIXELTYPE=DEFAULT/SIGNEDBYTE (use SIGNEDBYTE to get a signed Byte
   Band)
*/

/* Config Options

   GDAL_NETCDF_BOTTOMUP=YES/NO overrides bottom-up value on import
   GDAL_NETCDF_VERIFY_DIMS=[YES/STRICT] : Try to guess which dimensions
   represent the latitude and longitude only by their attributes (STRICT) or
   also by guessing the name (YES), default is YES.
   GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS=[YES/NO] Whether X/Y dimensions should
   be always considered as geospatial axis, even if the lack conventional
   attributes confirming it. Default is NO. GDAL_NETCDF_ASSUME_LONGLAT=[YES/NO]
   Whether when all else has failed for determining a CRS, a meaningful
   geotransform has been found, and is within the bounds -180,360 -90,90, if YES
   assume OGC:CRS84. Default is NO.

   // TODO: this unusued and a few others occur in the source that are not
   documented, flush out unused opts and document the rest mdsumner@gmail.com
   GDAL_NETCDF_CONVERT_LAT_180=YES/NO convert longitude values from ]180,360] to
   [-180,180]
*/

/* -------------------------------------------------------------------- */
/*      Driver-specific defines                                         */
/* -------------------------------------------------------------------- */

/* NETCDF driver defs */
static const size_t NCDF_MAX_STR_LEN = 8192;
#define NCDF_CONVENTIONS "Conventions"
#define NCDF_CONVENTIONS_CF_V1_5 "CF-1.5"
#define GDAL_DEFAULT_NCDF_CONVENTIONS NCDF_CONVENTIONS_CF_V1_5
#define NCDF_CONVENTIONS_CF_V1_6 "CF-1.6"
#define NCDF_CONVENTIONS_CF_V1_8 "CF-1.8"
#define NCDF_GEOTRANSFORM "GeoTransform"
#define NCDF_DIMNAME_X "x"
#define NCDF_DIMNAME_Y "y"
#define NCDF_DIMNAME_LON "lon"
#define NCDF_DIMNAME_LAT "lat"
#define NCDF_LONLAT "lon lat"
#define NCDF_DIMNAME_RLON "rlon"  // rotated longitude
#define NCDF_DIMNAME_RLAT "rlat"  // rotated latitude

/* compression parameters */
typedef enum
{
    NCDF_COMPRESS_NONE = 0,
    /* TODO */
    /* http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html#Packed%20Data%20Values
     */
    NCDF_COMPRESS_PACKED = 1,
    NCDF_COMPRESS_DEFLATE = 2,
    NCDF_COMPRESS_SZIP = 3 /* no support for writing */
} NetCDFCompressEnum;

static const int NCDF_DEFLATE_LEVEL = 1; /* best time/size ratio */

/* helper for libnetcdf errors */
#define NCDF_ERR(status)                                                       \
    do                                                                         \
    {                                                                          \
        int NCDF_ERR_status_ = (status);                                       \
        if (NCDF_ERR_status_ != NC_NOERR)                                      \
        {                                                                      \
            CPLError(CE_Failure, CPLE_AppDefined,                              \
                     "netcdf error #%d : %s .\nat (%s,%s,%d)\n", status,       \
                     nc_strerror(NCDF_ERR_status_), __FILE__, __FUNCTION__,    \
                     __LINE__);                                                \
        }                                                                      \
    } while (0)

#define NCDF_ERR_RET(status)                                                   \
    do                                                                         \
    {                                                                          \
        int NCDF_ERR_RET_status_ = (status);                                   \
        if (NCDF_ERR_RET_status_ != NC_NOERR)                                  \
        {                                                                      \
            NCDF_ERR(NCDF_ERR_RET_status_);                                    \
            return CE_Failure;                                                 \
        }                                                                      \
    } while (0)

#define ERR_RET(eErr)                                                          \
    do                                                                         \
    {                                                                          \
        CPLErr ERR_RET_eErr_ = (eErr);                                         \
        if (ERR_RET_eErr_ != CE_None)                                          \
            return ERR_RET_eErr_;                                              \
    } while (0)

/* Check for NC2 support in case it was not enabled at compile time. */
/* NC4 has to be detected at compile as it requires a special build of netcdf-4.
 */
#ifndef NETCDF_HAS_NC2
#ifdef NC_64BIT_OFFSET
#define NETCDF_HAS_NC2 1
#endif
#endif

/* Some additional metadata */
#define OGR_SG_ORIGINAL_LAYERNAME "ogr_layer_name"

/* -------------------------------------------------------------------- */
/*         CF-1 Coordinate Type Naming (Chapter 4.  Coordinate Types )  */
/* -------------------------------------------------------------------- */
static const char *const papszCFLongitudeVarNames[] = {CF_LONGITUDE_VAR_NAME,
                                                       "longitude", nullptr};
static const char *const papszCFLongitudeAttribNames[] = {
    CF_UNITS, CF_UNITS, CF_UNITS, CF_STD_NAME, CF_AXIS, CF_LNG_NAME, nullptr};
static const char *const papszCFLongitudeAttribValues[] = {
    CF_DEGREES_EAST,
    CF_DEGREE_EAST,
    CF_DEGREES_E,
    CF_LONGITUDE_STD_NAME,
    "X",
    CF_LONGITUDE_LNG_NAME,
    nullptr};
static const char *const papszCFLatitudeVarNames[] = {CF_LATITUDE_VAR_NAME,
                                                      "latitude", nullptr};
static const char *const papszCFLatitudeAttribNames[] = {
    CF_UNITS, CF_UNITS, CF_UNITS, CF_STD_NAME, CF_AXIS, CF_LNG_NAME, nullptr};
static const char *const papszCFLatitudeAttribValues[] = {CF_DEGREES_NORTH,
                                                          CF_DEGREE_NORTH,
                                                          CF_DEGREES_N,
                                                          CF_LATITUDE_STD_NAME,
                                                          "Y",
                                                          CF_LATITUDE_LNG_NAME,
                                                          nullptr};

static const char *const papszCFProjectionXVarNames[] = {CF_PROJ_X_VAR_NAME,
                                                         "xc", nullptr};
static const char *const papszCFProjectionXAttribNames[] = {CF_STD_NAME,
                                                            CF_AXIS, nullptr};
static const char *const papszCFProjectionXAttribValues[] = {CF_PROJ_X_COORD,
                                                             "X", nullptr};
static const char *const papszCFProjectionYVarNames[] = {CF_PROJ_Y_VAR_NAME,
                                                         "yc", nullptr};
static const char *const papszCFProjectionYAttribNames[] = {CF_STD_NAME,
                                                            CF_AXIS, nullptr};
static const char *const papszCFProjectionYAttribValues[] = {CF_PROJ_Y_COORD,
                                                             "Y", nullptr};

static const char *const papszCFVerticalAttribNames[] = {CF_AXIS, "positive",
                                                         "positive", nullptr};
static const char *const papszCFVerticalAttribValues[] = {"Z", "up", "down",
                                                          nullptr};
static const char *const papszCFVerticalUnitsValues[] = {
    /* units of pressure */
    "bar", "bars", "millibar", "millibars", "decibar", "decibars", "atmosphere",
    "atmospheres", "atm", "pascal", "pascals", "Pa", "hPa",
    /* units of length */
    "meter", "meters", "m", "kilometer", "kilometers", "km",
    /* dimensionless vertical coordinates */
    "level", "layer", "sigma_level", nullptr};
/* dimensionless vertical coordinates */
static const char *const papszCFVerticalStandardNameValues[] = {
    "atmosphere_ln_pressure_coordinate",
    "atmosphere_sigma_coordinate",
    "atmosphere_hybrid_sigma_pressure_coordinate",
    "atmosphere_hybrid_height_coordinate",
    "atmosphere_sleve_coordinate",
    "ocean_sigma_coordinate",
    "ocean_s_coordinate",
    "ocean_sigma_z_coordinate",
    "ocean_double_sigma_coordinate",
    "atmosphere_ln_pressure_coordinate",
    "atmosphere_sigma_coordinate",
    "atmosphere_hybrid_sigma_pressure_coordinate",
    "atmosphere_hybrid_height_coordinate",
    "atmosphere_sleve_coordinate",
    "ocean_sigma_coordinate",
    "ocean_s_coordinate",
    "ocean_sigma_z_coordinate",
    "ocean_double_sigma_coordinate",
    nullptr};

static const char *const papszCFTimeAttribNames[] = {CF_AXIS, CF_STD_NAME,
                                                     nullptr};
static const char *const papszCFTimeAttribValues[] = {"T", "time", nullptr};
static const char *const papszCFTimeUnitsValues[] = {
    "days since",   "day since", "d since",       "hours since",
    "hour since",   "h since",   "hr since",      "minutes since",
    "minute since", "min since", "seconds since", "second since",
    "sec since",    "s since",   nullptr};

/************************************************************************/
/* ==================================================================== */
/*                        netCDFWriterConfig classes                    */
/* ==================================================================== */
/************************************************************************/

class netCDFWriterConfigAttribute
{
  public:
    CPLString m_osName;
    CPLString m_osType;
    CPLString m_osValue;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfigField
{
  public:
    CPLString m_osName;
    CPLString m_osNetCDFName;
    CPLString m_osMainDim;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfigLayer
{
  public:
    CPLString m_osName;
    CPLString m_osNetCDFName;
    std::map<CPLString, CPLString> m_oLayerCreationOptions;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;
    std::map<CPLString, netCDFWriterConfigField> m_oFields;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfiguration
{
  public:
    bool m_bIsValid;
    std::map<CPLString, CPLString> m_oDatasetCreationOptions;
    std::map<CPLString, CPLString> m_oLayerCreationOptions;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;
    std::map<CPLString, netCDFWriterConfigField> m_oFields;
    std::map<CPLString, netCDFWriterConfigLayer> m_oLayers;

    netCDFWriterConfiguration() : m_bIsValid(false)
    {
    }

    bool Parse(const char *pszFilename);
    static bool SetNameValue(CPLXMLNode *psNode,
                             std::map<CPLString, CPLString> &oMap);
};

/************************************************************************/
/* ==================================================================== */
/*                           netCDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand;
class netCDFLayer;

class netCDFDataset final : public GDALPamDataset
{
    friend class netCDFRasterBand;  // TMP
    friend class netCDFLayer;
    friend class netCDFVariable;

    typedef enum
    {
        SINGLE_LAYER,
        SEPARATE_FILES,
        SEPARATE_GROUPS
    } MultipleLayerBehavior;

    /* basic dataset vars */
    CPLString osFilename;
#ifdef ENABLE_NCDUMP
    bool bFileToDestroyAtClosing;
#endif
    int cdfid;
#ifdef ENABLE_UFFD
    cpl_uffd_context *pCtx = nullptr;
#endif
    VSILFILE *fpVSIMEM = nullptr;
    int nSubDatasets;
    char **papszSubDatasets;
    char **papszMetadata;

    // Used to report metadata found in Sentinel 5
    std::map<std::string, CPLStringList> m_oMapDomainToJSon{};

    CPLStringList papszDimName;
    bool bBottomUp;
    NetCDFFormatEnum eFormat;
    bool bIsGdalFile;   /* was this file created by GDAL? */
    bool bIsGdalCfFile; /* was this file created by the (new) CF-compliant
                           driver? */
    char *pszCFProjection;
    const char *pszCFCoordinates;
    double nCFVersion;
    bool bSGSupport;
    MultipleLayerBehavior eMultipleLayerBehavior;
    std::vector<netCDFDataset *> apoVectorDatasets;
    std::string logHeader;
    int logCount;
    nccfdriver::netCDFVID vcdf;
    nccfdriver::OGR_NCScribe GeometryScribe;
    nccfdriver::OGR_NCScribe FieldScribe;
    nccfdriver::WBufferManager bufManager;

    bool bWriteGDALVersion = true;
    bool bWriteGDALHistory = true;

    /* projection/GT */
    double m_adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};
    int nXDimID;
    int nYDimID;
    bool bIsProjected;
    bool bIsGeographic;
    bool bSwitchedXY = false;

    /* state vars */
    bool bDefineMode;
    bool m_bHasProjection = false;
    bool m_bHasGeoTransform = false;
    bool m_bAddedProjectionVarsDefs = false;
    bool m_bAddedProjectionVarsData = false;
    bool bAddedGridMappingRef;

    /* create vars */
    char **papszCreationOptions;
    NetCDFCompressEnum eCompress;
    int nZLevel;
    bool bChunking;
    int nCreateMode;
    bool bSignedData;

    // IDs of the dimensions of the variables
    std::vector<int> m_anDimIds{};

    // Extra dimension info (size of those arrays is m_anDimIds.size() - 2)
    std::vector<int> m_anExtraDimVarIds{};
    std::vector<int> m_anExtraDimGroupIds{};

    std::vector<std::shared_ptr<OGRLayer>> papoLayers;

    netCDFWriterConfiguration oWriterConfig;

    struct ChunkKey
    {
        size_t xChunk;  // netCDF chunk number along X axis
        size_t yChunk;  // netCDF chunk number along Y axis
        int nBand;

        ChunkKey(size_t xChunkIn, size_t yChunkIn, int nBandIn)
            : xChunk(xChunkIn), yChunk(yChunkIn), nBand(nBandIn)
        {
        }

        bool operator==(const ChunkKey &other) const
        {
            return xChunk == other.xChunk && yChunk == other.yChunk &&
                   nBand == other.nBand;
        }

        bool operator!=(const ChunkKey &other) const
        {
            return !(operator==(other));
        }
    };

    struct KeyHasher
    {
        std::size_t operator()(const ChunkKey &k) const
        {
            return std::hash<size_t>{}(k.xChunk) ^
                   (std::hash<size_t>{}(k.yChunk) << 1) ^
                   (std::hash<size_t>{}(k.nBand) << 2);
        }
    };

    typedef lru11::Cache<
        ChunkKey, std::shared_ptr<std::vector<GByte>>, lru11::NullLock,
        std::unordered_map<
            ChunkKey,
            typename std::list<lru11::KeyValuePair<
                ChunkKey, std::shared_ptr<std::vector<GByte>>>>::iterator,
            KeyHasher>>
        ChunkCacheType;

    std::unique_ptr<ChunkCacheType> poChunkCache;

    static double rint(double);

    double FetchCopyParam(const char *pszGridMappingValue, const char *pszParam,
                          double dfDefault, bool *pbFound = nullptr);

    std::vector<std::string>
    FetchStandardParallels(const char *pszGridMappingValue);

    const char *FetchAttr(const char *pszVarFullName, const char *pszAttr);
    const char *FetchAttr(int nGroupId, int nVarId, const char *pszAttr);

    void ProcessCreationOptions();
    int DefVarDeflate(int nVarId, bool bChunkingArg = true);
    CPLErr AddProjectionVars(bool bDefsOnly, GDALProgressFunc pfnProgress,
                             void *pProgressData);
    bool AddGridMappingRef();

    bool GetDefineMode() const
    {
        return bDefineMode;
    }

    bool SetDefineMode(bool bNewDefineMode);

    CPLErr ReadAttributes(int, int);

    void CreateSubDatasetList(int nGroupId);

    void SetProjectionFromVar(int nGroupId, int nVarId, bool bReadSRSOnly,
                              const char *pszGivenGM, std::string *,
                              nccfdriver::SGeometry_Reader *,
                              std::vector<std::string> *paosRemovedMDItems);
    void SetProjectionFromVar(int nGroupId, int nVarId, bool bReadSRSOnly);

    bool ProcessNASAL2OceanGeoLocation(int nGroupId, int nVarId);

    bool ProcessNASAEMITGeoLocation(int nGroupId, int nVarId);

    int ProcessCFGeolocation(int nGroupId, int nVarId,
                             const std::string &osGeolocWKT,
                             std::string &osGeolocXNameOut,
                             std::string &osGeolocYNameOut);
    CPLErr Set1DGeolocation(int nGroupId, int nVarId, const char *szDimName);
    double *Get1DGeolocation(const char *szDimName, int &nVarLen);

    static bool CloneAttributes(int old_cdfid, int new_cdfid, int nSrcVarId,
                                int nDstVarId);
    static bool CloneVariableContent(int old_cdfid, int new_cdfid,
                                     int nSrcVarId, int nDstVarId);
    static bool CloneGrp(int nOldGrpId, int nNewGrpId, bool bIsNC4,
                         int nLayerId, int nDimIdToGrow, size_t nNewSize);
    bool GrowDim(int nLayerId, int nDimIdToGrow, size_t nNewSize);

    void ProcessSentinel3_SRAL_MWR();

    CPLErr
    FilterVars(int nCdfId, bool bKeepRasters, bool bKeepVectors,
               char **papszIgnoreVars, int *pnRasterVars, int *pnGroupId,
               int *pnVarId, int *pnIgnoredVars,
               // key is (dim1Id, dim2Id, nc_type varType)
               // value is (groupId, varId)
               std::map<std::array<int, 3>, std::vector<std::pair<int, int>>>
                   &oMap2DDimsToGroupAndVar);
    CPLErr CreateGrpVectorLayers(int nCdfId, const CPLString &osFeatureType,
                                 const std::vector<int> &anPotentialVectorVarID,
                                 const std::map<int, int> &oMapDimIdToCount,
                                 int nVarXId, int nVarYId, int nVarZId,
                                 int nProfileDimId, int nParentIndexVarID,
                                 bool bKeepRasters);

    bool DetectAndFillSGLayers(int ncid);
    CPLErr LoadSGVarIntoLayer(int ncid, int nc_basevarId);

    static GDALDataset *OpenMultiDim(GDALOpenInfo *);
    std::shared_ptr<GDALGroup> m_poRootGroup{};

    void SetGeoTransformNoUpdate(double *);
    void SetSpatialRefNoUpdate(const OGRSpatialReference *);

  protected:
    CPLXMLNode *SerializeToXML(const char *pszVRTPath) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    CPLErr Close() override;

  public:
    netCDFDataset();
    virtual ~netCDFDataset();
    bool SGCommitPendingTransaction();
    void SGLogPendingTransaction();
    static std::string generateLogName();

    /* Projection/GT */
    CPLErr GetGeoTransform(double *) override;
    CPLErr SetGeoTransform(double *) override;
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual char **GetMetadataDomainList() override;
    char **GetMetadata(const char *) override;

    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMD,
                               const char *pszDomain = "") override;

    virtual int TestCapability(const char *pszCap) override;

    virtual int GetLayerCount() override
    {
        return static_cast<int>(this->papoLayers.size());
    }

    virtual OGRLayer *GetLayer(int nIdx) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override;

    int GetCDFID() const
    {
        return cdfid;
    }

    inline bool HasInfiniteRecordDim()
    {
        return !bSGSupport;
    }

    /* static functions */
    static GDALDataset *Open(GDALOpenInfo *);

    static netCDFDataset *CreateLL(const char *pszFilename, int nXSize,
                                   int nYSize, int nBands, char **papszOptions);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papzOptions);
};

class netCDFLayer final : public OGRLayer
{
    typedef union
    {
        signed char chVal;
        unsigned char uchVal;
        short sVal;
        unsigned short usVal;
        int nVal;
        unsigned int unVal;
        GIntBig nVal64;
        GUIntBig unVal64;
        float fVal;
        double dfVal;
    } NCDFNoDataUnion;

    typedef struct
    {
        NCDFNoDataUnion uNoData;
        nc_type nType;
        int nVarId;
        int nDimCount;
        bool bHasWarnedAboutTruncation;
        int nMainDimId;
        int nSecDimId;
        bool bIsDays;
    } FieldDesc;

    netCDFDataset *m_poDS;
    int m_nLayerCDFId;
    OGRFeatureDefn *m_poFeatureDefn;
    CPLString m_osRecordDimName;
    int m_nRecordDimID;
    int m_nDefaultWidth;
    bool m_bAutoGrowStrings;
    int m_nDefaultMaxWidthDimId;
    int m_nXVarID;
    int m_nYVarID;
    int m_nZVarID;
    nc_type m_nXVarNCDFType;
    nc_type m_nYVarNCDFType;
    nc_type m_nZVarNCDFType;
    NCDFNoDataUnion m_uXVarNoData;
    NCDFNoDataUnion m_uYVarNoData;
    NCDFNoDataUnion m_uZVarNoData;
    CPLString m_osWKTVarName;
    int m_nWKTMaxWidth;
    int m_nWKTMaxWidthDimId;
    int m_nWKTVarID;
    nc_type m_nWKTNCDFType;
    CPLString m_osCoordinatesValue;
    std::vector<FieldDesc> m_aoFieldDesc;
    bool m_bLegacyCreateMode;
    int m_nCurFeatureId;
    CPLString m_osGridMapping;
    bool m_bWriteGDALTags;
    bool m_bUseStringInNC4;
    bool m_bNCDumpCompat;

    CPLString m_osProfileDimName;
    int m_nProfileDimID;
    CPLString m_osProfileVariables;
    int m_nProfileVarID;
    bool m_bProfileVarUnlimited;
    int m_nParentIndexVarID;
    std::shared_ptr<nccfdriver::SGeometry_Reader> m_simpleGeometryReader;
    std::unique_ptr<nccfdriver::netCDFVID>
        layerVID_alloc;  // Allocation wrapper for group specific netCDFVID
    nccfdriver::netCDFVID &layerVID;  // refers to the "correct" VID
    std::string m_sgCRSname;
    size_t m_SGeometryFeatInd;

    const netCDFWriterConfigLayer *m_poLayerConfig;

    nccfdriver::ncLayer_SG_Metadata m_layerSGDefn;

    OGRFeature *GetNextRawFeature();
    double Get1DVarAsDouble(int nVarId, nc_type nVarType, size_t nIndex,
                            NCDFNoDataUnion noDataVal, bool *pbIsNoData);
    CPLErr GetFillValue(int nVarID, char **ppszValue);
    CPLErr GetFillValue(int nVarID, double *pdfValue);
    void GetNoDataValueForFloat(int nVarId, NCDFNoDataUnion *puNoData);
    void GetNoDataValueForDouble(int nVarId, NCDFNoDataUnion *puNoData);
    void GetNoDataValue(int nVarId, nc_type nVarType,
                        NCDFNoDataUnion *puNoData);
    bool FillVarFromFeature(OGRFeature *poFeature, int nMainDimId,
                            size_t nIndex);
    OGRFeature *buildSGeometryFeature(size_t featureInd);
    void netCDFWriteAttributesFromConf(
        int cdfid, int varid,
        const std::vector<netCDFWriterConfigAttribute> &aoAttributes);

  protected:
    bool FillFeatureFromVar(OGRFeature *poFeature, int nMainDimId,
                            size_t nIndex);

  public:
    netCDFLayer(netCDFDataset *poDS, int nLayerCDFId, const char *pszName,
                OGRwkbGeometryType eGeomType, OGRSpatialReference *poSRS);
    virtual ~netCDFLayer();

    bool Create(char **papszOptions,
                const netCDFWriterConfigLayer *poLayerConfig);
    void SetRecordDimID(int nRecordDimID);
    void SetXYZVars(int nXVarId, int nYVarId, int nZVarId);
    void SetWKTGeometryField(const char *pszWKTVarName);
    void SetGridMapping(const char *pszGridMapping);
    void SetProfile(int nProfileDimID, int nParentIndexVarID);

    void EnableSGBypass()
    {
        this->m_bLegacyCreateMode = false;
    }

    bool AddField(int nVarId);

    int GetCDFID() const
    {
        return m_nLayerCDFId;
    }

    void SetCDFID(int nId)
    {
        m_nLayerCDFId = nId;
    }

    void SetSGeometryRepresentation(
        const std::shared_ptr<nccfdriver::SGeometry_Reader> &sg)
    {
        m_simpleGeometryReader = sg;
    }

    nccfdriver::ncLayer_SG_Metadata &getLayerSGMetadata()
    {
        return m_layerSGDefn;
    }

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual GIntBig GetFeatureCount(int bForce) override;

    virtual int TestCapability(const char *pszCap) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr CreateField(const OGRFieldDefn *poFieldDefn,
                               int bApproxOK) override;

    GDALDataset *GetDataset() override;
};

std::string NCDFGetProjectedCFUnit(const OGRSpatialReference *poSRS);
void NCDFWriteLonLatVarsAttributes(nccfdriver::netCDFVID &vcdf, int nVarLonID,
                                   int nVarLatID);
void NCDFWriteRLonRLatVarsAttributes(nccfdriver::netCDFVID &vcdf,
                                     int nVarRLonID, int nVarRLatID);
void NCDFWriteXYVarsAttributes(nccfdriver::netCDFVID &vcdf, int nVarXID,
                               int nVarYID, const OGRSpatialReference *poSRS);
int NCDFWriteSRSVariable(int cdfid, const OGRSpatialReference *poSRS,
                         char **ppszCFProjection, bool bWriteGDALTags,
                         const std::string & = std::string());

double NCDFGetDefaultNoDataValue(int nCdfId, int nVarId, int nVarType,
                                 bool &bGotNoData);

int64_t NCDFGetDefaultNoDataValueAsInt64(int nCdfId, int nVarId,
                                         bool &bGotNoData);
uint64_t NCDFGetDefaultNoDataValueAsUInt64(int nCdfId, int nVarId,
                                           bool &bGotNoData);

CPLErr NCDFGetAttr(int nCdfId, int nVarId, const char *pszAttrName,
                   double *pdfValue);
CPLErr NCDFGetAttr(int nCdfId, int nVarId, const char *pszAttrName,
                   char **pszValue);
bool NCDFIsUnlimitedDim(bool bIsNC4, int cdfid, int nDimId);
bool NCDFIsUserDefinedType(int ncid, int type);

CPLString NCDFGetGroupFullName(int nGroupId);

CPLErr NCDFResolveVar(int nStartGroupId, const char *pszVar, int *pnGroupId,
                      int *pnVarId, bool bMandatory = false);

// Dimension check functions.
bool NCDFIsVarLongitude(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarLatitude(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarProjectionX(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarProjectionY(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarVerticalCoord(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarTimeCoord(int nCdfId, int nVarId, const char *pszVarName);

std::string NCDFReadMetadataAsJson(int cdfid);

char **NCDFTokenizeCoordinatesAttribute(const char *pszCoordinates);

extern CPLMutex *hNCMutex;

#ifdef ENABLE_NCDUMP
bool netCDFDatasetCreateTempFile(NetCDFFormatEnum eFormat,
                                 const char *pszTmpFilename, VSILFILE *fpSrc);
#endif

int GDAL_nc_open(const char *pszFilename, int nMode, int *pID);
int GDAL_nc_close(int cdfid);

#endif
