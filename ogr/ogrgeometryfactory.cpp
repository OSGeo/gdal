/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Factory for converting geometry to and from well known binary
 *           format.
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
 * Revision 1.2  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

/************************************************************************/
/*                           createFromWkb()                            */
/*                                                                      */
/*      Convert a block of well known binary data into a geometry       */
/*      object (which may have subobjects).                             */
/************************************************************************/

OGRErr OGRGeometryFactory::createFromWkb(unsigned char *pabyData,
                                         OGRGeometry **ppoReturn,
                                         int nBytes )

{
    OGRwkbGeometryType eGeometryType;
    OGRwkbByteOrder eByteOrder;
    OGRErr	eErr;

    *ppoReturn = NULL;

    if( nBytes < 5 && nBytes != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = (OGRwkbByteOrder) *pabyData;

    assert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
    if( eByteOrder == wkbNDR )
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
    else
        eGeometryType = (OGRwkbGeometryType) pabyData[4];

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry of the appropriate type, and             */
/*      initialize from the input stream.                               */
/* -------------------------------------------------------------------- */
    switch( eGeometryType )
    {
      case wkbPoint:
        OGRPoint	*poPoint;

        poPoint = new OGRPoint();
        eErr = poPoint->importFromWkb( pabyData, nBytes );
        if( eErr == OGRERR_NONE )
        {
            *ppoReturn = poPoint;
        }
        else
        {
            delete poPoint;
        }
        return eErr;
        break;

      case wkbLineString:
        OGRLineString	*poLS;

        poLS = new OGRLineString();
        eErr = poLS->importFromWkb( pabyData, nBytes );
        if( eErr == OGRERR_NONE )
        {
            *ppoReturn = poLS;
        }
        else
        {
            delete poLS;
        }
        return eErr;
        break;

      case wkbPolygon:
        OGRPolygon	*poPG;

        poPG = new OGRPolygon();
        eErr = poPG->importFromWkb( pabyData, nBytes );
        if( eErr == OGRERR_NONE )
        {
            *ppoReturn = poPG;
        }
        else
        {
            delete poPG;
        }
        return eErr;
        break;

      default:
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }
}
