/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef OGR_ARROW_H
#define OGR_ARROW_H

#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include <map>
#include <set>

#include "ogr_include_arrow.h"

enum class OGRArrowGeomEncoding
{
    WKB,
    WKT,

    // F(ixed) S(ize) L(ist) of (x,y[,z][,m]) values / Interleaved layout
    GEOARROW_FSL_GENERIC,  // only used by OGRArrowWriterLayer::m_eGeomEncoding
    GEOARROW_FSL_POINT,
    GEOARROW_FSL_LINESTRING,
    GEOARROW_FSL_POLYGON,
    GEOARROW_FSL_MULTIPOINT,
    GEOARROW_FSL_MULTILINESTRING,
    GEOARROW_FSL_MULTIPOLYGON,

    // Struct of (x,y,[,z][,m])
    GEOARROW_STRUCT_GENERIC,  // only used by OGRArrowWriterLayer::m_eGeomEncoding
    GEOARROW_STRUCT_POINT,
    GEOARROW_STRUCT_LINESTRING,
    GEOARROW_STRUCT_POLYGON,
    GEOARROW_STRUCT_MULTIPOINT,
    GEOARROW_STRUCT_MULTILINESTRING,
    GEOARROW_STRUCT_MULTIPOLYGON,
};

/************************************************************************/
/*                         OGRArrowLayer                                */
/************************************************************************/

class OGRArrowDataset;

class OGRArrowLayer CPL_NON_FINAL
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGRArrowLayer>
{
  public:
    struct Constraint
    {
        enum class Type
        {
            Integer,
            Integer64,
            Real,
            String,
        };
        int iField = -1;      // index to a OGRFeatureDefn OGRField
        int iArrayIdx = -1;   // index to m_poBatchColumns
        int nOperation = -1;  // SWQ_xxxx
        Type eType{};
        OGRField sValue{};
        std::string osValue{};
    };

  private:
    OGRArrowLayer(const OGRArrowLayer &) = delete;
    OGRArrowLayer &operator=(const OGRArrowLayer &) = delete;

    int m_nUseOptimizedAttributeFilter = -1;
    bool m_bSpatialFilterIntersectsLayerExtent = true;
    bool m_bUseRecordBatchBaseImplementation = false;

    // Modified by UseRecordBatchBaseImplementation()
    mutable struct ArrowSchema m_sCachedSchema = {};

    bool SkipToNextFeatureDueToAttributeFilter() const;
    void ExploreExprNode(const swq_expr_node *poNode);
    bool UseRecordBatchBaseImplementation() const;

    template <typename SourceOffset>
    static struct ArrowArray *
    CreateWKBArrayFromWKTArray(const struct ArrowArray *sourceArray);

    int GetArrowSchemaInternal(struct ArrowSchema *out) const;

  protected:
    OGRArrowDataset *m_poArrowDS = nullptr;
    arrow::MemoryPool *m_poMemoryPool = nullptr;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    std::shared_ptr<arrow::Schema> m_poSchema{};
    std::string m_osFIDColumn{};
    int m_iFIDArrowColumn = -1;
    std::vector<std::vector<int>> m_anMapFieldIndexToArrowColumn{};
    std::vector<int> m_anMapGeomFieldIndexToArrowColumn{};
    std::vector<OGRArrowGeomEncoding> m_aeGeomEncoding{};

    //! Describe the bbox column of a geometry column
    struct GeomColBBOX
    {
        bool bIsFloat = false;
        int iArrowCol = -1;
        int iArrayIdx = -1;  // only valid when m_bIgnoredFields == true
        int iArrowSubfieldXMin = -1;
        int iArrowSubfieldYMin = -1;
        int iArrowSubfieldXMax = -1;
        int iArrowSubfieldYMax = -1;
    };

    //! Map from OGR geometry field index to GeomColBBOX
    std::map<int, GeomColBBOX> m_oMapGeomFieldIndexToGeomColBBOX{};

    const arrow::BinaryArray *m_poArrayWKB = nullptr;
    const arrow::LargeBinaryArray *m_poArrayWKBLarge = nullptr;
    const arrow::Array *m_poArrayBBOX = nullptr;
    const arrow::DoubleArray *m_poArrayXMinDouble = nullptr;
    const arrow::DoubleArray *m_poArrayYMinDouble = nullptr;
    const arrow::DoubleArray *m_poArrayXMaxDouble = nullptr;
    const arrow::DoubleArray *m_poArrayYMaxDouble = nullptr;
    const arrow::FloatArray *m_poArrayXMinFloat = nullptr;
    const arrow::FloatArray *m_poArrayYMinFloat = nullptr;
    const arrow::FloatArray *m_poArrayXMaxFloat = nullptr;
    const arrow::FloatArray *m_poArrayYMaxFloat = nullptr;

    //! References values in range [0, m_poSchema->field_count()-1]
    std::set<int> m_oSetBBoxArrowColumns{};

    bool m_bIgnoredFields = false;
    std::vector<int>
        m_anMapFieldIndexToArrayIndex{};  // only valid when m_bIgnoredFields is
                                          // set
    std::vector<int> m_anMapGeomFieldIndexToArrayIndex{};  // only valid when
        // m_bIgnoredFields is set
    int m_nRequestedFIDColumn = -1;  // only valid when m_bIgnoredFields is set

    bool m_bEOF = false;
    int64_t m_nFeatureIdx = 0;
    int64_t m_nIdxInBatch = 0;
    std::map<std::string, CPLJSONObject> m_oMapGeometryColumns{};
    mutable std::map<int, OGREnvelope> m_oMapExtents{};
    int m_iRecordBatch = -1;
    std::shared_ptr<arrow::RecordBatch> m_poBatch{};
    // m_poBatch->columns() is a relatively costly operation, so cache its
    // result
    std::vector<std::shared_ptr<arrow::Array>>
        m_poBatchColumns{};  // must always be == m_poBatch->columns()
    mutable std::shared_ptr<arrow::Array> m_poReadFeatureTmpArray{};

    std::vector<Constraint> m_asAttributeFilterConstraints{};

    std::map<std::string, std::unique_ptr<OGRFieldDefn>>
    LoadGDALSchema(const arrow::KeyValueMetadata *kv_metadata);

    void LoadGDALMetadata(const arrow::KeyValueMetadata *kv_metadata);

    OGRArrowLayer(OGRArrowDataset *poDS, const char *pszLayerName);

    virtual std::string GetDriverUCName() const = 0;
    static bool IsIntegerArrowType(arrow::Type::type typeId);
    static bool
    IsHandledListOrMapType(const std::shared_ptr<arrow::DataType> &valueType);
    static bool
    IsHandledListType(const std::shared_ptr<arrow::BaseListType> &listType);
    static bool
    IsHandledMapType(const std::shared_ptr<arrow::MapType> &mapType);
    static bool
    IsValidGeometryEncoding(const std::shared_ptr<arrow::Field> &field,
                            const std::string &osEncoding,
                            bool bWarnIfUnknownEncoding,
                            OGRwkbGeometryType &eGeomTypeOut,
                            OGRArrowGeomEncoding &eGeomEncodingOut);
    static OGRwkbGeometryType
    GetGeometryTypeFromString(const std::string &osType);
    bool
    MapArrowTypeToOGR(const std::shared_ptr<arrow::DataType> &type,
                      const std::shared_ptr<arrow::Field> &field,
                      OGRFieldDefn &oField, OGRFieldType &eType,
                      OGRFieldSubType &eSubType, const std::vector<int> &path,
                      const std::map<std::string, std::unique_ptr<OGRFieldDefn>>
                          &oMapFieldNameToGDALSchemaFieldDefn);
    void CreateFieldFromSchema(
        const std::shared_ptr<arrow::Field> &field,
        const std::vector<int> &path,
        const std::map<std::string, std::unique_ptr<OGRFieldDefn>>
            &oMapFieldNameToGDALSchemaFieldDefn);
    std::unique_ptr<OGRFieldDomain>
    BuildDomainFromBatch(const std::string &osDomainName,
                         const std::shared_ptr<arrow::RecordBatch> &poBatch,
                         int iCol) const;
    OGRwkbGeometryType ComputeGeometryColumnTypeProcessBatch(
        const std::shared_ptr<arrow::RecordBatch> &poBatch, int iGeomCol,
        int iBatchCol, OGRwkbGeometryType eGeomType) const;
    static bool ReadWKBBoundingBox(const uint8_t *data, size_t size,
                                   OGREnvelope &sEnvelope);
    OGRFeature *ReadFeature(
        int64_t nIdxInBatch,
        const std::vector<std::shared_ptr<arrow::Array>> &poColumnArrays) const;
    OGRGeometry *ReadGeometry(int iGeomField, const arrow::Array *array,
                              int64_t nIdxInBatch) const;
    virtual bool ReadNextBatch() = 0;
    virtual void InvalidateCachedBatches() = 0;
    OGRFeature *GetNextRawFeature();

    virtual bool CanRunNonForcedGetExtent()
    {
        return true;
    }

    void SetBatch(const std::shared_ptr<arrow::RecordBatch> &poBatch);

    // Refreshes Constraint.iArrayIdx from iField. To be called by SetIgnoredFields()
    void ComputeConstraintsArrayIdx();

    virtual bool FastGetExtent(int iGeomField, OGREnvelope *psExtent) const;
    bool FastGetExtent3D(int iGeomField, OGREnvelope3D *psExtent) const;
    static OGRErr GetExtentFromMetadata(const CPLJSONObject &oJSONDef,
                                        OGREnvelope3D *psExtent);

    int GetArrowSchema(struct ArrowArrayStream *,
                       struct ArrowSchema *out) override;
    int GetNextArrowArray(struct ArrowArrayStream *,
                          struct ArrowArray *out) override;

    virtual void IncrFeatureIdx()
    {
        ++m_nFeatureIdx;
    }

  public:
    virtual ~OGRArrowLayer() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    void ResetReading() override;

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRArrowLayer)
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                     int bForce = TRUE) override;
    OGRErr GetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                       int bForce = TRUE) override;
    OGRErr SetAttributeFilter(const char *pszFilter) override;

    void SetSpatialFilter(OGRGeometry *poGeom) override
    {
        SetSpatialFilter(0, poGeom);
    }

    void SetSpatialFilter(int iGeomField, OGRGeometry *poGeom) override;

    int TestCapability(const char *pszCap) override;

    bool GetArrowStream(struct ArrowArrayStream *out_stream,
                        CSLConstList papszOptions = nullptr) override;

    virtual std::unique_ptr<OGRFieldDomain>
    BuildDomain(const std::string &osDomainName, int iFieldIndex) const = 0;

    static void TimestampToOGR(int64_t timestamp,
                               const arrow::TimestampType *timestampType,
                               int nTZFlag, OGRField *psField);
};

/************************************************************************/
/*                         OGRArrowDataset                              */
/************************************************************************/

class OGRArrowDataset CPL_NON_FINAL : public GDALPamDataset
{
    std::shared_ptr<arrow::MemoryPool> m_poMemoryPool{};
    std::unique_ptr<OGRArrowLayer> m_poLayer{};
    std::vector<std::string> m_aosDomainNames{};
    std::map<std::string, int> m_oMapDomainNameToCol{};

  public:
    explicit OGRArrowDataset(
        const std::shared_ptr<arrow::MemoryPool> &poMemoryPool);

    inline arrow::MemoryPool *GetMemoryPool() const
    {
        return m_poMemoryPool.get();
    }

    inline const std::shared_ptr<arrow::MemoryPool> &GetSharedMemoryPool() const
    {
        return m_poMemoryPool;
    }

    void SetLayer(std::unique_ptr<OGRArrowLayer> &&poLayer);

    void RegisterDomainName(const std::string &osDomainName, int iFieldIndex);

    std::vector<std::string> GetFieldDomainNames(
        CSLConstList /*papszOptions*/ = nullptr) const override;
    const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;

    int GetLayerCount() override;
    OGRLayer *GetLayer(int idx) override;
};

/************************************************************************/
/*                        OGRArrowWriterLayer                           */
/************************************************************************/

class OGRArrowWriterLayer CPL_NON_FINAL : public OGRLayer

{
  protected:
    OGRArrowWriterLayer(const OGRArrowWriterLayer &) = delete;
    OGRArrowWriterLayer &operator=(const OGRArrowWriterLayer &) = delete;

    arrow::MemoryPool *m_poMemoryPool = nullptr;
    bool m_bInitializationOK = false;
    std::shared_ptr<arrow::io::OutputStream> m_poOutputStream{};
    std::shared_ptr<arrow::Schema> m_poSchema{};
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    std::map<std::string, std::unique_ptr<OGRFieldDomain>> m_oMapFieldDomains{};
    std::map<std::string, std::shared_ptr<arrow::Array>>
        m_oMapFieldDomainToStringArray{};

    bool m_bWriteFieldArrowExtensionName = false;
    OGRArrowGeomEncoding m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
    std::vector<OGRArrowGeomEncoding> m_aeGeomEncoding{};
    int m_nWKTCoordinatePrecision = -1;

    //! Base struct data type for GeoArrow struct geometry columns.
    // Constraint: if not empty, m_apoBaseStructGeomType.size() == m_poFeatureDefn->GetGeomFieldCount()
    std::vector<std::shared_ptr<arrow::DataType>> m_apoBaseStructGeomType{};

    //! Whether to use a struct field with the values of the bounding box
    // of the geometries. Used by Parquet.
    bool m_bWriteBBoxStruct = false;

    //! Schema fields for bounding box of geometry columns.
    // Constraint: if not empty, m_apoFieldsBBOX.size() == m_poFeatureDefn->GetGeomFieldCount()
    std::vector<std::shared_ptr<arrow::Field>> m_apoFieldsBBOX{};

    //! Array builers for bounding box of geometry columns.
    // m_apoBuildersBBOXStruct is for the top-level field of type struct.
    // m_apoBuildersBBOX{XMin|YMin|XMax|YMax} are for the floating-point values
    // Constraint: if not empty, m_apoBuildersBBOX{Struct|XMin|YMin|XMax|YMax}.size() == m_poFeatureDefn->GetGeomFieldCount()
    std::vector<std::shared_ptr<arrow::StructBuilder>>
        m_apoBuildersBBOXStruct{};
    std::vector<std::shared_ptr<arrow::FloatBuilder>> m_apoBuildersBBOXXMin{};
    std::vector<std::shared_ptr<arrow::FloatBuilder>> m_apoBuildersBBOXYMin{};
    std::vector<std::shared_ptr<arrow::FloatBuilder>> m_apoBuildersBBOXXMax{};
    std::vector<std::shared_ptr<arrow::FloatBuilder>> m_apoBuildersBBOXYMax{};

    std::string m_osFIDColumn{};
    int64_t m_nFeatureCount = 0;

    int64_t m_nRowGroupSize = 64 * 1024;
    arrow::Compression::type m_eCompression = arrow::Compression::UNCOMPRESSED;

    std::vector<std::shared_ptr<arrow::Field>> m_apoFieldsFromArrowSchema{};
    std::vector<std::shared_ptr<arrow::ArrayBuilder>> m_apoBuilders{};

    std::vector<uint8_t> m_abyBuffer{};

    std::vector<int> m_anTZFlag{};               // size: GetFieldCount()
    std::vector<OGREnvelope3D> m_aoEnvelopes{};  // size: GetGeomFieldCount()
    std::vector<std::set<OGRwkbGeometryType>>
        m_oSetWrittenGeometryTypes{};  // size: GetGeomFieldCount()

    static OGRArrowGeomEncoding
    GetPreciseArrowGeomEncoding(OGRArrowGeomEncoding eEncodingType,
                                OGRwkbGeometryType eGType);
    static const char *
    GetGeomEncodingAsString(OGRArrowGeomEncoding eGeomEncoding,
                            bool bForParquetGeo);

    virtual bool IsSupportedGeometryType(OGRwkbGeometryType eGType) const = 0;

    virtual std::string GetDriverUCName() const = 0;

    virtual bool IsFileWriterCreated() const = 0;
    virtual void CreateWriter() = 0;
    virtual bool CloseFileWriter() = 0;

    void CreateSchemaCommon();
    void FinalizeSchema();
    virtual void CreateSchema() = 0;

    virtual void PerformStepsBeforeFinalFlushGroup()
    {
    }

    void CreateArrayBuilders();

    //! Clear array builders
    void ClearArrayBuilers();

    virtual bool FlushGroup() = 0;
    bool FinalizeWriting();
    bool WriteArrays(std::function<bool(const std::shared_ptr<arrow::Field> &,
                                        const std::shared_ptr<arrow::Array> &)>
                         postProcessArray);

    virtual void FixupWKBGeometryBeforeWriting(GByte * /*pabyWKB*/,
                                               size_t /*nLen*/)
    {
    }

    virtual void FixupGeometryBeforeWriting(OGRGeometry * /* poGeom */)
    {
    }

    virtual bool IsSRSRequired() const = 0;
    bool WriteArrowBatchInternal(
        const struct ArrowSchema *schema, struct ArrowArray *array,
        CSLConstList papszOptions,
        std::function<bool(const std::shared_ptr<arrow::RecordBatch> &)>
            writeBatch);

    OGRErr BuildGeometry(OGRGeometry *poGeom, int iGeomField,
                         arrow::ArrayBuilder *poBuilder);

  public:
    OGRArrowWriterLayer(
        arrow::MemoryPool *poMemoryPool,
        const std::shared_ptr<arrow::io::OutputStream> &poOutputStream,
        const char *pszLayerName);

    ~OGRArrowWriterLayer() override;

    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason);
    std::vector<std::string> GetFieldDomainNames() const;
    const OGRFieldDomain *GetFieldDomain(const std::string &name) const;

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    void ResetReading() override
    {
    }

    OGRFeature *GetNextFeature() override
    {
        return nullptr;
    }

    int TestCapability(const char *pszCap) override;
    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    OGRErr CreateGeomField(const OGRGeomFieldDefn *poField,
                           int bApproxOK = TRUE) override;
    GIntBig GetFeatureCount(int bForce) override;

    bool IsArrowSchemaSupported(const struct ArrowSchema * /*schema*/,
                                CSLConstList /* papszOptions */,
                                std::string & /*osErrorMsg */) const override
    {
        return true;
    }

    bool
    CreateFieldFromArrowSchema(const struct ArrowSchema *schema,
                               CSLConstList papszOptions = nullptr) override;
    bool WriteArrowBatch(const struct ArrowSchema *schema,
                         struct ArrowArray *array,
                         CSLConstList papszOptions = nullptr) override = 0;

  protected:
    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    bool FlushFeatures();
};

#endif  // OGR_ARROW_H
