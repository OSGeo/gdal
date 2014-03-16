/******************************************************************************
 * $Id$
 *
 * Project:  GeoRSS Translator
 * Purpose:  Implements OGRGeoRSSDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_georss.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRGeoRSSDataSource()                          */
/************************************************************************/

OGRGeoRSSDataSource::OGRGeoRSSDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    fpOutput = NULL;

    pszName = NULL;

    eFormat = GEORSS_RSS;
    eGeomDialect = GEORSS_SIMPLE;
    bUseExtensions = FALSE;
    bWriteHeaderAndFooter = TRUE;
}

/************************************************************************/
/*                         ~OGRGeoRSSDataSource()                          */
/************************************************************************/

OGRGeoRSSDataSource::~OGRGeoRSSDataSource()

{
    if ( fpOutput != NULL )
    {
        if (bWriteHeaderAndFooter)
        {
            if (eFormat == GEORSS_RSS)
            {
                VSIFPrintfL(fpOutput, "  </channel>\n");
                VSIFPrintfL(fpOutput, "</rss>\n");
            }
            else
            {
                VSIFPrintfL(fpOutput, "</feed>\n");
            }
        }
        VSIFCloseL( fpOutput);
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoRSSDataSource::TestCapability( const char * pszCap )

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

OGRLayer *OGRGeoRSSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRGeoRSSDataSource::CreateLayer( const char * pszLayerName,
                                             OGRSpatialReference *poSRS,
                                             OGRwkbGeometryType eType,
                                             char ** papszOptions )

{
    if (fpOutput == NULL)
        return NULL;

    if (poSRS != NULL && eGeomDialect != GEORSS_GML)
    {
        OGRSpatialReference oSRS;
        oSRS.SetWellKnownGeogCS("WGS84");
        if (poSRS->IsSame(&oSRS) == FALSE)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "For a non GML dialect, only WGS84 SRS is supported");
            return NULL;
        }
    }

    nLayers++;
    papoLayers = (OGRGeoRSSLayer **) CPLRealloc(papoLayers, nLayers * sizeof(OGRGeoRSSLayer*));
    papoLayers[nLayers-1] = new OGRGeoRSSLayer( pszName, pszLayerName, this, poSRS, TRUE );
    
    return papoLayers[nLayers-1];
}

#ifdef HAVE_EXPAT
/************************************************************************/
/*                startElementValidateCbk()                             */
/************************************************************************/

void OGRGeoRSSDataSource::startElementValidateCbk(const char *pszName, const char **ppszAttr)
{
    if (validity == GEORSS_VALIDITY_UNKNOWN)
    {
        if (strcmp(pszName, "rss") == 0)
        {
            validity = GEORSS_VALIDITY_VALID;
            eFormat = GEORSS_RSS;
        }
        else if (strcmp(pszName, "feed") == 0)
        {
            validity = GEORSS_VALIDITY_VALID;
            eFormat = GEORSS_ATOM;
        }
        else if (strcmp(pszName, "rdf:RDF") == 0)
        {
            const char** ppszIter = ppszAttr;
            while(*ppszIter)
            {
                if (strcmp(*ppszIter, "xmlns:georss") == 0)
                {
                    validity = GEORSS_VALIDITY_VALID;
                    eFormat = GEORSS_RSS_RDF;
                }
                ppszIter += 2;
            }
        }
        else
        {
            validity = GEORSS_VALIDITY_INVALID;
        }
    }
}


/************************************************************************/
/*                      dataHandlerValidateCbk()                        */
/************************************************************************/

void OGRGeoRSSDataSource::dataHandlerValidateCbk(const char *data, int nLen)
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
    OGRGeoRSSDataSource* poDS = (OGRGeoRSSDataSource*) pUserData;
    poDS->startElementValidateCbk(pszName, ppszAttr);
}

static void XMLCALL dataHandlerValidateCbk(void *pUserData, const char *data, int nLen)
{
    OGRGeoRSSDataSource* poDS = (OGRGeoRSSDataSource*) pUserData;
    poDS->dataHandlerValidateCbk(data, nLen);
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGeoRSSDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GeoRSS driver does not support opening a file in update mode");
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
    
    validity = GEORSS_VALIDITY_UNKNOWN;
    
    XML_Parser oParser = OGRCreateExpatXMLParser();
    XML_SetUserData(oParser, this);
    XML_SetElementHandler(oParser, ::startElementValidateCbk, NULL);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerValidateCbk);
    oCurrentParser = oParser;
    
    char aBuf[BUFSIZ];
    int nDone;
    unsigned int nLen;
    int nCount = 0;
    
    /* Begin to parse the file and look for the <rss> or <feed> element */
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
            if (strstr(aBuf, "<?xml") && (strstr(aBuf, "<rss") || strstr(aBuf, "<feed")))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "XML parsing of GeoRSS file failed : %s at line %d, column %d",
                        XML_ErrorString(XML_GetErrorCode(oParser)),
                        (int)XML_GetCurrentLineNumber(oParser),
                        (int)XML_GetCurrentColumnNumber(oParser));
            }
            validity = GEORSS_VALIDITY_INVALID;
            break;
        }
        if (validity == GEORSS_VALIDITY_INVALID)
        {
            break;
        }
        else if (validity == GEORSS_VALIDITY_VALID)
        {
            break;
        }
        else
        {
            /* After reading 50 * BUFSIZ bytes, and not finding whether the file */
            /* is GeoRSS or not, we give up and fail silently */
            nCount ++;
            if (nCount == 50)
                break;
        }
    } while (!nDone && nLen > 0 );
    
    XML_ParserFree(oParser);
    
    VSIFCloseL(fp);
    
    if (validity == GEORSS_VALIDITY_VALID)
    {
        CPLDebug("GeoRSS", "%s seems to be a GeoRSS file.", pszFilename);

        nLayers = 1;
        papoLayers = (OGRGeoRSSLayer **) CPLRealloc(papoLayers, nLayers * sizeof(OGRGeoRSSLayer*));
        papoLayers[0] = new OGRGeoRSSLayer( pszName, "georss", this, NULL, FALSE );
    }

    return (validity == GEORSS_VALIDITY_VALID);
#else
    char aBuf[256];
    VSILFILE* fp = VSIFOpenL(pszFilename, "r");
    if (fp)
    {
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, 255, fp );
        aBuf[nLen] = 0;
        if (strstr(aBuf, "<?xml") && (strstr(aBuf, "<rss") || strstr(aBuf, "<feed")))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GeoRSS driver has not been built with read support. Expat library required");
        }
        VSIFCloseL(fp);
    }
    return FALSE;
#endif
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRGeoRSSDataSource::Create( const char *pszFilename, 
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
                 "You have to delete %s before being able to create it with the GeoRSS driver",
                 pszFilename);
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    pszName = CPLStrdup( pszFilename );

    fpOutput = VSIFOpenL( pszFilename, "w" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create GeoRSS file %s.", 
                  pszFilename );
        return FALSE;
    }

    const char* pszFormat = CSLFetchNameValue(papszOptions, "FORMAT");
    if (pszFormat)
    {
        if (EQUAL(pszFormat, "RSS"))
            eFormat = GEORSS_RSS;
        else if (EQUAL(pszFormat, "ATOM"))
            eFormat = GEORSS_ATOM;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for %s : %s", "FORMAT", pszFormat);
        }
    }

    const char* pszGeomDialect = CSLFetchNameValue(papszOptions, "GEOM_DIALECT");
    if (pszGeomDialect)
    {
        if (EQUAL(pszGeomDialect, "GML"))
            eGeomDialect = GEORSS_GML;
        else if (EQUAL(pszGeomDialect, "SIMPLE"))
            eGeomDialect = GEORSS_SIMPLE;
        else if (EQUAL(pszGeomDialect, "W3C_GEO"))
            eGeomDialect = GEORSS_W3C_GEO;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for %s : %s", "GEOM_DIALECT", pszGeomDialect);
        }
    }

    const char* pszWriteHeaderAndFooter = CSLFetchNameValue(papszOptions, "WRITE_HEADER_AND_FOOTER");
    if (pszWriteHeaderAndFooter && CSLTestBoolean(pszWriteHeaderAndFooter) == FALSE)
    {
        bWriteHeaderAndFooter = FALSE;
        return TRUE;
    }

    const char* pszHeader = NULL;
    const char* pszTitle = NULL;
    const char* pszDescription = NULL;
    const char* pszLink = NULL;
    const char* pszUpdated = NULL;
    const char* pszAuthorName = NULL;
    const char* pszId = NULL;

    pszHeader = CSLFetchNameValue(papszOptions, "HEADER");

    if (eFormat == GEORSS_RSS && pszHeader == NULL)
    {
        pszTitle = CSLFetchNameValue(papszOptions, "TITLE");
        if (pszTitle == NULL)
            pszTitle = "title";

        pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
        if (pszDescription == NULL)
            pszDescription = "channel_description";

        pszLink = CSLFetchNameValue(papszOptions, "LINK");
        if (pszLink == NULL)
            pszLink = "channel_link";

    }
    else if (eFormat == GEORSS_ATOM && pszHeader == NULL)
    {
        pszTitle = CSLFetchNameValue(papszOptions, "TITLE");
        if (pszTitle == NULL)
            pszTitle = "title";

        pszUpdated = CSLFetchNameValue(papszOptions, "UPDATED");
        if (pszUpdated == NULL)
            pszUpdated = "2009-01-01T00:00:00Z";

        pszAuthorName = CSLFetchNameValue(papszOptions, "AUTHOR_NAME");
        if (pszAuthorName == NULL)
            pszAuthorName = "author";

        pszId = CSLFetchNameValue(papszOptions, "ID");
        if (pszId == NULL)
            pszId = "id";
    }

    const char* pszUseExtensions = CSLFetchNameValue( papszOptions, "USE_EXTENSIONS");
    bUseExtensions =  (pszUseExtensions && CSLTestBoolean(pszUseExtensions));

/* -------------------------------------------------------------------- */
/*     Output header of GeoRSS file.                                       */
/* -------------------------------------------------------------------- */
    VSIFPrintfL(fpOutput, "<?xml version=\"1.0\"?>\n");
    if (eFormat == GEORSS_RSS)
    {
        VSIFPrintfL(fpOutput, "<rss version=\"2.0\" ");
        if (eGeomDialect == GEORSS_GML)
            VSIFPrintfL(fpOutput, "xmlns:georss=\"http://www.georss.org/georss\" xmlns:gml=\"http://www.opengis.net/gml\"");
        else if (eGeomDialect == GEORSS_SIMPLE)
            VSIFPrintfL(fpOutput, "xmlns:georss=\"http://www.georss.org/georss\"");
        else
            VSIFPrintfL(fpOutput, "xmlns:geo=\"http://www.w3.org/2003/01/geo/wgs84_pos#\"");
        VSIFPrintfL(fpOutput, ">\n");
        VSIFPrintfL(fpOutput, "  <channel>\n");
        if (pszHeader)
        {
            VSIFPrintfL(fpOutput, "%s", pszHeader);
        }
        else
        {
            VSIFPrintfL(fpOutput, "    <title>%s</title>\n", pszTitle);
            VSIFPrintfL(fpOutput, "    <description>%s</description>\n", pszDescription);
            VSIFPrintfL(fpOutput, "    <link>%s</link>\n", pszLink);
        }
    }
    else
    {
        VSIFPrintfL(fpOutput, "<feed xmlns=\"http://www.w3.org/2005/Atom\" ");
        if (eGeomDialect == GEORSS_GML)
            VSIFPrintfL(fpOutput, "xmlns:gml=\"http://www.opengis.net/gml\"");
        else if (eGeomDialect == GEORSS_SIMPLE)
            VSIFPrintfL(fpOutput, "xmlns:georss=\"http://www.georss.org/georss\"");
        else
            VSIFPrintfL(fpOutput, "xmlns:geo=\"http://www.w3.org/2003/01/geo/wgs84_pos#\"");
        VSIFPrintfL(fpOutput, ">\n");
        if (pszHeader)
        {
            VSIFPrintfL(fpOutput, "%s", pszHeader);
        }
        else
        {
            VSIFPrintfL(fpOutput, "  <title>%s</title>\n", pszTitle);
            VSIFPrintfL(fpOutput, "  <updated>%s</updated>\n", pszUpdated);
            VSIFPrintfL(fpOutput, "  <author><name>%s</name></author>\n", pszAuthorName);
            VSIFPrintfL(fpOutput, "  <id>%s</id>\n", pszId);
        }
    }

    return TRUE;
}
