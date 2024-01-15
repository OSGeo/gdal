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
#include "zarrdrivercore.h"

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

ZarrDataset::ZarrDataset(const std::shared_ptr<GDALGroup> &poRootGroup)
    : m_poRootGroup(poRootGroup)
{
}

/************************************************************************/
/*                           OpenMultidim()                             */
/************************************************************************/

GDALDataset *ZarrDataset::OpenMultidim(const char *pszFilename,
                                       bool bUpdateMode,
                                       CSLConstList papszOpenOptionsIn)
{
    CPLString osFilename(pszFilename);
    if (osFilename.back() == '/')
        osFilename.resize(osFilename.size() - 1);

    auto poSharedResource = ZarrSharedResource::Create(osFilename, bUpdateMode);
    poSharedResource->SetOpenOptions(papszOpenOptionsIn);

    auto poRG = poSharedResource->GetRootGroup();
    if (!poRG)
        return nullptr;
    return new ZarrDataset(poRG);
}

/************************************************************************/
/*                            ExploreGroup()                            */
/************************************************************************/

static bool ExploreGroup(const std::shared_ptr<GDALGroup> &poGroup,
                         std::vector<std::string> &aosArrays, int nRecCount)
{
    if (nRecCount == 32)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too deep recursion level in ExploreGroup()");
        return false;
    }
    const auto aosGroupArrayNames = poGroup->GetMDArrayNames();
    for (const auto &osArrayName : aosGroupArrayNames)
    {
        std::string osArrayFullname = poGroup->GetFullName();
        if (osArrayName != "/")
        {
            if (osArrayFullname != "/")
                osArrayFullname += '/';
            osArrayFullname += osArrayName;
        }
        aosArrays.emplace_back(std::move(osArrayFullname));
        if (aosArrays.size() == 10000)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many arrays found by ExploreGroup()");
            return false;
        }
    }

    const auto aosSubGroups = poGroup->GetGroupNames();
    for (const auto &osSubGroup : aosSubGroups)
    {
        const auto poSubGroup = poGroup->OpenGroup(osSubGroup);
        if (poSubGroup)
        {
            if (!ExploreGroup(poSubGroup, aosArrays, nRecCount + 1))
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *ZarrDataset::GetMetadataItem(const char *pszName,
                                         const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        return m_aosSubdatasets.FetchNameValue(pszName);
    return nullptr;
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **ZarrDataset::GetMetadata(const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        return m_aosSubdatasets.List();
    return nullptr;
}

/************************************************************************/
/*                      GetXYDimensionIndices()                         */
/************************************************************************/

static void GetXYDimensionIndices(const std::shared_ptr<GDALMDArray> &poArray,
                                  const GDALOpenInfo *poOpenInfo, size_t &iXDim,
                                  size_t &iYDim)
{
    const size_t nDims = poArray->GetDimensionCount();
    iYDim = nDims >= 2 ? nDims - 2 : 0;
    iXDim = nDims >= 2 ? nDims - 1 : 0;

    if (nDims >= 2)
    {
        const char *pszDimX =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "DIM_X");
        const char *pszDimY =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "DIM_Y");
        bool bFoundX = false;
        bool bFoundY = false;
        const auto &apoDims = poArray->GetDimensions();
        for (size_t i = 0; i < nDims; ++i)
        {
            if (pszDimX && apoDims[i]->GetName() == pszDimX)
            {
                bFoundX = true;
                iXDim = i;
            }
            else if (pszDimY && apoDims[i]->GetName() == pszDimY)
            {
                bFoundY = true;
                iYDim = i;
            }
            else if (!pszDimX &&
                     (apoDims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X ||
                      apoDims[i]->GetName() == "X"))
                iXDim = i;
            else if (!pszDimY &&
                     (apoDims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y ||
                      apoDims[i]->GetName() == "Y"))
                iYDim = i;
        }
        if (pszDimX)
        {
            if (!bFoundX && CPLGetValueType(pszDimX) == CPL_VALUE_INTEGER)
            {
                const int nTmp = atoi(pszDimX);
                if (nTmp >= 0 && nTmp <= static_cast<int>(nDims))
                {
                    iXDim = nTmp;
                    bFoundX = true;
                }
            }
            if (!bFoundX)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find dimension DIM_X=%s", pszDimX);
            }
        }
        if (pszDimY)
        {
            if (!bFoundY && CPLGetValueType(pszDimY) == CPL_VALUE_INTEGER)
            {
                const int nTmp = atoi(pszDimY);
                if (nTmp >= 0 && nTmp <= static_cast<int>(nDims))
                {
                    iYDim = nTmp;
                    bFoundY = true;
                }
            }
            if (!bFoundY)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find dimension DIM_Y=%s", pszDimY);
            }
        }
    }
}

/************************************************************************/
/*                       GetExtraDimSampleCount()                       */
/************************************************************************/

static uint64_t
GetExtraDimSampleCount(const std::shared_ptr<GDALMDArray> &poArray,
                       size_t iXDim, size_t iYDim)
{
    uint64_t nExtraDimSamples = 1;
    const auto &apoDims = poArray->GetDimensions();
    for (size_t i = 0; i < apoDims.size(); ++i)
    {
        if (i != iXDim && i != iYDim)
            nExtraDimSamples *= apoDims[i]->GetSize();
    }
    return nExtraDimSamples;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ZarrDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!ZARRDriverIdentify(poOpenInfo))
    {
        return nullptr;
    }

    CPLString osFilename(poOpenInfo->pszFilename);
    CPLString osArrayOfInterest;
    std::vector<uint64_t> anExtraDimIndices;
    if (STARTS_WITH(poOpenInfo->pszFilename, "ZARR:"))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(
            poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() < 2)
            return nullptr;
        osFilename = aosTokens[1];
        std::string osErrorMsg;
        if (osFilename == "http" || osFilename == "https")
        {
            osErrorMsg = "There is likely a quoting error of the whole "
                         "connection string, and the filename should "
                         "likely be prefixed with /vsicurl/";
        }
        else if (osFilename == "/vsicurl/http" ||
                 osFilename == "/vsicurl/https")
        {
            osErrorMsg = "There is likely a quoting error of the whole "
                         "connection string.";
        }
        else if (STARTS_WITH(osFilename.c_str(), "http://") ||
                 STARTS_WITH(osFilename.c_str(), "https://"))
        {
            osErrorMsg =
                "The filename should likely be prefixed with /vsicurl/";
        }
        if (!osErrorMsg.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMsg.c_str());
            return nullptr;
        }
        if (aosTokens.size() >= 3)
        {
            osArrayOfInterest = aosTokens[2];
            for (int i = 3; i < aosTokens.size(); ++i)
            {
                anExtraDimIndices.push_back(
                    static_cast<uint64_t>(CPLAtoGIntBig(aosTokens[i])));
            }
        }
    }

    auto poDSMultiDim = std::unique_ptr<GDALDataset>(
        OpenMultidim(osFilename.c_str(), poOpenInfo->eAccess == GA_Update,
                     poOpenInfo->papszOpenOptions));
    if (poDSMultiDim == nullptr ||
        (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
    {
        return poDSMultiDim.release();
    }

    auto poRG = poDSMultiDim->GetRootGroup();

    auto poDS = std::make_unique<ZarrDataset>(nullptr);
    std::shared_ptr<GDALMDArray> poMainArray;
    std::vector<std::string> aosArrays;
    std::string osMainArray;
    const bool bMultiband = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MULTIBAND", "YES"));
    size_t iXDim = 0;
    size_t iYDim = 0;

    if (!osArrayOfInterest.empty())
    {
        poMainArray = osArrayOfInterest == "/"
                          ? poRG->OpenMDArray("/")
                          : poRG->OpenMDArrayFromFullname(osArrayOfInterest);
        if (poMainArray == nullptr)
            return nullptr;
        GetXYDimensionIndices(poMainArray, poOpenInfo, iXDim, iYDim);

        if (poMainArray->GetDimensionCount() > 2)
        {
            if (anExtraDimIndices.empty())
            {
                const uint64_t nExtraDimSamples =
                    GetExtraDimSampleCount(poMainArray, iXDim, iYDim);
                if (bMultiband)
                {
                    if (nExtraDimSamples > 65536)  // arbitrary limit
                    {
                        if (poMainArray->GetDimensionCount() == 3)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Too many samples along the > 2D "
                                     "dimensions of %s. "
                                     "Use ZARR:\"%s\":%s:{i} syntax",
                                     osArrayOfInterest.c_str(),
                                     osFilename.c_str(),
                                     osArrayOfInterest.c_str());
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Too many samples along the > 2D "
                                     "dimensions of %s. "
                                     "Use ZARR:\"%s\":%s:{i}:{j} syntax",
                                     osArrayOfInterest.c_str(),
                                     osFilename.c_str(),
                                     osArrayOfInterest.c_str());
                        }
                        return nullptr;
                    }
                }
                else if (nExtraDimSamples != 1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Indices of extra dimensions must be specified");
                    return nullptr;
                }
            }
            else if (anExtraDimIndices.size() !=
                     poMainArray->GetDimensionCount() - 2)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong number of indices of extra dimensions");
                return nullptr;
            }
            else
            {
                for (const auto idx : anExtraDimIndices)
                {
                    poMainArray = poMainArray->at(idx);
                    if (poMainArray == nullptr)
                        return nullptr;
                }
                GetXYDimensionIndices(poMainArray, poOpenInfo, iXDim, iYDim);
            }
        }
        else if (!anExtraDimIndices.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected extra indices");
            return nullptr;
        }
    }
    else
    {
        ExploreGroup(poRG, aosArrays, 0);
        if (aosArrays.empty())
            return nullptr;

        if (aosArrays.size() == 1)
        {
            poMainArray = poRG->OpenMDArrayFromFullname(aosArrays[0]);
            if (poMainArray)
                osMainArray = poMainArray->GetFullName();
        }
        else  // at least 2 arrays
        {
            for (const auto &osArrayName : aosArrays)
            {
                auto poArray = poRG->OpenMDArrayFromFullname(osArrayName);
                if (poArray && poArray->GetDimensionCount() >= 2)
                {
                    if (osMainArray.empty())
                    {
                        poMainArray = std::move(poArray);
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

        if (poMainArray)
            GetXYDimensionIndices(poMainArray, poOpenInfo, iXDim, iYDim);

        int iCountSubDS = 1;

        if (poMainArray && poMainArray->GetDimensionCount() > 2)
        {
            const auto &apoDims = poMainArray->GetDimensions();
            const uint64_t nExtraDimSamples =
                GetExtraDimSampleCount(poMainArray, iXDim, iYDim);
            if (nExtraDimSamples > 65536)  // arbitrary limit
            {
                if (apoDims.size() == 3)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Too many samples along the > 2D dimensions of %s. "
                        "Use ZARR:\"%s\":%s:{i} syntax",
                        osMainArray.c_str(), osFilename.c_str(),
                        osMainArray.c_str());
                }
                else
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Too many samples along the > 2D dimensions of %s. "
                        "Use ZARR:\"%s\":%s:{i}:{j} syntax",
                        osMainArray.c_str(), osFilename.c_str(),
                        osMainArray.c_str());
                }
            }
            else if (nExtraDimSamples > 1 && bMultiband)
            {
                // nothing to do
            }
            else if (nExtraDimSamples > 1 && apoDims.size() == 3)
            {
                for (int i = 0; i < static_cast<int>(nExtraDimSamples); ++i)
                {
                    poDS->m_aosSubdatasets.AddString(CPLSPrintf(
                        "SUBDATASET_%d_NAME=ZARR:\"%s\":%s:%d", iCountSubDS,
                        osFilename.c_str(), osMainArray.c_str(), i));
                    poDS->m_aosSubdatasets.AddString(CPLSPrintf(
                        "SUBDATASET_%d_DESC=Array %s at index %d of %s",
                        iCountSubDS, osMainArray.c_str(), i,
                        apoDims[0]->GetName().c_str()));
                    ++iCountSubDS;
                }
            }
            else if (nExtraDimSamples > 1)
            {
                int nDimIdxI = 0;
                int nDimIdxJ = 0;
                for (int i = 0; i < static_cast<int>(nExtraDimSamples); ++i)
                {
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s:%d:%d",
                                   iCountSubDS, osFilename.c_str(),
                                   osMainArray.c_str(), nDimIdxI, nDimIdxJ));
                    poDS->m_aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_DESC=Array %s at "
                                   "index %d of %s and %d of %s",
                                   iCountSubDS, osMainArray.c_str(), nDimIdxI,
                                   apoDims[0]->GetName().c_str(), nDimIdxJ,
                                   apoDims[1]->GetName().c_str()));
                    ++iCountSubDS;
                    ++nDimIdxJ;
                    if (nDimIdxJ == static_cast<int>(apoDims[1]->GetSize()))
                    {
                        nDimIdxJ = 0;
                        ++nDimIdxI;
                    }
                }
            }
        }

        if (aosArrays.size() >= 2)
        {
            for (size_t i = 0; i < aosArrays.size(); ++i)
            {
                poDS->m_aosSubdatasets.AddString(
                    CPLSPrintf("SUBDATASET_%d_NAME=ZARR:\"%s\":%s", iCountSubDS,
                               osFilename.c_str(), aosArrays[i].c_str()));
                poDS->m_aosSubdatasets.AddString(
                    CPLSPrintf("SUBDATASET_%d_DESC=Array %s", iCountSubDS,
                               aosArrays[i].c_str()));
                ++iCountSubDS;
            }
        }
    }

    if (poMainArray && (bMultiband || poMainArray->GetDimensionCount() <= 2))
    {
        // Pass papszOpenOptions for LOAD_EXTRA_DIM_METADATA_DELAY
        auto poNewDS =
            std::unique_ptr<GDALDataset>(poMainArray->AsClassicDataset(
                iXDim, iYDim, poRG, poOpenInfo->papszOpenOptions));
        if (!poNewDS)
            return nullptr;

        if (poMainArray->GetDimensionCount() >= 2)
        {
            // If we have 3 arrays, check that the 2 ones that are not the main
            // 2D array are indexing variables of its dimensions. If so, don't
            // expose them as subdatasets
            if (aosArrays.size() == 3)
            {
                std::vector<std::string> aosOtherArrays;
                for (size_t i = 0; i < aosArrays.size(); ++i)
                {
                    if (aosArrays[i] != osMainArray)
                    {
                        aosOtherArrays.emplace_back(aosArrays[i]);
                    }
                }
                bool bMatchFound[] = {false, false};
                for (int i = 0; i < 2; i++)
                {
                    auto poIndexingVar =
                        poMainArray->GetDimensions()[i == 0 ? iXDim : iYDim]
                            ->GetIndexingVariable();
                    if (poIndexingVar)
                    {
                        for (int j = 0; j < 2; j++)
                        {
                            if (aosOtherArrays[j] ==
                                poIndexingVar->GetFullName())
                            {
                                bMatchFound[i] = true;
                                break;
                            }
                        }
                    }
                }
                if (bMatchFound[0] && bMatchFound[1])
                {
                    poDS->m_aosSubdatasets.Clear();
                }
            }
        }
        if (!poDS->m_aosSubdatasets.empty())
        {
            poNewDS->SetMetadata(poDS->m_aosSubdatasets.List(), "SUBDATASETS");
        }
        return poNewDS.release();
    }

    return poDS.release();
}

/************************************************************************/
/*                       ZarrDatasetDelete()                            */
/************************************************************************/

static CPLErr ZarrDatasetDelete(const char *pszFilename)
{
    if (STARTS_WITH(pszFilename, "ZARR:"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Delete() only supported on ZARR connection names "
                 "not starting with the ZARR: prefix");
        return CE_Failure;
    }
    return VSIRmdirRecursive(pszFilename) == 0 ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       ZarrDatasetRename()                            */
/************************************************************************/

static CPLErr ZarrDatasetRename(const char *pszNewName, const char *pszOldName)
{
    if (STARTS_WITH(pszNewName, "ZARR:") || STARTS_WITH(pszOldName, "ZARR:"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Rename() only supported on ZARR connection names "
                 "not starting with the ZARR: prefix");
        return CE_Failure;
    }
    return VSIRename(pszOldName, pszNewName) == 0 ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       ZarrDatasetCopyFiles()                         */
/************************************************************************/

static CPLErr ZarrDatasetCopyFiles(const char *pszNewName,
                                   const char *pszOldName)
{
    if (STARTS_WITH(pszNewName, "ZARR:") || STARTS_WITH(pszOldName, "ZARR:"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CopyFiles() only supported on ZARR connection names "
                 "not starting with the ZARR: prefix");
        return CE_Failure;
    }
    // VSISync() returns true in case of success
    return VSISync((std::string(pszOldName) + '/').c_str(), pszNewName, nullptr,
                   nullptr, nullptr, nullptr)
               ? CE_None
               : CE_Failure;
}

/************************************************************************/
/*                           ZarrDriver()                               */
/************************************************************************/

class ZarrDriver final : public GDALDriver
{
    bool m_bMetadataInitialized = false;
    void InitMetadata();

  public:
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override
    {
        if (EQUAL(pszName, "COMPRESSORS") ||
            EQUAL(pszName, "BLOSC_COMPRESSORS") ||
            EQUAL(pszName, GDAL_DMD_CREATIONOPTIONLIST) ||
            EQUAL(pszName, GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST))
        {
            InitMetadata();
        }
        return GDALDriver::GetMetadataItem(pszName, pszDomain);
    }

    char **GetMetadata(const char *pszDomain) override
    {
        InitMetadata();
        return GDALDriver::GetMetadata(pszDomain);
    }
};

void ZarrDriver::InitMetadata()
{
    if (m_bMetadataInitialized)
        return;
    m_bMetadataInitialized = true;

    {
        // A bit of a hack. Normally GetMetadata() should also return it,
        // but as this is only used for tests, just make GetMetadataItem()
        // handle it
        std::string osCompressors;
        std::string osFilters;
        char **decompressors = CPLGetDecompressors();
        for (auto iter = decompressors; iter && *iter; ++iter)
        {
            const auto psCompressor = CPLGetCompressor(*iter);
            if (psCompressor)
            {
                if (psCompressor->eType == CCT_COMPRESSOR)
                {
                    if (!osCompressors.empty())
                        osCompressors += ',';
                    osCompressors += *iter;
                }
                else if (psCompressor->eType == CCT_FILTER)
                {
                    if (!osFilters.empty())
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
        CPLXMLTreeCloser oTree(
            CPLCreateXMLNode(nullptr, CXT_Element, "CreationOptionList"));
        char **compressors = CPLGetCompressors();

        auto psCompressNode =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psCompressNode, "name", "COMPRESS");
        CPLAddXMLAttributeAndValue(psCompressNode, "type", "string-select");
        CPLAddXMLAttributeAndValue(psCompressNode, "description",
                                   "Compression method");
        CPLAddXMLAttributeAndValue(psCompressNode, "default", "NONE");
        {
            auto poValueNode =
                CPLCreateXMLNode(psCompressNode, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }

        auto psFilterNode =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psFilterNode, "name", "FILTER");
        CPLAddXMLAttributeAndValue(psFilterNode, "type", "string-select");
        CPLAddXMLAttributeAndValue(psFilterNode, "description",
                                   "Filter method (only for ZARR_V2)");
        CPLAddXMLAttributeAndValue(psFilterNode, "default", "NONE");
        {
            auto poValueNode =
                CPLCreateXMLNode(psFilterNode, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }

        auto psBlockSizeNode =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psBlockSizeNode, "name", "BLOCKSIZE");
        CPLAddXMLAttributeAndValue(psBlockSizeNode, "type", "string");
        CPLAddXMLAttributeAndValue(
            psBlockSizeNode, "description",
            "Comma separated list of chunk size along each dimension");

        auto psChunkMemoryLayout =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "name",
                                   "CHUNK_MEMORY_LAYOUT");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "type",
                                   "string-select");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "description",
                                   "Whether to use C (row-major) order or F "
                                   "(column-major) order in chunks");
        CPLAddXMLAttributeAndValue(psChunkMemoryLayout, "default", "C");
        {
            auto poValueNode =
                CPLCreateXMLNode(psChunkMemoryLayout, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "C");
        }
        {
            auto poValueNode =
                CPLCreateXMLNode(psChunkMemoryLayout, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "F");
        }

        auto psStringFormat =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psStringFormat, "name", "STRING_FORMAT");
        CPLAddXMLAttributeAndValue(psStringFormat, "type", "string-select");
        CPLAddXMLAttributeAndValue(psStringFormat, "default", "STRING");
        {
            auto poValueNode =
                CPLCreateXMLNode(psStringFormat, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "STRING");
        }
        {
            auto poValueNode =
                CPLCreateXMLNode(psStringFormat, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, "UNICODE");
        }

        auto psDimSeparatorNode =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psDimSeparatorNode, "name", "DIM_SEPARATOR");
        CPLAddXMLAttributeAndValue(psDimSeparatorNode, "type", "string");
        CPLAddXMLAttributeAndValue(
            psDimSeparatorNode, "description",
            "Dimension separator in chunk filenames. Default to decimal point "
            "for ZarrV2 and slash for ZarrV3");

        for (auto iter = compressors; iter && *iter; ++iter)
        {
            const auto psCompressor = CPLGetCompressor(*iter);
            if (psCompressor)
            {
                auto poValueNode = CPLCreateXMLNode(
                    (psCompressor->eType == CCT_COMPRESSOR) ? psCompressNode
                                                            : psFilterNode,
                    CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text,
                                 CPLString(*iter).toupper().c_str());

                const char *pszOptions =
                    CSLFetchNameValue(psCompressor->papszMetadata, "OPTIONS");
                if (pszOptions)
                {
                    CPLXMLTreeCloser oTreeCompressor(
                        CPLParseXMLString(pszOptions));
                    const auto psRoot =
                        oTreeCompressor.get()
                            ? CPLGetXMLNode(oTreeCompressor.get(), "=Options")
                            : nullptr;
                    if (psRoot)
                    {
                        for (CPLXMLNode *psNode = psRoot->psChild;
                             psNode != nullptr; psNode = psNode->psNext)
                        {
                            if (psNode->eType == CXT_Element)
                            {
                                const char *pszName =
                                    CPLGetXMLValue(psNode, "name", nullptr);
                                if (pszName &&
                                    !EQUAL(pszName, "TYPESIZE")   // Blosc
                                    && !EQUAL(pszName, "HEADER")  // LZ4
                                )
                                {
                                    CPLXMLNode *psNext = psNode->psNext;
                                    psNode->psNext = nullptr;
                                    CPLXMLNode *psOption =
                                        CPLCloneXMLTree(psNode);

                                    CPLXMLNode *psName =
                                        CPLGetXMLNode(psOption, "name");
                                    if (psName &&
                                        psName->eType == CXT_Attribute &&
                                        psName->psChild &&
                                        psName->psChild->pszValue)
                                    {
                                        CPLString osNewValue(*iter);
                                        osNewValue = osNewValue.toupper();
                                        osNewValue += '_';
                                        osNewValue += psName->psChild->pszValue;
                                        CPLFree(psName->psChild->pszValue);
                                        psName->psChild->pszValue =
                                            CPLStrdup(osNewValue.c_str());
                                    }

                                    CPLXMLNode *psDescription =
                                        CPLGetXMLNode(psOption, "description");
                                    if (psDescription &&
                                        psDescription->eType == CXT_Attribute &&
                                        psDescription->psChild &&
                                        psDescription->psChild->pszValue)
                                    {
                                        std::string osNewValue(
                                            psDescription->psChild->pszValue);
                                        if (psCompressor->eType ==
                                            CCT_COMPRESSOR)
                                        {
                                            osNewValue +=
                                                ". Only used when COMPRESS=";
                                        }
                                        else
                                        {
                                            osNewValue +=
                                                ". Only used when FILTER=";
                                        }
                                        osNewValue +=
                                            CPLString(*iter).toupper();
                                        CPLFree(
                                            psDescription->psChild->pszValue);
                                        psDescription->psChild->pszValue =
                                            CPLStrdup(osNewValue.c_str());
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
            char *pszXML = CPLSerializeXMLTree(oTree.get());
            GDALDriver::SetMetadataItem(
                GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST,
                CPLString(pszXML)
                    .replaceAll("CreationOptionList",
                                "MultiDimArrayCreationOptionList")
                    .c_str());
            CPLFree(pszXML);
        }

        {
            auto psArrayNameOption =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psArrayNameOption, "name", "ARRAY_NAME");
            CPLAddXMLAttributeAndValue(psArrayNameOption, "type", "string");
            CPLAddXMLAttributeAndValue(
                psArrayNameOption, "description",
                "Array name. If not specified, deduced from the filename");

            auto psAppendSubDSOption =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "name",
                                       "APPEND_SUBDATASET");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "type", "boolean");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "description",
                                       "Whether to append the new dataset to "
                                       "an existing Zarr hierarchy");
            CPLAddXMLAttributeAndValue(psAppendSubDSOption, "default", "NO");

            auto psFormat =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psFormat, "name", "FORMAT");
            CPLAddXMLAttributeAndValue(psFormat, "type", "string-select");
            CPLAddXMLAttributeAndValue(psFormat, "default", "ZARR_V2");
            {
                auto poValueNode =
                    CPLCreateXMLNode(psFormat, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "ZARR_V2");
            }
            {
                auto poValueNode =
                    CPLCreateXMLNode(psFormat, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "ZARR_V3");
            }

            auto psCreateZMetadata =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "name",
                                       "CREATE_ZMETADATA");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "type", "boolean");
            CPLAddXMLAttributeAndValue(
                psCreateZMetadata, "description",
                "Whether to create consolidated metadata into .zmetadata (Zarr "
                "V2 only)");
            CPLAddXMLAttributeAndValue(psCreateZMetadata, "default", "YES");

            auto psSingleArrayNode =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psSingleArrayNode, "name",
                                       "SINGLE_ARRAY");
            CPLAddXMLAttributeAndValue(psSingleArrayNode, "type", "boolean");
            CPLAddXMLAttributeAndValue(
                psSingleArrayNode, "description",
                "Whether to write a multi-band dataset as a single array, or "
                "one array per band");
            CPLAddXMLAttributeAndValue(psSingleArrayNode, "default", "YES");

            auto psInterleaveNode =
                CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
            CPLAddXMLAttributeAndValue(psInterleaveNode, "name", "INTERLEAVE");
            CPLAddXMLAttributeAndValue(psInterleaveNode, "type",
                                       "string-select");
            CPLAddXMLAttributeAndValue(psInterleaveNode, "default", "BAND");
            {
                auto poValueNode =
                    CPLCreateXMLNode(psInterleaveNode, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "BAND");
            }
            {
                auto poValueNode =
                    CPLCreateXMLNode(psInterleaveNode, CXT_Element, "Value");
                CPLCreateXMLNode(poValueNode, CXT_Text, "PIXEL");
            }

            char *pszXML = CPLSerializeXMLTree(oTree.get());
            GDALDriver::SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, pszXML);
            CPLFree(pszXML);
        }
    }
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset *
ZarrDataset::CreateMultiDimensional(const char *pszFilename,
                                    CSLConstList /*papszRootGroupOptions*/,
                                    CSLConstList papszOptions)
{
    const char *pszFormat =
        CSLFetchNameValueDef(papszOptions, "FORMAT", "ZARR_V2");
    std::shared_ptr<ZarrGroupBase> poRG;
    auto poSharedResource =
        ZarrSharedResource::Create(pszFilename, /*bUpdatable=*/true);
    if (EQUAL(pszFormat, "ZARR_V3"))
    {
        poRG = ZarrV3Group::CreateOnDisk(poSharedResource, std::string(), "/",
                                         pszFilename);
    }
    else
    {
        const bool bCreateZMetadata = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "CREATE_ZMETADATA", "YES"));
        if (bCreateZMetadata)
        {
            poSharedResource->EnableZMetadata();
        }
        poRG = ZarrV2Group::CreateOnDisk(poSharedResource, std::string(), "/",
                                         pszFilename);
    }
    if (!poRG)
        return nullptr;

    auto poDS = new ZarrDataset(poRG);
    poDS->SetDescription(pszFilename);
    return poDS;
}

/************************************************************************/
/*                            Create()                                  */
/************************************************************************/

GDALDataset *ZarrDataset::Create(const char *pszName, int nXSize, int nYSize,
                                 int nBandsIn, GDALDataType eType,
                                 char **papszOptions)
{
    if (nBandsIn <= 0 || nXSize <= 0 || nYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "nBands, nXSize, nYSize should be > 0");
        return nullptr;
    }

    const bool bAppendSubDS = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO"));
    const char *pszArrayName = CSLFetchNameValue(papszOptions, "ARRAY_NAME");

    std::shared_ptr<ZarrGroupBase> poRG;
    if (bAppendSubDS)
    {
        if (pszArrayName == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ARRAY_NAME should be provided when "
                     "APPEND_SUBDATASET is set to YES");
            return nullptr;
        }
        auto poDS =
            std::unique_ptr<GDALDataset>(OpenMultidim(pszName, true, nullptr));
        if (poDS == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszName);
            return nullptr;
        }
        poRG = std::dynamic_pointer_cast<ZarrGroupBase>(poDS->GetRootGroup());
    }
    else
    {
        const char *pszFormat =
            CSLFetchNameValueDef(papszOptions, "FORMAT", "ZARR_V2");
        auto poSharedResource =
            ZarrSharedResource::Create(pszName, /*bUpdatable=*/true);
        if (EQUAL(pszFormat, "ZARR_V3"))
        {
            poRG = ZarrV3Group::CreateOnDisk(poSharedResource, std::string(),
                                             "/", pszName);
        }
        else
        {
            const bool bCreateZMetadata = CPLTestBool(
                CSLFetchNameValueDef(papszOptions, "CREATE_ZMETADATA", "YES"));
            if (bCreateZMetadata)
            {
                poSharedResource->EnableZMetadata();
            }
            poRG = ZarrV2Group::CreateOnDisk(poSharedResource, std::string(),
                                             "/", pszName);
        }
        poSharedResource->SetRootGroup(poRG);
    }
    if (!poRG)
        return nullptr;

    auto poDS = std::make_unique<ZarrDataset>(poRG);
    poDS->SetDescription(pszName);
    poDS->nRasterYSize = nYSize;
    poDS->nRasterXSize = nXSize;
    poDS->eAccess = GA_Update;

    if (bAppendSubDS)
    {
        auto aoDims = poRG->GetDimensions();
        for (const auto &poDim : aoDims)
        {
            if (poDim->GetName() == "Y" &&
                poDim->GetSize() == static_cast<GUInt64>(nYSize))
            {
                poDS->m_poDimY = poDim;
            }
            else if (poDim->GetName() == "X" &&
                     poDim->GetSize() == static_cast<GUInt64>(nXSize))
            {
                poDS->m_poDimX = poDim;
            }
        }
        if (poDS->m_poDimY == nullptr)
        {
            poDS->m_poDimY =
                poRG->CreateDimension(std::string(pszArrayName) + "_Y",
                                      std::string(), std::string(), nYSize);
        }
        if (poDS->m_poDimX == nullptr)
        {
            poDS->m_poDimX =
                poRG->CreateDimension(std::string(pszArrayName) + "_X",
                                      std::string(), std::string(), nXSize);
        }
    }
    else
    {
        poDS->m_poDimY =
            poRG->CreateDimension("Y", std::string(), std::string(), nYSize);
        poDS->m_poDimX =
            poRG->CreateDimension("X", std::string(), std::string(), nXSize);
    }
    if (poDS->m_poDimY == nullptr || poDS->m_poDimX == nullptr)
        return nullptr;

    const bool bSingleArray =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SINGLE_ARRAY", "YES"));
    const bool bBandInterleave =
        EQUAL(CSLFetchNameValueDef(papszOptions, "INTERLEAVE", "BAND"), "BAND");
    const std::shared_ptr<GDALDimension> poBandDim(
        (bSingleArray && nBandsIn > 1)
            ? poRG->CreateDimension("Band", std::string(), std::string(),
                                    nBandsIn)
            : nullptr);

    const char *pszNonNullArrayName =
        pszArrayName ? pszArrayName : CPLGetBasename(pszName);
    if (poBandDim)
    {
        const std::vector<std::shared_ptr<GDALDimension>> apoDims(
            bBandInterleave
                ? std::vector<std::shared_ptr<GDALDimension>>{poBandDim,
                                                              poDS->m_poDimY,
                                                              poDS->m_poDimX}
                : std::vector<std::shared_ptr<GDALDimension>>{
                      poDS->m_poDimY, poDS->m_poDimX, poBandDim});
        poDS->m_poSingleArray = poRG->CreateMDArray(
            pszNonNullArrayName, apoDims, GDALExtendedDataType::Create(eType),
            papszOptions);
        if (!poDS->m_poSingleArray)
            return nullptr;
        poDS->SetMetadataItem("INTERLEAVE", bBandInterleave ? "BAND" : "PIXEL",
                              "IMAGE_STRUCTURE");
        for (int i = 0; i < nBandsIn; i++)
        {
            auto poSlicedArray = poDS->m_poSingleArray->GetView(
                CPLSPrintf(bBandInterleave ? "[%d,::,::]" : "[::,::,%d]", i));
            poDS->SetBand(i + 1, new ZarrRasterBand(poSlicedArray));
        }
    }
    else
    {
        const auto apoDims = std::vector<std::shared_ptr<GDALDimension>>{
            poDS->m_poDimY, poDS->m_poDimX};
        for (int i = 0; i < nBandsIn; i++)
        {
            auto poArray = poRG->CreateMDArray(
                nBandsIn == 1  ? pszNonNullArrayName
                : pszArrayName ? CPLSPrintf("%s_band%d", pszArrayName, i + 1)
                               : CPLSPrintf("Band%d", i + 1),
                apoDims, GDALExtendedDataType::Create(eType), papszOptions);
            if (poArray == nullptr)
                return nullptr;
            poDS->SetBand(i + 1, new ZarrRasterBand(poArray));
        }
    }

    return poDS.release();
}

/************************************************************************/
/*                           ~ZarrDataset()                             */
/************************************************************************/

ZarrDataset::~ZarrDataset()
{
    ZarrDataset::FlushCache(true);
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

CPLErr ZarrDataset::FlushCache(bool bAtClosing)
{
    CPLErr eErr = GDALDataset::FlushCache(bAtClosing);
    if (m_poSingleArray)
    {
        bool bFound = false;
        for (int i = 0; i < nBands; ++i)
        {
            if (papoBands[i]->GetColorInterpretation() != GCI_Undefined)
                bFound = true;
        }
        if (bFound)
        {
            const auto oStringDT = GDALExtendedDataType::CreateString();
            auto poAttr = m_poSingleArray->GetAttribute("COLOR_INTERPRETATION");
            if (!poAttr)
                poAttr = m_poSingleArray->CreateAttribute(
                    "COLOR_INTERPRETATION", {static_cast<GUInt64>(nBands)},
                    oStringDT);
            if (poAttr)
            {
                const GUInt64 nStartIndex = 0;
                const size_t nCount = nBands;
                const GInt64 arrayStep = 1;
                const GPtrDiff_t bufferStride = 1;
                std::vector<const char *> apszValues;
                for (int i = 0; i < nBands; ++i)
                {
                    const auto eColorInterp =
                        papoBands[i]->GetColorInterpretation();
                    apszValues.push_back(
                        GDALGetColorInterpretationName(eColorInterp));
                }
                poAttr->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                              oStringDT, apszValues.data());
            }
        }
    }
    return eErr;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *ZarrDataset::GetSpatialRef() const
{
    if (nBands >= 1)
        return cpl::down_cast<ZarrRasterBand *>(papoBands[0])
            ->m_poArray->GetSpatialRef()
            .get();
    return nullptr;
}

/************************************************************************/
/*                          SetSpatialRef()                             */
/************************************************************************/

CPLErr ZarrDataset::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    for (int i = 0; i < nBands; ++i)
    {
        cpl::down_cast<ZarrRasterBand *>(papoBands[i])
            ->m_poArray->SetSpatialRef(poSRS);
    }
    return CE_None;
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr ZarrDataset::GetGeoTransform(double *padfTransform)
{
    memcpy(padfTransform, &m_adfGeoTransform[0], 6 * sizeof(double));
    return m_bHasGT ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         SetGeoTransform()                            */
/************************************************************************/

CPLErr ZarrDataset::SetGeoTransform(double *padfTransform)
{
    if (padfTransform[2] != 0 || padfTransform[4] != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geotransform with rotated terms not supported");
        return CE_Failure;
    }
    if (m_poDimX == nullptr || m_poDimY == nullptr)
        return CE_Failure;

    memcpy(&m_adfGeoTransform[0], padfTransform, 6 * sizeof(double));
    m_bHasGT = true;

    const auto oDTFloat64 = GDALExtendedDataType::Create(GDT_Float64);
    {
        auto poX = m_poRootGroup->OpenMDArray(m_poDimX->GetName());
        if (!poX)
            poX = m_poRootGroup->CreateMDArray(m_poDimX->GetName(), {m_poDimX},
                                               oDTFloat64, nullptr);
        if (!poX)
            return CE_Failure;
        m_poDimX->SetIndexingVariable(poX);
        std::vector<double> adfX;
        try
        {
            adfX.reserve(nRasterXSize);
            for (int i = 0; i < nRasterXSize; ++i)
                adfX.emplace_back(padfTransform[0] +
                                  padfTransform[1] * (i + 0.5));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when allocating X array");
            return CE_Failure;
        }
        const GUInt64 nStartIndex = 0;
        const size_t nCount = adfX.size();
        const GInt64 arrayStep = 1;
        const GPtrDiff_t bufferStride = 1;
        if (!poX->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                        poX->GetDataType(), adfX.data()))
        {
            return CE_Failure;
        }
    }

    {
        auto poY = m_poRootGroup->OpenMDArray(m_poDimY->GetName());
        if (!poY)
            poY = m_poRootGroup->CreateMDArray(m_poDimY->GetName(), {m_poDimY},
                                               oDTFloat64, nullptr);
        if (!poY)
            return CE_Failure;
        m_poDimY->SetIndexingVariable(poY);
        std::vector<double> adfY;
        try
        {
            adfY.reserve(nRasterYSize);
            for (int i = 0; i < nRasterYSize; ++i)
                adfY.emplace_back(padfTransform[3] +
                                  padfTransform[5] * (i + 0.5));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when allocating Y array");
            return CE_Failure;
        }
        const GUInt64 nStartIndex = 0;
        const size_t nCount = adfY.size();
        const GInt64 arrayStep = 1;
        const GPtrDiff_t bufferStride = 1;
        if (!poY->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                        poY->GetDataType(), adfY.data()))
        {
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          SetMetadata()                               */
/************************************************************************/

CPLErr ZarrDataset::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    if (nBands >= 1 && (pszDomain == nullptr || pszDomain[0] == '\0'))
    {
        const auto oStringDT = GDALExtendedDataType::CreateString();
        const auto bSingleArray = m_poSingleArray != nullptr;
        const int nIters = bSingleArray ? 1 : nBands;
        for (int i = 0; i < nIters; ++i)
        {
            auto *poArray = bSingleArray
                                ? m_poSingleArray.get()
                                : cpl::down_cast<ZarrRasterBand *>(papoBands[i])
                                      ->m_poArray.get();
            for (auto iter = papszMetadata; iter && *iter; ++iter)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(*iter, &pszKey);
                if (pszKey && pszValue)
                {
                    auto poAttr =
                        poArray->CreateAttribute(pszKey, {}, oStringDT);
                    if (poAttr)
                    {
                        const GUInt64 nStartIndex = 0;
                        const size_t nCount = 1;
                        const GInt64 arrayStep = 1;
                        const GPtrDiff_t bufferStride = 1;
                        poAttr->Write(&nStartIndex, &nCount, &arrayStep,
                                      &bufferStride, oStringDT, &pszValue);
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

ZarrRasterBand::ZarrRasterBand(const std::shared_ptr<GDALMDArray> &poArray)
    : m_poArray(poArray)
{
    assert(poArray->GetDimensionCount() == 2);
    eDataType = poArray->GetDataType().GetNumericDataType();
    nBlockXSize = static_cast<int>(poArray->GetBlockSize()[1]);
    nBlockYSize = static_cast<int>(poArray->GetBlockSize()[0]);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ZarrRasterBand::GetNoDataValue(int *pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsDouble(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                        GetNoDataValueAsInt64()                       */
/************************************************************************/

int64_t ZarrRasterBand::GetNoDataValueAsInt64(int *pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsInt64(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                       */
/************************************************************************/

uint64_t ZarrRasterBand::GetNoDataValueAsUInt64(int *pbHasNoData)
{
    bool bHasNodata = false;
    const auto res = m_poArray->GetNoDataValueAsUInt64(&bHasNodata);
    if (pbHasNoData)
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

double ZarrRasterBand::GetOffset(int *pbSuccess)
{
    bool bHasValue = false;
    double dfRet = m_poArray->GetOffset(&bHasValue);
    if (pbSuccess)
        *pbSuccess = bHasValue ? TRUE : FALSE;
    return dfRet;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ZarrRasterBand::SetOffset(double dfNewOffset)
{
    return m_poArray->SetOffset(dfNewOffset) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ZarrRasterBand::GetScale(int *pbSuccess)
{
    bool bHasValue = false;
    double dfRet = m_poArray->GetScale(&bHasValue);
    if (pbSuccess)
        *pbSuccess = bHasValue ? TRUE : FALSE;
    return dfRet;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ZarrRasterBand::SetScale(double dfNewScale)
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

CPLErr ZarrRasterBand::SetUnitType(const char *pszNewValue)
{
    return m_poArray->SetUnit(pszNewValue ? pszNewValue : "") ? CE_None
                                                              : CE_Failure;
}

/************************************************************************/
/*                      GetColorInterpretation()                        */
/************************************************************************/

GDALColorInterp ZarrRasterBand::GetColorInterpretation()
{
    return m_eColorInterp;
}

/************************************************************************/
/*                      SetColorInterpretation()                        */
/************************************************************************/

CPLErr ZarrRasterBand::SetColorInterpretation(GDALColorInterp eColorInterp)
{
    auto poGDS = cpl::down_cast<ZarrDataset *>(poDS);
    m_eColorInterp = eColorInterp;
    if (!poGDS->m_poSingleArray)
    {
        const auto oStringDT = GDALExtendedDataType::CreateString();
        auto poAttr = m_poArray->GetAttribute("COLOR_INTERPRETATION");
        if (poAttr && (poAttr->GetDimensionCount() != 0 ||
                       poAttr->GetDataType().GetClass() != GEDTC_STRING))
            return CE_None;
        if (!poAttr)
            poAttr = m_poArray->CreateAttribute("COLOR_INTERPRETATION", {},
                                                oStringDT);
        if (poAttr)
        {
            const GUInt64 nStartIndex = 0;
            const size_t nCount = 1;
            const GInt64 arrayStep = 1;
            const GPtrDiff_t bufferStride = 1;
            const char *pszValue = GDALGetColorInterpretationName(eColorInterp);
            poAttr->Write(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                          oStringDT, &pszValue);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                    ZarrRasterBand::IReadBlock()                      */
/************************************************************************/

CPLErr ZarrRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pData)
{

    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GUInt64 arrayStartIdx[] = {static_cast<GUInt64>(nYOff),
                               static_cast<GUInt64>(nXOff)};
    size_t count[] = {static_cast<size_t>(nReqYSize),
                      static_cast<size_t>(nReqXSize)};
    constexpr GInt64 arrayStep[] = {1, 1};
    GPtrDiff_t bufferStride[] = {nBlockXSize, 1};
    return m_poArray->Read(arrayStartIdx, count, arrayStep, bufferStride,
                           m_poArray->GetDataType(), pData)
               ? CE_None
               : CE_Failure;
}

/************************************************************************/
/*                    ZarrRasterBand::IWriteBlock()                      */
/************************************************************************/

CPLErr ZarrRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void *pData)
{
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GUInt64 arrayStartIdx[] = {static_cast<GUInt64>(nYOff),
                               static_cast<GUInt64>(nXOff)};
    size_t count[] = {static_cast<size_t>(nReqYSize),
                      static_cast<size_t>(nReqXSize)};
    constexpr GInt64 arrayStep[] = {1, 1};
    GPtrDiff_t bufferStride[] = {nBlockXSize, 1};
    return m_poArray->Write(arrayStartIdx, count, arrayStep, bufferStride,
                            m_poArray->GetDataType(), pData)
               ? CE_None
               : CE_Failure;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr ZarrRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                 int nXSize, int nYSize, void *pData,
                                 int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType, GSpacing nPixelSpaceBuf,
                                 GSpacing nLineSpaceBuf,
                                 GDALRasterIOExtraArg *psExtraArg)
{
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));
    if (nXSize == nBufXSize && nYSize == nBufYSize && nBufferDTSize > 0 &&
        (nPixelSpaceBuf % nBufferDTSize) == 0 &&
        (nLineSpaceBuf % nBufferDTSize) == 0)
    {
        GUInt64 arrayStartIdx[] = {static_cast<GUInt64>(nYOff),
                                   static_cast<GUInt64>(nXOff)};
        size_t count[] = {static_cast<size_t>(nYSize),
                          static_cast<size_t>(nXSize)};
        constexpr GInt64 arrayStep[] = {1, 1};
        GPtrDiff_t bufferStride[] = {
            static_cast<GPtrDiff_t>(nLineSpaceBuf / nBufferDTSize),
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nBufferDTSize)};

        if (eRWFlag == GF_Read)
        {
            return m_poArray->Read(
                       arrayStartIdx, count, arrayStep, bufferStride,
                       GDALExtendedDataType::Create(eBufType), pData)
                       ? CE_None
                       : CE_Failure;
        }
        else
        {
            return m_poArray->Write(
                       arrayStartIdx, count, arrayStep, bufferStride,
                       GDALExtendedDataType::Create(eBufType), pData)
                       ? CE_None
                       : CE_Failure;
        }
    }
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpaceBuf, nLineSpaceBuf, psExtraArg);
}

/************************************************************************/
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new ZarrDriver();
    ZARRDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = ZarrDataset::Open;
    poDriver->pfnCreateMultiDimensional = ZarrDataset::CreateMultiDimensional;
    poDriver->pfnCreate = ZarrDataset::Create;
    poDriver->pfnDelete = ZarrDatasetDelete;
    poDriver->pfnRename = ZarrDatasetRename;
    poDriver->pfnCopyFiles = ZarrDatasetCopyFiles;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
