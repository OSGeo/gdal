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

#include "ogr_feather.h"

#include "../arrow_common/ograrrowwriterlayer.hpp"

/************************************************************************/
/*                      OGRFeatherWriterLayer()                         */
/************************************************************************/

OGRFeatherWriterLayer::OGRFeatherWriterLayer(
            arrow::MemoryPool* poMemoryPool,
            const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
            const char *pszLayerName):
    OGRArrowWriterLayer(poMemoryPool, poOutputStream, pszLayerName)
{
    m_bWriteFieldArrowExtensionName = true;
}

/************************************************************************/
/*                     ~OGRFeatherWriterLayer()                         */
/************************************************************************/

OGRFeatherWriterLayer::~OGRFeatherWriterLayer()
{
    if( m_bInitializationOK )
        FinalizeWriting();
}

/************************************************************************/
/*                       IsSupportedGeometryType()                      */
/************************************************************************/

bool OGRFeatherWriterLayer::IsSupportedGeometryType(OGRwkbGeometryType eGType) const
{
    if( eGType != wkbFlatten(eGType) )
    {
        const auto osConfigOptionName = "OGR_" + GetDriverUCName() + "_ALLOW_ALL_DIMS";
        if( !CPLTestBool(CPLGetConfigOption(osConfigOptionName.c_str(), "NO")) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only 2D geometry types are supported (unless the "
                     "%s configuration option is set to YES)",
                     osConfigOptionName.c_str());
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           SetOptions()                               */
/************************************************************************/

bool OGRFeatherWriterLayer::SetOptions(const std::string& osFilename,
                                       CSLConstList papszOptions,
                                       OGRSpatialReference *poSpatialRef,
                                       OGRwkbGeometryType eGType)
{
    const char* pszDefaultFormat =
        (EQUAL(CPLGetExtension(osFilename.c_str()), "arrows") ||
         STARTS_WITH_CI(osFilename.c_str(), "/vsistdout")) ? "STREAM" : "FILE";
    m_bStreamFormat = EQUAL(
        CSLFetchNameValueDef(papszOptions, "FORMAT", pszDefaultFormat), "STREAM");

    const char* pszGeomEncoding = CSLFetchNameValue(papszOptions, "GEOMETRY_ENCODING");
    m_eGeomEncoding = OGRArrowGeomEncoding::GEOARROW_GENERIC;
    if( pszGeomEncoding )
    {
        if( EQUAL(pszGeomEncoding, "WKB") )
            m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
        else if( EQUAL(pszGeomEncoding, "WKT") )
            m_eGeomEncoding = OGRArrowGeomEncoding::WKT;
        else if( EQUAL(pszGeomEncoding, "GEOARROW") )
            m_eGeomEncoding = OGRArrowGeomEncoding::GEOARROW_GENERIC;
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported GEOMETRY_ENCODING = %s",
                     pszGeomEncoding);
            return false;
        }
    }

    if( eGType != wkbNone )
    {
        if( !IsSupportedGeometryType(eGType) )
        {
            return false;
        }

        if( poSpatialRef == nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry column should have an associated CRS");
        }

        m_poFeatureDefn->SetGeomType(eGType);
        auto eGeomEncoding = m_eGeomEncoding;
        if( eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_GENERIC )
        {
            eGeomEncoding = GetPreciseArrowGeomEncoding(eGType);
            if( eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_GENERIC )
                return false;
        }
        m_aeGeomEncoding.push_back(eGeomEncoding);
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(
            CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry"));
        if( poSpatialRef )
        {
            auto poSRS = poSpatialRef->Clone();
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Release();
        }
    }

    m_osFIDColumn = CSLFetchNameValueDef(papszOptions, "FID", "");

    const char* pszCompression = CSLFetchNameValue(
        papszOptions, "COMPRESSION");
    if( pszCompression == nullptr )
    {
        auto oResult = arrow::util::Codec::GetCompressionType("lz4");
        if( oResult.ok() && arrow::util::Codec::IsAvailable(*oResult) )
        {
            pszCompression = "LZ4";
        }
        else
        {
            pszCompression = "NONE";
        }
    }

    if( EQUAL(pszCompression, "NONE") )
        pszCompression = "UNCOMPRESSED";
    auto oResult = arrow::util::Codec::GetCompressionType(
                        CPLString(pszCompression).tolower());
    if( !oResult.ok() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unrecognized compression method: %s", pszCompression);
        return false;
    }
    m_eCompression = *oResult;
    if( !arrow::util::Codec::IsAvailable(m_eCompression) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Compression method %s is known, but libarrow has not "
                 "been built with support for it", pszCompression);
        return false;
    }

    const char* pszRowGroupSize = CSLFetchNameValue(papszOptions, "BATCH_SIZE");
    if( pszRowGroupSize )
    {
        auto nRowGroupSize = static_cast<int64_t>(atoll(pszRowGroupSize));
        if( nRowGroupSize > 0 )
        {
            if( nRowGroupSize > INT_MAX )
                nRowGroupSize = INT_MAX;
            m_nRowGroupSize = nRowGroupSize;
        }
    }

    m_bInitializationOK = true;
    return true;
}

/************************************************************************/
/*                         CloseFileWriter()                            */
/************************************************************************/

void OGRFeatherWriterLayer::CloseFileWriter()
{
    auto status = m_poFileWriter->Close();
    if( !status.ok() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FileWriter::Close() failed with %s",
                 status.message().c_str());
    }
}

/************************************************************************/
/*                          CreateSchema()                              */
/************************************************************************/

void OGRFeatherWriterLayer::CreateSchema()
{
    CreateSchemaCommon();

    if( m_poFeatureDefn->GetGeomFieldCount() != 0 &&
        CPLTestBool(CPLGetConfigOption("OGR_ARROW_WRITE_GEO", "YES")) )
    {
        CPLJSONObject oRoot;
        oRoot.Add("schema_version", "0.1.0");
        oRoot.Add("primary_column",
                  m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        CPLJSONObject oColumns;
        oRoot.Add("columns", oColumns);
        for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i )
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            CPLJSONObject oColumn;
            oColumns.Add(poGeomFieldDefn->GetNameRef(), oColumn);
            oColumn.Add("encoding",
                        GetGeomEncodingAsString(m_aeGeomEncoding[i]));

            const auto poSRS = poGeomFieldDefn->GetSpatialRef();
            if( poSRS )
            {
                const char* const apszOptions[] = {
                    "FORMAT=WKT2_2019", "MULTILINE=NO", nullptr };
                char* pszWKT = nullptr;
                poSRS->exportToWkt(&pszWKT, apszOptions);
                if( pszWKT )
                    oColumn.Add("crs", pszWKT);
                CPLFree(pszWKT);

                const double dfCoordEpoch = poSRS->GetCoordinateEpoch();
                if( dfCoordEpoch > 0 )
                    oColumn.Add("epoch", dfCoordEpoch);
            }

#if 0
            if( m_aoEnvelopes[i].IsInit() &&
                CPLTestBool(CPLGetConfigOption(
                    "OGR_ARROW_WRITE_BBOX", "YES")) )
            {
                CPLJSONArray oBBOX;
                oBBOX.Add(m_aoEnvelopes[i].MinX);
                oBBOX.Add(m_aoEnvelopes[i].MinY);
                oBBOX.Add(m_aoEnvelopes[i].MaxX);
                oBBOX.Add(m_aoEnvelopes[i].MaxY);
                oColumn.Add("bbox", oBBOX);
            }
#endif
            const auto eType = poGeomFieldDefn->GetType();
            if( CPLTestBool(CPLGetConfigOption(
                    "OGR_ARROW_WRITE_GDAL_GEOMETRY_TYPE", "YES")) &&
                eType == wkbFlatten(eType) )
            {
                // Geometry type, place under a temporary "gdal:geometry_type" property
                // pending acceptance of proposal at
                // https://github.com/opengeospatial/geoparquet/issues/41
                const char* pszType = "mixed";
                if( wkbPoint == eType )
                    pszType = "Point";
                else if( wkbLineString == eType )
                    pszType =  "LineString";
                else if( wkbPolygon == eType )
                    pszType =  "Polygon";
                else if( wkbMultiPoint == eType )
                    pszType =  "MultiPoint";
                else if( wkbMultiLineString == eType )
                    pszType =  "MultiLineString";
                else if( wkbMultiPolygon == eType )
                    pszType =  "MultiPolygon";
                else if( wkbGeometryCollection == eType )
                    pszType =  "GeometryCollection";
                oColumn.Add("gdal:geometry_type", pszType);
            }
        }

        auto kvMetadata = m_poSchema->metadata() ? m_poSchema->metadata()->Copy() :
                              std::make_shared<arrow::KeyValueMetadata>();
        kvMetadata->Append("geo", oRoot.Format(CPLJSONObject::PrettyFormat::Plain));
        m_poSchema = m_poSchema->WithMetadata(kvMetadata);
        CPLAssert(m_poSchema);
    }
}

/************************************************************************/
/*                          CreateWriter()                              */
/************************************************************************/

void OGRFeatherWriterLayer::CreateWriter()
{
    CPLAssert( m_poFileWriter == nullptr );

    if( m_poSchema == nullptr )
    {
        CreateSchema();
    }
    else
    {
        FinalizeSchema();
    }

    auto options = arrow::ipc::IpcWriteOptions::Defaults();
    options.memory_pool = m_poMemoryPool;

    {
        auto result = arrow::util::Codec::Create(m_eCompression);
        if( !result.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec::Create() failed with %s",
                     result.status().message().c_str());
        }
        else
        {
            options.codec.reset(result->release());
        }
    }

    if( m_bStreamFormat )
    {
        auto result = arrow::ipc::MakeStreamWriter(m_poOutputStream,
                                                   m_poSchema,
                                                   options);
        if( !result.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "arrow::ipc::MakeStreamWriter() failed with %s",
                     result.status().message().c_str());
        }
        else
        {
            m_poFileWriter = *result;
        }
    }
    else
    {
        m_poFooterKeyValueMetadata = std::make_shared<arrow::KeyValueMetadata>();
        auto result = arrow::ipc::MakeFileWriter(m_poOutputStream,
                                                 m_poSchema,
                                                 options,
                                                 m_poFooterKeyValueMetadata);
        if( !result.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "arrow::ipc::MakeFileWriter() failed with %s",
                     result.status().message().c_str());
        }
        else
        {
            m_poFileWriter = *result;
        }
    }
}

/************************************************************************/
/*               PerformStepsBeforeFinalFlushGroup()                    */
/************************************************************************/

// Add a gdal:geo extension metadata for now, which embeds a bbox
void OGRFeatherWriterLayer::PerformStepsBeforeFinalFlushGroup()
{
    if( m_poFooterKeyValueMetadata &&
        m_poFeatureDefn->GetGeomFieldCount() != 0 &&
        CPLTestBool(CPLGetConfigOption("OGR_ARROW_WRITE_GDAL_FOOTER", "YES")) )
    {
        CPLJSONObject oRoot;
        oRoot.Add("primary_column",
                  m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        CPLJSONObject oColumns;
        oRoot.Add("columns", oColumns);
        for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i )
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            CPLJSONObject oColumn;
            oColumns.Add(poGeomFieldDefn->GetNameRef(), oColumn);
            oColumn.Add("encoding",
                        GetGeomEncodingAsString(m_aeGeomEncoding[i]));

            const auto poSRS = poGeomFieldDefn->GetSpatialRef();
            if( poSRS )
            {
                const char* const apszOptions[] = {
                    "FORMAT=WKT2_2019", "MULTILINE=NO", nullptr };
                char* pszWKT = nullptr;
                poSRS->exportToWkt(&pszWKT, apszOptions);
                if( pszWKT )
                    oColumn.Add("crs", pszWKT);
                CPLFree(pszWKT);

                const double dfCoordEpoch = poSRS->GetCoordinateEpoch();
                if( dfCoordEpoch > 0 )
                    oColumn.Add("epoch", dfCoordEpoch);
            }

            if( m_aoEnvelopes[i].IsInit() )
            {
                CPLJSONArray oBBOX;
                oBBOX.Add(m_aoEnvelopes[i].MinX);
                oBBOX.Add(m_aoEnvelopes[i].MinY);
                oBBOX.Add(m_aoEnvelopes[i].MaxX);
                oBBOX.Add(m_aoEnvelopes[i].MaxY);
                oColumn.Add("bbox", oBBOX);
            }
        }

        m_poFooterKeyValueMetadata->Append(
            GDAL_GEO_FOOTER_KEY,
            oRoot.Format(CPLJSONObject::PrettyFormat::Plain));
    }
}

/************************************************************************/
/*                            FlushGroup()                              */
/************************************************************************/

bool OGRFeatherWriterLayer::FlushGroup()
{
    std::vector<std::shared_ptr<arrow::Array>> columns;
    auto ret = WriteArrays([this, &columns](const std::shared_ptr<arrow::Field>&,
                                            const std::shared_ptr<arrow::Array>& array) {
        columns.emplace_back(array);
        return true;
    });

    if( ret )
    {
        auto poRecordBatch = arrow::RecordBatch::Make(
            m_poSchema,
            !columns.empty() ? columns[0]->length(): 0,
            columns);
        auto status = m_poFileWriter->WriteRecordBatch(*poRecordBatch);
        if( !status.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteRecordBatch() failed with %s", status.message().c_str());
            ret = false;
        }
    }

    m_apoBuilders.clear();
    return ret;
}
