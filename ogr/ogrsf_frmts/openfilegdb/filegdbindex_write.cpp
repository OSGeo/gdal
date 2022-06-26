/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements writing of FileGDB indices
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

#include <cctype>
#include <algorithm>
#include <limits>

#include "cpl_string.h"

namespace OpenFileGDB
{

/************************************************************************/
/*                          RemoveIndices()                             */
/************************************************************************/

void FileGDBTable::RemoveIndices()
{
    if( !m_bUpdate )
        return;

    CPLString osUCGeomFieldName;
    if( m_iGeomField >= 0 )
    {
        osUCGeomFieldName = m_apoFields[m_iGeomField]->GetName();
        osUCGeomFieldName.toupper();
    }

    GetIndexCount();
    for( const auto& poIndex: m_apoIndexes )
    {
        if( m_iObjectIdField >= 0 &&
            m_apoFields[m_iObjectIdField]->m_poIndex == poIndex.get() )
        {
            continue;
        }

        CPLString osUCIndexFieldName(poIndex->GetExpression());
        osUCIndexFieldName.toupper();
        if( osUCIndexFieldName == osUCGeomFieldName )
        {
            VSIUnlink( CPLResetExtension( m_osFilename.c_str(), "spx") );
        }
        else
        {
            VSIUnlink( CPLResetExtension( m_osFilename.c_str(),
                              (poIndex->GetIndexName() + ".atx").c_str()) );
        }
    }

    m_nHasSpatialIndex = false;
}

/************************************************************************/
/*                          RefreshIndices()                            */
/************************************************************************/

void FileGDBTable::RefreshIndices()
{
    if( !m_bUpdate )
        return;

    RemoveIndices();

    for( const auto& poIndex: m_apoIndexes )
    {
        if( m_iObjectIdField >= 0 &&
            m_apoFields[m_iObjectIdField]->m_poIndex == poIndex.get() )
        {
            continue;
        }

        if( m_iGeomField >= 0 &&
            m_apoFields[m_iGeomField]->m_poIndex == poIndex.get() &&
            m_eTableGeomType != FGTGT_MULTIPATCH )
        {
            CreateSpatialIndex();
        }
        else
        {
            const std::string osFieldName = poIndex->GetFieldName();
            const int iField = GetFieldIdx(osFieldName);
            if( iField >= 0 )
            {
                const auto eFieldType = m_apoFields[iField]->GetType();
                if( eFieldType == FGFT_INT16 ||
                    eFieldType == FGFT_INT32 ||
                    eFieldType == FGFT_FLOAT32 ||
                    eFieldType == FGFT_FLOAT64 ||
                    eFieldType == FGFT_STRING ||
                    eFieldType == FGFT_DATETIME )
                {
                    CreateAttributeIndex(poIndex.get());
                }
            }
        }
    }
}

/************************************************************************/
/*                          CreateIndex()                               */
/************************************************************************/

bool FileGDBTable::CreateIndex(const std::string& osIndexName,
                               const std::string& osExpression)
{
    if( !m_bUpdate )
        return false;

    if( osIndexName.empty() || !(
            (osIndexName[0] >= 'a' && osIndexName[0] <= 'z') ||
            (osIndexName[0] >= 'A' && osIndexName[0] <= 'Z')) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid index name: must start with a letter");
        return false;
    }

    for( const char ch: osIndexName )
    {
        if( !isalnum(ch) && ch != '_' )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid index name: must contain only alpha numeric character or _");
            return false;
        }
    }

    if( osIndexName.size() > 16 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid index name: cannot be greater than 16 characters");
        return false;
    }

    for( const auto& poIndex: m_apoIndexes )
    {
        if( EQUAL(poIndex->GetIndexName().c_str(), osIndexName.c_str()) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "An index with same name already exists");
            return false;
        }
    }

    const std::string osFieldName =
        FileGDBIndex::GetFieldNameFromExpression(osExpression);
    const int iField = GetFieldIdx(osFieldName);
    if( iField < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find field %s", osFieldName.c_str());
        return false;
    }

    if( m_apoFields[iField]->m_poIndex != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Field %s has already a registered index",
                 osFieldName.c_str());
        return false;
    }

    const auto eFieldType = m_apoFields[iField]->GetType();
    if( eFieldType != FGFT_OBJECTID &&
        eFieldType != FGFT_GEOMETRY &&
        eFieldType != FGFT_INT16 &&
        eFieldType != FGFT_INT32 &&
        eFieldType != FGFT_FLOAT32 &&
        eFieldType != FGFT_FLOAT64 &&
        eFieldType != FGFT_STRING &&
        eFieldType != FGFT_DATETIME )
    {
        // FGFT_GUID could potentially be added (cf a00000007.gdbindexes / GDBItemRelationshipTypes )
        // Not sure about FGFT_GLOBALID, FGFT_XML or FGFT_RASTER
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported field type for index creation");
        return false;
    }

    m_bDirtyGdbIndexesFile = true;

    auto poIndex = cpl::make_unique<FileGDBIndex>();
    poIndex->m_osIndexName = osIndexName;
    poIndex->m_osExpression = osExpression;

    if( iField != m_iObjectIdField && iField != m_iGeomField )
    {
        if( !CreateAttributeIndex(poIndex.get()) )
            return false;
    }

    m_apoFields[iField]->m_poIndex = poIndex.get();

    m_apoIndexes.push_back(std::move(poIndex));

    return true;
}

/************************************************************************/
/*                        CreateGdbIndexesFile()                        */
/************************************************************************/

void FileGDBTable::CreateGdbIndexesFile()
{
    std::vector<GByte> abyBuffer;

    WriteUInt32(abyBuffer, static_cast<uint32_t>(m_apoIndexes.size()));
    for( const auto& poIndex: m_apoIndexes )
    {
        const FileGDBField* poField = nullptr;
        for(size_t i=0;i<m_apoFields.size();i++)
        {
            if( CPLString(poIndex->GetFieldName()).toupper() ==
                CPLString(m_apoFields[i]->GetName()).toupper() )
            {
                poField = m_apoFields[i].get();
                break;
            }
        }
        if( poField == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field corresponding to index field name %s",
                     poIndex->GetFieldName().c_str());
            return;
        }

        WriteUTF16String(abyBuffer, poIndex->GetIndexName().c_str(), NUMBER_OF_CHARS_ON_UINT32);
        WriteUInt16(abyBuffer, 0); // unknown semantics
        if( poField->GetType() == FGFT_OBJECTID )
        {
            WriteUInt32(abyBuffer, 16); // unknown semantics
            WriteUInt16(abyBuffer, 0xFFFF); // unknown semantics
        }
        else if( poField->GetType() == FGFT_GEOMETRY )
        {
            WriteUInt32(abyBuffer, 4); // unknown semantics
            WriteUInt16(abyBuffer, 0); // unknown semantics
        }
        else
        {
            WriteUInt32(abyBuffer, 2); // unknown semantics
            WriteUInt16(abyBuffer, 0); // unknown semantics
        }
        WriteUInt32(abyBuffer, 1); // unknown semantics
        WriteUTF16String(abyBuffer, poIndex->GetExpression().c_str(), NUMBER_OF_CHARS_ON_UINT32);
        WriteUInt16(abyBuffer, 0); // unknown semantics
    }

    VSILFILE* fp = VSIFOpenL( CPLResetExtension( m_osFilename.c_str(), "gdbindexes"), "wb" );
    if( fp == nullptr )
        return;
    CPL_IGNORE_RET_VAL(VSIFWriteL(abyBuffer.data(), abyBuffer.size(), 1, fp));
    VSIFCloseL(fp);
}

/************************************************************************/
/*              ComputeOptimalSpatialIndexGridResolution()              */
/************************************************************************/

void FileGDBTable::ComputeOptimalSpatialIndexGridResolution()
{
    if( m_nValidRecordCount == 0 || m_iGeomField < 0 ||
        m_adfSpatialIndexGridResolution.size() != 1 )
    {
        return;
    }

    auto poGeomField = cpl::down_cast<FileGDBGeomField*>(m_apoFields[m_iGeomField].get());
    if( m_eTableGeomType == FGTGT_POINT )
    {
        // For point, use the density as the grid resolution
        int nValid = 0;
        for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
        {
            iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const OGRField* psField = GetFieldValue(m_iGeomField);
            if( psField != nullptr )
                nValid ++;
        }
        if( nValid > 0 )
        {
            const double dfArea =
                (poGeomField->GetXMax() - poGeomField->GetXMin()) *
                (poGeomField->GetYMax() - poGeomField->GetYMin());
            if( dfArea != 0 )
            {
                m_adfSpatialIndexGridResolution[0] = sqrt(dfArea / nValid);
            }
            else if( poGeomField->GetXMax() > poGeomField->GetXMin() )
            {
                m_adfSpatialIndexGridResolution[0] = (poGeomField->GetXMax() - poGeomField->GetXMin()) / nValid;
            }
            else if( poGeomField->GetYMax() > poGeomField->GetYMin() )
            {
                m_adfSpatialIndexGridResolution[0] = (poGeomField->GetYMax() - poGeomField->GetYMin()) / nValid;
            }
            else
            {
                return;
            }
            m_bDirtyGeomFieldSpatialIndexGridRes = true;
            poGeomField->m_adfSpatialIndexGridResolution = m_adfSpatialIndexGridResolution;
        }
    }

    else if( m_eTableGeomType == FGTGT_MULTIPOINT )
    {
        // For multipoint, use the density as the grid resolution
        int64_t nValid = 0;
        auto poGeomConverter = std::unique_ptr<FileGDBOGRGeometryConverter>(
            FileGDBOGRGeometryConverter::BuildConverter(poGeomField));
        for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
        {
            iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const OGRField* psField = GetFieldValue(m_iGeomField);
            if( psField != nullptr )
            {
                auto poGeom = std::unique_ptr<OGRGeometry>(poGeomConverter->GetAsGeometry(psField));
                if( poGeom != nullptr && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
                {
                    nValid += poGeom->toMultiPoint()->getNumGeometries();
                }
            }
        }
        if( nValid > 0 )
        {
            const double dfArea =
                (poGeomField->GetXMax() - poGeomField->GetXMin()) *
                (poGeomField->GetYMax() - poGeomField->GetYMin());
            if( dfArea != 0 )
            {
                m_adfSpatialIndexGridResolution[0] = sqrt(dfArea / nValid);
            }
            else if( poGeomField->GetXMax() > poGeomField->GetXMin() )
            {
                m_adfSpatialIndexGridResolution[0] = (poGeomField->GetXMax() - poGeomField->GetXMin()) / nValid;
            }
            else if( poGeomField->GetYMax() > poGeomField->GetYMin() )
            {
                m_adfSpatialIndexGridResolution[0] = (poGeomField->GetYMax() - poGeomField->GetYMin()) / nValid;
            }
            else
            {
                return;
            }
            m_bDirtyGeomFieldSpatialIndexGridRes = true;
            poGeomField->m_adfSpatialIndexGridResolution = m_adfSpatialIndexGridResolution;
        }
    }

    else
    {
        CPLDebug("OpenFileGDB", "Computing optimal grid size...");

        // For other types of geometries, just take the maximum extent along x/y
        // of all geometries
        double dfMaxSize = 0;
        OGREnvelope sEnvelope;
        for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
        {
            iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const OGRField* psField = GetFieldValue(m_iGeomField);
            if( psField != nullptr )
            {
                if( GetFeatureExtent(psField, &sEnvelope) )
                {
                    dfMaxSize = std::max(dfMaxSize, sEnvelope.MaxX - sEnvelope.MinX);
                    dfMaxSize = std::max(dfMaxSize, sEnvelope.MaxY - sEnvelope.MinY);
                }
            }
        }
        CPLDebug("OpenFileGDB", "Optimal grid size = %f", dfMaxSize);

        if( dfMaxSize > 0 )
        {
            m_bDirtyGeomFieldSpatialIndexGridRes = true;
            m_adfSpatialIndexGridResolution[0] = dfMaxSize;
            poGeomField->m_adfSpatialIndexGridResolution = m_adfSpatialIndexGridResolution;
        }
    }
}

/************************************************************************/
/*                           WriteIndex()                               */
/************************************************************************/

template<class ValueOIDPair> static bool WriteIndex(
    VSILFILE* fp,
    std::vector<ValueOIDPair>& asValues,
    void (*writeValueFunc)(std::vector<GByte>& abyPage,
                           const typename ValueOIDPair::first_type& value,
                           int maxStrSize),
    int& nDepth,
    int maxStrSize = 0)
{
    constexpr int IDX_PAGE_SIZE = 4096;
    constexpr int HEADER_SIZE_PAGE_REFERENCING_FEATURES = 12; // 3 * int32
    constexpr int SIZEOF_FEATURE_ID = 4; // sizeof(int)
    const int SIZEOF_INDEXED_VALUE = maxStrSize ? sizeof(uint16_t) * maxStrSize : sizeof(typename ValueOIDPair::first_type);
    const int NUM_MAX_FEATURES_PER_PAGE =
        (IDX_PAGE_SIZE - HEADER_SIZE_PAGE_REFERENCING_FEATURES) / (SIZEOF_FEATURE_ID + SIZEOF_INDEXED_VALUE);
    // static_assert(NUM_MAX_FEATURES_PER_PAGE == 340, "NUM_MAX_FEATURES_PER_PAGE == 340");
    const int OFFSET_FIRST_VAL_IN_PAGE =
        HEADER_SIZE_PAGE_REFERENCING_FEATURES +
            NUM_MAX_FEATURES_PER_PAGE * SIZEOF_FEATURE_ID;

    // Configurable only for debugging & autotest purposes
    const int numMaxFeaturesPerPage = [NUM_MAX_FEATURES_PER_PAGE]() {
        const int nVal = atoi(CPLGetConfigOption(
            "OPENFILEGDB_MAX_FEATURES_PER_SPX_PAGE", CPLSPrintf("%d", NUM_MAX_FEATURES_PER_PAGE)));
        if( nVal < 2 )
            return 2;
        if( nVal > NUM_MAX_FEATURES_PER_PAGE )
            return NUM_MAX_FEATURES_PER_PAGE;
        return nVal;
    } ();

    if( asValues.size() > static_cast<size_t>(INT_MAX) ||
        // Maximum number of values for depth == 4: this evaluates to ~ 13 billion values (~ features)
        asValues.size() >
            (((static_cast<uint64_t>(numMaxFeaturesPerPage) + 1) * numMaxFeaturesPerPage + 1) * numMaxFeaturesPerPage + 1) * numMaxFeaturesPerPage )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "More values in spatial index than can be handled");
        return false;
    }

    // Sort by ascending values, and for same value by ascending OID
    std::sort(asValues.begin(), asValues.end(),
              [](const ValueOIDPair& a, const ValueOIDPair& b)
    {
        return a.first < b.first || (a.first == b.first && a.second < b.second);
    });

    bool bRet = true;
    std::vector<GByte> abyPage;
    abyPage.reserve(IDX_PAGE_SIZE);

    const auto WriteRootPageNonLeaf = [=, &bRet, &asValues, &abyPage]
                (int nNumDirectChildren, int nSubPageIdxToFeatIdxMultiplier)
    {
        // Write root page (level 1)
        WriteUInt32(abyPage, 0); // id of next page at same level
        WriteUInt32(abyPage, nNumDirectChildren == 1 ? 1 : nNumDirectChildren-1);

        for( int i = 0; i < nNumDirectChildren; i++ )
        {
            WriteUInt32(abyPage, 2 + i); // id of subpage
        }

        // Add padding
        abyPage.resize(OFFSET_FIRST_VAL_IN_PAGE);

        if( nNumDirectChildren == 1 )
        {
            // Should only happen if OPENFILEGDB_FORCE_SPX_DEPTH is forced
            writeValueFunc(abyPage, asValues.back().first, maxStrSize);
        }
        else
        {
            for( int i = 0; i < nNumDirectChildren-1; i++ )
            {
                const int nFeatIdx = (i+1) * nSubPageIdxToFeatIdxMultiplier - 1;
                writeValueFunc(abyPage,asValues[nFeatIdx].first, maxStrSize);
            }
        }
        abyPage.resize(IDX_PAGE_SIZE);
        bRet &= VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp) == 1;
    };

    const auto WriteLeafPages = [=, &bRet, &asValues, &abyPage]
            (int pageBaseOffset, int nNumFeaturePages)
    {
        // Write leaf pages
        for( int i = 0; i < nNumFeaturePages; ++i )
        {
            abyPage.clear();
            int nNumFeaturesInPage = numMaxFeaturesPerPage;
            if( i + 1 < nNumFeaturePages )
            {
                WriteUInt32(abyPage, pageBaseOffset + i + 1); // id of next page at same level
            }
            else
            {
                WriteUInt32(abyPage, 0);
                nNumFeaturesInPage = static_cast<int>(asValues.size() - i * numMaxFeaturesPerPage);
            }
            CPLAssert(nNumFeaturesInPage > 0 && nNumFeaturesInPage <= NUM_MAX_FEATURES_PER_PAGE);
            WriteUInt32(abyPage, nNumFeaturesInPage);
            WriteUInt32(abyPage, 0); // unknown semantics

            // Write features' ID
            for( int j = 0; j < nNumFeaturesInPage; j++ )
            {
                WriteUInt32(abyPage, static_cast<uint32_t>(asValues[i * numMaxFeaturesPerPage + j].second));
            }

            // Add padding
            abyPage.resize(OFFSET_FIRST_VAL_IN_PAGE);

            // Write features' spatial index value
            for( int j = 0; j < nNumFeaturesInPage; j++ )
            {
                writeValueFunc(abyPage, asValues[i * numMaxFeaturesPerPage + j].first, maxStrSize);
            }

            abyPage.resize(IDX_PAGE_SIZE);
            bRet &= VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp) == 1;
        }
    };

    const auto WriteIntermediatePages = [=, &bRet, &asValues, &abyPage]
            (int pageBaseOffset, int nNumPagesThisLevel, int nNumPagesNextLevel, int nSubPageIdxToFeatIdxMultiplier)
    {
        for( int i = 0; i < nNumPagesThisLevel; ++i )
        {
            abyPage.clear();
            int nNumItemsInPage = numMaxFeaturesPerPage;
            if( i + 1 < nNumPagesThisLevel )
            {
                WriteUInt32(abyPage, pageBaseOffset + i + 1); // id of next page at same level
            }
            else
            {
                WriteUInt32(abyPage, 0);
                nNumItemsInPage = nNumPagesNextLevel - i * numMaxFeaturesPerPage;
                CPLAssert(nNumItemsInPage > 1 && nNumItemsInPage <= NUM_MAX_FEATURES_PER_PAGE + 1);
                nNumItemsInPage --;
            }
            CPLAssert(nNumItemsInPage > 0 && nNumItemsInPage <= NUM_MAX_FEATURES_PER_PAGE);
            WriteUInt32(abyPage, nNumItemsInPage);

            // Write subpages' ID
            for( int j = 0; j < 1 + nNumItemsInPage; j++ )
            {
                WriteUInt32(abyPage, pageBaseOffset + nNumPagesThisLevel + i * numMaxFeaturesPerPage + j);
            }

            // Add padding
            abyPage.resize(OFFSET_FIRST_VAL_IN_PAGE);

            // Write features' spatial index value
            for( int j = 0; j < nNumItemsInPage; j++ )
            {
                const int nFeatIdx = (i * numMaxFeaturesPerPage + j + 1) * nSubPageIdxToFeatIdxMultiplier - 1;
                writeValueFunc(abyPage, asValues[nFeatIdx].first, maxStrSize);
            }

            abyPage.resize(IDX_PAGE_SIZE);
            bRet &= VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp) == 1;
        }
    };

    const auto WriteLastTwoLevelPages =
        [numMaxFeaturesPerPage, WriteIntermediatePages, WriteLeafPages]
            (int pageBaseOffset, int nNumPagesBeforeLastLevel, int nNumFeaturePages)
    {
        // Write pages at level depth-1 (referencing pages of level depth)
        WriteIntermediatePages(pageBaseOffset, nNumPagesBeforeLastLevel, nNumFeaturePages, numMaxFeaturesPerPage);

        // Write leaf pages
        WriteLeafPages(pageBaseOffset + nNumPagesBeforeLastLevel, nNumFeaturePages);
    };

    if( asValues.empty() || nDepth == 1 ||
            (nDepth == 0 && static_cast<int>(asValues.size()) <= numMaxFeaturesPerPage) )
    {
        nDepth = 1;

        WriteUInt32(abyPage, 0); // id of next page
        WriteUInt32(abyPage, static_cast<uint32_t>(asValues.size()));
        WriteUInt32(abyPage, 0); // unknown semantics

        // Write features' ID
        for( const auto& pair: asValues )
            WriteUInt32(abyPage, static_cast<uint32_t>(pair.second));

        // Add padding
        abyPage.resize(OFFSET_FIRST_VAL_IN_PAGE);

        // Write features' spatial index value
        for( const auto& pair: asValues )
            writeValueFunc(abyPage,pair.first, maxStrSize);

        abyPage.resize(IDX_PAGE_SIZE);
        bRet &= VSIFWriteL(abyPage.data(), abyPage.size(), 1, fp) == 1;
    }
    else if( nDepth == 2 ||
                (nDepth == 0 && static_cast<int>(asValues.size()) <=
                    (numMaxFeaturesPerPage + 1) * numMaxFeaturesPerPage) )
    {
        nDepth = 2;

        const int nNumFeaturePages = static_cast<int>(DIV_ROUND_UP(
            asValues.size(), numMaxFeaturesPerPage));
        CPLAssert(nNumFeaturePages-1 <= NUM_MAX_FEATURES_PER_PAGE);

        // Write root page (level 1)
        WriteRootPageNonLeaf(nNumFeaturePages, numMaxFeaturesPerPage);

        // Write leaf pages (level 2)
        WriteLeafPages(2, nNumFeaturePages);
    }
    else if( nDepth == 3 ||
                (nDepth == 0 && static_cast<int>(asValues.size()) <=
                    ((numMaxFeaturesPerPage + 1) * numMaxFeaturesPerPage + 1) * numMaxFeaturesPerPage) )
    {
        nDepth = 3;

        // imagine simpler case: NUM_MAX_FEATURES_PER_PAGE = 2 and 9 values
        // ==> nNumFeaturePages = ceil(9 / 2) = 5
        // ==> nNumPagesLevel2 = ceil((5-1) / 2) = 2
        // level 1:
        //      page 1: point to page 2(, 3)
        // level 2:
        //      page 2: point to page 4, 5(, 6)
        //      page 3: point to page 6, 7(, 8)
        // level 3:
        //      page 4: point to feature 1, 2
        //      page 5: point to feature 3, 4
        //      page 6: point to feature 5, 6
        //      page 7: point to feature 7, 8
        //      page 8: point to feature 9

        // or NUM_MAX_FEATURES_PER_PAGE = 2 and 11 values
        // ==> nNumFeaturePages = ceil(11 / 2) = 6
        // ==> nNumPagesLevel2 = ceil((6-1) / 2) = 3
        // level 1:
        //      page 1: point to page 2, 3(, 4)
        // level 2:
        //      page 2: point to page 5, 6(, 7)
        //      page 3: point to page 7, 8(, 9)
        //      page 4: point to page 9(, 10)
        // level 3:
        //      page 5: point to feature 1, 2
        //      page 6: point to feature 3, 4
        //      page 7: point to feature 5, 6
        //      page 8: point to feature 7, 8
        //      page 9: point to feature 9, 10
        //      page 10: point to feature 11

        // or NUM_MAX_FEATURES_PER_PAGE = 2 and 14 values
        // ==> nNumFeaturePages = ceil(14 / 2) = 7
        // ==> nNumPagesLevel2 = ceil((7-1) / 2) = 3
        // level 1:
        //      page 1: point to page 2, 3(, 4)
        // level 2:
        //      page 2: point to page 5, 6(, 7)
        //      page 3: point to page 7, 8(, 9)
        //      page 4: point to page 9, 10(, 11)
        // level 3:
        //      page 5: point to feature 1, 2
        //      page 6: point to feature 3, 4
        //      page 7: point to feature 5, 6
        //      page 8: point to feature 7, 8
        //      page 9: point to feature 9, 10
        //      page 10: point to feature 11, 12
        //      page 11: point to feature 13, 14

        const int nNumFeaturePages = static_cast<int>(DIV_ROUND_UP(
            asValues.size(), numMaxFeaturesPerPage));
        const int nNumPagesLevel2 = nNumFeaturePages == 1 ? 1:
            DIV_ROUND_UP(nNumFeaturePages-1, numMaxFeaturesPerPage);
        CPLAssert(nNumPagesLevel2-1 <= NUM_MAX_FEATURES_PER_PAGE);

        // Write root page (level 1)
        WriteRootPageNonLeaf(nNumPagesLevel2, numMaxFeaturesPerPage * numMaxFeaturesPerPage);

        // Write level 2 and level 3 pages
        WriteLastTwoLevelPages(2, nNumPagesLevel2, nNumFeaturePages);
    }
    else
    {
        nDepth = 4;

        const int nNumFeaturePages = static_cast<int>(DIV_ROUND_UP(
            asValues.size(), numMaxFeaturesPerPage));
        const int nNumPagesLevel3 = nNumFeaturePages == 1 ? 1:
            DIV_ROUND_UP(nNumFeaturePages-1, numMaxFeaturesPerPage);
        const int nNumPagesLevel2 = nNumPagesLevel3 == 1 ? 1 :
            DIV_ROUND_UP(nNumPagesLevel3 - 1, numMaxFeaturesPerPage);
        CPLAssert(nNumPagesLevel2-1 <= NUM_MAX_FEATURES_PER_PAGE);

        // Write root page (level 1)
        WriteRootPageNonLeaf(nNumPagesLevel2, numMaxFeaturesPerPage * numMaxFeaturesPerPage * numMaxFeaturesPerPage);

        // Write pages at level 2 (referencing pages of level 3)
        WriteIntermediatePages(2, nNumPagesLevel2, nNumPagesLevel3, numMaxFeaturesPerPage * numMaxFeaturesPerPage);

        // Write pages at level 3 and 4
        WriteLastTwoLevelPages(2 + nNumPagesLevel2, nNumPagesLevel3, nNumFeaturePages);
    }

    // Write trailer
    std::vector<GByte> abyTrailer;
    CPLAssert(SIZEOF_INDEXED_VALUE <= 255);
    WriteUInt8(abyTrailer, static_cast<uint8_t>(SIZEOF_INDEXED_VALUE));
    WriteUInt8(abyTrailer, maxStrSize ? 0x20 : 0x40); // unknown semantics
    WriteUInt32(abyTrailer, 1); // unknown semantics
    WriteUInt32(abyTrailer, nDepth); // index depth
    WriteUInt32(abyTrailer, static_cast<uint32_t>(asValues.size()));
    WriteUInt32(abyTrailer, 0); // unknown semantics
    WriteUInt32(abyTrailer, 1); // unknown semantics
    bRet &= VSIFWriteL(abyTrailer.data(), abyTrailer.size(), 1, fp) == 1;

    return bRet;
}

/************************************************************************/
/*                        CreateSpatialIndex()                          */
/************************************************************************/

bool FileGDBTable::CreateSpatialIndex()
{
    if( m_iGeomField < 0 ||
        m_adfSpatialIndexGridResolution.empty() ||
        m_adfSpatialIndexGridResolution.size() > 3 )
    {
        return false;
    }

    if( m_eTableGeomType == FGTGT_MULTIPATCH )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Multipatch not supported for spatial index generation");
        return false;
    }

    auto poGeomField = cpl::down_cast<FileGDBGeomField*>(m_apoFields[m_iGeomField].get());
    if( m_adfSpatialIndexGridResolution.size() == 1 )
    {
        // Debug only
        const char* pszGridSize = CPLGetConfigOption("OPENFILEGDB_GRID_SIZE", nullptr);
        if( pszGridSize )
        {
            m_bDirtyGeomFieldSpatialIndexGridRes = true;
            m_adfSpatialIndexGridResolution[0] = CPLAtof(pszGridSize);
            poGeomField->m_adfSpatialIndexGridResolution = m_adfSpatialIndexGridResolution;
        }
        else
        {
            ComputeOptimalSpatialIndexGridResolution();
            if( m_adfSpatialIndexGridResolution[0] == 0 )
                return false;
        }
    }
    auto poGeomConverter = std::unique_ptr<FileGDBOGRGeometryConverter>(
        FileGDBOGRGeometryConverter::BuildConverter(poGeomField));
    typedef std::pair<int64_t, int> ValueOIDPair;
    std::vector<ValueOIDPair> asValues;

    const double dfGridStep = m_adfSpatialIndexGridResolution.back();
    const double dfShift = (1 << 29) / (dfGridStep / m_adfSpatialIndexGridResolution[0]);

    double dfYMinClamped;
    double dfYMaxClamped;
    GetMinMaxProjYForSpatialIndex(dfYMinClamped, dfYMaxClamped);

    const auto AddPointToIndex = [this, dfGridStep, dfShift, dfYMinClamped, dfYMaxClamped](
                                        double dfX, double dfY,
                                        std::vector<int64_t>& aSetValues)
    {
        dfY = std::min(std::max(dfY, dfYMinClamped), dfYMaxClamped);

        const double dfXShifted = dfX / dfGridStep + dfShift;
        const double dfYShifted = dfY / dfGridStep + dfShift;
        // Each value must fit on 31 bit (sign included)
        if( std::abs(dfXShifted) < (1 << 30) && std::abs(dfYShifted) < (1 << 30) )
        {
            const unsigned nX = static_cast<unsigned>(static_cast<int>(std::floor(dfXShifted)));
            const unsigned nY = static_cast<unsigned>(static_cast<int>(std::floor(dfYShifted)));
            const uint64_t nVal =
                ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                ((static_cast<uint64_t>(nX)) << 31) |
                nY;
            aSetValues.push_back(static_cast<int64_t>(nVal));
            return true;
        }
        return false;
    };

    // Adapted from GDALdllImageLineAllTouched() of alg/llrasterize.cpp
    const auto AddLineStringToIndex = [this, dfGridStep, dfShift, dfYMinClamped, dfYMaxClamped](
        const OGRLineString* poLS, std::vector<int64_t>& aSetValues)
    {
        const int nNumPoints = poLS->getNumPoints();
        if( nNumPoints < 2 )
            return;
        OGREnvelope sEnvelope;
        poLS->getEnvelope(&sEnvelope);
        double dfYShift = 0;
        if( sEnvelope.MaxY > dfYMaxClamped )
            dfYShift = dfYMaxClamped - sEnvelope.MaxY;
        else if( sEnvelope.MinY < dfYMinClamped )
            dfYShift = dfYMinClamped - sEnvelope.MinY;
        for( int i = 0; i < nNumPoints - 1; i++ )
        {
            double dfX = poLS->getX(i) / dfGridStep + dfShift;
            double dfY = (poLS->getY(i) + dfYShift) / dfGridStep + dfShift;
            double dfXEnd = poLS->getX(i+1) / dfGridStep + dfShift;
            double dfYEnd = (poLS->getY(i+1) + dfYShift) / dfGridStep + dfShift;
            if( !(std::abs(dfX) < (1 << 30) && std::abs(dfY) < (1 << 30) &&
                  std::abs(dfXEnd) < (1 << 30) && std::abs(dfYEnd) < (1 << 30)) )
            {
                return;
            }

            // Swap if needed so we can proceed from left2right (X increasing)
            if( dfX > dfXEnd )
            {
                std::swap(dfX, dfXEnd);
                std::swap(dfY, dfYEnd );
            }

            // Special case for vertical lines.
            if( floor(dfX) == floor(dfXEnd) || fabs(dfX - dfXEnd) < .01 )
            {
                if( dfYEnd < dfY )
                {
                    std::swap(dfY, dfYEnd );
                }

                const int iX = static_cast<int>(floor(dfXEnd));
                int iY = static_cast<int>(floor(dfY));
                int iYEnd = static_cast<int>(floor(dfYEnd));

                for( ; iY <= iYEnd; iY++ )
                {
                    const unsigned nX = static_cast<unsigned>(iX);
                    const unsigned nY = static_cast<unsigned>(iY);
                    const uint64_t nVal =
                        ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                        ((static_cast<uint64_t>(nX)) << 31) |
                        nY;
                    aSetValues.push_back(static_cast<int64_t>(nVal));
                }

                continue;  // Next segment.
            }

            // Special case for horizontal lines.
            if( floor(dfY) == floor(dfYEnd) || fabs(dfY - dfYEnd) < .01 )
            {
                if( dfXEnd < dfX )
                {
                    std::swap(dfX, dfXEnd);
                }

                int iX = static_cast<int>(floor(dfX));
                const int iY = static_cast<int>(floor(dfY));
                int iXEnd = static_cast<int>(floor(dfXEnd));

                for( ; iX <= iXEnd; iX++ )
                {
                    const unsigned nX = static_cast<unsigned>(iX);
                    const unsigned nY = static_cast<unsigned>(iY);
                    const uint64_t nVal =
                        ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                        ((static_cast<uint64_t>(nX)) << 31) |
                        nY;
                    aSetValues.push_back(static_cast<int64_t>(nVal));
                }

                continue;  // Next segment.
            }

/* -------------------------------------------------------------------- */
/*      General case - left to right sloped.                            */
/* -------------------------------------------------------------------- */

            // Recenter coordinates to avoid numeric precision issues
            // particularly the tests against a small epsilon below that could
            // lead to infinite looping otherwise.
            const int nXShift = static_cast<int>(floor(dfX));
            const int nYShift = static_cast<int>(floor(dfY));
            dfX -= nXShift;
            dfY -= nYShift;
            dfXEnd -= nXShift;
            dfYEnd -= nYShift;

            const double dfSlope = (dfYEnd - dfY) / (dfXEnd - dfX);

            // Step from pixel to pixel.
            while( dfX < dfXEnd )
            {
                const int iX = static_cast<int>(floor(dfX));
                const int iY = static_cast<int>(floor(dfY));

                // Burn in the current point.
                const unsigned nX = static_cast<unsigned>(iX + nXShift);
                const unsigned nY = static_cast<unsigned>(iY + nYShift);
                const uint64_t nVal =
                    ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                    ((static_cast<uint64_t>(nX)) << 31) |
                    nY;
                aSetValues.push_back(static_cast<int64_t>(nVal));

                double dfStepX = floor(dfX+1.0) - dfX;
                double dfStepY = dfStepX * dfSlope;

                // Step to right pixel without changing scanline?
                if( static_cast<int>(floor(dfY + dfStepY)) == iY )
                {
                    dfX += dfStepX;
                    dfY += dfStepY;
                }
                else if( dfSlope < 0 )
                {
                    dfStepY = iY - dfY;
                    if( dfStepY > -0.000000001 )
                        dfStepY = -0.000000001;

                    dfStepX = dfStepY / dfSlope;
                    dfX += dfStepX;
                    dfY += dfStepY;
                }
                else
                {
                    dfStepY = (iY + 1) - dfY;
                    if( dfStepY < 0.000000001 )
                        dfStepY = 0.000000001;

                    dfStepX = dfStepY / dfSlope;
                    dfX += dfStepX;
                    dfY += dfStepY;
                }
            }  // Next step along segment.
        }
    };

    // Adapted from GDALdllImageFilledPolygon() of alg/llrasterize.cpp
    const auto AddPolygonToIndex = [this, dfGridStep, dfShift, dfYMinClamped, dfYMaxClamped, AddLineStringToIndex](
        const OGRPolygon* poPoly, std::vector<int64_t>& aSetValues)
    {
        if( poPoly->IsEmpty() )
            return;

        // Burn contour of exterior ring, because burning the interior
        // can often result in nothing
        AddLineStringToIndex(poPoly->getExteriorRing(), aSetValues);

        OGREnvelope sEnvelope;
        poPoly->getEnvelope(&sEnvelope);

        double dfYShift = 0;
        if( sEnvelope.MaxY > dfYMaxClamped )
            dfYShift = dfYMaxClamped - sEnvelope.MaxY;
        else if( sEnvelope.MinY < dfYMinClamped )
            dfYShift = dfYMinClamped - sEnvelope.MinY;

        const int miny = static_cast<int>(floor((sEnvelope.MinY + dfYShift)/ dfGridStep + dfShift));
        const int maxy = static_cast<int>(floor((sEnvelope.MaxY + dfYShift)/ dfGridStep + dfShift));
        std::vector<double> intersections;

        // Burn interior of polygon
        for( int iY = miny; iY <= maxy; iY++ )
        {
            const double dy = iY + 0.5;
            intersections.clear();
            for( const auto* poRing: *poPoly )
            {
                const int nNumPoints = poRing->getNumPoints();
                for( int i = 0; i < nNumPoints - 1; ++i )
                {
                    double dy1 = (poRing->getY(i) + dfYShift) / dfGridStep + dfShift;
                    double dy2 = (poRing->getY(i+1) + dfYShift) / dfGridStep + dfShift;
                    if( (dy1 < dy && dy2 < dy) ||
                        (dy1 > dy && dy2 > dy) )
                        continue;

                    double dx1 = 0.0;
                    double dx2 = 0.0;
                    if( dy1 < dy2 )
                    {
                        dx1 = poRing->getX(i) / dfGridStep + dfShift;
                        dx2 = poRing->getX(i+1) / dfGridStep + dfShift;
                    }
                    else if( dy1 > dy2 )
                    {
                        std::swap(dy1, dy2);
                        dx2 = poRing->getX(i) / dfGridStep + dfShift;
                        dx1 = poRing->getX(i+1) / dfGridStep + dfShift;
                    }
                    else // if( fabs(dy1-dy2) < 1.e-6 )
                    {
                        const int iX1 =
                            static_cast<int>(floor(std::min(poRing->getX(i), poRing->getX(i+1)) / dfGridStep + dfShift));
                        const int iX2 =
                            static_cast<int>(floor(std::max(poRing->getX(i), poRing->getX(i+1)) / dfGridStep + dfShift));

                        // Fill the horizontal segment (separately from the rest).
                        for( int iX = iX1; iX <= iX2; ++iX )
                        {
                            const unsigned nX = static_cast<unsigned>(iX);
                            const unsigned nY = static_cast<unsigned>(iY);
                            const uint64_t nVal =
                                ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                                ((static_cast<uint64_t>(nX)) << 31) |
                                nY;
                            aSetValues.push_back(static_cast<int64_t>(nVal));
                        }
                        continue;
                    }

                    if( dy < dy2 && dy >= dy1 )
                    {
                        const double intersect = (dy-dy1) * (dx2-dx1) / (dy2-dy1) + dx1;
                        intersections.emplace_back(intersect);
                    }
                }
            }

            std::sort(intersections.begin(), intersections.end());

            for( size_t i = 0; i + 1 < intersections.size(); i += 2 )
            {
                const int iX1 =
                    static_cast<int>(floor(intersections[i]));
                const int iX2 =
                    static_cast<int>(floor(intersections[i+1]));

                for( int iX = iX1; iX <= iX2; ++iX )
                {
                    const unsigned nX = static_cast<unsigned>(iX);
                    const unsigned nY = static_cast<unsigned>(iY);
                    const uint64_t nVal =
                        ((static_cast<uint64_t>(m_adfSpatialIndexGridResolution.size() - 1)) << 62 ) |
                        ((static_cast<uint64_t>(nX)) << 31) |
                        nY;
                    aSetValues.push_back(static_cast<int64_t>(nVal));
                }
            }
        }
    };

    std::vector<int64_t> aSetValues;
    int iLastReported = 0;
    const auto nReportIncrement = m_nTotalRecordCount / 20;
    try
    {
        for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
        {
            if( m_nTotalRecordCount > 10000 &&
                (iCurFeat + 1 == m_nTotalRecordCount ||
                 iCurFeat - iLastReported >= nReportIncrement) )
            {
                CPLDebug("OpenFileGDB", "Spatial index building: %02.2f %%",
                         100 * double(iCurFeat + 1) / double(m_nTotalRecordCount));
                iLastReported = iCurFeat;
            }
            iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const OGRField* psField = GetFieldValue(m_iGeomField);
            if( psField != nullptr )
            {
                auto poGeom = std::unique_ptr<OGRGeometry>(poGeomConverter->GetAsGeometry(psField));
                if( poGeom != nullptr )
                {
                    aSetValues.clear();
                    const auto eGeomType = wkbFlatten(poGeom->getGeometryType());
                    if( eGeomType == wkbPoint )
                    {
                        const auto poPoint = poGeom->toPoint();
                        AddPointToIndex(poPoint->getX(), poPoint->getY(), aSetValues);
                    }
                    else if( eGeomType == wkbMultiPoint )
                    {
                        for( const auto poPoint: *(poGeom->toMultiPoint()) )
                        {
                            AddPointToIndex(poPoint->getX(), poPoint->getY(), aSetValues);
                        }
                    }
                    else if( eGeomType == wkbLineString )
                    {
                        AddLineStringToIndex(poGeom->toLineString(), aSetValues);
                    }
                    else if( eGeomType == wkbMultiLineString )
                    {
                        for( const auto poLS: *(poGeom->toMultiLineString()) )
                        {
                            AddLineStringToIndex(poLS, aSetValues);
                        }
                    }
                    else if( eGeomType == wkbCircularString ||
                             eGeomType == wkbCompoundCurve )
                    {
                        poGeom.reset(poGeom->getLinearGeometry());
                        if( poGeom )
                            AddLineStringToIndex(poGeom->toLineString(), aSetValues);
                    }
                    else if( eGeomType == wkbMultiCurve )
                    {
                        poGeom.reset(poGeom->getLinearGeometry());
                        if( poGeom )
                        {
                            for( const auto poLS: *(poGeom->toMultiLineString()) )
                            {
                                AddLineStringToIndex(poLS, aSetValues);
                            }
                        }
                    }
                    else if( eGeomType == wkbPolygon )
                    {
                        AddPolygonToIndex(poGeom->toPolygon(), aSetValues);
                    }
                    else if( eGeomType == wkbCurvePolygon )
                    {
                        poGeom.reset(poGeom->getLinearGeometry());
                        if( poGeom )
                            AddPolygonToIndex(poGeom->toPolygon(), aSetValues);
                    }
                    else if( eGeomType == wkbMultiPolygon )
                    {
                        for( const auto poPoly: *(poGeom->toMultiPolygon()) )
                        {
                            AddPolygonToIndex(poPoly, aSetValues);
                        }
                    }
                    else if( eGeomType == wkbMultiSurface )
                    {
                        poGeom.reset(poGeom->getLinearGeometry());
                        if( poGeom )
                        {
                            for( const auto poPoly: *(poGeom->toMultiPolygon()) )
                            {
                                AddPolygonToIndex(poPoly, aSetValues);
                            }
                        }
                    }

                    std::sort(aSetValues.begin(), aSetValues.end());

                    int64_t nLastVal = std::numeric_limits<int64_t>::min();
                    for( auto nVal: aSetValues )
                    {
                        if( nVal != nLastVal )
                        {
                            asValues.push_back(ValueOIDPair(nVal, iCurFeat+1));
                            nLastVal = nVal;
                        }
                    }
                }
            }
        }
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    const std::string osSPXFilename( CPLResetExtension(m_osFilename.c_str(), "spx") );
    VSILFILE* fp = VSIFOpenL( osSPXFilename.c_str(), "wb" );
    if( fp == nullptr )
        return false;

    // Configurable only for debugging purposes
    int nDepth = atoi(CPLGetConfigOption("OPENFILEGDB_FORCE_SPX_DEPTH", "0"));

    const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                   const typename ValueOIDPair::first_type& nval,
                                   int /* maxStrSize */)
    {
        WriteUInt64(abyPage, static_cast<uint64_t>(nval));
    };

    bool bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth);

    CPLDebug("OpenFileGDB", "Spatial index of depth %d", nDepth);

    VSIFCloseL(fp);

    if( !bRet )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Write error during .spx generation");
        VSIUnlink(osSPXFilename.c_str());
    }

    return bRet;
}

/************************************************************************/
/*                      CreateAttributeIndex()                          */
/************************************************************************/

bool FileGDBTable::CreateAttributeIndex(const FileGDBIndex* poIndex)
{
    const std::string osFieldName = poIndex->GetFieldName();
    const int iField = GetFieldIdx(osFieldName);
    if( iField < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find field %s", osFieldName.c_str());
        return false;
    }

    const std::string osIdxFilename(
        CPLResetExtension(m_osFilename.c_str(),
                          (poIndex->GetIndexName() + ".atx").c_str()) );
    VSILFILE* fp = VSIFOpenL( osIdxFilename.c_str(), "wb" );
    if( fp == nullptr )
        return false;

    bool bRet;
    int nDepth = 0;

    try
    {
        const auto eFieldType = m_apoFields[iField]->GetType();
        if( eFieldType == FGFT_INT16 )
        {
            typedef std::pair<int16_t, int> ValueOIDPair;
            std::vector<ValueOIDPair> asValues;
            for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
            {
                iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const OGRField* psField = GetFieldValue(iField);
                if( psField != nullptr )
                {
                    asValues.push_back(ValueOIDPair(static_cast<int16_t>(psField->Integer), iCurFeat+1));
                }
            }

            const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                           const typename ValueOIDPair::first_type& val,
                                           int /* maxStrSize */)
            {
                WriteUInt16(abyPage, static_cast<uint16_t>(val));
            };

            bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth);
        }
        else if( eFieldType == FGFT_INT32 )
        {
            typedef std::pair<int32_t, int> ValueOIDPair;
            std::vector<ValueOIDPair> asValues;
            for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
            {
                iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const OGRField* psField = GetFieldValue(iField);
                if( psField != nullptr )
                {
                    asValues.push_back(ValueOIDPair(psField->Integer, iCurFeat+1));
                }
            }

            const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                           const typename ValueOIDPair::first_type& val,
                                           int /* maxStrSize */)
            {
                WriteUInt32(abyPage, static_cast<uint32_t>(val));
            };

            bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth);
        }
        else if( eFieldType == FGFT_FLOAT32 )
        {
            typedef std::pair<float, int> ValueOIDPair;
            std::vector<ValueOIDPair> asValues;
            for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
            {
                iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const OGRField* psField = GetFieldValue(iField);
                if( psField != nullptr )
                {
                    asValues.push_back(ValueOIDPair(static_cast<float>(psField->Real), iCurFeat+1));
                }
            }

            const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                           const typename ValueOIDPair::first_type& val,
                                           int /* maxStrSize */)
            {
                WriteFloat32(abyPage, val);
            };

            bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth);
        }
        else if( eFieldType == FGFT_FLOAT64 ||
                 eFieldType == FGFT_DATETIME )
        {
            typedef std::pair<double, int> ValueOIDPair;
            std::vector<ValueOIDPair> asValues;
            m_apoFields[iField]->m_eType = FGFT_FLOAT64; // Hack to force reading DateTime as double
            for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
            {
                iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const OGRField* psField = GetFieldValue(iField);
                if( psField != nullptr )
                {
                    asValues.push_back(ValueOIDPair(psField->Real, iCurFeat+1));
                }
            }
            m_apoFields[iField]->m_eType = eFieldType;

            const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                           const typename ValueOIDPair::first_type& val,
                                           int /* maxStrSize */)
            {
                WriteFloat64(abyPage, val);
            };

            bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth);
        }
        else if( eFieldType == FGFT_STRING )
        {
            typedef std::pair<std::vector<std::uint16_t>, int> ValueOIDPair;
            std::vector<ValueOIDPair> asValues;
            bRet = true;
            const bool bIsLower = STARTS_WITH_CI(poIndex->GetExpression().c_str(), "LOWER(");
            int maxStrSize = 0;
            for( int iCurFeat = 0; iCurFeat < m_nTotalRecordCount; ++iCurFeat )
            {
                iCurFeat = GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const OGRField* psField = GetFieldValue(iField);
                if( psField != nullptr )
                {
                    wchar_t *pWide = CPLRecodeToWChar( psField->String,
                                                    CPL_ENC_UTF8,
                                                    CPL_ENC_UCS2 );
                    if(pWide == nullptr)
                    {
                        bRet = false;
                        break;
                    }
                    int nCount = 0;
                    std::vector<std::uint16_t> asUTF16Str;
                    while( nCount < MAX_CAR_COUNT_INDEXED_STR && pWide[nCount] != 0 )
                    {
                        nCount++;
                    }
                    if( nCount > maxStrSize )
                        maxStrSize = nCount;
                    asUTF16Str.reserve(nCount);
                    for(int i = 0; i < nCount; ++i )
                    {
                        if( bIsLower && pWide[i] >= 'A' && pWide[i] <= 'Z' )
                            asUTF16Str.push_back(static_cast<uint16_t>(pWide[i] + ('a' - 'A')));
                        else
                            asUTF16Str.push_back(static_cast<uint16_t>(pWide[i]));
                    }
                    CPLFree(pWide);

                    asValues.push_back(ValueOIDPair(asUTF16Str, iCurFeat+1));
                }
            }
            if( maxStrSize < MAX_CAR_COUNT_INDEXED_STR )
                maxStrSize ++;

            const auto writeValueFunc = [](std::vector<GByte>& abyPage,
                                           const typename ValueOIDPair::first_type& val,
                                           int l_maxStrSize)
            {
                for( size_t i = 0; i < val.size(); ++i )
                    WriteUInt16(abyPage, val[i]);
                for( size_t i = val.size(); i < static_cast<size_t>(l_maxStrSize); ++i )
                    WriteUInt16(abyPage, 32); // space
            };

            if( bRet )
            {
                bRet = WriteIndex(fp, asValues, writeValueFunc, nDepth, maxStrSize);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateAttributeIndex(%s): "
                     "Unsupported field type for index creation",
                     osFieldName.c_str());
            bRet = false;
        }
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        bRet = false;
    }

    VSIFCloseL(fp);

    if( !bRet )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Write error during %s generation", osIdxFilename.c_str());
        VSIUnlink(osIdxFilename.c_str());
    }

    return bRet;
}

} /* namespace OpenFileGDB */

