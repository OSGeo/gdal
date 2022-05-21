/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
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

#ifndef OGR_PARQUET_H
#define OGR_PARQUET_H

#include "ogrsf_frmts.h"

#include <functional>
#include <map>

#include "../arrow_common/ogr_arrow.h"
#include "ogr_include_parquet.h"

/************************************************************************/
/*                       OGRParquetLayerBase                            */
/************************************************************************/

class OGRParquetDataset;

class OGRParquetLayerBase CPL_NON_FINAL: public OGRArrowLayer
{
        OGRParquetLayerBase(const OGRParquetLayerBase&) = delete;
        OGRParquetLayerBase& operator= (const OGRParquetLayerBase&) = delete;

protected:
                            OGRParquetLayerBase(OGRParquetDataset* poDS,
                                                const char* pszLayerName);

        OGRParquetDataset*                          m_poDS = nullptr;
        std::shared_ptr<arrow::RecordBatchReader>   m_poRecordBatchReader{};

        void                LoadGeoMetadata(const std::shared_ptr<const arrow::KeyValueMetadata>& kv_metadata);
        bool                DealWithGeometryColumn(int iFieldIdx,
                                                   const std::shared_ptr<arrow::Field>& field,
                                                   std::function<OGRwkbGeometryType(void)> computeGeometryTypeFun);
public:
        int             TestCapability(const char* ) override;
};

/************************************************************************/
/*                        OGRParquetLayer                               */
/************************************************************************/

class OGRParquetLayer final: public OGRParquetLayerBase

{
        std::unique_ptr<parquet::arrow::FileReader> m_poArrowReader{};
        bool                                        m_bSingleBatch = false;
        int                                         m_iFIDParquetColumn = -1;
        std::vector<std::shared_ptr<arrow::DataType>> m_apoArrowDataTypes{}; // .size() == field ocunt
        std::vector<int>                            m_anMapFieldIndexToParquetColumn{};
        std::vector<int>                            m_anMapGeomFieldIndexToParquetColumn{};
        bool                                        m_bHasMissingMappingToParquet = false;

        std::vector<int>                            m_anRequestedParquetColumns{}; // only valid when m_bIgnoredFields is set
#ifdef DEBUG
        int                                         m_nExpectedBatchColumns = 0; // Should be equal to m_poBatch->num_columns() (when m_bIgnoredFields is set)
#endif
        CPLStringList                               m_aosFeatherMetadata{};

        void               EstablishFeatureDefn();
        bool               ReadNextBatch() override;
        OGRwkbGeometryType ComputeGeometryColumnType(int iGeomCol, int iParquetCol) const;
        void               CreateFieldFromSchema(
                               const std::shared_ptr<arrow::Field>& field,
                               bool bParquetColValid,
                               int &iParquetCol,
                               const std::vector<int>& path,
                               const std::map<std::string, std::unique_ptr<OGRFieldDefn>>& oMapFieldNameToGDALSchemaFieldDefn);
        bool               CheckMatchArrowParquetColumnNames(
                               int& iParquetCol,
                               const std::shared_ptr<arrow::Field>& field) const;
        OGRFeature*        GetFeatureExplicitFID(GIntBig nFID);
        OGRFeature*        GetFeatureByIndex(GIntBig nFID);

        virtual std::string GetDriverUCName() const override { return "PARQUET"; }

public:
        OGRParquetLayer(OGRParquetDataset* poDS,
                        const char* pszLayerName,
                        std::unique_ptr<parquet::arrow::FileReader>&& arrow_reader);

        void            ResetReading() override;
        OGRFeature     *GetFeature(GIntBig nFID) override;
        GIntBig         GetFeatureCount(int bForce) override;
        int             TestCapability(const char* pszCap) override;
        OGRErr          SetIgnoredFields( const char **papszFields ) override;
        const char*     GetMetadataItem( const char* pszName,
                                         const char* pszDomain = "" ) override;
        char**          GetMetadata( const char* pszDomain = "" ) override;

        std::unique_ptr<OGRFieldDomain> BuildDomain(const std::string& osDomainName,
                                                    int iFieldIndex) const override;

        parquet::arrow::FileReader* GetReader() const { return m_poArrowReader.get(); }
        const std::vector<int>& GetMapFieldIndexToParquetColumn() const { return m_anMapFieldIndexToParquetColumn; }
        const std::vector<std::shared_ptr<arrow::DataType>>& GetArrowFieldTypes() const { return m_apoArrowDataTypes; }
};

/************************************************************************/
/*                      OGRParquetDatasetLayer                          */
/************************************************************************/

#ifdef GDAL_USE_ARROWDATASET

class OGRParquetDatasetLayer final: public OGRParquetLayerBase
{
        std::shared_ptr<arrow::dataset::Scanner> m_poScanner{};

        void               EstablishFeatureDefn(const std::shared_ptr<arrow::Schema>& schema);

protected:
        std::string        GetDriverUCName() const override { return "PARQUET"; }
        bool               ReadNextBatch() override;

public:
                           OGRParquetDatasetLayer(
                               OGRParquetDataset* poDS,
                               const char* pszLayerName,
                               const std::shared_ptr<arrow::dataset::Scanner>& scanner,
                               const std::shared_ptr<arrow::Schema>& schema);

        void               ResetReading() override;
        GIntBig            GetFeatureCount(int bForce) override;

        // TODO
        std::unique_ptr<OGRFieldDomain> BuildDomain(const std::string& /*osDomainName*/,
                                                    int /*iFieldIndex*/) const override { return nullptr; }
};

#endif

/************************************************************************/
/*                         OGRParquetDataset                            */
/************************************************************************/

class OGRParquetDataset final: public OGRArrowDataset
{
public:
    explicit OGRParquetDataset(std::unique_ptr<arrow::MemoryPool>&& poMemoryPool);

    OGRLayer*            ExecuteSQL( const char *pszSQLCommand,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect ) override;
    void                 ReleaseResultSet( OGRLayer * poResultsSet ) override;
};

/************************************************************************/
/*                        OGRParquetWriterLayer                         */
/************************************************************************/

class OGRParquetWriterLayer final: public OGRArrowWriterLayer
{
        OGRParquetWriterLayer(const OGRParquetWriterLayer&) = delete;
        OGRParquetWriterLayer& operator= (const OGRParquetWriterLayer&) = delete;

        std::unique_ptr<parquet::arrow::FileWriter> m_poFileWriter{};
        std::shared_ptr<const arrow::KeyValueMetadata> m_poKeyValueMetadata{};
        bool                                           m_bForceCounterClockwiseOrientation = false;
        bool                                           m_bEdgesSpherical = false;
        parquet::WriterProperties::Builder             m_oWriterPropertiesBuilder{};

        virtual bool            IsFileWriterCreated() const override { return m_poFileWriter != nullptr; }
        virtual void            CreateWriter() override;
        virtual void            CloseFileWriter() override;

        virtual void            CreateSchema() override;
        virtual void            PerformStepsBeforeFinalFlushGroup() override;

        virtual bool            FlushGroup() override;

        virtual std::string GetDriverUCName() const override { return "PARQUET"; }

        virtual bool            IsSupportedGeometryType(OGRwkbGeometryType eGType) const override;

        virtual void            FixupGeometryBeforeWriting(OGRGeometry* poGeom) override;
        virtual bool            IsSRSRequired() const override { return false; }

public:
        OGRParquetWriterLayer( arrow::MemoryPool* poMemoryPool,
                               const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
                               const char *pszLayerName );

        ~OGRParquetWriterLayer() override;

        bool            SetOptions( CSLConstList papszOptions,
                                    OGRSpatialReference *poSpatialRef,
                                    OGRwkbGeometryType eGType );

        OGRErr          CreateGeomField( OGRGeomFieldDefn *poField, int bApproxOK = TRUE ) override;
};

/************************************************************************/
/*                        OGRParquetWriterDataset                       */
/************************************************************************/

class OGRParquetWriterDataset final: public GDALPamDataset
{
    std::unique_ptr<arrow::MemoryPool>       m_poMemoryPool{};
    std::unique_ptr<OGRParquetWriterLayer>   m_poLayer{};
    std::shared_ptr<arrow::io::OutputStream> m_poOutputStream{};

public:
    explicit OGRParquetWriterDataset(
        const std::shared_ptr<arrow::io::OutputStream>& poOutputStream);

    arrow::MemoryPool* GetMemoryPool() const { return m_poMemoryPool.get(); }

    int       GetLayerCount() override ;
    OGRLayer* GetLayer(int idx) override;
    int       TestCapability(const char* pszCap) override;
    std::vector<std::string> GetFieldDomainNames(CSLConstList /*papszOptions*/ = nullptr) const override;
    const OGRFieldDomain* GetFieldDomain(const std::string& name) const override;
    bool       AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                              std::string& failureReason) override;
protected:
    OGRLayer   *ICreateLayer( const char *pszName,
                              OGRSpatialReference *poSpatialRef = nullptr,
                              OGRwkbGeometryType eGType = wkbUnknown,
                              char ** papszOptions = nullptr ) override;

};

#endif // OGR_PARQUET_H
