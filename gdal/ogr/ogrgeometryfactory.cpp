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

    if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
        return poGeom;

    OGRMultiLineString *poMP = new OGRMultiLineString();
    poMP->addGeometryDirectly( poGeom );

    return poMP;
}

/************************************************************************/
/*                          organizePolygons()                          */
/************************************************************************/

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
 * @param papoPolygons array of geometry pointers - should all be OGRPolygons.
 * Ownership of the geometries is passed, but not of the array itself.
 * @param nPolygonCount number of items in papoPolygons
 * @param pbIsValidGeometry value will be set TRUE if result is valid or 
 * FALSE otherwise. 
 *
 * @return a single resulting geometry (either OGRPolygon or OGRMultiPolygon).
 */

enum
{
    CONTAINS,
    IS_CONTAINED_BY,
    NOT_RELATED
};

OGRGeometry* OGRGeometryFactory::organizePolygons( OGRGeometry **papoPolygons,
                                                   int nPolygonCount,
                                                   int *pbIsValidGeometry )
{
    int bUseFastVersion;
    int i, j;
    OGRGeometry* geom = NULL;

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
/*      Setup per polygon relation, envelope and area information.      */
/* -------------------------------------------------------------------- */
    OGREnvelope* envelopes = new OGREnvelope[nPolygonCount];
    int** relations = new int*[nPolygonCount];
    double* areas = new double[nPolygonCount];
    int go_on = TRUE;
    int bMixedUpGeometries = FALSE;
    int bNonPolygon = FALSE;

    for(i=0;i<nPolygonCount;i++)
    {
        relations[i] = new int[nPolygonCount];
        papoPolygons[i]->getEnvelope(&envelopes[i]);

        //fprintf(stderr, "[%d] %d points\n", i, ((OGRPolygon *)papoPolygons[i])->getExteriorRing()->getNumPoints());

        if( wkbFlatten(papoPolygons[i]->getGeometryType()) == wkbPolygon
            && ((OGRPolygon *) papoPolygons[i])->getNumInteriorRings() == 0
            && ((OGRPolygon *)papoPolygons[i])->getExteriorRing()->getNumPoints() >= 4)
        {
            areas[i] = ((OGRPolygon *)papoPolygons[i])->get_Area();
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
        
    /* This a several step algorithm :
       1) Compute in a matrix how polygons relate to each other
       (this is the moment for detecting pathological intersections and exiting)
       2) For each polygon, find the smallest enclosing polygon
       3) For each polygon, compute its inclusion depth (0 means toplevel)
       4) For each polygon of odd depth (= inner ring), add it to its outer ring
        
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

/* -------------------------------------------------------------------- */
/*      Compute relationships, if things seem well structured.          */
/* -------------------------------------------------------------------- */
    for(i=0; !bMixedUpGeometries && go_on && i<nPolygonCount; i++)
    {
        for(j=i+1; go_on && j<nPolygonCount;j++)
        {
            OGRPoint pointI, pointJ;
            const OGRLinearRing* exteriorRingI = ((OGRPolygon *)papoPolygons[i])->getExteriorRing();
            const OGRLinearRing* exteriorRingJ = ((OGRPolygon *)papoPolygons[j])->getExteriorRing();
            exteriorRingI->getPoint(0, &pointI);
            exteriorRingJ->getPoint(0, &pointJ);

            if (areas[i] > areas[j] && envelopes[i].Contains(envelopes[j]) &&
                ((bUseFastVersion && exteriorRingI->isPointInRing(&pointJ)) ||
                 (!bUseFastVersion && papoPolygons[i]->Contains(papoPolygons[j]))))
            {
                relations[i][j] = CONTAINS;
                relations[j][i] = IS_CONTAINED_BY;
            }
            else if (areas[j] > areas[i] && envelopes[j].Contains(envelopes[i]) &&
                     ((bUseFastVersion && exteriorRingJ->isPointInRing(&pointI)) ||
                      (!bUseFastVersion && papoPolygons[j]->Contains(papoPolygons[i]))))
            {
                relations[j][i] = CONTAINS;
                relations[i][j] = IS_CONTAINED_BY;
            }
            /* We use Overlaps instead of Intersects to be more 
               tolerant about touching polygons */ 
            else if ( bUseFastVersion || !envelopes[i].Intersects(envelopes[j])
                     || !papoPolygons[i]->Overlaps(papoPolygons[j]) )
            {
                relations[i][j] = NOT_RELATED;
                relations[j][i] = NOT_RELATED;
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
                papoPolygons[i]->exportToWkt(&wkt1);
                papoPolygons[j]->exportToWkt(&wkt2);
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
                addGeometryDirectly( papoPolygons[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to turn into one or more polygons based on the ring         */
/*      relationships.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        int* directContainerIndex = new int[nPolygonCount];

        /* Find the smallest enclosing polygon of each polygon */
        for(i=0;i<nPolygonCount;i++)
        {
            int jSmallestContainer = -1;
            double areaSmallestContainer = 0;
            for(j=0;j<nPolygonCount;j++)
            {
                if (i != j)
                {
                    if (relations[i][j] == IS_CONTAINED_BY)
                    {
                        if (jSmallestContainer < 0 || areas[j] < areaSmallestContainer)
                        {
                            jSmallestContainer = j;
                            areaSmallestContainer = areas[j];
                        }
                    }
                }
            }
            directContainerIndex[i] = jSmallestContainer;
        }

        /* Compute the inclusion depth of each polygon */
        int* containedDepth = new int [nPolygonCount];
        for(i=0;i<nPolygonCount;i++)
        {
            int depth = 0;
            int j = directContainerIndex[i];
            while (j >= 0)
            {
                j = directContainerIndex[j];
                depth++;
            }
            containedDepth[i] = depth;
//          fprintf(stderr, "%d is of depth %d\n", i, depth);
        }

        int nbTopLevelPolygons = 0;
        OGRPolygon** tempPolygons = new OGRPolygon*[nPolygonCount]; 

        /* Create a copy of toplevel polygons */
        for(i=0;i<nPolygonCount;i++)
        {
            if ((containedDepth[i] % 2) == 0)
            {
                nbTopLevelPolygons ++;
                tempPolygons[i] = (OGRPolygon*)papoPolygons[i];
                papoPolygons[i] = NULL;
                if (nbTopLevelPolygons == 1)
                    geom = tempPolygons[i];
            }
        }

        /* Add interior rings to toplevel polygons */
        for(i=0;i<nPolygonCount;i++)
        {
            if ((containedDepth[i] % 2) == 1)
            {
                tempPolygons[directContainerIndex[i]]->addRing(
                    ((OGRPolygon *)papoPolygons[i])->getExteriorRing());
                delete papoPolygons[i];
            }
        }

        if (nbTopLevelPolygons > 1)
        {
            geom = new OGRMultiPolygon();

            /* Add toplevel polygons to the multipolygon */
            for(i=0;i<nPolygonCount;i++)
            {
                if ((containedDepth[i] % 2) == 0)
                {
                    ((OGRMultiPolygon*)geom)->addGeometryDirectly(
                        tempPolygons[i]);
                    tempPolygons[i] = NULL;
                }
            }
        }

        delete[] tempPolygons;
        delete[] directContainerIndex;
        delete[] containedDepth;
    }

    for(i=0;i<nPolygonCount;i++)
    {
        delete[] relations[i];
    }
    delete[] relations;
    delete[] areas;
    delete[] envelopes;

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

