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

#include <algorithm>
#include <cassert>
#include <limits>

#ifdef HAVE_BLOSC
#include <blosc.h>
#endif

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

    auto poDS = std::unique_ptr<ZarrDataset>(new ZarrDataset());
    auto poRG = std::make_shared<ZarrGroupV2>(std::string(), "/");
    poRG->SetUpdatable(bUpdateMode);
    poDS->m_poRootGroup = poRG;

    const std::string osZarrayFilename(
                CPLFormFilename(pszFilename, ".zarray", nullptr));
    VSIStatBufL sStat;
    if( VSIStatL( osZarrayFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osZarrayFilename) )
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        const std::string osArrayName(CPLGetBasename(osFilename.c_str()));
        if( !poRG->LoadArray(osArrayName, osZarrayFilename, oRoot,
                             false, CPLJSONObject()) )
            return nullptr;

        return poDS.release();
    }

    poRG->SetDirectoryName(osFilename);

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
        return poDS.release();
    }

    const std::string osGroupFilename(
            CPLFormFilename(pszFilename, ".zgroup", nullptr));
    if( VSIStatL( osGroupFilename.c_str(), &sStat ) == 0 )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.Load(osGroupFilename) )
            return nullptr;
        return poDS.release();
    }

    // Zarr v3
    auto poRG_V3 = std::make_shared<ZarrGroupV3>(std::string(), "/");
    poRG_V3->SetDirectoryName(osFilename);
    // poRG_V3->SetUpdatable(bUpdateMode); // TODO
    poDS->m_poRootGroup = poRG_V3;
    return poDS.release();
}

/************************************************************************/
/*                            ExploreGroup()                            */
/************************************************************************/

static void ExploreGroup(const std::shared_ptr<GDALGroup>& poGroup,
                         std::vector<std::string>& aosArrays)
{
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
    }

    const auto aosSubGroups = poGroup->GetGroupNames();
    for( const auto& osSubGroup: aosSubGroups )
    {
        const auto poSubGroup = poGroup->OpenGroup(osSubGroup);
        if( poSubGroup )
            ExploreGroup(poSubGroup, aosArrays);
    }
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
        if( aosTokens.size() < 3 )
            return nullptr;
        osFilename = aosTokens[1];
        osArrayOfInterest = aosTokens[2];
        for( int i = 3; i < aosTokens.size(); ++i )
        {
            anExtraDimIndices.push_back(
                static_cast<uint64_t>(CPLAtoGIntBig(aosTokens[i])));
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

    std::unique_ptr<ZarrDataset> poDS(new ZarrDataset());
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
        ExploreGroup(poRG, aosArrays);
        if( aosArrays.empty() )
            return nullptr;

        std::string osMainArray;
        if( aosArrays.size() == 1 )
        {
            poMainArray = poRG->OpenMDArrayFromFullname(aosArrays[0]);
            if( poMainArray )
                osMainArray = poMainArray->GetFullName();
        }
        else if( aosArrays.size() >= 2 )
        {
            for( size_t i = 0; i < aosArrays.size(); ++i )
            {
                auto poArray = poRG->OpenMDArrayFromFullname(aosArrays[i]);
                if( poArray && poArray->GetDimensionCount() >= 2 )
                {
                    if( osMainArray.empty() )
                    {
                        poMainArray = poArray;
                        osMainArray = aosArrays[i];
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
                             poOpenInfo->pszFilename, osMainArray.c_str());
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Too many samples along the > 2D dimensions of %s. "
                             "Use ZARR:\"%s\":%s:{i}:{j} syntax",
                             osMainArray.c_str(),
                             poOpenInfo->pszFilename, osMainArray.c_str());
                }
            }
            else if( nExtraDimSamples > 1 && apoDims.size() == 3 )
            {
                for( int i = 0; i < static_cast<int>(nExtraDimSamples); ++i )
                {
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s:%d",
                                   iCountSubDS, poOpenInfo->pszFilename,
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
                                   iCountSubDS, poOpenInfo->pszFilename,
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
                                   iCountSubDS, poOpenInfo->pszFilename,
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
            EQUAL(pszName, "BLOSC_COMPRESSORS") )
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
        char** decompressors = CPLGetDecompressors();
        for( auto iter = decompressors; iter && *iter; ++iter )
        {
            if( !osCompressors.empty() )
                osCompressors += ',';
            osCompressors += *iter;
        }
        CSLDestroy(decompressors);
        GDALDriver::SetMetadataItem("COMPRESSORS", osCompressors.c_str());
    }
#ifdef HAVE_BLOSC
    {
        GDALDriver::SetMetadataItem("BLOSC_COMPRESSORS",
                                    blosc_list_compressors());
    }
#endif
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset * ZarrDataset::CreateMultiDimensional( const char * pszFilename,
                                                  CSLConstList /*papszRootGroupOptions*/,
                                                  CSLConstList /*papszOptions*/ )
{
    auto poRG = ZarrGroupV2::CreateOnDisk(std::string(), "/", pszFilename);
    if( !poRG )
        return nullptr;

    auto poDS = std::unique_ptr<ZarrDataset>(new ZarrDataset());
    poDS->SetDescription(pszFilename);
    poDS->m_poRootGroup = poRG;
    return poDS.release();
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
    //poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
    //                           "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='USE_ZMETADATA' type='boolean' description='Whether to use consolidated metadata from .zmetadata' default='YES'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = ZarrDataset::Identify;
    poDriver->pfnOpen = ZarrDataset::Open;
    poDriver->pfnCreateMultiDimensional = ZarrDataset::CreateMultiDimensional;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
