/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements OGRFlatGeobufLayer class.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2020, Björn Harrtell <bjorn at wololo dot org>
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
#include "cplerrors.h"
#include "geometryreader.h"
#include "geometrywriter.h"

#include <algorithm>
#include <stdexcept>

using namespace flatbuffers;
using namespace FlatGeobuf;
using namespace ogr_flatgeobuf;

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
    bool update)
{
    m_poHeader = poHeader;
    CPLAssert(poHeader);
    m_headerBuf = headerBuf;
    CPLAssert(pszFilename);
    if (pszFilename)
        m_osFilename = pszFilename;
    m_poFp = poFp;
    m_offsetFeatures = offset;
    m_offset = offset;
    m_create = false;
    m_update = update;

    m_featuresCount = m_poHeader->features_count();
    m_geometryType = m_poHeader->geometry_type();
    m_indexNodeSize = m_poHeader->index_node_size();
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

    CPLDebugOnly("FlatGeobuf", "geometryType: %d, hasZ: %d, hasM: %d, hasT: %d", (int) m_geometryType, m_hasZ, m_hasM, m_hasT);

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
    bool bCreateSpatialIndexAtClose,
    VSILFILE *poFpWrite,
    std::string &osTempFile) :
    m_eGType(eGType),
    m_bCreateSpatialIndexAtClose(bCreateSpatialIndexAtClose),
    m_poFpWrite(poFpWrite),
    m_osTempFile(osTempFile)
{
    m_create = true;

    if (pszLayerName)
        m_osLayerName = pszLayerName;
    if (pszFilename)
        m_osFilename = pszFilename;
    m_geometryType = GeometryWriter::translateOGRwkbGeometryType(eGType);
    if wkbHasZ(eGType)
        m_hasZ = true;
    if wkbHasM(eGType)
        m_hasM = true;
    if (poSpatialRef)
        m_poSRS = poSpatialRef->Clone();

    CPLDebugOnly("FlatGeobuf", "geometryType: %d, hasZ: %d, hasM: %d, hasT: %d", (int) m_geometryType, m_hasZ, m_hasM, m_hasT);

    SetMetadataItem(OLMD_FID64, "YES");

    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGType);
    m_poFeatureDefn->Reference();
}

OGRwkbGeometryType OGRFlatGeobufLayer::getOGRwkbGeometryType()
{
    OGRwkbGeometryType ogrType = OGRwkbGeometryType::wkbUnknown;
    if (static_cast<int>(m_geometryType) <= 17)
        ogrType = (OGRwkbGeometryType) m_geometryType;
    if (m_hasZ)
        ogrType = wkbSetZ(ogrType);
    if (m_hasM)
        ogrType = wkbSetM(ogrType);
    return ogrType;
}

static ColumnType toColumnType(OGRFieldType type, OGRFieldSubType subType)
{
    switch (type) {
        case OGRFieldType::OFTInteger:
            return subType == OFSTBoolean ? ColumnType::Bool : subType == OFSTInt16 ? ColumnType::Short : ColumnType::Int;
        case OGRFieldType::OFTInteger64: return ColumnType::Long;
        case OGRFieldType::OFTReal:
            return subType == OFSTFloat32 ? ColumnType::Float : ColumnType::Double;
        case OGRFieldType::OFTString: return ColumnType::String;
        case OGRFieldType::OFTDate: return ColumnType::DateTime;
        case OGRFieldType::OFTTime: return ColumnType::DateTime;
        case OGRFieldType::OFTDateTime: return ColumnType::DateTime;
        case OGRFieldType::OFTBinary: return ColumnType::Binary;
        default: CPLError(CE_Failure, CPLE_AppDefined, "toColumnType: Unknown OGRFieldType %d", type);
    }
    return ColumnType::String;
}

static OGRFieldType toOGRFieldType(ColumnType type, OGRFieldSubType& eSubType)
{
    eSubType = OFSTNone;
    switch (type) {
        case ColumnType::Byte: return OGRFieldType::OFTInteger;
        case ColumnType::UByte: return OGRFieldType::OFTInteger;
        case ColumnType::Bool: eSubType = OFSTBoolean; return OGRFieldType::OFTInteger;
        case ColumnType::Short: eSubType = OFSTInt16; return OGRFieldType::OFTInteger;
        case ColumnType::UShort: return OGRFieldType::OFTInteger;
        case ColumnType::Int: return OGRFieldType::OFTInteger;
        case ColumnType::UInt: return OGRFieldType::OFTInteger64;
        case ColumnType::Long: return OGRFieldType::OFTInteger64;
        case ColumnType::ULong: return OGRFieldType::OFTReal;
        case ColumnType::Float: eSubType = OFSTFloat32; return OGRFieldType::OFTReal;
        case ColumnType::Double: return OGRFieldType::OFTReal;
        case ColumnType::String: return OGRFieldType::OFTString;
        case ColumnType::Json: return OGRFieldType::OFTString;
        case ColumnType::DateTime: return OGRFieldType::OFTDateTime;
        case ColumnType::Binary: return OGRFieldType::OFTBinary;
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
        auto title = field->GetAlternativeNameRef();
        if (EQUAL(title, ""))
            title = nullptr;
        const char *description = nullptr;
        auto width = -1;
        auto precision = -1;
        auto scale = field->GetPrecision();
        if (scale == 0)
            scale = -1;
        if (columnType == ColumnType::Float || columnType == ColumnType::Double)
            precision = field->GetWidth();
        else
            width = field->GetWidth();
        auto nullable = CPL_TO_BOOL(field->IsNullable());
        auto unique = CPL_TO_BOOL(field->IsUnique());
        auto primaryKey = false;
        //CPLDebugOnly("FlatGeobuf", "Create column %s (index %d)", name, i);
        const auto column = CreateColumnDirect(fbb, name, columnType, title, description, width, precision, scale, nullable, unique, primaryKey);
        columns.push_back(column);
        //CPLDebugOnly("FlatGeobuf", "DEBUG writeColumns: Created column %s added as index %d", name, i);
    }
    CPLDebugOnly("FlatGeobuf", "Created %lu columns for writing", static_cast<long unsigned int>(columns.size()));
    return columns;
}

void OGRFlatGeobufLayer::readColumns()
{
    const auto columns = m_poHeader->columns();
    if (columns == nullptr)
        return;
    for (uint32_t i = 0; i < columns->size(); i++) {
        const auto column = columns->Get(i);
        const auto type = column->type();
        const auto name = column->name()->c_str();
        const auto title = column->title() != nullptr ? column->title()->c_str() : nullptr;
        const auto width = column->width();
        const auto precision = column->precision();
        const auto scale = column->scale();
        const auto nullable = column->nullable();
        const auto unique = column->unique();
        OGRFieldSubType eSubType = OFSTNone;
        const auto ogrType = toOGRFieldType(column->type(), eSubType);
        OGRFieldDefn field(name, ogrType);
        field.SetSubType(eSubType);
        field.SetAlternativeName(title);
        if (width != -1 && type != ColumnType::Float && type != ColumnType::Double)
            field.SetWidth(width);
        if (precision != -1)
            field.SetWidth(precision);
        field.SetPrecision(scale != -1 ? scale : 0);
        field.SetNullable(nullable);
        field.SetUnique(unique);
        m_poFeatureDefn->AddFieldDefn(&field);
        //CPLDebugOnly("FlatGeobuf", "DEBUG readColumns: Read column %s added as index %d", name, i);
    }
    CPLDebugOnly("FlatGeobuf", "Read %lu columns and added to feature definition", static_cast<long unsigned int>(columns->size()));
}

void OGRFlatGeobufLayer::writeHeader(VSILFILE *poFp, uint64_t featuresCount, std::vector<double> *extentVector) {
    size_t c;
    c = VSIFWriteL(&magicbytes, sizeof(magicbytes), 1, poFp);
    CPLDebugOnly("FlatGeobuf", "Wrote magicbytes (%lu bytes)", static_cast<long unsigned int>(c * sizeof(magicbytes)));
    m_writeOffset += sizeof(magicbytes);

    FlatBufferBuilder fbb;
    auto columns = writeColumns(fbb);

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
    CPLDebugOnly("FlatGeobuf", "Wrote header (%lu bytes)", static_cast<long unsigned int>(c));
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
        CPLDebugOnly("FlatGeobuf", "Writing empty layer");
        writeHeader(m_poFp, 0, nullptr);
        return;
    }

    CPLDebugOnly("FlatGeobuf", "Writing second pass sorted by spatial index");

    const uint64_t nTempFileSize = m_writeOffset;
    m_writeOffset = 0;
    m_indexNodeSize = 16;

    size_t c;

    if (m_featuresCount >= std::numeric_limits<size_t>::max() / 8) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features for this architecture");
        return;
    }

    NodeItem extent = calcExtent(m_featureItems);
    auto extentVector = extent.toVector();

    writeHeader(m_poFp, m_featuresCount, &extentVector);

    CPLDebugOnly("FlatGeobuf", "Sorting items for Packed R-tree");
    hilbertSort(m_featureItems);
    CPLDebugOnly("FlatGeobuf", "Calc new feature offsets");
    uint64_t featureOffset = 0;
    for (auto item : m_featureItems) {
        auto featureItem = std::static_pointer_cast<FeatureItem>(item);
        featureItem->nodeItem.offset = featureOffset;
        featureOffset += featureItem->size;
    }
    CPLDebugOnly("FlatGeobuf", "Creating Packed R-tree");
    c = 0;
    try {
        PackedRTree tree(m_featureItems, extent);
        CPLDebugOnly("FlatGeobuf", "PackedRTree extent %f, %f, %f, %f", extentVector[0], extentVector[1], extentVector[2], extentVector[3]);
        tree.streamWrite([this, &c] (uint8_t *data, size_t size) { c += VSIFWriteL(data, 1, size, m_poFp); });
    } catch (const std::exception& e) {
        CPLError(CE_Failure, CPLE_AppDefined, "Create: %s", e.what());
        return;
    }
    CPLDebugOnly("FlatGeobuf", "Wrote tree (%lu bytes)", static_cast<long unsigned int>(c));
    m_writeOffset += c;

    CPLDebugOnly("FlatGeobuf", "Writing feature buffers at offset %lu", static_cast<long unsigned int>(m_writeOffset));

    c = 0;

    // For temporary files not in memory, we use a batch strategy to write the
    // final file. That is to say we try to separate reads in the source temporary
    // file and writes in the target file as much as possible, and by reading
    // source features in increasing offset within a batch.
    const bool bUseBatchStrategy = !STARTS_WITH(m_osTempFile.c_str(), "/vsimem/");
    if( bUseBatchStrategy )
    {
        const uint32_t nMaxBufferSize = std::max(m_maxFeatureSize,
            static_cast<uint32_t>(std::min(
                static_cast<uint64_t>(100 * 1024 * 1024), nTempFileSize)));
        if( ensureFeatureBuf(nMaxBufferSize) != OGRERR_NONE )
            return;
        uint32_t offsetInBuffer = 0;
        struct BatchItem
        {
            size_t   featureIdx; // index of m_featureItems[]
            uint32_t offsetInBuffer;
        };
        std::vector<BatchItem> batch;

        const auto flushBatch = [this, &batch, &offsetInBuffer]()
        {
            // Sort by increasing source offset
            std::sort(
                batch.begin(), batch.end(),
                [this](const BatchItem& a, const BatchItem& b)
                {
                    return std::static_pointer_cast<FeatureItem>(
                                m_featureItems[a.featureIdx])->offset
                           < std::static_pointer_cast<FeatureItem>(
                                m_featureItems[b.featureIdx])->offset;
                }
            );

            // Read source features
            for( const auto& batchItem: batch )
            {
                const auto item = std::static_pointer_cast<FeatureItem>(
                    m_featureItems[batchItem.featureIdx]);
                if (VSIFSeekL(m_poFpWrite, item->offset, SEEK_SET) == -1) {
                    CPLErrorIO("seeking to temp feature location");
                    return false;
                }
                if (VSIFReadL(m_featureBuf + batchItem.offsetInBuffer, 1,
                              item->size, m_poFpWrite) != item->size) {
                    CPLErrorIO("reading temp feature");
                    return false;
                }
            }

            // Write target features
            if( offsetInBuffer > 0 &&
                VSIFWriteL(m_featureBuf, 1, offsetInBuffer, m_poFp) !=
                                                            offsetInBuffer ) {
                CPLErrorIO("writing feature");
                return false;
            }

            batch.clear();
            offsetInBuffer = 0;
            return true;
        };

        for (size_t i = 0; i < m_featuresCount; i++)
        {
            const auto featureItem = std::static_pointer_cast<FeatureItem>(m_featureItems[i]);
            const auto featureSize = featureItem->size;

            if( offsetInBuffer + featureSize > m_featureBufSize )
            {
                if( !flushBatch() )
                {
                    return;
                }
            }

            BatchItem bachItem;
            bachItem.offsetInBuffer = offsetInBuffer;
            bachItem.featureIdx = i;
            batch.emplace_back(bachItem);
            offsetInBuffer += featureSize;
            c += featureSize;
        }

        if( !flushBatch() )
        {
            return;
        }
    }
    else
    {
        const auto err = ensureFeatureBuf(m_maxFeatureSize);
        if (err != OGRERR_NONE)
            return;

        for (const std::shared_ptr<FlatGeobuf::Item>& item: m_featureItems) {
            const auto featureItem = std::static_pointer_cast<FeatureItem>(item);
            const auto featureSize = featureItem->size;

            //CPLDebugOnly("FlatGeobuf", "featureItem->offset: %lu", static_cast<long unsigned int>(featureItem->offset));
            //CPLDebugOnly("FlatGeobuf", "featureSize: %d", featureSize);
            if (VSIFSeekL(m_poFpWrite, featureItem->offset, SEEK_SET) == -1) {
                CPLErrorIO("seeking to temp feature location");
                return;
            }
            if (VSIFReadL(m_featureBuf, 1, featureSize, m_poFpWrite) != featureSize) {
                CPLErrorIO("reading temp feature");
                return;
            }
            if( VSIFWriteL(m_featureBuf, 1, featureSize, m_poFp) != featureSize ) {
                CPLErrorIO("writing feature");
                return;
            }
            c += featureSize;
        }
    }

    CPLDebugOnly("FlatGeobuf", "Wrote feature buffers (%lu bytes)", static_cast<long unsigned int>(c));
    m_writeOffset += c;

    CPLDebugOnly("FlatGeobuf", "Now at offset %lu", static_cast<long unsigned int>(m_writeOffset));
}

OGRFlatGeobufLayer::~OGRFlatGeobufLayer()
{
    if (m_create)
        Create();

    if (m_poFp)
        VSIFCloseL(m_poFp);

    if (m_poFpWrite)
        VSIFCloseL(m_poFpWrite);

    if (!m_osTempFile.empty())
        VSIUnlink(m_osTempFile.c_str());

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
    const auto treeSize = PackedRTree::size(m_featuresCount, m_indexNodeSize);
    const auto levelBounds = PackedRTree::generateLevelBounds(m_featuresCount, m_indexNodeSize);
    const auto bottomLevelOffset = m_offset - treeSize + (levelBounds.front().first * sizeof(NodeItem));
    const auto nodeItemOffset = bottomLevelOffset + (index * sizeof(NodeItem));
    const auto featureOffsetOffset = nodeItemOffset + (sizeof(double) * 4);
    if (VSIFSeekL(m_poFp, featureOffsetOffset, SEEK_SET) == -1)
        return CPLErrorIO("seeking feature offset");
    if (VSIFReadL(&featureOffset, sizeof(uint64_t), 1, m_poFp) != 1)
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
    if( m_sFilterEnvelope.IsInit() &&
        m_sExtent.IsInit() &&
        m_sFilterEnvelope.MinX <= m_sExtent.MinX &&
        m_sFilterEnvelope.MinY <= m_sExtent.MinY &&
        m_sFilterEnvelope.MaxX >= m_sExtent.MaxX &&
        m_sFilterEnvelope.MaxY >= m_sExtent.MaxY )
        return OGRERR_NONE;
    const auto indexNodeSize = m_poHeader->index_node_size();
    if (indexNodeSize == 0)
        return OGRERR_NONE;
    const auto featuresCount = m_poHeader->features_count();
    if (featuresCount == 0)
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
            CPLDebugOnly("FlatGeobuf", "Attempting spatial index query");
            OGREnvelope env;
            m_poFilterGeom->getEnvelope(&env);
            NodeItem n { env.MinX, env.MinY, env.MaxX, env.MaxY, 0 };
            CPLDebugOnly("FlatGeobuf", "Spatial index search on %f,%f,%f,%f", env.MinX, env.MinY, env.MaxX, env.MaxY);
            const auto treeOffset = sizeof(magicbytes) + sizeof(uoffset_t) + headerSize;
            const auto readNode = [this, treeOffset] (uint8_t *buf, size_t i, size_t s) {
                if (VSIFSeekL(m_poFp, treeOffset + i, SEEK_SET) == -1)
                    throw std::runtime_error("I/O seek failure");
                if (VSIFReadL(buf, 1, s, m_poFp) != s)
                    throw std::runtime_error("I/O read file");
            };
            m_foundItems = PackedRTree::streamSearch(featuresCount, indexNodeSize, n, readNode);
            m_featuresCount = m_foundItems.size();
            CPLDebugOnly("FlatGeobuf", "%lu features found in spatial index search", static_cast<long unsigned int>(m_featuresCount));

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
    if (m_poFilterGeom != nullptr || m_poAttrQuery != nullptr || m_featuresCount == 0)
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
            CPLDebugOnly("FlatGeobuf", "GetNextFeature: iteration end at %lu", static_cast<long unsigned int>(m_featuresPos));
            return nullptr;
        }

        if (readIndex() != OGRERR_NONE) {
            return nullptr;
        }

        if (m_queriedSpatialIndex && m_featuresCount == 0) {
            CPLDebugOnly("FlatGeobuf", "GetNextFeature: no features found");
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
        const auto newBufSize = std::max(1024U * 32U, featureSize);
        CPLDebugOnly("FlatGeobuf", "ensureFeatureBuf: newBufSize: %d", newBufSize);
        m_featureBuf = static_cast<GByte *>(VSIMalloc(newBufSize));
        if (m_featureBuf == nullptr)
            return CPLErrorMemoryAllocation("initial feature buffer");
        m_featureBufSize = newBufSize;
    } else if (m_featureBufSize < featureSize) {
        // Do not increase this x2 factor without modifying feature_max_buffer_size
        const auto newBufSize = std::max(m_featureBufSize * 2, featureSize);
        CPLDebugOnly("FlatGeobuf", "ensureFeatureBuf: newBufSize: %d", newBufSize);
        const auto featureBuf = static_cast<GByte *>(VSIRealloc(m_featureBuf, newBufSize));
        if (featureBuf == nullptr)
            return CPLErrorMemoryAllocation("feature buffer resize");
        m_featureBuf = featureBuf;
        m_featureBufSize = newBufSize;
    }
    return OGRERR_NONE;
}

OGRErr OGRFlatGeobufLayer::parseFeature(OGRFeature *poFeature) {
    GIntBig fid;
    auto seek = false;
    if (m_queriedSpatialIndex && !m_ignoreSpatialFilter) {
        const auto item = m_foundItems[m_featuresPos];
        m_offset = m_offsetFeatures + item.offset;
        fid = item.index;
        seek = true;
    } else {
        fid = m_featuresPos;
    }
    poFeature->SetFID(fid);


    //CPLDebugOnly("FlatGeobuf", "m_featuresPos: %lu", static_cast<long unsigned int>(m_featuresPos));

    if (m_featuresPos == 0)
        seek = true;

    if (seek && VSIFSeekL(m_poFp, m_offset, SEEK_SET) == -1) {
        if (VSIFEofL(m_poFp))
            return OGRERR_NONE;
        return CPLErrorIO("seeking to feature location");
    }
    uint32_t featureSize;
    if (VSIFReadL(&featureSize, sizeof(featureSize), 1, m_poFp) != 1) {
        if (VSIFEofL(m_poFp))
            return OGRERR_NONE;
        return CPLErrorIO("reading feature size");
    }
    CPL_LSBPTR32(&featureSize);

    // Sanity check to avoid allocated huge amount of memory on corrupted
    // feature
    if (featureSize > 100 * 1024 * 1024 )
    {
        if (featureSize > feature_max_buffer_size)
            return CPLErrorInvalidSize("feature");

        if( m_nFileSize == 0 )
        {
            VSIStatBufL sStatBuf;
            if( VSIStatL(m_osFilename.c_str(), &sStatBuf) == 0 )
            {
                m_nFileSize = sStatBuf.st_size;
            }
        }
        if( m_offset + featureSize > m_nFileSize )
        {
            return CPLErrorIO("reading feature size");
        }
    }

    const auto err = ensureFeatureBuf(featureSize);
    if (err != OGRERR_NONE)
        return err;
    if (VSIFReadL(m_featureBuf, 1, featureSize, m_poFp) != featureSize)
        return CPLErrorIO("reading feature");
    m_offset += featureSize + sizeof(featureSize);

    if (m_bVerifyBuffers) {
        const auto vBuf = const_cast<const uint8_t *>(reinterpret_cast<uint8_t *>(m_featureBuf));
        Verifier v(vBuf, featureSize);
        const auto ok = VerifyFeatureBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer verification failed");
            CPLDebugOnly("FlatGeobuf", "m_offset: %lu", static_cast<long unsigned int>(m_offset));
            CPLDebugOnly("FlatGeobuf", "m_featuresPos: %lu", static_cast<long unsigned int>(m_featuresPos));
            CPLDebugOnly("FlatGeobuf", "featureSize: %d", featureSize);
            return OGRERR_CORRUPT_DATA;
        }
    }

    const auto feature = GetRoot<Feature>(m_featureBuf);
    const auto geometry = feature->geometry();
    if (!m_poFeatureDefn->IsGeometryIgnored() && geometry != nullptr) {
        auto geometryType = m_geometryType;
        if (geometryType == GeometryType::Unknown)
            geometryType = geometry->type();
        GeometryReader reader { geometry, geometryType, m_hasZ, m_hasM };
        OGRGeometry *poOGRGeometry = reader.read();
        if (poOGRGeometry == nullptr) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to read geometry");
            return OGRERR_CORRUPT_DATA;
        }
// #ifdef DEBUG
//             char *wkt;
//             poOGRGeometry->exportToWkt(&wkt);
//             CPLDebugOnly("FlatGeobuf", "readGeometry as wkt: %s", wkt);
// #endif
        if (m_poSRS != nullptr)
            poOGRGeometry->assignSpatialReference(m_poSRS);
        poFeature->SetGeometryDirectly(poOGRGeometry);
    }

    const auto properties = feature->properties();
    if (properties != nullptr) {
        const auto data = properties->data();
        const auto size = properties->size();

        //CPLDebugOnly("FlatGeobuf", "DEBUG parseFeature: size: %lu", static_cast<long unsigned int>(size));

        //CPLDebugOnly("FlatGeobuf", "properties->size: %d", size);
        uoffset_t offset = 0;
        // size must be at least large enough to contain
        // a single column index and smallest value type
        if (size > 0 && size < (sizeof(uint16_t) + sizeof(uint8_t)))
            return CPLErrorInvalidSize("property value");
        while (offset + 1 < size) {
            if (offset + sizeof(uint16_t) > size)
                return CPLErrorInvalidSize("property value");
            uint16_t i = *((uint16_t *)(data + offset));
            CPL_LSBPTR16(&i);
            //CPLDebugOnly("FlatGeobuf", "DEBUG parseFeature: i: %hu", i);
            offset += sizeof(uint16_t);
            //CPLDebugOnly("FlatGeobuf", "DEBUG parseFeature: offset: %du", offset);
            // TODO: use columns from feature if defined
            const auto columns = m_poHeader->columns();
            if (columns == nullptr) {
                CPLErrorInvalidPointer("columns");
                return OGRERR_CORRUPT_DATA;
            }
            if (i >= columns->size()) {
                CPLError(CE_Failure, CPLE_AppDefined, "Column index %hu out of range", i);
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
                case ColumnType::Bool:
                    if (offset + sizeof(unsigned char) > size)
                        return CPLErrorInvalidSize("bool value");
                    if (!isIgnored)
                    {
                        ogrField->Integer = *(data + offset);
                    }
                    offset += sizeof(unsigned char);
                    break;

                case ColumnType::Byte:
                    if (offset + sizeof(signed char) > size)
                        return CPLErrorInvalidSize("byte value");
                    if (!isIgnored)
                    {
                        ogrField->Integer =
                            *reinterpret_cast<const signed char*>(data + offset);
                    }
                    offset += sizeof(signed char);
                    break;

                case ColumnType::UByte:
                    if (offset + sizeof(unsigned char) > size)
                        return CPLErrorInvalidSize("ubyte value");
                    if (!isIgnored)
                    {
                        ogrField->Integer =
                            *reinterpret_cast<const unsigned char*>(data + offset);
                    }
                    offset += sizeof(unsigned char);
                    break;

                case ColumnType::Short:
                    if (offset + sizeof(int16_t) > size)
                        return CPLErrorInvalidSize("short value");
                    if (!isIgnored)
                    {
                        short s;
                        memcpy(&s, data + offset, sizeof(int16_t));
                        CPL_LSBPTR16(&s);
                        ogrField->Integer = s;
                    }
                    offset += sizeof(int16_t);
                    break;

                case ColumnType::UShort:
                    if (offset + sizeof(uint16_t) > size)
                        return CPLErrorInvalidSize("ushort value");
                    if (!isIgnored)
                    {
                        uint16_t s;
                        memcpy(&s, data + offset, sizeof(uint16_t));
                        CPL_LSBPTR16(&s);
                        ogrField->Integer = s;
                    }
                    offset += sizeof(uint16_t);
                    break;

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

                case ColumnType::UInt:
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize("uint value");
                    if (!isIgnored)
                    {
                        uint32_t v;
                        memcpy(&v, data + offset, sizeof(int32_t));
                        CPL_LSBPTR32(&v);
                        ogrField->Integer64 = v;
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

                case ColumnType::ULong:
                    if (offset + sizeof(uint64_t) > size)
                        return CPLErrorInvalidSize("uint64 value");
                    if (!isIgnored)
                    {
                        uint64_t v;
                        memcpy(&v, data + offset, sizeof(v));
                        CPL_LSBPTR64(&v);
                        ogrField->Real = static_cast<double>(v);
                    }
                    offset += sizeof(int64_t);
                    break;

                case ColumnType::Float:
                    if (offset + sizeof(float) > size)
                        return CPLErrorInvalidSize("float value");
                    if (!isIgnored)
                    {
                        float f;
                        memcpy(&f, data + offset, sizeof(float));
                        CPL_LSBPTR32(&f);
                        ogrField->Real = f;
                    }
                    offset += sizeof(float);
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

                case ColumnType::String:
                case ColumnType::Json:
                {
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

                case ColumnType::Binary: {
                    if (offset + sizeof(uint32_t) > size)
                        return CPLErrorInvalidSize("binary length");
                    uint32_t len;
                    memcpy(&len, data + offset, sizeof(int32_t));
                    CPL_LSBPTR32(&len);
                    offset += sizeof(uint32_t);
                    if (len > static_cast<uint32_t>(INT_MAX) || len > size - offset)
                        return CPLErrorInvalidSize("binary value");
                    if (!isIgnored )
                    {
                        GByte *binary = static_cast<GByte*>(VSI_MALLOC_VERBOSE(len ? len : 1));
                        if (binary == nullptr)
                            return CPLErrorMemoryAllocation("string value");
                        memcpy(binary, data + offset, len);
                        ogrField->Binary.nCount = static_cast<int>(len);
                        ogrField->Binary.paData = binary;
                    }
                    offset += len;
                    break;
                }
            }
        }
    }
    return OGRERR_NONE;
}


OGRErr OGRFlatGeobufLayer::CreateField(OGRFieldDefn *poField, int /* bApproxOK */)
{
    // CPLDebugOnly("FlatGeobuf", "CreateField %s %s", poField->GetNameRef(), poField->GetFieldTypeName(poField->GetType()));
    if(!TestCapability(OLCCreateField))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

    if (m_poFeatureDefn->GetFieldCount() > std::numeric_limits<uint16_t>::max()) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create features with more than 65536 columns");
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

    std::vector<uint8_t> properties;
    properties.reserve(1024 * 4);
    FlatBufferBuilder fbb;

    for (int i = 0; i < fieldCount; i++) {
        const auto fieldDef = m_poFeatureDefn->GetFieldDefn(i);
        if (!poNewFeature->IsFieldSetAndNotNull(i))
            continue;

        uint16_t column_index_le = static_cast<uint16_t>(i);
        CPL_LSBPTR16(&column_index_le);

        //CPLDebugOnly("FlatGeobuf", "DEBUG ICreateFeature: column_index_le: %hu", column_index_le);

        std::copy(reinterpret_cast<const uint8_t *>(&column_index_le), reinterpret_cast<const uint8_t *>(&column_index_le + 1), std::back_inserter(properties));

        const auto fieldType = fieldDef->GetType();
        const auto fieldSubType = fieldDef->GetSubType();
        const auto field = poNewFeature->GetRawFieldRef(i);
        switch (fieldType) {
            case OGRFieldType::OFTInteger: {
                int nVal = field->Integer;
                if( fieldSubType == OFSTBoolean )
                {
                    GByte byVal = static_cast<GByte>(nVal);
                    std::copy(reinterpret_cast<const uint8_t *>(&byVal), reinterpret_cast<const uint8_t *>(&byVal + 1), std::back_inserter(properties));
                }
                else if( fieldSubType == OFSTInt16 )
                {
                    short sVal = static_cast<short>(nVal);
                    CPL_LSBPTR16(&sVal);
                    std::copy(reinterpret_cast<const uint8_t *>(&sVal), reinterpret_cast<const uint8_t *>(&sVal + 1), std::back_inserter(properties));
                }
                else
                {
                    CPL_LSBPTR32(&nVal);
                    std::copy(reinterpret_cast<const uint8_t *>(&nVal), reinterpret_cast<const uint8_t *>(&nVal + 1), std::back_inserter(properties));
                }
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
                if( fieldSubType == OFSTFloat32 )
                {
                    float fVal = static_cast<float>(dfVal);
                    CPL_LSBPTR32(&fVal);
                    std::copy(reinterpret_cast<const uint8_t *>(&fVal), reinterpret_cast<const uint8_t *>(&fVal + 1), std::back_inserter(properties));
                }
                else
                {
                    CPL_LSBPTR64(&dfVal);
                    std::copy(reinterpret_cast<const uint8_t *>(&dfVal), reinterpret_cast<const uint8_t *>(&dfVal + 1), std::back_inserter(properties));
                }
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

            case OGRFieldType::OFTBinary: {
                size_t len = field->Binary.nCount;
                if (len >= feature_max_buffer_size) {
                    CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Binary too long");
                    return OGRERR_FAILURE;
                }
                uint32_t l_le = static_cast<uint32_t>(len);
                CPL_LSBPTR32(&l_le);
                std::copy(reinterpret_cast<const uint8_t *>(&l_le), reinterpret_cast<const uint8_t *>(&l_le + 1), std::back_inserter(properties));
                std::copy(field->Binary.paData, field->Binary.paData + len, std::back_inserter(properties));
                break;
            }

            default:
                CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Missing implementation for OGRFieldType %d", fieldType);
                return OGRERR_FAILURE;
        }
    }

    //CPLDebugOnly("FlatGeobuf", "DEBUG ICreateFeature: properties.size(): %lu", static_cast<long unsigned int>(properties.size()));

    const auto ogrGeometry = poNewFeature->GetGeometryRef();
#ifdef DEBUG
    //char *wkt;
    //ogrGeometry->exportToWkt(&wkt);
    //CPLDebugOnly("FlatGeobuf", "poNewFeature as wkt: %s", wkt);
#endif
    if (ogrGeometry == nullptr || ogrGeometry->IsEmpty())
    {
        CPLDebugOnly("FlatGeobuf", "Skip writing feature without geometry");
        return OGRERR_NONE;
    }
    if (m_geometryType != GeometryType::Unknown && ogrGeometry->getGeometryType() != m_eGType) {
        CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Mismatched geometry type");
        return OGRERR_FAILURE;
    }

    GeometryWriter writer { fbb, ogrGeometry, m_geometryType, m_hasZ, m_hasM };
    const auto geometryOffset = writer.write(0);
    const auto pProperties = properties.empty() ? nullptr : &properties;
    // TODO: write columns if mixed schema in collection
    const auto feature = CreateFeatureDirect(fbb, geometryOffset, pProperties);
    fbb.FinishSizePrefixed(feature);

    OGREnvelope psEnvelope;
    ogrGeometry->getEnvelope(&psEnvelope);

    if (m_sExtent.IsInit())
        m_sExtent.Merge(psEnvelope);
    else
        m_sExtent = psEnvelope;

    if (m_featuresCount == 0) {
        if (m_poFpWrite == nullptr) {
            CPLErrorInvalidPointer("output file handler");
            return OGRERR_FAILURE;
        }
        writeHeader(m_poFpWrite, 0, nullptr);
        CPLDebugOnly("FlatGeobuf", "Writing first feature at offset: %lu", static_cast<long unsigned int>(m_writeOffset));
    }

    m_maxFeatureSize = std::max(m_featureBufSize, static_cast<uint32_t>(fbb.GetSize()));
    size_t c = VSIFWriteL(fbb.GetBufferPointer(), 1, fbb.GetSize(), m_poFpWrite);
    if (c == 0)
        return CPLErrorIO("writing feature");
    if (m_bCreateSpatialIndexAtClose) {
        const auto item = std::make_shared<FeatureItem>();
#if defined(__MINGW32__) && __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        item->size = static_cast<uint32_t>(fbb.GetSize());
        item->offset = m_writeOffset;
        item->nodeItem = {
            psEnvelope.MinX,
            psEnvelope.MinY,
            psEnvelope.MaxX,
            psEnvelope.MaxY,
            0
        };
#if defined(__MINGW32__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
        m_featureItems.push_back(item);
    }
    m_writeOffset += c;

    m_featuresCount++;

    return OGRERR_NONE;
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
    if (EQUAL(pszCap, OLCCreateField))
        return m_create || m_update;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return m_create || m_update;
    else if (EQUAL(pszCap, OLCRandomRead))
        return m_poHeader != nullptr && m_poHeader->index_node_size() > 0;
    else if (EQUAL(pszCap, OLCIgnoreFields))
        return true;
    else if (EQUAL(pszCap, OLCMeasuredGeometries))
        return true;
    else if (EQUAL(pszCap, OLCCurveGeometries))
        return true;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr && m_featuresCount > 0;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return m_sExtent.IsInit();
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return m_poHeader != nullptr && m_poHeader->index_node_size() > 0;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;
    else
        return false;
}

void OGRFlatGeobufLayer::ResetReading()
{
    CPLDebugOnly("FlatGeobuf", "ResetReading");
    m_offset = m_offsetFeatures;
    m_featuresPos = 0;
    m_foundItems.clear();
    m_featuresCount = m_poHeader ? m_poHeader->features_count() : 0;
    m_queriedSpatialIndex = false;
    m_ignoreSpatialFilter = false;
    m_ignoreAttributeFilter = false;
    return;
}

std::string OGRFlatGeobufLayer::GetTempFilePath(const CPLString &fileName, CSLConstList papszOptions) {
    const CPLString osDirname(CPLGetPath(fileName.c_str()));
    const CPLString osBasename(CPLGetBasename(fileName.c_str()));
    const char* pszTempDir = CSLFetchNameValue(papszOptions, "TEMPORARY_DIR");
    std::string osTempFile = pszTempDir ?
        CPLFormFilename(pszTempDir, osBasename, nullptr) :
        (STARTS_WITH(fileName, "/vsi") &&
        !STARTS_WITH(fileName, "/vsimem/")) ?
        CPLGenerateTempFilename(osBasename) :
        CPLFormFilename(osDirname, osBasename, nullptr);
    osTempFile += "_temp.fgb";
    return osTempFile;
}

VSILFILE *OGRFlatGeobufLayer::CreateOutputFile(const CPLString &pszFilename, CSLConstList papszOptions, bool isTemp) {
    std::string osTempFile;
    VSILFILE *poFpWrite;
    int savedErrno;
    if (isTemp) {
        CPLDebug("FlatGeobuf", "Spatial index requested will write to temp file and do second pass on close");
        osTempFile = GetTempFilePath(pszFilename, papszOptions);
        poFpWrite = VSIFOpenL(osTempFile.c_str(), "w+b");
        savedErrno = errno;
        // Unlink it now to avoid stale temporary file if killing the process
        // (only works on Unix)
        VSIUnlink(osTempFile.c_str());
    } else {
        CPLDebug("FlatGeobuf", "No spatial index will write directly to output");
        poFpWrite = VSIFOpenL(pszFilename, "wb");
        savedErrno = errno;
    }
    if (poFpWrite == nullptr) {
        CPLError(CE_Failure, CPLE_OpenFailed,
                    "Failed to create %s:\n%s",
                    pszFilename.c_str(), VSIStrerror(savedErrno));
        return nullptr;
    }
    return poFpWrite;
}

OGRFlatGeobufLayer *OGRFlatGeobufLayer::Create(
    const char *pszLayerName,
    const char *pszFilename,
    OGRSpatialReference *poSpatialRef,
    OGRwkbGeometryType eGType,
    bool bCreateSpatialIndexAtClose,
    char **papszOptions)
{
    std::string osTempFile = GetTempFilePath(pszFilename, papszOptions);
    VSILFILE *poFpWrite = CreateOutputFile(pszFilename, papszOptions, bCreateSpatialIndexAtClose);
    OGRFlatGeobufLayer *layer = new OGRFlatGeobufLayer(pszLayerName, pszFilename, poSpatialRef, eGType, bCreateSpatialIndexAtClose, poFpWrite, osTempFile);
    return layer;
}

OGRFlatGeobufLayer *OGRFlatGeobufLayer::Open(
    const Header *poHeader,
    GByte *headerBuf,
    const char *pszFilename,
    VSILFILE *poFp,
    uint64_t offset,
    bool update)
{
    OGRFlatGeobufLayer *layer = new OGRFlatGeobufLayer(poHeader, headerBuf, pszFilename, poFp, offset, update);
    return layer;
}

OGRFlatGeobufLayer *OGRFlatGeobufLayer::Open(const char* pszFilename, VSILFILE* fp, bool bVerifyBuffers, bool update)
{
    uint64_t offset = sizeof(magicbytes);
    CPLDebugOnly("FlatGeobuf", "Start at offset: %lu", static_cast<long unsigned int>(offset));
    if (VSIFSeekL(fp, offset, SEEK_SET) == -1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to get seek in file");
        return nullptr;
    }
    uint32_t headerSize;
    if (VSIFReadL(&headerSize, 4, 1, fp) != 1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header size");
        return nullptr;
    }
    CPL_LSBPTR32(&headerSize);
    CPLDebugOnly("FlatGeobuf", "headerSize: %d", headerSize);
    if (headerSize > header_max_buffer_size) {
        CPLError(CE_Failure, CPLE_AppDefined, "Header size too large (> 10 MB)");
        return nullptr;
    }
    std::unique_ptr<GByte, CPLFreeReleaser> buf(static_cast<GByte*>(VSIMalloc(headerSize)));
    if (buf == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to allocate memory for header");
        return nullptr;
    }
    if (VSIFReadL(buf.get(), 1, headerSize, fp) != headerSize) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header");
        return nullptr;
    }
    if (bVerifyBuffers) {
        Verifier v(buf.get(), headerSize);
        const auto ok = VerifyHeaderBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Header failed consistency verification");
            return nullptr;
        }
    }
    const auto header = GetHeader(buf.get());
    offset += 4 + headerSize;
    CPLDebugOnly("FlatGeobuf", "Add header size + length prefix to offset (%d)", 4 + headerSize);

    const auto featuresCount = header->features_count();

    if (featuresCount > std::min(
            static_cast<uint64_t>(std::numeric_limits<size_t>::max() / 8),
            static_cast<uint64_t>(100) * 1000 * 1000 * 1000)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features");
        return nullptr;
    }

    const auto index_node_size = header->index_node_size();
    if (index_node_size > 0) {
        try {
            const auto treeSize = PackedRTree::size(featuresCount);
            CPLDebugOnly("FlatGeobuf", "Tree start at offset (%lu)", static_cast<long unsigned int>(offset));
            offset += treeSize;
            CPLDebugOnly("FlatGeobuf", "Add tree size to offset (%lu)", static_cast<long unsigned int>(treeSize));
        } catch (const std::exception& e) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to calculate tree size: %s", e.what());
            return nullptr;
        }
    }

    CPLDebugOnly("FlatGeobuf", "Features start at offset (%lu)", static_cast<long unsigned int>(offset));

    CPLDebugOnly("FlatGeobuf", "Opening OGRFlatGeobufLayer");
    auto poLayer = OGRFlatGeobufLayer::Open(header, buf.release(), pszFilename, fp, offset, update);
    poLayer->VerifyBuffers(bVerifyBuffers);

    return poLayer;
}

OGRFlatGeobufBaseLayerInterface::~OGRFlatGeobufBaseLayerInterface() = default;
