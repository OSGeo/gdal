/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDriver and helper classes. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.4  1999/07/08 20:11:49  warmerda
 * fixed computation of feature count when spatial filter is in effect.
 *
 * Revision 1.3  1999/07/08 20:05:45  warmerda
 * added GetFeatureCount()
 *
 * Revision 1.2  1999/07/07 13:12:47  warmerda
 * Added spatial searching
 *
 * Revision 1.1  1999/07/05 18:58:07  warmerda
 * New
 *
 */

#include "ogrshape.h"
#include "cpl_conv.h"

/************************************************************************/
/* ==================================================================== */
/*			       OGRShapeLayer				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           OGRShapeLayer()                            */
/************************************************************************/

OGRShapeLayer::OGRShapeLayer( SHPHandle hSHPIn, DBFHandle hDBFIn )

{
    poFilterGeom = NULL;
    
    hSHP = hSHPIn;
    hDBF = hDBFIn;

    iNextShapeId = 0;

    nTotalShapeCount = hSHP->nRecords;
    
    poFeatureDefn = SHPReadOGRFeatureDefn( hSHP, hDBF );
    
}

/************************************************************************/
/*                           ~OGRShapeLayer()                           */
/************************************************************************/

OGRShapeLayer::~OGRShapeLayer()

{
    delete poFeatureDefn;

    if( hDBF != NULL )
        DBFClose( hDBF );

    SHPClose( hSHP );

    if( poFilterGeom != NULL )
        delete poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRShapeLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRShapeLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRShapeLayer::GetNextFeature( long * pnFeatureId )

{
    OGRFeature	*poFeature;

    while( TRUE )
    {
        if( iNextShapeId >= nTotalShapeCount )
        {
            return NULL;
        }
    
        if( pnFeatureId != NULL )
            *pnFeatureId = iNextShapeId;

        poFeature = SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn,
                                       iNextShapeId++ );

        if( poFilterGeom == NULL
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
            return poFeature;

        delete poFeature;
    }        
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRShapeLayer::GetFeatureCount( int bForce )

{
    if( poFilterGeom != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return nTotalShapeCount;
}


/************************************************************************/
/* ==================================================================== */
/*			     OGRShapeDataSource			        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::OGRShapeDataSource( const char * pszNameIn,
                                        OGRShapeLayer *poLayerIn )

{
    pszName = CPLStrdup( pszNameIn );
    poLayer = poLayerIn;
}

/************************************************************************/
/*                        ~OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::~OGRShapeDataSource()

{
    CPLFree( pszName );
    delete poLayer;
}

/************************************************************************/
/* ==================================================================== */
/*			     OGRShapeDriver			        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          ~OGRShapeDriver()                           */
/************************************************************************/

OGRShapeDriver::~OGRShapeDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRShapeDriver::GetName()

{
    return "ESRI Shapefile";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRShapeDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    SHPHandle	hSHP;
    DBFHandle	hDBF;

/* -------------------------------------------------------------------- */
/*      SHPOpen() should include better (CPL based) error reporting,    */
/*      and we should be trying to distinquish at this point whether    */
/*      failure is a result of trying to open a non-shapefile, or       */
/*      whether it was a shapefile and we want to report the error up.  */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        hSHP = SHPOpen( pszFilename, "r+" );
    else
        hSHP = SHPOpen( pszFilename, "r" );

    if( hSHP == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Open the .dbf file, if it exists.                               */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        hDBF = DBFOpen( pszFilename, "r+" );
    else
        hDBF = DBFOpen( pszFilename, "r" );

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer	*poLayer;

    poLayer = new OGRShapeLayer( hSHP, hDBF );
    
    return new OGRShapeDataSource( pszFilename, poLayer );
}

/************************************************************************/
/*                          RegisterOGRShape()                          */
/************************************************************************/

void RegisterOGRShape()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRShapeDriver );
}
