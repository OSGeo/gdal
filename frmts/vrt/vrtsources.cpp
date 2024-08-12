/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTSimpleSource, VRTFuncSource and
 *           VRTAveragedSource.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_vrt.h"
#include "vrtdataset.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "gdal_priv_templates.hpp"

/*! @cond Doxygen_Suppress */

// #define DEBUG_VERBOSE 1

// See #5459
#ifdef isnan
#define HAS_ISNAN_MACRO
#endif
#include <algorithm>
#if defined(HAS_ISNAN_MACRO) && !defined(isnan)
#define isnan std::isnan
#endif

/************************************************************************/
/* ==================================================================== */
/*                             VRTSource                                */
/* ==================================================================== */
/************************************************************************/

VRTSource::~VRTSource()
{
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSource::GetFileList(char *** /* ppapszFileList */, int * /* pnSize */,
                            int * /* pnMaxSize */, CPLHashSet * /* hSetFiles */)
{
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTSimpleSource                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTSimpleSource()                           */
/************************************************************************/

VRTSimpleSource::VRTSimpleSource() = default;

/************************************************************************/
/*                          VRTSimpleSource()                           */
/************************************************************************/

VRTSimpleSource::VRTSimpleSource(const VRTSimpleSource *poSrcSource,
                                 double dfXDstRatio, double dfYDstRatio)
    : m_poMapSharedSources(poSrcSource->m_poMapSharedSources),
      m_poRasterBand(poSrcSource->m_poRasterBand),
      m_poMaskBandMainBand(poSrcSource->m_poMaskBandMainBand),
      m_aosOpenOptions(poSrcSource->m_aosOpenOptions),
      m_nBand(poSrcSource->m_nBand),
      m_bGetMaskBand(poSrcSource->m_bGetMaskBand),
      m_dfSrcXOff(poSrcSource->m_dfSrcXOff),
      m_dfSrcYOff(poSrcSource->m_dfSrcYOff),
      m_dfSrcXSize(poSrcSource->m_dfSrcXSize),
      m_dfSrcYSize(poSrcSource->m_dfSrcYSize),
      m_nMaxValue(poSrcSource->m_nMaxValue), m_bRelativeToVRTOri(-1),
      m_nExplicitSharedStatus(poSrcSource->m_nExplicitSharedStatus),
      m_osSrcDSName(poSrcSource->m_osSrcDSName),
      m_bDropRefOnSrcBand(poSrcSource->m_bDropRefOnSrcBand)
{
    if (!poSrcSource->IsSrcWinSet() && !poSrcSource->IsDstWinSet() &&
        (dfXDstRatio != 1.0 || dfYDstRatio != 1.0))
    {
        auto l_band = GetRasterBand();
        if (l_band)
        {
            m_dfSrcXOff = 0;
            m_dfSrcYOff = 0;
            m_dfSrcXSize = l_band->GetXSize();
            m_dfSrcYSize = l_band->GetYSize();
            m_dfDstXOff = 0;
            m_dfDstYOff = 0;
            m_dfDstXSize = l_band->GetXSize() * dfXDstRatio;
            m_dfDstYSize = l_band->GetYSize() * dfYDstRatio;
        }
    }
    else if (poSrcSource->IsDstWinSet())
    {
        m_dfDstXOff = poSrcSource->m_dfDstXOff * dfXDstRatio;
        m_dfDstYOff = poSrcSource->m_dfDstYOff * dfYDstRatio;
        m_dfDstXSize = poSrcSource->m_dfDstXSize * dfXDstRatio;
        m_dfDstYSize = poSrcSource->m_dfDstYSize * dfYDstRatio;
    }
}

/************************************************************************/
/*                          ~VRTSimpleSource()                          */
/************************************************************************/

VRTSimpleSource::~VRTSimpleSource()

{
    if (!m_bDropRefOnSrcBand)
        return;

    if (m_poMaskBandMainBand != nullptr)
    {
        if (m_poMaskBandMainBand->GetDataset() != nullptr)
        {
            m_poMaskBandMainBand->GetDataset()->ReleaseRef();
        }
    }
    else if (m_poRasterBand != nullptr &&
             m_poRasterBand->GetDataset() != nullptr)
    {
        m_poRasterBand->GetDataset()->ReleaseRef();
    }
}

/************************************************************************/
/*                           FlushCache()                               */
/************************************************************************/

CPLErr VRTSimpleSource::FlushCache(bool bAtClosing)

{
    if (m_poMaskBandMainBand != nullptr)
    {
        return m_poMaskBandMainBand->FlushCache(bAtClosing);
    }
    else if (m_poRasterBand != nullptr)
    {
        return m_poRasterBand->FlushCache(bAtClosing);
    }
    return CE_None;
}

/************************************************************************/
/*                    UnsetPreservedRelativeFilenames()                 */
/************************************************************************/

void VRTSimpleSource::UnsetPreservedRelativeFilenames()
{
    if (!STARTS_WITH(m_osSourceFileNameOri.c_str(), "http://") &&
        !STARTS_WITH(m_osSourceFileNameOri.c_str(), "https://"))
    {
        m_bRelativeToVRTOri = -1;
        m_osSourceFileNameOri = "";
    }
}

/************************************************************************/
/*                             SetSrcBand()                             */
/************************************************************************/

void VRTSimpleSource::SetSrcBand(const char *pszFilename, int nBand)

{
    m_nBand = nBand;
    m_osSrcDSName = pszFilename;
}

/************************************************************************/
/*                             SetSrcBand()                             */
/************************************************************************/

void VRTSimpleSource::SetSrcBand(GDALRasterBand *poNewSrcBand)

{
    m_poRasterBand = poNewSrcBand;
    m_nBand = m_poRasterBand->GetBand();
    auto poDS = poNewSrcBand->GetDataset();
    if (poDS != nullptr)
    {
        m_osSrcDSName = poDS->GetDescription();
        m_aosOpenOptions = CSLDuplicate(poDS->GetOpenOptions());
    }
}

/************************************************************************/
/*                          SetSrcMaskBand()                            */
/************************************************************************/

// poSrcBand is not the mask band, but the band from which the mask band is
// taken.
void VRTSimpleSource::SetSrcMaskBand(GDALRasterBand *poNewSrcBand)

{
    m_poRasterBand = poNewSrcBand->GetMaskBand();
    m_poMaskBandMainBand = poNewSrcBand;
    m_nBand = poNewSrcBand->GetBand();
    auto poDS = poNewSrcBand->GetDataset();
    if (poDS != nullptr)
    {
        m_osSrcDSName = poDS->GetDescription();
        m_aosOpenOptions = CSLDuplicate(poDS->GetOpenOptions());
    }
    m_bGetMaskBand = true;
}

/************************************************************************/
/*                         RoundIfCloseToInt()                          */
/************************************************************************/

static double RoundIfCloseToInt(double dfValue)
{
    double dfClosestInt = floor(dfValue + 0.5);
    return (fabs(dfValue - dfClosestInt) < 1e-3) ? dfClosestInt : dfValue;
}

/************************************************************************/
/*                            SetSrcWindow()                            */
/************************************************************************/

void VRTSimpleSource::SetSrcWindow(double dfNewXOff, double dfNewYOff,
                                   double dfNewXSize, double dfNewYSize)

{
    m_dfSrcXOff = RoundIfCloseToInt(dfNewXOff);
    m_dfSrcYOff = RoundIfCloseToInt(dfNewYOff);
    m_dfSrcXSize = RoundIfCloseToInt(dfNewXSize);
    m_dfSrcYSize = RoundIfCloseToInt(dfNewYSize);
}

/************************************************************************/
/*                            SetDstWindow()                            */
/************************************************************************/

void VRTSimpleSource::SetDstWindow(double dfNewXOff, double dfNewYOff,
                                   double dfNewXSize, double dfNewYSize)

{
    m_dfDstXOff = RoundIfCloseToInt(dfNewXOff);
    m_dfDstYOff = RoundIfCloseToInt(dfNewYOff);
    m_dfDstXSize = RoundIfCloseToInt(dfNewXSize);
    m_dfDstYSize = RoundIfCloseToInt(dfNewYSize);
}

/************************************************************************/
/*                            GetDstWindow()                            */
/************************************************************************/

void VRTSimpleSource::GetDstWindow(double &dfDstXOff, double &dfDstYOff,
                                   double &dfDstXSize, double &dfDstYSize)
{
    dfDstXOff = m_dfDstXOff;
    dfDstYOff = m_dfDstYOff;
    dfDstXSize = m_dfDstXSize;
    dfDstYSize = m_dfDstYSize;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

static bool IsSlowSource(const char *pszSrcName)
{
    return strstr(pszSrcName, "/vsicurl/http") != nullptr ||
           strstr(pszSrcName, "/vsicurl/ftp") != nullptr ||
           (strstr(pszSrcName, "/vsicurl?") != nullptr &&
            strstr(pszSrcName, "&url=http") != nullptr);
}

CPLXMLNode *VRTSimpleSource::SerializeToXML(const char *pszVRTPath)

{
    CPLXMLNode *const psSrc =
        CPLCreateXMLNode(nullptr, CXT_Element, "SimpleSource");

    if (!m_osResampling.empty())
    {
        CPLCreateXMLNode(CPLCreateXMLNode(psSrc, CXT_Attribute, "resampling"),
                         CXT_Text, m_osResampling.c_str());
    }

    VSIStatBufL sStat;
    int bRelativeToVRT = FALSE;  // TODO(schwehr): Make this a bool?
    std::string osSourceFilename;

    if (m_bRelativeToVRTOri >= 0)
    {
        osSourceFilename = m_osSourceFileNameOri;
        bRelativeToVRT = m_bRelativeToVRTOri;
    }
    else if (IsSlowSource(m_osSrcDSName))
    {
        // Testing the existence of remote resources can be excruciating
        // slow, so let's just suppose they exist.
        osSourceFilename = m_osSrcDSName;
        bRelativeToVRT = FALSE;
    }
    // If this isn't actually a file, don't even try to know if it is a
    // relative path. It can't be !, and unfortunately CPLIsFilenameRelative()
    // can only work with strings that are filenames To be clear
    // NITF_TOC_ENTRY:CADRG_JOG-A_250K_1_0:some_path isn't a relative file
    // path.
    else if (VSIStatExL(m_osSrcDSName, &sStat, VSI_STAT_EXISTS_FLAG) != 0)
    {
        osSourceFilename = m_osSrcDSName;
        bRelativeToVRT = FALSE;

        // Try subdatasetinfo API first
        // Note: this will become the only branch when subdatasetinfo will become
        //       available for NITF_IM, RASTERLITE and TILEDB
        const auto oSubDSInfo{GDALGetSubdatasetInfo(osSourceFilename.c_str())};
        if (oSubDSInfo && !oSubDSInfo->GetPathComponent().empty())
        {
            auto path{oSubDSInfo->GetPathComponent()};
            std::string relPath{CPLExtractRelativePath(pszVRTPath, path.c_str(),
                                                       &bRelativeToVRT)};
            osSourceFilename = oSubDSInfo->ModifyPathComponent(relPath);
            GDALDestroySubdatasetInfo(oSubDSInfo);
        }
        else
        {
            for (const char *pszSyntax : VRTDataset::apszSpecialSyntax)
            {
                CPLString osPrefix(pszSyntax);
                osPrefix.resize(strchr(pszSyntax, ':') - pszSyntax + 1);
                if (pszSyntax[osPrefix.size()] == '"')
                    osPrefix += '"';
                if (EQUALN(osSourceFilename.c_str(), osPrefix, osPrefix.size()))
                {
                    if (STARTS_WITH_CI(pszSyntax + osPrefix.size(), "{ANY}"))
                    {
                        const char *pszLastPart =
                            strrchr(osSourceFilename.c_str(), ':') + 1;
                        // CSV:z:/foo.xyz
                        if ((pszLastPart[0] == '/' || pszLastPart[0] == '\\') &&
                            pszLastPart - osSourceFilename.c_str() >= 3 &&
                            pszLastPart[-3] == ':')
                            pszLastPart -= 2;
                        CPLString osPrefixFilename(osSourceFilename);
                        osPrefixFilename.resize(pszLastPart -
                                                osSourceFilename.c_str());
                        osSourceFilename = CPLExtractRelativePath(
                            pszVRTPath, pszLastPart, &bRelativeToVRT);
                        osSourceFilename = osPrefixFilename + osSourceFilename;
                    }
                    else if (STARTS_WITH_CI(pszSyntax + osPrefix.size(),
                                            "{FILENAME}"))
                    {
                        CPLString osFilename(osSourceFilename.c_str() +
                                             osPrefix.size());
                        size_t nPos = 0;
                        if (osFilename.size() >= 3 && osFilename[1] == ':' &&
                            (osFilename[2] == '\\' || osFilename[2] == '/'))
                            nPos = 2;
                        nPos = osFilename.find(
                            pszSyntax[osPrefix.size() + strlen("{FILENAME}")],
                            nPos);
                        if (nPos != std::string::npos)
                        {
                            const CPLString osSuffix = osFilename.substr(nPos);
                            osFilename.resize(nPos);
                            osSourceFilename = CPLExtractRelativePath(
                                pszVRTPath, osFilename, &bRelativeToVRT);
                            osSourceFilename =
                                osPrefix + osSourceFilename + osSuffix;
                        }
                    }
                    break;
                }
            }
        }
    }
    else
    {
        std::string osVRTFilename = pszVRTPath;
        std::string osSourceDataset = m_osSrcDSName;
        char *pszCurDir = CPLGetCurrentDir();
        if (CPLIsFilenameRelative(osSourceDataset.c_str()) &&
            !CPLIsFilenameRelative(osVRTFilename.c_str()) &&
            pszCurDir != nullptr)
        {
            osSourceDataset =
                CPLFormFilename(pszCurDir, osSourceDataset.c_str(), nullptr);
        }
        else if (!CPLIsFilenameRelative(osSourceDataset.c_str()) &&
                 CPLIsFilenameRelative(osVRTFilename.c_str()) &&
                 pszCurDir != nullptr)
        {
            osVRTFilename =
                CPLFormFilename(pszCurDir, osVRTFilename.c_str(), nullptr);
        }
        CPLFree(pszCurDir);
        osSourceFilename = CPLExtractRelativePath(
            osVRTFilename.c_str(), osSourceDataset.c_str(), &bRelativeToVRT);
    }

    CPLSetXMLValue(psSrc, "SourceFilename", osSourceFilename.c_str());

    CPLCreateXMLNode(CPLCreateXMLNode(CPLGetXMLNode(psSrc, "SourceFilename"),
                                      CXT_Attribute, "relativeToVRT"),
                     CXT_Text, bRelativeToVRT ? "1" : "0");

    // Determine if we must write the shared attribute. The config option
    // will override the m_nExplicitSharedStatus value
    const char *pszShared = CPLGetConfigOption("VRT_SHARED_SOURCE", nullptr);
    if ((pszShared == nullptr && m_nExplicitSharedStatus == 0) ||
        (pszShared != nullptr && !CPLTestBool(pszShared)))
    {
        CPLCreateXMLNode(
            CPLCreateXMLNode(CPLGetXMLNode(psSrc, "SourceFilename"),
                             CXT_Attribute, "shared"),
            CXT_Text, "0");
    }

    GDALSerializeOpenOptionsToXML(psSrc, m_aosOpenOptions.List());

    if (m_bGetMaskBand)
        CPLSetXMLValue(psSrc, "SourceBand", CPLSPrintf("mask,%d", m_nBand));
    else
        CPLSetXMLValue(psSrc, "SourceBand", CPLSPrintf("%d", m_nBand));

    // TODO: in a later version, no longer emit SourceProperties, which
    // is no longer used by GDAL 3.4
    if (m_poRasterBand)
    {
        /* Write a few additional useful properties of the dataset */
        /* so that we can use a proxy dataset when re-opening. See XMLInit() */
        /* below */
        CPLSetXMLValue(psSrc, "SourceProperties.#RasterXSize",
                       CPLSPrintf("%d", m_poRasterBand->GetXSize()));
        CPLSetXMLValue(psSrc, "SourceProperties.#RasterYSize",
                       CPLSPrintf("%d", m_poRasterBand->GetYSize()));
        CPLSetXMLValue(
            psSrc, "SourceProperties.#DataType",
            GDALGetDataTypeName(m_poRasterBand->GetRasterDataType()));

        int nBlockXSize = 0;
        int nBlockYSize = 0;
        m_poRasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

        CPLSetXMLValue(psSrc, "SourceProperties.#BlockXSize",
                       CPLSPrintf("%d", nBlockXSize));
        CPLSetXMLValue(psSrc, "SourceProperties.#BlockYSize",
                       CPLSPrintf("%d", nBlockYSize));
    }

    if (IsSrcWinSet())
    {
        CPLSetXMLValue(psSrc, "SrcRect.#xOff",
                       CPLSPrintf("%.15g", m_dfSrcXOff));
        CPLSetXMLValue(psSrc, "SrcRect.#yOff",
                       CPLSPrintf("%.15g", m_dfSrcYOff));
        CPLSetXMLValue(psSrc, "SrcRect.#xSize",
                       CPLSPrintf("%.15g", m_dfSrcXSize));
        CPLSetXMLValue(psSrc, "SrcRect.#ySize",
                       CPLSPrintf("%.15g", m_dfSrcYSize));
    }

    if (IsDstWinSet())
    {
        CPLSetXMLValue(psSrc, "DstRect.#xOff",
                       CPLSPrintf("%.15g", m_dfDstXOff));
        CPLSetXMLValue(psSrc, "DstRect.#yOff",
                       CPLSPrintf("%.15g", m_dfDstYOff));
        CPLSetXMLValue(psSrc, "DstRect.#xSize",
                       CPLSPrintf("%.15g", m_dfDstXSize));
        CPLSetXMLValue(psSrc, "DstRect.#ySize",
                       CPLSPrintf("%.15g", m_dfDstYSize));
    }

    return psSrc;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr
VRTSimpleSource::XMLInit(const CPLXMLNode *psSrc, const char *pszVRTPath,
                         std::map<CPLString, GDALDataset *> &oMapSharedSources)

{
    m_poMapSharedSources = &oMapSharedSources;

    m_osResampling = CPLGetXMLValue(psSrc, "resampling", "");

    /* -------------------------------------------------------------------- */
    /*      Prepare filename.                                               */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *psSourceFileNameNode =
        CPLGetXMLNode(psSrc, "SourceFilename");
    const char *pszFilename =
        psSourceFileNameNode ? CPLGetXMLValue(psSourceFileNameNode, nullptr, "")
                             : "";

    if (pszFilename[0] == '\0')
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Missing <SourceFilename> element in VRTRasterBand.");
        return CE_Failure;
    }

    // Backup original filename and relativeToVRT so as to be able to
    // serialize them identically again (#5985)
    m_osSourceFileNameOri = pszFilename;
    m_bRelativeToVRTOri =
        atoi(CPLGetXMLValue(psSourceFileNameNode, "relativetoVRT", "0"));
    const char *pszShared =
        CPLGetXMLValue(psSourceFileNameNode, "shared", nullptr);
    if (pszShared == nullptr)
    {
        pszShared = CPLGetConfigOption("VRT_SHARED_SOURCE", nullptr);
    }
    if (pszShared != nullptr)
    {
        m_nExplicitSharedStatus = CPLTestBool(pszShared);
    }

    m_osSrcDSName = VRTDataset::BuildSourceFilename(
        pszFilename, pszVRTPath, CPL_TO_BOOL(m_bRelativeToVRTOri));

    const char *pszSourceBand = CPLGetXMLValue(psSrc, "SourceBand", "1");
    m_bGetMaskBand = false;
    if (STARTS_WITH_CI(pszSourceBand, "mask"))
    {
        m_bGetMaskBand = true;
        if (pszSourceBand[4] == ',')
            m_nBand = atoi(pszSourceBand + 5);
        else
            m_nBand = 1;
    }
    else
    {
        m_nBand = atoi(pszSourceBand);
    }
    if (!GDALCheckBandCount(m_nBand, 0))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid <SourceBand> element in VRTRasterBand.");
        return CE_Failure;
    }

    m_aosOpenOptions = GDALDeserializeOpenOptionsFromXML(psSrc);
    if (strstr(m_osSrcDSName.c_str(), "<VRTDataset") != nullptr)
        m_aosOpenOptions.SetNameValue("ROOT_PATH", pszVRTPath);

    return ParseSrcRectAndDstRect(psSrc);
}

/************************************************************************/
/*                        ParseSrcRectAndDstRect()                      */
/************************************************************************/

CPLErr VRTSimpleSource::ParseSrcRectAndDstRect(const CPLXMLNode *psSrc)
{
    const auto GetAttrValue = [](const CPLXMLNode *psNode,
                                 const char *pszAttrName, double dfDefaultVal)
    {
        if (const char *pszVal = CPLGetXMLValue(psNode, pszAttrName, nullptr))
            return CPLAtof(pszVal);
        else
            return dfDefaultVal;
    };

    /* -------------------------------------------------------------------- */
    /*      Set characteristics.                                            */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *const psSrcRect = CPLGetXMLNode(psSrc, "SrcRect");
    if (psSrcRect)
    {
        double xOff = GetAttrValue(psSrcRect, "xOff", UNINIT_WINDOW);
        double yOff = GetAttrValue(psSrcRect, "yOff", UNINIT_WINDOW);
        double xSize = GetAttrValue(psSrcRect, "xSize", UNINIT_WINDOW);
        double ySize = GetAttrValue(psSrcRect, "ySize", UNINIT_WINDOW);
        // Test written that way to catch NaN values
        if (!(xOff >= INT_MIN && xOff <= INT_MAX) ||
            !(yOff >= INT_MIN && yOff <= INT_MAX) ||
            !(xSize > 0 || xSize == UNINIT_WINDOW) || xSize > INT_MAX ||
            !(ySize > 0 || ySize == UNINIT_WINDOW) || ySize > INT_MAX)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong values in SrcRect");
            return CE_Failure;
        }
        SetSrcWindow(xOff, yOff, xSize, ySize);
    }
    else
    {
        m_dfSrcXOff = UNINIT_WINDOW;
        m_dfSrcYOff = UNINIT_WINDOW;
        m_dfSrcXSize = UNINIT_WINDOW;
        m_dfSrcYSize = UNINIT_WINDOW;
    }

    const CPLXMLNode *const psDstRect = CPLGetXMLNode(psSrc, "DstRect");
    if (psDstRect)
    {
        double xOff = GetAttrValue(psDstRect, "xOff", UNINIT_WINDOW);
        ;
        double yOff = GetAttrValue(psDstRect, "yOff", UNINIT_WINDOW);
        double xSize = GetAttrValue(psDstRect, "xSize", UNINIT_WINDOW);
        ;
        double ySize = GetAttrValue(psDstRect, "ySize", UNINIT_WINDOW);
        // Test written that way to catch NaN values
        if (!(xOff >= INT_MIN && xOff <= INT_MAX) ||
            !(yOff >= INT_MIN && yOff <= INT_MAX) ||
            !(xSize > 0 || xSize == UNINIT_WINDOW) || xSize > INT_MAX ||
            !(ySize > 0 || ySize == UNINIT_WINDOW) || ySize > INT_MAX)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong values in DstRect");
            return CE_Failure;
        }
        SetDstWindow(xOff, yOff, xSize, ySize);
    }
    else
    {
        m_dfDstXOff = UNINIT_WINDOW;
        m_dfDstYOff = UNINIT_WINDOW;
        m_dfDstXSize = UNINIT_WINDOW;
        m_dfDstYSize = UNINIT_WINDOW;
    }

    return CE_None;
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSimpleSource::GetFileList(char ***ppapszFileList, int *pnSize,
                                  int *pnMaxSize, CPLHashSet *hSetFiles)
{
    if (!m_osSrcDSName.empty())
    {
        const char *pszFilename = m_osSrcDSName.c_str();

        /* --------------------------------------------------------------------
         */
        /*      Is it already in the list ? */
        /* --------------------------------------------------------------------
         */
        if (CPLHashSetLookup(hSetFiles, pszFilename) != nullptr)
            return;

        /* --------------------------------------------------------------------
         */
        /*      Grow array if necessary */
        /* --------------------------------------------------------------------
         */
        if (*pnSize + 1 >= *pnMaxSize)
        {
            *pnMaxSize = std::max(*pnSize + 2, 2 + 2 * (*pnMaxSize));
            *ppapszFileList = static_cast<char **>(
                CPLRealloc(*ppapszFileList, sizeof(char *) * (*pnMaxSize)));
        }

        /* --------------------------------------------------------------------
         */
        /*      Add the string to the list */
        /* --------------------------------------------------------------------
         */
        (*ppapszFileList)[*pnSize] = CPLStrdup(pszFilename);
        (*ppapszFileList)[(*pnSize + 1)] = nullptr;
        CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);

        (*pnSize)++;
    }
}

/************************************************************************/
/*                           OpenSource()                               */
/************************************************************************/

void VRTSimpleSource::OpenSource() const
{
    CPLAssert(m_poRasterBand == nullptr);

    /* ----------------------------------------------------------------- */
    /*      Create a proxy dataset                                       */
    /* ----------------------------------------------------------------- */
    GDALProxyPoolDataset *proxyDS = nullptr;
    std::string osKeyMapSharedSources;
    if (m_poMapSharedSources)
    {
        osKeyMapSharedSources = m_osSrcDSName;
        for (int i = 0; i < m_aosOpenOptions.size(); ++i)
        {
            osKeyMapSharedSources += "||";
            osKeyMapSharedSources += m_aosOpenOptions[i];
        }

        auto oIter = m_poMapSharedSources->find(osKeyMapSharedSources);
        if (oIter != m_poMapSharedSources->end())
            proxyDS = cpl::down_cast<GDALProxyPoolDataset *>(oIter->second);
    }

    if (proxyDS == nullptr)
    {
        int bShared = true;
        if (m_nExplicitSharedStatus != -1)
            bShared = m_nExplicitSharedStatus;

        const CPLString osUniqueHandle(CPLSPrintf("%p", m_poMapSharedSources));
        proxyDS = GDALProxyPoolDataset::Create(
            m_osSrcDSName, m_aosOpenOptions.List(), GA_ReadOnly, bShared,
            osUniqueHandle.c_str());
        if (proxyDS == nullptr)
            return;
    }
    else
    {
        proxyDS->Reference();
    }

    if (m_bGetMaskBand)
    {
        GDALProxyPoolRasterBand *poMaskBand =
            cpl::down_cast<GDALProxyPoolRasterBand *>(
                proxyDS->GetRasterBand(m_nBand));
        poMaskBand->AddSrcMaskBandDescriptionFromUnderlying();
    }

    /* -------------------------------------------------------------------- */
    /*      Get the raster band.                                            */
    /* -------------------------------------------------------------------- */

    m_poRasterBand = proxyDS->GetRasterBand(m_nBand);
    if (m_poRasterBand == nullptr || !ValidateOpenedBand(m_poRasterBand))
    {
        proxyDS->ReleaseRef();
        return;
    }

    if (m_bGetMaskBand)
    {
        m_poRasterBand = m_poRasterBand->GetMaskBand();
        if (m_poRasterBand == nullptr)
        {
            proxyDS->ReleaseRef();
            return;
        }
        m_poMaskBandMainBand = m_poRasterBand;
    }

    if (m_poMapSharedSources)
    {
        (*m_poMapSharedSources)[osKeyMapSharedSources] = proxyDS;
    }
}

/************************************************************************/
/*                         GetRasterBand()                              */
/************************************************************************/

GDALRasterBand *VRTSimpleSource::GetRasterBand() const
{
    if (m_poRasterBand == nullptr)
        OpenSource();
    return m_poRasterBand;
}

/************************************************************************/
/*                        GetMaskBandMainBand()                         */
/************************************************************************/

GDALRasterBand *VRTSimpleSource::GetMaskBandMainBand()
{
    if (m_poRasterBand == nullptr)
        OpenSource();
    return m_poMaskBandMainBand;
}

/************************************************************************/
/*                       IsSameExceptBandNumber()                       */
/************************************************************************/

int VRTSimpleSource::IsSameExceptBandNumber(VRTSimpleSource *poOtherSource)
{
    return m_dfSrcXOff == poOtherSource->m_dfSrcXOff &&
           m_dfSrcYOff == poOtherSource->m_dfSrcYOff &&
           m_dfSrcXSize == poOtherSource->m_dfSrcXSize &&
           m_dfSrcYSize == poOtherSource->m_dfSrcYSize &&
           m_dfDstXOff == poOtherSource->m_dfDstXOff &&
           m_dfDstYOff == poOtherSource->m_dfDstYOff &&
           m_dfDstXSize == poOtherSource->m_dfDstXSize &&
           m_dfDstYSize == poOtherSource->m_dfDstYSize &&
           !m_osSrcDSName.empty() &&
           m_osSrcDSName == poOtherSource->m_osSrcDSName;
}

/************************************************************************/
/*                              SrcToDst()                              */
/*                                                                      */
/*      Note: this is a no-op if the both src and dst windows are unset */
/************************************************************************/

void VRTSimpleSource::SrcToDst(double dfX, double dfY, double &dfXOut,
                               double &dfYOut) const

{
    dfXOut = ((dfX - m_dfSrcXOff) / m_dfSrcXSize) * m_dfDstXSize + m_dfDstXOff;
    dfYOut = ((dfY - m_dfSrcYOff) / m_dfSrcYSize) * m_dfDstYSize + m_dfDstYOff;
}

/************************************************************************/
/*                              DstToSrc()                              */
/*                                                                      */
/*      Note: this is a no-op if the both src and dst windows are unset */
/************************************************************************/

void VRTSimpleSource::DstToSrc(double dfX, double dfY, double &dfXOut,
                               double &dfYOut) const

{
    dfXOut = ((dfX - m_dfDstXOff) / m_dfDstXSize) * m_dfSrcXSize + m_dfSrcXOff;
    dfYOut = ((dfY - m_dfDstYOff) / m_dfDstYSize) * m_dfSrcYSize + m_dfSrcYOff;
}

/************************************************************************/
/*                          GetSrcDstWindow()                           */
/************************************************************************/

int VRTSimpleSource::GetSrcDstWindow(
    double dfXOff, double dfYOff, double dfXSize, double dfYSize, int nBufXSize,
    int nBufYSize, double *pdfReqXOff, double *pdfReqYOff, double *pdfReqXSize,
    double *pdfReqYSize, int *pnReqXOff, int *pnReqYOff, int *pnReqXSize,
    int *pnReqYSize, int *pnOutXOff, int *pnOutYOff, int *pnOutXSize,
    int *pnOutYSize, bool &bErrorOut)

{
    bErrorOut = false;

    if (m_dfSrcXSize == 0.0 || m_dfSrcYSize == 0.0 || m_dfDstXSize == 0.0 ||
        m_dfDstYSize == 0.0)
    {
        return FALSE;
    }

    const bool bDstWinSet = IsDstWinSet();

#ifdef DEBUG
    const bool bSrcWinSet = IsSrcWinSet();

    if (bSrcWinSet != bDstWinSet)
    {
        return FALSE;
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      If the input window completely misses the portion of the        */
    /*      virtual dataset provided by this source we have nothing to do.  */
    /* -------------------------------------------------------------------- */
    if (bDstWinSet)
    {
        if (dfXOff >= m_dfDstXOff + m_dfDstXSize ||
            dfYOff >= m_dfDstYOff + m_dfDstYSize ||
            dfXOff + dfXSize <= m_dfDstXOff || dfYOff + dfYSize <= m_dfDstYOff)
            return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      This request window corresponds to the whole output buffer.     */
    /* -------------------------------------------------------------------- */
    *pnOutXOff = 0;
    *pnOutYOff = 0;
    *pnOutXSize = nBufXSize;
    *pnOutYSize = nBufYSize;

    /* -------------------------------------------------------------------- */
    /*      If the input window extents outside the portion of the on       */
    /*      the virtual file that this source can set, then clip down       */
    /*      the requested window.                                           */
    /* -------------------------------------------------------------------- */
    bool bModifiedX = false;
    bool bModifiedY = false;
    double dfRXOff = dfXOff;
    double dfRYOff = dfYOff;
    double dfRXSize = dfXSize;
    double dfRYSize = dfYSize;

    if (bDstWinSet)
    {
        if (dfRXOff < m_dfDstXOff)
        {
            dfRXSize = dfRXSize + dfRXOff - m_dfDstXOff;
            dfRXOff = m_dfDstXOff;
            bModifiedX = true;
        }

        if (dfRYOff < m_dfDstYOff)
        {
            dfRYSize = dfRYSize + dfRYOff - m_dfDstYOff;
            dfRYOff = m_dfDstYOff;
            bModifiedY = true;
        }

        if (dfRXOff + dfRXSize > m_dfDstXOff + m_dfDstXSize)
        {
            dfRXSize = m_dfDstXOff + m_dfDstXSize - dfRXOff;
            bModifiedX = true;
        }

        if (dfRYOff + dfRYSize > m_dfDstYOff + m_dfDstYSize)
        {
            dfRYSize = m_dfDstYOff + m_dfDstYSize - dfRYOff;
            bModifiedY = true;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate requested region in virtual file into the source      */
    /*      band coordinates.                                               */
    /* -------------------------------------------------------------------- */
    const double dfScaleX = m_dfSrcXSize / m_dfDstXSize;
    const double dfScaleY = m_dfSrcYSize / m_dfDstYSize;

    *pdfReqXOff = (dfRXOff - m_dfDstXOff) * dfScaleX + m_dfSrcXOff;
    *pdfReqYOff = (dfRYOff - m_dfDstYOff) * dfScaleY + m_dfSrcYOff;
    *pdfReqXSize = dfRXSize * dfScaleX;
    *pdfReqYSize = dfRYSize * dfScaleY;

    if (!CPLIsFinite(*pdfReqXOff) || !CPLIsFinite(*pdfReqYOff) ||
        !CPLIsFinite(*pdfReqXSize) || !CPLIsFinite(*pdfReqYSize) ||
        *pdfReqXOff > INT_MAX || *pdfReqYOff > INT_MAX || *pdfReqXSize < 0 ||
        *pdfReqYSize < 0)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Clamp within the bounds of the available source data.           */
    /* -------------------------------------------------------------------- */
    if (*pdfReqXOff < 0)
    {
        *pdfReqXSize += *pdfReqXOff;
        *pdfReqXOff = 0;
        bModifiedX = true;
    }
    if (*pdfReqYOff < 0)
    {
        *pdfReqYSize += *pdfReqYOff;
        *pdfReqYOff = 0;
        bModifiedY = true;
    }

    *pnReqXOff = static_cast<int>(floor(*pdfReqXOff));
    *pnReqYOff = static_cast<int>(floor(*pdfReqYOff));

    constexpr double EPS = 1e-3;
    constexpr double ONE_MINUS_EPS = 1.0 - EPS;
    if (*pdfReqXOff - *pnReqXOff > ONE_MINUS_EPS)
    {
        (*pnReqXOff)++;
        *pdfReqXOff = *pnReqXOff;
    }
    if (*pdfReqYOff - *pnReqYOff > ONE_MINUS_EPS)
    {
        (*pnReqYOff)++;
        *pdfReqYOff = *pnReqYOff;
    }

    if (*pdfReqXSize > INT_MAX)
        *pnReqXSize = INT_MAX;
    else
        *pnReqXSize = static_cast<int>(floor(*pdfReqXSize + 0.5));

    if (*pdfReqYSize > INT_MAX)
        *pnReqYSize = INT_MAX;
    else
        *pnReqYSize = static_cast<int>(floor(*pdfReqYSize + 0.5));

    /* -------------------------------------------------------------------- */
    /*      Clamp within the bounds of the available source data.           */
    /* -------------------------------------------------------------------- */

    if (*pnReqXSize == 0)
        *pnReqXSize = 1;
    if (*pnReqYSize == 0)
        *pnReqYSize = 1;

    auto l_band = GetRasterBand();
    if (!l_band)
    {
        bErrorOut = true;
        return FALSE;
    }
    if (*pnReqXSize > INT_MAX - *pnReqXOff ||
        *pnReqXOff + *pnReqXSize > l_band->GetXSize())
    {
        *pnReqXSize = l_band->GetXSize() - *pnReqXOff;
        bModifiedX = true;
    }
    if (*pdfReqXOff + *pdfReqXSize > l_band->GetXSize())
    {
        *pdfReqXSize = l_band->GetXSize() - *pdfReqXOff;
        bModifiedX = true;
    }

    if (*pnReqYSize > INT_MAX - *pnReqYOff ||
        *pnReqYOff + *pnReqYSize > l_band->GetYSize())
    {
        *pnReqYSize = l_band->GetYSize() - *pnReqYOff;
        bModifiedY = true;
    }
    if (*pdfReqYOff + *pdfReqYSize > l_band->GetYSize())
    {
        *pdfReqYSize = l_band->GetYSize() - *pdfReqYOff;
        bModifiedY = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Don't do anything if the requesting region is completely off    */
    /*      the source image.                                               */
    /* -------------------------------------------------------------------- */
    if (*pnReqXOff >= l_band->GetXSize() || *pnReqYOff >= l_band->GetYSize() ||
        *pnReqXSize <= 0 || *pnReqYSize <= 0)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      If we haven't had to modify the source rectangle, then the      */
    /*      destination rectangle must be the whole region.                 */
    /* -------------------------------------------------------------------- */
    if (bModifiedX || bModifiedY)
    {
        /* --------------------------------------------------------------------
         */
        /*      Now transform this possibly reduced request back into the */
        /*      destination buffer coordinates in case the output region is */
        /*      less than the whole buffer. */
        /* --------------------------------------------------------------------
         */
        double dfDstULX = 0.0;
        double dfDstULY = 0.0;
        double dfDstLRX = 0.0;
        double dfDstLRY = 0.0;

        SrcToDst(*pdfReqXOff, *pdfReqYOff, dfDstULX, dfDstULY);
        SrcToDst(*pdfReqXOff + *pdfReqXSize, *pdfReqYOff + *pdfReqYSize,
                 dfDstLRX, dfDstLRY);
#if DEBUG_VERBOSE
        CPLDebug("VRT", "dfDstULX=%g dfDstULY=%g dfDstLRX=%g dfDstLRY=%g",
                 dfDstULX, dfDstULY, dfDstLRX, dfDstLRY);
#endif

        if (bModifiedX)
        {
            const double dfScaleWinToBufX = nBufXSize / dfXSize;

            const double dfOutXOff = (dfDstULX - dfXOff) * dfScaleWinToBufX;
            if (dfOutXOff <= 0)
                *pnOutXOff = 0;
            else if (dfOutXOff > INT_MAX)
                *pnOutXOff = INT_MAX;
            else
                *pnOutXOff = static_cast<int>(dfOutXOff + EPS);

            // Apply correction on floating-point source window
            {
                double dfDstDeltaX =
                    (dfOutXOff - *pnOutXOff) / dfScaleWinToBufX;
                double dfSrcDeltaX = dfDstDeltaX / m_dfDstXSize * m_dfSrcXSize;
                *pdfReqXOff -= dfSrcDeltaX;
                *pdfReqXSize = std::min(*pdfReqXSize + dfSrcDeltaX,
                                        static_cast<double>(INT_MAX));
            }

            double dfOutRightXOff = (dfDstLRX - dfXOff) * dfScaleWinToBufX;
            if (dfOutRightXOff < dfOutXOff)
                return FALSE;
            if (dfOutRightXOff > INT_MAX)
                dfOutRightXOff = INT_MAX;
            const int nOutRightXOff =
                static_cast<int>(ceil(dfOutRightXOff - EPS));
            *pnOutXSize = nOutRightXOff - *pnOutXOff;

            if (*pnOutXSize > INT_MAX - *pnOutXOff ||
                *pnOutXOff + *pnOutXSize > nBufXSize)
                *pnOutXSize = nBufXSize - *pnOutXOff;

            // Apply correction on floating-point source window
            {
                double dfDstDeltaX =
                    (nOutRightXOff - dfOutRightXOff) / dfScaleWinToBufX;
                double dfSrcDeltaX = dfDstDeltaX / m_dfDstXSize * m_dfSrcXSize;
                *pdfReqXSize = std::min(*pdfReqXSize + dfSrcDeltaX,
                                        static_cast<double>(INT_MAX));
            }
        }

        if (bModifiedY)
        {
            const double dfScaleWinToBufY = nBufYSize / dfYSize;

            const double dfOutYOff = (dfDstULY - dfYOff) * dfScaleWinToBufY;
            if (dfOutYOff <= 0)
                *pnOutYOff = 0;
            else if (dfOutYOff > INT_MAX)
                *pnOutYOff = INT_MAX;
            else
                *pnOutYOff = static_cast<int>(dfOutYOff + EPS);

            // Apply correction on floating-point source window
            {
                double dfDstDeltaY =
                    (dfOutYOff - *pnOutYOff) / dfScaleWinToBufY;
                double dfSrcDeltaY = dfDstDeltaY / m_dfDstYSize * m_dfSrcYSize;
                *pdfReqYOff -= dfSrcDeltaY;
                *pdfReqYSize = std::min(*pdfReqYSize + dfSrcDeltaY,
                                        static_cast<double>(INT_MAX));
            }

            double dfOutTopYOff = (dfDstLRY - dfYOff) * dfScaleWinToBufY;
            if (dfOutTopYOff < dfOutYOff)
                return FALSE;
            if (dfOutTopYOff > INT_MAX)
                dfOutTopYOff = INT_MAX;
            const int nOutTopYOff = static_cast<int>(ceil(dfOutTopYOff - EPS));
            *pnOutYSize = nOutTopYOff - *pnOutYOff;

            if (*pnOutYSize > INT_MAX - *pnOutYOff ||
                *pnOutYOff + *pnOutYSize > nBufYSize)
                *pnOutYSize = nBufYSize - *pnOutYOff;

            // Apply correction on floating-point source window
            {
                double dfDstDeltaY =
                    (nOutTopYOff - dfOutTopYOff) / dfScaleWinToBufY;
                double dfSrcDeltaY = dfDstDeltaY / m_dfDstYSize * m_dfSrcYSize;
                *pdfReqYSize = std::min(*pdfReqYSize + dfSrcDeltaY,
                                        static_cast<double>(INT_MAX));
            }
        }

        if (*pnOutXSize < 1 || *pnOutYSize < 1)
            return FALSE;
    }

    *pdfReqXOff = RoundIfCloseToInt(*pdfReqXOff);
    *pdfReqYOff = RoundIfCloseToInt(*pdfReqYOff);
    *pdfReqXSize = RoundIfCloseToInt(*pdfReqXSize);
    *pdfReqYSize = RoundIfCloseToInt(*pdfReqYSize);

    return TRUE;
}

/************************************************************************/
/*                          NeedMaxValAdjustment()                      */
/************************************************************************/

int VRTSimpleSource::NeedMaxValAdjustment() const
{
    if (!m_nMaxValue)
        return FALSE;

    auto l_band = GetRasterBand();
    if (!l_band)
        return FALSE;
    const char *pszNBITS = l_band->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    const int nBits = (pszNBITS) ? atoi(pszNBITS) : 0;
    if (nBits >= 1 && nBits <= 31)
    {
        const int nBandMaxValue = static_cast<int>((1U << nBits) - 1);
        return nBandMaxValue > m_nMaxValue;
    }
    return TRUE;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTSimpleSource::RasterIO(GDALDataType eVRTBandDataType, int nXOff,
                                 int nYOff, int nXSize, int nYSize, void *pData,
                                 int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType, GSpacing nPixelSpace,
                                 GSpacing nLineSpace,
                                 GDALRasterIOExtraArg *psExtraArgIn,
                                 WorkingState & /*oWorkingState*/)

{
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg *psExtraArg = &sExtraArg;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArgIn != nullptr && psExtraArgIn->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArgIn->dfXOff;
        dfYOff = psExtraArgIn->dfYOff;
        dfXSize = psExtraArgIn->dfXSize;
        dfYSize = psExtraArgIn->dfYSize;
    }

    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    if (!GetSrcDstWindow(dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                         &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                         &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                         &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }
#if DEBUG_VERBOSE
    CPLDebug("VRT",
             "nXOff=%d, nYOff=%d, nXSize=%d, nYSize=%d, nBufXSize=%d, "
             "nBufYSize=%d,\n"
             "dfReqXOff=%g, dfReqYOff=%g, dfReqXSize=%g, dfReqYSize=%g,\n"
             "nReqXOff=%d, nReqYOff=%d, nReqXSize=%d, nReqYSize=%d,\n"
             "nOutXOff=%d, nOutYOff=%d, nOutXSize=%d, nOutYSize=%d",
             nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, dfReqXOff,
             dfReqYOff, dfReqXSize, dfReqYSize, nReqXOff, nReqYOff, nReqXSize,
             nReqYSize, nOutXOff, nOutYOff, nOutXSize, nOutYSize);
#endif

    /* -------------------------------------------------------------------- */
    /*      Actually perform the IO request.                                */
    /* -------------------------------------------------------------------- */
    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if (psExtraArgIn != nullptr)
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    GByte *pabyOut = static_cast<unsigned char *>(pData) +
                     nOutXOff * nPixelSpace +
                     static_cast<GPtrDiff_t>(nOutYOff) * nLineSpace;

    auto l_band = GetRasterBand();
    if (!l_band)
        return CE_Failure;

    CPLErr eErr = CE_Failure;
    if (GDALDataTypeIsConversionLossy(l_band->GetRasterDataType(),
                                      eVRTBandDataType))
    {
        const int nBandDTSize = GDALGetDataTypeSizeBytes(eVRTBandDataType);
        void *pTemp = VSI_MALLOC3_VERBOSE(nOutXSize, nOutYSize, nBandDTSize);
        if (pTemp)
        {
            eErr = l_band->RasterIO(GF_Read, nReqXOff, nReqYOff, nReqXSize,
                                    nReqYSize, pTemp, nOutXSize, nOutYSize,
                                    eVRTBandDataType, 0, 0, psExtraArg);
            if (eErr == CE_None)
            {
                GByte *pabyTemp = static_cast<GByte *>(pTemp);
                for (int iY = 0; iY < nOutYSize; iY++)
                {
                    GDALCopyWords(
                        pabyTemp +
                            static_cast<size_t>(iY) * nBandDTSize * nOutXSize,
                        eVRTBandDataType, nBandDTSize,
                        pabyOut + static_cast<GPtrDiff_t>(iY * nLineSpace),
                        eBufType, static_cast<int>(nPixelSpace), nOutXSize);
                }
            }
            VSIFree(pTemp);
        }
    }
    else
    {
        eErr = l_band->RasterIO(GF_Read, nReqXOff, nReqYOff, nReqXSize,
                                nReqYSize, pabyOut, nOutXSize, nOutYSize,
                                eBufType, nPixelSpace, nLineSpace, psExtraArg);
    }

    if (NeedMaxValAdjustment())
    {
        for (int j = 0; j < nOutYSize; j++)
        {
            for (int i = 0; i < nOutXSize; i++)
            {
                int nVal = 0;
                GDALCopyWords(pabyOut + j * nLineSpace + i * nPixelSpace,
                              eBufType, 0, &nVal, GDT_Int32, 0, 1);
                if (nVal > m_nMaxValue)
                    nVal = m_nMaxValue;
                GDALCopyWords(&nVal, GDT_Int32, 0,
                              pabyOut + j * nLineSpace + i * nPixelSpace,
                              eBufType, 0, 1);
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTSimpleSource::GetMinimum(int nXSize, int nYSize, int *pbSuccess)
{
    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    auto l_band = GetRasterBand();
    if (!l_band ||
        !GetSrcDstWindow(0, 0, nXSize, nYSize, nXSize, nYSize, &dfReqXOff,
                         &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                         &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff,
                         &nOutYOff, &nOutXSize, &nOutYSize, bError) ||
        nReqXOff != 0 || nReqYOff != 0 || nReqXSize != l_band->GetXSize() ||
        nReqYSize != l_band->GetYSize())
    {
        *pbSuccess = FALSE;
        return 0;
    }

    const double dfVal = l_band->GetMinimum(pbSuccess);
    if (NeedMaxValAdjustment() && dfVal > m_nMaxValue)
        return m_nMaxValue;
    return dfVal;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTSimpleSource::GetMaximum(int nXSize, int nYSize, int *pbSuccess)
{
    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    auto l_band = GetRasterBand();
    if (!l_band ||
        !GetSrcDstWindow(0, 0, nXSize, nYSize, nXSize, nYSize, &dfReqXOff,
                         &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                         &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff,
                         &nOutYOff, &nOutXSize, &nOutYSize, bError) ||
        nReqXOff != 0 || nReqYOff != 0 || nReqXSize != l_band->GetXSize() ||
        nReqYSize != l_band->GetYSize())
    {
        *pbSuccess = FALSE;
        return 0;
    }

    const double dfVal = l_band->GetMaximum(pbSuccess);
    if (NeedMaxValAdjustment() && dfVal > m_nMaxValue)
        return m_nMaxValue;
    return dfVal;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTSimpleSource::GetHistogram(int nXSize, int nYSize, double dfMin,
                                     double dfMax, int nBuckets,
                                     GUIntBig *panHistogram,
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    auto l_band = GetRasterBand();
    if (!l_band || NeedMaxValAdjustment() ||
        !GetSrcDstWindow(0, 0, nXSize, nYSize, nXSize, nYSize, &dfReqXOff,
                         &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                         &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff,
                         &nOutYOff, &nOutXSize, &nOutYSize, bError) ||
        nReqXOff != 0 || nReqYOff != 0 || nReqXSize != l_band->GetXSize() ||
        nReqYSize != l_band->GetYSize())
    {
        return CE_Failure;
    }

    return l_band->GetHistogram(dfMin, dfMax, nBuckets, panHistogram,
                                bIncludeOutOfRange, bApproxOK, pfnProgress,
                                pProgressData);
}

/************************************************************************/
/*                          DatasetRasterIO()                           */
/************************************************************************/

CPLErr VRTSimpleSource::DatasetRasterIO(
    GDALDataType eVRTBandDataType, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap, GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArgIn)
{
    if (!EQUAL(GetType(), "SimpleSource"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DatasetRasterIO() not implemented for %s", GetType());
        return CE_Failure;
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg *psExtraArg = &sExtraArg;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArgIn != nullptr && psExtraArgIn->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArgIn->dfXOff;
        dfYOff = psExtraArgIn->dfYOff;
        dfXSize = psExtraArgIn->dfXSize;
        dfYSize = psExtraArgIn->dfYSize;
    }

    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    if (!GetSrcDstWindow(dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                         &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                         &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                         &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }

    auto l_band = GetRasterBand();
    if (!l_band)
        return CE_Failure;

    GDALDataset *poDS = l_band->GetDataset();
    if (poDS == nullptr)
        return CE_Failure;

    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if (psExtraArgIn != nullptr)
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    GByte *pabyOut = static_cast<unsigned char *>(pData) +
                     nOutXOff * nPixelSpace +
                     static_cast<GPtrDiff_t>(nOutYOff) * nLineSpace;

    CPLErr eErr = CE_Failure;

    if (GDALDataTypeIsConversionLossy(l_band->GetRasterDataType(),
                                      eVRTBandDataType))
    {
        const int nBandDTSize = GDALGetDataTypeSizeBytes(eVRTBandDataType);
        void *pTemp = VSI_MALLOC3_VERBOSE(
            nOutXSize, nOutYSize, cpl::fits_on<int>(nBandDTSize * nBandCount));
        if (pTemp)
        {
            eErr = poDS->RasterIO(GF_Read, nReqXOff, nReqYOff, nReqXSize,
                                  nReqYSize, pTemp, nOutXSize, nOutYSize,
                                  eVRTBandDataType, nBandCount, panBandMap, 0,
                                  0, 0, psExtraArg);
            if (eErr == CE_None)
            {
                GByte *pabyTemp = static_cast<GByte *>(pTemp);
                const size_t nSrcBandSpace =
                    static_cast<size_t>(nOutYSize) * nOutXSize * nBandDTSize;
                for (int iBand = 0; iBand < nBandCount; iBand++)
                {
                    for (int iY = 0; iY < nOutYSize; iY++)
                    {
                        GDALCopyWords(
                            pabyTemp + iBand * nSrcBandSpace +
                                static_cast<size_t>(iY) * nBandDTSize *
                                    nOutXSize,
                            eVRTBandDataType, nBandDTSize,
                            pabyOut + static_cast<GPtrDiff_t>(
                                          iY * nLineSpace + iBand * nBandSpace),
                            eBufType, static_cast<int>(nPixelSpace), nOutXSize);
                    }
                }
            }
            VSIFree(pTemp);
        }
    }
    else
    {
        eErr = poDS->RasterIO(GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                              pabyOut, nOutXSize, nOutYSize, eBufType,
                              nBandCount, panBandMap, nPixelSpace, nLineSpace,
                              nBandSpace, psExtraArg);
    }

    if (NeedMaxValAdjustment())
    {
        for (int k = 0; k < nBandCount; k++)
        {
            for (int j = 0; j < nOutYSize; j++)
            {
                for (int i = 0; i < nOutXSize; i++)
                {
                    int nVal = 0;
                    GDALCopyWords(pabyOut + k * nBandSpace + j * nLineSpace +
                                      i * nPixelSpace,
                                  eBufType, 0, &nVal, GDT_Int32, 0, 1);

                    if (nVal > m_nMaxValue)
                        nVal = m_nMaxValue;

                    GDALCopyWords(&nVal, GDT_Int32, 0,
                                  pabyOut + k * nBandSpace + j * nLineSpace +
                                      i * nPixelSpace,
                                  eBufType, 0, 1);
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                          SetResampling()                             */
/************************************************************************/

void VRTSimpleSource::SetResampling(const char *pszResampling)
{
    m_osResampling = (pszResampling) ? pszResampling : "";
}

/************************************************************************/
/* ==================================================================== */
/*                         VRTAveragedSource                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         VRTAveragedSource()                          */
/************************************************************************/

VRTAveragedSource::VRTAveragedSource()
{
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTAveragedSource::SerializeToXML(const char *pszVRTPath)

{
    CPLXMLNode *const psSrc = VRTSimpleSource::SerializeToXML(pszVRTPath);

    if (psSrc == nullptr)
        return nullptr;

    CPLFree(psSrc->pszValue);
    psSrc->pszValue = CPLStrdup("AveragedSource");

    return psSrc;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

void VRTAveragedSource::SetNoDataValue(double dfNewNoDataValue)

{
    if (dfNewNoDataValue == VRT_NODATA_UNSET)
    {
        m_bNoDataSet = FALSE;
        m_dfNoDataValue = VRT_NODATA_UNSET;
        return;
    }

    m_bNoDataSet = TRUE;
    m_dfNoDataValue = dfNewNoDataValue;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTAveragedSource::RasterIO(GDALDataType /*eVRTBandDataType*/, int nXOff,
                                   int nYOff, int nXSize, int nYSize,
                                   void *pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, GSpacing nPixelSpace,
                                   GSpacing nLineSpace,
                                   GDALRasterIOExtraArg *psExtraArgIn,
                                   WorkingState & /*oWorkingState*/)

{
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg *psExtraArg = &sExtraArg;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArgIn != nullptr && psExtraArgIn->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArgIn->dfXOff;
        dfYOff = psExtraArgIn->dfYOff;
        dfXSize = psExtraArgIn->dfXSize;
        dfYSize = psExtraArgIn->dfYSize;
    }

    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    if (!GetSrcDstWindow(dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                         &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                         &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                         &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }

    auto l_band = GetRasterBand();
    if (!l_band)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Allocate a temporary buffer to whole the full resolution        */
    /*      data from the area of interest.                                 */
    /* -------------------------------------------------------------------- */
    float *const pafSrc = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(sizeof(float), nReqXSize, nReqYSize));
    if (pafSrc == nullptr)
    {
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Load it.                                                        */
    /* -------------------------------------------------------------------- */
    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if (psExtraArgIn != nullptr)
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }

    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    const CPLErr eErr = l_band->RasterIO(
        GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize, pafSrc, nReqXSize,
        nReqYSize, GDT_Float32, 0, 0, psExtraArg);

    if (eErr != CE_None)
    {
        VSIFree(pafSrc);
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Do the averaging.                                               */
    /* -------------------------------------------------------------------- */
    for (int iBufLine = nOutYOff; iBufLine < nOutYOff + nOutYSize; iBufLine++)
    {
        const double dfYDst =
            (iBufLine / static_cast<double>(nBufYSize)) * nYSize + nYOff;

        for (int iBufPixel = nOutXOff; iBufPixel < nOutXOff + nOutXSize;
             iBufPixel++)
        {
            double dfXSrcStart, dfXSrcEnd, dfYSrcStart, dfYSrcEnd;
            int iXSrcStart, iYSrcStart, iXSrcEnd, iYSrcEnd;

            const double dfXDst =
                (iBufPixel / static_cast<double>(nBufXSize)) * nXSize + nXOff;

            // Compute the source image rectangle needed for this pixel.
            DstToSrc(dfXDst, dfYDst, dfXSrcStart, dfYSrcStart);
            DstToSrc(dfXDst + 1.0, dfYDst + 1.0, dfXSrcEnd, dfYSrcEnd);

            // Convert to integers, assuming that the center of the source
            // pixel must be in our rect to get included.
            if (dfXSrcEnd >= dfXSrcStart + 1)
            {
                iXSrcStart = static_cast<int>(floor(dfXSrcStart + 0.5));
                iXSrcEnd = static_cast<int>(floor(dfXSrcEnd + 0.5));
            }
            else
            {
                /* If the resampling factor is less than 100%, the distance */
                /* between the source pixel is < 1, so we stick to nearest */
                /* neighbour */
                iXSrcStart = static_cast<int>(floor(dfXSrcStart));
                iXSrcEnd = iXSrcStart + 1;
            }
            if (dfYSrcEnd >= dfYSrcStart + 1)
            {
                iYSrcStart = static_cast<int>(floor(dfYSrcStart + 0.5));
                iYSrcEnd = static_cast<int>(floor(dfYSrcEnd + 0.5));
            }
            else
            {
                iYSrcStart = static_cast<int>(floor(dfYSrcStart));
                iYSrcEnd = iYSrcStart + 1;
            }

            // Transform into the coordinate system of the source *buffer*
            iXSrcStart -= nReqXOff;
            iYSrcStart -= nReqYOff;
            iXSrcEnd -= nReqXOff;
            iYSrcEnd -= nReqYOff;

            double dfSum = 0.0;
            int nPixelCount = 0;

            for (int iY = iYSrcStart; iY < iYSrcEnd; iY++)
            {
                if (iY < 0 || iY >= nReqYSize)
                    continue;

                for (int iX = iXSrcStart; iX < iXSrcEnd; iX++)
                {
                    if (iX < 0 || iX >= nReqXSize)
                        continue;

                    const float fSampledValue =
                        pafSrc[iX + static_cast<size_t>(iY) * nReqXSize];
                    if (CPLIsNan(fSampledValue))
                        continue;

                    if (m_bNoDataSet &&
                        GDALIsValueInRange<float>(m_dfNoDataValue) &&
                        ARE_REAL_EQUAL(fSampledValue,
                                       static_cast<float>(m_dfNoDataValue)))
                        continue;

                    nPixelCount++;
                    dfSum += pafSrc[iX + static_cast<size_t>(iY) * nReqXSize];
                }
            }

            if (nPixelCount == 0)
                continue;

            // Compute output value.
            const float dfOutputValue = static_cast<float>(dfSum / nPixelCount);

            // Put it in the output buffer.
            GByte *pDstLocation =
                static_cast<GByte *>(pData) + nPixelSpace * iBufPixel +
                static_cast<GPtrDiff_t>(nLineSpace) * iBufLine;

            if (eBufType == GDT_Byte)
                *pDstLocation = static_cast<GByte>(
                    std::min(255.0, std::max(0.0, dfOutputValue + 0.5)));
            else
                GDALCopyWords(&dfOutputValue, GDT_Float32, 4, pDstLocation,
                              eBufType, 8, 1);
        }
    }

    VSIFree(pafSrc);

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTAveragedSource::GetMinimum(int /* nXSize */, int /* nYSize */,
                                     int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTAveragedSource::GetMaximum(int /* nXSize */, int /* nYSize */,
                                     int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTAveragedSource::GetHistogram(
    int /* nXSize */, int /* nYSize */, double /* dfMin */, double /* dfMax */,
    int /* nBuckets */, GUIntBig * /* panHistogram */,
    int /* bIncludeOutOfRange */, int /* bApproxOK */,
    GDALProgressFunc /* pfnProgress */, void * /* pProgressData */)
{
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                     VRTNoDataFromMaskSource                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                     VRTNoDataFromMaskSource()                        */
/************************************************************************/

VRTNoDataFromMaskSource::VRTNoDataFromMaskSource()
{
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTNoDataFromMaskSource::XMLInit(
    const CPLXMLNode *psSrc, const char *pszVRTPath,
    std::map<CPLString, GDALDataset *> &oMapSharedSources)

{
    /* -------------------------------------------------------------------- */
    /*      Do base initialization.                                         */
    /* -------------------------------------------------------------------- */
    {
        const CPLErr eErr =
            VRTSimpleSource::XMLInit(psSrc, pszVRTPath, oMapSharedSources);
        if (eErr != CE_None)
            return eErr;
    }

    if (const char *pszNODATA = CPLGetXMLValue(psSrc, "NODATA", nullptr))
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = CPLAtofM(pszNODATA);
    }

    m_dfMaskValueThreshold =
        CPLAtofM(CPLGetXMLValue(psSrc, "MaskValueThreshold", "0"));

    if (const char *pszRemappedValue =
            CPLGetXMLValue(psSrc, "RemappedValue", nullptr))
    {
        m_bHasRemappedValue = true;
        m_dfRemappedValue = CPLAtofM(pszRemappedValue);
    }

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTNoDataFromMaskSource::SerializeToXML(const char *pszVRTPath)

{
    CPLXMLNode *const psSrc = VRTSimpleSource::SerializeToXML(pszVRTPath);

    if (psSrc == nullptr)
        return nullptr;

    CPLFree(psSrc->pszValue);
    psSrc->pszValue = CPLStrdup("NoDataFromMaskSource");

    if (m_bNoDataSet)
    {
        CPLSetXMLValue(psSrc, "MaskValueThreshold",
                       CPLSPrintf("%.18g", m_dfMaskValueThreshold));

        GDALDataType eBandDT = GDT_Unknown;
        double dfNoDataValue = m_dfNoDataValue;
        const auto kMaxFloat = std::numeric_limits<float>::max();
        if (std::fabs(std::fabs(m_dfNoDataValue) - kMaxFloat) <
            1e-10 * kMaxFloat)
        {
            auto l_band = GetRasterBand();
            if (l_band)
            {
                eBandDT = l_band->GetRasterDataType();
                if (eBandDT == GDT_Float32)
                {
                    dfNoDataValue =
                        GDALAdjustNoDataCloseToFloatMax(m_dfNoDataValue);
                }
            }
        }
        CPLSetXMLValue(psSrc, "NODATA",
                       VRTSerializeNoData(dfNoDataValue, eBandDT, 18).c_str());
    }

    if (m_bHasRemappedValue)
    {
        CPLSetXMLValue(psSrc, "RemappedValue",
                       CPLSPrintf("%.18g", m_dfRemappedValue));
    }

    return psSrc;
}

/************************************************************************/
/*                           SetParameters()                            */
/************************************************************************/

void VRTNoDataFromMaskSource::SetParameters(double dfNoDataValue,
                                            double dfMaskValueThreshold)
{
    m_bNoDataSet = true;
    m_dfNoDataValue = dfNoDataValue;
    m_dfMaskValueThreshold = dfMaskValueThreshold;
    if (!m_bHasRemappedValue)
        m_dfRemappedValue = m_dfNoDataValue;
}

/************************************************************************/
/*                           SetParameters()                            */
/************************************************************************/

void VRTNoDataFromMaskSource::SetParameters(double dfNoDataValue,
                                            double dfMaskValueThreshold,
                                            double dfRemappedValue)
{
    SetParameters(dfNoDataValue, dfMaskValueThreshold);
    m_bHasRemappedValue = true;
    m_dfRemappedValue = dfRemappedValue;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTNoDataFromMaskSource::RasterIO(
    GDALDataType eVRTBandDataType, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace,
    GDALRasterIOExtraArg *psExtraArgIn, WorkingState &oWorkingState)

{
    if (!m_bNoDataSet)
    {
        return VRTSimpleSource::RasterIO(eVRTBandDataType, nXOff, nYOff, nXSize,
                                         nYSize, pData, nBufXSize, nBufYSize,
                                         eBufType, nPixelSpace, nLineSpace,
                                         psExtraArgIn, oWorkingState);
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg *psExtraArg = &sExtraArg;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArgIn != nullptr && psExtraArgIn->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArgIn->dfXOff;
        dfYOff = psExtraArgIn->dfYOff;
        dfXSize = psExtraArgIn->dfXSize;
        dfYSize = psExtraArgIn->dfYSize;
    }

    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    if (!GetSrcDstWindow(dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                         &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                         &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                         &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }

    auto l_band = GetRasterBand();
    if (!l_band)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Allocate temporary buffer(s).                                   */
    /* -------------------------------------------------------------------- */
    const auto eSrcBandDT = l_band->GetRasterDataType();
    const int nSrcBandDTSize = GDALGetDataTypeSizeBytes(eSrcBandDT);
    const auto eSrcMaskBandDT = l_band->GetMaskBand()->GetRasterDataType();
    const int nSrcMaskBandDTSize = GDALGetDataTypeSizeBytes(eSrcMaskBandDT);
    double dfRemappedValue = m_dfRemappedValue;
    if (!m_bHasRemappedValue)
    {
        if (eSrcBandDT == GDT_Byte &&
            m_dfNoDataValue >= std::numeric_limits<GByte>::min() &&
            m_dfNoDataValue <= std::numeric_limits<GByte>::max() &&
            static_cast<int>(m_dfNoDataValue) == m_dfNoDataValue)
        {
            if (m_dfNoDataValue == std::numeric_limits<GByte>::max())
                dfRemappedValue = m_dfNoDataValue - 1;
            else
                dfRemappedValue = m_dfNoDataValue + 1;
        }
        else if (eSrcBandDT == GDT_UInt16 &&
                 m_dfNoDataValue >= std::numeric_limits<uint16_t>::min() &&
                 m_dfNoDataValue <= std::numeric_limits<uint16_t>::max() &&
                 static_cast<int>(m_dfNoDataValue) == m_dfNoDataValue)
        {
            if (m_dfNoDataValue == std::numeric_limits<uint16_t>::max())
                dfRemappedValue = m_dfNoDataValue - 1;
            else
                dfRemappedValue = m_dfNoDataValue + 1;
        }
        else if (eSrcBandDT == GDT_Int16 &&
                 m_dfNoDataValue >= std::numeric_limits<int16_t>::min() &&
                 m_dfNoDataValue <= std::numeric_limits<int16_t>::max() &&
                 static_cast<int>(m_dfNoDataValue) == m_dfNoDataValue)
        {
            if (m_dfNoDataValue == std::numeric_limits<int16_t>::max())
                dfRemappedValue = m_dfNoDataValue - 1;
            else
                dfRemappedValue = m_dfNoDataValue + 1;
        }
        else
        {
            constexpr double EPS = 1e-3;
            if (m_dfNoDataValue == 0)
                dfRemappedValue = EPS;
            else
                dfRemappedValue = m_dfNoDataValue * (1 + EPS);
        }
    }
    const bool bByteOptim =
        (eSrcBandDT == GDT_Byte && eBufType == GDT_Byte &&
         eSrcMaskBandDT == GDT_Byte && m_dfMaskValueThreshold >= 0 &&
         m_dfMaskValueThreshold <= 255 &&
         static_cast<int>(m_dfMaskValueThreshold) == m_dfMaskValueThreshold &&
         m_dfNoDataValue >= 0 && m_dfNoDataValue <= 255 &&
         static_cast<int>(m_dfNoDataValue) == m_dfNoDataValue &&
         dfRemappedValue >= 0 && dfRemappedValue <= 255 &&
         static_cast<int>(dfRemappedValue) == dfRemappedValue);
    GByte *pabyWrkBuffer;
    try
    {
        if (bByteOptim && nOutXOff == 0 && nOutYOff == 0 &&
            nOutXSize == nBufXSize && nOutYSize == nBufYSize &&
            eSrcBandDT == eBufType && nPixelSpace == nSrcBandDTSize &&
            nLineSpace == nPixelSpace * nBufXSize)
        {
            pabyWrkBuffer = static_cast<GByte *>(pData);
        }
        else
        {
            oWorkingState.m_abyWrkBuffer.resize(static_cast<size_t>(nOutXSize) *
                                                nOutYSize * nSrcBandDTSize);
            pabyWrkBuffer =
                reinterpret_cast<GByte *>(oWorkingState.m_abyWrkBuffer.data());
        }
        oWorkingState.m_abyWrkBufferMask.resize(static_cast<size_t>(nOutXSize) *
                                                nOutYSize * nSrcMaskBandDTSize);
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory when allocating buffers");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Load data.                                                      */
    /* -------------------------------------------------------------------- */
    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if (psExtraArgIn != nullptr)
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }

    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    if (l_band->RasterIO(GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                         pabyWrkBuffer, nOutXSize, nOutYSize, eSrcBandDT, 0, 0,
                         psExtraArg) != CE_None)
    {
        return CE_Failure;
    }

    if (l_band->GetMaskBand()->RasterIO(
            GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
            oWorkingState.m_abyWrkBufferMask.data(), nOutXSize, nOutYSize,
            eSrcMaskBandDT, 0, 0, psExtraArg) != CE_None)
    {
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Do the processing.                                              */
    /* -------------------------------------------------------------------- */

    GByte *const pabyOut = static_cast<GByte *>(pData) +
                           nPixelSpace * nOutXOff +
                           static_cast<GPtrDiff_t>(nLineSpace) * nOutYOff;
    if (bByteOptim)
    {
        // Special case when everything fits on Byte
        const GByte nMaskValueThreshold =
            static_cast<GByte>(m_dfMaskValueThreshold);
        const GByte nNoDataValue = static_cast<GByte>(m_dfNoDataValue);
        const GByte nRemappedValue = static_cast<GByte>(dfRemappedValue);
        size_t nSrcIdx = 0;
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GSpacing nDstOffset = iY * nLineSpace;
            for (int iX = 0; iX < nOutXSize; iX++)
            {
                const GByte nMaskVal =
                    oWorkingState.m_abyWrkBufferMask[nSrcIdx];
                if (nMaskVal <= nMaskValueThreshold)
                {
                    pabyOut[static_cast<GPtrDiff_t>(nDstOffset)] = nNoDataValue;
                }
                else
                {
                    if (pabyWrkBuffer[nSrcIdx] == nNoDataValue)
                    {
                        pabyOut[static_cast<GPtrDiff_t>(nDstOffset)] =
                            nRemappedValue;
                    }
                    else
                    {
                        pabyOut[static_cast<GPtrDiff_t>(nDstOffset)] =
                            pabyWrkBuffer[nSrcIdx];
                    }
                }
                nDstOffset += nPixelSpace;
                nSrcIdx++;
            }
        }
    }
    else
    {
        size_t nSrcIdx = 0;
        double dfMaskVal = 0;
        const int nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);
        std::vector<GByte> abyDstNoData(nBufDTSize);
        GDALCopyWords(&m_dfNoDataValue, GDT_Float64, 0, abyDstNoData.data(),
                      eBufType, 0, 1);
        std::vector<GByte> abyRemappedValue(nBufDTSize);
        GDALCopyWords(&dfRemappedValue, GDT_Float64, 0, abyRemappedValue.data(),
                      eBufType, 0, 1);
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GSpacing nDstOffset = iY * nLineSpace;
            for (int iX = 0; iX < nOutXSize; iX++)
            {
                if (eSrcMaskBandDT == GDT_Byte)
                {
                    dfMaskVal = oWorkingState.m_abyWrkBufferMask[nSrcIdx];
                }
                else
                {
                    GDALCopyWords(oWorkingState.m_abyWrkBufferMask.data() +
                                      nSrcIdx * nSrcMaskBandDTSize,
                                  eSrcMaskBandDT, 0, &dfMaskVal, GDT_Float64, 0,
                                  1);
                }
                void *const pDst =
                    pabyOut + static_cast<GPtrDiff_t>(nDstOffset);
                if (!(dfMaskVal > m_dfMaskValueThreshold))
                {
                    memcpy(pDst, abyDstNoData.data(), nBufDTSize);
                }
                else
                {
                    const void *const pSrc =
                        pabyWrkBuffer + nSrcIdx * nSrcBandDTSize;
                    if (eSrcBandDT == eBufType)
                    {
                        // coverity[overrun-buffer-arg]
                        memcpy(pDst, pSrc, nBufDTSize);
                    }
                    else
                    {
                        GDALCopyWords(pSrc, eSrcBandDT, 0, pDst, eBufType, 0,
                                      1);
                    }
                    if (memcmp(pDst, abyDstNoData.data(), nBufDTSize) == 0)
                        memcpy(pDst, abyRemappedValue.data(), nBufDTSize);
                }
                nDstOffset += nPixelSpace;
                nSrcIdx++;
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTNoDataFromMaskSource::GetMinimum(int /* nXSize */, int /* nYSize */,
                                           int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTNoDataFromMaskSource::GetMaximum(int /* nXSize */, int /* nYSize */,
                                           int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTNoDataFromMaskSource::GetHistogram(
    int /* nXSize */, int /* nYSize */, double /* dfMin */, double /* dfMax */,
    int /* nBuckets */, GUIntBig * /* panHistogram */,
    int /* bIncludeOutOfRange */, int /* bApproxOK */,
    GDALProgressFunc /* pfnProgress */, void * /* pProgressData */)
{
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTComplexSource                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTComplexSource()                          */
/************************************************************************/

VRTComplexSource::VRTComplexSource(const VRTComplexSource *poSrcSource,
                                   double dfXDstRatio, double dfYDstRatio)
    : VRTSimpleSource(poSrcSource, dfXDstRatio, dfYDstRatio),
      m_nProcessingFlags(poSrcSource->m_nProcessingFlags),
      m_dfNoDataValue(poSrcSource->m_dfNoDataValue),
      m_osNoDataValueOri(poSrcSource->m_osNoDataValueOri),
      m_dfScaleOff(poSrcSource->m_dfScaleOff),
      m_dfScaleRatio(poSrcSource->m_dfScaleRatio),
      m_bSrcMinMaxDefined(poSrcSource->m_bSrcMinMaxDefined),
      m_dfSrcMin(poSrcSource->m_dfSrcMin), m_dfSrcMax(poSrcSource->m_dfSrcMax),
      m_dfDstMin(poSrcSource->m_dfDstMin), m_dfDstMax(poSrcSource->m_dfDstMax),
      m_dfExponent(poSrcSource->m_dfExponent),
      m_nColorTableComponent(poSrcSource->m_nColorTableComponent),
      m_adfLUTInputs(poSrcSource->m_adfLUTInputs),
      m_adfLUTOutputs(poSrcSource->m_adfLUTOutputs)
{
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

void VRTComplexSource::SetNoDataValue(double dfNewNoDataValue)

{
    if (dfNewNoDataValue == VRT_NODATA_UNSET)
    {
        m_nProcessingFlags &= ~PROCESSING_FLAG_NODATA;
        m_dfNoDataValue = VRT_NODATA_UNSET;
        return;
    }

    m_nProcessingFlags |= PROCESSING_FLAG_NODATA;
    m_dfNoDataValue = dfNewNoDataValue;
}

/************************************************************************/
/*                      GetAdjustedNoDataValue()                        */
/************************************************************************/

double VRTComplexSource::GetAdjustedNoDataValue() const
{
    if ((m_nProcessingFlags & PROCESSING_FLAG_NODATA) != 0)
    {
        auto l_band = GetRasterBand();
        if (l_band && l_band->GetRasterDataType() == GDT_Float32)
        {
            return GDALAdjustNoDataCloseToFloatMax(m_dfNoDataValue);
        }
    }
    return m_dfNoDataValue;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTComplexSource::SerializeToXML(const char *pszVRTPath)

{
    CPLXMLNode *psSrc = VRTSimpleSource::SerializeToXML(pszVRTPath);

    if (psSrc == nullptr)
        return nullptr;

    CPLFree(psSrc->pszValue);
    psSrc->pszValue = CPLStrdup("ComplexSource");

    if ((m_nProcessingFlags & PROCESSING_FLAG_USE_MASK_BAND) != 0)
    {
        CPLSetXMLValue(psSrc, "UseMaskBand", "true");
    }

    if ((m_nProcessingFlags & PROCESSING_FLAG_NODATA) != 0)
    {
        if (!m_osNoDataValueOri.empty() && GetRasterBandNoOpen() == nullptr)
        {
            CPLSetXMLValue(psSrc, "NODATA", m_osNoDataValueOri.c_str());
        }
        else
        {
            GDALDataType eBandDT = GDT_Unknown;
            double dfNoDataValue = m_dfNoDataValue;
            const auto kMaxFloat = std::numeric_limits<float>::max();
            if (std::fabs(std::fabs(m_dfNoDataValue) - kMaxFloat) <
                1e-10 * kMaxFloat)
            {
                auto l_band = GetRasterBand();
                if (l_band)
                {
                    dfNoDataValue = GetAdjustedNoDataValue();
                    eBandDT = l_band->GetRasterDataType();
                }
            }
            CPLSetXMLValue(
                psSrc, "NODATA",
                VRTSerializeNoData(dfNoDataValue, eBandDT, 18).c_str());
        }
    }

    if ((m_nProcessingFlags & PROCESSING_FLAG_SCALING_LINEAR) != 0)
    {
        CPLSetXMLValue(psSrc, "ScaleOffset", CPLSPrintf("%g", m_dfScaleOff));
        CPLSetXMLValue(psSrc, "ScaleRatio", CPLSPrintf("%g", m_dfScaleRatio));
    }
    else if ((m_nProcessingFlags & PROCESSING_FLAG_SCALING_EXPONENTIAL) != 0)
    {
        CPLSetXMLValue(psSrc, "Exponent", CPLSPrintf("%g", m_dfExponent));
        if (m_bSrcMinMaxDefined)
        {
            CPLSetXMLValue(psSrc, "SrcMin", CPLSPrintf("%g", m_dfSrcMin));
            CPLSetXMLValue(psSrc, "SrcMax", CPLSPrintf("%g", m_dfSrcMax));
        }
        CPLSetXMLValue(psSrc, "DstMin", CPLSPrintf("%g", m_dfDstMin));
        CPLSetXMLValue(psSrc, "DstMax", CPLSPrintf("%g", m_dfDstMax));
    }

    if (!m_adfLUTInputs.empty())
    {
        // Make sure we print with sufficient precision to address really close
        // entries (#6422).
        CPLString osLUT;
        if (m_adfLUTInputs.size() >= 2 &&
            CPLString().Printf("%g", m_adfLUTInputs[0]) ==
                CPLString().Printf("%g", m_adfLUTInputs[1]))
        {
            osLUT = CPLString().Printf("%.18g:%g", m_adfLUTInputs[0],
                                       m_adfLUTOutputs[0]);
        }
        else
        {
            osLUT = CPLString().Printf("%g:%g", m_adfLUTInputs[0],
                                       m_adfLUTOutputs[0]);
        }
        for (size_t i = 1; i < m_adfLUTInputs.size(); i++)
        {
            if (CPLString().Printf("%g", m_adfLUTInputs[i]) ==
                    CPLString().Printf("%g", m_adfLUTInputs[i - 1]) ||
                (i + 1 < m_adfLUTInputs.size() &&
                 CPLString().Printf("%g", m_adfLUTInputs[i]) ==
                     CPLString().Printf("%g", m_adfLUTInputs[i + 1])))
            {
                // TODO(schwehr): An explanation of the 18 would be helpful.
                // Can someone distill the issue down to a quick comment?
                // https://trac.osgeo.org/gdal/ticket/6422
                osLUT += CPLString().Printf(",%.18g:%g", m_adfLUTInputs[i],
                                            m_adfLUTOutputs[i]);
            }
            else
            {
                osLUT += CPLString().Printf(",%g:%g", m_adfLUTInputs[i],
                                            m_adfLUTOutputs[i]);
            }
        }
        CPLSetXMLValue(psSrc, "LUT", osLUT);
    }

    if (m_nColorTableComponent)
    {
        CPLSetXMLValue(psSrc, "ColorTableComponent",
                       CPLSPrintf("%d", m_nColorTableComponent));
    }

    return psSrc;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr
VRTComplexSource::XMLInit(const CPLXMLNode *psSrc, const char *pszVRTPath,
                          std::map<CPLString, GDALDataset *> &oMapSharedSources)

{
    /* -------------------------------------------------------------------- */
    /*      Do base initialization.                                         */
    /* -------------------------------------------------------------------- */
    {
        const CPLErr eErr =
            VRTSimpleSource::XMLInit(psSrc, pszVRTPath, oMapSharedSources);
        if (eErr != CE_None)
            return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Complex parameters.                                             */
    /* -------------------------------------------------------------------- */
    const char *pszScaleOffset = CPLGetXMLValue(psSrc, "ScaleOffset", nullptr);
    const char *pszScaleRatio = CPLGetXMLValue(psSrc, "ScaleRatio", nullptr);
    if (pszScaleOffset || pszScaleRatio)
    {
        m_nProcessingFlags |= PROCESSING_FLAG_SCALING_LINEAR;
        if (pszScaleOffset)
            m_dfScaleOff = CPLAtof(pszScaleOffset);
        if (pszScaleRatio)
            m_dfScaleRatio = CPLAtof(pszScaleRatio);
    }
    else if (CPLGetXMLValue(psSrc, "Exponent", nullptr) != nullptr &&
             CPLGetXMLValue(psSrc, "DstMin", nullptr) != nullptr &&
             CPLGetXMLValue(psSrc, "DstMax", nullptr) != nullptr)
    {
        m_nProcessingFlags |= PROCESSING_FLAG_SCALING_EXPONENTIAL;
        m_dfExponent = CPLAtof(CPLGetXMLValue(psSrc, "Exponent", "1.0"));

        const char *pszSrcMin = CPLGetXMLValue(psSrc, "SrcMin", nullptr);
        const char *pszSrcMax = CPLGetXMLValue(psSrc, "SrcMax", nullptr);
        if (pszSrcMin && pszSrcMax)
        {
            m_dfSrcMin = CPLAtof(pszSrcMin);
            m_dfSrcMax = CPLAtof(pszSrcMax);
            m_bSrcMinMaxDefined = true;
        }

        m_dfDstMin = CPLAtof(CPLGetXMLValue(psSrc, "DstMin", "0.0"));
        m_dfDstMax = CPLAtof(CPLGetXMLValue(psSrc, "DstMax", "0.0"));
    }

    if (const char *pszNODATA = CPLGetXMLValue(psSrc, "NODATA", nullptr))
    {
        m_nProcessingFlags |= PROCESSING_FLAG_NODATA;
        m_osNoDataValueOri = pszNODATA;
        m_dfNoDataValue = CPLAtofM(m_osNoDataValueOri.c_str());
    }

    const char *pszUseMaskBand = CPLGetXMLValue(psSrc, "UseMaskBand", nullptr);
    if (pszUseMaskBand && CPLTestBool(pszUseMaskBand))
    {
        m_nProcessingFlags |= PROCESSING_FLAG_USE_MASK_BAND;
    }

    const char *pszLUT = CPLGetXMLValue(psSrc, "LUT", nullptr);
    if (pszLUT)
    {
        const CPLStringList aosValues(
            CSLTokenizeString2(pszLUT, ",:", CSLT_ALLOWEMPTYTOKENS));

        const int nLUTItemCount = aosValues.size() / 2;
        try
        {
            m_adfLUTInputs.resize(nLUTItemCount);
            m_adfLUTOutputs.resize(nLUTItemCount);
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            m_adfLUTInputs.clear();
            m_adfLUTOutputs.clear();
            return CE_Failure;
        }

        for (int nIndex = 0; nIndex < nLUTItemCount; nIndex++)
        {
            m_adfLUTInputs[nIndex] = CPLAtof(aosValues[nIndex * 2]);
            m_adfLUTOutputs[nIndex] = CPLAtof(aosValues[nIndex * 2 + 1]);

            // Enforce the requirement that the LUT input array is
            // monotonically non-decreasing.
            if (std::isnan(m_adfLUTInputs[nIndex]) && nIndex != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "A Not-A-Number (NaN) source value should be the "
                         "first one of the LUT.");
                m_adfLUTInputs.clear();
                m_adfLUTOutputs.clear();
                return CE_Failure;
            }
            else if (nIndex > 0 &&
                     m_adfLUTInputs[nIndex] < m_adfLUTInputs[nIndex - 1])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Source values of the LUT are not listed in a "
                         "monotonically non-decreasing order");
                m_adfLUTInputs.clear();
                m_adfLUTOutputs.clear();
                return CE_Failure;
            }
        }

        m_nProcessingFlags |= PROCESSING_FLAG_LUT;
    }

    const char *pszColorTableComponent =
        CPLGetXMLValue(psSrc, "ColorTableComponent", nullptr);
    if (pszColorTableComponent)
    {
        m_nColorTableComponent = atoi(pszColorTableComponent);
        m_nProcessingFlags |= PROCESSING_FLAG_COLOR_TABLE_EXPANSION;
    }

    return CE_None;
}

/************************************************************************/
/*                              LookupValue()                           */
/************************************************************************/

double VRTComplexSource::LookupValue(double dfInput)
{
    auto beginIter = m_adfLUTInputs.begin();
    auto endIter = m_adfLUTInputs.end();
    size_t offset = 0;
    if (std::isnan(m_adfLUTInputs[0]))
    {
        if (std::isnan(dfInput) || m_adfLUTInputs.size() == 1)
            return m_adfLUTOutputs[0];
        ++beginIter;
        offset = 1;
    }

    // Find the index of the first element in the LUT input array that
    // is not smaller than the input value.
    const size_t i =
        offset +
        std::distance(beginIter, std::lower_bound(beginIter, endIter, dfInput));

    if (i == offset)
        return m_adfLUTOutputs[offset];

    // If the index is beyond the end of the LUT input array, the input
    // value is larger than all the values in the array.
    if (i == m_adfLUTInputs.size())
        return m_adfLUTOutputs.back();

    if (m_adfLUTInputs[i] == dfInput)
        return m_adfLUTOutputs[i];

    // Otherwise, interpolate.
    return m_adfLUTOutputs[i - 1] +
           (dfInput - m_adfLUTInputs[i - 1]) *
               ((m_adfLUTOutputs[i] - m_adfLUTOutputs[i - 1]) /
                (m_adfLUTInputs[i] - m_adfLUTInputs[i - 1]));
}

/************************************************************************/
/*                         SetLinearScaling()                           */
/************************************************************************/

void VRTComplexSource::SetLinearScaling(double dfOffset, double dfScale)
{
    m_nProcessingFlags &= ~PROCESSING_FLAG_SCALING_EXPONENTIAL;
    m_nProcessingFlags |= PROCESSING_FLAG_SCALING_LINEAR;
    m_dfScaleOff = dfOffset;
    m_dfScaleRatio = dfScale;
}

/************************************************************************/
/*                         SetPowerScaling()                           */
/************************************************************************/

void VRTComplexSource::SetPowerScaling(double dfExponentIn, double dfSrcMinIn,
                                       double dfSrcMaxIn, double dfDstMinIn,
                                       double dfDstMaxIn)
{
    m_nProcessingFlags &= ~PROCESSING_FLAG_SCALING_LINEAR;
    m_nProcessingFlags |= PROCESSING_FLAG_SCALING_EXPONENTIAL;
    m_dfExponent = dfExponentIn;
    m_dfSrcMin = dfSrcMinIn;
    m_dfSrcMax = dfSrcMaxIn;
    m_dfDstMin = dfDstMinIn;
    m_dfDstMax = dfDstMaxIn;
    m_bSrcMinMaxDefined = true;
}

/************************************************************************/
/*                    SetColorTableComponent()                          */
/************************************************************************/

void VRTComplexSource::SetColorTableComponent(int nComponent)
{
    m_nProcessingFlags |= PROCESSING_FLAG_COLOR_TABLE_EXPANSION;
    m_nColorTableComponent = nComponent;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTComplexSource::RasterIO(GDALDataType eVRTBandDataType, int nXOff,
                                  int nYOff, int nXSize, int nYSize,
                                  void *pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType, GSpacing nPixelSpace,
                                  GSpacing nLineSpace,
                                  GDALRasterIOExtraArg *psExtraArgIn,
                                  WorkingState &oWorkingState)

{
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg *psExtraArg = &sExtraArg;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArgIn != nullptr && psExtraArgIn->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArgIn->dfXOff;
        dfYOff = psExtraArgIn->dfYOff;
        dfXSize = psExtraArgIn->dfXSize;
        dfYSize = psExtraArgIn->dfYSize;
    }

    // The window we will actually request from the source raster band.
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;

    bool bError = false;
    if (!GetSrcDstWindow(dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                         &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                         &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                         &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize, bError))
    {
        return bError ? CE_Failure : CE_None;
    }
#if DEBUG_VERBOSE
    CPLDebug("VRT",
             "nXOff=%d, nYOff=%d, nXSize=%d, nYSize=%d, nBufXSize=%d, "
             "nBufYSize=%d,\n"
             "dfReqXOff=%g, dfReqYOff=%g, dfReqXSize=%g, dfReqYSize=%g,\n"
             "nReqXOff=%d, nReqYOff=%d, nReqXSize=%d, nReqYSize=%d,\n"
             "nOutXOff=%d, nOutYOff=%d, nOutXSize=%d, nOutYSize=%d",
             nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, dfReqXOff,
             dfReqYOff, dfReqXSize, dfReqYSize, nReqXOff, nReqYOff, nReqXSize,
             nReqYSize, nOutXOff, nOutYOff, nOutXSize, nOutYSize);
#endif

    auto poSourceBand = GetRasterBand();
    if (!poSourceBand)
        return CE_Failure;

    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if (psExtraArgIn != nullptr)
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    GByte *const pabyOut = static_cast<GByte *>(pData) +
                           nPixelSpace * nOutXOff +
                           static_cast<GPtrDiff_t>(nLineSpace) * nOutYOff;
    if (m_nProcessingFlags == PROCESSING_FLAG_NODATA)
    {
        // Optimization if doing only nodata processing
        const auto eSourceType = poSourceBand->GetRasterDataType();
        if (eSourceType == GDT_Byte)
        {
            if (!GDALIsValueInRange<GByte>(m_dfNoDataValue))
            {
                return VRTSimpleSource::RasterIO(
                    eVRTBandDataType, nXOff, nYOff, nXSize, nYSize, pData,
                    nBufXSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
                    psExtraArgIn, oWorkingState);
            }

            return RasterIOProcessNoData<GByte, GDT_Byte>(
                poSourceBand, eVRTBandDataType, nReqXOff, nReqYOff, nReqXSize,
                nReqYSize, pabyOut, nOutXSize, nOutYSize, eBufType, nPixelSpace,
                nLineSpace, psExtraArg, oWorkingState);
        }
        else if (eSourceType == GDT_Int16)
        {
            if (!GDALIsValueInRange<int16_t>(m_dfNoDataValue))
            {
                return VRTSimpleSource::RasterIO(
                    eVRTBandDataType, nXOff, nYOff, nXSize, nYSize, pData,
                    nBufXSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
                    psExtraArgIn, oWorkingState);
            }

            return RasterIOProcessNoData<int16_t, GDT_Int16>(
                poSourceBand, eVRTBandDataType, nReqXOff, nReqYOff, nReqXSize,
                nReqYSize, pabyOut, nOutXSize, nOutYSize, eBufType, nPixelSpace,
                nLineSpace, psExtraArg, oWorkingState);
        }
        else if (eSourceType == GDT_UInt16)
        {
            if (!GDALIsValueInRange<uint16_t>(m_dfNoDataValue))
            {
                return VRTSimpleSource::RasterIO(
                    eVRTBandDataType, nXOff, nYOff, nXSize, nYSize, pData,
                    nBufXSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
                    psExtraArgIn, oWorkingState);
            }

            return RasterIOProcessNoData<uint16_t, GDT_UInt16>(
                poSourceBand, eVRTBandDataType, nReqXOff, nReqYOff, nReqXSize,
                nReqYSize, pabyOut, nOutXSize, nOutYSize, eBufType, nPixelSpace,
                nLineSpace, psExtraArg, oWorkingState);
        }
    }

    const bool bIsComplex =
        CPL_TO_BOOL(GDALDataTypeIsComplex(eVRTBandDataType));
    CPLErr eErr;
    // For Int32, float32 isn't sufficiently precise as working data type
    if (eVRTBandDataType == GDT_CInt32 || eVRTBandDataType == GDT_CFloat64 ||
        eVRTBandDataType == GDT_Int32 || eVRTBandDataType == GDT_UInt32 ||
        eVRTBandDataType == GDT_Float64)
    {
        eErr = RasterIOInternal<double>(
            poSourceBand, eVRTBandDataType, nReqXOff, nReqYOff, nReqXSize,
            nReqYSize, pabyOut, nOutXSize, nOutYSize, eBufType, nPixelSpace,
            nLineSpace, psExtraArg, bIsComplex ? GDT_CFloat64 : GDT_Float64,
            oWorkingState);
    }
    else
    {
        eErr = RasterIOInternal<float>(
            poSourceBand, eVRTBandDataType, nReqXOff, nReqYOff, nReqXSize,
            nReqYSize, pabyOut, nOutXSize, nOutYSize, eBufType, nPixelSpace,
            nLineSpace, psExtraArg, bIsComplex ? GDT_CFloat32 : GDT_Float32,
            oWorkingState);
    }

    return eErr;
}

/************************************************************************/
/*                              hasZeroByte()                           */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static inline bool hasZeroByte(uint32_t v)
{
    // Cf https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
    return (((v)-0x01010101U) & ~(v)&0x80808080U) != 0;
}

/************************************************************************/
/*                       RasterIOProcessNoData()                        */
/************************************************************************/

// This method is an optimization of the generic RasterIOInternal()
// that deals with a VRTComplexSource with only a NODATA value in it, and
// no other processing flags.

// nReqXOff, nReqYOff, nReqXSize, nReqYSize are expressed in source band
// referential.
template <class SourceDT, GDALDataType eSourceType>
CPLErr VRTComplexSource::RasterIOProcessNoData(
    GDALRasterBand *poSourceBand, GDALDataType eVRTBandDataType, int nReqXOff,
    int nReqYOff, int nReqXSize, int nReqYSize, void *pData, int nOutXSize,
    int nOutYSize, GDALDataType eBufType, GSpacing nPixelSpace,
    GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg,
    WorkingState &oWorkingState)
{
    CPLAssert(m_nProcessingFlags == PROCESSING_FLAG_NODATA);
    CPLAssert(GDALIsValueInRange<SourceDT>(m_dfNoDataValue));

    /* -------------------------------------------------------------------- */
    /*      Read into a temporary buffer.                                   */
    /* -------------------------------------------------------------------- */
    try
    {
        // Cannot overflow since pData should at least have that number of
        // elements
        const size_t nPixelCount = static_cast<size_t>(nOutXSize) * nOutYSize;
        if (nPixelCount >
            static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) /
                sizeof(SourceDT))
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Too large temporary buffer");
            return CE_Failure;
        }
        oWorkingState.m_abyWrkBuffer.resize(sizeof(SourceDT) * nPixelCount);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return CE_Failure;
    }
    const auto paSrcData =
        reinterpret_cast<const SourceDT *>(oWorkingState.m_abyWrkBuffer.data());

    const GDALRIOResampleAlg eResampleAlgBack = psExtraArg->eResampleAlg;
    if (!m_osResampling.empty())
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }

    const CPLErr eErr = poSourceBand->RasterIO(
        GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
        oWorkingState.m_abyWrkBuffer.data(), nOutXSize, nOutYSize, eSourceType,
        sizeof(SourceDT), sizeof(SourceDT) * static_cast<GSpacing>(nOutXSize),
        psExtraArg);
    if (!m_osResampling.empty())
        psExtraArg->eResampleAlg = eResampleAlgBack;

    if (eErr != CE_None)
    {
        return eErr;
    }

    const auto nNoDataValue = static_cast<SourceDT>(m_dfNoDataValue);
    size_t idxBuffer = 0;
    if (eSourceType == eBufType &&
        !GDALDataTypeIsConversionLossy(eSourceType, eVRTBandDataType))
    {
        // Most optimized case: the output type is the same as the source type,
        // and conversion from the source type to the VRT band data type is
        // not lossy
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GByte *pDstLocation = static_cast<GByte *>(pData) +
                                  static_cast<GPtrDiff_t>(nLineSpace) * iY;

            int iX = 0;
            if (sizeof(SourceDT) == 1 && nPixelSpace == 1)
            {
                // Optimization to detect more quickly if source pixels are
                // at nodata.
                const GByte byNoDataValue = static_cast<GByte>(nNoDataValue);
                const uint32_t wordNoData =
                    (static_cast<uint32_t>(byNoDataValue) << 24) |
                    (byNoDataValue << 16) | (byNoDataValue << 8) |
                    byNoDataValue;

                // Warning: hasZeroByte() assumes WORD_SIZE = 4
                constexpr int WORD_SIZE = 4;
                for (; iX < nOutXSize - (WORD_SIZE - 1); iX += WORD_SIZE)
                {
                    uint32_t v;
                    static_assert(sizeof(v) == WORD_SIZE,
                                  "sizeof(v) == WORD_SIZE");
                    memcpy(&v, paSrcData + idxBuffer, sizeof(v));
                    // Cf https://graphics.stanford.edu/~seander/bithacks.html#ValueInWord
                    if (!hasZeroByte(v ^ wordNoData))
                    {
                        // No bytes are at nodata
                        memcpy(pDstLocation, &v, WORD_SIZE);
                        idxBuffer += WORD_SIZE;
                        pDstLocation += WORD_SIZE;
                    }
                    else if (v == wordNoData)
                    {
                        // All bytes are at nodata
                        idxBuffer += WORD_SIZE;
                        pDstLocation += WORD_SIZE;
                    }
                    else
                    {
                        // There are both bytes at nodata and valid bytes
                        for (int k = 0; k < WORD_SIZE; ++k)
                        {
                            if (paSrcData[idxBuffer] != nNoDataValue)
                            {
                                memcpy(pDstLocation, &paSrcData[idxBuffer],
                                       sizeof(SourceDT));
                            }
                            idxBuffer++;
                            pDstLocation += nPixelSpace;
                        }
                    }
                }
            }

            for (; iX < nOutXSize;
                 iX++, pDstLocation += nPixelSpace, idxBuffer++)
            {
                if (paSrcData[idxBuffer] != nNoDataValue)
                {
                    memcpy(pDstLocation, &paSrcData[idxBuffer],
                           sizeof(SourceDT));
                }
            }
        }
    }
    else if (!GDALDataTypeIsConversionLossy(eSourceType, eVRTBandDataType))
    {
        // Conversion from the source type to the VRT band data type is
        // not lossy, so we can directly convert from the source type to
        // the the output type
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GByte *pDstLocation = static_cast<GByte *>(pData) +
                                  static_cast<GPtrDiff_t>(nLineSpace) * iY;

            for (int iX = 0; iX < nOutXSize;
                 iX++, pDstLocation += nPixelSpace, idxBuffer++)
            {
                if (paSrcData[idxBuffer] != nNoDataValue)
                {
                    GDALCopyWords(&paSrcData[idxBuffer], eSourceType, 0,
                                  pDstLocation, eBufType, 0, 1);
                }
            }
        }
    }
    else
    {
        GByte abyTemp[2 * sizeof(double)];
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GByte *pDstLocation = static_cast<GByte *>(pData) +
                                  static_cast<GPtrDiff_t>(nLineSpace) * iY;

            for (int iX = 0; iX < nOutXSize;
                 iX++, pDstLocation += nPixelSpace, idxBuffer++)
            {
                if (paSrcData[idxBuffer] != nNoDataValue)
                {
                    // Convert first to the VRTRasterBand data type
                    // to get its clamping, before outputting to buffer data type
                    GDALCopyWords(&paSrcData[idxBuffer], eSourceType, 0,
                                  abyTemp, eVRTBandDataType, 0, 1);
                    GDALCopyWords(abyTemp, eVRTBandDataType, 0, pDstLocation,
                                  eBufType, 0, 1);
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          RasterIOInternal()                          */
/************************************************************************/

// nReqXOff, nReqYOff, nReqXSize, nReqYSize are expressed in source band
// referential.
template <class WorkingDT>
CPLErr VRTComplexSource::RasterIOInternal(
    GDALRasterBand *poSourceBand, GDALDataType eVRTBandDataType, int nReqXOff,
    int nReqYOff, int nReqXSize, int nReqYSize, void *pData, int nOutXSize,
    int nOutYSize, GDALDataType eBufType, GSpacing nPixelSpace,
    GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg,
    GDALDataType eWrkDataType, WorkingState &oWorkingState)
{
    const GDALColorTable *poColorTable = nullptr;
    const bool bIsComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eBufType));
    const int nWordSize = GDALGetDataTypeSizeBytes(eWrkDataType);
    assert(nWordSize != 0);

    // If no explicit <NODATA> is set, but UseMaskBand is set, and the band
    // has a nodata value, then use it as if it was set as <NODATA>
    int bNoDataSet = (m_nProcessingFlags & PROCESSING_FLAG_NODATA) != 0;
    double dfNoDataValue = GetAdjustedNoDataValue();

    if ((m_nProcessingFlags & PROCESSING_FLAG_USE_MASK_BAND) != 0 &&
        poSourceBand->GetMaskFlags() == GMF_NODATA)
    {
        dfNoDataValue = poSourceBand->GetNoDataValue(&bNoDataSet);
    }

    const bool bNoDataSetIsNan = bNoDataSet && CPLIsNan(dfNoDataValue);
    const bool bNoDataSetAndNotNan =
        bNoDataSet && !CPLIsNan(dfNoDataValue) &&
        GDALIsValueInRange<WorkingDT>(dfNoDataValue);
    const auto fWorkingDataTypeNoData = static_cast<WorkingDT>(dfNoDataValue);

    const GByte *pabyMask = nullptr;
    const WorkingDT *pafData = nullptr;
    if ((m_nProcessingFlags & PROCESSING_FLAG_SCALING_LINEAR) != 0 &&
        m_dfScaleRatio == 0 && bNoDataSet == FALSE &&
        (m_nProcessingFlags & PROCESSING_FLAG_USE_MASK_BAND) == 0)
    {
        /* ------------------------------------------------------------------ */
        /*      Optimization when writing a constant value */
        /*      (used by the -addalpha option of gdalbuildvrt) */
        /* ------------------------------------------------------------------ */
        // Already set to NULL when defined.
        // pafData = NULL;
    }
    else
    {
        /* ---------------------------------------------------------------- */
        /*      Read into a temporary buffer.                               */
        /* ---------------------------------------------------------------- */
        const size_t nPixelCount = static_cast<size_t>(nOutXSize) * nOutYSize;
        try
        {
            // Cannot overflow since pData should at least have that number of
            // elements
            if (nPixelCount >
                static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) /
                    static_cast<size_t>(nWordSize))
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Too large temporary buffer");
                return CE_Failure;
            }
            oWorkingState.m_abyWrkBuffer.resize(nWordSize * nPixelCount);
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return CE_Failure;
        }
        pafData = reinterpret_cast<const WorkingDT *>(
            oWorkingState.m_abyWrkBuffer.data());

        const GDALRIOResampleAlg eResampleAlgBack = psExtraArg->eResampleAlg;
        if (!m_osResampling.empty())
        {
            psExtraArg->eResampleAlg =
                GDALRasterIOGetResampleAlg(m_osResampling);
        }

        const CPLErr eErr = poSourceBand->RasterIO(
            GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
            oWorkingState.m_abyWrkBuffer.data(), nOutXSize, nOutYSize,
            eWrkDataType, nWordSize,
            nWordSize * static_cast<GSpacing>(nOutXSize), psExtraArg);
        if (!m_osResampling.empty())
            psExtraArg->eResampleAlg = eResampleAlgBack;

        if (eErr != CE_None)
        {
            return eErr;
        }

        // Allocate and read mask band if needed
        if (!bNoDataSet &&
            (m_nProcessingFlags & PROCESSING_FLAG_USE_MASK_BAND) != 0 &&
            (poSourceBand->GetMaskFlags() != GMF_ALL_VALID ||
             poSourceBand->GetColorInterpretation() == GCI_AlphaBand ||
             GetMaskBandMainBand() != nullptr))
        {
            try
            {
                oWorkingState.m_abyWrkBufferMask.resize(nPixelCount);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Out of memory when allocating mask buffer");
                return CE_Failure;
            }
            pabyMask = reinterpret_cast<const GByte *>(
                oWorkingState.m_abyWrkBufferMask.data());
            auto poMaskBand =
                (poSourceBand->GetColorInterpretation() == GCI_AlphaBand ||
                 GetMaskBandMainBand() != nullptr)
                    ? poSourceBand
                    : poSourceBand->GetMaskBand();
            if (poMaskBand->RasterIO(
                    GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                    oWorkingState.m_abyWrkBufferMask.data(), nOutXSize,
                    nOutYSize, GDT_Byte, 1, static_cast<GSpacing>(nOutXSize),
                    psExtraArg) != CE_None)
            {
                return CE_Failure;
            }
        }

        if (m_nColorTableComponent != 0)
        {
            poColorTable = poSourceBand->GetColorTable();
            if (poColorTable == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Source band has no color table.");
                return CE_Failure;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Selectively copy into output buffer with nodata masking,        */
    /*      and/or scaling.                                                 */
    /* -------------------------------------------------------------------- */
    size_t idxBuffer = 0;
    for (int iY = 0; iY < nOutYSize; iY++)
    {
        GByte *pDstLocation = static_cast<GByte *>(pData) +
                              static_cast<GPtrDiff_t>(nLineSpace) * iY;

        for (int iX = 0; iX < nOutXSize;
             iX++, pDstLocation += nPixelSpace, idxBuffer++)
        {
            WorkingDT afResult[2];
            if (pafData && !bIsComplex)
            {
                WorkingDT fResult = pafData[idxBuffer];
                if (bNoDataSetIsNan && CPLIsNan(fResult))
                    continue;
                if (bNoDataSetAndNotNan &&
                    ARE_REAL_EQUAL(fResult, fWorkingDataTypeNoData))
                    continue;
                if (pabyMask && pabyMask[idxBuffer] == 0)
                    continue;

                if (poColorTable)
                {
                    const GDALColorEntry *poEntry =
                        poColorTable->GetColorEntry(static_cast<int>(fResult));
                    if (poEntry)
                    {
                        if (m_nColorTableComponent == 1)
                            fResult = poEntry->c1;
                        else if (m_nColorTableComponent == 2)
                            fResult = poEntry->c2;
                        else if (m_nColorTableComponent == 3)
                            fResult = poEntry->c3;
                        else if (m_nColorTableComponent == 4)
                            fResult = poEntry->c4;
                    }
                    else
                    {
                        static bool bHasWarned = false;
                        if (!bHasWarned)
                        {
                            bHasWarned = true;
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "No entry %d.", static_cast<int>(fResult));
                        }
                        continue;
                    }
                }

                if ((m_nProcessingFlags & PROCESSING_FLAG_SCALING_LINEAR) != 0)
                {
                    fResult = static_cast<WorkingDT>(fResult * m_dfScaleRatio +
                                                     m_dfScaleOff);
                }
                else if ((m_nProcessingFlags &
                          PROCESSING_FLAG_SCALING_EXPONENTIAL) != 0)
                {
                    if (!m_bSrcMinMaxDefined)
                    {
                        int bSuccessMin = FALSE;
                        int bSuccessMax = FALSE;
                        double adfMinMax[2] = {
                            poSourceBand->GetMinimum(&bSuccessMin),
                            poSourceBand->GetMaximum(&bSuccessMax)};
                        if ((bSuccessMin && bSuccessMax) ||
                            poSourceBand->ComputeRasterMinMax(
                                TRUE, adfMinMax) == CE_None)
                        {
                            m_dfSrcMin = adfMinMax[0];
                            m_dfSrcMax = adfMinMax[1];
                            m_bSrcMinMaxDefined = true;
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot determine source min/max value");
                            return CE_Failure;
                        }
                    }

                    double dfPowVal =
                        (fResult - m_dfSrcMin) / (m_dfSrcMax - m_dfSrcMin);
                    if (dfPowVal < 0.0)
                        dfPowVal = 0.0;
                    else if (dfPowVal > 1.0)
                        dfPowVal = 1.0;
                    fResult =
                        static_cast<WorkingDT>((m_dfDstMax - m_dfDstMin) *
                                                   pow(dfPowVal, m_dfExponent) +
                                               m_dfDstMin);
                }

                if (!m_adfLUTInputs.empty())
                    fResult = static_cast<WorkingDT>(LookupValue(fResult));

                if (m_nMaxValue != 0 && fResult > m_nMaxValue)
                    fResult = static_cast<WorkingDT>(m_nMaxValue);

                afResult[0] = fResult;
                afResult[1] = 0;
            }
            else if (pafData && bIsComplex)
            {
                afResult[0] = pafData[2 * idxBuffer];
                afResult[1] = pafData[2 * idxBuffer + 1];

                // Do not use color table.
                if ((m_nProcessingFlags & PROCESSING_FLAG_SCALING_LINEAR) != 0)
                {
                    afResult[0] = static_cast<WorkingDT>(
                        afResult[0] * m_dfScaleRatio + m_dfScaleOff);
                    afResult[1] = static_cast<WorkingDT>(
                        afResult[1] * m_dfScaleRatio + m_dfScaleOff);
                }

                /* Do not use LUT */
            }
            else
            {
                afResult[0] = static_cast<WorkingDT>(m_dfScaleOff);
                afResult[1] = 0;

                if (!m_adfLUTInputs.empty())
                    afResult[0] =
                        static_cast<WorkingDT>(LookupValue(afResult[0]));

                if (m_nMaxValue != 0 && afResult[0] > m_nMaxValue)
                    afResult[0] = static_cast<WorkingDT>(m_nMaxValue);
            }

            if (eBufType == GDT_Byte && eVRTBandDataType == GDT_Byte)
            {
                *pDstLocation = static_cast<GByte>(std::min(
                    255.0f,
                    std::max(0.0f, static_cast<float>(afResult[0]) + 0.5f)));
            }
            else if (eBufType == eVRTBandDataType)
            {
                GDALCopyWords(afResult, eWrkDataType, 0, pDstLocation, eBufType,
                              0, 1);
            }
            else
            {
                GByte abyTemp[2 * sizeof(double)];
                // Convert first to the VRTRasterBand data type
                // to get its clamping, before outputting to buffer data type
                GDALCopyWords(afResult, eWrkDataType, 0, abyTemp,
                              eVRTBandDataType, 0, 1);
                GDALCopyWords(abyTemp, eVRTBandDataType, 0, pDstLocation,
                              eBufType, 0, 1);
            }
        }
    }

    return CE_None;
}

// Explicitly instantiate template method, as it is used in another file.
template CPLErr VRTComplexSource::RasterIOInternal<float>(
    GDALRasterBand *poSourceBand, GDALDataType eVRTBandDataType, int nReqXOff,
    int nReqYOff, int nReqXSize, int nReqYSize, void *pData, int nOutXSize,
    int nOutYSize, GDALDataType eBufType, GSpacing nPixelSpace,
    GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg,
    GDALDataType eWrkDataType, WorkingState &oWorkingState);

/************************************************************************/
/*                        AreValuesUnchanged()                          */
/************************************************************************/

bool VRTComplexSource::AreValuesUnchanged() const
{
    return m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
           m_adfLUTInputs.empty() && m_nColorTableComponent == 0 &&
           (m_nProcessingFlags & PROCESSING_FLAG_SCALING_EXPONENTIAL) == 0;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTComplexSource::GetMinimum(int nXSize, int nYSize, int *pbSuccess)
{
    if (AreValuesUnchanged())
    {
        return VRTSimpleSource::GetMinimum(nXSize, nYSize, pbSuccess);
    }

    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTComplexSource::GetMaximum(int nXSize, int nYSize, int *pbSuccess)
{
    if (AreValuesUnchanged())
    {
        return VRTSimpleSource::GetMaximum(nXSize, nYSize, pbSuccess);
    }

    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTComplexSource::GetHistogram(int nXSize, int nYSize, double dfMin,
                                      double dfMax, int nBuckets,
                                      GUIntBig *panHistogram,
                                      int bIncludeOutOfRange, int bApproxOK,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    if (AreValuesUnchanged())
    {
        return VRTSimpleSource::GetHistogram(
            nXSize, nYSize, dfMin, dfMax, nBuckets, panHistogram,
            bIncludeOutOfRange, bApproxOK, pfnProgress, pProgressData);
    }

    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTFuncSource                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTFuncSource()                            */
/************************************************************************/

VRTFuncSource::VRTFuncSource()
    : pfnReadFunc(nullptr), pCBData(nullptr), eType(GDT_Byte),
      fNoDataValue(static_cast<float>(VRT_NODATA_UNSET))
{
}

/************************************************************************/
/*                           ~VRTFuncSource()                           */
/************************************************************************/

VRTFuncSource::~VRTFuncSource()
{
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTFuncSource::SerializeToXML(CPL_UNUSED const char *pszVRTPath)
{
    return nullptr;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTFuncSource::RasterIO(GDALDataType /*eVRTBandDataType*/, int nXOff,
                               int nYOff, int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, GSpacing nPixelSpace,
                               GSpacing nLineSpace,
                               GDALRasterIOExtraArg * /* psExtraArg */,
                               WorkingState & /* oWorkingState */)
{
    if (nPixelSpace * 8 == GDALGetDataTypeSize(eBufType) &&
        nLineSpace == nPixelSpace * nXSize && nBufXSize == nXSize &&
        nBufYSize == nYSize && eBufType == eType)
    {
        return pfnReadFunc(pCBData, nXOff, nYOff, nXSize, nYSize, pData);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VRTFuncSource::RasterIO() - Irregular request.");
        CPLDebug("VRT", "Irregular request: %d,%d  %d,%d, %d,%d %d,%d %d,%d",
                 static_cast<int>(nPixelSpace) * 8,
                 GDALGetDataTypeSize(eBufType), static_cast<int>(nLineSpace),
                 static_cast<int>(nPixelSpace) * nXSize, nBufXSize, nXSize,
                 nBufYSize, nYSize, static_cast<int>(eBufType),
                 static_cast<int>(eType));

        return CE_Failure;
    }
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTFuncSource::GetMinimum(int /* nXSize */, int /* nYSize */,
                                 int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTFuncSource::GetMaximum(int /* nXSize */, int /* nYSize */,
                                 int *pbSuccess)
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTFuncSource::GetHistogram(
    int /* nXSize */, int /* nYSize */, double /* dfMin */, double /* dfMax */,
    int /* nBuckets */, GUIntBig * /* panHistogram */,
    int /* bIncludeOutOfRange */, int /* bApproxOK */,
    GDALProgressFunc /* pfnProgress */, void * /* pProgressData */)
{
    return CE_Failure;
}

/************************************************************************/
/*                        VRTParseCoreSources()                         */
/************************************************************************/

VRTSource *
VRTParseCoreSources(const CPLXMLNode *psChild, const char *pszVRTPath,
                    std::map<CPLString, GDALDataset *> &oMapSharedSources)

{
    VRTSource *poSource = nullptr;

    if (EQUAL(psChild->pszValue, "AveragedSource") ||
        (EQUAL(psChild->pszValue, "SimpleSource") &&
         STARTS_WITH_CI(CPLGetXMLValue(psChild, "Resampling", "Nearest"),
                        "Aver")))
    {
        poSource = new VRTAveragedSource();
    }
    else if (EQUAL(psChild->pszValue, "SimpleSource"))
    {
        poSource = new VRTSimpleSource();
    }
    else if (EQUAL(psChild->pszValue, "ComplexSource"))
    {
        poSource = new VRTComplexSource();
    }
    else if (EQUAL(psChild->pszValue, "NoDataFromMaskSource"))
    {
        poSource = new VRTNoDataFromMaskSource();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VRTParseCoreSources() - Unknown source : %s",
                 psChild->pszValue);
        return nullptr;
    }

    if (poSource->XMLInit(psChild, pszVRTPath, oMapSharedSources) == CE_None)
        return poSource;

    delete poSource;
    return nullptr;
}

/*! @endcond */
