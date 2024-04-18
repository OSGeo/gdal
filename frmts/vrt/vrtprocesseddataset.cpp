/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTProcessedDataset.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "vrtdataset.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

/************************************************************************/
/*                        VRTProcessedDatasetFunc                       */
/************************************************************************/

//! Structure holding information for a VRTProcessedDataset function.
struct VRTProcessedDatasetFunc
{
    //! Processing function name
    std::string osFuncName{};

    //! User data to provide to pfnInit, pfnFree, pfnProcess callbacks.
    void *pUserData = nullptr;

    //! Whether XML metadata has been specified
    bool bMetadataSpecified = false;

    //! Map of (constant argument name, constant value)
    std::map<std::string, std::string> oMapConstantArguments{};

    //! Set of builtin argument names (e.g "offset", "scale", "nodata")
    std::set<std::string> oSetBuiltinArguments{};

    //! Arguments defined in the VRT
    struct OtherArgument
    {
        std::string osType{};
        bool bRequired = false;
    };

    std::map<std::string, OtherArgument> oOtherArguments{};

    //! Requested input data type.
    GDALDataType eRequestedInputDT = GDT_Unknown;

    //! List of supported input datatypes. Empty if no restriction.
    std::vector<GDALDataType> aeSupportedInputDT{};

    //! List of supported input band counts. Empty if no restriction.
    std::vector<int> anSupportedInputBandCount{};

    //! Optional initialization function
    GDALVRTProcessedDatasetFuncInit pfnInit = nullptr;

    //! Optional free function
    GDALVRTProcessedDatasetFuncFree pfnFree = nullptr;

    //! Required processing function
    GDALVRTProcessedDatasetFuncProcess pfnProcess = nullptr;
};

/************************************************************************/
/*                      GetGlobalMapProcessedDatasetFunc()              */
/************************************************************************/

/** Return the registry of VRTProcessedDatasetFunc functions */
static std::map<std::string, VRTProcessedDatasetFunc> &
GetGlobalMapProcessedDatasetFunc()
{
    static std::map<std::string, VRTProcessedDatasetFunc> goMap;
    return goMap;
}

/************************************************************************/
/*                            Step::~Step()                             */
/************************************************************************/

/*! @cond Doxygen_Suppress */

/** Step destructor */
VRTProcessedDataset::Step::~Step()
{
    deinit();
}

/************************************************************************/
/*                           Step::deinit()                             */
/************************************************************************/

/** Free pWorkingData */
void VRTProcessedDataset::Step::deinit()
{
    if (pWorkingData)
    {
        const auto &oMapFunctions = GetGlobalMapProcessedDatasetFunc();
        const auto oIterFunc = oMapFunctions.find(osAlgorithm);
        if (oIterFunc != oMapFunctions.end())
        {
            if (oIterFunc->second.pfnFree)
            {
                oIterFunc->second.pfnFree(osAlgorithm.c_str(),
                                          oIterFunc->second.pUserData,
                                          pWorkingData);
            }
        }
        else
        {
            CPLAssert(false);
        }
        pWorkingData = nullptr;
    }
}

/************************************************************************/
/*                        Step::Step(Step&& other)                      */
/************************************************************************/

/** Move constructor */
VRTProcessedDataset::Step::Step(Step &&other)
    : osAlgorithm(std::move(other.osAlgorithm)),
      aosArguments(std::move(other.aosArguments)), eInDT(other.eInDT),
      eOutDT(other.eOutDT), nInBands(other.nInBands),
      nOutBands(other.nOutBands), adfInNoData(other.adfInNoData),
      adfOutNoData(other.adfOutNoData), pWorkingData(other.pWorkingData)
{
    other.pWorkingData = nullptr;
}

/************************************************************************/
/*                      Step operator=(Step&& other)                    */
/************************************************************************/

/** Move assignment operator */
VRTProcessedDataset::Step &VRTProcessedDataset::Step::operator=(Step &&other)
{
    if (&other != this)
    {
        deinit();
        osAlgorithm = std::move(other.osAlgorithm);
        aosArguments = std::move(other.aosArguments);
        eInDT = other.eInDT;
        eOutDT = other.eOutDT;
        nInBands = other.nInBands;
        nOutBands = other.nOutBands;
        adfInNoData = std::move(other.adfInNoData);
        adfOutNoData = std::move(other.adfOutNoData);
        std::swap(pWorkingData, other.pWorkingData);
    }
    return *this;
}

/************************************************************************/
/*                        VRTProcessedDataset()                         */
/************************************************************************/

/** Constructor */
VRTProcessedDataset::VRTProcessedDataset(int nXSize, int nYSize)
    : VRTDataset(nXSize, nYSize)
{
}

/************************************************************************/
/*                       ~VRTProcessedDataset()                         */
/************************************************************************/

VRTProcessedDataset::~VRTProcessedDataset()

{
    VRTProcessedDataset::FlushCache(true);
    VRTProcessedDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

/** Instantiate object from XML tree */
CPLErr VRTProcessedDataset::XMLInit(const CPLXMLNode *psTree,
                                    const char *pszVRTPathIn)

{
    if (Init(psTree, pszVRTPathIn, nullptr, nullptr, -1) != CE_None)
        return CE_Failure;

    const auto poSrcFirstBand = m_poSrcDS->GetRasterBand(1);
    const int nOvrCount = poSrcFirstBand->GetOverviewCount();
    for (int i = 0; i < nOvrCount; ++i)
    {
        auto poOvrDS = std::make_unique<VRTProcessedDataset>(0, 0);
        if (poOvrDS->Init(psTree, pszVRTPathIn, this, m_poSrcDS.get(), i) !=
            CE_None)
            break;
        m_apoOverviewDatasets.emplace_back(std::move(poOvrDS));
    }

    return CE_None;
}

/** Instantiate object from XML tree */
CPLErr VRTProcessedDataset::Init(const CPLXMLNode *psTree,
                                 const char *pszVRTPathIn,
                                 const VRTProcessedDataset *poParentDS,
                                 GDALDataset *poParentSrcDS, int iOvrLevel)

{
    const CPLXMLNode *psInput = CPLGetXMLNode(psTree, "Input");
    if (!psInput)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Input element missing");
        return CE_Failure;
    }

    if (pszVRTPathIn)
        m_osVRTPath = pszVRTPathIn;

    if (poParentSrcDS)
    {
        m_poSrcDS.reset(
            GDALCreateOverviewDataset(poParentSrcDS, iOvrLevel, true));
    }
    else if (const CPLXMLNode *psSourceFileNameNode =
                 CPLGetXMLNode(psInput, "SourceFilename"))
    {
        const bool bRelativeToVRT = CPL_TO_BOOL(
            atoi(CPLGetXMLValue(psSourceFileNameNode, "relativetoVRT", "0")));
        const std::string osFilename = VRTDataset::BuildSourceFilename(
            CPLGetXMLValue(psInput, "SourceFilename", ""), pszVRTPathIn,
            bRelativeToVRT);
        m_poSrcDS.reset(GDALDataset::Open(
            osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
            nullptr, nullptr));
    }
    else if (const CPLXMLNode *psVRTDataset =
                 CPLGetXMLNode(psInput, "VRTDataset"))
    {
        CPLXMLNode sVRTDatasetTmp = *psVRTDataset;
        sVRTDatasetTmp.psNext = nullptr;
        char *pszXML = CPLSerializeXMLTree(&sVRTDatasetTmp);
        m_poSrcDS.reset(VRTDataset::OpenXML(pszXML, pszVRTPathIn, GA_ReadOnly));
        CPLFree(pszXML);
    }
    else
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Input element should have a SourceFilename or VRTDataset element");
        return CE_Failure;
    }

    if (!m_poSrcDS)
        return CE_Failure;

    if (nRasterXSize == 0 && nRasterYSize == 0)
    {
        nRasterXSize = m_poSrcDS->GetRasterXSize();
        nRasterYSize = m_poSrcDS->GetRasterYSize();
    }
    else if (nRasterXSize != m_poSrcDS->GetRasterXSize() ||
             nRasterYSize != m_poSrcDS->GetRasterYSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent declared VRT dimensions with input dataset");
        return CE_Failure;
    }

    if (m_poSrcDS->GetRasterCount() == 0)
        return CE_Failure;

    // Inherit SRS from source if not explicitly defined in VRT
    if (!CPLGetXMLNode(psTree, "SRS"))
    {
        const OGRSpatialReference *poSRS = m_poSrcDS->GetSpatialRef();
        if (poSRS)
        {
            m_poSRS.reset(poSRS->Clone());
        }
    }

    // Inherit GeoTransform from source if not explicitly defined in VRT
    if (iOvrLevel < 0 && !CPLGetXMLNode(psTree, "GeoTransform"))
    {
        if (m_poSrcDS->GetGeoTransform(m_adfGeoTransform) == CE_None)
            m_bGeoTransformSet = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize blocksize before calling sub-init so that the        */
    /*      band initializers can get it from the dataset object when       */
    /*      they are created.                                               */
    /* -------------------------------------------------------------------- */

    const auto poSrcFirstBand = m_poSrcDS->GetRasterBand(1);
    poSrcFirstBand->GetBlockSize(&m_nBlockXSize, &m_nBlockYSize);
    if (const char *pszBlockXSize =
            CPLGetXMLValue(psTree, "BlockXSize", nullptr))
        m_nBlockXSize = atoi(pszBlockXSize);
    if (const char *pszBlockYSize =
            CPLGetXMLValue(psTree, "BlockYSize", nullptr))
        m_nBlockYSize = atoi(pszBlockYSize);

    // Initialize all the general VRT stuff.
    if (VRTDataset::XMLInit(psTree, pszVRTPathIn) != CE_None)
    {
        return CE_Failure;
    }

    // Use geotransform from parent for overviews
    if (iOvrLevel >= 0 && poParentDS->m_bGeoTransformSet)
    {
        m_bGeoTransformSet = true;
        m_adfGeoTransform[0] = poParentDS->m_adfGeoTransform[0];
        m_adfGeoTransform[1] = poParentDS->m_adfGeoTransform[1];
        m_adfGeoTransform[2] = poParentDS->m_adfGeoTransform[2];
        m_adfGeoTransform[3] = poParentDS->m_adfGeoTransform[3];
        m_adfGeoTransform[4] = poParentDS->m_adfGeoTransform[4];
        m_adfGeoTransform[5] = poParentDS->m_adfGeoTransform[5];

        m_adfGeoTransform[1] *=
            static_cast<double>(poParentDS->GetRasterXSize()) / nRasterXSize;
        m_adfGeoTransform[2] *=
            static_cast<double>(poParentDS->GetRasterYSize()) / nRasterYSize;
        m_adfGeoTransform[4] *=
            static_cast<double>(poParentDS->GetRasterXSize()) / nRasterXSize;
        m_adfGeoTransform[5] *=
            static_cast<double>(poParentDS->GetRasterYSize()) / nRasterYSize;
    }

    // Create bands automatically from source dataset if not explicitly defined
    // in VRT.
    if (!CPLGetXMLNode(psTree, "VRTRasterBand"))
    {
        for (int i = 0; i < m_poSrcDS->GetRasterCount(); ++i)
        {
            const auto poSrcBand = m_poSrcDS->GetRasterBand(i + 1);
            auto poBand = new VRTProcessedRasterBand(
                this, i + 1, poSrcBand->GetRasterDataType());
            poBand->CopyCommonInfoFrom(poSrcBand);
            SetBand(i + 1, poBand);
        }
    }

    const CPLXMLNode *psProcessingSteps =
        CPLGetXMLNode(psTree, "ProcessingSteps");
    if (!psProcessingSteps)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ProcessingSteps element missing");
        return CE_Failure;
    }

    const auto eInDT = poSrcFirstBand->GetRasterDataType();
    for (int i = 1; i < m_poSrcDS->GetRasterCount(); ++i)
    {
        const auto eDT = m_poSrcDS->GetRasterBand(i + 1)->GetRasterDataType();
        if (eDT != eInDT)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Not all bands of the input dataset have the same data "
                     "type. The data type of the first band will be used as "
                     "the reference one.");
            break;
        }
    }

    GDALDataType eCurrentDT = eInDT;
    int nCurrentBandCount = m_poSrcDS->GetRasterCount();

    std::vector<double> adfNoData;
    for (int i = 1; i <= nCurrentBandCount; ++i)
    {
        int bHasVal = FALSE;
        const double dfVal =
            m_poSrcDS->GetRasterBand(i)->GetNoDataValue(&bHasVal);
        adfNoData.emplace_back(
            bHasVal ? dfVal : std::numeric_limits<double>::quiet_NaN());
    }

    int nStepCount = 0;
    for (const CPLXMLNode *psStep = psProcessingSteps->psChild; psStep;
         psStep = psStep->psNext)
    {
        if (psStep->eType == CXT_Element &&
            strcmp(psStep->pszValue, "Step") == 0)
        {
            ++nStepCount;
        }
    }

    int iStep = 0;
    for (const CPLXMLNode *psStep = psProcessingSteps->psChild; psStep;
         psStep = psStep->psNext)
    {
        if (psStep->eType == CXT_Element &&
            strcmp(psStep->pszValue, "Step") == 0)
        {
            ++iStep;
            const bool bIsFinalStep = (iStep == nStepCount);
            std::vector<double> adfOutNoData;
            if (bIsFinalStep)
            {
                // Initialize adfOutNoData with nodata value of *output* bands
                // for final step
                for (int i = 1; i <= nBands; ++i)
                {
                    int bHasVal = FALSE;
                    const double dfVal =
                        GetRasterBand(i)->GetNoDataValue(&bHasVal);
                    adfOutNoData.emplace_back(
                        bHasVal ? dfVal
                                : std::numeric_limits<double>::quiet_NaN());
                }
            }
            if (!ParseStep(psStep, bIsFinalStep, eCurrentDT, nCurrentBandCount,
                           adfNoData, adfOutNoData))
                return CE_Failure;
            adfNoData = std::move(adfOutNoData);
        }
    }

    if (m_aoSteps.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "At least one step should be defined");
        return CE_Failure;
    }

    if (nCurrentBandCount != nBands)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Number of output bands of last step is not consistent with "
                 "number of VRTProcessedRasterBand's");
        return CE_Failure;
    }

    if (nBands > 1)
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    m_oXMLTree.reset(CPLCloneXMLTree(psTree));

    return CE_None;
}

/************************************************************************/
/*                            ParseStep()                               */
/************************************************************************/

/** Parse the current Step node and create a corresponding entry in m_aoSteps.
 *
 * @param psStep Step node
 * @param bIsFinalStep Whether this is the final step.
 * @param[in,out] eCurrentDT Input data type for this step.
 *                           Updated to output data type at end of method.
 * @param[in,out] nCurrentBandCount Input band count for this step.
 *                                  Updated to output band cout at end of
 *                                  method.
 * @param adfInNoData Input nodata values
 * @param[in,out] adfOutNoData Output nodata values, to be filled by this
 *                             method. When bIsFinalStep, this is also an
 *                             input parameter.
 * @return true on success.
 */
bool VRTProcessedDataset::ParseStep(const CPLXMLNode *psStep, bool bIsFinalStep,
                                    GDALDataType &eCurrentDT,
                                    int &nCurrentBandCount,
                                    std::vector<double> &adfInNoData,
                                    std::vector<double> &adfOutNoData)
{
    const char *pszStepName = CPLGetXMLValue(
        psStep, "name", CPLSPrintf("nr %d", 1 + int(m_aoSteps.size())));
    const char *pszAlgorithm = CPLGetXMLValue(psStep, "Algorithm", nullptr);
    if (!pszAlgorithm)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Step '%s' lacks a Algorithm element", pszStepName);
        return false;
    }

    const auto &oMapFunctions = GetGlobalMapProcessedDatasetFunc();
    const auto oIterFunc = oMapFunctions.find(pszAlgorithm);
    if (oIterFunc == oMapFunctions.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Step '%s' uses unregistered algorithm '%s'", pszStepName,
                 pszAlgorithm);
        return false;
    }

    const auto &oFunc = oIterFunc->second;

    if (!oFunc.aeSupportedInputDT.empty())
    {
        if (std::find(oFunc.aeSupportedInputDT.begin(),
                      oFunc.aeSupportedInputDT.end(),
                      eCurrentDT) == oFunc.aeSupportedInputDT.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Step '%s' (using algorithm '%s') does not "
                     "support input data type = '%s'",
                     pszStepName, pszAlgorithm,
                     GDALGetDataTypeName(eCurrentDT));
            return false;
        }
    }

    if (!oFunc.anSupportedInputBandCount.empty())
    {
        if (std::find(oFunc.anSupportedInputBandCount.begin(),
                      oFunc.anSupportedInputBandCount.end(),
                      nCurrentBandCount) ==
            oFunc.anSupportedInputBandCount.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Step '%s' (using algorithm '%s') does not "
                     "support input band count = %d",
                     pszStepName, pszAlgorithm, nCurrentBandCount);
            return false;
        }
    }

    Step oStep;
    oStep.osAlgorithm = pszAlgorithm;
    oStep.eInDT = oFunc.eRequestedInputDT != GDT_Unknown
                      ? oFunc.eRequestedInputDT
                      : eCurrentDT;
    oStep.nInBands = nCurrentBandCount;

    // Unless modified by pfnInit...
    oStep.eOutDT = oStep.eInDT;

    oStep.adfInNoData = adfInNoData;
    oStep.adfOutNoData = bIsFinalStep ? adfOutNoData : adfInNoData;

    // Deal with constant arguments
    for (const auto &nameValuePair : oFunc.oMapConstantArguments)
    {
        oStep.aosArguments.AddNameValue(nameValuePair.first.c_str(),
                                        nameValuePair.second.c_str());
    }

    // Deal with built-in arguments
    if (oFunc.oSetBuiltinArguments.find("nodata") !=
        oFunc.oSetBuiltinArguments.end())
    {
        int bHasVal = false;
        const auto poSrcFirstBand = m_poSrcDS->GetRasterBand(1);
        const double dfVal = poSrcFirstBand->GetNoDataValue(&bHasVal);
        if (bHasVal)
        {
            oStep.aosArguments.AddNameValue("nodata",
                                            CPLSPrintf("%.18g", dfVal));
        }
    }

    if (oFunc.oSetBuiltinArguments.find("offset_{band}") !=
        oFunc.oSetBuiltinArguments.end())
    {
        for (int i = 1; i <= m_poSrcDS->GetRasterCount(); ++i)
        {
            int bHasVal = false;
            const double dfVal = GetRasterBand(i)->GetOffset(&bHasVal);
            oStep.aosArguments.AddNameValue(
                CPLSPrintf("offset_%d", i),
                CPLSPrintf("%.18g", bHasVal ? dfVal : 0.0));
        }
    }

    if (oFunc.oSetBuiltinArguments.find("scale_{band}") !=
        oFunc.oSetBuiltinArguments.end())
    {
        for (int i = 1; i <= m_poSrcDS->GetRasterCount(); ++i)
        {
            int bHasVal = false;
            const double dfVal = GetRasterBand(i)->GetScale(&bHasVal);
            oStep.aosArguments.AddNameValue(
                CPLSPrintf("scale_%d", i),
                CPLSPrintf("%.18g", bHasVal ? dfVal : 1.0));
        }
    }

    // Parse arguments specified in VRT
    std::set<std::string> oFoundArguments;

    for (const CPLXMLNode *psStepChild = psStep->psChild; psStepChild;
         psStepChild = psStepChild->psNext)
    {
        if (psStepChild->eType == CXT_Element &&
            strcmp(psStepChild->pszValue, "Argument") == 0)
        {
            const char *pszParamName =
                CPLGetXMLValue(psStepChild, "name", nullptr);
            if (!pszParamName)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Step '%s' has a Argument without a name attribute",
                         pszStepName);
                return false;
            }
            const char *pszValue = CPLGetXMLValue(psStepChild, nullptr, "");
            auto oOtherArgIter =
                oFunc.oOtherArguments.find(CPLString(pszParamName).tolower());
            if (!oFunc.oOtherArguments.empty() &&
                oOtherArgIter == oFunc.oOtherArguments.end())
            {
                // If we got a parameter name like 'coefficients_1',
                // try to fetch the generic 'coefficients_{band}'
                std::string osParamName(pszParamName);
                const auto nPos = osParamName.rfind('_');
                if (nPos != std::string::npos)
                {
                    osParamName.resize(nPos + 1);
                    osParamName += "{band}";
                    oOtherArgIter = oFunc.oOtherArguments.find(
                        CPLString(osParamName).tolower());
                }
            }
            if (oOtherArgIter != oFunc.oOtherArguments.end())
            {
                oFoundArguments.insert(oOtherArgIter->first);

                const std::string &osType = oOtherArgIter->second.osType;
                if (osType == "boolean")
                {
                    if (!EQUAL(pszValue, "true") && !EQUAL(pszValue, "false"))
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Step '%s' has a Argument '%s' whose "
                                 "value '%s' is not a boolean",
                                 pszStepName, pszParamName, pszValue);
                        return false;
                    }
                }
                else if (osType == "integer")
                {
                    if (CPLGetValueType(pszValue) != CPL_VALUE_INTEGER)
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Step '%s' has a Argument '%s' whose "
                                 "value '%s' is not a integer",
                                 pszStepName, pszParamName, pszValue);
                        return false;
                    }
                }
                else if (osType == "double")
                {
                    const auto eType = CPLGetValueType(pszValue);
                    if (eType != CPL_VALUE_INTEGER && eType != CPL_VALUE_REAL)
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Step '%s' has a Argument '%s' whose "
                                 "value '%s' is not a double",
                                 pszStepName, pszParamName, pszValue);
                        return false;
                    }
                }
                else if (osType == "double_list")
                {
                    const CPLStringList aosTokens(
                        CSLTokenizeString2(pszValue, ",", 0));
                    for (int i = 0; i < aosTokens.size(); ++i)
                    {
                        const auto eType = CPLGetValueType(aosTokens[i]);
                        if (eType != CPL_VALUE_INTEGER &&
                            eType != CPL_VALUE_REAL)
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                     "Step '%s' has a Argument '%s' "
                                     "whose value '%s' is not a "
                                     "comma-separated list of doubles",
                                     pszStepName, pszParamName, pszValue);
                            return false;
                        }
                    }
                }
                else if (osType != "string")
                {
                    CPLDebug("VRT", "Unhandled argument type '%s'",
                             osType.c_str());
                    CPLAssert(0);
                }
            }
            else if (oFunc.bMetadataSpecified &&
                     oFunc.oSetBuiltinArguments.find(
                         CPLString(pszParamName).tolower()) ==
                         oFunc.oSetBuiltinArguments.end() &&
                     oFunc.oMapConstantArguments.find(
                         CPLString(pszParamName).tolower()) ==
                         oFunc.oMapConstantArguments.end())
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Step '%s' has a Argument '%s' which is not "
                         "supported",
                         pszStepName, pszParamName);
            }

            oStep.aosArguments.AddNameValue(pszParamName, pszValue);
        }
    }

    // Check that required arguments have been specified
    for (const auto &oIter : oFunc.oOtherArguments)
    {
        if (oIter.second.bRequired &&
            oFoundArguments.find(oIter.first) == oFoundArguments.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Step '%s' lacks required Argument '%s'", pszStepName,
                     oIter.first.c_str());
            return false;
        }
    }

    if (oFunc.pfnInit)
    {
        double *padfOutNoData = nullptr;
        if (bIsFinalStep)
        {
            oStep.nOutBands = nBands;
            padfOutNoData =
                static_cast<double *>(CPLMalloc(nBands * sizeof(double)));
            CPLAssert(adfOutNoData.size() == static_cast<size_t>(nBands));
            memcpy(padfOutNoData, adfOutNoData.data(), nBands * sizeof(double));
        }
        else
        {
            oStep.nOutBands = 0;
        }

        if (oFunc.pfnInit(pszAlgorithm, oFunc.pUserData,
                          oStep.aosArguments.List(), oStep.nInBands,
                          oStep.eInDT, adfInNoData.data(), &(oStep.nOutBands),
                          &(oStep.eOutDT), &padfOutNoData, m_osVRTPath.c_str(),
                          &(oStep.pWorkingData)) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Step '%s' (using algorithm '%s') init() function "
                     "failed",
                     pszStepName, pszAlgorithm);
            CPLFree(padfOutNoData);
            return false;
        }

        // Input nodata values may have been modified by pfnInit()
        oStep.adfInNoData = adfInNoData;

        if (padfOutNoData)
        {
            adfOutNoData =
                std::vector<double>(padfOutNoData, padfOutNoData + nBands);
        }
        else
        {
            adfOutNoData = std::vector<double>(
                oStep.nOutBands, std::numeric_limits<double>::quiet_NaN());
        }
        CPLFree(padfOutNoData);

        oStep.adfOutNoData = adfOutNoData;
    }
    else
    {
        oStep.nOutBands = oStep.nInBands;
        adfOutNoData = oStep.adfOutNoData;
    }

    eCurrentDT = oStep.eOutDT;
    nCurrentBandCount = oStep.nOutBands;

    m_aoSteps.emplace_back(std::move(oStep));

    return true;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTProcessedDataset::SerializeToXML(const char *pszVRTPathIn)

{
    CPLXMLNode *psTree = CPLCloneXMLTree(m_oXMLTree.get());
    if (psTree == nullptr)
        return psTree;

    /* -------------------------------------------------------------------- */
    /*      Remove VRTRasterBand nodes from the original tree and find the  */
    /*      last child.                                                     */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psLastChild = psTree->psChild;
    CPLXMLNode *psPrevChild = nullptr;
    while (psLastChild)
    {
        CPLXMLNode *psNextChild = psLastChild->psNext;
        if (psLastChild->eType == CXT_Element &&
            strcmp(psLastChild->pszValue, "VRTRasterBand") == 0)
        {
            if (psPrevChild)
                psPrevChild->psNext = psNextChild;
            else
                psTree->psChild = psNextChild;
            psLastChild->psNext = nullptr;
            CPLDestroyXMLNode(psLastChild);
            psLastChild = psPrevChild ? psPrevChild : psTree->psChild;
        }
        else if (!psNextChild)
        {
            break;
        }
        else
        {
            psPrevChild = psLastChild;
            psLastChild = psNextChild;
        }
    }
    CPLAssert(psLastChild);  // we have at least Input

    /* -------------------------------------------------------------------- */
    /*      Serialize bands.                                                */
    /* -------------------------------------------------------------------- */
    bool bHasWarnedAboutRAMUsage = false;
    size_t nAccRAMUsage = 0;
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        CPLXMLNode *psBandTree =
            static_cast<VRTRasterBand *>(papoBands[iBand])
                ->SerializeToXML(pszVRTPathIn, bHasWarnedAboutRAMUsage,
                                 nAccRAMUsage);

        if (psBandTree != nullptr)
        {
            psLastChild->psNext = psBandTree;
            psLastChild = psBandTree;
        }
    }

    return psTree;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *
VRTProcessedRasterBand::SerializeToXML(const char *pszVRTPathIn,
                                       bool &bHasWarnedAboutRAMUsage,
                                       size_t &nAccRAMUsage)

{
    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML(
        pszVRTPathIn, bHasWarnedAboutRAMUsage, nAccRAMUsage);

    /* -------------------------------------------------------------------- */
    /*      Set subclass.                                                   */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "subClass"),
                     CXT_Text, "VRTProcessedRasterBand");

    return psTree;
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/** Return block size */
void VRTProcessedDataset::GetBlockSize(int *pnBlockXSize,
                                       int *pnBlockYSize) const

{
    *pnBlockXSize = m_nBlockXSize;
    *pnBlockYSize = m_nBlockYSize;
}

/************************************************************************/
/*                            ProcessRegion()                           */
/************************************************************************/

/** Compute pixel values for the specified region.
 *
 * The output is stored in m_abyInput in a pixel-interleaved way.
 */
bool VRTProcessedDataset::ProcessRegion(int nXOff, int nYOff, int nBufXSize,
                                        int nBufYSize)
{

    CPLAssert(!m_aoSteps.empty());

    const int nFirstBandCount = m_aoSteps.front().nInBands;
    CPLAssert(nFirstBandCount == m_poSrcDS->GetRasterCount());
    const GDALDataType eFirstDT = m_aoSteps.front().eInDT;
    const int nFirstDTSize = GDALGetDataTypeSizeBytes(eFirstDT);
    auto &abyInput = m_abyInput;
    auto &abyOutput = m_abyOutput;
    try
    {
        abyInput.resize(static_cast<size_t>(nBufXSize) * nBufYSize *
                        nFirstBandCount * nFirstDTSize);
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        return false;
    }

    if (m_poSrcDS->RasterIO(
            GF_Read, nXOff, nYOff, nBufXSize, nBufYSize, abyInput.data(),
            nBufXSize, nBufYSize, eFirstDT, nFirstBandCount, nullptr,
            static_cast<GSpacing>(nFirstDTSize) * nFirstBandCount,
            static_cast<GSpacing>(nFirstDTSize) * nFirstBandCount * nBufXSize,
            nFirstDTSize, nullptr) != CE_None)
    {
        return false;
    }

    const double dfSrcXOff = nXOff;
    const double dfSrcYOff = nYOff;
    const double dfSrcXSize = nBufXSize;
    const double dfSrcYSize = nBufYSize;

    double adfSrcGT[6];
    if (m_poSrcDS->GetGeoTransform(adfSrcGT) != CE_None)
    {
        adfSrcGT[0] = 0;
        adfSrcGT[1] = 1;
        adfSrcGT[2] = 0;
        adfSrcGT[3] = 0;
        adfSrcGT[4] = 0;
        adfSrcGT[5] = 1;
    }

    GDALDataType eLastDT = eFirstDT;
    const auto &oMapFunctions = GetGlobalMapProcessedDatasetFunc();
    for (const auto &oStep : m_aoSteps)
    {
        const auto oIterFunc = oMapFunctions.find(oStep.osAlgorithm);
        CPLAssert(oIterFunc != oMapFunctions.end());

        // Data type adaptation
        if (eLastDT != oStep.eInDT)
        {
            try
            {
                abyOutput.resize(static_cast<size_t>(nBufXSize) * nBufYSize *
                                 oStep.nInBands *
                                 GDALGetDataTypeSizeBytes(oStep.eInDT));
            }
            catch (const std::bad_alloc &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Out of memory allocating working buffer");
                return false;
            }

            GDALCopyWords64(abyInput.data(), eLastDT,
                            GDALGetDataTypeSizeBytes(eLastDT), abyOutput.data(),
                            oStep.eInDT, GDALGetDataTypeSizeBytes(oStep.eInDT),
                            static_cast<size_t>(nBufXSize) * nBufYSize *
                                oStep.nInBands);

            std::swap(abyInput, abyOutput);
        }

        try
        {
            abyOutput.resize(static_cast<size_t>(nBufXSize) * nBufYSize *
                             oStep.nOutBands *
                             GDALGetDataTypeSizeBytes(oStep.eOutDT));
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating working buffer");
            return false;
        }

        const auto &oFunc = oIterFunc->second;
        if (oFunc.pfnProcess(
                oStep.osAlgorithm.c_str(), oFunc.pUserData, oStep.pWorkingData,
                oStep.aosArguments.List(), nBufXSize, nBufYSize,
                abyInput.data(), abyInput.size(), oStep.eInDT, oStep.nInBands,
                oStep.adfInNoData.data(), abyOutput.data(), abyOutput.size(),
                oStep.eOutDT, oStep.nOutBands, oStep.adfOutNoData.data(),
                dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize, adfSrcGT,
                m_osVRTPath.c_str(),
                /*papszExtra=*/nullptr) != CE_None)
        {
            return false;
        }

        std::swap(abyInput, abyOutput);
        eLastDT = oStep.eOutDT;
    }

    return true;
}

/************************************************************************/
/*                        VRTProcessedRasterBand()                      */
/************************************************************************/

/** Constructor */
VRTProcessedRasterBand::VRTProcessedRasterBand(VRTProcessedDataset *poDSIn,
                                               int nBandIn,
                                               GDALDataType eDataTypeIn)
{
    Initialize(poDSIn->GetRasterXSize(), poDSIn->GetRasterYSize());

    poDS = poDSIn;
    nBand = nBandIn;
    eAccess = GA_Update;
    eDataType = eDataTypeIn;

    poDSIn->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int VRTProcessedRasterBand::GetOverviewCount()
{
    auto poVRTDS = cpl::down_cast<VRTProcessedDataset *>(poDS);
    return static_cast<int>(poVRTDS->m_apoOverviewDatasets.size());
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand *VRTProcessedRasterBand::GetOverview(int iOvr)
{
    auto poVRTDS = cpl::down_cast<VRTProcessedDataset *>(poDS);
    if (iOvr < 0 ||
        iOvr >= static_cast<int>(poVRTDS->m_apoOverviewDatasets.size()))
        return nullptr;
    return poVRTDS->m_apoOverviewDatasets[iOvr]->GetRasterBand(nBand);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTProcessedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                          void *pImage)

{
    auto poVRTDS = cpl::down_cast<VRTProcessedDataset *>(poDS);

    int nBufXSize = 0;
    int nBufYSize = 0;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nBufXSize, &nBufYSize);

    const int nXPixelOff = nBlockXOff * nBlockXSize;
    const int nYPixelOff = nBlockYOff * nBlockYSize;
    if (!poVRTDS->ProcessRegion(nXPixelOff, nYPixelOff, nBufXSize, nBufYSize))
    {
        return CE_Failure;
    }

    const int nOutBands = poVRTDS->m_aoSteps.back().nOutBands;
    CPLAssert(nOutBands == poVRTDS->GetRasterCount());
    const auto eLastDT = poVRTDS->m_aoSteps.back().eOutDT;
    const int nLastDTSize = GDALGetDataTypeSizeBytes(eLastDT);
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);

    // Dispatch final output buffer to cached blocks of output bands
    for (int iDstBand = 0; iDstBand < nOutBands; ++iDstBand)
    {
        GDALRasterBlock *poBlock = nullptr;
        GByte *pDst;
        if (iDstBand + 1 == nBand)
        {
            pDst = static_cast<GByte *>(pImage);
        }
        else
        {
            auto poOtherBand = poVRTDS->papoBands[iDstBand];
            poBlock = poOtherBand->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock)
            {
                poBlock->DropLock();
                continue;
            }
            poBlock = poOtherBand->GetLockedBlockRef(
                nBlockXOff, nBlockYOff, /* bJustInitialized = */ true);
            if (!poBlock)
                continue;
            pDst = static_cast<GByte *>(poBlock->GetDataRef());
        }
        for (int iY = 0; iY < nBufYSize; ++iY)
        {
            GDALCopyWords(poVRTDS->m_abyInput.data() +
                              (iDstBand + static_cast<size_t>(iY) * nBufXSize *
                                              nOutBands) *
                                  nLastDTSize,
                          eLastDT, nLastDTSize * nOutBands,
                          pDst +
                              static_cast<size_t>(iY) * nBlockXSize * nDTSize,
                          eDataType, nDTSize, nBufXSize);
        }
        if (poBlock)
            poBlock->DropLock();
    }

    return CE_None;
}

/*! @endcond */

/************************************************************************/
/*                GDALVRTRegisterProcessedDatasetFunc()                 */
/************************************************************************/

/** Register a function to be used by VRTProcessedDataset.

 An example of content for pszXMLMetadata is:
 \verbatim
  <ProcessedDatasetFunctionArgumentsList>
     <Argument name='src_nodata' type='double' description='Override input nodata value'/>
     <Argument name='dst_nodata' type='double' description='Override output nodata value'/>
     <Argument name='replacement_nodata' description='value to substitute to a valid computed value that would be nodata' type='double'/>
     <Argument name='dst_intended_datatype' type='string' description='Intented datatype of output (which might be different than the working data type)'/>
     <Argument name='coefficients_{band}' description='Comma-separated coefficients for combining bands. First one is constant term' type='double_list' required='true'/>
  </ProcessedDatasetFunctionArgumentsList>
 \endverbatim

 @param pszFuncName Function name. Must be unique and not null.
 @param pUserData User data. May be nullptr. Must remain valid during the
                  lifetime of GDAL.
 @param pszXMLMetadata XML metadata describing the function arguments. May be
                       nullptr if there are no arguments.
 @param eRequestedInputDT If the pfnProcess callback only supports a single
                          data type, it should be specified in this parameter.
                          Otherwise set it to GDT_Unknown.
 @param paeSupportedInputDT List of supported input data types. May be nullptr
                            if all are supported or if eRequestedInputDT is
                            set to a non GDT_Unknown value.
 @param nSupportedInputDTSize Size of paeSupportedInputDT
 @param panSupportedInputBandCount List of supported band count. May be nullptr
                                   if any source band count is supported.
 @param nSupportedInputBandCountSize Size of panSupportedInputBandCount
 @param pfnInit Initialization function called when a VRTProcessedDataset
                step uses the register function. This initialization function
                will return the output data type, output band count and
                potentially initialize a working structure, typically parsing
                arguments. May be nullptr.
                If not specified, it will be assumed that the input and output
                data types are the same, and that the input number of bands
                and output number of bands are the same.
 @param pfnFree Free function that will free the working structure allocated
                by pfnInit. May be nullptr.
 @param pfnProcess Processing function called to compute pixel values. Must
                   not be nullptr.
 @param papszOptions Unused currently. Must be nullptr.
 @return CE_None in case of success, error otherwise.
 @since 3.9
 */
CPLErr GDALVRTRegisterProcessedDatasetFunc(
    const char *pszFuncName, void *pUserData, const char *pszXMLMetadata,
    GDALDataType eRequestedInputDT, const GDALDataType *paeSupportedInputDT,
    size_t nSupportedInputDTSize, const int *panSupportedInputBandCount,
    size_t nSupportedInputBandCountSize,
    GDALVRTProcessedDatasetFuncInit pfnInit,
    GDALVRTProcessedDatasetFuncFree pfnFree,
    GDALVRTProcessedDatasetFuncProcess pfnProcess,
    CPL_UNUSED CSLConstList papszOptions)
{
    if (pszFuncName == nullptr || pszFuncName[0] == '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszFuncName should be non-empty");
        return CE_Failure;
    }

    auto &oMap = GetGlobalMapProcessedDatasetFunc();
    if (oMap.find(pszFuncName) != oMap.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s already registered",
                 pszFuncName);
        return CE_Failure;
    }

    if (!pfnProcess)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "pfnProcess should not be null");
        return CE_Failure;
    }

    VRTProcessedDatasetFunc oFunc;
    oFunc.osFuncName = pszFuncName;
    oFunc.pUserData = pUserData;
    if (pszXMLMetadata)
    {
        oFunc.bMetadataSpecified = true;
        auto psTree = CPLXMLTreeCloser(CPLParseXMLString(pszXMLMetadata));
        if (!psTree)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot parse pszXMLMetadata=%s for %s", pszXMLMetadata,
                     pszFuncName);
            return CE_Failure;
        }
        const CPLXMLNode *psRoot = CPLGetXMLNode(
            psTree.get(), "=ProcessedDatasetFunctionArgumentsList");
        if (!psRoot)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No root ProcessedDatasetFunctionArgumentsList element in "
                     "pszXMLMetadata=%s for %s",
                     pszXMLMetadata, pszFuncName);
            return CE_Failure;
        }
        for (const CPLXMLNode *psIter = psRoot->psChild; psIter;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "Argument") == 0)
            {
                const char *pszName = CPLGetXMLValue(psIter, "name", nullptr);
                if (!pszName)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Missing Argument.name attribute in "
                             "pszXMLMetadata=%s for %s",
                             pszXMLMetadata, pszFuncName);
                    return CE_Failure;
                }
                const char *pszType = CPLGetXMLValue(psIter, "type", nullptr);
                if (!pszType)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Missing Argument.type attribute in "
                             "pszXMLMetadata=%s for %s",
                             pszXMLMetadata, pszFuncName);
                    return CE_Failure;
                }
                if (strcmp(pszType, "constant") == 0)
                {
                    const char *pszValue =
                        CPLGetXMLValue(psIter, "value", nullptr);
                    if (!pszValue)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Missing Argument.value attribute in "
                                 "pszXMLMetadata=%s for %s",
                                 pszXMLMetadata, pszFuncName);
                        return CE_Failure;
                    }
                    oFunc.oMapConstantArguments[CPLString(pszName).tolower()] =
                        pszValue;
                }
                else if (strcmp(pszType, "builtin") == 0)
                {
                    if (EQUAL(pszName, "nodata") ||
                        EQUAL(pszName, "offset_{band}") ||
                        EQUAL(pszName, "scale_{band}"))
                    {
                        oFunc.oSetBuiltinArguments.insert(
                            CPLString(pszName).tolower());
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Unsupported builtin parameter name %s in "
                                 "pszXMLMetadata=%s for %s. Only nodata, "
                                 "offset_{band} and scale_{band} are supported",
                                 pszName, pszXMLMetadata, pszFuncName);
                        return CE_Failure;
                    }
                }
                else if (strcmp(pszType, "boolean") == 0 ||
                         strcmp(pszType, "string") == 0 ||
                         strcmp(pszType, "integer") == 0 ||
                         strcmp(pszType, "double") == 0 ||
                         strcmp(pszType, "double_list") == 0)
                {
                    VRTProcessedDatasetFunc::OtherArgument otherArgument;
                    otherArgument.bRequired = CPLTestBool(
                        CPLGetXMLValue(psIter, "required", "false"));
                    otherArgument.osType = pszType;
                    oFunc.oOtherArguments[CPLString(pszName).tolower()] =
                        std::move(otherArgument);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unsupported type for parameter %s in "
                             "pszXMLMetadata=%s for %s. Only boolean, string, "
                             "integer, double and double_list are supported",
                             pszName, pszXMLMetadata, pszFuncName);
                    return CE_Failure;
                }
            }
        }
    }
    oFunc.eRequestedInputDT = eRequestedInputDT;
    if (nSupportedInputDTSize)
    {
        oFunc.aeSupportedInputDT.insert(
            oFunc.aeSupportedInputDT.end(), paeSupportedInputDT,
            paeSupportedInputDT + nSupportedInputDTSize);
    }
    if (nSupportedInputBandCountSize)
    {
        oFunc.anSupportedInputBandCount.insert(
            oFunc.anSupportedInputBandCount.end(), panSupportedInputBandCount,
            panSupportedInputBandCount + nSupportedInputBandCountSize);
    }
    oFunc.pfnInit = pfnInit;
    oFunc.pfnFree = pfnFree;
    oFunc.pfnProcess = pfnProcess;

    oMap[pszFuncName] = std::move(oFunc);

    return CE_None;
}
