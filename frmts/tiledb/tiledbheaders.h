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

#include <algorithm>
#include <list>
#include <variant>

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include "include_tiledb.h"

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 17)
struct gdal_tiledb_vector_of_bool
{
    size_t m_size = 0;
    size_t m_capacity = 0;
    bool *m_v = nullptr;

    gdal_tiledb_vector_of_bool() = default;

    ~gdal_tiledb_vector_of_bool()
    {
        std::free(m_v);
    }

    gdal_tiledb_vector_of_bool(gdal_tiledb_vector_of_bool &&other)
        : m_size(other.m_size), m_capacity(other.m_capacity),
          m_v(std::move(other.m_v))
    {
        other.m_size = 0;
        other.m_capacity = 0;
        other.m_v = nullptr;
    }

    gdal_tiledb_vector_of_bool(const gdal_tiledb_vector_of_bool &) = delete;
    gdal_tiledb_vector_of_bool &
    operator=(const gdal_tiledb_vector_of_bool &) = delete;
    gdal_tiledb_vector_of_bool &
    operator=(gdal_tiledb_vector_of_bool &&) = delete;

    size_t size() const
    {
        return m_size;
    }

    const bool *data() const
    {
        return m_v;
    }

    bool *data()
    {
        return m_v;
    }

    bool &operator[](size_t idx)
    {
        return m_v[idx];
    }

    bool operator[](size_t idx) const
    {
        return m_v[idx];
    }

    void resize(size_t new_size)
    {
        if (new_size > m_capacity)
        {
            const size_t new_capacity =
                std::max<size_t>(new_size, 2 * m_capacity);
            bool *new_v = static_cast<bool *>(
                std::realloc(m_v, new_capacity * sizeof(bool)));
            if (!new_v)
            {
                throw std::bad_alloc();
            }
            m_v = new_v;
            m_capacity = new_capacity;
        }
        if (new_size > m_size)
            memset(m_v + m_size, 0, (new_size - m_size) * sizeof(bool));
        m_size = new_size;
    }

    void clear()
    {
        resize(0);
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    void push_back(uint8_t v)
    {
        resize(size() + 1);
        m_v[size() - 1] = static_cast<bool>(v);
    }
};

#define VECTOR_OF_BOOL gdal_tiledb_vector_of_bool
#define VECTOR_OF_BOOL_IS_NOT_UINT8_T
#else
#define VECTOR_OF_BOOL std::vector<uint8_t>
#endif

typedef enum
{
    BAND = 0,
    PIXEL = 1,
    ATTRIBUTES = 2
} TILEDB_INTERLEAVE_MODE;

#define DEFAULT_TILE_CAPACITY 10000

#define DEFAULT_BATCH_SIZE 500000

constexpr const char *TILEDB_VALUES = "TDB_VALUES";

constexpr const char *GDAL_ATTRIBUTE_NAME = "_gdal";

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

    static GDALDataset *OpenMultiDimensional(GDALOpenInfo *);
    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papszOptions);
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
    uint64_t nBandStart = 1;
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

    CPLErr AddDimensions(tiledb::Domain &domain, const char *pszAttrName,
                         tiledb::Dimension &y, tiledb::Dimension &x,
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
  public:
    typedef std::variant<std::shared_ptr<std::string>,
#ifdef VECTOR_OF_BOOL_IS_NOT_UINT8_T
                         std::shared_ptr<VECTOR_OF_BOOL>,
#endif
                         std::shared_ptr<std::vector<uint8_t>>,
                         std::shared_ptr<std::vector<int16_t>>,
                         std::shared_ptr<std::vector<uint16_t>>,
                         std::shared_ptr<std::vector<int32_t>>,
                         std::shared_ptr<std::vector<int64_t>>,
                         std::shared_ptr<std::vector<float>>,
                         std::shared_ptr<std::vector<double>>>
        ArrayType;

  private:
    friend OGRTileDBDataset;
    GDALDataset *m_poDS = nullptr;
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
    GIntBig m_nNextFID = 1;
    int64_t m_nTotalFeatureCount = -1;
    bool m_bStats = false;
    bool m_bQueryComplete = false;
    bool m_bGrowBuffers = false;
    uint64_t m_nOffsetInResultSet = 0;
    uint64_t m_nRowCountInResultSet = 0;
    int m_nUseOptimizedAttributeFilter = -1;  // uninitialized

    tiledb_datatype_t m_eTileDBStringType = TILEDB_STRING_UTF8;

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

    // Boolean shared between the OGRTileDBLayer instance and the
    // OGRTileDBArrowArrayPrivateData instances, that are stored in
    // ArrowArray::private_data, so ReleaseArrowArray() function knows
    // if the OGRLayer is still alive.
    std::shared_ptr<bool> m_pbLayerStillAlive;

    // Flag set to false by GetNextArrowArray() to indicate that the m_anFIDs,
    // m_adfXs, m_adfYs, m_adfZs, m_aFieldValues, m_aFieldValueOffsets,
    // m_abyGeometries and m_anGeometryOffsets are currently used by a
    // ArrowArray returned. If this flag is still set to false when the
    // next SetupQuery() is called, we need to re-instanciate new arrays, so
    // the ArrowArray's can be used independently of the new state of the layer.
    bool m_bArrowBatchReleased = true;

    std::shared_ptr<std::vector<int64_t>> m_anFIDs;
    std::shared_ptr<std::vector<double>> m_adfXs;
    std::shared_ptr<std::vector<double>> m_adfYs;
    std::shared_ptr<std::vector<double>> m_adfZs;
    std::vector<tiledb_datatype_t> m_aeFieldTypes{};
    std::vector<int> m_aeFieldTypesInCreateField{};
    std::vector<size_t> m_anFieldValuesCapacity{};
    std::vector<ArrayType> m_aFieldValues;
    std::vector<std::shared_ptr<std::vector<uint64_t>>> m_aFieldValueOffsets;
    std::vector<std::vector<uint8_t>> m_aFieldValidity;
    size_t m_nGeometriesCapacity = 0;
    std::shared_ptr<std::vector<unsigned char>> m_abyGeometries;
    std::shared_ptr<std::vector<uint64_t>> m_anGeometryOffsets;

    struct OGRTileDBArrowArrayPrivateData
    {
        OGRTileDBLayer *m_poLayer = nullptr;
        std::shared_ptr<bool> m_pbLayerStillAlive;

        ArrayType valueHolder;
        std::shared_ptr<std::vector<uint8_t>> nullHolder;
        std::shared_ptr<std::vector<uint64_t>> offsetHolder;
    };

    size_t m_nBatchSize = DEFAULT_BATCH_SIZE;
    size_t m_nTileCapacity = DEFAULT_TILE_CAPACITY;
    double m_dfTileExtent = 0;
    double m_dfZTileExtent = 0;
    size_t m_nEstimatedWkbSizePerRow = 0;
    std::map<std::string, size_t> m_oMapEstimatedSizePerRow;
    double m_dfPadX = 0;
    double m_dfPadY = 0;
    double m_dfPadZ = 0;

    const char *GetDatabaseGeomColName();
    void InitializeSchemaAndArray();
    void FlushArrays();
    void AllocateNewBuffers();
    void ResetBuffers();
    void SwitchToReadingMode();
    void SwitchToWritingMode();
    bool InitFromStorage(tiledb::Context *poCtx, uint64_t nTimestamp,
                         CSLConstList papszOpenOptions);
    void SetReadBuffers(bool bGrowVariableSizeArrays);
    bool SetupQuery(tiledb::QueryCondition *queryCondition);
    OGRFeature *TranslateCurrentFeature();

    OGRFeature *GetNextRawFeature();
    std::unique_ptr<tiledb::QueryCondition>
    CreateQueryCondition(const swq_expr_node *poNode, bool &bAlwaysTrue,
                         bool &bAlwaysFalse);
    std::unique_ptr<tiledb::QueryCondition> CreateQueryCondition(
        int nOperation, bool bColumnIsLeft, const swq_expr_node *poColumn,
        const swq_expr_node *poValue, bool &bAlwaysTrue, bool &bAlwaysFalse);

    static void ReleaseArrowArray(struct ArrowArray *array);
    void FillBoolArray(struct ArrowArray *psChild, int iField,
                       const std::vector<bool> &abyValidityFromFilters);
    void SetNullBuffer(struct ArrowArray *psChild, int iField,
                       const std::vector<bool> &abyValidityFromFilters);
    template <typename T>
    void FillPrimitiveArray(struct ArrowArray *psChild, int iField,
                            const std::vector<bool> &abyValidityFromFilters);
    void FillBoolListArray(struct ArrowArray *psChild, int iField,
                           const std::vector<bool> &abyValidityFromFilters);
    template <typename T>
    void
    FillPrimitiveListArray(struct ArrowArray *psChild, int iField,
                           const std::vector<bool> &abyValidityFromFilters);
    template <typename T>
    void
    FillStringOrBinaryArray(struct ArrowArray *psChild, int iField,
                            const std::vector<bool> &abyValidityFromFilters);
    void FillTimeOrDateArray(struct ArrowArray *psChild, int iField,
                             const std::vector<bool> &abyValidityFromFilters);
    int GetArrowSchema(struct ArrowArrayStream *,
                       struct ArrowSchema *out_schema) override;
    int GetNextArrowArray(struct ArrowArrayStream *,
                          struct ArrowArray *out_array) override;

  public:
    OGRTileDBLayer(GDALDataset *poDS, const char *pszFilename,
                   const char *pszLayerName, const OGRwkbGeometryType eGType,
                   const OGRSpatialReference *poSRS);
    ~OGRTileDBLayer();
    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRTileDBLayer)
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField, int bApproxOK) override;
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

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
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
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    static GDALDataset *Open(GDALOpenInfo *, tiledb::Object::Type objectType);
    static GDALDataset *Create(const char *pszFilename,
                               CSLConstList papszOptions);
};

#endif  // TILEDB_HEADERS_H
