/******************************************************************************
 * $Id$
 *
 * Project:  SVG Translator
 * Purpose:  Implements OGRSVGLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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
/*                            OGRSVGLayer()                             */
/************************************************************************/

OGRSVGLayer::OGRSVGLayer( const char* pszFilename,
                          const char* pszLayerName,
                          SVGGeometryType svgGeomType,
                          OGRSVGDataSource* poDS)

{
    nNextFID = 0;

    this->poDS = poDS;
    this->svgGeomType = svgGeomType;
    osLayerName = pszLayerName;

    poFeatureDefn = NULL;

    nTotalFeatures = 0;

    ppoFeatureTab = NULL;
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    bStopParsing = FALSE;

    poSRS = new OGRSpatialReference("PROJCS[\"WGS 84 / Pseudo-Mercator\","
    "GEOGCS[\"WGS 84\","
    "    DATUM[\"WGS_1984\","
    "        SPHEROID[\"WGS 84\",6378137,298.257223563,"
    "            AUTHORITY[\"EPSG\",\"7030\"]],"
    "        AUTHORITY[\"EPSG\",\"6326\"]],"
    "    PRIMEM[\"Greenwich\",0,"
    "        AUTHORITY[\"EPSG\",\"8901\"]],"
    "    UNIT[\"degree\",0.0174532925199433,"
    "        AUTHORITY[\"EPSG\",\"9122\"]],"
    "    AUTHORITY[\"EPSG\",\"4326\"]],"
    "UNIT[\"metre\",1,"
    "    AUTHORITY[\"EPSG\",\"9001\"]],"
    "PROJECTION[\"Mercator_1SP\"],"
    "PARAMETER[\"central_meridian\",0],"
    "PARAMETER[\"scale_factor\",1],"
    "PARAMETER[\"false_easting\",0],"
    "PARAMETER[\"false_northing\",0],"
    "EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],"
    "AUTHORITY[\"EPSG\",\"3857\"],"
    "AXIS[\"X\",EAST],"
    "AXIS[\"Y\",NORTH]]");

    poFeature = NULL;

#ifdef HAVE_EXPAT
    oParser = NULL;
#endif

    fpSVG = VSIFOpenL( pszFilename, "r" );
    if( fpSVG == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszFilename);
        return;
    }

    ResetReading();
}

/************************************************************************/
/*                            ~OGRSVGLayer()                            */
/************************************************************************/

OGRSVGLayer::~OGRSVGLayer()

{
#ifdef HAVE_EXPAT
    if (oParser)
        XML_ParserFree(oParser);
#endif
    if (poFeatureDefn)
        poFeatureDefn->Release();
    
    if( poSRS != NULL )
        poSRS->Release();

    CPLFree(pszSubElementValue);

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);

    if (poFeature)
        delete poFeature;

    if (fpSVG)
        VSIFCloseL( fpSVG );
}

#ifdef HAVE_EXPAT

static void XMLCALL startElementCbk(void *pUserData,
                                    const char *pszName, const char **ppszAttr)
{
    ((OGRSVGLayer*)pUserData)->startElementCbk(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRSVGLayer*)pUserData)->endElementCbk(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRSVGLayer*)pUserData)->dataHandlerCbk(data, nLen);
}

#endif

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSVGLayer::ResetReading()

{
    nNextFID = 0;
    if (fpSVG)
    {
        VSIFSeekL( fpSVG, 0, SEEK_SET );
#ifdef HAVE_EXPAT
        if (oParser)
            XML_ParserFree(oParser);

        oParser = OGRCreateExpatXMLParser();
        XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
        XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
        XML_SetUserData(oParser, this);
#endif
    }

    CPLFree(pszSubElementValue);
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    iCurrentField = -1;

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    ppoFeatureTab = NULL;
    if (poFeature)
        delete poFeature;
    poFeature = NULL;

    depthLevel = 0;
    interestingDepthLevel = 0;
    inInterestingElement = FALSE;
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                         OGRSVGGetClass()                             */
/************************************************************************/

static const char* OGRSVGGetClass(const char **ppszAttr)
{
    const char** ppszIter = ppszAttr;
    while(*ppszIter)
    {
        if (strcmp(ppszIter[0], "class") == 0)
            return ppszIter[1];
        ppszIter += 2;
    }
    return "";
}

/************************************************************************/
/*                          OGRSVGParseD()                              */
/************************************************************************/

static void OGRSVGParseD(OGRLineString* poLS, const char* pszD)
{
    char szBuffer[32];
    int iBuffer = 0;
    const char* pszIter = pszD;
    char ch;
    int iNumber = 0;
    double dfPrevNumber = 0;
    int bRelativeLineto = FALSE;
    double dfX = 0, dfY = 0;
    int nPointCount = 0;
    while(TRUE)
    {
        ch = *(pszIter ++);

        if (ch == 'M' || ch == 'm')
        {
            if (nPointCount != 0)
            {
                CPLDebug("SVG", "Not ready to handle M/m not at the beginning");
                return;
            }
        }
        else if (ch == 'L')
        {
            bRelativeLineto = FALSE;
        }
        else if (ch == 'l')
        {
            if (nPointCount == 0)
            {
                CPLDebug("SVG", "Relative lineto at the beginning of the line");
                return;
            }
            bRelativeLineto = TRUE;
        }
        else if (ch == 'z' || ch == 'Z')
        {
            poLS->closeRings();
            return;
        }
        else if (ch == '+' || ch == '-' || ch == '.' ||
                 (ch >= '0' && ch <= '9'))
        {
            if (iBuffer == 30)
            {
                CPLDebug("SVG", "Too big number");
                return;
            }
            szBuffer[iBuffer ++] = ch;
        }
        else if (ch == ' ' || ch == 0)
        {
            if (iBuffer > 0)
            {
                szBuffer[iBuffer] = 0;
                if (iNumber == 1)
                {
                    /* Cloudmade --> negate y */
                    double dfNumber = -CPLAtof(szBuffer);

                    if (bRelativeLineto)
                    {
                        dfX += dfPrevNumber;
                        dfY += dfNumber;
                    }
                    else
                    {
                        dfX = dfPrevNumber;
                        dfY = dfNumber;
                    }
                    poLS->addPoint(dfX, dfY);
                    nPointCount ++;

                    iNumber = 0;
                }
                else
                {
                    iNumber = 1;
                    dfPrevNumber = CPLAtof(szBuffer);
                }

                iBuffer = 0;
            }
            if (ch == 0)
                break;
        }
    }
}

/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRSVGLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    int i;

    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    if (svgGeomType == SVG_POINTS &&
        strcmp(pszName, "circle") == 0 &&
        strcmp(OGRSVGGetClass(ppszAttr), "point") == 0)
    {
        int bHasFoundX = FALSE, bHasFoundY = FALSE;
        double dfX = 0, dfY = 0;
        for (i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "cx") == 0)
            {
                bHasFoundX = TRUE;
                dfX = CPLAtof(ppszAttr[i + 1]);
            }
            else if (strcmp(ppszAttr[i], "cy") == 0)
            {
                bHasFoundY = TRUE;
                /* Cloudmade --> negate y */
                dfY = - CPLAtof(ppszAttr[i + 1]);
            }
        }
        if (bHasFoundX && bHasFoundY)
        {
            interestingDepthLevel = depthLevel;
            inInterestingElement = TRUE;

            if (poFeature)
                delete poFeature;

            poFeature = new OGRFeature( poFeatureDefn );

            poFeature->SetFID( nNextFID++ );
            OGRPoint* poPoint = new OGRPoint( dfX, dfY );
            poPoint->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly( poPoint );
        }
    }
    else if (svgGeomType == SVG_LINES &&
             strcmp(pszName, "path") == 0 &&
             strcmp(OGRSVGGetClass(ppszAttr), "line") == 0)
    {
        const char* pszD = NULL;
        for (i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "d") == 0)
            {
                pszD = ppszAttr[i + 1];
                break;
            }
        }
        if (pszD)
        {
            interestingDepthLevel = depthLevel;
            inInterestingElement = TRUE;

            if (poFeature)
                delete poFeature;

            poFeature = new OGRFeature( poFeatureDefn );

            poFeature->SetFID( nNextFID++ );
            OGRLineString* poLS = new OGRLineString();
            OGRSVGParseD(poLS, pszD);
            poLS->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly( poLS );
        }
    }
    else if (svgGeomType == SVG_POLYGONS &&
             strcmp(pszName, "path") == 0 &&
             strcmp(OGRSVGGetClass(ppszAttr), "polygon") == 0)
    {
        const char* pszD = NULL;
        for (i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "d") == 0)
            {
                pszD = ppszAttr[i + 1];
                break;
            }
        }
        if (pszD)
        {
            interestingDepthLevel = depthLevel;
            inInterestingElement = TRUE;

            if (poFeature)
                delete poFeature;

            poFeature = new OGRFeature( poFeatureDefn );

            poFeature->SetFID( nNextFID++ );
            OGRPolygon* poPolygon = new OGRPolygon();
            OGRLinearRing* poLS = new OGRLinearRing();
            OGRSVGParseD(poLS, pszD);
            poPolygon->addRingDirectly(poLS);
            poPolygon->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly( poPolygon );
        }
    }
    else if (inInterestingElement &&
             depthLevel == interestingDepthLevel + 1 &&
             strncmp(pszName, "cm:", 3) == 0)
    {
        iCurrentField = poFeatureDefn->GetFieldIndex(pszName + 3);
    }

    depthLevel++;
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRSVGLayer::endElementCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    depthLevel--;

    if (inInterestingElement)
    {
        if (depthLevel == interestingDepthLevel)
        {
            inInterestingElement = FALSE;

            if( (m_poFilterGeom == NULL
                    || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
            {
                ppoFeatureTab = (OGRFeature**)
                        CPLRealloc(ppoFeatureTab,
                                    sizeof(OGRFeature*) * (nFeatureTabLength + 1));
                ppoFeatureTab[nFeatureTabLength] = poFeature;
                nFeatureTabLength++;
            }
            else
            {
                delete poFeature;
            }
            poFeature = NULL;
        }
        else if (depthLevel == interestingDepthLevel + 1)
        {
            if (poFeature && iCurrentField >= 0 && nSubElementValueLen)
            {
                pszSubElementValue[nSubElementValueLen] = 0;
                poFeature->SetField( iCurrentField, pszSubElementValue);
            }

            CPLFree(pszSubElementValue);
            pszSubElementValue = NULL;
            nSubElementValueLen = 0;
            iCurrentField = -1;
        }
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRSVGLayer::dataHandlerCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }

    nWithoutEventCounter = 0;

    if (iCurrentField >= 0)
    {
        char* pszNewSubElementValue = (char*) VSIRealloc(pszSubElementValue,
                                           nSubElementValueLen + nLen + 1);
        if (pszNewSubElementValue == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            XML_StopParser(oParser, XML_FALSE);
            bStopParsing = TRUE;
            return;
        }
        pszSubElementValue = pszNewSubElementValue;
        memcpy(pszSubElementValue + nSubElementValueLen, data, nLen);
        nSubElementValueLen += nLen;
        if (nSubElementValueLen > 100000)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too much data inside one element. File probably corrupted");
            XML_StopParser(oParser, XML_FALSE);
            bStopParsing = TRUE;
        }
    }
}
#endif

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSVGLayer::GetNextFeature()
{
    GetLayerDefn();

    if (fpSVG == NULL)
        return NULL;

    if (bStopParsing)
        return NULL;

#ifdef HAVE_EXPAT
    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }
    
    if (VSIFEofL(fpSVG))
        return NULL;
    
    char aBuf[BUFSIZ];
    
    CPLFree(ppoFeatureTab);
    ppoFeatureTab = NULL;
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;
    nWithoutEventCounter = 0;
    iCurrentField = -1;

    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen = (unsigned int)
                    VSIFReadL( aBuf, 1, sizeof(aBuf), fpSVG );
        nDone = VSIFEofL(fpSVG);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of SVG file failed : %s at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
            break;
        }
        nWithoutEventCounter ++;
    } while (!nDone && nFeatureTabLength == 0 && !bStopParsing &&
             nWithoutEventCounter < 1000);

    if (nWithoutEventCounter == 1000)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    return (nFeatureTabLength) ? ppoFeatureTab[nFeatureTabIndex++] : NULL;
#else
    return NULL;
#endif
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSVGLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poAttrQuery == NULL && m_poFilterGeom == NULL &&
               nTotalFeatures > 0;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else
        return FALSE;
}


/************************************************************************/
/*                       LoadSchema()                         */
/************************************************************************/

#ifdef HAVE_EXPAT

static void XMLCALL startElementLoadSchemaCbk(void *pUserData,
                                              const char *pszName,
                                              const char **ppszAttr)
{
    ((OGRSVGLayer*)pUserData)->startElementLoadSchemaCbk(pszName, ppszAttr);
}

static void XMLCALL endElementLoadSchemaCbk(void *pUserData, const char *pszName)
{
    ((OGRSVGLayer*)pUserData)->endElementLoadSchemaCbk(pszName);
}

static void XMLCALL dataHandlerLoadSchemaCbk(void *pUserData,
                                             const char *data, int nLen)
{
    ((OGRSVGLayer*)pUserData)->dataHandlerLoadSchemaCbk(data, nLen);
}


/** This function parses the whole file to build the schema */
void OGRSVGLayer::LoadSchema()
{
    CPLAssert(poFeatureDefn == NULL);

    for(int i=0;i<poDS->GetLayerCount();i++)
    {
        OGRSVGLayer* poLayer = (OGRSVGLayer*)poDS->GetLayer(i);
        poLayer->poFeatureDefn = new OGRFeatureDefn( poLayer->osLayerName );
        poLayer->poFeatureDefn->Reference();
        poLayer->poFeatureDefn->SetGeomType(poLayer->GetGeomType());
        poLayer->poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poLayer->poSRS);
    }

    oSchemaParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oSchemaParser, ::startElementLoadSchemaCbk,
                                         ::endElementLoadSchemaCbk);
    XML_SetCharacterDataHandler(oSchemaParser, ::dataHandlerLoadSchemaCbk);
    XML_SetUserData(oSchemaParser, this);

    if (fpSVG == NULL)
        return;

    VSIFSeekL( fpSVG, 0, SEEK_SET );

    inInterestingElement = FALSE;
    depthLevel = 0;
    nWithoutEventCounter = 0;
    bStopParsing = FALSE;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpSVG );
        nDone = VSIFEofL(fpSVG);
        if (XML_Parse(oSchemaParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of SVG file failed : %s at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oSchemaParser)),
                     (int)XML_GetCurrentLineNumber(oSchemaParser),
                     (int)XML_GetCurrentColumnNumber(oSchemaParser));
            bStopParsing = TRUE;
            break;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 1000);

    if (nWithoutEventCounter == 1000)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    XML_ParserFree(oSchemaParser);
    oSchemaParser = NULL;

    VSIFSeekL( fpSVG, 0, SEEK_SET );
}


/************************************************************************/
/*                  startElementLoadSchemaCbk()                         */
/************************************************************************/

void OGRSVGLayer::startElementLoadSchemaCbk(const char *pszName,
                                            const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    if (strcmp(pszName, "circle") == 0 &&
        strcmp(OGRSVGGetClass(ppszAttr), "point") == 0)
    {
        poCurLayer = (OGRSVGLayer*)poDS->GetLayer(0);
        poCurLayer->nTotalFeatures ++;
        inInterestingElement = TRUE;
        interestingDepthLevel = depthLevel;
    }
    else if (strcmp(pszName, "path") == 0 &&
             strcmp(OGRSVGGetClass(ppszAttr), "line") == 0)
    {
        poCurLayer = (OGRSVGLayer*)poDS->GetLayer(1);
        poCurLayer->nTotalFeatures ++;
        inInterestingElement = TRUE;
        interestingDepthLevel = depthLevel;
    }
    else if (strcmp(pszName, "path") == 0 &&
             strcmp(OGRSVGGetClass(ppszAttr), "polygon") == 0)
    {
        poCurLayer = (OGRSVGLayer*)poDS->GetLayer(2);
        poCurLayer->nTotalFeatures ++;
        inInterestingElement = TRUE;
        interestingDepthLevel = depthLevel;
    }
    else if (inInterestingElement)
    {
        if (depthLevel == interestingDepthLevel + 1 &&
            strncmp(pszName, "cm:", 3) == 0)
        {
            pszName += 3;
            if (poCurLayer->poFeatureDefn->GetFieldIndex(pszName) < 0)
            {
                OGRFieldDefn oFieldDefn(pszName, OFTString);
                if (strcmp(pszName, "timestamp") == 0)
                    oFieldDefn.SetType(OFTDateTime);
                else if (strcmp(pszName, "way_area") == 0 ||
                         strcmp(pszName, "area") == 0)
                    oFieldDefn.SetType(OFTReal);
                else if (strcmp(pszName, "z_order") == 0)
                    oFieldDefn.SetType(OFTInteger);
                poCurLayer->poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
        }
    }

    depthLevel++;
}

/************************************************************************/
/*                   endElementLoadSchemaCbk()                           */
/************************************************************************/

void OGRSVGLayer::endElementLoadSchemaCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    depthLevel--;

    if (inInterestingElement &&
        depthLevel == interestingDepthLevel)
    {
        inInterestingElement = FALSE;
    }
}

/************************************************************************/
/*                   dataHandlerLoadSchemaCbk()                         */
/************************************************************************/

void OGRSVGLayer::dataHandlerLoadSchemaCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oSchemaParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }

    nWithoutEventCounter = 0;
}
#else
void OGRSVGLayer::LoadSchema()
{
}
#endif

/************************************************************************/
/*                          GetLayerDefn()                              */
/************************************************************************/

OGRFeatureDefn * OGRSVGLayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
    {
        LoadSchema();
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetGeomType()                              */
/************************************************************************/

OGRwkbGeometryType OGRSVGLayer::GetGeomType()
{
    if (svgGeomType == SVG_POINTS)
        return wkbPoint;
    else if (svgGeomType == SVG_LINES)
        return wkbLineString;
    else
        return wkbPolygon;
}

/************************************************************************/
/*                           GetGeomType()                              */
/************************************************************************/

int OGRSVGLayer::GetFeatureCount( int bForce )
{
    if (m_poAttrQuery != NULL || m_poFilterGeom != NULL)
        return OGRLayer::GetFeatureCount(bForce);

    GetLayerDefn();

    return nTotalFeatures;
}
