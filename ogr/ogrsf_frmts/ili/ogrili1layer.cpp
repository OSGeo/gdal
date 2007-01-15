/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1Layer class.
 * Author:   Pirmin Kalberer, Sourcepole AG
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2005/12/19 17:33:21  pka
 * Interlis 1: Support for 100 columns (unlimited, if model given)
 * Interlis 1: Fixes for output
 * Interlis: Examples in driver documentation
 *
 * Revision 1.3  2005/09/21 00:59:36  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.2  2005/08/06 22:21:53  pka
 * Area polygonizer added
 *
 * Revision 1.1  2005/07/08 22:10:57  pka
 * Initial import of OGR Interlis driver
 *
 */

#include "ogr_ili1.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRILI1Layer()                              */
/************************************************************************/

OGRILI1Layer::OGRILI1Layer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRILI1DataSource *poDSIn )

{
    poFilterGeom = NULL;

    if( poSRSIn == NULL )
        poSRS = NULL;
    else
        poSRS = poSRSIn->Clone();

    poDS = poDSIn;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eReqType );

    nFeatures = 0;
    papoFeatures = NULL;
    nFeatureIdx = 0;

    bWriter = bWriterIn;
}

/************************************************************************/
/*                           ~OGRILI1Layer()                           */
/************************************************************************/

OGRILI1Layer::~OGRILI1Layer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();

    if( poFilterGeom != NULL )
        delete poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRILI1Layer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();
}

OGRErr OGRILI1Layer::AddFeature (OGRFeature *poFeature) {
    papoFeatures = (OGRFeature **)
        CPLRealloc( papoFeatures, sizeof(void*) * ++nFeatures );
    
    papoFeatures[nFeatures-1] = poFeature;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRILI1Layer::ResetReading(){
    nFeatureIdx = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRILI1Layer::GetNextFeature() {
    if (nFeatureIdx < nFeatures)
    {
      return papoFeatures[nFeatureIdx++];
    }
    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRILI1Layer::GetFeatureCount( int bForce ) {
    return nFeatures;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRILI1Layer::GetExtent(OGREnvelope *psExtent, int bForce ) {
  return OGRERR_NONE;
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

static void AppendCoordinateList( OGRLineString *poLine, OGRILI1DataSource *poDS)
{
    int         b3D = (poLine->getGeometryType() & wkb25DBit);

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        if (iPoint == 0) VSIFPrintf( poDS->GetTransferFile(), "STPT" );
        else VSIFPrintf( poDS->GetTransferFile(), "LIPT" );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getX(iPoint)) );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getY(iPoint)) );
        if (b3D) VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getZ(iPoint)) );
        VSIFPrintf( poDS->GetTransferFile(), "\n" );
    }
    VSIFPrintf( poDS->GetTransferFile(), "ELIN\n" );
}

int OGRILI1Layer::GeometryAppend( OGRGeometry *poGeometry )
{
/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        /* embedded in non-geometry fields
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getX()) );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getY()) );
        */
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        /* embedded in from non-geometry fields
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getX()) );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getY()) );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getZ()) );
        */
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString 
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        AppendCoordinateList( (OGRLineString *) poGeometry, poDS );
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon 
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        if( poPolygon->getExteriorRing() != NULL )
        {
            if( !GeometryAppend( poPolygon->getExteriorRing() ) )
                return FALSE;
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            if( !GeometryAppend( poRing ) )
                return FALSE;
        }
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

            if( !GeometryAppend( poMember ) )
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

OGRErr OGRILI1Layer::CreateFeature( OGRFeature *poFeature ) {
    VSIFPrintf( poDS->GetTransferFile(), "OBJE" );

    // Write all fields. 
    for(int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
          if ( !EQUAL(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), "ILI_Geometry") )
          {
            if ( poFeature->IsFieldSet( iField ) )
            {
                const char *pszRaw = poFeature->GetFieldAsString( iField );
                VSIFPrintf( poDS->GetTransferFile(), " %s", pszRaw );
            }
            else
            {
                VSIFPrintf( poDS->GetTransferFile(), " @" );
            }
        }
    }
    VSIFPrintf( poDS->GetTransferFile(), "\n" );

    // Write out Geometry
    if( poFeature->GetGeometryRef() != NULL )
    {
        if (EQUAL(poFeatureDefn->GetFieldDefn(poFeatureDefn->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))
        {
            VSIFPrintf( poDS->GetTransferFile(), poFeature->GetFieldAsString( poFeatureDefn->GetFieldCount()-1 ) );
        }
        else
        {
            GeometryAppend(poFeature->GetGeometryRef());
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI1Layer::TestCapability( const char * pszCap ) {
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRILI1Layer::CreateField( OGRFieldDefn *poField, int bApproxOK ) {
    poFeatureDefn->AddFieldDefn( poField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRILI1Layer::GetSpatialRef() {
    return poSRS;
}

