/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Include tiledb headers
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
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

#ifndef TILEDB_HEADERS_H
#define TILEDB_HEADERS_H

#include <list>
#include <variant>

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) /* XXXX was deprecated */
#endif

#include "tiledb/tiledb"
#include "tiledb/tiledb_experimental"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 9)
#define HAS_TILEDB_GROUP
#endif

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 10)
#define HAS_TILEDB_BOOL
#endif

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 11)
#define HAS_TILEDB_WORKING_OR_FILTER
#endif

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 14)
#define HAS_TILEDB_WORKING_UTF8_STRING_FILTER
#endif

typedef enum
{
    BAND = 0,
    PIXEL = 1,
    ATTRIBUTES = 2
} TILEDB_INTERLEAVE_MODE;

const CPLString TILEDB_VALUES("TDB_VALUES");

/************************************************************************/
/* ==================================================================== */
/*                               TileRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class TileDBRasterBand;

/************************************************************************/
/* ==================================================================== */
/*                               TileDBDataset                          */
/* ==================================================================== */
/************************************************************************/

class TileDBDataset : public GDALPamDataset
{
  protected:
    std::unique_ptr<tiledb::Context> m_ctx;

  public:
    static CPLErr AddFilter(tiledb::Context &ctx,
                            tiledb::FilterList &filterList,
                            const char *pszFilterName, const int level);
    static int Identify(GDALOpenInfo *);
    static CPLErr Delete(const char *pszFilename);
    static CPLString VSI_to_tiledb_uri(const char *pszUri);

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                            TileDRasterDataset                        */
/* ==================================================================== */
/************************************************************************/

class TileDBRasterDataset final : public TileDBDataset
{
    friend class TileDBRasterBand;

  protected:
    std::unique_ptr<tiledb::Context> m_roCtx;
    std::unique_ptr<tiledb::Array> m_array;
    std::unique_ptr<tiledb::Array> m_roArray;
    std::unique_ptr<tiledb::ArraySchema> m_schema;
    std::unique_ptr<tiledb::FilterList> m_filterList;
    CPLString osMetaDoc;
    TILEDB_INTERLEAVE_MODE eIndexMode = BAND;
    int nBitsPerSample = 8;
    GDALDataType eDataType = GDT_Unknown;
    int nBlockXSize = -1;
    int nBlockYSize = -1;
    int nBlocksX = 0;
    int nBlocksY = 0;
    int nBandStart = 1;
    bool bHasSubDatasets = false;
    int nSubDataCount = 0;
    char **papszSubDatasets = nullptr;
    CPLStringList m_osSubdatasetMD{};
    CPLXMLNode *psSubDatasetsTree = nullptr;
    char **papszAttributes = nullptr;
    std::list<std::unique_ptr<GDALDataset>> lpoAttributeDS = {};
    uint64_t nTimestamp = 0;

    bool bStats = FALSE;
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, int, int *, GSpacing, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;
    CPLErr CreateAttribute(GDALDataType eType, const CPLString &osAttrName,
                           const int nSubRasterCount = 1);

    CPLErr AddDimensions(tiledb::Domain &domain, tiledb::Dimension &y,
                         tiledb::Dimension &x,
                         tiledb::Dimension *poBands = nullptr);

  public:
    ~TileDBRasterDataset();
    CPLErr TryLoadCachedXML(char **papszSiblingFiles = nullptr,
                            bool bReload = true);
    CPLErr TryLoadXML(char **papszSiblingFiles = nullptr) override;
    CPLErr TrySaveXML() override;
    char **GetMetadata(const char *pszDomain) override;
    virtual CPLErr FlushCache(bool bAtClosing) override;
    static CPLErr CopySubDatasets(GDALDataset *poSrcDS,
                                  TileDBRasterDataset *poDstDS,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData);
    static TileDBRasterDataset *CreateLL(const char *pszFilename, int nXSize,
                                         int nYSize, int nBands,
                                         GDALDataType eType,
                                         char **papszOptions);
    static void SetBlockSize(GDALRasterBand *poBand, char **&papszOptions);

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/*                        OGRTileDBLayer                                */
/************************************************************************/

class OGRTileDBDataset;

class OGRTileDBLayer final : public OGRLayer,
                             public OGRGetNextFeatureThroughRaw<OGRTileDBLayer>
{
    friend OGRTileDBDataset;
    std::string m_osGroupName{};
    std::string m_osFilename{};
    uint64_t m_nTimestamp = 0;
    bool m_bUpdatable = false;
    enum class CurrentMode
    {
        None,
        ReadInProgress,
        WriteInProgress
    };
    CurrentMode m_eCurrentMode = CurrentMode::None;
    std::unique_ptr<tiledb::Context> m_ctx;
    std::unique_ptr<tiledb::Array> m_array;
    std::unique_ptr<tiledb::ArraySchema> m_schema;
    std::unique_ptr<tiledb::Query> m_query;
    std::unique_ptr<tiledb::FilterList> m_filterList;
    bool m_bAttributeFilterPartiallyTranslated =
        false;  // for debugging purposes
    bool m_bAttributeFilterAlwaysFalse = false;
    bool m_bAttributeFilterAlwaysTrue = false;
    std::unique_ptr<tiledb::QueryCondition> m_poQueryCondition;
    bool m_bInitializationAttempted = false;
    bool m_bInitialized = false;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    std::string m_osFIDColumn{};
    GIntBig m_nNextFID = -1;
    int64_t m_nTotalFeatureCount = -1;
    bool m_bQueryComplete = false;
    uint64_t m_nOffsetInResultSet = 0;
    uint64_t m_nRowCountInResultSet = 0;
    int m_nUseOptimizedAttributeFilter = -1;  // uninitialized

#ifdef HAS_TILEDB_WORKING_UTF8_STRING_FILTER
    tiledb_datatype_t m_eTileDBStringType = TILEDB_STRING_UTF8;
#else
    tiledb_datatype_t m_eTileDBStringType = TILEDB_STRING_ASCII;
#endif

    std::string m_osXDim = "_X";
    std::string m_osYDim = "_Y";
    std::string m_osZDim;  // may be empty

    // Domain extent
    double m_dfXStart = 0;
    double m_dfYStart = 0;
    double m_dfZStart = -10000;
    double m_dfXEnd = 0;
    double m_dfYEnd = 0;
    double m_dfZEnd = 10000;

    // Extent of all features
    OGREnvelope m_oLayerExtent;

    std::vector<int64_t> m_anFIDs;
    std::vector<double> m_adfXs;
    std::vector<double> m_adfYs;
    std::vector<double> m_adfZs;
    typedef std::variant<std::string, std::vector<uint8_t>,
                         std::vector<int16_t>, std::vector<int32_t>,
                         std::vector<int64_t>, std::vector<float>,
                         std::vector<double>>
        ArrayType;
    std::vector<ArrayType> m_aFieldValues;
    std::vector<std::vector<uint64_t>> m_aFieldValueOffsets;
    std::vector<std::vector<uint8_t>> m_aFieldValidity;
    std::vector<unsigned char> m_abyGeometries;
    std::vector<uint64_t> m_anGeometryOffsets;
    size_t m_nBatchSize = 65536;
    double m_dfTileExtent = 0;
    size_t m_nEstimatedWkbSizePerRow = 0;
    std::map<std::string, size_t> m_oMapEstimatedSizePerRow;
    double m_dfPadX = 0;
    double m_dfPadY = 0;
    double m_dfPadZ = 0;

    const char *GetDatabaseGeomColName();
    void InitializeSchemaAndArray();
    void FlushArrays();
    void ResetBuffers();
    void SwitchToReadingMode();
    void SwitchToWritingMode();
    bool InitFromStorage(tiledb::Context *poCtx, uint64_t nTimestamp,
                         CSLConstList papszOpenOptions);
    void SetReadBuffers(bool bGrowVariableSizeArrays = false);
    bool SetupQuery(tiledb::QueryCondition *queryCondition);
    OGRFeature *TranslateCurrentFeature();

    OGRFeature *GetNextRawFeature();
    std::unique_ptr<tiledb::QueryCondition>
    CreateQueryCondition(const swq_expr_node *poNode, bool &bAlwaysTrue,
                         bool &bAlwaysFalse);
    std::unique_ptr<tiledb::QueryCondition> CreateQueryCondition(
        int nOperation, bool bColumnIsLeft, const swq_expr_node *poColumn,
        const swq_expr_node *poValue, bool &bAlwaysTrue, bool &bAlwaysFalse);

  public:
    OGRTileDBLayer(const char *pszFilename, const char *pszLayerName,
                   const OGRwkbGeometryType eGType,
                   const OGRSpatialReference *poSRS);
    ~OGRTileDBLayer();
    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRTileDBLayer);
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK) override;
    int TestCapability(const char *) override;
    GIntBig GetFeatureCount(int bForce) override;
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }
    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    OGRErr SetAttributeFilter(const char *pszFilter) override;

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;
};

/************************************************************************/
/*                         OGRTileDBDataset                             */
/************************************************************************/

class OGRTileDBDataset final : public TileDBDataset
{
    friend OGRTileDBLayer;
    std::string m_osGroupName{};
    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};

  public:
    OGRTileDBDataset();
    ~OGRTileDBDataset();
    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }
    OGRLayer *GetLayer(int nIdx) override
    {
        return nIdx >= 0 && nIdx < GetLayerCount() ? m_apoLayers[nIdx].get()
                                                   : nullptr;
    }
    int TestCapability(const char *) override;
    OGRLayer *ICreateLayer(const char *pszName,
                           OGRSpatialReference *poSpatialRef = nullptr,
                           OGRwkbGeometryType eGType = wkbUnknown,
                           char **papszOptions = nullptr) override;
    static GDALDataset *Open(GDALOpenInfo *, tiledb::Object::Type objectType);
    static GDALDataset *Create(const char *pszFilename,
                               CSLConstList papszOptions);
};

#endif  // TILEDB_HEADERS_H
