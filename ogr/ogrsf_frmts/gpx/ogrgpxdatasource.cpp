/******************************************************************************
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#ifdef HAVE_EXPAT
#include "expat.h"
#endif
#include "ogr_core.h"
#include "ogr_expat.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

constexpr int SPACE_FOR_METADATA_BOUNDS = 160;

/************************************************************************/
/*                         ~OGRGPXDataSource()                          */
/************************************************************************/

OGRGPXDataSource::~OGRGPXDataSource()

{
    if (m_fpOutput != nullptr)
    {
        if (m_nLastRteId != -1)
            PrintLine("</rte>");
        else if (m_nLastTrkId != -1)
        {
            PrintLine("  </trkseg>");
            PrintLine("</trk>");
        }
        PrintLine("</gpx>");
        if (m_bIsBackSeekable)
        {
            /* Write the <bounds> element in the reserved space */
            if (m_dfMinLon <= m_dfMaxLon)
            {
                char szBounds[SPACE_FOR_METADATA_BOUNDS + 1];
                int nRet =
                    CPLsnprintf(szBounds, SPACE_FOR_METADATA_BOUNDS,
                                "<bounds minlat=\"%.15f\" minlon=\"%.15f\""
                                " maxlat=\"%.15f\" maxlon=\"%.15f\"/>",
                                m_dfMinLat, m_dfMinLon, m_dfMaxLat, m_dfMaxLon);
                if (nRet < SPACE_FOR_METADATA_BOUNDS)
                {
                    m_fpOutput->Seek(m_nOffsetBounds, SEEK_SET);
                    m_fpOutput->Write(szBounds, 1, strlen(szBounds));
                }
            }
        }
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPXDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCDeleteLayer))
        return FALSE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGPXDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return nullptr;

    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGPXDataSource::ICreateLayer(const char *pszLayerName,
                               const OGRGeomFieldDefn *poGeomFieldDefn,
                               CSLConstList papszOptions)
{
    GPXGeometryType gpxGeomType;
    const auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    if (eType == wkbPoint || eType == wkbPoint25D)
    {
        if (EQUAL(pszLayerName, "track_points"))
            gpxGeomType = GPX_TRACK_POINT;
        else if (EQUAL(pszLayerName, "route_points"))
            gpxGeomType = GPX_ROUTE_POINT;
        else
            gpxGeomType = GPX_WPT;
    }
    else if (eType == wkbLineString || eType == wkbLineString25D)
    {
        const char *pszForceGPXTrack =
            CSLFetchNameValue(papszOptions, "FORCE_GPX_TRACK");
        if (pszForceGPXTrack && CPLTestBool(pszForceGPXTrack))
            gpxGeomType = GPX_TRACK;
        else
            gpxGeomType = GPX_ROUTE;
    }
    else if (eType == wkbMultiLineString || eType == wkbMultiLineString25D)
    {
        const char *pszForceGPXRoute =
            CSLFetchNameValue(papszOptions, "FORCE_GPX_ROUTE");
        if (pszForceGPXRoute && CPLTestBool(pszForceGPXRoute))
            gpxGeomType = GPX_ROUTE;
        else
            gpxGeomType = GPX_TRACK;
    }
    else if (eType == wkbUnknown)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create GPX layer %s with unknown geometry type",
                 pszLayerName);
        return nullptr;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geometry type of `%s' not supported in GPX.\n",
                 OGRGeometryTypeToName(eType));
        return nullptr;
    }
    m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
        GetDescription(), pszLayerName, gpxGeomType, this, true, nullptr));

    return m_apoLayers.back().get();
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                startElementValidateCbk()                             */
/************************************************************************/

void OGRGPXDataSource::startElementValidateCbk(const char *pszNameIn,
                                               const char **ppszAttr)
{
    if (m_validity == GPX_VALIDITY_UNKNOWN)
    {
        if (strcmp(pszNameIn, "gpx") == 0)
        {
            m_validity = GPX_VALIDITY_VALID;
            for (int i = 0; ppszAttr[i] != nullptr; i += 2)
            {
                if (strcmp(ppszAttr[i], "version") == 0)
                {
                    m_osVersion = ppszAttr[i + 1];
                }
                else if (strcmp(ppszAttr[i], "xmlns:ogr") == 0)
                {
                    m_bUseExtensions = true;
                }
            }
        }
        else
        {
            m_validity = GPX_VALIDITY_INVALID;
        }
    }
    else if (m_validity == GPX_VALIDITY_VALID)
    {
        if (m_nDepth == 1 && strcmp(pszNameIn, "metadata") == 0)
        {
            m_bInMetadata = true;
        }
        else if (m_nDepth == 2 && m_bInMetadata)
        {
            if (strcmp(pszNameIn, "name") == 0)
            {
                m_osMetadataKey = "NAME";
            }
            else if (strcmp(pszNameIn, "desc") == 0)
            {
                m_osMetadataKey = "DESCRIPTION";
            }
            else if (strcmp(pszNameIn, "time") == 0)
            {
                m_osMetadataKey = "TIME";
            }
            else if (strcmp(pszNameIn, "author") == 0)
            {
                m_bInMetadataAuthor = true;
            }
            else if (strcmp(pszNameIn, "keywords") == 0)
            {
                m_osMetadataKey = "KEYWORDS";
            }
            else if (strcmp(pszNameIn, "copyright") == 0)
            {
                std::string osAuthor;
                for (int i = 0; ppszAttr[i] != nullptr; i += 2)
                {
                    if (strcmp(ppszAttr[i], "author") == 0)
                    {
                        osAuthor = ppszAttr[i + 1];
                    }
                }
                if (!osAuthor.empty())
                {
                    SetMetadataItem("COPYRIGHT_AUTHOR", osAuthor.c_str());
                }
                m_bInMetadataCopyright = true;
            }
            else if (strcmp(pszNameIn, "link") == 0)
            {
                ++m_nMetadataLinkCounter;
                std::string osHref;
                for (int i = 0; ppszAttr[i] != nullptr; i += 2)
                {
                    if (strcmp(ppszAttr[i], "href") == 0)
                    {
                        osHref = ppszAttr[i + 1];
                    }
                }
                if (!osHref.empty())
                {
                    SetMetadataItem(
                        CPLSPrintf("LINK_%d_HREF", m_nMetadataLinkCounter),
                        osHref.c_str());
                }
                m_bInMetadataLink = true;
            }
        }
        else if (m_nDepth == 3 && m_bInMetadataAuthor)
        {
            if (strcmp(pszNameIn, "name") == 0)
            {
                m_osMetadataKey = "AUTHOR_NAME";
            }
            else if (strcmp(pszNameIn, "email") == 0)
            {
                std::string osId, osDomain;
                for (int i = 0; ppszAttr[i] != nullptr; i += 2)
                {
                    if (strcmp(ppszAttr[i], "id") == 0)
                    {
                        osId = ppszAttr[i + 1];
                    }
                    else if (strcmp(ppszAttr[i], "domain") == 0)
                    {
                        osDomain = ppszAttr[i + 1];
                    }
                }
                if (!osId.empty() && !osDomain.empty())
                {
                    SetMetadataItem("AUTHOR_EMAIL",
                                    osId.append("@").append(osDomain).c_str());
                }
            }
            else if (strcmp(pszNameIn, "link") == 0)
            {
                std::string osHref;
                for (int i = 0; ppszAttr[i] != nullptr; i += 2)
                {
                    if (strcmp(ppszAttr[i], "href") == 0)
                    {
                        osHref = ppszAttr[i + 1];
                    }
                }
                if (!osHref.empty())
                {
                    SetMetadataItem("AUTHOR_LINK_HREF", osHref.c_str());
                }
                m_bInMetadataAuthorLink = true;
            }
        }
        else if (m_nDepth == 3 && m_bInMetadataCopyright)
        {
            if (strcmp(pszNameIn, "year") == 0)
            {
                m_osMetadataKey = "COPYRIGHT_YEAR";
            }
            else if (strcmp(pszNameIn, "license") == 0)
            {
                m_osMetadataKey = "COPYRIGHT_LICENSE";
            }
        }
        else if (m_nDepth == 3 && m_bInMetadataLink)
        {
            if (strcmp(pszNameIn, "text") == 0)
            {
                m_osMetadataKey =
                    CPLSPrintf("LINK_%d_TEXT", m_nMetadataLinkCounter);
            }
            else if (strcmp(pszNameIn, "type") == 0)
            {
                m_osMetadataKey =
                    CPLSPrintf("LINK_%d_TYPE", m_nMetadataLinkCounter);
            }
        }
        else if (m_nDepth == 4 && m_bInMetadataAuthorLink)
        {
            if (strcmp(pszNameIn, "text") == 0)
            {
                m_osMetadataKey = "AUTHOR_LINK_TEXT";
            }
            else if (strcmp(pszNameIn, "type") == 0)
            {
                m_osMetadataKey = "AUTHOR_LINK_TYPE";
            }
        }
        else if (m_nDepth == 2 && strcmp(pszNameIn, "extensions") == 0)
        {
            m_bUseExtensions = true;
        }
    }
    m_nDepth++;
}

/************************************************************************/
/*                    endElementValidateCbk()                           */
/************************************************************************/

void OGRGPXDataSource::endElementValidateCbk(const char * /*pszName */)
{
    m_nDepth--;
    if (m_nDepth == 4 && m_bInMetadataAuthorLink)
    {
        if (!m_osMetadataKey.empty())
        {
            SetMetadataItem(m_osMetadataKey.c_str(), m_osMetadataValue.c_str());
        }
        m_osMetadataKey.clear();
        m_osMetadataValue.clear();
    }
    else if (m_nDepth == 3 && (m_bInMetadataAuthor || m_bInMetadataCopyright ||
                               m_bInMetadataLink))
    {
        if (!m_osMetadataKey.empty())
        {
            SetMetadataItem(m_osMetadataKey.c_str(), m_osMetadataValue.c_str());
        }
        m_osMetadataKey.clear();
        m_osMetadataValue.clear();
        m_bInMetadataAuthorLink = false;
    }
    else if (m_nDepth == 2 && m_bInMetadata)
    {
        if (!m_osMetadataKey.empty())
        {
            SetMetadataItem(m_osMetadataKey.c_str(), m_osMetadataValue.c_str());
        }
        m_osMetadataKey.clear();
        m_osMetadataValue.clear();
        m_bInMetadataAuthor = false;
        m_bInMetadataCopyright = false;
    }
    else if (m_nDepth == 1 && m_bInMetadata)
    {
        m_bInMetadata = false;
    }
}

/************************************************************************/
/*                      dataHandlerValidateCbk()                        */
/************************************************************************/

void OGRGPXDataSource::dataHandlerValidateCbk(const char *data, int nLen)
{
    if (!m_osMetadataKey.empty())
    {
        m_osMetadataValue.append(data, nLen);
    }

    m_nDataHandlerCounter++;
    if (m_nDataHandlerCounter >= PARSER_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(m_oCurrentParser, XML_FALSE);
    }
}

static void XMLCALL startElementValidateCbk(void *pUserData,
                                            const char *pszName,
                                            const char **ppszAttr)
{
    OGRGPXDataSource *poDS = static_cast<OGRGPXDataSource *>(pUserData);
    poDS->startElementValidateCbk(pszName, ppszAttr);
}

static void XMLCALL endElementValidateCbk(void *pUserData, const char *pszName)
{
    OGRGPXDataSource *poDS = static_cast<OGRGPXDataSource *>(pUserData);
    poDS->endElementValidateCbk(pszName);
}

static void XMLCALL dataHandlerValidateCbk(void *pUserData, const char *data,
                                           int nLen)
{
    OGRGPXDataSource *poDS = static_cast<OGRGPXDataSource *>(pUserData);
    poDS->dataHandlerValidateCbk(data, nLen);
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGPXDataSource::Open(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGR/GPX driver does not support opening a file in "
                 "update mode");
        return FALSE;
    }
#ifdef HAVE_EXPAT
    SetDescription(pszFilename);

    /* -------------------------------------------------------------------- */
    /*      Try to open the file.                                           */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(pszFilename, "r");
    if (fp == nullptr)
        return FALSE;

    m_validity = GPX_VALIDITY_UNKNOWN;

    XML_Parser oParser = OGRCreateExpatXMLParser();
    m_oCurrentParser = oParser;
    XML_SetUserData(oParser, this);
    XML_SetElementHandler(oParser, ::startElementValidateCbk,
                          ::endElementValidateCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerValidateCbk);

    std::vector<char> aBuf(PARSER_BUF_SIZE);
    int nDone = 0;
    unsigned int nLen = 0;
    int nCount = 0;

    /* Begin to parse the file and look for the <gpx> element */
    /* It *MUST* be the first element of an XML file */
    /* So once we have read the first element, we know if we can */
    /* handle the file or not with that driver */
    uint64_t nTotalBytesRead = 0;
    do
    {
        m_nDataHandlerCounter = 0;
        nLen = static_cast<unsigned int>(
            VSIFReadL(aBuf.data(), 1, aBuf.size(), fp));
        nTotalBytesRead += nLen;
        nDone = VSIFEofL(fp);
        if (XML_Parse(oParser, aBuf.data(), nLen, nDone) == XML_STATUS_ERROR)
        {
            if (nLen <= PARSER_BUF_SIZE - 1)
                aBuf[nLen] = 0;
            else
                aBuf[PARSER_BUF_SIZE - 1] = 0;
            if (strstr(aBuf.data(), "<?xml") && strstr(aBuf.data(), "<gpx"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "XML parsing of GPX file failed : %s at line %d, "
                         "column %d",
                         XML_ErrorString(XML_GetErrorCode(oParser)),
                         static_cast<int>(XML_GetCurrentLineNumber(oParser)),
                         static_cast<int>(XML_GetCurrentColumnNumber(oParser)));
            }
            m_validity = GPX_VALIDITY_INVALID;
            break;
        }
        if (m_validity == GPX_VALIDITY_INVALID)
        {
            break;
        }
        else if (m_validity == GPX_VALIDITY_VALID)
        {
            /* If we have recognized the <gpx> element, now we try */
            /* to recognize if they are <extensions> tags */
            /* But we stop to look for after an arbitrary amount of bytes */
            if (m_bUseExtensions)
                break;
            else if (nTotalBytesRead > 1024 * 1024)
                break;
        }
        else
        {
            // After reading 50 * PARSER_BUF_SIZE bytes, and not finding whether the
            // file is GPX or not, we give up and fail silently.
            nCount++;
            if (nCount == 50)
                break;
        }
    } while (!nDone && nLen > 0);

    XML_ParserFree(oParser);

    VSIFCloseL(fp);

    if (m_validity == GPX_VALIDITY_VALID)
    {
        CPLDebug("GPX", "%s seems to be a GPX file.", pszFilename);
        if (m_bUseExtensions)
            CPLDebug("GPX", "It uses <extensions>");

        if (m_osVersion.empty())
        {
            /* Default to 1.1 */
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GPX schema version is unknown. "
                     "The driver may not be able to handle the file correctly "
                     "and will behave as if it is GPX 1.1.");
            m_osVersion = "1.1";
        }
        else if (m_osVersion == "1.0" || m_osVersion == "1.1")
        {
            /* Fine */
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GPX schema version '%s' is not handled by the driver. "
                     "The driver may not be able to handle the file correctly "
                     "and will behave as if it is GPX 1.1.",
                     m_osVersion.c_str());
        }

        m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
            GetDescription(), "waypoints", GPX_WPT, this, false,
            poOpenInfo->papszOpenOptions));
        m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
            GetDescription(), "routes", GPX_ROUTE, this, false,
            poOpenInfo->papszOpenOptions));
        m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
            GetDescription(), "tracks", GPX_TRACK, this, false,
            poOpenInfo->papszOpenOptions));
        m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
            GetDescription(), "route_points", GPX_ROUTE_POINT, this, false,
            poOpenInfo->papszOpenOptions));
        m_apoLayers.emplace_back(std::make_unique<OGRGPXLayer>(
            GetDescription(), "track_points", GPX_TRACK_POINT, this, false,
            poOpenInfo->papszOpenOptions));
    }

    return m_validity == GPX_VALIDITY_VALID;
#else
    VSILFILE *fp = VSIFOpenL(pszFilename, "r");
    if (fp)
    {
        char aBuf[256];
        unsigned int nLen =
            static_cast<unsigned int>(VSIFReadL(aBuf, 1, 255, fp));
        aBuf[nLen] = 0;
        if (strstr(aBuf, "<?xml") && strstr(aBuf, "<gpx"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "OGR/GPX driver has not been built with read support. "
                     "Expat library required");
        }
        VSIFCloseL(fp);
    }
    return FALSE;
#endif
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRGPXDataSource::Create(const char *pszFilename, char **papszOptions)
{
    if (strcmp(pszFilename, "/dev/stdout") == 0)
        pszFilename = "/vsistdout/";

    /* -------------------------------------------------------------------- */
    /*     Do not overwrite exiting file.                                   */
    /* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if (VSIStatL(pszFilename, &sStatBuf) == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "You have to delete %s before being able to create it with "
                 "the GPX driver",
                 pszFilename);
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */

    SetDescription(pszFilename);

    if (strcmp(pszFilename, "/vsistdout/") == 0)
    {
        m_bIsBackSeekable = false;
        m_fpOutput.reset(VSIFOpenL(pszFilename, "w"));
    }
    else
        m_fpOutput.reset(VSIFOpenL(pszFilename, "w+"));
    if (m_fpOutput == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Failed to create GPX file %s.",
                 pszFilename);
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      End of line character.                                          */
    /* -------------------------------------------------------------------- */
    const char *pszCRLFFormat = CSLFetchNameValue(papszOptions, "LINEFORMAT");

    bool bUseCRLF =
#ifdef _WIN32
        true
#else
        false
#endif
        ;
    if (pszCRLFFormat == nullptr)
    {
        // Use default value for OS.
    }
    else if (EQUAL(pszCRLFFormat, "CRLF"))
        bUseCRLF = true;
    else if (EQUAL(pszCRLFFormat, "LF"))
        bUseCRLF = false;
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                 pszCRLFFormat);
        // Use default value for OS.
    }
    m_pszEOL = (bUseCRLF) ? "\r\n" : "\n";

    /* -------------------------------------------------------------------- */
    /*      Look at use extensions options.                                 */
    /* -------------------------------------------------------------------- */
    const char *pszUseExtensions =
        CSLFetchNameValue(papszOptions, "GPX_USE_EXTENSIONS");
    const char *pszExtensionsNSURL = nullptr;
    if (pszUseExtensions && CPLTestBool(pszUseExtensions))
    {
        m_bUseExtensions = true;

        const char *pszExtensionsNSOption =
            CSLFetchNameValue(papszOptions, "GPX_EXTENSIONS_NS");
        const char *pszExtensionsNSURLOption =
            CSLFetchNameValue(papszOptions, "GPX_EXTENSIONS_NS_URL");
        if (pszExtensionsNSOption && pszExtensionsNSURLOption)
        {
            m_osExtensionsNS = pszExtensionsNSOption;
            pszExtensionsNSURL = pszExtensionsNSURLOption;
        }
        else
        {
            m_osExtensionsNS = "ogr";
            pszExtensionsNSURL = "http://osgeo.org/gdal";
        }
    }

    /* -------------------------------------------------------------------- */
    /*     Output header of GPX file.                                       */
    /* -------------------------------------------------------------------- */
    PrintLine("<?xml version=\"1.0\"?>");
    m_fpOutput->Printf("<gpx version=\"1.1\" creator=\"");
    const char *pszCreator = CSLFetchNameValue(papszOptions, "CREATOR");
    if (pszCreator)
    {
        char *pszXML = OGRGetXML_UTF8_EscapedString(pszCreator);
        m_fpOutput->Printf("%s", pszXML);
        CPLFree(pszXML);
    }
    else
    {
        m_fpOutput->Printf("GDAL %s", GDALVersionInfo("RELEASE_NAME"));
    }
    m_fpOutput->Printf(
        "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ");
    if (m_bUseExtensions)
        m_fpOutput->Printf("xmlns:%s=\"%s\" ", m_osExtensionsNS.c_str(),
                           pszExtensionsNSURL);
    m_fpOutput->Printf("xmlns=\"http://www.topografix.com/GPX/1/1\" ");
    PrintLine("xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 "
              "http://www.topografix.com/GPX/1/1/gpx.xsd\">");
    PrintLine("<metadata>");
    /*
    Something like:
        <metadata>
            <name>metadata name</name>
            <desc>metadata desc</desc>
            <author>
                <name>metadata author name</name>
                <email id="foo" domain="example.com"/>
                <link href="author_href"><text>author_text</text><type>author_type</type></link>
            </author>
            <copyright author="copyright author"><year>2023</year><license>my license</license></copyright>
            <link href="href"><text>text</text><type>type</type></link>
            <link href="href2"><text>text2</text><type>type2</type></link>
            <time>2007-11-25T17:58:00+01:00</time>
            <keywords>kw</keywords>
            <bounds minlat="-90" minlon="-180" maxlat="90" maxlon="179.9999999"/>
        </metadata>
    */

    if (const char *pszMetadataName =
            CSLFetchNameValue(papszOptions, "METADATA_NAME"))
    {
        char *pszXML = OGRGetXML_UTF8_EscapedString(pszMetadataName);
        PrintLine("  <name>%s</name>", pszXML);
        CPLFree(pszXML);
    }

    if (const char *pszMetadataDesc =
            CSLFetchNameValue(papszOptions, "METADATA_DESCRIPTION"))
    {
        char *pszXML = OGRGetXML_UTF8_EscapedString(pszMetadataDesc);
        PrintLine("  <desc>%s</desc>", pszXML);
        CPLFree(pszXML);
    }

    const char *pszMetadataAuthorName =
        CSLFetchNameValue(papszOptions, "METADATA_AUTHOR_NAME");
    const char *pszMetadataAuthorEmail =
        CSLFetchNameValue(papszOptions, "METADATA_AUTHOR_EMAIL");
    const char *pszMetadataAuthorLinkHref =
        CSLFetchNameValue(papszOptions, "METADATA_AUTHOR_LINK_HREF");
    if (pszMetadataAuthorName || pszMetadataAuthorEmail ||
        pszMetadataAuthorLinkHref)
    {
        PrintLine("  <author>");
        if (pszMetadataAuthorName)
        {
            char *pszXML = OGRGetXML_UTF8_EscapedString(pszMetadataAuthorName);
            PrintLine("    <name>%s</name>", pszXML);
            CPLFree(pszXML);
        }
        if (pszMetadataAuthorEmail)
        {
            std::string osEmail = pszMetadataAuthorEmail;
            auto nPos = osEmail.find('@');
            if (nPos != std::string::npos)
            {
                char *pszId = OGRGetXML_UTF8_EscapedString(
                    osEmail.substr(0, nPos).c_str());
                char *pszDomain = OGRGetXML_UTF8_EscapedString(
                    osEmail.substr(nPos + 1).c_str());
                PrintLine("    <email id=\"%s\" domain=\"%s\"/>", pszId,
                          pszDomain);
                CPLFree(pszId);
                CPLFree(pszDomain);
            }
        }
        if (pszMetadataAuthorLinkHref)
        {
            {
                char *pszXML =
                    OGRGetXML_UTF8_EscapedString(pszMetadataAuthorLinkHref);
                PrintLine("    <link href=\"%s\">", pszXML);
                CPLFree(pszXML);
            }
            if (const char *pszMetadataAuthorLinkText = CSLFetchNameValue(
                    papszOptions, "METADATA_AUTHOR_LINK_TEXT"))
            {
                char *pszXML =
                    OGRGetXML_UTF8_EscapedString(pszMetadataAuthorLinkText);
                PrintLine("      <text>%s</text>", pszXML);
                CPLFree(pszXML);
            }
            if (const char *pszMetadataAuthorLinkType = CSLFetchNameValue(
                    papszOptions, "METADATA_AUTHOR_LINK_TYPE"))
            {
                char *pszXML =
                    OGRGetXML_UTF8_EscapedString(pszMetadataAuthorLinkType);
                PrintLine("      <type>%s</type>", pszXML);
                CPLFree(pszXML);
            }
            PrintLine("    </link>");
        }
        PrintLine("  </author>");
    }

    if (const char *pszMetadataCopyrightAuthor =
            CSLFetchNameValue(papszOptions, "METADATA_COPYRIGHT_AUTHOR"))
    {
        {
            char *pszXML =
                OGRGetXML_UTF8_EscapedString(pszMetadataCopyrightAuthor);
            PrintLine("  <copyright author=\"%s\">", pszXML);
            CPLFree(pszXML);
        }
        if (const char *pszMetadataCopyrightYear =
                CSLFetchNameValue(papszOptions, "METADATA_COPYRIGHT_YEAR"))
        {
            char *pszXML =
                OGRGetXML_UTF8_EscapedString(pszMetadataCopyrightYear);
            PrintLine("      <year>%s</year>", pszXML);
            CPLFree(pszXML);
        }
        if (const char *pszMetadataCopyrightLicense =
                CSLFetchNameValue(papszOptions, "METADATA_COPYRIGHT_LICENSE"))
        {
            char *pszXML =
                OGRGetXML_UTF8_EscapedString(pszMetadataCopyrightLicense);
            PrintLine("      <license>%s</license>", pszXML);
            CPLFree(pszXML);
        }
        PrintLine("  </copyright>");
    }

    for (CSLConstList papszIter = papszOptions; papszIter && *papszIter;
         ++papszIter)
    {
        if (STARTS_WITH_CI(*papszIter, "METADATA_LINK_") &&
            strstr(*papszIter, "_HREF"))
        {
            const int nLinkNum = atoi(*papszIter + strlen("METADATA_LINK_"));
            const char *pszVal = strchr(*papszIter, '=');
            if (pszVal)
            {
                {
                    char *pszXML = OGRGetXML_UTF8_EscapedString(pszVal + 1);
                    PrintLine("  <link href=\"%s\">", pszXML);
                    CPLFree(pszXML);
                }
                if (const char *pszText = CSLFetchNameValue(
                        papszOptions,
                        CPLSPrintf("METADATA_LINK_%d_TEXT", nLinkNum)))
                {
                    char *pszXML = OGRGetXML_UTF8_EscapedString(pszText);
                    PrintLine("      <text>%s</text>", pszXML);
                    CPLFree(pszXML);
                }
                if (const char *pszType = CSLFetchNameValue(
                        papszOptions,
                        CPLSPrintf("METADATA_LINK_%d_TYPE", nLinkNum)))
                {
                    char *pszXML = OGRGetXML_UTF8_EscapedString(pszType);
                    PrintLine("      <type>%s</type>", pszXML);
                    CPLFree(pszXML);
                }
                PrintLine("  </link>");
            }
        }
    }

    if (const char *pszMetadataTime =
            CSLFetchNameValue(papszOptions, "METADATA_TIME"))
    {
        char *pszXML = OGRGetXML_UTF8_EscapedString(pszMetadataTime);
        PrintLine("  <time>%s</time>", pszXML);
        CPLFree(pszXML);
    }

    if (const char *pszMetadataKeywords =
            CSLFetchNameValue(papszOptions, "METADATA_KEYWORDS"))
    {
        char *pszXML = OGRGetXML_UTF8_EscapedString(pszMetadataKeywords);
        PrintLine("  <keywords>%s</keywords>", pszXML);
        CPLFree(pszXML);
    }

    if (m_bIsBackSeekable)
    {
        /* Reserve space for <bounds .../> within <metadata> */
        char szBounds[SPACE_FOR_METADATA_BOUNDS + 1];
        memset(szBounds, ' ', SPACE_FOR_METADATA_BOUNDS);
        szBounds[SPACE_FOR_METADATA_BOUNDS] = '\0';
        m_nOffsetBounds = m_fpOutput->Tell();
        PrintLine("%s", szBounds);
    }
    PrintLine("</metadata>");

    return TRUE;
}

/************************************************************************/
/*                             AddCoord()                               */
/************************************************************************/

void OGRGPXDataSource::AddCoord(double dfLon, double dfLat)
{
    m_dfMinLon = std::min(m_dfMinLon, dfLon);
    m_dfMinLat = std::min(m_dfMinLat, dfLat);
    m_dfMaxLon = std::max(m_dfMaxLon, dfLon);
    m_dfMaxLat = std::max(m_dfMaxLat, dfLat);
}

/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void OGRGPXDataSource::PrintLine(const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start(args, fmt);
    osWork.vPrintf(fmt, args);
    va_end(args);

    m_fpOutput->Write(osWork.c_str(), 1, osWork.size());
    m_fpOutput->Write(m_pszEOL, 1, strlen(m_pszEOL));
}
