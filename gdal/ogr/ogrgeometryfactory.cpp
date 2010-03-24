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
 ****************************************************************************/

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
                  pabyData[7] );
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
        && wkbFlatten(poGeom->getGeometryType()) != wkbMultiPolygon )
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
    poMP->addGeometryDirectly( poGeom );

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
    poMP->addGeometryDirectly( poGeom );

    return poMP;
}

/************************************************************************/
/*                        forceToMultiLinestring()                      */
/************************************************************************/

/**
 * Convert to multilinestring.
 *
 * Tries to force the provided geometry to be a multilinestring.
 *
 * - linestrings are placed in a multilinestring.
 * - geometry collections will be converted to multilinestring if they only 
 * contain linestrings.
 * - polygons will be changed to a collection of linestrings (one per ring).
 *
 * The passed in geometry is
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
/*      promoted to MultiLineString.                                    */
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

/* -------------------------------------------------------------------- */
/*      Turn a linestring into a multilinestring.                       */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        poMP->addGeometryDirectly( poGeom );
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Convert polygons into a multilinestring.                        */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRPolygon *poPoly = (OGRPolygon *) poGeom;
        int iRing;

        for( iRing = 0; iRing < poPoly->getNumInteriorRings()+1; iRing++ )
        {
            OGRLineString *poNewLS, *poLR;

            if( iRing == 0 )
                poLR = poPoly->getExteriorRing();
            else
                poLR = poPoly->getInteriorRing(iRing-1);

            poNewLS = new OGRLineString();
            poNewLS->addSubLineString( poLR );
            poMP->addGeometryDirectly( poNewLS );
        }
        
        delete poPoly;

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Convert multi-polygons into a multilinestring.                  */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRMultiPolygon *poMPoly = (OGRMultiPolygon *) poGeom;
        int iPoly;

        for( iPoly = 0; iPoly < poMPoly->getNumGeometries(); iPoly++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*) poMPoly->getGeometryRef(iPoly);
            int iRing;

            for( iRing = 0; iRing < poPoly->getNumInteriorRings()+1; iRing++ )
            {
                OGRLineString *poNewLS, *poLR;
                
                if( iRing == 0 )
                    poLR = poPoly->getExteriorRing();
                else
                    poLR = poPoly->getInteriorRing(iRing-1);
                
                poNewLS = new OGRLineString();
                poNewLS->addSubLineString( poLR );
                poMP->addGeometryDirectly( poNewLS );
            }
        }
        delete poMPoly;

        return poMP;
    }

    return poGeom;
}

/************************************************************************/
/*                          organizePolygons()                          */
/************************************************************************/

typedef struct _sPolyExtended sPolyExtended;

struct _sPolyExtended
{
    OGRPolygon*     poPolygon;
    OGREnvelope     sEnvelope;
    OGRLinearRing*  poExteriorRing;
    OGRPoint        poAPoint;
    int             nInitialIndex;
    int             bIsTopLevel;
    OGRPolygon*     poEnclosingPolygon;
    double          dfArea;
    int             bIsCW;
};

static int OGRGeometryFactoryCompareArea(const void* p1, const void* p2)
{
    const sPolyExtended* psPoly1 = (const sPolyExtended*) p1;
    const sPolyExtended* psPoly2 = (const sPolyExtended*) p2;
    if (psPoly2->dfArea < psPoly1->dfArea)
        return -1;
    else if (psPoly2->dfArea > psPoly1->dfArea)
        return 1;
    else
        return 0;
}

static int OGRGeometryFactoryCompareByIndex(const void* p1, const void* p2)
{
    const sPolyExtended* psPoly1 = (const sPolyExtended*) p1;
    const sPolyExtended* psPoly2 = (const sPolyExtended*) p2;
    if (psPoly1->nInitialIndex < psPoly2->nInitialIndex)
        return -1;
    else if (psPoly1->nInitialIndex > psPoly2->nInitialIndex)
        return 1;
    else
        return 0;
}

#define N_CRITICAL_PART_NUMBER   100

typedef enum
{
   METHOD_NORMAL,
   METHOD_SKIP,
   METHOD_ONLY_CCW
} OrganizePolygonMethod;

/**
 * Organize polygons based on geometries.
 *
 * Analyse a set of rings (passed as simple polygons), and based on a 
 * geometric analysis convert them into a polygon with inner rings, 
 * or a MultiPolygon if dealing with more than one polygon.
 *
 * All the input geometries must be OGRPolygons with only a valid exterior
 * ring (at least 4 points) and no interior rings. 
 *
 * The passed in geometries become the responsibility of the method, but the
 * papoPolygons "pointer array" remains owned by the caller.
 *
 * For faster computation, a polygon is considered to be inside
 * another one if a single point of its external ring is included into the other one.
 * (unless 'OGR_DEBUG_ORGANIZE_POLYGONS' configuration option is set to TRUE.
 * In that case, a slower algorithm that tests exact topological relationships 
 * is used if GEOS is available.)
 *
 * In cases where a big number of polygons is passed to this function, the default processing
 * may be really slow. You can skip the processing by adding METHOD=SKIP
 * to the option list (the result of the function will be a multi-polygon with all polygons
 * as toplevel polygons) or only make it analyze counterclockwise polygons by adding
 * METHOD=ONLY_CCW to the option list if you can assume that the outline
 * of holes is counterclockwise defined (this is the convention for shapefiles e.g.)
 *
 * If the OGR_ORGANIZE_POLYGONS configuration option is defined, its value will override
 * the value of the METHOD option of papszOptions (usefull to modify the behaviour of the
 * shapefile driver)
 *
 * @param papoPolygons array of geometry pointers - should all be OGRPolygons.
 * Ownership of the geometries is passed, but not of the array itself.
 * @param nPolygonCount number of items in papoPolygons
 * @param pbIsValidGeometry value will be set TRUE if result is valid or 
 * FALSE otherwise. 
 * @param papszOptions a list of strings for passing options
 *
 * @return a single resulting geometry (either OGRPolygon or OGRMultiPolygon).
 */

OGRGeometry* OGRGeometryFactory::organizePolygons( OGRGeometry **papoPolygons,
                                                   int nPolygonCount,
                                                   int *pbIsValidGeometry,
                                                   const char** papszOptions )
{
    int bUseFastVersion;
    int i, j;
    OGRGeometry* geom = NULL;
    OrganizePolygonMethod method = METHOD_NORMAL;

/* -------------------------------------------------------------------- */
/*      Trivial case of a single polygon.                               */
/* -------------------------------------------------------------------- */
    if (nPolygonCount == 1)
    {
        geom = papoPolygons[0];
        papoPolygons[0] = NULL;

        if( pbIsValidGeometry )
            *pbIsValidGeometry = TRUE;

        return geom;
    }

    if (CSLTestBoolean(CPLGetConfigOption("OGR_DEBUG_ORGANIZE_POLYGONS", "NO")))
    {
        /* -------------------------------------------------------------------- */
        /*      A wee bit of a warning.                                         */
        /* -------------------------------------------------------------------- */
        static int firstTime = 1;
        if (!haveGEOS() && firstTime)
        {
            CPLDebug("OGR",
                    "In OGR_DEBUG_ORGANIZE_POLYGONS mode, GDAL should be built with GEOS support enabled in order "
                    "OGRGeometryFactory::organizePolygons to provide reliable results on complex polygons.");
            firstTime = 0;
        }
        bUseFastVersion = !haveGEOS();
    }
    else
    {
        bUseFastVersion = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Setup per polygon envelope and area information.                */
/* -------------------------------------------------------------------- */
    sPolyExtended* asPolyEx = new sPolyExtended[nPolygonCount];

    int go_on = TRUE;
    int bMixedUpGeometries = FALSE;
    int bNonPolygon = FALSE;
    int bFoundCCW = FALSE;

    const char* pszMethodValue = CSLFetchNameValue( (char**)papszOptions, "METHOD" );
    const char* pszMethodValueOption = CPLGetConfigOption("OGR_ORGANIZE_POLYGONS", NULL);
    if (pszMethodValueOption != NULL && pszMethodValueOption[0] != '\0')
        pszMethodValue = pszMethodValueOption;

    if (pszMethodValue != NULL)
    {
        if (EQUAL(pszMethodValue, "SKIP"))
        {
            method = METHOD_SKIP;
            bMixedUpGeometries = TRUE;
        }
        else if (EQUAL(pszMethodValue, "ONLY_CCW"))
        {
            method = METHOD_ONLY_CCW;
        }
        else if (!EQUAL(pszMethodValue, "DEFAULT"))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized value for METHOD option : %s", pszMethodValue);
        }
    }

    int nCountCWPolygon = 0;
    int indexOfCWPolygon = -1;

    for(i=0;i<nPolygonCount;i++)
    {
        asPolyEx[i].nInitialIndex = i;
        asPolyEx[i].poPolygon = (OGRPolygon*)papoPolygons[i];
        papoPolygons[i]->getEnvelope(&asPolyEx[i].sEnvelope);

        if( wkbFlatten(papoPolygons[i]->getGeometryType()) == wkbPolygon
            && ((OGRPolygon *) papoPolygons[i])->getNumInteriorRings() == 0
            && ((OGRPolygon *)papoPolygons[i])->getExteriorRing()->getNumPoints() >= 4)
        {
            asPolyEx[i].dfArea = asPolyEx[i].poPolygon->get_Area();
            asPolyEx[i].poExteriorRing = asPolyEx[i].poPolygon->getExteriorRing();
            asPolyEx[i].poExteriorRing->getPoint(0, &asPolyEx[i].poAPoint);
            asPolyEx[i].bIsCW = asPolyEx[i].poExteriorRing->isClockwise();
            if (asPolyEx[i].bIsCW)
            {
                indexOfCWPolygon = i;
                nCountCWPolygon ++;
            }
            if (!bFoundCCW)
                bFoundCCW = ! (asPolyEx[i].bIsCW);
        }
        else
        {
            if( !bMixedUpGeometries )
            {
                CPLError( 
                    CE_Warning, CPLE_AppDefined, 
                    "organizePolygons() received an unexpected geometry.\n"
                    "Either a polygon with interior rings, or a polygon with less than 4 points,\n"
                    "or a non-Polygon geometry.  Return arguments as a collection." );
                bMixedUpGeometries = TRUE;
            }
            if( wkbFlatten(papoPolygons[i]->getGeometryType()) != wkbPolygon )
                bNonPolygon = TRUE;
        }
    }

    /* If we are in ONLY_CCW mode and that we have found that there is only one outer ring, */
    /* then it is pretty easy : we can assume that all other rings are inside */
    if (method == METHOD_ONLY_CCW && nCountCWPolygon == 1 && bUseFastVersion)
    {
        geom = asPolyEx[indexOfCWPolygon].poPolygon;
        for(i=0; i<nPolygonCount; i++)
        {
            if (i != indexOfCWPolygon)
            {
                ((OGRPolygon*)geom)->addRing(asPolyEx[i].poPolygon->getExteriorRing());
                delete asPolyEx[i].poPolygon;
            }
        }
        delete [] asPolyEx;
        if (pbIsValidGeometry)
            *pbIsValidGeometry = TRUE;
        return geom;
    }

    /* Emits a warning if the number of parts is sufficiently big to anticipate for */
    /* very long computation time, and the user didn't specify an explicit method */
    if (nPolygonCount > N_CRITICAL_PART_NUMBER && method == METHOD_NORMAL && pszMethodValue == NULL)
    {
        static int firstTime = 1;
        if (firstTime)
        {
            if (bFoundCCW)
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                     "organizePolygons() received a polygon with more than %d parts. "
                     "The processing may be really slow.\n"
                     "You can skip the processing by setting METHOD=SKIP, "
                     "or only make it analyze counter-clock wise parts by setting "
                     "METHOD=ONLY_CCW if you can assume that the "
                     "outline of holes is counter-clock wise defined", N_CRITICAL_PART_NUMBER);
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                        "organizePolygons() received a polygon with more than %d parts. "
                        "The processing may be really slow.\n"
                        "You can skip the processing by setting METHOD=SKIP.",
                        N_CRITICAL_PART_NUMBER);
            }
            firstTime = 0;
        }
    }


    /* This a several steps algorithm :
       1) Sort polygons by descending areas
       2) For each polygon of rank i, find its smallest enclosing polygon
          among the polygons of rank [i-1 ... 0]. If there are no such polygon,
          this is a toplevel polygon. Otherwise, depending on if the enclosing
          polygon is toplevel or not, we can decide if we are toplevel or not
       3) Re-sort the polygons to retrieve their inital order (nicer for some applications)
       4) For each non toplevel polygon (= inner ring), add it to its outer ring
       5) Add the toplevel polygons to the multipolygon

       Complexity : O(nPolygonCount^2)
    */

    /* Compute how each polygon relate to the other ones
       To save a bit of computation we always begin the computation by a test 
       on the enveloppe. We also take into account the areas to avoid some 
       useless tests.  (A contains B implies envelop(A) contains envelop(B) 
       and area(A) > area(B)) In practise, we can hope that few full geometry 
       intersection of inclusion test is done:
       * if the polygons are well separated geographically (a set of islands 
       for example), no full geometry intersection or inclusion test is done. 
       (the envelopes don't intersect each other)

       * if the polygons are 'lake inside an island inside a lake inside an 
       area' and that each polygon is much smaller than its enclosing one, 
       their bounding boxes are stricly contained into each oter, and thus, 
       no full geometry intersection or inclusion test is done
    */

    if (!bMixedUpGeometries)
    {
        /* STEP 1 : Sort polygons by descending area */
        qsort(asPolyEx, nPolygonCount, sizeof(sPolyExtended), OGRGeometryFactoryCompareArea);
    }
    papoPolygons = NULL; /* just to use to avoid it afterwards */

/* -------------------------------------------------------------------- */
/*      Compute relationships, if things seem well structured.          */
/* -------------------------------------------------------------------- */

    /* The first (largest) polygon is necessarily top-level */
    asPolyEx[0].bIsTopLevel = TRUE;
    asPolyEx[0].poEnclosingPolygon = NULL;

    int nCountTopLevel = 1;

    /* STEP 2 */
    for(i=1; !bMixedUpGeometries && go_on && i<nPolygonCount; i++)
    {
        if (method == METHOD_ONLY_CCW && asPolyEx[i].bIsCW)
        {
            nCountTopLevel ++;
            asPolyEx[i].bIsTopLevel = TRUE;
            asPolyEx[i].poEnclosingPolygon = NULL;
            continue;
        }

        for(j=i-1; go_on && j>=0;j--)
        {
            int b_i_inside_j = FALSE;

            if (method == METHOD_ONLY_CCW && asPolyEx[j].bIsCW == FALSE)
            {
                /* In that mode, i which is CCW if we reach here can only be */
                /* included in a CW polygon */
                continue;
            }

            if (asPolyEx[j].sEnvelope.Contains(asPolyEx[i].sEnvelope))
            {
                if (bUseFastVersion)
                {
                    /* Note that isPointInRing only test strict inclusion in the ring */
                    if (asPolyEx[j].poExteriorRing->isPointInRing(&asPolyEx[i].poAPoint, FALSE))
                    {
                        b_i_inside_j = TRUE;
                    }
                    else if (asPolyEx[j].poExteriorRing->isPointOnRingBoundary(&asPolyEx[i].poAPoint, FALSE))
                    {
                        /* If the point of i is on the boundary of j, we will iterate over the other points of i */
                        int k, nPoints = asPolyEx[i].poExteriorRing->getNumPoints();
                        for(k=1;k<nPoints;k++)
                        {
                            OGRPoint point;
                            asPolyEx[i].poExteriorRing->getPoint(k, &point);
                            if (asPolyEx[j].poExteriorRing->isPointInRing(&point, FALSE))
                            {
                                /* If then point is strictly included in j, then i is considered inside j */
                                b_i_inside_j = TRUE;
                                break;
                            }
                            else if (asPolyEx[j].poExteriorRing->isPointOnRingBoundary(&point, FALSE))
                            {
                                /* If it is on the boundary of j, iterate again */ 
                            }
                            else
                            {
                                /* If it is outside, then i cannot be inside j */
                                break;
                            }
                        }
                    }
                }
                else if (asPolyEx[j].poPolygon->Contains(asPolyEx[i].poPolygon))
                {
                    b_i_inside_j = TRUE;
                }
            }


            if (b_i_inside_j)
            {
                if (asPolyEx[j].bIsTopLevel)
                {
                    /* We are a lake */
                    asPolyEx[i].bIsTopLevel = FALSE;
                    asPolyEx[i].poEnclosingPolygon = asPolyEx[j].poPolygon;
                }
                else
                {
                    /* We are included in a something not toplevel (a lake), */
                    /* so in OGCSF we are considered as toplevel too */
                    nCountTopLevel ++;
                    asPolyEx[i].bIsTopLevel = TRUE;
                    asPolyEx[i].poEnclosingPolygon = NULL;
                }
                break;
            }
            /* We use Overlaps instead of Intersects to be more 
               tolerant about touching polygons */ 
            else if ( bUseFastVersion || !asPolyEx[i].sEnvelope.Intersects(asPolyEx[j].sEnvelope)
                     || !asPolyEx[i].poPolygon->Overlaps(asPolyEx[j].poPolygon) )
            {

            }
            else
            {
                /* Bad... The polygons are intersecting but no one is
                   contained inside the other one. This is a really broken
                   case. We just make a multipolygon with the whole set of
                   polygons */
                go_on = FALSE;
#ifdef DEBUG
                char* wkt1;
                char* wkt2;
                asPolyEx[i].poPolygon->exportToWkt(&wkt1);
                asPolyEx[j].poPolygon->exportToWkt(&wkt2);
                CPLDebug( "OGR", 
                          "Bad intersection for polygons %d and %d\n"
                          "geom %d: %s\n"
                          "geom %d: %s", 
                          i, j, i, wkt1, j, wkt2 );
                CPLFree(wkt1);
                CPLFree(wkt2);
#endif
            }
        }

        if (j < 0)
        {
            /* We come here because we are not included in anything */
            /* We are toplevel */
            nCountTopLevel ++;
            asPolyEx[i].bIsTopLevel = TRUE;
            asPolyEx[i].poEnclosingPolygon = NULL;
        }
    }

    if (pbIsValidGeometry)
        *pbIsValidGeometry = go_on && !bMixedUpGeometries;

/* -------------------------------------------------------------------- */
/*      Things broke down - just turn everything into a multipolygon.   */
/* -------------------------------------------------------------------- */
    if ( !go_on || bMixedUpGeometries )
    {
        if( bNonPolygon )
            geom = new OGRGeometryCollection();
        else
            geom = new OGRMultiPolygon();

        for( i=0; i < nPolygonCount; i++ )
        {
            ((OGRGeometryCollection*)geom)->
                addGeometryDirectly( asPolyEx[i].poPolygon );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to turn into one or more polygons based on the ring         */
/*      relationships.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        /* STEP 3: Resort in initial order */
        qsort(asPolyEx, nPolygonCount, sizeof(sPolyExtended), OGRGeometryFactoryCompareByIndex);

        /* STEP 4: Add holes as rings of their enclosing polygon */
        for(i=0;i<nPolygonCount;i++)
        {
            if (asPolyEx[i].bIsTopLevel == FALSE)
            {
                asPolyEx[i].poEnclosingPolygon->addRing(
                    asPolyEx[i].poPolygon->getExteriorRing());
                delete asPolyEx[i].poPolygon;
            }
            else if (nCountTopLevel == 1)
            {
                geom = asPolyEx[i].poPolygon;
            }
        }

        /* STEP 5: Add toplevel polygons */
        if (nCountTopLevel > 1)
        {
            for(i=0;i<nPolygonCount;i++)
            {
                if (asPolyEx[i].bIsTopLevel)
                {
                    if (geom == NULL)
                    {
                        geom = new OGRMultiPolygon();
                        ((OGRMultiPolygon*)geom)->addGeometryDirectly(asPolyEx[i].poPolygon);
                    }
                    else
                    {
                        ((OGRMultiPolygon*)geom)->addGeometryDirectly(asPolyEx[i].poPolygon);
                    }
                }
            }
        }
    }

    delete[] asPolyEx;

    return geom;
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

/************************************************************************/
/*                           createFromFgf()                            */
/************************************************************************/

/**
 * Create a geometry object of the appropriate type from it's FGF (FDO 
 * Geometry Format) binary representation.
 *
 * Also note that this is a static method, and that there
 * is no need to instantiate an OGRGeometryFactory object.  
 *
 * The C function OGR_G_CreateFromFgf() is the same as this method.
 *
 * @param pabyData pointer to the input BLOB data.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL in case
 *                  of failure.
 * @param nBytes the number of bytes available in pabyData.
 * @param pnBytesConsumed if not NULL, it will be set to the number of bytes 
 * consumed (at most nBytes).
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromFgf( unsigned char *pabyData,
                                          OGRSpatialReference * poSR,
                                          OGRGeometry **ppoReturn,
                                          int nBytes,
                                          int *pnBytesConsumed )

{
    OGRErr       eErr = OGRERR_NONE;
    OGRGeometry *poGeom = NULL;
    GInt32       nGType, nGDim;
    int          nTupleSize = 0;
    int          iOrdinal = 0;
    
    (void) iOrdinal;

    *ppoReturn = NULL;

    if( nBytes < 4 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Decode the geometry type.                                       */
/* -------------------------------------------------------------------- */
    memcpy( &nGType, pabyData + 0, 4 );
    CPL_LSBPTR32( &nGType );

    if( nGType < 0 || nGType > 13 )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Decode the dimentionality if appropriate.                       */
/* -------------------------------------------------------------------- */
    switch( nGType )
    {
      case 1: // Point
      case 2: // LineString
      case 3: // Polygon
        
        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nGDim, pabyData + 4, 4 );
        CPL_LSBPTR32( &nGDim );
        
        if( nGDim < 0 || nGDim > 3 )
            return OGRERR_CORRUPT_DATA;

        nTupleSize = 2;
        if( nGDim & 0x01 ) // Z
            nTupleSize++;
        if( nGDim & 0x02 ) // M
            nTupleSize++;

        break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      None                                                            */
/* -------------------------------------------------------------------- */
    if( nGType == 0 ) 
    {
        if( pnBytesConsumed )
            *pnBytesConsumed = 4;
    }

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    else if( nGType == 1 )
    {
        double  adfTuple[4];

        if( nBytes < nTupleSize * 8 + 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( adfTuple, pabyData + 8, nTupleSize*8 );
#ifdef CPL_MSB
        for( iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
            CPL_SWAP64PTR( adfTuple + iOrdinal );
#endif
        if( nTupleSize > 2 )
            poGeom = new OGRPoint( adfTuple[0], adfTuple[1], adfTuple[2] );
        else
            poGeom = new OGRPoint( adfTuple[0], adfTuple[1] );

        if( pnBytesConsumed )
            *pnBytesConsumed = 8 + nTupleSize * 8;
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    else if( nGType == 2 )
    {
        double adfTuple[4];
        GInt32 nPointCount;
        int    iPoint;
        OGRLineString *poLS;

        if( nBytes < 12 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nPointCount, pabyData + 8, 4 );
        CPL_LSBPTR32( &nPointCount );
        
        if( nBytes < nTupleSize * 8 * nPointCount + 12 )
            return OGRERR_NOT_ENOUGH_DATA;

        poGeom = poLS = new OGRLineString();
        poLS->setNumPoints( nPointCount );

        for( iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            memcpy( adfTuple, pabyData + 12 + 8*nTupleSize*iPoint, 
                    nTupleSize*8 );
#ifdef CPL_MSB
            for( iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
                CPL_SWAP64PTR( adfTuple + iOrdinal );
#endif
            if( nTupleSize > 2 )
                poLS->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
            else
                poLS->setPoint( iPoint, adfTuple[0], adfTuple[1] );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = 12 + nTupleSize * 8 * nPointCount;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( nGType == 3 )
    {
        double adfTuple[4];
        GInt32 nPointCount;
        GInt32 nRingCount;
        int    iPoint, iRing;
        OGRLinearRing *poLR;
        OGRPolygon *poPoly;
        int    nNextByte;

        if( nBytes < 12 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nRingCount, pabyData + 8, 4 );
        CPL_LSBPTR32( &nRingCount );

        nNextByte = 12;
        
        poGeom = poPoly = new OGRPolygon();

        for( iRing = 0; iRing < nRingCount; iRing++ )
        {
            if( nBytes - nNextByte < 4 )
                return OGRERR_NOT_ENOUGH_DATA;

            memcpy( &nPointCount, pabyData + nNextByte, 4 );
            CPL_LSBPTR32( &nPointCount );

            nNextByte += 4;

            if( nBytes - nNextByte < nTupleSize * 8 * nPointCount )
                return OGRERR_NOT_ENOUGH_DATA;

            poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );
            
            for( iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                memcpy( adfTuple, pabyData + nNextByte, nTupleSize*8 );
                nNextByte += nTupleSize * 8;

#ifdef CPL_MSB
                for( iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
                    CPL_SWAP64PTR( adfTuple + iOrdinal );
#endif
                if( nTupleSize > 2 )
                    poLR->setPoint( iPoint, adfTuple[0], adfTuple[1], adfTuple[2] );
                else
                    poLR->setPoint( iPoint, adfTuple[0], adfTuple[1] );
            }

            poPoly->addRingDirectly( poLR );
        }

        if( pnBytesConsumed )
            *pnBytesConsumed = nNextByte;
    }

/* -------------------------------------------------------------------- */
/*      GeometryCollections of various kinds.                           */
/* -------------------------------------------------------------------- */
    else if( nGType == 4         // MultiPoint
             || nGType == 5      // MultiLineString
             || nGType == 6      // MultiPolygon
             || nGType == 7 )    // MultiGeometry
    {
        OGRGeometryCollection *poGC = NULL;
        GInt32 nGeomCount;
        int iGeom, nBytesUsed;

        if( nGType == 4 )
            poGC = new OGRMultiPoint();
        else if( nGType == 5 )
            poGC = new OGRMultiLineString();
        else if( nGType == 6 )
            poGC = new OGRMultiPolygon();
        else if( nGType == 7 )
            poGC = new OGRGeometryCollection();

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nGeomCount, pabyData + 4, 4 );
        CPL_LSBPTR32( &nGeomCount );

        nBytesUsed = 8;

        for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
        {
            int nThisGeomSize;
            OGRGeometry *poThisGeom = NULL;
         
            eErr = createFromFgf( pabyData + nBytesUsed, poSR, &poThisGeom,
                                  nBytes - nBytesUsed, &nThisGeomSize);
            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }

            nBytesUsed += nThisGeomSize;
            eErr = poGC->addGeometryDirectly( poThisGeom );
            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }
        }

        poGeom = poGC;
        if( pnBytesConsumed )
            *pnBytesConsumed = nBytesUsed;
    }

/* -------------------------------------------------------------------- */
/*      Currently unsupported geometry.                                 */
/*                                                                      */
/*      We need to add 10/11/12/13 curve types in some fashion.         */
/* -------------------------------------------------------------------- */
    else
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        if( poGeom != NULL && poSR )
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
/*                        OGR_G_CreateFromFgf()                         */
/************************************************************************/

OGRErr CPL_DLL OGR_G_CreateFromFgf( unsigned char *pabyData, 
                                    OGRSpatialReferenceH hSRS,
                                    OGRGeometryH *phGeometry, 
                                    int nBytes, int *pnBytesConsumed )

{
    return OGRGeometryFactory::createFromFgf( pabyData, 
                                              (OGRSpatialReference *) hSRS,
                                              (OGRGeometry **) phGeometry,
                                              nBytes, pnBytesConsumed );
}

