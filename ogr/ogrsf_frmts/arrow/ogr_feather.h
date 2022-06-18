/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
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

#ifndef OGR_FEATHER_H
#define OGR_FEATHER_H

#include "ogrsf_frmts.h"

#include <map>

#include "../arrow_common/ogr_arrow.h"

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244 )  /*  warning 4244: 'initializing': conversion from 'int32_t' to 'int16_t', possible loss of data */
#pragma warning( disable : 4458 )  /*  warning 4458: declaration of 'type_id' hides class member */
#endif

#include "arrow/ipc/writer.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif

constexpr const char* GDAL_GEO_FOOTER_KEY  = "gdal:geo";
constexpr const char* ARROW_DRIVER_NAME_UC = "ARROW";

/************************************************************************/
/*                        OGRFeatherLayer                               */
/************************************************************************/

class OGRFeatherDataset;

class OGRFeatherLayer final: public OGRArrowLayer

{
        OGRFeatherLayer(const OGRFeatherLayer&) = delete;
        OGRFeatherLayer& operator= (const OGRFeatherLayer&) = delete;

        OGRFeatherDataset*                          m_poDS = nullptr;

        // Variable only for seekable file format
        std::shared_ptr<arrow::ipc::RecordBatchFileReader> m_poRecordBatchFileReader{};

        // Variables only for streamable IPC format
        std::shared_ptr<arrow::io::RandomAccessFile> m_poFile{};
        bool                                         m_bSeekable = true;
        arrow::ipc::IpcReadOptions                   m_oOptions{};
        std::shared_ptr<arrow::ipc::RecordBatchStreamReader>   m_poRecordBatchReader{};
        bool                                        m_bResetRecordBatchReaderAsked = false;
        bool                                        m_bSingleBatch = false;
        std::shared_ptr<arrow::RecordBatch>         m_poBatchIdx0{};
        std::shared_ptr<arrow::RecordBatch>         m_poBatchIdx1{};

        CPLStringList                               m_aosFeatherMetadata{};

        virtual std::string GetDriverUCName() const override { return ARROW_DRIVER_NAME_UC; }

        bool               ResetRecordBatchReader();

        void               EstablishFeatureDefn();
        void               LoadGeoMetadata(const arrow::KeyValueMetadata* kv_metadata,
                                           const std::string& key);
        OGRwkbGeometryType ComputeGeometryColumnType(int iGeomCol, int iCol) const;
        bool               ReadNextBatch() override;
        OGRFeature*        GetNextRawFeature();

        virtual bool       CanRunNonForcedGetExtent() override;

        bool               ReadNextBatchFile();
        bool               ReadNextBatchStream();
        void               TryToCacheFirstTwoBatches();

public:
        OGRFeatherLayer(OGRFeatherDataset* poDS,
                        const char* pszLayerName,
                        std::shared_ptr<arrow::ipc::RecordBatchFileReader>& poRecordBatchFileReader);
        OGRFeatherLayer(OGRFeatherDataset* poDS,
                        const char* pszLayerName,
                        std::shared_ptr<arrow::io::RandomAccessFile> poFile,
                        bool bSeekable,
                        const arrow::ipc::IpcReadOptions& oOptions,
                        std::shared_ptr<arrow::ipc::RecordBatchStreamReader>& poRecordBatchStreamReader);

        void            ResetReading() override;
        int             TestCapability(const char* pszCap) override;
        GIntBig         GetFeatureCount(int bForce) override;
        const char*     GetMetadataItem( const char* pszName,
                                         const char* pszDomain = "" ) override;
        char**          GetMetadata( const char* pszDomain = "" ) override;

        GDALDataset*    GetDataset() override;

        std::unique_ptr<OGRFieldDomain> BuildDomain(const std::string& osDomainName,
                                                    int iFieldIndex) const override;
};

/************************************************************************/
/*                         OGRFeatherDataset                            */
/************************************************************************/

class OGRFeatherDataset final: public OGRArrowDataset
{
public:
    explicit OGRFeatherDataset(const std::shared_ptr<arrow::MemoryPool>& poMemoryPool);
};

/************************************************************************/
/*                        OGRFeatherWriterLayer                         */
/************************************************************************/

class OGRFeatherWriterLayer final: public OGRArrowWriterLayer

{
        OGRFeatherWriterLayer(const OGRFeatherWriterLayer&) = delete;
        OGRFeatherWriterLayer& operator= (const OGRFeatherWriterLayer&) = delete;

        bool                                             m_bStreamFormat = false;
        std::shared_ptr<arrow::ipc::RecordBatchWriter>   m_poFileWriter{};
        std::shared_ptr<arrow::KeyValueMetadata>         m_poFooterKeyValueMetadata{};

        virtual bool            IsFileWriterCreated() const override { return m_poFileWriter != nullptr; }
        virtual void            CreateWriter() override;
        virtual void            CloseFileWriter() override;

        virtual void            CreateSchema() override;
        virtual void            PerformStepsBeforeFinalFlushGroup() override;

        virtual bool            FlushGroup() override;

        virtual std::string GetDriverUCName() const override { return ARROW_DRIVER_NAME_UC; }

        virtual bool            IsSupportedGeometryType(OGRwkbGeometryType eGType) const override;
        virtual bool            IsSRSRequired() const override { return true; }

public:
        OGRFeatherWriterLayer( arrow::MemoryPool* poMemoryPool,
                               const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
                               const char *pszLayerName );

        ~OGRFeatherWriterLayer() override;

        bool            SetOptions( const std::string& osFilename,
                                    CSLConstList papszOptions,
                                    OGRSpatialReference *poSpatialRef,
                                    OGRwkbGeometryType eGType );
};

/************************************************************************/
/*                        OGRFeatherWriterDataset                       */
/************************************************************************/

class OGRFeatherWriterDataset final: public GDALPamDataset
{
    const std::string                        m_osFilename{};
    std::unique_ptr<arrow::MemoryPool>       m_poMemoryPool{};
    std::unique_ptr<OGRFeatherWriterLayer>   m_poLayer{};
    std::shared_ptr<arrow::io::OutputStream> m_poOutputStream{};

public:
    explicit OGRFeatherWriterDataset(
        const char* pszFilename,
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

#endif // OGR_FEATHER_H
