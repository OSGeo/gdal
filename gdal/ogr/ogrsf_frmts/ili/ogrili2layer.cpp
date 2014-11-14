/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_ili2.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRILI2Layer()                              */
/************************************************************************/

OGRILI2Layer::OGRILI2Layer( OGRFeatureDefn* poFeatureDefnIn,
                            GeomFieldInfos oGeomFieldInfosIn,
                            OGRILI2DataSource *poDSIn )
{
    poFeatureDefn = poFeatureDefnIn;
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    oGeomFieldInfos = oGeomFieldInfosIn;

    poDS = poDSIn;

    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           ~OGRILI2Layer()                           */
/************************************************************************/

OGRILI2Layer::~OGRILI2Layer()
{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    listFeatureIt = listFeature.begin();
    while(listFeatureIt != listFeature.end())
    {
      OGRFeature *poFeature = *(listFeatureIt++);
      delete poFeature;
    }
}


/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRILI2Layer::ISetFeature (OGRFeature *poFeature)
{
    listFeature.push_back(poFeature);
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRILI2Layer::ResetReading()
{
    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRILI2Layer::GetNextFeature()
{
    OGRFeature *poFeature = NULL;
    while (listFeatureIt != listFeature.end())
    {
      poFeature = *(listFeatureIt++);
      //apply filters
      if( (m_poFilterGeom == NULL
           || FilterGeometry( poFeature->GetGeometryRef() ) )
          && (m_poAttrQuery == NULL
              || m_poAttrQuery->Evaluate( poFeature )) )
          return poFeature->Clone();
    }
    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRILI2Layer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        return listFeature.size();
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

static char* d2str(double val)
{
    static char strbuf[255];
    if( val == (int) val )
        sprintf( strbuf, "%d", (int) val );
    else if( fabs(val) < 370 )
        CPLsprintf( strbuf, "%.16g", val );
    else if( fabs(val) > 100000000.0  )
        CPLsprintf( strbuf, "%.16g", val );
    else
        CPLsprintf( strbuf, "%.3f", val );
    return strbuf;
}

static void AppendCoordinateList( OGRLineString *poLine, VSILFILE* fp )
{
    int         b3D = wkbHasZ(poLine->getGeometryType());

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        VSIFPrintfL(fp, "<COORD>");
        VSIFPrintfL(fp, "<C1>%s</C1>", d2str(poLine->getX(iPoint)));
        VSIFPrintfL(fp, "<C2>%s</C2>", d2str(poLine->getY(iPoint)));
        if (b3D) VSIFPrintfL(fp, "<C3>%s</C3>", d2str(poLine->getZ(iPoint)));
        VSIFPrintfL(fp, "</COORD>\n");
    }
}

static int OGR2ILIGeometryAppend( OGRGeometry *poGeometry, VSILFILE* fp, const char *attrname, CPLString iliGeomType )
{
    //CPLDebug( "OGR_ILI", "OGR2ILIGeometryAppend getGeometryType %s iliGeomType %s", poGeometry->getGeometryName(), iliGeomType.c_str());
/* -------------------------------------------------------------------- */
/*      2D/3D Point                                                     */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint || poGeometry->getGeometryType() == wkbPoint25D )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        VSIFPrintfL(fp, "<%s>\n", attrname);
        VSIFPrintfL(fp, "<COORD>");
        VSIFPrintfL(fp, "<C1>%s</C1>", d2str(poPoint->getX()));
        VSIFPrintfL(fp, "<C2>%s</C2>", d2str(poPoint->getY()));
        if( poGeometry->getGeometryType() == wkbPoint25D )
            VSIFPrintfL(fp, "<C3>%s</C3>", d2str(poPoint->getZ()));
        VSIFPrintfL(fp, "</COORD>\n");
        VSIFPrintfL(fp, "</%s>\n", attrname);
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        if (attrname) VSIFPrintfL(fp, "<%s>\n", attrname);
        VSIFPrintfL(fp, "<POLYLINE>\n");
        // unclipped polyline, add one sequence
        // VSIFPrintfL(fp, "<SEGMENTS>\n");
        AppendCoordinateList( (OGRLineString *) poGeometry, fp );
        // VSIFPrintfL(fp, "</SEGMENTS>\n");
        VSIFPrintfL(fp, "</POLYLINE>\n");
        if (attrname) VSIFPrintfL(fp, "</%s>\n", attrname);
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        if (attrname) VSIFPrintfL(fp, "<%s>\n", attrname);
        if( iliGeomType == "Surface" || iliGeomType == "Area" )
        {
            //VSIFPrintfL(fp, "<MULTISURFACE>\n");
            VSIFPrintfL(fp, "<SURFACE>\n");
            VSIFPrintfL(fp, "<BOUNDARY>\n");
        }

        if( poPolygon->getExteriorRing() != NULL )
        {
            if( !OGR2ILIGeometryAppend( poPolygon->getExteriorRing(), fp, NULL, "" ) )
                return FALSE;
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            if( !OGR2ILIGeometryAppend( poRing, fp, NULL, "" ) )
                return FALSE;
        }
        if( iliGeomType == "Surface" || iliGeomType == "Area" )
        {
            VSIFPrintfL(fp, "</BOUNDARY>\n");
            VSIFPrintfL(fp, "</SURFACE>\n");
            //VSIFPrintfL(fp, "</MULTISURFACE>\n");
        }
        if (attrname) VSIFPrintfL(fp, "</%s>\n", attrname);
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon                                                    */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon )
        {
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString )
        {
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
        {
        }
        else
        {
        }

        for( iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            if( !OGR2ILIGeometryAppend( poMember, fp, NULL, "" ) )
                return FALSE;
        }

    }

    else
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRILI2Layer::ICreateFeature( OGRFeature *poFeature ) {
    static char         szTempBuffer[80];
    const char* tid;
    int iField = 0;
    if (poFeatureDefn->GetFieldCount() && EQUAL(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), "TID"))
    {
        tid = poFeature->GetFieldAsString(0);
        ++iField;
    }
    else
    {
        sprintf( szTempBuffer, "%ld", poFeature->GetFID() );
        tid = szTempBuffer;
    }

    VSILFILE* fp = poDS->GetOutputFP();
    if (fp == NULL)
        return CE_Failure;

    VSIFPrintfL(fp, "<%s TID=\"%s\">\n", poFeatureDefn->GetName(), tid);

    // Write out Geometries
    for( int iGeomField = 0; iGeomField < poFeatureDefn->GetGeomFieldCount(); iGeomField++ )
    {
        OGRGeomFieldDefn *poFieldDefn = poFeatureDefn->GetGeomFieldDefn(iGeomField);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if( poGeom != NULL )
        {
            CPLString iliGeomType = GetIliGeomType(poFieldDefn->GetNameRef());
            OGR2ILIGeometryAppend(poGeom, fp, poFieldDefn->GetNameRef(), iliGeomType);
        }
    }

    // Write all "set" fields. 
    for( ; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        
        OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) )
        {
            const char *pszRaw = poFeature->GetFieldAsString( iField );
            VSIFPrintfL(fp, "<%s>%s</%s>\n", poField->GetNameRef(), pszRaw, poField->GetNameRef());
        }
    }

    VSIFPrintfL(fp, "</%s>\n", poFeatureDefn->GetName());

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI2Layer::TestCapability( CPL_UNUSED const char * pszCap ) {
    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRILI2Layer::CreateField( OGRFieldDefn *poField, CPL_UNUSED int bApproxOK ) {
    poFeatureDefn->AddFieldDefn( poField );
    return OGRERR_NONE;
}
