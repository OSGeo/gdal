/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRGMLDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
#include "gmlutils.h"
#include "ogr_p.h"

#include <vector>

CPL_CVSID("$Id$");

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

    poGlobalSRS = NULL;
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
        PrintLine( fpOutput, "%s", 
                    "</ogr:FeatureCollection>" );

        if( bFpOutputIsNonSeekable)
        {
            VSIFCloseL( fpOutput );
            fpOutput = NULL;
        }

        InsertHeader();

        if( !bFpOutputIsNonSeekable
            && nBoundedByLocation != -1
            && sBoundingRect.IsInit() 
            && VSIFSeekL( fpOutput, nBoundedByLocation, SEEK_SET ) == 0 )
        {
            if (IsGML3Output())
            {
                int bCoordSwap = FALSE;
                char* pszSRSName;
                if (poGlobalSRS)
                    pszSRSName = GML_GetSRSName(poGlobalSRS, IsLongSRSRequired(), &bCoordSwap);
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
            else
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

    delete poGlobalSRS;

    delete poStoredGMLFeature;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGMLDataSource::Open( const char * pszNewName, int bTestOpen )

{
    VSILFILE   *fp;
    char        szHeader[2048];
    int         nNumberOfFeatures = 0;
    CPLString   osWithVsiGzip;

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszNewName, "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open GML file `%s'.", 
                      pszNewName );

        return FALSE;
    }

    int bExpatCompatibleEncoding = FALSE;
    int bHas3D = FALSE;
    int bHintConsiderEPSGAsURN = FALSE;

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is GML, load a header chunk and check      */
/*      for signs it is GML                                             */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        size_t nRead = VSIFReadL( szHeader, 1, sizeof(szHeader), fp );
        if (nRead <= 0)
        {
            VSIFCloseL( fp );
            return FALSE;
        }
        szHeader[MIN(nRead, sizeof(szHeader))-1] = '\0';

        /* Might be a OS-Mastermap gzipped GML, so let be nice and try to open */
        /* it transparently with /vsigzip/ */
        if ( ((GByte*)szHeader)[0] == 0x1f && ((GByte*)szHeader)[1] == 0x8b &&
             EQUAL(CPLGetExtension(pszNewName), "gz") &&
             strncmp(pszNewName, "/vsigzip/", strlen("/vsigzip/")) != 0 )
        {
            VSIFCloseL( fp );
            osWithVsiGzip = "/vsigzip/";
            osWithVsiGzip += pszNewName;

            pszNewName = osWithVsiGzip;

            fp = VSIFOpenL( pszNewName, "r" );
            if( fp == NULL )
                return FALSE;

            nRead = VSIFReadL( szHeader, 1, sizeof(szHeader), fp );
            if (nRead <= 0)
            {
                VSIFCloseL( fp );
                return FALSE;
            }
            szHeader[MIN(nRead, sizeof(szHeader))-1] = '\0';
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
                                        EQUALN(pszEncoding + 10, "ISO-8859-1", 10));
        else
            bExpatCompatibleEncoding = TRUE; /* utf-8 is the default */

        bHas3D = strstr(szPtr, "srsDimension=\"3\"") != NULL || strstr(szPtr, "<gml:Z>") != NULL;

/* -------------------------------------------------------------------- */
/*      Here, we expect the opening chevrons of GML tree root element   */
/* -------------------------------------------------------------------- */
        if( szPtr[0] != '<' 
            || strstr(szPtr,"opengis.net/gml") == NULL )
        {
            VSIFCloseL( fp );
            return FALSE;
        }

        /* Ignore GeoRSS documents. They will be recognized by the GeoRSS driver */
        if( strstr(szPtr, "<rss") != NULL && strstr(szPtr, "xmlns:georss") != NULL )
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
        else if (strncmp(pszNewName, "/vsimem/tempwfs_", strlen("/vsimem/tempwfs_")) == 0)
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
    }
    
/* -------------------------------------------------------------------- */
/*      We assume now that it is GML.  Close and instantiate a          */
/*      GMLReader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

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
                  pszNewName );
        return FALSE;
    }

    poReader->SetSourceFile( pszNewName );
    
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
        pszXlinkResolvedFilename = CPLStrdup( pszNewName );
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
                            CPLResetExtension( pszNewName, "resolved.gml" ) );

        // Check if the file already exists.
        VSIStatBufL sResStatBuf, sGMLStatBuf;
        if( VSIStatL( pszXlinkResolvedFilename, &sResStatBuf ) == 0 )
        {
            VSIStatL( pszNewName, &sGMLStatBuf );
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
    const char *pszGFSFilename;
    VSIStatBufL sGFSStatBuf, sGMLStatBuf;
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

/* -------------------------------------------------------------------- */
/*      Can we find a GML Feature Schema (.gfs) for the input file?     */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema )
    {
        pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
        if (strncmp(pszGFSFilename, "/vsigzip/", strlen("/vsigzip/")) == 0)
            pszGFSFilename += strlen("/vsigzip/");
        if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) == 0 )
        {
            VSIStatL( pszNewName, &sGMLStatBuf );
            if( sGMLStatBuf.st_mtime > sGFSStatBuf.st_mtime )
            {
                CPLDebug( "GML", 
                          "Found %s but ignoring because it appears\n"
                          "be older than the associated GML file.", 
                          pszGFSFilename );
            }
            else
            {
                bHaveSchema = poReader->LoadClasses( pszGFSFilename );
                if (bHaveSchema)
                {
                    const char *pszXSDFilename;
                    pszXSDFilename = CPLResetExtension( pszNewName, "xsd" );
                    if( VSIStatExL( pszXSDFilename, &sGMLStatBuf,
                                    VSI_STAT_EXISTS_FLAG ) == 0 )
                    {
                        CPLDebug("GML", "Using %s file, ignoring %s",
                                 pszGFSFilename, pszXSDFilename);
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
    const char *pszXSDFilename;
    int bHasFoundXSD = FALSE;

    if( !bHaveSchema )
    {
        pszXSDFilename = CPLResetExtension( pszNewName, "xsd" );
        if( VSIStatL( pszXSDFilename, &sGMLStatBuf ) == 0 )
        {
            bHasFoundXSD = TRUE;

            std::vector<GMLFeatureClass*> aosClasses;
            bHaveSchema = GMLParseXSD( pszXSDFilename, aosClasses );
            if (bHaveSchema)
            {
                std::vector<GMLFeatureClass*>::const_iterator iter = aosClasses.begin();
                std::vector<GMLFeatureClass*>::const_iterator eiter = aosClasses.end();
                while (iter != eiter)
                {
                    GMLFeatureClass* poClass = *iter;
                    iter ++;

                    /* We have no way of knowing if the geometry type is 25D */
                    /* when examining the xsd only, so if there was a hint */
                    /* it is, we force to 25D */
                    if (bHas3D)
                    {
                        poClass->SetGeometryType(
                            poClass->GetGeometryType() | wkb25DBit);
                    }
                    poReader->AddClass( poClass );
                }
                poReader->SetClassListLocked( TRUE );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Force a first pass to establish the schema.  Eventually we      */
/*      will have mechanisms for remembering the schema and related     */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema )
    {
        if( !poReader->PrescanForSchema( TRUE ) )
        {
            // we assume an errors have been reported.
            return FALSE;
        }

        if( bHasFoundXSD )
        {
            CPLDebug("GML", "Generating %s file, ignoring %s",
                     pszGFSFilename, pszXSDFilename);
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
        !EQUALN(pszNewName, "/vsitar/", strlen("/vsitar/")) &&
        !EQUALN(pszNewName, "/vsizip/", strlen("/vsizip/")) &&
        !EQUALN(pszNewName, "/vsigzip/vsi", strlen("/vsigzip/vsi")) &&
        !EQUALN(pszNewName, "/vsigzip//vsi", strlen("/vsigzip//vsi")) &&
        !EQUALN(pszNewName, "/vsicurl/", strlen("/vsicurl/")))
    {
        VSILFILE    *fp = NULL;

        pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
        if (strncmp(pszGFSFilename, "/vsigzip/", strlen("/vsigzip/")) == 0)
            pszGFSFilename += strlen("/vsigzip/");

        if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) != 0 
            && (fp = VSIFOpenL( pszGFSFilename, "wt" )) != NULL )
        {
            VSIFCloseL( fp );
            poReader->SaveClasses( pszGFSFilename );
        }
        else
        {
            CPLDebug("GML", 
                     "Not saving %s files already exists or can't be created.",
                     pszGFSFilename );
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
    OGRwkbGeometryType eGType 
        = (OGRwkbGeometryType) poClass->GetGeometryType();

    if( poClass->GetFeatureCount() == 0 )
        eGType = wkbUnknown;
    
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

    poLayer = new OGRGMLLayer( poClass->GetName(), poSRS, FALSE,
                               eGType, this );
    delete poSRS;

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

    for( int iField = 0; iField < poClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProperty = poClass->GetProperty( iField );
        OGRFieldType eFType;

        if( poProperty->GetType() == GMLPT_Untyped )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_String )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_Integer )
            eFType = OFTInteger;
        else if( poProperty->GetType() == GMLPT_Real )
            eFType = OFTReal;
        else if( poProperty->GetType() == GMLPT_StringList )
            eFType = OFTStringList;
        else if( poProperty->GetType() == GMLPT_IntegerList )
            eFType = OFTIntegerList;
        else if( poProperty->GetType() == GMLPT_RealList )
            eFType = OFTRealList;
        else
            eFType = OFTString;
        
        OGRFieldDefn oField( poProperty->GetName(), eFType );
        if ( EQUALN(oField.GetNameRef(), "ogr:", 4) )
          oField.SetName(poProperty->GetName()+4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth( poProperty->GetWidth() );
        if( poProperty->GetPrecision() > 0 )
            oField.SetPrecision( poProperty->GetPrecision() );

        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }

    return poLayer;
}

/************************************************************************/
/*                         GetGlobalSRSName()                           */
/************************************************************************/

const char *OGRGMLDataSource::GetGlobalSRSName()
{
    if (poReader->CanUseGlobalSRSName() || bIsWFS)
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

    PrintLine( fpOutput, "%s", 
                "<ogr:FeatureCollection" );

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
                    CSLFetchNameValue( papszOptions, "XSISCHEMAURI" ) );
    }
    else if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        char *pszBasename = CPLStrdup(CPLGetBasename( pszName ));

        PrintLine( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine( fpOutput, 
              "     xsi:schemaLocation=\"http://ogr.maptools.org/ %s\"", 
                    CPLResetExtension( pszBasename, "xsd" ) );
        CPLFree( pszBasename );
    }

    PrintLine( fpOutput, "%s", 
                "     xmlns:ogr=\"http://ogr.maptools.org/\"" );
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
    if( CSLFetchBoolean( papszOptions, "BOUNDEDBY", TRUE ) &&
        !bFpOutputIsNonSeekable )
    {
        nBoundedByLocation = (int) VSIFTellL( fpOutput );

        if( nBoundedByLocation != -1 )
            PrintLine( fpOutput, "%350s", "" );
    }
    else
        nBoundedByLocation = -1;

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGMLDataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )

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
            poGlobalSRS = poSRS->Clone();
    }
    else
    {
        if (poSRS == NULL ||
            (poGlobalSRS != NULL && poSRS->IsSame(poGlobalSRS)))
        {
            delete poGlobalSRS;
            poGlobalSRS = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRGMLLayer *poLayer;

    poLayer = new OGRGMLLayer( pszCleanLayerName, poSRS, TRUE, eType, this );

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

/* -------------------------------------------------------------------- */
/*      Emit the start of the schema section.                           */
/* -------------------------------------------------------------------- */
    const char *pszTargetNameSpace = "http://ogr.maptools.org/";
    const char *pszPrefix = "ogr";

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
                    "    <gmlsf:ComplianceLevel>0</gmlsf:ComplianceLevel>");
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
                        "    <gmlsf:ComplianceLevel>0</gmlsf:ComplianceLevel>");
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
    int iLayer;

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

/* -------------------------------------------------------------------- */
/*      Define the geometry attribute.                                  */
/* -------------------------------------------------------------------- */
        const char* pszGeometryTypeName = "GeometryPropertyType";
        switch(wkbFlatten(poFDefn->GetGeomType()))
        {
            case wkbPoint:
                pszGeometryTypeName = "PointPropertyType";
                break;
            case wkbLineString:
                if (IsGML3Output())
                    pszGeometryTypeName = "CurvePropertyType";
                else
                    pszGeometryTypeName = "LineStringPropertyType";
                break;
            case wkbPolygon:
                if (IsGML3Output())
                    pszGeometryTypeName = "SurfacePropertyType";
                else
                    pszGeometryTypeName = "PolygonPropertyType";
                break;
            case wkbMultiPoint:
                pszGeometryTypeName = "MultiPointPropertyType";
                break;
            case wkbMultiLineString:
                if (IsGML3Output())
                    pszGeometryTypeName = "MutliCurvePropertyType";
                else
                    pszGeometryTypeName = "MultiLineStringPropertyType";
                break;
            case wkbMultiPolygon:
                if (IsGML3Output())
                    pszGeometryTypeName = "MultiSurfacePropertyType";
                else
                    pszGeometryTypeName = "MultiPolygonPropertyType";
                break;
            case wkbGeometryCollection:
                pszGeometryTypeName = "MultiGeometryPropertyType";
                break;
            default:
                break;
        }

        if (poFDefn->GetGeomType() != wkbNone)
        {
            PrintLine( fpSchema,
                "        <xs:element name=\"geometryProperty\" type=\"gml:%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\"/>", pszGeometryTypeName );
        }
            
/* -------------------------------------------------------------------- */
/*      Emit each of the attributes.                                    */
/* -------------------------------------------------------------------- */
        for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( poFieldDefn->GetType() == OFTInteger )
            {
                int nWidth;

                if( poFieldDefn->GetWidth() > 0 )
                    nWidth = poFieldDefn->GetWidth();
                else
                    nWidth = 16;

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">", poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:integer\">");
                PrintLine( fpSchema, "              <xs:totalDigits value=\"%d\"/>", nWidth);
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTReal )
            {
                int nWidth, nDecimals;

                nWidth = poFieldDefn->GetWidth();
                nDecimals = poFieldDefn->GetPrecision();

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
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
            else if( poFieldDefn->GetType() == OFTString )
            {
                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
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
    if (EQUAL(pszSQLCommand, "SELECT ValidateSchema()"))
    {
        CPLString osXSDFilename = CPLResetExtension( pszName, "xsd" );
        int bIsValid = FALSE;
        CPLErrorReset();
        CPLXMLSchemaPtr pSchema = CPLLoadXMLSchema(osXSDFilename);
        if (pSchema)
        {
            bIsValid = CPLValidateXML(pszName, pSchema, NULL);
            CPLFreeXMLSchema(pSchema);
        }
        else
        {
            if (strstr(CPLGetLastErrorMsg(), "not implemented due to missing libxml2 support") == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot load %s", osXSDFilename.c_str());
            }
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
