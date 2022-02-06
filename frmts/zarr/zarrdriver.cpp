/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "zarr.h"

#include "cpl_minixml.h"

#include <algorithm>
#include <cassert>
#include <limits>

#ifdef HAVE_BLOSC
#include <blosc.h>
#endif

/************************************************************************/
/*                            ZarrDataset()                             */
/************************************************************************/

ZarrDataset::ZarrDataset(const std::shared_ptr<GDALGroup>& poRootGroup):
    m_poRootGroup(poRootGroup)
{
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ZarrDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( STARTS_WITH(poOpenInfo->pszFilename, "ZARR:") )
    {
        return TRUE;
    }

    if( !poOpenInfo->bIsDirectory )
    {
        return FALSE;
    }

    CPLString osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                              ".zarray", nullptr );

    VSIStatBufL sStat;
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                    ".zgroup", nullptr );
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    // Zarr V3
    osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                    "zarr.json", nullptr );
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           OpenMultidim()                             */
/************************************************************************/

GDALDataset* ZarrDataset::OpenMultidim(const char* pszFilename,
                                       bool bUpdateMode,
                                       CSLConstList papszOpenOptions)
{
    CPLString osFilename(pszFilename);
    if( osFilename.back() == '/' )
        osFilename.resize(osFilename.size() - 1);

    auto poSharedResource = std::make_shared<ZarrSharedResource>(osFilename);
    poSharedResource->SetOpenOptions(papszOpenOptions);

    auto poRG = ZarrGroupV2::Create(poSharedResource, std::string(), "/");
    poRG->SetUpdatable(bUpdateMode);
    poRG->SetDirectoryName(osFilename);

    const std::string osZarrayFilename(
                CPLFormFilename(pszFilename, ".zarray", nullptr));
    VSIStatBufL sStat;
    if( VSIStatL( osZarrayFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osZarrayFilename) )
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        if( oRoot["_NCZARR_ARRAY"].IsValid() )
        {
            // If opening a NCZarr array, initialize its group from NCZarr
            // metadata.
            const std::string osGroupFilename(
                    CPLFormFilename(CPLGetDirname(osFilename.c_str()), ".zgroup", nullptr));
            if( VSIStatL( osGroupFilename.c_str(), &sStat ) == 0 )
            {
                CPLJSONDocument oDocGroup;
                if( oDocGroup.Load(osGroupFilename) )
                {
                    if( !poRG->InitFromZGroup(oDocGroup.GetRoot()) )
                        return nullptr;
                }
            }
        }
        const std::string osArrayName(CPLGetBasename(osFilename.c_str()));
        std::set<std::string> oSetFilenamesInLoading;
        if( !poRG->LoadArray(osArrayName, osZarrayFilename, oRoot,
                             false, CPLJSONObject(), oSetFilenamesInLoading) )
            return nullptr;

        return new ZarrDataset(poRG);
    }

    const std::string osZmetadataFilename(
            CPLFormFilename(pszFilename, ".zmetadata", nullptr));
    if( CPLTestBool(CSLFetchNameValueDef(
                papszOpenOptions, "USE_ZMETADATA", "YES")) &&
        VSIStatL( osZmetadataFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osZmetadataFilename) )
            return nullptr;

        poRG->InitFromZMetadata(oDoc.GetRoot());
        poSharedResource->EnableZMetadata();
        poSharedResource->InitFromZMetadata(oDoc.GetRoot());

        return new ZarrDataset(poRG);
    }

    const std::string osGroupFilename(
            CPLFormFilename(pszFilename, ".zgroup", nullptr));
    if( VSIStatL( osGroupFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osGroupFilename) )
            return nullptr;

        if( !poRG->InitFromZGroup(oDoc.GetRoot()) )
            return nullptr;
        return new ZarrDataset(poRG);
    }

    // Zarr v3
    auto poRG_V3 = ZarrGroupV3::Create(poSharedResource,
                                       std::string(), "/", osFilename);
    poRG_V3->SetUpdatable(bUpdateMode);
    return new ZarrDataset(poRG_V3);
}

/************************************************************************/
/*                            ExploreGroup()                            */
/************************************************************************/

static bool ExploreGroup(const std::shared_ptr<GDALGroup>& poGroup,
                         std::vector<std::string>& aosArrays,
                         int nRecCount)
{
    if( nRecCount == 32 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too deep recursion level in ExploreGroup()");
        return false;
    }
    const auto aosGroupArrayNames = poGroup->GetMDArrayNames();
    for( const auto& osArrayName: aosGroupArrayNames )
    {
        std::string osArrayFullname = poGroup->GetFullName();
        if( osArrayName != "/" )
        {
            if( osArrayFullname != "/" )
                osArrayFullname += '/';
            osArrayFullname += osArrayName;
        }
        aosArrays.emplace_back(std::move(osArrayFullname));
        if( aosArrays.size() == 10000 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                 "Too many arrays found by ExploreGroup()");
            return false;
        }
    }

    const auto aosSubGroups = poGroup->GetGroupNames();
    for( const auto& osSubGroup: aosSubGroups )
    {
        const auto poSubGroup = poGroup->OpenGroup(osSubGroup);
        if( poSubGroup )
        {
            if( !ExploreGroup(poSubGroup, aosArrays, nRecCount + 1) )
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char* ZarrDataset::GetMetadataItem(const char* pszName, const char* pszDomain)
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS") )
        return m_aosSubdatasets.FetchNameValue(pszName);
    return nullptr;
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char** ZarrDataset::GetMetadata(const char* pszDomain)
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS") )
        return m_aosSubdatasets.List();
    return nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* ZarrDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
    {
        return nullptr;
    }
    if( poOpenInfo->eAccess == GA_Update &&
        (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Update not supported");
        return nullptr;
    }

    CPLString osFilename(poOpenInfo->pszFilename);
    CPLString osArrayOfInterest;
    std::vector<uint64_t> anExtraDimIndices;
    if( STARTS_WITH(poOpenInfo->pszFilename, "ZARR:") )
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2( poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS ));
        if( aosTokens.size() < 2 )
            return nullptr;
        osFilename = aosTokens[1];
        if( aosTokens.size() >= 3 )
        {
            osArrayOfInterest = aosTokens[2];
            for( int i = 3; i < aosTokens.size(); ++i )
            {
                anExtraDimIndices.push_back(
                    static_cast<uint64_t>(CPLAtoGIntBig(aosTokens[i])));
            }
        }
    }

    auto poDSMultiDim = std::unique_ptr<GDALDataset>(
        OpenMultidim(osFilename.c_str(), poOpenInfo->eAccess == GA_Update,
                     poOpenInfo->papszOpenOptions));
    if( poDSMultiDim == nullptr ||
        (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0 )
    {
        return poDSMultiDim.release();
    }

    auto poRG = poDSMultiDim->GetRootGroup();

    auto poDS = cpl::make_unique<ZarrDataset>(nullptr);
    std::shared_ptr<GDALMDArray> poMainArray;
    if( !osArrayOfInterest.empty() )
    {
        poMainArray = osArrayOfInterest == "/" ? poRG->OpenMDArray("/") :
                            poRG->OpenMDArrayFromFullname(osArrayOfInterest);
        if( poMainArray == nullptr )
            return nullptr;
        if( poMainArray->GetDimensionCount() > 2 )
        {
            if ( anExtraDimIndices.empty() )
            {
                uint64_t nExtraDimSamples = 1;
                const auto& apoDims = poMainArray->GetDimensions();
                for( size_t i = 2; i < apoDims.size(); ++i )
                    nExtraDimSamples *= apoDims[i]->GetSize();
                if( nExtraDimSamples != 1 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Indices of extra dimensions must be specified");
                    return nullptr;
                }
            }
            else if( anExtraDimIndices.size() != poMainArray->GetDimensionCount() - 2 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong number of indices of extra dimensions");
                return nullptr;
            }
            else
            {
                for( const auto idx: anExtraDimIndices )
                {
                    poMainArray = poMainArray->at(idx);
                    if( poMainArray == nullptr )
                        return nullptr;
                }
            }
        }
        else if( !anExtraDimIndices.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected extra indices");
            return nullptr;
        }
    }
    else
    {
        std::vector<std::string> aosArrays;
        ExploreGroup(poRG, aosArrays, 0);
        if( aosArrays.empty() )
            return nullptr;

        std::string osMainArray;
        if( aosArrays.size() == 1 )
        {
            poMainArray = poRG->OpenMDArrayFromFullname(aosArrays[0]);
            if( poMainArray )
                osMainArray = poMainArray->GetFullName();
        }
        else // at least 2 arrays
        {
            for( const auto& osArrayName: aosArrays )
            {
                auto poArray = poRG->OpenMDArrayFromFullname(osArrayName);
                if( poArray && poArray->GetDimensionCount() >= 2 )
                {
                    if( osMainArray.empty() )
                    {
                        poMainArray = poArray;
                        osMainArray = osArrayName;
                    }
                    else
                    {
                        poMainArray.reset();
                        osMainArray.clear();
                        break;
                    }
                }
            }
        }

        int iCountSubDS = 1;

        if( poMainArray && poMainArray->GetDimensionCount() > 2 )
        {
            uint64_t nExtraDimSamples = 1;
            const auto& apoDims = poMainArray->GetDimensions();
            for( size_t i = 0; i < apoDims.size() - 2; ++i )
                nExtraDimSamples *= apoDims[i]->GetSize();
            if( nExtraDimSamples > 1024 ) // arbitrary limit
            {
                if( apoDims.size() == 3 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Too many samples along the > 2D dimensions of %s. "
                             "Use ZARR:\"%s\":%s:{i} syntax",
                             osMainArray.c_str(),
                             osFilename.c_str(), osMainArray.c_str());
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Too many samples along the > 2D dimensions of %s. "
                             "Use ZARR:\"%s\":%s:{i}:{j} syntax",
                             osMainArray.c_str(),
                             osFilename.c_str(), osMainArray.c_str());
                }
            }
            else if( nExtraDimSamples > 1 && apoDims.size() == 3 )
            {
                for( int i = 0; i < static_cast<int>(nExtraDimSamples); ++i )
                {
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s:%d",
                                   iCountSubDS, osFilename.c_str(),
                                   osMainArray.c_str(), i));
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_DESC=Array %s at index %d of %s",
                                   iCountSubDS, osMainArray.c_str(),
                                   i, apoDims[0]->GetName().c_str()));
                    ++iCountSubDS;
                }
            }
            else if( nExtraDimSamples > 1 )
            {
                int nDimIdxI = 0;
                int nDimIdxJ = 0;
                for( int i = 0; i < static_cast<int>(nExtraDimSamples); ++i )
                {
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s:%d:%d",
                                   iCountSubDS, osFilename.c_str(),
                                   osMainArray.c_str(), nDimIdxI, nDimIdxJ));
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_DESC=Array %s at "
                                   "index %d of %s and %d of %s",
                                   iCountSubDS, osMainArray.c_str(),
                                   nDimIdxI, apoDims[0]->GetName().c_str(),
                                   nDimIdxJ, apoDims[1]->GetName().c_str()));
                    ++iCountSubDS;
                    ++nDimIdxJ;
                    if( nDimIdxJ == static_cast<int>(apoDims[1]->GetSize()) )
                    {
                        nDimIdxJ = 0;
                        ++nDimIdxI;
                    }
                }
            }
        }

        if( aosArrays.size() >= 2 )
        {
            for( size_t i = 0; i < aosArrays.size(); ++i )
            {
                if( aosArrays[i] != osMainArray )
                {
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s",
                                   iCountSubDS, osFilename.c_str(),
                                   aosArrays[i].c_str()));
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_DESC=Array %s",
                                   iCountSubDS, aosArrays[i].c_str()));
                    ++iCountSubDS;
                }
            }
        }
    }

    if( poMainArray && poMainArray->GetDimensionCount() <= 2 )
    {
        std::unique_ptr<GDALDataset> poNewDS;
        if( poMainArray->GetDimensionCount() == 2 )
            poNewDS.reset(poMainArray->AsClassicDataset(1, 0));
        else
            poNewDS.reset(poMainArray->AsClassicDataset(0, 0));
        if( !poNewDS )
            return nullptr;
        poNewDS->SetMetadata(poDS->m_aosSubdatasets.List(), "SUBDATASETS");
        return poNewDS.release();
    }

    return poDS.release();
}

/************************************************************************/
/*                           ZarrDriver()                               */
/************************************************************************/

class ZarrDriver final: public GDALDriver
{
    bool m_bMetadataInitialized = false;
    void InitMetadata();

public:
    const char* GetMetadataItem(const char* pszName, const char* pszDomain) override
    {
        if( EQUAL(pszName, "COMPRESSORS") ||
            EQUAL(pszName, "BLOSC_COMPRESSORS") ||
            EQUAL(pszName, GDAL_DMD_CREATIONOPTIONLIST) ||
            EQUAL(pszName, GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST) )
        {
            InitMetadata();
        }
        return GDALDriver::GetMetadataItem(pszName, pszDomain);
    }

    char** GetMetadata(const char* pszDomain) override
    {
        InitMetadata();
        return GDALDriver::GetMetadata(pszDomain);
    }
};

void ZarrDriver::InitMetadata()
{
    if( m_bMetadataInitialized )
        return;
    m_bMetadataInitialized = true;

    {
        // A bit of a hack. Normally GetMetadata() should also return it,
        // but as this is only used for tests, just make GetMetadataItem()
        // handle it
        std::string osCompressors;
        std::string osFilters;
        char** decompressors = CPLGetDecompressors();
        for( auto iter = decompressors; iter && *iter; ++iter )
        {
            const auto psCompressor = CPLGetCompressor(*iter);
            if( psCompressor )
            {
                if( psCompressor->eType == CCT_COMPRESSOR )
                {
                    if( !osCompressors.empty() )
                        osCompressors += ',';
                    osCompressors += *iter;
                }
                else if( psCompressor->eType == CCT_FILTER )
                {
                    if( !osFilters.empty() )
                        osFilters += ',';
                    osFilters += *iter;
                }
            }
        }
        CSLDestroy(decompressors);
        GDALDriver::SetMetadataItem("COMPRESSORS", osCompressors.c_str());
        GDALDriver::SetMetadataItem("FILTERS", osFilters.c_str());
    }
#ifdef HAVE_BLOSC
    {
        GDALDriver::SetMetadataItem("BLOSC_COMPRESSORS",
                                    blosc_list_compressors());
    }
#endif

    {
        CPLXMLTreeCloser oTree(CPLCreateXMLNode(
                            nullptr, CXT_Element, "CreationOptionList"));
        char** compressors = CPLGetCompressors();

        auto psCompressNode = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psCompressNode, "name", "COMPRESS");
        CPLAddXMLAttributeAndValue(psCompressNode, "type", "string-select");
        CPLAddXMLAttributeAndValue(psCompressNode, "description", "Compression method");
        CPLAddXMLAttributeAndValue(psCompressNode, "default", "NONE");
        {
            auto poValueNode = CPLCreateXMLNode(psCompressNode, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }

        auto psFilterNode = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psFilterNode, "name", "FILTER");
        CPLAddXMLAttributeAndValue(psFilterNode, "type", "string-select");
        CPLAddXMLAttributeAndValue(psFilterNode, "description", "Filter method (only for ZARR_V2)");
        CPLAddXMLAttributeAndValue(psFilterNode, "default", "NONE");
        {
            auto poValueNode = CPLCreateXMLNode(psFilterNode, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }

        auto psBlockSizeNode = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psBlockSizeNode, "name", "BLOCKSIZE");
        CPLAddXMLAttributeAndValue(psBlockSizeNode, "type", "string");
        CPLAddXMLAttributeAndValue(psBlockSizeNode, "description",
            "Comma separated list of chunk size along each dimension");

        auto psChunkMemoryLayout = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "name", "CHUNK_MEMORY_LAYOUT");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "type", "string-select");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "description",
            "Whether to use C (row-major) order or F (column-major) order in chunks");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "default", "C");
        {
            auto poValueNode = CPLCreateXMLNode(psChunkMemoryLayout, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "C");
        }
        {
            auto poValueNode = CPLCreateXMLNode(psChunkMemoryLayout, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "F");
        }

        auto psStringFormat = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psStringFormat, "name", "STRING_FORMAT");
        CPLAddXMLAttributeAndValue(psStringFormat, "type", "string-select");
        CPLAddXMLAttributeAndValue(psStringFormat, "default", "STRING");
        {
            auto poValueNode = CPLCreateXMLNode(psStringFormat, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "STRING");
        }
        {
            auto poValueNode = CPLCreateXMLNode(psStringFormat, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "UNICODE");
        }

        auto psDimSeparatorNode = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psDimSeparatorNode, "name", "DIM_SEPARATOR");
        CPLAddXMLAttributeAndValue(psDimSeparatorNode, "type", "string");
        CPLAddXMLAttributeAndValue(psDimSeparatorNode, "description",
            "Dimension separator in chunk filenames. Default to decimal point for ZarrV2 and slash for ZarrV3");

        for( auto iter = compressors; iter && *iter; ++iter )
        {
            const auto psCompressor = CPLGetCompressor(*iter);
            if( psCompressor )
            {
                auto poValueNode = CPLCreateXMLNode(
                    (psCompressor->eType == CCT_COMPRESSOR) ? psCompressNode : psFilterNode,
                    CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text,
                                 CPLString(*iter).toupper().c_str());

                const char* pszOptions = CSLFetchNameValue(
                                    psCompressor->papszMetadata, "OPTIONS");
                if( pszOptions )
                {
                    CPLXMLTreeCloser oTreeCompressor(CPLParseXMLString(pszOptions));
                    const auto psRoot = oTreeCompressor.get() ?
                        CPLGetXMLNode(oTreeCompressor.get(), "=Options") : nullptr;
                    if( psRoot )
                    {
                        for( CPLXMLNode* psNode = psRoot->psChild;
                                    psNode != nullptr; psNode = psNode->psNext )
                        {
                            if( psNode->eType == CXT_Element )
                            {
                                const char* pszName = CPLGetXMLValue(
                                    psNode, "name", nullptr);
                                if( pszName
                                    && !EQUAL(pszName, "TYPESIZE") // Blosc
                                    && !EQUAL(pszName, "HEADER") // LZ4
                                  )
                                {
                                    CPLXMLNode* psNext = psNode->psNext;
                                    psNode->psNext = nullptr;
                                    CPLXMLNode* psOption = CPLCloneXMLTree(psNode);

                                    CPLXMLNode* psName = CPLGetXMLNode(psOption, "name");
                                    if( psName && psName->eType == CXT_Attribute &&
                                        psName->psChild && psName->psChild->pszValue )
                                    {
                                        CPLString osNewValue(*iter);
                                        osNewValue = osNewValue.toupper();
                                        osNewValue += '_';
                                        osNewValue += psName->psChild->pszValue;
                                        CPLFree(psName->psChild->pszValue);
                                        psName->psChild->pszValue = CPLStrdup(osNewValue.c_str());
                                    }

                                    CPLXMLNode* psDescription = CPLGetXMLNode(psOption, "description");
                                    if( psDescription && psDescription->eType == CXT_Attribute &&
                                        psDescription->psChild && psDescription->psChild->pszValue )
                                    {
                                        std::string osNewValue(psDescription->psChild->pszValue);
                                        if( psCompressor->eType == CCT_COMPRESSOR )
                                        {
                                            osNewValue += ". Only used when COMPRESS=";
                                        }
                                        else
                                        {
                                            osNewValue += ". Only used when FILTER=";
                                        }
                                        osNewValue += CPLString(*iter).toupper();
                                        CPLFree(psDescription->psChild->pszValue);
                                        psDescription->psChild->pszValue = CPLStrdup(osNewValue.c_str());
                                    }

                                    CPLAddXMLChild(oTree.get(), psOption);
                                    psNode->psNext = psNext;
                                }
                            }
                        }
                    }
                }
            }
        }
        CSLDestroy(compressors);

        {
            char* pszXML = CPLSerializeXMLTree(oTree.get());
            GDALDriver::SetMetadataItem(GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST,
                CPLString(pszXML).replaceAll("CreationOptionList",
                                             "MultiDimArrayCreationOptionList").c_str());
            CPLFree(pszXML);
        }

        {
            auto psArrayNameOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psArrayNameOption, "name", "ARRAY_NAME");
            CPLAddXMLAttributeAndValue(psArrayNameOption, "type", "string");
            CPLAddXMLAttributeAndValue(psArrayNameOption, "description",
                "Array name. If not specified, deduced from the filename");

            auto psAppendSubDSOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "name", "APPEND_SUBDATASET");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "type", "boolean");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "description",
                "Whether to append the new dataset to an existing Zarr hierarchy");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "default", "NO");

            auto psFormat = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psFormat, "name", "FORMAT");
            CPLAddXMLAttributeAndValue(psFormat, "type", "string-select");
            CPLAddXMLAttributeAndValue(psFormat, "default", "ZARR_V2");
            {
                auto poValueNode = CPLCreateXMLNode(psFormat, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "ZARR_V2");
            }
            {
                auto poValueNode = CPLCreateXMLNode(psFormat, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "ZARR_V3");
            }

            auto psCreateZMetadata = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "name", "CREATE_ZMETADATA");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "type", "boolean");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "description",
                "Whether to create consolidated metadata into .zmetadata (Zarr V2 only)");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "default", "YES");

            char* pszXML = CPLSerializeXMLTree(oTree.get());
            GDALDriver::SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, pszXML);
            CPLFree(pszXML);
        }
    }
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset * ZarrDataset::CreateMultiDimensional( const char * pszFilename,
                                                  CSLConstList /*papszRootGroupOptions*/,
                                                  CSLConstList papszOptions )
{
    const char* pszFormat = CSLFetchNameValueDef(papszOptions, "FORMAT", "ZARR_V2");
    std::shared_ptr<GDALGroup> poRG;
    auto poSharedResource = std::make_shared<ZarrSharedResource>(pszFilename);
    if( EQUAL(pszFormat, "ZARR_V3") )
    {
        poRG = ZarrGroupV3::CreateOnDisk(poSharedResource,
                                         std::string(), "/", pszFilename);
    }
    else
    {
        const bool bCreateZMetadata = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "CREATE_ZMETADATA", "YES"));
        if( bCreateZMetadata )
        {
            poSharedResource->EnableZMetadata();
        }
        poRG = ZarrGroupV2::CreateOnDisk(poSharedResource,
                                         std::string(), "/", pszFilename);
    }
    if( !poRG )
        return nullptr;

    auto poDS = new ZarrDataset(poRG);
    poDS->SetDescription(pszFilename);
    return poDS;
}

/************************************************************************/
/*                            Create()                                  */
/************************************************************************/

GDALDataset * ZarrDataset::Create( const char * pszName,
                                   int nXSize, int nYSize, int nBands,
                                   GDALDataType eType,
                                   char ** papszOptions )
{
    if( nBands <= 0 || nXSize <= 0 || nYSize <= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "nBands, nXSize, nYSize should be > 0");
        return nullptr;
    }

    const bool bAppendSubDS = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO"));
    const char* pszArrayName = CSLFetchNameValue(papszOptions, "ARRAY_NAME");

    std::shared_ptr<GDALGroup> poRG;
    if( bAppendSubDS )
    {
        if( pszArrayName == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ARRAY_NAME should be provided when "
                     "APPEND_SUBDATASET is set to YES");
            return nullptr;
        }
        auto poDS = std::unique_ptr<GDALDataset>(OpenMultidim(pszName, true, nullptr));
        if( poDS == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszName);
            return nullptr;
        }
        poRG = poDS->GetRootGroup();
    }
    else
    {
        const char* pszFormat = CSLFetchNameValueDef(papszOptions, "FORMAT", "ZARR_V2");
        auto poSharedResource = std::make_shared<ZarrSharedResource>(pszName);
        if( EQUAL(pszFormat, "ZARR_V3") )
        {
            poRG = ZarrGroupV3::CreateOnDisk(poSharedResource,
                                             std::string(), "/", pszName);
        }
        else
        {
            const bool bCreateZMetadata = CPLTestBool(
                CSLFetchNameValueDef(papszOptions, "CREATE_ZMETADATA", "YES"));
            if( bCreateZMetadata )
            {
                poSharedResource->EnableZMetadata();
            }
            poRG = ZarrGroupV2::CreateOnDisk(poSharedResource,
                                             std::string(), "/", pszName);
        }
    }
    if( !poRG )
        return nullptr;

    auto poDS = cpl::make_unique<ZarrDataset>(poRG);
    poDS->SetDescription(pszName);
    poDS->nRasterYSize = nYSize;
    poDS->nRasterXSize = nXSize;
    poDS->eAccess = GA_Update;

    if( bAppendSubDS )
    {
        auto aoDims = poRG->GetDimensions();
        for( const auto& poDim: aoDims )
        {
            if( poDim->GetName() == "Y" &&
                poDim->GetSize() == static_cast<GUInt64>(nYSize) )
            {
                poDS->m_poDimY = poDim;
            }
            else if( poDim->GetName() == "X" &&
                poDim->GetSize() == static_cast<GUInt64>(nXSize) )
            {
                poDS->m_poDimX = poDim;
            }
        }
        if( poDS->m_poDimY == nullptr )
        {
            poDS->m_poDimY = poRG->CreateDimension(
                std::string(pszArrayName) + "_Y", std::string(), std::string(), nYSize);
        }
        if( poDS->m_poDimX == nullptr )
        {
            poDS->m_poDimX = poRG->CreateDimension(
                std::string(pszArrayName) + "_X", std::string(), std::string(), nXSize);
        }
    }
    else
    {
        poDS->m_poDimY = poRG->CreateDimension("Y", std::string(), std::string(), nYSize);
        poDS->m_poDimX = poRG->CreateDimension("X", std::string(), std::string(), nXSize);
    }
    if( poDS->m_poDimY == nullptr || poDS->m_poDimX == nullptr )
        return nullptr;
    const auto aoDims = std::vector<std::shared_ptr<GDALDimension>>{
        poDS->m_poDimY, poDS->m_poDimX};

    for( int i = 0; i < nBands; i++ )
    {
        auto poArray = poRG->CreateMDArray(
            pszArrayName ?
                (nBands == 1 ? pszArrayName : CPLSPrintf("%s_band%d", pszArrayName, i+1)):
                (nBands == 1 ? CPLGetBasename(pszName) : CPLSPrintf("Band%d", i+1)),
            aoDims,
            GDALExtendedDataType::Create(eType),
            papszOptions);
        if( poArray == nullptr )
            return nullptr;
        poDS->SetBand(i + 1, new ZarrRasterBand(poArray));
    }

    return poDS.release();
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference* ZarrDataset::GetSpatialRef() const
{
    if( nBands >= 1 )
        return cpl::down_cast<ZarrRasterBand*>(papoBands[0])->m_poArray->GetSpatialRef().get();
    return nullptr;
}

/************************************************************************/
/*                          SetSpatialRef()                             */
/************************************************************************/

CPLErr ZarrDataset::SetSpatialRef(const OGRSpatialReference* poSRS)
{
    for( int i = 0; i < nBands; ++i )
    {
        cpl::down_cast<ZarrRasterBand*>(papoBands[i])->m_poArray->SetSpatialRef(poSRS);
    }
    return CE_None;
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr ZarrDataset::GetGeoTransform( double * padfTransform )
{
    memcpy(padfTransform, &m_adfGeoTransform[0], 6 * sizeof(double));
    return m_bHasGT ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         SetGeoTransform()                            */
/************************************************************************/

CPLErr ZarrDataset::SetGeoTransform( double * padfTransform )
{
    if( padfTransform[2] != 0 || padfTransform[4] != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geotransform with rotated terms not supported");
        return CE_Failure;
    }
    if( m_poDimX == nullptr || m_poDimY == nullptr )
        return CE_Failure;

    memcpy(&m_adfGeoTransform[0], padfTransform, 6 * sizeof(double));
    m_bHasGT = true;

    const auto oDTFloat64 = GDALExtendedDataType::Create(GDT_Float64);
    {
        auto poX = m_poRootGroup->OpenMDArray(m_poDimX->GetName());
        if( !poX )
            poX = m_poRootGroup->CreateMDArray(m_poDimX->GetName(), { m_poDimX },
                                               oDTFloat64,
                                               nullptr);
        if( !poX )
            return CE_Failure;
        m_poDimX->SetIndexingVariable(poX);
        std::vector<double> adfX;
        try
        {
            adfX.reserve(nRasterXSize);
            for( int i = 0; i < nRasterXSize; ++i )
                adfX.emplace_back( padfTransform[0] + padfTransform[1] * (i + 0.5) );
        }
        catch( const std::exception& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when allocating X array");
            return CE_Failure;
        }
        const GUInt64 nStartIndex = 0;
        const size_t nCount = adfX.size();
        const GInt64 arrayStep = 1;
        const GPtrDiff_t bufferStride = 1;
        if( !poX->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                        poX->GetDataType(), adfX.data()) )
        {
            return CE_Failure;
        }
    }

    {
        auto poY = m_poRootGroup->OpenMDArray(m_poDimY->GetName());
        if( !poY )
            poY = m_poRootGroup->CreateMDArray(m_poDimY->GetName(), { m_poDimY },
                                               oDTFloat64,
                                               nullptr);
        if( !poY )
            return CE_Failure;
        m_poDimY->SetIndexingVariable(poY);
        std::vector<double> adfY;
        try
        {
            adfY.reserve(nRasterYSize);
            for( int i = 0; i < nRasterYSize; ++i )
                adfY.emplace_back( padfTransform[3] + padfTransform[5] * (i + 0.5) );
        }
        catch( const std::exception& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when allocating Y array");
            return CE_Failure;
        }
        const GUInt64 nStartIndex = 0;
        const size_t nCount = adfY.size();
        const GInt64 arrayStep = 1;
        const GPtrDiff_t bufferStride = 1;
        if( !poY->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                        poY->GetDataType(), adfY.data()) )
        {
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          SetMetadata()                               */
/************************************************************************/

CPLErr ZarrDataset::SetMetadata(char** papszMetadata, const char* pszDomain)
{
    if( nBands >= 1 && (pszDomain == nullptr || pszDomain[0] == '\0') )
    {
        const auto oStringDT = GDALExtendedDataType::CreateString();
        for( int i = 0; i < nBands; ++i )
        {
            auto& poArray = cpl::down_cast<ZarrRasterBand*>(papoBands[i])->m_poArray;
            for( auto iter = papszMetadata; iter && *iter; ++iter )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(*iter, &pszKey);
                if( pszKey && pszValue )
                {
                    auto poAttr = poArray->CreateAttribute(pszKey, {},
                                                           oStringDT);
                    if( poAttr )
                    {
                        const GUInt64 nStartIndex = 0;
                        const size_t nCount = 1;
                        const GInt64 arrayStep = 1;
                        const GPtrDiff_t bufferStride = 1;
                        poAttr->Write(&nStartIndex, &nCount,
                                      &arrayStep, &bufferStride,
                                      oStringDT, &pszValue);
                    }
                }
                CPLFree(pszKey);
            }
        }
    }
    return GDALDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                    ZarrRasterBand::ZarrRasterBand()                  */
/************************************************************************/

ZarrRasterBand::ZarrRasterBand(const std::shared_ptr<GDALMDArray>& poArray):
    m_poArray(poArray)
{
    assert( poArray->GetDimensionCount() == 2 );
    eDataType = poArray->GetDataType().GetNumericDataType();
    nBlockXSize = static_cast<int>(poArray->GetBlockSize()[1]);
    nBlockYSize = static_cast<int>(poArray->GetBlockSize()[0]);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ZarrRasterBand::GetNoDataValue(int* pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsDouble(&bHasNodata);
    if( pbHasNoData )
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                        GetNoDataValueAsInt64()                       */
/************************************************************************/

int64_t ZarrRasterBand::GetNoDataValueAsInt64(int* pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsInt64(&bHasNodata);
    if( pbHasNoData )
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                       */
/************************************************************************/

uint64_t ZarrRasterBand::GetNoDataValueAsUInt64(int* pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsUInt64(&bHasNodata);
    if( pbHasNoData )
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ZarrRasterBand::SetNoDataValue(double dfNoData)
{
    return m_poArray->SetNoDataValue(dfNoData) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       SetNoDataValueAsInt64()                        */
/************************************************************************/

CPLErr ZarrRasterBand::SetNoDataValueAsInt64(int64_t nNoData)
{
    return m_poArray->SetNoDataValue(nNoData) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       SetNoDataValueAsUInt64()                       */
/************************************************************************/

CPLErr ZarrRasterBand::SetNoDataValueAsUInt64(uint64_t nNoData)
{
    return m_poArray->SetNoDataValue(nNoData) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ZarrRasterBand::GetOffset( int *pbSuccess )
{
    bool bHasValue = false;
    double dfRet = m_poArray->GetOffset(&bHasValue);
    if( pbSuccess )
        *pbSuccess = bHasValue ? TRUE : FALSE;
    return dfRet;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ZarrRasterBand::SetOffset( double dfNewOffset )
{
    return m_poArray->SetOffset(dfNewOffset) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ZarrRasterBand::GetScale( int *pbSuccess )
{
    bool bHasValue = false;
    double dfRet = m_poArray->GetScale(&bHasValue);
    if( pbSuccess )
        *pbSuccess = bHasValue ? TRUE : FALSE;
    return dfRet;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ZarrRasterBand::SetScale( double dfNewScale )
{
    return m_poArray->SetScale(dfNewScale) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                             GetUnitType()                            */
/************************************************************************/

const char *ZarrRasterBand::GetUnitType()
{
    return m_poArray->GetUnit().c_str();
}

/************************************************************************/
/*                             SetUnitType()                            */
/************************************************************************/

CPLErr ZarrRasterBand::SetUnitType( const char * pszNewValue )
{
    return m_poArray->SetUnit(pszNewValue ? pszNewValue : "") ? CE_None : CE_Failure;
}

/************************************************************************/
/*                    ZarrRasterBand::IReadBlock()                      */
/************************************************************************/

CPLErr ZarrRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pData )
{

    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GUInt64 arrayStartIdx[] = { static_cast<GUInt64>(nYOff),
                                static_cast<GUInt64>(nXOff) };
    size_t count[] = { static_cast<size_t>(nReqYSize),
                       static_cast<size_t>(nReqXSize) };
    constexpr GInt64 arrayStep[] = { 1, 1 };
    GPtrDiff_t bufferStride[] = { nBlockXSize , 1 };
    return m_poArray->Read(arrayStartIdx, count, arrayStep, bufferStride,
                           m_poArray->GetDataType(), pData) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                    ZarrRasterBand::IWriteBlock()                      */
/************************************************************************/

CPLErr ZarrRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff, void * pData )
{
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GUInt64 arrayStartIdx[] = { static_cast<GUInt64>(nYOff),
                                static_cast<GUInt64>(nXOff) };
    size_t count[] = { static_cast<size_t>(nReqYSize),
                       static_cast<size_t>(nReqXSize) };
    constexpr GInt64 arrayStep[] = { 1, 1 };
    GPtrDiff_t bufferStride[] = { nBlockXSize, 1 };
    return m_poArray->Write(arrayStartIdx, count, arrayStep, bufferStride,
                            m_poArray->GetDataType(), pData) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr ZarrRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg )
{
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));
    if( nXSize == nBufXSize && nYSize == nBufYSize && nBufferDTSize > 0 &&
        (nPixelSpaceBuf % nBufferDTSize) == 0 &&
        (nLineSpaceBuf % nBufferDTSize) == 0 )
    {
        GUInt64 arrayStartIdx[] = { static_cast<GUInt64>(nYOff),
                                    static_cast<GUInt64>(nXOff) };
        size_t count[] = { static_cast<size_t>(nYSize),
                           static_cast<size_t>(nXSize) };
        constexpr GInt64 arrayStep[] = { 1, 1 };
        GPtrDiff_t bufferStride[] = {
            static_cast<GPtrDiff_t>(nLineSpaceBuf / nBufferDTSize),
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nBufferDTSize) };

        if( eRWFlag == GF_Read )
        {
            return m_poArray->Read(arrayStartIdx, count, arrayStep, bufferStride,
                                   GDALExtendedDataType::Create(eBufType), pData) ?
                                 CE_None : CE_Failure;
        }
        else
        {
            return m_poArray->Write(arrayStartIdx, count, arrayStep, bufferStride,
                                   GDALExtendedDataType::Create(eBufType), pData) ?
                                 CE_None : CE_Failure;
        }
    }
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpaceBuf, nLineSpaceBuf,
                                     psExtraArg);
}

/************************************************************************/
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if( GDALGetDriverByName( "Zarr" ) != nullptr )
        return;

    GDALDriver *poDriver = new ZarrDriver();

    poDriver->SetDescription( "Zarr" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Zarr" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 Int64 UInt64 "
                               "Float32 Float64 CFloat32 CFloat64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='USE_ZMETADATA' type='boolean' description='Whether to use consolidated metadata from .zmetadata' default='YES'/>"
"   <Option name='CACHE_TILE_PRESENCE' type='boolean' description='Whether to establish an initial listing of present tiles' default='NO'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST,
"<MultiDimDatasetCreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='ZARR_V2'>"
"     <Value>ZARR_V2</Value>"
"     <Value>ZARR_V3</Value>"
"   </Option>"
"   <Option name='CREATE_ZMETADATA' type='boolean' "
    "description='Whether to create consolidated metadata into .zmetadata (Zarr V2 only)' default='YES'/>"
"</MultiDimDatasetCreationOptionList>" );


    poDriver->pfnIdentify = ZarrDataset::Identify;
    poDriver->pfnOpen = ZarrDataset::Open;
    poDriver->pfnCreateMultiDimensional = ZarrDataset::CreateMultiDimensional;
    poDriver->pfnCreate = ZarrDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
