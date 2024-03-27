/******************************************************************************
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXLayer class.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
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

#include "cpl_port.h"
#include "ogr_gpx.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#ifdef HAVE_EXPAT
#include "expat.h"
#endif
#include "ogr_core.h"
#include "ogr_expat.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

constexpr int FLD_TRACK_FID = 0;
constexpr int FLD_TRACK_SEG_ID = 1;
#ifdef HAVE_EXPAT
constexpr int FLD_TRACK_PT_ID = 2;
#endif
constexpr int FLD_TRACK_NAME = 3;

constexpr int FLD_ROUTE_FID = 0;
#ifdef HAVE_EXPAT
constexpr int FLD_ROUTE_PT_ID = 1;
#endif
constexpr int FLD_ROUTE_NAME = 2;

/************************************************************************/
/*                            OGRGPXLayer()                             */
/*                                                                      */
/*      Note that the OGRGPXLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRGPXLayer::OGRGPXLayer(const char *pszFilename, const char *pszLayerName,
                         GPXGeometryType gpxGeomTypeIn,
                         OGRGPXDataSource *poDSIn, bool bWriteModeIn,
                         CSLConstList papszOpenOptions)
    : m_poDS(poDSIn), m_gpxGeomType(gpxGeomTypeIn), m_bWriteMode(bWriteModeIn)
{
#ifdef HAVE_EXPAT
    const char *gpxVersion = m_poDS->GetVersion();
#endif

    m_nMaxLinks =
        atoi(CSLFetchNameValueDef(papszOpenOptions, "N_MAX_LINKS",
                                  CPLGetConfigOption("GPX_N_MAX_LINKS", "2")));
    if (m_nMaxLinks < 0)
        m_nMaxLinks = 2;
    if (m_nMaxLinks > 100)
        m_nMaxLinks = 100;

    m_bEleAs25D = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptions, "ELE_AS_25D",
                             CPLGetConfigOption("GPX_ELE_AS_25D", "NO")));

    const bool bShortNames = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptions, "SHORT_NAMES",
                             CPLGetConfigOption("GPX_SHORT_NAMES", "NO")));

    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->Reference();

    if (m_gpxGeomType == GPX_TRACK_POINT)
    {
        /* Don't move this code. This fields must be number 0, 1 and 2 */
        /* in order to make OGRGPXLayer::startElementCbk work */
        OGRFieldDefn oFieldTrackFID("track_fid", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldTrackFID);

        OGRFieldDefn oFieldTrackSegID(
            (bShortNames) ? "trksegid" : "track_seg_id", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldTrackSegID);

        OGRFieldDefn oFieldTrackSegPointID(
            (bShortNames) ? "trksegptid" : "track_seg_point_id", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldTrackSegPointID);

        if (m_bWriteMode)
        {
            OGRFieldDefn oFieldName("track_name", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldName);
        }
    }
    else if (m_gpxGeomType == GPX_ROUTE_POINT)
    {
        /* Don't move this code. See above */
        OGRFieldDefn oFieldRouteFID("route_fid", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldRouteFID);

        OGRFieldDefn oFieldRoutePointID(
            (bShortNames) ? "rteptid" : "route_point_id", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldRoutePointID);

        if (m_bWriteMode)
        {
            OGRFieldDefn oFieldName("route_name", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldName);
        }
    }

    m_iFirstGPXField = m_poFeatureDefn->GetFieldCount();

    if (m_gpxGeomType == GPX_WPT || m_gpxGeomType == GPX_TRACK_POINT ||
        m_gpxGeomType == GPX_ROUTE_POINT)
    {
        m_poFeatureDefn->SetGeomType((m_bEleAs25D) ? wkbPoint25D : wkbPoint);
        /* Position info */

        OGRFieldDefn oFieldEle("ele", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldEle);

        OGRFieldDefn oFieldTime("time", OFTDateTime);
        m_poFeatureDefn->AddFieldDefn(&oFieldTime);

#ifdef HAVE_EXPAT
        if (m_gpxGeomType == GPX_TRACK_POINT && strcmp(gpxVersion, "1.0") == 0)
        {
            OGRFieldDefn oFieldCourse("course", OFTReal);
            m_poFeatureDefn->AddFieldDefn(&oFieldCourse);

            OGRFieldDefn oFieldSpeed("speed", OFTReal);
            m_poFeatureDefn->AddFieldDefn(&oFieldSpeed);
        }
#endif

        OGRFieldDefn oFieldMagVar("magvar", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldMagVar);

        OGRFieldDefn oFieldGeoidHeight("geoidheight", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldGeoidHeight);

        /* Description info */

        OGRFieldDefn oFieldName("name", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldName);

        OGRFieldDefn oFieldCmt("cmt", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldCmt);

        OGRFieldDefn oFieldDesc("desc", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDesc);

        OGRFieldDefn oFieldSrc("src", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldSrc);

#ifdef HAVE_EXPAT
        if (strcmp(gpxVersion, "1.0") == 0)
        {
            OGRFieldDefn oFieldUrl("url", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldUrl);

            OGRFieldDefn oFieldUrlName("urlname", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldUrlName);
        }
        else
#endif
        {
            for (int i = 1; i <= m_nMaxLinks; i++)
            {
                char szFieldName[32];
                snprintf(szFieldName, sizeof(szFieldName), "link%d_href", i);
                OGRFieldDefn oFieldLinkHref(szFieldName, OFTString);
                m_poFeatureDefn->AddFieldDefn(&oFieldLinkHref);

                snprintf(szFieldName, sizeof(szFieldName), "link%d_text", i);
                OGRFieldDefn oFieldLinkText(szFieldName, OFTString);
                m_poFeatureDefn->AddFieldDefn(&oFieldLinkText);

                snprintf(szFieldName, sizeof(szFieldName), "link%d_type", i);
                OGRFieldDefn oFieldLinkType(szFieldName, OFTString);
                m_poFeatureDefn->AddFieldDefn(&oFieldLinkType);
            }
        }

        OGRFieldDefn oFieldSym("sym", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldSym);

        OGRFieldDefn oFieldType("type", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldType);

        /* Accuracy info */

        OGRFieldDefn oFieldFix("fix", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldFix);

        OGRFieldDefn oFieldSat("sat", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldSat);

        OGRFieldDefn oFieldHdop("hdop", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldHdop);

        OGRFieldDefn oFieldVdop("vdop", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldVdop);

        OGRFieldDefn oFieldPdop("pdop", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldPdop);

        OGRFieldDefn oFieldAgeofgpsdata("ageofdgpsdata", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldAgeofgpsdata);

        OGRFieldDefn oFieldDgpsid("dgpsid", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldDgpsid);
    }
    else
    {
        if (m_gpxGeomType == GPX_TRACK)
            m_poFeatureDefn->SetGeomType((m_bEleAs25D) ? wkbMultiLineString25D
                                                       : wkbMultiLineString);
        else
            m_poFeatureDefn->SetGeomType((m_bEleAs25D) ? wkbLineString25D
                                                       : wkbLineString);

        OGRFieldDefn oFieldName("name", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldName);

        OGRFieldDefn oFieldCmt("cmt", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldCmt);

        OGRFieldDefn oFieldDesc("desc", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDesc);

        OGRFieldDefn oFieldSrc("src", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldSrc);

        for (int i = 1; i <= m_nMaxLinks; i++)
        {
            char szFieldName[32];
            snprintf(szFieldName, sizeof(szFieldName), "link%d_href", i);
            OGRFieldDefn oFieldLinkHref(szFieldName, OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldLinkHref);

            snprintf(szFieldName, sizeof(szFieldName), "link%d_text", i);
            OGRFieldDefn oFieldLinkText(szFieldName, OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldLinkText);

            snprintf(szFieldName, sizeof(szFieldName), "link%d_type", i);
            OGRFieldDefn oFieldLinkType(szFieldName, OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldLinkType);
        }

        OGRFieldDefn oFieldNumber("number", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldNumber);

        OGRFieldDefn oFieldType("type", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldType);
    }

    /* Number of 'standard' GPX attributes */
    m_nGPXFields = m_poFeatureDefn->GetFieldCount();

    m_poSRS = new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG);
    m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if (m_poFeatureDefn->GetGeomFieldCount() != 0)
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);

    if (!m_bWriteMode)
    {
        m_fpGPX.reset(VSIFOpenL(pszFilename, "r"));
        if (m_fpGPX == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                     pszFilename);
            return;
        }

        if (m_poDS->GetUseExtensions() ||
            CPLTestBool(CPLGetConfigOption("GPX_USE_EXTENSIONS", "FALSE")))
        {
            LoadExtensionsSchema();
        }
    }

    OGRGPXLayer::ResetReading();
}

/************************************************************************/
/*                            ~OGRGPXLayer()                            */
/************************************************************************/

OGRGPXLayer::~OGRGPXLayer()

{
#ifdef HAVE_EXPAT
    if (m_oParser)
        XML_ParserFree(m_oParser);
#endif
    m_poFeatureDefn->Release();

    if (m_poSRS != nullptr)
        m_poSRS->Release();
}

#ifdef HAVE_EXPAT

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    static_cast<OGRGPXLayer *>(pUserData)->startElementCbk(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    static_cast<OGRGPXLayer *>(pUserData)->endElementCbk(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    static_cast<OGRGPXLayer *>(pUserData)->dataHandlerCbk(data, nLen);
}

#endif

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGPXLayer::ResetReading()

{
    m_nNextFID = 0;
    if (m_fpGPX)
    {
        m_fpGPX->Seek(0, SEEK_SET);
#ifdef HAVE_EXPAT
        if (m_oParser)
            XML_ParserFree(m_oParser);

        m_oParser = OGRCreateExpatXMLParser();
        XML_SetElementHandler(m_oParser, ::startElementCbk, ::endElementCbk);
        XML_SetCharacterDataHandler(m_oParser, ::dataHandlerCbk);
        XML_SetUserData(m_oParser, this);
#endif
    }
    m_hasFoundLat = false;
    m_hasFoundLon = false;
    m_inInterestingElement = false;
    m_osSubElementName.clear();
    m_osSubElementValue.clear();

    m_poFeature.reset();
    m_oFeatureQueue.clear();
    m_multiLineString.reset();
    m_lineString.reset();

    m_depthLevel = 0;
    m_interestingDepthLevel = 0;

    m_trkFID = 0;
    m_trkSegId = 0;
    m_trkSegPtId = 0;
    m_rteFID = 0;
    m_rtePtId = 0;
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                        startElementCbk()                             */
/************************************************************************/

/** Replace ':' from XML NS element name by '_' more OGR friendly */
static char *OGRGPX_GetOGRCompatibleTagName(const char *pszName)
{
    char *pszModName = CPLStrdup(pszName);
    for (int i = 0; pszModName[i] != 0; i++)
    {
        if (pszModName[i] == ':')
            pszModName[i] = '_';
    }
    return pszModName;
}

void OGRGPXLayer::AddStrToSubElementValue(const char *pszStr)
{
    try
    {
        m_osSubElementValue.append(pszStr);
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory when parsing GPX file");
        XML_StopParser(m_oParser, XML_FALSE);
        m_bStopParsing = true;
    }
}

void OGRGPXLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    if (m_bStopParsing)
        return;

    m_nWithoutEventCounter = 0;

    if ((m_gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0) ||
        (m_gpxGeomType == GPX_ROUTE_POINT && strcmp(pszName, "rtept") == 0) ||
        (m_gpxGeomType == GPX_TRACK_POINT && strcmp(pszName, "trkpt") == 0))
    {
        m_interestingDepthLevel = m_depthLevel;

        m_poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        m_inInterestingElement = true;
        m_hasFoundLat = false;
        m_hasFoundLon = false;
        m_inExtensions = false;
        m_inLink = false;
        m_iCountLink = 0;

        for (int i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "lat") == 0 && ppszAttr[i + 1][0])
            {
                m_hasFoundLat = true;
                m_latVal = CPLAtof(ppszAttr[i + 1]);
            }
            else if (strcmp(ppszAttr[i], "lon") == 0 && ppszAttr[i + 1][0])
            {
                m_hasFoundLon = true;
                m_lonVal = CPLAtof(ppszAttr[i + 1]);
            }
        }

        m_poFeature->SetFID(m_nNextFID++);

        if (m_hasFoundLat && m_hasFoundLon)
        {
            m_poFeature->SetGeometryDirectly(new OGRPoint(m_lonVal, m_latVal));
        }
        else
        {
            CPLDebug("GPX",
                     "Skipping %s (FID=" CPL_FRMT_GIB
                     ") without lat and/or lon",
                     pszName, m_nNextFID);
        }

        if (m_gpxGeomType == GPX_ROUTE_POINT)
        {
            m_rtePtId++;
            m_poFeature->SetField(FLD_ROUTE_FID, m_rteFID - 1);
            m_poFeature->SetField(FLD_ROUTE_PT_ID, m_rtePtId - 1);
        }
        else if (m_gpxGeomType == GPX_TRACK_POINT)
        {
            m_trkSegPtId++;

            m_poFeature->SetField(FLD_TRACK_FID, m_trkFID - 1);
            m_poFeature->SetField(FLD_TRACK_SEG_ID, m_trkSegId - 1);
            m_poFeature->SetField(FLD_TRACK_PT_ID, m_trkSegPtId - 1);
        }
    }
    else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
    {
        m_interestingDepthLevel = m_depthLevel;

        m_inExtensions = false;
        m_inLink = false;
        m_iCountLink = 0;
        m_poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        m_inInterestingElement = true;

        m_multiLineString = std::make_unique<OGRMultiLineString>();
        m_lineString.reset();

        m_poFeature->SetFID(m_nNextFID++);
    }
    else if (m_gpxGeomType == GPX_TRACK_POINT && strcmp(pszName, "trk") == 0)
    {
        m_trkFID++;
        m_trkSegId = 0;
    }
    else if (m_gpxGeomType == GPX_TRACK_POINT && strcmp(pszName, "trkseg") == 0)
    {
        m_trkSegId++;
        m_trkSegPtId = 0;
    }
    else if (m_gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
    {
        m_interestingDepthLevel = m_depthLevel;

        m_poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_inLink = false;
        m_iCountLink = 0;

        m_lineString = std::make_unique<OGRLineString>();
        m_poFeature->SetFID(m_nNextFID++);
    }
    else if (m_gpxGeomType == GPX_ROUTE_POINT && strcmp(pszName, "rte") == 0)
    {
        m_rteFID++;
        m_rtePtId = 0;
    }
    else if (m_inInterestingElement)
    {
        if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trkseg") == 0 &&
            m_depthLevel == m_interestingDepthLevel + 1)
        {
            if (m_multiLineString)
            {
                m_lineString = std::make_unique<OGRLineString>();
            }
        }
        else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trkpt") == 0 &&
                 m_depthLevel == m_interestingDepthLevel + 2)
        {
            if (m_lineString)
            {
                m_hasFoundLat = false;
                m_hasFoundLon = false;
                for (int i = 0; ppszAttr[i]; i += 2)
                {
                    if (strcmp(ppszAttr[i], "lat") == 0 && ppszAttr[i + 1][0])
                    {
                        m_hasFoundLat = true;
                        m_latVal = CPLAtof(ppszAttr[i + 1]);
                    }
                    else if (strcmp(ppszAttr[i], "lon") == 0 &&
                             ppszAttr[i + 1][0])
                    {
                        m_hasFoundLon = true;
                        m_lonVal = CPLAtof(ppszAttr[i + 1]);
                    }
                }

                if (m_hasFoundLat && m_hasFoundLon)
                {
                    m_lineString->addPoint(m_lonVal, m_latVal);
                }
                else
                {
                    CPLDebug("GPX", "Skipping %s without lat and/or lon",
                             pszName);
                }
            }
        }
        else if (m_gpxGeomType == GPX_ROUTE && strcmp(pszName, "rtept") == 0 &&
                 m_depthLevel == m_interestingDepthLevel + 1)
        {
            if (m_lineString)
            {
                m_hasFoundLat = false;
                m_hasFoundLon = false;
                for (int i = 0; ppszAttr[i]; i += 2)
                {
                    if (strcmp(ppszAttr[i], "lat") == 0 && ppszAttr[i + 1][0])
                    {
                        m_hasFoundLat = true;
                        m_latVal = CPLAtof(ppszAttr[i + 1]);
                    }
                    else if (strcmp(ppszAttr[i], "lon") == 0 &&
                             ppszAttr[i + 1][0])
                    {
                        m_hasFoundLon = true;
                        m_lonVal = CPLAtof(ppszAttr[i + 1]);
                    }
                }

                if (m_hasFoundLat && m_hasFoundLon)
                {
                    m_lineString->addPoint(m_lonVal, m_latVal);
                }
                else
                {
                    CPLDebug("GPX", "Skipping %s without lat and/or lon",
                             pszName);
                }
            }
        }
        else if (m_bEleAs25D && strcmp(pszName, "ele") == 0 &&
                 m_lineString != nullptr &&
                 ((m_gpxGeomType == GPX_ROUTE &&
                   m_depthLevel == m_interestingDepthLevel + 2) ||
                  (m_gpxGeomType == GPX_TRACK &&
                   m_depthLevel == m_interestingDepthLevel + 3)))
        {
            m_osSubElementName = pszName;
        }
        else if (m_depthLevel == m_interestingDepthLevel + 1 &&
                 strcmp(pszName, "extensions") == 0)
        {
            if (m_poDS->GetUseExtensions())
            {
                m_inExtensions = true;
            }
        }
        else if (m_depthLevel == m_interestingDepthLevel + 1 ||
                 (m_inExtensions &&
                  m_depthLevel == m_interestingDepthLevel + 2))
        {
            m_osSubElementName.clear();
            m_iCurrentField = -1;

            if (strcmp(pszName, "link") == 0)
            {
                m_iCountLink++;
                if (m_iCountLink <= m_nMaxLinks)
                {
                    if (ppszAttr[0] && ppszAttr[1] &&
                        strcmp(ppszAttr[0], "href") == 0)
                    {
                        char szFieldName[32];
                        snprintf(szFieldName, sizeof(szFieldName),
                                 "link%d_href", m_iCountLink);
                        m_iCurrentField =
                            m_poFeatureDefn->GetFieldIndex(szFieldName);
                        m_poFeature->SetField(m_iCurrentField, ppszAttr[1]);
                    }
                }
                else
                {
                    static int once = 1;
                    if (once)
                    {
                        once = 0;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "GPX driver only reads %d links per element. "
                                 "Others will be ignored. "
                                 "This can be changed with the GPX_N_MAX_LINKS "
                                 "environment variable",
                                 m_nMaxLinks);
                    }
                }
                m_inLink = true;
                m_iCurrentField = -1;
            }
            else
            {
                for (int iField = 0; iField < m_poFeatureDefn->GetFieldCount();
                     iField++)
                {
                    bool bMatch = false;
                    if (iField >= m_nGPXFields)
                    {
                        char *pszCompatibleName =
                            OGRGPX_GetOGRCompatibleTagName(pszName);
                        bMatch = strcmp(m_poFeatureDefn->GetFieldDefn(iField)
                                            ->GetNameRef(),
                                        pszCompatibleName) == 0;
                        CPLFree(pszCompatibleName);
                    }
                    else
                    {
                        bMatch = strcmp(m_poFeatureDefn->GetFieldDefn(iField)
                                            ->GetNameRef(),
                                        pszName) == 0;
                    }

                    if (bMatch)
                    {
                        m_iCurrentField = iField;
                        m_osSubElementName = pszName;
                        break;
                    }
                }
            }
        }
        else if (m_depthLevel == m_interestingDepthLevel + 2 && m_inLink)
        {
            m_osSubElementName.clear();
            m_iCurrentField = -1;
            if (m_iCountLink <= m_nMaxLinks)
            {
                if (strcmp(pszName, "type") == 0)
                {
                    char szFieldName[32];
                    snprintf(szFieldName, sizeof(szFieldName), "link%d_type",
                             m_iCountLink);
                    m_iCurrentField =
                        m_poFeatureDefn->GetFieldIndex(szFieldName);
                    m_osSubElementName = pszName;
                }
                else if (strcmp(pszName, "text") == 0)
                {
                    char szFieldName[32];
                    snprintf(szFieldName, sizeof(szFieldName), "link%d_text",
                             m_iCountLink);
                    m_iCurrentField =
                        m_poFeatureDefn->GetFieldIndex(szFieldName);
                    m_osSubElementName = pszName;
                }
            }
        }
        else if (m_inExtensions && m_depthLevel > m_interestingDepthLevel + 2)
        {
            AddStrToSubElementValue((ppszAttr[0] == nullptr)
                                        ? CPLSPrintf("<%s>", pszName)
                                        : CPLSPrintf("<%s ", pszName));
            for (int i = 0; ppszAttr[i]; i += 2)
            {
                AddStrToSubElementValue(
                    CPLSPrintf("%s=\"%s\" ", ppszAttr[i], ppszAttr[i + 1]));
            }
            if (ppszAttr[0] != nullptr)
            {
                AddStrToSubElementValue(">");
            }
        }
    }

    m_depthLevel++;
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRGPXLayer::endElementCbk(const char *pszName)
{
    if (m_bStopParsing)
        return;

    m_nWithoutEventCounter = 0;

    m_depthLevel--;

    if (m_inInterestingElement)
    {
        if ((m_gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0) ||
            (m_gpxGeomType == GPX_ROUTE_POINT &&
             strcmp(pszName, "rtept") == 0) ||
            (m_gpxGeomType == GPX_TRACK_POINT && strcmp(pszName, "trkpt") == 0))
        {
            const bool bIsValid = (m_hasFoundLat && m_hasFoundLon);
            m_inInterestingElement = false;

            if (bIsValid &&
                (m_poFilterGeom == nullptr ||
                 FilterGeometry(m_poFeature->GetGeometryRef())) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(m_poFeature.get())))
            {
                if (auto poGeom = m_poFeature->GetGeometryRef())
                {
                    poGeom->assignSpatialReference(m_poSRS);

                    if (m_bEleAs25D)
                    {
                        const int iEleField =
                            m_poFeatureDefn->GetFieldIndex("ele");
                        if (iEleField >= 0 &&
                            m_poFeature->IsFieldSetAndNotNull(iEleField))
                        {
                            const double val =
                                m_poFeature->GetFieldAsDouble(iEleField);
                            poGeom->toPoint()->setZ(val);
                            poGeom->setCoordinateDimension(3);
                        }
                    }
                }

                m_oFeatureQueue.push_back(std::move(m_poFeature));
            }
            else
            {
                m_poFeature.reset();
            }
        }
        else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
        {
            m_poFeature->SetGeometryDirectly(m_multiLineString.release());
            m_lineString.reset();
            m_multiLineString.reset();

            m_inInterestingElement = false;
            if ((m_poFilterGeom == nullptr ||
                 FilterGeometry(m_poFeature->GetGeometryRef())) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(m_poFeature.get())))
            {
                if (m_poFeature->GetGeometryRef() != nullptr)
                {
                    m_poFeature->GetGeometryRef()->assignSpatialReference(
                        m_poSRS);
                }

                m_oFeatureQueue.push_back(std::move(m_poFeature));
            }
            else
            {
                m_poFeature.reset();
            }
        }
        else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trkseg") == 0 &&
                 m_depthLevel == m_interestingDepthLevel + 1)
        {
            if (m_multiLineString)
            {
                m_multiLineString->addGeometry(std::move(m_lineString));
            }
            else
            {
                m_lineString.reset();
            }
        }
        else if (m_gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
        {
            m_poFeature->SetGeometryDirectly(m_lineString.release());
            m_lineString.reset();

            m_inInterestingElement = false;
            if ((m_poFilterGeom == nullptr ||
                 FilterGeometry(m_poFeature->GetGeometryRef())) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(m_poFeature.get())))
            {
                if (m_poFeature->GetGeometryRef() != nullptr)
                {
                    m_poFeature->GetGeometryRef()->assignSpatialReference(
                        m_poSRS);
                }

                m_oFeatureQueue.push_back(std::move(m_poFeature));
            }
            else
            {
                m_poFeature.reset();
            }
        }
        else if (m_bEleAs25D && strcmp(pszName, "ele") == 0 &&
                 m_lineString != nullptr &&
                 ((m_gpxGeomType == GPX_ROUTE &&
                   m_depthLevel == m_interestingDepthLevel + 2) ||
                  (m_gpxGeomType == GPX_TRACK &&
                   m_depthLevel == m_interestingDepthLevel + 3)))
        {
            m_lineString->setCoordinateDimension(3);

            if (!m_osSubElementValue.empty())
            {
                const double val = CPLAtof(m_osSubElementValue.c_str());
                const int i = m_lineString->getNumPoints() - 1;
                if (i >= 0)
                    m_lineString->setPoint(i, m_lineString->getX(i),
                                           m_lineString->getY(i), val);
            }

            m_osSubElementName.clear();
            m_osSubElementValue.clear();
        }
        else if (m_depthLevel == m_interestingDepthLevel + 1 &&
                 strcmp(pszName, "extensions") == 0)
        {
            m_inExtensions = false;
        }
        else if ((m_depthLevel == m_interestingDepthLevel + 1 ||
                  (m_inExtensions &&
                   m_depthLevel == m_interestingDepthLevel + 2)) &&
                 !m_osSubElementName.empty() && m_osSubElementName == pszName)
        {
            if (m_poFeature && !m_osSubElementValue.empty())
            {
                if (m_osSubElementValue == "time" && m_iCurrentField >= 0 &&
                    m_poFeature->GetFieldDefnRef(m_iCurrentField)->GetType() ==
                        OFTDateTime)
                {
                    OGRField sField;
                    if (OGRParseXMLDateTime(m_osSubElementValue.c_str(),
                                            &sField))
                    {
                        m_poFeature->SetField(m_iCurrentField, &sField);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Could not parse %s as a valid dateTime",
                                 m_osSubElementValue.c_str());
                    }
                }
                else
                {
                    m_poFeature->SetField(m_iCurrentField,
                                          m_osSubElementValue.c_str());
                }
            }
            if (strcmp(pszName, "link") == 0)
                m_inLink = false;

            m_osSubElementName.clear();
            m_osSubElementValue.clear();
        }
        else if (m_inLink && m_depthLevel == m_interestingDepthLevel + 2)
        {
            if (m_iCurrentField != -1 && !m_osSubElementName.empty() &&
                m_osSubElementName == pszName && m_poFeature &&
                !m_osSubElementValue.empty())
            {
                m_poFeature->SetField(m_iCurrentField,
                                      m_osSubElementValue.c_str());
            }

            m_osSubElementName.clear();
            m_osSubElementValue.clear();
        }
        else if (m_inExtensions && m_depthLevel > m_interestingDepthLevel + 2)
        {
            AddStrToSubElementValue(CPLSPrintf("</%s>", pszName));
        }
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRGPXLayer::dataHandlerCbk(const char *data, int nLen)
{
    if (m_bStopParsing)
        return;

    m_nDataHandlerCounter++;
    if (m_nDataHandlerCounter >= PARSER_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(m_oParser, XML_FALSE);
        m_bStopParsing = true;
        return;
    }

    m_nWithoutEventCounter = 0;

    if (!m_osSubElementName.empty())
    {
        if (m_inExtensions && m_depthLevel > m_interestingDepthLevel + 2)
        {
            if (data[0] == '\n')
                return;
        }
        try
        {
            m_osSubElementValue.append(data, nLen);
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when parsing GPX file");
            XML_StopParser(m_oParser, XML_FALSE);
            m_bStopParsing = true;
            return;
        }
        if (m_osSubElementValue.size() > 100000)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too much data inside one element. "
                     "File probably corrupted");
            XML_StopParser(m_oParser, XML_FALSE);
            m_bStopParsing = true;
        }
    }
}
#endif

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGPXLayer::GetNextFeature()
{
    if (m_bWriteMode)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GPX file");
        return nullptr;
    }

    if (m_fpGPX == nullptr)
        return nullptr;

#ifdef HAVE_EXPAT

    if (m_bStopParsing)
        return nullptr;

    if (!m_oFeatureQueue.empty())
    {
        OGRFeature *poFeature = std::move(m_oFeatureQueue.front()).release();
        m_oFeatureQueue.pop_front();
        return poFeature;
    }

    if (m_fpGPX->Eof())
        return nullptr;

    std::vector<char> aBuf(PARSER_BUF_SIZE);
    m_nWithoutEventCounter = 0;

    int nDone = 0;
    do
    {
        m_nDataHandlerCounter = 0;
        unsigned int nLen = static_cast<unsigned int>(
            m_fpGPX->Read(aBuf.data(), 1, aBuf.size()));
        nDone = m_fpGPX->Eof();
        if (XML_Parse(m_oParser, aBuf.data(), nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of GPX file failed : "
                     "%s at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(m_oParser)),
                     static_cast<int>(XML_GetCurrentLineNumber(m_oParser)),
                     static_cast<int>(XML_GetCurrentColumnNumber(m_oParser)));
            m_bStopParsing = true;
            break;
        }
        m_nWithoutEventCounter++;
    } while (!nDone && m_oFeatureQueue.empty() && !m_bStopParsing &&
             m_nWithoutEventCounter < 10);

    if (m_nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        m_bStopParsing = true;
    }

    if (!m_oFeatureQueue.empty())
    {
        OGRFeature *poFeature = std::move(m_oFeatureQueue.front()).release();
        m_oFeatureQueue.pop_front();
        return poFeature;
    }
#endif
    return nullptr;
}

/************************************************************************/
/*                  OGRGPX_GetXMLCompatibleTagName()                    */
/************************************************************************/

static char *OGRGPX_GetXMLCompatibleTagName(const char *pszExtensionsNS,
                                            const char *pszName)
{
    /* Skip "ogr_" for example if NS is "ogr". Useful for GPX -> GPX roundtrip
     */
    if (strncmp(pszName, pszExtensionsNS, strlen(pszExtensionsNS)) == 0 &&
        pszName[strlen(pszExtensionsNS)] == '_')
    {
        pszName += strlen(pszExtensionsNS) + 1;
    }

    char *pszModName = CPLStrdup(pszName);
    for (int i = 0; pszModName[i] != 0; i++)
    {
        if (pszModName[i] == ' ')
            pszModName[i] = '_';
    }
    return pszModName;
}

/************************************************************************/
/*                     OGRGPX_GetUTF8String()                           */
/************************************************************************/

static char *OGRGPX_GetUTF8String(const char *pszString)
{
    char *pszEscaped = nullptr;
    if (!CPLIsUTF8(pszString, -1) &&
        CPLTestBool(CPLGetConfigOption("OGR_FORCE_ASCII", "YES")))
    {
        static bool bFirstTime = true;
        if (bFirstTime)
        {
            bFirstTime = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s is not a valid UTF-8 string. Forcing it to ASCII.\n"
                     "If you still want the original string and change the XML "
                     "file encoding\n"
                     "afterwards, you can define OGR_FORCE_ASCII=NO as "
                     "configuration option.\n"
                     "This warning won't be issued anymore",
                     pszString);
        }
        else
        {
            CPLDebug("OGR",
                     "%s is not a valid UTF-8 string. Forcing it to ASCII",
                     pszString);
        }
        pszEscaped = CPLForceToASCII(pszString, -1, '?');
    }
    else
    {
        pszEscaped = CPLStrdup(pszString);
    }

    return pszEscaped;
}

/************************************************************************/
/*                   OGRGPX_WriteXMLExtension()                          */
/************************************************************************/

bool OGRGPXLayer::OGRGPX_WriteXMLExtension(const char *pszTagName,
                                           const char *pszContent)
{
    CPLXMLNode *poXML = CPLParseXMLString(pszContent);
    if (poXML)
    {
        const char *pszUnderscore = strchr(pszTagName, '_');
        char *pszTagNameWithNS = CPLStrdup(pszTagName);
        if (pszUnderscore)
            pszTagNameWithNS[pszUnderscore - pszTagName] = ':';

        /* If we detect a Garmin GPX extension, add its xmlns */
        const char *pszXMLNS = nullptr;
        if (strcmp(pszTagName, "gpxx_WaypointExtension") == 0)
            pszXMLNS = " xmlns:gpxx=\"http://www.garmin.com/xmlschemas/"
                       "GpxExtensions/v3\"";

        /* Don't XML escape here */
        char *pszUTF8 = OGRGPX_GetUTF8String(pszContent);
        m_poDS->PrintLine("    <%s%s>%s</%s>", pszTagNameWithNS,
                          (pszXMLNS) ? pszXMLNS : "", pszUTF8,
                          pszTagNameWithNS);
        CPLFree(pszUTF8);

        CPLFree(pszTagNameWithNS);
        CPLDestroyXMLNode(poXML);

        return true;
    }

    return false;
}

/************************************************************************/
/*                      WriteFeatureAttributes()                        */
/************************************************************************/

static void AddIdent(VSILFILE *fp, int nIdentLevel)
{
    for (int i = 0; i < nIdentLevel; i++)
        VSIFPrintfL(fp, "  ");
}

void OGRGPXLayer::WriteFeatureAttributes(const OGRFeature *poFeature,
                                         int nIdentLevel)
{
    VSILFILE *fp = m_poDS->GetOutputFP();

    /* Begin with standard GPX fields */
    int i = m_iFirstGPXField;
    for (; i < m_nGPXFields; i++)
    {
        const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (poFeature->IsFieldSetAndNotNull(i))
        {
            const char *pszName = poFieldDefn->GetNameRef();
            if (strcmp(pszName, "time") == 0)
            {
                char *pszDate = OGRGetXMLDateTime(poFeature->GetRawFieldRef(i));
                AddIdent(fp, nIdentLevel);
                m_poDS->PrintLine("<time>%s</time>", pszDate);
                CPLFree(pszDate);
            }
            else if (STARTS_WITH(pszName, "link"))
            {
                if (strstr(pszName, "href"))
                {
                    AddIdent(fp, nIdentLevel);
                    VSIFPrintfL(fp, "<link href=\"%s\">",
                                poFeature->GetFieldAsString(i));
                    if (poFeature->IsFieldSetAndNotNull(i + 1))
                        VSIFPrintfL(fp, "<text>%s</text>",
                                    poFeature->GetFieldAsString(i + 1));
                    if (poFeature->IsFieldSetAndNotNull(i + 2))
                        VSIFPrintfL(fp, "<type>%s</type>",
                                    poFeature->GetFieldAsString(i + 2));
                    m_poDS->PrintLine("</link>");
                }
            }
            else if (poFieldDefn->GetType() == OFTReal)
            {
                char szValue[64];
                OGRFormatDouble(szValue, sizeof(szValue),
                                poFeature->GetFieldAsDouble(i), '.');
                AddIdent(fp, nIdentLevel);
                m_poDS->PrintLine("<%s>%s</%s>", pszName, szValue, pszName);
            }
            else
            {
                char *pszValue = OGRGetXML_UTF8_EscapedString(
                    poFeature->GetFieldAsString(i));
                AddIdent(fp, nIdentLevel);
                m_poDS->PrintLine("<%s>%s</%s>", pszName, pszValue, pszName);
                CPLFree(pszValue);
            }
        }
    }

    /* Write "extra" fields within the <extensions> tag */
    const int n = m_poFeatureDefn->GetFieldCount();
    if (i < n)
    {
        const std::string &osExtensionsNS = m_poDS->GetExtensionsNS();
        AddIdent(fp, nIdentLevel);
        m_poDS->PrintLine("<extensions>");
        for (; i < n; i++)
        {
            const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            if (poFeature->IsFieldSetAndNotNull(i))
            {
                char *compatibleName = OGRGPX_GetXMLCompatibleTagName(
                    osExtensionsNS.c_str(), poFieldDefn->GetNameRef());

                if (poFieldDefn->GetType() == OFTReal)
                {
                    char szValue[64];
                    OGRFormatDouble(szValue, sizeof(szValue),
                                    poFeature->GetFieldAsDouble(i), '.');
                    AddIdent(fp, nIdentLevel + 1);
                    m_poDS->PrintLine("<%s:%s>%s</%s:%s>",
                                      osExtensionsNS.c_str(), compatibleName,
                                      szValue, osExtensionsNS.c_str(),
                                      compatibleName);
                }
                else
                {
                    const char *pszRaw = poFeature->GetFieldAsString(i);

                    /* Try to detect XML content */
                    if (pszRaw[0] == '<' && pszRaw[strlen(pszRaw) - 1] == '>')
                    {
                        if (OGRGPX_WriteXMLExtension(compatibleName, pszRaw))
                        {
                            CPLFree(compatibleName);
                            continue;
                        }
                    }

                    /* Try to detect XML escaped content */
                    else if (STARTS_WITH(pszRaw, "&lt;") &&
                             STARTS_WITH(pszRaw + strlen(pszRaw) - 4, "&gt;"))
                    {
                        char *pszUnescapedContent =
                            CPLUnescapeString(pszRaw, nullptr, CPLES_XML);

                        if (OGRGPX_WriteXMLExtension(compatibleName,
                                                     pszUnescapedContent))
                        {
                            CPLFree(pszUnescapedContent);
                            CPLFree(compatibleName);
                            continue;
                        }

                        CPLFree(pszUnescapedContent);
                    }

                    /* Remove leading spaces for a numeric field */
                    if (poFieldDefn->GetType() == OFTInteger)
                    {
                        while (*pszRaw == ' ')
                            pszRaw++;
                    }

                    char *pszEscaped = OGRGetXML_UTF8_EscapedString(pszRaw);
                    AddIdent(fp, nIdentLevel + 1);
                    m_poDS->PrintLine("<%s:%s>%s</%s:%s>",
                                      osExtensionsNS.c_str(), compatibleName,
                                      pszEscaped, osExtensionsNS.c_str(),
                                      compatibleName);
                    CPLFree(pszEscaped);
                }
                CPLFree(compatibleName);
            }
        }
        AddIdent(fp, nIdentLevel);
        m_poDS->PrintLine("</extensions>");
    }
}

/************************************************************************/
/*                CheckAndFixCoordinatesValidity()                      */
/************************************************************************/

OGRErr OGRGPXLayer::CheckAndFixCoordinatesValidity(double *pdfLatitude,
                                                   double *pdfLongitude)
{
    if (pdfLatitude != nullptr && (*pdfLatitude < -90 || *pdfLatitude > 90))
    {
        static bool bFirstWarning = true;
        if (bFirstWarning)
        {
            bFirstWarning = false;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Latitude %f is invalid. Valid range is [-90,90]. "
                     "This warning will not be issued any more",
                     *pdfLatitude);
        }
        return OGRERR_FAILURE;
    }

    if (pdfLongitude != nullptr &&
        (*pdfLongitude < -180 || *pdfLongitude > 180))
    {
        static bool bFirstWarning = true;
        if (bFirstWarning)
        {
            bFirstWarning = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Longitude %f has been modified to fit into "
                     "range [-180,180]. This warning will not be "
                     "issued any more",
                     *pdfLongitude);
        }

        *pdfLongitude = fmod(*pdfLongitude + 180.0, 360.0) - 180.0;
        return OGRERR_NONE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGPXLayer::ICreateFeature(OGRFeature *poFeature)

{
    VSILFILE *fp = m_poDS->GetOutputFP();
    if (fp == nullptr)
        return OGRERR_FAILURE;

    char szLat[64];
    char szLon[64];
    char szAlt[64];

    const OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (m_gpxGeomType == GPX_WPT)
    {
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_ROUTE)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot write a 'wpt' element after a 'rte' element.\n");
            return OGRERR_FAILURE;
        }
        else if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot write a 'wpt' element after a 'trk' element.\n");
            return OGRERR_FAILURE;
        }

        m_poDS->SetLastGPXGeomTypeWritten(m_gpxGeomType);

        if (poGeom == nullptr ||
            wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Features without geometry or with non-ponctual geometries not "
                "supported by GPX writer in waypoints layer.");
            return OGRERR_FAILURE;
        }

        if (poGeom->getCoordinateDimension() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "POINT EMPTY geometries not supported by GPX writer.");
            return OGRERR_FAILURE;
        }

        const OGRPoint *point = poGeom->toPoint();
        double lat = point->getY();
        double lon = point->getX();
        CheckAndFixCoordinatesValidity(&lat, &lon);
        m_poDS->AddCoord(lon, lat);
        OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
        OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
        m_poDS->PrintLine("<wpt lat=\"%s\" lon=\"%s\">", szLat, szLon);
        WriteFeatureAttributes(poFeature);
        m_poDS->PrintLine("</wpt>");
    }
    else if (m_gpxGeomType == GPX_ROUTE)
    {
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK ||
            m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK_POINT)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot write a 'rte' element after a 'trk' element.\n");
            return OGRERR_FAILURE;
        }

        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_ROUTE_POINT &&
            m_poDS->m_nLastRteId != -1)
        {
            m_poDS->PrintLine("</rte>");
            m_poDS->m_nLastRteId = -1;
        }

        m_poDS->SetLastGPXGeomTypeWritten(m_gpxGeomType);

        const OGRLineString *line = nullptr;

        if (poGeom == nullptr)
        {
            m_poDS->PrintLine("<rte>");
            WriteFeatureAttributes(poFeature);
            m_poDS->PrintLine("</rte>");
            return OGRERR_NONE;
        }

        switch (poGeom->getGeometryType())
        {
            case wkbLineString:
            case wkbLineString25D:
            {
                line = poGeom->toLineString();
                break;
            }

            case wkbMultiLineString:
            case wkbMultiLineString25D:
            {
                int nGeometries =
                    poGeom->toMultiLineString()->getNumGeometries();
                if (nGeometries == 0)
                {
                    line = nullptr;
                }
                else if (nGeometries == 1)
                {
                    line = poGeom->toMultiLineString()->getGeometryRef(0);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Multiline with more than one line is not "
                             "supported for 'rte' element.");
                    return OGRERR_FAILURE;
                }
                break;
            }

            default:
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Geometry type of `%s' not supported for 'rte' element.\n",
                    OGRGeometryTypeToName(poGeom->getGeometryType()));
                return OGRERR_FAILURE;
            }
        }

        m_poDS->PrintLine("<rte>");
        WriteFeatureAttributes(poFeature);
        if (line)
        {
            const int n = line->getNumPoints();
            for (int i = 0; i < n; i++)
            {
                double lat = line->getY(i);
                double lon = line->getX(i);
                CheckAndFixCoordinatesValidity(&lat, &lon);
                m_poDS->AddCoord(lon, lat);
                OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
                OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
                m_poDS->PrintLine("  <rtept lat=\"%s\" lon=\"%s\">", szLat,
                                  szLon);
                if (poGeom->getGeometryType() == wkbLineString25D ||
                    poGeom->getGeometryType() == wkbMultiLineString25D)
                {
                    OGRFormatDouble(szAlt, sizeof(szAlt), line->getZ(i), '.');
                    m_poDS->PrintLine("    <ele>%s</ele>", szAlt);
                }
                m_poDS->PrintLine("  </rtept>");
            }
        }
        m_poDS->PrintLine("</rte>");
    }
    else if (m_gpxGeomType == GPX_TRACK)
    {
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_ROUTE_POINT &&
            m_poDS->m_nLastRteId != -1)
        {
            m_poDS->PrintLine("</rte>");
            m_poDS->m_nLastRteId = -1;
        }
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK_POINT &&
            m_poDS->m_nLastTrkId != -1)
        {
            m_poDS->PrintLine("  </trkseg>");
            m_poDS->PrintLine("</trk>");
            m_poDS->m_nLastTrkId = -1;
            m_poDS->m_nLastTrkSegId = -1;
        }

        m_poDS->SetLastGPXGeomTypeWritten(m_gpxGeomType);

        if (poGeom == nullptr)
        {
            m_poDS->PrintLine("<trk>");
            WriteFeatureAttributes(poFeature);
            m_poDS->PrintLine("</trk>");
            return OGRERR_NONE;
        }

        switch (poGeom->getGeometryType())
        {
            case wkbLineString:
            case wkbLineString25D:
            {
                const OGRLineString *line = poGeom->toLineString();
                const int n = line->getNumPoints();
                m_poDS->PrintLine("<trk>");
                WriteFeatureAttributes(poFeature);
                m_poDS->PrintLine("  <trkseg>");
                for (int i = 0; i < n; i++)
                {
                    double lat = line->getY(i);
                    double lon = line->getX(i);
                    CheckAndFixCoordinatesValidity(&lat, &lon);
                    m_poDS->AddCoord(lon, lat);
                    OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
                    OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
                    m_poDS->PrintLine("    <trkpt lat=\"%s\" lon=\"%s\">",
                                      szLat, szLon);
                    if (line->getGeometryType() == wkbLineString25D)
                    {
                        OGRFormatDouble(szAlt, sizeof(szAlt), line->getZ(i),
                                        '.');
                        m_poDS->PrintLine("        <ele>%s</ele>", szAlt);
                    }
                    m_poDS->PrintLine("    </trkpt>");
                }
                m_poDS->PrintLine("  </trkseg>");
                m_poDS->PrintLine("</trk>");
                break;
            }

            case wkbMultiLineString:
            case wkbMultiLineString25D:
            {
                m_poDS->PrintLine("<trk>");
                WriteFeatureAttributes(poFeature);
                for (auto &&line : poGeom->toMultiLineString())
                {
                    const int n = (line) ? line->getNumPoints() : 0;
                    m_poDS->PrintLine("  <trkseg>");
                    for (int i = 0; i < n; i++)
                    {
                        double lat = line->getY(i);
                        double lon = line->getX(i);
                        CheckAndFixCoordinatesValidity(&lat, &lon);
                        m_poDS->AddCoord(lon, lat);
                        OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
                        OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
                        m_poDS->PrintLine("    <trkpt lat=\"%s\" lon=\"%s\">",
                                          szLat, szLon);
                        if (line->getGeometryType() == wkbLineString25D)
                        {
                            OGRFormatDouble(szAlt, sizeof(szAlt), line->getZ(i),
                                            '.');
                            m_poDS->PrintLine("        <ele>%s</ele>", szAlt);
                        }
                        m_poDS->PrintLine("    </trkpt>");
                    }
                    m_poDS->PrintLine("  </trkseg>");
                }
                m_poDS->PrintLine("</trk>");
                break;
            }

            default:
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Geometry type of `%s' not supported for 'trk' element.\n",
                    OGRGeometryTypeToName(poGeom->getGeometryType()));
                return OGRERR_FAILURE;
            }
        }
    }
    else if (m_gpxGeomType == GPX_ROUTE_POINT)
    {
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK ||
            m_poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK_POINT)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot write a 'rte' element after a 'trk' element.\n");
            return OGRERR_FAILURE;
        }

        if (poGeom == nullptr ||
            wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Features without geometry or with non-ponctual geometries not "
                "supported by GPX writer in route_points layer.");
            return OGRERR_FAILURE;
        }

        if (poGeom->getCoordinateDimension() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "POINT EMPTY geometries not supported by GPX writer.");
            return OGRERR_FAILURE;
        }

        if (!poFeature->IsFieldSetAndNotNull(FLD_ROUTE_FID))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Field %s must be set.",
                m_poFeatureDefn->GetFieldDefn(FLD_ROUTE_FID)->GetNameRef());
            return OGRERR_FAILURE;
        }
        if (poFeature->GetFieldAsInteger(FLD_ROUTE_FID) < 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Invalid value for field %s.",
                m_poFeatureDefn->GetFieldDefn(FLD_ROUTE_FID)->GetNameRef());
            return OGRERR_FAILURE;
        }

        m_poDS->SetLastGPXGeomTypeWritten(m_gpxGeomType);

        if (m_poDS->m_nLastRteId != poFeature->GetFieldAsInteger(FLD_ROUTE_FID))
        {
            if (m_poDS->m_nLastRteId != -1)
            {
                m_poDS->PrintLine("</rte>");
            }
            m_poDS->PrintLine("<rte>");
            if (poFeature->IsFieldSetAndNotNull(FLD_ROUTE_NAME))
            {
                char *pszValue = OGRGetXML_UTF8_EscapedString(
                    poFeature->GetFieldAsString(FLD_ROUTE_NAME));
                m_poDS->PrintLine("  <%s>%s</%s>", "name", pszValue, "name");
                CPLFree(pszValue);
            }
        }

        m_poDS->m_nLastRteId = poFeature->GetFieldAsInteger(FLD_ROUTE_FID);

        const OGRPoint *point = poGeom->toPoint();
        double lat = point->getY();
        double lon = point->getX();
        CheckAndFixCoordinatesValidity(&lat, &lon);
        m_poDS->AddCoord(lon, lat);
        OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
        OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
        m_poDS->PrintLine("  <rtept lat=\"%s\" lon=\"%s\">", szLat, szLon);
        WriteFeatureAttributes(poFeature, 2);
        m_poDS->PrintLine("  </rtept>");
    }
    else
    {
        if (m_poDS->GetLastGPXGeomTypeWritten() == GPX_ROUTE_POINT &&
            m_poDS->m_nLastRteId != -1)
        {
            m_poDS->PrintLine("</rte>");
            m_poDS->m_nLastRteId = -1;
        }

        if (poGeom == nullptr ||
            wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Features without geometry or with non-ponctual geometries not "
                "supported by GPX writer in track_points layer.");
            return OGRERR_FAILURE;
        }

        if (poGeom->getCoordinateDimension() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "POINT EMPTY geometries not supported by GPX writer.");
            return OGRERR_FAILURE;
        }

        if (!poFeature->IsFieldSetAndNotNull(FLD_TRACK_FID))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Field %s must be set.",
                m_poFeatureDefn->GetFieldDefn(FLD_TRACK_FID)->GetNameRef());
            return OGRERR_FAILURE;
        }
        if (poFeature->GetFieldAsInteger(FLD_TRACK_FID) < 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Invalid value for field %s.",
                m_poFeatureDefn->GetFieldDefn(FLD_TRACK_FID)->GetNameRef());
            return OGRERR_FAILURE;
        }
        if (!poFeature->IsFieldSetAndNotNull(FLD_TRACK_SEG_ID))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Field %s must be set.",
                m_poFeatureDefn->GetFieldDefn(FLD_TRACK_SEG_ID)->GetNameRef());
            return OGRERR_FAILURE;
        }
        if (poFeature->GetFieldAsInteger(FLD_TRACK_SEG_ID) < 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "Invalid value for field %s.",
                m_poFeatureDefn->GetFieldDefn(FLD_TRACK_SEG_ID)->GetNameRef());
            return OGRERR_FAILURE;
        }

        m_poDS->SetLastGPXGeomTypeWritten(m_gpxGeomType);

        if (m_poDS->m_nLastTrkId != poFeature->GetFieldAsInteger(FLD_TRACK_FID))
        {
            if (m_poDS->m_nLastTrkId != -1)
            {
                m_poDS->PrintLine("  </trkseg>");
                m_poDS->PrintLine("</trk>");
            }
            m_poDS->PrintLine("<trk>");

            if (poFeature->IsFieldSetAndNotNull(FLD_TRACK_NAME))
            {
                char *pszValue = OGRGetXML_UTF8_EscapedString(
                    poFeature->GetFieldAsString(FLD_TRACK_NAME));
                m_poDS->PrintLine("  <%s>%s</%s>", "name", pszValue, "name");
                CPLFree(pszValue);
            }

            m_poDS->PrintLine("  <trkseg>");
        }
        else if (m_poDS->m_nLastTrkSegId !=
                 poFeature->GetFieldAsInteger(FLD_TRACK_SEG_ID))
        {
            m_poDS->PrintLine("  </trkseg>");
            m_poDS->PrintLine("  <trkseg>");
        }

        m_poDS->m_nLastTrkId = poFeature->GetFieldAsInteger(FLD_TRACK_FID);
        m_poDS->m_nLastTrkSegId =
            poFeature->GetFieldAsInteger(FLD_TRACK_SEG_ID);

        const OGRPoint *point = poGeom->toPoint();
        double lat = point->getY();
        double lon = point->getX();
        CheckAndFixCoordinatesValidity(&lat, &lon);
        m_poDS->AddCoord(lon, lat);
        OGRFormatDouble(szLat, sizeof(szLat), lat, '.');
        OGRFormatDouble(szLon, sizeof(szLon), lon, '.');
        m_poDS->PrintLine("    <trkpt lat=\"%s\" lon=\"%s\">", szLat, szLon);
        WriteFeatureAttributes(poFeature, 3);
        m_poDS->PrintLine("    </trkpt>");
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGPXLayer::CreateField(const OGRFieldDefn *poField, int /*bApproxOK*/)
{
    for (int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++)
    {
        if (strcmp(m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                   poField->GetNameRef()) == 0)
        {
            return OGRERR_NONE;
        }
    }
    if (!m_poDS->GetUseExtensions())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Field of name '%s' is not supported in GPX schema. "
                 "Use GPX_USE_EXTENSIONS creation option to allow use of the "
                 "<extensions> element.",
                 poField->GetNameRef());
        return OGRERR_FAILURE;
    }
    else
    {
        m_poFeatureDefn->AddFieldDefn(poField);
        return OGRERR_NONE;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPXLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCSequentialWrite))
        return m_bWriteMode;
    else if (EQUAL(pszCap, OLCCreateField))
        return m_bWriteMode;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                       LoadExtensionsSchema()                         */
/************************************************************************/

#ifdef HAVE_EXPAT

static void XMLCALL startElementLoadSchemaCbk(void *pUserData,
                                              const char *pszName,
                                              const char **ppszAttr)
{
    static_cast<OGRGPXLayer *>(pUserData)->startElementLoadSchemaCbk(pszName,
                                                                     ppszAttr);
}

static void XMLCALL endElementLoadSchemaCbk(void *pUserData,
                                            const char *pszName)
{
    static_cast<OGRGPXLayer *>(pUserData)->endElementLoadSchemaCbk(pszName);
}

static void XMLCALL dataHandlerLoadSchemaCbk(void *pUserData, const char *data,
                                             int nLen)
{
    static_cast<OGRGPXLayer *>(pUserData)->dataHandlerLoadSchemaCbk(data, nLen);
}

/** This function parses the whole file to detect the extensions fields */
void OGRGPXLayer::LoadExtensionsSchema()
{
    m_oSchemaParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(m_oSchemaParser, ::startElementLoadSchemaCbk,
                          ::endElementLoadSchemaCbk);
    XML_SetCharacterDataHandler(m_oSchemaParser, ::dataHandlerLoadSchemaCbk);
    XML_SetUserData(m_oSchemaParser, this);

    m_fpGPX->Seek(0, SEEK_SET);

    m_inInterestingElement = false;
    m_inExtensions = false;
    m_depthLevel = 0;
    m_currentFieldDefn = nullptr;
    m_osSubElementName.clear();
    m_osSubElementValue.clear();
    m_nWithoutEventCounter = 0;
    m_bStopParsing = false;

    std::vector<char> aBuf(PARSER_BUF_SIZE);
    int nDone = 0;
    do
    {
        m_nDataHandlerCounter = 0;
        unsigned int nLen = static_cast<unsigned int>(
            m_fpGPX->Read(aBuf.data(), 1, aBuf.size()));
        nDone = m_fpGPX->Eof();
        if (XML_Parse(m_oSchemaParser, aBuf.data(), nLen, nDone) ==
            XML_STATUS_ERROR)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "XML parsing of GPX file failed : "
                "%s at line %d, column %d",
                XML_ErrorString(XML_GetErrorCode(m_oSchemaParser)),
                static_cast<int>(XML_GetCurrentLineNumber(m_oSchemaParser)),
                static_cast<int>(XML_GetCurrentColumnNumber(m_oSchemaParser)));
            m_bStopParsing = true;
            break;
        }
        m_nWithoutEventCounter++;
    } while (!nDone && !m_bStopParsing && m_nWithoutEventCounter < 10);

    if (m_nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        m_bStopParsing = true;
    }

    XML_ParserFree(m_oSchemaParser);
    m_oSchemaParser = nullptr;

    m_fpGPX->Seek(0, SEEK_SET);
}

/************************************************************************/
/*                  startElementLoadSchemaCbk()                         */
/************************************************************************/

void OGRGPXLayer::startElementLoadSchemaCbk(const char *pszName,
                                            CPL_UNUSED const char **ppszAttr)
{
    if (m_bStopParsing)
        return;

    m_nWithoutEventCounter = 0;

    if (m_gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0)
    {
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_interestingDepthLevel = m_depthLevel;
    }
    else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
    {
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_interestingDepthLevel = m_depthLevel;
    }
    else if (m_gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
    {
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_interestingDepthLevel = m_depthLevel;
    }
    else if (m_gpxGeomType == GPX_TRACK_POINT && strcmp(pszName, "trkpt") == 0)
    {
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_interestingDepthLevel = m_depthLevel;
    }
    else if (m_gpxGeomType == GPX_ROUTE_POINT && strcmp(pszName, "rtept") == 0)
    {
        m_inInterestingElement = true;
        m_inExtensions = false;
        m_interestingDepthLevel = m_depthLevel;
    }
    else if (m_inInterestingElement)
    {
        if (m_depthLevel == m_interestingDepthLevel + 1 &&
            strcmp(pszName, "extensions") == 0)
        {
            m_inExtensions = true;
            m_extensionsDepthLevel = m_depthLevel;
        }
        else if (m_inExtensions && m_depthLevel == m_extensionsDepthLevel + 1)
        {
            m_osSubElementName = pszName;

            int iField = 0;  // Used after for.
            for (; iField < m_poFeatureDefn->GetFieldCount(); iField++)
            {
                bool bMatch = false;
                if (iField >= m_nGPXFields)
                {
                    char *pszCompatibleName =
                        OGRGPX_GetOGRCompatibleTagName(pszName);
                    bMatch =
                        strcmp(
                            m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                            pszCompatibleName) == 0;
                    CPLFree(pszCompatibleName);
                }
                else
                {
                    bMatch =
                        strcmp(
                            m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                            pszName) == 0;
                }

                if (bMatch)
                {
                    m_currentFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
                    break;
                }
            }
            if (iField == m_poFeatureDefn->GetFieldCount())
            {
                char *pszCompatibleName =
                    OGRGPX_GetOGRCompatibleTagName(pszName);
                OGRFieldDefn newFieldDefn(pszCompatibleName, OFTInteger);
                CPLFree(pszCompatibleName);

                m_poFeatureDefn->AddFieldDefn(&newFieldDefn);
                m_currentFieldDefn = m_poFeatureDefn->GetFieldDefn(
                    m_poFeatureDefn->GetFieldCount() - 1);

                if (m_poFeatureDefn->GetFieldCount() == 100)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too many fields. File probably corrupted");
                    XML_StopParser(m_oSchemaParser, XML_FALSE);
                    m_bStopParsing = true;
                }
            }
        }
    }

    m_depthLevel++;
}

/************************************************************************/
/*                   endElementLoadSchemaCbk()                           */
/************************************************************************/

static bool OGRGPXIsInt(const char *pszStr)
{
    while (*pszStr == ' ')
        pszStr++;

    for (int i = 0; pszStr[i]; i++)
    {
        if (pszStr[i] == '+' || pszStr[i] == '-')
        {
            if (i != 0)
                return false;
        }
        else if (!(pszStr[i] >= '0' && pszStr[i] <= '9'))
            return false;
    }
    return true;
}

void OGRGPXLayer::endElementLoadSchemaCbk(const char *pszName)
{
    if (m_bStopParsing)
        return;

    m_nWithoutEventCounter = 0;

    m_depthLevel--;

    if (m_inInterestingElement)
    {
        if (m_gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0)
        {
            m_inInterestingElement = false;
            m_inExtensions = false;
        }
        else if (m_gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
        {
            m_inInterestingElement = false;
            m_inExtensions = false;
        }
        else if (m_gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
        {
            m_inInterestingElement = false;
            m_inExtensions = false;
        }
        else if (m_gpxGeomType == GPX_TRACK_POINT &&
                 strcmp(pszName, "trkpt") == 0)
        {
            m_inInterestingElement = false;
            m_inExtensions = false;
        }
        else if (m_gpxGeomType == GPX_ROUTE_POINT &&
                 strcmp(pszName, "rtept") == 0)
        {
            m_inInterestingElement = false;
            m_inExtensions = false;
        }
        else if (m_depthLevel == m_interestingDepthLevel + 1 &&
                 strcmp(pszName, "extensions") == 0)
        {
            m_inExtensions = false;
        }
        else if (m_inExtensions && m_depthLevel == m_extensionsDepthLevel + 1 &&
                 !m_osSubElementName.empty() && m_osSubElementName == pszName)
        {
            if (!m_osSubElementValue.empty() && m_currentFieldDefn)
            {
                if (m_currentFieldDefn->GetType() == OFTInteger ||
                    m_currentFieldDefn->GetType() == OFTReal)
                {
                    char *pszRemainingStr = nullptr;
                    CPLStrtod(m_osSubElementValue.c_str(), &pszRemainingStr);
                    if (pszRemainingStr == nullptr || *pszRemainingStr == 0 ||
                        *pszRemainingStr == ' ')
                    {
                        if (m_currentFieldDefn->GetType() == OFTInteger)
                        {
                            if (!OGRGPXIsInt(m_osSubElementValue.c_str()))
                            {
                                m_currentFieldDefn->SetType(OFTReal);
                            }
                        }
                    }
                    else
                    {
                        m_currentFieldDefn->SetType(OFTString);
                    }
                }
            }

            m_osSubElementName.clear();
            m_osSubElementValue.clear();
            m_currentFieldDefn = nullptr;
        }
    }
}

/************************************************************************/
/*                   dataHandlerLoadSchemaCbk()                         */
/************************************************************************/

void OGRGPXLayer::dataHandlerLoadSchemaCbk(const char *data, int nLen)
{
    if (m_bStopParsing)
        return;

    m_nDataHandlerCounter++;
    if (m_nDataHandlerCounter >= PARSER_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(m_oSchemaParser, XML_FALSE);
        m_bStopParsing = true;
        return;
    }

    m_nWithoutEventCounter = 0;

    if (!m_osSubElementName.empty())
    {
        try
        {
            m_osSubElementValue.append(data, nLen);
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory when parsing GPX file");
            XML_StopParser(m_oSchemaParser, XML_FALSE);
            m_bStopParsing = true;
            return;
        }
        if (m_osSubElementValue.size() > 100000)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too much data inside one element. "
                     "File probably corrupted");
            XML_StopParser(m_oSchemaParser, XML_FALSE);
            m_bStopParsing = true;
        }
    }
}
#else
void OGRGPXLayer::LoadExtensionsSchema()
{
}
#endif

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRGPXLayer::GetDataset()
{
    return m_poDS;
}
