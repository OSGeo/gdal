/******************************************************************************
 * $Id: ogrgpxlayer.cpp
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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


/************************************************************************/
/*                            OGRGPXLayer()                             */
/*                                                                      */
/*      Note that the OGRGPXLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRGPXLayer::OGRGPXLayer( const char* pszFilename,
                          const char* pszLayerName,
                          GPXGeometryType gpxGeomType,
                          OGRGPXDataSource* poDS,
                          int bWriteMode)

{
    eof = FALSE;
    nNextFID = 0;
    
    this->poDS = poDS;
    this->bWriteMode = bWriteMode;
    this->gpxGeomType = gpxGeomType;
    
    pszElementToScan = pszLayerName;

    nFeatures = 0;
    
    bEleAs25D =  CSLTestBoolean(CPLGetConfigOption("GPX_ELE_AS_25D", "NO"));
    
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
    
    if (gpxGeomType == GPX_WPT)
    {
        poFeatureDefn->SetGeomType((bEleAs25D) ? wkbPoint25D : wkbPoint);
        /* Position info */
        
        OGRFieldDefn oFieldEle("ele", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldEle );
        
        OGRFieldDefn oFieldTime("time", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldTime );
        
        OGRFieldDefn oFieldMagVar("magvar", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldMagVar );
    
        OGRFieldDefn oFieldGeoidHeight("geoidheight", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldGeoidHeight );
            
        /* Description info */
        
        OGRFieldDefn oFieldName("name", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldName );
        
        OGRFieldDefn oFieldCmt("cmt", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldCmt );
        
        OGRFieldDefn oFieldDesc("desc", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldDesc );
        
        OGRFieldDefn oFieldSrc("src", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldSrc );
        
        /* TODO : link */
        
        OGRFieldDefn oFieldSym("sym", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldSym );
        
        OGRFieldDefn oFieldType("type", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldType );
    
        /* Accuracy info */
        
        OGRFieldDefn oFieldFix("fix", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldFix );
        
        OGRFieldDefn oFieldSat("sat", OFTInteger );
        poFeatureDefn->AddFieldDefn( &oFieldSat );
        
        OGRFieldDefn oFieldHdop("hdop", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldHdop );
        
        OGRFieldDefn oFieldVdop("vdop", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldVdop );
        
        OGRFieldDefn oFieldPdop("pdop", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldPdop );
        
        OGRFieldDefn oFieldAgeofgpsdata("ageofdgpsdata", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldAgeofgpsdata );
        
        OGRFieldDefn oFieldDgpsid("dgpsid", OFTInteger );
        poFeatureDefn->AddFieldDefn( &oFieldDgpsid );
    }
    else
    {
        if (gpxGeomType == GPX_TRACK)
            poFeatureDefn->SetGeomType((bEleAs25D) ? wkbMultiLineString25D : wkbMultiLineString);
        else
            poFeatureDefn->SetGeomType((bEleAs25D) ? wkbLineString25D : wkbLineString);
        
        OGRFieldDefn oFieldName("name", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldName );
        
        OGRFieldDefn oFieldCmt("cmt", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldCmt );
        
        OGRFieldDefn oFieldDesc("desc", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldDesc );
        
        OGRFieldDefn oFieldSrc("src", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldSrc );
        
        /* TODO : link */
        
        OGRFieldDefn oFieldNumber("number", OFTInteger );
        poFeatureDefn->AddFieldDefn( &oFieldNumber );
        
        OGRFieldDefn oFieldType("type", OFTString );
        poFeatureDefn->AddFieldDefn( &oFieldType );
    }
   
    ppoFeatureTab = NULL;
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    pszSubElementName = NULL;
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;

    poSRS = new OGRSpatialReference("GEOGCS[\"WGS 84\", "
        "   DATUM[\"WGS_1984\","
        "       SPHEROID[\"WGS 84\",6378137,298.257223563,"
        "           AUTHORITY[\"EPSG\",\"7030\"]],"
        "           AUTHORITY[\"EPSG\",\"6326\"]],"
        "       PRIMEM[\"Greenwich\",0,"
        "           AUTHORITY[\"EPSG\",\"8901\"]],"
        "       UNIT[\"degree\",0.01745329251994328,"
        "           AUTHORITY[\"EPSG\",\"9122\"]],"
        "           AUTHORITY[\"EPSG\",\"4326\"]]");

    if (bWriteMode == FALSE)
    {
        fpGPX = VSIFOpenL( pszFilename, "r" );
        if( fpGPX == NULL )
            return;
    }
    else
        fpGPX = NULL;


#ifdef HAVE_EXPAT
    oParser = NULL;
#endif

    ResetReading();
}

/************************************************************************/
/*                            ~OGRGPXLayer()                            */
/************************************************************************/

OGRGPXLayer::~OGRGPXLayer()

{
#ifdef HAVE_EXPAT
    XML_ParserFree(oParser);
#endif
    poFeatureDefn->Release();
    
    if( poSRS != NULL )
        poSRS->Release();

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);

    if (fpGPX)
        VSIFCloseL( fpGPX );
}


#ifdef HAVE_EXPAT

extern "C"
{
static void XMLCALL startElementCbk(void *pUserData, const char *pszName, const char **ppszAttr);
static void XMLCALL endElementCbk(void *pUserData, const char *pszName);
static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen);
}

static void XMLCALL startElementCbk(void *pUserData, const char *pszName, const char **ppszAttr)
{
    ((OGRGPXLayer*)pUserData)->startElementCbk(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRGPXLayer*)pUserData)->endElementCbk(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRGPXLayer*)pUserData)->dataHandlerCbk(data, nLen);
}

#endif

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGPXLayer::ResetReading()

{
    eof = FALSE;
    nNextFID = 0;
    if (fpGPX)
    {
        VSIFSeekL( fpGPX, 0, SEEK_SET );
#ifdef HAVE_EXPAT
        if (oParser)
            XML_ParserFree(oParser);
        
        oParser = XML_ParserCreate(NULL);
        XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
        XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
        XML_SetUserData(oParser, this);
#endif
    }
    hasFoundLat = FALSE;
    hasFoundLon = FALSE;
    inInterestingElement = FALSE;
    CPLFree(pszSubElementName);
    pszSubElementName = NULL;
    CPLFree(pszSubElementValue);
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    
    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    ppoFeatureTab = NULL;
    poFeature = NULL;
    multiLineString = NULL;
    lineString = NULL;

    depthLevel = 0;
    interestingDepthLevel = 0;
}


/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRGPXLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    int i;
    if (gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0)
    {
        interestingDepthLevel = depthLevel;
        
        if (poFeature) delete poFeature;

        poFeature = new OGRFeature( poFeatureDefn );
        inInterestingElement = TRUE;
        hasFoundLat = FALSE;
        hasFoundLon = FALSE;
        for (i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "lat") == 0)
            {
                hasFoundLat = TRUE;
                latVal = CPLAtof(ppszAttr[i + 1]);
            }
            else if (strcmp(ppszAttr[i], "lon") == 0)
            {
                hasFoundLon = TRUE;
                lonVal = CPLAtof(ppszAttr[i + 1]);
            }
        }

        if (hasFoundLat && hasFoundLon)
        {
            poFeature->SetFID( nNextFID++ );
            poFeature->SetGeometryDirectly( new OGRPoint( lonVal, latVal ) );
        }
    }
    else if (gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
    {
        interestingDepthLevel = depthLevel;
        
        if (poFeature) delete poFeature;

        poFeature = new OGRFeature( poFeatureDefn );
        inInterestingElement = TRUE;

        multiLineString = new OGRMultiLineString ();
        lineString = NULL;
        poFeature->SetFID( nNextFID++ );
        poFeature->SetGeometryDirectly( multiLineString );
    }
    else if (gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
    {
        interestingDepthLevel = depthLevel;
        
        if (poFeature) delete poFeature;

        poFeature = new OGRFeature( poFeatureDefn );
        inInterestingElement = TRUE;

        lineString = new OGRLineString ();
        poFeature->SetFID( nNextFID++ );
        poFeature->SetGeometryDirectly( lineString );
    }
    else if (inInterestingElement)
    {
        if (gpxGeomType == GPX_TRACK && strcmp(pszName, "trkseg") == 0 &&
            depthLevel == interestingDepthLevel + 1)
        {
            if (multiLineString)
            {
                lineString = new OGRLineString ();
                multiLineString->addGeometryDirectly( lineString );
            }
        }
        else if (gpxGeomType == GPX_TRACK && strcmp(pszName, "trkpt") == 0 &&
                 depthLevel == interestingDepthLevel + 2)
        {
            if (lineString)
            {
                hasFoundLat = FALSE;
                hasFoundLon = FALSE;
                for (i = 0; ppszAttr[i]; i += 2)
                {
                    if (strcmp(ppszAttr[i], "lat") == 0)
                    {
                        hasFoundLat = TRUE;
                        latVal = CPLAtof(ppszAttr[i + 1]);
                    }
                    else if (strcmp(ppszAttr[i], "lon") == 0)
                    {
                        hasFoundLon = TRUE;
                        lonVal = CPLAtof(ppszAttr[i + 1]);
                    }
                }

                if (hasFoundLat && hasFoundLon)
                {
                    lineString->addPoint(lonVal, latVal);
                }
            }
        }
        else if (gpxGeomType == GPX_ROUTE && strcmp(pszName, "rtept") == 0 &&
                 depthLevel == interestingDepthLevel + 1)
        {
            if (lineString)
            {
                hasFoundLat = FALSE;
                hasFoundLon = FALSE;
                for (i = 0; ppszAttr[i]; i += 2)
                {
                    if (strcmp(ppszAttr[i], "lat") == 0)
                    {
                        hasFoundLat = TRUE;
                        latVal = CPLAtof(ppszAttr[i + 1]);
                    }
                    else if (strcmp(ppszAttr[i], "lon") == 0)
                    {
                        hasFoundLon = TRUE;
                        lonVal = CPLAtof(ppszAttr[i + 1]);
                    }
                }

                if (hasFoundLat && hasFoundLon)
                {
                    lineString->addPoint(lonVal, latVal);
                }
            }
        }
        else if (bEleAs25D &&
                 strcmp(pszName, "ele") == 0 &&
                 lineString != NULL &&
                 ((gpxGeomType == GPX_ROUTE && depthLevel == interestingDepthLevel + 2) ||
                  (gpxGeomType == GPX_TRACK && depthLevel == interestingDepthLevel + 3)))
        {
            CPLFree(pszSubElementName);
            pszSubElementName = CPLStrdup(pszName);
        }
        else if (depthLevel == interestingDepthLevel + 1)
        {
            CPLFree(pszSubElementName);
            pszSubElementName = NULL;
            for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
            {
                if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), pszName ) == 0)
                {
                    pszSubElementName = CPLStrdup(pszName);
                    break;
                }
            }
        }
    }

    depthLevel++;
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRGPXLayer::endElementCbk(const char *pszName)
{
    depthLevel--;

    if (inInterestingElement)
    {
        if (gpxGeomType == GPX_WPT && strcmp(pszName, "wpt") == 0)
        {
            int bIsValid = (hasFoundLat && hasFoundLon);
            inInterestingElement = FALSE;

            if( bIsValid
                &&  (m_poFilterGeom == NULL
                    || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
            {
                if( poFeature->GetGeometryRef() != NULL )
                {
                    poFeature->GetGeometryRef()->assignSpatialReference( poSRS );
                    
                    if (bEleAs25D)
                    {
                        for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
                        {
                            if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), "ele" ) == 0)
                            {
                                if( poFeature->IsFieldSet( iField ) )
                                {
                                    double val =  poFeature->GetFieldAsDouble( iField);
                                    ((OGRPoint*)poFeature->GetGeometryRef())->setZ(val);
                                    poFeature->GetGeometryRef()->setCoordinateDimension(3);
                                }
                                break;
                            }
                        }
                    }
                }

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
        else if (gpxGeomType == GPX_TRACK && strcmp(pszName, "trk") == 0)
        {
            if( (m_poFilterGeom == NULL
                    || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
            {
                if( poFeature->GetGeometryRef() != NULL )
                {
                    poFeature->GetGeometryRef()->assignSpatialReference( poSRS );
                }

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
            multiLineString = NULL;
            lineString = NULL;
        }
        else if (gpxGeomType == GPX_TRACK && strcmp(pszName, "trkseg") == 0 &&
                 depthLevel == interestingDepthLevel + 1)
        {
            lineString = NULL;
        }
        else if (gpxGeomType == GPX_ROUTE && strcmp(pszName, "rte") == 0)
        {
            if( (m_poFilterGeom == NULL
                    || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
            {
                if( poFeature->GetGeometryRef() != NULL )
                {
                    poFeature->GetGeometryRef()->assignSpatialReference( poSRS );
                }

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
            lineString = NULL;
        }
        else if (bEleAs25D &&
                 strcmp(pszName, "ele") == 0 &&
                 lineString != NULL &&
                 ((gpxGeomType == GPX_ROUTE && depthLevel == interestingDepthLevel + 2) ||
                 (gpxGeomType == GPX_TRACK && depthLevel == interestingDepthLevel + 3)))
        {
            poFeature->GetGeometryRef()->setCoordinateDimension(3);

            if (nSubElementValueLen)
            {
                pszSubElementValue[nSubElementValueLen] = 0;
    
                double val = CPLAtof(pszSubElementValue);
                int i = lineString->getNumPoints() - 1;
                if (i >= 0)
                    lineString->setPoint(i, lineString->getX(i), lineString->getY(i), val);
            }

            CPLFree(pszSubElementName);
            pszSubElementName = NULL;
            CPLFree(pszSubElementValue);
            pszSubElementValue = NULL;
            nSubElementValueLen = 0;
        }
        else if (depthLevel == interestingDepthLevel + 1 &&
                 pszSubElementName && strcmp(pszName, pszSubElementName) == 0)
        {
            if (poFeature && pszSubElementValue && nSubElementValueLen)
            {
                pszSubElementValue[nSubElementValueLen] = 0;
                for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
                {
                    if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), pszName ) == 0)
                    {
                        poFeature->SetField( iField, pszSubElementValue);
                        break;
                    }
                }
            }

            CPLFree(pszSubElementName);
            pszSubElementName = NULL;
            CPLFree(pszSubElementValue);
            pszSubElementValue = NULL;
            nSubElementValueLen = 0;
        }
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRGPXLayer::dataHandlerCbk(const char *data, int nLen)
{
    if (pszSubElementName)
    {
        pszSubElementValue = (char*) CPLRealloc(pszSubElementValue, nSubElementValueLen + nLen + 1);
        memcpy(pszSubElementValue + nSubElementValueLen, data, nLen);
        nSubElementValueLen += nLen;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGPXLayer::GetNextFeature()
{
#ifdef HAVE_EXPAT
    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }
    
    if (VSIFEofL(fpGPX))
        return NULL;
    
    char aBuf[BUFSIZ];
    
    CPLFree(ppoFeatureTab);
    ppoFeatureTab = NULL;
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;

    int nDone;
    do
    {
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpGPX );
        nDone = VSIFEofL(fpGPX);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            break;
        }
    } while (!nDone && nFeatureTabLength == 0);
    
    return (nFeatureTabLength) ? ppoFeatureTab[nFeatureTabIndex++] : NULL;
#else
    return NULL;
#endif
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGPXLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                      WriteFeatureAttributes()                        */
/************************************************************************/

void OGRGPXLayer::WriteFeatureAttributes( OGRFeature *poFeature )
{
    FILE* fp = poDS->GetOutputFP();
    
    int n= poFeatureDefn->GetFieldCount();
    for(int i=0;i<n;i++)
    { 
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( i );
        if( poFeature->IsFieldSet( i ) )
        {
            VSIFPrintf(fp, "  <%s>%s</%s>\n",
                       poFieldDefn->GetNameRef(),
                       poFeature->GetFieldAsString( i ),
                       poFieldDefn->GetNameRef());
        }
    }
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGPXLayer::CreateFeature( OGRFeature *poFeature )

{
    FILE* fp = poDS->GetOutputFP();
    if (fp == NULL)
        return CE_Failure;
    
    OGRGeometry     *poGeom = poFeature->GetGeometryRef();
    
    if (gpxGeomType == GPX_WPT)
    {
        if (poDS->GetLastGPXGeomTypeWritten() == GPX_ROUTE)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Cannot write a 'wpt' element after a 'rte' element.\n");
            return OGRERR_FAILURE;
        }
        else
        if (poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Cannot write a 'wpt' element after a 'trk' element.\n");
            return OGRERR_FAILURE;
        }
        
        poDS->SetLastGPXGeomTypeWritten(gpxGeomType);
        
        switch( poGeom->getGeometryType() )
        {
            case wkbPoint:
            case wkbPoint25D:
            {
                OGRPoint* point = (OGRPoint*)poGeom;
                VSIFPrintf(fp, "<wpt lat=\"%.15f\" lon=\"%.15f\">\n", point->getY(), point->getX());
                WriteFeatureAttributes(poFeature);
                VSIFPrintf(fp, "</wpt>\n");
                break;
            }
            
            default:
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                    "Geometry type of `%s' not supported fort 'wpt' element.\n",
                    OGRGeometryTypeToName(poGeom->getGeometryType()) );
                return OGRERR_FAILURE;
            }
        }
    }
    else if (gpxGeomType == GPX_ROUTE)
    {
        if (poDS->GetLastGPXGeomTypeWritten() == GPX_TRACK)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Cannot write a 'rte' element after a 'trk' element.\n");
            return OGRERR_FAILURE;
        }
        
        poDS->SetLastGPXGeomTypeWritten(gpxGeomType);
            
        OGRLineString* line = NULL;
        switch( poGeom->getGeometryType() )
        {
            case wkbLineString:
            case wkbLineString25D:
            {
                line = (OGRLineString*)poGeom;
                break;
            }
            
            case wkbMultiLineString:
            case wkbMultiLineString25D:
            {
                if (((OGRGeometryCollection*)poGeom)->getNumGeometries () == 1)
                {
                    line = (OGRLineString*) ( ((OGRGeometryCollection*)poGeom)->getGeometryRef(0) );
                }
                else
                {
                    CPLError( CE_Failure, CPLE_NotSupported,
                            "Multiline with more than one line is not supported for 'rte' element.\n");
                    return OGRERR_FAILURE;
                }
                break;
            }

            default:
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                            "Geometry type of `%s' not supported for 'rte' element.\n",
                            OGRGeometryTypeToName(poGeom->getGeometryType()) );
                return OGRERR_FAILURE;
            }
        }
        
        if (line)
        {
            int n = line->getNumPoints();
            int i;
            VSIFPrintf(fp, "<rte>\n");
            WriteFeatureAttributes(poFeature);
            for(i=0;i<n;i++)
            {
                VSIFPrintf(fp, "  <rtept lat=\"%.15f\" lon=\"%.15f\">\n", line->getY(i), line->getX(i));
                if (poGeom->getGeometryType() == wkbLineString25D ||
                    poGeom->getGeometryType() == wkbMultiLineString25D)
                {
                    VSIFPrintf(fp, "    <ele>%f</ele>\n", line->getZ(i));
                }
                VSIFPrintf(fp, "  </rtept>\n");
            }
            VSIFPrintf(fp, "</rte>\n");
        }
    }
    else
    {
        poDS->SetLastGPXGeomTypeWritten(gpxGeomType);
        switch( poGeom->getGeometryType() )
        {
            case wkbLineString:
            case wkbLineString25D:
            {
                OGRLineString* line = (OGRLineString*)poGeom;
                int n = line->getNumPoints();
                int i;
                VSIFPrintf(fp, "<trk>\n");
                WriteFeatureAttributes(poFeature);
                VSIFPrintf(fp, "  <trkseg>\n");
                for(i=0;i<n;i++)
                {
                    VSIFPrintf(fp, "    <trkpt lat=\"%.15f\" lon=\"%.15f\">\n", line->getY(i), line->getX(i));
                    if (line->getGeometryType() == wkbLineString25D)
                    {
                        VSIFPrintf(fp, "        <ele>%f</ele>\n", line->getZ(i));
                    }
                    VSIFPrintf(fp, "    </trkpt>\n");
                }
                VSIFPrintf(fp, "  </trkseg>\n");
                VSIFPrintf(fp, "</trk>\n");
                break;
            }

            case wkbMultiLineString:
            case wkbMultiLineString25D:
            {
                int nGeometries = ((OGRGeometryCollection*)poGeom)->getNumGeometries ();
                VSIFPrintf(fp, "<trk>\n");
                WriteFeatureAttributes(poFeature);
                int j;
                for(j=0;j<nGeometries;j++)
                {
                    OGRLineString* line = (OGRLineString*) ( ((OGRGeometryCollection*)poGeom)->getGeometryRef(j) );
                    int n = line->getNumPoints();
                    int i;
                    VSIFPrintf(fp, "  <trkseg>\n");
                    for(i=0;i<n;i++)
                    {
                        VSIFPrintf(fp, "    <trkpt lat=\"%.15f\" lon=\"%.15f\">\n", line->getY(i), line->getX(i));
                        if (line->getGeometryType() == wkbLineString25D)
                        {
                            VSIFPrintf(fp, "        <ele>%f</ele>\n", line->getZ(i));
                        }
                        VSIFPrintf(fp, "    </trkpt>\n");
                    }
                    VSIFPrintf(fp, "  </trkseg>\n");
                }
                VSIFPrintf(fp, "</trk>\n");
                break;
            }

            default:
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                            "Geometry type of `%s' not supported for 'trk' element.\n",
                            OGRGeometryTypeToName(poGeom->getGeometryType()) );
                return OGRERR_FAILURE;
            }
        }
    }

    return OGRERR_NONE;
}



/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGPXLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                   poField->GetNameRef() ) == 0)
        {
            return OGRERR_NONE;
        }
    }
    CPLError(CE_Failure, CPLE_NotSupported,
             "Field of name '%s' is not supported in GPX schema",
                poField->GetNameRef());
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPXLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return bWriteMode;
    else
        return FALSE;
}

