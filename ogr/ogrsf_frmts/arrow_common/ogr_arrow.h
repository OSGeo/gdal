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
    GEOARROW_GENERIC, // only used by OGRArrowWriterLayer::m_eGeomEncoding
    GEOARROW_POINT,
    GEOARROW_LINESTRING,
    GEOARROW_POLYGON,
    GEOARROW_MULTIPOINT,
    GEOARROW_MULTILINESTRING,
    GEOARROW_MULTIPOLYGON,
};

/************************************************************************/
/*                         OGRArrowLayer                                */
/************************************************************************/

class OGRArrowDataset;

class OGRArrowLayer CPL_NON_FINAL: public OGRLayer,
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
            int          iField{};
            int          iArrayIdx{};
            int          nOperation{};
            Type         eType{};
            OGRField     sValue{};
            std::string  osValue{};
        };

private:
        OGRArrowLayer(const OGRArrowLayer&) = delete;
        OGRArrowLayer& operator= (const OGRArrowLayer&) = delete;

        std::vector<Constraint>  m_asAttributeFilterConstraints{};
        int                      m_nUseOptimizedAttributeFilter = -1;
        bool                     SkipToNextFeatureDueToAttributeFilter() const;
        void                     ExploreExprNode(const swq_expr_node* poNode);

protected:
        OGRArrowDataset*                            m_poArrowDS = nullptr;
        arrow::MemoryPool*                          m_poMemoryPool = nullptr;
        OGRFeatureDefn*                             m_poFeatureDefn = nullptr;
        std::shared_ptr<arrow::Schema>              m_poSchema{};
        std::string                                 m_osFIDColumn{};
        int                                         m_iFIDArrowColumn = -1;
        std::vector<std::vector<int>>               m_anMapFieldIndexToArrowColumn{};
        std::vector<int>                            m_anMapGeomFieldIndexToArrowColumn{};
        std::vector<OGRArrowGeomEncoding>           m_aeGeomEncoding{};

        bool                                        m_bIgnoredFields = false;
        std::vector<int>                            m_anMapFieldIndexToArrayIndex{}; // only valid when m_bIgnoredFields is set
        std::vector<int>                            m_anMapGeomFieldIndexToArrayIndex{}; // only valid when m_bIgnoredFields is set
        int                                         m_nRequestedFIDColumn = -1; // only valid when m_bIgnoredFields is set

        bool                                        m_bEOF = false;
        int64_t                                     m_nFeatureIdx = 0;
        int64_t                                     m_nIdxInBatch = 0;
        std::map<std::string, CPLJSONObject>        m_oMapGeometryColumns{};
        int                                         m_iRecordBatch = -1;
        std::shared_ptr<arrow::RecordBatch>         m_poBatch{};
        // m_poBatch->columns() is a relatively costly operation, so cache its result
        std::vector<std::shared_ptr<arrow::Array>>  m_poBatchColumns{}; // must always be == m_poBatch->columns()
        mutable std::shared_ptr<arrow::Array>       m_poReadFeatureTmpArray{};

        std::map<std::string, std::unique_ptr<OGRFieldDefn>> LoadGDALMetadata(const arrow::KeyValueMetadata* kv_metadata);

                    OGRArrowLayer(OGRArrowDataset* poDS, const char* pszLayerName);

        virtual std::string GetDriverUCName() const = 0;
        static bool IsIntegerArrowType(arrow::Type::type typeId);
        static bool IsValidGeometryEncoding(const std::shared_ptr<arrow::Field>& field,
                                            const std::string& osEncoding,
                                            OGRwkbGeometryType& eGeomTypeOut,
                                            OGRArrowGeomEncoding& eGeomEncodingOut);
        static OGRwkbGeometryType GetGeometryTypeFromString(const std::string& osType);
        bool        MapArrowTypeToOGR(const std::shared_ptr<arrow::DataType>& type,
                                      const std::shared_ptr<arrow::Field>& field,
                                      OGRFieldDefn& oField,
                                      OGRFieldType& eType,
                                      OGRFieldSubType& eSubType,
                                      const std::vector<int>& path,
                                      const std::map<std::string, std::unique_ptr<OGRFieldDefn>>& oMapFieldNameToGDALSchemaFieldDefn);
        void               CreateFieldFromSchema(
                               const std::shared_ptr<arrow::Field>& field,
                               const std::vector<int>& path,
                               const std::map<std::string, std::unique_ptr<OGRFieldDefn>>& oMapFieldNameToGDALSchemaFieldDefn);
        std::unique_ptr<OGRFieldDomain> BuildDomainFromBatch(
                                    const std::string& osDomainName,
                                    const std::shared_ptr<arrow::RecordBatch>& poBatch,
                                    int iCol) const;
        OGRwkbGeometryType ComputeGeometryColumnTypeProcessBatch(
            const std::shared_ptr<arrow::RecordBatch>& poBatch,
            int iGeomCol, int iBatchCol,
            OGRwkbGeometryType eGeomType) const;
        static bool        ReadWKBBoundingBox(const uint8_t* data, size_t size, OGREnvelope& sEnvelope);
        OGRFeature*        ReadFeature(int64_t nIdxInBatch,
                                       const std::vector<std::shared_ptr<arrow::Array>>& poColumnArrays) const;
        OGRGeometry* ReadGeometry(int iGeomField,
                                  const arrow::Array* array,
                                  int64_t nIdxInBatch) const;
        virtual bool       ReadNextBatch() = 0;
        OGRFeature*        GetNextRawFeature();

        virtual bool       CanRunNonForcedGetExtent() { return true; }

        void               SetBatch(const std::shared_ptr<arrow::RecordBatch>& poBatch) { m_poBatch = poBatch; m_poBatchColumns = m_poBatch->columns(); }

public:
        virtual ~OGRArrowLayer() override;

        OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }
        void            ResetReading() override;
        const char*     GetFIDColumn() override { return m_osFIDColumn.c_str(); }
        DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRArrowLayer)
        OGRErr          GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
        OGRErr          GetExtent(int iGeomField, OGREnvelope *psExtent,
                                  int bForce = TRUE) override;
        OGRErr          SetAttributeFilter( const char* pszFilter ) override;

        virtual std::unique_ptr<OGRFieldDomain> BuildDomain(const std::string& osDomainName,
                                                             int iFieldIndex) const = 0;

        static void TimestampToOGR(int64_t timestamp,
                                   const arrow::TimestampType* timestampType,
                                   OGRField* psField);
};

/************************************************************************/
/*                         OGRArrowDataset                              */
/************************************************************************/

class OGRArrowDataset CPL_NON_FINAL: public GDALPamDataset
{
    std::unique_ptr<arrow::MemoryPool> m_poMemoryPool{};
    std::unique_ptr<OGRArrowLayer>     m_poLayer{};
    std::vector<std::string>           m_aosDomainNames{};
    std::map<std::string, int>         m_oMapDomainNameToCol{};

public:
    explicit OGRArrowDataset(std::unique_ptr<arrow::MemoryPool>&& poMemoryPool);

    inline arrow::MemoryPool* GetMemoryPool() const { return m_poMemoryPool.get(); }
    void SetLayer(std::unique_ptr<OGRArrowLayer>&& poLayer);

    void RegisterDomainName(const std::string& osDomainName, int iFieldIndex);

    std::vector<std::string> GetFieldDomainNames(CSLConstList /*papszOptions*/ = nullptr) const override;
    const OGRFieldDomain* GetFieldDomain(const std::string& name) const override;

    int GetLayerCount() override ;
    OGRLayer* GetLayer(int idx) override;
};

/************************************************************************/
/*                        OGRArrowWriterLayer                           */
/************************************************************************/

class OGRArrowWriterLayer CPL_NON_FINAL: public OGRLayer

{
protected:
        OGRArrowWriterLayer(const OGRArrowWriterLayer&) = delete;
        OGRArrowWriterLayer& operator= (const OGRArrowWriterLayer&) = delete;

        arrow::MemoryPool*                          m_poMemoryPool = nullptr;
        bool                                        m_bInitializationOK = false;
        std::shared_ptr<arrow::io::OutputStream>    m_poOutputStream{};
        std::shared_ptr<arrow::Schema>              m_poSchema{};
        OGRFeatureDefn*                             m_poFeatureDefn = nullptr;
        std::map<std::string, std::unique_ptr<OGRFieldDomain>> m_oMapFieldDomains{};
        std::map<std::string, std::shared_ptr<arrow::Array>>   m_oMapFieldDomainToStringArray{};

        bool                                        m_bWriteFieldArrowExtensionName = false;
        OGRArrowGeomEncoding                        m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
        std::vector<OGRArrowGeomEncoding>           m_aeGeomEncoding{};

        std::string                                 m_osFIDColumn{};
        int64_t                                     m_nFeatureCount = 0;

        int64_t                                     m_nRowGroupSize = 64 * 1024;
        arrow::Compression::type                    m_eCompression = arrow::Compression::UNCOMPRESSED;

        std::vector<std::shared_ptr<arrow::ArrayBuilder>> m_apoBuilders{};

        std::vector<uint8_t>                        m_abyBuffer{};

        std::vector<int>                            m_anTZFlag{};    // size: GetFieldCount()
        std::vector<OGREnvelope>                    m_aoEnvelopes{}; // size: GetGeomFieldCount()
        std::vector<std::set<OGRwkbGeometryType>>   m_oSetWrittenGeometryTypes{}; // size: GetGeomFieldCount()

        static OGRArrowGeomEncoding GetPreciseArrowGeomEncoding(
                                                    OGRwkbGeometryType eGType);
        static const char*      GetGeomEncodingAsString(
                                    OGRArrowGeomEncoding eGeomEncoding);

        virtual bool            IsSupportedGeometryType(OGRwkbGeometryType eGType) const = 0;

        virtual std::string GetDriverUCName() const = 0;

        virtual bool            IsFileWriterCreated() const = 0;
        virtual void            CreateWriter() = 0;
        virtual void            CloseFileWriter() = 0;

        void                    CreateSchemaCommon();
        void                    FinalizeSchema();
        virtual void            CreateSchema() = 0;
        virtual void            PerformStepsBeforeFinalFlushGroup() {}

        void                    CreateArrayBuilders();
        virtual bool            FlushGroup() = 0;
        void                    FinalizeWriting();
        bool                    WriteArrays(std::function<bool(const std::shared_ptr<arrow::Field>&,
                                                               const std::shared_ptr<arrow::Array>&)> postProcessArray);

        virtual void            FixupGeometryBeforeWriting(OGRGeometry* /* poGeom */ ) {}
        virtual bool            IsSRSRequired() const = 0;

public:
        OGRArrowWriterLayer( arrow::MemoryPool* poMemoryPool,
                               const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
                               const char *pszLayerName );

        ~OGRArrowWriterLayer() override;

        bool            AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                       std::string& failureReason);
        std::vector<std::string> GetFieldDomainNames() const;
        const OGRFieldDomain* GetFieldDomain(const std::string& name) const;

        OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }
        void            ResetReading() override {}
        OGRFeature     *GetNextFeature() override { return nullptr; }
        int             TestCapability(const char* pszCap) override;
        OGRErr          CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE ) override;
        OGRErr          CreateGeomField( OGRGeomFieldDefn *poField, int bApproxOK = TRUE ) override;
        GIntBig         GetFeatureCount(int bForce) override;

protected:
        OGRErr          ICreateFeature( OGRFeature* poFeature ) override;
};


#endif // OGR_ARROW_H
