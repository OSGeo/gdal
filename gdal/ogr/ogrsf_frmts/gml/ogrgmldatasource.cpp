/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRGMLDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 *
 ******************************************************************************
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module implementing GML_SKIP_RESOLVE_ELEMS HUGE
 * Developed for Faunalia ( http://www.faunalia.it) with funding from
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_gml.h"

#include <algorithm>
#include <vector>

#include "cpl_conv.h"
#include "cpl_http.h"
#include "cpl_string.h"
#include "cpl_vsi_error.h"
#include "gmlreaderp.h"
#include "gmlregistry.h"
#include "gmlutils.h"
#include "ogr_p.h"
#include "parsexsd.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                   ReplaceSpaceByPct20IfNeeded()                      */
/************************************************************************/

static CPLString ReplaceSpaceByPct20IfNeeded(const char *pszURL)
{
    // Replace ' ' by '%20'.
    CPLString osRet = pszURL;
    const char *pszNeedle = strstr(pszURL, "; ");
    if (pszNeedle)
    {
        char *pszTmp = static_cast<char *>(CPLMalloc(strlen(pszURL) + 2 + 1));
        const int nBeforeNeedle = static_cast<int>(pszNeedle - pszURL);
        memcpy(pszTmp, pszURL, nBeforeNeedle);
        strcpy(pszTmp + nBeforeNeedle, ";%20");
        strcpy(pszTmp + nBeforeNeedle + strlen(";%20"),
               pszNeedle + strlen("; "));
        osRet = pszTmp;
        CPLFree(pszTmp);
    }

    return osRet;
}

/************************************************************************/
/*                         OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::OGRGMLDataSource() :
    papoLayers(NULL),
    nLayers(0),
    pszName(NULL),
    papszCreateOptions(NULL),
    fpOutput(NULL),
    bFpOutputIsNonSeekable(false),
    bFpOutputSingleFile(false),
    bBBOX3D(false),
    nBoundedByLocation(-1),
    nSchemaInsertLocation(-1),
    bIsOutputGML3(false),
    bIsOutputGML3Deegree(false),
    bIsOutputGML32(false),
    eSRSNameFormat(SRSNAME_SHORT),
    bWriteSpaceIndentation(true),
    poWriteGlobalSRS(NULL),
    bWriteGlobalSRS(false),
    poReader(NULL),
    bOutIsTempFile(false),
    bExposeGMLId(false),
    bExposeFid(false),
    bIsWFS(false),
    bUseGlobalSRSName(false),
    m_bInvertAxisOrderIfLatLong(false),
    m_bConsiderEPSGAsURN(false),
    m_eSwapCoordinates(GML_SWAP_AUTO),
    m_bGetSecondaryGeometryOption(false),
    eReadMode(STANDARD),
    poStoredGMLFeature(NULL),
    poLastReadLayer(NULL),
    bEmptyAsNull(true)
{}

/************************************************************************/
/*                        ~OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::~OGRGMLDataSource()
{
    if( fpOutput != NULL )
    {
        if( nLayers == 0 )
            WriteTopElements();

        const char *pszPrefix = GetAppPrefix();
        if( RemoveAppPrefix() )
            PrintLine(fpOutput, "</FeatureCollection>");
        else
            PrintLine(fpOutput, "</%s:FeatureCollection>", pszPrefix);

        if( bFpOutputIsNonSeekable)
        {
            VSIFCloseL(fpOutput);
            fpOutput = NULL;
        }

        InsertHeader();

        if( !bFpOutputIsNonSeekable
            && nBoundedByLocation != -1
            && VSIFSeekL(fpOutput, nBoundedByLocation, SEEK_SET) == 0 )
        {
            if (bWriteGlobalSRS && sBoundingRect.IsInit()  && IsGML3Output())
            {
                bool bCoordSwap = false;
                char *pszSRSName = poWriteGlobalSRS
                    ? GML_GetSRSName(
                        poWriteGlobalSRS, eSRSNameFormat, &bCoordSwap)
                    : CPLStrdup("");
                char szLowerCorner[75] = {};
                char szUpperCorner[75] = {};
                if (bCoordSwap)
                {
                    OGRMakeWktCoordinate(
                        szLowerCorner, sBoundingRect.MinY, sBoundingRect.MinX,
                        sBoundingRect.MinZ, bBBOX3D ? 3 : 2);
                    OGRMakeWktCoordinate(
                        szUpperCorner, sBoundingRect.MaxY, sBoundingRect.MaxX,
                        sBoundingRect.MaxZ, bBBOX3D ? 3 : 2);
                }
                else
                {
                    OGRMakeWktCoordinate(
                        szLowerCorner, sBoundingRect.MinX, sBoundingRect.MinY,
                        sBoundingRect.MinZ, bBBOX3D ? 3 : 2);
                    OGRMakeWktCoordinate(
                        szUpperCorner, sBoundingRect.MaxX, sBoundingRect.MaxY,
                        sBoundingRect.MaxZ, (bBBOX3D) ? 3 : 2);
                }
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "  ");
                PrintLine(
                    fpOutput,
                    "<gml:boundedBy><gml:Envelope%s%s><gml:lowerCorner>%s"
                    "</gml:lowerCorner><gml:upperCorner>%s</gml:upperCorner>"
                    "</gml:Envelope></gml:boundedBy>",
                    bBBOX3D ? " srsDimension=\"3\"" : "", pszSRSName,
                    szLowerCorner, szUpperCorner);
                CPLFree(pszSRSName);
            }
            else if (bWriteGlobalSRS && sBoundingRect.IsInit())
            {
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "  ");
                PrintLine(fpOutput, "<gml:boundedBy>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "    ");
                PrintLine(fpOutput, "<gml:Box>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "      ");
                VSIFPrintfL(fpOutput,
                            "<gml:coord><gml:X>%.16g</gml:X>"
                            "<gml:Y>%.16g</gml:Y>",
                            sBoundingRect.MinX, sBoundingRect.MinY);
                if (bBBOX3D)
                    VSIFPrintfL(fpOutput, "<gml:Z>%.16g</gml:Z>",
                                sBoundingRect.MinZ);
                PrintLine(fpOutput, "</gml:coord>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "      ");
                VSIFPrintfL(fpOutput,
                            "<gml:coord><gml:X>%.16g</gml:X>"
                            "<gml:Y>%.16g</gml:Y>",
                            sBoundingRect.MaxX, sBoundingRect.MaxY);
                if (bBBOX3D)
                    VSIFPrintfL(fpOutput, "<gml:Z>%.16g</gml:Z>",
                                sBoundingRect.MaxZ);
                PrintLine(fpOutput, "</gml:coord>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "    ");
                PrintLine(fpOutput, "</gml:Box>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "  ");
                PrintLine(fpOutput, "</gml:boundedBy>");
            }
            else
            {
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fpOutput, "  ");
                if (IsGML3Output())
                    PrintLine(fpOutput,
                              "<gml:boundedBy><gml:Null /></gml:boundedBy>");
                else
                    PrintLine(fpOutput,
                              "<gml:boundedBy><gml:null>missing"
                              "</gml:null></gml:boundedBy>");
            }
        }

        if (fpOutput)
            VSIFCloseL(fpOutput);
    }

    CSLDestroy(papszCreateOptions);
    CPLFree(pszName);

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree(papoLayers);

    if( poReader )
    {
        if (bOutIsTempFile)
            VSIUnlink(poReader->GetSourceFileName());
        delete poReader;
    }

    delete poWriteGlobalSRS;

    delete poStoredGMLFeature;

    if (osXSDFilename.compare(
            CPLSPrintf("/vsimem/tmp_gml_xsd_%p.xsd", this)) == 0)
        VSIUnlink(osXSDFilename);
}

/************************************************************************/
/*                            CheckHeader()                             */
/************************************************************************/

bool OGRGMLDataSource::CheckHeader(const char *pszStr)
{
    if( strstr(pszStr, "opengis.net/gml") == NULL &&
        strstr(pszStr, "<csw:GetRecordsResponse") == NULL )
    {
        return false;
    }

    // Ignore .xsd schemas.
    if( strstr(pszStr, "<schema") != NULL ||
        strstr(pszStr, "<xs:schema") != NULL ||
        strstr(pszStr, "<xsd:schema") != NULL )
    {
        return false;
    }

    // Ignore GeoRSS documents. They will be recognized by the GeoRSS driver.
    if( strstr(pszStr, "<rss") != NULL &&
        strstr(pszStr, "xmlns:georss") != NULL )
    {
        return false;
    }

    // Ignore OpenJUMP .jml documents.
    // They will be recognized by the OpenJUMP driver.
    if( strstr(pszStr, "<JCSDataFile") != NULL )
    {
        return false;
    }

    // Ignore OGR WFS xml description files, or WFS Capabilities results.
    if( strstr(pszStr, "<OGRWFSDataSource>") != NULL ||
        strstr(pszStr, "<wfs:WFS_Capabilities") != NULL )
    {
        return false;
    }

    // Ignore WMTS capabilities results.
    if( strstr(pszStr, "http://www.opengis.net/wmts/1.0") != NULL )
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                          ExtractSRSName()                            */
/************************************************************************/

static bool ExtractSRSName(const char *pszXML, char *szSRSName,
                           size_t sizeof_szSRSName)
{
    szSRSName[0] = '\0';

    const char *pszSRSName = strstr(pszXML, "srsName=\"");
    if( pszSRSName != NULL )
    {
        pszSRSName += 9;
        const char *pszEndQuote = strchr(pszSRSName, '"');
        if (pszEndQuote != NULL &&
            static_cast<size_t>(pszEndQuote - pszSRSName) < sizeof_szSRSName)
        {
            memcpy(szSRSName, pszSRSName, pszEndQuote - pszSRSName);
            szSRSName[pszEndQuote - pszSRSName] = '\0';
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRGMLDataSource::Open( GDALOpenInfo *poOpenInfo )

{
    // Extract XSD filename from connection string if present.
    osFilename = poOpenInfo->pszFilename;
    const char *pszXSDFilenameTmp = strstr(poOpenInfo->pszFilename, ",xsd=");
    if (pszXSDFilenameTmp != NULL)
    {
        osFilename.resize(pszXSDFilenameTmp - poOpenInfo->pszFilename);
        osXSDFilename = pszXSDFilenameTmp + strlen(",xsd=");
    }
    else
    {
        osXSDFilename =
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "XSD", "");
    }

    const char *pszFilename = osFilename.c_str();

    pszName = CPLStrdup(poOpenInfo->pszFilename);

    // Open the source file.
    VSILFILE *fpToClose = NULL;
    VSILFILE *fp = NULL;
    if( poOpenInfo->fpL != NULL )
    {
        fp = poOpenInfo->fpL;
        VSIFSeekL(fp, 0, SEEK_SET);
    }
    else
    {
        fp = VSIFOpenL(pszFilename, "r");
        if( fp == NULL )
            return false;
        fpToClose = fp;
    }

    // Load a header chunk and check for signs it is GML.
    char szHeader[4096] = {};
    size_t nRead = VSIFReadL(szHeader, 1, sizeof(szHeader) - 1, fp);
    if (nRead == 0)
    {
        if( fpToClose )
            VSIFCloseL(fpToClose);
        return false;
    }
    szHeader[nRead] = '\0';

    CPLString osWithVsiGzip;

    // Might be a OS-Mastermap gzipped GML, so let be nice and try to open
    // it transparently with /vsigzip/.
    if ( ((GByte*)szHeader)[0] == 0x1f && ((GByte*)szHeader)[1] == 0x8b &&
            EQUAL(CPLGetExtension(pszFilename), "gz") &&
            !STARTS_WITH(pszFilename, "/vsigzip/") )
    {
        if( fpToClose )
            VSIFCloseL(fpToClose);
        fpToClose = NULL;
        osWithVsiGzip = "/vsigzip/";
        osWithVsiGzip += pszFilename;

        pszFilename = osWithVsiGzip;

        fp = fpToClose = VSIFOpenL(pszFilename, "r");
        if( fp == NULL )
            return false;

        nRead = VSIFReadL(szHeader, 1, sizeof(szHeader) - 1, fp);
        if (nRead == 0)
        {
            VSIFCloseL(fpToClose);
            return false;
        }
        szHeader[nRead] = '\0';
    }

    // Check for a UTF-8 BOM and skip if found.

    // TODO: BOM is variable-length parameter and depends on encoding. */
    // Add BOM detection for other encodings.

    // Used to skip to actual beginning of XML data
    char *szPtr = szHeader;

    if( (static_cast<unsigned char>(szHeader[0]) == 0xEF) &&
        (static_cast<unsigned char>(szHeader[1]) == 0xBB) &&
        (static_cast<unsigned char>(szHeader[2]) == 0xBF) )
    {
        szPtr += 3;
    }

    bool bExpatCompatibleEncoding = false;

    const char *pszEncoding = strstr(szPtr, "encoding=");
    if (pszEncoding)
        bExpatCompatibleEncoding =
            (pszEncoding[9] == '\'' || pszEncoding[9] == '"') &&
            (STARTS_WITH_CI(pszEncoding + 10, "UTF-8") ||
             STARTS_WITH_CI(pszEncoding + 10, "ISO-8859-15") ||
             (STARTS_WITH_CI(pszEncoding + 10, "ISO-8859-1") &&
              pszEncoding[20] == pszEncoding[9]));
    else
        bExpatCompatibleEncoding = true;  // utf-8 is the default.

    const bool bHas3D = strstr(szPtr, "srsDimension=\"3\"") != NULL ||
                        strstr(szPtr, "<gml:Z>") != NULL;

    // Here, we expect the opening chevrons of GML tree root element.
    if( szPtr[0] != '<' || !CheckHeader(szPtr) )
    {
        if( fpToClose )
            VSIFCloseL(fpToClose);
        return false;
    }

    // Now we definitely own the file descriptor.
    if( fp == poOpenInfo->fpL )
        poOpenInfo->fpL = NULL;

    // Small optimization: if we parse a <wfs:FeatureCollection> and
    // that numberOfFeatures is set, we can use it to set the FeatureCount
    // but *ONLY* if there's just one class.
    const char *pszFeatureCollection = strstr(szPtr, "wfs:FeatureCollection");
    if (pszFeatureCollection == NULL)
        // GML 3.2.1 output.
        pszFeatureCollection = strstr(szPtr, "gml:FeatureCollection");
    if (pszFeatureCollection == NULL)
    {
        // Deegree WFS 1.0.0 output.
        pszFeatureCollection = strstr(szPtr, "<FeatureCollection");
        if (pszFeatureCollection &&
            strstr(szPtr, "xmlns:wfs=\"http://www.opengis.net/wfs\"") == NULL)
            pszFeatureCollection = NULL;
    }

    GIntBig nNumberOfFeatures = 0;
    if (pszFeatureCollection)
    {
        bExposeGMLId = true;
        bIsWFS = true;
        const char *pszNumberOfFeatures = strstr(szPtr, "numberOfFeatures=");
        if (pszNumberOfFeatures)
        {
            pszNumberOfFeatures += 17;
            char ch = pszNumberOfFeatures[0];
            if ((ch == '\'' || ch == '"') &&
                strchr(pszNumberOfFeatures + 1, ch) != NULL)
            {
                nNumberOfFeatures = CPLAtoGIntBig(pszNumberOfFeatures + 1);
            }
        }
        else if ((pszNumberOfFeatures = strstr(szPtr, "numberReturned=")) !=
                 NULL)
        {
            // WFS 2.0.0
            pszNumberOfFeatures += 15;
            char ch = pszNumberOfFeatures[0];
            if ((ch == '\'' || ch == '"') &&
                strchr(pszNumberOfFeatures + 1, ch) != NULL)
            {
                // 'unknown' might be a valid value in a corrected version of
                // WFS 2.0 but it will also evaluate to 0, that is considered as
                // unknown, so nothing particular to do.
                nNumberOfFeatures = CPLAtoGIntBig(pszNumberOfFeatures + 1);
            }
        }
    }
    else if (STARTS_WITH(pszFilename, "/vsimem/tempwfs_"))
    {
        // http://regis.intergraph.com/wfs/dcmetro/request.asp? returns a
        // <G:FeatureCollection> Who knows what servers can return?  When
        // in the context of the WFS driver always expose the gml:id to avoid
        // later crashes.
        bExposeGMLId = true;
        bIsWFS = true;
    }
    else
    {
        bExposeGMLId = strstr(szPtr, " gml:id=\"") != NULL ||
                       strstr(szPtr, " gml:id='") != NULL;
        bExposeFid = strstr(szPtr, " fid=\"") != NULL ||
                     strstr(szPtr, " fid='") != NULL;
    }

    const char *pszExposeGMLId =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "EXPOSE_GML_ID",
                             CPLGetConfigOption("GML_EXPOSE_GML_ID", NULL));
    if (pszExposeGMLId)
        bExposeGMLId = CPLTestBool(pszExposeGMLId);

    const char *pszExposeFid =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "EXPOSE_FID",
                             CPLGetConfigOption("GML_EXPOSE_FID", NULL));
    if (pszExposeFid)
        bExposeFid = CPLTestBool(pszExposeFid);

    const bool bHintConsiderEPSGAsURN =
        strstr(szPtr, "xmlns:fme=\"http://www.safe.com/gml/fme\"") != NULL;

    char szSRSName[128] = {};
    bool bAnalyzeSRSPerFeature = true;

    // MTKGML.
    if( strstr(szPtr, "<Maastotiedot") != NULL )
    {
        if( strstr(
                szPtr,
                "http://xml.nls.fi/XML/Namespace/"
                "Maastotietojarjestelma/SiirtotiedostonMalli/2011-02") == NULL )
            CPLDebug(
                "GML",
                "Warning: a MTKGML file was detected, "
                "but its namespace is unknown");
        bAnalyzeSRSPerFeature = false;
        bUseGlobalSRSName = true;
        if( !ExtractSRSName(szPtr, szSRSName, sizeof(szSRSName)) )
            strcpy(szSRSName, "EPSG:3067");
    }

    const char *pszSchemaLocation = strstr(szPtr, "schemaLocation=");
    if (pszSchemaLocation)
        pszSchemaLocation += strlen("schemaLocation=");

    bool bCheckAuxFile = true;
    if (STARTS_WITH(pszFilename, "/vsicurl_streaming/"))
        bCheckAuxFile = false;
    else if (STARTS_WITH(pszFilename, "/vsicurl/") &&
             (strstr(pszFilename, "?SERVICE=") ||
              strstr(pszFilename, "&SERVICE=")) )
        bCheckAuxFile = false;

    bool bIsWFSJointLayer = bIsWFS && strstr(szPtr, "<wfs:Tuple>");
    if( bIsWFSJointLayer )
        bExposeGMLId = false;

    // We assume now that it is GML.  Instantiate a GMLReader on it.
    const char *pszReadMode =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "READ_MODE",
                             CPLGetConfigOption("GML_READ_MODE", "AUTO"));
    if( EQUAL(pszReadMode, "AUTO") )
        pszReadMode = NULL;
    if (pszReadMode == NULL || EQUAL(pszReadMode, "STANDARD"))
        eReadMode = STANDARD;
    else if (EQUAL(pszReadMode, "SEQUENTIAL_LAYERS"))
        eReadMode = SEQUENTIAL_LAYERS;
    else if (EQUAL(pszReadMode, "INTERLEAVED_LAYERS"))
        eReadMode = INTERLEAVED_LAYERS;
    else
    {
        CPLDebug("GML",
                 "Unrecognized value for GML_READ_MODE configuration option.");
    }

    m_bInvertAxisOrderIfLatLong = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "INVERT_AXIS_ORDER_IF_LAT_LONG",
        CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES")));

    const char *pszConsiderEPSGAsURN = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "CONSIDER_EPSG_AS_URN",
        CPLGetConfigOption("GML_CONSIDER_EPSG_AS_URN", "AUTO"));
    if( !EQUAL(pszConsiderEPSGAsURN, "AUTO") )
        m_bConsiderEPSGAsURN = CPLTestBool(pszConsiderEPSGAsURN);
    else if (bHintConsiderEPSGAsURN)
    {
        // GML produced by FME (at least CanVec GML) seem to honour EPSG axis
        // ordering.
        CPLDebug("GML",
                 "FME-produced GML --> "
                 "consider that GML_CONSIDER_EPSG_AS_URN is set to YES");
        m_bConsiderEPSGAsURN = true;
    }
    else
    {
        m_bConsiderEPSGAsURN = false;
    }

    const char *pszSwapCoordinates = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "SWAP_COORDINATES",
        CPLGetConfigOption("GML_SWAP_COORDINATES", "AUTO"));
    m_eSwapCoordinates =
        EQUAL(pszSwapCoordinates, "AUTO")
            ? GML_SWAP_AUTO
            : CPLTestBool(pszSwapCoordinates) ? GML_SWAP_YES : GML_SWAP_NO;

    m_bGetSecondaryGeometryOption =
        CPLTestBool(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));

    // EXPAT is faster than Xerces, so when it is safe to use it, use it!
    // The only interest of Xerces is for rare encodings that Expat doesn't
    // handle, but UTF-8 is well handled by Expat.
    bool bUseExpatParserPreferably = bExpatCompatibleEncoding;

    // Override default choice.
    const char *pszGMLParser = CPLGetConfigOption("GML_PARSER", NULL);
    if (pszGMLParser)
    {
        if (EQUAL(pszGMLParser, "EXPAT"))
            bUseExpatParserPreferably = true;
        else if (EQUAL(pszGMLParser, "XERCES"))
            bUseExpatParserPreferably = false;
    }

    poReader = CreateGMLReader(bUseExpatParserPreferably,
                               m_bInvertAxisOrderIfLatLong,
                               m_bConsiderEPSGAsURN,
                               m_eSwapCoordinates,
                               m_bGetSecondaryGeometryOption);
    if( poReader == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be GML but the GML reader can't\n"
                 "be instantiated, likely because Xerces or Expat support was\n"
                 "not configured in.",
                 pszFilename);
        VSIFCloseL(fp);
        return false;
    }

    poReader->SetSourceFile(pszFilename);
    static_cast<GMLReader *>(poReader)->SetIsWFSJointLayer(bIsWFSJointLayer);
    bEmptyAsNull =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "EMPTY_AS_NULL", true);
    static_cast<GMLReader *>(poReader)->SetEmptyAsNull(bEmptyAsNull);
    static_cast<GMLReader *>(poReader)->SetReportAllAttributes(CPLFetchBool(
        poOpenInfo->papszOpenOptions, "GML_ATTRIBUTES_TO_OGR_FIELDS",
        CPLTestBool(CPLGetConfigOption("GML_ATTRIBUTES_TO_OGR_FIELDS", "NO"))));

    // Find <gml:description>, <gml:name> and <gml:boundedBy>
    FindAndParseTopElements(fp);

    if( szSRSName[0] != '\0' )
        poReader->SetGlobalSRSName(szSRSName);

    // Resolve the xlinks in the source file and save it with the
    // extension ".resolved.gml". The source file will to set to that.
    char *pszXlinkResolvedFilename = NULL;
    const char *pszOption = CPLGetConfigOption("GML_SAVE_RESOLVED_TO", NULL);
    bool bResolve = true;
    bool bHugeFile = false;
    if( pszOption != NULL && STARTS_WITH_CI(pszOption, "SAME") )
    {
        // "SAME" will overwrite the existing gml file.
        pszXlinkResolvedFilename = CPLStrdup(pszFilename);
    }
    else if( pszOption != NULL &&
             CPLStrnlen(pszOption, 5) >= 5 &&
             STARTS_WITH_CI(pszOption - 4 + strlen(pszOption), ".gml") )
    {
        // Any string ending with ".gml" will try and write to it.
        pszXlinkResolvedFilename = CPLStrdup(pszOption);
    }
    else
    {
        // When no option is given or is not recognised,
        // use the same file name with the extension changed to .resolved.gml
        pszXlinkResolvedFilename =
            CPLStrdup(CPLResetExtension(pszFilename, "resolved.gml"));

        // Check if the file already exists.
        VSIStatBufL sResStatBuf, sGMLStatBuf;
        if( bCheckAuxFile && VSIStatL(pszXlinkResolvedFilename, &sResStatBuf) == 0 )
        {
            if( VSIStatL(pszFilename, &sGMLStatBuf) == 0 &&
                sGMLStatBuf.st_mtime > sResStatBuf.st_mtime )
            {
                CPLDebug("GML",
                         "Found %s but ignoring because it appears\n"
                         "be older than the associated GML file.",
                         pszXlinkResolvedFilename);
            }
            else
            {
                poReader->SetSourceFile(pszXlinkResolvedFilename);
                bResolve = false;
            }
        }
    }

    const char *pszSkipOption =
        CPLGetConfigOption("GML_SKIP_RESOLVE_ELEMS", "ALL");
    char **papszSkip = NULL;
    if( EQUAL(pszSkipOption, "ALL") )
        bResolve = false;
    else if( EQUAL(pszSkipOption, "HUGE") )
        // Exactly as NONE, but intended for HUGE files
        bHugeFile = true;
    else if( !EQUAL(pszSkipOption, "NONE") )  // Use this to resolve everything.
        papszSkip = CSLTokenizeString2(
            pszSkipOption, ",", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
    bool bHaveSchema = false;
    bool bSchemaDone = false;

    // Is some GML Feature Schema (.gfs) TEMPLATE required?
    const char *pszGFSTemplateName =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "GFS_TEMPLATE",
                             CPLGetConfigOption("GML_GFS_TEMPLATE", NULL));
    if( pszGFSTemplateName != NULL )
    {
        // Attempt to load the GFS TEMPLATE.
        bHaveSchema = poReader->LoadClasses(pszGFSTemplateName);
    }

    if( bResolve )
    {
        if ( bHugeFile )
        {
            bSchemaDone = true;
            bool bSqliteIsTempFile =
                CPLTestBool(CPLGetConfigOption("GML_HUGE_TEMPFILE", "YES"));
            int iSqliteCacheMB =
                atoi(CPLGetConfigOption("OGR_SQLITE_CACHE", "0"));
            if( poReader->HugeFileResolver(pszXlinkResolvedFilename,
                                           bSqliteIsTempFile,
                                           iSqliteCacheMB) == false )
            {
                // Assume an error has been reported.
                VSIFCloseL(fp);
                CPLFree(pszXlinkResolvedFilename);
                return false;
            }
        }
        else
        {
            poReader->ResolveXlinks(pszXlinkResolvedFilename,
                                    &bOutIsTempFile,
                                    papszSkip);
        }
    }

    CPLFree(pszXlinkResolvedFilename);
    pszXlinkResolvedFilename = NULL;
    CSLDestroy(papszSkip);
    papszSkip = NULL;

    // If the source filename for the reader is still the GML filename, then
    // we can directly provide the file pointer. Otherwise close it.
    if (strcmp(poReader->GetSourceFileName(), pszFilename) == 0)
        poReader->SetFP(fp);
    else
        VSIFCloseL(fp);
    fp = NULL;

    // Is a prescan required?
    if( bHaveSchema && !bSchemaDone )
    {
        // We must detect which layers are actually present in the .gml
        // and how many features they have.
        if( !poReader->PrescanForTemplate() )
        {
            // Assume an error has been reported.
            return false;
        }
    }

    CPLString osGFSFilename = CPLResetExtension(pszFilename, "gfs");
    if (STARTS_WITH(osGFSFilename, "/vsigzip/"))
        osGFSFilename = osGFSFilename.substr(strlen("/vsigzip/"));

    // Can we find a GML Feature Schema (.gfs) for the input file?
    if( !bHaveSchema && osXSDFilename.empty())
    {
        VSIStatBufL sGFSStatBuf;
        if( bCheckAuxFile && VSIStatL(osGFSFilename, &sGFSStatBuf) == 0 )
        {
            VSIStatBufL sGMLStatBuf;
            if( VSIStatL(pszFilename, &sGMLStatBuf) == 0 &&
                sGMLStatBuf.st_mtime > sGFSStatBuf.st_mtime )
            {
                CPLDebug("GML",
                         "Found %s but ignoring because it appears\n"
                         "be older than the associated GML file.",
                         osGFSFilename.c_str());
            }
            else
            {
                bHaveSchema = poReader->LoadClasses(osGFSFilename);
                if (bHaveSchema)
                {
                    pszXSDFilenameTmp = CPLResetExtension(pszFilename, "xsd");
                    if( VSIStatExL(pszXSDFilenameTmp, &sGMLStatBuf,
                                   VSI_STAT_EXISTS_FLAG) == 0 )
                    {
                        CPLDebug("GML", "Using %s file, ignoring %s",
                                 osGFSFilename.c_str(), pszXSDFilenameTmp);
                    }
                }
            }
        }
    }

    // Can we find an xsd which might conform to tbe GML3 Level 0
    // profile?  We really ought to look for it based on the rules
    // schemaLocation in the GML feature collection but for now we
    // just hopes it is in the same director with the same name.

    bool bHasFoundXSD = false;

    if( !bHaveSchema )
    {
        char **papszTypeNames = NULL;

        VSIStatBufL sXSDStatBuf;
        if (osXSDFilename.empty())
        {
            osXSDFilename = CPLResetExtension(pszFilename, "xsd");
            if( bCheckAuxFile && VSIStatExL(osXSDFilename, &sXSDStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
            {
                bHasFoundXSD = true;
            }
        }
        else
        {
            if ( STARTS_WITH(osXSDFilename, "http://") ||
                 STARTS_WITH(osXSDFilename, "https://") ||
                 VSIStatExL(osXSDFilename, &sXSDStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
            {
                bHasFoundXSD = true;
            }
        }

        // If not found, try if there is a schema in the gml_registry.xml
        // that might match a declared namespace and featuretype.
        if( !bHasFoundXSD )
        {
            GMLRegistry oRegistry(
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "REGISTRY",
                                     CPLGetConfigOption("GML_REGISTRY", "")));
            if( oRegistry.Parse() )
            {
                CPLString osHeader(szHeader);
                for( size_t iNS = 0; iNS < oRegistry.aoNamespaces.size(); iNS++ )
                {
                    GMLRegistryNamespace &oNamespace =
                        oRegistry.aoNamespaces[iNS];
                    // When namespace is omitted or fit with case sensitive match for
                    // name space prefix, then go next to find feature match.
                    //
                    // Case sensitive comparison since below test that also
                    // uses the namespace prefix is case sensitive.
                    if( !oNamespace.osPrefix.empty() &&
                        osHeader.find(CPLSPrintf("xmlns:%s",
                            oNamespace.osPrefix.c_str()))
                              == std::string::npos )
                    {
                        // namespace does not match with one of registry definition.
                        // go to next entry.
                        continue;
                    }

                    const char *pszURIToFind =
                        CPLSPrintf("\"%s\"", oNamespace.osURI.c_str());
                    if( strstr(szHeader, pszURIToFind) != NULL )
                    {
                        if( oNamespace.bUseGlobalSRSName )
                            bUseGlobalSRSName = true;

                        for( size_t iTypename = 0;
                             iTypename < oNamespace.aoFeatureTypes.size();
                             iTypename++ )
                        {
                            const char *pszElementToFind = NULL;

                            GMLRegistryFeatureType &oFeatureType =
                                oNamespace.aoFeatureTypes[iTypename];

                            if( !oNamespace.osPrefix.empty() )
                            {
                                if ( !oFeatureType.osElementValue.empty() )
                                    pszElementToFind = CPLSPrintf(
                                        "%s:%s>%s", oNamespace.osPrefix.c_str(),
                                        oFeatureType.osElementName.c_str(),
                                        oFeatureType.osElementValue.c_str());
                                else
                                    pszElementToFind = CPLSPrintf(
                                        "%s:%s", oNamespace.osPrefix.c_str(),
                                        oFeatureType.osElementName.c_str());
                            }
                            else
                            {
                                if ( !oFeatureType.osElementValue.empty() )
                                    pszElementToFind = CPLSPrintf("%s>%s",
                                                                  oFeatureType.osElementName.c_str(),
                                                                  oFeatureType.osElementValue.c_str());
                                else
                                    pszElementToFind = CPLSPrintf("<%s",
                                                                  oFeatureType.osElementName.c_str());
                            }


                            // Case sensitive test since in a CadastralParcel
                            // feature there is a property basicPropertyUnit
                            // xlink, not to be confused with a top-level
                            // BasicPropertyUnit feature.
                            if( osHeader.find(pszElementToFind) !=
                                std::string::npos )
                            {
                                if( !oFeatureType.osSchemaLocation.empty() )
                                {
                                    osXSDFilename =
                                        oFeatureType.osSchemaLocation;
                                    if( STARTS_WITH(osXSDFilename, "http://") ||
                                        STARTS_WITH(osXSDFilename, "https://") ||
                                        VSIStatExL(osXSDFilename, &sXSDStatBuf,
                                                   VSI_STAT_EXISTS_FLAG) == 0 )
                                    {
                                        bHasFoundXSD = true;
                                        bHaveSchema = true;
                                        CPLDebug(
                                            "GML",
                                            "Found %s for %s:%s in registry",
                                            osXSDFilename.c_str(),
                                            oNamespace.osPrefix.c_str(),
                                            oFeatureType.osElementName.c_str());
                                    }
                                    else
                                    {
                                        CPLDebug("GML", "Cannot open %s",
                                                 osXSDFilename.c_str());
                                    }
                                }
                                else
                                {
                                    bHaveSchema = poReader->LoadClasses(
                                        oFeatureType.osGFSSchemaLocation);
                                    if( bHaveSchema )
                                    {
                                        CPLDebug("GML", "Found %s for %s:%s in registry",
                                                oFeatureType.osGFSSchemaLocation.c_str(),
                                                oNamespace.osPrefix.c_str(),
                                                oFeatureType.osElementName.c_str());
                                    }
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        /* For WFS, try to fetch the application schema */
        if( bIsWFS && !bHaveSchema && pszSchemaLocation != NULL &&
            (pszSchemaLocation[0] == '\'' || pszSchemaLocation[0] == '"') &&
             strchr(pszSchemaLocation + 1, pszSchemaLocation[0]) != NULL )
        {
            char *pszSchemaLocationTmp1 = CPLStrdup(pszSchemaLocation + 1);
            int nTruncLen = static_cast<int>(
                strchr(pszSchemaLocation + 1, pszSchemaLocation[0]) -
                (pszSchemaLocation + 1));
            pszSchemaLocationTmp1[nTruncLen] = '\0';
            char *pszSchemaLocationTmp2 =
                CPLUnescapeString(pszSchemaLocationTmp1, NULL, CPLES_XML);
            CPLString osEscaped =
                ReplaceSpaceByPct20IfNeeded(pszSchemaLocationTmp2);
            CPLFree(pszSchemaLocationTmp2);
            pszSchemaLocationTmp2 = CPLStrdup(osEscaped);
            if (pszSchemaLocationTmp2)
            {
                // pszSchemaLocationTmp2 is of the form:
                // http://namespace1 http://namespace1_schema_location http://namespace2 http://namespace1_schema_location2
                // So we try to find http://namespace1_schema_location that
                // contains hints that it is the WFS application */ schema,
                // i.e. if it contains typename= and
                // request=DescribeFeatureType.
                char **papszTokens =
                    CSLTokenizeString2(pszSchemaLocationTmp2, " \r\n", 0);
                int nTokens = CSLCount(papszTokens);
                if ((nTokens % 2) == 0)
                {
                    for(int i = 0; i < nTokens; i += 2)
                    {
                        const char *pszEscapedURL = papszTokens[i + 1];
                        char *pszLocation =
                            CPLUnescapeString(pszEscapedURL, NULL, CPLES_URL);
                        CPLString osLocation = pszLocation;
                        CPLFree(pszLocation);
                        if (osLocation.ifind("typename=") != std::string::npos &&
                            osLocation.ifind("request=DescribeFeatureType") !=
                                std::string::npos)
                        {
                            CPLString osTypeName =
                                CPLURLGetValue(osLocation, "typename");
                            papszTypeNames =
                                CSLTokenizeString2(osTypeName, ",", 0);

                            if (!bHasFoundXSD && CPLHTTPEnabled() &&
                                CPLFetchBool(
                                    poOpenInfo->papszOpenOptions,
                                    "DOWNLOAD_SCHEMA",
                                    CPLTestBool(CPLGetConfigOption(
                                        "GML_DOWNLOAD_WFS_SCHEMA", "YES"))))
                            {
                                CPLHTTPResult *psResult =
                                    CPLHTTPFetch(pszEscapedURL, NULL);
                                if (psResult)
                                {
                                    if (psResult->nStatus == 0 &&
                                        psResult->pabyData != NULL)
                                    {
                                        bHasFoundXSD = true;
                                        osXSDFilename = CPLSPrintf(
                                            "/vsimem/tmp_gml_xsd_%p.xsd", this);
                                        VSILFILE *fpMem = VSIFileFromMemBuffer(
                                            osXSDFilename, psResult->pabyData,
                                            psResult->nDataLen, TRUE);
                                        VSIFCloseL(fpMem);
                                        psResult->pabyData = NULL;
                                    }
                                    CPLHTTPDestroyResult(psResult);
                                }
                            }
                            break;
                        }
                    }
                }
                CSLDestroy(papszTokens);
            }
            CPLFree(pszSchemaLocationTmp2);
            CPLFree(pszSchemaLocationTmp1);
        }

        bool bHasFeatureProperties = false;
        if( bHasFoundXSD )
        {
            std::vector<GMLFeatureClass *> aosClasses;
            bool bFullyUnderstood = false;
            bHaveSchema =
                GMLParseXSD(osXSDFilename, aosClasses, bFullyUnderstood);

            if( bHaveSchema && !bFullyUnderstood && bIsWFSJointLayer )
            {
                CPLDebug("GML",
                         "Schema found, but only partially understood. "
                         "Cannot be used in a WFS join context");

                std::vector<GMLFeatureClass *>::const_iterator oIter =
                    aosClasses.begin();
                std::vector<GMLFeatureClass *>::const_iterator oEndIter =
                    aosClasses.end();
                while (oIter != oEndIter)
                {
                    GMLFeatureClass *poClass = *oIter;

                    delete poClass;
                    ++oIter;
                }
                aosClasses.resize(0);
                bHaveSchema = false;
            }

            if( bHaveSchema )
            {
                CPLDebug("GML", "Using %s", osXSDFilename.c_str());
                std::vector<GMLFeatureClass *>::const_iterator oIter =
                    aosClasses.begin();
                std::vector<GMLFeatureClass *>::const_iterator oEndIter =
                    aosClasses.end();
                while (oIter != oEndIter)
                {
                    GMLFeatureClass *poClass = *oIter;

                    if( poClass->HasFeatureProperties() )
                    {
                        bHasFeatureProperties = true;
                        break;
                    }
                    ++oIter;
                }

                oIter = aosClasses.begin();
                while (oIter != oEndIter)
                {
                    GMLFeatureClass *poClass = *oIter;
                    ++oIter;

                    // We have no way of knowing if the geometry type is 25D
                    // when examining the xsd only, so if there was a hint
                    // it is, we force to 25D.
                    if (bHas3D && poClass->GetGeometryPropertyCount() == 1)
                    {
                        poClass->GetGeometryProperty(0)->SetType(
                            wkbSetZ((OGRwkbGeometryType)poClass->GetGeometryProperty(0)->GetType()));
                    }

                    bool bAddClass = true;
                    // If typenames are declared, only register the matching
                    // classes, in case the XSD contains more layers, but not if
                    // feature classes contain feature properties, in which case
                    // we will have embedded features that will be reported as
                    // top-level features.
                    if( papszTypeNames != NULL && !bHasFeatureProperties )
                    {
                        bAddClass = false;
                        char **papszIter = papszTypeNames;
                        while (*papszIter && !bAddClass)
                        {
                            const char *pszTypeName = *papszIter;
                            if (strcmp(pszTypeName, poClass->GetName()) == 0)
                                bAddClass = true;
                            papszIter++;
                        }

                        // Retry by removing prefixes.
                        if (!bAddClass)
                        {
                            papszIter = papszTypeNames;
                            while (*papszIter && !bAddClass)
                            {
                                const char *pszTypeName = *papszIter;
                                const char *pszColon = strchr(pszTypeName, ':');
                                if (pszColon)
                                {
                                    pszTypeName = pszColon + 1;
                                    if (strcmp(pszTypeName,
                                               poClass->GetName()) == 0)
                                    {
                                        poClass->SetName(pszTypeName);
                                        bAddClass = true;
                                    }
                                }
                                papszIter++;
                            }
                        }
                    }

                    if (bAddClass)
                        poReader->AddClass(poClass);
                    else
                        delete poClass;
                }

                poReader->SetClassListLocked(true);
            }
        }

        if (bHaveSchema && bIsWFS)
        {
            if( bIsWFSJointLayer )
            {
                BuildJointClassFromXSD();
            }

            // For WFS, we can assume sequential layers.
            if (poReader->GetClassCount() > 1 && pszReadMode == NULL &&
                !bHasFeatureProperties)
            {
                CPLDebug("GML",
                         "WFS output. Using SEQUENTIAL_LAYERS read mode");
                eReadMode = SEQUENTIAL_LAYERS;
            }
            // Sometimes the returned schema contains only <xs:include> that we
            // don't resolve so ignore it.
            else if (poReader->GetClassCount() == 0)
                bHaveSchema = false;
        }

        CSLDestroy(papszTypeNames);
    }

    // Force a first pass to establish the schema.  Eventually we will have
    // mechanisms for remembering the schema and related information.
    if( !bHaveSchema ||
        CPLFetchBool(poOpenInfo->papszOpenOptions, "FORCE_SRS_DETECTION",
                     false) )
    {
        bool bOnlyDetectSRS = bHaveSchema;
        if( !poReader->PrescanForSchema(true, bAnalyzeSRSPerFeature,
                                        bOnlyDetectSRS) )
        {
            // Assume an error was reported.
            return false;
        }
        if( !bHaveSchema )
        {
            if( bIsWFSJointLayer && poReader->GetClassCount() == 1 )
            {
                BuildJointClassFromScannedSchema();
            }

            if( bHasFoundXSD )
            {
                CPLDebug("GML", "Generating %s file, ignoring %s",
                         osGFSFilename.c_str(), osXSDFilename.c_str());
            }
        }
    }

    if (poReader->GetClassCount() > 1 && poReader->IsSequentialLayers() &&
        pszReadMode == NULL)
    {
        CPLDebug("GML", "Layers are monoblock. Using SEQUENTIAL_LAYERS read mode");
        eReadMode = SEQUENTIAL_LAYERS;
    }

    // Save the schema file if possible.  Don't make a fuss if we
    // can't.  It could be read-only directory or something.
    if( !bHaveSchema && !poReader->HasStoppedParsing() &&
        !STARTS_WITH_CI(pszFilename, "/vsitar/") &&
        !STARTS_WITH_CI(pszFilename, "/vsizip/") &&
        !STARTS_WITH_CI(pszFilename, "/vsigzip/vsi") &&
        !STARTS_WITH_CI(pszFilename, "/vsigzip//vsi") &&
        !STARTS_WITH_CI(pszFilename, "/vsicurl/") &&
        !STARTS_WITH_CI(pszFilename, "/vsicurl_streaming/"))
    {
        VSILFILE *l_fp = NULL;

        VSIStatBufL sGFSStatBuf;
        if( VSIStatExL(osGFSFilename, &sGFSStatBuf, VSI_STAT_EXISTS_FLAG) != 0 &&
            (l_fp = VSIFOpenL(osGFSFilename, "wt")) != NULL )
        {
            VSIFCloseL(l_fp);
            poReader->SaveClasses(osGFSFilename);
        }
        else
        {
            CPLDebug("GML",
                     "Not saving %s files already exists or can't be created.",
                     osGFSFilename.c_str());
        }
    }

    // Translate the GMLFeatureClasses into layers.
    papoLayers = static_cast<OGRGMLLayer **>(
        CPLCalloc(sizeof(OGRGMLLayer *), poReader->GetClassCount()));
    nLayers = 0;

    if (poReader->GetClassCount() == 1 && nNumberOfFeatures != 0)
    {
        GMLFeatureClass *poClass = poReader->GetClass(0);
        GIntBig nFeatureCount = poClass->GetFeatureCount();
        if (nFeatureCount < 0)
        {
            poClass->SetFeatureCount(nNumberOfFeatures);
        }
        else if (nFeatureCount != nNumberOfFeatures)
        {
            CPLDebug("GML",
                     "Feature count in header, "
                     "and actual feature count don't match");
        }
    }

    if (bIsWFS && poReader->GetClassCount() == 1)
        bUseGlobalSRSName = true;

    while( nLayers < poReader->GetClassCount() )
    {
        papoLayers[nLayers] = TranslateGMLSchema(poReader->GetClass(nLayers));
        nLayers++;
    }

    return true;
}

/************************************************************************/
/*                          BuildJointClassFromXSD()                    */
/************************************************************************/

void OGRGMLDataSource::BuildJointClassFromXSD()
{
    CPLString osJointClassName = "join";
    for(int i = 0; i < poReader->GetClassCount(); i++)
    {
        osJointClassName += "_";
        osJointClassName += poReader->GetClass(i)->GetName();
    }
    GMLFeatureClass *poJointClass = new GMLFeatureClass(osJointClassName);
    poJointClass->SetElementName("Tuple");
    for(int i = 0; i < poReader->GetClassCount(); i++)
    {
        GMLFeatureClass *poClass = poReader->GetClass(i);

        {
            CPLString osPropertyName;
            osPropertyName.Printf("%s.%s", poClass->GetName(), "gml_id");
            GMLPropertyDefn *poNewProperty =
                new GMLPropertyDefn(osPropertyName);
            CPLString osSrcElement;
            osSrcElement.Printf("member|%s@id", poClass->GetName());
            poNewProperty->SetSrcElement(osSrcElement);
            poNewProperty->SetType(GMLPT_String);
            poJointClass->AddProperty(poNewProperty);
        }

        for( int iField = 0; iField < poClass->GetPropertyCount(); iField++ )
        {
            GMLPropertyDefn *poProperty = poClass->GetProperty(iField);
            CPLString osPropertyName;
            osPropertyName.Printf("%s.%s", poClass->GetName(),
                                  poProperty->GetName());
            GMLPropertyDefn *poNewProperty =
                new GMLPropertyDefn(osPropertyName);

            poNewProperty->SetType(poProperty->GetType());
            CPLString osSrcElement;
            osSrcElement.Printf("member|%s|%s",
                                poClass->GetName(),
                                poProperty->GetSrcElement());
            poNewProperty->SetSrcElement(osSrcElement);
            poNewProperty->SetWidth(poProperty->GetWidth());
            poNewProperty->SetPrecision(poProperty->GetPrecision());
            poNewProperty->SetNullable(poProperty->IsNullable());

            poJointClass->AddProperty(poNewProperty);
        }
        for( int iField = 0;
             iField < poClass->GetGeometryPropertyCount();
             iField++ )
        {
            GMLGeometryPropertyDefn *poProperty =
                poClass->GetGeometryProperty(iField);
            CPLString osPropertyName;
            osPropertyName.Printf("%s.%s", poClass->GetName(),
                                  poProperty->GetName());
            CPLString osSrcElement;
            osSrcElement.Printf("member|%s|%s", poClass->GetName(),
                                poProperty->GetSrcElement());
            GMLGeometryPropertyDefn *poNewProperty =
                new GMLGeometryPropertyDefn(osPropertyName, osSrcElement,
                                            poProperty->GetType(), -1,
                                            poProperty->IsNullable());
            poJointClass->AddGeometryProperty(poNewProperty);
        }
    }
    poJointClass->SetSchemaLocked(true);

    poReader->ClearClasses();
    poReader->AddClass(poJointClass);
}

/************************************************************************/
/*                   BuildJointClassFromScannedSchema()                 */
/************************************************************************/

void OGRGMLDataSource::BuildJointClassFromScannedSchema()
{
    // Make sure that all properties of a same base feature type are
    // consecutive. If not, reorder.
    std::vector<std::vector<GMLPropertyDefn*> > aapoProps;
    GMLFeatureClass *poClass = poReader->GetClass(0);
    CPLString osJointClassName = "join";

    for( int iField = 0; iField < poClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProp = poClass->GetProperty(iField);
        CPLString osPrefix(poProp->GetName());
        size_t iPos = osPrefix.find('.');
        if( iPos != std::string::npos )
            osPrefix.resize(iPos);
        int iSubClass = 0;  // Used after for.
        for( ; iSubClass < static_cast<int>(aapoProps.size()); iSubClass++ )
        {
            CPLString osPrefixClass(aapoProps[iSubClass][0]->GetName());
            iPos = osPrefixClass.find('.');
            if( iPos != std::string::npos )
                osPrefixClass.resize(iPos);
            if( osPrefix == osPrefixClass )
                break;
        }
        if( iSubClass == static_cast<int>(aapoProps.size()) )
        {
            osJointClassName += "_";
            osJointClassName += osPrefix;
            aapoProps.push_back(std::vector<GMLPropertyDefn*>());
        }
        aapoProps[iSubClass].push_back(poProp);
    }
    poClass->SetElementName(poClass->GetName());
    poClass->SetName(osJointClassName);

    poClass->StealProperties();
    std::vector< std::pair< CPLString, std::vector<GMLGeometryPropertyDefn*> > >
        aapoGeomProps;
    for( int iSubClass = 0; iSubClass < static_cast<int>(aapoProps.size());
         iSubClass++ )
    {
        CPLString osPrefixClass(aapoProps[iSubClass][0]->GetName());
        size_t iPos = osPrefixClass.find('.');
        if( iPos != std::string::npos )
            osPrefixClass.resize(iPos);
        // Need to leave a space between > > for -Werror=c++0x-compat.
        aapoGeomProps.push_back(std::pair<CPLString, std::vector<GMLGeometryPropertyDefn *> >
                (osPrefixClass, std::vector<GMLGeometryPropertyDefn*>()));
        for( int iField = 0;
             iField < static_cast<int>(aapoProps[iSubClass].size());
             iField++ )
        {
            poClass->AddProperty(aapoProps[iSubClass][iField]);
        }
    }
    aapoProps.resize(0);

    // Reorder geometry fields too
    for( int iField = 0;
         iField < poClass->GetGeometryPropertyCount();
         iField++ )
    {
        GMLGeometryPropertyDefn *poProp = poClass->GetGeometryProperty(iField);
        CPLString osPrefix(poProp->GetName());
        size_t iPos = osPrefix.find('.');
        if( iPos != std::string::npos )
            osPrefix.resize(iPos);
        int iSubClass = 0;  // Used after for.
        for( ; iSubClass < static_cast<int>(aapoGeomProps.size()); iSubClass++ )
        {
            if( osPrefix == aapoGeomProps[iSubClass].first )
                break;
        }
        if( iSubClass == static_cast<int>(aapoGeomProps.size()) )
            aapoGeomProps.push_back(
                std::pair<CPLString, std::vector<GMLGeometryPropertyDefn*> >(
                    osPrefix, std::vector<GMLGeometryPropertyDefn *>()));
        aapoGeomProps[iSubClass].second.push_back(poProp);
    }
    poClass->StealGeometryProperties();
    for( int iSubClass = 0; iSubClass < static_cast<int>(aapoGeomProps.size());
         iSubClass++ )
    {
        for( int iField = 0;
             iField < static_cast<int>(aapoGeomProps[iSubClass].second.size());
             iField++ )
        {
            poClass->AddGeometryProperty(aapoGeomProps[iSubClass].second[iField]);
        }
    }
}

/************************************************************************/
/*                         TranslateGMLSchema()                         */
/************************************************************************/

OGRGMLLayer *OGRGMLDataSource::TranslateGMLSchema( GMLFeatureClass *poClass )

{
    // Create an empty layer.
    const char *pszSRSName = poClass->GetSRSName();
    OGRSpatialReference *poSRS = NULL;
    if (pszSRSName)
    {
        poSRS = new OGRSpatialReference();
        if (poSRS->SetFromUserInput(pszSRSName) != OGRERR_NONE)
        {
            delete poSRS;
            poSRS = NULL;
        }
    }
    else
    {
        pszSRSName = GetGlobalSRSName();
        if (pszSRSName)
        {
            poSRS = new OGRSpatialReference();
            if (poSRS->SetFromUserInput(pszSRSName) != OGRERR_NONE)
            {
                delete poSRS;
                poSRS = NULL;
            }

            if (poSRS != NULL && m_bInvertAxisOrderIfLatLong &&
                GML_IsSRSLatLongOrder(pszSRSName))
            {
                OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode("GEOGCS");
                if( poGEOGCS != NULL )
                    poGEOGCS->StripNodes("AXIS");

                OGR_SRSNode *poPROJCS = poSRS->GetAttrNode("PROJCS");
                if (poPROJCS != NULL && poSRS->EPSGTreatsAsNorthingEasting())
                    poPROJCS->StripNodes("AXIS");

                if (!poClass->HasExtents() && sBoundingRect.IsInit())
                {
                    poClass->SetExtents(sBoundingRect.MinY, sBoundingRect.MaxY,
                                        sBoundingRect.MinX, sBoundingRect.MaxX);
                }
            }
        }

        if (!poClass->HasExtents() && sBoundingRect.IsInit())
        {
            poClass->SetExtents(sBoundingRect.MinX, sBoundingRect.MaxX,
                                sBoundingRect.MinY, sBoundingRect.MaxY);
        }
    }

    // Report a COMPD_CS only if GML_REPORT_COMPD_CS is explicitly set to TRUE.
    if( poSRS != NULL &&
        !CPLTestBool(CPLGetConfigOption("GML_REPORT_COMPD_CS", "FALSE")) )
    {
        OGR_SRSNode *poCOMPD_CS = poSRS->GetAttrNode("COMPD_CS");
        if( poCOMPD_CS != NULL )
        {
            OGR_SRSNode *poCandidateRoot = poCOMPD_CS->GetNode("PROJCS");
            if( poCandidateRoot == NULL )
                poCandidateRoot = poCOMPD_CS->GetNode("GEOGCS");
            if( poCandidateRoot != NULL )
            {
                poSRS->SetRoot(poCandidateRoot->Clone());
            }
        }
    }

    OGRGMLLayer *poLayer = new OGRGMLLayer(poClass->GetName(), false, this);

    // Added attributes (properties).
    if (bExposeGMLId)
    {
        OGRFieldDefn oField("gml_id", OFTString);
        oField.SetNullable(FALSE);
        poLayer->GetLayerDefn()->AddFieldDefn(&oField);
    }
    else if (bExposeFid)
    {
        OGRFieldDefn oField("fid", OFTString);
        oField.SetNullable(FALSE);
        poLayer->GetLayerDefn()->AddFieldDefn(&oField);
    }

    for( int iField = 0;
         iField < poClass->GetGeometryPropertyCount();
         iField++ )
    {
        GMLGeometryPropertyDefn *poProperty =
            poClass->GetGeometryProperty(iField);
        OGRGeomFieldDefn oField(poProperty->GetName(),
                                (OGRwkbGeometryType)poProperty->GetType());
        if( poClass->GetGeometryPropertyCount() == 1 &&
            poClass->GetFeatureCount() == 0 )
        {
            oField.SetType(wkbUnknown);
        }
        oField.SetSpatialRef(poSRS);
        oField.SetNullable(poProperty->IsNullable());
        poLayer->GetLayerDefn()->AddGeomFieldDefn(&oField);
    }

    for( int iField = 0; iField < poClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProperty = poClass->GetProperty(iField);
        OGRFieldType eFType;

        if( poProperty->GetType() == GMLPT_Untyped )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_String )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_Integer ||
                 poProperty->GetType() == GMLPT_Boolean ||
                 poProperty->GetType() == GMLPT_Short )
            eFType = OFTInteger;
        else if( poProperty->GetType() == GMLPT_Integer64 )
            eFType = OFTInteger64;
        else if( poProperty->GetType() == GMLPT_Real ||
                 poProperty->GetType() == GMLPT_Float )
            eFType = OFTReal;
        else if( poProperty->GetType() == GMLPT_StringList )
            eFType = OFTStringList;
        else if( poProperty->GetType() == GMLPT_IntegerList ||
                 poProperty->GetType() == GMLPT_BooleanList )
            eFType = OFTIntegerList;
        else if( poProperty->GetType() == GMLPT_Integer64List )
            eFType = OFTInteger64List;
        else if( poProperty->GetType() == GMLPT_RealList )
            eFType = OFTRealList;
        else if( poProperty->GetType() == GMLPT_FeaturePropertyList )
            eFType = OFTStringList;
        else
            eFType = OFTString;

        OGRFieldDefn oField(poProperty->GetName(), eFType);
        if ( STARTS_WITH_CI(oField.GetNameRef(), "ogr:") )
          oField.SetName(poProperty->GetName() + 4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth(poProperty->GetWidth());
        if( poProperty->GetPrecision() > 0 )
            oField.SetPrecision(poProperty->GetPrecision());
        if( poProperty->GetType() == GMLPT_Boolean ||
            poProperty->GetType() == GMLPT_BooleanList )
            oField.SetSubType(OFSTBoolean);
        else if( poProperty->GetType() == GMLPT_Short)
            oField.SetSubType(OFSTInt16);
        else if( poProperty->GetType() == GMLPT_Float)
            oField.SetSubType(OFSTFloat32);
        if( !bEmptyAsNull )
            oField.SetNullable(poProperty->IsNullable() );

        poLayer->GetLayerDefn()->AddFieldDefn(&oField);
    }

    if( poSRS != NULL )
        poSRS->Release();

    return poLayer;
}

/************************************************************************/
/*                         GetGlobalSRSName()                           */
/************************************************************************/

const char *OGRGMLDataSource::GetGlobalSRSName()
{
    if (poReader->CanUseGlobalSRSName() || bUseGlobalSRSName)
        return poReader->GetGlobalSRSName();
    else
        return NULL;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

bool OGRGMLDataSource::Create( const char *pszFilename,
                               char **papszOptions )

{
    if( fpOutput != NULL || poReader != NULL )
    {
        CPLAssert(false);
        return false;
    }

    if( strcmp(pszFilename, "/dev/stdout") == 0 )
        pszFilename = "/vsistdout/";

    // Read options.
    CSLDestroy(papszCreateOptions);
    papszCreateOptions = CSLDuplicate(papszOptions);

    const char* pszFormat = CSLFetchNameValue(papszCreateOptions, "FORMAT");
    bIsOutputGML3 = pszFormat && EQUAL(pszFormat, "GML3");
    bIsOutputGML3Deegree = pszFormat && EQUAL(pszFormat, "GML3Deegree");
    bIsOutputGML32 = pszFormat && EQUAL(pszFormat, "GML3.2");
    if (bIsOutputGML3Deegree || bIsOutputGML32)
        bIsOutputGML3 = true;

    eSRSNameFormat = (bIsOutputGML3) ? SRSNAME_OGC_URN : SRSNAME_SHORT;
    if( bIsOutputGML3 )
    {
        const char *pszLongSRS =
            CSLFetchNameValue(papszCreateOptions, "GML3_LONGSRS");
        const char *pszSRSNameFormat =
            CSLFetchNameValue(papszCreateOptions, "SRSNAME_FORMAT");
        if( pszSRSNameFormat )
        {
            if( pszLongSRS )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Both GML3_LONGSRS and SRSNAME_FORMAT specified. "
                         "Ignoring GML3_LONGSRS");
            }
            if( EQUAL(pszSRSNameFormat, "SHORT") )
                eSRSNameFormat = SRSNAME_SHORT;
            else if( EQUAL(pszSRSNameFormat, "OGC_URN") )
                eSRSNameFormat = SRSNAME_OGC_URN;
            else if( EQUAL(pszSRSNameFormat, "OGC_URL") )
                eSRSNameFormat = SRSNAME_OGC_URL;
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Invalid value for SRSNAME_FORMAT. "
                         "Using SRSNAME_OGC_URN");
            }
        }
        else if( pszLongSRS && !CPLTestBool(pszLongSRS) )
            eSRSNameFormat = SRSNAME_SHORT;
    }

    bWriteSpaceIndentation = CPLTestBool(
        CSLFetchNameValueDef(papszCreateOptions, "SPACE_INDENTATION", "YES"));

    // Create the output file.
    pszName = CPLStrdup(pszFilename);
    osFilename = pszName;

    if( strcmp(pszFilename, "/vsistdout/") == 0 ||
        STARTS_WITH(pszFilename, "/vsigzip/") )
    {
        fpOutput = VSIFOpenExL(pszFilename, "wb", true);
        bFpOutputIsNonSeekable = true;
        bFpOutputSingleFile = true;
    }
    else if ( STARTS_WITH(pszFilename, "/vsizip/"))
    {
        if (EQUAL(CPLGetExtension(pszFilename), "zip"))
        {
            CPLFree(pszName);
            pszName = CPLStrdup(CPLFormFilename(pszFilename, "out.gml", NULL));
        }

        fpOutput = VSIFOpenExL(pszName, "wb", true);
        bFpOutputIsNonSeekable = true;
    }
    else
        fpOutput = VSIFOpenExL(pszFilename, "wb+", true);
    if( fpOutput == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to create GML file %s: %s",
                 pszFilename, VSIGetLastErrorMsg());
        return false;
    }

    // Write out "standard" header.
    PrintLine(fpOutput, "%s",
              "<?xml version=\"1.0\" encoding=\"utf-8\" ?>");

    if (!bFpOutputIsNonSeekable)
        nSchemaInsertLocation = static_cast<int>(VSIFTellL(fpOutput));

    const char *pszPrefix = GetAppPrefix();
    const char *pszTargetNameSpace = CSLFetchNameValueDef(
        papszOptions, "TARGET_NAMESPACE", "http://ogr.maptools.org/");

    if( RemoveAppPrefix() )
        PrintLine(fpOutput, "<FeatureCollection");
    else
        PrintLine(fpOutput, "<%s:FeatureCollection", pszPrefix);

    if (IsGML32Output())
    {
        char *pszGMLId = CPLEscapeString(
            CSLFetchNameValueDef(papszOptions, "GML_ID", "aFeatureCollection"),
            -1, CPLES_XML);
        PrintLine(fpOutput, "     gml:id=\"%s\"", pszGMLId);
        CPLFree(pszGMLId);
    }

    // Write out schema info if provided in creation options.
    const char *pszSchemaURI = CSLFetchNameValue(papszOptions, "XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue(papszOptions, "XSISCHEMA");

    if( pszSchemaURI != NULL )
    {
        PrintLine(
            fpOutput,
            "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine(fpOutput, "     xsi:schemaLocation=\"%s\"", pszSchemaURI);
    }
    else if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt, "EXTERNAL") )
    {
        char *pszBasename = CPLStrdup(CPLGetBasename(pszName));

        PrintLine(
            fpOutput,
            "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine(fpOutput, "     xsi:schemaLocation=\"%s %s\"",
                  pszTargetNameSpace, CPLResetExtension(pszBasename, "xsd"));
        CPLFree(pszBasename);
    }

    if( RemoveAppPrefix() )
        PrintLine(fpOutput, "     xmlns=\"%s\"", pszTargetNameSpace);
    else
        PrintLine(fpOutput,
                  "     xmlns:%s=\"%s\"", pszPrefix, pszTargetNameSpace);

    if (IsGML32Output())
        PrintLine(fpOutput, "%s",
                  "     xmlns:gml=\"http://www.opengis.net/gml/3.2\">");
    else
        PrintLine(fpOutput, "%s",
                  "     xmlns:gml=\"http://www.opengis.net/gml\">");

    return true;
}

/************************************************************************/
/*                         WriteTopElements()                           */
/************************************************************************/

void OGRGMLDataSource::WriteTopElements()
{
    const char *pszDescription = CSLFetchNameValueDef(
        papszCreateOptions, "DESCRIPTION", GetMetadataItem("DESCRIPTION"));
    if( pszDescription != NULL )
    {
        if (bWriteSpaceIndentation)
            VSIFPrintfL(fpOutput, "  ");
        char *pszTmp = CPLEscapeString(pszDescription, -1, CPLES_XML);
        PrintLine(fpOutput, "<gml:description>%s</gml:description>", pszTmp);
        CPLFree(pszTmp);
    }

    const char *l_pszName = CSLFetchNameValueDef(papszCreateOptions, "NAME",
                                                 GetMetadataItem("NAME"));
    if( l_pszName != NULL )
    {
        if (bWriteSpaceIndentation)
            VSIFPrintfL(fpOutput, "  ");
        char *pszTmp = CPLEscapeString(l_pszName, -1, CPLES_XML);
        PrintLine(fpOutput, "<gml:name>%s</gml:name>", pszTmp);
        CPLFree(pszTmp);
    }

    // Should we initialize an area to place the boundedBy element?
    // We will need to seek back to fill it in.
    nBoundedByLocation = -1;
    if( CPLFetchBool(papszCreateOptions, "BOUNDEDBY", true))
    {
        if (!bFpOutputIsNonSeekable )
        {
            nBoundedByLocation = static_cast<int>(VSIFTellL(fpOutput));

            if( nBoundedByLocation != -1 )
                PrintLine(fpOutput, "%350s", "");
        }
        else
        {
            if (bWriteSpaceIndentation)
                VSIFPrintfL(fpOutput, "  ");
            if (IsGML3Output())
                PrintLine(fpOutput,
                          "<gml:boundedBy><gml:Null /></gml:boundedBy>");
            else
                PrintLine(
                    fpOutput,
                    "<gml:boundedBy><gml:null>missing</gml:null></gml:boundedBy>");
        }
    }
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGMLDataSource::ICreateLayer(const char *pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               CPL_UNUSED char **papszOptions)
{
    // Verify we are in update mode.
    if( fpOutput == NULL )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened for read access.\n"
                 "New layer %s cannot be created.\n",
                 pszName, pszLayerName);

        return NULL;
    }

    // Ensure name is safe as an element name.
    char *pszCleanLayerName = CPLStrdup(pszLayerName);

    CPLCleanXMLElementName(pszCleanLayerName);
    if( strcmp(pszCleanLayerName, pszLayerName) != 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Layer name '%s' adjusted to '%s' for XML validity.",
                 pszLayerName, pszCleanLayerName);
    }

    // Set or check validity of global SRS.
    if (nLayers == 0)
    {
        WriteTopElements();
        if (poSRS)
            poWriteGlobalSRS = poSRS->Clone();
        bWriteGlobalSRS = true;
    }
    else if( bWriteGlobalSRS )
    {
        if( poWriteGlobalSRS != NULL )
        {
            if (poSRS == NULL || !poSRS->IsSame(poWriteGlobalSRS))
            {
                delete poWriteGlobalSRS;
                poWriteGlobalSRS = NULL;
                bWriteGlobalSRS = false;
            }
        }
        else
        {
            if( poSRS != NULL )
                bWriteGlobalSRS = false;
        }
    }

    // Create the layer object.
    OGRGMLLayer *poLayer = new OGRGMLLayer(pszCleanLayerName, true, this);
    poLayer->GetLayerDefn()->SetGeomType(eType);
    if( eType != wkbNone )
    {
        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetName(
            "geometryProperty");
        if( poSRS != NULL )
        {
            // Clone it since mapogroutput assumes that it can destroys
            // the SRS it has passed to use, instead of dereferencing it.
            poSRS = poSRS->Clone();
            poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Dereference();
        }
    }

    CPLFree(pszCleanLayerName);

    // Add layer to data source layer list.
    papoLayers = static_cast<OGRGMLLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRGMLLayer *) * (nLayers + 1)));

    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCCurveGeometries) )
        return bIsOutputGML3;
    else if( EQUAL(pszCap, ODsCRandomLayerWrite) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGMLDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            GrowExtents()                             */
/************************************************************************/

void OGRGMLDataSource::GrowExtents( OGREnvelope3D *psGeomBounds,
                                    int nCoordDimension )

{
    sBoundingRect.Merge(*psGeomBounds);
    if (nCoordDimension == 3)
        bBBOX3D = true;
}

/************************************************************************/
/*                            InsertHeader()                            */
/*                                                                      */
/*      This method is used to update boundedby info for a              */
/*      dataset, and insert schema descriptions depending on            */
/*      selection options in effect.                                    */
/************************************************************************/

void OGRGMLDataSource::InsertHeader()

{
    int nSchemaStart = 0;

    if( bFpOutputSingleFile )
        return;

    // Do we want to write the schema within the GML instance doc
    // or to a separate file?  For now we only support external.
    const char *pszSchemaURI =
        CSLFetchNameValue(papszCreateOptions, "XSISCHEMAURI");
    const char *pszSchemaOpt =
        CSLFetchNameValue(papszCreateOptions, "XSISCHEMA");

    if( pszSchemaURI != NULL )
        return;

    VSILFILE *fpSchema = NULL;
    if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt, "EXTERNAL") )
    {
        const char *pszXSDFilename = CPLResetExtension(pszName, "xsd");

        fpSchema = VSIFOpenL(pszXSDFilename, "wt");
        if( fpSchema == NULL )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open file %.500s for schema output.",
                     pszXSDFilename);
            return;
        }
        PrintLine(fpSchema, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    }
    else if( EQUAL(pszSchemaOpt, "INTERNAL") )
    {
        if (fpOutput == NULL)
            return;
        nSchemaStart = static_cast<int>(VSIFTellL(fpOutput));
        fpSchema = fpOutput;
    }
    else
    {
        return;
    }

    // Write the schema section at the end of the file.  Once
    // complete, we will read it back in, and then move the whole
    // file "down" enough to insert the schema at the beginning.

    // Detect if there are fields of List types.
    bool bHasListFields = false;

    for( int iLayer = 0; !bHasListFields && iLayer < GetLayerCount(); iLayer++ )
    {
        OGRFeatureDefn *poFDefn = papoLayers[iLayer]->GetLayerDefn();
        for( int iField = 0;
             !bHasListFields && iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( poFieldDefn->GetType() == OFTIntegerList ||
                poFieldDefn->GetType() == OFTInteger64List ||
                poFieldDefn->GetType() == OFTRealList ||
                poFieldDefn->GetType() == OFTStringList )
            {
                bHasListFields = true;
            }
        }
    }

    // Emit the start of the schema section.
    const char *pszPrefix = GetAppPrefix();
    if( pszPrefix[0] == '\0' )
        pszPrefix = "ogr";
    const char *pszTargetNameSpace = CSLFetchNameValueDef(
        papszCreateOptions, "TARGET_NAMESPACE", "http://ogr.maptools.org/");

    if (IsGML3Output())
    {
        PrintLine(fpSchema, "<xs:schema ");
        PrintLine(fpSchema, "    targetNamespace=\"%s\"", pszTargetNameSpace);
        PrintLine(fpSchema, "    xmlns:%s=\"%s\"",
                  pszPrefix, pszTargetNameSpace);
        PrintLine(fpSchema,
                  "    xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"");
        if (IsGML32Output())
        {
            PrintLine(fpSchema,
                      "    xmlns:gml=\"http://www.opengis.net/gml/3.2\"");
            PrintLine(fpSchema,
                      "    xmlns:gmlsf=\"http://www.opengis.net/gmlsf/2.0\"");
        }
        else
        {
            PrintLine(fpSchema, "    xmlns:gml=\"http://www.opengis.net/gml\"");
            if (!IsGML3DeegreeOutput())
            {
                PrintLine(fpSchema,
                          "    xmlns:gmlsf=\"http://www.opengis.net/gmlsf\"");
            }
        }
        PrintLine(fpSchema, "    elementFormDefault=\"qualified\"");
        PrintLine(fpSchema, "    version=\"1.0\">");

        if (IsGML32Output())
        {
            PrintLine(fpSchema, "<xs:annotation>");
            PrintLine(fpSchema,
                "  <xs:appinfo source=\"http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd\">");
            PrintLine(fpSchema,
                "    <gmlsf:ComplianceLevel>%d</gmlsf:ComplianceLevel>", (bHasListFields) ? 1 : 0);
            PrintLine(fpSchema, "  </xs:appinfo>");
            PrintLine(fpSchema, "</xs:annotation>");

            PrintLine(
                fpSchema,
                "<xs:import namespace=\"http://www.opengis.net/gml/3.2\" schemaLocation=\"http://schemas.opengis.net/gml/3.2.1/gml.xsd\"/>");
            PrintLine(
                fpSchema,
                "<xs:import namespace=\"http://www.opengis.net/gmlsf/2.0\" schemaLocation=\"http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd\"/>");
        }
        else
        {
            if (!IsGML3DeegreeOutput())
            {
                PrintLine(fpSchema, "<xs:annotation>");
                PrintLine(
                    fpSchema,
                    "  <xs:appinfo source=\"http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsfLevels.xsd\">");
                PrintLine(fpSchema,
                        "    <gmlsf:ComplianceLevel>%d</gmlsf:ComplianceLevel>", (bHasListFields) ? 1 : 0);
                PrintLine(fpSchema,
                        "    <gmlsf:GMLProfileSchema>http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsf.xsd</gmlsf:GMLProfileSchema>");
                PrintLine(fpSchema, "  </xs:appinfo>");
                PrintLine(fpSchema, "</xs:annotation>");
            }

            PrintLine(fpSchema,
                      "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengis.net/gml/3.1.1/base/gml.xsd\"/>");
            if (!IsGML3DeegreeOutput())
            {
                PrintLine(fpSchema,
                          "<xs:import namespace=\"http://www.opengis.net/gmlsf\" schemaLocation=\"http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsfLevels.xsd\"/>");
            }
        }
    }
    else
    {
        PrintLine(fpSchema,
                  "<xs:schema targetNamespace=\"%s\" xmlns:%s=\"%s\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:gml=\"http://www.opengis.net/gml\" elementFormDefault=\"qualified\" version=\"1.0\">",
                  pszTargetNameSpace, pszPrefix, pszTargetNameSpace);

        PrintLine(fpSchema,
                  "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengis.net/gml/2.1.2/feature.xsd\"/>");
    }

    // Define the FeatureCollection.
    if (IsGML3Output())
    {
        if (IsGML32Output())
        {
            // GML Simple Features profile v2.0 mentions gml:AbstractGML as
            // substitutionGroup but using gml:AbstractFeature makes it
            // usablable by GMLJP2 v2.
            PrintLine(fpSchema,
                      "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:AbstractFeature\"/>",
                      pszPrefix);
        }
        else if (IsGML3DeegreeOutput())
        {
            PrintLine(fpSchema,
                      "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>",
                      pszPrefix);
        }
        else
        {
            PrintLine(fpSchema,
                      "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_GML\"/>",
                      pszPrefix);
        }

        PrintLine(fpSchema, "<xs:complexType name=\"FeatureCollectionType\">");
        PrintLine(fpSchema, "  <xs:complexContent>");
        if (IsGML3DeegreeOutput())
        {
            PrintLine(fpSchema, "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">");
            PrintLine(fpSchema, "      <xs:sequence>");
            PrintLine(fpSchema, "        <xs:element name=\"featureMember\" minOccurs=\"0\" maxOccurs=\"unbounded\">");
        }
        else
        {
            PrintLine(fpSchema, "    <xs:extension base=\"gml:AbstractFeatureType\">");
            PrintLine(fpSchema, "      <xs:sequence minOccurs=\"0\" maxOccurs=\"unbounded\">");
            PrintLine(fpSchema, "        <xs:element name=\"featureMember\">");
        }
        PrintLine(fpSchema, "          <xs:complexType>");
        if (IsGML32Output())
        {
            PrintLine(fpSchema, "            <xs:complexContent>");
            PrintLine(fpSchema, "              <xs:extension base=\"gml:AbstractFeatureMemberType\">");
            PrintLine(fpSchema, "                <xs:sequence>");
            PrintLine(fpSchema, "                  <xs:element ref=\"gml:AbstractFeature\"/>");
            PrintLine(fpSchema, "                </xs:sequence>");
            PrintLine(fpSchema, "              </xs:extension>");
            PrintLine(fpSchema, "            </xs:complexContent>");
        }
        else
        {
            PrintLine(fpSchema, "            <xs:sequence>");
            PrintLine(fpSchema,
                      "              <xs:element ref=\"gml:_Feature\"/>");
            PrintLine(fpSchema, "            </xs:sequence>");
        }
        PrintLine(fpSchema, "          </xs:complexType>");
        PrintLine(fpSchema, "        </xs:element>");
        PrintLine(fpSchema, "      </xs:sequence>");
        PrintLine(fpSchema, "    </xs:extension>");
        PrintLine(fpSchema, "  </xs:complexContent>");
        PrintLine(fpSchema, "</xs:complexType>");
    }
    else
    {
        PrintLine(fpSchema,
                  "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>",
                  pszPrefix);

        PrintLine(fpSchema, "<xs:complexType name=\"FeatureCollectionType\">");
        PrintLine(fpSchema, "  <xs:complexContent>");
        PrintLine(
            fpSchema,
            "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">");
        PrintLine(fpSchema, "      <xs:attribute name=\"lockId\" type=\"xs:string\" use=\"optional\"/>");
        PrintLine(fpSchema, "      <xs:attribute name=\"scope\" type=\"xs:string\" use=\"optional\"/>");
        PrintLine(fpSchema, "    </xs:extension>");
        PrintLine(fpSchema, "  </xs:complexContent>");
        PrintLine(fpSchema, "</xs:complexType>");
    }

    // Define the schema for each layer.
    for( int iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        OGRFeatureDefn *poFDefn = papoLayers[iLayer]->GetLayerDefn();

        // Emit initial stuff for a feature type.
        if( IsGML32Output() )
        {
            PrintLine(
                fpSchema,
                "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:AbstractFeature\"/>",
                poFDefn->GetName(), pszPrefix, poFDefn->GetName());
        }
        else
        {
            PrintLine(
                fpSchema,
                "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:_Feature\"/>",
                poFDefn->GetName(), pszPrefix, poFDefn->GetName());
        }

        PrintLine(fpSchema, "<xs:complexType name=\"%s_Type\">",
                  poFDefn->GetName());
        PrintLine(fpSchema, "  <xs:complexContent>");
        PrintLine(fpSchema,
                  "    <xs:extension base=\"gml:AbstractFeatureType\">");
        PrintLine(fpSchema, "      <xs:sequence>");

        for( int iGeomField = 0; iGeomField < poFDefn->GetGeomFieldCount(); iGeomField++ )
        {
            OGRGeomFieldDefn *poFieldDefn =
                poFDefn->GetGeomFieldDefn(iGeomField);

            // Define the geometry attribute.
            const char *pszGeometryTypeName = "GeometryPropertyType";
            const char *pszComment = "";
            OGRwkbGeometryType eGType = wkbFlatten(poFieldDefn->GetType());
            switch(eGType)
            {
                case wkbPoint:
                    pszGeometryTypeName = "PointPropertyType";
                    break;
                case wkbLineString:
                case wkbCircularString:
                case wkbCompoundCurve:
                    if (IsGML3Output())
                    {
                        if( eGType == wkbLineString )
                            pszComment = " <!-- restricted to LineString -->";
                        else if( eGType == wkbCircularString )
                            pszComment = " <!-- contains CircularString -->";
                        else if( eGType == wkbCompoundCurve )
                            pszComment = " <!-- contains CompoundCurve -->";
                        pszGeometryTypeName = "CurvePropertyType";
                    }
                    else
                        pszGeometryTypeName = "LineStringPropertyType";
                    break;
                case wkbPolygon:
                case wkbCurvePolygon:
                    if (IsGML3Output())
                    {
                        if( eGType == wkbPolygon )
                            pszComment = " <!-- restricted to Polygon -->";
                        else if( eGType == wkbCurvePolygon )
                            pszComment = " <!-- contains CurvePolygon -->";
                        pszGeometryTypeName = "SurfacePropertyType";
                    }
                    else
                        pszGeometryTypeName = "PolygonPropertyType";
                    break;
                case wkbMultiPoint:
                    pszGeometryTypeName = "MultiPointPropertyType";
                    break;
                case wkbMultiLineString:
                case wkbMultiCurve:
                    if (IsGML3Output())
                    {
                        if( eGType == wkbMultiLineString )
                            pszComment = " <!-- restricted to MultiLineString -->";
                        else if( eGType == wkbMultiCurve )
                            pszComment = " <!-- contains non-linear MultiCurve -->";
                        pszGeometryTypeName = "MultiCurvePropertyType";
                    }
                    else
                        pszGeometryTypeName = "MultiLineStringPropertyType";
                    break;
                case wkbMultiPolygon:
                case wkbMultiSurface:
                    if (IsGML3Output())
                    {
                        if( eGType == wkbMultiPolygon )
                            pszComment = " <!-- restricted to MultiPolygon -->";
                        else if( eGType == wkbMultiSurface )
                            pszComment = " <!-- contains non-linear MultiSurface -->";
                        pszGeometryTypeName = "MultiSurfacePropertyType";
                    }
                    else
                        pszGeometryTypeName = "MultiPolygonPropertyType";
                    break;
                case wkbGeometryCollection:
                    pszGeometryTypeName = "MultiGeometryPropertyType";
                    break;
                default:
                    break;
            }

            int nMinOccurs = poFieldDefn->IsNullable() ? 0 : 1;
            PrintLine(fpSchema,
                "        <xs:element name=\"%s\" type=\"gml:%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"1\"/>%s",
                      poFieldDefn->GetNameRef(), pszGeometryTypeName,
                      nMinOccurs, pszComment);
        }

        // Emit each of the attributes.
        for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( IsGML3Output() &&
                strcmp(poFieldDefn->GetNameRef(), "gml_id") == 0 )
                continue;
            else if( !IsGML3Output() &&
                     strcmp(poFieldDefn->GetNameRef(), "fid") == 0 )
                continue;

            int nMinOccurs = poFieldDefn->IsNullable() ? 0 : 1;
            if( poFieldDefn->GetType() == OFTInteger ||
                poFieldDefn->GetType() == OFTIntegerList )
            {
                int nWidth =
                    poFieldDefn->GetWidth() > 0 ? poFieldDefn->GetWidth() : 16;

                PrintLine(fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"%s\">",
                          poFieldDefn->GetNameRef(),
                          nMinOccurs,
                          poFieldDefn->GetType() == OFTIntegerList ? "unbounded": "1");
                PrintLine(fpSchema, "          <xs:simpleType>");
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    PrintLine(
                        fpSchema,
                        "            <xs:restriction base=\"xs:boolean\">");
                }
                else if( poFieldDefn->GetSubType() == OFSTInt16 )
                {
                    PrintLine(fpSchema,
                              "            <xs:restriction base=\"xs:short\">");
                }
                else
                {
                    PrintLine(
                        fpSchema,
                        "            <xs:restriction base=\"xs:integer\">");
                    PrintLine(fpSchema,
                              "              <xs:totalDigits value=\"%d\"/>",
                              nWidth);
                }
                PrintLine(fpSchema, "            </xs:restriction>");
                PrintLine(fpSchema, "          </xs:simpleType>");
                PrintLine(fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTInteger64 ||
                     poFieldDefn->GetType() == OFTInteger64List )
            {
                int nWidth =
                    poFieldDefn->GetWidth() > 0 ? poFieldDefn->GetWidth() : 16;

                PrintLine(fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"%s\">",
                          poFieldDefn->GetNameRef(),
                          nMinOccurs,
                          poFieldDefn->GetType() == OFTInteger64List ? "unbounded": "1");
                PrintLine(fpSchema, "          <xs:simpleType>");
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    PrintLine(
                        fpSchema,
                        "            <xs:restriction base=\"xs:boolean\">");
                }
                else if( poFieldDefn->GetSubType() == OFSTInt16 )
                {
                    PrintLine(fpSchema,
                              "            <xs:restriction base=\"xs:short\">");
                }
                else
                {
                    PrintLine(fpSchema,
                              "            <xs:restriction base=\"xs:long\">");
                    PrintLine(fpSchema,
                              "              <xs:totalDigits value=\"%d\"/>",
                              nWidth);
                }
                PrintLine(fpSchema, "            </xs:restriction>");
                PrintLine(fpSchema, "          </xs:simpleType>");
                PrintLine(fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTReal ||
                     poFieldDefn->GetType() == OFTRealList )
            {
                int nWidth, nDecimals;

                nWidth = poFieldDefn->GetWidth();
                nDecimals = poFieldDefn->GetPrecision();

                PrintLine(fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"%s\">",
                          poFieldDefn->GetNameRef(),
                          nMinOccurs,
                          poFieldDefn->GetType() == OFTRealList ? "unbounded": "1");
                PrintLine(fpSchema, "          <xs:simpleType>");
                if( poFieldDefn->GetSubType() == OFSTFloat32 )
                    PrintLine(fpSchema, "            <xs:restriction base=\"xs:float\">");
                else
                    PrintLine(
                        fpSchema,
                        "            <xs:restriction base=\"xs:decimal\">");
                if (nWidth > 0)
                {
                    PrintLine(fpSchema,
                              "              <xs:totalDigits value=\"%d\"/>",
                              nWidth);
                    PrintLine(fpSchema,
                              "              <xs:fractionDigits value=\"%d\"/>",
                              nDecimals);
                }
                PrintLine(fpSchema, "            </xs:restriction>");
                PrintLine(fpSchema, "          </xs:simpleType>");
                PrintLine(fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTString ||
                     poFieldDefn->GetType() == OFTStringList )
            {
                PrintLine(fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"%s\">",
                          poFieldDefn->GetNameRef(),
                          nMinOccurs,
                          poFieldDefn->GetType() == OFTStringList ? "unbounded": "1");
                PrintLine(fpSchema, "          <xs:simpleType>");
                PrintLine(fpSchema,
                          "            <xs:restriction base=\"xs:string\">");
                if( poFieldDefn->GetWidth() != 0 )
                {
                    PrintLine(fpSchema, "              <xs:maxLength value=\"%d\"/>", poFieldDefn->GetWidth());
                }
                PrintLine(fpSchema, "            </xs:restriction>");
                PrintLine(fpSchema, "          </xs:simpleType>");
                PrintLine(fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime )
            {
                PrintLine(fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"%d\" maxOccurs=\"1\">",
                           poFieldDefn->GetNameRef(),
                           nMinOccurs );
                PrintLine(fpSchema, "          <xs:simpleType>");
                PrintLine(fpSchema,
                          "            <xs:restriction base=\"xs:string\">");
                PrintLine(fpSchema, "            </xs:restriction>");
                PrintLine(fpSchema, "          </xs:simpleType>");
                PrintLine(fpSchema, "        </xs:element>");
            }
            else
            {
                // TODO.
            }
        }  // Next field.

        // Finish off feature type.
        PrintLine(fpSchema, "      </xs:sequence>");
        PrintLine(fpSchema, "    </xs:extension>");
        PrintLine(fpSchema, "  </xs:complexContent>");
        PrintLine(fpSchema, "</xs:complexType>");
    }  // Next layer.

    PrintLine(fpSchema, "</xs:schema>");

    // Move schema to the start of the file.
    if( fpSchema == fpOutput )
    {
        // Read the schema into memory.
        int nSchemaSize = static_cast<int>(VSIFTellL(fpOutput) - nSchemaStart);
        char *pszSchema = static_cast<char *>(CPLMalloc(nSchemaSize + 1));

        VSIFSeekL(fpOutput, nSchemaStart, SEEK_SET);

        VSIFReadL(pszSchema, 1, nSchemaSize, fpOutput);
        pszSchema[nSchemaSize] = '\0';

        // Move file data down by "schema size" bytes from after <?xml> header
        // so we have room insert the schema.  Move in pretty big chunks.
        int nChunkSize = std::min(nSchemaStart - nSchemaInsertLocation, 250000);
        char *pszChunk = static_cast<char *>(CPLMalloc(nChunkSize));

        for( int nEndOfUnmovedData = nSchemaStart;
             nEndOfUnmovedData > nSchemaInsertLocation; )
        {
            const int nBytesToMove =
                std::min(nChunkSize, nEndOfUnmovedData - nSchemaInsertLocation);

            VSIFSeekL(fpOutput, nEndOfUnmovedData - nBytesToMove, SEEK_SET);
            VSIFReadL(pszChunk, 1, nBytesToMove, fpOutput);
            VSIFSeekL(fpOutput, nEndOfUnmovedData - nBytesToMove + nSchemaSize,
                      SEEK_SET);
            VSIFWriteL(pszChunk, 1, nBytesToMove, fpOutput);

            nEndOfUnmovedData -= nBytesToMove;
        }

        CPLFree(pszChunk);

        // Write the schema in the opened slot.
        VSIFSeekL(fpOutput, nSchemaInsertLocation, SEEK_SET);
        VSIFWriteL(pszSchema, 1, nSchemaSize, fpOutput);

        VSIFSeekL(fpOutput, 0, SEEK_END);

        nBoundedByLocation += nSchemaSize;

        CPLFree(pszSchema);
    }
    else
    {
        // Close external schema files.
        VSIFCloseL(fpSchema);
    }
}

/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void OGRGMLDataSource::PrintLine(VSILFILE *fp, const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start(args, fmt);
    osWork.vPrintf(fmt, args);
    va_end(args);

#ifdef WIN32
    const char *pszEOL = "\r\n";
#else
    const char *pszEOL = "\n";
#endif

    VSIFPrintfL(fp, "%s%s", osWork.c_str(), pszEOL);
}

/************************************************************************/
/*                     OGRGMLSingleFeatureLayer                         */
/************************************************************************/

class OGRGMLSingleFeatureLayer : public OGRLayer
{
  private:
    int                 nVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
    explicit            OGRGMLSingleFeatureLayer(int nVal );
    virtual ~OGRGMLSingleFeatureLayer() { poFeatureDefn->Release(); }

    virtual void        ResetReading() override { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                      OGRGMLSingleFeatureLayer()                      */
/************************************************************************/

OGRGMLSingleFeatureLayer::OGRGMLSingleFeatureLayer( int nValIn ) :
    nVal(nValIn),
    poFeatureDefn(new OGRFeatureDefn("SELECT")),
    iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField("Validates", OFTInteger);
    poFeatureDefn->AddFieldDefn(&oField);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMLSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId++);
    return poFeature;
}

/************************************************************************/
/*                            ExecuteSQL()                              */
/************************************************************************/

OGRLayer * OGRGMLDataSource::ExecuteSQL( const char *pszSQLCommand,
                                         OGRGeometry *poSpatialFilter,
                                         const char *pszDialect )
{
    if (poReader != NULL && EQUAL(pszSQLCommand, "SELECT ValidateSchema()"))
    {
        bool bIsValid = false;
        if (!osXSDFilename.empty() )
        {
            CPLErrorReset();
            bIsValid =
                CPL_TO_BOOL(CPLValidateXML(osFilename, osXSDFilename, NULL));
        }
        return new OGRGMLSingleFeatureLayer(bIsValid);
    }

    return OGRDataSource::ExecuteSQL(pszSQLCommand, poSpatialFilter,
                                     pszDialect);
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRGMLDataSource::ReleaseResultSet( OGRLayer *poResultsSet )
{
    delete poResultsSet;
}

/************************************************************************/
/*                      FindAndParseTopElements()                       */
/************************************************************************/

void OGRGMLDataSource::FindAndParseTopElements(VSILFILE *fp)
{
    // Build a shortened XML file that contain only the global
    // boundedBy element, so as to be able to parse it easily.

    char szStartTag[128];
    char *pszXML = static_cast<char *>(CPLMalloc(8192 + 128 + 3 + 1));
    VSIFSeekL(fp, 0, SEEK_SET);
    int nRead = static_cast<int>(VSIFReadL(pszXML, 1, 8192, fp));
    pszXML[nRead] = 0;

    const char *pszStartTag = strchr(pszXML, '<');
    if (pszStartTag != NULL)
    {
        while (pszStartTag != NULL && pszStartTag[1] == '?')
            pszStartTag = strchr(pszStartTag + 1, '<');

        if (pszStartTag != NULL)
        {
            pszStartTag++;
            const char *pszEndTag = strchr(pszStartTag, ' ');
            if (pszEndTag != NULL && pszEndTag - pszStartTag < 128 )
            {
                memcpy(szStartTag, pszStartTag, pszEndTag - pszStartTag);
                szStartTag[pszEndTag - pszStartTag] = '\0';
            }
            else
                pszStartTag = NULL;
        }
    }

    const char *pszDescription = strstr(pszXML, "<gml:description>");
    if( pszDescription )
    {
        pszDescription += strlen("<gml:description>");
        const char *pszEndDescription =
            strstr(pszDescription, "</gml:description>");
        if( pszEndDescription )
        {
            CPLString osTmp(pszDescription);
            osTmp.resize(pszEndDescription - pszDescription);
            char *pszTmp = CPLUnescapeString(osTmp, NULL, CPLES_XML);
            if( pszTmp )
                SetMetadataItem("DESCRIPTION", pszTmp);
            CPLFree(pszTmp);
        }
    }

    const char *l_pszName = strstr(pszXML, "<gml:name");
    if( l_pszName )
        l_pszName = strchr(l_pszName, '>');
    if( l_pszName )
    {
        l_pszName++;
        const char *pszEndName = strstr(l_pszName, "</gml:name>");
        if( pszEndName )
        {
            CPLString osTmp(l_pszName);
            osTmp.resize(pszEndName - l_pszName);
            char *pszTmp = CPLUnescapeString(osTmp, NULL, CPLES_XML);
            if( pszTmp )
                SetMetadataItem("NAME", pszTmp);
            CPLFree(pszTmp);
        }
    }

    char *pszEndBoundedBy = strstr(pszXML, "</wfs:boundedBy>");
    bool bWFSBoundedBy = false;
    if (pszEndBoundedBy != NULL)
        bWFSBoundedBy = true;
    else
        pszEndBoundedBy = strstr(pszXML, "</gml:boundedBy>");
    if (pszStartTag != NULL && pszEndBoundedBy != NULL)
    {
        char szSRSName[128] = {};

        // Find a srsName somewhere for some WFS 2.0 documents that have not it
        // set at the <wfs:boundedBy> element. e.g.
        // http://geoserv.weichand.de:8080/geoserver/wfs?SERVICE=WFS&REQUEST=GetFeature&VERSION=2.0.0&TYPENAME=bvv:gmd_ex
        if( bIsWFS )
        {
            ExtractSRSName(pszXML, szSRSName, sizeof(szSRSName));
        }

        pszEndBoundedBy[strlen("</gml:boundedBy>")] = '\0';
        strcat(pszXML, "</");
        strcat(pszXML, szStartTag);
        strcat(pszXML, ">");

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLXMLNode *psXML = CPLParseXMLString(pszXML);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (psXML != NULL)
        {
            CPLXMLNode *psBoundedBy = NULL;
            CPLXMLNode *psIter = psXML;
            while(psIter != NULL)
            {
                psBoundedBy = CPLGetXMLNode(
                    psIter, bWFSBoundedBy ? "wfs:boundedBy" : "gml:boundedBy");
                if (psBoundedBy != NULL)
                    break;
                psIter = psIter->psNext;
            }

            const char *pszLowerCorner = NULL;
            const char *pszUpperCorner = NULL;
            const char *pszSRSName = NULL;
            if (psBoundedBy != NULL)
            {
                CPLXMLNode *psEnvelope =
                    CPLGetXMLNode(psBoundedBy, "gml:Envelope");
                if (psEnvelope)
                {
                    pszSRSName = CPLGetXMLValue(psEnvelope, "srsName", NULL);
                    pszLowerCorner =
                        CPLGetXMLValue(psEnvelope, "gml:lowerCorner", NULL);
                    pszUpperCorner =
                        CPLGetXMLValue(psEnvelope, "gml:upperCorner", NULL);
                }
            }

            if( bIsWFS && pszSRSName == NULL &&
                pszLowerCorner != NULL && pszUpperCorner != NULL &&
                szSRSName[0] != '\0' )
            {
                pszSRSName = szSRSName;
            }

            if (pszSRSName != NULL && pszLowerCorner != NULL &&
                pszUpperCorner != NULL)
            {
                char **papszLC = CSLTokenizeString(pszLowerCorner);
                char **papszUC = CSLTokenizeString(pszUpperCorner);
                if (CSLCount(papszLC) >= 2 && CSLCount(papszUC) >= 2)
                {
                    CPLDebug("GML", "Global SRS = %s", pszSRSName);

                    if (STARTS_WITH(pszSRSName,
                                    "http://www.opengis.net/gml/srs/epsg.xml#"))
                    {
                        std::string osWork;
                        osWork.assign("EPSG:", 5);
                        osWork.append(pszSRSName + 40);
                        poReader->SetGlobalSRSName(osWork.c_str());
                    }
                    else
                    {
                        poReader->SetGlobalSRSName(pszSRSName);
                    }

                    const double dfMinX = CPLAtofM(papszLC[0]);
                    const double dfMinY = CPLAtofM(papszLC[1]);
                    const double dfMaxX = CPLAtofM(papszUC[0]);
                    const double dfMaxY = CPLAtofM(papszUC[1]);

                    SetExtents(dfMinX, dfMinY, dfMaxX, dfMaxY);
                }
                CSLDestroy(papszLC);
                CSLDestroy(papszUC);
            }

            CPLDestroyXMLNode(psXML);
        }
    }

    CPLFree(pszXML);
}

/************************************************************************/
/*                             SetExtents()                             */
/************************************************************************/

void OGRGMLDataSource::SetExtents(double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY)
{
    sBoundingRect.MinX = dfMinX;
    sBoundingRect.MinY = dfMinY;
    sBoundingRect.MaxX = dfMaxX;
    sBoundingRect.MaxY = dfMaxY;
}

/************************************************************************/
/*                             GetAppPrefix()                           */
/************************************************************************/

const char *OGRGMLDataSource::GetAppPrefix()
{
    return CSLFetchNameValueDef(papszCreateOptions, "PREFIX", "ogr");
}

/************************************************************************/
/*                            RemoveAppPrefix()                         */
/************************************************************************/

bool OGRGMLDataSource::RemoveAppPrefix()
{
    if( CPLTestBool(
            CSLFetchNameValueDef(papszCreateOptions, "STRIP_PREFIX", "FALSE")))
        return true;
    const char *pszPrefix = GetAppPrefix();
    return pszPrefix[0] == '\0';
}

/************************************************************************/
/*                        WriteFeatureBoundedBy()                       */
/************************************************************************/

bool OGRGMLDataSource::WriteFeatureBoundedBy()
{
    return CPLTestBool(CSLFetchNameValueDef(
        papszCreateOptions, "WRITE_FEATURE_BOUNDED_BY", "TRUE"));
}

/************************************************************************/
/*                          GetSRSDimensionLoc()                        */
/************************************************************************/

const char *OGRGMLDataSource::GetSRSDimensionLoc()
{
    return CSLFetchNameValue(papszCreateOptions, "SRSDIMENSION_LOC");
}
