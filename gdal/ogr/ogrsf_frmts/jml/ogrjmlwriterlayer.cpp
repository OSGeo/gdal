/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRJMLWriterLayer()                        */
/************************************************************************/

OGRJMLWriterLayer::OGRJMLWriterLayer( const char* pszLayerName,
                                                OGRJMLDataset* poDS,
                                                VSILFILE* fp,
                                                int bAddRGBField,
                                                int bAddOGRStyleField,
                                                int bClassicGML )

{
    this->poDS = poDS;
    this->fp = fp;
    bFeaturesWritten = FALSE;
    this->bAddRGBField = bAddRGBField;
    this->bAddOGRStyleField = bAddOGRStyleField;
    this->bClassicGML = bClassicGML;
    nNextFID = 0;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    
    VSIFPrintfL(fp, "<?xml version='1.0' encoding='UTF-8'?>\n"
                    "<JCSDataFile xmlns:gml=\"http://www.opengis.net/gml\" "
                    "xmlns:xsi=\"http://www.w3.org/2000/10/XMLSchema-instance\" >\n"
                    "<JCSGMLInputTemplate>\n"
                    "<CollectionElement>featureCollection</CollectionElement>\n"
                    "<FeatureElement>feature</FeatureElement>\n"
                    "<GeometryElement>geometry</GeometryElement>\n"
                    "<ColumnDefinitions>\n");

}

/************************************************************************/
/*                        ~OGRJMLWriterLayer()                          */
/************************************************************************/

OGRJMLWriterLayer::~OGRJMLWriterLayer()
{
    if( !bFeaturesWritten )
        VSIFPrintfL(fp, "</ColumnDefinitions>\n</JCSGMLInputTemplate>\n<featureCollection>\n");
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
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRJMLWriterLayer::CreateFeature( OGRFeature *poFeature )

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
        VSIFPrintfL(fp, "</ColumnDefinitions>\n</JCSGMLInputTemplate>\n<featureCollection>\n");
        bFeaturesWritten = TRUE;
    }

    if( bClassicGML )
        VSIFPrintfL(fp, "   <featureMember>\n");
    VSIFPrintfL(fp, "     <feature>\n");

    /* Add geometry */
    VSIFPrintfL(fp, "          <geometry>\n");
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom != NULL )
    {
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
        if( poFeature->IsFieldSet(i) )
        {
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString )
            {
                char* pszValue = OGRGetXML_UTF8_EscapedString(
                                            poFeature->GetFieldAsString(i) );
                VSIFPrintfL(fp, "%s", pszValue);
                CPLFree(pszValue);
            }
            else if( eType == OFTDateTime )
            {
                int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                poFeature->GetFieldAsDateTime(i,
                                    &nYear, &nMonth, &nDay,
                                    &nHour, &nMinute, &nSecond,
                                    &nTZFlag );
                VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%02d",
                            nYear, nMonth, nDay, nHour, nMinute, nSecond);
                /* When writing time zone, OpenJUMP expects .XXX seconds */
                /* to be written */
                if( nTZFlag > 1 )
                {
                    int nOffset = (nTZFlag - 100) * 15;
                    int nHours = (int) (nOffset / 60);  // round towards zero
                    int nMinutes = ABS(nOffset - nHours * 60);

                    VSIFPrintfL(fp, ".000");
                    if( nOffset < 0 )
                    {
                        VSIFPrintfL(fp, "-" );
                        nHours = ABS(nHours);
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
        if( poFeature->GetStyleString() != NULL )
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
        if( poFeature->GetStyleString() != NULL )
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            OGRwkbGeometryType eGeomType =
                poGeom ? wkbFlatten(poGeom->getGeometryType()) : wkbUnknown;
            OGRStyleMgr oMgr;
            oMgr.InitFromFeature(poFeature);
            for(int i=0;i<oMgr.GetPartCount();i++)
            {
                OGRStyleTool* poTool = oMgr.GetPart(i);
                if( poTool != NULL )
                {
                    const char* pszColor = NULL;
                    if( poTool->GetType() == OGRSTCPen &&
                        eGeomType != wkbPolygon && eGeomType != wkbMultiPolygon )
                    {
                        GBool bIsNull;
                        pszColor = ((OGRStylePen*)poTool)->Color(bIsNull);
                        if( bIsNull ) pszColor = NULL;
                    }
                    else if( poTool->GetType() == OGRSTCBrush )
                    {
                        GBool bIsNull;
                        pszColor = ((OGRStyleBrush*)poTool)->ForeColor(bIsNull);
                        if( bIsNull ) pszColor = NULL;
                    }
                    int R, G, B, A;
                    if( pszColor != NULL &&
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
                                            CPL_UNUSED int bApproxOK )
{
    if( bFeaturesWritten )
        return OGRERR_FAILURE;
    
    if( !bAddRGBField && strcmp( poFieldDefn->GetNameRef(), "R_G_B" ) == 0 )
        return OGRERR_FAILURE;

    const char* pszType;
    OGRFieldType eType = poFieldDefn->GetType();
    if( eType == OFTInteger )
        pszType = "INTEGER";
    else if( eType == OFTReal )
        pszType = "DOUBLE";
    else if( eType == OFTDate || eType == OFTDateTime )
        pszType = "DATE";
    else
        pszType = "STRING";
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
    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;
    else if( EQUAL(pszCap,OLCCreateField) )
        return !bFeaturesWritten;
    else 
        return FALSE;
}
