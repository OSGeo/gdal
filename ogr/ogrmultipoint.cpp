/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPoint class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.3  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/06/25 20:44:43  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.1  1999/05/31 20:42:05  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiPoint::getGeometryType()

{
    return wkbMultiPoint;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPoint::getGeometryName()

{
    return "MULTIPOINT";
}

/************************************************************************/
/*                            addGeometry()                             */
/*                                                                      */
/*      Add a new geometry to a collection.  Subclasses should          */
/*      override this to verify the type of the new geometry, and       */
/*      then call this method to actually add it.                       */
/************************************************************************/

OGRErr OGRMultiPoint::addGeometry( OGRGeometry * poNewGeom )

{
    if( poNewGeom->getGeometryType() != wkbPoint )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    return OGRGeometryCollection::addGeometry( poNewGeom );
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRMultiPoint::clone()

{
    OGRMultiPoint       *poNewGC;

    poNewGC = new OGRMultiPoint;
    poNewGC->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < getNumGeometries(); i++ )
    {
        poNewGC->addGeometry( getGeometryRef(i) );
    }

    return poNewGC;
}
