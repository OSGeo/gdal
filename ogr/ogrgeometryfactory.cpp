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
 * Revision 1.5  1999/05/31 11:05:08  warmerda
 * added some documentation
 *
 * Revision 1.4  1999/05/23 05:34:40  warmerda
 * added support for clone(), multipolygons and geometry collections
 *
 * Revision 1.3  1999/05/20 14:35:44  warmerda
 * added support for well known text format
 *
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
/************************************************************************/

/**
 * Create a geometry object of the appropriate type from it's well known
 * binary representation.
 *
 * Note that if nBytes is passed as zero, no checking can be done on whether
 * the pabyData is sufficient.  This can result in a crash if the input
 * data is corrupt.  This function returns no indication of the number of
 * bytes from the data source actually used to represent the returned
 * geometry object.  Use OGRGeometry::WkbSize() on the returned geometry to
 * establish the number of bytes it required in WKB format.
 *
 * Also note that this is a static method, and that there
 * is no need to instantiate an OGRGeometryFactory object.  
 *
 * @param pabyData pointer to the input BLOB data.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL in case
 *                  of failure.
 * @param nBytes the number of bytes available in pabyData, or zero if it isn't
 *               known.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromWkb(unsigned char *pabyData,
                                         OGRSpatialReference * poSR,
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
            poPoint->assignSpatialReference( poSR );
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
            poLS->assignSpatialReference( poSR );
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
            poPG->assignSpatialReference( poSR );
            *ppoReturn = poPG;
        }
        else
        {
            delete poPG;
        }
        return eErr;
        break;

      case wkbGeometryCollection:
        OGRGeometryCollection	*poC;

        poC = new OGRGeometryCollection();
        eErr = poC->importFromWkb( pabyData, nBytes );
        if( eErr == OGRERR_NONE )
        {
            poC->assignSpatialReference( poSR );
            *ppoReturn = poC;
        }
        else
        {
            delete poC;
        }
        return eErr;
        break;

      case wkbMultiPolygon:
        OGRMultiPolygon	*poMP;

        poMP = new OGRMultiPolygon();
        eErr = poMP->importFromWkb( pabyData, nBytes );
        if( eErr == OGRERR_NONE )
        {
            poMP->assignSpatialReference( poSR );
            *ppoReturn = poMP;
        }
        else
        {
            delete poMP;
        }
        return eErr;
        break;

      default:
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }
}

/************************************************************************/
/*                           createFromWkt()                            */
/************************************************************************/

/**
 * Create a geometry object of the appropriate type from it's well known
 * text representation.
 *
 * There is no way of establishing how much text was consumed to create the
 * geometry.  If the object is converted back to text representation, the
 * result may be of a different size due to differences in numerical
 * precision and white space.  The OGRGeometry::importFromWkt() method
 * (used by this method) does return information on text consumed but requires
 * that the object type already have been established.
 *
 * @param pszData input zero terminated string containing well known text
 *                representation of the geometry to be created.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL if the
 *                  method fails. 
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromWkt(const char *pszData,
                                         OGRSpatialReference * poSR,
                                         OGRGeometry **ppoReturn )

{
    OGRErr	eErr;
    char	szToken[OGR_WKT_TOKEN_MAX];
    char	*pszInput = (char *) pszData;

    *ppoReturn = NULL;

/* -------------------------------------------------------------------- */
/*      Get the first token, which should be the geometry type.         */
/* -------------------------------------------------------------------- */
    if( OGRWktReadToken( pszData, szToken ) == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry of the appropriate type, and             */
/*      initialize from the input stream.                               */
/* -------------------------------------------------------------------- */
    if( EQUAL(szToken,"POINT") )
    {
        OGRPoint	*poPoint;

        poPoint = new OGRPoint();
        eErr = poPoint->importFromWkt( &pszInput );
        if( eErr == OGRERR_NONE )
        {
            poPoint->assignSpatialReference( poSR );
            *ppoReturn = poPoint;
        }
        else
        {
            delete poPoint;
        }
        return eErr;
    }

    else if( EQUAL(szToken,"LINESTRING") )
    {
        OGRLineString	*poLS;

        poLS = new OGRLineString();
        eErr = poLS->importFromWkt( &pszInput );
        if( eErr == OGRERR_NONE )
        {
            poLS->assignSpatialReference( poSR );
            *ppoReturn = poLS;
        }
        else
        {
            delete poLS;
        }
        return eErr;
    }

    else if( EQUAL(szToken,"POLYGON") )
    {
        OGRPolygon	*poPG;

        poPG = new OGRPolygon();
        eErr = poPG->importFromWkt( &pszInput );
        if( eErr == OGRERR_NONE )
        {
            poPG->assignSpatialReference( poSR );
            *ppoReturn = poPG;
        }
        else
        {
            delete poPG;
        }
        return eErr;
    }
    
    else if( EQUAL(szToken,"GEOMETRYCOLLECTION") )
    {
        OGRGeometryCollection	*poPG;

        poPG = new OGRGeometryCollection();
        eErr = poPG->importFromWkt( &pszInput );
        if( eErr == OGRERR_NONE )
        {
            poPG->assignSpatialReference( poSR );
            *ppoReturn = poPG;
        }
        else
        {
            delete poPG;
        }
        return eErr;
    }
    
    else if( EQUAL(szToken,"MULTIPOLYGON") )
    {
        OGRMultiPolygon	*poPG;

        poPG = new OGRMultiPolygon();
        eErr = poPG->importFromWkt( &pszInput );
        if( eErr == OGRERR_NONE )
        {
            poPG->assignSpatialReference( poSR );
            *ppoReturn = poPG;
        }
        else
        {
            delete poPG;
        }
        return eErr;
    }
    else
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
}

