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

#undef DO_NOT_DEFINE_GDAL_DATE_NAME
#include "gdal_version_full/gdal_version.h"

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

    m_bForceCounterClockwiseOrientation =
        EQUAL(CSLFetchNameValueDef(papszOptions, "POLYGON_ORIENTATION", "COUNTERCLOCKWISE"),
              "COUNTERCLOCKWISE");

    if( eGType != wkbNone )
    {
        if( !IsSupportedGeometryType(eGType) )
        {
            return false;
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

    m_oWriterPropertiesBuilder.compression(m_eCompression);
    const std::string osCreator = CSLFetchNameValueDef(papszOptions, "CREATOR", "");
    if( !osCreator.empty() )
        m_oWriterPropertiesBuilder.created_by(osCreator);
    else
        m_oWriterPropertiesBuilder.created_by("GDAL " GDAL_RELEASE_NAME ", using " CREATED_BY_VERSION);

    // Undocumented option. Not clear it is useful besides unit test purposes
    if( !CPLTestBool(CSLFetchNameValueDef(papszOptions, "STATISTICS", "YES")) )
        m_oWriterPropertiesBuilder.disable_statistics();

    if( m_eGeomEncoding == OGRArrowGeomEncoding::WKB && eGType != wkbNone )
    {
        m_oWriterPropertiesBuilder.disable_statistics(
            parquet::schema::ColumnPath::FromDotString(
                m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()));
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

    m_bEdgesSpherical = EQUAL(
        CSLFetchNameValueDef(papszOptions, "EDGES", "PLANAR"), "SPHERICAL");

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
/*                            IdentifyCRS()                             */
/************************************************************************/

static OGRSpatialReference IdentifyCRS(const OGRSpatialReference* poSRS)
{
    OGRSpatialReference oSRSIdentified(*poSRS);

    if( poSRS->GetAuthorityName(nullptr) == nullptr )
    {
        // Try to find a registered CRS that matches the input one
        int nEntries = 0;
        int* panConfidence = nullptr;
        OGRSpatialReferenceH* pahSRS =
            poSRS->FindMatches(nullptr, &nEntries, &panConfidence);

        // If there are several matches >= 90%, take the only one
        // that is EPSG
        int iOtherAuthority = -1;
        int iEPSG = -1;
        const char* const apszOptions[] =
        {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
            nullptr
        };
        int iConfidenceBestMatch = -1;
        for(int iSRS = 0; iSRS < nEntries; iSRS++ )
        {
            auto poCandidateCRS = OGRSpatialReference::FromHandle(pahSRS[iSRS]);
            if( panConfidence[iSRS] < iConfidenceBestMatch ||
                panConfidence[iSRS] < 70 )
            {
                break;
            }
            if( poSRS->IsSame(poCandidateCRS, apszOptions) )
            {
                const char* pszAuthName =
                   poCandidateCRS->GetAuthorityName(nullptr);
                if( pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG") )
                {
                    iOtherAuthority = -2;
                    if( iEPSG < 0 )
                    {
                        iConfidenceBestMatch = panConfidence[iSRS];
                        iEPSG = iSRS;
                    }
                    else
                    {
                        iEPSG = -1;
                        break;
                    }
                }
                else if( iEPSG < 0 && pszAuthName != nullptr )
                {
                    if( EQUAL(pszAuthName, "OGC") )
                    {
                        const char* pszAuthCode = poCandidateCRS->GetAuthorityCode(nullptr);
                        if( pszAuthCode && EQUAL(pszAuthCode, "CRS84") )
                        {
                            iOtherAuthority = iSRS;
                            break;
                        }
                    }
                    else if( iOtherAuthority == -1 )
                    {
                        iConfidenceBestMatch = panConfidence[iSRS];
                        iOtherAuthority = iSRS;
                    }
                    else
                        iOtherAuthority = -2;
                }
            }
        }
        if( iEPSG >= 0 )
        {
            oSRSIdentified = *OGRSpatialReference::FromHandle(pahSRS[iEPSG]);
        }
        else if( iOtherAuthority >= 0 )
        {
            oSRSIdentified = *OGRSpatialReference::FromHandle(pahSRS[iOtherAuthority]);
        }
        OSRFreeSRSArray(pahSRS);
        CPLFree(panConfidence);
    }

    return oSRSIdentified;
}

/************************************************************************/
/*                      RemoveIDFromMemberOfEnsembles()                 */
/************************************************************************/

static void RemoveIDFromMemberOfEnsembles(CPLJSONObject& obj)
{
    // Remove "id" from members of datum ensembles for compatibility with
    // older PROJ versions
    // Cf https://github.com/opengeospatial/geoparquet/discussions/110
    // and https://github.com/OSGeo/PROJ/pull/3221
    if( obj.GetType() == CPLJSONObject::Type::Object )
    {
        for( auto& subObj: obj.GetChildren() )
        {
            RemoveIDFromMemberOfEnsembles(subObj);
        }
    }
    else if( obj.GetType() == CPLJSONObject::Type::Array &&
             obj.GetName() == "members" )
    {
        for( auto& subObj: obj.ToArray() )
        {
            subObj.Delete("id");
        }
    }
}

/************************************************************************/
/*                            GetGeoMetadata()                          */
/************************************************************************/

std::string OGRParquetWriterLayer::GetGeoMetadata() const
{
    if( m_poFeatureDefn->GetGeomFieldCount() != 0 &&
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_GEO", "YES")) )
    {
        CPLJSONObject oRoot;
        oRoot.Add("version", "0.4.0");
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

            if( CPLTestBool(CPLGetConfigOption(
                    "OGR_PARQUET_WRITE_CRS", "YES")) )
            {
                const auto poSRS = poGeomFieldDefn->GetSpatialRef();
                if( poSRS )
                {
                    OGRSpatialReference oSRSIdentified(IdentifyCRS(poSRS));

                    const char* pszAuthName =
                        oSRSIdentified.GetAuthorityName(nullptr);
                    const char* pszAuthCode =
                        oSRSIdentified.GetAuthorityCode(nullptr);

                    bool bOmitCRS = false;
                    if( pszAuthName != nullptr && pszAuthCode != nullptr &&
                        ((EQUAL(pszAuthName, "EPSG") && EQUAL(pszAuthCode, "4326")) ||
                         (EQUAL(pszAuthName, "OGC") && EQUAL(pszAuthCode, "CRS84"))) )
                    {
                        // To make things less confusing for non-geo-aware
                        // consumers, omit EPSG:4326 / OGC:CRS84 CRS by default
                        bOmitCRS = CPLTestBool(
                            CPLGetConfigOption("OGR_PARQUET_CRS_OMIT_IF_WGS84", "YES"));
                    }

                    if( bOmitCRS )
                    {
                        // do nothing
                    }
                    else if( EQUAL(CPLGetConfigOption(
                        "OGR_PARQUET_CRS_ENCODING", "PROJJSON"), "PROJJSON") )
                    {
                        // CRS encoded as PROJJSON for GeoParquet >= 0.4.0
                        char* pszPROJJSON = nullptr;
                        oSRSIdentified.exportToPROJJSON(&pszPROJJSON, nullptr);
                        CPLJSONDocument oCRSDoc;
                        oCRSDoc.LoadMemory(pszPROJJSON);
                        CPLFree(pszPROJJSON);
                        CPLJSONObject oCRSRoot = oCRSDoc.GetRoot();
                        RemoveIDFromMemberOfEnsembles(oCRSRoot);
                        oColumn.Add("crs", oCRSRoot);
                    }
                    else
                    {
                        // WKT was used in GeoParquet <= 0.3.0
                        const char* const apszOptions[] = {
                            "FORMAT=WKT2_2019", "MULTILINE=NO", nullptr };
                        char* pszWKT = nullptr;
                        oSRSIdentified.exportToWkt(&pszWKT, apszOptions);
                        if( pszWKT )
                            oColumn.Add("crs", pszWKT);
                        CPLFree(pszWKT);
                    }

                    const double dfCoordEpoch = poSRS->GetCoordinateEpoch();
                    if( dfCoordEpoch > 0 )
                        oColumn.Add("epoch", dfCoordEpoch);
                }
                else
                {
                    oColumn.AddNull("crs");
                }
            }

            if( m_bEdgesSpherical )
            {
                oColumn.Add("edges", "spherical");
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

            if( m_bForceCounterClockwiseOrientation )
                oColumn.Add("orientation", "counterclockwise");

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

        return oRoot.Format(CPLJSONObject::PrettyFormat::Plain);
    }
    return std::string();
}

/************************************************************************/
/*               PerformStepsBeforeFinalFlushGroup()                    */
/************************************************************************/

void OGRParquetWriterLayer::PerformStepsBeforeFinalFlushGroup()
{
    if( m_poKeyValueMetadata )
    {
        const std::string osGeoMetadata = GetGeoMetadata();
        auto poTmpSchema = m_poSchema;
        if( !osGeoMetadata.empty() )
        {
            // HACK: it would be good for Arrow to provide a clean way to alter
            // key value metadata before finalizing.
            // We need to write metadata at end to write the bounding box.
            const_cast<arrow::KeyValueMetadata*>(m_poKeyValueMetadata.get())->Append(
                    "geo", osGeoMetadata);

            auto kvMetadata = poTmpSchema->metadata() ? poTmpSchema->metadata()->Copy() :
                                  std::make_shared<arrow::KeyValueMetadata>();
            kvMetadata->Append("geo", osGeoMetadata);
            poTmpSchema = poTmpSchema->WithMetadata(kvMetadata);
        }

        if( CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_ARROW_SCHEMA", "YES")) )
        {
            auto status = ::arrow::ipc::SerializeSchema(*poTmpSchema, m_poMemoryPool);
            if( status.ok() )
            {
                // The serialized schema is not UTF-8, which is required for Thrift
                std::string schema_as_string = (*status)->ToString();
                std::string schema_base64 = ::arrow::util::base64_encode(schema_as_string);
                static const std::string kArrowSchemaKey = "ARROW:schema";
                const_cast<arrow::KeyValueMetadata*>(m_poKeyValueMetadata.get())->Append(
                  kArrowSchemaKey, schema_base64);
            }
        }
    }
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

    auto metadata = schema.metadata() ? schema.metadata()->Copy() :
                        std::make_shared<arrow::KeyValueMetadata>();
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
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRParquetWriterLayer::CreateGeomField( OGRGeomFieldDefn *poField,
                                               int bApproxOK )
{
    OGRErr eErr = OGRArrowWriterLayer::CreateGeomField(poField, bApproxOK);
    if( eErr == OGRERR_NONE && m_aeGeomEncoding.back() == OGRArrowGeomEncoding::WKB )
    {
        m_oWriterPropertiesBuilder.disable_statistics(
            parquet::schema::ColumnPath::FromDotString(
                m_poFeatureDefn->GetGeomFieldDefn(
                    m_poFeatureDefn->GetGeomFieldCount() - 1)->GetNameRef()));
    }
    return eErr;
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

    auto arrowWriterProperties = parquet::ArrowWriterProperties::Builder().store_schema()->build();
    CPL_IGNORE_RET_VAL(Open(*m_poSchema, m_poMemoryPool, m_poOutputStream,
         m_oWriterPropertiesBuilder.build(),
         arrowWriterProperties,
         &m_poFileWriter,
         &m_poKeyValueMetadata));
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
    if( !m_bForceCounterClockwiseOrientation )
        return;

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
