/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

OGRILI2Layer::OGRILI2Layer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRILI2DataSource *poDSIn )

{
    if( poSRSIn == NULL )
        poSRS = NULL;
    else
        poSRS = poSRSIn->Clone();

    poDS = poDSIn;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eReqType );

    bWriter = bWriterIn;

    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           ~OGRILI2Layer()                           */
/************************************************************************/

OGRILI2Layer::~OGRILI2Layer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();

    listFeatureIt = listFeature.begin();
    while(listFeatureIt != listFeature.end())
    {
      OGRFeature *poFeature = *(listFeatureIt++);
      delete poFeature;
    }
}


/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRILI2Layer::SetFeature (OGRFeature *poFeature) {
    listFeature.push_back(poFeature);
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRILI2Layer::ResetReading(){
    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRILI2Layer::GetNextFeature() {
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
        sprintf( strbuf, "%.16g", val );
    else if( fabs(val) > 100000000.0  )
        sprintf( strbuf, "%.16g", val );
    else
        sprintf( strbuf, "%.3f", val );
    return strbuf;
}

static void AppendCoordinateList( OGRLineString *poLine, IOM_OBJECT sequence)
{
	IOM_OBJECT coordValue;
    int         b3D = (poLine->getGeometryType() & wkb25DBit);

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        coordValue=iom_addattrobj(sequence,"segment","COORD");
        iom_setattrvalue(coordValue,"C1", d2str(poLine->getX(iPoint)));
        iom_setattrvalue(coordValue,"C2", d2str(poLine->getY(iPoint)));
        if (b3D) iom_setattrvalue(coordValue,"C3", d2str(poLine->getZ(iPoint)));
        iom_releaseobject(coordValue);
    }
}

static int OGR2ILIGeometryAppend( OGRGeometry *poGeometry, IOM_OBJECT obj, const char *attrname )
{
    IOM_OBJECT polylineValue;
    IOM_OBJECT sequence;
    IOM_OBJECT coordValue;
    IOM_OBJECT multisurface;
    IOM_OBJECT surface;
    IOM_OBJECT boundary;

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        coordValue=iom_changeattrobj(obj,attrname,0,"COORD");
        iom_setattrvalue(coordValue,"C1", d2str(poPoint->getX()));
        iom_setattrvalue(coordValue,"C2", d2str(poPoint->getY()));
        iom_releaseobject(coordValue);
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        coordValue=iom_changeattrobj(obj,attrname,0,"COORD");
        iom_setattrvalue(coordValue,"C1", d2str(poPoint->getX()));
        iom_setattrvalue(coordValue,"C2", d2str(poPoint->getY()));
        iom_setattrvalue(coordValue,"C3", d2str(poPoint->getZ()));
        iom_releaseobject(coordValue);
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString 
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        //int bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");
        if (attrname) polylineValue=iom_changeattrobj(obj,attrname,0,"POLYLINE");
        else polylineValue=iom_addattrobj(obj,"polyline","POLYLINE");
        // unclipped polyline, add one sequence
        sequence=iom_changeattrobj(polylineValue,"sequence",0,"SEGMENTS");
        AppendCoordinateList( (OGRLineString *) poGeometry, sequence );
        iom_releaseobject(sequence);
        iom_releaseobject(polylineValue);
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon 
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        multisurface=iom_changeattrobj(obj,attrname,0,"MULTISURFACE");
        surface=iom_changeattrobj(multisurface,"surface",0,"SURFACE");
        boundary=iom_changeattrobj(surface,"boundary",0,"BOUNDARY");

        if( poPolygon->getExteriorRing() != NULL )
        {
            if( !OGR2ILIGeometryAppend( poPolygon->getExteriorRing(), boundary, NULL  ) )
                return FALSE;
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            if( !OGR2ILIGeometryAppend( poRing, boundary, NULL ) )
                return FALSE;
        }
        iom_releaseobject(boundary);
        iom_releaseobject(surface);
        iom_releaseobject(multisurface);
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

            if( !OGR2ILIGeometryAppend( poMember, obj, NULL ) )
                return FALSE;
        }

    }

    else
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRILI2Layer::CreateFeature( OGRFeature *poFeature ) {
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
    // create new object
    IOM_OBJECT obj;
    obj=iom_newobject(poDS->GetBasket(), poFeatureDefn->GetName(), tid);

    // Write out Geometry
    if( poFeature->GetGeometryRef() != NULL )
    {
        OGR2ILIGeometryAppend(poFeature->GetGeometryRef(), obj, "Geometry");
    }
    // Write all "set" fields. 
    for( ; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        
        OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) )
        {
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            //while( *pszRaw == ' ' )
            //    pszRaw++;

            //char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

            iom_setattrvalue(obj, poField->GetNameRef(), pszRaw);
            //CPLFree( pszEscaped );
        }
    }

    iom_releaseobject(obj);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI2Layer::TestCapability( const char * pszCap ) {
   return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRILI2Layer::CreateField( OGRFieldDefn *poField, int bApproxOK ) {
    poFeatureDefn->AddFieldDefn( poField );
    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRILI2Layer::GetSpatialRef() {
    return poSRS;
}
