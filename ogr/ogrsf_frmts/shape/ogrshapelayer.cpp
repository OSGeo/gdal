/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrshape.h"

#include <cerrno>
#include <limits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogrlayerpool.h"
#include "ograrrowarrayhelper.h"
#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"

/************************************************************************/
/*                           OGRShapeLayer()                            */
/************************************************************************/

OGRShapeLayer::OGRShapeLayer(OGRShapeDataSource *poDSIn,
                             const char *pszFullNameIn, SHPHandle hSHPIn,
                             DBFHandle hDBFIn,
                             const OGRSpatialReference *poSRSIn, bool bSRSSetIn,
                             const std::string &osPrjFilename, bool bUpdate,
                             OGRwkbGeometryType eReqType,
                             CSLConstList papszCreateOptions)
    : OGRAbstractProxiedLayer(poDSIn->GetPool()), m_poDS(poDSIn),
      m_osFullName(pszFullNameIn), m_hSHP(hSHPIn), m_hDBF(hDBFIn),
      m_bUpdateAccess(bUpdate), m_eRequestedGeomType(eReqType),
      m_bHSHPWasNonNULL(hSHPIn != nullptr), m_bHDBFWasNonNULL(hDBFIn != nullptr)
{
    if (m_hSHP != nullptr)
    {
        m_nTotalShapeCount = m_hSHP->nRecords;
        if (m_hDBF != nullptr && m_hDBF->nRecords != m_nTotalShapeCount)
        {
            CPLDebug("Shape",
                     "Inconsistent record number in .shp (%d) and in .dbf (%d)",
                     m_hSHP->nRecords, m_hDBF->nRecords);
        }
    }
    else if (m_hDBF != nullptr)
    {
        m_nTotalShapeCount = m_hDBF->nRecords;
    }
#ifdef DEBUG
    else
    {
        CPLError(CE_Fatal, CPLE_AssertionFailed,
                 "Should not happen: Both m_hSHP and m_hDBF are nullptrs");
    }
#endif

    if (!TouchLayer())
    {
        CPLDebug("Shape", "TouchLayer in shape ctor failed. ");
    }

    if (m_hDBF != nullptr && m_hDBF->pszCodePage != nullptr)
    {
        CPLDebug("Shape", "DBF Codepage = %s for %s", m_hDBF->pszCodePage,
                 m_osFullName.c_str());

        // Not too sure about this, but it seems like better than nothing.
        m_osEncoding = ConvertCodePage(m_hDBF->pszCodePage);
    }

    if (m_hDBF != nullptr)
    {
        if (!(m_hDBF->nUpdateYearSince1900 == 95 && m_hDBF->nUpdateMonth == 7 &&
              m_hDBF->nUpdateDay == 26))
        {
            SetMetadataItem("DBF_DATE_LAST_UPDATE",
                            CPLSPrintf("%04d-%02d-%02d",
                                       m_hDBF->nUpdateYearSince1900 + 1900,
                                       m_hDBF->nUpdateMonth,
                                       m_hDBF->nUpdateDay));
        }
        struct tm tm;
        CPLUnixTimeToYMDHMS(time(nullptr), &tm);
        DBFSetLastModifiedDate(m_hDBF, tm.tm_year, tm.tm_mon + 1, tm.tm_mday);
    }

    const char *pszShapeEncoding =
        CSLFetchNameValue(m_poDS->GetOpenOptions(), "ENCODING");
    if (pszShapeEncoding == nullptr && m_osEncoding == "")
        pszShapeEncoding = CSLFetchNameValue(papszCreateOptions, "ENCODING");
    if (pszShapeEncoding == nullptr)
        pszShapeEncoding = CPLGetConfigOption("SHAPE_ENCODING", nullptr);
    if (pszShapeEncoding != nullptr)
        m_osEncoding = pszShapeEncoding;

    if (m_osEncoding != "")
    {
        CPLDebug("Shape", "Treating as encoding '%s'.", m_osEncoding.c_str());

        if (!OGRShapeLayer::TestCapability(OLCStringsAsUTF8))
        {
            CPLDebug("Shape", "Cannot recode from '%s'. Disabling recoding",
                     m_osEncoding.c_str());
            m_osEncoding = "";
        }
    }
    SetMetadataItem("SOURCE_ENCODING", m_osEncoding, "SHAPEFILE");

    m_poFeatureDefn = SHPReadOGRFeatureDefn(
        CPLGetBasenameSafe(m_osFullName.c_str()).c_str(), m_hSHP, m_hDBF,
        m_osEncoding,
        CPLFetchBool(m_poDS->GetOpenOptions(), "ADJUST_TYPE", false));

    // To make sure that
    //  GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef() == GetSpatialRef()
    OGRwkbGeometryType eGeomType = m_poFeatureDefn->GetGeomType();
    if (eGeomType != wkbNone)
    {
        OGRwkbGeometryType eType = wkbUnknown;

        if (m_eRequestedGeomType == wkbNone)
        {
            eType = eGeomType;

            const char *pszAdjustGeomType = CSLFetchNameValueDef(
                m_poDS->GetOpenOptions(), "ADJUST_GEOM_TYPE", "FIRST_SHAPE");
            const bool bFirstShape = EQUAL(pszAdjustGeomType, "FIRST_SHAPE");
            const bool bAllShapes = EQUAL(pszAdjustGeomType, "ALL_SHAPES");
            if ((m_hSHP != nullptr) && (m_hSHP->nRecords > 0) &&
                wkbHasM(eType) && (bFirstShape || bAllShapes))
            {
                bool bMIsUsed = false;
                for (int iShape = 0; iShape < m_hSHP->nRecords; iShape++)
                {
                    SHPObject *psShape = SHPReadObject(m_hSHP, iShape);
                    if (psShape)
                    {
                        if (psShape->bMeasureIsUsed && psShape->nVertices > 0 &&
                            psShape->padfM != nullptr)
                        {
                            for (int i = 0; i < psShape->nVertices; i++)
                            {
                                // Per the spec, if the M value is smaller than
                                // -1e38, it is a nodata value.
                                if (psShape->padfM[i] > -1e38)
                                {
                                    bMIsUsed = true;
                                    break;
                                }
                            }
                        }

                        SHPDestroyObject(psShape);
                    }
                    if (bFirstShape || bMIsUsed)
                        break;
                }
                if (!bMIsUsed)
                    eType = OGR_GT_SetModifier(eType, wkbHasZ(eType), FALSE);
            }
        }
        else
        {
            eType = m_eRequestedGeomType;
        }

        OGRSpatialReference *poSRSClone = poSRSIn ? poSRSIn->Clone() : nullptr;
        if (poSRSClone)
        {
            poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
        auto poGeomFieldDefn = std::make_unique<OGRShapeGeomFieldDefn>(
            m_osFullName.c_str(), eType, bSRSSetIn, poSRSClone);
        if (!osPrjFilename.empty())
            poGeomFieldDefn->SetPrjFilename(osPrjFilename);
        if (poSRSClone)
            poSRSClone->Release();
        m_poFeatureDefn->SetGeomType(wkbNone);
        m_poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }

    SetDescription(m_poFeatureDefn->GetName());
    m_bRewindOnWrite = CPLTestBool(CPLGetConfigOption(
        "SHAPE_REWIND_ON_WRITE",
        m_hSHP != nullptr && m_hSHP->nShapeType != SHPT_MULTIPATCH ? "NO"
                                                                   : "YES"));

    m_poFeatureDefn->Seal(/* bSealFields = */ true);
}

/************************************************************************/
/*                           ~OGRShapeLayer()                           */
/************************************************************************/

OGRShapeLayer::~OGRShapeLayer()

{
    if (m_eNeedRepack == YES && m_bAutoRepack)
        Repack();

    if (m_bResizeAtClose && m_hDBF != nullptr)
    {
        ResizeDBF();
    }
    if (m_bCreateSpatialIndexAtClose && m_hSHP != nullptr)
    {
        CreateSpatialIndex(0);
    }

    if (m_nFeaturesRead > 0 && m_poFeatureDefn != nullptr)
    {
        CPLDebug("Shape", "%d features read on layer '%s'.",
                 static_cast<int>(m_nFeaturesRead), m_poFeatureDefn->GetName());
    }

    ClearMatchingFIDs();
    ClearSpatialFIDs();

    if (m_poFeatureDefn != nullptr)
        m_poFeatureDefn->Release();

    if (m_hDBF != nullptr)
        DBFClose(m_hDBF);

    if (m_hSHP != nullptr)
        SHPClose(m_hSHP);

    if (m_hQIX != nullptr)
        SHPCloseDiskTree(m_hQIX);

    if (m_hSBN != nullptr)
        SBNCloseDiskTree(m_hSBN);
}

/************************************************************************/
/*                       SetModificationDate()                          */
/************************************************************************/

void OGRShapeLayer::SetModificationDate(const char *pszStr)
{
    if (m_hDBF && pszStr)
    {
        int year = 0;
        int month = 0;
        int day = 0;
        if ((sscanf(pszStr, "%04d-%02d-%02d", &year, &month, &day) == 3 ||
             sscanf(pszStr, "%04d/%02d/%02d", &year, &month, &day) == 3) &&
            (year >= 1900 && year <= 1900 + 255 && month >= 1 && month <= 12 &&
             day >= 1 && day <= 31))
        {
            DBFSetLastModifiedDate(m_hDBF, year - 1900, month, day);
        }
    }
}

/************************************************************************/
/*                       SetWriteDBFEOFChar()                           */
/************************************************************************/

void OGRShapeLayer::SetWriteDBFEOFChar(bool b)
{
    if (m_hDBF)
    {
        DBFSetWriteEndOfFileChar(m_hDBF, b);
    }
}

/************************************************************************/
/*                          ConvertCodePage()                           */
/************************************************************************/

static CPLString GetEncodingFromLDIDNumber(int nLDID)
{
    int nCP = -1;  // Windows code page.

    // http://www.autopark.ru/ASBProgrammerGuide/DBFSTRUC.HTM
    switch (nLDID)
    {
        case 1:
            nCP = 437;
            break;
        case 2:
            nCP = 850;
            break;
        case 3:
            nCP = 1252;
            break;
        case 4:
            nCP = 10000;
            break;
        case 8:
            nCP = 865;
            break;
        case 10:
            nCP = 850;
            break;
        case 11:
            nCP = 437;
            break;
        case 13:
            nCP = 437;
            break;
        case 14:
            nCP = 850;
            break;
        case 15:
            nCP = 437;
            break;
        case 16:
            nCP = 850;
            break;
        case 17:
            nCP = 437;
            break;
        case 18:
            nCP = 850;
            break;
        case 19:
            nCP = 932;
            break;
        case 20:
            nCP = 850;
            break;
        case 21:
            nCP = 437;
            break;
        case 22:
            nCP = 850;
            break;
        case 23:
            nCP = 865;
            break;
        case 24:
            nCP = 437;
            break;
        case 25:
            nCP = 437;
            break;
        case 26:
            nCP = 850;
            break;
        case 27:
            nCP = 437;
            break;
        case 28:
            nCP = 863;
            break;
        case 29:
            nCP = 850;
            break;
        case 31:
            nCP = 852;
            break;
        case 34:
            nCP = 852;
            break;
        case 35:
            nCP = 852;
            break;
        case 36:
            nCP = 860;
            break;
        case 37:
            nCP = 850;
            break;
        case 38:
            nCP = 866;
            break;
        case 55:
            nCP = 850;
            break;
        case 64:
            nCP = 852;
            break;
        case 77:
            nCP = 936;
            break;
        case 78:
            nCP = 949;
            break;
        case 79:
            nCP = 950;
            break;
        case 80:
            nCP = 874;
            break;
        case 87:
            return CPL_ENC_ISO8859_1;
        case 88:
            nCP = 1252;
            break;
        case 89:
            nCP = 1252;
            break;
        case 100:
            nCP = 852;
            break;
        case 101:
            nCP = 866;
            break;
        case 102:
            nCP = 865;
            break;
        case 103:
            nCP = 861;
            break;
        case 104:
            nCP = 895;
            break;
        case 105:
            nCP = 620;
            break;
        case 106:
            nCP = 737;
            break;
        case 107:
            nCP = 857;
            break;
        case 108:
            nCP = 863;
            break;
        case 120:
            nCP = 950;
            break;
        case 121:
            nCP = 949;
            break;
        case 122:
            nCP = 936;
            break;
        case 123:
            nCP = 932;
            break;
        case 124:
            nCP = 874;
            break;
        case 134:
            nCP = 737;
            break;
        case 135:
            nCP = 852;
            break;
        case 136:
            nCP = 857;
            break;
        case 150:
            nCP = 10007;
            break;
        case 151:
            nCP = 10029;
            break;
        case 200:
            nCP = 1250;
            break;
        case 201:
            nCP = 1251;
            break;
        case 202:
            nCP = 1254;
            break;
        case 203:
            nCP = 1253;
            break;
        case 204:
            nCP = 1257;
            break;
        default:
            break;
    }

    if (nCP < 0)
        return CPLString();
    return CPLString().Printf("CP%d", nCP);
}

static CPLString GetEncodingFromCPG(const char *pszCPG)
{
    // see https://support.esri.com/en/technical-article/000013192
    CPLString m_osEncodingFromCPG;
    const int nCPG = atoi(pszCPG);
    if ((nCPG >= 437 && nCPG <= 950) || (nCPG >= 1250 && nCPG <= 1258))
    {
        m_osEncodingFromCPG.Printf("CP%d", nCPG);
    }
    else if (STARTS_WITH_CI(pszCPG, "8859"))
    {
        if (pszCPG[4] == '-')
            m_osEncodingFromCPG.Printf("ISO-8859-%s", pszCPG + 5);
        else
            m_osEncodingFromCPG.Printf("ISO-8859-%s", pszCPG + 4);
    }
    else if (STARTS_WITH_CI(pszCPG, "UTF-8") || STARTS_WITH_CI(pszCPG, "UTF8"))
        m_osEncodingFromCPG = CPL_ENC_UTF8;
    else if (STARTS_WITH_CI(pszCPG, "ANSI 1251"))
        m_osEncodingFromCPG = "CP1251";
    else
    {
        // Try just using the CPG value directly.  Works for stuff like Big5.
        m_osEncodingFromCPG = pszCPG;
    }
    return m_osEncodingFromCPG;
}

CPLString OGRShapeLayer::ConvertCodePage(const char *pszCodePage)

{
    CPLString l_m_osEncoding;

    if (pszCodePage == nullptr)
        return l_m_osEncoding;

    std::string m_osEncodingFromLDID;
    if (m_hDBF->iLanguageDriver != 0)
    {
        SetMetadataItem("LDID_VALUE", CPLSPrintf("%d", m_hDBF->iLanguageDriver),
                        "SHAPEFILE");

        m_osEncodingFromLDID =
            GetEncodingFromLDIDNumber(m_hDBF->iLanguageDriver);
    }
    if (!m_osEncodingFromLDID.empty())
    {
        SetMetadataItem("ENCODING_FROM_LDID", m_osEncodingFromLDID.c_str(),
                        "SHAPEFILE");
    }

    std::string m_osEncodingFromCPG;
    if (!STARTS_WITH_CI(pszCodePage, "LDID/"))
    {
        SetMetadataItem("CPG_VALUE", pszCodePage, "SHAPEFILE");

        m_osEncodingFromCPG = GetEncodingFromCPG(pszCodePage);

        if (!m_osEncodingFromCPG.empty())
            SetMetadataItem("ENCODING_FROM_CPG", m_osEncodingFromCPG.c_str(),
                            "SHAPEFILE");

        l_m_osEncoding = std::move(m_osEncodingFromCPG);
    }
    else if (!m_osEncodingFromLDID.empty())
    {
        l_m_osEncoding = std::move(m_osEncodingFromLDID);
    }

    return l_m_osEncoding;
}

/************************************************************************/
/*                            CheckForQIX()                             */
/************************************************************************/

bool OGRShapeLayer::CheckForQIX()

{
    if (m_bCheckedForQIX)
        return m_hQIX != nullptr;

    const std::string osQIXFilename =
        CPLResetExtensionSafe(m_osFullName.c_str(), "qix");

    m_hQIX = SHPOpenDiskTree(osQIXFilename.c_str(), nullptr);

    m_bCheckedForQIX = true;

    return m_hQIX != nullptr;
}

/************************************************************************/
/*                            CheckForSBN()                             */
/************************************************************************/

bool OGRShapeLayer::CheckForSBN()

{
    if (m_bCheckedForSBN)
        return m_hSBN != nullptr;

    const std::string osSBNFilename =
        CPLResetExtensionSafe(m_osFullName.c_str(), "sbn");

    m_hSBN = SBNOpenDiskTree(osSBNFilename.c_str(), nullptr);

    m_bCheckedForSBN = true;

    return m_hSBN != nullptr;
}

/************************************************************************/
/*                            ScanIndices()                             */
/*                                                                      */
/*      Utilize optional spatial and attribute indices if they are      */
/*      available.                                                      */
/************************************************************************/

bool OGRShapeLayer::ScanIndices()

{
    m_iMatchingFID = 0;

    /* -------------------------------------------------------------------- */
    /*      Utilize attribute index if appropriate.                         */
    /* -------------------------------------------------------------------- */
    if (m_poAttrQuery != nullptr)
    {
        CPLAssert(m_panMatchingFIDs == nullptr);

        InitializeIndexSupport(m_osFullName.c_str());

        m_panMatchingFIDs =
            m_poAttrQuery->EvaluateAgainstIndices(this, nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Check for spatial index if we have a spatial query.             */
    /* -------------------------------------------------------------------- */

    if (m_poFilterGeom == nullptr || m_hSHP == nullptr)
        return true;

    OGREnvelope oSpatialFilterEnvelope;
    bool bTryQIXorSBN = true;

    m_poFilterGeom->getEnvelope(&oSpatialFilterEnvelope);

    OGREnvelope oLayerExtent;
    if (GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE)
    {
        if (oSpatialFilterEnvelope.Contains(oLayerExtent))
        {
            // The spatial filter is larger than the layer extent. No use of
            // .qix file for now.
            return true;
        }
        else if (!oSpatialFilterEnvelope.Intersects(oLayerExtent))
        {
            // No intersection : no need to check for .qix or .sbn.
            bTryQIXorSBN = false;

            // Set an empty result for spatial FIDs.
            free(m_panSpatialFIDs);
            m_panSpatialFIDs = static_cast<int *>(calloc(1, sizeof(int)));
            m_nSpatialFIDCount = 0;

            delete m_poFilterGeomLastValid;
            m_poFilterGeomLastValid = m_poFilterGeom->clone();
        }
    }

    if (bTryQIXorSBN)
    {
        if (!m_bCheckedForQIX)
            CPL_IGNORE_RET_VAL(CheckForQIX());
        if (m_hQIX == nullptr && !m_bCheckedForSBN)
            CPL_IGNORE_RET_VAL(CheckForSBN());
    }

    /* -------------------------------------------------------------------- */
    /*      Compute spatial index if appropriate.                           */
    /* -------------------------------------------------------------------- */
    if (bTryQIXorSBN && (m_hQIX != nullptr || m_hSBN != nullptr) &&
        m_panSpatialFIDs == nullptr)
    {
        double adfBoundsMin[4] = {oSpatialFilterEnvelope.MinX,
                                  oSpatialFilterEnvelope.MinY, 0.0, 0.0};
        double adfBoundsMax[4] = {oSpatialFilterEnvelope.MaxX,
                                  oSpatialFilterEnvelope.MaxY, 0.0, 0.0};

        if (m_hQIX != nullptr)
            m_panSpatialFIDs = SHPSearchDiskTreeEx(
                m_hQIX, adfBoundsMin, adfBoundsMax, &m_nSpatialFIDCount);
        else
            m_panSpatialFIDs = SBNSearchDiskTree(
                m_hSBN, adfBoundsMin, adfBoundsMax, &m_nSpatialFIDCount);

        CPLDebug("SHAPE", "Used spatial index, got %d matches.",
                 m_nSpatialFIDCount);

        delete m_poFilterGeomLastValid;
        m_poFilterGeomLastValid = m_poFilterGeom->clone();
    }

    /* -------------------------------------------------------------------- */
    /*      Use spatial index if appropriate.                               */
    /* -------------------------------------------------------------------- */
    if (m_panSpatialFIDs != nullptr)
    {
        // Use resulting list as matching FID list (but reallocate and
        // terminate with OGRNullFID).
        if (m_panMatchingFIDs == nullptr)
        {
            m_panMatchingFIDs = static_cast<GIntBig *>(
                CPLMalloc(sizeof(GIntBig) * (m_nSpatialFIDCount + 1)));
            for (int i = 0; i < m_nSpatialFIDCount; i++)
                m_panMatchingFIDs[i] =
                    static_cast<GIntBig>(m_panSpatialFIDs[i]);
            m_panMatchingFIDs[m_nSpatialFIDCount] = OGRNullFID;
        }
        // Cull attribute index matches based on those in the spatial index
        // result set.  We assume that the attribute results are in sorted
        // order.
        else
        {
            int iWrite = 0;
            int iSpatial = 0;

            for (int iRead = 0; m_panMatchingFIDs[iRead] != OGRNullFID; iRead++)
            {
                while (iSpatial < m_nSpatialFIDCount &&
                       m_panSpatialFIDs[iSpatial] < m_panMatchingFIDs[iRead])
                    iSpatial++;

                if (iSpatial == m_nSpatialFIDCount)
                    continue;

                if (m_panSpatialFIDs[iSpatial] == m_panMatchingFIDs[iRead])
                    m_panMatchingFIDs[iWrite++] = m_panMatchingFIDs[iRead];
            }
            m_panMatchingFIDs[iWrite] = OGRNullFID;
        }

        if (m_nSpatialFIDCount > 100000)
        {
            ClearSpatialFIDs();
        }
    }

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRShapeLayer::ResetReading()

{
    if (!TouchLayer())
        return;

    m_iMatchingFID = 0;

    m_iNextShapeId = 0;

    if (m_bHeaderDirty && m_bUpdateAccess)
        SyncToDisk();

    if (m_hDBF)
        VSIFClearErrL(VSI_SHP_GetVSIL(m_hDBF->fp));
}

/************************************************************************/
/*                        ClearMatchingFIDs()                           */
/************************************************************************/

void OGRShapeLayer::ClearMatchingFIDs()
{
    /* -------------------------------------------------------------------- */
    /*      Clear previous index search result, if any.                     */
    /* -------------------------------------------------------------------- */
    CPLFree(m_panMatchingFIDs);
    m_panMatchingFIDs = nullptr;
}

/************************************************************************/
/*                        ClearSpatialFIDs()                           */
/************************************************************************/

void OGRShapeLayer::ClearSpatialFIDs()
{
    if (m_panSpatialFIDs != nullptr)
    {
        CPLDebug("SHAPE", "Clear m_panSpatialFIDs");
        free(m_panSpatialFIDs);
    }
    m_panSpatialFIDs = nullptr;
    m_nSpatialFIDCount = 0;

    delete m_poFilterGeomLastValid;
    m_poFilterGeomLastValid = nullptr;
}

/************************************************************************/
/*                         ISetSpatialFilter()                          */
/************************************************************************/

OGRErr OGRShapeLayer::ISetSpatialFilter(int iGeomField,
                                        const OGRGeometry *poGeomIn)
{
    ClearMatchingFIDs();

    if (poGeomIn == nullptr)
    {
        // Do nothing.
    }
    else if (m_poFilterGeomLastValid != nullptr &&
             m_poFilterGeomLastValid->Equals(poGeomIn))
    {
        // Do nothing.
    }
    else if (m_panSpatialFIDs != nullptr)
    {
        // We clear the spatialFIDs only if we have a new non-NULL spatial
        // filter, otherwise we keep the previous result cached. This can be
        // useful when several SQL layers rely on the same table layer, and use
        // the same spatial filters. But as there is in the destructor of
        // OGRGenSQLResultsLayer a clearing of the spatial filter of the table
        // layer, we need this trick.
        ClearSpatialFIDs();
    }

    return OGRLayer::ISetSpatialFilter(iGeomField, poGeomIn);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRShapeLayer::SetAttributeFilter(const char *pszAttributeFilter)
{
    ClearMatchingFIDs();

    return OGRLayer::SetAttributeFilter(pszAttributeFilter);
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily reposition        */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::SetNextByIndex(GIntBig nIndex)

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if (nIndex < 0 || nIndex > INT_MAX)
        return OGRERR_FAILURE;

    // Eventually we should try to use m_panMatchingFIDs list
    // if available and appropriate.
    if (m_poFilterGeom != nullptr || m_poAttrQuery != nullptr)
        return OGRLayer::SetNextByIndex(nIndex);

    m_iNextShapeId = static_cast<int>(nIndex);

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FetchShape()                             */
/*                                                                      */
/*      Take a shape id, a geometry, and a feature, and set the feature */
/*      if the shapeid bbox intersects the geometry.                    */
/************************************************************************/

OGRFeature *OGRShapeLayer::FetchShape(int iShapeId)

{
    OGRFeature *poFeature = nullptr;

    if (m_poFilterGeom != nullptr && m_hSHP != nullptr)
    {
        SHPObject *psShape = SHPReadObject(m_hSHP, iShapeId);

        // do not trust degenerate bounds on non-point geometries
        // or bounds on null shapes.
        if (psShape == nullptr ||
            (psShape->nSHPType != SHPT_POINT &&
             psShape->nSHPType != SHPT_POINTZ &&
             psShape->nSHPType != SHPT_POINTM &&
             (psShape->dfXMin == psShape->dfXMax ||
              psShape->dfYMin == psShape->dfYMax)) ||
            psShape->nSHPType == SHPT_NULL)
        {
            poFeature = SHPReadOGRFeature(m_hSHP, m_hDBF, m_poFeatureDefn,
                                          iShapeId, psShape, m_osEncoding,
                                          m_bHasWarnedWrongWindingOrder);
        }
        else if (m_sFilterEnvelope.MaxX < psShape->dfXMin ||
                 m_sFilterEnvelope.MaxY < psShape->dfYMin ||
                 psShape->dfXMax < m_sFilterEnvelope.MinX ||
                 psShape->dfYMax < m_sFilterEnvelope.MinY)
        {
            SHPDestroyObject(psShape);
            poFeature = nullptr;
        }
        else
        {
            poFeature = SHPReadOGRFeature(m_hSHP, m_hDBF, m_poFeatureDefn,
                                          iShapeId, psShape, m_osEncoding,
                                          m_bHasWarnedWrongWindingOrder);
        }
    }
    else
    {
        poFeature = SHPReadOGRFeature(m_hSHP, m_hDBF, m_poFeatureDefn, iShapeId,
                                      nullptr, m_osEncoding,
                                      m_bHasWarnedWrongWindingOrder);
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRShapeLayer::GetNextFeature()

{
    if (!TouchLayer())
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Collect a matching list if we have attribute or spatial         */
    /*      indices.  Only do this on the first request for a given pass    */
    /*      of course.                                                      */
    /* -------------------------------------------------------------------- */
    if ((m_poAttrQuery != nullptr || m_poFilterGeom != nullptr) &&
        m_iNextShapeId == 0 && m_panMatchingFIDs == nullptr)
    {
        ScanIndices();
    }

    /* -------------------------------------------------------------------- */
    /*      Loop till we find a feature matching our criteria.              */
    /* -------------------------------------------------------------------- */
    OGRFeature *poFeature = nullptr;

    while (true)
    {
        if (m_panMatchingFIDs != nullptr)
        {
            if (m_panMatchingFIDs[m_iMatchingFID] == OGRNullFID)
            {
                return nullptr;
            }

            // Check the shape object's geometry, and if it matches
            // any spatial filter, return it.
            poFeature =
                FetchShape(static_cast<int>(m_panMatchingFIDs[m_iMatchingFID]));

            m_iMatchingFID++;
        }
        else
        {
            if (m_iNextShapeId >= m_nTotalShapeCount)
            {
                return nullptr;
            }

            if (m_hDBF)
            {
                if (DBFIsRecordDeleted(m_hDBF, m_iNextShapeId))
                    poFeature = nullptr;
                else if (VSIFEofL(VSI_SHP_GetVSIL(m_hDBF->fp)) ||
                         VSIFErrorL(VSI_SHP_GetVSIL(m_hDBF->fp)))
                    return nullptr;  //* I/O error.
                else
                    poFeature = FetchShape(m_iNextShapeId);
            }
            else
                poFeature = FetchShape(m_iNextShapeId);

            m_iNextShapeId++;
        }

        if (poFeature != nullptr)
        {
            OGRGeometry *poGeom = poFeature->GetGeometryRef();
            if (poGeom != nullptr)
            {
                poGeom->assignSpatialReference(GetSpatialRef());
            }

            m_nFeaturesRead++;

            if ((m_poFilterGeom == nullptr || FilterGeometry(poGeom)) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(poFeature)))
            {
                return poFeature;
            }

            delete poFeature;
        }
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRShapeLayer::GetFeature(GIntBig nFeatureId)

{
    if (!TouchLayer() || nFeatureId > INT_MAX)
        return nullptr;

    OGRFeature *poFeature = SHPReadOGRFeature(
        m_hSHP, m_hDBF, m_poFeatureDefn, static_cast<int>(nFeatureId), nullptr,
        m_osEncoding, m_bHasWarnedWrongWindingOrder);

    if (poFeature == nullptr)
    {
        // Reading shape feature failed.
        return nullptr;
    }

    if (poFeature->GetGeometryRef() != nullptr)
    {
        poFeature->GetGeometryRef()->assignSpatialReference(GetSpatialRef());
    }

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                             StartUpdate()                            */
/************************************************************************/

bool OGRShapeLayer::StartUpdate(const char *pszOperation)
{
    if (!m_poDS->UncompressIfNeeded())
        return false;

    if (!TouchLayer())
        return false;

    if (!m_bUpdateAccess)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s : unsupported operation on a read-only datasource.",
                 pszOperation);
        return false;
    }

    return true;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRShapeLayer::ISetFeature(OGRFeature *poFeature)

{
    if (!StartUpdate("SetFeature"))
        return OGRERR_FAILURE;

    GIntBig nFID = poFeature->GetFID();
    if (nFID < 0 || (m_hSHP != nullptr && nFID >= m_hSHP->nRecords) ||
        (m_hDBF != nullptr && nFID >= m_hDBF->nRecords))
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    m_bHeaderDirty = true;
    if (CheckForQIX() || CheckForSBN())
        DropSpatialIndex();

    unsigned int nOffset = 0;
    unsigned int nSize = 0;
    bool bIsLastRecord = false;
    if (m_hSHP != nullptr)
    {
        nOffset = m_hSHP->panRecOffset[nFID];
        nSize = m_hSHP->panRecSize[nFID];
        bIsLastRecord = (nOffset + nSize + 8 == m_hSHP->nFileSize);
    }

    OGRErr eErr = SHPWriteOGRFeature(m_hSHP, m_hDBF, m_poFeatureDefn, poFeature,
                                     m_osEncoding, &m_bTruncationWarningEmitted,
                                     m_bRewindOnWrite);

    if (m_hSHP != nullptr)
    {
        if (bIsLastRecord)
        {
            // Optimization: we don't need repacking if this is the last
            // record of the file. Just potential truncation
            CPLAssert(nOffset == m_hSHP->panRecOffset[nFID]);
            CPLAssert(m_hSHP->panRecOffset[nFID] + m_hSHP->panRecSize[nFID] +
                          8 ==
                      m_hSHP->nFileSize);
            if (m_hSHP->panRecSize[nFID] < nSize)
            {
                VSIFTruncateL(VSI_SHP_GetVSIL(m_hSHP->fpSHP),
                              m_hSHP->nFileSize);
            }
        }
        else if (nOffset != m_hSHP->panRecOffset[nFID] ||
                 nSize != m_hSHP->panRecSize[nFID])
        {
            m_bSHPNeedsRepack = true;
            m_eNeedRepack = YES;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteFeature(GIntBig nFID)

{
    if (!StartUpdate("DeleteFeature"))
        return OGRERR_FAILURE;

    if (nFID < 0 || (m_hSHP != nullptr && nFID >= m_hSHP->nRecords) ||
        (m_hDBF != nullptr && nFID >= m_hDBF->nRecords))
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    if (!m_hDBF)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to delete shape in shapefile with no .dbf file.  "
                 "Deletion is done by marking record deleted in dbf "
                 "and is not supported without a .dbf file.");
        return OGRERR_FAILURE;
    }

    if (DBFIsRecordDeleted(m_hDBF, static_cast<int>(nFID)))
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    if (!DBFMarkRecordDeleted(m_hDBF, static_cast<int>(nFID), TRUE))
        return OGRERR_FAILURE;

    m_bHeaderDirty = true;
    if (CheckForQIX() || CheckForSBN())
        DropSpatialIndex();
    m_eNeedRepack = YES;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::ICreateFeature(OGRFeature *poFeature)

{
    if (!StartUpdate("CreateFeature"))
        return OGRERR_FAILURE;

    if (m_hDBF != nullptr &&
        !VSI_SHP_WriteMoreDataOK(m_hDBF->fp, m_hDBF->nRecordLength))
    {
        return OGRERR_FAILURE;
    }

    m_bHeaderDirty = true;
    if (CheckForQIX() || CheckForSBN())
        DropSpatialIndex();

    poFeature->SetFID(OGRNullFID);

    if (m_nTotalShapeCount == 0 &&
        wkbFlatten(m_eRequestedGeomType) == wkbUnknown && m_hSHP != nullptr &&
        m_hSHP->nShapeType != SHPT_MULTIPATCH &&
        poFeature->GetGeometryRef() != nullptr)
    {
        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        int nShapeType = -1;

        switch (poGeom->getGeometryType())
        {
            case wkbPoint:
                nShapeType = SHPT_POINT;
                m_eRequestedGeomType = wkbPoint;
                break;

            case wkbPoint25D:
                nShapeType = SHPT_POINTZ;
                m_eRequestedGeomType = wkbPoint25D;
                break;

            case wkbPointM:
                nShapeType = SHPT_POINTM;
                m_eRequestedGeomType = wkbPointM;
                break;

            case wkbPointZM:
                nShapeType = SHPT_POINTZ;
                m_eRequestedGeomType = wkbPointZM;
                break;

            case wkbMultiPoint:
                nShapeType = SHPT_MULTIPOINT;
                m_eRequestedGeomType = wkbMultiPoint;
                break;

            case wkbMultiPoint25D:
                nShapeType = SHPT_MULTIPOINTZ;
                m_eRequestedGeomType = wkbMultiPoint25D;
                break;

            case wkbMultiPointM:
                nShapeType = SHPT_MULTIPOINTM;
                m_eRequestedGeomType = wkbMultiPointM;
                break;

            case wkbMultiPointZM:
                nShapeType = SHPT_MULTIPOINTZ;
                m_eRequestedGeomType = wkbMultiPointM;
                break;

            case wkbLineString:
            case wkbMultiLineString:
                nShapeType = SHPT_ARC;
                m_eRequestedGeomType = wkbLineString;
                break;

            case wkbLineString25D:
            case wkbMultiLineString25D:
                nShapeType = SHPT_ARCZ;
                m_eRequestedGeomType = wkbLineString25D;
                break;

            case wkbLineStringM:
            case wkbMultiLineStringM:
                nShapeType = SHPT_ARCM;
                m_eRequestedGeomType = wkbLineStringM;
                break;

            case wkbLineStringZM:
            case wkbMultiLineStringZM:
                nShapeType = SHPT_ARCZ;
                m_eRequestedGeomType = wkbLineStringZM;
                break;

            case wkbPolygon:
            case wkbMultiPolygon:
            case wkbTriangle:
                nShapeType = SHPT_POLYGON;
                m_eRequestedGeomType = wkbPolygon;
                break;

            case wkbPolygon25D:
            case wkbMultiPolygon25D:
            case wkbTriangleZ:
                nShapeType = SHPT_POLYGONZ;
                m_eRequestedGeomType = wkbPolygon25D;
                break;

            case wkbPolygonM:
            case wkbMultiPolygonM:
            case wkbTriangleM:
                nShapeType = SHPT_POLYGONM;
                m_eRequestedGeomType = wkbPolygonM;
                break;

            case wkbPolygonZM:
            case wkbMultiPolygonZM:
            case wkbTriangleZM:
                nShapeType = SHPT_POLYGONZ;
                m_eRequestedGeomType = wkbPolygonZM;
                break;

            default:
                nShapeType = -1;
                break;
        }

        if (wkbFlatten(poGeom->getGeometryType()) == wkbTIN ||
            wkbFlatten(poGeom->getGeometryType()) == wkbPolyhedralSurface)
        {
            nShapeType = SHPT_MULTIPATCH;
            m_eRequestedGeomType = wkbUnknown;
        }

        if (wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
        {
            const OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
            bool bIsMultiPatchCompatible = false;
            for (int iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++)
            {
                OGRwkbGeometryType eSubGeomType =
                    wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
                if (eSubGeomType == wkbTIN ||
                    eSubGeomType == wkbPolyhedralSurface)
                {
                    bIsMultiPatchCompatible = true;
                }
                else if (eSubGeomType != wkbMultiPolygon)
                {
                    bIsMultiPatchCompatible = false;
                    break;
                }
            }
            if (bIsMultiPatchCompatible)
            {
                nShapeType = SHPT_MULTIPATCH;
                m_eRequestedGeomType = wkbUnknown;
            }
        }

        if (nShapeType != -1)
        {
            whileUnsealing(m_poFeatureDefn)->SetGeomType(m_eRequestedGeomType);
            ResetGeomType(nShapeType);
        }
    }

    const OGRErr eErr = SHPWriteOGRFeature(
        m_hSHP, m_hDBF, m_poFeatureDefn, poFeature, m_osEncoding,
        &m_bTruncationWarningEmitted, m_bRewindOnWrite);

    if (m_hSHP != nullptr)
        m_nTotalShapeCount = m_hSHP->nRecords;
    else if (m_hDBF != nullptr)
        m_nTotalShapeCount = m_hDBF->nRecords;
#ifdef DEBUG
    else  // Silence coverity.
        CPLError(CE_Fatal, CPLE_AssertionFailed,
                 "Should not happen: Both m_hSHP and m_hDBF are nullptrs");
#endif

    return eErr;
}

/************************************************************************/
/*               GetFeatureCountWithSpatialFilterOnly()                 */
/*                                                                      */
/* Specialized implementation of GetFeatureCount() when there is *only* */
/* a spatial filter and no attribute filter.                            */
/************************************************************************/

int OGRShapeLayer::GetFeatureCountWithSpatialFilterOnly()

{
    /* -------------------------------------------------------------------- */
    /*      Collect a matching list if we have attribute or spatial         */
    /*      indices.  Only do this on the first request for a given pass    */
    /*      of course.                                                      */
    /* -------------------------------------------------------------------- */
    if (m_panMatchingFIDs == nullptr)
    {
        ScanIndices();
    }

    int nFeatureCount = 0;
    int iLocalMatchingFID = 0;
    int iLocalNextShapeId = 0;
    bool bExpectPoints = false;

    if (wkbFlatten(m_poFeatureDefn->GetGeomType()) == wkbPoint)
        bExpectPoints = true;

    /* -------------------------------------------------------------------- */
    /*      Loop till we find a feature matching our criteria.              */
    /* -------------------------------------------------------------------- */

    SHPObject sShape;
    memset(&sShape, 0, sizeof(sShape));

    while (true)
    {
        int iShape = -1;

        if (m_panMatchingFIDs != nullptr)
        {
            iShape = static_cast<int>(m_panMatchingFIDs[iLocalMatchingFID]);
            if (iShape == OGRNullFID)
                break;
            iLocalMatchingFID++;
        }
        else
        {
            if (iLocalNextShapeId >= m_nTotalShapeCount)
                break;
            iShape = iLocalNextShapeId++;

            if (m_hDBF)
            {
                if (DBFIsRecordDeleted(m_hDBF, iShape))
                    continue;

                if (VSIFEofL(VSI_SHP_GetVSIL(m_hDBF->fp)) ||
                    VSIFErrorL(VSI_SHP_GetVSIL(m_hDBF->fp)))
                    break;
            }
        }

        // Read full shape for point layers.
        SHPObject *psShape = nullptr;
        if (bExpectPoints ||
            m_hSHP->panRecOffset[iShape] == 0 /* lazy shx loading case */)
            psShape = SHPReadObject(m_hSHP, iShape);

        /* --------------------------------------------------------------------
         */
        /*      Only read feature type and bounding box for now. In case of */
        /*      inconclusive tests on bounding box only, we will read the full
         */
        /*      shape later. */
        /* --------------------------------------------------------------------
         */
        else if (iShape >= 0 && iShape < m_hSHP->nRecords &&
                 m_hSHP->panRecSize[iShape] > 4 + 8 * 4)
        {
            GByte abyBuf[4 + 8 * 4] = {};
            if (m_hSHP->sHooks.FSeek(
                    m_hSHP->fpSHP, m_hSHP->panRecOffset[iShape] + 8, 0) == 0 &&
                m_hSHP->sHooks.FRead(abyBuf, sizeof(abyBuf), 1,
                                     m_hSHP->fpSHP) == 1)
            {
                memcpy(&(sShape.nSHPType), abyBuf, 4);
                CPL_LSBPTR32(&(sShape.nSHPType));
                if (sShape.nSHPType != SHPT_NULL &&
                    sShape.nSHPType != SHPT_POINT &&
                    sShape.nSHPType != SHPT_POINTM &&
                    sShape.nSHPType != SHPT_POINTZ)
                {
                    psShape = &sShape;
                    memcpy(&(sShape.dfXMin), abyBuf + 4, 8);
                    memcpy(&(sShape.dfYMin), abyBuf + 12, 8);
                    memcpy(&(sShape.dfXMax), abyBuf + 20, 8);
                    memcpy(&(sShape.dfYMax), abyBuf + 28, 8);
                    CPL_LSBPTR64(&(sShape.dfXMin));
                    CPL_LSBPTR64(&(sShape.dfYMin));
                    CPL_LSBPTR64(&(sShape.dfXMax));
                    CPL_LSBPTR64(&(sShape.dfYMax));
                }
            }
            else
            {
                break;
            }
        }

        if (psShape != nullptr && psShape->nSHPType != SHPT_NULL)
        {
            OGRGeometry *poGeometry = nullptr;
            OGREnvelope sGeomEnv;
            // Test if we have a degenerated bounding box.
            if (psShape->nSHPType != SHPT_POINT &&
                psShape->nSHPType != SHPT_POINTZ &&
                psShape->nSHPType != SHPT_POINTM &&
                (psShape->dfXMin == psShape->dfXMax ||
                 psShape->dfYMin == psShape->dfYMax))
            {
                // Need to read the full geometry to compute the envelope.
                if (psShape == &sShape)
                    psShape = SHPReadObject(m_hSHP, iShape);

                if (psShape)
                {
                    poGeometry = SHPReadOGRObject(
                        m_hSHP, iShape, psShape, m_bHasWarnedWrongWindingOrder);
                    if (poGeometry)
                        poGeometry->getEnvelope(&sGeomEnv);
                    psShape = nullptr;
                }
            }
            else
            {
                // Trust the shape bounding box as the shape envelope.
                sGeomEnv.MinX = psShape->dfXMin;
                sGeomEnv.MinY = psShape->dfYMin;
                sGeomEnv.MaxX = psShape->dfXMax;
                sGeomEnv.MaxY = psShape->dfYMax;
            }

            /* --------------------------------------------------------------------
             */
            /*      If there is no */
            /*      intersection between the envelopes we are sure not to have
             */
            /*      any intersection. */
            /* --------------------------------------------------------------------
             */
            if (sGeomEnv.MaxX < m_sFilterEnvelope.MinX ||
                sGeomEnv.MaxY < m_sFilterEnvelope.MinY ||
                m_sFilterEnvelope.MaxX < sGeomEnv.MinX ||
                m_sFilterEnvelope.MaxY < sGeomEnv.MinY)
            {
            }
            /* --------------------------------------------------------------------
             */
            /*      If the filter geometry is its own envelope and if the */
            /*      envelope of the geometry is inside the filter geometry, */
            /*      the geometry itself is inside the filter geometry */
            /* --------------------------------------------------------------------
             */
            else if (m_bFilterIsEnvelope &&
                     sGeomEnv.MinX >= m_sFilterEnvelope.MinX &&
                     sGeomEnv.MinY >= m_sFilterEnvelope.MinY &&
                     sGeomEnv.MaxX <= m_sFilterEnvelope.MaxX &&
                     sGeomEnv.MaxY <= m_sFilterEnvelope.MaxY)
            {
                nFeatureCount++;
            }
            else
            {
                /* --------------------------------------------------------------------
                 */
                /*      Fallback to full intersect test (using GEOS) if we still
                 */
                /*      don't know for sure. */
                /* --------------------------------------------------------------------
                 */
                if (OGRGeometryFactory::haveGEOS())
                {
                    // Read the full geometry.
                    if (poGeometry == nullptr)
                    {
                        if (psShape == &sShape)
                            psShape = SHPReadObject(m_hSHP, iShape);
                        if (psShape)
                        {
                            poGeometry =
                                SHPReadOGRObject(m_hSHP, iShape, psShape,
                                                 m_bHasWarnedWrongWindingOrder);
                            psShape = nullptr;
                        }
                    }
                    if (poGeometry == nullptr)
                    {
                        nFeatureCount++;
                    }
                    else if (m_pPreparedFilterGeom != nullptr)
                    {
                        if (OGRPreparedGeometryIntersects(
                                m_pPreparedFilterGeom,
                                OGRGeometry::ToHandle(poGeometry)))
                        {
                            nFeatureCount++;
                        }
                    }
                    else if (m_poFilterGeom->Intersects(poGeometry))
                        nFeatureCount++;
                }
                else
                {
                    nFeatureCount++;
                }
            }

            delete poGeometry;
        }
        else
        {
            nFeatureCount++;
        }

        if (psShape && psShape != &sShape)
            SHPDestroyObject(psShape);
    }

    return nFeatureCount;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRShapeLayer::GetFeatureCount(int bForce)

{
    // Check if the spatial filter is non-trivial.
    bool bHasTrivialSpatialFilter = false;
    if (m_poFilterGeom != nullptr)
    {
        OGREnvelope oSpatialFilterEnvelope;
        m_poFilterGeom->getEnvelope(&oSpatialFilterEnvelope);

        OGREnvelope oLayerExtent;
        if (GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE)
        {
            if (oSpatialFilterEnvelope.Contains(oLayerExtent))
            {
                bHasTrivialSpatialFilter = true;
            }
            else
            {
                bHasTrivialSpatialFilter = false;
            }
        }
        else
        {
            bHasTrivialSpatialFilter = false;
        }
    }
    else
    {
        bHasTrivialSpatialFilter = true;
    }

    if (bHasTrivialSpatialFilter && m_poAttrQuery == nullptr)
        return m_nTotalShapeCount;

    if (!TouchLayer())
        return 0;

    // Spatial filter only.
    if (m_poAttrQuery == nullptr && m_hSHP != nullptr)
    {
        return GetFeatureCountWithSpatialFilterOnly();
    }

    // Attribute filter only.
    if (m_poAttrQuery != nullptr && m_poFilterGeom == nullptr)
    {
        // See if we can ignore reading geometries.
        const bool bSaveGeometryIgnored =
            CPL_TO_BOOL(m_poFeatureDefn->IsGeometryIgnored());
        if (!AttributeFilterEvaluationNeedsGeometry())
            m_poFeatureDefn->SetGeometryIgnored(TRUE);

        GIntBig nRet = OGRLayer::GetFeatureCount(bForce);

        m_poFeatureDefn->SetGeometryIgnored(bSaveGeometryIgnored);
        return nRet;
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                            IGetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHP files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRShapeLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                 bool bForce)

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if (m_hSHP == nullptr)
        return OGRERR_FAILURE;

    double adMin[4] = {0.0, 0.0, 0.0, 0.0};
    double adMax[4] = {0.0, 0.0, 0.0, 0.0};

    SHPGetInfo(m_hSHP, nullptr, nullptr, adMin, adMax);

    psExtent->MinX = adMin[0];
    psExtent->MinY = adMin[1];
    psExtent->MaxX = adMax[0];
    psExtent->MaxY = adMax[1];

    if (std::isnan(adMin[0]) || std::isnan(adMin[1]) || std::isnan(adMax[0]) ||
        std::isnan(adMax[1]))
    {
        CPLDebug("SHAPE", "Invalid extent in shape header");

        // Disable filters to avoid infinite recursion in GetNextFeature()
        // that calls ScanIndices() that call GetExtent.
        OGRFeatureQuery *poAttrQuery = m_poAttrQuery;
        m_poAttrQuery = nullptr;
        OGRGeometry *poFilterGeom = m_poFilterGeom;
        m_poFilterGeom = nullptr;

        psExtent->MinX = 0;
        psExtent->MinY = 0;
        psExtent->MaxX = 0;
        psExtent->MaxY = 0;

        const OGRErr eErr = OGRLayer::IGetExtent(iGeomField, psExtent, bForce);

        m_poAttrQuery = poAttrQuery;
        m_poFilterGeom = poFilterGeom;
        return eErr;
    }

    return OGRERR_NONE;
}

OGRErr OGRShapeLayer::IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                                   bool bForce)
{
    if (m_poFilterGeom || m_poAttrQuery)
        return OGRLayer::IGetExtent3D(iGeomField, psExtent3D, bForce);

    if (!TouchLayer())
        return OGRERR_FAILURE;

    if (m_hSHP == nullptr)
        return OGRERR_FAILURE;

    double adMin[4] = {0.0, 0.0, 0.0, 0.0};
    double adMax[4] = {0.0, 0.0, 0.0, 0.0};

    SHPGetInfo(m_hSHP, nullptr, nullptr, adMin, adMax);

    psExtent3D->MinX = adMin[0];
    psExtent3D->MinY = adMin[1];
    psExtent3D->MaxX = adMax[0];
    psExtent3D->MaxY = adMax[1];

    if (OGR_GT_HasZ(m_poFeatureDefn->GetGeomType()))
    {
        psExtent3D->MinZ = adMin[2];
        psExtent3D->MaxZ = adMax[2];
    }
    else
    {
        psExtent3D->MinZ = std::numeric_limits<double>::infinity();
        psExtent3D->MaxZ = -std::numeric_limits<double>::infinity();
    }

    if (std::isnan(adMin[0]) || std::isnan(adMin[1]) || std::isnan(adMax[0]) ||
        std::isnan(adMax[1]))
    {
        CPLDebug("SHAPE", "Invalid extent in shape header");

        // Disable filters to avoid infinite recursion in GetNextFeature()
        // that calls ScanIndices() that call GetExtent.
        OGRFeatureQuery *poAttrQuery = m_poAttrQuery;
        m_poAttrQuery = nullptr;
        OGRGeometry *poFilterGeom = m_poFilterGeom;
        m_poFilterGeom = nullptr;

        const OGRErr eErr =
            OGRLayer::IGetExtent3D(iGeomField, psExtent3D, bForce);

        m_poAttrQuery = poAttrQuery;
        m_poFilterGeom = poFilterGeom;
        return eErr;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeLayer::TestCapability(const char *pszCap)

{
    if (!TouchLayer())
        return FALSE;

    if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;

    if (EQUAL(pszCap, OLCSequentialWrite) || EQUAL(pszCap, OLCRandomWrite))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCFastFeatureCount))
    {
        if (!(m_poFilterGeom == nullptr || CheckForQIX() || CheckForSBN()))
            return FALSE;

        if (m_poAttrQuery != nullptr)
        {
            InitializeIndexSupport(m_osFullName.c_str());
            return m_poAttrQuery->CanUseIndex(this);
        }
        return TRUE;
    }

    if (EQUAL(pszCap, OLCDeleteFeature))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCFastSpatialFilter))
        return CheckForQIX() || CheckForSBN();

    if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;

    if (EQUAL(pszCap, OLCFastGetExtent3D))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

    if (EQUAL(pszCap, OLCFastSetNextByIndex))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

    if (EQUAL(pszCap, OLCCreateField))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCDeleteField))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCReorderFields))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCAlterFieldDefn) ||
        EQUAL(pszCap, OLCAlterGeomFieldDefn))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCRename))
        return m_bUpdateAccess;

    if (EQUAL(pszCap, OLCIgnoreFields))
        return TRUE;

    if (EQUAL(pszCap, OLCStringsAsUTF8))
    {
        // No encoding defined: we don't know.
        if (m_osEncoding.empty())
            return FALSE;

        if (m_hDBF == nullptr || DBFGetFieldCount(m_hDBF) == 0)
            return TRUE;

        // Otherwise test that we can re-encode field names to UTF-8.
        const int nFieldCount = DBFGetFieldCount(m_hDBF);
        for (int i = 0; i < nFieldCount; i++)
        {
            char szFieldName[XBASE_FLDNAME_LEN_READ + 1] = {};
            int nWidth = 0;
            int nPrecision = 0;

            DBFGetFieldInfo(m_hDBF, i, szFieldName, &nWidth, &nPrecision);

            if (!CPLCanRecode(szFieldName, m_osEncoding, CPL_ENC_UTF8))
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    if (EQUAL(pszCap, OLCMeasuredGeometries))
        return TRUE;

    if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::CreateField(const OGRFieldDefn *poFieldDefn,
                                  int bApproxOK)

{
    if (!StartUpdate("CreateField"))
        return OGRERR_FAILURE;

    CPLAssert(nullptr != poFieldDefn);

    bool bDBFJustCreated = false;
    if (m_hDBF == nullptr)
    {
        const CPLString osFilename =
            CPLResetExtensionSafe(m_osFullName.c_str(), "dbf");
        m_hDBF = DBFCreate(osFilename);

        if (m_hDBF == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to create DBF file `%s'.", osFilename.c_str());
            return OGRERR_FAILURE;
        }

        bDBFJustCreated = true;
    }

    if (m_hDBF->nHeaderLength + XBASE_FLDHDR_SZ > 65535)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field %s. Header length limit reached "
                 "(max 65535 bytes, 2046 fields).",
                 poFieldDefn->GetNameRef());
        return OGRERR_FAILURE;
    }

    CPLErrorReset();

    if (m_poFeatureDefn->GetFieldCount() == 255)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Creating a 256th field, "
                 "but some DBF readers might only support 255 fields");
    }

    /* -------------------------------------------------------------------- */
    /*      Normalize field name                                            */
    /* -------------------------------------------------------------------- */
    CPLString osFieldName;
    if (!m_osEncoding.empty())
    {
        CPLClearRecodeWarningFlags();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLErr eLastErr = CPLGetLastErrorType();
        char *const pszRecoded =
            CPLRecode(poFieldDefn->GetNameRef(), CPL_ENC_UTF8, m_osEncoding);
        CPLPopErrorHandler();
        osFieldName = pszRecoded;
        CPLFree(pszRecoded);
        if (CPLGetLastErrorType() != eLastErr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create field name '%s': cannot convert to %s",
                     poFieldDefn->GetNameRef(), m_osEncoding.c_str());
            return OGRERR_FAILURE;
        }
    }
    else
    {
        osFieldName = poFieldDefn->GetNameRef();
    }

    const int nNameSize = static_cast<int>(osFieldName.size());
    char szNewFieldName[XBASE_FLDNAME_LEN_WRITE + 1];
    CPLString osRadixFieldName;
    CPLString osRadixFieldNameUC;
    {
        char *pszTmp = CPLScanString(
            osFieldName, std::min(nNameSize, XBASE_FLDNAME_LEN_WRITE), TRUE,
            TRUE);
        strncpy(szNewFieldName, pszTmp, sizeof(szNewFieldName) - 1);
        szNewFieldName[sizeof(szNewFieldName) - 1] = '\0';
        osRadixFieldName = pszTmp;
        osRadixFieldNameUC = CPLString(osRadixFieldName).toupper();
        CPLFree(pszTmp);
    }

    CPLString osNewFieldNameUC(szNewFieldName);
    osNewFieldNameUC.toupper();

    if (m_oSetUCFieldName.empty())
    {
        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
        {
            CPLString key(m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            key.toupper();
            m_oSetUCFieldName.insert(key);
        }
    }

    bool bFoundFieldName =
        m_oSetUCFieldName.find(osNewFieldNameUC) != m_oSetUCFieldName.end();

    if (!bApproxOK && (bFoundFieldName || !EQUAL(osFieldName, szNewFieldName)))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Failed to add field named '%s'", poFieldDefn->GetNameRef());

        return OGRERR_FAILURE;
    }

    if (bFoundFieldName)
    {
        int nRenameNum = 1;
        while (bFoundFieldName && nRenameNum < 10)
        {
            CPLsnprintf(szNewFieldName, sizeof(szNewFieldName), "%.8s_%.1d",
                        osRadixFieldName.c_str(), nRenameNum);
            osNewFieldNameUC.Printf("%.8s_%.1d", osRadixFieldNameUC.c_str(),
                                    nRenameNum);
            bFoundFieldName = m_oSetUCFieldName.find(osNewFieldNameUC) !=
                              m_oSetUCFieldName.end();
            nRenameNum++;
        }

        while (bFoundFieldName && nRenameNum < 100)
        {
            CPLsnprintf(szNewFieldName, sizeof(szNewFieldName), "%.8s%.2d",
                        osRadixFieldName.c_str(), nRenameNum);
            osNewFieldNameUC.Printf("%.8s%.2d", osRadixFieldNameUC.c_str(),
                                    nRenameNum);
            bFoundFieldName = m_oSetUCFieldName.find(osNewFieldNameUC) !=
                              m_oSetUCFieldName.end();
            nRenameNum++;
        }

        if (bFoundFieldName)
        {
            // One hundred similar field names!!?
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Too many field names like '%s' when truncated to %d letters "
                "for Shapefile format.",
                poFieldDefn->GetNameRef(), XBASE_FLDNAME_LEN_WRITE);
            return OGRERR_FAILURE;
        }
    }

    OGRFieldDefn oModFieldDefn(poFieldDefn);

    if (!EQUAL(osFieldName, szNewFieldName))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Normalized/laundered field name: '%s' to '%s'",
                 poFieldDefn->GetNameRef(), szNewFieldName);

        // Set field name with normalized value.
        oModFieldDefn.SetName(szNewFieldName);
    }

    /* -------------------------------------------------------------------- */
    /*      Add field to layer                                              */
    /* -------------------------------------------------------------------- */
    char chType = 'C';
    int nWidth = 0;
    int nDecimals = 0;

    switch (oModFieldDefn.GetType())
    {
        case OFTInteger:
            if (oModFieldDefn.GetSubType() == OFSTBoolean)
            {
                chType = 'L';
                nWidth = 1;
            }
            else
            {
                chType = 'N';
                nWidth = oModFieldDefn.GetWidth();
                if (nWidth == 0)
                    nWidth = 9;
            }
            break;

        case OFTInteger64:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            if (nWidth == 0)
                nWidth = 18;
            break;

        case OFTReal:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            nDecimals = oModFieldDefn.GetPrecision();
            if (nWidth == 0)
            {
                nWidth = 24;
                nDecimals = 15;
            }
            break;

        case OFTString:
            chType = 'C';
            nWidth = oModFieldDefn.GetWidth();
            if (nWidth == 0)
                nWidth = 80;
            else if (nWidth > OGR_DBF_MAX_FIELD_WIDTH)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %s of width %d truncated to %d.",
                         szNewFieldName, nWidth, OGR_DBF_MAX_FIELD_WIDTH);
                nWidth = OGR_DBF_MAX_FIELD_WIDTH;
            }
            break;

        case OFTDate:
            chType = 'D';
            nWidth = 8;
            break;

        case OFTDateTime:
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "Field %s created as String field, though DateTime requested.",
                szNewFieldName);
            chType = 'C';
            nWidth = static_cast<int>(strlen("YYYY-MM-DDTHH:MM:SS.sss+HH:MM"));
            oModFieldDefn.SetType(OFTString);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Can't create fields of type %s on shapefile layers.",
                     OGRFieldDefn::GetFieldTypeName(oModFieldDefn.GetType()));

            return OGRERR_FAILURE;
            break;
    }

    oModFieldDefn.SetWidth(nWidth);
    oModFieldDefn.SetPrecision(nDecimals);

    // Suppress the dummy FID field if we have created it just before.
    if (DBFGetFieldCount(m_hDBF) == 1 && m_poFeatureDefn->GetFieldCount() == 0)
    {
        DBFDeleteField(m_hDBF, 0);
    }

    const int iNewField = DBFAddNativeFieldType(m_hDBF, szNewFieldName, chType,
                                                nWidth, nDecimals);

    if (iNewField != -1)
    {
        m_oSetUCFieldName.insert(osNewFieldNameUC);

        whileUnsealing(m_poFeatureDefn)->AddFieldDefn(&oModFieldDefn);

        if (bDBFJustCreated)
        {
            for (int i = 0; i < m_nTotalShapeCount; i++)
            {
                DBFWriteNULLAttribute(m_hDBF, i, 0);
            }
        }

        return OGRERR_NONE;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Can't create field %s in Shape DBF file, reason unknown.",
             szNewFieldName);

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteField(int iField)
{
    if (!StartUpdate("DeleteField"))
        return OGRERR_FAILURE;

    if (iField < 0 || iField >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    m_oSetUCFieldName.clear();

    if (DBFDeleteField(m_hDBF, iField))
    {
        TruncateDBF();

        return whileUnsealing(m_poFeatureDefn)->DeleteFieldDefn(iField);
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRShapeLayer::ReorderFields(int *panMap)
{
    if (!StartUpdate("ReorderFields"))
        return OGRERR_FAILURE;

    if (m_poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, m_poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

    if (DBFReorderFields(m_hDBF, panMap))
    {
        return whileUnsealing(m_poFeatureDefn)->ReorderFieldDefns(panMap);
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRShapeLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                     int nFlagsIn)
{
    if (!StartUpdate("AlterFieldDefn"))
        return OGRERR_FAILURE;

    if (iField < 0 || iField >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    m_oSetUCFieldName.clear();

    OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
    OGRFieldType eType = poFieldDefn->GetType();

    auto oTemporaryUnsealer(poFieldDefn->GetTemporaryUnsealer());

    // On reading we support up to 11 characters
    char szFieldName[XBASE_FLDNAME_LEN_READ + 1] = {};
    int nWidth = 0;
    int nPrecision = 0;
    DBFGetFieldInfo(m_hDBF, iField, szFieldName, &nWidth, &nPrecision);
    char chNativeType = DBFGetNativeFieldType(m_hDBF, iField);

    if ((nFlagsIn & ALTER_TYPE_FLAG) &&
        poNewFieldDefn->GetType() != poFieldDefn->GetType())
    {
        if (poNewFieldDefn->GetType() == OFTInteger64 &&
            poFieldDefn->GetType() == OFTInteger)
        {
            eType = poNewFieldDefn->GetType();
        }
        else if (poNewFieldDefn->GetType() != OFTString)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Can only convert to OFTString");
            return OGRERR_FAILURE;
        }
        else
        {
            chNativeType = 'C';
            eType = poNewFieldDefn->GetType();
        }
    }

    if (nFlagsIn & ALTER_NAME_FLAG)
    {
        CPLString osFieldName;
        if (!m_osEncoding.empty())
        {
            CPLClearRecodeWarningFlags();
            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            char *pszRecoded = CPLRecode(poNewFieldDefn->GetNameRef(),
                                         CPL_ENC_UTF8, m_osEncoding);
            CPLPopErrorHandler();
            osFieldName = pszRecoded;
            CPLFree(pszRecoded);
            if (CPLGetLastErrorType() != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to rename field name to '%s': "
                         "cannot convert to %s",
                         poNewFieldDefn->GetNameRef(), m_osEncoding.c_str());
                return OGRERR_FAILURE;
            }
        }
        else
        {
            osFieldName = poNewFieldDefn->GetNameRef();
        }

        strncpy(szFieldName, osFieldName, sizeof(szFieldName) - 1);
        szFieldName[sizeof(szFieldName) - 1] = '\0';
    }
    if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        nWidth = poNewFieldDefn->GetWidth();
        nPrecision = poNewFieldDefn->GetPrecision();
    }

    if (DBFAlterFieldDefn(m_hDBF, iField, szFieldName, chNativeType, nWidth,
                          nPrecision))
    {
        if (nFlagsIn & ALTER_TYPE_FLAG)
            poFieldDefn->SetType(eType);
        if (nFlagsIn & ALTER_NAME_FLAG)
            poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
        if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
        {
            poFieldDefn->SetWidth(nWidth);
            poFieldDefn->SetPrecision(nPrecision);

            TruncateDBF();
        }
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                         AlterGeomFieldDefn()                         */
/************************************************************************/

OGRErr OGRShapeLayer::AlterGeomFieldDefn(
    int iGeomField, const OGRGeomFieldDefn *poNewGeomFieldDefn, int nFlagsIn)
{
    if (!StartUpdate("AlterGeomFieldDefn"))
        return OGRERR_FAILURE;

    if (iGeomField < 0 || iGeomField >= m_poFeatureDefn->GetGeomFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    auto poFieldDefn = cpl::down_cast<OGRShapeGeomFieldDefn *>(
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField));
    auto oTemporaryUnsealer(poFieldDefn->GetTemporaryUnsealer());

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NAME_FLAG)
    {
        if (strcmp(poNewGeomFieldDefn->GetNameRef(),
                   poFieldDefn->GetNameRef()) != 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the geometry field name is not supported for "
                     "shapefiles");
            return OGRERR_FAILURE;
        }
    }

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_TYPE_FLAG)
    {
        if (poFieldDefn->GetType() != poNewGeomFieldDefn->GetType())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the geometry field type is not supported for "
                     "shapefiles");
            return OGRERR_FAILURE;
        }
    }

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG)
    {
        const auto poNewSRSRef = poNewGeomFieldDefn->GetSpatialRef();
        if (poNewSRSRef && poNewSRSRef->GetCoordinateEpoch() > 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Setting a coordinate epoch is not supported for "
                     "shapefiles");
            return OGRERR_FAILURE;
        }
    }

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_FLAG)
    {
        if (poFieldDefn->GetPrjFilename().empty())
        {
            poFieldDefn->SetPrjFilename(
                CPLResetExtensionSafe(m_osFullName.c_str(), "prj").c_str());
        }

        const auto poNewSRSRef = poNewGeomFieldDefn->GetSpatialRef();
        if (poNewSRSRef)
        {
            char *pszWKT = nullptr;
            VSILFILE *fp = nullptr;
            const char *const apszOptions[] = {"FORMAT=WKT1_ESRI", nullptr};
            if (poNewSRSRef->exportToWkt(&pszWKT, apszOptions) == OGRERR_NONE &&
                (fp = VSIFOpenL(poFieldDefn->GetPrjFilename().c_str(), "wt")) !=
                    nullptr)
            {
                VSIFWriteL(pszWKT, strlen(pszWKT), 1, fp);
                VSIFCloseL(fp);
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot write %s",
                         poFieldDefn->GetPrjFilename().c_str());
                CPLFree(pszWKT);
                return OGRERR_FAILURE;
            }

            CPLFree(pszWKT);

            auto poNewSRS = poNewSRSRef->Clone();
            poFieldDefn->SetSpatialRef(poNewSRS);
            poNewSRS->Release();
        }
        else
        {
            poFieldDefn->SetSpatialRef(nullptr);
            VSIStatBufL sStat;
            if (VSIStatL(poFieldDefn->GetPrjFilename().c_str(), &sStat) == 0 &&
                VSIUnlink(poFieldDefn->GetPrjFilename().c_str()) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot delete %s",
                         poFieldDefn->GetPrjFilename().c_str());
                return OGRERR_FAILURE;
            }
        }
        poFieldDefn->SetSRSSet();
    }

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NAME_FLAG)
        poFieldDefn->SetName(poNewGeomFieldDefn->GetNameRef());
    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG)
        poFieldDefn->SetNullable(poNewGeomFieldDefn->IsNullable());

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *OGRShapeGeomFieldDefn::GetSpatialRef() const

{
    if (m_bSRSSet)
        return poSRS;

    m_bSRSSet = true;

    /* -------------------------------------------------------------------- */
    /*      Is there an associated .prj file we can read?                   */
    /* -------------------------------------------------------------------- */
    std::string l_osPrjFile =
        CPLResetExtensionSafe(m_osFullName.c_str(), "prj");

    char *apszOptions[] = {
        const_cast<char *>("EMIT_ERROR_IF_CANNOT_OPEN_FILE=FALSE"), nullptr};
    char **papszLines = CSLLoad2(l_osPrjFile.c_str(), -1, -1, apszOptions);
    if (papszLines == nullptr)
    {
        l_osPrjFile = CPLResetExtensionSafe(m_osFullName.c_str(), "PRJ");
        papszLines = CSLLoad2(l_osPrjFile.c_str(), -1, -1, apszOptions);
    }

    if (papszLines != nullptr)
    {
        m_osPrjFile = std::move(l_osPrjFile);

        auto poSRSNonConst = new OGRSpatialReference();
        poSRSNonConst->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // Remove UTF-8 BOM if found
        // http://lists.osgeo.org/pipermail/gdal-dev/2014-July/039527.html
        if (static_cast<unsigned char>(papszLines[0][0]) == 0xEF &&
            static_cast<unsigned char>(papszLines[0][1]) == 0xBB &&
            static_cast<unsigned char>(papszLines[0][2]) == 0xBF)
        {
            memmove(papszLines[0], papszLines[0] + 3,
                    strlen(papszLines[0] + 3) + 1);
        }
        if (STARTS_WITH_CI(papszLines[0], "GEOGCS["))
        {
            // Strip AXIS[] in GEOGCS to address use case of
            // https://github.com/OSGeo/gdal/issues/8452
            std::string osVal;
            for (CSLConstList papszIter = papszLines; *papszIter; ++papszIter)
                osVal += *papszIter;
            OGR_SRSNode oSRSNode;
            const char *pszVal = osVal.c_str();
            if (oSRSNode.importFromWkt(&pszVal) == OGRERR_NONE)
            {
                oSRSNode.StripNodes("AXIS");
                char *pszWKT = nullptr;
                oSRSNode.exportToWkt(&pszWKT);
                if (pszWKT)
                {
                    CSLDestroy(papszLines);
                    papszLines =
                        static_cast<char **>(CPLCalloc(2, sizeof(char *)));
                    papszLines[0] = pszWKT;
                }
            }
        }
        if (poSRSNonConst->importFromESRI(papszLines) != OGRERR_NONE)
        {
            delete poSRSNonConst;
            poSRSNonConst = nullptr;
        }
        CSLDestroy(papszLines);

        if (poSRSNonConst)
        {
            if (CPLTestBool(CPLGetConfigOption("USE_OSR_FIND_MATCHES", "YES")))
            {
                auto poSRSMatch = poSRSNonConst->FindBestMatch();
                if (poSRSMatch)
                {
                    poSRSNonConst->Release();
                    poSRSNonConst = poSRSMatch;
                    poSRSNonConst->SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                }
            }
            else
            {
                poSRSNonConst->AutoIdentifyEPSG();
            }
            poSRS = poSRSNonConst;
        }
    }

    return poSRS;
}

/************************************************************************/
/*                           ResetGeomType()                            */
/*                                                                      */
/*      Modify the geometry type for this file.  Used to convert to     */
/*      a different geometry type when a layer was created with a       */
/*      type of unknown, and we get to the first feature to             */
/*      establish the type.                                             */
/************************************************************************/

int OGRShapeLayer::ResetGeomType(int nNewGeomType)

{
    if (m_nTotalShapeCount > 0)
        return FALSE;

    if (m_hSHP->fpSHX == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGRShapeLayer::ResetGeomType failed: SHX file is closed");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Update .shp header.                                             */
    /* -------------------------------------------------------------------- */
    int nStartPos = static_cast<int>(m_hSHP->sHooks.FTell(m_hSHP->fpSHP));

    char abyHeader[100] = {};
    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHP, 0, SEEK_SET) != 0 ||
        m_hSHP->sHooks.FRead(abyHeader, 100, 1, m_hSHP->fpSHP) != 1)
        return FALSE;

    *(reinterpret_cast<GInt32 *>(abyHeader + 32)) = CPL_LSBWORD32(nNewGeomType);

    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHP, 0, SEEK_SET) != 0 ||
        m_hSHP->sHooks.FWrite(abyHeader, 100, 1, m_hSHP->fpSHP) != 1)
        return FALSE;

    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHP, nStartPos, SEEK_SET) != 0)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Update .shx header.                                             */
    /* -------------------------------------------------------------------- */
    nStartPos = static_cast<int>(m_hSHP->sHooks.FTell(m_hSHP->fpSHX));

    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHX, 0, SEEK_SET) != 0 ||
        m_hSHP->sHooks.FRead(abyHeader, 100, 1, m_hSHP->fpSHX) != 1)
        return FALSE;

    *(reinterpret_cast<GInt32 *>(abyHeader + 32)) = CPL_LSBWORD32(nNewGeomType);

    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHX, 0, SEEK_SET) != 0 ||
        m_hSHP->sHooks.FWrite(abyHeader, 100, 1, m_hSHP->fpSHX) != 1)
        return FALSE;

    if (m_hSHP->sHooks.FSeek(m_hSHP->fpSHX, nStartPos, SEEK_SET) != 0)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Update other information.                                       */
    /* -------------------------------------------------------------------- */
    m_hSHP->nShapeType = nNewGeomType;

    return TRUE;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRShapeLayer::SyncToDisk()

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if (m_bHeaderDirty)
    {
        if (m_hSHP != nullptr)
            SHPWriteHeader(m_hSHP);

        if (m_hDBF != nullptr)
            DBFUpdateHeader(m_hDBF);

        m_bHeaderDirty = false;
    }

    if (m_hSHP != nullptr)
    {
        m_hSHP->sHooks.FFlush(m_hSHP->fpSHP);
        if (m_hSHP->fpSHX != nullptr)
            m_hSHP->sHooks.FFlush(m_hSHP->fpSHX);
    }

    if (m_hDBF != nullptr)
    {
        m_hDBF->sHooks.FFlush(m_hDBF->fp);
    }

    if (m_eNeedRepack == YES && m_bAutoRepack)
        Repack();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DropSpatialIndex()                          */
/************************************************************************/

OGRErr OGRShapeLayer::DropSpatialIndex()

{
    if (!StartUpdate("DropSpatialIndex"))
        return OGRERR_FAILURE;

    if (!CheckForQIX() && !CheckForSBN())
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Layer %s has no spatial index, DROP SPATIAL INDEX failed.",
                 m_poFeatureDefn->GetName());
        return OGRERR_FAILURE;
    }

    const bool bHadQIX = m_hQIX != nullptr;

    SHPCloseDiskTree(m_hQIX);
    m_hQIX = nullptr;
    m_bCheckedForQIX = false;

    SBNCloseDiskTree(m_hSBN);
    m_hSBN = nullptr;
    m_bCheckedForSBN = false;

    if (bHadQIX)
    {
        const std::string osQIXFilename =
            CPLResetExtensionSafe(m_osFullName.c_str(), "qix");
        CPLDebug("SHAPE", "Unlinking index file %s", osQIXFilename.c_str());

        if (VSIUnlink(osQIXFilename.c_str()) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to delete file %s.\n%s", osQIXFilename.c_str(),
                     VSIStrerror(errno));
            return OGRERR_FAILURE;
        }
    }

    if (!m_bSbnSbxDeleted)
    {
        const char papszExt[2][4] = {"sbn", "sbx"};
        for (int i = 0; i < 2; i++)
        {
            const std::string osIndexFilename =
                CPLResetExtensionSafe(m_osFullName.c_str(), papszExt[i]);
            CPLDebug("SHAPE", "Trying to unlink index file %s",
                     osIndexFilename.c_str());

            if (VSIUnlink(osIndexFilename.c_str()) != 0)
            {
                CPLDebug("SHAPE", "Failed to delete file %s.\n%s",
                         osIndexFilename.c_str(), VSIStrerror(errno));
            }
        }
    }
    m_bSbnSbxDeleted = true;

    ClearSpatialFIDs();

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

OGRErr OGRShapeLayer::CreateSpatialIndex(int nMaxDepth)

{
    if (!StartUpdate("CreateSpatialIndex"))
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      If we have an existing spatial index, blow it away first.       */
    /* -------------------------------------------------------------------- */
    if (CheckForQIX())
        DropSpatialIndex();

    m_bCheckedForQIX = false;

    /* -------------------------------------------------------------------- */
    /*      Build a quadtree structure for this file.                       */
    /* -------------------------------------------------------------------- */
    OGRShapeLayer::SyncToDisk();
    SHPTree *psTree = SHPCreateTree(m_hSHP, 2, nMaxDepth, nullptr, nullptr);

    if (nullptr == psTree)
    {
        // TODO(mloskot): Is it better to return OGRERR_NOT_ENOUGH_MEMORY?
        CPLDebug("SHAPE",
                 "Index creation failure. Likely, memory allocation error.");

        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Trim unused nodes from the tree.                                */
    /* -------------------------------------------------------------------- */
    SHPTreeTrimExtraNodes(psTree);

    /* -------------------------------------------------------------------- */
    /*      Dump tree to .qix file.                                         */
    /* -------------------------------------------------------------------- */
    char *pszQIXFilename =
        CPLStrdup(CPLResetExtensionSafe(m_osFullName.c_str(), "qix").c_str());

    CPLDebug("SHAPE", "Creating index file %s", pszQIXFilename);

    SHPWriteTree(psTree, pszQIXFilename);
    CPLFree(pszQIXFilename);

    /* -------------------------------------------------------------------- */
    /*      cleanup                                                         */
    /* -------------------------------------------------------------------- */
    SHPDestroyTree(psTree);

    CPL_IGNORE_RET_VAL(CheckForQIX());

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CheckFileDeletion()                            */
/************************************************************************/

static void CheckFileDeletion(const CPLString &osFilename)
{
    // On Windows, sometimes the file is still triansiently reported
    // as existing although being deleted, which makes QGIS things that
    // an issue arose. The following helps to reduce that risk.
    VSIStatBufL sStat;
    if (VSIStatL(osFilename, &sStat) == 0 && VSIStatL(osFilename, &sStat) == 0)
    {
        CPLDebug("Shape",
                 "File %s is still reported as existing whereas "
                 "it should have been deleted",
                 osFilename.c_str());
    }
}

/************************************************************************/
/*                         ForceDeleteFile()                            */
/************************************************************************/

static void ForceDeleteFile(const CPLString &osFilename)
{
    if (VSIUnlink(osFilename) != 0)
    {
        // In case of failure retry with a small delay (Windows specific)
        CPLSleep(0.1);
        if (VSIUnlink(osFilename) != 0)
        {
            CPLDebug("Shape", "Cannot delete %s : %s", osFilename.c_str(),
                     VSIStrerror(errno));
        }
    }
    CheckFileDeletion(osFilename);
}

/************************************************************************/
/*                               Repack()                               */
/*                                                                      */
/*      Repack the shape and dbf file, dropping deleted records.        */
/*      FIDs may change.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::Repack()

{
    if (m_eNeedRepack == NO)
    {
        CPLDebug("Shape", "REPACK: nothing to do. Was done previously");
        return OGRERR_NONE;
    }

    if (!StartUpdate("Repack"))
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Build a list of records to be dropped.                          */
    /* -------------------------------------------------------------------- */
    std::vector<int> anRecordsToDelete;
    OGRErr eErr = OGRERR_NONE;

    CPLDebug("Shape", "REPACK: Checking if features have been deleted");

    if (m_hDBF != nullptr)
    {
        try
        {
            for (int iShape = 0; iShape < m_nTotalShapeCount; iShape++)
            {
                if (DBFIsRecordDeleted(m_hDBF, iShape))
                {
                    anRecordsToDelete.push_back(iShape);
                }
                if (VSIFEofL(VSI_SHP_GetVSIL(m_hDBF->fp)) ||
                    VSIFErrorL(VSI_SHP_GetVSIL(m_hDBF->fp)))
                {
                    return OGRERR_FAILURE;  // I/O error.
                }
            }
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory in Repack()");
            return OGRERR_FAILURE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If there are no records marked for deletion, we take no         */
    /*      action.                                                         */
    /* -------------------------------------------------------------------- */
    if (anRecordsToDelete.empty() && !m_bSHPNeedsRepack)
    {
        CPLDebug("Shape", "REPACK: nothing to do");
        return OGRERR_NONE;
    }

    /* -------------------------------------------------------------------- */
    /*      Find existing filenames with exact case (see #3293).            */
    /* -------------------------------------------------------------------- */
    const CPLString osDirname(CPLGetPathSafe(m_osFullName.c_str()));
    const CPLString osBasename(CPLGetBasenameSafe(m_osFullName.c_str()));

    CPLString osDBFName;
    CPLString osSHPName;
    CPLString osSHXName;
    CPLString osCPGName;
    char **papszCandidates = VSIReadDir(osDirname);
    int i = 0;
    while (papszCandidates != nullptr && papszCandidates[i] != nullptr)
    {
        const CPLString osCandidateBasename =
            CPLGetBasenameSafe(papszCandidates[i]);
        const CPLString osCandidateExtension =
            CPLGetExtensionSafe(papszCandidates[i]);
#ifdef _WIN32
        // On Windows, as filenames are case insensitive, a shapefile layer can
        // be made of foo.shp and FOO.DBF, so use case insensitive comparison.
        if (EQUAL(osCandidateBasename, osBasename))
#else
        if (osCandidateBasename.compare(osBasename) == 0)
#endif
        {
            if (EQUAL(osCandidateExtension, "dbf"))
                osDBFName =
                    CPLFormFilenameSafe(osDirname, papszCandidates[i], nullptr);
            else if (EQUAL(osCandidateExtension, "shp"))
                osSHPName =
                    CPLFormFilenameSafe(osDirname, papszCandidates[i], nullptr);
            else if (EQUAL(osCandidateExtension, "shx"))
                osSHXName =
                    CPLFormFilenameSafe(osDirname, papszCandidates[i], nullptr);
            else if (EQUAL(osCandidateExtension, "cpg"))
                osCPGName =
                    CPLFormFilenameSafe(osDirname, papszCandidates[i], nullptr);
        }

        i++;
    }
    CSLDestroy(papszCandidates);
    papszCandidates = nullptr;

    if (m_hDBF != nullptr && osDBFName.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the DBF file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        return OGRERR_FAILURE;
    }

    if (m_hSHP != nullptr && osSHPName.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the SHP file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        return OGRERR_FAILURE;
    }

    if (m_hSHP != nullptr && osSHXName.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the SHX file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup any existing spatial index.  It will become             */
    /*      meaningless when the fids change.                               */
    /* -------------------------------------------------------------------- */
    if (CheckForQIX() || CheckForSBN())
        DropSpatialIndex();

    /* -------------------------------------------------------------------- */
    /*      Create a new dbf file, matching the old.                        */
    /* -------------------------------------------------------------------- */
    bool bMustReopenDBF = false;
    CPLString oTempFileDBF;
    const int nNewRecords =
        m_nTotalShapeCount - static_cast<int>(anRecordsToDelete.size());

    if (m_hDBF != nullptr && !anRecordsToDelete.empty())
    {
        CPLDebug("Shape", "REPACK: repacking .dbf");
        bMustReopenDBF = true;

        oTempFileDBF = CPLFormFilenameSafe(osDirname, osBasename, nullptr);
        oTempFileDBF += "_packed.dbf";

        DBFHandle hNewDBF = DBFCloneEmpty(m_hDBF, oTempFileDBF);
        if (hNewDBF == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to create temp file %s.", oTempFileDBF.c_str());
            return OGRERR_FAILURE;
        }

        // Delete temporary .cpg file if existing.
        if (!osCPGName.empty())
        {
            CPLString oCPGTempFile =
                CPLFormFilenameSafe(osDirname, osBasename, nullptr);
            oCPGTempFile += "_packed.cpg";
            ForceDeleteFile(oCPGTempFile);
        }

        /* --------------------------------------------------------------------
         */
        /*      Copy over all records that are not deleted. */
        /* --------------------------------------------------------------------
         */
        int iDestShape = 0;
        size_t iNextDeletedShape = 0;

        for (int iShape = 0; iShape < m_nTotalShapeCount && eErr == OGRERR_NONE;
             iShape++)
        {
            if (iNextDeletedShape < anRecordsToDelete.size() &&
                anRecordsToDelete[iNextDeletedShape] == iShape)
            {
                iNextDeletedShape++;
            }
            else
            {
                void *pTuple = const_cast<char *>(DBFReadTuple(m_hDBF, iShape));
                if (pTuple == nullptr ||
                    !DBFWriteTuple(hNewDBF, iDestShape++, pTuple))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error writing record %d in .dbf", iShape);
                    eErr = OGRERR_FAILURE;
                }
            }
        }

        DBFClose(hNewDBF);

        if (eErr != OGRERR_NONE)
        {
            VSIUnlink(oTempFileDBF);
            return eErr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Now create a shapefile matching the old one.                    */
    /* -------------------------------------------------------------------- */
    bool bMustReopenSHP = m_hSHP != nullptr;
    CPLString oTempFileSHP;
    CPLString oTempFileSHX;

    SHPInfo sSHPInfo;
    memset(&sSHPInfo, 0, sizeof(sSHPInfo));
    unsigned int *panRecOffsetNew = nullptr;
    unsigned int *panRecSizeNew = nullptr;

    // On Windows, use the pack-in-place approach, ie copy the content of
    // the _packed files on top of the existing opened files. This avoids
    // many issues with files being locked, at the expense of more I/O
    const bool bPackInPlace =
        CPLTestBool(CPLGetConfigOption("OGR_SHAPE_PACK_IN_PLACE",
#ifdef _WIN32
                                       "YES"
#else
                                       "NO"
#endif
                                       ));

    if (m_hSHP != nullptr)
    {
        CPLDebug("Shape", "REPACK: repacking .shp + .shx");

        oTempFileSHP = CPLFormFilenameSafe(osDirname, osBasename, nullptr);
        oTempFileSHP += "_packed.shp";
        oTempFileSHX = CPLFormFilenameSafe(osDirname, osBasename, nullptr);
        oTempFileSHX += "_packed.shx";

        SHPHandle hNewSHP = SHPCreate(oTempFileSHP, m_hSHP->nShapeType);
        if (hNewSHP == nullptr)
        {
            if (!oTempFileDBF.empty())
                VSIUnlink(oTempFileDBF);
            return OGRERR_FAILURE;
        }

        /* --------------------------------------------------------------------
         */
        /*      Copy over all records that are not deleted. */
        /* --------------------------------------------------------------------
         */
        size_t iNextDeletedShape = 0;

        for (int iShape = 0; iShape < m_nTotalShapeCount && eErr == OGRERR_NONE;
             iShape++)
        {
            if (iNextDeletedShape < anRecordsToDelete.size() &&
                anRecordsToDelete[iNextDeletedShape] == iShape)
            {
                iNextDeletedShape++;
            }
            else
            {
                SHPObject *hObject = SHPReadObject(m_hSHP, iShape);
                if (hObject == nullptr ||
                    SHPWriteObject(hNewSHP, -1, hObject) == -1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error writing record %d in .shp", iShape);
                    eErr = OGRERR_FAILURE;
                }

                if (hObject)
                    SHPDestroyObject(hObject);
            }
        }

        if (bPackInPlace)
        {
            // Backup information of the updated shape context so as to
            // restore it later in the current shape context
            memcpy(&sSHPInfo, hNewSHP, sizeof(sSHPInfo));

            // Use malloc like shapelib does
            panRecOffsetNew = reinterpret_cast<unsigned int *>(
                malloc(sizeof(unsigned int) * hNewSHP->nMaxRecords));
            panRecSizeNew = reinterpret_cast<unsigned int *>(
                malloc(sizeof(unsigned int) * hNewSHP->nMaxRecords));
            if (panRecOffsetNew == nullptr || panRecSizeNew == nullptr)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate panRecOffsetNew/panRecSizeNew");
                eErr = OGRERR_FAILURE;
            }
            else
            {
                memcpy(panRecOffsetNew, hNewSHP->panRecOffset,
                       sizeof(unsigned int) * hNewSHP->nRecords);
                memcpy(panRecSizeNew, hNewSHP->panRecSize,
                       sizeof(unsigned int) * hNewSHP->nRecords);
            }
        }

        SHPClose(hNewSHP);

        if (eErr != OGRERR_NONE)
        {
            VSIUnlink(oTempFileSHP);
            VSIUnlink(oTempFileSHX);
            if (!oTempFileDBF.empty())
                VSIUnlink(oTempFileDBF);
            free(panRecOffsetNew);
            free(panRecSizeNew);
            return eErr;
        }
    }

    // We could also use pack in place for Unix but this involves extra I/O
    // w.r.t to the delete and rename approach

    if (bPackInPlace)
    {
        if (m_hDBF != nullptr && !oTempFileDBF.empty())
        {
            if (!OGRShapeDataSource::CopyInPlace(VSI_SHP_GetVSIL(m_hDBF->fp),
                                                 oTempFileDBF))
            {
                CPLError(
                    CE_Failure, CPLE_FileIO,
                    "An error occurred while copying the content of %s on top "
                    "of %s. "
                    "The non corrupted version is in the _packed.dbf, "
                    "_packed.shp and _packed.shx files that you should rename "
                    "on top of the main ones.",
                    oTempFileDBF.c_str(), VSI_SHP_GetFilename(m_hDBF->fp));
                free(panRecOffsetNew);
                free(panRecSizeNew);

                DBFClose(m_hDBF);
                m_hDBF = nullptr;
                if (m_hSHP != nullptr)
                {
                    SHPClose(m_hSHP);
                    m_hSHP = nullptr;
                }

                return OGRERR_FAILURE;
            }

            // Refresh current handle
            m_hDBF->nRecords = nNewRecords;
        }

        if (m_hSHP != nullptr && !oTempFileSHP.empty())
        {
            if (!OGRShapeDataSource::CopyInPlace(VSI_SHP_GetVSIL(m_hSHP->fpSHP),
                                                 oTempFileSHP))
            {
                CPLError(
                    CE_Failure, CPLE_FileIO,
                    "An error occurred while copying the content of %s on top "
                    "of %s. "
                    "The non corrupted version is in the _packed.dbf, "
                    "_packed.shp and _packed.shx files that you should rename "
                    "on top of the main ones.",
                    oTempFileSHP.c_str(), VSI_SHP_GetFilename(m_hSHP->fpSHP));
                free(panRecOffsetNew);
                free(panRecSizeNew);

                if (m_hDBF != nullptr)
                {
                    DBFClose(m_hDBF);
                    m_hDBF = nullptr;
                }
                SHPClose(m_hSHP);
                m_hSHP = nullptr;

                return OGRERR_FAILURE;
            }
            if (!OGRShapeDataSource::CopyInPlace(VSI_SHP_GetVSIL(m_hSHP->fpSHX),
                                                 oTempFileSHX))
            {
                CPLError(
                    CE_Failure, CPLE_FileIO,
                    "An error occurred while copying the content of %s on top "
                    "of %s. "
                    "The non corrupted version is in the _packed.dbf, "
                    "_packed.shp and _packed.shx files that you should rename "
                    "on top of the main ones.",
                    oTempFileSHX.c_str(), VSI_SHP_GetFilename(m_hSHP->fpSHX));
                free(panRecOffsetNew);
                free(panRecSizeNew);

                if (m_hDBF != nullptr)
                {
                    DBFClose(m_hDBF);
                    m_hDBF = nullptr;
                }
                SHPClose(m_hSHP);
                m_hSHP = nullptr;

                return OGRERR_FAILURE;
            }

            // Refresh current handle
            m_hSHP->nRecords = sSHPInfo.nRecords;
            m_hSHP->nMaxRecords = sSHPInfo.nMaxRecords;
            m_hSHP->nFileSize = sSHPInfo.nFileSize;
            CPLAssert(sizeof(sSHPInfo.adBoundsMin) == 4 * sizeof(double));
            memcpy(m_hSHP->adBoundsMin, sSHPInfo.adBoundsMin,
                   sizeof(sSHPInfo.adBoundsMin));
            memcpy(m_hSHP->adBoundsMax, sSHPInfo.adBoundsMax,
                   sizeof(sSHPInfo.adBoundsMax));
            free(m_hSHP->panRecOffset);
            free(m_hSHP->panRecSize);
            m_hSHP->panRecOffset = panRecOffsetNew;
            m_hSHP->panRecSize = panRecSizeNew;
        }
        else
        {
            // The free() are not really necessary but CSA doesn't realize it
            free(panRecOffsetNew);
            free(panRecSizeNew);
        }

        // Now that everything is successful, we can delete the temp files
        if (!oTempFileDBF.empty())
        {
            ForceDeleteFile(oTempFileDBF);
        }
        if (!oTempFileSHP.empty())
        {
            ForceDeleteFile(oTempFileSHP);
            ForceDeleteFile(oTempFileSHX);
        }
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Cleanup the old .dbf, .shp, .shx and rename the new ones. */
        /* --------------------------------------------------------------------
         */
        if (!oTempFileDBF.empty())
        {
            DBFClose(m_hDBF);
            m_hDBF = nullptr;

            if (VSIUnlink(osDBFName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to delete old DBF file: %s",
                         VSIStrerror(errno));

                m_hDBF =
                    m_poDS->DS_DBFOpen(osDBFName, m_bUpdateAccess ? "r+" : "r");

                VSIUnlink(oTempFileDBF);

                return OGRERR_FAILURE;
            }

            if (VSIRename(oTempFileDBF, osDBFName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Can not rename new DBF file: %s", VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            CheckFileDeletion(oTempFileDBF);
        }

        if (!oTempFileSHP.empty())
        {
            SHPClose(m_hSHP);
            m_hSHP = nullptr;

            if (VSIUnlink(osSHPName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Can not delete old SHP file: %s", VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            if (VSIUnlink(osSHXName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Can not delete old SHX file: %s", VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            if (VSIRename(oTempFileSHP, osSHPName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Can not rename new SHP file: %s", VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            if (VSIRename(oTempFileSHX, osSHXName) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Can not rename new SHX file: %s", VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            CheckFileDeletion(oTempFileSHP);
            CheckFileDeletion(oTempFileSHX);
        }

        /* --------------------------------------------------------------------
         */
        /*      Reopen the shapefile */
        /*                                                                      */
        /* We do not need to reimplement OGRShapeDataSource::OpenFile() here */
        /* with the fully featured error checking. */
        /* If all operations above succeeded, then all necessary files are */
        /* in the right place and accessible. */
        /* --------------------------------------------------------------------
         */

        const char *const pszAccess = m_bUpdateAccess ? "r+" : "r";

        if (bMustReopenSHP)
            m_hSHP = m_poDS->DS_SHPOpen(osSHPName, pszAccess);
        if (bMustReopenDBF)
            m_hDBF = m_poDS->DS_DBFOpen(osDBFName, pszAccess);

        if ((bMustReopenSHP && nullptr == m_hSHP) ||
            (bMustReopenDBF && nullptr == m_hDBF))
            return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Update total shape count.                                       */
    /* -------------------------------------------------------------------- */
    if (m_hDBF != nullptr)
        m_nTotalShapeCount = m_hDBF->nRecords;
    m_bSHPNeedsRepack = false;
    m_eNeedRepack = NO;

    return OGRERR_NONE;
}

/************************************************************************/
/*                               ResizeDBF()                            */
/*                                                                      */
/*      Autoshrink columns of the DBF file to their minimum             */
/*      size, according to the existing data.                           */
/************************************************************************/

OGRErr OGRShapeLayer::ResizeDBF()

{
    if (!StartUpdate("ResizeDBF"))
        return OGRERR_FAILURE;

    if (m_hDBF == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Attempt to RESIZE a shapefile with no .dbf file not supported.");
        return OGRERR_FAILURE;
    }

    /* Look which columns must be examined */
    int *panColMap = static_cast<int *>(
        CPLMalloc(m_poFeatureDefn->GetFieldCount() * sizeof(int)));
    int *panBestWidth = static_cast<int *>(
        CPLMalloc(m_poFeatureDefn->GetFieldCount() * sizeof(int)));
    int nStringCols = 0;
    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        if (m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTString ||
            m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTInteger ||
            m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTInteger64)
        {
            panColMap[nStringCols] = i;
            panBestWidth[nStringCols] = 1;
            nStringCols++;
        }
    }

    if (nStringCols == 0)
    {
        // Nothing to do.
        CPLFree(panColMap);
        CPLFree(panBestWidth);
        return OGRERR_NONE;
    }

    CPLDebug("SHAPE", "Computing optimal column size...");

    bool bAlreadyWarned = false;
    for (int i = 0; i < m_hDBF->nRecords; i++)
    {
        if (!DBFIsRecordDeleted(m_hDBF, i))
        {
            for (int j = 0; j < nStringCols; j++)
            {
                if (DBFIsAttributeNULL(m_hDBF, i, panColMap[j]))
                    continue;

                const char *pszVal =
                    DBFReadStringAttribute(m_hDBF, i, panColMap[j]);
                const int nLen = static_cast<int>(strlen(pszVal));
                if (nLen > panBestWidth[j])
                    panBestWidth[j] = nLen;
            }
        }
        else if (!bAlreadyWarned)
        {
            bAlreadyWarned = true;
            CPLDebug(
                "SHAPE",
                "DBF file would also need a REPACK due to deleted records");
        }
    }

    for (int j = 0; j < nStringCols; j++)
    {
        const int iField = panColMap[j];
        OGRFieldDefn *const poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);

        const char chNativeType = DBFGetNativeFieldType(m_hDBF, iField);
        char szFieldName[XBASE_FLDNAME_LEN_READ + 1] = {};
        int nOriWidth = 0;
        int nPrecision = 0;
        DBFGetFieldInfo(m_hDBF, iField, szFieldName, &nOriWidth, &nPrecision);

        if (panBestWidth[j] < nOriWidth)
        {
            CPLDebug("SHAPE",
                     "Shrinking field %d (%s) from %d to %d characters", iField,
                     poFieldDefn->GetNameRef(), nOriWidth, panBestWidth[j]);

            if (!DBFAlterFieldDefn(m_hDBF, iField, szFieldName, chNativeType,
                                   panBestWidth[j], nPrecision))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Shrinking field %d (%s) from %d to %d characters failed",
                    iField, poFieldDefn->GetNameRef(), nOriWidth,
                    panBestWidth[j]);

                CPLFree(panColMap);
                CPLFree(panBestWidth);

                return OGRERR_FAILURE;
            }
            else
            {
                whileUnsealing(poFieldDefn)->SetWidth(panBestWidth[j]);
            }
        }
    }

    TruncateDBF();

    CPLFree(panColMap);
    CPLFree(panBestWidth);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          TruncateDBF()                               */
/************************************************************************/

void OGRShapeLayer::TruncateDBF()
{
    if (m_hDBF == nullptr)
        return;

    m_hDBF->sHooks.FSeek(m_hDBF->fp, 0, SEEK_END);
    vsi_l_offset nOldSize = m_hDBF->sHooks.FTell(m_hDBF->fp);
    vsi_l_offset nNewSize =
        m_hDBF->nRecordLength * static_cast<SAOffset>(m_hDBF->nRecords) +
        m_hDBF->nHeaderLength;
    if (m_hDBF->bWriteEndOfFileChar)
        nNewSize++;
    if (nNewSize < nOldSize)
    {
        CPLDebug("SHAPE",
                 "Truncating DBF file from " CPL_FRMT_GUIB " to " CPL_FRMT_GUIB
                 " bytes",
                 nOldSize, nNewSize);
        VSIFTruncateL(VSI_SHP_GetVSIL(m_hDBF->fp), nNewSize);
    }
    m_hDBF->sHooks.FSeek(m_hDBF->fp, 0, SEEK_SET);
}

/************************************************************************/
/*                        RecomputeExtent()                             */
/*                                                                      */
/*      Force recomputation of the extent of the .SHP file              */
/************************************************************************/

OGRErr OGRShapeLayer::RecomputeExtent()
{
    if (!StartUpdate("RecomputeExtent"))
        return OGRERR_FAILURE;

    if (m_hSHP == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The RECOMPUTE EXTENT operation is not permitted on a layer "
                 "without .SHP file.");
        return OGRERR_FAILURE;
    }

    double adBoundsMin[4] = {0.0, 0.0, 0.0, 0.0};
    double adBoundsMax[4] = {0.0, 0.0, 0.0, 0.0};

    bool bHasBeenInit = false;

    for (int iShape = 0; iShape < m_nTotalShapeCount; iShape++)
    {
        if (m_hDBF == nullptr || !DBFIsRecordDeleted(m_hDBF, iShape))
        {
            SHPObject *psObject = SHPReadObject(m_hSHP, iShape);
            if (psObject != nullptr && psObject->nSHPType != SHPT_NULL &&
                psObject->nVertices != 0)
            {
                if (!bHasBeenInit)
                {
                    bHasBeenInit = true;
                    adBoundsMin[0] = psObject->padfX[0];
                    adBoundsMax[0] = psObject->padfX[0];
                    adBoundsMin[1] = psObject->padfY[0];
                    adBoundsMax[1] = psObject->padfY[0];
                    if (psObject->padfZ)
                    {
                        adBoundsMin[2] = psObject->padfZ[0];
                        adBoundsMax[2] = psObject->padfZ[0];
                    }
                    if (psObject->padfM)
                    {
                        adBoundsMin[3] = psObject->padfM[0];
                        adBoundsMax[3] = psObject->padfM[0];
                    }
                }

                for (int i = 0; i < psObject->nVertices; i++)
                {
                    adBoundsMin[0] =
                        std::min(adBoundsMin[0], psObject->padfX[i]);
                    adBoundsMin[1] =
                        std::min(adBoundsMin[1], psObject->padfY[i]);
                    adBoundsMax[0] =
                        std::max(adBoundsMax[0], psObject->padfX[i]);
                    adBoundsMax[1] =
                        std::max(adBoundsMax[1], psObject->padfY[i]);
                    if (psObject->padfZ)
                    {
                        adBoundsMin[2] =
                            std::min(adBoundsMin[2], psObject->padfZ[i]);
                        adBoundsMax[2] =
                            std::max(adBoundsMax[2], psObject->padfZ[i]);
                    }
                    if (psObject->padfM)
                    {
                        adBoundsMax[3] =
                            std::max(adBoundsMax[3], psObject->padfM[i]);
                        adBoundsMin[3] =
                            std::min(adBoundsMin[3], psObject->padfM[i]);
                    }
                }
            }
            SHPDestroyObject(psObject);
        }
    }

    if (memcmp(m_hSHP->adBoundsMin, adBoundsMin, 4 * sizeof(double)) != 0 ||
        memcmp(m_hSHP->adBoundsMax, adBoundsMax, 4 * sizeof(double)) != 0)
    {
        m_bHeaderDirty = true;
        m_hSHP->bUpdated = TRUE;
        memcpy(m_hSHP->adBoundsMin, adBoundsMin, 4 * sizeof(double));
        memcpy(m_hSHP->adBoundsMax, adBoundsMax, 4 * sizeof(double));
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                              TouchLayer()                            */
/************************************************************************/

bool OGRShapeLayer::TouchLayer()
{
    m_poDS->SetLastUsedLayer(this);

    if (m_eFileDescriptorsState == FD_OPENED)
        return true;
    if (m_eFileDescriptorsState == FD_CANNOT_REOPEN)
        return false;

    return ReopenFileDescriptors();
}

/************************************************************************/
/*                        ReopenFileDescriptors()                       */
/************************************************************************/

bool OGRShapeLayer::ReopenFileDescriptors()
{
    CPLDebug("SHAPE", "ReopenFileDescriptors(%s)", m_osFullName.c_str());

    const bool bRealUpdateAccess =
        m_bUpdateAccess &&
        (!m_poDS->IsZip() || !m_poDS->GetTemporaryUnzipDir().empty());

    if (m_bHSHPWasNonNULL)
    {
        m_hSHP = m_poDS->DS_SHPOpen(m_osFullName.c_str(),
                                    bRealUpdateAccess ? "r+" : "r");

        if (m_hSHP == nullptr)
        {
            m_eFileDescriptorsState = FD_CANNOT_REOPEN;
            return false;
        }
    }

    if (m_bHDBFWasNonNULL)
    {
        m_hDBF = m_poDS->DS_DBFOpen(m_osFullName.c_str(),
                                    bRealUpdateAccess ? "r+" : "r");

        if (m_hDBF == nullptr)
        {
            CPLError(
                CE_Failure, CPLE_OpenFailed, "Cannot reopen %s",
                CPLResetExtensionSafe(m_osFullName.c_str(), "dbf").c_str());
            m_eFileDescriptorsState = FD_CANNOT_REOPEN;
            return false;
        }
    }

    m_eFileDescriptorsState = FD_OPENED;

    return true;
}

/************************************************************************/
/*                        CloseUnderlyingLayer()                        */
/************************************************************************/

void OGRShapeLayer::CloseUnderlyingLayer()
{
    CPLDebug("SHAPE", "CloseUnderlyingLayer(%s)", m_osFullName.c_str());

    if (m_hDBF != nullptr)
        DBFClose(m_hDBF);
    m_hDBF = nullptr;

    if (m_hSHP != nullptr)
        SHPClose(m_hSHP);
    m_hSHP = nullptr;

    // We close QIX and reset the check flag, so that CheckForQIX()
    // will retry opening it if necessary when the layer is active again.
    if (m_hQIX != nullptr)
        SHPCloseDiskTree(m_hQIX);
    m_hQIX = nullptr;
    m_bCheckedForQIX = false;

    if (m_hSBN != nullptr)
        SBNCloseDiskTree(m_hSBN);
    m_hSBN = nullptr;
    m_bCheckedForSBN = false;

    m_eFileDescriptorsState = FD_CLOSED;
}

/************************************************************************/
/*                            AddToFileList()                           */
/************************************************************************/

void OGRShapeLayer::AddToFileList(CPLStringList &oFileList)
{
    if (!TouchLayer())
        return;

    if (m_hSHP)
    {
        const char *pszSHPFilename = VSI_SHP_GetFilename(m_hSHP->fpSHP);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(pszSHPFilename));
        const std::string osSHPExt = CPLGetExtensionSafe(pszSHPFilename);
        const std::string osSHXFilename = CPLResetExtensionSafe(
            pszSHPFilename, (osSHPExt[0] == 's') ? "shx" : "SHX");
        oFileList.AddStringDirectly(
            VSIGetCanonicalFilename(osSHXFilename.c_str()));
    }

    if (m_hDBF)
    {
        const char *pszDBFFilename = VSI_SHP_GetFilename(m_hDBF->fp);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(pszDBFFilename));
        if (m_hDBF->pszCodePage != nullptr && m_hDBF->iLanguageDriver == 0)
        {
            const std::string osDBFExt = CPLGetExtensionSafe(pszDBFFilename);
            const std::string osCPGFilename = CPLResetExtensionSafe(
                pszDBFFilename, (osDBFExt[0] == 'd') ? "cpg" : "CPG");
            oFileList.AddStringDirectly(
                VSIGetCanonicalFilename(osCPGFilename.c_str()));
        }
    }

    if (m_hSHP)
    {
        if (GetSpatialRef() != nullptr)
        {
            OGRShapeGeomFieldDefn *poGeomFieldDefn =
                cpl::down_cast<OGRShapeGeomFieldDefn *>(
                    GetLayerDefn()->GetGeomFieldDefn(0));
            oFileList.AddStringDirectly(
                VSIGetCanonicalFilename(poGeomFieldDefn->GetPrjFilename()));
        }
        if (CheckForQIX())
        {
            const std::string osQIXFilename =
                CPLResetExtensionSafe(m_osFullName.c_str(), "qix");
            oFileList.AddStringDirectly(
                VSIGetCanonicalFilename(osQIXFilename.c_str()));
        }
        else if (CheckForSBN())
        {
            const std::string osSBNFilename =
                CPLResetExtensionSafe(m_osFullName.c_str(), "sbn");
            oFileList.AddStringDirectly(
                VSIGetCanonicalFilename(osSBNFilename.c_str()));
            const std::string osSBXFilename =
                CPLResetExtensionSafe(m_osFullName.c_str(), "sbx");
            oFileList.AddStringDirectly(
                VSIGetCanonicalFilename(osSBXFilename.c_str()));
        }
    }
}

/************************************************************************/
/*                   UpdateFollowingDeOrRecompression()                 */
/************************************************************************/

void OGRShapeLayer::UpdateFollowingDeOrRecompression()
{
    CPLAssert(m_poDS->IsZip());
    CPLString osDSDir = m_poDS->GetTemporaryUnzipDir();
    if (osDSDir.empty())
        osDSDir = m_poDS->GetVSIZipPrefixeDir();

    if (GetSpatialRef() != nullptr)
    {
        OGRShapeGeomFieldDefn *poGeomFieldDefn =
            cpl::down_cast<OGRShapeGeomFieldDefn *>(
                GetLayerDefn()->GetGeomFieldDefn(0));
        poGeomFieldDefn->SetPrjFilename(
            CPLFormFilenameSafe(
                osDSDir.c_str(),
                CPLGetFilename(poGeomFieldDefn->GetPrjFilename().c_str()),
                nullptr)
                .c_str());
    }

    m_osFullName = CPLFormFilenameSafe(
        osDSDir, CPLGetFilename(m_osFullName.c_str()), nullptr);
    CloseUnderlyingLayer();
}

/************************************************************************/
/*                           Rename()                                   */
/************************************************************************/

OGRErr OGRShapeLayer::Rename(const char *pszNewName)
{
    if (!TestCapability(OLCRename))
        return OGRERR_FAILURE;

    if (m_poDS->GetLayerByName(pszNewName) != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer %s already exists",
                 pszNewName);
        return OGRERR_FAILURE;
    }

    if (!m_poDS->UncompressIfNeeded())
        return OGRERR_FAILURE;

    CPLStringList oFileList;
    AddToFileList(oFileList);

    const std::string osDirname = CPLGetPathSafe(m_osFullName.c_str());
    for (int i = 0; i < oFileList.size(); ++i)
    {
        const std::string osRenamedFile =
            CPLFormFilenameSafe(osDirname.c_str(), pszNewName,
                                CPLGetExtensionSafe(oFileList[i]).c_str());
        VSIStatBufL sStat;
        if (VSIStatL(osRenamedFile.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "File %s already exists",
                     osRenamedFile.c_str());
            return OGRERR_FAILURE;
        }
    }

    CloseUnderlyingLayer();

    for (int i = 0; i < oFileList.size(); ++i)
    {
        const std::string osRenamedFile =
            CPLFormFilenameSafe(osDirname.c_str(), pszNewName,
                                CPLGetExtensionSafe(oFileList[i]).c_str());
        if (VSIRename(oFileList[i], osRenamedFile.c_str()) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot rename %s to %s",
                     oFileList[i], osRenamedFile.c_str());
            return OGRERR_FAILURE;
        }
    }

    if (GetSpatialRef() != nullptr)
    {
        OGRShapeGeomFieldDefn *poGeomFieldDefn =
            cpl::down_cast<OGRShapeGeomFieldDefn *>(
                GetLayerDefn()->GetGeomFieldDefn(0));
        poGeomFieldDefn->SetPrjFilename(
            CPLFormFilenameSafe(
                osDirname.c_str(), pszNewName,
                CPLGetExtensionSafe(poGeomFieldDefn->GetPrjFilename().c_str())
                    .c_str())
                .c_str());
    }

    m_osFullName =
        CPLFormFilenameSafe(osDirname.c_str(), pszNewName,
                            CPLGetExtensionSafe(m_osFullName.c_str()).c_str());

    if (!ReopenFileDescriptors())
        return OGRERR_FAILURE;

    SetDescription(pszNewName);
    whileUnsealing(m_poFeatureDefn)->SetName(pszNewName);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetDataset()                                */
/************************************************************************/

GDALDataset *OGRShapeLayer::GetDataset()
{
    return m_poDS;
}

/************************************************************************/
/*                        GetNextArrowArray()                           */
/************************************************************************/

// Specialized implementation restricted to situations where only retrieving
// of FID values is asked (without filters)
// In other cases, fall back to generic implementation.
int OGRShapeLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                     struct ArrowArray *out_array)
{
    m_bLastGetNextArrowArrayUsedOptimizedCodePath = false;
    if (!TouchLayer())
    {
        memset(out_array, 0, sizeof(*out_array));
        return EIO;
    }

    if (!m_hDBF || m_poAttrQuery != nullptr || m_poFilterGeom != nullptr)
    {
        return OGRLayer::GetNextArrowArray(stream, out_array);
    }

    // If any field is not ignored, use generic implementation
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; ++i)
    {
        if (!m_poFeatureDefn->GetFieldDefn(i)->IsIgnored())
            return OGRLayer::GetNextArrowArray(stream, out_array);
    }
    if (GetGeomType() != wkbNone &&
        !m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored())
        return OGRLayer::GetNextArrowArray(stream, out_array);

    OGRArrowArrayHelper sHelper(m_poDS, m_poFeatureDefn,
                                m_aosArrowArrayStreamOptions, out_array);
    if (out_array->release == nullptr)
    {
        return ENOMEM;
    }

    if (!sHelper.m_bIncludeFID)
    {
        out_array->release(out_array);
        return OGRLayer::GetNextArrowArray(stream, out_array);
    }

    m_bLastGetNextArrowArrayUsedOptimizedCodePath = true;
    int nCount = 0;
    while (m_iNextShapeId < m_nTotalShapeCount)
    {
        const bool bIsDeleted =
            CPL_TO_BOOL(DBFIsRecordDeleted(m_hDBF, m_iNextShapeId));
        if (bIsDeleted)
        {
            ++m_iNextShapeId;
            continue;
        }
        if (VSIFEofL(VSI_SHP_GetVSIL(m_hDBF->fp)) ||
            VSIFErrorL(VSI_SHP_GetVSIL(m_hDBF->fp)))
        {
            out_array->release(out_array);
            memset(out_array, 0, sizeof(*out_array));
            return EIO;
        }
        sHelper.m_panFIDValues[nCount] = m_iNextShapeId;
        ++m_iNextShapeId;
        ++nCount;
        if (nCount == sHelper.m_nMaxBatchSize)
            break;
    }
    sHelper.Shrink(nCount);
    if (nCount == 0)
    {
        out_array->release(out_array);
        memset(out_array, 0, sizeof(*out_array));
    }
    return 0;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char *OGRShapeLayer::GetMetadataItem(const char *pszName,
                                           const char *pszDomain)
{
    if (pszName && pszDomain && EQUAL(pszDomain, "__DEBUG__") &&
        EQUAL(pszName, "LAST_GET_NEXT_ARROW_ARRAY_USED_OPTIMIZED_CODE_PATH"))
    {
        return m_bLastGetNextArrowArrayUsedOptimizedCodePath ? "YES" : "NO";
    }
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}
