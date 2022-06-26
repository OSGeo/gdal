/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements writing of FileGDB tables
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

#ifdef GDAL_CMAKE_BUILD
#include "gdal_version_full/gdal_version.h"
#else
#include "gdal_version.h"
#endif

#include "filegdbtable.h"

#include <algorithm>
#include <cwchar>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "filegdbtable_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogrpgeogeometry.h"

namespace OpenFileGDB
{

constexpr uint8_t EXT_SHAPE_SEGMENT_ARC = 1;
constexpr int TABLX_HEADER_SIZE = 16;
constexpr int TABLX_FEATURES_PER_PAGE = 1024;

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

bool FileGDBTable::Create(const char* pszFilename,
                          int nTablxOffsetSize,
                          FileGDBTableGeometryType eTableGeomType,
                          bool bGeomTypeHasZ,
                          bool bGeomTypeHasM)
{
    CPLAssert(m_fpTable == nullptr);

    m_bUpdate = true;
    m_eTableGeomType = eTableGeomType;
    m_nTablxOffsetSize = nTablxOffsetSize;
    m_bGeomTypeHasZ = bGeomTypeHasZ;
    m_bGeomTypeHasM = bGeomTypeHasM;
    m_bHasReadGDBIndexes = TRUE;

    if( !EQUAL(CPLGetExtension(pszFilename), "gdbtable") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FileGDB table extension must be gdbtable");
        return false;
    }

    m_osFilename = pszFilename;
    m_fpTable = VSIFOpenL( pszFilename, "wb+" );
    if( m_fpTable == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Cannot create %s: %s", m_osFilename.c_str(),
                 VSIStrerror(errno));
        return false;
    }

    const std::string osTableXName = CPLFormFilename(CPLGetPath(pszFilename),
                                        CPLGetBasename(pszFilename), "gdbtablx");
    m_fpTableX = VSIFOpenL( osTableXName.c_str(), "wb+" );
    if( m_fpTable == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Cannot create %s: %s", osTableXName.c_str(),
                 VSIStrerror(errno));
        return false;
    }

    if( !WriteHeader(m_fpTable) )
        return false;

    if( !WriteHeaderX(m_fpTableX) )
        return false;

    m_bDirtyTableXTrailer = true;

    return true;
}

/************************************************************************/
/*                          SetTextUTF16()                              */
/************************************************************************/

bool FileGDBTable::SetTextUTF16()
{
    if( m_nOffsetFieldDesc != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetTextUTF16() should be called immediately after Create()");
        return false;
    }

    m_bStringsAreUTF8 = false;
    return true;
}

/************************************************************************/
/*                           WriteHeader()                              */
/************************************************************************/

bool FileGDBTable::WriteHeader(VSILFILE* fpTable)
{
    // Could be useful in case we get something wrong...
    const char* pszCreator = CPLGetConfigOption("OPENFILEGDB_CREATOR",
                                                "GDAL " GDAL_RELEASE_NAME);

    m_nFileSize = 0;
    m_bDirtyHeader = true;
    m_bDirtyFieldDescriptors = true;
    m_nOffsetFieldDesc = 0;
    m_nFieldDescLength = 0;

    VSIFSeekL(fpTable, 0, SEEK_SET);

    bool bRet = WriteUInt32(fpTable, 3) && // version number
                WriteUInt32(fpTable, m_nValidRecordCount) &&// number of valid rows
                WriteUInt32(fpTable, m_nHeaderBufferMaxSize) && // largest size of a feature record / field description
                WriteUInt32(fpTable, 5) && // magic value
                WriteUInt32(fpTable, 0) && // magic value
                WriteUInt32(fpTable, 0) && // magic value
                WriteUInt64(fpTable, m_nFileSize) &&
                WriteUInt64(fpTable, m_nOffsetFieldDesc);

    if( bRet && pszCreator[0] != '\0' )
    {
        // Writing the creator is not part of the "spec", but we just use
        // the fact that there might be ghost areas in the file
        bRet = WriteUInt32(fpTable, static_cast<uint32_t>(strlen(pszCreator))) &&
               VSIFWriteL(pszCreator, strlen(pszCreator), 1, fpTable) == 1;
    }

    if( !bRet )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot write .gdbtable header");
        return false;
    }

    m_nFileSize = VSIFTellL(fpTable);
    return true;
}

/************************************************************************/
/*                           WriteHeaderX()                             */
/************************************************************************/

bool FileGDBTable::WriteHeaderX(VSILFILE* fpTableX)
{
    VSIFSeekL(fpTableX, 0, SEEK_SET);
    if( !WriteUInt32(fpTableX, 3) || // version number
        !WriteUInt32(fpTableX, m_n1024BlocksPresent) ||
        !WriteUInt32(fpTableX, m_nTotalRecordCount) ||
        !WriteUInt32(fpTableX, m_nTablxOffsetSize) )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot write .gdbtablx header");
        return false;
    }
    return true;
}

/************************************************************************/
/*                                Sync()                                */
/************************************************************************/

bool FileGDBTable::Sync(VSILFILE* fpTable, VSILFILE* fpTableX)
{
    if( !m_bUpdate )
        return true;

    if( fpTable == nullptr )
        fpTable = m_fpTable;

    if( fpTableX == nullptr )
        fpTableX = m_fpTableX;

    bool bRet = true;

    if( m_bDirtyGdbIndexesFile )
    {
        m_bDirtyGdbIndexesFile = false;
        CreateGdbIndexesFile();
    }

    if( m_bDirtyIndices )
    {
        m_bDirtyIndices = false;
        RefreshIndices();
    }

    if( m_bDirtyFieldDescriptors && fpTable )
        bRet = WriteFieldDescriptors(fpTable);

    if( m_bDirtyGeomFieldBBox && fpTable )
    {
        VSIFSeekL(fpTable, m_nOffsetFieldDesc + m_nGeomFieldBBoxSubOffset, SEEK_SET);
        const auto poGeomField = cpl::down_cast<const FileGDBGeomField*>(m_apoFields[m_iGeomField].get());
        bRet &= WriteFloat64(fpTable, poGeomField->GetXMin());
        bRet &= WriteFloat64(fpTable, poGeomField->GetYMin());
        bRet &= WriteFloat64(fpTable, poGeomField->GetXMax());
        bRet &= WriteFloat64(fpTable, poGeomField->GetYMax());
        if( m_bGeomTypeHasZ )
        {
            bRet &= WriteFloat64(fpTable, poGeomField->GetZMin());
            bRet &= WriteFloat64(fpTable, poGeomField->GetZMax());
        }
        m_bDirtyGeomFieldBBox = false;
    }

    if( m_bDirtyGeomFieldSpatialIndexGridRes && fpTable )
    {
        VSIFSeekL(fpTable, m_nOffsetFieldDesc + m_nGeomFieldSpatialIndexGridResSubOffset, SEEK_SET);
        const auto poGeomField = cpl::down_cast<const FileGDBGeomField*>(m_apoFields[m_iGeomField].get());
        const auto& adfSpatialIndexGridResolution = poGeomField->GetSpatialIndexGridResolution();
        for( double dfSize: adfSpatialIndexGridResolution )
            bRet &= WriteFloat64(fpTable, dfSize);
        m_bDirtyGeomFieldSpatialIndexGridRes = false;
    }

    if( m_bDirtyHeader && fpTable)
    {
        VSIFSeekL(fpTable, 4, SEEK_SET);
        bRet &= WriteUInt32(fpTable, m_nValidRecordCount);
        m_nHeaderBufferMaxSize = std::max(m_nHeaderBufferMaxSize,
                          std::max(m_nRowBufferMaxSize, m_nFieldDescLength));
        bRet &= WriteUInt32(fpTable, m_nHeaderBufferMaxSize);

        VSIFSeekL(fpTable, 24, SEEK_SET);
        bRet &= WriteUInt64(fpTable, m_nFileSize);
        bRet &= WriteUInt64(fpTable, m_nOffsetFieldDesc);

        VSIFSeekL(fpTable, 0, SEEK_END);
        CPLAssert( VSIFTellL(fpTable) == m_nFileSize );
        m_bDirtyHeader = false;
    }

    if( m_bDirtyTableXHeader && fpTableX )
    {
        VSIFSeekL(fpTableX, 4, SEEK_SET);
        bRet &= WriteUInt32(fpTableX, m_n1024BlocksPresent);
        bRet &= WriteUInt32(fpTableX, m_nTotalRecordCount);
        m_bDirtyTableXHeader = false;
    }

    if( m_bDirtyTableXTrailer && fpTableX )
    {
        m_nOffsetTableXTrailer = TABLX_HEADER_SIZE +
            m_nTablxOffsetSize * TABLX_FEATURES_PER_PAGE * (vsi_l_offset)m_n1024BlocksPresent;
        VSIFSeekL( fpTableX, m_nOffsetTableXTrailer, SEEK_SET );
        const uint32_t n1024BlocksTotal = DIV_ROUND_UP(m_nTotalRecordCount, TABLX_FEATURES_PER_PAGE);
        if( !m_abyTablXBlockMap.empty() )
        {
            CPLAssert( m_abyTablXBlockMap.size() >= (n1024BlocksTotal + 7) / 8 );
        }
        // Size of the bitmap in terms of 32-bit words, rounded to a multiple
        // of 32.
        const uint32_t nBitmapInt32Words = DIV_ROUND_UP(DIV_ROUND_UP(
            static_cast<uint32_t>(m_abyTablXBlockMap.size()), 4), 32) * 32;
        m_abyTablXBlockMap.resize(nBitmapInt32Words * 4);
        bRet &= WriteUInt32(fpTableX, nBitmapInt32Words);
        bRet &= WriteUInt32(fpTableX, n1024BlocksTotal);
        bRet &= WriteUInt32(fpTableX, m_n1024BlocksPresent);
        uint32_t nTrailingZero32BitWords = 0;
        for( int i = static_cast<int>(m_abyTablXBlockMap.size() / 4) - 1; i >= 0; -- i)
        {
            if( m_abyTablXBlockMap[4 * i] != 0 ||
                m_abyTablXBlockMap[4 * i + 1] != 0 ||
                m_abyTablXBlockMap[4 * i + 2] != 0 ||
                m_abyTablXBlockMap[4 * i + 3] != 0 )
            {
                break;
            }
            nTrailingZero32BitWords ++;
        }
        const uint32_t nLeadingNonZero32BitWords = nBitmapInt32Words - nTrailingZero32BitWords;
        bRet &= WriteUInt32(fpTableX, nLeadingNonZero32BitWords);
        if( !m_abyTablXBlockMap.empty() )
        {
#ifdef DEBUG
            uint32_t nCountBlocks = 0;
            for(uint32_t i=0;i<n1024BlocksTotal;i++)
                nCountBlocks += TEST_BIT(m_abyTablXBlockMap.data(), i) != 0;
            if( nCountBlocks != m_n1024BlocksPresent )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Sync(): nCountBlocks(=%u) != m_n1024BlocksPresent(=%u)",
                         nCountBlocks, m_n1024BlocksPresent);
            }
#endif
            bRet &= VSIFWriteL(m_abyTablXBlockMap.data(), 1,
                               m_abyTablXBlockMap.size(), fpTableX) == m_abyTablXBlockMap.size();
        }
        m_bDirtyTableXTrailer = false;
    }

    if( m_bFreelistCanBeDeleted )
    {
        DeleteFreeList();
    }

    if( fpTable )
        VSIFFlushL( fpTable );

    if( fpTableX )
        VSIFFlushL( fpTableX );

    return bRet;
}

/************************************************************************/
/*                          EncodeEnvelope()                            */
/************************************************************************/

#define CHECK_CAN_BE_ENCODED_ON_VARUINT(v, msg) \
    if( !((v) >= 0 && (v) <= static_cast<double>(std::numeric_limits<uint64_t>::max()))) \
    { \
        CPLError(CE_Failure, CPLE_AppDefined, msg); \
        return false; \
    }

#define CHECK_CAN_BE_ENCODED_ON_VARINT(v, oldV, msg) \
    if( !((v) >= static_cast<double>(std::numeric_limits<int64_t>::min()) && \
          (v) <= static_cast<double>(std::numeric_limits<int64_t>::max())) ) \
    { \
        CPLError(CE_Failure, CPLE_AppDefined, msg); \
        return false; \
    } \
    if( !(((v) - (oldV)) >= static_cast<double>(std::numeric_limits<int64_t>::min()) && \
          ((v) - (oldV)) <= static_cast<double>(std::numeric_limits<int64_t>::max())) ) \
    { \
        CPLError(CE_Failure, CPLE_AppDefined, msg); \
        return false; \
    }

static bool EncodeEnvelope(std::vector<GByte>& abyBuffer,
                           const FileGDBGeomField* poGeomField,
                           const OGRGeometry* poGeom)
{
    OGREnvelope oEnvelope;
    poGeom->getEnvelope(&oEnvelope);

    double dfVal;

    dfVal = (oEnvelope.MinX - poGeomField->GetXOrigin()) * poGeomField->GetXYScale();
    CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode X value");
    WriteVarUInt(abyBuffer, static_cast<uint64_t>(dfVal + 0.5));

    dfVal = (oEnvelope.MinY - poGeomField->GetYOrigin()) * poGeomField->GetXYScale();
    CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode Y value");
    WriteVarUInt(abyBuffer, static_cast<uint64_t>(dfVal + 0.5));

    dfVal = (oEnvelope.MaxX - oEnvelope.MinX) * poGeomField->GetXYScale();
    CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode X value");
    WriteVarUInt(abyBuffer, static_cast<uint64_t>(dfVal + 0.5));

    dfVal = (oEnvelope.MaxY - oEnvelope.MinY) * poGeomField->GetXYScale();
    CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode Y value");
    WriteVarUInt(abyBuffer, static_cast<uint64_t>(dfVal + 0.5));

    return true;
}

/************************************************************************/
/*                          EncodeGeometry()                            */
/************************************************************************/

bool FileGDBTable::EncodeGeometry(const FileGDBGeomField* poGeomField,
                                  const OGRGeometry* poGeom)
{
    m_abyGeomBuffer.clear();

    const auto bIs3D = poGeom->Is3D();
    const auto bIsMeasured = poGeom->IsMeasured();

    const auto WriteEndOfCurveOrSurface = [this, bIs3D, bIsMeasured, poGeomField, poGeom](int nCurveDescrCount)
    {
        WriteVarUInt(m_abyGeomBuffer, static_cast<uint32_t>(m_adfX.size()));
        if( m_adfX.empty() )
            return true;
        WriteVarUInt(m_abyGeomBuffer, static_cast<uint32_t>(m_anNumberPointsPerPart.size()));
        if( nCurveDescrCount > 0 )
            WriteVarUInt(m_abyGeomBuffer, nCurveDescrCount);

        if( !EncodeEnvelope(m_abyGeomBuffer, poGeomField, poGeom) )
            return false;

        for( int iPart = 0; iPart < static_cast<int>(m_anNumberPointsPerPart.size()) - 1; ++iPart )
        {
            WriteVarUInt(m_abyGeomBuffer, m_anNumberPointsPerPart[iPart]);
        }

        {
            int64_t nLastX = 0;
            int64_t nLastY = 0;
            for( size_t i = 0; i < m_adfX.size(); ++i )
            {
                double dfVal = std::round((m_adfX[i] - poGeomField->GetXOrigin()) * poGeomField->GetXYScale());
                CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastX, "Cannot encode X value");
                const int64_t nX = static_cast<int64_t>(dfVal);
                WriteVarInt(m_abyGeomBuffer, nX - nLastX);

                dfVal = std::round((m_adfY[i] - poGeomField->GetYOrigin()) * poGeomField->GetXYScale());
                CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastY, "Cannot encode Y value");
                const int64_t nY = static_cast<int64_t>(dfVal);
                WriteVarInt(m_abyGeomBuffer, nY - nLastY);

                nLastX = nX;
                nLastY = nY;
            }
        }

        if( bIs3D )
        {
            int64_t nLastZ = 0;
            for( size_t i = 0; i < m_adfZ.size(); ++i )
            {
                double dfVal = std::round((m_adfZ[i] - poGeomField->GetZOrigin()) * poGeomField->GetZScale());
                CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastZ, "Cannot encode Z value");
                const int64_t nZ = static_cast<int64_t>(dfVal);
                WriteVarInt(m_abyGeomBuffer, nZ - nLastZ);

                nLastZ = nZ;
            }
        }

        if( bIsMeasured )
        {
            int64_t nLastM = 0;
            for( size_t i = 0; i < m_adfM.size(); ++i )
            {
                double dfVal = std::round((m_adfM[i] - poGeomField->GetMOrigin()) * poGeomField->GetMScale());
                CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastM, "Cannot encode M value");
                const int64_t nM = static_cast<int64_t>(dfVal);
                WriteVarInt(m_abyGeomBuffer, nM - nLastM);

                nLastM = nM;
            }
        }

        if( !m_abyCurvePart.empty() )
        {
            m_abyGeomBuffer.insert(m_abyGeomBuffer.end(),
                                   m_abyCurvePart.begin(),
                                   m_abyCurvePart.end());
        }

        return true;
    };

    const auto eFlatType = wkbFlatten(poGeom->getGeometryType());
    switch( eFlatType )
    {
        case wkbPoint:
        {
            if( bIs3D )
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POINTZM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POINTZ));
                }
            }
            else
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POINTM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POINT));
                }
            }
            const auto poPoint = poGeom->toPoint();
            double dfVal;

            dfVal = (poPoint->getX() - poGeomField->GetXOrigin()) * poGeomField->GetXYScale() + 1;
            CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode value");
            WriteVarUInt(m_abyGeomBuffer, static_cast<uint64_t>(dfVal + 0.5));

            dfVal = (poPoint->getY() - poGeomField->GetYOrigin()) * poGeomField->GetXYScale() + 1;
            CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode Y value");
            WriteVarUInt(m_abyGeomBuffer, static_cast<uint64_t>(dfVal + 0.5));

            if( bIs3D )
            {
                dfVal = (poPoint->getZ() - poGeomField->GetZOrigin()) * poGeomField->GetZScale() + 1;
                CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode Z value");
                WriteVarUInt(m_abyGeomBuffer, static_cast<uint64_t>(dfVal + 0.5));
            }

            if( bIsMeasured )
            {
                dfVal = (poPoint->getM() - poGeomField->GetMOrigin()) * poGeomField->GetMScale() + 1;
                CHECK_CAN_BE_ENCODED_ON_VARUINT(dfVal, "Cannot encode M value");
                WriteVarUInt(m_abyGeomBuffer, static_cast<uint64_t>(dfVal + 0.5));
            }

            return true;
        }

        case wkbMultiPoint:
        {
            if( bIs3D )
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_MULTIPOINTZM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_MULTIPOINTZ));
                }
            }
            else
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_MULTIPOINTM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_MULTIPOINT));
                }
            }

            const auto poMultiPoint = poGeom->toMultiPoint();
            const auto nNumGeoms = poMultiPoint->getNumGeometries();
            WriteVarUInt(m_abyGeomBuffer, nNumGeoms);
            if( nNumGeoms == 0 )
                return true;

            if( !EncodeEnvelope(m_abyGeomBuffer, poGeomField, poGeom) )
                return false;

            {
                int64_t nLastX = 0;
                int64_t nLastY = 0;
                for( const auto* poPoint: *poMultiPoint )
                {
                    const double dfX = poPoint->getX();
                    const double dfY = poPoint->getY();

                    double dfVal = std::round((dfX - poGeomField->GetXOrigin()) * poGeomField->GetXYScale());
                    CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastX, "Cannot encode value");
                    const int64_t nX = static_cast<int64_t>(dfVal);
                    WriteVarInt(m_abyGeomBuffer, nX - nLastX);

                    dfVal = std::round((dfY - poGeomField->GetYOrigin()) * poGeomField->GetXYScale());
                    CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastY, "Cannot encode Y value");
                    const int64_t nY = static_cast<int64_t>(dfVal);
                    WriteVarInt(m_abyGeomBuffer, nY - nLastY);

                    nLastX = nX;
                    nLastY = nY;
                }
            }

            if( bIs3D )
            {
                int64_t nLastZ = 0;
                for( const auto* poPoint: *poMultiPoint )
                {
                    const double dfZ = poPoint->getZ();

                    double dfVal = std::round((dfZ - poGeomField->GetZOrigin()) * poGeomField->GetZScale());
                    CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastZ, "Bad Z value");
                    const int64_t nZ = static_cast<int64_t>(dfVal);
                    WriteVarInt(m_abyGeomBuffer, nZ - nLastZ);

                    nLastZ = nZ;
                }
            }

            if( bIsMeasured )
            {
                int64_t nLastM = 0;
                for( const auto* poPoint: *poMultiPoint )
                {
                    const double dfM = poPoint->getM();

                    double dfVal = std::round((dfM - poGeomField->GetMOrigin()) * poGeomField->GetMScale());
                    CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastM, "Bad M value");
                    const int64_t nM = static_cast<int64_t>(dfVal);
                    WriteVarInt(m_abyGeomBuffer, nM - nLastM);

                    nLastM = nM;
                }
            }

            return true;
        }

        case wkbLineString:
        case wkbCircularString:
        case wkbCompoundCurve:
        case wkbMultiLineString:
        case wkbMultiCurve:
        {
            m_abyCurvePart.clear();
            m_anNumberPointsPerPart.clear();
            m_adfX.clear();
            m_adfY.clear();
            m_adfZ.clear();
            m_adfM.clear();

            int nCurveDescrCount = 0;
            const auto ProcessCurve = [this, bIs3D, bIsMeasured, &nCurveDescrCount](const OGRCurve* poCurve)
            {
                if( auto poCC = dynamic_cast<const OGRCompoundCurve*>(poCurve) )
                {
                    const size_t nSizeBefore = m_adfX.size();
                    bool bFirstSubCurve = true;
                    for( const auto* poSubCurve: *poCC )
                    {
                        if( const auto poLS = dynamic_cast<const OGRLineString*>(poSubCurve) )
                        {
                            const int nNumPoints = poLS->getNumPoints();
                            for( int i = (bFirstSubCurve ? 0 : 1); i < nNumPoints; ++i)
                            {
                                m_adfX.push_back(poLS->getX(i));
                                m_adfY.push_back(poLS->getY(i));
                                if( bIs3D )
                                    m_adfZ.push_back(poLS->getZ(i));
                                if( bIsMeasured )
                                    m_adfM.push_back(poLS->getM(i));
                            }
                        }
                        else if( const auto poCS = dynamic_cast<const OGRCircularString*>(poSubCurve) )
                        {
                            const int nNumPoints = poCS->getNumPoints();
                            for( int i = 0; i < nNumPoints; i++)
                            {
                                if( i > 0 || bFirstSubCurve )
                                {
                                    m_adfX.push_back(poCS->getX(i));
                                    m_adfY.push_back(poCS->getY(i));
                                    if( bIs3D )
                                        m_adfZ.push_back(poCS->getZ(i));
                                    if( bIsMeasured )
                                        m_adfM.push_back(poCS->getM(i));
                                }
                                if( i + 1 < nNumPoints )
                                {
                                    ++nCurveDescrCount;
                                    ++i;
                                    WriteVarUInt(m_abyCurvePart, static_cast<uint32_t>(m_adfX.size() - 1));
                                    WriteUInt8(m_abyCurvePart, EXT_SHAPE_SEGMENT_ARC);
                                    WriteFloat64(m_abyCurvePart, poCS->getX(i));
                                    WriteFloat64(m_abyCurvePart, poCS->getY(i));
                                    WriteUInt32(m_abyCurvePart, (1 << 7)); // DefinedIP
                                }
                            }
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                        bFirstSubCurve = false;
                    }
                    m_anNumberPointsPerPart.push_back(static_cast<uint32_t>(m_adfX.size() - nSizeBefore));
                }
                else if( const auto poLS = dynamic_cast<const OGRLineString*>(poCurve) )
                {
                    const int nNumPoints = poLS->getNumPoints();
                    m_anNumberPointsPerPart.push_back(nNumPoints);
                    for( int i = 0; i < nNumPoints; ++i)
                    {
                        m_adfX.push_back(poLS->getX(i));
                        m_adfY.push_back(poLS->getY(i));
                        if( bIs3D )
                            m_adfZ.push_back(poLS->getZ(i));
                        if( bIsMeasured )
                            m_adfM.push_back(poLS->getM(i));
                    }
                }
                else if( const auto poCS = dynamic_cast<const OGRCircularString*>(poCurve) )
                {
                    const int nNumPoints = poCS->getNumPoints();
                    const size_t nSizeBefore = m_adfX.size();
                    for( int i = 0; i < nNumPoints; i++)
                    {
                        m_adfX.push_back(poCS->getX(i));
                        m_adfY.push_back(poCS->getY(i));
                        if( bIs3D )
                            m_adfZ.push_back(poCS->getZ(i));
                        if( bIsMeasured )
                            m_adfM.push_back(poCS->getM(i));
                        if( i + 1 < nNumPoints )
                        {
                            ++nCurveDescrCount;
                            ++i;
                            WriteVarUInt(m_abyCurvePart, static_cast<uint32_t>(m_adfX.size() - 1));
                            WriteUInt8(m_abyCurvePart, EXT_SHAPE_SEGMENT_ARC);
                            WriteFloat64(m_abyCurvePart, poCS->getX(i));
                            WriteFloat64(m_abyCurvePart, poCS->getY(i));
                            WriteUInt32(m_abyCurvePart, (1 << 7)); // DefinedIP
                        }
                    }
                    m_anNumberPointsPerPart.push_back(static_cast<uint32_t>(m_adfX.size() - nSizeBefore));
                }
                else
                {
                    CPLAssert(false);
                }

            };

            if( eFlatType == wkbMultiLineString || eFlatType == wkbMultiCurve )
            {
                const auto poMultiCurve = poGeom->toMultiCurve();
                for( const auto* poCurve: *poMultiCurve )
                {
                    ProcessCurve(poCurve);
                }
            }
            else
            {
                ProcessCurve(poGeom->toCurve());
            }

            if( nCurveDescrCount > 0 )
            {
                WriteVarUInt(m_abyGeomBuffer, SHPT_GENERALPOLYLINE |
                                       (1U << 29) | // has curves
                                       ((bIsMeasured ? 1U : 0U) << 30) |
                                       ((bIs3D ? 1U : 0U) << 31));
            }
            else if( bIs3D )
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_ARCZM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_ARCZ));
                }
            }
            else
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_ARCM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_ARC));
                }
            }

            return WriteEndOfCurveOrSurface(nCurveDescrCount);
        }

        case wkbPolygon:
        case wkbCurvePolygon:
        case wkbMultiPolygon:
        case wkbMultiSurface:
        {
            m_abyCurvePart.clear();
            m_anNumberPointsPerPart.clear();
            m_adfX.clear();
            m_adfY.clear();
            m_adfZ.clear();
            m_adfM.clear();

            int nCurveDescrCount = 0;
            const auto ProcessSurface = [this, bIs3D, bIsMeasured, &nCurveDescrCount](const OGRSurface* poSurface)
            {
                if( const auto poPolygon = dynamic_cast<const OGRPolygon*>(poSurface) )
                {
                    bool bFirstRing = true;
                    for( const auto* poLS: *poPolygon )
                    {
                        const int nNumPoints = poLS->getNumPoints();
                        m_anNumberPointsPerPart.push_back(nNumPoints);
                        const bool bIsClockwise = CPL_TO_BOOL(poLS->isClockwise());
                        const bool bReverseOrder = (bFirstRing && !bIsClockwise) || (!bFirstRing && bIsClockwise);
                        bFirstRing = false;
                        for( int i = 0; i < nNumPoints; ++i)
                        {
                            const int j = bReverseOrder ? nNumPoints - 1 - i : i;
                            m_adfX.push_back(poLS->getX(j));
                            m_adfY.push_back(poLS->getY(j));
                            if( bIs3D )
                                m_adfZ.push_back(poLS->getZ(j));
                            if( bIsMeasured )
                                m_adfM.push_back(poLS->getM(j));
                        }
                    }
                }
                else if( const auto poCurvePoly = dynamic_cast<const OGRCurvePolygon*>(poSurface) )
                {
                    bool bFirstRing = true;
                    for( const auto* poRing: *poCurvePoly )
                    {
                        const bool bIsClockwise = CPL_TO_BOOL(poRing->isClockwise());
                        const bool bReverseOrder = (bFirstRing && !bIsClockwise) || (!bFirstRing && bIsClockwise);
                        bFirstRing = false;
                        if( auto poCC = dynamic_cast<const OGRCompoundCurve*>(poRing) )
                        {
                            const size_t nSizeBefore = m_adfX.size();
                            bool bFirstSubCurve = true;
                            const int nNumCurves = poCC->getNumCurves();
                            for( int iSubCurve = 0; iSubCurve < nNumCurves; ++iSubCurve )
                            {
                                const OGRCurve* poSubCurve =
                                     poCC->getCurve(bReverseOrder ? nNumCurves - 1 - iSubCurve: iSubCurve);
                                if( auto poLS = dynamic_cast<const OGRLineString*>(poSubCurve) )
                                {
                                    const int nNumPoints = poLS->getNumPoints();
                                    for( int i = (bFirstSubCurve ? 0 : 1); i < nNumPoints; ++i)
                                    {
                                        const int j = bReverseOrder ? nNumPoints - 1 - i : i;
                                        m_adfX.push_back(poLS->getX(j));
                                        m_adfY.push_back(poLS->getY(j));
                                        if( bIs3D )
                                            m_adfZ.push_back(poLS->getZ(j));
                                        if( bIsMeasured )
                                            m_adfM.push_back(poLS->getM(j));
                                    }
                                }
                                else if( auto poCS = dynamic_cast<const OGRCircularString*>(poSubCurve) )
                                {
                                    const int nNumPoints = poCS->getNumPoints();
                                    for( int i = 0; i < nNumPoints; i++)
                                    {
                                        if( i > 0 || bFirstSubCurve )
                                        {
                                            const int j = bReverseOrder ? nNumPoints - 1 - i : i;
                                            m_adfX.push_back(poCS->getX(j));
                                            m_adfY.push_back(poCS->getY(j));
                                            if( bIs3D )
                                                m_adfZ.push_back(poCS->getZ(j));
                                            if( bIsMeasured )
                                                m_adfM.push_back(poCS->getM(j));
                                        }
                                        if( i + 1 < nNumPoints )
                                        {
                                            ++nCurveDescrCount;
                                            ++i;
                                            const int j = bReverseOrder ? nNumPoints - 1 - i : i;
                                            WriteVarUInt(m_abyCurvePart, static_cast<uint32_t>(m_adfX.size() - 1));
                                            WriteUInt8(m_abyCurvePart, EXT_SHAPE_SEGMENT_ARC);
                                            WriteFloat64(m_abyCurvePart, poCS->getX(j));
                                            WriteFloat64(m_abyCurvePart, poCS->getY(j));
                                            WriteUInt32(m_abyCurvePart, (1 << 7)); // DefinedIP
                                        }
                                    }
                                }
                                else
                                {
                                    CPLAssert(false);
                                }
                                bFirstSubCurve = false;
                            }
                            m_anNumberPointsPerPart.push_back(static_cast<uint32_t>(m_adfX.size() - nSizeBefore));
                        }
                        else if( const auto poLS = dynamic_cast<const OGRLineString*>(poRing) )
                        {
                            const int nNumPoints = poLS->getNumPoints();
                            m_anNumberPointsPerPart.push_back(nNumPoints);
                            for( int i = 0; i < nNumPoints; ++i)
                            {
                                const int j = bReverseOrder ? nNumPoints - 1 - i : i;
                                m_adfX.push_back(poLS->getX(j));
                                m_adfY.push_back(poLS->getY(j));
                                if( bIs3D )
                                    m_adfZ.push_back(poLS->getZ(j));
                                if( bIsMeasured )
                                    m_adfM.push_back(poLS->getM(j));
                            }
                        }
                        else if( const auto poCS = dynamic_cast<const OGRCircularString*>(poRing) )
                        {
                            const int nNumPoints = poCS->getNumPoints();
                            const size_t nSizeBefore = m_adfX.size();
                            for( int i = 0; i < nNumPoints; i++)
                            {
                                int j = bReverseOrder ? nNumPoints - 1 - i : i;
                                m_adfX.push_back(poCS->getX(j));
                                m_adfY.push_back(poCS->getY(j));
                                if( bIs3D )
                                    m_adfZ.push_back(poCS->getZ(j));
                                if( bIsMeasured )
                                    m_adfM.push_back(poCS->getM(j));
                                if( i + 1 < nNumPoints )
                                {
                                    ++nCurveDescrCount;
                                    ++i;
                                    j = bReverseOrder ? nNumPoints - 1 - i : i;
                                    WriteVarUInt(m_abyCurvePart, static_cast<uint32_t>(m_adfX.size() - 1));
                                    WriteUInt8(m_abyCurvePart, EXT_SHAPE_SEGMENT_ARC);
                                    WriteFloat64(m_abyCurvePart, poCS->getX(j));
                                    WriteFloat64(m_abyCurvePart, poCS->getY(j));
                                    WriteUInt32(m_abyCurvePart, (1 << 7)); // DefinedIP
                                }
                            }
                            m_anNumberPointsPerPart.push_back(static_cast<uint32_t>(m_adfX.size() - nSizeBefore));
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                    }
                }
                else
                {
                    CPLAssert(false);
                }
            };

            if( eFlatType == wkbMultiPolygon || eFlatType == wkbMultiSurface )
            {
                const auto poMultiSurface = poGeom->toMultiSurface();
                for( const auto* poSurface: *poMultiSurface )
                {
                    ProcessSurface(poSurface);
                }
            }
            else
            {
                ProcessSurface(poGeom->toSurface());
            }

            if( nCurveDescrCount > 0 )
            {
                WriteVarUInt(m_abyGeomBuffer, SHPT_GENERALPOLYGON |
                                       (1U << 29) | // has curves
                                       ((bIsMeasured ? 1U : 0U) << 30) |
                                       ((bIs3D ? 1U : 0U) << 31));
            }
            else if( bIs3D )
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POLYGONZM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POLYGONZ));
                }
            }
            else
            {
                if( bIsMeasured )
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POLYGONM));
                }
                else
                {
                    WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_POLYGON));
                }
            }

            return WriteEndOfCurveOrSurface(nCurveDescrCount);
        }

        case wkbTIN:
        case wkbPolyhedralSurface:
        case wkbGeometryCollection:
        {
            int nParts = 0;
            int* panPartStart = nullptr;
            int* panPartType = nullptr;
            int nPoints = 0;
            OGRRawPoint* poPoints = nullptr;
            double* padfZ = nullptr;
            OGRErr eErr = OGRCreateMultiPatch( poGeom,
                                               TRUE,
                                               nParts,
                                               panPartStart,
                                               panPartType,
                                               nPoints,
                                               poPoints,
                                               padfZ );
            if( eErr != OGRERR_NONE )
                return false;

            WriteUInt8(m_abyGeomBuffer, static_cast<uint8_t>(SHPT_MULTIPATCH));
            WriteVarUInt(m_abyGeomBuffer, nPoints);
            if( nPoints != 0 )
            {
                // Apparently we must write the size of the extended buffer shape
                // representation, even if we don't exactly follow this format
                // when writing to FileGDB files...
                int nShapeBufferSize = 4;  // All types start with integer type number.
                nShapeBufferSize += 16 * 2;  // xy bbox.
                nShapeBufferSize += 4;  // nparts.
                nShapeBufferSize += 4;  // npoints.
                nShapeBufferSize += 4 * nParts;  // panPartStart[nparts].
                nShapeBufferSize += 4 * nParts;  // panPartType[nparts].
                nShapeBufferSize += 8 * 2 * nPoints;  // xy points.
                nShapeBufferSize += 16;  // z bbox.
                nShapeBufferSize += 8 * nPoints;  // z points.
                WriteVarUInt(m_abyGeomBuffer, nShapeBufferSize);

                WriteVarUInt(m_abyGeomBuffer, nParts);

                if( !EncodeEnvelope(m_abyGeomBuffer, poGeomField, poGeom) )
                    return false;

                for( int i = 0; i < nParts - 1; i++ )
                {
                    WriteVarUInt(m_abyGeomBuffer, panPartStart[i+1] - panPartStart[i]);
                }

                for( int i = 0; i < nParts; i++ )
                {
                    WriteVarUInt(m_abyGeomBuffer, panPartType[i]);
                }

                {
                    int64_t nLastX = 0;
                    int64_t nLastY = 0;
                    for( int i = 0; i < nPoints; ++i )
                    {
                        double dfVal = std::round((poPoints[i].x - poGeomField->GetXOrigin()) * poGeomField->GetXYScale());
                        CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastX, "Cannot encode value");
                        const int64_t nX = static_cast<int64_t>(dfVal);
                        WriteVarInt(m_abyGeomBuffer, nX - nLastX);

                        dfVal = std::round((poPoints[i].y - poGeomField->GetYOrigin()) * poGeomField->GetXYScale());
                        CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastY, "Cannot encode Y value");
                        const int64_t nY = static_cast<int64_t>(dfVal);
                        WriteVarInt(m_abyGeomBuffer, nY - nLastY);

                        nLastX = nX;
                        nLastY = nY;
                    }
                }

                {
                    int64_t nLastZ = 0;
                    for( int i = 0; i < nPoints; ++i )
                    {
                        double dfVal = std::round((padfZ[i] - poGeomField->GetZOrigin()) * poGeomField->GetZScale());
                        CHECK_CAN_BE_ENCODED_ON_VARINT(dfVal, nLastZ, "Bad Z value");
                        const int64_t nZ = static_cast<int64_t>(dfVal);
                        WriteVarInt(m_abyGeomBuffer, nZ - nLastZ);

                        nLastZ = nZ;
                    }
                }
            }
            CPLFree(panPartStart);
            CPLFree(panPartType);
            CPLFree(poPoints);
            CPLFree(padfZ);
            return true;
        }

        default:
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported geometry type");
            return false;
        }
    }
}

/************************************************************************/
/*                          EncodeFeature()                             */
/************************************************************************/

bool FileGDBTable::EncodeFeature(const std::vector<OGRField>& asRawFields,
                                 const OGRGeometry* poGeom,
                                 int iSkipField)
{
    m_abyBuffer.clear();
    if( iSkipField >= 0 && m_apoFields[iSkipField]->IsNullable() )
        m_abyBuffer.resize(BIT_ARRAY_SIZE_IN_BYTES(m_nCountNullableFields-1), 0xFF);
    else
        m_abyBuffer.resize(m_nNullableFieldsSizeInBytes, 0xFF);

    if( asRawFields.size() != m_apoFields.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad size of asRawFields");
        return false;
    }
    int iNullableField = 0;
    for( int i = 0; i < static_cast<int>(m_apoFields.size()); ++i )
    {
        if( i == iSkipField )
            continue;
        auto& poField = m_apoFields[i];
        if( poField->GetType() == FGFT_OBJECTID )
        {
            // Implicit field
            continue;
        }
        if( i == m_iGeomField )
        {
            if( poGeom == nullptr )
            {
                if( !poField->IsNullable() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Attempting to write null geometry in non-nullable geometry field");
                    return false;
                }
                iNullableField ++;
                continue;
            }

            auto poGeomField = cpl::down_cast<FileGDBGeomField*>(poField.get());
            if( !EncodeGeometry( poGeomField, poGeom) )
                return false;
            if( !poGeom->IsEmpty() )
            {
                OGREnvelope3D oEnvelope;
                poGeom->getEnvelope(&oEnvelope);
                m_bDirtyGeomFieldBBox = true;
                if( std::isnan(poGeomField->GetXMin()) )
                {
                    poGeomField->SetXYMinMax(oEnvelope.MinX,
                                             oEnvelope.MinY,
                                             oEnvelope.MaxX,
                                             oEnvelope.MaxY);
                    poGeomField->SetZMinMax(oEnvelope.MinZ, oEnvelope.MaxZ);
                }
                else
                {
                    poGeomField->SetXYMinMax(std::min(poGeomField->GetXMin(), oEnvelope.MinX),
                                             std::min(poGeomField->GetYMin(), oEnvelope.MinY),
                                             std::max(poGeomField->GetXMax(), oEnvelope.MaxX),
                                             std::max(poGeomField->GetYMax(), oEnvelope.MaxY));
                    poGeomField->SetZMinMax(std::min(poGeomField->GetZMin(), oEnvelope.MinZ),
                                            std::max(poGeomField->GetZMax(), oEnvelope.MaxZ));
                }
            }

            if( m_abyGeomBuffer.size() + m_abyBuffer.size() > static_cast<size_t>(INT_MAX) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large feature");
                return false;
            }

            WriteVarUInt(m_abyBuffer, m_abyGeomBuffer.size());
            m_abyBuffer.insert(m_abyBuffer.end(),
                             m_abyGeomBuffer.begin(), m_abyGeomBuffer.end());

            if( poField->IsNullable() )
            {
                m_abyBuffer[iNullableField / 8] &= ~(1 << (iNullableField % 8));
                iNullableField ++;
            }
            continue;
        }

        if( OGR_RawField_IsNull(&asRawFields[i]) ||
            OGR_RawField_IsUnset(&asRawFields[i]) )
        {
            if( !poField->IsNullable() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Attempting to write null/empty field in non-nullable field");
                return false;
            }
            iNullableField ++;
            continue;
        }

        switch( poField->GetType() )
        {
            case FGFT_UNDEFINED:
            {
                CPLAssert(false);
                break;
            }

            case FGFT_INT16:
            {
                WriteInt16(m_abyBuffer, static_cast<int16_t>(asRawFields[i].Integer));
                break;
            }

            case FGFT_INT32:
            {
                WriteInt32(m_abyBuffer, asRawFields[i].Integer);
                break;
            }

            case FGFT_FLOAT32:
            {
                WriteFloat32(m_abyBuffer, static_cast<float>(asRawFields[i].Real));
                break;
            }

            case FGFT_FLOAT64:
            {
                WriteFloat64(m_abyBuffer, asRawFields[i].Real);
                break;
            }

            case FGFT_STRING:
            case FGFT_XML:
            {
                if( m_bStringsAreUTF8 || poField->GetType() == FGFT_XML )
                {
                    const auto nLen = strlen(asRawFields[i].String);
                    WriteVarUInt(m_abyBuffer, nLen);
                    if( nLen > 0 )
                    {
                        if( nLen + m_abyBuffer.size() > static_cast<size_t>(INT_MAX) )
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Too large feature");
                            return false;
                        }
                        m_abyBuffer.insert(m_abyBuffer.end(),
                                         reinterpret_cast<const uint8_t*>(asRawFields[i].String),
                                         reinterpret_cast<const uint8_t*>(asRawFields[i].String) + nLen);
                    }
                }
                else
                {
                    WriteUTF16String(m_abyBuffer, asRawFields[i].String,
                                     NUMBER_OF_BYTES_ON_VARUINT);
                }
                break;
            }

            case FGFT_DATETIME:
            {
                WriteFloat64(m_abyBuffer, FileGDBOGRDateToDoubleDate(&asRawFields[i]));
                break;
            }

            case FGFT_OBJECTID:
            {
                CPLAssert(false); // not possible given above processing
                break;
            }

            case FGFT_GEOMETRY:
            {
                CPLAssert(false); // not possible given above processing
                break;
            }

            case FGFT_BINARY:
            {
                WriteVarUInt(m_abyBuffer, asRawFields[i].Binary.nCount);
                if( asRawFields[i].Binary.nCount )
                {
                    if( static_cast<size_t>(asRawFields[i].Binary.nCount) + m_abyBuffer.size() >
                            static_cast<size_t>(INT_MAX) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Too large feature");
                        return false;
                    }
                    m_abyBuffer.insert(m_abyBuffer.end(),
                                     asRawFields[i].Binary.paData,
                                     asRawFields[i].Binary.paData + asRawFields[i].Binary.nCount);
                }
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
                const auto nLen = strlen(asRawFields[i].String);
                if( nLen != 38 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Bad size for UUID field");
                    return false;
                }
                std::vector<unsigned> anVals(16);
                sscanf(asRawFields[i].String,
                    "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                    &anVals[3], &anVals[2], &anVals[1], &anVals[0],
                    &anVals[5], &anVals[4],
                    &anVals[7], &anVals[6],
                    &anVals[8], &anVals[9],
                    &anVals[10], &anVals[11], &anVals[12],
                    &anVals[13], &anVals[14], &anVals[15]);
                for( auto v: anVals )
                {
                    m_abyBuffer.push_back(static_cast<uint8_t>(v));
                }
                break;
            }

        }

        if( poField->IsNullable() )
        {
            m_abyBuffer[iNullableField / 8] &= ~(1 << (iNullableField % 8));
            iNullableField ++;
        }
    }

    if( m_abyBuffer.size() > static_cast<size_t>(INT_MAX) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large feature");
        return false;
    }

    return true;
}

/************************************************************************/
/*                       SeekIntoTableXForNewFeature()                  */
/************************************************************************/

bool FileGDBTable::SeekIntoTableXForNewFeature(int nObjectID)
{
    int iCorrectedRow;
    bool bWriteEmptyPageAtEnd = false;
    const uint32_t nPageSize = TABLX_FEATURES_PER_PAGE * m_nTablxOffsetSize;

    if( m_abyTablXBlockMap.empty() )
    {
        // Is the OID to write in the current allocated pages, or in the next
        // page ?
        if( (nObjectID - 1) / TABLX_FEATURES_PER_PAGE <=
                ((m_nTotalRecordCount == 0) ? 0 : (1 + (m_nTotalRecordCount - 1) / TABLX_FEATURES_PER_PAGE)) )
        {
            iCorrectedRow = nObjectID - 1;
            const auto n1024BlocksPresentBefore = m_n1024BlocksPresent;
            m_n1024BlocksPresent = DIV_ROUND_UP(std::max(m_nTotalRecordCount, nObjectID), TABLX_FEATURES_PER_PAGE);
            bWriteEmptyPageAtEnd = m_n1024BlocksPresent > n1024BlocksPresentBefore;
        }
        else
        {
            // No, then we have a sparse table, and need to use a bitmap
            m_abyTablXBlockMap.resize( (DIV_ROUND_UP(nObjectID, TABLX_FEATURES_PER_PAGE) + 7) / 8 );
            for(int i = 0; i < DIV_ROUND_UP(m_nTotalRecordCount, TABLX_FEATURES_PER_PAGE); ++i )
                m_abyTablXBlockMap[i / 8] |= (1 << (i % 8));
            const int iBlock = (nObjectID - 1) / TABLX_FEATURES_PER_PAGE;
            m_abyTablXBlockMap[iBlock / 8] |= (1 << (iBlock % 8));
            iCorrectedRow = DIV_ROUND_UP(m_nTotalRecordCount, TABLX_FEATURES_PER_PAGE) *
                TABLX_FEATURES_PER_PAGE + ((nObjectID - 1) % TABLX_FEATURES_PER_PAGE);
            m_n1024BlocksPresent ++;
            bWriteEmptyPageAtEnd = true;
        }
    }
    else
    {
        const int iBlock = (nObjectID - 1) / TABLX_FEATURES_PER_PAGE;

        if( nObjectID <= m_nTotalRecordCount )
        {
            CPLAssert( iBlock / 8 < static_cast<int>(m_abyTablXBlockMap.size()) );
            if( TEST_BIT(m_abyTablXBlockMap.data(), iBlock) == 0 )
            {
                // This requires rewriting the gdbtablx file to insert
                // a new page
                GUInt32 nCountBlocksBefore = 0;
                for(int i=0;i<iBlock;i++)
                    nCountBlocksBefore += TEST_BIT(m_abyTablXBlockMap.data(), i) != 0;

                std::vector<GByte> abyTmp(nPageSize);
                uint64_t nOffset = TABLX_HEADER_SIZE +
                    static_cast<uint64_t>(m_n1024BlocksPresent - 1) * nPageSize;
                for( int i = m_n1024BlocksPresent - 1; i >= static_cast<int>(nCountBlocksBefore); --i )
                {
                    VSIFSeekL(m_fpTableX, nOffset, SEEK_SET);
                    if( VSIFReadL(abyTmp.data(), nPageSize, 1, m_fpTableX) != 1 )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                 "Cannot read .gdtablx page at offset %u",
                                 static_cast<uint32_t>(nOffset));
                        return false;
                    }
                    VSIFSeekL(m_fpTableX, VSIFTellL(m_fpTableX), SEEK_SET);
                    if( VSIFWriteL(abyTmp.data(), nPageSize, 1, m_fpTableX) != 1 )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                 "Cannot rewrite .gdtablx page of offset %u",
                                 static_cast<uint32_t>(nOffset));
                        return false;
                    }
                    nOffset -= nPageSize;
                }
                abyTmp.clear();
                abyTmp.resize(nPageSize);
                nOffset = TABLX_HEADER_SIZE +
                    static_cast<uint64_t>(nCountBlocksBefore) * nPageSize;
                VSIFSeekL(m_fpTableX, nOffset, SEEK_SET);
                if( VSIFWriteL(abyTmp.data(), nPageSize, 1, m_fpTableX) != 1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                                 "Cannot write empty .gdtablx page of offset %u",
                                 static_cast<uint32_t>(nOffset));
                    return false;
                }
                m_abyTablXBlockMap[iBlock / 8] |= (1 << (iBlock % 8));
                m_n1024BlocksPresent ++;
                m_bDirtyTableXTrailer = true;
                m_nOffsetTableXTrailer = 0;
                m_nCountBlocksBeforeIBlockIdx = iBlock;
                m_nCountBlocksBeforeIBlockValue = nCountBlocksBefore;
            }
        }
        else if( DIV_ROUND_UP(nObjectID, TABLX_FEATURES_PER_PAGE) > DIV_ROUND_UP(m_nTotalRecordCount, TABLX_FEATURES_PER_PAGE) )
        {
            m_abyTablXBlockMap.resize( (DIV_ROUND_UP(nObjectID, TABLX_FEATURES_PER_PAGE) + 7) / 8 );
            m_abyTablXBlockMap[iBlock / 8] |= (1 << (iBlock % 8));
            m_n1024BlocksPresent ++;
            bWriteEmptyPageAtEnd = true;
        }

        GUInt32 nCountBlocksBefore = 0;
        // In case of sequential access, optimization to avoid recomputing
        // the number of blocks since the beginning of the map
        if( iBlock >= m_nCountBlocksBeforeIBlockIdx )
        {
            nCountBlocksBefore = m_nCountBlocksBeforeIBlockValue;
            for(int i=m_nCountBlocksBeforeIBlockIdx;i<iBlock;i++)
                nCountBlocksBefore += TEST_BIT(m_abyTablXBlockMap.data(), i) != 0;
        }
        else
        {
            nCountBlocksBefore = 0;
            for(int i=0;i<iBlock;i++)
                nCountBlocksBefore += TEST_BIT(m_abyTablXBlockMap.data(), i) != 0;
        }

        m_nCountBlocksBeforeIBlockIdx = iBlock;
        m_nCountBlocksBeforeIBlockValue = nCountBlocksBefore;
        iCorrectedRow = nCountBlocksBefore * TABLX_FEATURES_PER_PAGE + ((nObjectID - 1) % TABLX_FEATURES_PER_PAGE);
    }

    if( bWriteEmptyPageAtEnd )
    {
        m_bDirtyTableXTrailer = true;
        m_nOffsetTableXTrailer = 0;
        std::vector<GByte> abyTmp(nPageSize);
        uint64_t nOffset = TABLX_HEADER_SIZE +
                static_cast<uint64_t>(m_n1024BlocksPresent - 1) * nPageSize;
        VSIFSeekL(m_fpTableX, nOffset, SEEK_SET);
        if( VSIFWriteL(abyTmp.data(), nPageSize, 1, m_fpTableX) != 1 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot write empty .gdtablx page of offset %u",
                         static_cast<uint32_t>(nOffset));
            return false;
        }
    }

    const uint64_t nOffset = TABLX_HEADER_SIZE + static_cast<uint64_t>(iCorrectedRow) * m_nTablxOffsetSize;
    VSIFSeekL(m_fpTableX, nOffset, SEEK_SET);

    return true;
}

/************************************************************************/
/*                        WriteFeatureOffset()                          */
/************************************************************************/

void FileGDBTable::WriteFeatureOffset(uint64_t nFeatureOffset, GByte* pabyBuffer)
{
    CPL_LSBPTR64(&nFeatureOffset);
    memcpy(pabyBuffer, &nFeatureOffset, m_nTablxOffsetSize);
}

/************************************************************************/
/*                        WriteFeatureOffset()                          */
/************************************************************************/

bool FileGDBTable::WriteFeatureOffset(uint64_t nFeatureOffset)
{
    CPL_LSBPTR64(&nFeatureOffset);
    return VSIFWriteL(&nFeatureOffset, m_nTablxOffsetSize, 1, m_fpTableX) == 1;
}

/************************************************************************/
/*                          CreateFeature()                             */
/************************************************************************/

bool FileGDBTable::CreateFeature(const std::vector<OGRField>& asRawFields,
                                 const OGRGeometry* poGeom,
                                 int* pnFID)
{
    if( !m_bUpdate )
        return false;

    if( m_bDirtyFieldDescriptors && !WriteFieldDescriptors(m_fpTable) )
        return false;

    int nObjectID;
    if( pnFID != nullptr && *pnFID > 0 )
    {
        if( *pnFID <= m_nTotalRecordCount &&
            GetOffsetInTableForRow((*pnFID)-1) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create feature of ID %d because one already exists",
                     *pnFID);
            return false;
        }
        nObjectID = *pnFID;
    }
    else
    {
        if( m_nTotalRecordCount == std::numeric_limits<int>::max() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Maximum number of records per table reached");
            return false;
        }
        nObjectID = m_nTotalRecordCount + 1;
    }

    try
    {
        if( !EncodeFeature(asRawFields, poGeom, -1) )
            return false;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    const uint64_t nFreeOffset = GetOffsetOfFreeAreaFromFreeList(
        static_cast<uint32_t>(sizeof(uint32_t) + m_abyBuffer.size()));
    if( nFreeOffset == OFFSET_MINUS_ONE )
    {
        if( ((m_nFileSize + m_abyBuffer.size()) >> (8 * m_nTablxOffsetSize)) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Maximum file size for m_nTablxOffsetSize = %u reached",
                     m_nTablxOffsetSize);
            return false;
        }
    }

    if( !SeekIntoTableXForNewFeature(nObjectID) )
        return false;

    if( nFreeOffset == OFFSET_MINUS_ONE )
    {
        VSIFSeekL(m_fpTable, m_nFileSize, SEEK_SET);
    }
    else
    {
        VSIFSeekL(m_fpTable, nFreeOffset, SEEK_SET);
    }
    if( !WriteUInt32(m_fpTable, static_cast<uint32_t>(m_abyBuffer.size())) )
        return false;
    if( !m_abyBuffer.empty() &&
        VSIFWriteL(m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable) != m_abyBuffer.size() )
    {
        return false;
    }

    if( !WriteFeatureOffset(nFreeOffset == OFFSET_MINUS_ONE ? m_nFileSize : nFreeOffset) )
        return false;
    if( pnFID )
        *pnFID = nObjectID;

    m_nRowBlobLength = static_cast<uint32_t>(m_abyBuffer.size());
    m_nRowBufferMaxSize = std::max(m_nRowBufferMaxSize, m_nRowBlobLength);
    if( nFreeOffset == OFFSET_MINUS_ONE )
    {
        m_nFileSize += sizeof(uint32_t) + m_nRowBlobLength;
    }

    m_nTotalRecordCount = std::max(m_nTotalRecordCount, nObjectID);
    m_nValidRecordCount ++;

    m_bDirtyHeader = true;
    m_bDirtyTableXHeader = true;

    m_bDirtyIndices = true;

    return true;
}

/************************************************************************/
/*                          UpdateFeature()                             */
/************************************************************************/

bool FileGDBTable::UpdateFeature(int nFID,
                                 const std::vector<OGRField>& asRawFields,
                                 const OGRGeometry* poGeom)
{
    if( !m_bUpdate )
        return false;

    if( m_bDirtyFieldDescriptors && !WriteFieldDescriptors(m_fpTable) )
        return false;

    vsi_l_offset nOffsetInTableX = 0;
    vsi_l_offset nOffsetInTable = GetOffsetInTableForRow(nFID-1, &nOffsetInTableX);
    if( nOffsetInTable == 0 )
        return false;

    try
    {
        if( !EncodeFeature(asRawFields, poGeom, -1) )
            return false;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    VSIFSeekL(m_fpTable, nOffsetInTable, SEEK_SET);
    uint32_t nOldFeatureSize = 0;
    if( !ReadUInt32(m_fpTable, nOldFeatureSize) )
        return false;

    m_nCurRow = -1;

    if( m_abyBuffer.size() <= nOldFeatureSize )
    {
        // Can rewrite-in-place
        VSIFSeekL(m_fpTable, nOffsetInTable, SEEK_SET);

        if( !WriteUInt32(m_fpTable, static_cast<uint32_t>(m_abyBuffer.size())) )
            return false;
        if( !m_abyBuffer.empty() &&
            VSIFWriteL(m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable) != m_abyBuffer.size() )
        {
            return false;
        }

        m_nRowBlobLength = 0;
        const size_t nSizeToBlank = nOldFeatureSize - m_abyBuffer.size();
        if( nSizeToBlank > 0 )
        {
            // Blank unused areas of the old feature
            m_abyBuffer.clear();
            try
            {
                m_abyBuffer.resize(nSizeToBlank);
                CPL_IGNORE_RET_VAL(VSIFWriteL( m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable ));
            }
            catch( const std::exception& e )
            {
                CPLDebug("OpenFileGDB", "Could not blank no longer part of feature: %s", e.what());
            }
        }
    }
    else
    {
        // Updated feature is larger than older one: append at end of .gdbtable
        const uint64_t nFreeOffset = GetOffsetOfFreeAreaFromFreeList(
            static_cast<uint32_t>(m_abyBuffer.size()));

        if( nFreeOffset == OFFSET_MINUS_ONE )
        {
            if( ((m_nFileSize + m_abyBuffer.size()) >> (8 * m_nTablxOffsetSize)) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Maximum file size for m_nTablxOffsetSize = %u reached",
                         m_nTablxOffsetSize);
                return false;
            }

            VSIFSeekL(m_fpTable, m_nFileSize, SEEK_SET);
        }
        else
        {
            VSIFSeekL(m_fpTable, nFreeOffset, SEEK_SET);
        }

        if( !WriteUInt32(m_fpTable, static_cast<uint32_t>(m_abyBuffer.size())) )
            return false;
        if( !m_abyBuffer.empty() &&
            VSIFWriteL(m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable) != m_abyBuffer.size() )
        {
            return false;
        }

        // Update offset of feature in .gdbtablx
        VSIFSeekL(m_fpTableX, nOffsetInTableX, SEEK_SET);
        if( !WriteFeatureOffset(nFreeOffset == OFFSET_MINUS_ONE ? m_nFileSize : nFreeOffset) )
            return false;

        m_nRowBlobLength = static_cast<uint32_t>(m_abyBuffer.size());
        m_nRowBufferMaxSize = std::max(m_nRowBufferMaxSize, m_nRowBlobLength);
        if( nFreeOffset == OFFSET_MINUS_ONE )
        {
            m_nFileSize += sizeof(uint32_t) + m_nRowBlobLength;
        }

        AddEntryToFreelist(nOffsetInTable, sizeof(uint32_t) + nOldFeatureSize);

        // Blank previously used area
        VSIFSeekL(m_fpTable, nOffsetInTable, SEEK_SET);
        const uint32_t nNegatedOldFeatureSize = static_cast<uint32_t>(-static_cast<int>(nOldFeatureSize));
        if( !WriteUInt32(m_fpTable, nNegatedOldFeatureSize) )
            return false;
        m_abyBuffer.clear();
        try
        {
            m_abyBuffer.resize(nOldFeatureSize);
            CPL_IGNORE_RET_VAL(VSIFWriteL( m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable ));
        }
        catch( const std::exception& e )
        {
            CPLDebug("OpenFileGDB", "Could not blank old feature: %s", e.what());
        }
    }

    m_bDirtyIndices = true;

    return true;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

bool FileGDBTable::DeleteFeature(int nFID)
{
    if( !m_bUpdate )
        return false;

    if( m_bDirtyFieldDescriptors && !WriteFieldDescriptors(m_fpTable) )
        return false;

    vsi_l_offset nOffsetInTableX = 0;
    vsi_l_offset nOffsetInTable = GetOffsetInTableForRow(nFID-1, &nOffsetInTableX);
    if( nOffsetInTable == 0 )
        return false;

    // Set 0 as offset for the feature in .gdbtablx
    VSIFSeekL(m_fpTableX, nOffsetInTableX, SEEK_SET);
    if( !WriteFeatureOffset(0) )
        return false;

    // Negate the size of the feature in .gdbtable
    VSIFSeekL(m_fpTable, nOffsetInTable, SEEK_SET);
    uint32_t nFeatureSize = 0;
    if( !ReadUInt32(m_fpTable, nFeatureSize) )
        return false;
    if( nFeatureSize > static_cast<uint32_t>(INT_MAX) )
        return false;
    const int nDeletedFeatureSize = static_cast<uint32_t>(-static_cast<int32_t>(nFeatureSize));
    VSIFSeekL(m_fpTable, nOffsetInTable, SEEK_SET);
    if( !WriteUInt32(m_fpTable, nDeletedFeatureSize) )
        return false;

    AddEntryToFreelist(nOffsetInTable, sizeof(uint32_t) + nFeatureSize);

    // Blank feature content
    m_nCurRow = -1;
    m_abyBuffer.clear();
    try
    {
        m_abyBuffer.resize(nFeatureSize);
        CPL_IGNORE_RET_VAL(VSIFWriteL( m_abyBuffer.data(), 1, m_abyBuffer.size(), m_fpTable ));
    }
    catch( const std::exception& e )
    {
        CPLDebug("OpenFileGDB", "Could not blank deleted feature: %s", e.what());
    }

    m_nValidRecordCount --;
    m_bDirtyHeader = true;

    m_bDirtyIndices = true;

    return true;
}

/************************************************************************/
/*               WholeFileRewriter::~WholeFileRewriter()                */
/************************************************************************/

FileGDBTable::WholeFileRewriter::~WholeFileRewriter()
{
    if( m_bIsInit )
        Rollback();
}

/************************************************************************/
/*                    WholeFileRewriter::Begin()                        */
/************************************************************************/

bool FileGDBTable::WholeFileRewriter::Begin()
{
    m_bOldDirtyIndices = m_oTable.m_bDirtyIndices;
    m_oTable.RemoveIndices();
    m_oTable.m_bDirtyIndices = false;
    if( !m_oTable.Sync() )
        return false;

    // On Windows, we might have issues renaming opened files, even if trying
    // to close them before, so updating opened files is less risky.
    m_bModifyInPlace = CPLTestBool(
        CPLGetConfigOption("OPENFILEGDB_MODIFY_IN_PLACE",
#ifdef _WIN32
                           "YES"
#else
                           "NO"
#endif
    ));

    m_osGdbTablx = CPLFormFilename(CPLGetPath(m_oTable.m_osFilename.c_str()),
        CPLGetBasename(m_oTable.m_osFilename.c_str()), "gdbtablx");

    m_osBackupGdbTable = CPLResetExtension(m_oTable.m_osFilename.c_str(),
                                           "_backup.gdbtable");
    VSIStatBufL sStat;
    if( VSIStatL(m_osBackupGdbTable.c_str(), &sStat) == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create backup file %s as it already exists",
                 m_osBackupGdbTable.c_str());
        return false;
    }

    m_osBackupGdbTablx = CPLResetExtension(m_osGdbTablx.c_str(),
                                           "_backup.gdbtablx");

    if( m_bModifyInPlace )
    {
        // Create backups of .gdtable and .gdtablx if something wrongs happen
        if( CPLCopyFile(m_osBackupGdbTable.c_str(), m_oTable.m_osFilename.c_str()) != 0 )
        {
            VSIUnlink(m_osBackupGdbTable.c_str());
            m_osBackupGdbTable.clear();
            return false;
        }

        if( CPLCopyFile(m_osBackupGdbTablx.c_str(), m_osGdbTablx.c_str()) != 0 )
        {
            VSIUnlink(m_osBackupGdbTable.c_str());
            VSIUnlink(m_osBackupGdbTablx.c_str());
            m_osBackupGdbTable.clear();
            m_osBackupGdbTablx.clear();
            return false;
        }

        m_osBackupValidFilename = m_oTable.m_osFilename + ".backup_valid";
        VSILFILE* fp = VSIFOpenL(m_osBackupValidFilename.c_str(), "wb");
        if( fp != nullptr )
            VSIFCloseL(fp);

        m_fpOldGdbtable = VSIFOpenL(m_osBackupGdbTable.c_str(), "rb");
        if( m_fpOldGdbtable == nullptr )
        {
            VSIUnlink(m_osBackupValidFilename.c_str());
            VSIUnlink(m_osBackupGdbTable.c_str());
            VSIUnlink(m_osBackupGdbTablx.c_str());
            m_osBackupValidFilename.clear();
            m_osBackupGdbTable.clear();
            m_osBackupGdbTablx.clear();
            return false;
        }

        m_fpOldGdbtablx = m_oTable.m_fpTableX;
        m_fpTable = m_oTable.m_fpTable;
        m_fpTableX = m_oTable.m_fpTableX;
    }
    else
    {
        m_osTmpGdbTable = CPLResetExtension(m_oTable.m_osFilename.c_str(),
                                            "_compress.gdbtable");
        m_osTmpGdbTablx = CPLResetExtension(m_osGdbTablx.c_str(),
                                            "_compress.gdbtablx");

        m_fpOldGdbtable = m_oTable.m_fpTable;
        m_fpOldGdbtablx = m_oTable.m_fpTableX;

        m_fpTable = VSIFOpenL(m_osTmpGdbTable.c_str(), "wb+");
        if( m_fpTable == nullptr )
        {
            return false;
        }

        m_fpTableX = VSIFOpenL(m_osTmpGdbTablx.c_str(), "wb+");
        if( m_fpTableX == nullptr )
        {
            VSIFCloseL(m_fpTable);
            m_fpTable = nullptr;
            VSIUnlink(m_osTmpGdbTable.c_str());
            return false;
        }

        if( !m_oTable.WriteHeaderX(m_fpTableX) )
        {
            VSIFCloseL(m_fpTable);
            m_fpTable = nullptr;
            VSIFCloseL(m_fpTableX);
            m_fpTableX = nullptr;
            VSIUnlink(m_osTmpGdbTable.c_str());
            VSIUnlink(m_osTmpGdbTablx.c_str());
            m_osTmpGdbTable.clear();
            m_osTmpGdbTablx.clear();
            return false;
        }
    }

    m_nOldFileSize = m_oTable.m_nFileSize;
    m_nOldOffsetFieldDesc = m_oTable.m_nOffsetFieldDesc;
    m_nOldFieldDescLength = m_oTable.m_nFieldDescLength;
    m_bIsInit = true;

    if( !m_oTable.WriteHeader(m_fpTable) )
    {
        Rollback();
        return false;
    }
    if( m_bModifyInPlace )
    {
        VSIFTruncateL(m_fpTable, m_oTable.m_nFileSize);
    }

    // Rewrite field descriptors
    if( !m_oTable.Sync(m_fpTable, m_fpTableX) )
    {
        Rollback();
        return false;
    }

    VSIFSeekL(m_fpTable, m_oTable.m_nFileSize, SEEK_SET);

    return true;
}

/************************************************************************/
/*                    WholeFileRewriter::Commit()                       */
/************************************************************************/

bool FileGDBTable::WholeFileRewriter::Commit()
{
    m_oTable.m_bDirtyTableXTrailer = true;
    m_oTable.m_bDirtyHeader = true;
    if( !m_oTable.Sync(m_fpTable, m_fpTableX) )
    {
        Rollback();
        return false;
    }

    if( m_bModifyInPlace )
    {
        VSIFCloseL(m_fpOldGdbtable);
        VSIUnlink(m_osBackupValidFilename.c_str());
        VSIUnlink(m_osBackupGdbTable.c_str());
        VSIUnlink(m_osBackupGdbTablx.c_str());
    }
    else
    {
        VSIFCloseL(m_oTable.m_fpTable);
        VSIFCloseL(m_oTable.m_fpTableX);
        m_oTable.m_fpTable = nullptr;
        m_oTable.m_fpTableX = nullptr;

        const bool bUseWIN32CodePath = CPLTestBool(
            CPLGetConfigOption("OPENFILEGDB_SIMUL_WIN32",
#ifdef _WIN32
                               "YES"
#else
                               "NO"
#endif
        ));

        if( bUseWIN32CodePath )
        {
            // Renaming over an open file doesn't work on Windows
            VSIFCloseL(m_fpTable);
            VSIFCloseL(m_fpTableX);
            m_fpTable = nullptr;
            m_fpTableX = nullptr;

            // _wrename() on Windows doesn't honour POSIX semantics and forbids
            // renaming over an existing file, hence create a temporary backup
            if( VSIRename(m_oTable.m_osFilename.c_str(), m_osBackupGdbTable.c_str()) != 0  )
            {
                m_oTable.m_fpTable = VSIFOpenL(m_oTable.m_osFilename.c_str(), "rb+");
                m_oTable.m_fpTableX = VSIFOpenL(m_osGdbTablx.c_str(), "rb+");
                Rollback();
                return false;
            }

            if( VSIRename(m_osGdbTablx.c_str(), m_osBackupGdbTablx.c_str()) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Renaming of %s onto %s failed, but renaming of "
                         "%s onto %s succeeded. Dataset in corrupt state",
                         m_osGdbTablx.c_str(), m_osBackupGdbTablx.c_str(),
                         m_oTable.m_osFilename.c_str(), m_osBackupGdbTable.c_str());
                Rollback();
                return false;
            }
        }
        else
        {
            m_oTable.m_fpTable = m_fpTable;
            m_oTable.m_fpTableX = m_fpTableX;
        }

        if( VSIRename(m_osTmpGdbTable.c_str(), m_oTable.m_osFilename.c_str()) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Renaming of %s onto %s failed",
                     m_osTmpGdbTable.c_str(), m_oTable.m_osFilename.c_str());
            Rollback();
            return false;
        }

        if( VSIRename(m_osTmpGdbTablx.c_str(), m_osGdbTablx.c_str()) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Renaming of %s onto %s failed",
                     m_osTmpGdbTablx.c_str(), m_osGdbTablx.c_str());
            Rollback();
            return false;
        }

        if( bUseWIN32CodePath )
        {
            m_oTable.m_fpTable = VSIFOpenL(m_oTable.m_osFilename.c_str(), "rb+");
            m_oTable.m_fpTableX = VSIFOpenL(m_osGdbTablx.c_str(), "rb+");
            VSIUnlink(m_osBackupGdbTable.c_str());
            VSIUnlink(m_osBackupGdbTablx.c_str());
        }
    }

    m_oTable.DeleteFreeList();
    if( m_bOldDirtyIndices )
    {
        m_oTable.m_bDirtyIndices = true;
        m_oTable.Sync();
    }

    m_bIsInit = false;

    return true;
}

/************************************************************************/
/*                   WholeFileRewriter::Rollback()                      */
/************************************************************************/

void FileGDBTable::WholeFileRewriter::Rollback()
{
    CPLAssert(m_bIsInit);
    m_bIsInit = false;

    if( m_bModifyInPlace )
    {
        VSIFCloseL(m_fpOldGdbtable);
        m_fpOldGdbtable = nullptr;

        // Try to restore from backup files in case of failure
        if( CPLCopyFile(m_oTable.m_osFilename.c_str(), m_osBackupGdbTable.c_str()) == 0 &&
            CPLCopyFile(m_osGdbTablx.c_str(), m_osBackupGdbTablx.c_str()) == 0 )
        {
            VSIUnlink(m_osBackupValidFilename.c_str());
            VSIUnlink(m_osBackupGdbTable.c_str());
            VSIUnlink(m_osBackupGdbTablx.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s and %s are corrupted, and couldn't be restored from "
                     "their backups %s and %s. You'll have to manually replace "
                     "the former files by the latter ones.",
                     m_oTable.m_osFilename.c_str(),
                     m_osGdbTablx.c_str(),
                     m_osBackupGdbTable.c_str(),
                     m_osBackupGdbTablx.c_str());
        }
    }
    else
    {
        VSIFCloseL(m_fpTable);
        VSIFCloseL(m_fpTableX);
        m_fpTable = nullptr;
        m_fpTableX = nullptr;
        VSIUnlink(m_osTmpGdbTable.c_str());
        VSIUnlink(m_osTmpGdbTablx.c_str());
    }

    m_oTable.m_nFileSize = m_nOldFileSize;
    m_oTable.m_nOffsetFieldDesc = m_nOldOffsetFieldDesc;
    m_oTable.m_nFieldDescLength = m_nOldFieldDescLength;

    m_oTable.m_bDirtyFieldDescriptors = false;
    m_oTable.m_bDirtyTableXHeader = false;
    m_oTable.m_bDirtyTableXTrailer = false;
    m_oTable.m_bDirtyHeader = false;
}

/************************************************************************/
/*                                Repack()                              */
/************************************************************************/

bool FileGDBTable::Repack()
{
    if( !m_bUpdate || !Sync() )
        return false;

    bool bRepackNeeded = false;
    if( m_nOffsetFieldDesc > 40 )
    {
        // If the field descriptor section is not at offset 40, it is possible
        // that there's our "ghost area" there.
        GByte abyBuffer[8] = {0};
        VSIFSeekL(m_fpTable, 40, SEEK_SET);
        VSIFReadL(abyBuffer, 1, sizeof(abyBuffer), m_fpTable);
        if( !(memcmp(abyBuffer + 4, "GDAL", 4) == 0 &&
              static_cast<uint64_t>(40) + sizeof(uint32_t) +
                  GetUInt32(abyBuffer, 0) == m_nOffsetFieldDesc) )
        {
            CPLDebug("OpenFileGDB",
                     "Repack(%s): field descriptors not at beginning of file",
                     m_osFilename.c_str());
            bRepackNeeded = true;
        }
    }

    uint64_t nExpectedOffset =
        m_nOffsetFieldDesc + sizeof(uint32_t) + m_nFieldDescLength;

    std::vector<GByte> abyBufferOffsets;
    abyBufferOffsets.resize(TABLX_FEATURES_PER_PAGE * m_nTablxOffsetSize);

    // Scan all features
    for( uint32_t iPage = 0; !bRepackNeeded && iPage < m_n1024BlocksPresent; ++iPage )
    {
        const vsi_l_offset nOffsetInTableX =
            TABLX_HEADER_SIZE + m_nTablxOffsetSize * static_cast<vsi_l_offset>(iPage) * TABLX_FEATURES_PER_PAGE;
        VSIFSeekL(m_fpTableX, nOffsetInTableX, SEEK_SET);
        if( VSIFReadL(abyBufferOffsets.data(),
                      m_nTablxOffsetSize * TABLX_FEATURES_PER_PAGE, 1, m_fpTableX) != 1 )
            return false;

        GByte* pabyBufferOffsets = abyBufferOffsets.data();
        for( int i = 0; i < TABLX_FEATURES_PER_PAGE; i++, pabyBufferOffsets += m_nTablxOffsetSize )
        {
            const uint64_t nOffset = ReadFeatureOffset(pabyBufferOffsets);
            if( nOffset != 0 )
            {
                if( !bRepackNeeded && nOffset != nExpectedOffset )
                {
                    bRepackNeeded = true;
                    CPLDebug("OpenFileGDB",
                             "Repack(%s): feature at offset " CPL_FRMT_GUIB
                             " instead of " CPL_FRMT_GUIB ". Repack needed",
                             m_osFilename.c_str(),
                             static_cast<GUIntBig>(nOffset),
                             static_cast<GUIntBig>(nExpectedOffset));
                    break;
                }

                // Read feature size
                VSIFSeekL(m_fpTable, nOffset, SEEK_SET);
                uint32_t nFeatureSize = 0;
                if( !ReadUInt32(m_fpTable, nFeatureSize) )
                    return false;

                nExpectedOffset += sizeof(uint32_t);
                nExpectedOffset += nFeatureSize;
            }
        }
    }

    if( !bRepackNeeded )
    {
        if( m_nFileSize > nExpectedOffset )
        {
            CPLDebug("OpenFileGDB",
                     "Deleted features at end of file. Truncating it");

            m_nFileSize = nExpectedOffset;
            VSIFTruncateL(m_fpTable, m_nFileSize);
            m_bDirtyHeader = true;

            DeleteFreeList();

            return Sync();
        }

        CPLDebug("OpenFileGDB",
                 "Repack(%s): file already compacted",
                 m_osFilename.c_str());
        return true;
    }

    WholeFileRewriter oWholeFileRewriter(*this);
    if( !oWholeFileRewriter.Begin() )
        return false;

    uint32_t nRowBufferMaxSize = 0;
    m_nCurRow = -1;

    // Rewrite all features
    for( uint32_t iPage = 0; iPage < m_n1024BlocksPresent; ++iPage )
    {
        const vsi_l_offset nOffsetInTableX = TABLX_HEADER_SIZE +
            m_nTablxOffsetSize * static_cast<vsi_l_offset>(iPage) * TABLX_FEATURES_PER_PAGE;
        VSIFSeekL(oWholeFileRewriter.m_fpOldGdbtablx, nOffsetInTableX, SEEK_SET);
        if( VSIFReadL(abyBufferOffsets.data(),
                      m_nTablxOffsetSize * TABLX_FEATURES_PER_PAGE, 1,
                      oWholeFileRewriter.m_fpOldGdbtablx) != 1 )
            return false;

        GByte* pabyBufferOffsets = abyBufferOffsets.data();
        for( int i = 0; i < TABLX_FEATURES_PER_PAGE; i++, pabyBufferOffsets += m_nTablxOffsetSize )
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
                if (VSIFReadL(m_abyBuffer.data(), nFeatureSize, 1,
                              oWholeFileRewriter.m_fpOldGdbtable) != 1 )
                    return false;

                // Update offset of updated feature
                WriteFeatureOffset(m_nFileSize, pabyBufferOffsets);

                // Write feature size
                if( !WriteUInt32(oWholeFileRewriter.m_fpTable, nFeatureSize) )
                    return false;
                if( VSIFWriteL(m_abyBuffer.data(), nFeatureSize, 1,
                               oWholeFileRewriter.m_fpTable) != 1 )
                    return false;

                if( nFeatureSize > nRowBufferMaxSize )
                    nRowBufferMaxSize = nFeatureSize;
                m_nFileSize += sizeof(uint32_t) + nFeatureSize;
            }
        }
        VSIFSeekL(oWholeFileRewriter.m_fpTableX, nOffsetInTableX, SEEK_SET);
        if( VSIFWriteL(abyBufferOffsets.data(),
                       m_nTablxOffsetSize * TABLX_FEATURES_PER_PAGE, 1,
                       oWholeFileRewriter.m_fpTableX) != 1 )
            return false;
    }

    m_nRowBufferMaxSize = nRowBufferMaxSize;
    m_nHeaderBufferMaxSize = std::max(m_nFieldDescLength, m_nRowBufferMaxSize);

    return oWholeFileRewriter.Commit();
}

/************************************************************************/
/*                          RecomputeExtent()                           */
/************************************************************************/

void FileGDBTable::RecomputeExtent()
{
    if( !m_bUpdate || m_iGeomField < 0 )
        return;

    // Scan all features
    OGREnvelope sLayerEnvelope;
    OGREnvelope sFeatureEnvelope;
    for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
    {
        iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
        if( iCurFeat < 0 )
            break;
        const auto psGeomField = GetFieldValue(m_iGeomField);
        if( psGeomField && GetFeatureExtent(psGeomField, &sFeatureEnvelope) )
        {
            sLayerEnvelope.Merge(sFeatureEnvelope);
        }
    }

    m_bDirtyGeomFieldBBox = true;
    auto poGeomField = cpl::down_cast<FileGDBGeomField*>(m_apoFields[m_iGeomField].get());
    if( sLayerEnvelope.IsInit() )
    {
        poGeomField->SetXYMinMax(sLayerEnvelope.MinX,
                                 sLayerEnvelope.MinY,
                                 sLayerEnvelope.MaxX,
                                 sLayerEnvelope.MaxY);
    }
    else
    {
        poGeomField->SetXYMinMax(FileGDBGeomField::ESRI_NAN,
                                 FileGDBGeomField::ESRI_NAN,
                                 FileGDBGeomField::ESRI_NAN,
                                 FileGDBGeomField::ESRI_NAN);
    }
}

} /* namespace OpenFileGDB */
