/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements management of FileGDB field write support
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"

#include "filegdbtable.h"
#include "filegdbtable_priv.h"

#include <algorithm>
#include <limits>

#include "cpl_string.h"
#include "cpl_time.h"

#include "ogr_core.h"
#include "ogr_api.h"

namespace OpenFileGDB
{

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

bool FileGDBTable::CreateField(std::unique_ptr<FileGDBField>&& psField)
{
    if( !m_bUpdate )
        return false;

    // Encoded on a uint16_t
    if( m_apoFields.size() == 65535 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too many fields");
        return false;
    }

    if( psField->GetType() == FGFT_RASTER )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unhandled field type");
        return false;
    }

    if( GetFieldIdx(psField->GetName()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Field %s already exists",
                 psField->GetName().c_str());
        return false;
    }

    if( psField->GetType() == FGFT_GEOMETRY )
    {
        if( m_iGeomField >= 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only one geometry field supported");
            return false;
        }
        m_iGeomField = static_cast<int>(m_apoFields.size());
        m_adfSpatialIndexGridResolution = cpl::down_cast<
            const FileGDBGeomField*>(psField.get())->GetSpatialIndexGridResolution();
    }

    if( psField->GetType() == FGFT_OBJECTID )
    {
        if( m_iObjectIdField >= 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only one ObjectId field supported");
            return false;
        }
        m_iObjectIdField = static_cast<int>(m_apoFields.size());
    }

    bool bRewriteTable = false;
    if( m_nTotalRecordCount != 0 )
    {
        const bool bHasDefault = !OGR_RawField_IsNull(psField->GetDefault()) &&
                                 !OGR_RawField_IsUnset(psField->GetDefault());
        if( psField->GetType() == FGFT_GEOMETRY )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot add a geometry field to a non-empty table");
            return false;
        }
        else if( psField->GetType() == FGFT_OBJECTID )
        {
            // nothing to do but rewrite the feature definition
        }
        else if( (m_nCountNullableFields % 8) != 0 && psField->IsNullable() )
        {
            // Adding a nullable field to a feature definition that has already
            // nullable fields, with the last bitmap byte not completely filled.
            // We just need to rewrite the feature definition, not the features.
        }
        else if( !psField->IsNullable() && !bHasDefault )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot add non-nullable field without default value to "
                     "a non-empty table");
            return false;
        }
        else
        {
            bRewriteTable = true;
        }
    }

    m_nCurRow = -1;
    m_bDirtyFieldDescriptors = true;
    const bool bIsNullable = psField->IsNullable();
    if( bIsNullable )
    {
        m_nCountNullableFields ++;
        m_nNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(m_nCountNullableFields);
    }
    psField->SetParent(this);
    m_apoFields.emplace_back(std::move(psField));

    if( bRewriteTable && !RewriteTableToAddLastAddedField() )
    {
        if( bIsNullable )
        {
            m_nCountNullableFields --;
            m_nNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(m_nCountNullableFields);
        }
        m_apoFields.resize(m_apoFields.size() - 1);
        m_bDirtyFieldDescriptors = true;
        return false;
    }

    return true;
}

/************************************************************************/
/*                  RewriteTableToAddLastAddedField()                   */
/************************************************************************/

bool FileGDBTable::RewriteTableToAddLastAddedField()
{
    int nOldCountNullableFields = m_nCountNullableFields;
    if( m_apoFields.back()->IsNullable() )
    {
        nOldCountNullableFields --;
    }
    const int nOldNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(nOldCountNullableFields);
    int nExtraBytes = 0;
    if( nOldNullableFieldsSizeInBytes != m_nNullableFieldsSizeInBytes )
        nExtraBytes ++;
    std::vector<GByte> abyDefaultVal;

    const auto& psLastField = m_apoFields.back();
    if( !psLastField->IsNullable() )
    {
        const bool bHasDefault = !OGR_RawField_IsNull(psLastField->GetDefault()) &&
                                 !OGR_RawField_IsUnset(psLastField->GetDefault());
        CPL_IGNORE_RET_VAL(bHasDefault);
        CPLAssert( bHasDefault );
        if( psLastField->GetType() == FGFT_STRING )
        {
            const std::string osDefaultVal(psLastField->GetDefault()->String);
            WriteVarUInt(abyDefaultVal, osDefaultVal.size());
            abyDefaultVal.insert(abyDefaultVal.end(),
                                 reinterpret_cast<const GByte*>(osDefaultVal.c_str()),
                                 reinterpret_cast<const GByte*>(osDefaultVal.c_str()) + osDefaultVal.size());
        }
        else if( psLastField->GetType() == FGFT_INT16 )
        {
            WriteInt16(abyDefaultVal, static_cast<int16_t>(psLastField->GetDefault()->Integer));
        }
        else if( psLastField->GetType() == FGFT_INT32 )
        {
            WriteInt32(abyDefaultVal, psLastField->GetDefault()->Integer);
        }
        else if( psLastField->GetType() == FGFT_FLOAT32 )
        {
            WriteFloat32(abyDefaultVal, static_cast<float>(psLastField->GetDefault()->Real));
        }
        else if( psLastField->GetType() == FGFT_FLOAT64 )
        {
            WriteFloat64(abyDefaultVal, psLastField->GetDefault()->Real);
        }
        else if( psLastField->GetType() == FGFT_DATETIME )
        {
            WriteFloat64(abyDefaultVal, FileGDBOGRDateToDoubleDate(psLastField->GetDefault()));
        }
        nExtraBytes += static_cast<int>(abyDefaultVal.size());
    }
    CPLAssert( nExtraBytes != 0 );

    std::vector<GByte> abyBufferOffsets;
    abyBufferOffsets.resize(1024 * m_nTablxOffsetSize);

    WholeFileRewriter oWholeFileRewriter(*this);
    if( !oWholeFileRewriter.Begin() )
        return false;

    if( CPLTestBool(CPLGetConfigOption(
            "OPENFILEGDB_SIMUL_ERROR_IN_RewriteTableToAddLastAddedField", "FALSE")) )
    {
        return false;
    }

    uint32_t nRowBufferMaxSize = 0;
    m_nCurRow = -1;

    // Rewrite all features
    for( uint32_t iPage = 0; iPage < m_n1024BlocksPresent; ++iPage )
    {
        const vsi_l_offset nOffsetInTableX = 16 + m_nTablxOffsetSize * static_cast<vsi_l_offset>(iPage) * 1024;
        VSIFSeekL(oWholeFileRewriter.m_fpOldGdbtablx, nOffsetInTableX, SEEK_SET);
        if( VSIFReadL(abyBufferOffsets.data(), m_nTablxOffsetSize * 1024, 1,
                      oWholeFileRewriter.m_fpOldGdbtablx) != 1 )
            return false;

        GByte* pabyBufferOffsets = abyBufferOffsets.data();
        for( int i = 0; i < 1024; i++, pabyBufferOffsets += m_nTablxOffsetSize )
        {
            const uint64_t nOffset = ReadFeatureOffset(pabyBufferOffsets);
            if( nOffset != 0 )
            {
                // Read feature size
                VSIFSeekL(oWholeFileRewriter.m_fpOldGdbtable, nOffset, SEEK_SET);
                uint32_t nFeatureSize = 0;
                if( !ReadUInt32(oWholeFileRewriter.m_fpOldGdbtable, nFeatureSize) )
                    return false;

                // Read feature data
                if( nFeatureSize > m_abyBuffer.size() )
                {
                    try
                    {
                        m_abyBuffer.resize(nFeatureSize);
                    }
                    catch( const std::exception& e )
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
                        return false;
                    }
                }
                if (VSIFReadL(m_abyBuffer.data(), nFeatureSize, 1, oWholeFileRewriter.m_fpOldGdbtable) != 1 )
                    return false;

                // Update offset of updated feature
                WriteFeatureOffset(m_nFileSize, pabyBufferOffsets);

                // Write updated feature size
                const uint32_t nNewFeatureSize = nFeatureSize + nExtraBytes;
                if( !WriteUInt32(oWholeFileRewriter.m_fpTable, nNewFeatureSize) )
                    return false;

                // Write updated feature data
                if( nOldNullableFieldsSizeInBytes > 0 )
                {
                    if( VSIFWriteL(m_abyBuffer.data(), nOldNullableFieldsSizeInBytes,
                                   1, oWholeFileRewriter.m_fpTable) != 1 )
                        return false;
                }
                if( nOldNullableFieldsSizeInBytes != m_nNullableFieldsSizeInBytes )
                {
                    CPLAssert(psLastField->IsNullable());
                    const GByte byNewNullableFieldByte = 0xFF;
                    if( VSIFWriteL(&byNewNullableFieldByte, 1, 1, oWholeFileRewriter.m_fpTable) != 1 )
                        return false;
                }
                if( nFeatureSize - nOldNullableFieldsSizeInBytes > 0 )
                {
                    if( VSIFWriteL(m_abyBuffer.data() + nOldNullableFieldsSizeInBytes,
                                   nFeatureSize - nOldNullableFieldsSizeInBytes, 1,
                                   oWholeFileRewriter.m_fpTable) != 1 )
                        return false;
                }
                if( !abyDefaultVal.empty() )
                {
                    if( VSIFWriteL(abyDefaultVal.data(), abyDefaultVal.size(), 1,
                        oWholeFileRewriter.m_fpTable) != 1 )
                        return false;
                }

                if( nNewFeatureSize > nRowBufferMaxSize )
                    nRowBufferMaxSize = nNewFeatureSize;
                m_nFileSize += sizeof(uint32_t) + nNewFeatureSize;
            }
        }
        VSIFSeekL(oWholeFileRewriter.m_fpTableX, nOffsetInTableX, SEEK_SET);
        if( VSIFWriteL(abyBufferOffsets.data(), m_nTablxOffsetSize * 1024, 1,
                       oWholeFileRewriter.m_fpTableX) != 1 )
            return false;
    }

    m_nRowBufferMaxSize = nRowBufferMaxSize;
    m_nHeaderBufferMaxSize = std::max(m_nFieldDescLength, m_nRowBufferMaxSize);

    return oWholeFileRewriter.Commit();
}

/************************************************************************/
/*                       WriteFieldDescriptor()                         */
/************************************************************************/

static void WriteFieldDescriptor(std::vector<GByte>& abyBuffer,
                                 const FileGDBField* psField,
                                 bool bGeomTypeHasZ,
                                 bool bGeomTypeHasM,
                                 bool bStringsAreUTF8,
                                 uint32_t& nGeomFieldBBoxOffsetOut,
                                 uint32_t& nGeomFieldSpatialIndexGridResOffsetOut)
{
    WriteUTF16String(abyBuffer, psField->GetName().c_str(), NUMBER_OF_CHARS_ON_UINT8);
    WriteUTF16String(abyBuffer, psField->GetAlias().c_str(), NUMBER_OF_CHARS_ON_UINT8);
    WriteUInt8(abyBuffer, static_cast<uint8_t>(psField->GetType()));
    constexpr int UNKNOWN_FIELD_FLAG = 4;
    const auto& sDefault = *(psField->GetDefault());
    switch( psField->GetType() )
    {
        case FGFT_UNDEFINED:
        {
            CPLAssert(false);
            break;
        }

        case FGFT_INT16:
        {
            WriteUInt8(abyBuffer, 2); // sizeof(int16)
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                WriteUInt8(abyBuffer, 2); // sizeof(int16)
                WriteInt16(abyBuffer, static_cast<int16_t>(sDefault.Integer));
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_INT32:
        {
            WriteUInt8(abyBuffer, 4); // sizeof(int32)
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                WriteUInt8(abyBuffer, 4); // sizeof(int32)
                WriteInt32(abyBuffer, sDefault.Integer);
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_FLOAT32:
        {
            WriteUInt8(abyBuffer, 4); // sizeof(float32)
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                WriteUInt8(abyBuffer, 4); // sizeof(float32)
                WriteFloat32(abyBuffer, static_cast<float>(sDefault.Real));
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_FLOAT64:
        {
            WriteUInt8(abyBuffer, 8); // sizeof(float64)
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                WriteUInt8(abyBuffer, 8); // sizeof(float64)
                WriteFloat64(abyBuffer, sDefault.Real);
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_STRING:
        {
            WriteUInt32(abyBuffer, psField->GetMaxWidth());
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                if( bStringsAreUTF8 )
                {
                    const auto nLen = strlen(sDefault.String);
                    WriteVarUInt(abyBuffer, nLen);
                    if( nLen > 0 )
                    {
                        abyBuffer.insert(abyBuffer.end(),
                                         sDefault.String,
                                         sDefault.String + nLen);
                    }
                }
                else
                {
                    WriteUTF16String(abyBuffer,
                                     sDefault.String,
                                     NUMBER_OF_BYTES_ON_VARUINT);
                }
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_DATETIME:
        {
            WriteUInt8(abyBuffer, 8); // sizeof(float64)
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            if( !OGR_RawField_IsNull(&sDefault) &&
                !OGR_RawField_IsUnset(&sDefault) )
            {
                WriteUInt8(abyBuffer, 8); // sizeof(float64)
                WriteFloat64(abyBuffer, FileGDBOGRDateToDoubleDate(&sDefault));
            }
            else
            {
                WriteUInt8(abyBuffer, 0); // size of default value
            }
            break;
        }

        case FGFT_OBJECTID:
        {
            WriteUInt8(abyBuffer, 4); // sizeof(uint32) ?
            WriteUInt8(abyBuffer, 2); // magic value
            break;
        }

        case FGFT_GEOMETRY:
        {
            const auto* geomField = cpl::down_cast<const FileGDBGeomField*>(psField);
            WriteUInt8(abyBuffer, 0); // unknown role
            WriteUInt8(abyBuffer, static_cast<uint8_t>(
                2 | UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            WriteUTF16String(abyBuffer, geomField->GetWKT().c_str(),
                             NUMBER_OF_BYTES_ON_UINT16);
            WriteUInt8(abyBuffer, static_cast<uint8_t>(
                1 |
                (((geomField->HasMOriginScaleTolerance() ? 1 : 0)) << 1) |
                (((geomField->HasZOriginScaleTolerance() ? 1 : 0)) << 2)));
            WriteFloat64(abyBuffer, geomField->GetXOrigin());
            WriteFloat64(abyBuffer, geomField->GetYOrigin());
            WriteFloat64(abyBuffer, geomField->GetXYScale());
            if( geomField->HasMOriginScaleTolerance() )
            {
                WriteFloat64(abyBuffer, geomField->GetMOrigin());
                WriteFloat64(abyBuffer, geomField->GetMScale());
            }
            if( geomField->HasZOriginScaleTolerance() )
            {
                WriteFloat64(abyBuffer, geomField->GetZOrigin());
                WriteFloat64(abyBuffer, geomField->GetZScale());
            }
            WriteFloat64(abyBuffer, geomField->GetXYTolerance());
            if( geomField->HasMOriginScaleTolerance() )
            {
                WriteFloat64(abyBuffer, geomField->GetMTolerance());
            }
            if( geomField->HasZOriginScaleTolerance() )
            {
                WriteFloat64(abyBuffer, geomField->GetZTolerance());
            }
            nGeomFieldBBoxOffsetOut = static_cast<uint32_t>(abyBuffer.size());
            WriteFloat64(abyBuffer, geomField->GetXMin());
            WriteFloat64(abyBuffer, geomField->GetYMin());
            WriteFloat64(abyBuffer, geomField->GetXMax());
            WriteFloat64(abyBuffer, geomField->GetYMax());
            if( bGeomTypeHasZ )
            {
                WriteFloat64(abyBuffer, geomField->GetZMin());
                WriteFloat64(abyBuffer, geomField->GetZMax());
            }
            if( bGeomTypeHasM )
            {
                WriteFloat64(abyBuffer, geomField->GetMMin());
                WriteFloat64(abyBuffer, geomField->GetMMax());
            }
            WriteUInt8(abyBuffer, 0); // possibly an indicator of existence of spatial index or its type?
            const auto &adfSpatialIndexGridResolution = geomField->GetSpatialIndexGridResolution();
            WriteUInt32(abyBuffer, static_cast<uint32_t>(adfSpatialIndexGridResolution.size()));
            nGeomFieldSpatialIndexGridResOffsetOut = static_cast<uint32_t>(abyBuffer.size());
            for( double dfSize: adfSpatialIndexGridResolution )
                WriteFloat64(abyBuffer, dfSize);
            break;
        }

        case FGFT_BINARY:
        {
            WriteUInt8(abyBuffer, 0); // unknown role
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            break;
        }

        case FGFT_RASTER:
        {
            // Not handled for now
            CPLAssert(false);
            break;
        }

        case FGFT_GUID:
        case FGFT_GLOBALID:
        {
            WriteUInt8(abyBuffer, 38); // size
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            break;
        }

        case FGFT_XML:
        {
            WriteUInt8(abyBuffer, 0); // unknown role
            WriteUInt8(abyBuffer, static_cast<uint8_t>(UNKNOWN_FIELD_FLAG | static_cast<int>(psField->IsNullable())));
            break;
        }
    }
}

/************************************************************************/
/*                       WriteFieldDescriptors()                        */
/************************************************************************/

bool FileGDBTable::WriteFieldDescriptors(VSILFILE* fpTable)
{
    m_bDirtyFieldDescriptors = false;

    // In-memory field descriptors
    std::vector<GByte> abyBuffer;

    WriteUInt32(abyBuffer, 0); // size of field section, excluding this field. Will be patched later
    WriteUInt32(abyBuffer, 4); // version of the file

    const uint32_t nLayerFlags = static_cast<uint32_t>(m_eTableGeomType) |
                           ((m_bStringsAreUTF8 ? 1 : 0) << 8) | // string encoding
                           (((m_eTableGeomType != FGTGT_NONE) ? 1 : 0) << 9) | // "high precision storage"
                           (static_cast<uint32_t>(m_bGeomTypeHasM) << 30U) |
                           (static_cast<uint32_t>(m_bGeomTypeHasZ) << 31U);
    WriteUInt32(abyBuffer, nLayerFlags);

    WriteUInt16(abyBuffer, static_cast<uint16_t>(m_apoFields.size()));

    m_nGeomFieldBBoxSubOffset = 0;
    for( const auto& poField: m_apoFields )
    {
        WriteFieldDescriptor(abyBuffer, poField.get(),
                             m_bGeomTypeHasZ, m_bGeomTypeHasM,
                             m_bStringsAreUTF8,
                             m_nGeomFieldBBoxSubOffset,
                             m_nGeomFieldSpatialIndexGridResSubOffset);
    }

    // Just to immitate the behavior of the FileGDB SDK !
    abyBuffer.push_back(0xDE);
    abyBuffer.push_back(0xAD);
    abyBuffer.push_back(0xBE);
    abyBuffer.push_back(0xEF);

    // Patch size of field section at beginning of buffer
    const auto nFieldSectionSize = static_cast<uint32_t>(abyBuffer.size() - sizeof(uint32_t));
    WriteUInt32(abyBuffer, nFieldSectionSize, 0);

    bool bUpdateFileSize = false;
    const auto nOldFieldDescLength = m_nFieldDescLength;
    if( m_nOffsetFieldDesc + m_nFieldDescLength == m_nFileSize )
    {
        // Optimization: if the field descriptor section is already at end of
        // file, we can rewrite-in-place whatever its new size
        VSIFSeekL(fpTable, m_nOffsetFieldDesc, SEEK_SET);
        bUpdateFileSize = true;
    }
    else if( abyBuffer.size() > m_nFieldDescLength )
    {
        if( m_nOffsetFieldDesc != 0 )
        {
            VSIFSeekL(fpTable, m_nOffsetFieldDesc, SEEK_SET);
            // Cancel unused bytes with NUL characters
            std::vector<GByte> abyNul(m_nFieldDescLength + sizeof(uint32_t));
            CPL_IGNORE_RET_VAL(VSIFWriteL(abyNul.data(), 1, abyNul.size(), fpTable));
        }
        VSIFSeekL(fpTable, m_nFileSize, SEEK_SET);
        m_bDirtyHeader = true;
        m_nOffsetFieldDesc = m_nFileSize;
        m_nFileSize += abyBuffer.size();
    }
    else
    {
        VSIFSeekL(fpTable, m_nOffsetFieldDesc, SEEK_SET);
    }

    // Write new field descriptor
    m_nFieldDescLength = nFieldSectionSize;
    if( VSIFWriteL(abyBuffer.data(), 1, abyBuffer.size(), fpTable) != abyBuffer.size() )
        return false;

    if( bUpdateFileSize )
    {
        m_nFileSize = VSIFTellL(fpTable);
        VSIFTruncateL(fpTable, m_nFileSize);
        m_bDirtyHeader = true;
    }
    else if( nOldFieldDescLength != 0 && m_nFieldDescLength < nOldFieldDescLength )
    {
        // Cancel unused bytes with NUL characters
        std::vector<GByte> abyNul(nOldFieldDescLength - m_nFieldDescLength);
        CPL_IGNORE_RET_VAL(VSIFWriteL(abyNul.data(), 1, abyNul.size(), fpTable));
    }

    return true;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

bool FileGDBTable::DeleteField(int iField)
{
    if( !m_bUpdate )
        return false;

    if( iField < 0 || iField >= static_cast<int>(m_apoFields.size()) )
    {
        return false;
    }

    if( m_iGeomField == iField )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geometry field deletion not supported");
        return false;
    }

    bool bRet = true;
    if( iField != m_iObjectIdField )
    {
        std::vector<GByte> abyBlank;

        // Little hack: we present the geometry field as a binary one
        // to avoid any conversion
        const int iGeomFieldBackup = m_iGeomField;
        if( m_iGeomField >= 0 )
            m_apoFields[m_iGeomField]->m_eType = FGFT_BINARY;
        m_iGeomField = -1;

        for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
        {
            iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            auto asValues = GetAllFieldValues();

            if( m_nRowBlobLength > 0 )
            {
                if( EncodeFeature(asValues, nullptr, iField) )
                {
                    VSIFSeekL( m_fpTable, VSIFTellL(m_fpTable) - sizeof(uint32_t) - m_nRowBlobLength, SEEK_SET );

                    abyBlank.resize(m_nRowBlobLength - m_abyBuffer.size());

                    if( !WriteUInt32(m_fpTable, static_cast<uint32_t>(m_abyBuffer.size())) ||
                        VSIFWriteL(m_abyBuffer.data(), m_abyBuffer.size(), 1, m_fpTable) != 1 ||
                        (!abyBlank.empty() && VSIFWriteL(abyBlank.data(), abyBlank.size(), 1, m_fpTable) != 1) )
                    {
                        bRet = false;
                    }
                }
                else
                {
                    bRet = false;
                }
            }

            FreeAllFieldValues(asValues);
        }

        if( iGeomFieldBackup >= 0 )
            m_apoFields[iGeomFieldBackup]->m_eType = FGFT_GEOMETRY;
        m_iGeomField = iGeomFieldBackup;
    }

    // Delete linked index if existing
    GetIndexCount();
    if( m_apoFields[iField]->m_poIndex )
    {
        for( size_t i = 0; i < m_apoIndexes.size(); ++i )
        {
            if( m_apoIndexes[i].get() == m_apoFields[iField]->m_poIndex )
            {
                m_bDirtyGdbIndexesFile = true;

                if( iField != m_iObjectIdField )
                {
                    VSIUnlink( CPLResetExtension( m_osFilename.c_str(),
                          (m_apoIndexes[i]->GetIndexName() + ".atx").c_str()) );
                }

                m_apoIndexes.erase(m_apoIndexes.begin() + i);
                break;
            }
        }
    }

    // Renumber objectId and geomField indices
    if( m_iObjectIdField == iField )
        m_iObjectIdField = -1;
    else if( iField < m_iObjectIdField )
        m_iObjectIdField --;

    if( iField < m_iGeomField )
        m_iGeomField --;

    if( m_apoFields[iField]->IsNullable() )
    {
        m_nCountNullableFields --;
        m_nNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(m_nCountNullableFields);
    }

    m_apoFields.erase(m_apoFields.begin() + iField);

    m_bDirtyFieldDescriptors = true;

    return bRet;
}

/************************************************************************/
/*                            AlterField()                              */
/************************************************************************/

bool FileGDBTable::AlterField(int iField,
                              const std::string& osName,
                              const std::string& osAlias,
                              FileGDBFieldType eType,
                              bool bNullable,
                              int nMaxWidth,
                              const OGRField& sDefault)
{
    if( !m_bUpdate )
        return false;

    if( iField < 0 || iField >= static_cast<int>(m_apoFields.size()) )
    {
        return false;
    }

    if( m_iGeomField == iField )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AlterField() not supported on geometry field");
        return false;
    }

    if( m_apoFields[iField]->GetType() != eType )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AlterField() does not support modifying the field type");
        return false;
    }

    if( m_apoFields[iField]->IsNullable() != bNullable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AlterField() does not support modifying the nullable state");
        return false;
    }

    const bool bRenameField = m_apoFields[iField]->GetName() != osName;
    if( bRenameField && GetFieldIdx(osName) >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AlterField() cannot rename a field to an existing field name");
        return false;
    }

    // Update linked index if existing
    GetIndexCount();
    auto poIndex = m_apoFields[iField]->m_poIndex;

    m_apoFields[iField] = cpl::make_unique<FileGDBField>(
            osName, osAlias,
            eType,
            bNullable, nMaxWidth, sDefault);
    m_apoFields[iField]->SetParent(this);
    m_apoFields[iField]->m_poIndex = poIndex;
    if( poIndex && bRenameField )
    {
        m_bDirtyGdbIndexesFile = true;
        if( STARTS_WITH_CI(poIndex->GetExpression().c_str(), "LOWER(") )
            poIndex->m_osExpression = "LOWER(" + osName + ")";
        else
            poIndex->m_osExpression = osName;
    }
    m_bDirtyFieldDescriptors = true;

    return true;
}

/************************************************************************/
/*                          AlterGeomField()                            */
/************************************************************************/

bool FileGDBTable::AlterGeomField( const std::string& osName,
                                   const std::string& osAlias,
                                   bool bNullable,
                                   const std::string& osWKT)
{
    if( !m_bUpdate )
        return false;
    if( m_iGeomField < 0 )
        return false;

    auto poGeomField = cpl::down_cast<FileGDBGeomField*>(
        m_apoFields[m_iGeomField].get());
    if( poGeomField->IsNullable() != bNullable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AlterGeomField() does not support modifying the nullable state");
        return false;
    }

    const bool bRenameField = poGeomField->GetName() != osName;

    poGeomField->m_osName = osName;
    poGeomField->m_osAlias = osAlias;
    poGeomField->m_bNullable = bNullable;
    poGeomField->m_osWKT = osWKT;
    auto poIndex = poGeomField->m_poIndex;
    if( poIndex && bRenameField )
    {
        poIndex->m_osExpression = osName;
        m_bDirtyGdbIndexesFile = true;
    }
    m_bDirtyFieldDescriptors = true;

    return true;
}


} /* namespace OpenFileGDB */
