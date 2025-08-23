/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDataset
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vrtdataset.h"

#include "cpl_error_internal.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "gdal_thread_pool.h"
#include "gdal_utils.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <typeinfo>
#include "gdal_proxy.h"

/*! @cond Doxygen_Suppress */

#define VRT_PROTOCOL_PREFIX "vrt://"

constexpr int DEFAULT_BLOCK_SIZE = 128;

/************************************************************************/
/*                            VRTDataset()                              */
/************************************************************************/

VRTDataset::VRTDataset(int nXSize, int nYSize, int nBlockXSize, int nBlockYSize)
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    m_bBlockSizeSpecified = nBlockXSize > 0 && nBlockYSize > 0;
    m_nBlockXSize =
        nBlockXSize > 0 ? nBlockXSize : std::min(DEFAULT_BLOCK_SIZE, nXSize);
    m_nBlockYSize =
        nBlockYSize > 0 ? nBlockYSize : std::min(DEFAULT_BLOCK_SIZE, nYSize);

    GDALRegister_VRT();

    poDriver = static_cast<GDALDriver *>(GDALGetDriverByName("VRT"));
}

/************************************************************************/
/*                          IsDefaultBlockSize()                        */
/************************************************************************/

/* static */ bool VRTDataset::IsDefaultBlockSize(int nBlockSize, int nDimension)
{
    return nBlockSize == DEFAULT_BLOCK_SIZE ||
           (nBlockSize < DEFAULT_BLOCK_SIZE && nBlockSize == nDimension);
}

/*! @endcond */

/************************************************************************/
/*                              VRTCreate()                             */
/************************************************************************/

/**
 * @see VRTDataset::VRTDataset()
 */

VRTDatasetH CPL_STDCALL VRTCreate(int nXSize, int nYSize)

{
    auto poDS = new VRTDataset(nXSize, nYSize);
    poDS->eAccess = GA_Update;
    return poDS;
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                            ~VRTDataset()                            */
/************************************************************************/

VRTDataset::~VRTDataset()

{
    VRTDataset::FlushCache(true);
    CPLFree(m_pszVRTPath);

    for (size_t i = 0; i < m_apoOverviews.size(); i++)
        delete m_apoOverviews[i];
    for (size_t i = 0; i < m_apoOverviewsBak.size(); i++)
        delete m_apoOverviewsBak[i];
    CSLDestroy(m_papszXMLVRTMetadata);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr VRTDataset::FlushCache(bool bAtClosing)

{
    if (m_poRootGroup)
        return m_poRootGroup->Serialize() ? CE_None : CE_Failure;
    else
        return VRTFlushCacheStruct<VRTDataset>::FlushCache(*this, bAtClosing);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr VRTWarpedDataset::FlushCache(bool bAtClosing)

{
    return VRTFlushCacheStruct<VRTWarpedDataset>::FlushCache(*this, bAtClosing);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr VRTPansharpenedDataset::FlushCache(bool bAtClosing)

{
    return VRTFlushCacheStruct<VRTPansharpenedDataset>::FlushCache(*this,
                                                                   bAtClosing);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr VRTProcessedDataset::FlushCache(bool bAtClosing)

{
    return VRTFlushCacheStruct<VRTProcessedDataset>::FlushCache(*this,
                                                                bAtClosing);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

template <class T>
CPLErr VRTFlushCacheStruct<T>::FlushCache(T &obj, bool bAtClosing)
{
    CPLErr eErr = obj.GDALDataset::FlushCache(bAtClosing);

    if (!obj.m_bNeedsFlush || !obj.m_bWritable)
        return eErr;

    // We don't write to disk if there is no filename.  This is a
    // memory only dataset.
    if (strlen(obj.GetDescription()) == 0 ||
        STARTS_WITH_CI(obj.GetDescription(), "<VRTDataset"))
        return eErr;

    obj.m_bNeedsFlush = false;

    // Serialize XML representation to disk
    const std::string osVRTPath(CPLGetPathSafe(obj.GetDescription()));
    CPLXMLNode *psDSTree = obj.T::SerializeToXML(osVRTPath.c_str());
    if (!CPLSerializeXMLTreeToFile(psDSTree, obj.GetDescription()))
        eErr = CE_Failure;
    CPLDestroyXMLNode(psDSTree);
    return eErr;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTDataset::GetMetadata(const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "xml:VRT"))
    {
        /* ------------------------------------------------------------------ */
        /*      Convert tree to a single block of XML text.                   */
        /* ------------------------------------------------------------------ */
        const char *pszDescription = GetDescription();
        char *l_pszVRTPath = CPLStrdup(
            pszDescription[0] && !STARTS_WITH(pszDescription, "<VRTDataset")
                ? CPLGetPathSafe(pszDescription).c_str()
                : "");
        CPLXMLNode *psDSTree = SerializeToXML(l_pszVRTPath);
        char *pszXML = CPLSerializeXMLTree(psDSTree);

        CPLDestroyXMLNode(psDSTree);

        CPLFree(l_pszVRTPath);

        CSLDestroy(m_papszXMLVRTMetadata);
        m_papszXMLVRTMetadata =
            static_cast<char **>(CPLMalloc(2 * sizeof(char *)));
        m_papszXMLVRTMetadata[0] = pszXML;
        m_papszXMLVRTMetadata[1] = nullptr;
        return m_papszXMLVRTMetadata;
    }

    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *VRTDataset::GetMetadataItem(const char *pszName,
                                        const char *pszDomain)

{
    if (pszName && pszDomain && EQUAL(pszDomain, "__DEBUG__"))
    {
        if (EQUAL(pszName, "MULTI_THREADED_RASTERIO_LAST_USED"))
            return m_bMultiThreadedRasterIOLastUsed ? "1" : "0";
        else if (EQUAL(pszName, "CheckCompatibleForDatasetIO()"))
            return CheckCompatibleForDatasetIO() ? "1" : "0";
    }
    return GDALDataset::GetMetadataItem(pszName, pszDomain);
}

/*! @endcond */

/************************************************************************/
/*                            VRTFlushCache(bool bAtClosing) */
/************************************************************************/

/**
 * @see VRTDataset::FlushCache(bool bAtClosing)
 */

void CPL_STDCALL VRTFlushCache(VRTDatasetH hDataset)
{
    VALIDATE_POINTER0(hDataset, "VRTFlushCache");

    static_cast<VRTDataset *>(GDALDataset::FromHandle(hDataset))
        ->FlushCache(false);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDataset::SerializeToXML(const char *pszVRTPathIn)

{
    if (m_poRootGroup)
        return m_poRootGroup->SerializeToXML(pszVRTPathIn);

    /* -------------------------------------------------------------------- */
    /*      Setup root node and attributes.                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset");

    char szNumber[128] = {'\0'};
    snprintf(szNumber, sizeof(szNumber), "%d", GetRasterXSize());
    CPLSetXMLValue(psDSTree, "#rasterXSize", szNumber);

    snprintf(szNumber, sizeof(szNumber), "%d", GetRasterYSize());
    CPLSetXMLValue(psDSTree, "#rasterYSize", szNumber);

    /* -------------------------------------------------------------------- */
    /*      SRS                                                             */
    /* -------------------------------------------------------------------- */
    if (m_poSRS && !m_poSRS->IsEmpty())
    {
        char *pszWKT = nullptr;
        m_poSRS->exportToWkt(&pszWKT);
        CPLXMLNode *psSRSNode =
            CPLCreateXMLElementAndValue(psDSTree, "SRS", pszWKT);
        CPLFree(pszWKT);
        const auto &mapping = m_poSRS->GetDataAxisToSRSAxisMapping();
        CPLString osMapping;
        for (size_t i = 0; i < mapping.size(); ++i)
        {
            if (!osMapping.empty())
                osMapping += ",";
            osMapping += CPLSPrintf("%d", mapping[i]);
        }
        CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                   osMapping.c_str());
        const double dfCoordinateEpoch = m_poSRS->GetCoordinateEpoch();
        if (dfCoordinateEpoch > 0)
        {
            std::string osCoordinateEpoch = CPLSPrintf("%f", dfCoordinateEpoch);
            if (osCoordinateEpoch.find('.') != std::string::npos)
            {
                while (osCoordinateEpoch.back() == '0')
                    osCoordinateEpoch.pop_back();
            }
            CPLAddXMLAttributeAndValue(psSRSNode, "coordinateEpoch",
                                       osCoordinateEpoch.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Geotransform.                                                   */
    /* -------------------------------------------------------------------- */
    if (m_bGeoTransformSet)
    {
        CPLSetXMLValue(
            psDSTree, "GeoTransform",
            CPLSPrintf("%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                       m_gt[0], m_gt[1], m_gt[2], m_gt[3], m_gt[4], m_gt[5]));
    }

    /* -------------------------------------------------------------------- */
    /*      Metadata                                                        */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psMD = oMDMD.Serialize();
    if (psMD != nullptr)
    {
        CPLAddXMLChild(psDSTree, psMD);
    }

    /* -------------------------------------------------------------------- */
    /*      GCPs                                                            */
    /* -------------------------------------------------------------------- */
    if (!m_asGCPs.empty())
    {
        GDALSerializeGCPListToXML(psDSTree, m_asGCPs, m_poGCP_SRS.get());
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize bands.                                                */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psLastChild = psDSTree->psChild;
    for (; psLastChild != nullptr && psLastChild->psNext;
         psLastChild = psLastChild->psNext)
    {
    }
    CPLAssert(psLastChild);  // we have at least rasterXSize
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

    /* -------------------------------------------------------------------- */
    /*      Serialize dataset mask band.                                    */
    /* -------------------------------------------------------------------- */
    if (m_poMaskBand)
    {
        CPLXMLNode *psBandTree = m_poMaskBand->SerializeToXML(
            pszVRTPathIn, bHasWarnedAboutRAMUsage, nAccRAMUsage);

        if (psBandTree != nullptr)
        {
            CPLXMLNode *psMaskBandElement =
                CPLCreateXMLNode(psDSTree, CXT_Element, "MaskBand");
            CPLAddXMLChild(psMaskBandElement, psBandTree);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Overview factors.                                               */
    /* -------------------------------------------------------------------- */
    if (!m_anOverviewFactors.empty())
    {
        CPLString osOverviewList;
        for (int nOvFactor : m_anOverviewFactors)
        {
            if (!osOverviewList.empty())
                osOverviewList += " ";
            osOverviewList += CPLSPrintf("%d", nOvFactor);
        }
        CPLXMLNode *psOverviewList = CPLCreateXMLElementAndValue(
            psDSTree, "OverviewList", osOverviewList);
        if (!m_osOverviewResampling.empty())
        {
            CPLAddXMLAttributeAndValue(psOverviewList, "resampling",
                                       m_osOverviewResampling);
        }
    }

    return psDSTree;
}

/*! @endcond */
/************************************************************************/
/*                          VRTSerializeToXML()                         */
/************************************************************************/

/**
 * @see VRTDataset::SerializeToXML()
 */

CPLXMLNode *CPL_STDCALL VRTSerializeToXML(VRTDatasetH hDataset,
                                          const char *pszVRTPath)
{
    VALIDATE_POINTER1(hDataset, "VRTSerializeToXML", nullptr);

    return static_cast<VRTDataset *>(GDALDataset::FromHandle(hDataset))
        ->SerializeToXML(pszVRTPath);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                        IsRawRasterBandEnabled()                      */
/************************************************************************/

/** Return whether VRTRawRasterBand support is enabled */

/* static */
bool VRTDataset::IsRawRasterBandEnabled()
{
#ifdef GDAL_VRT_ENABLE_RAWRASTERBAND
    if (CPLTestBool(CPLGetConfigOption("GDAL_VRT_ENABLE_RAWRASTERBAND", "YES")))
    {
        return true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VRTRawRasterBand support has been disabled at run-time.");
    }
    return false;
#else
    CPLError(CE_Failure, CPLE_NotSupported,
             "VRTRawRasterBand is disabled in this GDAL build");
    return false;
#endif
}

/************************************************************************/
/*                             InitBand()                               */
/************************************************************************/

std::unique_ptr<VRTRasterBand>
VRTDataset::InitBand(const char *pszSubclass, int nBand,
                     bool bAllowPansharpenedOrProcessed)
{
    if (auto poProcessedDS = dynamic_cast<VRTProcessedDataset *>(this))
    {
        if (bAllowPansharpenedOrProcessed &&
            EQUAL(pszSubclass, "VRTProcessedRasterBand"))
        {
            return std::make_unique<VRTProcessedRasterBand>(poProcessedDS,
                                                            nBand);
        }
    }
    else if (EQUAL(pszSubclass, "VRTSourcedRasterBand"))
        return std::make_unique<VRTSourcedRasterBand>(this, nBand);
    else if (EQUAL(pszSubclass, "VRTDerivedRasterBand"))
        return std::make_unique<VRTDerivedRasterBand>(this, nBand);
    else if (EQUAL(pszSubclass, "VRTRawRasterBand"))
    {
#ifdef GDAL_VRT_ENABLE_RAWRASTERBAND
        if (!VRTDataset::IsRawRasterBandEnabled())
        {
            return nullptr;
        }
        return std::make_unique<VRTRawRasterBand>(this, nBand);
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VRTDataset::InitBand(): cannot instantiate VRTRawRasterBand, "
                 "because disabled in this GDAL build");
        return nullptr;
#endif
    }
    else if (EQUAL(pszSubclass, "VRTWarpedRasterBand") &&
             dynamic_cast<VRTWarpedDataset *>(this) != nullptr)
    {
        return std::make_unique<VRTWarpedRasterBand>(this, nBand);
    }
    else if (bAllowPansharpenedOrProcessed &&
             EQUAL(pszSubclass, "VRTPansharpenedRasterBand") &&
             dynamic_cast<VRTPansharpenedDataset *>(this) != nullptr)
    {
        return std::make_unique<VRTPansharpenedRasterBand>(this, nBand);
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "VRTRasterBand of unrecognized subclass '%s'.", pszSubclass);
    return nullptr;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDataset::XMLInit(const CPLXMLNode *psTree, const char *pszVRTPathIn)

{
    if (pszVRTPathIn != nullptr)
        m_pszVRTPath = CPLStrdup(pszVRTPathIn);

    /* -------------------------------------------------------------------- */
    /*      Check for an SRS node.                                          */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *psSRSNode = CPLGetXMLNode(psTree, "SRS");
    if (psSRSNode)
    {
        m_poSRS.reset(new OGRSpatialReference());
        m_poSRS->SetFromUserInput(
            CPLGetXMLValue(psSRSNode, nullptr, ""),
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get());
        const char *pszMapping =
            CPLGetXMLValue(psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
        if (pszMapping)
        {
            char **papszTokens =
                CSLTokenizeStringComplex(pszMapping, ",", FALSE, FALSE);
            std::vector<int> anMapping;
            for (int i = 0; papszTokens && papszTokens[i]; i++)
            {
                anMapping.push_back(atoi(papszTokens[i]));
            }
            CSLDestroy(papszTokens);
            m_poSRS->SetDataAxisToSRSAxisMapping(anMapping);
        }
        else
        {
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }

        const char *pszCoordinateEpoch =
            CPLGetXMLValue(psSRSNode, "coordinateEpoch", nullptr);
        if (pszCoordinateEpoch)
            m_poSRS->SetCoordinateEpoch(CPLAtof(pszCoordinateEpoch));
    }

    /* -------------------------------------------------------------------- */
    /*      Check for a GeoTransform node.                                  */
    /* -------------------------------------------------------------------- */
    const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
    if (strlen(pszGT) > 0)
    {
        const CPLStringList aosTokens(
            CSLTokenizeStringComplex(pszGT, ",", FALSE, FALSE));
        if (aosTokens.size() != 6)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GeoTransform node does not have expected six values.");
        }
        else
        {
            for (int iTA = 0; iTA < 6; iTA++)
                m_gt[iTA] = CPLAtof(aosTokens[iTA]);
            m_bGeoTransformSet = TRUE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psGCPList = CPLGetXMLNode(psTree, "GCPList"))
    {
        OGRSpatialReference *poSRS = nullptr;
        GDALDeserializeGCPListFromXML(psGCPList, m_asGCPs, &poSRS);
        m_poGCP_SRS.reset(poSRS);
    }

    /* -------------------------------------------------------------------- */
    /*      Apply any dataset level metadata.                               */
    /* -------------------------------------------------------------------- */
    oMDMD.XMLInit(psTree, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Create dataset mask band.                                       */
    /* -------------------------------------------------------------------- */

    /* Parse dataset mask band first */
    const CPLXMLNode *psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");

    const CPLXMLNode *psChild = nullptr;
    if (psMaskBandNode)
        psChild = psMaskBandNode->psChild;
    else
        psChild = nullptr;

    for (; psChild != nullptr; psChild = psChild->psNext)
    {
        if (psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "VRTRasterBand"))
        {
            const char *pszSubclass =
                CPLGetXMLValue(psChild, "subclass", "VRTSourcedRasterBand");

            auto poBand = InitBand(pszSubclass, 0, false);
            if (poBand != nullptr &&
                poBand->XMLInit(psChild, pszVRTPathIn, m_oMapSharedSources) ==
                    CE_None)
            {
                SetMaskBand(std::move(poBand));
                break;
            }
            else
            {
                return CE_Failure;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    int l_nBands = 0;
    for (psChild = psTree->psChild; psChild != nullptr;
         psChild = psChild->psNext)
    {
        if (psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "VRTRasterBand"))
        {
            const char *pszSubclass =
                CPLGetXMLValue(psChild, "subclass", "VRTSourcedRasterBand");
            if (dynamic_cast<VRTProcessedDataset *>(this) &&
                !EQUAL(pszSubclass, "VRTProcessedRasterBand"))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only subClass=VRTProcessedRasterBand supported");
                return CE_Failure;
            }

            if (CPLGetXMLNode(psChild, "PixelFunctionType") != nullptr &&
                !EQUAL(pszSubclass, "VRTDerivedRasterBand"))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Pixel functions may only be used with "
                         "subClass=VRTDerivedRasterBand");
                return CE_Failure;
            }

            auto poBand = InitBand(pszSubclass, l_nBands + 1, true);
            if (poBand != nullptr &&
                poBand->XMLInit(psChild, pszVRTPathIn, m_oMapSharedSources) ==
                    CE_None)
            {
                l_nBands++;
                SetBand(l_nBands, std::move(poBand));
            }
            else
            {
                return CE_Failure;
            }
        }
    }

    if (const CPLXMLNode *psGroup = CPLGetXMLNode(psTree, "Group"))
    {
        const char *pszName = CPLGetXMLValue(psGroup, "name", nullptr);
        if (pszName == nullptr || !EQUAL(pszName, "/"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing name or not equal to '/'");
            return CE_Failure;
        }

        m_poRootGroup = VRTGroup::Create(std::string(), "/");
        m_poRootGroup->SetIsRootGroup();
        if (!m_poRootGroup->XMLInit(m_poRootGroup, m_poRootGroup, psGroup,
                                    pszVRTPathIn))
        {
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create virtual overviews.                                       */
    /* -------------------------------------------------------------------- */
    const char *pszSubClass = CPLGetXMLValue(psTree, "subClass", "");
    if (EQUAL(pszSubClass, ""))
    {
        m_aosOverviewList =
            CSLTokenizeString(CPLGetXMLValue(psTree, "OverviewList", ""));
        m_osOverviewResampling =
            CPLGetXMLValue(psTree, "OverviewList.resampling", "");
    }

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int VRTDataset::GetGCPCount()

{
    return static_cast<int>(m_asGCPs.size());
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *VRTDataset::GetGCPs()

{
    return gdal::GCP::c_ptr(m_asGCPs);
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr VRTDataset::SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                           const OGRSpatialReference *poGCP_SRS)

{
    m_poGCP_SRS.reset(poGCP_SRS ? poGCP_SRS->Clone() : nullptr);
    m_asGCPs = gdal::GCP::fromC(pasGCPListIn, nGCPCountIn);

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr VRTDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    m_poSRS.reset(poSRS ? poSRS->Clone() : nullptr);

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::SetGeoTransform(const GDALGeoTransform &gt)

{
    m_gt = gt;
    m_bGeoTransformSet = TRUE;

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::GetGeoTransform(GDALGeoTransform &gt) const

{
    gt = m_gt;

    return m_bGeoTransformSet ? CE_None : CE_Failure;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTDataset::SetMetadata(char **papszMetadata, const char *pszDomain)

{
    SetNeedsFlush();

    return GDALDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTDataset::SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain)

{
    SetNeedsFlush();

    return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VRTDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes > 20 &&
        strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
               "<VRTDataset") != nullptr)
        return TRUE;

    if (strstr(poOpenInfo->pszFilename, "<VRTDataset") != nullptr)
        return TRUE;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, VRT_PROTOCOL_PREFIX))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Does this appear to be a virtual dataset definition XML         */
    /*      file?                                                           */
    /* -------------------------------------------------------------------- */
    if (!Identify(poOpenInfo))
        return nullptr;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, VRT_PROTOCOL_PREFIX))
        return OpenVRTProtocol(poOpenInfo->pszFilename);

    /* -------------------------------------------------------------------- */
    /*      Try to read the whole file into memory.                         */
    /* -------------------------------------------------------------------- */
    char *pszXML = nullptr;
    VSILFILE *fp = poOpenInfo->fpL;

    char *pszVRTPath = nullptr;
    if (fp != nullptr)
    {
        poOpenInfo->fpL = nullptr;

        GByte *pabyOut = nullptr;
        if (!VSIIngestFile(fp, poOpenInfo->pszFilename, &pabyOut, nullptr,
                           INT_MAX - 1))
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return nullptr;
        }
        pszXML = reinterpret_cast<char *>(pabyOut);

        char *pszCurDir = CPLGetCurrentDir();
        std::string currentVrtFilename =
            CPLProjectRelativeFilenameSafe(pszCurDir, poOpenInfo->pszFilename);
        CPLString osInitialCurrentVrtFilename(currentVrtFilename);
        CPLFree(pszCurDir);

#if !defined(_WIN32)
        char filenameBuffer[2048];

        while (true)
        {
            VSIStatBuf statBuffer;
            int lstatCode = lstat(currentVrtFilename.c_str(), &statBuffer);
            if (lstatCode == -1)
            {
                if (errno == ENOENT)
                {
                    // File could be a virtual file, let later checks handle it.
                    break;
                }
                else
                {
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                    CPLFree(pszXML);
                    CPLError(CE_Failure, CPLE_FileIO, "Failed to lstat %s: %s",
                             currentVrtFilename.c_str(), VSIStrerror(errno));
                    return nullptr;
                }
            }

            if (!VSI_ISLNK(statBuffer.st_mode))
            {
                break;
            }

            const int bufferSize = static_cast<int>(
                readlink(currentVrtFilename.c_str(), filenameBuffer,
                         sizeof(filenameBuffer)));
            if (bufferSize != -1)
            {
                filenameBuffer[std::min(
                    bufferSize, static_cast<int>(sizeof(filenameBuffer)) - 1)] =
                    0;
                // The filename in filenameBuffer might be a relative path
                // from the linkfile resolve it before looping
                currentVrtFilename = CPLProjectRelativeFilenameSafe(
                    CPLGetDirnameSafe(currentVrtFilename.c_str()).c_str(),
                    filenameBuffer);
            }
            else
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                CPLFree(pszXML);
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to read filename from symlink %s: %s",
                         currentVrtFilename.c_str(), VSIStrerror(errno));
                return nullptr;
            }
        }
#endif  // !defined(__WIN32)

        if (osInitialCurrentVrtFilename == currentVrtFilename)
            pszVRTPath =
                CPLStrdup(CPLGetPathSafe(poOpenInfo->pszFilename).c_str());
        else
            pszVRTPath =
                CPLStrdup(CPLGetPathSafe(currentVrtFilename.c_str()).c_str());

        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    }
    /* -------------------------------------------------------------------- */
    /*      Or use the filename as the XML input.                           */
    /* -------------------------------------------------------------------- */
    else
    {
        pszXML = CPLStrdup(poOpenInfo->pszFilename);
    }

    if (CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ROOT_PATH") != nullptr)
    {
        CPLFree(pszVRTPath);
        pszVRTPath = CPLStrdup(
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ROOT_PATH"));
    }

    /* -------------------------------------------------------------------- */
    /*      Turn the XML representation into a VRTDataset.                  */
    /* -------------------------------------------------------------------- */
    auto poDS = OpenXML(pszXML, pszVRTPath, poOpenInfo->eAccess);

    if (poDS != nullptr)
        poDS->m_bNeedsFlush = false;

    if (poDS != nullptr)
    {
        if (poDS->GetRasterCount() == 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 &&
            strstr(pszXML, "VRTPansharpenedDataset") == nullptr)
        {
            poDS.reset();
        }
        else if (poDS->GetRootGroup() == nullptr &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
        {
            poDS.reset();
        }
    }

    CPLFree(pszXML);
    CPLFree(pszVRTPath);

    /* -------------------------------------------------------------------- */
    /*      Initialize info for later overview discovery.                   */
    /* -------------------------------------------------------------------- */

    if (poDS != nullptr)
    {
        if (fp != nullptr)
        {
            poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);
            if (poOpenInfo->AreSiblingFilesLoaded())
                poDS->oOvManager.TransferSiblingFiles(
                    poOpenInfo->StealSiblingFiles());
        }

        // Creating virtual overviews, but only if there is no higher priority
        // overview source, ie. a Overview element at VRT band level,
        // or external .vrt.ovr
        if (!poDS->m_aosOverviewList.empty())
        {
            if (poDS->nBands > 0)
            {
                auto poBand = dynamic_cast<VRTRasterBand *>(poDS->papoBands[0]);
                if (poBand && !poBand->m_aoOverviewInfos.empty())
                {
                    poDS->m_aosOverviewList.Clear();
                    CPLDebug("VRT",
                             "Ignoring virtual overviews of OverviewList "
                             "because Overview element is present on VRT band");
                }
                else if (poBand &&
                         poBand->GDALRasterBand::GetOverviewCount() > 0)
                {
                    poDS->m_aosOverviewList.Clear();
                    CPLDebug("VRT",
                             "Ignoring virtual overviews of OverviewList "
                             "because external .vrt.ovr is available");
                }
            }
            for (int iOverview = 0; iOverview < poDS->m_aosOverviewList.size();
                 iOverview++)
            {
                const int nOvFactor = atoi(poDS->m_aosOverviewList[iOverview]);
                if (nOvFactor <= 1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid overview factor");
                    return nullptr;
                }

                poDS->AddVirtualOverview(
                    nOvFactor, poDS->m_osOverviewResampling.empty()
                                   ? "nearest"
                                   : poDS->m_osOverviewResampling.c_str());
            }
            poDS->m_aosOverviewList.Clear();
        }

        if (poDS->eAccess == GA_Update && poDS->m_poRootGroup &&
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "<VRT"))
        {
            poDS->m_poRootGroup->SetFilename(poOpenInfo->pszFilename);
        }
    }

    return poDS.release();
}

/************************************************************************/
/*                         OpenVRTProtocol()                            */
/*                                                                      */
/*      Create an open VRTDataset from a vrt:// string.                 */
/************************************************************************/

GDALDataset *VRTDataset::OpenVRTProtocol(const char *pszSpec)

{
    CPLAssert(STARTS_WITH_CI(pszSpec, VRT_PROTOCOL_PREFIX));
    CPLString osFilename(pszSpec + strlen(VRT_PROTOCOL_PREFIX));
    const auto nPosQuotationMark = osFilename.find('?');
    CPLString osQueryString;
    if (nPosQuotationMark != std::string::npos)
    {
        osQueryString = osFilename.substr(nPosQuotationMark + 1);
        osFilename.resize(nPosQuotationMark);
    }

    // Parse query string, get args required for initial Open()
    const CPLStringList aosTokens(CSLTokenizeString2(osQueryString, "&", 0));
    CPLStringList aosAllowedDrivers;
    CPLStringList aosOpenOptions;

    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(
             aosTokens, /* bReturnNullKeyIfNotNameValue = */ true))
    {
        if (!pszKey)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid option specification: %s\n"
                     "must be in the form 'key=value'",
                     pszValue);
            return nullptr;
        }
        else if (EQUAL(pszKey, "if"))
        {
            if (!aosAllowedDrivers.empty())
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'if' option should be specified once, use commas "
                         "to input multiple values.");
                return nullptr;
            }
            aosAllowedDrivers = CSLTokenizeString2(pszValue, ",", 0);
        }
        else if (EQUAL(pszKey, "oo"))
        {
            if (!aosOpenOptions.empty())
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'oo' option should be specified once, use commas "
                         "to input multiple values.");
                return nullptr;
            }
            aosOpenOptions = CSLTokenizeString2(pszValue, ",", 0);
        }
    }

    // We don't open in GDAL_OF_SHARED mode to avoid issues when we open a
    // http://.jp2 file with the JP2OpenJPEG driver through the HTTP driver,
    // which returns a /vsimem/ file
    auto poSrcDS = std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>(
        GDALDataset::Open(osFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                          aosAllowedDrivers.List(), aosOpenOptions.List(),
                          nullptr));
    if (poSrcDS == nullptr)
    {
        return nullptr;
    }

    bool bFound_transpose = false;
    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(aosTokens))
    {
        if (EQUAL(pszKey, "transpose"))
        {
            bFound_transpose = true;
            const CPLStringList aosTransposeTokens(
                CSLTokenizeString2(pszValue, ":", 0));
            if (aosTransposeTokens.size() != 2)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid transpose option: %s", pszValue);
                return nullptr;
            }
            const CPLStringList aosTransposeIndex(
                CSLTokenizeString2(aosTransposeTokens[1], ",", 0));
            // fail if not two values
            if (aosTransposeIndex.size() != 2)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid transpose option: %s", pszValue);
                return nullptr;
            }
            int index_x = atoi(aosTransposeIndex[0]);
            int index_y = atoi(aosTransposeIndex[1]);

            auto poMDimDS = std::unique_ptr<GDALDataset>(
                GDALDataset::Open(osFilename, GDAL_OF_MULTIDIM_RASTER));
            if (!poMDimDS)
                return nullptr;
            auto poMdimGroup = poMDimDS->GetRootGroup();
            if (!poMdimGroup)
            {
                return nullptr;
            }
            auto poArray =
                poMdimGroup->OpenMDArrayFromFullname(aosTransposeTokens[0]);
            if (!poArray)
            {
                return nullptr;
            }

            auto poClassicDS = poArray->AsClassicDataset(index_x, index_y);

            if (!poClassicDS)
                return nullptr;
            poSrcDS =
                std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>(
                    poClassicDS);
        }
    }
    // scan for sd_name/sd in tokens, close the source dataset and reopen if found/valid
    bool bFound_subdataset = false;
    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(aosTokens))
    {
        if (EQUAL(pszKey, "sd_name"))
        {
            if (bFound_transpose)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'sd_name' is mutually exclusive with option "
                         "'transpose'");
                return nullptr;
            }
            if (bFound_subdataset)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'sd_name' is mutually exclusive with option "
                         "'sd'");
                return nullptr;
            }
            char **papszSubdatasets = poSrcDS->GetMetadata("SUBDATASETS");
            int nSubdatasets = CSLCount(papszSubdatasets);

            if (nSubdatasets > 0)
            {
                bool bFound = false;
                for (int j = 0; j < nSubdatasets && papszSubdatasets[j]; j += 2)
                {
                    const char *pszEqual = strchr(papszSubdatasets[j], '=');
                    if (!pszEqual)
                    {
                        CPLError(CE_Failure, CPLE_IllegalArg,
                                 "'sd_name:' failed to obtain "
                                 "subdataset string ");
                        return nullptr;
                    }
                    const char *pszSubdatasetSource = pszEqual + 1;
                    GDALSubdatasetInfoH info =
                        GDALGetSubdatasetInfo(pszSubdatasetSource);
                    char *component =
                        info ? GDALSubdatasetInfoGetSubdatasetComponent(info)
                             : nullptr;

                    bFound = component && EQUAL(pszValue, component);
                    bFound_subdataset = true;
                    CPLFree(component);
                    GDALDestroySubdatasetInfo(info);
                    if (bFound)
                    {
                        poSrcDS.reset(GDALDataset::Open(
                            pszSubdatasetSource,
                            GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                            aosAllowedDrivers.List(), aosOpenOptions.List(),
                            nullptr));
                        if (poSrcDS == nullptr)
                        {
                            return nullptr;
                        }

                        break;
                    }
                }

                if (!bFound)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "'sd_name' option should be be a valid "
                             "subdataset component name");
                    return nullptr;
                }
            }
        }

        if (EQUAL(pszKey, "sd"))
        {
            if (bFound_transpose)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'sd' is mutually exclusive with option "
                         "'transpose'");
                return nullptr;
            }
            if (bFound_subdataset)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "'sd' is mutually exclusive with option "
                         "'sd_name'");
                return nullptr;
            }
            CSLConstList papszSubdatasets = poSrcDS->GetMetadata("SUBDATASETS");
            int nSubdatasets = CSLCount(papszSubdatasets);

            if (nSubdatasets > 0)
            {
                int iSubdataset = atoi(pszValue);
                if (iSubdataset < 1 || iSubdataset > (nSubdatasets) / 2)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "'sd' option should indicate a valid "
                             "subdataset component number (starting with 1)");
                    return nullptr;
                }
                const std::string osSubdatasetSource(
                    strstr(papszSubdatasets[(iSubdataset - 1) * 2], "=") + 1);
                if (osSubdatasetSource.empty())
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "'sd:' failed to obtain subdataset "
                             "string ");
                    return nullptr;
                }

                poSrcDS.reset(GDALDataset::Open(
                    osSubdatasetSource.c_str(),
                    GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    aosAllowedDrivers.List(), aosOpenOptions.List(), nullptr));
                if (poSrcDS == nullptr)
                {
                    return nullptr;
                }
                bFound_subdataset = true;
            }
        }
    }

    std::vector<int> anBands;

    CPLStringList argv;
    argv.AddString("-of");
    argv.AddString("VRT");

    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(aosTokens))
    {
        if (EQUAL(pszKey, "bands"))
        {
            const CPLStringList aosBands(CSLTokenizeString2(pszValue, ",", 0));
            for (int j = 0; j < aosBands.size(); j++)
            {
                if (EQUAL(aosBands[j], "mask"))
                {
                    anBands.push_back(0);
                }
                else
                {
                    const int nBand = atoi(aosBands[j]);
                    if (nBand <= 0 || nBand > poSrcDS->GetRasterCount())
                    {
                        CPLError(CE_Failure, CPLE_IllegalArg,
                                 "Invalid band number: %s", aosBands[j]);
                        return nullptr;
                    }
                    anBands.push_back(nBand);
                }
            }

            for (const int nBand : anBands)
            {
                argv.AddString("-b");
                argv.AddString(nBand == 0 ? "mask" : CPLSPrintf("%d", nBand));
            }
        }

        else if (EQUAL(pszKey, "a_nodata"))
        {
            argv.AddString("-a_nodata");
            argv.AddString(pszValue);
        }

        else if (EQUAL(pszKey, "a_srs"))
        {
            argv.AddString("-a_srs");
            argv.AddString(pszValue);
        }

        else if (EQUAL(pszKey, "a_ullr"))
        {
            // Parse the limits
            const CPLStringList aosUllr(CSLTokenizeString2(pszValue, ",", 0));
            // fail if not four values
            if (aosUllr.size() != 4)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid a_ullr option: %s", pszValue);
                return nullptr;
            }

            argv.AddString("-a_ullr");
            argv.AddString(aosUllr[0]);
            argv.AddString(aosUllr[1]);
            argv.AddString(aosUllr[2]);
            argv.AddString(aosUllr[3]);
        }

        else if (EQUAL(pszKey, "ovr"))
        {
            argv.AddString("-ovr");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "expand"))
        {
            argv.AddString("-expand");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "a_scale"))
        {
            argv.AddString("-a_scale");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "a_offset"))
        {
            argv.AddString("-a_offset");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "ot"))
        {
            argv.AddString("-ot");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "gcp"))
        {
            const CPLStringList aosGCP(CSLTokenizeString2(pszValue, ",", 0));

            if (aosGCP.size() < 4 || aosGCP.size() > 5)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid value for GCP: %s\n  need 4, or 5 "
                         "numbers, comma separated: "
                         "'gcp=<pixel>,<line>,<easting>,<northing>[,<"
                         "elevation>]'",
                         pszValue);
                return nullptr;
            }
            argv.AddString("-gcp");
            for (int j = 0; j < aosGCP.size(); j++)
            {
                argv.AddString(aosGCP[j]);
            }
        }
        else if (EQUAL(pszKey, "scale") || STARTS_WITH_CI(pszKey, "scale_"))
        {
            const CPLStringList aosScaleParams(
                CSLTokenizeString2(pszValue, ",", 0));

            if (!(aosScaleParams.size() == 2) &&
                !(aosScaleParams.size() == 4) && !(aosScaleParams.size() == 1))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid value for scale, (or scale_bn): "
                         "%s\n  need 'scale=true', or 2 or 4 "
                         "numbers, comma separated: "
                         "'scale=src_min,src_max[,dst_min,dst_max]' or "
                         "'scale_bn=src_min,src_max[,dst_min,dst_max]'",
                         pszValue);
                return nullptr;
            }

            // -scale because scale=true or scale=min,max or scale=min,max,dstmin,dstmax
            if (aosScaleParams.size() == 1 && CPLTestBool(aosScaleParams[0]))
            {
                argv.AddString(CPLSPrintf("-%s", pszKey));
            }
            // add remaining params (length 2 or 4)
            if (aosScaleParams.size() > 1)
            {
                argv.AddString(CPLSPrintf("-%s", pszKey));
                for (int j = 0; j < aosScaleParams.size(); j++)
                {
                    argv.AddString(aosScaleParams[j]);
                }
            }
        }
        else if (EQUAL(pszKey, "exponent") ||
                 STARTS_WITH_CI(pszKey, "exponent_"))
        {
            argv.AddString(CPLSPrintf("-%s", pszKey));
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "outsize"))
        {
            const CPLStringList aosOutSize(
                CSLTokenizeString2(pszValue, ",", 0));
            if (aosOutSize.size() != 2)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid outsize option: %s, must be two"
                         "values separated by comma pixel,line or two "
                         "fraction values with percent symbol",
                         pszValue);
                return nullptr;
            }
            argv.AddString("-outsize");
            argv.AddString(aosOutSize[0]);
            argv.AddString(aosOutSize[1]);
        }
        else if (EQUAL(pszKey, "projwin"))
        {
            // Parse the limits
            const CPLStringList aosProjWin(
                CSLTokenizeString2(pszValue, ",", 0));
            // fail if not four values
            if (aosProjWin.size() != 4)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid projwin option: %s", pszValue);
                return nullptr;
            }

            argv.AddString("-projwin");
            argv.AddString(aosProjWin[0]);
            argv.AddString(aosProjWin[1]);
            argv.AddString(aosProjWin[2]);
            argv.AddString(aosProjWin[3]);
        }
        else if (EQUAL(pszKey, "projwin_srs"))
        {
            argv.AddString("-projwin_srs");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "tr"))
        {
            const CPLStringList aosTargetResolution(
                CSLTokenizeString2(pszValue, ",", 0));
            if (aosTargetResolution.size() != 2)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid tr option: %s, must be two "
                         "values separated by comma xres,yres",
                         pszValue);
                return nullptr;
            }
            argv.AddString("-tr");
            argv.AddString(aosTargetResolution[0]);
            argv.AddString(aosTargetResolution[1]);
        }
        else if (EQUAL(pszKey, "r"))
        {
            argv.AddString("-r");
            argv.AddString(pszValue);
        }

        else if (EQUAL(pszKey, "srcwin"))
        {
            // Parse the limits
            const CPLStringList aosSrcWin(CSLTokenizeString2(pszValue, ",", 0));
            // fail if not four values
            if (aosSrcWin.size() != 4)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid srcwin option: %s, must be four "
                         "values separated by comma xoff,yoff,xsize,ysize",
                         pszValue);
                return nullptr;
            }

            argv.AddString("-srcwin");
            argv.AddString(aosSrcWin[0]);
            argv.AddString(aosSrcWin[1]);
            argv.AddString(aosSrcWin[2]);
            argv.AddString(aosSrcWin[3]);
        }

        else if (EQUAL(pszKey, "a_gt"))
        {
            // Parse the limits
            const CPLStringList aosAGeoTransform(
                CSLTokenizeString2(pszValue, ",", 0));
            // fail if not six values
            if (aosAGeoTransform.size() != 6)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid a_gt option: %s",
                         pszValue);
                return nullptr;
            }

            argv.AddString("-a_gt");
            argv.AddString(aosAGeoTransform[0]);
            argv.AddString(aosAGeoTransform[1]);
            argv.AddString(aosAGeoTransform[2]);
            argv.AddString(aosAGeoTransform[3]);
            argv.AddString(aosAGeoTransform[4]);
            argv.AddString(aosAGeoTransform[5]);
        }
        else if (EQUAL(pszKey, "oo"))
        {
            // do nothing, we passed this in earlier
        }
        else if (EQUAL(pszKey, "if"))
        {
            // do nothing, we passed this in earlier
        }
        else if (EQUAL(pszKey, "sd_name"))
        {
            // do nothing, we passed this in earlier
        }
        else if (EQUAL(pszKey, "sd"))
        {
            // do nothing, we passed this in earlier
        }
        else if (EQUAL(pszKey, "transpose"))
        {
            // do nothing, we passed this in earlier
        }
        else if (EQUAL(pszKey, "unscale"))
        {
            if (CPLTestBool(pszValue))
            {
                argv.AddString("-unscale");
            }
        }
        else if (EQUAL(pszKey, "a_coord_epoch"))
        {
            argv.AddString("-a_coord_epoch");
            argv.AddString(pszValue);
        }
        else if (EQUAL(pszKey, "nogcp"))
        {
            if (CPLTestBool(pszValue))
            {
                argv.AddString("-nogcp");
            }
        }
        else if (EQUAL(pszKey, "epo"))
        {
            if (CPLTestBool(pszValue))
            {
                argv.AddString("-epo");
            }
        }
        else if (EQUAL(pszKey, "eco"))
        {
            if (CPLTestBool(pszValue))
            {
                argv.AddString("-eco");
            }
        }

        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unknown option: %s",
                     pszKey);
            return nullptr;
        }
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(argv.List(), nullptr);

    auto hRet = GDALTranslate("", GDALDataset::ToHandle(poSrcDS.get()),
                              psOptions, nullptr);

    GDALTranslateOptionsFree(psOptions);

    // Situation where we open a http://.jp2 file with the JP2OpenJPEG driver
    // through the HTTP driver, which returns a /vsimem/ file
    const bool bPatchSourceFilename =
        (STARTS_WITH(osFilename.c_str(), "http://") ||
         STARTS_WITH(osFilename.c_str(), "https://")) &&
        osFilename != poSrcDS->GetDescription();

    poSrcDS.reset();

    auto poDS = dynamic_cast<VRTDataset *>(GDALDataset::FromHandle(hRet));
    if (poDS)
    {
        if (bPatchSourceFilename)
        {
            for (int i = 0; i < poDS->nBands; ++i)
            {
                auto poBand =
                    dynamic_cast<VRTSourcedRasterBand *>(poDS->papoBands[i]);
                if (poBand && poBand->m_papoSources.size() == 1 &&
                    poBand->m_papoSources[0]->IsSimpleSource())
                {
                    auto poSource = cpl::down_cast<VRTSimpleSource *>(
                        poBand->m_papoSources[0].get());
                    poSource->m_bRelativeToVRTOri = 0;
                    poSource->m_osSourceFileNameOri = osFilename;
                }
            }
        }
        poDS->SetDescription(pszSpec);
        poDS->SetWritable(false);
    }
    return poDS;
}

/************************************************************************/
/*                              OpenXML()                               */
/*                                                                      */
/*      Create an open VRTDataset from a supplied XML representation    */
/*      of the dataset.                                                 */
/************************************************************************/

std::unique_ptr<VRTDataset> VRTDataset::OpenXML(const char *pszXML,
                                                const char *pszVRTPath,
                                                GDALAccess eAccessIn)

{
    /* -------------------------------------------------------------------- */
    /*      Parse the XML.                                                  */
    /* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psTree(CPLParseXMLString(pszXML));
    if (psTree == nullptr)
        return nullptr;

    CPLXMLNode *psRoot = CPLGetXMLNode(psTree.get(), "=VRTDataset");
    if (psRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing VRTDataset element.");
        return nullptr;
    }

    const char *pszSubClass = CPLGetXMLValue(psRoot, "subClass", "");

    const bool bIsPansharpened =
        strcmp(pszSubClass, "VRTPansharpenedDataset") == 0;
    const bool bIsProcessed = strcmp(pszSubClass, "VRTProcessedDataset") == 0;

    if (!bIsPansharpened && !bIsProcessed &&
        CPLGetXMLNode(psRoot, "Group") == nullptr &&
        (CPLGetXMLNode(psRoot, "rasterXSize") == nullptr ||
         CPLGetXMLNode(psRoot, "rasterYSize") == nullptr ||
         CPLGetXMLNode(psRoot, "VRTRasterBand") == nullptr))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing one of rasterXSize, rasterYSize or bands on"
                 " VRTDataset.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the new virtual dataset object.                          */
    /* -------------------------------------------------------------------- */
    const int nXSize = atoi(CPLGetXMLValue(psRoot, "rasterXSize", "0"));
    const int nYSize = atoi(CPLGetXMLValue(psRoot, "rasterYSize", "0"));

    if (!bIsPansharpened && !bIsProcessed &&
        CPLGetXMLNode(psRoot, "VRTRasterBand") != nullptr &&
        !GDALCheckDatasetDimensions(nXSize, nYSize))
    {
        return nullptr;
    }

    std::unique_ptr<VRTDataset> poDS;
    if (strcmp(pszSubClass, "VRTWarpedDataset") == 0)
        poDS = std::make_unique<VRTWarpedDataset>(nXSize, nYSize);
    else if (bIsPansharpened)
        poDS = std::make_unique<VRTPansharpenedDataset>(nXSize, nYSize);
    else if (bIsProcessed)
        poDS = std::make_unique<VRTProcessedDataset>(nXSize, nYSize);
    else
    {
        poDS = std::make_unique<VRTDataset>(nXSize, nYSize);
        poDS->eAccess = eAccessIn;
    }

    if (poDS->XMLInit(psRoot, pszVRTPath) != CE_None)
    {
        poDS.reset();
    }

    /* -------------------------------------------------------------------- */
    /*      Try to return a regular handle on the file.                     */
    /* -------------------------------------------------------------------- */

    return poDS;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTDataset::AddBand(GDALDataType eType, char **papszOptions)

{
    if (eType == GDT_Unknown || eType == GDT_TypeCount)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Illegal GDT_Unknown/GDT_TypeCount argument");
        return CE_Failure;
    }

    SetNeedsFlush();

    /* ==================================================================== */
    /*      Handle a new raw band.                                          */
    /* ==================================================================== */
    const char *pszSubClass = CSLFetchNameValue(papszOptions, "subclass");

    if (pszSubClass != nullptr && EQUAL(pszSubClass, "VRTRawRasterBand"))
    {
#ifdef GDAL_VRT_ENABLE_RAWRASTERBAND
        if (!VRTDataset::IsRawRasterBandEnabled())
        {
            return CE_Failure;
        }
        const int nWordDataSize = GDALGetDataTypeSizeBytes(eType);

        /* ---------------------------------------------------------------- */
        /*      Collect required information.                               */
        /* ---------------------------------------------------------------- */
        const char *pszImageOffset =
            CSLFetchNameValueDef(papszOptions, "ImageOffset", "0");
        vsi_l_offset nImageOffset = CPLScanUIntBig(
            pszImageOffset, static_cast<int>(strlen(pszImageOffset)));

        int nPixelOffset = nWordDataSize;
        const char *pszPixelOffset =
            CSLFetchNameValue(papszOptions, "PixelOffset");
        if (pszPixelOffset != nullptr)
            nPixelOffset = atoi(pszPixelOffset);

        int nLineOffset;
        const char *pszLineOffset =
            CSLFetchNameValue(papszOptions, "LineOffset");
        if (pszLineOffset != nullptr)
            nLineOffset = atoi(pszLineOffset);
        else
        {
            if (nPixelOffset > INT_MAX / GetRasterXSize() ||
                nPixelOffset < INT_MIN / GetRasterXSize())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Int overflow");
                return CE_Failure;
            }
            nLineOffset = nPixelOffset * GetRasterXSize();
        }

        const char *pszByteOrder = CSLFetchNameValue(papszOptions, "ByteOrder");

        const char *pszFilename =
            CSLFetchNameValue(papszOptions, "SourceFilename");
        if (pszFilename == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "AddBand() requires a SourceFilename option for "
                     "VRTRawRasterBands.");
            return CE_Failure;
        }

        const bool bRelativeToVRT =
            CPLFetchBool(papszOptions, "relativeToVRT", false);

        /* --------------------------------------------------------------- */
        /*      Create and initialize the band.                            */
        /* --------------------------------------------------------------- */

        auto poBand = std::make_unique<VRTRawRasterBand>(
            this, GetRasterCount() + 1, eType);

        const std::string osPath = CPLGetPathSafe(GetDescription());
        const CPLErr eErr = poBand->SetRawLink(
            pszFilename, osPath.empty() ? nullptr : osPath.c_str(),
            bRelativeToVRT, nImageOffset, nPixelOffset, nLineOffset,
            pszByteOrder);
        if (eErr == CE_None)
            SetBand(GetRasterCount() + 1, std::move(poBand));

        return eErr;
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VRTDataset::AddBand(): cannot instantiate VRTRawRasterBand, "
                 "because disabled in this GDAL build");
        return CE_Failure;
#endif
    }

    /* ==================================================================== */
    /*      Handle a new "sourced" band.                                    */
    /* ==================================================================== */
    else
    {
        VRTSourcedRasterBand *poBand = nullptr;

        int nBlockXSizeIn =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "0"));
        int nBlockYSizeIn =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "0"));
        if (nBlockXSizeIn == 0 && nBlockYSizeIn == 0)
        {
            nBlockXSizeIn = m_nBlockXSize;
            nBlockYSizeIn = m_nBlockYSize;
        }

        /* ---- Check for our sourced band 'derived' subclass ---- */
        if (pszSubClass != nullptr &&
            EQUAL(pszSubClass, "VRTDerivedRasterBand"))
        {

            /* We'll need a pointer to the subclass in case we need */
            /* to set the new band's pixel function below. */
            VRTDerivedRasterBand *poDerivedBand = new VRTDerivedRasterBand(
                this, GetRasterCount() + 1, eType, GetRasterXSize(),
                GetRasterYSize(), nBlockXSizeIn, nBlockYSizeIn);

            /* Set the pixel function options it provided. */
            const char *pszFuncName =
                CSLFetchNameValue(papszOptions, "PixelFunctionType");
            if (pszFuncName != nullptr)
                poDerivedBand->SetPixelFunctionName(pszFuncName);

            const char *pszLanguage =
                CSLFetchNameValue(papszOptions, "PixelFunctionLanguage");
            if (pszLanguage != nullptr)
                poDerivedBand->SetPixelFunctionLanguage(pszLanguage);

            const char *pszSkipNonContributingSources =
                CSLFetchNameValue(papszOptions, "SkipNonContributingSources");
            if (pszSkipNonContributingSources != nullptr)
            {
                poDerivedBand->SetSkipNonContributingSources(
                    CPLTestBool(pszSkipNonContributingSources));
            }
            for (const auto &[pszKey, pszValue] :
                 cpl::IterateNameValue(static_cast<CSLConstList>(papszOptions)))
            {
                if (STARTS_WITH(pszKey, "_PIXELFN_ARG_"))
                {
                    poDerivedBand->AddPixelFunctionArgument(pszKey + 13,
                                                            pszValue);
                }
            }

            const char *pszTransferTypeName =
                CSLFetchNameValue(papszOptions, "SourceTransferType");
            if (pszTransferTypeName != nullptr)
            {
                const GDALDataType eTransferType =
                    GDALGetDataTypeByName(pszTransferTypeName);
                if (eTransferType == GDT_Unknown)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "invalid SourceTransferType: \"%s\".",
                             pszTransferTypeName);
                    delete poDerivedBand;
                    return CE_Failure;
                }
                poDerivedBand->SetSourceTransferType(eTransferType);
            }

            /* We're done with the derived band specific stuff, so */
            /* we can assign the base class pointer now. */
            poBand = poDerivedBand;
        }
        else
        {
            /* ---- Standard sourced band ---- */
            poBand = new VRTSourcedRasterBand(
                this, GetRasterCount() + 1, eType, GetRasterXSize(),
                GetRasterYSize(), nBlockXSizeIn, nBlockYSizeIn);
        }

        SetBand(GetRasterCount() + 1, poBand);

        for (int i = 0; papszOptions != nullptr && papszOptions[i] != nullptr;
             i++)
        {
            if (STARTS_WITH_CI(papszOptions[i], "AddFuncSource="))
            {
                char **papszTokens = CSLTokenizeStringComplex(
                    papszOptions[i] + 14, ",", TRUE, FALSE);
                if (CSLCount(papszTokens) < 1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "AddFuncSource(): required argument missing.");
                    // TODO: How should this error be handled?  Return
                    // CE_Failure?
                }

                VRTImageReadFunc pfnReadFunc = nullptr;
                sscanf(papszTokens[0], "%p", &pfnReadFunc);

                void *pCBData = nullptr;
                if (CSLCount(papszTokens) > 1)
                    sscanf(papszTokens[1], "%p", &pCBData);

                const double dfNoDataValue = (CSLCount(papszTokens) > 2)
                                                 ? CPLAtof(papszTokens[2])
                                                 : VRT_NODATA_UNSET;

                poBand->AddFuncSource(pfnReadFunc, pCBData, dfNoDataValue);

                CSLDestroy(papszTokens);
            }
        }

        return CE_None;
    }
}

/*! @endcond */
/************************************************************************/
/*                              VRTAddBand()                            */
/************************************************************************/

/**
 * @see VRTDataset::VRTAddBand().
 *
 * @note The return type of this function is int, but the actual values
 * returned are of type CPLErr.
 */

int CPL_STDCALL VRTAddBand(VRTDatasetH hDataset, GDALDataType eType,
                           char **papszOptions)

{
    VALIDATE_POINTER1(hDataset, "VRTAddBand", 0);

    return static_cast<VRTDataset *>(GDALDataset::FromHandle(hDataset))
        ->AddBand(eType, papszOptions);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *VRTDataset::Create(const char *pszName, int nXSize, int nYSize,
                                int nBandsIn, GDALDataType eType,
                                char **papszOptions)

{
    return CreateVRTDataset(pszName, nXSize, nYSize, nBandsIn, eType,
                            const_cast<CSLConstList>(papszOptions))
        .release();
}

/************************************************************************/
/*                            CreateVRTDataset()                        */
/************************************************************************/

std::unique_ptr<VRTDataset>
VRTDataset::CreateVRTDataset(const char *pszName, int nXSize, int nYSize,
                             int nBandsIn, GDALDataType eType,
                             CSLConstList papszOptions)

{
    if (STARTS_WITH_CI(pszName, "<VRTDataset"))
    {
        auto poDS = OpenXML(pszName, nullptr, GA_Update);
        if (poDS != nullptr)
            poDS->SetDescription("<FromXML>");
        return poDS;
    }

    const char *pszSubclass = CSLFetchNameValue(papszOptions, "SUBCLASS");

    std::unique_ptr<VRTDataset> poDS;

    const int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "0"));
    const int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "0"));
    if (pszSubclass == nullptr || EQUAL(pszSubclass, "VRTDataset"))
        poDS = std::make_unique<VRTDataset>(nXSize, nYSize, nBlockXSize,
                                            nBlockYSize);
    else if (EQUAL(pszSubclass, "VRTWarpedDataset"))
    {
        poDS = std::make_unique<VRTWarpedDataset>(nXSize, nYSize, nBlockXSize,
                                                  nBlockYSize);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "SUBCLASS=%s not recognised.",
                 pszSubclass);
        return nullptr;
    }
    poDS->eAccess = GA_Update;

    poDS->SetDescription(pszName);

    for (int iBand = 0; iBand < nBandsIn; iBand++)
        poDS->AddBand(eType, nullptr);

    poDS->SetNeedsFlush();

    poDS->oOvManager.Initialize(poDS.get(), pszName);

    return poDS;
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset *
VRTDataset::CreateMultiDimensional(const char *pszFilename,
                                   CSLConstList /*papszRootGroupOptions*/,
                                   CSLConstList /*papszOptions*/)
{
    VRTDataset *poDS = new VRTDataset(0, 0);
    poDS->eAccess = GA_Update;
    poDS->SetDescription(pszFilename);
    poDS->m_poRootGroup = VRTGroup::Create(std::string(), "/");
    poDS->m_poRootGroup->SetIsRootGroup();
    poDS->m_poRootGroup->SetFilename(pszFilename);
    poDS->m_poRootGroup->SetDirty();

    return poDS;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **VRTDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    int nSize = CSLCount(papszFileList);
    int nMaxSize = nSize;

    // Do not need an element deallocator as each string points to an
    // element of the papszFileList.
    CPLHashSet *hSetFiles =
        CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, nullptr);

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        static_cast<VRTRasterBand *>(papoBands[iBand])
            ->GetFileList(&papszFileList, &nSize, &nMaxSize, hSetFiles);
    }

    CPLHashSetDestroy(hSetFiles);

    return papszFileList;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

/* We implement Delete() to avoid that the default implementation */
/* in GDALDriver::Delete() destroys the source files listed by GetFileList(),*/
/* which would be an undesired effect... */
CPLErr VRTDataset::Delete(const char *pszFilename)
{
    GDALDriverH hDriver = GDALIdentifyDriver(pszFilename, nullptr);

    if (!hDriver || !EQUAL(GDALGetDriverShortName(hDriver), "VRT"))
        return CE_Failure;

    if (strstr(pszFilename, "<VRTDataset") == nullptr &&
        VSIUnlink(pszFilename) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Deleting %s failed:\n%s",
                 pszFilename, VSIStrerror(errno));
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          CreateMaskBand()                            */
/************************************************************************/

CPLErr VRTDataset::CreateMaskBand(int)
{
    if (m_poMaskBand != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT dataset has already a mask band");
        return CE_Failure;
    }

    SetMaskBand(std::make_unique<VRTSourcedRasterBand>(this, 0));

    return CE_None;
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTDataset::SetMaskBand(std::unique_ptr<VRTRasterBand> poMaskBandIn)
{
    m_poMaskBand = std::move(poMaskBandIn);
    m_poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTDataset::CloseDependentDatasets()
{
    /* We need to call it before removing the sources, otherwise */
    /* we would remove them from the serizalized VRT */
    FlushCache(true);

    int bHasDroppedRef = GDALDataset::CloseDependentDatasets();

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        bHasDroppedRef |= static_cast<VRTRasterBand *>(papoBands[iBand])
                              ->CloseDependentDatasets();
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                      CheckCompatibleForDatasetIO()                   */
/************************************************************************/

/* We will return TRUE only if all the bands are VRTSourcedRasterBands */
/* made of identical sources, that are strictly VRTSimpleSource, and that */
/* the band number of each source is the band number of the */
/* VRTSourcedRasterBand. */

bool VRTDataset::CheckCompatibleForDatasetIO() const
{
    size_t nSources = 0;
    const std::unique_ptr<VRTSource> *papoSources = nullptr;
    CPLString osResampling;

    if (m_nCompatibleForDatasetIO >= 0)
    {
        return CPL_TO_BOOL(m_nCompatibleForDatasetIO);
    }

    m_nCompatibleForDatasetIO = false;

    GDALDataset *poFirstBandSourceDS = nullptr;
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        auto poVRTBand = static_cast<VRTRasterBand *>(papoBands[iBand]);
        assert(poVRTBand);
        if (!poVRTBand->IsSourcedRasterBand())
            return false;

        const VRTSourcedRasterBand *poBand =
            static_cast<const VRTSourcedRasterBand *>(poVRTBand);

        // Do not allow VRTDerivedRasterBand for example
        if (typeid(*poBand) != typeid(VRTSourcedRasterBand))
            return false;

        if (iBand == 0)
        {
            nSources = poBand->m_papoSources.size();
            papoSources = poBand->m_papoSources.data();
            for (auto &poSource : poBand->m_papoSources)
            {
                if (!poSource->IsSimpleSource())
                    return false;

                const VRTSimpleSource *poSimpleSource =
                    static_cast<const VRTSimpleSource *>(poSource.get());
                if (poSimpleSource->GetType() !=
                    VRTSimpleSource::GetTypeStatic())
                    return false;

                if (poSimpleSource->m_nBand != iBand + 1 ||
                    poSimpleSource->m_bGetMaskBand ||
                    (nSources > 1 && poSimpleSource->m_osSrcDSName.empty()))
                {
                    return false;
                }
                if (nSources == 1 && poSimpleSource->m_osSrcDSName.empty())
                {
                    if (auto poSourceBand = poSimpleSource->GetRasterBand())
                    {
                        poFirstBandSourceDS = poSourceBand->GetDataset();
                    }
                    else
                    {
                        return false;
                    }
                }
                osResampling = poSimpleSource->GetResampling();
            }
        }
        else if (nSources != poBand->m_papoSources.size())
        {
            return false;
        }
        else
        {
            for (size_t iSource = 0; iSource < nSources; iSource++)
            {
                if (!poBand->m_papoSources[iSource]->IsSimpleSource())
                    return false;
                const VRTSimpleSource *poRefSource =
                    static_cast<const VRTSimpleSource *>(
                        papoSources[iSource].get());

                const VRTSimpleSource *poSource =
                    static_cast<const VRTSimpleSource *>(
                        poBand->m_papoSources[iSource].get());
                if (poSource->GetType() != VRTSimpleSource::GetTypeStatic())
                    return false;
                if (poSource->m_nBand != iBand + 1 ||
                    poSource->m_bGetMaskBand ||
                    (nSources > 1 && poSource->m_osSrcDSName.empty()))
                    return false;
                if (!poSource->IsSameExceptBandNumber(poRefSource))
                    return false;
                if (osResampling.compare(poSource->GetResampling()) != 0)
                    return false;
                if (nSources == 1 && poSource->m_osSrcDSName.empty())
                {
                    auto poSourceBand = poSource->GetRasterBand();
                    if (!poSourceBand ||
                        poFirstBandSourceDS != poSourceBand->GetDataset())
                    {
                        return false;
                    }
                }
            }
        }
    }

    m_nCompatibleForDatasetIO = nSources != 0;
    return CPL_TO_BOOL(m_nCompatibleForDatasetIO);
}

/************************************************************************/
/*                         GetSingleSimpleSource()                      */
/*                                                                      */
/* Returns a non-NULL dataset if the VRT is made of a single source     */
/* that is a simple source, in its full extent, and with all of its     */
/* bands. Basically something produced by :                             */
/*   gdal_translate src dst.vrt -of VRT (-a_srs / -a_ullr)              */
/************************************************************************/

GDALDataset *VRTDataset::GetSingleSimpleSource()
{
    if (!CheckCompatibleForDatasetIO())
        return nullptr;

    VRTSourcedRasterBand *poVRTBand =
        static_cast<VRTSourcedRasterBand *>(papoBands[0]);
    if (poVRTBand->m_papoSources.size() != 1)
        return nullptr;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->m_papoSources[0].get());

    GDALRasterBand *poBand = poSource->GetRasterBand();
    if (poBand == nullptr || poSource->GetMaskBandMainBand() != nullptr)
        return nullptr;

    GDALDataset *poSrcDS = poBand->GetDataset();
    if (poSrcDS == nullptr)
        return nullptr;

    /* Check that it uses the full source dataset */
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;
    bool bError = false;
    if (!poSource->GetSrcDstWindow(
            0, 0, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
            poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), &dfReqXOff,
            &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff, &nReqYOff,
            &nReqXSize, &nReqYSize, &nOutXOff, &nOutYOff, &nOutXSize,
            &nOutYSize, bError))
        return nullptr;

    if (nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != poSrcDS->GetRasterXSize() ||
        nReqYSize != poSrcDS->GetRasterYSize())
        return nullptr;

    if (nOutXOff != 0 || nOutYOff != 0 ||
        nOutXSize != poSrcDS->GetRasterXSize() ||
        nOutYSize != poSrcDS->GetRasterYSize())
        return nullptr;

    return poSrcDS;
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr VRTDataset::AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
                              int nBufXSize, int nBufYSize, GDALDataType eDT,
                              int nBandCount, int *panBandList,
                              char **papszOptions)
{
    if (!CheckCompatibleForDatasetIO())
        return CE_None;

    VRTSourcedRasterBand *poVRTBand =
        static_cast<VRTSourcedRasterBand *>(papoBands[0]);
    if (poVRTBand->m_papoSources.size() != 1)
        return CE_None;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->m_papoSources[0].get());

    /* Find source window and buffer size */
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;
    bool bError = false;
    if (!poSource->GetSrcDstWindow(nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                   nBufYSize, &dfReqXOff, &dfReqYOff,
                                   &dfReqXSize, &dfReqYSize, &nReqXOff,
                                   &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff,
                                   &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }

    GDALRasterBand *poBand = poSource->GetRasterBand();
    if (poBand == nullptr || poSource->GetMaskBandMainBand() != nullptr)
        return CE_None;

    GDALDataset *poSrcDS = poBand->GetDataset();
    if (poSrcDS == nullptr)
        return CE_None;

    return poSrcDS->AdviseRead(nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                               nOutXSize, nOutYSize, eDT, nBandCount,
                               panBandList, papszOptions);
}

/************************************************************************/
/*                           GetNumThreads()                            */
/************************************************************************/

/* static */ int VRTDataset::GetNumThreads(GDALDataset *poDS)
{
    const char *pszNumThreads = nullptr;
    if (poDS)
        pszNumThreads = CSLFetchNameValueDef(poDS->GetOpenOptions(),
                                             "NUM_THREADS", nullptr);
    if (!pszNumThreads)
        pszNumThreads = CPLGetConfigOption("VRT_NUM_THREADS", nullptr);
    if (!pszNumThreads)
        pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    if (EQUAL(pszNumThreads, "0") || EQUAL(pszNumThreads, "1"))
        return atoi(pszNumThreads);
    const int nMaxPoolSize = GDALGetMaxDatasetPoolSize();
    const int nLimit = std::min(CPLGetNumCPUs(), nMaxPoolSize);
    if (EQUAL(pszNumThreads, "ALL_CPUS"))
        return nLimit;
    return std::min(atoi(pszNumThreads), nLimit);
}

/************************************************************************/
/*                       VRTDatasetRasterIOJob                          */
/************************************************************************/

/** Structure used to declare a threaded job to satisfy IRasterIO()
 * on a given source.
 */
struct VRTDatasetRasterIOJob
{
    std::atomic<int> *pnCompletedJobs = nullptr;
    std::atomic<bool> *pbSuccess = nullptr;
    CPLErrorAccumulator *poErrorAccumulator = nullptr;

    GDALDataType eVRTBandDataType = GDT_Unknown;
    int nXOff = 0;
    int nYOff = 0;
    int nXSize = 0;
    int nYSize = 0;
    void *pData = nullptr;
    int nBufXSize = 0;
    int nBufYSize = 0;
    int nBandCount = 0;
    BANDMAP_TYPE panBandMap = nullptr;
    GDALDataType eBufType = GDT_Unknown;
    GSpacing nPixelSpace = 0;
    GSpacing nLineSpace = 0;
    GSpacing nBandSpace = 0;
    GDALRasterIOExtraArg *psExtraArg = nullptr;
    VRTSimpleSource *poSource = nullptr;

    static void Func(void *pData);
};

/************************************************************************/
/*                     VRTDatasetRasterIOJob::Func()                    */
/************************************************************************/

void VRTDatasetRasterIOJob::Func(void *pData)
{
    auto psJob = std::unique_ptr<VRTDatasetRasterIOJob>(
        static_cast<VRTDatasetRasterIOJob *>(pData));
    if (*psJob->pbSuccess)
    {
        GDALRasterIOExtraArg sArg = *(psJob->psExtraArg);
        sArg.pfnProgress = nullptr;
        sArg.pProgressData = nullptr;

        auto oAccumulator = psJob->poErrorAccumulator->InstallForCurrentScope();
        CPL_IGNORE_RET_VAL(oAccumulator);

        if (psJob->poSource->DatasetRasterIO(
                psJob->eVRTBandDataType, psJob->nXOff, psJob->nYOff,
                psJob->nXSize, psJob->nYSize, psJob->pData, psJob->nBufXSize,
                psJob->nBufYSize, psJob->eBufType, psJob->nBandCount,
                psJob->panBandMap, psJob->nPixelSpace, psJob->nLineSpace,
                psJob->nBandSpace, &sArg) != CE_None)
        {
            *psJob->pbSuccess = false;
        }
    }

    ++(*psJob->pnCompletedJobs);
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg)
{
    m_bMultiThreadedRasterIOLastUsed = false;

    if (nBands == 1 && nBandCount == 1)
    {
        VRTSourcedRasterBand *poBand =
            dynamic_cast<VRTSourcedRasterBand *>(papoBands[0]);
        if (poBand)
        {
            return poBand->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg);
        }
    }

    bool bLocalCompatibleForDatasetIO =
        CPL_TO_BOOL(CheckCompatibleForDatasetIO());
    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nBufXSize < nXSize || nBufYSize < nYSize) && m_apoOverviews.empty())
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);

        if (bTried)
        {
            return eErr;
        }

        for (int iBand = 0; iBand < nBands; iBand++)
        {
            VRTSourcedRasterBand *poBand =
                static_cast<VRTSourcedRasterBand *>(papoBands[iBand]);

            // If there are overviews, let VRTSourcedRasterBand::IRasterIO()
            // do the job.
            if (poBand->GetOverviewCount() != 0)
            {
                bLocalCompatibleForDatasetIO = false;
                break;
            }
        }
    }

    // If resampling with non-nearest neighbour, we need to be careful
    // if the VRT band exposes a nodata value, but the sources do not have it.
    // To also avoid edge effects on sources when downsampling, use the
    // base implementation of IRasterIO() (that is acquiring sources at their
    // nominal resolution, and then downsampling), but only if none of the
    // contributing sources have overviews.
    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
    {
        for (int iBandIndex = 0; iBandIndex < nBandCount; iBandIndex++)
        {
            VRTSourcedRasterBand *poBand = static_cast<VRTSourcedRasterBand *>(
                GetRasterBand(panBandMap[iBandIndex]));
            if (!poBand->CanIRasterIOBeForwardedToEachSource(
                    eRWFlag, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                    psExtraArg))
            {
                bLocalCompatibleForDatasetIO = false;
                break;
            }
        }
    }

    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read)
    {
        for (int iBandIndex = 0; iBandIndex < nBandCount; iBandIndex++)
        {
            VRTSourcedRasterBand *poBand = static_cast<VRTSourcedRasterBand *>(
                GetRasterBand(panBandMap[iBandIndex]));

            /* Dirty little trick to initialize the buffer without doing */
            /* any real I/O */
            std::vector<std::unique_ptr<VRTSource>> papoSavedSources;
            std::swap(papoSavedSources, poBand->m_papoSources);

            GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
            psExtraArg->pfnProgress = nullptr;

            GByte *pabyBandData =
                static_cast<GByte *>(pData) + iBandIndex * nBandSpace;

            poBand->IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                              pabyBandData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace, psExtraArg);

            psExtraArg->pfnProgress = pfnProgressGlobal;

            std::swap(papoSavedSources, poBand->m_papoSources);
        }

        CPLErr eErr = CE_None;

        // Use the last band, because when sources reference a GDALProxyDataset,
        // they don't necessary instantiate all underlying rasterbands.
        VRTSourcedRasterBand *poBand =
            static_cast<VRTSourcedRasterBand *>(papoBands[nBands - 1]);

        double dfXOff = nXOff;
        double dfYOff = nYOff;
        double dfXSize = nXSize;
        double dfYSize = nYSize;
        if (psExtraArg->bFloatingPointWindowValidity)
        {
            dfXOff = psExtraArg->dfXOff;
            dfYOff = psExtraArg->dfYOff;
            dfXSize = psExtraArg->dfXSize;
            dfYSize = psExtraArg->dfYSize;
        }

        int nContributingSources = 0;
        int nMaxThreads = 0;
        constexpr int MINIMUM_PIXEL_COUNT_FOR_THREADED_IO = 1000 * 1000;
        if ((static_cast<int64_t>(nBufXSize) * nBufYSize >=
                 MINIMUM_PIXEL_COUNT_FOR_THREADED_IO ||
             static_cast<int64_t>(nXSize) * nYSize >=
                 MINIMUM_PIXEL_COUNT_FOR_THREADED_IO) &&
            poBand->CanMultiThreadRasterIO(dfXOff, dfYOff, dfXSize, dfYSize,
                                           nContributingSources) &&
            nContributingSources > 1 &&
            (nMaxThreads = VRTDataset::GetNumThreads(this)) > 1)
        {
            m_bMultiThreadedRasterIOLastUsed = true;
            m_oMapSharedSources.InitMutex();

            CPLErrorAccumulator errorAccumulator;
            std::atomic<bool> bSuccess = true;
            CPLWorkerThreadPool *psThreadPool = GDALGetGlobalThreadPool(
                std::min(nContributingSources, nMaxThreads));

            CPLDebugOnly(
                "VRT",
                "IRasterIO(): use optimized "
                "multi-threaded code path for mosaic. "
                "Using %d threads",
                std::min(nContributingSources, psThreadPool->GetThreadCount()));

            auto oQueue = psThreadPool->CreateJobQueue();
            std::atomic<int> nCompletedJobs = 0;
            for (auto &poSource : poBand->m_papoSources)
            {
                if (!poSource->IsSimpleSource())
                    continue;
                auto poSimpleSource =
                    cpl::down_cast<VRTSimpleSource *>(poSource.get());
                if (poSimpleSource->DstWindowIntersects(dfXOff, dfYOff, dfXSize,
                                                        dfYSize))
                {
                    auto psJob = new VRTDatasetRasterIOJob();
                    psJob->pbSuccess = &bSuccess;
                    psJob->poErrorAccumulator = &errorAccumulator;
                    psJob->pnCompletedJobs = &nCompletedJobs;
                    psJob->eVRTBandDataType = poBand->GetRasterDataType();
                    psJob->nXOff = nXOff;
                    psJob->nYOff = nYOff;
                    psJob->nXSize = nXSize;
                    psJob->nYSize = nYSize;
                    psJob->pData = pData;
                    psJob->nBufXSize = nBufXSize;
                    psJob->nBufYSize = nBufYSize;
                    psJob->eBufType = eBufType;
                    psJob->nBandCount = nBandCount;
                    psJob->panBandMap = panBandMap;
                    psJob->nPixelSpace = nPixelSpace;
                    psJob->nLineSpace = nLineSpace;
                    psJob->nBandSpace = nBandSpace;
                    psJob->psExtraArg = psExtraArg;
                    psJob->poSource = poSimpleSource;

                    if (!oQueue->SubmitJob(VRTDatasetRasterIOJob::Func, psJob))
                    {
                        delete psJob;
                        bSuccess = false;
                        break;
                    }
                }
            }

            while (oQueue->WaitEvent())
            {
                // Quite rough progress callback. We could do better by counting
                // the number of contributing pixels.
                if (psExtraArg->pfnProgress)
                {
                    psExtraArg->pfnProgress(double(nCompletedJobs.load()) /
                                                nContributingSources,
                                            "", psExtraArg->pProgressData);
                }
            }

            errorAccumulator.ReplayErrors();
            eErr = bSuccess ? CE_None : CE_Failure;
        }
        else
        {
            GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
            void *pProgressDataGlobal = psExtraArg->pProgressData;

            const int nSources = static_cast<int>(poBand->m_papoSources.size());
            for (int iSource = 0; eErr == CE_None && iSource < nSources;
                 iSource++)
            {
                psExtraArg->pfnProgress = GDALScaledProgress;
                psExtraArg->pProgressData = GDALCreateScaledProgress(
                    1.0 * iSource / nSources, 1.0 * (iSource + 1) / nSources,
                    pfnProgressGlobal, pProgressDataGlobal);

                VRTSimpleSource *poSource = static_cast<VRTSimpleSource *>(
                    poBand->m_papoSources[iSource].get());

                eErr = poSource->DatasetRasterIO(
                    poBand->GetRasterDataType(), nXOff, nYOff, nXSize, nYSize,
                    pData, nBufXSize, nBufYSize, eBufType, nBandCount,
                    panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                    psExtraArg);

                GDALDestroyScaledProgress(psExtraArg->pProgressData);
            }

            psExtraArg->pfnProgress = pfnProgressGlobal;
            psExtraArg->pProgressData = pProgressDataGlobal;
        }

        if (eErr == CE_None && psExtraArg->pfnProgress)
        {
            psExtraArg->pfnProgress(1.0, "", psExtraArg->pProgressData);
        }

        return eErr;
    }

    CPLErr eErr;
    if (eRWFlag == GF_Read &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        nBufXSize < nXSize && nBufYSize < nYSize && nBandCount > 1)
    {
        // Force going through VRTSourcedRasterBand::IRasterIO(), otherwise
        // GDALDataset::IRasterIOResampled() would be used without source
        // overviews being potentially used.
        eErr = GDALDataset::BandBasedRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg);
    }
    else
    {
        eErr = GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nBandCount, panBandMap, nPixelSpace,
                                      nLineSpace, nBandSpace, psExtraArg);
    }
    return eErr;
}

/************************************************************************/
/*                  UnsetPreservedRelativeFilenames()                   */
/************************************************************************/

void VRTDataset::UnsetPreservedRelativeFilenames()
{
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (!static_cast<VRTRasterBand *>(papoBands[iBand])
                 ->IsSourcedRasterBand())
            continue;

        VRTSourcedRasterBand *poBand =
            static_cast<VRTSourcedRasterBand *>(papoBands[iBand]);
        for (auto &poSource : poBand->m_papoSources)
        {
            if (!poSource->IsSimpleSource())
                continue;

            VRTSimpleSource *poSimpleSource =
                static_cast<VRTSimpleSource *>(poSource.get());
            poSimpleSource->UnsetPreservedRelativeFilenames();
        }
    }
}

/************************************************************************/
/*                        BuildVirtualOverviews()                       */
/************************************************************************/

static bool CheckBandForOverview(GDALRasterBand *poBand,
                                 GDALRasterBand *&poFirstBand, int &nOverviews,
                                 std::set<std::pair<int, int>> &oSetOvrSizes,
                                 std::vector<GDALDataset *> &apoOverviewsBak)
{
    if (!cpl::down_cast<VRTRasterBand *>(poBand)->IsSourcedRasterBand())
        return false;

    VRTSourcedRasterBand *poVRTBand =
        cpl::down_cast<VRTSourcedRasterBand *>(poBand);
    if (poVRTBand->m_papoSources.size() != 1)
        return false;
    if (!poVRTBand->m_papoSources[0]->IsSimpleSource())
        return false;

    VRTSimpleSource *poSource =
        cpl::down_cast<VRTSimpleSource *>(poVRTBand->m_papoSources[0].get());
    const char *pszType = poSource->GetType();
    if (pszType != VRTSimpleSource::GetTypeStatic() &&
        pszType != VRTComplexSource::GetTypeStatic())
    {
        return false;
    }
    GDALRasterBand *poSrcBand = poBand->GetBand() == 0
                                    ? poSource->GetMaskBandMainBand()
                                    : poSource->GetRasterBand();
    if (poSrcBand == nullptr)
        return false;

    // To prevent recursion
    apoOverviewsBak.push_back(nullptr);
    const int nOvrCount = poSrcBand->GetOverviewCount();
    oSetOvrSizes.insert(
        std::pair<int, int>(poSrcBand->GetXSize(), poSrcBand->GetYSize()));
    for (int i = 0; i < nOvrCount; ++i)
    {
        auto poSrcOvrBand = poSrcBand->GetOverview(i);
        if (poSrcOvrBand)
        {
            oSetOvrSizes.insert(std::pair<int, int>(poSrcOvrBand->GetXSize(),
                                                    poSrcOvrBand->GetYSize()));
        }
    }
    apoOverviewsBak.resize(0);

    if (nOvrCount == 0)
        return false;
    if (poFirstBand == nullptr)
    {
        if (poSrcBand->GetXSize() == 0 || poSrcBand->GetYSize() == 0)
            return false;
        poFirstBand = poSrcBand;
        nOverviews = nOvrCount;
    }
    else if (nOvrCount < nOverviews)
        nOverviews = nOvrCount;
    return true;
}

void VRTDataset::BuildVirtualOverviews()
{
    // Currently we expose virtual overviews only if the dataset is made of
    // a single SimpleSource/ComplexSource, in each band.
    // And if the underlying sources have overviews of course
    if (!m_apoOverviews.empty() || !m_apoOverviewsBak.empty())
        return;

    int nOverviews = 0;
    GDALRasterBand *poFirstBand = nullptr;
    std::set<std::pair<int, int>> oSetOvrSizes;

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (!CheckBandForOverview(papoBands[iBand], poFirstBand, nOverviews,
                                  oSetOvrSizes, m_apoOverviewsBak))
            return;
    }

    if (m_poMaskBand)
    {
        if (!CheckBandForOverview(m_poMaskBand.get(), poFirstBand, nOverviews,
                                  oSetOvrSizes, m_apoOverviewsBak))
            return;
    }
    if (poFirstBand == nullptr)
    {
        // to make cppcheck happy
        CPLAssert(false);
        return;
    }

    VRTSourcedRasterBand *l_poVRTBand =
        cpl::down_cast<VRTSourcedRasterBand *>(papoBands[0]);
    VRTSimpleSource *poSource =
        cpl::down_cast<VRTSimpleSource *>(l_poVRTBand->m_papoSources[0].get());
    const double dfDstToSrcXRatio =
        poSource->m_dfDstXSize / poSource->m_dfSrcXSize;
    const double dfDstToSrcYRatio =
        poSource->m_dfDstYSize / poSource->m_dfSrcYSize;

    for (int j = 0; j < nOverviews; j++)
    {
        auto poOvrBand = poFirstBand->GetOverview(j);
        if (!poOvrBand)
            return;
        const double dfXRatio = static_cast<double>(poOvrBand->GetXSize()) /
                                poFirstBand->GetXSize();
        const double dfYRatio = static_cast<double>(poOvrBand->GetYSize()) /
                                poFirstBand->GetYSize();
        if (dfXRatio >= dfDstToSrcXRatio || dfYRatio >= dfDstToSrcYRatio)
        {
            continue;
        }
        int nOvrXSize = static_cast<int>(0.5 + nRasterXSize * dfXRatio);
        int nOvrYSize = static_cast<int>(0.5 + nRasterYSize * dfYRatio);

        // Look for a source overview whose size is very close to the
        // theoretical computed one.
        bool bSrcOvrMatchFound = false;
        for (const auto &ovrSize : oSetOvrSizes)
        {
            if (std::abs(ovrSize.first - nOvrXSize) <= 1 &&
                std::abs(ovrSize.second - nOvrYSize) <= 1)
            {
                bSrcOvrMatchFound = true;
                nOvrXSize = ovrSize.first;
                nOvrYSize = ovrSize.second;
                break;
            }
        }

        if (!bSrcOvrMatchFound &&
            (nOvrXSize < DEFAULT_BLOCK_SIZE || nOvrYSize < DEFAULT_BLOCK_SIZE))
        {
            break;
        }

        int nBlockXSize = 0;
        int nBlockYSize = 0;
        l_poVRTBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        if (VRTDataset::IsDefaultBlockSize(nBlockXSize, nRasterXSize))
            nBlockXSize = 0;
        if (VRTDataset::IsDefaultBlockSize(nBlockYSize, nRasterYSize))
            nBlockYSize = 0;

        VRTDataset *poOvrVDS =
            new VRTDataset(nOvrXSize, nOvrYSize, nBlockXSize, nBlockYSize);
        m_apoOverviews.push_back(poOvrVDS);

        const auto CreateOverviewBand =
            [&poOvrVDS, nOvrXSize, nOvrYSize, dfXRatio,
             dfYRatio](VRTSourcedRasterBand *poVRTBand)
        {
            auto poOvrVRTBand = std::make_unique<VRTSourcedRasterBand>(
                poOvrVDS, poVRTBand->GetBand(), poVRTBand->GetRasterDataType(),
                nOvrXSize, nOvrYSize);
            poOvrVRTBand->CopyCommonInfoFrom(poVRTBand);
            poOvrVRTBand->m_bNoDataValueSet = poVRTBand->m_bNoDataValueSet;
            poOvrVRTBand->m_dfNoDataValue = poVRTBand->m_dfNoDataValue;
            poOvrVRTBand->m_bHideNoDataValue = poVRTBand->m_bHideNoDataValue;

            VRTSimpleSource *poSrcSource = cpl::down_cast<VRTSimpleSource *>(
                poVRTBand->m_papoSources[0].get());
            std::unique_ptr<VRTSimpleSource> poNewSource;
            const char *pszType = poSrcSource->GetType();
            if (pszType == VRTSimpleSource::GetTypeStatic())
            {
                poNewSource = std::make_unique<VRTSimpleSource>(
                    poSrcSource, dfXRatio, dfYRatio);
            }
            else if (pszType == VRTComplexSource::GetTypeStatic())
            {
                poNewSource = std::make_unique<VRTComplexSource>(
                    cpl::down_cast<VRTComplexSource *>(poSrcSource), dfXRatio,
                    dfYRatio);
            }
            else
            {
                CPLAssert(false);
            }
            if (poNewSource)
            {
                auto poNewSourceBand = poVRTBand->GetBand() == 0
                                           ? poNewSource->GetMaskBandMainBand()
                                           : poNewSource->GetRasterBand();
                CPLAssert(poNewSourceBand);
                auto poNewSourceBandDS = poNewSourceBand->GetDataset();
                if (poNewSourceBandDS)
                    poNewSourceBandDS->Reference();
                poOvrVRTBand->AddSource(std::move(poNewSource));
            }

            return poOvrVRTBand;
        };

        for (int i = 0; i < nBands; i++)
        {
            VRTSourcedRasterBand *poSrcBand =
                cpl::down_cast<VRTSourcedRasterBand *>(GetRasterBand(i + 1));
            poOvrVDS->SetBand(poOvrVDS->GetRasterCount() + 1,
                              CreateOverviewBand(poSrcBand));
        }

        if (m_poMaskBand)
        {
            VRTSourcedRasterBand *poSrcBand =
                cpl::down_cast<VRTSourcedRasterBand *>(m_poMaskBand.get());
            poOvrVDS->SetMaskBand(CreateOverviewBand(poSrcBand));
        }
    }
}

/************************************************************************/
/*                        AddVirtualOverview()                          */
/************************************************************************/

bool VRTDataset::AddVirtualOverview(int nOvFactor, const char *pszResampling)
{
    if (nRasterXSize / nOvFactor == 0 || nRasterYSize / nOvFactor == 0)
    {
        return false;
    }

    CPLStringList argv;
    argv.AddString("-of");
    argv.AddString("VRT");
    argv.AddString("-outsize");
    argv.AddString(CPLSPrintf("%d", nRasterXSize / nOvFactor));
    argv.AddString(CPLSPrintf("%d", nRasterYSize / nOvFactor));
    argv.AddString("-r");
    argv.AddString(pszResampling);

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    if (!VRTDataset::IsDefaultBlockSize(nBlockXSize, nRasterXSize))
    {
        argv.AddString("-co");
        argv.AddString(CPLSPrintf("BLOCKXSIZE=%d", nBlockXSize));
    }
    if (!VRTDataset::IsDefaultBlockSize(nBlockYSize, nRasterYSize))
    {
        argv.AddString("-co");
        argv.AddString(CPLSPrintf("BLOCKYSIZE=%d", nBlockYSize));
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(argv.List(), nullptr);

    // Add a dummy overview so that BuildVirtualOverviews() doesn't trigger
    m_apoOverviews.push_back(nullptr);
    CPLAssert(m_bCanTakeRef);
    m_bCanTakeRef =
        false;  // we don't want hOverviewDS to take a reference on ourselves.
    GDALDatasetH hOverviewDS =
        GDALTranslate("", GDALDataset::ToHandle(this), psOptions, nullptr);
    m_bCanTakeRef = true;
    m_apoOverviews.pop_back();

    GDALTranslateOptionsFree(psOptions);
    if (hOverviewDS == nullptr)
        return false;

    m_anOverviewFactors.push_back(nOvFactor);
    m_apoOverviews.push_back(GDALDataset::FromHandle(hOverviewDS));
    return true;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr VRTDataset::IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions)
{
    if (CPLTestBool(CPLGetConfigOption("VRT_VIRTUAL_OVERVIEWS", "NO")))
    {
        SetNeedsFlush();
        if (nOverviews == 0 ||
            (!m_apoOverviews.empty() && m_anOverviewFactors.empty()))
        {
            m_anOverviewFactors.clear();
            m_apoOverviewsBak.insert(m_apoOverviewsBak.end(),
                                     m_apoOverviews.begin(),
                                     m_apoOverviews.end());
            m_apoOverviews.clear();
        }
        m_osOverviewResampling = pszResampling;
        for (int i = 0; i < nOverviews; i++)
        {
            if (std::find(m_anOverviewFactors.begin(),
                          m_anOverviewFactors.end(),
                          panOverviewList[i]) == m_anOverviewFactors.end())
            {
                AddVirtualOverview(panOverviewList[i], pszResampling);
            }
        }
        return CE_None;
    }

    if (!oOvManager.IsInitialized())
    {
        const char *pszDesc = GetDescription();
        if (pszDesc[0])
        {
            oOvManager.Initialize(this, pszDesc);
        }
    }

    // Make implicit overviews invisible, but do not destroy them in case they
    // are already used.  Should the client do that?  Behavior might undefined
    // in GDAL API?
    if (!m_apoOverviews.empty())
    {
        m_apoOverviewsBak.insert(m_apoOverviewsBak.end(),
                                 m_apoOverviews.begin(), m_apoOverviews.end());
        m_apoOverviews.clear();
    }
    else
    {
        // Add a dummy overview so that GDALDataset::IBuildOverviews()
        // doesn't manage to get a virtual implicit overview.
        m_apoOverviews.push_back(nullptr);
    }

    CPLErr eErr = GDALDataset::IBuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
        pfnProgress, pProgressData, papszOptions);

    m_apoOverviews.clear();
    return eErr;
}

/************************************************************************/
/*                         GetShiftedDataset()                          */
/*                                                                      */
/* Returns true if the VRT is made of a single source that is a simple  */
/* in its full resolution.                                              */
/************************************************************************/

bool VRTDataset::GetShiftedDataset(int nXOff, int nYOff, int nXSize, int nYSize,
                                   GDALDataset *&poSrcDataset, int &nSrcXOff,
                                   int &nSrcYOff)
{
    if (!CheckCompatibleForDatasetIO())
        return false;

    VRTSourcedRasterBand *poVRTBand =
        static_cast<VRTSourcedRasterBand *>(papoBands[0]);
    if (poVRTBand->m_papoSources.size() != 1)
        return false;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->m_papoSources[0].get());

    GDALRasterBand *poBand = poSource->GetRasterBand();
    if (!poBand || poSource->GetMaskBandMainBand() ||
        poBand->GetRasterDataType() != poVRTBand->GetRasterDataType())
        return false;

    poSrcDataset = poBand->GetDataset();
    if (!poSrcDataset)
        return false;

    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;
    bool bError = false;
    if (!poSource->GetSrcDstWindow(nXOff, nYOff, nXSize, nYSize, nXSize, nYSize,
                                   &dfReqXOff, &dfReqYOff, &dfReqXSize,
                                   &dfReqYSize, &nReqXOff, &nReqYOff,
                                   &nReqXSize, &nReqYSize, &nOutXOff, &nOutYOff,
                                   &nOutXSize, &nOutYSize, bError))
        return false;

    if (nReqXSize != nXSize || nReqYSize != nYSize || nReqXSize != nOutXSize ||
        nReqYSize != nOutYSize)
        return false;

    nSrcXOff = nReqXOff;
    nSrcYOff = nReqYOff;
    return true;
}

/************************************************************************/
/*                       GetCompressionFormats()                        */
/************************************************************************/

CPLStringList VRTDataset::GetCompressionFormats(int nXOff, int nYOff,
                                                int nXSize, int nYSize,
                                                int nBandCount,
                                                const int *panBandList)
{
    GDALDataset *poSrcDataset;
    int nSrcXOff;
    int nSrcYOff;
    if (!GetShiftedDataset(nXOff, nYOff, nXSize, nYSize, poSrcDataset, nSrcXOff,
                           nSrcYOff))
        return CPLStringList();
    return poSrcDataset->GetCompressionFormats(nSrcXOff, nSrcYOff, nXSize,
                                               nYSize, nBandCount, panBandList);
}

/************************************************************************/
/*                       ReadCompressedData()                           */
/************************************************************************/

CPLErr VRTDataset::ReadCompressedData(const char *pszFormat, int nXOff,
                                      int nYOff, int nXSize, int nYSize,
                                      int nBandCount, const int *panBandList,
                                      void **ppBuffer, size_t *pnBufferSize,
                                      char **ppszDetailedFormat)
{
    GDALDataset *poSrcDataset;
    int nSrcXOff;
    int nSrcYOff;
    if (!GetShiftedDataset(nXOff, nYOff, nXSize, nYSize, poSrcDataset, nSrcXOff,
                           nSrcYOff))
        return CE_Failure;
    return poSrcDataset->ReadCompressedData(
        pszFormat, nSrcXOff, nSrcYOff, nXSize, nYSize, nBandCount, panBandList,
        ppBuffer, pnBufferSize, ppszDetailedFormat);
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void VRTDataset::ClearStatistics()
{
    for (int i = 1; i <= nBands; ++i)
    {
        bool bChanged = false;
        GDALRasterBand *poBand = GetRasterBand(i);
        CSLConstList papszOldMD = poBand->GetMetadata();
        CPLStringList aosNewMD;
        for (const char *pszMDItem : cpl::Iterate(papszOldMD))
        {
            if (STARTS_WITH_CI(pszMDItem, "STATISTICS_"))
            {
                bChanged = true;
            }
            else
            {
                aosNewMD.AddString(pszMDItem);
            }
        }
        if (bChanged)
        {
            poBand->SetMetadata(aosNewMD.List());
        }
    }

    GDALDataset::ClearStatistics();
}

/************************************************************************/
/*                 VRTMapSharedResources::LockGuard()                   */
/************************************************************************/

std::unique_ptr<std::lock_guard<std::mutex>>
VRTMapSharedResources::LockGuard() const
{
    std::unique_ptr<std::lock_guard<std::mutex>> poLockGuard;
    if (m_bUseMutex)
    {
        poLockGuard = std::make_unique<std::lock_guard<std::mutex>>(m_oMutex);
    }
    return poLockGuard;
}

/************************************************************************/
/*                   VRTMapSharedResources::Get()                       */
/************************************************************************/

GDALDataset *VRTMapSharedResources::Get(const std::string &osKey) const
{
    auto poLockGuard = LockGuard();
    CPL_IGNORE_RET_VAL(poLockGuard);
    auto oIter = m_oMap.find(osKey);
    GDALDataset *poRet = nullptr;
    if (oIter != m_oMap.end())
        poRet = oIter->second;
    return poRet;
}

/************************************************************************/
/*                   VRTMapSharedResources::Insert()                    */
/************************************************************************/

void VRTMapSharedResources::Insert(const std::string &osKey, GDALDataset *poDS)
{
    auto poLockGuard = LockGuard();
    CPL_IGNORE_RET_VAL(poLockGuard);
    m_oMap[osKey] = poDS;
}

/************************************************************************/
/*                   VRTMapSharedResources::InitMutex()                 */
/************************************************************************/

void VRTMapSharedResources::InitMutex()
{
    m_bUseMutex = true;
}

/*! @endcond */
