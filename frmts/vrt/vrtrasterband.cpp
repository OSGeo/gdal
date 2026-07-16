/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "vrtdataset.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "gdal.h"
#include "gdalantirecursion.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "vrt_priv.h"

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTRasterBand                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand()
{
    VRTRasterBand::Initialize(0, 0);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void VRTRasterBand::Initialize(int nXSize, int nYSize)

{
    poDS = nullptr;
    nBand = 0;
    eAccess = GA_ReadOnly;
    eDataType = GDT_UInt8;

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    nBlockXSize = std::min(128, nXSize);
    nBlockYSize = std::min(128, nYSize);
}

/************************************************************************/
/*                           ~VRTRasterBand()                           */
/************************************************************************/

VRTRasterBand::~VRTRasterBand() = default;

/************************************************************************/
/*                         CopyCommonInfoFrom()                         */
/*                                                                      */
/*      Copy common metadata, pixel descriptions, and color             */
/*      interpretation from the provided source band.                   */
/************************************************************************/

CPLErr VRTRasterBand::CopyCommonInfoFrom(GDALRasterBand *poSrcBand)

{
    SetMetadata(poSrcBand->GetMetadata());
    const char *pszNBits =
        poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    SetMetadataItem("NBITS", pszNBits, "IMAGE_STRUCTURE");
    if (poSrcBand->GetRasterDataType() == GDT_UInt8)
    {
        poSrcBand->EnablePixelTypeSignedByteWarning(false);
        const char *pszPixelType =
            poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        poSrcBand->EnablePixelTypeSignedByteWarning(true);
        SetMetadataItem("PIXELTYPE", pszPixelType, "IMAGE_STRUCTURE");
    }
    SetColorTable(poSrcBand->GetColorTable());
    SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if (strlen(poSrcBand->GetDescription()) > 0)
        SetDescription(poSrcBand->GetDescription());

    GDALCopyNoDataValue(this, poSrcBand);
    SetOffset(poSrcBand->GetOffset());
    SetScale(poSrcBand->GetScale());
    SetCategoryNames(poSrcBand->GetCategoryNames());
    if (!EQUAL(poSrcBand->GetUnitType(), ""))
        SetUnitType(poSrcBand->GetUnitType());

    GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();
    if (poRAT != nullptr &&
        static_cast<GIntBig>(poRAT->GetColumnCount()) * poRAT->GetRowCount() <
            1024 * 1024)
    {
        SetDefaultRAT(poRAT);
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadata(CSLConstList papszMetadata,
                                  const char *pszDomain)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return GDALRasterBand::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadataItem(const char *pszName, const char *pszValue,
                                      const char *pszDomain)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    if (EQUAL(pszName, "HideNoDataValue"))
    {
        m_bHideNoDataValue = CPLTestBool(pszValue);
        return CE_None;
    }

    return GDALRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *VRTRasterBand::GetUnitType()

{
    return m_osUnitType.c_str();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetUnitType(const char *pszNewValue)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    m_osUnitType = pszNewValue ? pszNewValue : "";

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double VRTRasterBand::GetOffset(int *pbSuccess)

{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetOffset(double dfNewOffset)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    m_dfOffset = dfNewOffset;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double VRTRasterBand::GetScale(int *pbSuccess)

{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetScale(double dfNewScale)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    m_dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **VRTRasterBand::GetCategoryNames()

{
    return m_aosCategoryNames.List();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr VRTRasterBand::SetCategoryNames(char **papszNewNames)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    m_aosCategoryNames = CSLDuplicate(papszNewNames);

    return CE_None;
}

/************************************************************************/
/*                       VRTParseCategoryNames()                        */
/************************************************************************/

CPLStringList VRTParseCategoryNames(const CPLXMLNode *psCategoryNames)
{
    CPLStringList oCategoryNames;

    for (const CPLXMLNode *psEntry = psCategoryNames->psChild;
         psEntry != nullptr; psEntry = psEntry->psNext)
    {
        if (psEntry->eType != CXT_Element ||
            !EQUAL(psEntry->pszValue, "Category") ||
            (psEntry->psChild != nullptr &&
             psEntry->psChild->eType != CXT_Text))
            continue;

        oCategoryNames.AddString((psEntry->psChild) ? psEntry->psChild->pszValue
                                                    : "");
    }

    return oCategoryNames;
}

/************************************************************************/
/*                         VRTParseColorTable()                         */
/************************************************************************/

std::unique_ptr<GDALColorTable>
VRTParseColorTable(const CPLXMLNode *psColorTable)
{
    auto poColorTable = std::make_unique<GDALColorTable>();
    int iEntry = 0;

    for (const CPLXMLNode *psEntry = psColorTable->psChild; psEntry != nullptr;
         psEntry = psEntry->psNext)
    {
        if (psEntry->eType != CXT_Element || !EQUAL(psEntry->pszValue, "Entry"))
        {
            continue;
        }

        const GDALColorEntry sCEntry = {
            static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c1", "0"))),
            static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c2", "0"))),
            static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c3", "0"))),
            static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c4", "255")))};

        poColorTable->SetColorEntry(iEntry++, &sCEntry);
    }

    return poColorTable;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTRasterBand::XMLInit(const CPLXMLNode *psTree, const char *pszVRTPath,
                              VRTMapSharedResources &oMapSharedSources)

{
    /* -------------------------------------------------------------------- */
    /*      Validate a bit.                                                 */
    /* -------------------------------------------------------------------- */
    if (psTree == nullptr || psTree->eType != CXT_Element ||
        !EQUAL(psTree->pszValue, "VRTRasterBand"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid node passed to VRTRasterBand::XMLInit().");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Set the band if provided as an attribute.                       */
    /* -------------------------------------------------------------------- */
    const char *pszBand = CPLGetXMLValue(psTree, "band", nullptr);
    if (pszBand != nullptr)
    {
        int nNewBand = atoi(pszBand);
        if (nNewBand != nBand)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid band number. Got %s, expected %d. Ignoring "
                     "provided one, and using %d instead",
                     pszBand, nBand, nBand);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Set the band if provided as an attribute.                       */
    /* -------------------------------------------------------------------- */
    const char *pszDataType = CPLGetXMLValue(psTree, "dataType", nullptr);
    if (pszDataType != nullptr)
    {
        eDataType = GDALGetDataTypeByName(pszDataType);
        if (eDataType == GDT_Unknown)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid dataType = %s",
                     pszDataType);
            return CE_Failure;
        }
    }

    const char *pszBlockXSize = CPLGetXMLValue(psTree, "blockXSize", nullptr);
    if (pszBlockXSize)
    {
        int nBlockXSizeIn = atoi(pszBlockXSize);
        if (nBlockXSizeIn >= 32 && nBlockXSizeIn <= 16384)
            nBlockXSize = nBlockXSizeIn;
    }

    const char *pszBlockYSize = CPLGetXMLValue(psTree, "blockYSize", nullptr);
    if (pszBlockYSize)
    {
        int nBlockYSizeIn = atoi(pszBlockYSize);
        if (nBlockYSizeIn >= 32 && nBlockYSizeIn <= 16384)
            nBlockYSize = nBlockYSizeIn;
    }

    /* -------------------------------------------------------------------- */
    /*      Apply any band level metadata.                                  */
    /* -------------------------------------------------------------------- */
    oMDMD.XMLInit(psTree, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Collect various other items of metadata.                        */
    /* -------------------------------------------------------------------- */
    SetDescription(CPLGetXMLValue(psTree, "Description", ""));

    const char *pszNoDataValue = CPLGetXMLValue(psTree, "NoDataValue", nullptr);
    if (pszNoDataValue != nullptr)
    {
        if (eDataType == GDT_Int64)
        {
            SetNoDataValueAsInt64(static_cast<int64_t>(
                std::strtoll(pszNoDataValue, nullptr, 10)));
        }
        else if (eDataType == GDT_UInt64)
        {
            SetNoDataValueAsUInt64(static_cast<uint64_t>(
                std::strtoull(pszNoDataValue, nullptr, 10)));
        }
        else
        {
            SetNoDataValue(CPLAtofM(pszNoDataValue));
        }
    }

    if (CPLGetXMLValue(psTree, "HideNoDataValue", nullptr) != nullptr)
        m_bHideNoDataValue =
            CPLTestBool(CPLGetXMLValue(psTree, "HideNoDataValue", "0"));

    SetUnitType(CPLGetXMLValue(psTree, "UnitType", nullptr));

    SetOffset(CPLAtof(CPLGetXMLValue(psTree, "Offset", "0.0")));
    SetScale(CPLAtof(CPLGetXMLValue(psTree, "Scale", "1.0")));

    if (CPLGetXMLValue(psTree, "ColorInterp", nullptr) != nullptr)
    {
        const char *pszInterp = CPLGetXMLValue(psTree, "ColorInterp", nullptr);
        SetColorInterpretation(GDALGetColorInterpretationByName(pszInterp));
    }

    /* -------------------------------------------------------------------- */
    /*      Category names.                                                 */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psCategoryNames =
            CPLGetXMLNode(psTree, "CategoryNames"))
    {
        m_aosCategoryNames = VRTParseCategoryNames(psCategoryNames);
    }

    /* -------------------------------------------------------------------- */
    /*      Collect a color table.                                          */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psColorTable = CPLGetXMLNode(psTree, "ColorTable"))
    {
        auto poColorTable = VRTParseColorTable(psColorTable);
        if (poColorTable)
            SetColorTable(poColorTable.get());
    }

    /* -------------------------------------------------------------------- */
    /*      Raster Attribute Table                                          */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psRAT =
            CPLGetXMLNode(psTree, "GDALRasterAttributeTable"))
    {
        m_poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();
        m_poRAT->XMLInit(psRAT, "");
    }

    /* -------------------------------------------------------------------- */
    /*      Histograms                                                      */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *psHist = CPLGetXMLNode(psTree, "Histograms");
    if (psHist != nullptr)
    {
        CPLXMLNode sHistTemp = *psHist;
        sHistTemp.psNext = nullptr;
        m_psSavedHistograms.reset(CPLCloneXMLTree(&sHistTemp));
    }

    /* ==================================================================== */
    /*      Overviews                                                       */
    /* ==================================================================== */
    const CPLXMLNode *psNode = psTree->psChild;

    for (; psNode != nullptr; psNode = psNode->psNext)
    {
        if (psNode->eType != CXT_Element ||
            !EQUAL(psNode->pszValue, "Overview"))
            continue;

        /* --------------------------------------------------------------------
         */
        /*      Prepare filename. */
        /* --------------------------------------------------------------------
         */
        const CPLXMLNode *psFileNameNode =
            CPLGetXMLNode(psNode, "SourceFilename");
        const char *pszFilename =
            psFileNameNode ? CPLGetXMLValue(psFileNameNode, nullptr, nullptr)
                           : nullptr;

        if (pszFilename == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Missing <SourceFilename> element in Overview.");
            return CE_Failure;
        }

        if (STARTS_WITH_CI(pszFilename, "MEM:::") && pszVRTPath != nullptr &&
            !CPLTestBool(CPLGetConfigOption("VRT_ALLOW_MEM_DRIVER", "NO")))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "<SourceFilename> points to a MEM dataset, which is "
                     "rather suspect! "
                     "If you know what you are doing, define the "
                     "VRT_ALLOW_MEM_DRIVER configuration option to YES");
            return CE_Failure;
        }

        char *pszSrcDSName = nullptr;
        if (pszVRTPath != nullptr &&
            atoi(CPLGetXMLValue(psFileNameNode, "relativetoVRT", "0")))
        {
            pszSrcDSName = CPLStrdup(
                CPLProjectRelativeFilenameSafe(pszVRTPath, pszFilename)
                    .c_str());
        }
        else
            pszSrcDSName = CPLStrdup(pszFilename);

        /* --------------------------------------------------------------------
         */
        /*      Get the raster band. */
        /* --------------------------------------------------------------------
         */
        const int nSrcBand = atoi(CPLGetXMLValue(psNode, "SourceBand", "1"));

        m_aoOverviewInfos.resize(m_aoOverviewInfos.size() + 1);
        m_aoOverviewInfos.back().osFilename = pszSrcDSName;
        m_aoOverviewInfos.back().nBand = nSrcBand;

        CPLFree(pszSrcDSName);
    }

    /* ==================================================================== */
    /*      Mask band (specific to that raster band)                        */
    /* ==================================================================== */
    const CPLXMLNode *psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");
    if (psMaskBandNode)
        psNode = psMaskBandNode->psChild;
    else
        psNode = nullptr;
    for (; psNode != nullptr; psNode = psNode->psNext)
    {
        if (psNode->eType != CXT_Element ||
            !EQUAL(psNode->pszValue, "VRTRasterBand"))
            continue;

        if (cpl::down_cast<VRTDataset *>(poDS)->m_poMaskBand != nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Illegal mask band at raster band level when a dataset "
                     "mask band already exists.");
            return CE_Failure;
        }

        const char *pszSubclass =
            CPLGetXMLValue(psNode, "subclass", "VRTSourcedRasterBand");
        std::unique_ptr<VRTRasterBand> poBand;

        if (EQUAL(pszSubclass, "VRTSourcedRasterBand"))
            poBand = std::make_unique<VRTSourcedRasterBand>(GetDataset(), 0);
        else if (EQUAL(pszSubclass, "VRTDerivedRasterBand"))
            poBand = std::make_unique<VRTDerivedRasterBand>(GetDataset(), 0);
        else if (EQUAL(pszSubclass, "VRTRawRasterBand"))
        {
#ifdef GDAL_VRT_ENABLE_RAWRASTERBAND
            if (!VRTDataset::IsRawRasterBandEnabled())
            {
                return CE_Failure;
            }
            poBand = std::make_unique<VRTRawRasterBand>(GetDataset(), 0);
#else
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VRTRasterBand::XMLInit(): cannot instantiate "
                     "VRTRawRasterBand, because disabled in this GDAL build");
            return CE_Failure;
#endif
        }
        else if (EQUAL(pszSubclass, "VRTWarpedRasterBand"))
            poBand = std::make_unique<VRTWarpedRasterBand>(GetDataset(), 0);
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VRTRasterBand of unrecognized subclass '%s'.",
                     pszSubclass);
            return CE_Failure;
        }

        if (poBand->XMLInit(psNode, pszVRTPath, oMapSharedSources) == CE_None)
        {
            SetMaskBand(std::move(poBand));
        }
        else
        {
            return CE_Failure;
        }

        break;
    }

    return CE_None;
}

/************************************************************************/
/*                         VRTSerializeNoData()                         */
/************************************************************************/

CPLString VRTSerializeNoData(double dfVal, GDALDataType eDataType,
                             int nPrecision)
{
    if (std::isnan(dfVal))
    {
        return "nan";
    }
    else if (eDataType == GDT_Float16 && dfVal == -6.55e4)
    {
        // To avoid rounding out of the range of GFloat16
        return "-6.55e4";
    }
    else if (eDataType == GDT_Float16 && dfVal == 6.55e4)
    {
        // To avoid rounding out of the range of GFloat16
        return "6.55e4";
    }
    else if (eDataType == GDT_Float32 &&
             dfVal == -static_cast<double>(std::numeric_limits<float>::max()))
    {
        // To avoid rounding out of the range of float
        return "-3.4028234663852886e+38";
    }
    else if (eDataType == GDT_Float32 &&
             dfVal == static_cast<double>(std::numeric_limits<float>::max()))
    {
        // To avoid rounding out of the range of float
        return "3.4028234663852886e+38";
    }
    else
    {
        char szFormat[16];
        snprintf(szFormat, sizeof(szFormat), "%%.%dg", nPrecision);
        return CPLSPrintf(szFormat, dfVal);
    }
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTRasterBand::SerializeToXML(const char *pszVRTPath,
                                          bool &bHasWarnedAboutRAMUsage,
                                          size_t &nAccRAMUsage)

{
    CPLXMLNode *psTree =
        CPLCreateXMLNode(nullptr, CXT_Element, "VRTRasterBand");

    /* -------------------------------------------------------------------- */
    /*      Various kinds of metadata.                                      */
    /* -------------------------------------------------------------------- */
    CPLSetXMLValue(psTree, "#dataType",
                   GDALGetDataTypeName(GetRasterDataType()));

    if (nBand > 0)
        CPLSetXMLValue(psTree, "#band", CPLSPrintf("%d", GetBand()));

    // Do not serialize block size of VRTWarpedRasterBand since it is already
    // serialized at the dataset level.
    if (dynamic_cast<VRTWarpedRasterBand *>(this) == nullptr)
    {
        if (!VRTDataset::IsDefaultBlockSize(nBlockXSize, nRasterXSize))
        {
            CPLSetXMLValue(psTree, "#blockXSize",
                           CPLSPrintf("%d", nBlockXSize));
        }

        if (!VRTDataset::IsDefaultBlockSize(nBlockYSize, nRasterYSize))
        {
            CPLSetXMLValue(psTree, "#blockYSize",
                           CPLSPrintf("%d", nBlockYSize));
        }
    }

    CPLXMLNode *psMD = oMDMD.Serialize();
    if (psMD != nullptr)
    {
        CPLAddXMLChild(psTree, psMD);
    }

    if (strlen(GetDescription()) > 0)
        CPLSetXMLValue(psTree, "Description", GetDescription());

    if (m_bNoDataValueSet)
    {
        CPLSetXMLValue(
            psTree, "NoDataValue",
            VRTSerializeNoData(m_dfNoDataValue, eDataType, 18).c_str());
    }
    else if (m_bNoDataSetAsInt64)
    {
        CPLSetXMLValue(psTree, "NoDataValue",
                       CPLSPrintf(CPL_FRMT_GIB,
                                  static_cast<GIntBig>(m_nNoDataValueInt64)));
    }
    else if (m_bNoDataSetAsUInt64)
    {
        CPLSetXMLValue(psTree, "NoDataValue",
                       CPLSPrintf(CPL_FRMT_GUIB,
                                  static_cast<GUIntBig>(m_nNoDataValueUInt64)));
    }

    if (m_bHideNoDataValue)
        CPLSetXMLValue(psTree, "HideNoDataValue",
                       CPLSPrintf("%d", static_cast<int>(m_bHideNoDataValue)));

    if (!m_osUnitType.empty())
        CPLSetXMLValue(psTree, "UnitType", m_osUnitType.c_str());

    if (m_dfOffset != 0.0)
        CPLSetXMLValue(psTree, "Offset", CPLSPrintf("%.16g", m_dfOffset));

    if (m_dfScale != 1.0)
        CPLSetXMLValue(psTree, "Scale", CPLSPrintf("%.16g", m_dfScale));

    if (m_eColorInterp != GCI_Undefined)
        CPLSetXMLValue(psTree, "ColorInterp",
                       GDALGetColorInterpretationName(m_eColorInterp));

    /* -------------------------------------------------------------------- */
    /*      Category names.                                                 */
    /* -------------------------------------------------------------------- */
    if (!m_aosCategoryNames.empty())
    {
        CPLXMLNode *psCT_XML =
            CPLCreateXMLNode(psTree, CXT_Element, "CategoryNames");
        CPLXMLNode *psLastChild = nullptr;

        for (const char *pszName : m_aosCategoryNames)
        {
            CPLXMLNode *psNode =
                CPLCreateXMLElementAndValue(nullptr, "Category", pszName);
            if (psLastChild == nullptr)
                psCT_XML->psChild = psNode;
            else
                psLastChild->psNext = psNode;
            psLastChild = psNode;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Histograms.                                                     */
    /* -------------------------------------------------------------------- */
    if (m_psSavedHistograms != nullptr)
        CPLAddXMLChild(psTree, CPLCloneXMLTree(m_psSavedHistograms.get()));

    /* -------------------------------------------------------------------- */
    /*      Color Table.                                                    */
    /* -------------------------------------------------------------------- */
    if (m_poColorTable != nullptr)
    {
        CPLXMLNode *psCT_XML =
            CPLCreateXMLNode(psTree, CXT_Element, "ColorTable");
        CPLXMLNode *psLastChild = nullptr;

        for (int iEntry = 0; iEntry < m_poColorTable->GetColorEntryCount();
             iEntry++)
        {
            CPLXMLNode *psEntry_XML =
                CPLCreateXMLNode(nullptr, CXT_Element, "Entry");
            if (psLastChild == nullptr)
                psCT_XML->psChild = psEntry_XML;
            else
                psLastChild->psNext = psEntry_XML;
            psLastChild = psEntry_XML;

            GDALColorEntry sEntry;
            m_poColorTable->GetColorEntryAsRGB(iEntry, &sEntry);

            CPLSetXMLValue(psEntry_XML, "#c1", CPLSPrintf("%d", sEntry.c1));
            CPLSetXMLValue(psEntry_XML, "#c2", CPLSPrintf("%d", sEntry.c2));
            CPLSetXMLValue(psEntry_XML, "#c3", CPLSPrintf("%d", sEntry.c3));
            CPLSetXMLValue(psEntry_XML, "#c4", CPLSPrintf("%d", sEntry.c4));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Raster Attribute Table                                          */
    /* -------------------------------------------------------------------- */
    if (m_poRAT != nullptr)
    {
        CPLXMLNode *psSerializedRAT = m_poRAT->Serialize();
        if (psSerializedRAT != nullptr)
            CPLAddXMLChild(psTree, psSerializedRAT);
    }

    /* ==================================================================== */
    /*      Overviews                                                       */
    /* ==================================================================== */

    for (const auto &ovrInfo : m_aoOverviewInfos)
    {
        CPLXMLNode *psOVR_XML =
            CPLCreateXMLNode(psTree, CXT_Element, "Overview");

        int bRelativeToVRT = FALSE;
        const char *pszRelativePath = nullptr;
        VSIStatBufL sStat;

        if (VSIStatExL(ovrInfo.osFilename, &sStat, VSI_STAT_EXISTS_FLAG) != 0)
        {
            pszRelativePath = ovrInfo.osFilename;
            bRelativeToVRT = FALSE;
        }
        else
        {
            pszRelativePath = CPLExtractRelativePath(
                pszVRTPath, ovrInfo.osFilename, &bRelativeToVRT);
        }

        CPLSetXMLValue(psOVR_XML, "SourceFilename", pszRelativePath);

        CPLCreateXMLNode(
            CPLCreateXMLNode(CPLGetXMLNode(psOVR_XML, "SourceFilename"),
                             CXT_Attribute, "relativeToVRT"),
            CXT_Text, bRelativeToVRT ? "1" : "0");

        CPLSetXMLValue(psOVR_XML, "SourceBand",
                       CPLSPrintf("%d", ovrInfo.nBand));
    }

    /* ==================================================================== */
    /*      Mask band (specific to that raster band)                        */
    /* ==================================================================== */

    nAccRAMUsage += CPLXMLNodeGetRAMUsageEstimate(psTree);

    if (m_poMaskBand != nullptr)
    {
        CPLXMLNode *psBandTree = m_poMaskBand->SerializeToXML(
            pszVRTPath, bHasWarnedAboutRAMUsage, nAccRAMUsage);

        if (psBandTree != nullptr)
        {
            CPLXMLNode *psMaskBandElement =
                CPLCreateXMLNode(psTree, CXT_Element, "MaskBand");
            CPLAddXMLChild(psMaskBandElement, psBandTree);
        }
    }

    return psTree;
}

/************************************************************************/
/*                         ResetNoDataValues()                          */
/************************************************************************/

void VRTRasterBand::ResetNoDataValues()
{
    m_bNoDataValueSet = false;
    m_dfNoDataValue = VRT_DEFAULT_NODATA_VALUE;

    m_bNoDataSetAsInt64 = false;
    m_nNoDataValueInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;

    m_bNoDataSetAsUInt64 = false;
    m_nNoDataValueUInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr VRTRasterBand::SetNoDataValue(double dfNewValue)

{
    if (eDataType == GDT_Float32)
    {
        dfNewValue = GDALAdjustNoDataCloseToFloatMax(dfNewValue);
    }

    ResetNoDataValues();

    m_bNoDataValueSet = true;
    m_dfNoDataValue = dfNewValue;

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                    IsNoDataValueInDataTypeRange()                    */
/************************************************************************/

bool VRTRasterBand::IsNoDataValueInDataTypeRange() const
{
    if (m_bNoDataSetAsInt64)
        return eDataType == GDT_Int64;
    if (m_bNoDataSetAsUInt64)
        return eDataType == GDT_UInt64;
    if (!m_bNoDataValueSet)
        return true;
    if (!std::isfinite(m_dfNoDataValue))
        return eDataType == GDT_Float16 || eDataType == GDT_Float32 ||
               eDataType == GDT_Float64;
    GByte abyTempBuffer[2 * sizeof(double)];
    CPLAssert(GDALGetDataTypeSizeBytes(eDataType) <=
              static_cast<int>(sizeof(abyTempBuffer)));
    GDALCopyWords(&m_dfNoDataValue, GDT_Float64, 0, &abyTempBuffer[0],
                  eDataType, 0, 1);
    double dfNoDataValueAfter = 0;
    GDALCopyWords(&abyTempBuffer[0], eDataType, 0, &dfNoDataValueAfter,
                  GDT_Float64, 0, 1);
    return std::fabs(dfNoDataValueAfter - m_dfNoDataValue) < 1.0;
}

/************************************************************************/
/*                       SetNoDataValueAsInt64()                        */
/************************************************************************/

CPLErr VRTRasterBand::SetNoDataValueAsInt64(int64_t nNewValue)

{
    ResetNoDataValues();

    m_bNoDataSetAsInt64 = true;
    m_nNoDataValueInt64 = nNewValue;

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                       SetNoDataValueAsUInt64()                       */
/************************************************************************/

CPLErr VRTRasterBand::SetNoDataValueAsUInt64(uint64_t nNewValue)

{
    ResetNoDataValues();

    m_bNoDataSetAsUInt64 = true;
    m_nNoDataValueUInt64 = nNewValue;

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                         DeleteNoDataValue()                          */
/************************************************************************/

CPLErr VRTRasterBand::DeleteNoDataValue()
{
    ResetNoDataValues();

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                          UnsetNoDataValue()                          */
/************************************************************************/

CPLErr VRTRasterBand::UnsetNoDataValue()
{
    return DeleteNoDataValue();
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRTRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (m_bNoDataSetAsInt64)
    {
        if (pbSuccess)
            *pbSuccess = !m_bHideNoDataValue;
        return GDALGetNoDataValueCastToDouble(m_nNoDataValueInt64);
    }

    if (m_bNoDataSetAsUInt64)
    {
        if (pbSuccess)
            *pbSuccess = !m_bHideNoDataValue;
        return GDALGetNoDataValueCastToDouble(m_nNoDataValueUInt64);
    }

    if (pbSuccess)
        *pbSuccess = m_bNoDataValueSet && !m_bHideNoDataValue;

    return m_dfNoDataValue;
}

/************************************************************************/
/*                       GetNoDataValueAsInt64()                        */
/************************************************************************/

int64_t VRTRasterBand::GetNoDataValueAsInt64(int *pbSuccess)

{
    if (eDataType == GDT_UInt64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValueAsUInt64() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    }
    if (eDataType != GDT_Int64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValue() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    }

    if (pbSuccess)
        *pbSuccess = m_bNoDataSetAsInt64 && !m_bHideNoDataValue;

    return m_nNoDataValueInt64;
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                       */
/************************************************************************/

uint64_t VRTRasterBand::GetNoDataValueAsUInt64(int *pbSuccess)

{
    if (eDataType == GDT_Int64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValueAsInt64() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    }
    if (eDataType != GDT_UInt64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetNoDataValue() should be called instead");
        if (pbSuccess)
            *pbSuccess = FALSE;
        return GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    }

    if (pbSuccess)
        *pbSuccess = m_bNoDataSetAsUInt64 && !m_bHideNoDataValue;

    return m_nNoDataValueUInt64;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr VRTRasterBand::SetColorTable(GDALColorTable *poTableIn)

{
    if (poTableIn == nullptr)
        m_poColorTable.reset();
    else
    {
        m_poColorTable.reset(poTableIn->Clone());
        m_eColorInterp = GCI_PaletteIndex;
    }

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *VRTRasterBand::GetColorTable()

{
    return m_poColorTable.get();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr VRTRasterBand::SetColorInterpretation(GDALColorInterp eInterpIn)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    m_eColorInterp = eInterpIn;

    return CE_None;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *VRTRasterBand::GetDefaultRAT()
{
    return m_poRAT.get();
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr VRTRasterBand::SetDefaultRAT(const GDALRasterAttributeTable *poRAT)
{
    if (poRAT == nullptr)
        m_poRAT.reset();
    else
        m_poRAT.reset(poRAT->Clone());

    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp VRTRasterBand::GetColorInterpretation()

{
    return m_eColorInterp;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTRasterBand::GetHistogram(double dfMin, double dfMax, int nBuckets,
                                   GUIntBig *panHistogram,
                                   int bIncludeOutOfRange, int bApproxOK,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData)

{
    /* -------------------------------------------------------------------- */
    /*      Check if we have a matching histogram.                          */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem =
        PamFindMatchingHistogram(m_psSavedHistograms.get(), dfMin, dfMax,
                                 nBuckets, bIncludeOutOfRange, bApproxOK);
    if (psHistItem != nullptr)
    {
        GUIntBig *panTempHist = nullptr;

        if (PamParseHistogram(psHistItem, &dfMin, &dfMax, &nBuckets,
                              &panTempHist, &bIncludeOutOfRange, &bApproxOK))
        {
            memcpy(panHistogram, panTempHist, sizeof(GUIntBig) * nBuckets);
            CPLFree(panTempHist);
            return CE_None;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      We don't have an existing histogram matching the request, so    */
    /*      generate one manually.                                          */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = GDALRasterBand::GetHistogram(
        dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange, bApproxOK,
        pfnProgress, pProgressData);

    /* -------------------------------------------------------------------- */
    /*      Save an XML description of this histogram.                      */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None)
    {
        CPLXMLNode *psXMLHist =
            PamHistogramToXMLTree(dfMin, dfMax, nBuckets, panHistogram,
                                  bIncludeOutOfRange, bApproxOK);
        if (psXMLHist != nullptr)
        {
            cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

            if (m_psSavedHistograms == nullptr)
                m_psSavedHistograms.reset(
                    CPLCreateXMLNode(nullptr, CXT_Element, "Histograms"));

            CPLAddXMLChild(m_psSavedHistograms.get(), psXMLHist);
        }
    }

    return eErr;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr VRTRasterBand::SetDefaultHistogram(double dfMin, double dfMax,
                                          int nBuckets, GUIntBig *panHistogram)

{
    /* -------------------------------------------------------------------- */
    /*      Do we have a matching histogram we should replace?              */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psNode = PamFindMatchingHistogram(
        m_psSavedHistograms.get(), dfMin, dfMax, nBuckets, TRUE, TRUE);
    if (psNode != nullptr)
    {
        /* blow this one away */
        CPLRemoveXMLChild(m_psSavedHistograms.get(), psNode);
        CPLDestroyXMLNode(psNode);
    }

    /* -------------------------------------------------------------------- */
    /*      Translate into a histogram XML tree.                            */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem = PamHistogramToXMLTree(dfMin, dfMax, nBuckets,
                                                   panHistogram, TRUE, FALSE);
    if (psHistItem == nullptr)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Insert our new default histogram at the front of the            */
    /*      histogram list so that it will be the default histogram.        */
    /* -------------------------------------------------------------------- */
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    if (m_psSavedHistograms == nullptr)
        m_psSavedHistograms.reset(
            CPLCreateXMLNode(nullptr, CXT_Element, "Histograms"));

    psHistItem->psNext = m_psSavedHistograms->psChild;
    m_psSavedHistograms->psChild = psHistItem;

    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr VRTRasterBand::GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                          int *pnBuckets,
                                          GUIntBig **ppanHistogram, int bForce,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)

{
    if (m_psSavedHistograms != nullptr)
    {
        for (CPLXMLNode *psXMLHist = m_psSavedHistograms->psChild;
             psXMLHist != nullptr; psXMLHist = psXMLHist->psNext)
        {
            if (psXMLHist->eType != CXT_Element ||
                !EQUAL(psXMLHist->pszValue, "HistItem"))
                continue;

            int bIncludeOutOfRange;
            int bApprox;
            if (PamParseHistogram(psXMLHist, pdfMin, pdfMax, pnBuckets,
                                  ppanHistogram, &bIncludeOutOfRange, &bApprox))
                return CE_None;

            return CE_Failure;
        }
    }

    return GDALRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets,
                                               ppanHistogram, bForce,
                                               pfnProgress, pProgressData);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

void VRTRasterBand::GetFileList(char ***ppapszFileList, int *pnSize,
                                int *pnMaxSize, CPLHashSet *hSetFiles)
{
    for (unsigned int iOver = 0; iOver < m_aoOverviewInfos.size(); iOver++)
    {
        const CPLString &osFilename = m_aoOverviewInfos[iOver].osFilename;

        /* --------------------------------------------------------------------
         */
        /*      Is the filename even a real filesystem object? */
        /* --------------------------------------------------------------------
         */
        VSIStatBufL sStat;
        if (VSIStatL(osFilename, &sStat) != 0)
            return;

        /* --------------------------------------------------------------------
         */
        /*      Is it already in the list ? */
        /* --------------------------------------------------------------------
         */
        if (CPLHashSetLookup(hSetFiles, osFilename) != nullptr)
            return;

        /* --------------------------------------------------------------------
         */
        /*      Grow array if necessary */
        /* --------------------------------------------------------------------
         */
        if (*pnSize + 1 >= *pnMaxSize)
        {
            *pnMaxSize = 2 + 2 * (*pnMaxSize);
            *ppapszFileList = static_cast<char **>(
                CPLRealloc(*ppapszFileList, sizeof(char *) * (*pnMaxSize)));
        }

        /* --------------------------------------------------------------------
         */
        /*      Add the string to the list */
        /* --------------------------------------------------------------------
         */
        (*ppapszFileList)[*pnSize] = CPLStrdup(osFilename);
        (*ppapszFileList)[(*pnSize + 1)] = nullptr;
        CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);

        (*pnSize)++;
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int VRTRasterBand::GetOverviewCount()

{
    VRTDataset *poVRTDS = cpl::down_cast<VRTDataset *>(poDS);
    if (!poVRTDS->AreOverviewsEnabled())
        return 0;

    // First: overviews declared in <Overview> element
    if (!m_aoOverviewInfos.empty())
        return static_cast<int>(m_aoOverviewInfos.size());

    // If not found, external .ovr overviews
    const int nOverviewCount = GDALRasterBand::GetOverviewCount();
    if (nOverviewCount)
        return nOverviewCount;

    if (poVRTDS->m_apoOverviews.empty())
    {
        // If not found, implicit virtual overviews

        const std::string osFctId("VRTRasterBand::GetOverviewCount");
        GDALAntiRecursionGuard oGuard(osFctId);
        if (oGuard.GetCallDepth() >= 32)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
            return 0;
        }

        GDALAntiRecursionGuard oGuard2(oGuard, poVRTDS->GetDescription());
        if (oGuard2.GetCallDepth() >= 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
            return 0;
        }

        poVRTDS->BuildVirtualOverviews();
    }
    if (!poVRTDS->m_apoOverviews.empty() && poVRTDS->m_apoOverviews[0])
        return static_cast<int>(poVRTDS->m_apoOverviews.size());

    return 0;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTRasterBand::GetOverview(int iOverview)

{
    // First: overviews declared in <Overview> element
    if (!m_aoOverviewInfos.empty())
    {
        if (iOverview < 0 ||
            iOverview >= static_cast<int>(m_aoOverviewInfos.size()))
            return nullptr;

        if (m_aoOverviewInfos[iOverview].poBand == nullptr &&
            !m_aoOverviewInfos[iOverview].bTriedToOpen)
        {
            m_aoOverviewInfos[iOverview].bTriedToOpen = TRUE;
            CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
            GDALDataset *poSrcDS = GDALDataset::FromHandle(GDALOpenShared(
                m_aoOverviewInfos[iOverview].osFilename, GA_ReadOnly));

            if (poSrcDS == nullptr)
                return nullptr;
            if (poSrcDS == poDS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Recursive opening attempt");
                GDALClose(GDALDataset::ToHandle(poSrcDS));
                return nullptr;
            }

            m_aoOverviewInfos[iOverview].poBand =
                poSrcDS->GetRasterBand(m_aoOverviewInfos[iOverview].nBand);

            if (m_aoOverviewInfos[iOverview].poBand == nullptr)
            {
                GDALClose(GDALDataset::ToHandle(poSrcDS));
            }
        }

        return m_aoOverviewInfos[iOverview].poBand;
    }

    // If not found, external .ovr overviews
    GDALRasterBand *poRet = GDALRasterBand::GetOverview(iOverview);
    if (poRet)
        return poRet;

    // If not found, implicit virtual overviews
    VRTDataset *poVRTDS = cpl::down_cast<VRTDataset *>(poDS);
    poVRTDS->BuildVirtualOverviews();
    if (!poVRTDS->m_apoOverviews.empty() && poVRTDS->m_apoOverviews[0])
    {
        if (iOverview < 0 ||
            iOverview >= static_cast<int>(poVRTDS->m_apoOverviews.size()))
            return nullptr;

        auto poOvrBand = poVRTDS->m_apoOverviews[iOverview]->GetRasterBand(
            nBand ? nBand : 1);
        if (m_bIsMaskBand)
            return poOvrBand->GetMaskBand();
        return poOvrBand;
    }

    return nullptr;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void VRTRasterBand::SetDescription(const char *pszDescription)

{
    cpl::down_cast<VRTDataset *>(poDS)->SetNeedsFlush();

    GDALRasterBand::SetDescription(pszDescription);
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

CPLErr VRTRasterBand::CreateMaskBand(int nFlagsIn)
{
    VRTDataset *poGDS = cpl::down_cast<VRTDataset *>(poDS);

    if (poGDS->m_poMaskBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create mask band at raster band level when a dataset "
                 "mask band already exists.");
        return CE_Failure;
    }

    if (m_poMaskBand != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT band has already a mask band");
        return CE_Failure;
    }

    if ((nFlagsIn & GMF_PER_DATASET) != 0)
        return poGDS->CreateMaskBand(nFlagsIn);

    SetMaskBand(std::make_unique<VRTSourcedRasterBand>(poGDS, 0));

    return CE_None;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *VRTRasterBand::GetMaskBand()
{
    VRTDataset *poGDS = cpl::down_cast<VRTDataset *>(poDS);

    if (poGDS->m_poMaskBand)
        return poGDS->m_poMaskBand.get();
    else if (m_poMaskBand)
        return m_poMaskBand.get();
    else
        return GDALRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int VRTRasterBand::GetMaskFlags()
{
    VRTDataset *poGDS = cpl::down_cast<VRTDataset *>(poDS);

    if (poGDS->m_poMaskBand)
        return GMF_PER_DATASET;
    else if (m_poMaskBand)
        return 0;
    else
        return GDALRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                            SetMaskBand()                             */
/************************************************************************/

void VRTRasterBand::SetMaskBand(std::unique_ptr<VRTRasterBand> poMaskBand)
{
    m_poMaskBand = std::move(poMaskBand);
    m_poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                           SetIsMaskBand()                            */
/************************************************************************/

void VRTRasterBand::SetIsMaskBand()
{
    nBand = 0;
    m_bIsMaskBand = true;
}

/************************************************************************/
/*                             IsMaskBand()                             */
/************************************************************************/

bool VRTRasterBand::IsMaskBand() const
{
    return m_bIsMaskBand || m_eColorInterp == GCI_AlphaBand;
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int VRTRasterBand::CloseDependentDatasets()
{
    int ret = FALSE;
    for (auto &oOverviewInfo : m_aoOverviewInfos)
    {
        if (oOverviewInfo.CloseDataset())
        {
            ret = TRUE;
        }
    }
    return ret;
}

/*! @endcond */
