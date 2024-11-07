/******************************************************************************
 *
 * Project:  SVG Translator
 * Purpose:  Implements OGRSVGDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_svg.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          OGRSVGDataSource()                          */
/************************************************************************/

OGRSVGDataSource::OGRSVGDataSource()
    : papoLayers(nullptr), nLayers(0)
#ifdef HAVE_EXPAT
      ,
      eValidity(SVG_VALIDITY_UNKNOWN), bIsCloudmade(false),
      oCurrentParser(nullptr), nDataHandlerCounter(0)
#endif
{
}

/************************************************************************/
/*                         ~OGRSVGDataSource()                          */
/************************************************************************/

OGRSVGDataSource::~OGRSVGDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSVGDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;
    else
        return papoLayers[iLayer];
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                startElementValidateCbk()                             */
/************************************************************************/

void OGRSVGDataSource::startElementValidateCbk(const char *pszNameIn,
                                               const char **ppszAttr)
{
    if (eValidity == SVG_VALIDITY_UNKNOWN)
    {
        if (strcmp(pszNameIn, "svg") == 0)
        {
            eValidity = SVG_VALIDITY_VALID;
            for (int i = 0; ppszAttr[i] != nullptr; i += 2)
            {
                if (strcmp(ppszAttr[i], "xmlns:cm") == 0 &&
                    strcmp(ppszAttr[i + 1], "http://cloudmade.com/") == 0)
                {
                    bIsCloudmade = true;
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

void OGRSVGDataSource::dataHandlerValidateCbk(CPL_UNUSED const char *data,
                                              CPL_UNUSED int nLen)
{
    nDataHandlerCounter++;
    if (nDataHandlerCounter >= PARSER_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oCurrentParser, XML_FALSE);
    }
}

static void XMLCALL startElementValidateCbk(void *pUserData,
                                            const char *pszName,
                                            const char **ppszAttr)
{
    OGRSVGDataSource *poDS = (OGRSVGDataSource *)pUserData;
    poDS->startElementValidateCbk(pszName, ppszAttr);
}

static void XMLCALL dataHandlerValidateCbk(void *pUserData, const char *data,
                                           int nLen)
{
    OGRSVGDataSource *poDS = (OGRSVGDataSource *)pUserData;
    poDS->dataHandlerValidateCbk(data, nLen);
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSVGDataSource::Open(const char *pszFilename)

{
#ifdef HAVE_EXPAT
    /* -------------------------------------------------------------------- */
    /*      Try to open the file.                                           */
    /* -------------------------------------------------------------------- */
    CPLString osFilename;  // keep in that scope
    if (EQUAL(CPLGetExtension(pszFilename), "svgz") &&
        strstr(pszFilename, "/vsigzip/") == nullptr)
    {
        osFilename = CPLString("/vsigzip/") + pszFilename;
        pszFilename = osFilename.c_str();
    }

    VSILFILE *fp = VSIFOpenL(pszFilename, "r");
    if (fp == nullptr)
        return FALSE;

    eValidity = SVG_VALIDITY_UNKNOWN;

    XML_Parser oParser = OGRCreateExpatXMLParser();
    oCurrentParser = oParser;
    XML_SetUserData(oParser, this);
    XML_SetElementHandler(oParser, ::startElementValidateCbk, nullptr);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerValidateCbk);

    std::vector<char> aBuf(PARSER_BUF_SIZE);
    int nDone = 0;
    unsigned int nLen = 0;
    int nCount = 0;

    /* Begin to parse the file and look for the <svg> element */
    /* It *MUST* be the first element of an XML file */
    /* So once we have read the first element, we know if we can */
    /* handle the file or not with that driver */
    do
    {
        nDataHandlerCounter = 0;
        nLen = (unsigned int)VSIFReadL(aBuf.data(), 1, aBuf.size(), fp);
        nDone = nLen < aBuf.size();
        if (XML_Parse(oParser, aBuf.data(), nLen, nDone) == XML_STATUS_ERROR)
        {
            if (nLen <= PARSER_BUF_SIZE - 1)
                aBuf[nLen] = 0;
            else
                aBuf[PARSER_BUF_SIZE - 1] = 0;
            if (strstr(aBuf.data(), "<?xml") && strstr(aBuf.data(), "<svg"))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
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
            /* After reading 50 * PARSER_BUF_SIZE bytes, and not finding whether the
             * file */
            /* is SVG or not, we give up and fail silently */
            nCount++;
            if (nCount == 50)
                break;
        }
    } while (!nDone && nLen > 0);

    XML_ParserFree(oParser);

    VSIFCloseL(fp);

    if (eValidity == SVG_VALIDITY_VALID)
    {
        if (bIsCloudmade)
        {
            nLayers = 3;
            papoLayers = (OGRSVGLayer **)CPLRealloc(
                papoLayers, nLayers * sizeof(OGRSVGLayer *));
            papoLayers[0] =
                new OGRSVGLayer(pszFilename, "points", SVG_POINTS, this);
            papoLayers[1] =
                new OGRSVGLayer(pszFilename, "lines", SVG_LINES, this);
            papoLayers[2] =
                new OGRSVGLayer(pszFilename, "polygons", SVG_POLYGONS, this);
        }
        else
        {
            CPLDebug(
                "SVG",
                "%s seems to be a SVG file, but not a Cloudmade vector one.",
                pszFilename);
        }
    }

    return nLayers > 0;
#else
    char aBuf[256];
    VSILFILE *fp = VSIFOpenL(pszFilename, "r");
    if (fp)
    {
        unsigned int nLen = (unsigned int)VSIFReadL(aBuf, 1, 255, fp);
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
