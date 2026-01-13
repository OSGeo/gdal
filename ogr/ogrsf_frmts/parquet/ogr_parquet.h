/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_PARQUET_H
#define OGR_PARQUET_H

#include "ogrsf_frmts.h"

#include "cpl_json.h"

#include <functional>
#include <map>
#include <set>

#include "../arrow_common/ogr_arrow.h"
#include "ogr_include_parquet.h"

constexpr int DEFAULT_COMPRESSION_LEVEL = -1;
constexpr int OGR_PARQUET_ZSTD_DEFAULT_COMPRESSION_LEVEL = 9;

/************************************************************************/
/*                       OGRParquetLayerBase                            */
/************************************************************************/

class OGRParquetDataset;

class OGRParquetLayerBase CPL_NON_FINAL : public OGRArrowLayer
{
    OGRParquetLayerBase(const OGRParquetLayerBase &) = delete;
    OGRParquetLayerBase &operator=(const OGRParquetLayerBase &) = delete;

  protected:
    OGRParquetLayerBase(OGRParquetDataset *poDS, const char *pszLayerName,
                        CSLConstList papszOpenOptions);

    OGRParquetDataset *m_poDS = nullptr;
    std::shared_ptr<arrow::RecordBatchReader> m_poRecordBatchReader{};
    CPLStringList m_aosGeomPossibleNames{};
    std::string m_osCRS{};

#if PARQUET_VERSION_MAJOR >= 21
    std::set<int>
        m_geoStatsWithBBOXAvailable{};  // key is index of OGR geometry column
#endif

    void LoadGeoMetadata(
        const std::shared_ptr<const arrow::KeyValueMetadata> &kv_metadata);
    bool DealWithGeometryColumn(
        int iFieldIdx, const std::shared_ptr<arrow::Field> &field,
        std::function<OGRwkbGeometryType(void)> computeGeometryTypeFun,
        const parquet::ColumnDescriptor *parquetColumn,
        const parquet::FileMetaData *metadata, int iColumn);

    void InvalidateCachedBatches() override;

    static bool ParseGeometryColumnCovering(const CPLJSONObject &oJSONDef,
                                            std::string &osBBOXColumn,
                                            std::string &osXMin,
                                            std::string &osYMin,
                                            std::string &osXMax,
                                            std::string &osYMax);

  public:
    int TestCapability(const char *) const override;

    void ResetReading() override;

    GDALDataset *GetDataset() override;

    static int GetNumCPUs();
};

/************************************************************************/
/*                        OGRParquetLayer                               */
/************************************************************************/

class OGRParquetLayer final : public OGRParquetLayerBase

{
    std::unique_ptr<parquet::arrow::FileReader> m_poArrowReader{};
    bool m_bSingleBatch = false;
    int m_iFIDParquetColumn = -1;
    std::shared_ptr<arrow::DataType> m_poFIDType{};
    std::vector<std::shared_ptr<arrow::DataType>>
        m_apoArrowDataTypes{};  // .size() == field ocunt
    std::vector<int> m_anMapFieldIndexToParquetColumn{};
    std::vector<std::vector<int>> m_anMapGeomFieldIndexToParquetColumns{};
    bool m_bHasMissingMappingToParquet = false;

    //! Contains pairs of (selected feature idx, total feature idx) break points.
    std::vector<std::pair<int64_t, int64_t>> m_asFeatureIdxRemapping{};
    //! Iterator over m_asFeatureIdxRemapping
    std::vector<std::pair<int64_t, int64_t>>::iterator
        m_oFeatureIdxRemappingIter{};
    //! Feature index among the potentially restricted set of selected row groups
    int64_t m_nFeatureIdxSelected = 0;
    std::vector<int> m_anRequestedParquetColumns{};  // only valid when
                                                     // m_bIgnoredFields is set
    CPLStringList m_aosFeatherMetadata{};

    //! Describe the bbox column of a geometry column
    struct GeomColBBOXParquet
    {
        int iParquetXMin = -1;
        int iParquetYMin = -1;
        int iParquetXMax = -1;
        int iParquetYMax = -1;
        std::vector<int> anParquetCols{};
    };

    //! Map from OGR geometry field index to GeomColBBOXParquet
    std::map<int, GeomColBBOXParquet>
        m_oMapGeomFieldIndexToGeomColBBOXParquet{};

    //! GDAL creation options that were used to create the file (if done by GDAL)
    CPLStringList m_aosCreationOptions{};

    void EstablishFeatureDefn();
    void ProcessGeometryColumnCovering(
        const std::shared_ptr<arrow::Field> &field,
        const CPLJSONObject &oJSONGeometryColumn,
        const std::map<std::string, int> &oMapParquetColumnNameToIdx);
    bool CreateRecordBatchReader(int iStartingRowGroup);
    bool CreateRecordBatchReader(const std::vector<int> &anRowGroups);
    bool ReadNextBatch() override;

    void InvalidateCachedBatches() override;

    OGRwkbGeometryType ComputeGeometryColumnType(int iGeomCol,
                                                 int iParquetCol) const;
    void CreateFieldFromSchema(
        const std::shared_ptr<arrow::Field> &field, bool bParquetColValid,
        int &iParquetCol, const std::vector<int> &path,
        const std::map<std::string, std::unique_ptr<OGRFieldDefn>>
            &oMapFieldNameToGDALSchemaFieldDefn);
    bool CheckMatchArrowParquetColumnNames(
        int &iParquetCol, const std::shared_ptr<arrow::Field> &field) const;
    OGRFeature *GetFeatureExplicitFID(GIntBig nFID);
    OGRFeature *GetFeatureByIndex(GIntBig nFID);

    std::string GetDriverUCName() const override
    {
        return "PARQUET";
    }

    bool FastGetExtent(int iGeomField, OGREnvelope *psExtent) const override;

    void IncrFeatureIdx() override;

  public:
    OGRParquetLayer(OGRParquetDataset *poDS, const char *pszLayerName,
                    std::unique_ptr<parquet::arrow::FileReader> &&arrow_reader,
                    CSLConstList papszOpenOptions);

    void ResetReading() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    GIntBig GetFeatureCount(int bForce) override;
    int TestCapability(const char *pszCap) const override;
    OGRErr SetIgnoredFields(CSLConstList papszFields) override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;
    CSLConstList GetMetadata(const char *pszDomain = "") override;
    OGRErr SetNextByIndex(GIntBig nIndex) override;

    bool GetArrowStream(struct ArrowArrayStream *out_stream,
                        CSLConstList papszOptions = nullptr) override;

    std::unique_ptr<OGRFieldDomain> BuildDomain(const std::string &osDomainName,
                                                int iFieldIndex) const override;

    parquet::arrow::FileReader *GetReader() const
    {
        return m_poArrowReader.get();
    }

    std::vector<int>
    GetParquetColumnIndicesForArrowField(const std::string &field_name) const;

    const std::vector<std::shared_ptr<arrow::DataType>> &
    GetArrowFieldTypes() const
    {
        return m_apoArrowDataTypes;
    }

    int GetFIDParquetColumn() const
    {
        return m_iFIDParquetColumn;
    }

    const CPLStringList &GetCreationOptions() const
    {
        return m_aosCreationOptions;
    }

    static constexpr int OGR_FID_INDEX = -2;
    bool GetMinMaxForOGRField(int iRowGroup,  // -1 for all
                              int iOGRField,  // or OGR_FID_INDEX
                              bool bComputeMin, OGRField &sMin, bool &bFoundMin,
                              bool bComputeMax, OGRField &sMax, bool &bFoundMax,
                              OGRFieldType &eType, OGRFieldSubType &eSubType,
                              std::string &osMinTmp,
                              std::string &osMaxTmp) const;

    bool GetMinMaxForParquetCol(int iRowGroup,  // -1 for all
                                int iCol,
                                const std::shared_ptr<arrow::DataType>
                                    &arrowType,  // potentially nullptr
                                bool bComputeMin, OGRField &sMin,
                                bool &bFoundMin, bool bComputeMax,
                                OGRField &sMax, bool &bFoundMax,
                                OGRFieldType &eType, OGRFieldSubType &eSubType,
                                std::string &osMinTmp,
                                std::string &osMaxTmp) const;

    bool GeomColsBBOXParquet(int iGeom, int &iParquetXMin, int &iParquetYMin,
                             int &iParquetXMax, int &iParquetYMax) const;
};

/************************************************************************/
/*                      OGRParquetDatasetLayer                          */
/************************************************************************/

#ifdef GDAL_USE_ARROWDATASET

class OGRParquetDatasetLayer final : public OGRParquetLayerBase
{
    bool m_bIsVSI = false;
    bool m_bRebuildScanner = true;
    bool m_bSkipFilterGeometry = false;
    std::shared_ptr<arrow::dataset::Dataset> m_poDataset{};
    std::shared_ptr<arrow::dataset::Scanner> m_poScanner{};
    std::vector<std::string> m_aosProjectedFields{};

    void EstablishFeatureDefn();
    void
    ProcessGeometryColumnCovering(const std::shared_ptr<arrow::Field> &field,
                                  const CPLJSONObject &oJSONGeometryColumn);

    void BuildScanner();

    //! Translate a OGR SQL expression into an Arrow one
    // bFullyTranslated should be set to true before calling this method.
    arrow::compute::Expression BuildArrowFilter(const swq_expr_node *poNode,
                                                bool &bFullyTranslated);

  protected:
    std::string GetDriverUCName() const override
    {
        return "PARQUET";
    }

    bool ReadNextBatch() override;

    bool FastGetExtent(int iGeomField, OGREnvelope *psExtent) const override;

  public:
    OGRParquetDatasetLayer(
        OGRParquetDataset *poDS, const char *pszLayerName, bool bIsVSI,
        const std::shared_ptr<arrow::dataset::Dataset> &dataset,
        CSLConstList papszOpenOptions);

    OGRFeature *GetNextFeature() override;

    GIntBig GetFeatureCount(int bForce) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *pszFilter) override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    int TestCapability(const char *) const override;

    // TODO
    std::unique_ptr<OGRFieldDomain>
    BuildDomain(const std::string & /*osDomainName*/,
                int /*iFieldIndex*/) const override
    {
        return nullptr;
    }
};

#endif

/************************************************************************/
/*                         OGRParquetDataset                            */
/************************************************************************/

class OGRParquetDataset final : public OGRArrowDataset
{
    std::shared_ptr<arrow::fs::FileSystem> m_poFS{};

  public:
    explicit OGRParquetDataset();
    ~OGRParquetDataset() override;

    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override;

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poResultsSet) override;

    int TestCapability(const char *) const override;

    void SetFileSystem(const std::shared_ptr<arrow::fs::FileSystem> &fs)
    {
        m_poFS = fs;
    }

    std::unique_ptr<OGRParquetLayer>
    CreateReaderLayer(const std::string &osFilename, VSILFILE *&fpIn,
                      CSLConstList papszOpenOptionsIn);
};

/************************************************************************/
/*                        OGRParquetWriterLayer                         */
/************************************************************************/

class OGRParquetWriterDataset;

class OGRParquetWriterLayer final : public OGRArrowWriterLayer
{
    OGRParquetWriterLayer(const OGRParquetWriterLayer &) = delete;
    OGRParquetWriterLayer &operator=(const OGRParquetWriterLayer &) = delete;

    OGRParquetWriterDataset *m_poDataset = nullptr;
    std::unique_ptr<parquet::arrow::FileWriter> m_poFileWriter{};
    std::shared_ptr<const arrow::KeyValueMetadata> m_poKeyValueMetadata{};
    bool m_bForceCounterClockwiseOrientation = false;
    parquet::WriterProperties::Builder m_oWriterPropertiesBuilder{};

    //! Temporary GeoPackage dataset. Only used in SORT_BY_BBOX mode
    std::unique_ptr<GDALDataset> m_poTmpGPKG{};
    //! Temporary GeoPackage layer. Only used in SORT_BY_BBOX mode
    OGRLayer *m_poTmpGPKGLayer = nullptr;
    //! Number of features written by ICreateFeature(). Only used in SORT_BY_BBOX mode
    GIntBig m_nTmpFeatureCount = 0;

    //! Whether to write "geo" footer metadata;
    bool m_bWriteGeoMetadata = true;

    bool IsFileWriterCreated() const override
    {
        return m_poFileWriter != nullptr;
    }

    void CreateWriter() override;
    bool CloseFileWriter() override;

    void CreateSchema() override;
    void PerformStepsBeforeFinalFlushGroup() override;

    bool FlushGroup() override;

    std::string GetDriverUCName() const override
    {
        return "PARQUET";
    }

    virtual bool
    IsSupportedGeometryType(OGRwkbGeometryType eGType) const override;

    virtual void FixupWKBGeometryBeforeWriting(GByte *pabyWKB,
                                               size_t nLen) override;
    void FixupGeometryBeforeWriting(OGRGeometry *poGeom) override;

    bool IsSRSRequired() const override
    {
        return false;
    }

    std::string GetGeoMetadata() const;

    //! Copy temporary GeoPackage layer to final Parquet file
    bool CopyTmpGpkgLayerToFinalFile();

  public:
    OGRParquetWriterLayer(
        OGRParquetWriterDataset *poDS, arrow::MemoryPool *poMemoryPool,
        const std::shared_ptr<arrow::io::OutputStream> &poOutputStream,
        const char *pszLayerName);

    CPLErr SetMetadata(CSLConstList papszMetadata,
                       const char *pszDomain) override;

    bool SetOptions(const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                    CSLConstList papszOptions);

    OGRErr CreateGeomField(const OGRGeomFieldDefn *poField,
                           int bApproxOK = TRUE) override;

    int TestCapability(const char *pszCap) const override;
#if PARQUET_VERSION_MAJOR <= 10
    // Parquet <= 10 doesn't support the WriteRecordBatch() API
    bool IsArrowSchemaSupported(const struct ArrowSchema *schema,
                                CSLConstList papszOptions,
                                std::string &osErrorMsg) const override
    {
        return OGRLayer::IsArrowSchemaSupported(schema, papszOptions,
                                                osErrorMsg);
    }

    bool
    CreateFieldFromArrowSchema(const struct ArrowSchema *schema,
                               CSLConstList papszOptions = nullptr) override
    {
        return OGRLayer::CreateFieldFromArrowSchema(schema, papszOptions);
    }

    bool WriteArrowBatch(const struct ArrowSchema *schema,
                         struct ArrowArray *array,
                         CSLConstList papszOptions = nullptr) override
    {
        return OGRLayer::WriteArrowBatch(schema, array, papszOptions);
    }
#else
    bool IsArrowSchemaSupported(const struct ArrowSchema *schema,
                                CSLConstList papszOptions,
                                std::string &osErrorMsg) const override;
    bool
    CreateFieldFromArrowSchema(const struct ArrowSchema *schema,
                               CSLConstList papszOptions = nullptr) override;
    bool WriteArrowBatch(const struct ArrowSchema *schema,
                         struct ArrowArray *array,
                         CSLConstList papszOptions = nullptr) override;
#endif

    GDALDataset *GetDataset() override;

  protected:
    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    friend class OGRParquetWriterDataset;
    bool Close();
};

/************************************************************************/
/*                        OGRParquetWriterDataset                       */
/************************************************************************/

class OGRParquetWriterDataset final : public GDALPamDataset
{
    std::unique_ptr<arrow::MemoryPool> m_poMemoryPool{};
    std::unique_ptr<OGRParquetWriterLayer> m_poLayer{};
    std::shared_ptr<arrow::io::OutputStream> m_poOutputStream{};

  public:
    explicit OGRParquetWriterDataset(
        const std::shared_ptr<arrow::io::OutputStream> &poOutputStream);

    ~OGRParquetWriterDataset() override;

    arrow::MemoryPool *GetMemoryPool() const
    {
        return m_poMemoryPool.get();
    }

    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override;

    int GetLayerCount() const override;
    const OGRLayer *GetLayer(int idx) const override;
    int TestCapability(const char *pszCap) const override;
    std::vector<std::string> GetFieldDomainNames(
        CSLConstList /*papszOptions*/ = nullptr) const override;
    const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;
    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;

    GDALMultiDomainMetadata &GetMultiDomainMetadata()
    {
        return oMDMD;
    }

  protected:
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
};

#endif  // OGR_PARQUET_H
