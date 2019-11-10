/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements OGRFlatGeobufLayer class
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
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

#include "ogrsf_frmts.h"
#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "ogr_p.h"

#include "ogr_flatgeobuf.h"

using namespace flatbuffers;
using namespace FlatGeobuf;

static std::nullptr_t CPLErrorInvalidPointer(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected nullptr: %s", message);
    return nullptr;
}

static std::nullptr_t CPLErrorInvalidLength(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid length detected: %s", message);
    return nullptr;
}

static OGRErr CPLErrorInvalidSize(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid size detected: %s", message);
    return OGRERR_CORRUPT_DATA;
}

static OGRErr CPLErrorMemoryAllocation(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Could not allocate memory: %s", message);
    return OGRERR_NOT_ENOUGH_MEMORY;
}

static OGRErr CPLErrorIO(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected I/O failure: %s", message);
    return OGRERR_FAILURE;
}

OGRFlatGeobufLayer::OGRFlatGeobufLayer(
    const Header *poHeader,
    GByte *headerBuf,
    const char *pszFilename,
    VSILFILE *poFp,
    uint64_t offset,
    uint64_t offsetIndices)
{
    m_poHeader = poHeader;
    CPLAssert(poHeader);
    m_headerBuf = headerBuf;
    CPLAssert(pszFilename);
    if (pszFilename)
        m_osFilename = pszFilename;
    m_poFp = poFp;
    m_offsetFeatures = offset;
    m_offsetIndices = offsetIndices;
    m_offset = offset;
    m_create = false;

    m_featuresCount = m_poHeader->features_count();
    m_geometryType = m_poHeader->geometry_type();
    m_hasZ = m_poHeader->hasZ();
    m_hasM = m_poHeader->hasM();
    m_hasT = m_poHeader->hasT();
    const auto envelope = m_poHeader->envelope();
    if( envelope && envelope->size() == 4 )
    {
        m_sExtent.MinX = (*envelope)[0];
        m_sExtent.MinY = (*envelope)[1];
        m_sExtent.MaxX = (*envelope)[2];
        m_sExtent.MaxY = (*envelope)[3];
    }

    CPLDebug("FlatGeobuf", "m_hasZ: %d", m_hasZ);
    CPLDebug("FlatGeobuf", "m_hasM: %d", m_hasM);
    CPLDebug("FlatGeobuf", "m_hasT: %d", m_hasT);

    const auto crs = m_poHeader->crs();
    if (crs != nullptr) {
        m_poSRS = new OGRSpatialReference();
        m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        const auto org = crs->org();
        const auto code = crs->code();
        const auto wkt = crs->wkt();
        if ((org == nullptr || EQUAL(org->c_str(), "EPSG")) && code != 0) {
            m_poSRS->importFromEPSG(code);
        } else if( org && code != 0 ) {
            CPLString osCode;
            osCode.Printf("%s:%d", org->c_str(), code);
            if( m_poSRS->SetFromUserInput(osCode.c_str()) != OGRERR_NONE &&
                wkt != nullptr )
            {
                m_poSRS->importFromWkt(wkt->c_str());
            }
        } else if (wkt) {
            m_poSRS->importFromWkt(wkt->c_str());
        }
    }

    m_eGType = getOGRwkbGeometryType();

    const char *pszName = m_poHeader->name() ? m_poHeader->name()->c_str() : "unknown";
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(wkbNone);
    OGRGeomFieldDefn *poGeomFieldDefn = new OGRGeomFieldDefn(nullptr, m_eGType);
    if (m_poSRS != nullptr)
        poGeomFieldDefn->SetSpatialRef(m_poSRS);
    m_poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, false);
    readColumns();
    m_poFeatureDefn->Reference();
}

OGRFlatGeobufLayer::OGRFlatGeobufLayer(
    const char *pszLayerName,
    const char *pszFilename,
    OGRSpatialReference *poSpatialRef,
    OGRwkbGeometryType eGType,
    VSILFILE *poFpWrite,
    std::string oTempFile,
    bool bCreateSpatialIndexAtClose) :
    m_eGType(eGType),
    m_poFpWrite(poFpWrite),
    m_oTempFile(oTempFile)
{
    m_create = true;
    m_bCreateSpatialIndexAtClose = bCreateSpatialIndexAtClose;

    if (pszLayerName)
        m_osLayerName = pszLayerName;
    if (pszFilename)
        m_osFilename = pszFilename;
    if (!translateOGRwkbGeometryType())
        m_bCanCreate = false;
    if (poSpatialRef)
        m_poSRS = poSpatialRef->Clone();

    CPLDebug("FlatGeobuf", "geometryType: %d, hasZ: %d, hasM: %d, hasT: %d", (int) m_geometryType, m_hasZ, m_hasM, m_hasT);

    SetMetadataItem(OLMD_FID64, "YES");

    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGType);
    m_poFeatureDefn->Reference();
}

bool OGRFlatGeobufLayer::translateOGRwkbGeometryType()
{
    const auto flatType = wkbFlatten(m_eGType);
    switch (flatType) {
        case OGRwkbGeometryType::wkbPoint: m_geometryType = GeometryType::Point; break;
        case OGRwkbGeometryType::wkbMultiPoint: m_geometryType = GeometryType::MultiPoint; break;
        case OGRwkbGeometryType::wkbLineString: m_geometryType = GeometryType::LineString; break;
        case OGRwkbGeometryType::wkbMultiLineString: m_geometryType = GeometryType::MultiLineString; break;
        case OGRwkbGeometryType::wkbPolygon: m_geometryType = GeometryType::Polygon; break;
        case OGRwkbGeometryType::wkbMultiPolygon: m_geometryType = GeometryType::MultiPolygon; break;
        default:
            CPLError(CE_Failure, CPLE_NotSupported, "toGeometryType: Unknown OGRwkbGeometryType %d", (int) m_eGType);
            return false;
    }
    if wkbHasZ(m_eGType)
        m_hasZ = true;
    if wkbHasM(m_eGType)
        m_hasM = true;
    return true;
}

OGRwkbGeometryType OGRFlatGeobufLayer::getOGRwkbGeometryType()
{
    OGRwkbGeometryType ogrType = OGRwkbGeometryType::wkbUnknown;
    switch (m_geometryType) {
        case GeometryType::Point: ogrType = OGRwkbGeometryType::wkbPoint; break;
        case GeometryType::MultiPoint: ogrType = OGRwkbGeometryType::wkbMultiPoint; break;
        case GeometryType::LineString: ogrType = OGRwkbGeometryType::wkbLineString; break;
        case GeometryType::MultiLineString: ogrType = OGRwkbGeometryType::wkbMultiLineString; break;
        case GeometryType::Polygon: ogrType = OGRwkbGeometryType::wkbPolygon; break;
        case GeometryType::MultiPolygon: ogrType = OGRwkbGeometryType::wkbMultiPolygon; break;
        default:
            CPLError(CE_Failure, CPLE_NotSupported, "toOGRwkbGeometryType: Unknown FlatGeobuf::GeometryType %d", (int) m_geometryType);
    }
    if (m_hasZ)
        ogrType = wkbSetZ(ogrType);
    if (m_hasM)
        ogrType = wkbSetM(ogrType);
    return ogrType;
}

ColumnType OGRFlatGeobufLayer::toColumnType(OGRFieldType type, OGRFieldSubType /* subType */)
{
    switch (type) {
        case OGRFieldType::OFTInteger: return ColumnType::Int;
        case OGRFieldType::OFTInteger64: return ColumnType::Long;
        case OGRFieldType::OFTReal: return ColumnType::Double;
        case OGRFieldType::OFTString: return ColumnType::String;
        case OGRFieldType::OFTDate: return ColumnType::DateTime;
        case OGRFieldType::OFTTime: return ColumnType::DateTime;
        case OGRFieldType::OFTDateTime: return ColumnType::DateTime;
        default: CPLError(CE_Failure, CPLE_AppDefined, "toColumnType: Unknown OGRFieldType %d", type);
    }
    return ColumnType::String;
}

OGRFieldType OGRFlatGeobufLayer::toOGRFieldType(ColumnType type)
{
    switch (type) {
        case ColumnType::Int: return OGRFieldType::OFTInteger;
        case ColumnType::Long: return OGRFieldType::OFTInteger64;
        case ColumnType::Double: return OGRFieldType::OFTReal;
        case ColumnType::String: return OGRFieldType::OFTString;
        case ColumnType::DateTime: return OGRFieldType::OFTDateTime;
        default: CPLError(CE_Failure, CPLE_AppDefined, "toOGRFieldType: Unknown ColumnType %d", (int) type);
    }
    return OGRFieldType::OFTString;
}

const std::vector<Offset<Column>> OGRFlatGeobufLayer::writeColumns(FlatBufferBuilder &fbb)
{
    std::vector<Offset<Column>> columns;
    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++) {
        const auto field = m_poFeatureDefn->GetFieldDefn(i);
        const auto name = field->GetNameRef();
        const auto columnType = toColumnType(field->GetType(), field->GetSubType());
        //CPLDebug("FlatGeobuf", "Create column %s (index %d)", name, i);
        const auto column = CreateColumnDirect(fbb, name, columnType);
        columns.push_back(column);
    }
    return columns;
}

void OGRFlatGeobufLayer::readColumns()
{
    const auto columns = m_poHeader->columns();
    if (columns == nullptr)
        return;
    for (uint32_t i = 0; i < columns->size(); i++) {
        const auto column = columns->Get(i);
        const auto name = column->name()->c_str();
        const auto type = toOGRFieldType(column->type());
        OGRFieldDefn field(name, type);
        m_poFeatureDefn->AddFieldDefn(&field);
    }
}

void OGRFlatGeobufLayer::WriteHeader(VSILFILE *poFp, uint64_t featuresCount, std::vector<double> *extentVector) {
    size_t c;
    c = VSIFWriteL(&magicbytes, sizeof(magicbytes), 1, poFp);
    CPLDebug("FlatGeobuf", "Wrote magicbytes (%lu bytes)", static_cast<long unsigned int>(c * sizeof(magicbytes)));
    m_writeOffset += sizeof(magicbytes);

    FlatBufferBuilder fbb;
    const auto columns = writeColumns(fbb);

    flatbuffers::Offset<Crs> crs = 0;
    if (m_poSRS) {
        int nAuthorityCode = 0;
        const char* pszAuthorityName = m_poSRS->GetAuthorityName( nullptr );
        if ( pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0 )
        {
            // Try to force identify an EPSG code.
            m_poSRS->AutoIdentifyEPSG();

            pszAuthorityName = m_poSRS->GetAuthorityName(nullptr);
            if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
            {
                const char* pszAuthorityCode = m_poSRS->GetAuthorityCode(nullptr);
                if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
                {
                    /* Import 'clean' SRS */
                    m_poSRS->importFromEPSG( atoi(pszAuthorityCode) );

                    pszAuthorityName = m_poSRS->GetAuthorityName(nullptr);
                }
            }
        }
        if ( pszAuthorityName != nullptr && strlen(pszAuthorityName) > 0 )
        {
            // For the root authority name 'EPSG', the authority code
            // should always be integral
            nAuthorityCode = atoi( m_poSRS->GetAuthorityCode(nullptr) );
        }

        // Translate SRS to WKT.
        char *pszWKT = nullptr;
        const char* const apszOptionsWkt[] = { "FORMAT=WKT2_2018", nullptr };
        m_poSRS->exportToWkt( &pszWKT, apszOptionsWkt );
        if( pszWKT && pszWKT[0] == '\0' )
        {
            CPLFree(pszWKT);
            pszWKT = nullptr;
        }

        crs = CreateCrsDirect(fbb, pszAuthorityName, nAuthorityCode, m_poSRS->GetName(), nullptr, pszWKT);
        CPLFree(pszWKT);
    }

    const auto header = CreateHeaderDirect(
        fbb, m_osLayerName.c_str(), extentVector, m_geometryType, m_hasZ, m_hasM, m_hasT, m_hasTM, &columns, featuresCount, m_indexNodeSize, crs);
    fbb.FinishSizePrefixed(header);
    c = VSIFWriteL(fbb.GetBufferPointer(), 1, fbb.GetSize(), poFp);
    CPLDebug("FlatGeobuf", "Wrote header (%lu bytes)", static_cast<long unsigned int>(c));
    m_writeOffset += c;
}

void OGRFlatGeobufLayer::Create() {
    // no spatial index requested, we are done
    if (!m_bCreateSpatialIndexAtClose)
        return;

    m_poFp = VSIFOpenL(m_osFilename.c_str(), "wb");
    if (m_poFp == nullptr) {
        CPLError(CE_Failure, CPLE_OpenFailed,
                    "Failed to create %s:\n%s",
                    m_osFilename.c_str(), VSIStrerror(errno));
        return;
    }

    // check if something has been written, if not write empty layer and bail
    if (m_writeOffset == 0 || m_featuresCount == 0) {
        CPLDebug("FlatGeobuf", "Writing empty layer");
        WriteHeader(m_poFp, 0, nullptr);
        return;
    }

    CPLDebug("FlatGeobuf", "Writing second pass sorted by spatial index");

    m_writeOffset = 0;
    m_indexNodeSize = 16;

    size_t c;

    if (m_featuresCount >= std::numeric_limits<size_t>::max() / 8) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features for this architecture");
        return;
    }

    Rect extent = calcExtent(m_featureItems);
    auto extentVector = extent.toVector();

    WriteHeader(m_poFp, m_featuresCount, &extentVector);

    CPLDebug("FlatGeobuf", "Sorting items for Packed R-tree");
    hilbertSort(m_featureItems);
    CPLDebug("FlatGeobuf", "Creating Packed R-tree");
    c = 0;
    try {
        PackedRTree tree(m_featureItems, extent);
        CPLDebug("FlatGeobuf", "PackedRTree extent %f, %f, %f, %f", extentVector[0], extentVector[1], extentVector[2], extentVector[3]);
        tree.streamWrite([this, &c] (uint8_t *data, size_t size) { c += VSIFWriteL(data, 1, size, m_poFp); });
    } catch (const std::exception& e) {
        CPLError(CE_Failure, CPLE_AppDefined, "Create: %s", e.what());
        return;
    }
    CPLDebug("FlatGeobuf", "Wrote tree (%lu bytes)", static_cast<long unsigned int>(c));
    m_writeOffset += c;

    CPLDebug("FlatGeobuf", "Writing feature offsets at offset %lu", static_cast<long unsigned int>(m_writeOffset));
    c = 0;
    for (size_t i = 0, foffset = 0; i < m_featuresCount; i++) {
        uint64_t offset_le = foffset;
        CPL_LSBPTR64(&offset_le);
        c += VSIFWriteL(&offset_le, sizeof(uint64_t), 1, m_poFp);
        foffset += std::static_pointer_cast<FeatureItem>(m_featureItems[i])->size;
    }
    CPLDebug("FlatGeobuf", "Wrote feature offsets (%lu bytes)", static_cast<long unsigned int>(c * sizeof(uint64_t)));
    m_writeOffset += c * sizeof(uint64_t);

    CPLDebug("FlatGeobuf", "Writing feature buffers at offset %lu", static_cast<long unsigned int>(m_writeOffset));
    c = 0;
    for (size_t i = 0; i < m_featuresCount; i++) {
        const auto item = std::static_pointer_cast<FeatureItem>(m_featureItems[i]);
        const auto featureSize = item->size;
        const auto err = ensureFeatureBuf(featureSize);
        if (err != OGRERR_NONE)
            return;
        //CPLDebug("FlatGeobuf", "item->offset: %lu", static_cast<long unsigned int>(item->offset));
        //CPLDebug("FlatGeobuf", "featureSize: %d", featureSize);
        if (VSIFSeekL(m_poFpWrite, item->offset, SEEK_SET) == -1) {
            CPLErrorIO("seeking to temp feature location");
            return;
        }
        if (VSIFReadL(m_featureBuf, 1, featureSize, m_poFpWrite) != featureSize) {
            CPLErrorIO("reading temp feature");
            return;
        }
        c += VSIFWriteL(m_featureBuf, 1, featureSize, m_poFp);
    }
    CPLDebug("FlatGeobuf", "Wrote feature buffers (%lu bytes)", static_cast<long unsigned int>(c));
    m_writeOffset += c;

    CPLDebug("FlatGeobuf", "Now at offset %lu", static_cast<long unsigned int>(m_writeOffset));
}

OGRFlatGeobufLayer::~OGRFlatGeobufLayer()
{
    if (m_create)
        Create();

    if (m_poFp)
        VSIFCloseL(m_poFp);

    if (m_poFpWrite)
        VSIFCloseL(m_poFpWrite);

    if (!m_oTempFile.empty())
        VSIUnlink(m_oTempFile.c_str());

    if (m_poFeatureDefn)
        m_poFeatureDefn->Release();

    if (m_poSRS)
        m_poSRS->Release();

    if (m_featureBuf)
        VSIFree(m_featureBuf);

    if (m_headerBuf)
        VSIFree(m_headerBuf);
}

OGRErr OGRFlatGeobufLayer::readFeatureOffset(uint64_t index, uint64_t &featureOffset) {
    if (VSIFSeekL(m_poFp, m_offsetIndices + (index * sizeof(featureOffset)), SEEK_SET) == -1)
        return CPLErrorIO("seeking feature offset");
    if (VSIFReadL(&featureOffset, sizeof(featureOffset), 1, m_poFp) != 1)
        return CPLErrorIO("reading feature offset");
    #if !CPL_IS_LSB
        CPL_LSBPTR64(&featureOffset);
    #endif
    return OGRERR_NONE;
}

OGRFeature *OGRFlatGeobufLayer::GetFeature(GIntBig nFeatureId)
{
    if (m_featuresCount == 0) {
        return OGRLayer::GetFeature(nFeatureId);
    } else {
        if (static_cast<uint64_t>(nFeatureId) >= m_featuresCount) {
            CPLError(CE_Failure, CPLE_AppDefined, "Requested feature id is out of bounds");
            return nullptr;
        }
        ResetReading();
        m_ignoreSpatialFilter = true;
        m_ignoreAttributeFilter = true;
        uint64_t featureOffset;
        const auto err = readFeatureOffset(nFeatureId, featureOffset);
        if (err != OGRERR_NONE) {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected error reading feature offset from id");
            return nullptr;
        }
        m_offset = m_offsetFeatures + featureOffset;
        OGRFeature *poFeature = GetNextFeature();
        if (poFeature != nullptr)
            poFeature->SetFID(nFeatureId);
        ResetReading();
        return poFeature;
    }
}

OGRErr OGRFlatGeobufLayer::readIndex()
{
    if (m_queriedSpatialIndex || !m_poFilterGeom)
        return OGRERR_NONE;
    const auto indexNodeSize = m_poHeader->index_node_size();
    if (indexNodeSize == 0)
        return OGRERR_NONE;
    const auto featuresCount = m_poHeader->features_count();
    if (indexNodeSize == 0)
        return OGRERR_NONE;

    if (VSIFSeekL(m_poFp, sizeof(magicbytes), SEEK_SET) == -1) // skip magic bytes
        return CPLErrorIO("seeking past magic bytes");
    uoffset_t headerSize;
    if (VSIFReadL(&headerSize, sizeof(uoffset_t), 1, m_poFp) != 1)
        return CPLErrorIO("reading header size");
    CPL_LSBPTR32(&headerSize);

    try {
        const auto treeSize = indexNodeSize > 0 ? PackedRTree::size(featuresCount) : 0;
        if (treeSize > 0 && m_poFilterGeom && !m_ignoreSpatialFilter) {
            CPLDebug("FlatGeobuf", "Attempting spatial index query");
            OGREnvelope env;
            m_poFilterGeom->getEnvelope(&env);
            Rect r { env.MinX, env.MinY, env.MaxX, env.MaxY };
            CPLDebug("FlatGeobuf", "Spatial index search on %f,%f,%f,%f", env.MinX, env.MinY, env.MaxX, env.MaxY);
            const auto treeOffset = sizeof(magicbytes) + sizeof(uoffset_t) + headerSize;
            const auto readNode = [this, treeOffset] (uint8_t *buf, size_t i, size_t s) {
                if (VSIFSeekL(m_poFp, treeOffset + i, SEEK_SET) == -1)
                    throw std::runtime_error("I/O seek failure");
                if (VSIFReadL(buf, 1, s, m_poFp) != s)
                    throw std::runtime_error("I/O read file");
            };
            const auto foundFeatureIndices = PackedRTree::streamSearch(featuresCount, indexNodeSize, r, readNode);
            m_featuresCount = foundFeatureIndices.size();
            CPLDebug("FlatGeobuf", "%lu features found in spatial index search", static_cast<long unsigned int>(m_featuresCount));

            // read feature offsets for the found indices
            // zip and sort on offset as pairs in m_indexOffsets
            uint64_t featureOffset;
            m_indexOffsets.reserve(foundFeatureIndices.size());
            for (auto i : foundFeatureIndices) {
                const auto err = readFeatureOffset(i, featureOffset);
                if (err != OGRERR_NONE)
                    return err;
                m_indexOffsets.push_back({ i, featureOffset });
            }
            std::sort(m_indexOffsets.begin(), m_indexOffsets.end(),
                [&](const IndexOffset i, const IndexOffset j) { return i.offset < j.offset; }
            );

            m_queriedSpatialIndex = true;
        }
    } catch (const std::exception &e) {
        CPLError(CE_Failure, CPLE_AppDefined, "readIndex: Unexpected failure: %s", e.what());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

GIntBig OGRFlatGeobufLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != nullptr || m_poAttrQuery != nullptr)
        return OGRLayer::GetFeatureCount(bForce);
    else
        return m_featuresCount;
}

OGRFeature *OGRFlatGeobufLayer::GetNextFeature()
{
    if (m_create)
        return nullptr;

    while( true ) {
        if (m_featuresCount > 0 && m_featuresPos >= m_featuresCount) {
            CPLDebug("FlatGeobuf", "GetNextFeature: iteration end at %lu", static_cast<long unsigned int>(m_featuresPos));
            return nullptr;
        }

        if (readIndex() != OGRERR_NONE) {
            return nullptr;
        }

        if (m_queriedSpatialIndex && m_featuresCount == 0) {
            CPLDebug("FlatGeobuf", "GetNextFeature: no features found");
            return nullptr;
        }

        auto poFeature = std::unique_ptr<OGRFeature>(new OGRFeature(m_poFeatureDefn));
        if (parseFeature(poFeature.get()) != OGRERR_NONE) {
            CPLError(CE_Failure, CPLE_AppDefined, "Fatal error parsing feature");
            return nullptr;
        }

        if (VSIFEofL(m_poFp)) {
            CPLDebug("FlatGeobuf", "GetNextFeature: iteration end due to EOF");
            return nullptr;
        }

        m_featuresPos++;

        if ((m_poFilterGeom == nullptr || m_ignoreSpatialFilter || FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_ignoreAttributeFilter || m_poAttrQuery->Evaluate(poFeature.get())))
            return poFeature.release();
    }
}

OGRErr OGRFlatGeobufLayer::ensureFeatureBuf(uint32_t featureSize) {
    if (m_featureBufSize == 0) {
        m_featureBufSize = std::max(1024U * 32U, featureSize);
        CPLDebug("FlatGeobuf", "ensureFeatureBuf: m_featureBufSize: %d", m_featureBufSize);
        m_featureBuf = static_cast<GByte *>(VSIMalloc(m_featureBufSize));
        if (m_featureBuf == nullptr)
            return CPLErrorMemoryAllocation("initial feature buffer");
    } else if (m_featureBufSize < featureSize) {
        m_featureBufSize = std::max(m_featureBufSize * 2, featureSize);
        CPLDebug("FlatGeobuf", "ensureFeatureBuf: m_featureBufSize: %d", m_featureBufSize);
        const auto featureBuf = static_cast<GByte *>(VSIRealloc(m_featureBuf, m_featureBufSize));
        if (featureBuf == nullptr)
            return CPLErrorMemoryAllocation("feature buffer resize");
        m_featureBuf = featureBuf;
    }
    return OGRERR_NONE;
}

OGRErr OGRFlatGeobufLayer::parseFeature(OGRFeature *poFeature) {
    GIntBig fid;
    auto seek = false;
    if (m_queriedSpatialIndex && !m_ignoreSpatialFilter) {
        const auto indexOffset = m_indexOffsets[m_featuresPos];
        m_offset = m_offsetFeatures + indexOffset.offset;
        fid = indexOffset.index;
        seek = true;
    } else {
        fid = m_featuresPos;
    }
    poFeature->SetFID(fid);

    //CPLDebug("FlatGeobuf", "m_featuresPos: %lu", static_cast<long unsigned int>(m_featuresPos));

    if (m_featuresPos == 0)
        seek = true;

    if (seek && VSIFSeekL(m_poFp, m_offset, SEEK_SET) == -1) {
        if (VSIFEofL(m_poFp))
            return OGRERR_NONE;
        return CPLErrorIO("seeking to feature location");
    }
    uint32_t featureSize;
    if (VSIFReadL(&featureSize, sizeof(uoffset_t), 1, m_poFp) != 1) {
        if (VSIFEofL(m_poFp))
            return OGRERR_NONE;
        return CPLErrorIO("reading feature size");
    }
    CPL_LSBPTR32(&featureSize);
    if (featureSize > feature_max_buffer_size)
        return CPLErrorInvalidSize("feature");

    const auto err = ensureFeatureBuf(featureSize);
    if (err != OGRERR_NONE)
        return err;
    if (VSIFReadL(m_featureBuf, 1, featureSize, m_poFp) != featureSize)
        return CPLErrorIO("reading feature");
    m_offset += featureSize + sizeof(uoffset_t);

    if (m_bVerifyBuffers) {
        const auto vBuf = const_cast<const uint8_t *>(reinterpret_cast<uint8_t *>(m_featureBuf));
        Verifier v(vBuf, featureSize);
        const auto ok = VerifyFeatureBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer verification failed");
            CPLDebug("FlatGeobuf", "m_offset: %lu", static_cast<long unsigned int>(m_offset));
            CPLDebug("FlatGeobuf", "m_featuresPos: %lu", static_cast<long unsigned int>(m_featuresPos));
            CPLDebug("FlatGeobuf", "featureSize: %d", featureSize);
            return OGRERR_CORRUPT_DATA;
        }
    }

    const auto feature = GetRoot<Feature>(m_featureBuf);
    if (!m_poFeatureDefn->IsGeometryIgnored()) {
        auto ogrGeometry = readGeometry(feature);
        if (ogrGeometry == nullptr) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to read geometry");
            return OGRERR_CORRUPT_DATA;
        }
        if (m_poSRS != nullptr)
            ogrGeometry->assignSpatialReference(m_poSRS);
        poFeature->SetGeometryDirectly(ogrGeometry);
    }
    #ifdef DEBUG
        //char *wkt;
        //ogrGeometry->exportToWkt(&wkt);
        //CPLDebug("FlatGeobuf", "readGeometry as wkt: %s", wkt);
    #endif
    const auto properties = feature->properties();
    if (properties != nullptr) {
        const auto data = properties->data();
        const auto size = properties->size();
        //CPLDebug("FlatGeobuf", "properties->size: %d", size);
        uoffset_t offset = 0;
        // size must be at least large enough to contain
        // a single column index and smallest value type
        if (size > 0 && size < (sizeof(uint16_t) + sizeof(uint8_t)))
            return CPLErrorInvalidSize("property value");
        while (offset < (size - 1)) {
            if (offset + sizeof(uint16_t) > size)
                return CPLErrorInvalidSize("property value");
            uint16_t i = *((uint16_t *)(data + offset));
            CPL_LSBPTR16(&i);
            offset += sizeof(uint16_t);
            //CPLDebug("FlatGeobuf", "i: %d", i);
            const auto columns = m_poHeader->columns();
            if (columns == nullptr) {
                CPLError(CE_Failure, CPLE_AppDefined, "Unexpected undefined columns");
                return OGRERR_CORRUPT_DATA;
            }
            if (i >= columns->size()) {
                CPLError(CE_Failure, CPLE_AppDefined, "Column index out of range");
                return OGRERR_CORRUPT_DATA;
            }
            const auto column = columns->Get(i);
            const auto type = column->type();
            const auto isIgnored = poFeature->GetFieldDefnRef(i)->IsIgnored();
            const auto ogrField = poFeature->GetRawFieldRef(i);
            if( !OGR_RawField_IsUnset(ogrField) ) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %d set more than once", i);
                return OGRERR_CORRUPT_DATA;
            }

            switch (type) {
                case ColumnType::Int:
                    if (offset + sizeof(int32_t) > size)
                        return CPLErrorInvalidSize("int32 value");
                    if (!isIgnored)
                    {
                        memcpy(&ogrField->Integer, data + offset, sizeof(int32_t));
                        CPL_LSBPTR32(&ogrField->Integer);
                    }
                    offset += sizeof(int32_t);
                    break;
                case ColumnType::Long:
                    if (offset + sizeof(int64_t) > size)
                        return CPLErrorInvalidSize("int64 value");
                    if (!isIgnored)
                    {
                        memcpy(&ogrField->Integer64, data + offset, sizeof(int64_t));
                        CPL_LSBPTR64(&ogrField->Integer64);
                    }
                    offset += sizeof(int64_t);
                    break;
                case ColumnType::Double:
                    if (offset + sizeof(double) > size)
                        return CPLErrorInvalidSize("double value");
                    if (!isIgnored)
                    {
                        memcpy(&ogrField->Real, data + offset, sizeof(double));
                        CPL_LSBPTR64(&ogrField->Real);
                    }
                    offset += sizeof(double);
                    break;
                case ColumnType::DateTime: {
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize("datetime length ");
                    uint32_t len;
                    memcpy(&len, data + offset, sizeof(int32_t));
                    CPL_LSBPTR32(&len);
                    offset += sizeof(uint32_t);
                    if (len > size - offset || len > 32)
                        return CPLErrorInvalidSize("datetime value");
                    if (!isIgnored)
                    {
                        char str[32+1];
                        memcpy(str, data + offset, len);
                        str[len] = '\0';
                        if( !OGRParseDate(str, ogrField, 0) )
                        {
                            OGR_RawField_SetUnset(ogrField);
                        }
                    }
                    offset += len;
                    break;
                }
                case ColumnType::String: {
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize("string length");
                    uint32_t len;
                    memcpy(&len, data + offset, sizeof(int32_t));
                    CPL_LSBPTR32(&len);
                    offset += sizeof(uint32_t);
                    if (len > size - offset)
                        return CPLErrorInvalidSize("string value");
                    if (!isIgnored )
                    {
                        char *str = static_cast<char*>(VSI_MALLOC_VERBOSE(len + 1));
                        if (str == nullptr)
                            return CPLErrorMemoryAllocation("string value");
                        memcpy(str, data + offset, len);
                        str[len] = '\0';
                        ogrField->String = str;
                    }
                    offset += len;
                    break;
                }
                default:
                    CPLError(CE_Failure, CPLE_AppDefined, "GetNextFeature: Unknown column->type: %d", (int) type);
            }
        }
    }
    return OGRERR_NONE;
}

OGRPoint *OGRFlatGeobufLayer::readPoint(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t offset)
{
    const auto offsetXy = offset * 2;
    const auto aXy = pXy.data();
    if (offsetXy >= pXy.size())
        return CPLErrorInvalidLength("XY data");
    if (m_hasZ) {
        const auto pZ = feature->z();
        if (pZ == nullptr)
            return CPLErrorInvalidPointer("Z data");
        if (offset >= pZ->size())
            return CPLErrorInvalidLength("Z data");
        const auto aZ = pZ->data();
        if (m_hasM) {
            const auto pM = feature->m();
            if (pM == nullptr)
                return CPLErrorInvalidPointer("M data");
            if (offset >= pM->size())
                return CPLErrorInvalidLength("M data");
            const auto aM = pM->data();
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[offset]),
                                  flatbuffers::EndianScalar(aM[offset]) };
        } else {
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[offset]) };
        }
    } else if (m_hasM) {
        const auto pM = feature->m();
        if (pM == nullptr)
            return CPLErrorInvalidPointer("M data");
        if (offset >= pM->size())
            return CPLErrorInvalidLength("M data");
        const auto aM = pM->data();
        return OGRPoint::createXYM( flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                    flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                    flatbuffers::EndianScalar(aM[offset]) );
    } else {
        return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                              flatbuffers::EndianScalar(aXy[offsetXy + 1]) };
    }
}

OGRMultiPoint *OGRFlatGeobufLayer::readMultiPoint(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len)
{
    if (len >= feature_max_buffer_size)
        return CPLErrorInvalidLength("MultiPoint");
    const auto mp = new OGRMultiPoint();
    for (uint32_t i = 0; i < len; i++) {
        const auto p = readPoint(feature, pXy, i);
        if (p == nullptr) {
            delete mp;
            return nullptr;
        }
        mp->addGeometryDirectly(p);
    }

    return mp;
}

OGRLineString *OGRFlatGeobufLayer::readLineString(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset)
{
    const auto ls = new OGRLineString();
    if (readSimpleCurve(feature, pXy, len, offset, ls) != OGRERR_NONE) {
        delete ls;
        return nullptr;
    }
    return ls;
}

OGRMultiLineString *OGRFlatGeobufLayer::readMultiLineString(const Feature *feature, const flatbuffers::Vector<double> &pXy)
{
    const auto pEnds = feature->ends();
    if (pEnds == nullptr)
        return CPLErrorInvalidPointer("MultiLineString ends data");
    const auto mls = new OGRMultiLineString();
    uint32_t offset = 0;
    for (uint32_t i = 0; i < pEnds->size(); i++) {
        const auto e = pEnds->Get(i);
        const auto ls = readLineString(feature, pXy, e - offset, offset);
        if (ls == nullptr) {
            delete mls;
            return nullptr;
        }
        mls->addGeometryDirectly(ls);
        offset = e;
    }
    return mls;
}

OGRLinearRing *OGRFlatGeobufLayer::readLinearRing(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset)
{
    const auto lr = new OGRLinearRing();
    if (readSimpleCurve(feature, pXy, len, offset, lr) != OGRERR_NONE) {
        delete lr;
        return nullptr;
    }
    return lr;
}

OGRErr OGRFlatGeobufLayer::readSimpleCurve(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset, OGRSimpleCurve *sc)
{
    if (offset > feature_max_buffer_size || len > feature_max_buffer_size - offset)
        return CPLErrorInvalidSize("curve offset max");
    const uint32_t offsetLen = len + offset;
    if (offsetLen > pXy.size() / 2)
        return CPLErrorInvalidSize("curve XY offset");
    const auto aXy = pXy.data();
    const auto ogrXY = reinterpret_cast<const OGRRawPoint *>(aXy) + offset;
    if (m_hasZ) {
        const auto pZ = feature->z();
        if (pZ == nullptr) {
            CPLErrorInvalidPointer("Z data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pZ->size())
            return CPLErrorInvalidSize("curve Z offset");
        const auto aZ = pZ->data();
        if (m_hasM) {
            const auto pM = feature->m();
            if (pM == nullptr) {
                CPLErrorInvalidPointer("M data");
                return OGRERR_CORRUPT_DATA;
            }
            if (offsetLen > pM->size())
                return CPLErrorInvalidSize("curve M offset");
            const auto aM = pM->data();
#if CPL_IS_LSB
            sc->setPoints(len, ogrXY, aZ + offset, aM + offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPoint(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aZ[offset + i]),
                             flatbuffers::EndianScalar(aM[offset + i]));
            }
#endif
        } else {
#if CPL_IS_LSB
            sc->setPoints(len, ogrXY, aZ + offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPoint(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aZ[offset + i]));
            }
#endif
        }
    } else if (m_hasM) {
        const auto pM = feature->m();
        if (pM == nullptr) {
            CPLErrorInvalidPointer("M data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pM->size())
            return CPLErrorInvalidSize("curve M offset");
        const auto aM = pM->data();
#if CPL_IS_LSB
        sc->setPointsM(len, ogrXY, aM + offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPointM(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aM[offset + i]));
            }
#endif
    } else {
#if CPL_IS_LSB
        sc->setPoints(len, ogrXY);
#else
        sc->setNumPoints(len, false);
        for( uint32_t i = 0; i < len; i++ )
        {
            sc->setPoint(i,
                         flatbuffers::EndianScalar(ogrXY[i].x),
                         flatbuffers::EndianScalar(ogrXY[i].y));
        }
#endif
    }
    return OGRERR_NONE;
}

OGRPolygon *OGRFlatGeobufLayer::readPolygon(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset)
{
    const auto pEnds = feature->ends();
    const auto p = new OGRPolygon();
    if (pEnds == nullptr || pEnds->size() < 2) {
        const auto lr = readLinearRing(feature, pXy, len / 2);
        if (lr == nullptr) {
            delete p;
            return nullptr;
        }
        p->addRingDirectly(lr);
    } else {
        for (uint32_t i = 0; i < pEnds->size(); i++) {
            const auto e = pEnds->Get(i);
            const auto lr = readLinearRing(feature, pXy, e - offset, offset);
            offset = e;
            if (lr == nullptr)
                continue;
            p->addRingDirectly(lr);
        }
        if (p->IsEmpty()) {
            delete p;
            return nullptr;
        }
    }
    return p;
}

OGRMultiPolygon *OGRFlatGeobufLayer::readMultiPolygon(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len)
{
    const auto pLengths = feature->lengths();
    if (pLengths == nullptr || pLengths->size() < 2) {
        const auto mp = new OGRMultiPolygon();
        const auto p = readPolygon(feature, pXy, len);
        if (p == nullptr) {
            delete p;
            delete mp;
            return CPLErrorInvalidPointer("MultiPolygon part result");
        }
        mp->addGeometryDirectly(p);
        return mp;
    } else {
        const auto pEnds = feature->ends();
        if (pEnds == nullptr)
            return CPLErrorInvalidPointer("MultiPolygon ends data");
        uint32_t offset = 0;
        uint32_t roffset = 0;
        const auto mp = new OGRMultiPolygon();
        for (uint32_t i = 0; i < pLengths->size(); i++) {
            const auto p = new OGRPolygon();
            uint32_t ringCount = pLengths->Get(i);
            for (uint32_t j = 0; j < ringCount; j++) {
                if (roffset >= pEnds->size()) {
                    delete p;
                    delete mp;
                    return CPLErrorInvalidLength("MultiPolygon ends data");
                }
                uint32_t e = pEnds->Get(roffset++);
                const auto lr = readLinearRing(feature, pXy, e - offset, offset);
                offset = e;
                if (lr == nullptr)
                    continue;
                p->addRingDirectly(lr);
            }
            if (p->IsEmpty()) {
                delete p;
                delete mp;
                return CPLErrorInvalidLength("MultiPolygon part");
            }
            mp->addGeometryDirectly(p);
        }
        return mp;
    }
}

OGRGeometry *OGRFlatGeobufLayer::readGeometry(const Feature *feature)
{
    const auto pXy = feature->xy();
    if (pXy == nullptr)
        return CPLErrorInvalidPointer("XY data");
    if (m_hasZ && feature->z() == nullptr)
        return CPLErrorInvalidPointer("Z data");
    if (m_hasM && feature->m() == nullptr)
        return CPLErrorInvalidPointer("M data");
    const auto xySize = pXy->size();
    if (xySize >= (feature_max_buffer_size / sizeof(OGRRawPoint)))
        return CPLErrorInvalidLength("XY data");
    switch (m_geometryType) {
        case GeometryType::Point:
            return readPoint(feature, *pXy);
        case GeometryType::MultiPoint:
            return readMultiPoint(feature, *pXy, xySize / 2);
        case GeometryType::LineString:
            return readLineString(feature, *pXy, xySize / 2);
        case GeometryType::MultiLineString:
            return readMultiLineString(feature, *pXy);
        case GeometryType::Polygon:
            return readPolygon(feature, *pXy, xySize);
        case GeometryType::MultiPolygon:
            return readMultiPolygon(feature, *pXy, xySize);
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "readGeometry: Unknown FlatGeobuf::GeometryType %d", (int) m_geometryType);
    }
    return nullptr;
}

OGRErr OGRFlatGeobufLayer::CreateField(OGRFieldDefn *poField, int /* bApproxOK */)
{
    // CPLDebug("FlatGeobuf", "CreateField %s %s", poField->GetNameRef(), poField->GetFieldTypeName(poField->GetType()));
    if(!TestCapability(OLCCreateField))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

    m_poFeatureDefn->AddFieldDefn(poField);

    return OGRERR_NONE;
}

OGRErr OGRFlatGeobufLayer::ICreateFeature(OGRFeature *poNewFeature)
{
    if (!m_bCanCreate) {
        CPLError(CE_Failure, CPLE_AppDefined, "Source not valid for direct conversion");
        return OGRERR_FAILURE;
    }

    const auto fieldCount = m_poFeatureDefn->GetFieldCount();

    if (fieldCount >= std::numeric_limits<uint16_t>::max()) {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create features with more than 65536 columns");
        return OGRERR_FAILURE;
    }

    std::vector<uint8_t> properties;
    properties.reserve(1024 * 4);
    FlatBufferBuilder fbb;

    for (int i = 0; i < fieldCount; i++) {
        const auto fieldDef = m_poFeatureDefn->GetFieldDefn(i);
        if (!poNewFeature->IsFieldSetAndNotNull(i))
            continue;

        uint16_t column_index_le = static_cast<uint16_t>(i);
        CPL_LSBPTR16(&column_index_le);
        std::copy(reinterpret_cast<const uint8_t *>(&column_index_le), reinterpret_cast<const uint8_t *>(&column_index_le + 1), std::back_inserter(properties));

        const auto fieldType = fieldDef->GetType();
        const auto field = poNewFeature->GetRawFieldRef(i);
        switch (fieldType) {
            case OGRFieldType::OFTInteger: {
                int nVal = field->Integer;
                CPL_LSBPTR32(&nVal);
                std::copy(reinterpret_cast<const uint8_t *>(&nVal), reinterpret_cast<const uint8_t *>(&nVal + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTInteger64: {
                GIntBig nVal = field->Integer64;
                CPL_LSBPTR64(&nVal);
                std::copy(reinterpret_cast<const uint8_t *>(&nVal), reinterpret_cast<const uint8_t *>(&nVal + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTReal: {
                double dfVal = field->Real;
                CPL_LSBPTR64(&dfVal);
                std::copy(reinterpret_cast<const uint8_t *>(&dfVal), reinterpret_cast<const uint8_t *>(&dfVal + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTDate:
            case OGRFieldType::OFTTime:
            case OGRFieldType::OFTDateTime: {
                char *str = OGRGetXMLDateTime(field);
                size_t len = strlen(str);
                uint32_t l_le = static_cast<uint32_t>(len);
                CPL_LSBPTR32(&l_le);
                std::copy(reinterpret_cast<const uint8_t *>(&l_le), reinterpret_cast<const uint8_t *>(&l_le + 1), std::back_inserter(properties));
                std::copy(str, str + len, std::back_inserter(properties));
                CPLFree(str);
                break;
            }
            case OGRFieldType::OFTString: {
                size_t len = strlen(field->String);
                if (len >= feature_max_buffer_size) {
                    CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: String too long");
                    return OGRERR_FAILURE;
                }
                uint32_t l_le = static_cast<uint32_t>(len);
                CPL_LSBPTR32(&l_le);
                std::copy(reinterpret_cast<const uint8_t *>(&l_le), reinterpret_cast<const uint8_t *>(&l_le + 1), std::back_inserter(properties));
                std::copy(field->String, field->String + len, std::back_inserter(properties));
                break;
            }
            default:
                CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Missing implementation for OGRFieldType %d", fieldType);
                return OGRERR_FAILURE;
        }
    }

    const auto ogrGeometry = poNewFeature->GetGeometryRef();
#ifdef DEBUG
    //char *wkt;
    //ogrGeometry->exportToWkt(&wkt);
    //CPLDebug("FlatGeobuf", "poNewFeature as wkt: %s", wkt);
#endif
    if (ogrGeometry == nullptr || ogrGeometry->IsEmpty())
        return OGRERR_NONE;
    if (ogrGeometry->getGeometryType() != m_eGType) {
        CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Mismatched geometry type");
        return OGRERR_FAILURE;
    }

    GeometryContext gc;
    switch (m_geometryType) {
        case GeometryType::Point:
            writePoint(ogrGeometry->toPoint(), gc);
            break;
        case GeometryType::MultiPoint:
            writeMultiPoint(ogrGeometry->toMultiPoint(), gc);
            break;
        case GeometryType::LineString:
            writeLineString(ogrGeometry->toLineString(), gc);
            break;
        case GeometryType::MultiLineString:
            writeMultiLineString(ogrGeometry->toMultiLineString(), gc);
            break;
        case GeometryType::Polygon:
            writePolygon(ogrGeometry->toPolygon(), gc, false, 0);
            break;
        case GeometryType::MultiPolygon:
            writeMultiPolygon(ogrGeometry->toMultiPolygon(), gc);
            break;
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Unknown FlatGeobuf::GeometryType %d", (int) m_geometryType);
            return OGRERR_FAILURE;
    }
    const auto pEnds = gc.ends.empty() ? nullptr : &gc.ends;
    const auto pLengths = gc.lengths.empty() ? nullptr : &gc.lengths;
    const auto pXy = gc.xy.empty() ? nullptr : &gc.xy;
    const auto pZ = gc.z.empty() ? nullptr : &gc.z;
    const auto pM = gc.m.empty() ? nullptr : &gc.m;
    const auto pProperties = properties.empty() ? nullptr : &properties;
    const auto feature = CreateFeatureDirect(fbb, pEnds, pLengths, pXy, pZ, pM, nullptr, nullptr, pProperties);
    fbb.FinishSizePrefixed(feature);

    OGREnvelope psEnvelope;
    ogrGeometry->getEnvelope(&psEnvelope);

    if (m_featuresCount == 0) {
        if (m_poFpWrite == nullptr) {
            CPLErrorInvalidPointer("output file handler");
            return OGRERR_FAILURE;
        }
        WriteHeader(m_poFpWrite, 0, nullptr);
        CPLDebug("FlatGeobuf", "Writing first feature at offset: %lu", static_cast<long unsigned int>(m_writeOffset));
    }

    size_t c = VSIFWriteL(fbb.GetBufferPointer(), 1, fbb.GetSize(), m_poFpWrite);
    if (c == 0)
        return CPLErrorIO("writing feature");
    if (m_bCreateSpatialIndexAtClose) {
        const auto item = std::make_shared<FeatureItem>();
        item->size = static_cast<uint32_t>(fbb.GetSize());
        item->offset = m_writeOffset;
        item->rect = {
            psEnvelope.MinX,
            psEnvelope.MinY,
            psEnvelope.MaxX,
            psEnvelope.MaxY
        };
        m_featureItems.push_back(item);
    }
    m_writeOffset += c;

    m_featuresCount++;

    return OGRERR_NONE;
}

void OGRFlatGeobufLayer::writePoint(OGRPoint *p, GeometryContext &gc)
{
    gc.xy.push_back(p->getX());
    gc.xy.push_back(p->getY());
    if (m_hasZ)
        gc.z.push_back(p->getZ());
    if (m_hasM)
        gc.m.push_back(p->getM());
}

void OGRFlatGeobufLayer::writeMultiPoint(OGRMultiPoint *mp, GeometryContext &gc)
{
    for (int i = 0; i < mp->getNumGeometries(); i++)
        writePoint(mp->getGeometryRef(i)->toPoint(), gc);
}

uint32_t OGRFlatGeobufLayer::writeLineString(OGRLineString *ls, GeometryContext &gc)
{
    uint32_t numPoints = ls->getNumPoints();
    const auto xyLength = gc.xy.size();
    gc.xy.resize(xyLength + (numPoints * 2));
    const auto zLength = gc.z.size();
    double *padfZOut = nullptr;
    if (m_hasZ) {
        gc.z.resize(zLength + numPoints);
        padfZOut = gc.z.data() + zLength;
    }
    const auto mLength = gc.m.size();
    double *padfMOut = nullptr;
    if (m_hasM) {
        gc.m.resize(mLength + numPoints);
        padfMOut = gc.m.data() + mLength;
    }
    ls->getPoints(reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(gc.xy.data() + xyLength)), sizeof(OGRRawPoint),
                  reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(gc.xy.data() + xyLength)) + 1, sizeof(OGRRawPoint),
                  padfZOut, sizeof(double),
                  padfMOut, sizeof(double));
    return numPoints;
}

void OGRFlatGeobufLayer::writeMultiLineString(OGRMultiLineString *mls, GeometryContext &gc)
{
    uint32_t e = 0;
    const auto numGeometries = mls->getNumGeometries();
    for (int i = 0; i < numGeometries; i++)
    {
        e += writeLineString(mls->getGeometryRef(i)->toLineString(), gc);
        gc.ends.push_back(e);
    }
}

uint32_t OGRFlatGeobufLayer::writePolygon(OGRPolygon *p, GeometryContext &gc, bool isMulti, uint32_t e)
{
    const auto exteriorRing = p->getExteriorRing();
    const auto numInteriorRings = p->getNumInteriorRings();
    e += writeLineString(exteriorRing, gc);
    if (numInteriorRings > 0 || isMulti) {
        gc.ends.push_back(e);
        for (int i = 0; i < numInteriorRings; i++)
        {
            e += writeLineString(p->getInteriorRing(i), gc);
            gc.ends.push_back(e);
        }
    }
    return e;
}

void OGRFlatGeobufLayer::writeMultiPolygon(OGRMultiPolygon *mp, GeometryContext &gc)
{
    uint32_t e = 0;
    const auto isMulti = mp->getNumGeometries() > 1;
    for (int i = 0; i < mp->getNumGeometries(); i++) {
        const auto p = mp->getGeometryRef(i)->toPolygon();
        e = writePolygon(p, gc, isMulti, e);
        if (isMulti)
            gc.lengths.push_back(p->getNumInteriorRings() + 1);
    }
}

OGRErr OGRFlatGeobufLayer::GetExtent(OGREnvelope* psExtent, int bForce)
{
    if( m_sExtent.IsInit() )
    {
        *psExtent = m_sExtent;
        return OGRERR_NONE;
    }
    return OGRLayer::GetExtent(psExtent, bForce);
}

int OGRFlatGeobufLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return m_create;
    else if (EQUAL(pszCap, ODsCCreateLayer))
        return m_create;
    else if (EQUAL(pszCap, OLCCreateField))
        return m_create;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return m_create;
    else if (EQUAL(pszCap, OLCCreateGeomField))
        return m_create;
    else if (EQUAL(pszCap, OLCIgnoreFields))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return m_sExtent.IsInit();
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return true;
    else
        return false;
}

void OGRFlatGeobufLayer::ResetReading()
{
    CPLDebug("FlatGeobuf", "ResetReading");
    m_offset = m_offsetFeatures;
    m_featuresPos = 0;
    m_indexOffsets.clear();
    m_featuresCount = m_poHeader ? m_poHeader->features_count() : 0;
    m_queriedSpatialIndex = false;
    m_ignoreSpatialFilter = false;
    m_ignoreAttributeFilter = false;
    return;
}
