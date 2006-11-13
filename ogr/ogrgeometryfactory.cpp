/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Factory for converting geometry to and from well known binary
 *           format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.33  2006/11/13 17:55:49  fwarmerdam
 * Fixed memory leak in forceToMultiLineString.
 *
 * Revision 1.32  2006/09/05 15:54:54  fwarmerdam
 * Fixed docs on nBytes for createFromWkb().
 *
 * Revision 1.31  2006/08/24 19:32:21  hobu
 * a little example for OGR_G_CreateFromWkt
 *
 * Revision 1.30  2006/07/07 00:05:46  mloskot
 * Removed GEOS C++ API usage from OGR and autotools.
 *
 * Revision 1.29  2006/01/31 02:38:37  fwarmerdam
 * Fixed corrupt data reporting.
 *
 * Revision 1.28  2005/10/21 15:58:34  fwarmerdam
 * use c++ casting for GEOSGeom
 *
 * Revision 1.27  2005/10/20 19:55:29  fwarmerdam
 * added GEOS C API support
 *
 * Revision 1.26  2005/08/05 20:28:55  fwarmerdam
 * pass nbytes in OGR_G_CreateFromWkb()
 *
 * Revision 1.25  2005/04/18 15:42:17  fwarmerdam
 * fix geos exception catching
 *
 * Revision 1.24  2005/02/22 17:17:30  hobu
 * typo in the haveGEOS() method
 *
 * Revision 1.23  2005/02/22 12:48:09  fwarmerdam
 * added OGRGeometryFactory::haveGEOS()
 *
 * Revision 1.22  2004/08/20 21:21:28  warmerda
 * added support for managing a persistent geos::GeometryFactory
 *
 * Revision 1.21  2004/07/10 04:54:01  warmerda
 * added GEOS methods
 *
 * Revision 1.20  2003/06/09 13:48:54  warmerda
 * added DB2 V7.2 byte order hack
 *
 * Revision 1.19  2003/06/04 13:50:54  warmerda
 * applied patch for IBM DB2 V7.2 WKB bug (with byte order flag)
 *
 * Revision 1.18  2003/04/28 15:39:33  warmerda
 * ryan added forceToMultiPolyline and forceToMultiPoint
 *
 * Revision 1.17  2003/03/31 15:55:42  danmo
 * Added C API function docs
 *
 * Revision 1.16  2003/03/06 20:29:27  warmerda
 * added GML import/export entry points
 *
 * Revision 1.15  2003/02/19 02:57:49  warmerda
 * added wkbLinearRing support
 *
 * Revision 1.14  2003/01/14 22:14:04  warmerda
 * added logic to force Geometry collection to multipolygon
 *
 * Revision 1.13  2003/01/08 22:04:11  warmerda
 * added forceToPolygon and forceToMultiPolygon methods
 *
 * Revision 1.12  2002/09/26 18:12:38  warmerda
 * added C support
 *
 * Revision 1.11  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.10  2001/11/01 16:56:08  warmerda
 * added createGeometry and destroyGeometry methods
 *
 * Revision 1.9  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/06/01 14:34:02  warmerda
 * added debugging of corrupt geometry
 *
 * Revision 1.7  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.6  1999/05/31 20:41:27  warmerda
 * Cleaned up functions substantially, by moving shared code to end of case
 * statements.  createFromWkt() now updates pointer to text to indicate how
 * much text was consumed.  Added multipoint, and multilinestring support.
 *
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
#include "ogr_api.h"
#include "ogr_p.h"
#include <assert.h>
#include "ogr_geos.h"

CPL_CVSID("$Id$");

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
 * The C function OGR_G_CreateFromWkb() is the same as this method.
 *
 * @param pabyData pointer to the input BLOB data.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL in case
 *                  of failure.
 * @param nBytes the number of bytes available in pabyData, or -1 if it isn't
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
    OGRErr      eErr;
    OGRGeometry *poGeom;

    *ppoReturn = NULL;

    if( nBytes < 5 && nBytes != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.  The extra tests are to work around    */
/*      bug sin the WKB of DB2 v7.2 as identified by Safe Software.     */
/* -------------------------------------------------------------------- */
    eByteOrder = DB2_V72_FIX_BYTE_ORDER((OGRwkbByteOrder) *pabyData);


    if( eByteOrder != wkbXDR && eByteOrder != wkbNDR )
    {
        CPLDebug( "OGR", 
                  "OGRGeometryFactory::createFromWkb() - got corrupt data.\n"
                  "%02X%02X%02X%02X%02X%02X%02X%02X\n", 
                  pabyData[0],
                  pabyData[1],
                  pabyData[2],
                  pabyData[3],
                  pabyData[4],
                  pabyData[5],
                  pabyData[6],
                  pabyData[7],
                  pabyData[8] );
        return OGRERR_CORRUPT_DATA;
    }

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
    poGeom = createGeometry( eGeometryType );
    
    if( poGeom == NULL )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Import from binary.                                             */
/* -------------------------------------------------------------------- */
    eErr = poGeom->importFromWkb( pabyData, nBytes );

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        poGeom->assignSpatialReference( poSR );
        *ppoReturn = poGeom;
    }
    else
    {
        delete poGeom;
    }

    return eErr;
}

/************************************************************************/
/*                        OGR_G_CreateFromWkb()                         */
/************************************************************************/
/**
 * Create a geometry object of the appropriate type from it's well known
 * binary representation.
 *
 * Note that if nBytes is passed as zero, no checking can be done on whether
 * the pabyData is sufficient.  This can result in a crash if the input
 * data is corrupt.  This function returns no indication of the number of
 * bytes from the data source actually used to represent the returned
 * geometry object.  Use OGR_G_WkbSize() on the returned geometry to
 * establish the number of bytes it required in WKB format.
 *
 * The OGRGeometryFactory::createFromWkb() CPP method  is the same as this
 * function.
 *
 * @param pabyData pointer to the input BLOB data.
 * @param hSRS handle to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param phGeometry the newly created geometry object will 
 * be assigned to the indicated handle on return.  This will be NULL in case
 * of failure.
 * @param nBytes the number of bytes of data available in pabyData, or -1
 * if it is not known, but assumed to be sufficient.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr CPL_DLL OGR_G_CreateFromWkb( unsigned char *pabyData, 
                                    OGRSpatialReferenceH hSRS,
                                    OGRGeometryH *phGeometry, 
                                    int nBytes )

{
    return OGRGeometryFactory::createFromWkb( pabyData, 
                                              (OGRSpatialReference *) hSRS,
                                              (OGRGeometry **) phGeometry,
                                              nBytes );
}

/************************************************************************/
/*                           createFromWkt()                            */
/************************************************************************/

/**
 * Create a geometry object of the appropriate type from it's well known
 * text representation.
 *
 * The C function OGR_G_CreateFromWkt() is the same as this method.
 *
 * @param ppszData input zero terminated string containing well known text
 *                representation of the geometry to be created.  The pointer
 *                is updated to point just beyond that last character consumed.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL if the
 *                  method fails. 
 *
 *  <b>Example:</b>
 *
 *  <pre>
 *    const char* wkt= "POINT(0 0)";
 *  
 *    // cast because OGR_G_CreateFromWkt will move the pointer 
 *    char* pszWkt = (char*) wkt.c_str(); 
 *    OGRSpatialReferenceH ref = OSRNewSpatialReference(NULL);
 *    OGRGeometryH new_geom;
 *    OGRErr err = OGR_G_CreateFromWkt(&pszWkt, ref, &new_geom);
 *  </pre>
 *
 *
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromWkt(char **ppszData,
                                         OGRSpatialReference * poSR,
                                         OGRGeometry **ppoReturn )

{
    OGRErr      eErr;
    char        szToken[OGR_WKT_TOKEN_MAX];
    char        *pszInput = *ppszData;
    OGRGeometry *poGeom;

    *ppoReturn = NULL;

/* -------------------------------------------------------------------- */
/*      Get the first token, which should be the geometry type.         */
/* -------------------------------------------------------------------- */
    if( OGRWktReadToken( pszInput, szToken ) == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry of the appropriate type.                 */
/* -------------------------------------------------------------------- */
    if( EQUAL(szToken,"POINT") )
    {
        poGeom = new OGRPoint();
    }

    else if( EQUAL(szToken,"LINESTRING") )
    {
        poGeom = new OGRLineString();
    }

    else if( EQUAL(szToken,"POLYGON") )
    {
        poGeom = new OGRPolygon();
    }
    
    else if( EQUAL(szToken,"GEOMETRYCOLLECTION") )
    {
        poGeom = new OGRGeometryCollection();
    }
    
    else if( EQUAL(szToken,"MULTIPOLYGON") )
    {
        poGeom = new OGRMultiPolygon();
    }

    else if( EQUAL(szToken,"MULTIPOINT") )
    {
        poGeom = new OGRMultiPoint();
    }

    else if( EQUAL(szToken,"MULTILINESTRING") )
    {
        poGeom = new OGRMultiLineString();
    }

    else
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

/* -------------------------------------------------------------------- */
/*      Do the import.                                                  */
/* -------------------------------------------------------------------- */
    eErr = poGeom->importFromWkt( &pszInput );
    
/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        poGeom->assignSpatialReference( poSR );
        *ppoReturn = poGeom;
        *ppszData = pszInput;
    }
    else
    {
        delete poGeom;
    }
    
    return eErr;
}

/************************************************************************/
/*                        OGR_G_CreateFromWkt()                         */
/************************************************************************/
/**
 * Create a geometry object of the appropriate type from it's well known
 * text representation.
 *
 * The OGRGeometryFactory::createFromWkt CPP method is the same as this
 * function.
 *
 * @param ppszData input zero terminated string containing well known text
 *                representation of the geometry to be created.  The pointer
 *                is updated to point just beyond that last character consumed.
 * @param hSRS handle to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param phGeometry the newly created geometry object will be assigned to the
 *                  indicated handle on return.  This will be NULL if the
 *                  method fails. 
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr CPL_DLL OGR_G_CreateFromWkt( char **ppszData, 
                                    OGRSpatialReferenceH hSRS,
                                    OGRGeometryH *phGeometry )

{
    return OGRGeometryFactory::createFromWkt( ppszData,
                                              (OGRSpatialReference *) hSRS,
                                              (OGRGeometry **) phGeometry );
}

/************************************************************************/
/*                           createGeometry()                           */
/************************************************************************/

/** 
 * Create an empty geometry of desired type.
 *
 * This is equivelent to allocating the desired geometry with new, but
 * the allocation is guaranteed to take place in the context of the 
 * GDAL/OGR heap. 
 *
 * This method is the same as the C function OGR_G_CreateGeometry().
 *
 * @param eGeometryType the type code of the geometry class to be instantiated.
 *
 * @return the newly create geometry or NULL on failure.
 */

OGRGeometry *
OGRGeometryFactory::createGeometry( OGRwkbGeometryType eGeometryType )

{
    switch( wkbFlatten(eGeometryType) )
    {
      case wkbPoint:
          return new OGRPoint();

      case wkbLineString:
          return new OGRLineString();

      case wkbPolygon:
          return new OGRPolygon();

      case wkbGeometryCollection:
          return new OGRGeometryCollection();

      case wkbMultiPolygon:
          return new OGRMultiPolygon();

      case wkbMultiPoint:
          return new OGRMultiPoint();

      case wkbMultiLineString:
          return new OGRMultiLineString();

      case wkbLinearRing:
          return new OGRLinearRing();

      default:
          return NULL;
    }

    return NULL;
}

/************************************************************************/
/*                        OGR_G_CreateGeometry()                        */
/************************************************************************/
/** 
 * Create an empty geometry of desired type.
 *
 * This is equivelent to allocating the desired geometry with new, but
 * the allocation is guaranteed to take place in the context of the 
 * GDAL/OGR heap. 
 *
 * This function is the same as the CPP method 
 * OGRGeometryFactory::createGeometry.
 *
 * @param eGeometryType the type code of the geometry to be created.
 *
 * @return handle to the newly create geometry or NULL on failure.
 */

OGRGeometryH OGR_G_CreateGeometry( OGRwkbGeometryType eGeometryType )

{
    return (OGRGeometryH) OGRGeometryFactory::createGeometry( eGeometryType );
}


/************************************************************************/
/*                          destroyGeometry()                           */
/************************************************************************/

/**
 * Destroy geometry object.
 *
 * Equivalent to invoking delete on a geometry, but it guaranteed to take 
 * place within the context of the GDAL/OGR heap.
 *
 * This method is the same as the C function OGR_G_DestroyGeometry().
 *
 * @param poGeom the geometry to deallocate.
 */

void OGRGeometryFactory::destroyGeometry( OGRGeometry *poGeom )

{
    delete poGeom;
}


/************************************************************************/
/*                        OGR_G_DestroyGeometry()                       */
/************************************************************************/
/**
 * Destroy geometry object.
 *
 * Equivalent to invoking delete on a geometry, but it guaranteed to take 
 * place within the context of the GDAL/OGR heap.
 *
 * This function is the same as the CPP method 
 * OGRGeometryFactory::destroyGeometry.
 *
 * @param hGeom handle to the geometry to delete.
 */

void OGR_G_DestroyGeometry( OGRGeometryH hGeom )

{
    OGRGeometryFactory::destroyGeometry( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                           forceToPolygon()                           */
/************************************************************************/

/**
 * Convert to polygon.
 *
 * Tries to force the provided geometry to be a polygon.  Currently
 * this just effects a change on multipolygons.  The passed in geometry is
 * consumed and a new one returned (or potentially the same one). 
 * 
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToPolygon( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

    if( wkbFlatten(poGeom->getGeometryType()) != wkbGeometryCollection
        || wkbFlatten(poGeom->getGeometryType()) != wkbMultiPolygon )
        return poGeom;

    // build an aggregated polygon from all the polygon rings in the container.
    OGRPolygon *poPolygon = new OGRPolygon();
    OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
    int iGeom;

    for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
    {
        if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType()) 
            != wkbPolygon )
            continue;

        OGRPolygon *poOldPoly = (OGRPolygon *) poGC->getGeometryRef(iGeom);
        int   iRing;

        poPolygon->addRing( poOldPoly->getExteriorRing() );

        for( iRing = 0; iRing < poOldPoly->getNumInteriorRings(); iRing++ )
            poPolygon->addRing( poOldPoly->getInteriorRing( iRing ) );
    }
    
    delete poGC;

    return poPolygon;
}

/************************************************************************/
/*                        forceToMultiPolygon()                         */
/************************************************************************/

/**
 * Convert to multipolygon.
 *
 * Tries to force the provided geometry to be a multipolygon.  Currently
 * this just effects a change on polygons.  The passed in geometry is
 * consumed and a new one returned (or potentially the same one). 
 * 
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToMultiPolygon( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPolygon.                                       */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection )
    {
        int iGeom;
        int bAllPoly = TRUE;
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType())
                != wkbPolygon )
                bAllPoly = FALSE;
        }

        if( !bAllPoly )
            return poGeom;
        
        OGRMultiPolygon *poMP = new OGRMultiPolygon();

        while( poGC->getNumGeometries() > 0 )
        {
            poMP->addGeometryDirectly( poGC->getGeometryRef(0) );
            poGC->removeGeometry( 0, FALSE );
        }

        delete poGC;

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Eventually we should try to split the polygon into component    */
/*      island polygons.  But thats alot of work and can be put off.    */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) != wkbPolygon )
        return poGeom;

    OGRMultiPolygon *poMP = new OGRMultiPolygon();
    poMP->addGeometry( poGeom );

    return poMP;
}

/************************************************************************/
/*                        forceToMultiPoint()                           */
/************************************************************************/

/**
 * Convert to multipoint.
 *
 * Tries to force the provided geometry to be a multipoint.  Currently
 * this just effects a change on points.  The passed in geometry is
 * consumed and a new one returned (or potentially the same one). 
 * 
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToMultiPoint( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPoint.                                         */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection )
    {
        int iGeom;
        int bAllPoint = TRUE;
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType())
                != wkbPoint )
                bAllPoint = FALSE;
        }

        if( !bAllPoint )
            return poGeom;
        
        OGRMultiPoint *poMP = new OGRMultiPoint();

        while( poGC->getNumGeometries() > 0 )
        {
            poMP->addGeometryDirectly( poGC->getGeometryRef(0) );
            poGC->removeGeometry( 0, FALSE );
        }

        delete poGC;

        return poMP;
    }

    if( wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
        return poGeom;

    OGRMultiPoint *poMP = new OGRMultiPoint();
    poMP->addGeometry( poGeom );

    return poMP;
}

/************************************************************************/
/*                        forceToMultiLinestring()                      */
/************************************************************************/

/**
 * Convert to multilinestring.
 *
 * Tries to force the provided geometry to be a multilinestring.  Currently
 * this just effects a change on linestrings.  The passed in geometry is
 * consumed and a new one returned (or potentially the same one). 
 * 
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToMultiLineString( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPoint.                                         */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection )
    {
        int iGeom;
        int bAllLines = TRUE;
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType())
                != wkbLineString )
                bAllLines = FALSE;
        }

        if( !bAllLines )
            return poGeom;
        
        OGRMultiLineString *poMP = new OGRMultiLineString();

        while( poGC->getNumGeometries() > 0 )
        {
            poMP->addGeometryDirectly( poGC->getGeometryRef(0) );
            poGC->removeGeometry( 0, FALSE );
        }

        delete poGC;

        return poMP;
    }

    if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
        return poGeom;

    OGRMultiLineString *poMP = new OGRMultiLineString();
    poMP->addGeometryDirectly( poGeom );

    return poMP;
}

/************************************************************************/
/*                           createFromGML()                            */
/************************************************************************/

/**
 * Create geometry from GML.
 *
 * This method translates a fragment of GML containing only the geometry
 * portion into a corresponding OGRGeometry.  There are many limitations
 * on the forms of GML geometries supported by this parser, but they are
 * too numerous to list here. 
 *
 * The C function OGR_G_CreateFromGML() is the same as this method.
 *
 * @param pszData The GML fragment for the geometry.
 *
 * @return a geometry on succes, or NULL on error.  
 */

OGRGeometry *OGRGeometryFactory::createFromGML( const char *pszData )

{
    OGRGeometryH hGeom;

    hGeom = OGR_G_CreateFromGML( pszData );
    
    return (OGRGeometry *) hGeom;
}

/************************************************************************/
/*                           createFromGEOS()                           */
/************************************************************************/

OGRGeometry *
OGRGeometryFactory::createFromGEOS( GEOSGeom geosGeom )

{
#ifndef HAVE_GEOS 

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

#else

    size_t nSize = 0;
    unsigned char *pabyBuf = NULL;
    OGRGeometry *poGeometry = NULL;

    pabyBuf = GEOSGeomToWKB_buf( geosGeom, &nSize );
    if( pabyBuf == NULL || nSize == 0 )
    {
        return NULL;
    }

    if( OGRGeometryFactory::createFromWkb( (unsigned char *) pabyBuf, 
                                           NULL, &poGeometry, (int) nSize )
        != OGRERR_NONE )
    {
        poGeometry = NULL;
    }

    if( pabyBuf != NULL )
    {
        free( pabyBuf );
    }

    return poGeometry;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                       getGEOSGeometryFactory()                       */
/************************************************************************/

void *OGRGeometryFactory::getGEOSGeometryFactory() 

{
    // XXX - mloskot - What to do with this call
    // after GEOS C++ API has been stripped?
    return NULL;
}

/************************************************************************/
/*                              haveGEOS()                              */
/************************************************************************/

/**
 * Test if GEOS enabled.
 *
 * This static method returns TRUE if GEOS support is built into OGR,
 * otherwise it returns FALSE.
 *
 * @return TRUE if available, otherwise FALSE.
 */

int OGRGeometryFactory::haveGEOS()

{
#ifndef HAVE_GEOS 
    return FALSE;
#else
    return TRUE;
#endif
}

