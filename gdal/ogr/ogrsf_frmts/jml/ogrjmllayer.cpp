/******************************************************************************
 * $Id$
 *
 * Project:  JML Translator
 * Purpose:  Implements OGRJMLLayer class.
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_jml.h"
#include "cpl_conv.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

#ifdef HAVE_EXPAT

/************************************************************************/
/*                              OGRJMLLayer()                           */
/************************************************************************/

OGRJMLLayer::OGRJMLLayer( const char* pszLayerName,
                                    OGRJMLDataset* poDS,
                                    VSILFILE* fp )

{
    nNextFID = 0;

    this->poDS = poDS;
    this->fp = fp;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    bAccumulateElementValue = FALSE;
    pszElementValue = (char*)CPLCalloc(1024, 1);
    nElementValueLen = 0;
    nElementValueAlloc = 1024;

    ppoFeatureTab = NULL;
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;

    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    currentDepth = 0;
    bStopParsing = FALSE;
    bHasReadSchema = FALSE;
    
    bSchemaFinished = FALSE;
    nJCSGMLInputTemplateDepth = 0;
    nCollectionElementDepth = 0;
    nFeatureElementDepth = 0;
    nGeometryElementDepth = 0;
    nColumnDepth = 0;
    nNameDepth = 0;
    nTypeDepth = 0;
    nAttributeElementDepth = 0;
    iAttr = -1;
    iRGBField = -1;

    poFeature = NULL;

    oParser = NULL;
}

/************************************************************************/
/*                             ~OGRJMLLayer()                           */
/************************************************************************/

OGRJMLLayer::~OGRJMLLayer()

{
    if (oParser)
        XML_ParserFree(oParser);
    poFeatureDefn->Release();

    CPLFree(pszElementValue);

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);

    if (poFeature)
        delete poFeature;
}


/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRJMLLayer::GetLayerDefn()
{
    if (!bHasReadSchema)
        LoadSchema();

    return poFeatureDefn;
}

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRJMLLayer*)pUserData)->startElementCbk(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRJMLLayer*)pUserData)->endElementCbk(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRJMLLayer*)pUserData)->dataHandlerCbk(data, nLen);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRJMLLayer::ResetReading()

{
    nNextFID = 0;

    VSIFSeekL( fp, 0, SEEK_SET );
    if (oParser)
        XML_ParserFree(oParser);

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    delete poFeature;
    poFeature = NULL;

    currentDepth = 0;

    nCollectionElementDepth = 0;
    nFeatureElementDepth = 0;
    nGeometryElementDepth = 0;
    nAttributeElementDepth = 0;
    iAttr = -1;

    bAccumulateElementValue = FALSE;
    nElementValueLen = 0;
    pszElementValue[0] = '\0';
}

/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRJMLLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    
    if( nFeatureElementDepth > 0 && nAttributeElementDepth == 0 &&
        nGeometryElementDepth == 0 && osGeometryElement.compare(pszName) == 0 )
    {
        nGeometryElementDepth = currentDepth;
        bAccumulateElementValue = TRUE;
    }
    else if( nFeatureElementDepth > 0 && nAttributeElementDepth == 0 &&
             nGeometryElementDepth == 0 )
    {
        /* We assume that attributes are present in the order they are */
        /* declared, so as a first guess, we can try the aoColumns[iAttr + 1] */
        int i = (iAttr+1 < poFeatureDefn->GetFieldCount()) ? -1 : 0;
        for(; i<(int)aoColumns.size();i++)
        {
            const OGRJMLColumn& oColumn =
                (i < 0) ? aoColumns[iAttr + 1] : aoColumns[i];
            if(oColumn.osElementName != pszName )
                continue;

            if( oColumn.bIsBody )
            {
                if( oColumn.osAttributeName.size() &&
                    ppszAttr != NULL &&
                    oColumn.osAttributeName.compare(ppszAttr[0]) == 0 &&
                    oColumn.osAttributeValue.compare(ppszAttr[1]) == 0 )
                {
                    /* <osElementName osAttributeName="osAttributeValue">value</osElementName> */

                    bAccumulateElementValue = TRUE;
                    nAttributeElementDepth = currentDepth;
                    iAttr = (i < 0) ? iAttr + 1 : i;
                    break;
                }
                else if( oColumn.osAttributeName.size() == 0 )
                {
                    /* <osElementName>value</osElementName> */

                    bAccumulateElementValue = TRUE;
                    nAttributeElementDepth = currentDepth;
                    iAttr = (i < 0) ? iAttr + 1 : i;
                    break;
                }
            }
            else if( oColumn.osAttributeName.size() &&
                      ppszAttr != NULL &&
                      oColumn.osAttributeName.compare(ppszAttr[0]) == 0 )
            {
                /* <osElementName osAttributeName="value"></osElementName> */

                AddStringToElementValue(ppszAttr[1], strlen(ppszAttr[1]));

                nAttributeElementDepth = currentDepth;
                iAttr = (i < 0) ? iAttr + 1 : i;
                break;
            }
        }
    }
    else if( nGeometryElementDepth > 0 )
    {
        AddStringToElementValue("<", 1);
        AddStringToElementValue(pszName, (int)strlen(pszName));

        const char** papszIter = ppszAttr;
        while( papszIter && *papszIter != NULL )
        {
            AddStringToElementValue(" ", 1);
            AddStringToElementValue(papszIter[0], strlen(papszIter[0]));
            AddStringToElementValue("=\"", 2);
            AddStringToElementValue(papszIter[1], strlen(papszIter[1]));
            AddStringToElementValue("\"", 1);
            papszIter += 2;
        }

        AddStringToElementValue(">", 1);
    }
    else if( nCollectionElementDepth > 0 &&
             nFeatureElementDepth == 0 && osFeatureElement.compare(pszName) == 0 )
    {
        nFeatureElementDepth = currentDepth;
        poFeature = new OGRFeature(poFeatureDefn);
    }
    else if( nCollectionElementDepth == 0 && osCollectionElement.compare(pszName) == 0 )
    {
        nCollectionElementDepth = currentDepth;
    }

    currentDepth++;
}

/************************************************************************/
/*                        StopAccumulate()                              */
/************************************************************************/

void OGRJMLLayer::StopAccumulate()
{
    bAccumulateElementValue = FALSE;
    nElementValueLen = 0;
    pszElementValue[0] = '\0';
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRJMLLayer::endElementCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    
    currentDepth--;
    
    if( nAttributeElementDepth == currentDepth )
    {
        if( nElementValueLen )
            poFeature->SetField(iAttr, pszElementValue);
        nAttributeElementDepth = 0;
        StopAccumulate();
    }
    else if( nGeometryElementDepth > 0 && currentDepth > nGeometryElementDepth )
    {
        AddStringToElementValue("</", 2);
        AddStringToElementValue(pszName, (int)strlen(pszName));
        AddStringToElementValue(">", 1);
    }
    else if( nGeometryElementDepth == currentDepth )
    {
        if( nElementValueLen )
        {
            OGRGeometry* poGeom = (OGRGeometry* )OGR_G_CreateFromGML(pszElementValue);
            if( poGeom != NULL &&
                poGeom->getGeometryType() == wkbGeometryCollection &&
                poGeom->IsEmpty() )
            {
                delete poGeom;
            }
            else
                poFeature->SetGeometryDirectly(poGeom);
        }

        nGeometryElementDepth = 0;
        StopAccumulate();
    }
    else if( nFeatureElementDepth == currentDepth )
    {
        /* Builds a style string from R_G_B if we don't already have a */
        /* style string */
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        int R, G, B;
        if( iRGBField >= 0 && poFeature->IsFieldSet(iRGBField) &&
            poFeature->GetStyleString() == NULL && poGeom != NULL &&
            sscanf(poFeature->GetFieldAsString(iRGBField),
                       "%02X%02X%02X", &R, &G, &B) == 3 )
        {
            OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());
            if( eGeomType == wkbPoint || eGeomType == wkbMultiPoint ||
                eGeomType == wkbLineString || eGeomType == wkbMultiLineString )
            {
                poFeature->SetStyleString(CPLSPrintf("PEN(c:#%02X%02X%02X)", R, G, B));
            }
            else if( eGeomType == wkbPolygon || eGeomType == wkbMultiPolygon )
            {
                poFeature->SetStyleString(CPLSPrintf("BRUSH(fc:#%02X%02X%02X)", R, G, B));
            }
        }

        poFeature->SetFID(nNextFID++);

        if( (m_poFilterGeom == NULL
                || FilterGeometry( poGeom ) )
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
        iAttr = -1;

        nFeatureElementDepth = 0;
    }
    else if( nCollectionElementDepth == currentDepth )
    {
        nCollectionElementDepth = 0;
    }
}

/************************************************************************/
/*                        AddStringToElementValue()                     */
/************************************************************************/

void OGRJMLLayer::AddStringToElementValue(const char *data, int nLen)
{
    if( nElementValueLen + nLen + 1 > nElementValueAlloc )
    {
        char* pszNewElementValue = (char*) VSIRealloc(pszElementValue,
                                        nElementValueLen + nLen + 1 + 1000);
        if (pszNewElementValue == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            XML_StopParser(oParser, XML_FALSE);
            bStopParsing = TRUE;
            return;
        }
        nElementValueAlloc =  nElementValueLen + nLen + 1 + 1000;
        pszElementValue = pszNewElementValue;
    }
    memcpy(pszElementValue + nElementValueLen, data, nLen);
    nElementValueLen += nLen;
    pszElementValue[nElementValueLen] = '\0';
    if (nElementValueLen > 10000000)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Too much data inside one element. File probably corrupted");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = TRUE;
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRJMLLayer::dataHandlerCbk(const char *data, int nLen)
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
    
    if (bAccumulateElementValue)
    {
        AddStringToElementValue(data, nLen);
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRJMLLayer::GetNextFeature()
{
    if (!bHasReadSchema)
        LoadSchema();

    if (bStopParsing)
        return NULL;

    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }
    
    if (VSIFEofL(fp))
        return NULL;
    
    char aBuf[BUFSIZ];
    
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;
    
    nWithoutEventCounter = 0;

    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
                (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fp );
        nDone = VSIFEofL(fp);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of JML file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nFeatureTabLength == 0 &&
             nWithoutEventCounter < 10);

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    return (nFeatureTabLength) ? ppoFeatureTab[nFeatureTabIndex++] : NULL;
}

static void XMLCALL startElementLoadSchemaCbk(void *pUserData, const char *pszName,
                                              const char **ppszAttr)
{
    ((OGRJMLLayer*)pUserData)->startElementLoadSchemaCbk(pszName, ppszAttr);
}

static void XMLCALL endElementLoadSchemaCbk(void *pUserData, const char *pszName)
{
    ((OGRJMLLayer*)pUserData)->endElementLoadSchemaCbk(pszName);
}

/************************************************************************/
/*                           LoadSchema()                              */
/************************************************************************/

/** This function parses the beginning of the file to detect the fields */
void OGRJMLLayer::LoadSchema()
{
    if (bHasReadSchema)
        return;

    bHasReadSchema = TRUE;

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementLoadSchemaCbk,
                          ::endElementLoadSchemaCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fp, 0, SEEK_SET );

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fp );
        nDone = VSIFEofL(fp);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of JML file failed : %s at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && !bSchemaFinished && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }
    
    if( osCollectionElement.size() == 0 || osFeatureElement.size() == 0 ||
        osGeometryElement.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing CollectionElement, FeatureElement or GeometryElement");
        bStopParsing = TRUE;
    }

    ResetReading();
}


/************************************************************************/
/*                  startElementLoadSchemaCbk()                         */
/************************************************************************/

void OGRJMLLayer::startElementLoadSchemaCbk(const char *pszName,
                                                 const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    if( nJCSGMLInputTemplateDepth == 0 &&
        strcmp(pszName, "JCSGMLInputTemplate") == 0 )
        nJCSGMLInputTemplateDepth = currentDepth;
    else if( nJCSGMLInputTemplateDepth > 0 )
    {
        if( nCollectionElementDepth == 0 &&
            strcmp(pszName, "CollectionElement") == 0 )
        {
            nCollectionElementDepth = currentDepth;
            bAccumulateElementValue = TRUE;
        }
        else if( nFeatureElementDepth == 0 &&
                strcmp(pszName, "FeatureElement") == 0 )
        {
            nFeatureElementDepth = currentDepth;
            bAccumulateElementValue = TRUE;
        }
        else if( nGeometryElementDepth == 0 &&
                 strcmp(pszName, "GeometryElement") == 0 )
        {
            nGeometryElementDepth = currentDepth;
            bAccumulateElementValue = TRUE;
        }
        else  if( nColumnDepth == 0 && strcmp(pszName, "column") == 0 )
        {
            nColumnDepth = currentDepth;
            oCurColumn.osName = "";
            oCurColumn.osType = "";
            oCurColumn.osElementName = "";
            oCurColumn.osAttributeName = "";
            oCurColumn.osAttributeValue = "";
            oCurColumn.bIsBody = FALSE;
        }
        else if( nColumnDepth > 0 )
        {
            if( nNameDepth == 0 && strcmp(pszName, "name") == 0 )
            {
                nNameDepth = currentDepth;
                bAccumulateElementValue = TRUE;
            }
            else if( nTypeDepth == 0 && strcmp(pszName, "type") == 0 )
            {
                nTypeDepth = currentDepth;
                bAccumulateElementValue = TRUE;
            }
            else if( strcmp(pszName, "valueElement") == 0 )
            {
                const char** papszIter = ppszAttr;
                while( papszIter && *papszIter != NULL )
                {
                    if( strcmp(*papszIter, "elementName") == 0 )
                        oCurColumn.osElementName = papszIter[1];
                    else if( strcmp(*papszIter, "attributeName") == 0 )
                        oCurColumn.osAttributeName = papszIter[1];
                    else if( strcmp(*papszIter, "attributeValue") == 0 )
                        oCurColumn.osAttributeValue = papszIter[1];
                    papszIter += 2;
                }
            }
            else if( strcmp(pszName, "valueLocation") == 0 )
            {
                const char** papszIter = ppszAttr;
                while( papszIter && *papszIter != NULL )
                {
                    if( strcmp(*papszIter, "position") == 0 )
                        oCurColumn.bIsBody = strcmp(papszIter[1], "body") == 0;
                    else if( strcmp(*papszIter, "attributeName") == 0 )
                        oCurColumn.osAttributeName = papszIter[1];
                    papszIter += 2;
                }
            }
        }
    }

    currentDepth++;
}

/************************************************************************/
/*                   endElementLoadSchemaCbk()                          */
/************************************************************************/

void OGRJMLLayer::endElementLoadSchemaCbk(CPL_UNUSED const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    currentDepth--;

    if( nJCSGMLInputTemplateDepth == currentDepth )
    {
        nJCSGMLInputTemplateDepth = 0;
        bSchemaFinished = TRUE;
    }
    else if( nCollectionElementDepth == currentDepth )
    {
        nCollectionElementDepth = 0;
        osCollectionElement = pszElementValue;
        //CPLDebug("JML", "osCollectionElement = %s", osCollectionElement.c_str());
        StopAccumulate();
    }
    else if( nFeatureElementDepth == currentDepth )
    {
        nFeatureElementDepth = 0;
        osFeatureElement = pszElementValue;
        //CPLDebug("JML", "osFeatureElement = %s", osFeatureElement.c_str());
        StopAccumulate();
    }
    else if( nGeometryElementDepth == currentDepth )
    {
        nGeometryElementDepth = 0;
        osGeometryElement = pszElementValue;
        //CPLDebug("JML", "osGeometryElement = %s", osGeometryElement.c_str());
        StopAccumulate();
    }
    else if( nColumnDepth == currentDepth )
    {
        int bIsOK = TRUE;
        if( oCurColumn.osName.size() == 0 )
            bIsOK = FALSE;
        if( oCurColumn.osType.size() == 0 )
            bIsOK = FALSE;
        if( oCurColumn.osElementName.size() == 0 )
            bIsOK = FALSE;
        if( oCurColumn.bIsBody )
        {
            if( oCurColumn.osAttributeName.size() == 0 &&
                oCurColumn.osAttributeValue.size() != 0 )
                bIsOK = FALSE;
            if( oCurColumn.osAttributeName.size() != 0 &&
                oCurColumn.osAttributeValue.size() == 0 )
                bIsOK = FALSE;
            /* Only 2 valid possibilities : */
            /* <osElementName osAttributeName="osAttributeValue">value</osElementName> */
            /* <osElementName>value</osElementName> */
        }
        else
        {
            /* <osElementName osAttributeName="value"></osElementName> */
            if( oCurColumn.osAttributeName.size() == 0 )
                bIsOK = FALSE;
            if( oCurColumn.osAttributeValue.size() != 0 )
                bIsOK = FALSE;
        }
        
        if( bIsOK )
        {
            OGRFieldType eType = OFTString;
            if( EQUAL(oCurColumn.osType, "INTEGER") )
                eType = OFTInteger;
            else if( EQUAL(oCurColumn.osType, "DOUBLE") )
                eType = OFTReal;
            else if( EQUAL(oCurColumn.osType, "DATE") )
                eType = OFTDateTime;
            OGRFieldDefn oField( oCurColumn.osName, eType );

            if( oCurColumn.osName == "R_G_B" && eType == OFTString )
                iRGBField = poFeatureDefn->GetFieldCount();

            poFeatureDefn->AddFieldDefn(&oField);
            aoColumns.push_back(oCurColumn);
        }
        else
        {
            CPLDebug("JML", "Invalid column definition: name = %s, type = %s, "
                    "elementName = %s, attributeName = %s, attributeValue = %s, bIsBody = %d", 
                    oCurColumn.osName.c_str(),
                    oCurColumn.osType.c_str(), 
                    oCurColumn.osElementName.c_str(),
                    oCurColumn.osAttributeName.c_str(), 
                    oCurColumn.osAttributeValue.c_str(),
                    oCurColumn.bIsBody);
        }

        nColumnDepth = 0;
    }
    else if( nNameDepth == currentDepth )
    {
        nNameDepth = 0;
        oCurColumn.osName = pszElementValue;
        //CPLDebug("JML", "oCurColumn.osName = %s", oCurColumn.osName.c_str());
        StopAccumulate();
    }
    else if( nTypeDepth == currentDepth )
    {
        nTypeDepth = 0;
        oCurColumn.osType = pszElementValue;
        //CPLDebug("JML", "oCurColumn.osType = %s", oCurColumn.osType.c_str());
        StopAccumulate();
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJMLLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else 
        return FALSE;
}

#endif /* HAVE_EXPAT */
