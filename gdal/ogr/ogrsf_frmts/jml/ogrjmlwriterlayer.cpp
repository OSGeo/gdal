/******************************************************************************
 *
 * Project:  JML Translator
 * Purpose:  Implements OGRJMLWriterLayer class.
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

#include <cstdlib>

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRJMLWriterLayer()                        */
/************************************************************************/

OGRJMLWriterLayer::OGRJMLWriterLayer( const char* pszLayerName,
                                      OGRSpatialReference * poSRS,
                                      OGRJMLDataset * poDSIn,
                                      VSILFILE* fpIn,
                                      bool bAddRGBFieldIn,
                                      bool bAddOGRStyleFieldIn,
                                      bool bClassicGMLIn ) :
    poDS(poDSIn),
    poFeatureDefn(new OGRFeatureDefn( pszLayerName )),
    fp(fpIn),
    bFeaturesWritten(false),
    bAddRGBField(bAddRGBFieldIn),
    bAddOGRStyleField(bAddOGRStyleFieldIn),
    bClassicGML(bClassicGMLIn),
    nNextFID(0),
    nBBoxOffset(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    if( poSRS )
    {
        const char* pszAuthName = poSRS->GetAuthorityName(nullptr);
        const char* pszAuthCode = poSRS->GetAuthorityCode(nullptr);
        if( pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG") &&
            pszAuthCode != nullptr )
        {
            osSRSAttr = " srsName=\"http://www.opengis.net/gml/srs/epsg.xml#";
            osSRSAttr += pszAuthCode;
            osSRSAttr += "\"";
        }
    }

    VSIFPrintfL(fp, "<?xml version='1.0' encoding='UTF-8'?>\n"
                    "<JCSDataFile xmlns:gml=\"http://www.opengis.net/gml\" "
                    "xmlns:xsi=\"http://www.w3.org/2000/10/XMLSchema-instance\" >\n"
                    "<JCSGMLInputTemplate>\n"
                    "<CollectionElement>featureCollection</CollectionElement>\n"
                    "<FeatureElement>feature</FeatureElement>\n"
                    "<GeometryElement>geometry</GeometryElement>\n"
                    "<CRSElement>boundedBy</CRSElement>\n"
                    "<ColumnDefinitions>\n");
}

/************************************************************************/
/*                        ~OGRJMLWriterLayer()                          */
/************************************************************************/

OGRJMLWriterLayer::~OGRJMLWriterLayer()
{
    if( !bFeaturesWritten )
    {
        VSIFPrintfL(
            fp, "</ColumnDefinitions>\n</JCSGMLInputTemplate>\n"
            "<featureCollection>\n"
            "  <gml:boundedBy>\n"
            "    <gml:Box%s>\n"
            "      <gml:coordinates decimal=\".\" cs=\",\" ts=\" \">0.00,0.00 -1.00,-1.00</gml:coordinates>\n"
            "    </gml:Box>\n"
            "  </gml:boundedBy>\n", osSRSAttr.c_str() );
    }
    else if( nBBoxOffset > 0 )
    {
        VSIFSeekL(fp, nBBoxOffset, SEEK_SET );
        if( sLayerExtent.IsInit() )
        {
            char szBuffer[101];
            CPLsnprintf(szBuffer, sizeof(szBuffer), "%.10f,%.10f %.10f,%.10f",
                        sLayerExtent.MinX, sLayerExtent.MinY,
                        sLayerExtent.MaxX, sLayerExtent.MaxY);
            VSIFPrintfL(fp, "%s", szBuffer);
        }
        else
        {
            VSIFPrintfL(fp, "0.00,0.00 -1.00,-1.00");
        }
        VSIFSeekL(fp, 0, SEEK_END );
    }
    VSIFPrintfL(fp, "</featureCollection>\n</JCSDataFile>\n");
    poFeatureDefn->Release();
}

/************************************************************************/
/*                         WriteColumnDeclaration()                     */
/************************************************************************/

void OGRJMLWriterLayer::WriteColumnDeclaration( const char* pszName,
                                                     const char* pszType )
{
    char* pszEscapedName = OGRGetXML_UTF8_EscapedString( pszName );
    if( bClassicGML )
    {
        VSIFPrintfL(fp, "     <column>\n"
                        "          <name>%s</name>\n"
                        "          <type>%s</type>\n"
                        "          <valueElement elementName=\"%s\"/>\n"
                        "          <valueLocation position=\"body\"/>\n"
                        "     </column>\n",
                    pszEscapedName, pszType, pszEscapedName);
    }
    else
    {
        VSIFPrintfL(fp, "     <column>\n"
                        "          <name>%s</name>\n"
                        "          <type>%s</type>\n"
                        "          <valueElement elementName=\"property\" attributeName=\"name\" attributeValue=\"%s\"/>\n"
                        "          <valueLocation position=\"body\"/>\n"
                        "     </column>\n",
                    pszEscapedName, pszType, pszEscapedName);
    }
    CPLFree(pszEscapedName);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRJMLWriterLayer::ICreateFeature( OGRFeature *poFeature )

{
    /* Finish column declaration if we haven't yet created a feature */
    if( !bFeaturesWritten )
    {
        if( bAddOGRStyleField && poFeatureDefn->GetFieldIndex("OGR_STYLE") < 0 )
        {
            WriteColumnDeclaration( "OGR_STYLE", "STRING" );
        }
        if( bAddRGBField && poFeatureDefn->GetFieldIndex("R_G_B") < 0 )
        {
            WriteColumnDeclaration( "R_G_B", "STRING" );
        }
        VSIFPrintfL( fp, "</ColumnDefinitions>\n</JCSGMLInputTemplate>\n"
                     "<featureCollection>\n"
                     "  <gml:boundedBy>\n"
                     "    <gml:Box%s>\n"
                     "      <gml:coordinates decimal=\".\" cs=\",\" ts=\" \">",
                     osSRSAttr.c_str() );
        if( EQUAL(poDS->GetDescription(), "/vsistdout/") )
        {
            VSIFPrintfL( fp, "0.00,0.00 -1.00,-1.00" );
        }
        else
        {
            nBBoxOffset = VSIFTellL(fp);
            VSIFPrintfL( fp,
                         // 100 characters reserved
                         "                                                  "
                         "                                                  ");
        }
        VSIFPrintfL( fp, "</gml:coordinates>\n"
                         "    </gml:Box>\n"
                         "  </gml:boundedBy>\n" );
        bFeaturesWritten = true;
    }

    if( bClassicGML )
        VSIFPrintfL(fp, "   <featureMember>\n");
    VSIFPrintfL(fp, "     <feature>\n");

    /* Add geometry */
    VSIFPrintfL(fp, "          <geometry>\n");
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom != nullptr )
    {
        if( !poGeom->IsEmpty() )
        {
            OGREnvelope sExtent;
            poGeom->getEnvelope(&sExtent);
            sLayerExtent.Merge(sExtent);
        }
        char* pszGML = poGeom->exportToGML();
        VSIFPrintfL(fp, "                %s\n", pszGML);
        CPLFree(pszGML);
    }
    else
    {
        VSIFPrintfL(fp, "                %s\n",
                    "<gml:MultiGeometry></gml:MultiGeometry>");
    }
    VSIFPrintfL(fp, "          </geometry>\n");

    /* Add fields */
    for(int i=0;i<poFeature->GetFieldCount();i++)
    {
        char* pszName = OGRGetXML_UTF8_EscapedString(
                                poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        if( bClassicGML )
            VSIFPrintfL(fp, "          <%s>", pszName);
        else
            VSIFPrintfL(fp, "          <property name=\"%s\">", pszName);
        if( poFeature->IsFieldSetAndNotNull(i) )
        {
            const OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString )
            {
                char* pszValue = OGRGetXML_UTF8_EscapedString(
                                            poFeature->GetFieldAsString(i) );
                VSIFPrintfL(fp, "%s", pszValue);
                CPLFree(pszValue);
            }
            else if( eType == OFTDateTime )
            {
                int nYear = 0;
                int nMonth = 0;
                int nDay = 0;
                int nHour = 0;
                int nMinute = 0;
                int nTZFlag = 0;
                float fSecond = 0.0f;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                              &nHour, &nMinute, &fSecond, &nTZFlag);
                /* When writing time zone, OpenJUMP expects .XXX seconds */
                /* to be written */
                if( nTZFlag > 1 || OGR_GET_MS(fSecond) != 0 )
                    VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%06.3f",
                                nYear, nMonth, nDay,
                                nHour, nMinute, fSecond);
                else
                    VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%02d",
                                nYear, nMonth, nDay,
                                nHour, nMinute, (int)fSecond);
                if( nTZFlag > 1 )
                {
                    int nOffset = (nTZFlag - 100) * 15;
                    int nHours = (int) (nOffset / 60);  // round towards zero
                    int nMinutes = std::abs(nOffset - nHours * 60);

                    if( nOffset < 0 )
                    {
                        VSIFPrintfL(fp, "-" );
                        nHours = std::abs(nHours);
                    }
                    else
                        VSIFPrintfL(fp, "+" );

                    VSIFPrintfL(fp, "%02d%02d", nHours, nMinutes );
                }
            }
            else
            {
                VSIFPrintfL(fp, "%s", poFeature->GetFieldAsString(i));
            }
        }
        if( bClassicGML )
            VSIFPrintfL(fp, "</%s>\n", pszName);
        else
            VSIFPrintfL(fp, "</property>\n");
        CPLFree(pszName);
    }

    /* Add OGR_STYLE from feature style string (if asked) */
    if( bAddOGRStyleField && poFeatureDefn->GetFieldIndex("OGR_STYLE") < 0 )
    {
        if( bClassicGML )
            VSIFPrintfL(fp, "          <OGR_STYLE>");
        else
            VSIFPrintfL(fp, "          <property name=\"%s\">", "OGR_STYLE");
        if( poFeature->GetStyleString() != nullptr )
        {
            char* pszValue = OGRGetXML_UTF8_EscapedString( poFeature->GetStyleString() );
            VSIFPrintfL(fp, "%s", pszValue);
            CPLFree(pszValue);
        }
        if( bClassicGML )
            VSIFPrintfL(fp, "</OGR_STYLE>\n");
        else
            VSIFPrintfL(fp, "</property>\n");
    }

    /* Derive R_G_B field from feature style string */
    if( bAddRGBField && poFeatureDefn->GetFieldIndex("R_G_B") < 0 )
    {
        if( bClassicGML )
            VSIFPrintfL(fp, "          <R_G_B>");
        else
            VSIFPrintfL(fp, "          <property name=\"%s\">", "R_G_B");
        if( poFeature->GetStyleString() != nullptr )
        {
            OGRwkbGeometryType eGeomType =
                poGeom ? wkbFlatten(poGeom->getGeometryType()) : wkbUnknown;
            OGRStyleMgr oMgr;
            oMgr.InitFromFeature(poFeature);
            for(int i=0;i<oMgr.GetPartCount();i++)
            {
                OGRStyleTool* poTool = oMgr.GetPart(i);
                if( poTool != nullptr )
                {
                    const char* pszColor = nullptr;
                    if( poTool->GetType() == OGRSTCPen &&
                        eGeomType != wkbPolygon && eGeomType != wkbMultiPolygon )
                    {
                        GBool bIsNull;
                        pszColor = ((OGRStylePen*)poTool)->Color(bIsNull);
                        if( bIsNull ) pszColor = nullptr;
                    }
                    else if( poTool->GetType() == OGRSTCBrush )
                    {
                        GBool bIsNull;
                        pszColor = ((OGRStyleBrush*)poTool)->ForeColor(bIsNull);
                        if( bIsNull ) pszColor = nullptr;
                    }
                    int R, G, B, A;
                    if( pszColor != nullptr &&
                        poTool->GetRGBFromString(pszColor, R, G, B, A) && A != 0 )
                    {
                        VSIFPrintfL(fp, "%02X%02X%02X", R, G, B);
                    }
                    delete poTool;
                }
            }
        }
        if( bClassicGML )
            VSIFPrintfL(fp, "</R_G_B>\n");
        else
            VSIFPrintfL(fp, "</property>\n");
    }

    VSIFPrintfL(fp, "     </feature>\n");
    if( bClassicGML )
        VSIFPrintfL(fp, "   </featureMember>\n");

    poFeature->SetFID(nNextFID ++);

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRJMLWriterLayer::CreateField( OGRFieldDefn *poFieldDefn,
                                       int bApproxOK )
{
    if( bFeaturesWritten )
        return OGRERR_FAILURE;

    if( !bAddRGBField && strcmp( poFieldDefn->GetNameRef(), "R_G_B" ) == 0 )
        return OGRERR_FAILURE;

    const char* pszType = nullptr;
    OGRFieldType eType = poFieldDefn->GetType();
    if( eType == OFTInteger )
    {
        pszType = "INTEGER";
    }
    else if( eType == OFTInteger64 )
    {
        pszType = "OBJECT";
    }
    else if( eType == OFTReal )
    {
        pszType = "DOUBLE";
    }
    else if( eType == OFTDate || eType == OFTDateTime )
    {
        pszType = "DATE";
    }
    else
    {
        if( eType != OFTString )
        {
            if( bApproxOK )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Field of type %s unhandled natively. Converting to string",
                        OGRFieldDefn::GetFieldTypeName(eType));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field of type %s unhandled natively.",
                         OGRFieldDefn::GetFieldTypeName(eType));
                return OGRERR_FAILURE;
            }
        }
        pszType = "STRING";
    }
    WriteColumnDeclaration( poFieldDefn->GetNameRef(), pszType );

    poFeatureDefn->AddFieldDefn( poFieldDefn );
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJMLWriterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;
    if( EQUAL(pszCap,OLCCreateField) )
        return !bFeaturesWritten;

    return FALSE;
}
