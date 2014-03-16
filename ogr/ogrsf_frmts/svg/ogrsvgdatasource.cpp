/******************************************************************************
 * $Id$
 *
 * Project:  SVG Translator
 * Purpose:  Implements OGRSVGDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_svg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRSVGDataSource()                          */
/************************************************************************/

OGRSVGDataSource::OGRSVGDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    bIsCloudmade = FALSE;

    pszName = NULL;
}

/************************************************************************/
/*                         ~OGRSVGDataSource()                          */
/************************************************************************/

OGRSVGDataSource::~OGRSVGDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszName );
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSVGDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                startElementValidateCbk()                             */
/************************************************************************/

void OGRSVGDataSource::startElementValidateCbk(const char *pszName,
                                               const char **ppszAttr)
{
    if (eValidity == SVG_VALIDITY_UNKNOWN)
    {
        if (strcmp(pszName, "svg") == 0)
        {
            int i;
            eValidity = SVG_VALIDITY_VALID;
            for(i=0; ppszAttr[i] != NULL; i+= 2)
            {
                if (strcmp(ppszAttr[i], "xmlns:cm") == 0 &&
                    strcmp(ppszAttr[i+1], "http://cloudmade.com/") == 0)
                {
                    bIsCloudmade = TRUE;
                    break;
                }
            }
        }
        else
        {
            eValidity = SVG_VALIDITY_INVALID;
        }
    }
}


/************************************************************************/
/*                      dataHandlerValidateCbk()                        */
/************************************************************************/

void OGRSVGDataSource::dataHandlerValidateCbk(const char *data, int nLen)
{
    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oCurrentParser, XML_FALSE);
    }
}


static void XMLCALL startElementValidateCbk(void *pUserData,
                                            const char *pszName, const char **ppszAttr)
{
    OGRSVGDataSource* poDS = (OGRSVGDataSource*) pUserData;
    poDS->startElementValidateCbk(pszName, ppszAttr);
}

static void XMLCALL dataHandlerValidateCbk(void *pUserData, const char *data, int nLen)
{
    OGRSVGDataSource* poDS = (OGRSVGDataSource*) pUserData;
    poDS->dataHandlerValidateCbk(data, nLen);
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSVGDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/SVG driver does not support opening a file in update mode");
        return FALSE;
    }
#ifdef HAVE_EXPAT
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Try to open the file.                                           */
/* -------------------------------------------------------------------- */
    CPLString osFilename(pszFilename);
    if (EQUAL(CPLGetExtension(pszFilename), "svgz") &&
        strstr(pszFilename, "/vsigzip/") == NULL)
    {
        osFilename = CPLString("/vsigzip/") + pszFilename;
        pszFilename = osFilename.c_str();
    }

    VSILFILE* fp = VSIFOpenL(pszFilename, "r");
    if (fp == NULL)
        return FALSE;
    
    eValidity = SVG_VALIDITY_UNKNOWN;

    XML_Parser oParser = OGRCreateExpatXMLParser();
    oCurrentParser = oParser;
    XML_SetUserData(oParser, this);
    XML_SetElementHandler(oParser, ::startElementValidateCbk, NULL);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerValidateCbk);
    
    char aBuf[BUFSIZ];
    int nDone;
    unsigned int nLen;
    int nCount = 0;
    
    /* Begin to parse the file and look for the <svg> element */
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
            if (strstr(aBuf, "<?xml") && strstr(aBuf, "<svg"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "XML parsing of SVG file failed : %s at line %d, column %d",
                        XML_ErrorString(XML_GetErrorCode(oParser)),
                        (int)XML_GetCurrentLineNumber(oParser),
                        (int)XML_GetCurrentColumnNumber(oParser));
            }
            eValidity = SVG_VALIDITY_INVALID;
            break;
        }
        if (eValidity == SVG_VALIDITY_INVALID)
        {
            break;
        }
        else if (eValidity == SVG_VALIDITY_VALID)
        {
            break;
        }
        else
        {
            /* After reading 50 * BUFSIZE bytes, and not finding whether the file */
            /* is SVG or not, we give up and fail silently */
            nCount ++;
            if (nCount == 50)
                break;
        }
    } while (!nDone && nLen > 0 );

    XML_ParserFree(oParser);

    VSIFCloseL(fp);

    if (eValidity == SVG_VALIDITY_VALID)
    {
        if (bIsCloudmade)
        {
            nLayers = 3;
            papoLayers =(OGRSVGLayer **) CPLRealloc(papoLayers,
                                            nLayers * sizeof(OGRSVGLayer*));
            papoLayers[0] = new OGRSVGLayer( pszFilename, "points", SVG_POINTS, this );
            papoLayers[1] = new OGRSVGLayer( pszFilename, "lines", SVG_LINES, this );
            papoLayers[2] = new OGRSVGLayer( pszFilename, "polygons", SVG_POLYGONS, this );
        }
        else
        {
            CPLDebug("SVG",
                     "%s seems to be a SVG file, but not a Cloudmade vector one.",
                     pszFilename);
        }
    }

    return (nLayers > 0);
#else
    char aBuf[256];
    VSILFILE* fp = VSIFOpenL(pszFilename, "r");
    if (fp)
    {
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, 255, fp );
        aBuf[nLen] = 0;
        if (strstr(aBuf, "<?xml") && strstr(aBuf, "<svg") &&
            strstr(aBuf, "http://cloudmade.com/"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/SVG driver has not been built with read support. "
                    "Expat library required");
        }
        VSIFCloseL(fp);
    }
    return FALSE;
#endif
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRSVGDataSource::TestCapability( const char *pszCap )
{
    return FALSE;
}
