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

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowwriterlayer.hpp"

/************************************************************************/
/*                      OGRParquetWriterLayer()                         */
/************************************************************************/

OGRParquetWriterLayer::OGRParquetWriterLayer(
            arrow::MemoryPool* poMemoryPool,
            const std::shared_ptr<arrow::io::OutputStream>& poOutputStream,
            const char *pszLayerName):
    OGRArrowWriterLayer(poMemoryPool, poOutputStream, pszLayerName)
{
}

/************************************************************************/
/*                     ~OGRParquetWriterLayer()                         */
/************************************************************************/

OGRParquetWriterLayer::~OGRParquetWriterLayer()
{
    if( m_bInitializationOK )
        FinalizeWriting();
}

/************************************************************************/
/*                       IsSupportedGeometryType()                      */
/************************************************************************/

bool OGRParquetWriterLayer::IsSupportedGeometryType(OGRwkbGeometryType eGType) const
{
    const auto eFlattenType = wkbFlatten(eGType);
    if( !OGR_GT_HasM(eGType) && eFlattenType <= wkbGeometryCollection )
    {
        return true;
    }

    const auto osConfigOptionName = "OGR_" + GetDriverUCName() + "_ALLOW_ALL_DIMS";
    if( CPLTestBool(CPLGetConfigOption(osConfigOptionName.c_str(), "NO")) )
    {
        return true;
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "Only 2D and Z geometry types are supported (unless the "
             "%s configuration option is set to YES)",
             osConfigOptionName.c_str());
    return false;
}

/************************************************************************/
/*                           SetOptions()                               */
/************************************************************************/

bool OGRParquetWriterLayer::SetOptions(CSLConstList papszOptions,
                                       OGRSpatialReference *poSpatialRef,
                                       OGRwkbGeometryType eGType)
{
    const char* pszGeomEncoding = CSLFetchNameValue(papszOptions, "GEOMETRY_ENCODING");
    m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
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
        auto oResult = arrow::util::Codec::GetCompressionType("snappy");
        if( oResult.ok() && arrow::util::Codec::IsAvailable(*oResult) )
        {
            pszCompression = "SNAPPY";
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

    const char* pszRowGroupSize = CSLFetchNameValue(papszOptions, "ROW_GROUP_SIZE");
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

void OGRParquetWriterLayer::CloseFileWriter()
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
/*               PerformStepsBeforeFinalFlushGroup()                    */
/************************************************************************/

void OGRParquetWriterLayer::PerformStepsBeforeFinalFlushGroup()
{
    if( m_poKeyValueMetadata &&
        m_poFeatureDefn->GetGeomFieldCount() != 0 &&
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_GEO", "YES")) )
    {
        CPLJSONObject oRoot;
        oRoot.Add("version", "0.1.0");
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

            if( m_aoEnvelopes[i].IsInit() &&
                CPLTestBool(CPLGetConfigOption(
                    "OGR_PARQUET_WRITE_BBOX", "YES")) )
            {
                CPLJSONArray oBBOX;
                oBBOX.Add(m_aoEnvelopes[i].MinX);
                oBBOX.Add(m_aoEnvelopes[i].MinY);
                oBBOX.Add(m_aoEnvelopes[i].MaxX);
                oBBOX.Add(m_aoEnvelopes[i].MaxY);
                oColumn.Add("bbox", oBBOX);
            }

            const auto GetStringGeometryType = [](OGRwkbGeometryType eType)
            {
                const auto eFlattenType = wkbFlatten(eType);
                std::string osType = "Unknown";
                if( wkbPoint == eFlattenType )
                    osType = "Point";
                else if( wkbLineString == eFlattenType )
                    osType = "LineString";
                else if( wkbPolygon == eFlattenType )
                    osType = "Polygon";
                else if( wkbMultiPoint == eFlattenType )
                    osType = "MultiPoint";
                else if( wkbMultiLineString == eFlattenType )
                    osType = "MultiLineString";
                else if( wkbMultiPolygon == eFlattenType )
                    osType = "MultiPolygon";
                else if( wkbGeometryCollection == eFlattenType )
                    osType = "GeometryCollection";
                if( osType != "Unknown" )
                {
                    // M and ZM not supported officially currently, but it
                    // doesn't hurt to anticipate
                    if( OGR_GT_HasZ(eType) && OGR_GT_HasM(eType) )
                        osType += " ZM";
                    else if( OGR_GT_HasZ(eType) )
                        osType += " Z";
                    else if( OGR_GT_HasM(eType) )
                        osType += " M";
                }
                return osType;
            };

            if( m_oSetWrittenGeometryTypes[i].empty() )
            {
                const auto eType = poGeomFieldDefn->GetType();
                oColumn.Add("geometry_type", GetStringGeometryType(eType));
            }
            else if( m_oSetWrittenGeometryTypes[i].size() == 1 )
            {
                const auto eType = *(m_oSetWrittenGeometryTypes[i].begin());
                oColumn.Add("geometry_type", GetStringGeometryType(eType));
            }
            else
            {
                CPLJSONArray oArray;
                for( const auto eType: m_oSetWrittenGeometryTypes[i] )
                {
                    oArray.Add(GetStringGeometryType(eType));
                }
                oColumn.Add("geometry_type", oArray);
            }
        }

        // HACK: it would be good for Arrow to provide a clean way to alter
        // key value metadata before finalizing.
        // We need to write metadata at end to write the bounding box.
        const_cast<arrow::KeyValueMetadata*>(m_poKeyValueMetadata.get())->Append(
                "geo", oRoot.Format(CPLJSONObject::PrettyFormat::Plain));
    }
}

/************************************************************************/
/*                         GetSchemaMetadata()                          */
/************************************************************************/

// From ${arrow_root}/src/parquet/arrow/writer.cpp
static
arrow::Status GetSchemaMetadata(const ::arrow::Schema& schema, ::arrow::MemoryPool* pool,
                         const parquet::ArrowWriterProperties& properties,
                         std::shared_ptr<const arrow::KeyValueMetadata>* out) {
  if (!properties.store_schema()) {
    *out = nullptr;
    return arrow::Status::OK();
  }

  static const std::string kArrowSchemaKey = "ARROW:schema";
  std::shared_ptr<arrow::KeyValueMetadata> result;
  if (schema.metadata()) {
    result = schema.metadata()->Copy();
  } else {
    result = std::make_shared<arrow::KeyValueMetadata>();
  }

  if( CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_ARROW_SCHEMA", "YES")) )
  {
      ARROW_ASSIGN_OR_RAISE(std::shared_ptr<arrow::Buffer> serialized,
                            ::arrow::ipc::SerializeSchema(schema, pool));

      // The serialized schema is not UTF-8, which is required for Thrift
      std::string schema_as_string = serialized->ToString();
      std::string schema_base64 = ::arrow::util::base64_encode(schema_as_string);
      result->Append(kArrowSchemaKey, schema_base64);
  }
  *out = result;
  return arrow::Status::OK();
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

// Same as parquet::arrow::FileWriter::Open(), except we also
// return KeyValueMetadata
static
arrow::Status Open(const ::arrow::Schema& schema, ::arrow::MemoryPool* pool,
                        std::shared_ptr<::arrow::io::OutputStream> sink,
                        std::shared_ptr<parquet::WriterProperties> properties,
                        std::shared_ptr<parquet::ArrowWriterProperties> arrow_properties,
                        std::unique_ptr<parquet::arrow::FileWriter>* writer,
                        std::shared_ptr<const arrow::KeyValueMetadata>* outMetadata) {
    std::shared_ptr<parquet::SchemaDescriptor> parquet_schema;
    RETURN_NOT_OK(
      parquet::arrow::ToParquetSchema(&schema, *properties, *arrow_properties, &parquet_schema));

    auto schema_node = std::static_pointer_cast<parquet::schema::GroupNode>(parquet_schema->schema_root());

    std::shared_ptr<const arrow::KeyValueMetadata> metadata;
    RETURN_NOT_OK(GetSchemaMetadata(schema, pool, *arrow_properties, &metadata));

    *outMetadata = metadata;

    std::unique_ptr<parquet::ParquetFileWriter> base_writer;
    PARQUET_CATCH_NOT_OK(base_writer = parquet::ParquetFileWriter::Open(
        std::move(sink), schema_node,
        std::move(properties),
        metadata));

    auto schema_ptr = std::make_shared<::arrow::Schema>(schema);
    return parquet::arrow::FileWriter::Make(
        pool, std::move(base_writer), std::move(schema_ptr),
        std::move(arrow_properties), writer);
}

/************************************************************************/
/*                          CreateSchema()                              */
/************************************************************************/

void OGRParquetWriterLayer::CreateSchema()
{
    CreateSchemaCommon();
}

/************************************************************************/
/*                          CreateWriter()                              */
/************************************************************************/

void OGRParquetWriterLayer::CreateWriter()
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

    auto writerProperties = parquet::WriterProperties::Builder().compression(m_eCompression)->build();
    auto arrowWriterProperties = parquet::ArrowWriterProperties::Builder().store_schema()->build();
    Open(*m_poSchema, m_poMemoryPool, m_poOutputStream,
         writerProperties,
         arrowWriterProperties,
         &m_poFileWriter,
         &m_poKeyValueMetadata);
}

/************************************************************************/
/*                            FlushGroup()                              */
/************************************************************************/

bool OGRParquetWriterLayer::FlushGroup()
{
    auto status = m_poFileWriter->NewRowGroup(m_apoBuilders[0]->length());
    if( !status.ok() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NewRowGroup() failed with %s", status.message().c_str());
        m_apoBuilders.clear();
        return false;
    }

    auto ret = WriteArrays([this](const std::shared_ptr<arrow::Field>& field,
                                  const std::shared_ptr<arrow::Array>& array)
    {
        auto l_status = m_poFileWriter->WriteColumnChunk(*array);
        if( !l_status.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteColumnChunk() failed for field %s: %s",
                 field->name().c_str(),
                 l_status.message().c_str());
            return false;
        }
        return true;
    });

    m_apoBuilders.clear();
    return ret;
}

/************************************************************************/
/*                     FixupGeometryBeforeWriting()                     */
/************************************************************************/

void OGRParquetWriterLayer::FixupGeometryBeforeWriting(OGRGeometry* poGeom)
{
    const auto eFlattenType = wkbFlatten(poGeom->getGeometryType());
    // Polygon rings MUST follow the right-hand rule for orientation
    // (counterclockwise external rings, clockwise internal rings)
    if( eFlattenType == wkbPolygon )
    {
        bool bFirstRing = true;
        for( auto poRing: poGeom->toPolygon() )
        {
            if( (bFirstRing && poRing->isClockwise()) ||
                (!bFirstRing && !poRing->isClockwise()) )
            {
                poRing->reverseWindingOrder();
            }
            bFirstRing = false;
        }
    }
    else if( eFlattenType == wkbMultiPolygon ||
             eFlattenType == wkbGeometryCollection )
    {
        for( auto poSubGeom: poGeom->toGeometryCollection() )
        {
            FixupGeometryBeforeWriting(poSubGeom);
        }
    }
}
