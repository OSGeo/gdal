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

static std::nullptr_t CPLErrorInvalidPointer() {
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected nullptr - possible data corruption");
    return nullptr;
}

static std::nullptr_t CPLErrorInvalidLength() {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid length detected - possible data corruption");
    return nullptr;
}

static OGRErr CPLErrorInvalidSize() {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid size detected - possible data corruption");
    return OGRERR_CORRUPT_DATA;
}

static OGRErr CPLErrorMemoryAllocation() {
    CPLError(CE_Failure, CPLE_AppDefined, "Could not allocate memory");
    return OGRERR_NOT_ENOUGH_MEMORY;
}

static OGRErr CPLErrorIO() {
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected I/O failure");
    return OGRERR_FAILURE;
}

OGRFlatGeobufLayer::OGRFlatGeobufLayer(const Header *poHeader, GByte *headerBuf, const char *pszFilename, uint64_t offset)
{
    CPLDebug("FlatGeobuf", "offset: %lu", static_cast<long unsigned int>(offset));

    m_poHeader = poHeader;
    m_headerBuf = headerBuf;

    CPLAssert(poHeader);
    CPLAssert(pszFilename);

    if (pszFilename)
        m_osFilename = pszFilename;
    m_offsetInit = offset;
    m_offset = offset;
    m_create = false;

    m_featuresCount = m_poHeader->features_count();
    CPLDebug("FlatGeobuf", "m_featuresCount: %lu", static_cast<long unsigned int>(m_featuresCount));
    m_geometryType = m_poHeader->geometry_type();
    m_hasZ = m_poHeader->hasZ();
    m_hasM = m_poHeader->hasM();
    m_hasT = m_poHeader->hasT();

    CPLDebug("FlatGeobuf", "m_hasZ: %d", m_hasZ);
    CPLDebug("FlatGeobuf", "m_hasM: %d", m_hasM);
    CPLDebug("FlatGeobuf", "m_hasT: %d", m_hasT);

    auto crs = m_poHeader->crs();
    if (crs != nullptr) {
        m_poSRS = new OGRSpatialReference();
        auto org = crs->org();
        auto code = crs->code();
        auto wkt = crs->wkt();
        m_poSRS->SetAuthority(nullptr, org->c_str(), code);
        if (org == nullptr && code != 0) {
            m_poSRS->importFromEPSG(code);
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
    OGRwkbGeometryType eGType)
{
    CPLDebug("FlatGeobuf", "Request to create layer %s", pszLayerName);

    if (pszLayerName)
        m_osLayerName = pszLayerName;
    if (pszFilename)
        m_osFilename = pszFilename;
    m_create = true;
    m_eGType = eGType;
    if (!translateOGRwkbGeometryType())
        bCanCreate = false;
    if (poSpatialRef)
        m_poSRS = poSpatialRef->Clone();

    CPLDebug("FlatGeobuf", "eGType: %d", (int) eGType);
    CPLDebug("FlatGeobuf", "m_geometryType: %d", (int) m_geometryType);
    CPLDebug("FlatGeobuf", "m_hasZ: %d", m_hasZ);
    CPLDebug("FlatGeobuf", "m_hasM: %d", m_hasM);
    CPLDebug("FlatGeobuf", "m_hasT: %d", m_hasT);

    SetMetadataItem(OLMD_FID64, "YES");

    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGType);
    m_poFeatureDefn->Reference();
}

bool OGRFlatGeobufLayer::translateOGRwkbGeometryType()
{
    auto flatType = wkbFlatten(m_eGType);
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
        auto field = m_poFeatureDefn->GetFieldDefn(i);
        auto name = field->GetNameRef();
        auto columnType = toColumnType(field->GetType(), field->GetSubType());
        CPLDebug("FlatGeobuf", "Create column %s (index %d)", name, i);
        auto column = CreateColumnDirect(fbb, name, columnType);
        columns.push_back(column);
    }
    return columns;
}

void OGRFlatGeobufLayer::readColumns()
{
    auto columns = m_poHeader->columns();
    if (columns == nullptr)
        return;
    for (uint32_t i = 0; i < columns->size(); i++) {
        auto column = columns->Get(i);
        auto name = column->name()->c_str();
        auto type = toOGRFieldType(column->type());
        OGRFieldDefn field(name, type);
        m_poFeatureDefn->AddFieldDefn(&field);
    }
}

void OGRFlatGeobufLayer::Create() {
    CPLDebug("FlatGeobuf", "Request to create %lu features", static_cast<long unsigned int>(m_featuresCount));

    if (m_featuresCount >= std::numeric_limits<size_t>::max() / 8) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features for this architecture");
        return;
    }

    size_t c;
    uint64_t offset = 0;

    m_poFp = VSIFOpenL(m_osFilename.c_str(), "wb");
    if (m_poFp == nullptr) {
        CPLError(CE_Failure, CPLE_OpenFailed,
                    "Failed to create %s:\n%s",
                    m_osFilename.c_str(), VSIStrerror(errno));
        return;
    }

    c = VSIFWriteL(&magicbytes, sizeof(magicbytes), 1, m_poFp);
    CPLDebug("FlatGeobuf", "Wrote magicbytes (%lu bytes)", static_cast<long unsigned int>(c * sizeof(magicbytes)));
    offset += c;

    Rect extent = calcExtent(m_featureItems);
    const auto extentVector = extent.toVector();

    FlatBufferBuilder fbb;
    auto columns = writeColumns(fbb);

    if (m_featuresCount == 0) {
        CPLDebug("FlatGeobuf", "Spatial index cannot be created without any features");
        bCreateSpatialIndexAtClose = false;
    }

    uint16_t indexNodeSize = bCreateSpatialIndexAtClose ? 16 : 0;

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
        if (nAuthorityCode != 0) {
            CPLDebug("FlatGeobuf", "Creating CRS with code %d", nAuthorityCode);
            crs = CreateCrsDirect(fbb, pszAuthorityName, nAuthorityCode, m_poSRS->GetName(), nullptr, pszWKT);
        }
        CPLFree(pszWKT);
    }

    auto header = CreateHeaderDirect(
        fbb, m_osLayerName.c_str(), &extentVector, m_geometryType, m_hasZ, m_hasM, m_hasT, m_hasTM, &columns, m_featuresCount, indexNodeSize, crs);
    fbb.FinishSizePrefixed(header);
    c = VSIFWriteL(fbb.GetBufferPointer(), 1, fbb.GetSize(), m_poFp);
    CPLDebug("FlatGeobuf", "Wrote header (%lu bytes)", static_cast<long unsigned int>(c));
    offset += c;

    if (bCreateSpatialIndexAtClose) {
        CPLDebug("FlatGeobuf", "Sorting items for Packed R-tree");
        hilbertSort(m_featureItems);
        CPLDebug("FlatGeobuf", "Creating Packed R-tree");
        try {
            PackedRTree tree(m_featureItems, extent);
            CPLDebug("FlatGeobuf", "PackedRTree extent %f, %f, %f, %f", extentVector[0], extentVector[1], extentVector[2], extentVector[3]);
            tree.streamWrite([this, &c] (uint8_t *data, size_t size) { c = VSIFWriteL(data, 1, size, m_poFp); });
        } catch (const std::exception& e) {
            CPLError(CE_Failure, CPLE_AppDefined, "Create: %s", e.what());
            return;
        }
        CPLDebug("FlatGeobuf", "Wrote tree (%lu bytes)", static_cast<long unsigned int>(c));
        offset += c;
    }

    CPLDebug("FlatGeobuf", "Writing feature offsets at offset %lu", static_cast<long unsigned int>(offset));
    c = 0;
    for (size_t i = 0, foffset = 0; i < m_featuresCount; i++) {
        c += VSIFWriteL(&foffset, 8, 1, m_poFp);
        foffset += std::static_pointer_cast<FeatureItem>(m_featureItems[i])->size;
    }
    CPLDebug("FlatGeobuf", "Wrote feature offsets (%lu bytes)", static_cast<long unsigned int>(c * 8));
    offset += c * 8;

    CPLDebug("FlatGeobuf", "Writing feature buffers at offset %lu", static_cast<long unsigned int>(offset));
    c = 0;
    for (size_t i = 0; i < m_featuresCount; i++) {
        auto item = std::static_pointer_cast<FeatureItem>(m_featureItems[i]);
        c += VSIFWriteL(item->data, 1, item->size, m_poFp);
    }
    CPLDebug("FlatGeobuf", "Wrote feature buffers (%lu bytes)", static_cast<long unsigned int>(c));
    offset += c;

    CPLDebug("FlatGeobuf", "Now at offset %lu", static_cast<long unsigned int>(offset));
}

OGRFlatGeobufLayer::~OGRFlatGeobufLayer()
{
    if (m_create)
        Create();

    if (m_poFp)
        VSIFCloseL(m_poFp);

    if (m_poFeatureDefn)
        m_poFeatureDefn->Release();

    if (m_poSRS)
        m_poSRS->Release();

    if (m_featureBuf)
        VSIFree(m_featureBuf);

    if (m_featureOffsets)
        VSIFree(m_featureOffsets);

    if (m_headerBuf)
        VSIFree(m_headerBuf);
}

OGRFeature *OGRFlatGeobufLayer::GetFeature(GIntBig nFeatureId)
{
    if (static_cast<uint64_t>(nFeatureId) >= m_featuresCount)
        return nullptr;
    ResetReading();
    m_ignoreSpatialFilter = true;
    m_ignoreAttributeFilter = true;
    m_offset = m_offsetInit + m_featureOffsets[nFeatureId];
    OGRFeature *poFeature = GetNextFeature();
    if (poFeature != nullptr)
        poFeature->SetFID(nFeatureId);
    ResetReading();
    return poFeature;
}

OGRErr OGRFlatGeobufLayer::readIndex()
{
    if (m_queriedSpatialIndex)
        return OGRERR_NONE;

    auto indexNodeSize = m_poHeader->index_node_size();
    auto featuresCount = m_poHeader->features_count();
    auto featureOffetsCount = static_cast<size_t>(featuresCount);
    auto featureOffsetsSize = featureOffetsCount * 8;

    if (m_poFp == nullptr) {
        CPLDebug("FlatGeobuf", "readIndex: (will attempt to open file %s)", m_osFilename.c_str());
        m_poFp = VSIFOpenL(m_osFilename.c_str(), "rb");
        if (m_poFp == nullptr) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to open file");
            return OGRERR_FAILURE;
        }
        //m_poFp = (VSILFILE*) VSICreateCachedFile ( (VSIVirtualHandle*) m_poFp);
    }

    if (VSIFSeekL(m_poFp, sizeof(magicbytes), SEEK_SET) == -1) // skip magic bytes
        return CPLErrorIO();
    uoffset_t headerSize;
    if (VSIFReadL(&headerSize, sizeof(uoffset_t), 1, m_poFp) != 1)
        return CPLErrorIO();
    CPL_LSBPTR32(&headerSize);

    try {
        auto treeSize = indexNodeSize > 0 ? PackedRTree::size(featuresCount) : 0;
        if (treeSize > 0 && m_poFilterGeom && !m_ignoreSpatialFilter) {
            CPLDebug("FlatGeobuf", "Attempting spatial index query");
            OGREnvelope env;
            m_poFilterGeom->getEnvelope(&env);
            Rect r { env.MinX, env.MinY, env.MaxX, env.MaxY };
            CPLDebug("FlatGeobuf", "Spatial index search on %f,%f,%f,%f", env.MinX, env.MinY, env.MaxX, env.MaxY);
            auto readNode = [this, headerSize] (uint8_t *buf, size_t i, size_t s) {
                if (VSIFSeekL(m_poFp, sizeof(magicbytes) + sizeof(uoffset_t) + headerSize + i, SEEK_SET) == -1)
                    throw std::runtime_error("I/O seek failure");
                if (VSIFReadL(buf, 1, s, m_poFp) != s)
                    throw std::runtime_error("I/O read file");
            };
            m_foundFeatureIndices = PackedRTree::streamSearch(featuresCount, indexNodeSize, r, readNode);
            m_featuresCount = m_foundFeatureIndices.size();
            CPLDebug("FlatGeobuf", "%lu features found in spatial index search", static_cast<long unsigned int>(m_featuresCount));
            m_queriedSpatialIndex = true;
        }

        if (!m_featureOffsets) {
            CPLDebug("FlatGeobuf", "Seek to feature offsets index position");
            if (VSIFSeekL(m_poFp, sizeof(magicbytes) + sizeof(uoffset_t) + headerSize + treeSize, SEEK_SET) == -1)
                return CPLErrorIO();
            m_featureOffsets = static_cast<uint64_t *>(VSI_MALLOC_VERBOSE(featureOffsetsSize));
            if (!m_featureOffsets)
                return CPLErrorMemoryAllocation();
            CPLDebug("FlatGeobuf", "Reading feature offsets index");
            if (VSIFReadL(m_featureOffsets, 8, featureOffetsCount, m_poFp) != featureOffetsCount)
                return CPLErrorIO();
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
    while( true ) {
        if (m_featuresPos >= m_featuresCount) {
            CPLDebug("FlatGeobuf", "GetNextFeature: iteration end at %lu", static_cast<long unsigned int>(m_featuresPos));
            if (m_poFp != nullptr) {
                VSIFCloseL(m_poFp);
                m_poFp = nullptr;
            }
            return nullptr;
        }

        if (m_poFp == nullptr) {
            CPLDebug("FlatGeobuf", "GetNextFeature: iteration start (will attempt to open file %s)", m_osFilename.c_str());
            m_poFp = VSIFOpenL(m_osFilename.c_str(), "rb");
            if (m_poFp == nullptr) {
                CPLError(CE_Failure, CPLE_AppDefined, "Failed to open file");
                return nullptr;
            }
            //m_poFp = (VSILFILE*) VSICreateCachedFile ( (VSIVirtualHandle*) m_poFp);
        }

        if (readIndex() != OGRERR_NONE) {
            CPLError(CE_Failure, CPLE_AppDefined, "Fatal error querying spatial index");
            ResetReading();
            return nullptr;
        }

        if (m_featuresCount == 0) {
            CPLDebug("FlatGeobuf", "GetNextFeature: no features found");
            if (m_poFp != nullptr) {
                VSIFCloseL(m_poFp);
                m_poFp = nullptr;
            }
            return nullptr;
        }

        OGRFeature *poFeature = new OGRFeature(m_poFeatureDefn);
        OGRGeometry *ogrGeometry = nullptr;
        if (parseFeature(poFeature, &ogrGeometry) != OGRERR_NONE) {
            CPLError(CE_Failure, CPLE_AppDefined, "Fatal error parsing feature");
            delete poFeature;
            ResetReading();
            return nullptr;
        }

        m_featuresPos++;

        if ((m_poFilterGeom == nullptr || m_ignoreSpatialFilter || FilterGeometry(ogrGeometry)) &&
            (m_poAttrQuery == nullptr || m_ignoreAttributeFilter || m_poAttrQuery->Evaluate(poFeature)))
            return poFeature;
    }
}

OGRErr OGRFlatGeobufLayer::parseFeature(OGRFeature *poFeature, OGRGeometry **ogrGeometry) {
    GIntBig fid;
    if (m_queriedSpatialIndex && !m_ignoreSpatialFilter) {
        auto i = m_foundFeatureIndices[static_cast<size_t>(m_featuresPos)];
        m_offset = m_offsetInit + m_featureOffsets[i];
        fid = i;
    } else if (m_featuresPos > 0) {
        m_offset += m_featureSize + sizeof(uoffset_t);
        fid = m_featuresPos;
    } else {
        fid = m_featuresPos;
    }
    poFeature->SetFID(fid);

    if (VSIFSeekL(m_poFp, m_offset, SEEK_SET) == -1)
        return CPLErrorIO();
    if (VSIFReadL(&m_featureSize, sizeof(uoffset_t), 1, m_poFp) != 1)
        return CPLErrorIO();
    CPL_LSBPTR32(&m_featureSize);
    if (m_featureSize > feature_max_buffer_size) {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature size too large (>= 2GB)");
        return OGRERR_CORRUPT_DATA;
    }
    if (m_featureBufSize == 0) {
        m_featureBufSize = std::max(1024U * 32U, m_featureSize);
        CPLDebug("FlatGeobuf", "GetNextFeature: m_featureBufSize: %d", m_featureBufSize);
        m_featureBuf = static_cast<GByte *>(VSIMalloc(m_featureBufSize));
        if (m_featureBuf == nullptr)
            return CPLErrorMemoryAllocation();
    } else if (m_featureBufSize < m_featureSize) {
        m_featureBufSize = std::max(m_featureBufSize * 2, m_featureSize);
        CPLDebug("FlatGeobuf", "GetNextFeature: m_featureBufSize: %d", m_featureBufSize);
        auto featureBuf = static_cast<GByte *>(VSIRealloc(m_featureBuf, m_featureBufSize));
        if (featureBuf == nullptr)
            return CPLErrorMemoryAllocation();
        m_featureBuf = featureBuf;
    }
    if (VSIFReadL(m_featureBuf, 1, m_featureSize, m_poFp) != m_featureSize)
        return CPLErrorIO();

    if (bVerifyBuffers) {
        const uint8_t * vBuf = const_cast<const uint8_t *>(reinterpret_cast<uint8_t *>(m_featureBuf));
        Verifier v(vBuf, m_featureSize);
        auto ok = VerifyFeatureBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer verification failed");
            CPLDebug("FlatGeobuf", "m_offset: %lu", static_cast<long unsigned int>(m_offset));
            CPLDebug("FlatGeobuf", "m_featuresPos: %lu", static_cast<long unsigned int>(m_featuresPos));
            CPLDebug("FlatGeobuf", "featureSize: %d", m_featureSize);
            return OGRERR_CORRUPT_DATA;
        }
    }

    auto feature = GetRoot<Feature>(m_featureBuf);
    if (!m_poFeatureDefn->IsGeometryIgnored()) {
        *ogrGeometry = readGeometry(feature);
        if (*ogrGeometry == nullptr) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to read geometry");
            return OGRERR_CORRUPT_DATA;
        }
        if (m_poSRS != nullptr)
            (*ogrGeometry)->assignSpatialReference(m_poSRS);
        poFeature->SetGeometryDirectly(*ogrGeometry);
    }
    #ifdef DEBUG
        //char *wkt;
        //ogrGeometry->exportToWkt(&wkt);
        //CPLDebug("FlatGeobuf", "readGeometry as wkt: %s", wkt);
    #endif
    auto properties = feature->properties();
    if (properties != nullptr) {
        auto data = properties->data();
        auto size = properties->size();
        //CPLDebug("FlatGeobuf", "properties->size: %d", size);
        uoffset_t offset = 0;
        // size must be at least large enough to contain
        // a single column index and smallest value type
        if (size > 0 && size < (sizeof(uint16_t) + sizeof(uint8_t)))
            return CPLErrorInvalidSize();
        while (offset < (size - 1)) {
            if (offset + sizeof(uint16_t) > size)
                return CPLErrorInvalidSize();
            uint16_t i = *((uint16_t *)(data + offset));
            offset += sizeof(uint16_t);
            //CPLDebug("FlatGeobuf", "i: %d", i);
            auto columns = m_poHeader->columns();
            if (columns == nullptr) {
                CPLError(CE_Failure, CPLE_AppDefined, "Unexpected undefined columns");
                return OGRERR_CORRUPT_DATA;
            }
            if (i >= columns->size()) {
                CPLError(CE_Failure, CPLE_AppDefined, "Column index out of range");
                return OGRERR_CORRUPT_DATA;
            }
            auto column = columns->Get(i);
            auto type = column->type();
            auto isIgnored = poFeature->GetFieldDefnRef(i)->IsIgnored();
            auto ogrField = poFeature->GetRawFieldRef(i);
            switch (type) {
                case ColumnType::Int:
                    if (offset + sizeof(int32_t) > size)
                        return CPLErrorInvalidSize();
                    if (!isIgnored)
                        ogrField->Integer = *((int32_t *)(data + offset));
                    offset += sizeof(int32_t);
                    break;
                case ColumnType::Long:
                    if (offset + sizeof(int64_t) > size)
                        return CPLErrorInvalidSize();
                    if (!isIgnored)
                        ogrField->Integer64 = *((int64_t *)(data + offset));
                    offset += sizeof(int64_t);
                    break;
                case ColumnType::Double:
                    if (offset + sizeof(double) > size)
                        return CPLErrorInvalidSize();
                    if (!isIgnored)
                        ogrField->Real = *((double *)(data + offset));
                    offset += sizeof(double);
                    break;
                case ColumnType::DateTime: {
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize();
                    uint32_t len = *((uint32_t *)(data + offset));
                    offset += sizeof(uint32_t);
                    if (offset + len > size)
                        return CPLErrorInvalidSize();
                    char *str = (char *) VSIMalloc(len + 1);
                    memcpy(str, data + offset, len);
                    offset += len;
                    str[len] = '\0';
                    if (!isIgnored)
                        OGRParseDate(str, ogrField, 0);
                    break;
                }
                case ColumnType::String: {
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize();
                    uint32_t len = *((uint32_t *)(data + offset));
                    offset += sizeof(uint32_t);
                    if (offset + len > size)
                        return CPLErrorInvalidSize();
                    uint8_t *str = (uint8_t *) VSIMalloc(len + 1);
                    memcpy(str, data + offset, len);
                    offset += len;
                    str[len] = '\0';
                    if (!isIgnored)
                        ogrField->String = (char *) str;
                    break;
                }
                default:
                    CPLError(CE_Failure, CPLE_AppDefined, "GetNextFeature: Unknown column->type: %d", (int) type);
            }
        }
    }
    return OGRERR_NONE;
}

OGRPoint *OGRFlatGeobufLayer::readPoint(const Feature *feature, uint32_t offset)
{
    auto aXy = feature->xy()->data();
    if (m_hasZ) {
        auto aZ = feature->z()->data();
        if (m_hasM) {
            auto aM = feature->m()->data();
            return new OGRPoint { aXy[offset + 0], aXy[offset + 1], aZ[offset], aM[offset] };
        } else {
            return new OGRPoint { aXy[offset + 0], aXy[offset + 1], aZ[offset] };
        }
    } else if (m_hasM) {
        auto aM = feature->m()->data();
        return new OGRPoint { aXy[offset + 0], aXy[offset + 1], 0.0, aM[offset] };
    } else {
        return new OGRPoint { aXy[offset + 0], aXy[offset + 1] };
    }
}

OGRMultiPoint *OGRFlatGeobufLayer::readMultiPoint(const Feature *feature, uint32_t len)
{
    auto mp = new OGRMultiPoint();
    for (uint32_t i = 0; i < len; i = i + 2)
        mp->addGeometryDirectly(readPoint(feature, i));
    return mp;
}

OGRLineString *OGRFlatGeobufLayer::readLineString(const Feature *feature, uint32_t len, uint32_t offset)
{
    auto ls = new OGRLineString();
    readSimpleCurve(feature, len, offset, ls);
    return ls;
}

OGRMultiLineString *OGRFlatGeobufLayer::readMultiLineString(const Feature *feature)
{
    auto ends = feature->ends();
    if (ends == nullptr)
        return CPLErrorInvalidPointer();
    auto mls = new OGRMultiLineString();
    uint32_t offset = 0;
    for (uint32_t i = 0; i < ends->size(); i++) {
        auto e = ends->Get(i);
        mls->addGeometryDirectly(readLineString(feature, e - offset, offset));
        offset = e;
    }
    return mls;
}

OGRLinearRing *OGRFlatGeobufLayer::readLinearRing(const Feature *feature, uint32_t len, uint32_t offset)
{
    auto lr = new OGRLinearRing();
    readSimpleCurve(feature, len, offset, lr);
    return lr;
}

void OGRFlatGeobufLayer::readSimpleCurve(const Feature *feature, uint32_t len, uint32_t offset, OGRSimpleCurve *sc)
{
    auto xy = feature->xy()->data();
    if (m_hasZ) {
        auto aZ = feature->z()->data();
        if (m_hasM) {
            auto aM = feature->m()->data();
            sc->setPoints(len, (OGRRawPoint *) xy + offset, aZ + offset, aM + offset);
        } else {
            sc->setPoints(len, (OGRRawPoint *) xy + offset, aZ + offset);
        }
    } else {
        sc->setPoints(len, (OGRRawPoint *) xy + offset);
    }
}

OGRPolygon *OGRFlatGeobufLayer::readPolygon(const Feature *feature, uint32_t len, uint32_t offset)
{
    auto ends = feature->ends();
    auto p = new OGRPolygon();
    if (ends == nullptr || ends->size() < 2) {
        p->addRingDirectly(readLinearRing(feature, len / 2));
    } else {
        for (uint32_t i = 0; i < ends->size(); i++) {
            auto e = ends->Get(i);
            p->addRingDirectly(readLinearRing(feature, e - offset, offset));
            offset = e;
        }
    }
    return p;
}

OGRMultiPolygon *OGRFlatGeobufLayer::readMultiPolygon(const Feature *feature, uint32_t len)
{
    auto lengths = feature->lengths();
    if (lengths == nullptr || lengths->size() < 2) {
        auto mp = new OGRMultiPolygon();
        mp->addGeometryDirectly(readPolygon(feature, len));
        return mp;
    } else {
        auto ends = feature->ends();
        if (ends == nullptr)
            return CPLErrorInvalidPointer();
        uint32_t offset = 0;
        uint32_t roffset = 0;
        auto mp = new OGRMultiPolygon();
        for (uint32_t i = 0; i < lengths->size(); i++) {
            auto p = new OGRPolygon();
            uint32_t ringCount = lengths->Get(i);
            for (uint32_t j = 0; j < ringCount; j++) {
                uint32_t e = ends->Get(roffset++);
                p->addRingDirectly(readLinearRing(feature, e - offset, offset));
                offset = e;
            }
            mp->addGeometryDirectly(p);
        }
        return mp;
    }
}

OGRGeometry *OGRFlatGeobufLayer::readGeometry(const Feature *feature)
{
    auto pXy = feature->xy();
    if (pXy == nullptr)
        return CPLErrorInvalidPointer();
    if (m_hasZ && feature->z() == nullptr)
        return CPLErrorInvalidPointer();
    if (m_hasM && feature->m() == nullptr)
        return CPLErrorInvalidPointer();
    auto xySize = pXy->size();
    if (xySize >= (feature_max_buffer_size / sizeof(OGRRawPoint)))
        return CPLErrorInvalidLength();
    switch (m_geometryType) {
        case GeometryType::Point:
            return readPoint(feature);
        case GeometryType::MultiPoint:
            return readMultiPoint(feature, xySize);
        case GeometryType::LineString:
            return readLineString(feature, xySize / 2);
        case GeometryType::MultiLineString:
            return readMultiLineString(feature);
        case GeometryType::Polygon:
            return readPolygon(feature, xySize);
        case GeometryType::MultiPolygon:
            return readMultiPolygon(feature, xySize);
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "readGeometry: Unknown FlatGeobuf::GeometryType %d", (int) m_geometryType);
    }
    return nullptr;
}

OGRErr OGRFlatGeobufLayer::CreateField(OGRFieldDefn *poField, int /* bApproxOK */)
{
    CPLDebug("FlatGeobuf", "CreateField %s %s", poField->GetNameRef(), poField->GetFieldTypeName(poField->GetType()));
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
    if (!bCanCreate) {
        CPLError(CE_Failure, CPLE_AppDefined, "Source not valid for direct conversion");
        return OGRERR_FAILURE;
    }

    auto fieldCount = m_poFeatureDefn->GetFieldCount();

    if (fieldCount >= std::numeric_limits<uint16_t>::max()) {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create features with more than 65536 columns");
        return OGRERR_FAILURE;
    }

    std::vector<uint8_t> properties;
    properties.reserve(1024 * 4);
    FlatBufferBuilder fbb;

    for (int i = 0; i < fieldCount; i++) {
        auto fieldDef = m_poFeatureDefn->GetFieldDefn(i);
        if (!poNewFeature->IsFieldSetAndNotNull(i))
            continue;

        uint16_t column_index = static_cast<uint16_t>(i);
        std::copy(reinterpret_cast<const uint8_t *>(&column_index), reinterpret_cast<const uint8_t *>(&column_index + 1), std::back_inserter(properties));

        auto fieldType = fieldDef->GetType();
        auto field = poNewFeature->GetRawFieldRef(i);
        switch (fieldType) {
            case OGRFieldType::OFTInteger: {
                std::copy(reinterpret_cast<const uint8_t *>(&field->Integer), reinterpret_cast<const uint8_t *>(&field->Integer + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTInteger64: {
                std::copy(reinterpret_cast<const uint8_t *>(&field->Integer64), reinterpret_cast<const uint8_t *>(&field->Integer64 + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTReal: {
                std::copy(reinterpret_cast<const uint8_t *>(&field->Real), reinterpret_cast<const uint8_t *>(&field->Real + 1), std::back_inserter(properties));
                break;
            }
            case OGRFieldType::OFTDate:
            case OGRFieldType::OFTTime:
            case OGRFieldType::OFTDateTime: {
                char *str = OGRGetXMLDateTime(field);
                size_t len = strlen(str);
                if (len >= feature_max_buffer_size) {
                    CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: String too long");
                    CPLFree(str);
                    return OGRERR_FAILURE;
                }
                uint32_t l = static_cast<uint32_t>(len);
                std::copy(reinterpret_cast<const uint8_t *>(&l), reinterpret_cast<const uint8_t *>(&l + 1), std::back_inserter(properties));
                std::copy(str, str + l, std::back_inserter(properties));
                CPLFree(str);
                break;
            }
            case OGRFieldType::OFTString: {
                size_t len = strlen(field->String);
                if (len >= feature_max_buffer_size) {
                    CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: String too long");
                    return OGRERR_FAILURE;
                }
                uint32_t l = static_cast<uint32_t>(len);
                std::copy(reinterpret_cast<const uint8_t *>(&l), reinterpret_cast<const uint8_t *>(&l + 1), std::back_inserter(properties));
                std::copy(field->String, field->String + l, std::back_inserter(properties));
                break;
            }
            default:
                CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Missing implementation for OGRFieldType %d", fieldType);
                return OGRERR_FAILURE;
        }
    }

    auto ogrGeometry = poNewFeature->GetGeometryRef();
#ifdef DEBUG
    //char *wkt;
    //ogrGeometry->exportToWkt(&wkt);
    //CPLDebug("FlatGeobuf", "poNewFeature as wkt: %s", wkt);
#endif
    if (ogrGeometry == nullptr)
        return 0;
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
    auto pEnds = gc.ends.size() == 0 ? nullptr : &gc.ends;
    auto pLengths = gc.lengths.size() == 0 ? nullptr : &gc.lengths;
    auto pXy = gc.xy.size() == 0 ? nullptr : &gc.xy;
    auto pZ = gc.z.size() == 0 ? nullptr : &gc.z;
    auto pM = gc.m.size() == 0 ? nullptr : &gc.m;
    auto pProperties = properties.size() == 0 ? nullptr : &properties;
    auto feature = CreateFeatureDirect(fbb, pEnds, pLengths, pXy, pZ, pM, nullptr, nullptr, pProperties);
    fbb.FinishSizePrefixed(feature);

    OGREnvelope psEnvelope;
    ogrGeometry->getEnvelope(&psEnvelope);

    auto item = std::make_shared<FeatureItem>();
    item->buf = fbb.Release();
    item->data = item->buf.data();
    item->size = static_cast<uint32_t>(item->buf.size());
    item->rect = {
        psEnvelope.MinX,
        psEnvelope.MinY,
        psEnvelope.MaxX,
        psEnvelope.MaxY
    };

    m_featureItems.push_back(item);

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
    auto xyLength = gc.xy.size();
    gc.xy.resize(xyLength + (numPoints * 2));
    auto zLength = gc.z.size();
    double *padfZOut = nullptr;
    if (m_hasZ) {
        gc.z.resize(zLength + numPoints);
        padfZOut = gc.z.data() + zLength;
    }
    ls->getPoints(reinterpret_cast<OGRRawPoint *>(gc.xy.data() + xyLength), padfZOut);
    if (m_hasM) {
        for (int i = 0; i < ls->getNumPoints(); i++)
            gc.m.push_back(ls->getM(i));
    }
    return numPoints;
}

void OGRFlatGeobufLayer::writeMultiLineString(OGRMultiLineString *mls, GeometryContext &gc)
{
    auto e = 0;
    if (mls->getNumGeometries() > 1)
        for (int i = 0; i < mls->getNumGeometries(); i++)
            gc.ends.push_back(e += writeLineString(mls->getGeometryRef(i)->toLineString(), gc));
    else
        gc.ends.push_back(writeLineString(mls->getGeometryRef(0)->toLineString(), gc));
}

uint32_t OGRFlatGeobufLayer::writePolygon(OGRPolygon *p, GeometryContext &gc, bool isMulti, uint32_t e)
{
    auto exteriorRing = p->getExteriorRing();
    auto numInteriorRings = p->getNumInteriorRings();
    e += writeLineString(exteriorRing, gc);
    if (numInteriorRings > 0 || isMulti) {
        gc.ends.push_back(e);
        for (int i = 0; i < numInteriorRings; i++)
            gc.ends.push_back(e += writeLineString(p->getInteriorRing(i), gc));
    }
    return e;
}

void OGRFlatGeobufLayer::writeMultiPolygon(OGRMultiPolygon *mp, GeometryContext &gc)
{
    uint32_t e = 0;
    auto isMulti = mp->getNumGeometries() > 1;
    for (int i = 0; i < mp->getNumGeometries(); i++) {
        auto p = mp->getGeometryRef(i)->toPolygon();
        e = writePolygon(p, gc, isMulti, e);
        if (isMulti)
            gc.lengths.push_back(p->getNumInteriorRings() + 1);
    }
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
        return true;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return true;
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return true;
    else
        return false;
}

void OGRFlatGeobufLayer::ResetReading()
{
    CPLDebug("FlatGeobuf", "ResetReading");
    m_offset = m_offsetInit;
    m_featuresPos = 0;
    m_featuresCount = m_poHeader ? m_poHeader->features_count() : 0;
    m_featureSize = 0;
    m_queriedSpatialIndex = false;
    m_ignoreSpatialFilter = false;
    m_ignoreAttributeFilter = false;
    return;
}
