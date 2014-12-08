/******************************************************************************
 * $Id$
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
 * Portions of this module implenting GML_SKIP_RESOLVE_ELEMS HUGE
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ****************************************************************************/

#include "ogr_gml.h"
#include "parsexsd.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_http.h"
#include "gmlutils.h"
#include "ogr_p.h"
#include "gmlregistry.h"

#include <vector>

CPL_CVSID("$Id$");

static int ExtractSRSName(const char* pszXML, char* szSRSName,
                          size_t sizeof_szSRSName);

/************************************************************************/
/*                   ReplaceSpaceByPct20IfNeeded()                      */
/************************************************************************/

static CPLString ReplaceSpaceByPct20IfNeeded(const char* pszURL)
{
    /* Replace ' ' by '%20' */
    CPLString osRet = pszURL;
    const char* pszNeedle = strstr(pszURL, "; ");
    if (pszNeedle)
    {
        char* pszTmp = (char*)CPLMalloc(strlen(pszURL) + 2 +1);
        int nBeforeNeedle = (int)(pszNeedle - pszURL);
        memcpy(pszTmp, pszURL, nBeforeNeedle);
        strcpy(pszTmp + nBeforeNeedle, ";%20");
        strcpy(pszTmp + nBeforeNeedle + strlen(";%20"), pszNeedle + strlen("; "));
        osRet = pszTmp;
        CPLFree(pszTmp);
    }

    return osRet;
}

/************************************************************************/
/*                         OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::OGRGMLDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    poReader = NULL;
    fpOutput = NULL;
    bFpOutputIsNonSeekable = FALSE;
    bFpOutputSingleFile = FALSE;
    bIsOutputGML3 = FALSE;
    bIsOutputGML3Deegree = FALSE;
    bIsOutputGML32 = FALSE;
    bIsLongSRSRequired = FALSE;
    bWriteSpaceIndentation = TRUE;

    papszCreateOptions = NULL;
    bOutIsTempFile = FALSE;

    bExposeGMLId = FALSE;
    bExposeFid = FALSE;
    nSchemaInsertLocation = -1;
    nBoundedByLocation = -1;
    bBBOX3D = FALSE;

    poWriteGlobalSRS = NULL;
    bWriteGlobalSRS = FALSE;
    bUseGlobalSRSName = FALSE;
    bIsWFS = FALSE;

    eReadMode = STANDARD;
    poStoredGMLFeature = NULL;
    poLastReadLayer = NULL;

    m_bInvertAxisOrderIfLatLong = FALSE;
    m_bConsiderEPSGAsURN = FALSE;
    m_bGetSecondaryGeometryOption = FALSE;
}

/************************************************************************/
/*                        ~OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::~OGRGMLDataSource()

{

    if( fpOutput != NULL )
    {
        const char* pszPrefix = GetAppPrefix();
        if( RemoveAppPrefix() )
            PrintLine( fpOutput, "</FeatureCollection>" );
        else
            PrintLine( fpOutput, "</%s:FeatureCollection>", pszPrefix );

        if( bFpOutputIsNonSeekable)
        {
            VSIFCloseL( fpOutput );
            fpOutput = NULL;
        }

        InsertHeader();

        if( !bFpOutputIsNonSeekable
            && nBoundedByLocation != -1
            && VSIFSeekL( fpOutput, nBoundedByLocation, SEEK_SET ) == 0 )
        {
            if (bWriteGlobalSRS && sBoundingRect.IsInit()  && IsGML3Output())
            {
                int bCoordSwap = FALSE;
                char* pszSRSName;
                if (poWriteGlobalSRS)
                    pszSRSName = GML_GetSRSName(poWriteGlobalSRS, IsLongSRSRequired(), &bCoordSwap);
                else
                    pszSRSName = CPLStrdup("");
                char szLowerCorner[75], szUpperCorner[75];
                if (bCoordSwap)
                {
                    OGRMakeWktCoordinate(szLowerCorner, sBoundingRect.MinY, sBoundingRect.MinX, sBoundingRect.MinZ, (bBBOX3D) ? 3 : 2);
                    OGRMakeWktCoordinate(szUpperCorner, sBoundingRect.MaxY, sBoundingRect.MaxX, sBoundingRect.MaxZ, (bBBOX3D) ? 3 : 2);
                }
                else
                {
                    OGRMakeWktCoordinate(szLowerCorner, sBoundingRect.MinX, sBoundingRect.MinY, sBoundingRect.MinZ, (bBBOX3D) ? 3 : 2);
                    OGRMakeWktCoordinate(szUpperCorner, sBoundingRect.MaxX, sBoundingRect.MaxY, sBoundingRect.MaxZ, (bBBOX3D) ? 3 : 2);
                }
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "  ");
                PrintLine( fpOutput, "<gml:boundedBy><gml:Envelope%s%s><gml:lowerCorner>%s</gml:lowerCorner><gml:upperCorner>%s</gml:upperCorner></gml:Envelope></gml:boundedBy>",
                           (bBBOX3D) ? " srsDimension=\"3\"" : "", pszSRSName, szLowerCorner, szUpperCorner);
                CPLFree(pszSRSName);
            }
            else if (bWriteGlobalSRS && sBoundingRect.IsInit())
            {
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "  ");
                PrintLine( fpOutput, "<gml:boundedBy>" );
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "    ");
                PrintLine( fpOutput, "<gml:Box>" );
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "      ");
                VSIFPrintfL( fpOutput,
                            "<gml:coord><gml:X>%.16g</gml:X>"
                            "<gml:Y>%.16g</gml:Y>",
                            sBoundingRect.MinX, sBoundingRect.MinY );
                if (bBBOX3D)
                    VSIFPrintfL( fpOutput, "<gml:Z>%.16g</gml:Z>",
                               sBoundingRect.MinZ );
                PrintLine( fpOutput, "</gml:coord>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "      ");
                VSIFPrintfL( fpOutput,
                            "<gml:coord><gml:X>%.16g</gml:X>"
                            "<gml:Y>%.16g</gml:Y>",
                            sBoundingRect.MaxX, sBoundingRect.MaxY );
                if (bBBOX3D)
                    VSIFPrintfL( fpOutput, "<gml:Z>%.16g</gml:Z>",
                               sBoundingRect.MaxZ );
                PrintLine( fpOutput, "</gml:coord>");
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "    ");
                PrintLine( fpOutput, "</gml:Box>" );
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "  ");
                PrintLine( fpOutput, "</gml:boundedBy>" );
            }
            else
            {
                if (bWriteSpaceIndentation)
                    VSIFPrintfL( fpOutput, "  ");
                if (IsGML3Output())
                    PrintLine( fpOutput, "<gml:boundedBy><gml:Null /></gml:boundedBy>" );
                else
                    PrintLine( fpOutput, "<gml:boundedBy><gml:null>missing</gml:null></gml:boundedBy>" );
            }
        }

        if (fpOutput)
            VSIFCloseL( fpOutput );
    }

    CSLDestroy( papszCreateOptions );
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    if( poReader )
    {
        if (bOutIsTempFile)
            VSIUnlink(poReader->GetSourceFileName());
        delete poReader;
    }

    delete poWriteGlobalSRS;

    delete poStoredGMLFeature;

    if (osXSDFilename.compare(CPLSPrintf("/vsimem/tmp_gml_xsd_%p.xsd", this)) == 0)
        VSIUnlink(osXSDFilename);
}

/************************************************************************/
/*                            CheckHeader()                             */
/************************************************************************/

int OGRGMLDataSource::CheckHeader(const char* pszStr)
{
    if( strstr(pszStr,"opengis.net/gml") == NULL )
    {
        return FALSE;
    }

    /* Ignore .xsd schemas */
    if( strstr(pszStr, "<schema") != NULL
        || strstr(pszStr, "<xs:schema") != NULL
        || strstr(pszStr, "<xsd:schema") != NULL )
    {
        return FALSE;
    }

    /* Ignore GeoRSS documents. They will be recognized by the GeoRSS driver */
    if( strstr(pszStr, "<rss") != NULL && strstr(pszStr, "xmlns:georss") != NULL )
    {
        return FALSE;
    }

    /* Ignore OpenJUMP .jml documents. They will be recognized by the OpenJUMP driver */
    if( strstr(pszStr, "<JCSDataFile") != NULL )
    {
        return FALSE;
    }

    /* Ignore OGR WFS xml description files */
    if( strstr(pszStr, "<OGRWFSDataSource>") != NULL )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGMLDataSource::Open( const char * pszNameIn )

{
    VSILFILE   *fp;
    char        szHeader[4096];
    int         nNumberOfFeatures = 0;
    CPLString   osWithVsiGzip;
    const char *pszSchemaLocation = NULL;
    int bCheckAuxFile = TRUE;

/* -------------------------------------------------------------------- */
/*      Extract xsd filename from connexion string if present.          */
/* -------------------------------------------------------------------- */
    osFilename = pszNameIn;
    const char *pszXSDFilenameTmp = strstr(pszNameIn, ",xsd=");
    if (pszXSDFilenameTmp != NULL)
    {
        osFilename.resize(pszXSDFilenameTmp - pszNameIn);
        osXSDFilename = pszXSDFilenameTmp + strlen(",xsd=");
    }
    const char *pszFilename = osFilename.c_str();

    pszName = CPLStrdup( pszNameIn );

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "r" );
    if( fp == NULL )
        return FALSE;

    int bExpatCompatibleEncoding = FALSE;
    int bHas3D = FALSE;
    int bHintConsiderEPSGAsURN = FALSE;
    int bAnalyzeSRSPerFeature = TRUE;

    char szSRSName[128];
    szSRSName[0] = '\0';

/* -------------------------------------------------------------------- */
/*      Load a header chunk and check for signs it is GML               */
/* -------------------------------------------------------------------- */

    size_t nRead = VSIFReadL( szHeader, 1, sizeof(szHeader)-1, fp );
    if (nRead <= 0)
    {
        VSIFCloseL( fp );
        return FALSE;
    }
    szHeader[nRead] = '\0';

    /* Might be a OS-Mastermap gzipped GML, so let be nice and try to open */
    /* it transparently with /vsigzip/ */
    if ( ((GByte*)szHeader)[0] == 0x1f && ((GByte*)szHeader)[1] == 0x8b &&
            EQUAL(CPLGetExtension(pszFilename), "gz") &&
            strncmp(pszFilename, "/vsigzip/", strlen("/vsigzip/")) != 0 )
    {
        VSIFCloseL( fp );
        osWithVsiGzip = "/vsigzip/";
        osWithVsiGzip += pszFilename;

        pszFilename = osWithVsiGzip;

        fp = VSIFOpenL( pszFilename, "r" );
        if( fp == NULL )
            return FALSE;

        nRead = VSIFReadL( szHeader, 1, sizeof(szHeader) - 1, fp );
        if (nRead <= 0)
        {
            VSIFCloseL( fp );
            return FALSE;
        }
        szHeader[nRead] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Check for a UTF-8 BOM and skip if found                         */
/*                                                                      */
/*      TODO: BOM is variable-lenght parameter and depends on encoding. */
/*            Add BOM detection for other encodings.                    */
/* -------------------------------------------------------------------- */

    // Used to skip to actual beginning of XML data
    char* szPtr = szHeader;

    if( ( (unsigned char)szHeader[0] == 0xEF )
        && ( (unsigned char)szHeader[1] == 0xBB )
        && ( (unsigned char)szHeader[2] == 0xBF) )
    {
        szPtr += 3;
    }

    const char* pszEncoding = strstr(szPtr, "encoding=");
    if (pszEncoding)
        bExpatCompatibleEncoding = (pszEncoding[9] == '\'' || pszEncoding[9] == '"') &&
                                    (EQUALN(pszEncoding + 10, "UTF-8", 5) ||
                                    EQUALN(pszEncoding + 10, "ISO-8859-15", 11) ||
                                    (EQUALN(pszEncoding + 10, "ISO-8859-1", 10) &&
                                        pszEncoding[20] == pszEncoding[9])) ;
    else
        bExpatCompatibleEncoding = TRUE; /* utf-8 is the default */

    bHas3D = strstr(szPtr, "srsDimension=\"3\"") != NULL || strstr(szPtr, "<gml:Z>") != NULL;

/* -------------------------------------------------------------------- */
/*      Here, we expect the opening chevrons of GML tree root element   */
/* -------------------------------------------------------------------- */
    if( szPtr[0] != '<' || !CheckHeader(szPtr) )
    {
        VSIFCloseL( fp );
        return FALSE;
    }

    /* Small optimization: if we parse a <wfs:FeatureCollection>  and */
    /* that numberOfFeatures is set, we can use it to set the FeatureCount */
    /* but *ONLY* if there's just one class ! */
    const char* pszFeatureCollection = strstr(szPtr, "wfs:FeatureCollection");
    if (pszFeatureCollection == NULL)
        pszFeatureCollection = strstr(szPtr, "gml:FeatureCollection"); /* GML 3.2.1 output */
    if (pszFeatureCollection == NULL)
    {
        pszFeatureCollection = strstr(szPtr, "<FeatureCollection"); /* Deegree WFS 1.0.0 output */
        if (pszFeatureCollection && strstr(szPtr, "xmlns:wfs=\"http://www.opengis.net/wfs\"") == NULL)
            pszFeatureCollection = NULL;
    }
    if (pszFeatureCollection)
    {
        bExposeGMLId = TRUE;
        bIsWFS = TRUE;
        const char* pszNumberOfFeatures = strstr(szPtr, "numberOfFeatures=");
        if (pszNumberOfFeatures)
        {
            pszNumberOfFeatures += 17;
            char ch = pszNumberOfFeatures[0];
            if ((ch == '\'' || ch == '"') && strchr(pszNumberOfFeatures + 1, ch) != NULL)
            {
                nNumberOfFeatures = atoi(pszNumberOfFeatures + 1);
            }
        }
        else if ((pszNumberOfFeatures = strstr(szPtr, "numberReturned=")) != NULL) /* WFS 2.0.0 */
        {
            pszNumberOfFeatures += 15;
            char ch = pszNumberOfFeatures[0];
            if ((ch == '\'' || ch == '"') && strchr(pszNumberOfFeatures + 1, ch) != NULL)
            {
                /* 'unknown' might be a valid value in a corrected version of WFS 2.0 */
                /* but it will also evaluate to 0, that is considered as unknown, so nothing */
                /* particular to do */
                nNumberOfFeatures = atoi(pszNumberOfFeatures + 1);
            }
        }
    }
    else if (strncmp(pszFilename, "/vsimem/tempwfs_", strlen("/vsimem/tempwfs_")) == 0)
    {
        /* http://regis.intergraph.com/wfs/dcmetro/request.asp? returns a <G:FeatureCollection> */
        /* Who knows what servers can return ? Ok, so when in the context of the WFS driver */
        /* always expose the gml:id to avoid later crashes */
        bExposeGMLId = TRUE;
        bIsWFS = TRUE;
    }
    else
    {
        bExposeGMLId = strstr(szPtr, " gml:id=\"") != NULL ||
                        strstr(szPtr, " gml:id='") != NULL;
        bExposeFid = strstr(szPtr, " fid=\"") != NULL ||
                        strstr(szPtr, " fid='") != NULL;
        
        const char* pszExposeGMLId = CPLGetConfigOption("GML_EXPOSE_GML_ID", NULL);
        if (pszExposeGMLId)
            bExposeGMLId = CSLTestBoolean(pszExposeGMLId);

        const char* pszExposeFid = CPLGetConfigOption("GML_EXPOSE_FID", NULL);
        if (pszExposeFid)
            bExposeFid = CSLTestBoolean(pszExposeFid);
    }

    bHintConsiderEPSGAsURN = strstr(szPtr, "xmlns:fme=\"http://www.safe.com/gml/fme\"") != NULL;

    /* MTKGML */
    if( strstr(szPtr, "<Maastotiedot") != NULL )
    {
        if( strstr(szPtr, "http://xml.nls.fi/XML/Namespace/Maastotietojarjestelma/SiirtotiedostonMalli/2011-02") == NULL )
            CPLDebug("GML", "Warning: a MTKGML file was detected, but its namespace is unknown");
        bAnalyzeSRSPerFeature = FALSE;
        bUseGlobalSRSName = TRUE;
        if( !ExtractSRSName(szPtr, szSRSName, sizeof(szSRSName)) )
            strcpy(szSRSName, "EPSG:3067");
    }

    pszSchemaLocation = strstr(szPtr, "schemaLocation=");
    if (pszSchemaLocation)
        pszSchemaLocation += strlen("schemaLocation=");

    if (bIsWFS && EQUALN(pszFilename, "/vsicurl_streaming/", strlen("/vsicurl_streaming/")))
        bCheckAuxFile = FALSE;

/* -------------------------------------------------------------------- */
/*      We assume now that it is GML.  Instantiate a GMLReader on it.   */
/* -------------------------------------------------------------------- */

    const char* pszReadMode = CPLGetConfigOption("GML_READ_MODE", NULL);
    if (pszReadMode == NULL || EQUAL(pszReadMode, "STANDARD"))
        eReadMode = STANDARD;
    else if (EQUAL(pszReadMode, "SEQUENTIAL_LAYERS"))
        eReadMode = SEQUENTIAL_LAYERS;
    else if (EQUAL(pszReadMode, "INTERLEAVED_LAYERS"))
        eReadMode = INTERLEAVED_LAYERS;
    else
    {
        CPLDebug("GML", "Unrecognized value for GML_READ_MODE configuration option.");
    }

    m_bInvertAxisOrderIfLatLong = CSLTestBoolean(
        CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES"));

    const char* pszConsiderEPSGAsURN =
        CPLGetConfigOption("GML_CONSIDER_EPSG_AS_URN", NULL);
    if (pszConsiderEPSGAsURN != NULL)
        m_bConsiderEPSGAsURN = CSLTestBoolean(pszConsiderEPSGAsURN);
    else if (bHintConsiderEPSGAsURN)
    {
        /* GML produced by FME (at least CanVec GML) seem to honour EPSG axis ordering */
        CPLDebug("GML", "FME-produced GML --> consider that GML_CONSIDER_EPSG_AS_URN is set to YES");
        m_bConsiderEPSGAsURN = TRUE;
    }
    else
        m_bConsiderEPSGAsURN = FALSE;

    m_bGetSecondaryGeometryOption = CSLTestBoolean(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));

    /* EXPAT is faster than Xerces, so when it is safe to use it, use it ! */
    /* The only interest of Xerces is for rare encodings that Expat doesn't handle */
    /* but UTF-8 is well handled by Expat */
    int bUseExpatParserPreferably = bExpatCompatibleEncoding;

    /* Override default choice */
    const char* pszGMLParser = CPLGetConfigOption("GML_PARSER", NULL);
    if (pszGMLParser)
    {
        if (EQUAL(pszGMLParser, "EXPAT"))
            bUseExpatParserPreferably = TRUE;
        else if (EQUAL(pszGMLParser, "XERCES"))
            bUseExpatParserPreferably = FALSE;
    }

    poReader = CreateGMLReader( bUseExpatParserPreferably,
                                m_bInvertAxisOrderIfLatLong,
                                m_bConsiderEPSGAsURN,
                                m_bGetSecondaryGeometryOption );
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s appears to be GML but the GML reader can't\n"
                  "be instantiated, likely because Xerces or Expat support wasn't\n"
                  "configured in.", 
                  pszFilename );
        VSIFCloseL( fp );
        return FALSE;
    }

    poReader->SetSourceFile( pszFilename );

/* -------------------------------------------------------------------- */
/*      Find <gml:boundedBy>                                            */
/* -------------------------------------------------------------------- */

    FindAndParseBoundedBy(fp);

    if( szSRSName[0] != '\0' )
        poReader->SetGlobalSRSName(szSRSName);

/* -------------------------------------------------------------------- */
/*      Resolve the xlinks in the source file and save it with the      */
/*      extension ".resolved.gml". The source file will to set to that. */
/* -------------------------------------------------------------------- */

    char *pszXlinkResolvedFilename = NULL;
    const char *pszOption = CPLGetConfigOption("GML_SAVE_RESOLVED_TO", NULL);
    int bResolve = TRUE;
    int bHugeFile = FALSE;
    if( pszOption != NULL && EQUALN( pszOption, "SAME", 4 ) )
    {
        // "SAME" will overwrite the existing gml file
        pszXlinkResolvedFilename = CPLStrdup( pszFilename );
    }
    else if( pszOption != NULL &&
             CPLStrnlen( pszOption, 5 ) >= 5 &&
             EQUALN( pszOption - 4 + strlen( pszOption ), ".gml", 4 ) )
    {
        // Any string ending with ".gml" will try and write to it
        pszXlinkResolvedFilename = CPLStrdup( pszOption );
    }
    else
    {
        // When no option is given or is not recognised,
        // use the same file name with the extension changed to .resolved.gml
        pszXlinkResolvedFilename = CPLStrdup(
                            CPLResetExtension( pszFilename, "resolved.gml" ) );

        // Check if the file already exists.
        VSIStatBufL sResStatBuf, sGMLStatBuf;
        if( bCheckAuxFile && VSIStatL( pszXlinkResolvedFilename, &sResStatBuf ) == 0 )
        {
            VSIStatL( pszFilename, &sGMLStatBuf );
            if( sGMLStatBuf.st_mtime > sResStatBuf.st_mtime )
            {
                CPLDebug( "GML", 
                          "Found %s but ignoring because it appears\n"
                          "be older than the associated GML file.", 
                          pszXlinkResolvedFilename );
            }
            else
            {
                poReader->SetSourceFile( pszXlinkResolvedFilename );
                bResolve = FALSE;
            }
        }
    }

    const char *pszSkipOption = CPLGetConfigOption( "GML_SKIP_RESOLVE_ELEMS",
                                                    "ALL");
    char **papszSkip = NULL;
    if( EQUAL( pszSkipOption, "ALL" ) )
        bResolve = FALSE;
    else if( EQUAL( pszSkipOption, "HUGE" ) )//exactly as NONE, but intended for HUGE files
        bHugeFile = TRUE;
    else if( !EQUAL( pszSkipOption, "NONE" ) )//use this to resolve everything
        papszSkip = CSLTokenizeString2( pszSkipOption, ",",
                                           CSLT_STRIPLEADSPACES |
                                           CSLT_STRIPENDSPACES );
    int         bHaveSchema = FALSE;
    int         bSchemaDone = FALSE;
 
/* -------------------------------------------------------------------- */
/*      Is some GML Feature Schema (.gfs) TEMPLATE required ?           */
/* -------------------------------------------------------------------- */
    const char *pszGFSTemplateName = 
                CPLGetConfigOption( "GML_GFS_TEMPLATE", NULL);
    if( pszGFSTemplateName != NULL )
    {
        /* attempting to load the GFS TEMPLATE */
        bHaveSchema = poReader->LoadClasses( pszGFSTemplateName );
    }	

    if( bResolve )
    {
        if ( bHugeFile )
        {
            bSchemaDone = TRUE;
            int bSqliteIsTempFile =
                CSLTestBoolean(CPLGetConfigOption( "GML_HUGE_TEMPFILE", "YES"));
            int iSqliteCacheMB = atoi(CPLGetConfigOption( "OGR_SQLITE_CACHE", "0"));
            if( poReader->HugeFileResolver( pszXlinkResolvedFilename,
                                            bSqliteIsTempFile, 
                                            iSqliteCacheMB ) == FALSE )
            {
                // we assume an errors have been reported.
                VSIFCloseL(fp);
                CPLFree( pszXlinkResolvedFilename );
                return FALSE;
            }
        }
        else
        {
            poReader->ResolveXlinks( pszXlinkResolvedFilename,
                                     &bOutIsTempFile,
                                     papszSkip );
        }
    }

    CPLFree( pszXlinkResolvedFilename );
    pszXlinkResolvedFilename = NULL;
    CSLDestroy( papszSkip );
    papszSkip = NULL;

    /* If the source filename for the reader is still the GML filename, then */
    /* we can directly provide the file pointer. Otherwise we close it */
    if (strcmp(poReader->GetSourceFileName(), pszFilename) == 0)
        poReader->SetFP(fp);
    else
        VSIFCloseL(fp);
    fp = NULL;

    /* Is a prescan required ? */
    if( bHaveSchema && !bSchemaDone )
    {
        /* We must detect which layers are actually present in the .gml */
        /* and how many features they have */
        if( !poReader->PrescanForTemplate() )
        {
            // we assume an errors have been reported.
            return FALSE;
        }
    }

    CPLString osGFSFilename = CPLResetExtension( pszFilename, "gfs" );
    if (strncmp(osGFSFilename, "/vsigzip/", strlen("/vsigzip/")) == 0)
        osGFSFilename = osGFSFilename.substr(strlen("/vsigzip/"));

/* -------------------------------------------------------------------- */
/*      Can we find a GML Feature Schema (.gfs) for the input file?     */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema && osXSDFilename.size() == 0)
    {
        VSIStatBufL sGFSStatBuf;
        if( bCheckAuxFile && VSIStatL( osGFSFilename, &sGFSStatBuf ) == 0 )
        {
            VSIStatBufL sGMLStatBuf;
            VSIStatL( pszFilename, &sGMLStatBuf );
            if( sGMLStatBuf.st_mtime > sGFSStatBuf.st_mtime )
            {
                CPLDebug( "GML", 
                          "Found %s but ignoring because it appears\n"
                          "be older than the associated GML file.", 
                          osGFSFilename.c_str() );
            }
            else
            {
                bHaveSchema = poReader->LoadClasses( osGFSFilename );
                if (bHaveSchema)
                {
                    const char *pszXSDFilenameTmp;
                    pszXSDFilenameTmp = CPLResetExtension( pszFilename, "xsd" );
                    if( VSIStatExL( pszXSDFilenameTmp, &sGMLStatBuf,
                                    VSI_STAT_EXISTS_FLAG ) == 0 )
                    {
                        CPLDebug("GML", "Using %s file, ignoring %s",
                                 osGFSFilename.c_str(), pszXSDFilenameTmp);
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Can we find an xsd which might conform to tbe GML3 Level 0      */
/*      profile?  We really ought to look for it based on the rules     */
/*      schemaLocation in the GML feature collection but for now we     */
/*      just hopes it is in the same director with the same name.       */
/* -------------------------------------------------------------------- */
    int bHasFoundXSD = FALSE;

    if( !bHaveSchema )
    {
        char** papszTypeNames = NULL;

        VSIStatBufL sXSDStatBuf;
        if (osXSDFilename.size() == 0)
        {
            osXSDFilename = CPLResetExtension( pszFilename, "xsd" );
            if( bCheckAuxFile && VSIStatExL( osXSDFilename, &sXSDStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
            {
                bHasFoundXSD = TRUE;
            }
        }
        else
        {
            if ( strncmp(osXSDFilename, "http://", 7) == 0 ||
                 strncmp(osXSDFilename, "https://", 8) == 0 ||
                 VSIStatExL( osXSDFilename, &sXSDStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
            {
                bHasFoundXSD = TRUE;
            }
        }

        /* If not found, try if there is a schema in the gml_registry.xml */
        /* that might match a declared namespace and featuretype */
        if( !bHasFoundXSD )
        {
            GMLRegistry oRegistry;
            if( oRegistry.Parse() )
            {
                CPLString osHeader(szHeader);
                for( size_t iNS = 0; iNS < oRegistry.aoNamespaces.size(); iNS ++ )
                {
                    GMLRegistryNamespace& oNamespace = oRegistry.aoNamespaces[iNS];
                    const char* pszNSToFind =
                                CPLSPrintf("xmlns:%s", oNamespace.osPrefix.c_str());
                    const char* pszURIToFind =
                                CPLSPrintf("\"%s\"", oNamespace.osURI.c_str());
                    /* Case sensitive comparison since below test that also */
                    /* uses the namespace prefix is case sensitive */
                    if( osHeader.find(pszNSToFind) != std::string::npos &&
                        strstr(szHeader, pszURIToFind) != NULL )
                    {
                        if( oNamespace.bUseGlobalSRSName )
                            bUseGlobalSRSName = TRUE;
                        
                        for( size_t iTypename = 0;
                                    iTypename < oNamespace.aoFeatureTypes.size();
                                    iTypename ++ )
                        {
                            const char* pszElementToFind = NULL;
                            
                            GMLRegistryFeatureType& oFeatureType =
                                        oNamespace.aoFeatureTypes[iTypename];
                            
                            if ( oFeatureType.osElementValue.size() ) 
                                pszElementToFind = CPLSPrintf("%s:%s>%s",
                                                              oNamespace.osPrefix.c_str(),
                                                              oFeatureType.osElementName.c_str(),
                                                              oFeatureType.osElementValue.c_str());
                            else
                                pszElementToFind = CPLSPrintf("%s:%s",
                                                              oNamespace.osPrefix.c_str(),
                                                              oFeatureType.osElementName.c_str());

                            /* Case sensitive test since in a CadastralParcel feature */
                            /* there is a property basicPropertyUnit xlink, not to be */
                            /* confused with a top-level BasicPropertyUnit feature... */
                            if( osHeader.find(pszElementToFind) != std::string::npos )
                            {
                                if( oFeatureType.osSchemaLocation.size() )
                                {
                                    osXSDFilename = oFeatureType.osSchemaLocation;
                                    if( strncmp(osXSDFilename, "http://", 7) == 0 ||
                                        strncmp(osXSDFilename, "https://", 8) == 0 ||
                                        VSIStatExL( osXSDFilename, &sXSDStatBuf,
                                                    VSI_STAT_EXISTS_FLAG ) == 0 )
                                    {
                                        bHasFoundXSD = TRUE;
                                        bHaveSchema = TRUE;
                                        CPLDebug("GML", "Found %s for %s:%s in registry",
                                                osXSDFilename.c_str(), 
                                                oNamespace.osPrefix.c_str(),
                                                oFeatureType.osElementName.c_str());
                                    }
                                    else
                                    {
                                        CPLDebug("GML", "Cannot open %s", osXSDFilename.c_str());
                                    }
                                }
                                else
                                {
                                    bHaveSchema = poReader->LoadClasses(
                                        oFeatureType.osGFSSchemaLocation );
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
            char* pszSchemaLocationTmp1 = CPLStrdup(pszSchemaLocation + 1);
            int nTruncLen = (int)(strchr(pszSchemaLocation + 1, pszSchemaLocation[0]) - (pszSchemaLocation + 1));
            pszSchemaLocationTmp1[nTruncLen] = '\0';
            char* pszSchemaLocationTmp2 = CPLUnescapeString(
                pszSchemaLocationTmp1, NULL, CPLES_XML);
            CPLString osEscaped = ReplaceSpaceByPct20IfNeeded(pszSchemaLocationTmp2);
            CPLFree(pszSchemaLocationTmp2);
            pszSchemaLocationTmp2 = CPLStrdup(osEscaped);
            if (pszSchemaLocationTmp2)
            {
                /* pszSchemaLocationTmp2 is of the form : */
                /* http://namespace1 http://namespace1_schema_location http://namespace2 http://namespace1_schema_location2 */
                /* So we try to find http://namespace1_schema_location that contains hints that it is the WFS application */
                /* schema, i.e. if it contains typename= and request=DescribeFeatureType */
                char** papszTokens = CSLTokenizeString2(pszSchemaLocationTmp2, " \r\n", 0);
                int nTokens = CSLCount(papszTokens);
                if ((nTokens % 2) == 0)
                {
                    for(int i=0;i<nTokens;i+=2)
                    {
                        const char* pszEscapedURL = papszTokens[i+1];
                        char* pszLocation = CPLUnescapeString(pszEscapedURL, NULL, CPLES_URL);
                        CPLString osLocation = pszLocation;
                        CPLFree(pszLocation);
                        if (osLocation.ifind("typename=") != std::string::npos &&
                            osLocation.ifind("request=DescribeFeatureType") != std::string::npos)
                        {
                            CPLString osTypeName = CPLURLGetValue(osLocation, "typename");
                            papszTypeNames = CSLTokenizeString2( osTypeName, ",", 0);

                            if (!bHasFoundXSD && CPLHTTPEnabled() &&
                                CSLTestBoolean(CPLGetConfigOption("GML_DOWNLOAD_WFS_SCHEMA", "YES")))
                            {
                                CPLHTTPResult* psResult = CPLHTTPFetch(pszEscapedURL, NULL);
                                if (psResult)
                                {
                                    if (psResult->nStatus == 0 && psResult->pabyData != NULL)
                                    {
                                        bHasFoundXSD = TRUE;
                                        osXSDFilename =
                                            CPLSPrintf("/vsimem/tmp_gml_xsd_%p.xsd", this);
                                        VSILFILE* fpMem = VSIFileFromMemBuffer(
                                            osXSDFilename, psResult->pabyData,
                                            psResult->nDataLen, TRUE);
                                        VSIFCloseL(fpMem);
                                        psResult->pabyData = NULL;
                                    }
                                    CPLHTTPDestroyResult(psResult);
                                }
                            }
                        }
                    }
                }
                CSLDestroy(papszTokens);
            }
            CPLFree(pszSchemaLocationTmp2);
            CPLFree(pszSchemaLocationTmp1);
        }

        int bHasFeatureProperties = FALSE;
        if( bHasFoundXSD )
        {
            std::vector<GMLFeatureClass*> aosClasses;
            bHaveSchema = GMLParseXSD( osXSDFilename, aosClasses );
            if( bHaveSchema )
            {
                CPLDebug("GML", "Using %s", osXSDFilename.c_str());
                std::vector<GMLFeatureClass*>::const_iterator iter = aosClasses.begin();
                std::vector<GMLFeatureClass*>::const_iterator eiter = aosClasses.end();
                while (iter != eiter)
                {
                    GMLFeatureClass* poClass = *iter;

                    if( poClass->HasFeatureProperties() )
                    {
                        bHasFeatureProperties = TRUE;
                        break;
                    }
                    iter ++;
                }

                iter = aosClasses.begin();
                while (iter != eiter)
                {
                    GMLFeatureClass* poClass = *iter;
                    iter ++;

                    /* We have no way of knowing if the geometry type is 25D */
                    /* when examining the xsd only, so if there was a hint */
                    /* it is, we force to 25D */
                    if (bHas3D && poClass->GetGeometryPropertyCount() == 1)
                    {
                        poClass->GetGeometryProperty(0)->SetType(
                            wkbSetZ((OGRwkbGeometryType)poClass->GetGeometryProperty(0)->GetType()));
                    }

                    int bAddClass = TRUE;
                    /* If typenames are declared, only register the matching classes, in case */
                    /* the XSD contains more layers, but not if feature classes contain */
                    /* feature properties, in which case we will have embedded features that */
                    /* will be reported as top-level features */
                    if( papszTypeNames != NULL && !bHasFeatureProperties )
                    {
                        bAddClass = FALSE;
                        char** papszIter = papszTypeNames;
                        while (*papszIter && !bAddClass)
                        {
                            const char* pszTypeName = *papszIter;
                            if (strcmp(pszTypeName, poClass->GetName()) == 0)
                                bAddClass = TRUE;
                            papszIter ++;
                        }

                        /* Retry by removing prefixes */
                        if (!bAddClass)
                        {
                            papszIter = papszTypeNames;
                            while (*papszIter && !bAddClass)
                            {
                                const char* pszTypeName = *papszIter;
                                const char* pszColon = strchr(pszTypeName, ':');
                                if (pszColon)
                                {
                                    pszTypeName = pszColon + 1;
                                    if (strcmp(pszTypeName, poClass->GetName()) == 0)
                                    {
                                        poClass->SetName(pszTypeName);
                                        bAddClass = TRUE;
                                    }
                                }
                                papszIter ++;
                            }
                        }

                    }

                    if (bAddClass)
                        poReader->AddClass( poClass );
                    else
                        delete poClass;
                }

                poReader->SetClassListLocked( TRUE );
            }
        }

        if (bHaveSchema && bIsWFS)
        {
            /* For WFS, we can assume sequential layers */
            if (poReader->GetClassCount() > 1 && pszReadMode == NULL &&
                !bHasFeatureProperties)
            {
                CPLDebug("GML", "WFS output. Using SEQUENTIAL_LAYERS read mode");
                eReadMode = SEQUENTIAL_LAYERS;
            }
            /* Sometimes the returned schema contains only <xs:include> that we don't resolve */
            /* so ignore it */
            else if (poReader->GetClassCount() == 0)
                bHaveSchema = FALSE;
        }

        CSLDestroy(papszTypeNames);
    }

/* -------------------------------------------------------------------- */
/*      Force a first pass to establish the schema.  Eventually we      */
/*      will have mechanisms for remembering the schema and related     */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema )
    {
        if( !poReader->PrescanForSchema( TRUE, bAnalyzeSRSPerFeature ) )
        {
            // we assume an errors have been reported.
            return FALSE;
        }

        if( bHasFoundXSD )
        {
            CPLDebug("GML", "Generating %s file, ignoring %s",
                     osGFSFilename.c_str(), osXSDFilename.c_str());
        }
    }

    if (poReader->GetClassCount() > 1 && poReader->IsSequentialLayers() &&
        pszReadMode == NULL)
    {
        CPLDebug("GML", "Layers are monoblock. Using SEQUENTIAL_LAYERS read mode");
        eReadMode = SEQUENTIAL_LAYERS;
    }

/* -------------------------------------------------------------------- */
/*      Save the schema file if possible.  Don't make a fuss if we      */
/*      can't ... could be read-only directory or something.            */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema && !poReader->HasStoppedParsing() &&
        !EQUALN(pszFilename, "/vsitar/", strlen("/vsitar/")) &&
        !EQUALN(pszFilename, "/vsizip/", strlen("/vsizip/")) &&
        !EQUALN(pszFilename, "/vsigzip/vsi", strlen("/vsigzip/vsi")) &&
        !EQUALN(pszFilename, "/vsigzip//vsi", strlen("/vsigzip//vsi")) &&
        !EQUALN(pszFilename, "/vsicurl/", strlen("/vsicurl/")) &&
        !EQUALN(pszFilename, "/vsicurl_streaming/", strlen("/vsicurl_streaming/")))
    {
        VSILFILE    *fp = NULL;

        VSIStatBufL sGFSStatBuf;
        if( VSIStatExL( osGFSFilename, &sGFSStatBuf, VSI_STAT_EXISTS_FLAG ) != 0
            && (fp = VSIFOpenL( osGFSFilename, "wt" )) != NULL )
        {
            VSIFCloseL( fp );
            poReader->SaveClasses( osGFSFilename );
        }
        else
        {
            CPLDebug("GML", 
                     "Not saving %s files already exists or can't be created.",
                     osGFSFilename.c_str() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the GMLFeatureClasses into layers.                    */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRGMLLayer **)
        CPLCalloc( sizeof(OGRGMLLayer *), poReader->GetClassCount());
    nLayers = 0;

    if (poReader->GetClassCount() == 1 && nNumberOfFeatures != 0)
    {
        GMLFeatureClass *poClass = poReader->GetClass(0);
        int nFeatureCount = poClass->GetFeatureCount();
        if (nFeatureCount < 0)
        {
            poClass->SetFeatureCount(nNumberOfFeatures);
        }
        else if (nFeatureCount != nNumberOfFeatures)
        {
            CPLDebug("GML", "Feature count in header, and actual feature count don't match");
        }
    }

    if (bIsWFS && poReader->GetClassCount() == 1)
        bUseGlobalSRSName = TRUE;

    while( nLayers < poReader->GetClassCount() )
    {
        papoLayers[nLayers] = TranslateGMLSchema(poReader->GetClass(nLayers));
        nLayers++;
    }
    

    
    return TRUE;
}

/************************************************************************/
/*                         TranslateGMLSchema()                         */
/************************************************************************/

OGRGMLLayer *OGRGMLDataSource::TranslateGMLSchema( GMLFeatureClass *poClass )

{
    OGRGMLLayer *poLayer;

/* -------------------------------------------------------------------- */
/*      Create an empty layer.                                          */
/* -------------------------------------------------------------------- */

    const char* pszSRSName = poClass->GetSRSName();
    OGRSpatialReference* poSRS = NULL;
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
                OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode( "GEOGCS" );
                if( poGEOGCS != NULL )
                    poGEOGCS->StripNodes( "AXIS" );

                OGR_SRSNode *poPROJCS = poSRS->GetAttrNode( "PROJCS" );
                if (poPROJCS != NULL && poSRS->EPSGTreatsAsNorthingEasting())
                    poPROJCS->StripNodes( "AXIS" );

                if (!poClass->HasExtents() &&
                    sBoundingRect.IsInit())
                {
                    poClass->SetExtents(sBoundingRect.MinY,
                                        sBoundingRect.MaxY,
                                        sBoundingRect.MinX,
                                        sBoundingRect.MaxX);
                }
            }
        }

        if (!poClass->HasExtents() &&
            sBoundingRect.IsInit())
        {
            poClass->SetExtents(sBoundingRect.MinX,
                                sBoundingRect.MaxX,
                                sBoundingRect.MinY,
                                sBoundingRect.MaxY);
        }
    }

    /* Report a COMPD_CS only if GML_REPORT_COMPD_CS is explicitely set to TRUE */
    if( poSRS != NULL &&
        !CSLTestBoolean(CPLGetConfigOption("GML_REPORT_COMPD_CS", "FALSE")) )
    {
        OGR_SRSNode *poCOMPD_CS = poSRS->GetAttrNode( "COMPD_CS" );
        if( poCOMPD_CS != NULL )
        {
            OGR_SRSNode* poCandidateRoot = poCOMPD_CS->GetNode( "PROJCS" );
            if( poCandidateRoot == NULL )
                poCandidateRoot = poCOMPD_CS->GetNode( "GEOGCS" );
            if( poCandidateRoot != NULL )
            {
                poSRS->SetRoot(poCandidateRoot->Clone());
            }
        }
    }


    poLayer = new OGRGMLLayer( poClass->GetName(), FALSE, this );

/* -------------------------------------------------------------------- */
/*      Added attributes (properties).                                  */
/* -------------------------------------------------------------------- */
    if (bExposeGMLId)
    {
        OGRFieldDefn oField( "gml_id", OFTString );
        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }
    else if (bExposeFid)
    {
        OGRFieldDefn oField( "fid", OFTString );
        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }

    int iField;
    for( iField = 0; iField < poClass->GetGeometryPropertyCount(); iField++ )
    {
        GMLGeometryPropertyDefn *poProperty = poClass->GetGeometryProperty( iField );
        OGRGeomFieldDefn oField( poProperty->GetName(), (OGRwkbGeometryType)poProperty->GetType() );
        if( poClass->GetGeometryPropertyCount() == 1 && poClass->GetFeatureCount() == 0 )
        {
            oField.SetType(wkbUnknown);
        }
        oField.SetSpatialRef(poSRS);
        poLayer->GetLayerDefn()->AddGeomFieldDefn( &oField );
    }

    for( iField = 0; iField < poClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProperty = poClass->GetProperty( iField );
        OGRFieldType eFType;

        if( poProperty->GetType() == GMLPT_Untyped )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_String )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_Integer ||
                 poProperty->GetType() == GMLPT_Boolean ||
                 poProperty->GetType() == GMLPT_Short )
            eFType = OFTInteger;
        else if( poProperty->GetType() == GMLPT_Real ||
                 poProperty->GetType() == GMLPT_Float )
            eFType = OFTReal;
        else if( poProperty->GetType() == GMLPT_StringList )
            eFType = OFTStringList;
        else if( poProperty->GetType() == GMLPT_IntegerList ||
                 poProperty->GetType() == GMLPT_BooleanList )
            eFType = OFTIntegerList;
        else if( poProperty->GetType() == GMLPT_RealList )
            eFType = OFTRealList;
        else if( poProperty->GetType() == GMLPT_FeaturePropertyList )
            eFType = OFTStringList;
        else
            eFType = OFTString;
        
        OGRFieldDefn oField( poProperty->GetName(), eFType );
        if ( EQUALN(oField.GetNameRef(), "ogr:", 4) )
          oField.SetName(poProperty->GetName()+4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth( poProperty->GetWidth() );
        if( poProperty->GetPrecision() > 0 )
            oField.SetPrecision( poProperty->GetPrecision() );
        if( poProperty->GetType() == GMLPT_Boolean ||
            poProperty->GetType() == GMLPT_BooleanList )
            oField.SetSubType(OFSTBoolean);
        else if( poProperty->GetType() == GMLPT_Short) 
            oField.SetSubType(OFSTInt16);
        else if( poProperty->GetType() == GMLPT_Float) 
            oField.SetSubType(OFSTFloat32);

        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
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

int OGRGMLDataSource::Create( const char *pszFilename, 
                              char **papszOptions )

{
    if( fpOutput != NULL || poReader != NULL )
    {
        CPLAssert( FALSE );
        return FALSE;
    }

    if( strcmp(pszFilename,"/dev/stdout") == 0 )
        pszFilename = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*      Read options                                                    */
/* -------------------------------------------------------------------- */

    CSLDestroy(papszCreateOptions);
    papszCreateOptions = CSLDuplicate(papszOptions);

    const char* pszFormat = CSLFetchNameValue(papszCreateOptions, "FORMAT");
    bIsOutputGML3 = pszFormat && EQUAL(pszFormat, "GML3");
    bIsOutputGML3Deegree = pszFormat && EQUAL(pszFormat, "GML3Deegree");
    bIsOutputGML32 = pszFormat && EQUAL(pszFormat, "GML3.2");
    if (bIsOutputGML3Deegree || bIsOutputGML32)
        bIsOutputGML3 = TRUE;

    bIsLongSRSRequired =
        CSLTestBoolean(CSLFetchNameValueDef(papszCreateOptions, "GML3_LONGSRS", "YES"));

    bWriteSpaceIndentation =
        CSLTestBoolean(CSLFetchNameValueDef(papszCreateOptions, "SPACE_INDENTATION", "YES"));

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    pszName = CPLStrdup( pszFilename );
    osFilename = pszName;

    if( strcmp(pszFilename,"/vsistdout/") == 0 ||
        strncmp(pszFilename,"/vsigzip/", 9) == 0 )
    {
        fpOutput = VSIFOpenL(pszFilename, "wb");
        bFpOutputIsNonSeekable = TRUE;
        bFpOutputSingleFile = TRUE;
    }
    else if ( strncmp(pszFilename,"/vsizip/", 8) == 0)
    {
        if (EQUAL(CPLGetExtension(pszFilename), "zip"))
        {
            CPLFree(pszName);
            pszName = CPLStrdup(CPLFormFilename(pszFilename, "out.gml", NULL));
        }

        fpOutput = VSIFOpenL(pszName, "wb");
        bFpOutputIsNonSeekable = TRUE;
    }
    else
        fpOutput = VSIFOpenL( pszFilename, "wb+" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create GML file %s.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write out "standard" header.                                    */
/* -------------------------------------------------------------------- */
    PrintLine( fpOutput, "%s", 
                "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" );

    if (!bFpOutputIsNonSeekable)
        nSchemaInsertLocation = (int) VSIFTellL( fpOutput );

    const char* pszPrefix = GetAppPrefix();
    const char* pszTargetNameSpace = CSLFetchNameValueDef(papszOptions,"TARGET_NAMESPACE", "http://ogr.maptools.org/");

    if( RemoveAppPrefix() )
        PrintLine( fpOutput, "<FeatureCollection" );
    else
        PrintLine( fpOutput, "<%s:FeatureCollection", pszPrefix );

    if (IsGML32Output())
        PrintLine( fpOutput, "%s",
                "     gml:id=\"aFeatureCollection\"" );

/* -------------------------------------------------------------------- */
/*      Write out schema info if provided in creation options.          */
/* -------------------------------------------------------------------- */
    const char *pszSchemaURI = CSLFetchNameValue(papszOptions,"XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue( papszOptions, "XSISCHEMA" );

    if( pszSchemaURI != NULL )
    {
        PrintLine( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine( fpOutput, 
              "     xsi:schemaLocation=\"%s\"", 
                    pszSchemaURI );
    }
    else if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        char *pszBasename = CPLStrdup(CPLGetBasename( pszName ));

        PrintLine( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine( fpOutput, 
              "     xsi:schemaLocation=\"%s %s\"",
                    pszTargetNameSpace,
                    CPLResetExtension( pszBasename, "xsd" ) );
        CPLFree( pszBasename );
    }

    if( RemoveAppPrefix() )
        PrintLine( fpOutput,
                "     xmlns=\"%s\"", pszTargetNameSpace );
    else
        PrintLine( fpOutput,
                "     xmlns:%s=\"%s\"", pszPrefix, pszTargetNameSpace );

    if (IsGML32Output())
        PrintLine( fpOutput, "%s",
                "     xmlns:gml=\"http://www.opengis.net/gml/3.2\">" );
    else
        PrintLine( fpOutput, "%s",
                    "     xmlns:gml=\"http://www.opengis.net/gml\">" );

/* -------------------------------------------------------------------- */
/*      Should we initialize an area to place the boundedBy element?    */
/*      We will need to seek back to fill it in.                        */
/* -------------------------------------------------------------------- */
    nBoundedByLocation = -1;
    if( CSLFetchBoolean( papszOptions, "BOUNDEDBY", TRUE ))
    {
        if (!bFpOutputIsNonSeekable )
        {
            nBoundedByLocation = (int) VSIFTellL( fpOutput );

            if( nBoundedByLocation != -1 )
                PrintLine( fpOutput, "%350s", "" );
        }
        else
        {
            if (bWriteSpaceIndentation)
                VSIFPrintfL( fpOutput, "  ");
            if (IsGML3Output())
                PrintLine( fpOutput, "<gml:boundedBy><gml:Null /></gml:boundedBy>" );
            else
                PrintLine( fpOutput, "<gml:boundedBy><gml:null>missing</gml:null></gml:boundedBy>" );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGMLDataSource::ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                CPL_UNUSED char ** papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened for read access.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Ensure name is safe as an element name.                         */
/* -------------------------------------------------------------------- */
    char *pszCleanLayerName = CPLStrdup( pszLayerName );

    CPLCleanXMLElementName( pszCleanLayerName );
    if( strcmp(pszCleanLayerName,pszLayerName) != 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer name '%s' adjusted to '%s' for XML validity.",
                  pszLayerName, pszCleanLayerName );
    }

/* -------------------------------------------------------------------- */
/*      Set or check validity of global SRS.                            */
/* -------------------------------------------------------------------- */
    if (nLayers == 0)
    {
        if (poSRS)
            poWriteGlobalSRS = poSRS->Clone();
        bWriteGlobalSRS = TRUE;
    }
    else if( bWriteGlobalSRS )
    {
        if( poWriteGlobalSRS != NULL )
        {
            if (poSRS == NULL || !poSRS->IsSame(poWriteGlobalSRS))
            {
                delete poWriteGlobalSRS;
                poWriteGlobalSRS = NULL;
                bWriteGlobalSRS = FALSE;
            }
        }
        else
        {
            if( poSRS != NULL )
                bWriteGlobalSRS = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRGMLLayer *poLayer;

    poLayer = new OGRGMLLayer( pszCleanLayerName, TRUE, this );
    poLayer->GetLayerDefn()->SetGeomType(eType);
    if( eType != wkbNone )
    {
        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetName("geometryProperty");
        if( poSRS != NULL )
        {
            /* Clone it since mapogroutput assumes that it can destroys */
            /* the SRS it has passed to use, instead of deferencing it */
            poSRS = poSRS->Clone();
            poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Dereference();
        }
    }

    CPLFree( pszCleanLayerName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRGMLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRGMLLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return bIsOutputGML3;
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

void OGRGMLDataSource::GrowExtents( OGREnvelope3D *psGeomBounds, int nCoordDimension )

{
    sBoundingRect.Merge( *psGeomBounds );
    if (nCoordDimension == 3)
        bBBOX3D = TRUE;
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
    VSILFILE        *fpSchema;
    int         nSchemaStart = 0;

    if( bFpOutputSingleFile )
        return;

/* -------------------------------------------------------------------- */
/*      Do we want to write the schema within the GML instance doc      */
/*      or to a separate file?  For now we only support external.       */
/* -------------------------------------------------------------------- */
    const char *pszSchemaURI = CSLFetchNameValue(papszCreateOptions,
                                                 "XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue( papszCreateOptions, 
                                                  "XSISCHEMA" );

    if( pszSchemaURI != NULL )
        return;

    if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        const char *pszXSDFilename = CPLResetExtension( pszName, "xsd" );

        fpSchema = VSIFOpenL( pszXSDFilename, "wt" );
        if( fpSchema == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open file %.500s for schema output.", 
                      pszXSDFilename );
            return;
        }
        PrintLine( fpSchema, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" );
    }
    else if( EQUAL(pszSchemaOpt,"INTERNAL") )
    {
        if (fpOutput == NULL)
            return;
        nSchemaStart = (int) VSIFTellL( fpOutput );
        fpSchema = fpOutput;
    }
    else                                                               
        return;

/* ==================================================================== */
/*      Write the schema section at the end of the file.  Once          */
/*      complete, we will read it back in, and then move the whole      */
/*      file "down" enough to insert the schema at the beginning.       */
/* ==================================================================== */

/* ==================================================================== */
/*      Detect if there are fields of List types.                       */
/* ==================================================================== */
    int iLayer;
    int bHasListFields = FALSE;

    for( iLayer = 0; !bHasListFields && iLayer < GetLayerCount(); iLayer++ )
    {
        OGRFeatureDefn *poFDefn = GetLayer(iLayer)->GetLayerDefn();
        for( int iField = 0; !bHasListFields && iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( poFieldDefn->GetType() == OFTIntegerList ||
                poFieldDefn->GetType() == OFTRealList ||
                poFieldDefn->GetType() == OFTStringList )
            {
                bHasListFields = TRUE;
            }
        } /* next field */
    } /* next layer */

/* -------------------------------------------------------------------- */
/*      Emit the start of the schema section.                           */
/* -------------------------------------------------------------------- */
    const char* pszPrefix = GetAppPrefix();
    if( pszPrefix[0] == '\0' )
        pszPrefix = "ogr";
    const char* pszTargetNameSpace = CSLFetchNameValueDef(papszCreateOptions,"TARGET_NAMESPACE", "http://ogr.maptools.org/");

    if (IsGML3Output())
    {
        PrintLine( fpSchema,
                    "<xs:schema ");
        PrintLine( fpSchema,
                   "    targetNamespace=\"%s\"", pszTargetNameSpace );
        PrintLine( fpSchema,
                   "    xmlns:%s=\"%s\"",
                    pszPrefix, pszTargetNameSpace );
        PrintLine( fpSchema,
                   "    xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"");
        if (IsGML32Output())
        {
            PrintLine( fpSchema,
                   "    xmlns:gml=\"http://www.opengis.net/gml/3.2\"");
            PrintLine( fpSchema,
                    "    xmlns:gmlsf=\"http://www.opengis.net/gmlsf/2.0\"");
        }
        else
        {
            PrintLine( fpSchema,
                    "    xmlns:gml=\"http://www.opengis.net/gml\"");
            if (!IsGML3DeegreeOutput())
            {
                PrintLine( fpSchema,
                        "    xmlns:gmlsf=\"http://www.opengis.net/gmlsf\"");
            }
        }
        PrintLine( fpSchema,
                   "    elementFormDefault=\"qualified\"");
        PrintLine( fpSchema,
                   "    version=\"1.0\">");

        if (IsGML32Output())
        {
            PrintLine( fpSchema,
                    "<xs:annotation>");
            PrintLine( fpSchema,
                    "  <xs:appinfo source=\"http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd\">");
            PrintLine( fpSchema,
                    "    <gmlsf:ComplianceLevel>%d</gmlsf:ComplianceLevel>", (bHasListFields) ? 1 : 0);
            PrintLine( fpSchema,
                    "  </xs:appinfo>");
            PrintLine( fpSchema,
                    "</xs:annotation>");

            PrintLine( fpSchema,
                        "<xs:import namespace=\"http://www.opengis.net/gml/3.2\" schemaLocation=\"http://schemas.opengis.net/gml/3.2.1/gml.xsd\"/>" );
            PrintLine( fpSchema,
                        "<xs:import namespace=\"http://www.opengis.net/gmlsf/2.0\" schemaLocation=\"http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd\"/>" );
        }
        else
        {
            if (!IsGML3DeegreeOutput())
            {
                PrintLine( fpSchema,
                        "<xs:annotation>");
                PrintLine( fpSchema,
                        "  <xs:appinfo source=\"http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsfLevels.xsd\">");
                PrintLine( fpSchema,
                        "    <gmlsf:ComplianceLevel>%d</gmlsf:ComplianceLevel>", (bHasListFields) ? 1 : 0);
                PrintLine( fpSchema,
                        "    <gmlsf:GMLProfileSchema>http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsf.xsd</gmlsf:GMLProfileSchema>");
                PrintLine( fpSchema,
                        "  </xs:appinfo>");
                PrintLine( fpSchema,
                        "</xs:annotation>");
            }

            PrintLine( fpSchema,
                        "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengis.net/gml/3.1.1/base/gml.xsd\"/>" );
            if (!IsGML3DeegreeOutput())
            {
                PrintLine( fpSchema,
                            "<xs:import namespace=\"http://www.opengis.net/gmlsf\" schemaLocation=\"http://schemas.opengis.net/gml/3.1.1/profiles/gmlsfProfile/1.0.0/gmlsfLevels.xsd\"/>" );
            }
        }
    }
    else
    {
        PrintLine( fpSchema,
                    "<xs:schema targetNamespace=\"%s\" xmlns:%s=\"%s\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:gml=\"http://www.opengis.net/gml\" elementFormDefault=\"qualified\" version=\"1.0\">",
                    pszTargetNameSpace, pszPrefix, pszTargetNameSpace );

        PrintLine( fpSchema,
                    "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengis.net/gml/2.1.2/feature.xsd\"/>" );
    }

/* -------------------------------------------------------------------- */
/*      Define the FeatureCollection                                    */
/* -------------------------------------------------------------------- */
    if (IsGML3Output())
    {
        if (IsGML32Output())
        {
            PrintLine( fpSchema,
                        "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:AbstractGML\"/>",
                        pszPrefix );
        }
        else if (IsGML3DeegreeOutput())
        {
            PrintLine( fpSchema,
                        "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>",
                        pszPrefix );
        }
        else
        {
            PrintLine( fpSchema,
                        "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_GML\"/>",
                        pszPrefix );
        }

        PrintLine( fpSchema, "<xs:complexType name=\"FeatureCollectionType\">");
        PrintLine( fpSchema, "  <xs:complexContent>" );
        if (IsGML3DeegreeOutput())
        {
            PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">" );
            PrintLine( fpSchema, "      <xs:sequence>" );
            PrintLine( fpSchema, "        <xs:element name=\"featureMember\" minOccurs=\"0\" maxOccurs=\"unbounded\">" );
        }
        else
        {
            PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureType\">" );
            PrintLine( fpSchema, "      <xs:sequence minOccurs=\"0\" maxOccurs=\"unbounded\">" );
            PrintLine( fpSchema, "        <xs:element name=\"featureMember\">" );
        }
        PrintLine( fpSchema, "          <xs:complexType>" );
        if (IsGML32Output())
        {
            PrintLine( fpSchema, "            <xs:complexContent>" );
            PrintLine( fpSchema, "              <xs:extension base=\"gml:AbstractFeatureMemberType\">" );
            PrintLine( fpSchema, "                <xs:sequence>" );
            PrintLine( fpSchema, "                  <xs:element ref=\"gml:AbstractFeature\"/>" );
            PrintLine( fpSchema, "                </xs:sequence>" );
            PrintLine( fpSchema, "              </xs:extension>" );
            PrintLine( fpSchema, "            </xs:complexContent>" );
        }
        else
        {
            PrintLine( fpSchema, "            <xs:sequence>" );
            PrintLine( fpSchema, "              <xs:element ref=\"gml:_Feature\"/>" );
            PrintLine( fpSchema, "            </xs:sequence>" );
        }
        PrintLine( fpSchema, "          </xs:complexType>" );
        PrintLine( fpSchema, "        </xs:element>" );
        PrintLine( fpSchema, "      </xs:sequence>" );
        PrintLine( fpSchema, "    </xs:extension>" );
        PrintLine( fpSchema, "  </xs:complexContent>" );
        PrintLine( fpSchema, "</xs:complexType>" );
    }
    else
    {
        PrintLine( fpSchema,
                    "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>",
                    pszPrefix );

        PrintLine( fpSchema, "<xs:complexType name=\"FeatureCollectionType\">");
        PrintLine( fpSchema, "  <xs:complexContent>" );
        PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">" );
        PrintLine( fpSchema, "      <xs:attribute name=\"lockId\" type=\"xs:string\" use=\"optional\"/>" );
        PrintLine( fpSchema, "      <xs:attribute name=\"scope\" type=\"xs:string\" use=\"optional\"/>" );
        PrintLine( fpSchema, "    </xs:extension>" );
        PrintLine( fpSchema, "  </xs:complexContent>" );
        PrintLine( fpSchema, "</xs:complexType>" );
    }

/* ==================================================================== */
/*      Define the schema for each layer.                               */
/* ==================================================================== */

    for( iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        OGRFeatureDefn *poFDefn = GetLayer(iLayer)->GetLayerDefn();
        
/* -------------------------------------------------------------------- */
/*      Emit initial stuff for a feature type.                          */
/* -------------------------------------------------------------------- */
        if (IsGML32Output())
        {
            PrintLine(
                fpSchema,
                "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:AbstractFeature\"/>",
                poFDefn->GetName(), pszPrefix, poFDefn->GetName() );
        }
        else
        {
            PrintLine(
                fpSchema,
                "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:_Feature\"/>",
                poFDefn->GetName(), pszPrefix, poFDefn->GetName() );
        }

        PrintLine( fpSchema, "<xs:complexType name=\"%s_Type\">", poFDefn->GetName());
        PrintLine( fpSchema, "  <xs:complexContent>");
        PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureType\">");
        PrintLine( fpSchema, "      <xs:sequence>");

        for( int iGeomField = 0; iGeomField < poFDefn->GetGeomFieldCount(); iGeomField++ )
        {
            OGRGeomFieldDefn *poFieldDefn = poFDefn->GetGeomFieldDefn(iGeomField);

    /* -------------------------------------------------------------------- */
    /*      Define the geometry attribute.                                  */
    /* -------------------------------------------------------------------- */
            const char* pszGeometryTypeName = "GeometryPropertyType";
            const char* pszComment = "";
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

            PrintLine( fpSchema,
                "        <xs:element name=\"%s\" type=\"gml:%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\"/>%s",
                       poFieldDefn->GetNameRef(), pszGeometryTypeName, pszComment );
        }

/* -------------------------------------------------------------------- */
/*      Emit each of the attributes.                                    */
/* -------------------------------------------------------------------- */
        for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( IsGML3Output() && strcmp(poFieldDefn->GetNameRef(), "gml_id") == 0 )
                continue;
            else if( !IsGML3Output() && strcmp(poFieldDefn->GetNameRef(), "fid") == 0 )
                continue;

            if( poFieldDefn->GetType() == OFTInteger ||
                poFieldDefn->GetType() == OFTIntegerList  )
            {
                int nWidth;

                if( poFieldDefn->GetWidth() > 0 )
                    nWidth = poFieldDefn->GetWidth();
                else
                    nWidth = 16;

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"%s\">",
                           poFieldDefn->GetNameRef(), poFieldDefn->GetType() == OFTIntegerList ? "unbounded": "1" );
                PrintLine( fpSchema, "          <xs:simpleType>");
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    PrintLine( fpSchema, "            <xs:restriction base=\"xs:boolean\">");
                }
                else if( poFieldDefn->GetSubType() == OFSTInt16 )
                {
                    PrintLine( fpSchema, "            <xs:restriction base=\"xs:short\">");
                }
                else
                {
                    PrintLine( fpSchema, "            <xs:restriction base=\"xs:integer\">");
                    PrintLine( fpSchema, "              <xs:totalDigits value=\"%d\"/>", nWidth);
                }
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTReal ||
                     poFieldDefn->GetType() == OFTRealList  )
            {
                int nWidth, nDecimals;

                nWidth = poFieldDefn->GetWidth();
                nDecimals = poFieldDefn->GetPrecision();

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"%s\">",
                           poFieldDefn->GetNameRef(), poFieldDefn->GetType() == OFTRealList ? "unbounded": "1" );
                PrintLine( fpSchema, "          <xs:simpleType>");
                if( poFieldDefn->GetSubType() == OFSTFloat32 )
                    PrintLine( fpSchema, "            <xs:restriction base=\"xs:float\">");
                else
                    PrintLine( fpSchema, "            <xs:restriction base=\"xs:decimal\">");
                if (nWidth > 0)
                {
                    PrintLine( fpSchema, "              <xs:totalDigits value=\"%d\"/>", nWidth);
                    PrintLine( fpSchema, "              <xs:fractionDigits value=\"%d\"/>", nDecimals);
                }
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTString ||
                     poFieldDefn->GetType() == OFTStringList )
            {
                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"%s\">", 
                           poFieldDefn->GetNameRef(), poFieldDefn->GetType() == OFTStringList ? "unbounded": "1" );
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:string\">");
                if( poFieldDefn->GetWidth() != 0 )
                {
                    PrintLine( fpSchema, "              <xs:maxLength value=\"%d\"/>", poFieldDefn->GetWidth());
                }
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime )
            {
                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:string\">");
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else
            {
                /* TODO */
            }
        } /* next field */

/* -------------------------------------------------------------------- */
/*      Finish off feature type.                                        */
/* -------------------------------------------------------------------- */
        PrintLine( fpSchema, "      </xs:sequence>");
        PrintLine( fpSchema, "    </xs:extension>");
        PrintLine( fpSchema, "  </xs:complexContent>");
        PrintLine( fpSchema, "</xs:complexType>" );
    } /* next layer */

    PrintLine( fpSchema, "</xs:schema>" );

/* ==================================================================== */
/*      Move schema to the start of the file.                           */
/* ==================================================================== */
    if( fpSchema == fpOutput )
    {
/* -------------------------------------------------------------------- */
/*      Read the schema into memory.                                    */
/* -------------------------------------------------------------------- */
        int nSchemaSize = (int) VSIFTellL( fpOutput ) - nSchemaStart;
        char *pszSchema = (char *) CPLMalloc(nSchemaSize+1);
    
        VSIFSeekL( fpOutput, nSchemaStart, SEEK_SET );

        VSIFReadL( pszSchema, 1, nSchemaSize, fpOutput );
        pszSchema[nSchemaSize] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Move file data down by "schema size" bytes from after <?xml>    */
/*      header so we have room insert the schema.  Move in pretty       */
/*      big chunks.                                                     */
/* -------------------------------------------------------------------- */
        int nChunkSize = MIN(nSchemaStart-nSchemaInsertLocation,250000);
        char *pszChunk = (char *) CPLMalloc(nChunkSize);
        int nEndOfUnmovedData = nSchemaStart;

        for( nEndOfUnmovedData = nSchemaStart;
             nEndOfUnmovedData > nSchemaInsertLocation; )
        {
            int nBytesToMove = 
                MIN(nChunkSize, nEndOfUnmovedData - nSchemaInsertLocation );

            VSIFSeekL( fpOutput, nEndOfUnmovedData - nBytesToMove, SEEK_SET );
            VSIFReadL( pszChunk, 1, nBytesToMove, fpOutput );
            VSIFSeekL( fpOutput, nEndOfUnmovedData - nBytesToMove + nSchemaSize, 
                      SEEK_SET );
            VSIFWriteL( pszChunk, 1, nBytesToMove, fpOutput );
        
            nEndOfUnmovedData -= nBytesToMove;
        }

        CPLFree( pszChunk );

/* -------------------------------------------------------------------- */
/*      Write the schema in the opened slot.                            */
/* -------------------------------------------------------------------- */
        VSIFSeekL( fpOutput, nSchemaInsertLocation, SEEK_SET );
        VSIFWriteL( pszSchema, 1, nSchemaSize, fpOutput );

        VSIFSeekL( fpOutput, 0, SEEK_END );

        nBoundedByLocation += nSchemaSize;

        CPLFree(pszSchema);
    }
/* -------------------------------------------------------------------- */
/*      Close external schema files.                                    */
/* -------------------------------------------------------------------- */
    else
        VSIFCloseL( fpSchema );
}


/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void OGRGMLDataSource::PrintLine(VSILFILE* fp, const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start( args, fmt );
    osWork.vPrintf( fmt, args );
    va_end( args );

#ifdef WIN32
    const char* pszEOL = "\r\n";
#else
    const char* pszEOL = "\n";
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
                        OGRGMLSingleFeatureLayer(int nVal );
                        ~OGRGMLSingleFeatureLayer() { poFeatureDefn->Release(); }

    virtual void        ResetReading() { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) { return FALSE; }
};

/************************************************************************/
/*                      OGRGMLSingleFeatureLayer()                      */
/************************************************************************/

OGRGMLSingleFeatureLayer::OGRGMLSingleFeatureLayer( int nVal )
{
    poFeatureDefn = new OGRFeatureDefn( "SELECT" );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( "Validates", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oField );

    this->nVal = nVal;
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGRGMLSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId ++);
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
        int bIsValid = FALSE;
        if (osXSDFilename.size())
        {
            CPLErrorReset();
            bIsValid = CPLValidateXML(osFilename, osXSDFilename, NULL);
        }
        return new OGRGMLSingleFeatureLayer(bIsValid);
    }

    return OGRDataSource::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRGMLDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    delete poResultsSet;
}

/************************************************************************/
/*                          ExtractSRSName()                            */
/************************************************************************/

static int ExtractSRSName(const char* pszXML, char* szSRSName,
                          size_t sizeof_szSRSName)
{
    szSRSName[0] = '\0';

    const char* pszSRSName = strstr(pszXML, "srsName=\"");
    if( pszSRSName != NULL )
    {
        pszSRSName += 9;
        const char* pszEndQuote = strchr(pszSRSName, '"');
        if (pszEndQuote != NULL &&
            (size_t)(pszEndQuote - pszSRSName) < sizeof_szSRSName)
        {
            memcpy(szSRSName, pszSRSName, pszEndQuote - pszSRSName);
            szSRSName[pszEndQuote - pszSRSName] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                         FindAndParseBoundedBy()                      */
/************************************************************************/

void OGRGMLDataSource::FindAndParseBoundedBy(VSILFILE* fp)
{
    /* Build a shortened XML file that contain only the global */
    /* boundedBy element, so as to be able to parse it easily */

    char szStartTag[128];
    char* pszXML = (char*)CPLMalloc(8192 + 128 + 3 + 1);
    VSIFSeekL(fp, 0, SEEK_SET);
    int nRead = (int)VSIFReadL(pszXML, 1, 8192, fp);
    pszXML[nRead] = 0;

    const char* pszStartTag = strchr(pszXML, '<');
    if (pszStartTag != NULL)
    {
        while (pszStartTag != NULL && pszStartTag[1] == '?')
            pszStartTag = strchr(pszStartTag + 1, '<');

        if (pszStartTag != NULL)
        {
            pszStartTag ++;
            const char* pszEndTag = strchr(pszStartTag, ' ');
            if (pszEndTag != NULL && pszEndTag - pszStartTag < 128 )
            {
                memcpy(szStartTag, pszStartTag, pszEndTag - pszStartTag);
                szStartTag[pszEndTag - pszStartTag] = '\0';
            }
            else
                pszStartTag = NULL;
        }
    }

    char* pszEndBoundedBy = strstr(pszXML, "</wfs:boundedBy>");
    int bWFSBoundedBy = FALSE;
    if (pszEndBoundedBy != NULL)
        bWFSBoundedBy = TRUE;
    else
        pszEndBoundedBy = strstr(pszXML, "</gml:boundedBy>");
    if (pszStartTag != NULL && pszEndBoundedBy != NULL)
    {
        const char* pszSRSName = NULL;
        char szSRSName[128];

        szSRSName[0] = '\0';

        /* Find a srsName somewhere for some WFS 2.0 documents */
        /* that have not it set at the <wfs:boundedBy> element */
        /* e.g. http://geoserv.weichand.de:8080/geoserver/wfs?SERVICE=WFS&REQUEST=GetFeature&VERSION=2.0.0&TYPENAME=bvv:gmd_ex */
        if( bIsWFS )
        {
            ExtractSRSName(pszXML, szSRSName, sizeof(szSRSName));
        }

        pszEndBoundedBy[strlen("</gml:boundedBy>")] = '\0';
        strcat(pszXML, "</");
        strcat(pszXML, szStartTag);
        strcat(pszXML, ">");

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLXMLNode* psXML = CPLParseXMLString(pszXML);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (psXML != NULL)
        {
            CPLXMLNode* psBoundedBy = NULL;
            CPLXMLNode* psIter = psXML;
            while(psIter != NULL)
            {
                psBoundedBy = CPLGetXMLNode(psIter, bWFSBoundedBy ?
                                        "wfs:boundedBy" : "gml:boundedBy");
                if (psBoundedBy != NULL)
                    break;
                psIter = psIter->psNext;
            }

            const char* pszLowerCorner = NULL;
            const char* pszUpperCorner = NULL;
            if (psBoundedBy != NULL)
            {
                CPLXMLNode* psEnvelope = CPLGetXMLNode(psBoundedBy, "gml:Envelope");
                if (psEnvelope)
                {
                    pszSRSName = CPLGetXMLValue(psEnvelope, "srsName", NULL);
                    pszLowerCorner = CPLGetXMLValue(psEnvelope, "gml:lowerCorner", NULL);
                    pszUpperCorner = CPLGetXMLValue(psEnvelope, "gml:upperCorner", NULL);
                }
            }

            if( bIsWFS && pszSRSName == NULL &&
                pszLowerCorner != NULL && pszUpperCorner != NULL &&
                szSRSName[0] != '\0' )
            {
                pszSRSName = szSRSName;
            }

            if (pszSRSName != NULL && pszLowerCorner != NULL && pszUpperCorner != NULL)
            {
                char** papszLC = CSLTokenizeString(pszLowerCorner);
                char** papszUC = CSLTokenizeString(pszUpperCorner);
                if (CSLCount(papszLC) >= 2 && CSLCount(papszUC) >= 2)
                {
                    CPLDebug("GML", "Global SRS = %s", pszSRSName);

                    if (strncmp(pszSRSName, "http://www.opengis.net/gml/srs/epsg.xml#", 40) == 0)
                    {
                        std::string osWork;
                        osWork.assign("EPSG:", 5);
                        osWork.append(pszSRSName+40);
                        poReader->SetGlobalSRSName(osWork.c_str());
                    }
                    else
                        poReader->SetGlobalSRSName(pszSRSName);

                    double dfMinX = CPLAtofM(papszLC[0]);
                    double dfMinY = CPLAtofM(papszLC[1]);
                    double dfMaxX = CPLAtofM(papszUC[0]);
                    double dfMaxY = CPLAtofM(papszUC[1]);

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

void OGRGMLDataSource::SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{
    sBoundingRect.MinX = dfMinX;
    sBoundingRect.MinY = dfMinY;
    sBoundingRect.MaxX = dfMaxX;
    sBoundingRect.MaxY = dfMaxY;
}

/************************************************************************/
/*                             GetAppPrefix()                           */
/************************************************************************/

const char* OGRGMLDataSource::GetAppPrefix()
{
    return CSLFetchNameValueDef(papszCreateOptions, "PREFIX", "ogr");
}

/************************************************************************/
/*                            RemoveAppPrefix()                         */
/************************************************************************/

int OGRGMLDataSource::RemoveAppPrefix()
{
    if( CSLTestBoolean(CSLFetchNameValueDef(
            papszCreateOptions, "STRIP_PREFIX", "FALSE")) )
        return TRUE;
    const char* pszPrefix = GetAppPrefix();
    return( pszPrefix[0] == '\0' );
}

/************************************************************************/
/*                        WriteFeatureBoundedBy()                       */
/************************************************************************/

int OGRGMLDataSource::WriteFeatureBoundedBy()
{
    return CSLTestBoolean(CSLFetchNameValueDef(
                    papszCreateOptions, "WRITE_FEATURE_BOUNDED_BY", "TRUE"));
}

/************************************************************************/
/*                          GetSRSDimensionLoc()                        */
/************************************************************************/

const char* OGRGMLDataSource::GetSRSDimensionLoc()
{
    return CSLFetchNameValue(papszCreateOptions, "SRSDIMENSION_LOC");
}
