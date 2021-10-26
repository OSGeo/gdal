/******************************************************************************
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_ili2.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRILI2Layer()                              */
/************************************************************************/

OGRILI2Layer::OGRILI2Layer( OGRFeatureDefn* poFeatureDefnIn,
                            const GeomFieldInfos& oGeomFieldInfosIn,
                            OGRILI2DataSource *poDSIn ) :
    poFeatureDefn(poFeatureDefnIn),
    oGeomFieldInfos(oGeomFieldInfosIn),
    poDS(poDSIn)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

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
/*                             AddFeature()                             */
/************************************************************************/

void OGRILI2Layer::AddFeature (OGRFeature *poFeature)
{
    poFeature->SetFID( static_cast<GIntBig>(1 + listFeature.size()) );
    listFeature.push_back(poFeature);
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
    while (listFeatureIt != listFeature.end())
    {
      OGRFeature *poFeature = *(listFeatureIt++);
      //apply filters
      if( (m_poFilterGeom == nullptr
           || FilterGeometry( poFeature->GetGeometryRef() ) )
          && (m_poAttrQuery == nullptr
              || m_poAttrQuery->Evaluate( poFeature )) )
          return poFeature->Clone();
    }
    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRILI2Layer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        return listFeature.size();
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

static const char* d2str(double val)
{
    if( val == (int) val )
        return CPLSPrintf("%d", (int) val );
    if( fabs(val) < 370 )
        return CPLSPrintf("%.16g", val );
    if( fabs(val) > 100000000.0  )
        return CPLSPrintf("%.16g", val );

    return CPLSPrintf("%.3f", val );
}

static void AppendCoordinateList( const OGRLineString *poLine, VSILFILE* fp )
{
    const bool b3D = CPL_TO_BOOL(wkbHasZ(poLine->getGeometryType()));

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        VSIFPrintfL(fp, "<COORD>");
        VSIFPrintfL(fp, "<C1>%s</C1>", d2str(poLine->getX(iPoint)));
        VSIFPrintfL(fp, "<C2>%s</C2>", d2str(poLine->getY(iPoint)));
        if (b3D)
            VSIFPrintfL(fp, "<C3>%s</C3>", d2str(poLine->getZ(iPoint)));
        VSIFPrintfL(fp, "</COORD>\n");
    }
}

static int OGR2ILIGeometryAppend( const OGRGeometry *poGeometry, VSILFILE* fp,
                                  const char *attrname, CPLString iliGeomType )
{
#ifdef DEBUG_VERBOSE
    CPLDebug( "OGR_ILI",
              "OGR2ILIGeometryAppend getGeometryType %s iliGeomType %s",
              poGeometry->getGeometryName(), iliGeomType.c_str());
#endif
/* -------------------------------------------------------------------- */
/*      2D/3D Point                                                     */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint ||
        poGeometry->getGeometryType() == wkbPoint25D )
    {
        const OGRPoint *poPoint = poGeometry->toPoint();

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
        AppendCoordinateList( poGeometry->toLineString(), fp );
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
        const OGRPolygon      *poPolygon = poGeometry->toPolygon();

        if (attrname) VSIFPrintfL(fp, "<%s>\n", attrname);
        if( iliGeomType == "Surface" || iliGeomType == "Area" )
        {
            //VSIFPrintfL(fp, "<MULTISURFACE>\n");
            VSIFPrintfL(fp, "<SURFACE>\n");
            VSIFPrintfL(fp, "<BOUNDARY>\n");
        }

        for( auto&& poRing: *poPolygon )
        {
            if( !OGR2ILIGeometryAppend( poRing, fp, nullptr, "" ) )
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
        const OGRGeometryCollection *poGC = poGeometry->toGeometryCollection();

#if 0
        // TODO: Why were these all blank?
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
#endif
        for( auto&& poMember: *poGC )
        {
            if( !OGR2ILIGeometryAppend( poMember, fp, nullptr, "" ) )
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
    char szTempBuffer[80];
    const char* tid = nullptr;
    int iField = 0;
    if( poFeatureDefn->GetFieldCount() &&
        EQUAL(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), "TID") )
    {
        tid = poFeature->GetFieldAsString(0);
        ++iField;
    }
    else
    {
        snprintf( szTempBuffer, sizeof(szTempBuffer), CPL_FRMT_GIB,
                  poFeature->GetFID() );
        tid = szTempBuffer;
    }

    VSILFILE* fp = poDS->GetOutputFP();
    if (fp == nullptr)
        return OGRERR_FAILURE;

    VSIFPrintfL(fp, "<%s TID=\"%s\">\n", poFeatureDefn->GetName(), tid);

    // Write out Geometries
    for( int iGeomField = 0;
         iGeomField < poFeatureDefn->GetGeomFieldCount();
         iGeomField++ )
    {
        OGRGeomFieldDefn *poFieldDefn
            = poFeatureDefn->GetGeomFieldDefn(iGeomField);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if( poGeom != nullptr )
        {
            CPLString iliGeomType = GetIliGeomType(poFieldDefn->GetNameRef());
            OGR2ILIGeometryAppend( poGeom, fp, poFieldDefn->GetNameRef(),
                                   iliGeomType );
        }
    }

    // Write all "set" fields.
    for( ; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {

        OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSetAndNotNull( iField ) )
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
    if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRILI2Layer::CreateField( OGRFieldDefn *poField, int /* bApproxOK */ ) {
    poFeatureDefn->AddFieldDefn( poField );
    return OGRERR_NONE;
}
