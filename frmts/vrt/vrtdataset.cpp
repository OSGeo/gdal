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

#include "vrtdataset.h"

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "gdal_utils.h"

#include <algorithm>
#include <typeinfo>
#include "gdal_proxy.h"

/*! @cond Doxygen_Suppress */

#define VRT_PROTOCOL_PREFIX "vrt://"

/************************************************************************/
/*                            VRTDataset()                             */
/************************************************************************/

VRTDataset::VRTDataset(int nXSize, int nYSize, int nBlockXSize, int nBlockYSize)
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
    m_bBlockSizeSpecified = nBlockXSize > 0 && nBlockYSize > 0;
    m_nBlockXSize = nBlockXSize > 0 ? nBlockXSize : std::min(128, nXSize);
    m_nBlockYSize = nBlockYSize > 0 ? nBlockYSize : std::min(128, nYSize);

    GDALRegister_VRT();

    poDriver = static_cast<GDALDriver *>(GDALGetDriverByName("VRT"));
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
    if (m_poSRS)
        m_poSRS->Release();
    if (m_poGCP_SRS)
        m_poGCP_SRS->Release();
    if (m_nGCPCount > 0)
    {
        GDALDeinitGCPs(m_nGCPCount, m_pasGCPList);
        CPLFree(m_pasGCPList);
    }
    CPLFree(m_pszVRTPath);

    delete m_poMaskBand;

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
    const std::string osVRTPath(CPLGetPath(obj.GetDescription()));
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
                ? CPLGetPath(pszDescription)
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
                    osCoordinateEpoch.resize(osCoordinateEpoch.size() - 1);
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
                       m_adfGeoTransform[0], m_adfGeoTransform[1],
                       m_adfGeoTransform[2], m_adfGeoTransform[3],
                       m_adfGeoTransform[4], m_adfGeoTransform[5]));
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
    if (m_nGCPCount > 0)
    {
        GDALSerializeGCPListToXML(psDSTree, m_pasGCPList, m_nGCPCount,
                                  m_poGCP_SRS);
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
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        CPLXMLNode *psBandTree = static_cast<VRTRasterBand *>(papoBands[iBand])
                                     ->SerializeToXML(pszVRTPathIn);

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
        CPLXMLNode *psBandTree = m_poMaskBand->SerializeToXML(pszVRTPathIn);

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
/*                             InitBand()                               */
/************************************************************************/

VRTRasterBand *VRTDataset::InitBand(const char *pszSubclass, int nBand,
                                    bool bAllowPansharpened)
{
    VRTRasterBand *poBand = nullptr;
    if (EQUAL(pszSubclass, "VRTSourcedRasterBand"))
        poBand = new VRTSourcedRasterBand(this, nBand);
    else if (EQUAL(pszSubclass, "VRTDerivedRasterBand"))
        poBand = new VRTDerivedRasterBand(this, nBand);
    else if (EQUAL(pszSubclass, "VRTRawRasterBand"))
        poBand = new VRTRawRasterBand(this, nBand);
    else if (EQUAL(pszSubclass, "VRTWarpedRasterBand") &&
             dynamic_cast<VRTWarpedDataset *>(this) != nullptr)
        poBand = new VRTWarpedRasterBand(this, nBand);
    else if (bAllowPansharpened &&
             EQUAL(pszSubclass, "VRTPansharpenedRasterBand") &&
             dynamic_cast<VRTPansharpenedDataset *>(this) != nullptr)
        poBand = new VRTPansharpenedRasterBand(this, nBand);
    else
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VRTRasterBand of unrecognized subclass '%s'.", pszSubclass);
    return poBand;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDataset::XMLInit(CPLXMLNode *psTree, const char *pszVRTPathIn)

{
    if (pszVRTPathIn != nullptr)
        m_pszVRTPath = CPLStrdup(pszVRTPathIn);

    /* -------------------------------------------------------------------- */
    /*      Check for an SRS node.                                          */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psSRSNode = CPLGetXMLNode(psTree, "SRS");
    if (psSRSNode)
    {
        if (m_poSRS)
            m_poSRS->Release();
        m_poSRS = new OGRSpatialReference();
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
    if (strlen(CPLGetXMLValue(psTree, "GeoTransform", "")) > 0)
    {
        const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
        char **papszTokens = CSLTokenizeStringComplex(pszGT, ",", FALSE, FALSE);
        if (CSLCount(papszTokens) != 6)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GeoTransform node does not have expected six values.");
        }
        else
        {
            for (int iTA = 0; iTA < 6; iTA++)
                m_adfGeoTransform[iTA] = CPLAtof(papszTokens[iTA]);
            m_bGeoTransformSet = TRUE;
        }

        CSLDestroy(papszTokens);
    }

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode(psTree, "GCPList");

    if (psGCPList != nullptr)
    {
        GDALDeserializeGCPListFromXML(psGCPList, &m_pasGCPList, &m_nGCPCount,
                                      &m_poGCP_SRS);
    }

    /* -------------------------------------------------------------------- */
    /*      Apply any dataset level metadata.                               */
    /* -------------------------------------------------------------------- */
    oMDMD.XMLInit(psTree, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Create dataset mask band.                                       */
    /* -------------------------------------------------------------------- */

    /* Parse dataset mask band first */
    CPLXMLNode *psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");

    CPLXMLNode *psChild = nullptr;
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

            VRTRasterBand *poBand = InitBand(pszSubclass, 0, false);
            if (poBand != nullptr &&
                poBand->XMLInit(psChild, pszVRTPathIn, m_oMapSharedSources) ==
                    CE_None)
            {
                SetMaskBand(poBand);
                break;
            }
            else
            {
                delete poBand;
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

            VRTRasterBand *poBand = InitBand(pszSubclass, l_nBands + 1, true);
            if (poBand != nullptr &&
                poBand->XMLInit(psChild, pszVRTPathIn, m_oMapSharedSources) ==
                    CE_None)
            {
                l_nBands++;
                SetBand(l_nBands, poBand);
            }
            else
            {
                delete poBand;
                return CE_Failure;
            }
        }
    }

    CPLXMLNode *psGroup = CPLGetXMLNode(psTree, "Group");
    if (psGroup)
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
    return m_nGCPCount;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *VRTDataset::GetGCPs()

{
    return m_pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr VRTDataset::SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                           const OGRSpatialReference *poGCP_SRS)

{
    if (m_poGCP_SRS)
        m_poGCP_SRS->Release();
    if (m_nGCPCount > 0)
    {
        GDALDeinitGCPs(m_nGCPCount, m_pasGCPList);
        CPLFree(m_pasGCPList);
    }

    m_poGCP_SRS = poGCP_SRS ? poGCP_SRS->Clone() : nullptr;

    m_nGCPCount = nGCPCountIn;

    m_pasGCPList = GDALDuplicateGCPs(nGCPCountIn, pasGCPListIn);

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr VRTDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    if (m_poSRS)
        m_poSRS->Release();
    if (poSRS)
        m_poSRS = poSRS->Clone();
    else
        m_poSRS = nullptr;

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::SetGeoTransform(double *padfGeoTransformIn)

{
    memcpy(m_adfGeoTransform, padfGeoTransformIn, sizeof(double) * 6);
    m_bGeoTransformSet = TRUE;

    SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::GetGeoTransform(double *padfGeoTransform)

{
    memcpy(padfGeoTransform, m_adfGeoTransform, sizeof(double) * 6);

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
        const char *currentVrtFilename =
            CPLProjectRelativeFilename(pszCurDir, poOpenInfo->pszFilename);
        CPLString osInitialCurrentVrtFilename(currentVrtFilename);
        CPLFree(pszCurDir);

#if defined(HAVE_READLINK) && defined(HAVE_LSTAT)
        char filenameBuffer[2048];

        while (true)
        {
            VSIStatBuf statBuffer;
            int lstatCode = lstat(currentVrtFilename, &statBuffer);
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
                             currentVrtFilename, VSIStrerror(errno));
                    return nullptr;
                }
            }

            if (!VSI_ISLNK(statBuffer.st_mode))
            {
                break;
            }

            const int bufferSize = static_cast<int>(readlink(
                currentVrtFilename, filenameBuffer, sizeof(filenameBuffer)));
            if (bufferSize != -1)
            {
                filenameBuffer[std::min(
                    bufferSize, static_cast<int>(sizeof(filenameBuffer)) - 1)] =
                    0;
                // The filename in filenameBuffer might be a relative path
                // from the linkfile resolve it before looping
                currentVrtFilename = CPLProjectRelativeFilename(
                    CPLGetDirname(currentVrtFilename), filenameBuffer);
            }
            else
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                CPLFree(pszXML);
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to read filename from symlink %s: %s",
                         currentVrtFilename, VSIStrerror(errno));
                return nullptr;
            }
        }
#endif  // HAVE_READLINK && HAVE_LSTAT

        if (osInitialCurrentVrtFilename == currentVrtFilename)
            pszVRTPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        else
            pszVRTPath = CPLStrdup(CPLGetPath(currentVrtFilename));

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
    VRTDataset *poDS = static_cast<VRTDataset *>(
        OpenXML(pszXML, pszVRTPath, poOpenInfo->eAccess));

    if (poDS != nullptr)
        poDS->m_bNeedsFlush = false;

    if (poDS != nullptr)
    {
        if (poDS->GetRasterCount() == 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 &&
            strstr(pszXML, "VRTPansharpenedDataset") == nullptr)
        {
            delete poDS;
            poDS = nullptr;
        }
        else if (poDS->GetRootGroup() == nullptr &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0)
        {
            delete poDS;
            poDS = nullptr;
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
            poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);
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
                    delete poDS;
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

    return poDS;
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
    CPLStringList aosTokens(CSLTokenizeString2(osQueryString, "&", 0));
    CPLStringList aosAllowedDrivers;
    CPLStringList aosOpenOptions;

    for (int i = 0; i < aosTokens.size(); i++)
    {
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue(aosTokens[i], &pszKey);
        if (pszKey && pszValue)
        {
            if (EQUAL(pszKey, "if"))
            {
                if (aosAllowedDrivers.size() > 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "'if' option should be specified once, use commas "
                             "to input multiple values.");
                    CPLFree(pszKey);
                    return nullptr;
                }
                aosAllowedDrivers = CSLTokenizeString2(pszValue, ",", 0);
            }
            if (EQUAL(pszKey, "oo"))
            {
                if (aosOpenOptions.size() > 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "'oo' option should be specified once, use commas "
                             "to input multiple values.");
                    CPLFree(pszKey);
                    return nullptr;
                }
                aosOpenOptions = CSLTokenizeString2(pszValue, ",", 0);
            }
        }
        if (!pszKey)
        {

            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid option specification: %s\n"
                     "must be in the form 'key=value'",
                     aosTokens[i]);
            return nullptr;
        }
        CPLFree(pszKey);
    }

    // We don't open in GDAL_OF_SHARED mode to avoid issues when we open a
    // http://.jp2 file with the JP2OpenJPEG driver through the HTTP driver,
    // which returns a /vsimem/ file
    auto poSrcDS =
        GDALDataset::Open(osFilename, GDAL_OF_RASTER, aosAllowedDrivers.List(),
                          aosOpenOptions.List(), nullptr);
    if (poSrcDS == nullptr)
    {
        return nullptr;
    }

    std::vector<int> anBands;

    CPLStringList argv;
    argv.AddString("-of");
    argv.AddString("VRT");

    for (int i = 0; i < aosTokens.size(); i++)
    {
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue(aosTokens[i], &pszKey);
        if (pszKey && pszValue)
        {
            if (EQUAL(pszKey, "bands"))
            {
                CPLStringList aosBands(CSLTokenizeString2(pszValue, ",", 0));
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
                            poSrcDS->ReleaseRef();
                            CPLFree(pszKey);
                            return nullptr;
                        }
                        anBands.push_back(nBand);
                    }
                }

                for (const int nBand : anBands)
                {
                    argv.AddString("-b");
                    argv.AddString(nBand == 0 ? "mask"
                                              : CPLSPrintf("%d", nBand));
                }
            }

            else if (EQUAL(pszKey, "a_srs"))
            {
                argv.AddString("-a_srs");
                argv.AddString(pszValue);
            }

            else if (EQUAL(pszKey, "a_ullr"))
            {

                // Parse the limits
                CPLStringList aosUllr(CSLTokenizeString2(pszValue, ",", 0));
                // fail if not four values
                if (aosUllr.size() != 4)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid a_ullr option: %s", pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                CPLStringList aosGCP(CSLTokenizeString2(pszValue, ",", 0));

                if (aosGCP.size() < 4 || aosGCP.size() > 5)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid value for GCP: %s\n  need 4, or 5 "
                             "numbers, comma separated: "
                             "'gcp=<pixel>,<line>,<easting>,<northing>[,<"
                             "elevation>]'",
                             pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                CPLStringList aosScaleParams(
                    CSLTokenizeString2(pszValue, ",", 0));

                if (!(aosScaleParams.size() == 2) &&
                    !(aosScaleParams.size() == 4) &&
                    !(aosScaleParams.size() == 1))
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid value for scale, (or scale_bn): "
                             "%s\n  need 'scale=true', or 2 or 4 "
                             "numbers, comma separated: "
                             "'scale=src_min,src_max[,dst_min,dst_max]' or "
                             "'scale_bn=src_min,src_max[,dst_min,dst_max]'",
                             pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
                    return nullptr;
                }

                // -scale because scale=true or scale=min,max or scale=min,max,dstmin,dstmax
                if (aosScaleParams.size() == 1 &&
                    CPLTestBool(aosScaleParams[0]))
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
                CPLStringList aosOutSize(CSLTokenizeString2(pszValue, ",", 0));
                if (aosOutSize.size() != 2)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid outsize option: %s, must be two"
                             "values separated by comma pixel,line or two "
                             "fraction values with percent symbol",
                             pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
                    return nullptr;
                }
                argv.AddString("-outsize");
                argv.AddString(aosOutSize[0]);
                argv.AddString(aosOutSize[1]);
            }
            else if (EQUAL(pszKey, "projwin"))
            {

                // Parse the limits
                CPLStringList aosProjWin(CSLTokenizeString2(pszValue, ",", 0));
                // fail if not four values
                if (aosProjWin.size() != 4)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid projwin option: %s", pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                CPLStringList aosTargetResolution(
                    CSLTokenizeString2(pszValue, ",", 0));
                if (aosTargetResolution.size() != 2)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid tr option: %s, must be two "
                             "values separated by comma xres,yres",
                             pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                CPLStringList aosSrcWin(CSLTokenizeString2(pszValue, ",", 0));
                // fail if not four values
                if (aosSrcWin.size() != 4)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid srcwin option: %s, must be four "
                             "values separated by comma xoff,yoff,xsize,ysize",
                             pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                CPLStringList aosAGeoTransform(
                    CSLTokenizeString2(pszValue, ",", 0));
                // fail if not six values
                if (aosAGeoTransform.size() != 6)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid a_gt option: %s", pszValue);
                    poSrcDS->ReleaseRef();
                    CPLFree(pszKey);
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
                poSrcDS->ReleaseRef();
                CPLFree(pszKey);
                return nullptr;
            }
        }
        CPLFree(pszKey);
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(argv.List(), nullptr);

    auto hRet =
        GDALTranslate("", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr);

    GDALTranslateOptionsFree(psOptions);

    // Situation where we open a http://.jp2 file with the JP2OpenJPEG driver
    // through the HTTP driver, which returns a /vsimem/ file
    const bool bPatchSourceFilename =
        (STARTS_WITH(osFilename.c_str(), "http://") ||
         STARTS_WITH(osFilename.c_str(), "https://")) &&
        osFilename != poSrcDS->GetDescription();

    poSrcDS->ReleaseRef();

    auto poDS = cpl::down_cast<VRTDataset *>(GDALDataset::FromHandle(hRet));
    if (poDS)
    {
        if (bPatchSourceFilename)
        {
            for (int i = 0; i < poDS->nBands; ++i)
            {
                auto poBand =
                    dynamic_cast<VRTSourcedRasterBand *>(poDS->papoBands[i]);
                if (poBand && poBand->nSources == 1 &&
                    poBand->papoSources[0]->IsSimpleSource())
                {
                    auto poSource = cpl::down_cast<VRTSimpleSource *>(
                        poBand->papoSources[0]);
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

GDALDataset *VRTDataset::OpenXML(const char *pszXML, const char *pszVRTPath,
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

    if (!bIsPansharpened && CPLGetXMLNode(psRoot, "Group") == nullptr &&
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

    if (!bIsPansharpened && CPLGetXMLNode(psRoot, "VRTRasterBand") != nullptr &&
        !GDALCheckDatasetDimensions(nXSize, nYSize))
    {
        return nullptr;
    }

    VRTDataset *poDS = nullptr;
    if (strcmp(pszSubClass, "VRTWarpedDataset") == 0)
        poDS = new VRTWarpedDataset(nXSize, nYSize);
    else if (bIsPansharpened)
        poDS = new VRTPansharpenedDataset(nXSize, nYSize);
    else
    {
        poDS = new VRTDataset(nXSize, nYSize);
        poDS->eAccess = eAccessIn;
    }

    if (poDS->XMLInit(psRoot, pszVRTPath) != CE_None)
    {
        delete poDS;
        poDS = nullptr;
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
    SetNeedsFlush();

    /* ==================================================================== */
    /*      Handle a new raw band.                                          */
    /* ==================================================================== */
    const char *pszSubClass = CSLFetchNameValue(papszOptions, "subclass");

    if (pszSubClass != nullptr && EQUAL(pszSubClass, "VRTRawRasterBand"))
    {
        const int nWordDataSize = GDALGetDataTypeSizeBytes(eType);

        /* --------------------------------------------------------------------
         */
        /*      Collect required information. */
        /* --------------------------------------------------------------------
         */
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

        /* --------------------------------------------------------------------
         */
        /*      Create and initialize the band. */
        /* --------------------------------------------------------------------
         */

        VRTRawRasterBand *poBand =
            new VRTRawRasterBand(this, GetRasterCount() + 1, eType);

        char *l_pszVRTPath = CPLStrdup(CPLGetPath(GetDescription()));
        if (EQUAL(l_pszVRTPath, ""))
        {
            CPLFree(l_pszVRTPath);
            l_pszVRTPath = nullptr;
        }

        const CPLErr eErr = poBand->SetRawLink(
            pszFilename, l_pszVRTPath, bRelativeToVRT, nImageOffset,
            nPixelOffset, nLineOffset, pszByteOrder);
        CPLFree(l_pszVRTPath);
        if (eErr != CE_None)
        {
            delete poBand;
            return eErr;
        }

        SetBand(GetRasterCount() + 1, poBand);

        return CE_None;
    }

    /* ==================================================================== */
    /*      Handle a new "sourced" band.                                    */
    /* ==================================================================== */
    else
    {
        VRTSourcedRasterBand *poBand = nullptr;

        /* ---- Check for our sourced band 'derived' subclass ---- */
        if (pszSubClass != nullptr &&
            EQUAL(pszSubClass, "VRTDerivedRasterBand"))
        {

            /* We'll need a pointer to the subclass in case we need */
            /* to set the new band's pixel function below. */
            VRTDerivedRasterBand *poDerivedBand =
                new VRTDerivedRasterBand(this, GetRasterCount() + 1, eType,
                                         GetRasterXSize(), GetRasterYSize());

            /* Set the pixel function options it provided. */
            const char *pszFuncName =
                CSLFetchNameValue(papszOptions, "PixelFunctionType");
            if (pszFuncName != nullptr)
                poDerivedBand->SetPixelFunctionName(pszFuncName);

            const char *pszLanguage =
                CSLFetchNameValue(papszOptions, "PixelFunctionLanguage");
            if (pszLanguage != nullptr)
                poDerivedBand->SetPixelFunctionLanguage(pszLanguage);

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
            /* we can assigned the base class pointer now. */
            poBand = poDerivedBand;
        }
        else
        {
            int nBlockXSizeIn =
                atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "0"));
            int nBlockYSizeIn =
                atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "0"));
            if (nBlockXSizeIn == 0 && nBlockYSizeIn == 0)
            {
                nBlockXSizeIn = m_nBlockXSize;
                nBlockYSizeIn = m_nBlockYSize;
            }
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
    if (STARTS_WITH_CI(pszName, "<VRTDataset"))
    {
        GDALDataset *poDS = OpenXML(pszName, nullptr, GA_Update);
        if (poDS != nullptr)
            poDS->SetDescription("<FromXML>");
        return poDS;
    }

    const char *pszSubclass = CSLFetchNameValue(papszOptions, "SUBCLASS");

    VRTDataset *poDS = nullptr;

    const int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "0"));
    const int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "0"));
    if (pszSubclass == nullptr || EQUAL(pszSubclass, "VRTDataset"))
        poDS = new VRTDataset(nXSize, nYSize, nBlockXSize, nBlockYSize);
    else if (EQUAL(pszSubclass, "VRTWarpedDataset"))
    {
        poDS = new VRTWarpedDataset(nXSize, nYSize, nBlockXSize, nBlockYSize);
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

    poDS->oOvManager.Initialize(poDS, pszName);

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

    SetMaskBand(new VRTSourcedRasterBand(this, 0));

    return CE_None;
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTDataset::SetMaskBand(VRTRasterBand *poMaskBandIn)
{
    delete m_poMaskBand;
    m_poMaskBand = poMaskBandIn;
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

int VRTDataset::CheckCompatibleForDatasetIO()
{
    int nSources = 0;
    VRTSource **papoSources = nullptr;
    CPLString osResampling;

    if (m_bCompatibleForDatasetIO >= 0)
    {
        return m_bCompatibleForDatasetIO;
    }

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (!static_cast<VRTRasterBand *>(papoBands[iBand])
                 ->IsSourcedRasterBand())
            return FALSE;

        VRTSourcedRasterBand *poBand =
            static_cast<VRTSourcedRasterBand *>(papoBands[iBand]);

        // Do not allow VRTDerivedRasterBand for example
        if (typeid(*poBand) != typeid(VRTSourcedRasterBand))
            return FALSE;

        if (iBand == 0)
        {
            nSources = poBand->nSources;
            papoSources = poBand->papoSources;
            for (int iSource = 0; iSource < nSources; iSource++)
            {
                if (!papoSources[iSource]->IsSimpleSource())
                    return FALSE;

                VRTSimpleSource *poSource =
                    static_cast<VRTSimpleSource *>(papoSources[iSource]);
                if (!EQUAL(poSource->GetType(), "SimpleSource"))
                    return FALSE;

                if (poSource->m_nBand != iBand + 1 ||
                    poSource->m_bGetMaskBand || poSource->m_osSrcDSName.empty())
                    return FALSE;
                osResampling = poSource->GetResampling();
            }
        }
        else if (nSources != poBand->nSources)
        {
            return FALSE;
        }
        else
        {
            for (int iSource = 0; iSource < nSources; iSource++)
            {
                if (!poBand->papoSources[iSource]->IsSimpleSource())
                    return FALSE;
                VRTSimpleSource *poRefSource =
                    static_cast<VRTSimpleSource *>(papoSources[iSource]);

                VRTSimpleSource *poSource = static_cast<VRTSimpleSource *>(
                    poBand->papoSources[iSource]);
                if (!EQUAL(poSource->GetType(), "SimpleSource"))
                    return FALSE;
                if (poSource->m_nBand != iBand + 1 ||
                    poSource->m_bGetMaskBand || poSource->m_osSrcDSName.empty())
                    return FALSE;
                if (!poSource->IsSameExceptBandNumber(poRefSource))
                    return FALSE;
                if (osResampling.compare(poSource->GetResampling()) != 0)
                    return FALSE;
            }
        }
    }

    return nSources != 0;
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
    if (poVRTBand->nSources != 1)
        return nullptr;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->papoSources[0]);

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
    if (poVRTBand->nSources != 1)
        return CE_None;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->papoSources[0]);

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
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg)
{
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
            const int nSavedSources = poBand->nSources;
            poBand->nSources = 0;

            GByte *pabyBandData =
                static_cast<GByte *>(pData) + iBandIndex * nBandSpace;

            poBand->IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                              pabyBandData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace, psExtraArg);

            poBand->nSources = nSavedSources;
        }

        CPLErr eErr = CE_None;
        GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
        void *pProgressDataGlobal = psExtraArg->pProgressData;

        // Use the last band, because when sources reference a GDALProxyDataset,
        // they don't necessary instantiate all underlying rasterbands.
        VRTSourcedRasterBand *poBand =
            static_cast<VRTSourcedRasterBand *>(papoBands[nBands - 1]);
        for (int iSource = 0; eErr == CE_None && iSource < poBand->nSources;
             iSource++)
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData = GDALCreateScaledProgress(
                1.0 * iSource / poBand->nSources,
                1.0 * (iSource + 1) / poBand->nSources, pfnProgressGlobal,
                pProgressDataGlobal);

            VRTSimpleSource *poSource =
                static_cast<VRTSimpleSource *>(poBand->papoSources[iSource]);

            eErr = poSource->DatasetRasterIO(
                poBand->GetRasterDataType(), nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, psExtraArg);

            GDALDestroyScaledProgress(psExtraArg->pProgressData);
        }

        psExtraArg->pfnProgress = pfnProgressGlobal;
        psExtraArg->pProgressData = pProgressDataGlobal;

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
        const int nSources = poBand->nSources;
        VRTSource **papoSources = poBand->papoSources;
        for (int iSource = 0; iSource < nSources; iSource++)
        {
            if (!papoSources[iSource]->IsSimpleSource())
                continue;

            VRTSimpleSource *poSource =
                static_cast<VRTSimpleSource *>(papoSources[iSource]);
            poSource->UnsetPreservedRelativeFilenames();
        }
    }
}

/************************************************************************/
/*                        BuildVirtualOverviews()                       */
/************************************************************************/

static bool CheckBandForOverview(GDALRasterBand *poBand,
                                 GDALRasterBand *&poFirstBand, int &nOverviews,
                                 std::vector<GDALDataset *> &apoOverviewsBak)
{
    if (!cpl::down_cast<VRTRasterBand *>(poBand)->IsSourcedRasterBand())
        return false;

    VRTSourcedRasterBand *poVRTBand =
        cpl::down_cast<VRTSourcedRasterBand *>(poBand);
    if (poVRTBand->nSources != 1)
        return false;
    if (!poVRTBand->papoSources[0]->IsSimpleSource())
        return false;

    VRTSimpleSource *poSource =
        cpl::down_cast<VRTSimpleSource *>(poVRTBand->papoSources[0]);
    if (!EQUAL(poSource->GetType(), "SimpleSource") &&
        !EQUAL(poSource->GetType(), "ComplexSource"))
        return false;
    GDALRasterBand *poSrcBand = poBand->GetBand() == 0
                                    ? poSource->GetMaskBandMainBand()
                                    : poSource->GetRasterBand();
    if (poSrcBand == nullptr)
        return false;

    // To prevent recursion
    apoOverviewsBak.push_back(nullptr);
    const int nOvrCount = poSrcBand->GetOverviewCount();
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

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (!CheckBandForOverview(papoBands[iBand], poFirstBand, nOverviews,
                                  m_apoOverviewsBak))
            return;
    }

    if (m_poMaskBand)
    {
        if (!CheckBandForOverview(m_poMaskBand, poFirstBand, nOverviews,
                                  m_apoOverviewsBak))
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
        cpl::down_cast<VRTSimpleSource *>(l_poVRTBand->papoSources[0]);
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
        const int nOvrXSize = static_cast<int>(0.5 + nRasterXSize * dfXRatio);
        const int nOvrYSize = static_cast<int>(0.5 + nRasterYSize * dfYRatio);
        if (nOvrXSize < 128 || nOvrYSize < 128)
            break;
        VRTDataset *poOvrVDS = new VRTDataset(nOvrXSize, nOvrYSize);
        m_apoOverviews.push_back(poOvrVDS);

        const auto CreateOverviewBand =
            [&poOvrVDS, nOvrXSize, nOvrYSize, dfXRatio,
             dfYRatio](VRTSourcedRasterBand *poVRTBand)
        {
            VRTSourcedRasterBand *poOvrVRTBand = new VRTSourcedRasterBand(
                poOvrVDS, poVRTBand->GetBand(), poVRTBand->GetRasterDataType(),
                nOvrXSize, nOvrYSize);
            poOvrVRTBand->CopyCommonInfoFrom(poVRTBand);
            poOvrVRTBand->m_bNoDataValueSet = poVRTBand->m_bNoDataValueSet;
            poOvrVRTBand->m_dfNoDataValue = poVRTBand->m_dfNoDataValue;
            poOvrVRTBand->m_bHideNoDataValue = poVRTBand->m_bHideNoDataValue;

            VRTSimpleSource *poSrcSource =
                cpl::down_cast<VRTSimpleSource *>(poVRTBand->papoSources[0]);
            VRTSimpleSource *poNewSource = nullptr;
            if (EQUAL(poSrcSource->GetType(), "SimpleSource"))
            {
                poNewSource =
                    new VRTSimpleSource(poSrcSource, dfXRatio, dfYRatio);
            }
            else if (EQUAL(poSrcSource->GetType(), "ComplexSource"))
            {
                poNewSource = new VRTComplexSource(
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
                poOvrVRTBand->AddSource(poNewSource);
            }

            return poOvrVRTBand;
        };

        for (int i = 0; i < nBands; i++)
        {
            VRTSourcedRasterBand *poSrcBand =
                cpl::down_cast<VRTSourcedRasterBand *>(GetRasterBand(i + 1));
            auto poOvrVRTBand = CreateOverviewBand(poSrcBand);
            poOvrVDS->SetBand(poOvrVDS->GetRasterCount() + 1, poOvrVRTBand);
        }

        if (m_poMaskBand)
        {
            VRTSourcedRasterBand *poSrcBand =
                cpl::down_cast<VRTSourcedRasterBand *>(m_poMaskBand);
            auto poOvrVRTBand = CreateOverviewBand(poSrcBand);
            poOvrVDS->SetMaskBand(poOvrVRTBand);
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
    m_apoOverviews.resize(m_apoOverviews.size() - 1);

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
    if (poVRTBand->nSources != 1)
        return false;

    VRTSimpleSource *poSource =
        static_cast<VRTSimpleSource *>(poVRTBand->papoSources[0]);

    GDALRasterBand *poBand = poSource->GetRasterBand();
    if (!poBand || poSource->GetMaskBandMainBand())
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
        char **papszOldMD = poBand->GetMetadata();
        char **papszNewMD = nullptr;
        for (char **papszIter = papszOldMD; papszIter && papszIter[0];
             ++papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "STATISTICS_"))
            {
                bChanged = true;
            }
            else
            {
                papszNewMD = CSLAddString(papszNewMD, *papszIter);
            }
        }
        if (bChanged)
        {
            poBand->SetMetadata(papszNewMD);
        }
        CSLDestroy(papszNewMD);
    }

    GDALDataset::ClearStatistics();
}

/*! @endcond */
