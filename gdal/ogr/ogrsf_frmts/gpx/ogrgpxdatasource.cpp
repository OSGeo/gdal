/******************************************************************************
 * $Id$
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_gpx.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

#define SPACE_FOR_METADATA 160

/************************************************************************/
/*                          OGRGPXDataSource()                          */
/************************************************************************/

OGRGPXDataSource::OGRGPXDataSource()

{
    lastGPXGeomTypeWritten = GPX_NONE;
    bUseExtensions = FALSE;
    pszExtensionsNS = NULL;

    papoLayers = NULL;
    nLayers = 0;
    
    fpOutput = NULL;
    nOffsetBounds = -1;
    dfMinLat = 90;
    dfMinLon = 180;
    dfMaxLat = -90;
    dfMaxLon = -180;

    pszName = NULL;
    pszVersion = NULL;

    bIsBackSeekable = TRUE;
    pszEOL = "\n";

    nLastRteId = -1;
    nLastTrkId = -1;
    nLastTrkSegId = -1;
}

/************************************************************************/
/*                         ~OGRGPXDataSource()                          */
/************************************************************************/

OGRGPXDataSource::~OGRGPXDataSource()

{
    if ( fpOutput != NULL )
    {
        if (nLastRteId != -1)
            PrintLine("</rte>");
        else if (nLastTrkId != -1)
        {
            PrintLine("  </trkseg>");
            PrintLine("</trk>");
        }
        PrintLine("</gpx>");
        if ( bIsBackSeekable )
        {
            /* Write the <bound> element in the reserved space */
            if (dfMinLon <= dfMaxLon)
            {
                char szMetadata[SPACE_FOR_METADATA+1];
                int nRet = snprintf(szMetadata, SPACE_FOR_METADATA,
                         "<metadata><bounds minlat=\"%.15f\" minlon=\"%.15f\" maxlat=\"%.15f\" maxlon=\"%.15f\"/></metadata>",
                        dfMinLat, dfMinLon, dfMaxLat, dfMaxLon);
                if (nRet < SPACE_FOR_METADATA)
                {
                    VSIFSeekL(fpOutput, nOffsetBounds, SEEK_SET);
                    VSIFWriteL(szMetadata, 1, strlen(szMetadata), fpOutput);
                }
            }
            VSIFCloseL( fpOutput);
        }
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszExtensionsNS );
    CPLFree( pszName );
    CPLFree( pszVersion );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPXDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return FALSE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGPXDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRGPXDataSource::ICreateLayer( const char * pszLayerName,
                                          OGRSpatialReference *poSRS,
                                          OGRwkbGeometryType eType,
                                          char ** papszOptions )

{
    GPXGeometryType gpxGeomType;
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
        const char *pszForceGPXTrack = CSLFetchNameValue( papszOptions, "FORCE_GPX_TRACK");
        if (pszForceGPXTrack && CSLTestBoolean(pszForceGPXTrack))
            gpxGeomType = GPX_TRACK;
        else
            gpxGeomType = GPX_ROUTE;
    }
    else if (eType == wkbMultiLineString || eType == wkbMultiLineString25D)
    {
        const char *pszForceGPXRoute = CSLFetchNameValue( papszOptions, "FORCE_GPX_ROUTE");
        if (pszForceGPXRoute && CSLTestBoolean(pszForceGPXRoute))
            gpxGeomType = GPX_ROUTE;
        else
            gpxGeomType = GPX_TRACK;
    }
    else if (eType == wkbUnknown)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create GPX layer %s with unknown geometry type", pszLayerName);
        return NULL;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                    "Geometry type of `%s' not supported in GPX.\n",
                    OGRGeometryTypeToName(eType) );
        return NULL;
    }
    nLayers++;
    papoLayers = (OGRGPXLayer **) CPLRealloc(papoLayers, nLayers * sizeof(OGRGPXLayer*));
    papoLayers[nLayers-1] = new OGRGPXLayer( pszName, pszLayerName, gpxGeomType, this, TRUE );
    
    return papoLayers[nLayers-1];
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                startElementValidateCbk()                             */
/************************************************************************/

void OGRGPXDataSource::startElementValidateCbk(const char *pszName, const char **ppszAttr)
{
    if (validity == GPX_VALIDITY_UNKNOWN)
    {
        if (strcmp(pszName, "gpx") == 0)
        {
            int i;
            validity = GPX_VALIDITY_VALID;
            for(i=0; ppszAttr[i] != NULL; i+= 2)
            {
                if (strcmp(ppszAttr[i], "version") == 0)
                {
                    pszVersion = CPLStrdup(ppszAttr[i+1]);
                    break;
                }
            }
        }
        else
        {
            validity = GPX_VALIDITY_INVALID;
        }
    }
    else if (validity == GPX_VALIDITY_VALID)
    {
        if (strcmp(pszName, "extensions") == 0)
        {
            bUseExtensions = TRUE;
        }
        nElementsRead++;
    }
}


/************************************************************************/
/*                      dataHandlerValidateCbk()                        */
/************************************************************************/

void OGRGPXDataSource::dataHandlerValidateCbk(const char *data, int nLen)
{
    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File probably corrupted (million laugh pattern)");
        XML_StopParser(oCurrentParser, XML_FALSE);
    }
}


static void XMLCALL startElementValidateCbk(void *pUserData, const char *pszName, const char **ppszAttr)
{
    OGRGPXDataSource* poDS = (OGRGPXDataSource*) pUserData;
    poDS->startElementValidateCbk(pszName, ppszAttr);
}

static void XMLCALL dataHandlerValidateCbk(void *pUserData, const char *data, int nLen)
{
    OGRGPXDataSource* poDS = (OGRGPXDataSource*) pUserData;
    poDS->dataHandlerValidateCbk(data, nLen);
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGPXDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GPX driver does not support opening a file in update mode");
        return FALSE;
    }
#ifdef HAVE_EXPAT
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Try to open the file.                                           */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszFilename, "r");
    if (fp == NULL)
        return FALSE;
    
    validity = GPX_VALIDITY_UNKNOWN;
    CPLFree(pszVersion);
    pszVersion = NULL;
    bUseExtensions = FALSE;
    nElementsRead = 0;
    
    XML_Parser oParser = OGRCreateExpatXMLParser();
    oCurrentParser = oParser;
    XML_SetUserData(oParser, this);
    XML_SetElementHandler(oParser, ::startElementValidateCbk, NULL);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerValidateCbk);
    
    char aBuf[BUFSIZ];
    int nDone;
    unsigned int nLen;
    int nCount = 0;
    
    /* Begin to parse the file and look for the <gpx> element */
    /* It *MUST* be the first element of an XML file */
    /* So once we have read the first element, we know if we can */
    /* handle the file or not with that driver */
    do
    {
        nDataHandlerCounter = 0;
        nLen = (unsigned int) VSIFReadL( aBuf, 1, sizeof(aBuf), fp );
        nDone = VSIFEofL(fp);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            if (nLen <= BUFSIZ-1)
                aBuf[nLen] = 0;
            else
                aBuf[BUFSIZ-1] = 0;
            if (strstr(aBuf, "<?xml") && strstr(aBuf, "<gpx"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "XML parsing of GPX file failed : %s at line %d, column %d",
                        XML_ErrorString(XML_GetErrorCode(oParser)),
                        (int)XML_GetCurrentLineNumber(oParser),
                        (int)XML_GetCurrentColumnNumber(oParser));
            }
            validity = GPX_VALIDITY_INVALID;
            break;
        }
        if (validity == GPX_VALIDITY_INVALID)
        {
            break;
        }
        else if (validity == GPX_VALIDITY_VALID)
        {
            /* If we have recognized the <gpx> element, now we try */
            /* to recognize if they are <extensions> tags */
            /* But we stop to look for after an arbitrary number of tags */
            if (bUseExtensions)
                break;
            else if (nElementsRead > 200)
                break;
        }
        else
        {
            /* After reading 50 * BUFSIZE bytes, and not finding whether the file */
            /* is GPX or not, we give up and fail silently */
            nCount ++;
            if (nCount == 50)
                break;
        }
    } while (!nDone && nLen > 0 );
    
    XML_ParserFree(oParser);
    
    VSIFCloseL(fp);
    
    if (validity == GPX_VALIDITY_VALID)
    {
        CPLDebug("GPX", "%s seems to be a GPX file.", pszFilename);
        if (bUseExtensions)
            CPLDebug("GPX", "It uses <extensions>");

        if (pszVersion == NULL)
        {
            /* Default to 1.1 */
            CPLError(CE_Warning, CPLE_AppDefined, "GPX schema version is unknown. "
                     "The driver may not be able to handle the file correctly and will behave as if it is GPX 1.1.");
            pszVersion = CPLStrdup("1.1");
        }
        else if (strcmp(pszVersion, "1.0") == 0 || strcmp(pszVersion, "1.1") == 0)
        {
            /* Fine */
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GPX schema version '%s' is not handled by the driver. "
                     "The driver may not be able to handle the file correctly and will behave as if it is GPX 1.1.", pszVersion);
        }

        nLayers = 5;
        papoLayers = (OGRGPXLayer **) CPLRealloc(papoLayers, nLayers * sizeof(OGRGPXLayer*));
        papoLayers[0] = new OGRGPXLayer( pszName, "waypoints", GPX_WPT, this, FALSE );
        papoLayers[1] = new OGRGPXLayer( pszName, "routes", GPX_ROUTE, this, FALSE );
        papoLayers[2] = new OGRGPXLayer( pszName, "tracks", GPX_TRACK, this, FALSE );
        papoLayers[3] = new OGRGPXLayer( pszName, "route_points", GPX_ROUTE_POINT, this, FALSE );
        papoLayers[4] = new OGRGPXLayer( pszName, "track_points", GPX_TRACK_POINT, this, FALSE );
    }

    return (validity == GPX_VALIDITY_VALID);
#else
    char aBuf[256];
    VSILFILE* fp = VSIFOpenL(pszFilename, "r");
    if (fp)
    {
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, 255, fp );
        aBuf[nLen] = 0;
        if (strstr(aBuf, "<?xml") && strstr(aBuf, "<gpx"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GPX driver has not been built with read support. Expat library required");
        }
        VSIFCloseL(fp);
    }
    return FALSE;
#endif
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRGPXDataSource::Create( const char *pszFilename, 
                              char **papszOptions )
{
    if( fpOutput != NULL)
    {
        CPLAssert( FALSE );
        return FALSE;
    }

    if (strcmp(pszFilename, "/dev/stdout") == 0)
        pszFilename = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*     Do not override exiting file.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "You have to delete %s before being able to create it with the GPX driver",
                 pszFilename);
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */

    pszName = CPLStrdup( pszFilename );

    if( strcmp(pszName, "/vsistdout/") == 0 )
    {
        bIsBackSeekable = FALSE;
        fpOutput = VSIFOpenL( pszFilename, "w" );
    }
    else
        fpOutput = VSIFOpenL( pszFilename, "w+" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create GPX file %s.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      End of line character.                                          */
/* -------------------------------------------------------------------- */
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");

    int bUseCRLF;
    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = TRUE;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = FALSE;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    pszEOL = (bUseCRLF) ? "\r\n" : "\n";

/* -------------------------------------------------------------------- */
/*      Look at use extensions options.                                 */
/* -------------------------------------------------------------------- */
    const char* pszUseExtensions = CSLFetchNameValue( papszOptions, "GPX_USE_EXTENSIONS");
    const char* pszExtensionsNSURL = NULL;
    if (pszUseExtensions && CSLTestBoolean(pszUseExtensions))
    {
        bUseExtensions = TRUE;

        const char* pszExtensionsNSOption = CSLFetchNameValue( papszOptions, "GPX_EXTENSIONS_NS");
        const char* pszExtensionsNSURLOption = CSLFetchNameValue( papszOptions, "GPX_EXTENSIONS_NS_URL");
        if (pszExtensionsNSOption && pszExtensionsNSURLOption)
        {
            pszExtensionsNS = CPLStrdup(pszExtensionsNSOption);
            pszExtensionsNSURL = pszExtensionsNSURLOption;
        }
        else
        {
            pszExtensionsNS = CPLStrdup("ogr");
            pszExtensionsNSURL = "http://osgeo.org/gdal";
        }
    }
    
/* -------------------------------------------------------------------- */
/*     Output header of GPX file.                                       */
/* -------------------------------------------------------------------- */
    PrintLine("<?xml version=\"1.0\"?>");
    VSIFPrintfL(fpOutput, "<gpx version=\"1.1\" creator=\"GDAL " GDAL_RELEASE_NAME "\" ");
    VSIFPrintfL(fpOutput, "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ");
    if (bUseExtensions)
        VSIFPrintfL(fpOutput, "xmlns:%s=\"%s\" ", pszExtensionsNS, pszExtensionsNSURL);
    VSIFPrintfL(fpOutput, "xmlns=\"http://www.topografix.com/GPX/1/1\" ");
    PrintLine("xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">");
    if (bIsBackSeekable)
    {
      /* Reserve space for <metadata><bounds/></metadata> */
      char szMetadata[SPACE_FOR_METADATA+1];
      memset(szMetadata, ' ', SPACE_FOR_METADATA);
      szMetadata[SPACE_FOR_METADATA] = '\0';
      nOffsetBounds = (int) VSIFTellL(fpOutput);
      PrintLine("%s", szMetadata);
    }

    return TRUE;
}

/************************************************************************/
/*                             AddCoord()                               */
/************************************************************************/

void OGRGPXDataSource::AddCoord(double dfLon, double dfLat)
{
    if (dfLon < dfMinLon) dfMinLon = dfLon;
    if (dfLat < dfMinLat) dfMinLat = dfLat;
    if (dfLon > dfMaxLon) dfMaxLon = dfLon;
    if (dfLat > dfMaxLat) dfMaxLat = dfLat;
}

/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void OGRGPXDataSource::PrintLine(const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start( args, fmt );
    osWork.vPrintf( fmt, args );
    va_end( args );

    VSIFPrintfL(fpOutput, "%s%s", osWork.c_str(), pszEOL);
}
