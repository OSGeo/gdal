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
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys dot com>
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
#include "ogr_geos.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           createFromWkb()                            */
/************************************************************************/

/**
 * \brief Create a geometry object of the appropriate type from it's well known binary representation.
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
 *                  of failure. If not NULL, *ppoReturn should be freed with
 *                  OGRGeometryFactory::destroyGeometry() after use.
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
                                         int nBytes,
                                         OGRwkbVariant eWkbVariant )

{
    OGRwkbGeometryType eGeometryType;
    OGRwkbByteOrder eByteOrder;
    OGRErr      eErr;
    OGRGeometry *poGeom;

    *ppoReturn = NULL;

    if( nBytes < 9 && nBytes != -1 )
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
                  "%02X%02X%02X%02X%02X%02X%02X%02X%02X\n", 
                  pabyData[0],
                  pabyData[1],
                  pabyData[2],
                  pabyData[3],
                  pabyData[4],
                  pabyData[5],
                  pabyData[6],
                  pabyData[7],
                  pabyData[8]);
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */

    OGRBoolean bIs3D;
    OGRErr err = OGRReadWKBGeometryType( pabyData, eWkbVariant, &eGeometryType, &bIs3D );

    if( err != OGRERR_NONE )
        return err;


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
    eErr = poGeom->importFromWkb( pabyData, nBytes, eWkbVariant );

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        if ( poGeom->hasCurveGeometry() && 
             CSLTestBoolean(CPLGetConfigOption("OGR_STROKE_CURVE", "FALSE")) ) 
        {
            OGRGeometry* poNewGeom = poGeom->getLinearGeometry();
            delete poGeom; 
            poGeom = poNewGeom; 
        }
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
 * \brief Create a geometry object of the appropriate type from it's well known binary representation.
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
 * of failure. If not NULL, *phGeometry should be freed with
 * OGR_G_DestroyGeometry() after use.
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
 * \brief Create a geometry object of the appropriate type from it's well known text representation.
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
 *                  method fails. If not NULL, *ppoReturn should be freed with
 *                  OGRGeometryFactory::destroyGeometry() after use.
 *
 *  <b>Example:</b>
 *
 *  <pre>
 *    const char* wkt= "POINT(0 0)";
 *  
 *    // cast because OGR_G_CreateFromWkt will move the pointer 
 *    char* pszWkt = (char*) wkt;
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

    else if( EQUAL(szToken,"CIRCULARSTRING") )
    {
        poGeom = new OGRCircularString();
    }

    else if( EQUAL(szToken,"COMPOUNDCURVE") )
    {
        poGeom = new OGRCompoundCurve();
    }

    else if( EQUAL(szToken,"CURVEPOLYGON") )
    {
        poGeom = new OGRCurvePolygon();
    }

    else if( EQUAL(szToken,"MULTICURVE") )
    {
        poGeom = new OGRMultiCurve();
    }

    else if( EQUAL(szToken,"MULTISURFACE") )
    {
        poGeom = new OGRMultiSurface();
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
        if ( poGeom->hasCurveGeometry() && 
             CSLTestBoolean(CPLGetConfigOption("OGR_STROKE_CURVE", "FALSE")) ) 
        {
            OGRGeometry* poNewGeom = poGeom->getLinearGeometry();
            delete poGeom; 
            poGeom = poNewGeom; 
        }
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
 * \brief Create a geometry object of the appropriate type from it's well known text representation.
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
 *                  method fails. If not NULL, *phGeometry should be freed with
 *                  OGR_G_DestroyGeometry() after use.
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
 * \brief Create an empty geometry of desired type.
 *
 * This is equivalent to allocating the desired geometry with new, but
 * the allocation is guaranteed to take place in the context of the 
 * GDAL/OGR heap. 
 *
 * This method is the same as the C function OGR_G_CreateGeometry().
 *
 * @param eGeometryType the type code of the geometry class to be instantiated.
 *
 * @return the newly create geometry or NULL on failure. Should be freed with
 *          OGRGeometryFactory::destroyGeometry() after use.
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

      case wkbCircularString:
          return new OGRCircularString();

      case wkbCompoundCurve:
          return new OGRCompoundCurve();

      case wkbCurvePolygon:
          return new OGRCurvePolygon();

      case wkbMultiCurve:
          return new OGRMultiCurve();

      case wkbMultiSurface:
          return new OGRMultiSurface();

      default:
          return NULL;
    }
}

/************************************************************************/
/*                        OGR_G_CreateGeometry()                        */
/************************************************************************/
/** 
 * \brief Create an empty geometry of desired type.
 *
 * This is equivalent to allocating the desired geometry with new, but
 * the allocation is guaranteed to take place in the context of the 
 * GDAL/OGR heap. 
 *
 * This function is the same as the CPP method 
 * OGRGeometryFactory::createGeometry.
 *
 * @param eGeometryType the type code of the geometry to be created.
 *
 * @return handle to the newly create geometry or NULL on failure. Should be freed with
 *         OGR_G_DestroyGeometry() after use.
 */

OGRGeometryH OGR_G_CreateGeometry( OGRwkbGeometryType eGeometryType )

{
    return (OGRGeometryH) OGRGeometryFactory::createGeometry( eGeometryType );
}


/************************************************************************/
/*                          destroyGeometry()                           */
/************************************************************************/

/**
 * \brief Destroy geometry object.
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
 * \brief Destroy geometry object.
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
 * \brief Convert to polygon.
 *
 * Tries to force the provided geometry to be a polygon. This effects a change
 * on multipolygons.
 * Starting with GDAL 2.0, curve polygons or closed curves will be changed to polygons.
 * The passed in geometry is consumed and a new one returned (or potentially the same one). 
 * 
 * @param poGeom the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToPolygon( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

    if( eGeomType == wkbCurvePolygon )
    {
        if( !poGeom->hasCurveGeometry(TRUE) )
            return OGRSurface::CastToPolygon(((OGRCurvePolygon*)poGeom));

        OGRPolygon* poPoly = ((OGRCurvePolygon*)poGeom)->CurvePolyToPoly();
        delete poGeom;
        return poPoly;
    }

    if( OGR_GT_IsCurve(eGeomType) &&
        ((OGRCurve*)poGeom)->getNumPoints() >= 3 &&
        ((OGRCurve*)poGeom)->get_IsClosed() )
    {
        OGRPolygon *poPolygon = new OGRPolygon();
        poPolygon->assignSpatialReference(poGeom->getSpatialReference());

        if( !poGeom->hasCurveGeometry(TRUE) )
        {
            poPolygon->addRingDirectly( OGRCurve::CastToLinearRing((OGRCurve*)poGeom) );
        }
        else
        {
            OGRLineString* poLS = ((OGRCurve*)poGeom)->CurveToLine();
            poPolygon->addRingDirectly( OGRCurve::CastToLinearRing(poLS) );
            delete poGeom;
        }
        return poPolygon;
    }

    if( eGeomType != wkbGeometryCollection
        && eGeomType != wkbMultiPolygon
        && eGeomType != wkbMultiSurface )
        return poGeom;

    // build an aggregated polygon from all the polygon rings in the container.
    OGRPolygon *poPolygon = new OGRPolygon();
    OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
    if( poGeom->hasCurveGeometry() )
    {
        OGRGeometryCollection *poNewGC = (OGRGeometryCollection *) poGC->getLinearGeometry();
        delete poGC;
        poGeom = poGC = poNewGC;
    }

    poPolygon->assignSpatialReference(poGeom->getSpatialReference());
    int iGeom;

    for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
    {
        if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType()) 
            != wkbPolygon )
            continue;

        OGRPolygon *poOldPoly = (OGRPolygon *) poGC->getGeometryRef(iGeom);
        int   iRing;
        
        if( poOldPoly->getExteriorRing() == NULL )
            continue;

        poPolygon->addRingDirectly( poOldPoly->stealExteriorRing() );

        for( iRing = 0; iRing < poOldPoly->getNumInteriorRings(); iRing++ )
            poPolygon->addRingDirectly( poOldPoly->stealInteriorRing( iRing ) );
    }
    
    delete poGC;

    return poPolygon;
}

/************************************************************************/
/*                        OGR_G_ForceToPolygon()                        */
/************************************************************************/

/**
 * \brief Convert to polygon.
 *
 * This function is the same as the C++ method 
 * OGRGeometryFactory::forceToPolygon().
 *
 * @param hGeom handle to the geometry to convert (ownership surrendered).
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL/OGR 1.8.0
 */

OGRGeometryH OGR_G_ForceToPolygon( OGRGeometryH hGeom )

{
    return (OGRGeometryH) 
        OGRGeometryFactory::forceToPolygon( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                        forceToMultiPolygon()                         */
/************************************************************************/

/**
 * \brief Convert to multipolygon.
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

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

/* -------------------------------------------------------------------- */
/*      If this is already a MultiPolygon, nothing to do                */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiPolygon )
    {
        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      If this is already a MultiSurface with compatible content,      */
/*      just cast                                                       */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiSurface &&
        !((OGRMultiSurface*)poGeom)->hasCurveGeometry(TRUE) )
    {
        return OGRMultiSurface::CastToMultiPolygon((OGRMultiSurface*)poGeom);
    }

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPolygon.                                       */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbGeometryCollection ||
        eGeomType == wkbMultiSurface )
    {
        int iGeom;
        int bAllPoly = TRUE;
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
        if( poGeom->hasCurveGeometry() )
        {
            OGRGeometryCollection *poNewGC = (OGRGeometryCollection *) poGC->getLinearGeometry();
            delete poGC;
            poGeom = poGC = poNewGC;
        }

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            OGRwkbGeometryType eSubGeomType = wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
            if( eSubGeomType != wkbPolygon )
                bAllPoly = FALSE;
        }

        if( !bAllPoly )
            return poGeom;
        
        OGRMultiPolygon *poMP = new OGRMultiPolygon();
        poMP->assignSpatialReference(poGeom->getSpatialReference());

        while( poGC->getNumGeometries() > 0 )
        {
            poMP->addGeometryDirectly( poGC->getGeometryRef(0) );
            poGC->removeGeometry( 0, FALSE );
        }

        delete poGC;

        return poMP;
    }

    if( eGeomType == wkbCurvePolygon )
    {
        OGRPolygon* poPoly = ((OGRCurvePolygon*)poGeom)->CurvePolyToPoly();
        OGRMultiPolygon *poMP = new OGRMultiPolygon();
        poMP->assignSpatialReference(poGeom->getSpatialReference());
        poMP->addGeometryDirectly( poPoly );
        delete poGeom;
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Eventually we should try to split the polygon into component    */
/*      island polygons.  But thats alot of work and can be put off.    */
/* -------------------------------------------------------------------- */
    if( eGeomType != wkbPolygon )
        return poGeom;

    OGRMultiPolygon *poMP = new OGRMultiPolygon();
    poMP->assignSpatialReference(poGeom->getSpatialReference());
    poMP->addGeometryDirectly( poGeom );

    return poMP;
}

/************************************************************************/
/*                     OGR_G_ForceToMultiPolygon()                      */
/************************************************************************/

/**
 * \brief Convert to multipolygon.
 *
 * This function is the same as the C++ method 
 * OGRGeometryFactory::forceToMultiPolygon().
 *
 * @param hGeom handle to the geometry to convert (ownership surrendered).
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL/OGR 1.8.0
 */

OGRGeometryH OGR_G_ForceToMultiPolygon( OGRGeometryH hGeom )

{
    return (OGRGeometryH) 
        OGRGeometryFactory::forceToMultiPolygon( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                        forceToMultiPoint()                           */
/************************************************************************/

/**
 * \brief Convert to multipoint.
 *
 * Tries to force the provided geometry to be a multipoint.  Currently
 * this just effects a change on points or collection of points.
 * The passed in geometry is
 * consumed and a new one returned (or potentially the same one). 
 * 
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToMultiPoint( OGRGeometry *poGeom )

{
    if( poGeom == NULL )
        return NULL;

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

/* -------------------------------------------------------------------- */
/*      If this is already a MultiPoint, nothing to do                  */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiPoint )
    {
        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPoint.                                         */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbGeometryCollection )
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
        poMP->assignSpatialReference(poGeom->getSpatialReference());

        while( poGC->getNumGeometries() > 0 )
        {
            poMP->addGeometryDirectly( poGC->getGeometryRef(0) );
            poGC->removeGeometry( 0, FALSE );
        }

        delete poGC;

        return poMP;
    }

    if( eGeomType != wkbPoint )
        return poGeom;

    OGRMultiPoint *poMP = new OGRMultiPoint();
    poMP->assignSpatialReference(poGeom->getSpatialReference());
    poMP->addGeometryDirectly( poGeom );

    return poMP;
}

/************************************************************************/
/*                      OGR_G_ForceToMultiPoint()                       */
/************************************************************************/

/**
 * \brief Convert to multipoint.
 *
 * This function is the same as the C++ method 
 * OGRGeometryFactory::forceToMultiPoint().
 *
 * @param hGeom handle to the geometry to convert (ownership surrendered).
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL/OGR 1.8.0
 */

OGRGeometryH OGR_G_ForceToMultiPoint( OGRGeometryH hGeom )

{
    return (OGRGeometryH) 
        OGRGeometryFactory::forceToMultiPoint( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                        forceToMultiLinestring()                      */
/************************************************************************/

/**
 * \brief Convert to multilinestring.
 *
 * Tries to force the provided geometry to be a multilinestring.
 *
 * - linestrings are placed in a multilinestring.
 * - circularstrings and compoundcurves will be approximated and placed in a
 * multilinestring.
 * - geometry collections will be converted to multilinestring if they only 
 * contain linestrings.
 * - polygons will be changed to a collection of linestrings (one per ring).
 * - curvepolygons will be approximated and changed to a collection of linestrings (one per ring).
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

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

/* -------------------------------------------------------------------- */
/*      If this is already a MultiLineString, nothing to do             */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiLineString )
    {
        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiLineString.                                    */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbGeometryCollection )
    {
        int iGeom;
        int bAllLines = TRUE;
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
        if( poGeom->hasCurveGeometry() )
        {
            OGRGeometryCollection *poNewGC = (OGRGeometryCollection *) poGC->getLinearGeometry();
            delete poGC;
            poGeom = poGC = poNewGC;
        }

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            if( poGC->getGeometryRef(iGeom)->getGeometryType() != wkbLineString )
                bAllLines = FALSE;
        }

        if( !bAllLines )
            return poGeom;
        
        OGRMultiLineString *poMP = new OGRMultiLineString();
        poMP->assignSpatialReference(poGeom->getSpatialReference());

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
    if( eGeomType == wkbLineString )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        poMP->assignSpatialReference(poGeom->getSpatialReference());
        poMP->addGeometryDirectly( poGeom );
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Convert polygons into a multilinestring.                        */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbPolygon || eGeomType == wkbCurvePolygon )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRPolygon *poPoly;
        if( eGeomType == wkbPolygon )
            poPoly = (OGRPolygon *) poGeom;
        else
        {
            poPoly = ((OGRCurvePolygon*) poGeom)->CurvePolyToPoly();
            delete poGeom;
            poGeom = poPoly;
        }
        int iRing;

        poMP->assignSpatialReference(poGeom->getSpatialReference());

        for( iRing = 0; iRing < poPoly->getNumInteriorRings()+1; iRing++ )
        {
            OGRLineString *poNewLS, *poLR;

            if( iRing == 0 )
            {
                poLR = poPoly->getExteriorRing();
                if( poLR == NULL )
                    break;
            }
            else
                poLR = poPoly->getInteriorRing(iRing-1);

            if (poLR == NULL || poLR->getNumPoints() == 0)
                continue;

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
    if( eGeomType == wkbMultiPolygon || eGeomType == wkbMultiSurface )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRMultiPolygon *poMPoly;
        if( eGeomType == wkbMultiPolygon )
            poMPoly = (OGRMultiPolygon *) poGeom;
        else
        {
            poMPoly = (OGRMultiPolygon *) poGeom->getLinearGeometry();
            delete poGeom;
            poGeom = poMPoly;
        }
        int iPoly;

        poMP->assignSpatialReference(poGeom->getSpatialReference());

        for( iPoly = 0; iPoly < poMPoly->getNumGeometries(); iPoly++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*) poMPoly->getGeometryRef(iPoly);
            int iRing;

            for( iRing = 0; iRing < poPoly->getNumInteriorRings()+1; iRing++ )
            {
                OGRLineString *poNewLS, *poLR;
                
                if( iRing == 0 )
                {
                    poLR = poPoly->getExteriorRing();
                    if( poLR == NULL )
                        break;
                }
                else
                    poLR = poPoly->getInteriorRing(iRing-1);
    
                if (poLR == NULL || poLR->getNumPoints() == 0)
                    continue;
                
                poNewLS = new OGRLineString();
                poNewLS->addSubLineString( poLR );
                poMP->addGeometryDirectly( poNewLS );
            }
        }
        delete poMPoly;

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If it's a curve line, approximate it and wrap in a multilinestring */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbCircularString ||
        eGeomType == wkbCompoundCurve )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        poMP->assignSpatialReference(poGeom->getSpatialReference());
        poMP->addGeometryDirectly( ((OGRCurve*)poGeom)->CurveToLine() );
        delete poGeom;
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If this is already a MultiCurve with compatible content,        */
/*      just cast                                                       */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiCurve &&
        !((OGRMultiCurve*)poGeom)->hasCurveGeometry(TRUE) )
    {
        return OGRMultiCurve::CastToMultiLineString((OGRMultiCurve*)poGeom);
    }

/* -------------------------------------------------------------------- */
/*      If it's a multicurve, call getLinearGeometry()                */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiCurve )
    {
        OGRGeometry* poNewGeom = poGeom->getLinearGeometry();
        CPLAssert( wkbFlatten(poNewGeom->getGeometryType()) == wkbMultiLineString );
        delete poGeom;
        return (OGRMultiLineString*) poNewGeom;
    }

    return poGeom;
}

/************************************************************************/
/*                    OGR_G_ForceToMultiLineString()                    */
/************************************************************************/

/**
 * \brief Convert to multilinestring.
 *
 * This function is the same as the C++ method 
 * OGRGeometryFactory::forceToMultiLineString().
 *
 * @param hGeom handle to the geometry to convert (ownership surrendered).
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL/OGR 1.8.0
 */

OGRGeometryH OGR_G_ForceToMultiLineString( OGRGeometryH hGeom )

{
    return (OGRGeometryH) 
        OGRGeometryFactory::forceToMultiLineString( (OGRGeometry *) hGeom );
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
   METHOD_ONLY_CCW,
   METHOD_CCW_INNER_JUST_AFTER_CW_OUTER
} OrganizePolygonMethod;

/**
 * \brief Organize polygons based on geometries.
 *
 * Analyse a set of rings (passed as simple polygons), and based on a 
 * geometric analysis convert them into a polygon with inner rings, 
 * (or a MultiPolygon if dealing with more than one polygon) that follow the
 * OGC Simple Feature specification.
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
 * of holes is counterclockwise defined (this is the convention for example in shapefiles,
 * Personal Geodatabases or File Geodatabases).
 *
 * For FileGDB, in most cases, but not always, a faster method than ONLY_CCW can be used. It is
 * CCW_INNER_JUST_AFTER_CW_OUTER. When using it, inner rings are assumed to be
 * counterclockwise oriented, and following immediately the outer ring (clockwise
 * oriented) that they belong to. If that assumption is not met, an inner ring
 * could be attached to the wrong outer ring, so this method must be used
 * with care.
 *
 * If the OGR_ORGANIZE_POLYGONS configuration option is defined, its value will override
 * the value of the METHOD option of papszOptions (useful to modify the behaviour of the
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
        else if (EQUAL(pszMethodValue, "CCW_INNER_JUST_AFTER_CW_OUTER"))
        {
            method = METHOD_CCW_INNER_JUST_AFTER_CW_OUTER;
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
            if( method != METHOD_CCW_INNER_JUST_AFTER_CW_OUTER )
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
    if ((method == METHOD_ONLY_CCW || method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER) &&
        nCountCWPolygon == 1 && bUseFastVersion && !bNonPolygon )
    {
        geom = asPolyEx[indexOfCWPolygon].poPolygon;
        for(i=0; i<nPolygonCount; i++)
        {
            if (i != indexOfCWPolygon)
            {
                ((OGRPolygon*)geom)->addRingDirectly(asPolyEx[i].poPolygon->stealExteriorRing());
                delete asPolyEx[i].poPolygon;
            }
        }
        delete [] asPolyEx;
        if (pbIsValidGeometry)
            *pbIsValidGeometry = TRUE;
        return geom;
    }

    if( method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER && !bNonPolygon && asPolyEx[0].bIsCW )
    {
        /* Inner rings are CCW oriented and follow immediately the outer */
        /* ring (that is CW oriented) in which they are included */
        OGRMultiPolygon* poMulti = NULL;
        OGRPolygon* poCur = asPolyEx[0].poPolygon;
        OGRGeometry* poRet = poCur;
        /* We have already checked that the first ring is CW */
        OGREnvelope* psEnvelope = &(asPolyEx[0].sEnvelope);
        for(i=1;i<nPolygonCount;i++)
        {
            if( asPolyEx[i].bIsCW )
            {
                if( poMulti == NULL )
                {
                    poMulti = new OGRMultiPolygon();
                    poRet = poMulti;
                    poMulti->addGeometryDirectly(poCur);
                }
                poCur = asPolyEx[i].poPolygon;
                poMulti->addGeometryDirectly(poCur);
                psEnvelope = &(asPolyEx[i].sEnvelope);
            }
            else
            {
                poCur->addRingDirectly(asPolyEx[i].poPolygon->stealExteriorRing());
                if(!(asPolyEx[i].poAPoint.getX() >= psEnvelope->MinX &&
                     asPolyEx[i].poAPoint.getX() <= psEnvelope->MaxX &&
                     asPolyEx[i].poAPoint.getY() >= psEnvelope->MinY &&
                     asPolyEx[i].poAPoint.getY() <= psEnvelope->MaxY))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Part %d does not respect CCW_INNER_JUST_AFTER_CW_OUTER rule", i);
                }
                delete asPolyEx[i].poPolygon;
            }
        }
        delete [] asPolyEx;
        if (pbIsValidGeometry)
            *pbIsValidGeometry = TRUE;
        return poRet;
    }
    else if( method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER && !bNonPolygon )
    {
        method = METHOD_ONLY_CCW;
        for(i=0;i<nPolygonCount;i++)
            asPolyEx[i].dfArea = asPolyEx[i].poPolygon->get_Area();
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
                    if( method == METHOD_ONLY_CCW && j == 0 )
                    {
                        /* We are testing if a CCW ring is in the biggest CW ring */
                        /* It *must* be inside as this is the last candidate, otherwise */
                        /* the winding order rules is broken */
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
                            if (asPolyEx[j].poExteriorRing->isPointOnRingBoundary(&point, FALSE))
                            {
                                /* If it is on the boundary of j, iterate again */ 
                            }
                            else if (asPolyEx[j].poExteriorRing->isPointInRing(&point, FALSE))
                            {
                                /* If then point is strictly included in j, then i is considered inside j */
                                b_i_inside_j = TRUE;
                                break;
                            }
                            else 
                            {
                                /* If it is outside, then i cannot be inside j */
                                break;
                            }
                        }
                        if( !b_i_inside_j && k == nPoints && nPoints > 2 )
                        {
                            /* all points of i are on the boundary of j ... */
                            /* take a point in the middle of a segment of i and */
                            /* test it against j */
                            for(k=0;k<nPoints-1;k++)
                            {
                                OGRPoint point1, point2, pointMiddle;
                                asPolyEx[i].poExteriorRing->getPoint(k, &point1);
                                asPolyEx[i].poExteriorRing->getPoint(k+1, &point2);
                                pointMiddle.setX((point1.getX() + point2.getX()) / 2);
                                pointMiddle.setY((point1.getY() + point2.getY()) / 2);
                                if (asPolyEx[j].poExteriorRing->isPointOnRingBoundary(&pointMiddle, FALSE))
                                {
                                    /* If it is on the boundary of j, iterate again */ 
                                }
                                else if (asPolyEx[j].poExteriorRing->isPointInRing(&pointMiddle, FALSE))
                                {
                                    /* If then point is strictly included in j, then i is considered inside j */
                                    b_i_inside_j = TRUE;
                                    break;
                                }
                                else 
                                {
                                    /* If it is outside, then i cannot be inside j */
                                    break;
                                }
                            }
                        }
                    }
                    /* Note that isPointInRing only test strict inclusion in the ring */
                    else if (asPolyEx[j].poExteriorRing->isPointInRing(&asPolyEx[i].poAPoint, FALSE))
                    {
                        b_i_inside_j = TRUE;
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
                asPolyEx[i].poEnclosingPolygon->addRingDirectly(
                    asPolyEx[i].poPolygon->stealExteriorRing());
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
 * \brief Create geometry from GML.
 *
 * This method translates a fragment of GML containing only the geometry
 * portion into a corresponding OGRGeometry.  There are many limitations
 * on the forms of GML geometries supported by this parser, but they are
 * too numerous to list here. 
 *
 * The following GML2 elements are parsed : Point, LineString, Polygon,
 * MultiPoint, MultiLineString, MultiPolygon, MultiGeometry.
 *
 * (OGR >= 1.8.0) The following GML3 elements are parsed : Surface, MultiSurface,
 * PolygonPatch, Triangle, Rectangle, Curve, MultiCurve, LineStringSegment, Arc,
 * Circle, CompositeSurface, OrientableSurface, Solid, Tin, TriangulatedSurface.
 *
 * Arc and Circle elements are stroked to linestring, by using a
 * 4 degrees step, unless the user has overridden the value with the
 * OGR_ARC_STEPSIZE configuration variable.
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
OGRGeometryFactory::createFromGEOS( GEOSContextHandle_t hGEOSCtxt, GEOSGeom geosGeom )

{
#ifndef HAVE_GEOS 

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

#else

    size_t nSize = 0;
    unsigned char *pabyBuf = NULL;
    OGRGeometry *poGeometry = NULL;

    /* Special case as POINT EMPTY cannot be translated to WKB */
    if (GEOSGeomTypeId_r(hGEOSCtxt, geosGeom) == GEOS_POINT &&
        GEOSisEmpty_r(hGEOSCtxt, geosGeom))
        return new OGRPoint();

#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 3)
    /* GEOSGeom_getCoordinateDimension only available in GEOS 3.3.0 (unreleased at time of writing) */
    int nCoordDim = GEOSGeom_getCoordinateDimension_r(hGEOSCtxt, geosGeom);
    GEOSWKBWriter* wkbwriter = GEOSWKBWriter_create_r(hGEOSCtxt);
    GEOSWKBWriter_setOutputDimension_r(hGEOSCtxt, wkbwriter, nCoordDim);
    pabyBuf = GEOSWKBWriter_write_r(hGEOSCtxt, wkbwriter, geosGeom, &nSize );
    GEOSWKBWriter_destroy_r(hGEOSCtxt, wkbwriter);
#else
    pabyBuf = GEOSGeomToWKB_buf_r( hGEOSCtxt, geosGeom, &nSize );
#endif
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
        /* Since GEOS 3.1.1, so we test 3.2.0 */
#if GEOS_CAPI_VERSION_MAJOR >= 2 || (GEOS_CAPI_VERSION_MAJOR == 1 && GEOS_CAPI_VERSION_MINOR >= 6)
        GEOSFree_r( hGEOSCtxt, pabyBuf );
#else
        free( pabyBuf );
#endif
    }

    return poGeometry;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                              haveGEOS()                              */
/************************************************************************/

/**
 * \brief Test if GEOS enabled.
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
 * \brief Create a geometry object of the appropriate type from it's FGF (FDO Geometry Format) binary representation.
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
    return createFromFgfInternal(pabyData, poSR, ppoReturn, nBytes,
                                 pnBytesConsumed, 0);
}


/************************************************************************/
/*                       createFromFgfInternal()                        */
/************************************************************************/

OGRErr OGRGeometryFactory::createFromFgfInternal( unsigned char *pabyData,
                                                  OGRSpatialReference * poSR,
                                                  OGRGeometry **ppoReturn,
                                                  int nBytes,
                                                  int *pnBytesConsumed,
                                                  int nRecLevel )
{
    OGRErr       eErr = OGRERR_NONE;
    OGRGeometry *poGeom = NULL;
    GInt32       nGType, nGDim;
    int          nTupleSize = 0;
    int          iOrdinal = 0;
    
    (void) iOrdinal;

    /* Arbitrary value, but certainly large enough for reasonable usages ! */
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Too many recursiong level (%d) while parsing FGF geometry.",
                    nRecLevel );
        return OGRERR_CORRUPT_DATA;
    }

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

        if (nPointCount < 0 || nPointCount > INT_MAX / (nTupleSize * 8))
            return OGRERR_CORRUPT_DATA;

        if( nBytes - 12 < nTupleSize * 8 * nPointCount )
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

        if (nRingCount < 0 || nRingCount > INT_MAX / 4)
            return OGRERR_CORRUPT_DATA;

        /* Each ring takes at least 4 bytes */
        if (nBytes - 12 < nRingCount * 4)
            return OGRERR_NOT_ENOUGH_DATA;

        nNextByte = 12;
        
        poGeom = poPoly = new OGRPolygon();

        for( iRing = 0; iRing < nRingCount; iRing++ )
        {
            if( nBytes - nNextByte < 4 )
            {
                delete poGeom;
                return OGRERR_NOT_ENOUGH_DATA;
            }

            memcpy( &nPointCount, pabyData + nNextByte, 4 );
            CPL_LSBPTR32( &nPointCount );

            if (nPointCount < 0 || nPointCount > INT_MAX / (nTupleSize * 8))
            {
                delete poGeom;
                return OGRERR_CORRUPT_DATA;
            }

            nNextByte += 4;

            if( nBytes - nNextByte < nTupleSize * 8 * nPointCount )
            {
                delete poGeom;
                return OGRERR_NOT_ENOUGH_DATA;
            }

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

        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        memcpy( &nGeomCount, pabyData + 4, 4 );
        CPL_LSBPTR32( &nGeomCount );

        if (nGeomCount < 0 || nGeomCount > INT_MAX / 4)
            return OGRERR_CORRUPT_DATA;

        /* Each geometry takes at least 4 bytes */
        if (nBytes - 8 < 4 * nGeomCount)
            return OGRERR_NOT_ENOUGH_DATA;

        nBytesUsed = 8;

        if( nGType == 4 )
            poGC = new OGRMultiPoint();
        else if( nGType == 5 )
            poGC = new OGRMultiLineString();
        else if( nGType == 6 )
            poGC = new OGRMultiPolygon();
        else if( nGType == 7 )
            poGC = new OGRGeometryCollection();
        
        for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
        {
            int nThisGeomSize;
            OGRGeometry *poThisGeom = NULL;
         
            eErr = createFromFgfInternal( pabyData + nBytesUsed, poSR, &poThisGeom,
                                  nBytes - nBytesUsed, &nThisGeomSize, nRecLevel + 1);
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
                delete poThisGeom;
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

/************************************************************************/
/*                SplitLineStringAtDateline()                           */
/************************************************************************/

#define SWAP_DBL(a,b) do { double tmp = a; a = b; b = tmp; } while(0)

static void SplitLineStringAtDateline(OGRGeometryCollection* poMulti,
                                      const OGRLineString* poLS,
                                      double dfDateLineOffset)
{
    double dfLeftBorderX = 180 - dfDateLineOffset;
    double dfRightBorderX = -180 + dfDateLineOffset;
    double dfDiffSpace = 360 - dfDateLineOffset;

    int i;
    int bIs3D = poLS->getCoordinateDimension() == 3;
    OGRLineString* poNewLS = new OGRLineString();
    poMulti->addGeometryDirectly(poNewLS);
    for(i=0;i<poLS->getNumPoints();i++)
    {
        double dfX = poLS->getX(i);
        if (i > 0 && fabs(dfX - poLS->getX(i-1)) > dfDiffSpace)
        {
            double dfX1 = poLS->getX(i-1);
            double dfY1 = poLS->getY(i-1);
            double dfZ1 = poLS->getY(i-1);
            double dfX2 = poLS->getX(i);
            double dfY2 = poLS->getY(i);
            double dfZ2 = poLS->getY(i);

            if (dfX1 > -180 && dfX1 < dfRightBorderX && dfX2 == 180 &&
                i+1 < poLS->getNumPoints() &&
                poLS->getX(i+1) > -180 && poLS->getX(i+1) < dfRightBorderX)
            {
                if( bIs3D )
                    poNewLS->addPoint(-180, poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(-180, poLS->getY(i));

                i++;

                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i), poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(poLS->getX(i), poLS->getY(i));
                continue;
            }
            else if (dfX1 > dfLeftBorderX && dfX1 < 180 && dfX2 == -180 &&
                     i+1 < poLS->getNumPoints() &&
                     poLS->getX(i+1) > dfLeftBorderX && poLS->getX(i+1) < 180)
            {
                if( bIs3D )
                    poNewLS->addPoint(180, poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(180, poLS->getY(i));

                i++;

                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i), poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(poLS->getX(i), poLS->getY(i));
                continue;
            }

            if (dfX1 < dfRightBorderX && dfX2 > dfLeftBorderX)
            {
                SWAP_DBL(dfX1, dfX2);
                SWAP_DBL(dfY1, dfY2);
                SWAP_DBL(dfZ1, dfZ2);
            }
            if (dfX1 > dfLeftBorderX && dfX2 < dfRightBorderX)
                dfX2 += 360;

            if (dfX1 <= 180 && dfX2 >= 180 && dfX1 < dfX2)
            {
                double dfRatio = (180 - dfX1) / (dfX2 - dfX1);
                double dfY = dfRatio * dfY2 + (1 - dfRatio) * dfY1;
                double dfZ = dfRatio * dfZ2 + (1 - dfRatio) * dfZ1;
                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i-1) > dfLeftBorderX ? 180 : -180, dfY, dfZ);
                else
                    poNewLS->addPoint(poLS->getX(i-1) > dfLeftBorderX ? 180 : -180, dfY);
                poNewLS = new OGRLineString();
                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i-1) > dfLeftBorderX ? -180 : 180, dfY, dfZ);
                else
                    poNewLS->addPoint(poLS->getX(i-1) > dfLeftBorderX ? -180 : 180, dfY);
                poMulti->addGeometryDirectly(poNewLS);
            }
            else
            {
                poNewLS = new OGRLineString();
                poMulti->addGeometryDirectly(poNewLS);
            }
        }
        if( bIs3D )
            poNewLS->addPoint(dfX, poLS->getY(i), poLS->getZ(i));
        else
            poNewLS->addPoint(dfX, poLS->getY(i));
    }
}

/************************************************************************/
/*               FixPolygonCoordinatesAtDateLine()                      */
/************************************************************************/

#ifdef HAVE_GEOS
static void FixPolygonCoordinatesAtDateLine(OGRPolygon* poPoly, double dfDateLineOffset)
{
    double dfLeftBorderX = 180 - dfDateLineOffset;
    double dfRightBorderX = -180 + dfDateLineOffset;
    double dfDiffSpace = 360 - dfDateLineOffset;

    int i, iPart;
    for(iPart = 0; iPart < 1 + poPoly->getNumInteriorRings(); iPart++)
    {
        OGRLineString* poLS = (iPart == 0) ? poPoly->getExteriorRing() :
                                             poPoly->getInteriorRing(iPart-1);
        int bGoEast = FALSE;
        int bIs3D = poLS->getCoordinateDimension() == 3;
        for(i=1;i<poLS->getNumPoints();i++)
        {
            double dfX = poLS->getX(i);
            double dfPrevX = poLS->getX(i-1);
            double dfDiffLong = fabs(dfX - dfPrevX);
            if (dfDiffLong > dfDiffSpace)
            {
                if ((dfPrevX > dfLeftBorderX && dfX < dfRightBorderX) || (dfX < 0 && bGoEast))
                {
                    dfX += 360;
                    bGoEast = TRUE;
                    if( bIs3D )
                        poLS->setPoint(i, dfX, poLS->getY(i), poLS->getZ(i));
                    else
                        poLS->setPoint(i, dfX, poLS->getY(i));
                }
                else if (dfPrevX < dfRightBorderX && dfX > dfLeftBorderX)
                {
                    int j;
                    for(j=i-1;j>=0;j--)
                    {
                        dfX = poLS->getX(j);
                        if (dfX < 0)
                        {
                            if( bIs3D )
                                poLS->setPoint(j, dfX + 360, poLS->getY(j), poLS->getZ(j));
                            else
                                poLS->setPoint(j, dfX + 360, poLS->getY(j));
                        }
                    }
                    bGoEast = FALSE;
                }
                else
                {
                    bGoEast = FALSE;
                }
            }
        }
    }
}
#endif

/************************************************************************/
/*                            Sub360ToLon()                             */
/************************************************************************/

static void Sub360ToLon( OGRGeometry* poGeom )
{
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbPolygon:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int nSubGeomCount = OGR_G_GetGeometryCount((OGRGeometryH)poGeom);
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                Sub360ToLon((OGRGeometry*)OGR_G_GetGeometryRef((OGRGeometryH)poGeom, iGeom));
            }
            
            break;
        }

        case wkbLineString:
        {
            OGRLineString* poLineString = (OGRLineString* )poGeom;
            int nPointCount = poLineString->getNumPoints();
            int nCoordDim = poLineString->getCoordinateDimension();
            for( int iPoint = 0; iPoint < nPointCount; iPoint++)
            {
                if (nCoordDim == 2)
                    poLineString->setPoint(iPoint,
                                     poLineString->getX(iPoint) - 360,
                                     poLineString->getY(iPoint));
                else
                    poLineString->setPoint(iPoint,
                                     poLineString->getX(iPoint) - 360,
                                     poLineString->getY(iPoint),
                                     poLineString->getZ(iPoint));
            }
            break;
        }
            
        default:
            break;
    }
}

/************************************************************************/
/*                        AddSimpleGeomToMulti()                        */
/************************************************************************/

static void AddSimpleGeomToMulti(OGRGeometryCollection* poMulti,
                                 const OGRGeometry* poGeom)
{
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbPolygon:
        case wkbLineString:
            poMulti->addGeometry(poGeom);
            break;
            
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int nSubGeomCount = OGR_G_GetGeometryCount((OGRGeometryH)poGeom);
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                OGRGeometry* poSubGeom =
                    (OGRGeometry*)OGR_G_GetGeometryRef((OGRGeometryH)poGeom, iGeom);
                AddSimpleGeomToMulti(poMulti, poSubGeom);
            }
            break;
        }
            
        default:
            break;
    }
}

/************************************************************************/
/*                 CutGeometryOnDateLineAndAddToMulti()                 */
/************************************************************************/

static void CutGeometryOnDateLineAndAddToMulti(OGRGeometryCollection* poMulti,
                                               const OGRGeometry* poGeom,
                                               double dfDateLineOffset)
{
    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());
    switch (eGeomType)
    {
        case wkbPolygon:
        case wkbLineString:
        {
            int bWrapDateline = FALSE;
            int bSplitLineStringAtDateline = FALSE;
            OGREnvelope oEnvelope;
            
            poGeom->getEnvelope(&oEnvelope);
            
            /* Naive heuristics... Place to improvement... */
            OGRGeometry* poDupGeom = NULL;
            
            double dfLeftBorderX = 180 - dfDateLineOffset;
            double dfRightBorderX = -180 + dfDateLineOffset;
            double dfDiffSpace = 360 - dfDateLineOffset;
            
            if (oEnvelope.MinX > dfLeftBorderX && oEnvelope.MaxX > 180)
            {
#ifndef HAVE_GEOS
                CPLError( CE_Failure, CPLE_NotSupported, 
                        "GEOS support not enabled." );
#else
                bWrapDateline = TRUE;
#endif
            }
            else
            {
                OGRLineString* poLS;
                if (eGeomType == wkbPolygon)
                    poLS = ((OGRPolygon*)poGeom)->getExteriorRing();
                else
                    poLS = (OGRLineString*)poGeom;
                if (poLS)
                {
                    int i;
                    double dfMaxSmallDiffLong = 0;
                    int bHasBigDiff = FALSE;
                    /* Detect big gaps in longitude */
                    for(i=1;i<poLS->getNumPoints();i++)
                    {
                        double dfPrevX = poLS->getX(i-1);
                        double dfX = poLS->getX(i);
                        double dfDiffLong = fabs(dfX - dfPrevX);
                        if (dfDiffLong > dfDiffSpace &&
                            ((dfX > dfLeftBorderX && dfPrevX < dfRightBorderX) || (dfPrevX > dfLeftBorderX && dfX < dfRightBorderX)))
                            bHasBigDiff = TRUE;
                        else if (dfDiffLong > dfMaxSmallDiffLong)
                            dfMaxSmallDiffLong = dfDiffLong;
                    }
                    if (bHasBigDiff && dfMaxSmallDiffLong < dfDateLineOffset)
                    {
                        if (eGeomType == wkbLineString)
                            bSplitLineStringAtDateline = TRUE;
                        else
                        {
#ifndef HAVE_GEOS
                            CPLError( CE_Failure, CPLE_NotSupported, 
                                    "GEOS support not enabled." );
#else
                            bWrapDateline = TRUE;
                            poDupGeom = poGeom->clone();
                            FixPolygonCoordinatesAtDateLine((OGRPolygon*)poDupGeom, dfDateLineOffset);
#endif
                        }
                    }
                }
            }

            if (bSplitLineStringAtDateline)
            {
                SplitLineStringAtDateline(poMulti, (OGRLineString*)poGeom, dfDateLineOffset);
            }
            else if (bWrapDateline)
            {
                const OGRGeometry* poWorkGeom = (poDupGeom) ? poDupGeom : poGeom;
                OGRGeometry* poRectangle1 = NULL;
                OGRGeometry* poRectangle2 = NULL;
                const char* pszWKT1 = "POLYGON((0 90,180 90,180 -90,0 -90,0 90))";
                const char* pszWKT2 = "POLYGON((180 90,360 90,360 -90,180 -90,180 90))";
                OGRGeometryFactory::createFromWkt((char**)&pszWKT1, NULL, &poRectangle1);
                OGRGeometryFactory::createFromWkt((char**)&pszWKT2, NULL, &poRectangle2);
                OGRGeometry* poGeom1 = poWorkGeom->Intersection(poRectangle1);
                OGRGeometry* poGeom2 = poWorkGeom->Intersection(poRectangle2);
                delete poRectangle1;
                delete poRectangle2;
                
                if (poGeom1 != NULL && poGeom2 != NULL)
                {
                    AddSimpleGeomToMulti(poMulti, poGeom1);
                    Sub360ToLon(poGeom2);
                    AddSimpleGeomToMulti(poMulti, poGeom2);
                }
                else
                {
                    AddSimpleGeomToMulti(poMulti, poGeom);
                }
                
                delete poGeom1;
                delete poGeom2;
                delete poDupGeom;
            }
            else
            {
                poMulti->addGeometry(poGeom);
            }   
            break;
        }
            
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int nSubGeomCount = OGR_G_GetGeometryCount((OGRGeometryH)poGeom);
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                OGRGeometry* poSubGeom =
                    (OGRGeometry*)OGR_G_GetGeometryRef((OGRGeometryH)poGeom, iGeom);
                CutGeometryOnDateLineAndAddToMulti(poMulti, poSubGeom, dfDateLineOffset);
            }
            break;
        }
            
        default:
            break;
    }
}

/************************************************************************/
/*                       transformWithOptions()                         */
/************************************************************************/

OGRGeometry* OGRGeometryFactory::transformWithOptions( const OGRGeometry* poSrcGeom,
                                                       OGRCoordinateTransformation *poCT,
                                                       char** papszOptions )
{
    OGRGeometry* poDstGeom = poSrcGeom->clone();
    if (poCT != NULL)
    {
        OGRErr eErr = poDstGeom->transform(poCT);
        if (eErr != OGRERR_NONE)
        {
            delete poDstGeom;
            return NULL;
        }
    }
    
    if (CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "WRAPDATELINE", "NO")))
    {
        OGRwkbGeometryType eType = wkbFlatten(poSrcGeom->getGeometryType());
        OGRwkbGeometryType eNewType;
        if (eType == wkbPolygon || eType == wkbMultiPolygon)
            eNewType = wkbMultiPolygon;
        else if (eType == wkbLineString || eType == wkbMultiLineString)
            eNewType = wkbMultiLineString;
        else
            eNewType = wkbGeometryCollection;
        
        OGRGeometryCollection* poMulti =
            (OGRGeometryCollection* )createGeometry(eNewType);
            
        double dfDateLineOffset = CPLAtofM(CSLFetchNameValueDef(papszOptions, "DATELINEOFFSET", "10"));
        if(dfDateLineOffset <= 0 || dfDateLineOffset >= 360)
            dfDateLineOffset = 10;

        CutGeometryOnDateLineAndAddToMulti(poMulti, poDstGeom, dfDateLineOffset);
        
        if (poMulti->getNumGeometries() == 0)
        {
            delete poMulti;
        }            
        else if (poMulti->getNumGeometries() == 1)
        {
            delete poDstGeom;
            poDstGeom = poMulti->getGeometryRef(0)->clone();
            delete poMulti;
        }
        else
        {
            delete poDstGeom;
            poDstGeom = poMulti;
        }
    }

    return poDstGeom;
}

/************************************************************************/
/*                       OGRGF_GetDefaultStepSize()                     */
/************************************************************************/

static double OGRGF_GetDefaultStepSize()
{
    return CPLAtofM(CPLGetConfigOption("OGR_ARC_STEPSIZE","4"));
}

/************************************************************************/
/*                        approximateArcAngles()                        */
/************************************************************************/

/**
 * Stroke arc to linestring.
 *
 * Stroke an arc of a circle to a linestring based on a center
 * point, radius, start angle and end angle, all angles in degrees.
 *
 * If the dfMaxAngleStepSizeDegrees is zero, then a default value will be
 * used.  This is currently 4 degrees unless the user has overridden the
 * value with the OGR_ARC_STEPSIZE configuration variable. 
 *
 * @see CPLSetConfigOption()
 *
 * @param dfCenterX center X
 * @param dfCenterY center Y
 * @param dfZ center Z
 * @param dfPrimaryRadius X radius of ellipse.
 * @param dfSecondaryRadius Y radius of ellipse. 
 * @param dfRotation rotation of the ellipse clockwise.
 * @param dfStartAngle angle to first point on arc (clockwise of X-positive) 
 * @param dfEndAngle angle to last point on arc (clockwise of X-positive) 
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * 
 * @return OGRLineString geometry representing an approximation of the arc.
 *
 * @since OGR 1.8.0
 */

OGRGeometry* OGRGeometryFactory::approximateArcAngles( 
    double dfCenterX, double dfCenterY, double dfZ,
    double dfPrimaryRadius, double dfSecondaryRadius, double dfRotation, 
    double dfStartAngle, double dfEndAngle,
    double dfMaxAngleStepSizeDegrees )

{
    double             dfSlice;
    int                iPoint, nVertexCount;
    OGRLineString     *poLine = new OGRLineString();
    double             dfRotationRadians = dfRotation * M_PI / 180.0;

    // support default arc step setting.
    if( dfMaxAngleStepSizeDegrees < 1e-6 )
    {
        dfMaxAngleStepSizeDegrees = OGRGF_GetDefaultStepSize();
    }

    // switch direction 
    dfStartAngle *= -1;
    dfEndAngle *= -1;

    // Figure out the number of slices to make this into.
    nVertexCount = (int) 
        ceil(fabs(dfEndAngle - dfStartAngle)/dfMaxAngleStepSizeDegrees) + 1;
    nVertexCount = MAX(2,nVertexCount);
    dfSlice = (dfEndAngle-dfStartAngle)/(nVertexCount-1);

/* -------------------------------------------------------------------- */
/*      Compute the interpolated points.                                */
/* -------------------------------------------------------------------- */
    for( iPoint=0; iPoint < nVertexCount; iPoint++ )
    {
        double      dfAngleOnEllipse;
        double      dfArcX, dfArcY;
        double      dfEllipseX, dfEllipseY;

        dfAngleOnEllipse = (dfStartAngle + iPoint * dfSlice) * M_PI / 180.0;

        // Compute position on the unrotated ellipse. 
        dfEllipseX = cos(dfAngleOnEllipse) * dfPrimaryRadius;
        dfEllipseY = sin(dfAngleOnEllipse) * dfSecondaryRadius;
        
        // Rotate this position around the center of the ellipse.
        dfArcX = dfCenterX 
            + dfEllipseX * cos(dfRotationRadians) 
            + dfEllipseY * sin(dfRotationRadians);
        dfArcY = dfCenterY 
            - dfEllipseX * sin(dfRotationRadians)
            + dfEllipseY * cos(dfRotationRadians);

        poLine->setPoint( iPoint, dfArcX, dfArcY, dfZ );
    }

    return poLine;
}

/************************************************************************/
/*                     OGR_G_ApproximateArcAngles()                     */
/************************************************************************/

/**
 * Stroke arc to linestring.
 *
 * Stroke an arc of a circle to a linestring based on a center
 * point, radius, start angle and end angle, all angles in degrees.
 *
 * If the dfMaxAngleStepSizeDegrees is zero, then a default value will be
 * used.  This is currently 4 degrees unless the user has overridden the
 * value with the OGR_ARC_STEPSIZE configuration variable.
 *
 * @see CPLSetConfigOption()
 *
 * @param dfCenterX center X
 * @param dfCenterY center Y
 * @param dfZ center Z
 * @param dfPrimaryRadius X radius of ellipse.
 * @param dfSecondaryRadius Y radius of ellipse.
 * @param dfRotation rotation of the ellipse clockwise.
 * @param dfStartAngle angle to first point on arc (clockwise of X-positive)
 * @param dfEndAngle angle to last point on arc (clockwise of X-positive)
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 *
 * @return OGRLineString geometry representing an approximation of the arc.
 *
 * @since OGR 1.8.0
 */

OGRGeometryH CPL_DLL 
OGR_G_ApproximateArcAngles( 
    double dfCenterX, double dfCenterY, double dfZ,
    double dfPrimaryRadius, double dfSecondaryRadius, double dfRotation,
    double dfStartAngle, double dfEndAngle,
    double dfMaxAngleStepSizeDegrees )

{
    return (OGRGeometryH) OGRGeometryFactory::approximateArcAngles(
        dfCenterX, dfCenterY, dfZ, 
        dfPrimaryRadius, dfSecondaryRadius, dfRotation,
        dfStartAngle, dfEndAngle, dfMaxAngleStepSizeDegrees );
}

/************************************************************************/
/*                           forceToLineString()                        */
/************************************************************************/

/**
 * \brief Convert to line string.
 *
 * Tries to force the provided geometry to be a line string.  This nominally
 * effects a change on multilinestrings.
 * In GDAL 2.0, for polygons or curvepolygons that have a single exterior ring,
 * it will return the ring. For circular strings or compound curves, it will
 * return an approximated line string.
 *
 * The passed in geometry is
 * consumed and a new one returned (or potentially the same one).
 *
 * @param poGeom the input geometry - ownership is passed to the method.
 * @param bOnlyInOrder flag that, if set to FALSE, indicate that the order of
 *                     points in a linestring might be reversed if it enables
 *                     to match the extremity of another linestring. If set
 *                     to TRUE, the start of a linestring must match the end
 *                     of another linestring.
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToLineString( OGRGeometry *poGeom, bool bOnlyInOrder )

{
    if( poGeom == NULL )
        return NULL;

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

/* -------------------------------------------------------------------- */
/*      If this is already a LineString, nothing to do                  */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbLineString )
    {
        /* except if it is a linearring */
        poGeom = OGRCurve::CastToLineString((OGRCurve*)poGeom);

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      If it's a polygon with a single ring, return it                 */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbPolygon || eGeomType == wkbCurvePolygon )
    {
        OGRCurvePolygon* poCP = (OGRCurvePolygon*)poGeom;
        if( poCP->getNumInteriorRings() == 0 )
        {
            OGRCurve* poRing = poCP->stealExteriorRingCurve();
            delete poCP;
            return forceToLineString(poRing);
        }
        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      If it's a curve line, call CurveToLine()                        */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbCircularString ||
        eGeomType == wkbCompoundCurve )
    {
        OGRGeometry* poNewGeom = ((OGRCurve*)poGeom)->CurveToLine();
        delete poGeom;
        return poNewGeom;
    }


    if( eGeomType != wkbGeometryCollection
        && eGeomType != wkbMultiLineString
        && eGeomType != wkbMultiCurve )
        return poGeom;

    // build an aggregated linestring from all the linestrings in the container.
    OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
    if( poGeom->hasCurveGeometry() )
    {
        OGRGeometryCollection *poNewGC = (OGRGeometryCollection *) poGC->getLinearGeometry();
        delete poGC;
        poGeom = poGC = poNewGC;
    }

    if( poGC->getNumGeometries() == 0 )
    {
        poGeom = new OGRLineString();
        poGeom->assignSpatialReference(poGC->getSpatialReference());
        delete poGC;
        return poGeom;
    }

    int iGeom0 = 0;
    while( iGeom0 < poGC->getNumGeometries() )
    {
        if( wkbFlatten(poGC->getGeometryRef(iGeom0)->getGeometryType())
            != wkbLineString )
        {
            iGeom0++;
            continue;
        }

        OGRLineString *poLineString0 = (OGRLineString *) poGC->getGeometryRef(iGeom0);
        if( poLineString0->getNumPoints() < 2 )
        {
            iGeom0++;
            continue;
        }

        OGRPoint pointStart0, pointEnd0;
        poLineString0->StartPoint( &pointStart0 );
        poLineString0->EndPoint( &pointEnd0 );

        int iGeom1;
        for( iGeom1 = iGeom0 + 1; iGeom1 < poGC->getNumGeometries(); iGeom1++ )
        {
            if( wkbFlatten(poGC->getGeometryRef(iGeom1)->getGeometryType())
                != wkbLineString )
                continue;

            OGRLineString *poLineString1 = (OGRLineString *) poGC->getGeometryRef(iGeom1);
            if( poLineString1->getNumPoints() < 2 )
                continue;

            OGRPoint pointStart1, pointEnd1;
            poLineString1->StartPoint( &pointStart1 );
            poLineString1->EndPoint( &pointEnd1 );

            if ( !bOnlyInOrder &&
                 ( pointEnd0.Equals( &pointEnd1 ) || pointStart0.Equals( &pointStart1 ) ) )
            {
                poLineString1->reversePoints();
                poLineString1->StartPoint( &pointStart1 );
                poLineString1->EndPoint( &pointEnd1 );
            }

            if ( pointEnd0.Equals( &pointStart1 ) )
            {
                poLineString0->addSubLineString( poLineString1, 1 );
                poGC->removeGeometry( iGeom1 );
                break;
            }

            if( pointEnd1.Equals( &pointStart0 ) )
            {
                poLineString1->addSubLineString( poLineString0, 1 );
                poGC->removeGeometry( iGeom0 );
                break;
            }
        }

        if ( iGeom1 == poGC->getNumGeometries() )
        {
            iGeom0++;
        }
    }

    if ( poGC->getNumGeometries() == 1 )
    {
        OGRLineString *poLineString = (OGRLineString *) poGC->getGeometryRef(0);
        poGC->removeGeometry( 0, FALSE );
        delete poGC;

        return poLineString;
    }

    return poGC;
}

/************************************************************************/
/*                      OGR_G_ForceToLineString()                       */
/************************************************************************/

/**
 * \brief Convert to line string.
 *
 * This function is the same as the C++ method
 * OGRGeometryFactory::forceToLineString().
 *
 * @param hGeom handle to the geometry to convert (ownership surrendered).
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL/OGR 1.10.0
 */

OGRGeometryH OGR_G_ForceToLineString( OGRGeometryH hGeom )

{
    return (OGRGeometryH)
        OGRGeometryFactory::forceToLineString( (OGRGeometry *) hGeom );
}


/************************************************************************/
/*                           forceTo()                                  */
/************************************************************************/

/**
 * \brief Convert to another geometry type
 *
 * Tries to force the provided geometry to the specified geometry type.
 *
 * It can promote 'single' geometry type to their corresponding collection type
 * (see OGR_GT_GetCollection()) or the reverse. non-linear geometry type to
 * their corresponding linear geometry type (see OGR_GT_GetLinear()), by
 * possibly approximating circular arcs they may contain.
 * Regarding conversion from linear geometry types to curve geometry types, only
 * "wraping" will be done. No attempt to retrieve potential circular arcs by
 * de-approximating stroking will be done. For that, OGRGeometry::getCurveGeometry()
 * can be used.
 *
 * The passed in geometry is consumed and a new one returned (or potentially the same one).
 *
 * @param poGeom the input geometry - ownership is passed to the method.
 * @param eTargetType target output geometry type.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 * @return new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometry * OGRGeometryFactory::forceTo( OGRGeometry* poGeom,
                                           OGRwkbGeometryType eTargetType,
                                           const char*const* papszOptions )
{
    if( poGeom == NULL )
        return poGeom;

    eTargetType = wkbFlatten(eTargetType);
    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if( eType == eTargetType || eTargetType == wkbUnknown )
        return poGeom;

    if( poGeom->IsEmpty() )
    {
        OGRGeometry* poRet = createGeometry(eTargetType);
        if( poRet )
            poRet->assignSpatialReference(poGeom->getSpatialReference());
        delete poGeom;
        return poRet;
    }

    /* Promote single to multi */
    if( !OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) &&
         OGR_GT_IsSubClassOf(OGR_GT_GetCollection(eType), eTargetType) )
    {
        OGRGeometryCollection* poRet = (OGRGeometryCollection*)createGeometry(eTargetType);
        if( poRet )
        {
            poRet->assignSpatialReference(poGeom->getSpatialReference());
            if( eType == wkbLineString )
                poGeom = OGRCurve::CastToLineString( (OGRCurve*)poGeom );
            poRet->addGeometryDirectly(poGeom);
        }
        else
            delete poGeom;
        return poRet;
    }

    int bIsCurve = OGR_GT_IsCurve(eType);
    if( bIsCurve && eTargetType == wkbCompoundCurve )
    {
        return OGRCurve::CastToCompoundCurve((OGRCurve*)poGeom);
    }
    else if( bIsCurve && eTargetType == wkbCurvePolygon )
    {
        OGRCurve* poCurve = (OGRCurve*)poGeom;
        if( poCurve->getNumPoints() >= 3 && poCurve->get_IsClosed() )
        {
            OGRCurvePolygon* poCP = new OGRCurvePolygon();
            if( poCP->addRingDirectly( poCurve ) == OGRERR_NONE )
            {
                poCP->assignSpatialReference(poGeom->getSpatialReference());
                return poCP;
            }
            else
            {
                delete poCP;
            }
        }
    }
    else if( eType == wkbLineString &&
             OGR_GT_IsSubClassOf(eTargetType, wkbMultiSurface) )
    {
        OGRGeometry* poTmp = forceTo(poGeom, wkbPolygon, papszOptions);
        if( wkbFlatten(poTmp->getGeometryType()) != eType)
            return forceTo(poTmp, eTargetType, papszOptions);
    }
    else if( bIsCurve && eTargetType == wkbMultiSurface )
    {
        OGRGeometry* poTmp = forceTo(poGeom, wkbCurvePolygon, papszOptions);
        if( wkbFlatten(poTmp->getGeometryType()) != eType)
            return forceTo(poTmp, eTargetType, papszOptions);
    }
    else if( bIsCurve && eTargetType == wkbMultiPolygon )
    {
        OGRGeometry* poTmp = forceTo(poGeom, wkbPolygon, papszOptions);
        if( wkbFlatten(poTmp->getGeometryType()) != eType)
            return forceTo(poTmp, eTargetType, papszOptions);
    }
    else if (eType == wkbPolygon && eTargetType == wkbCurvePolygon)
    {
        return OGRSurface::CastToCurvePolygon((OGRPolygon*)poGeom);
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) &&
             eTargetType == wkbCompoundCurve )
    {
        OGRCurvePolygon* poPoly = (OGRCurvePolygon*)poGeom;
        if( poPoly->getNumInteriorRings() == 0 )
        {
            OGRCurve* poRet = poPoly->stealExteriorRingCurve();
            if( poRet )
                poRet->assignSpatialReference(poGeom->getSpatialReference());
            delete poPoly;
            return forceTo(poRet, eTargetType, papszOptions);
        }
    }
    else if ( eType == wkbMultiPolygon && eTargetType == wkbMultiSurface )
    {
        return OGRMultiPolygon::CastToMultiSurface((OGRMultiPolygon*)poGeom);
    }
    else if ( eType == wkbMultiLineString && eTargetType == wkbMultiCurve )
    {
        return OGRMultiLineString::CastToMultiCurve((OGRMultiLineString*)poGeom);
    }
    else if ( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        OGRGeometryCollection* poGC = (OGRGeometryCollection*)poGeom;
        if( poGC->getNumGeometries() == 1 )
        {
            OGRGeometry* poSubGeom = poGC->getGeometryRef(0);
            if( poSubGeom )
                poSubGeom->assignSpatialReference(poGeom->getSpatialReference());
            poGC->removeGeometry(0, FALSE);
            OGRGeometry* poRet = forceTo(poSubGeom, eTargetType, papszOptions);
            if( OGR_GT_IsSubClassOf(wkbFlatten(poRet->getGeometryType()), eTargetType) )
            {
                delete poGC;
                return poRet;
            }
            poGC->addGeometryDirectly(poSubGeom);
        }
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) &&
             (OGR_GT_IsSubClassOf(eTargetType, wkbMultiSurface) ||
              OGR_GT_IsSubClassOf(eTargetType, wkbMultiCurve)) )
    {
        OGRCurvePolygon* poCP = (OGRCurvePolygon*)poGeom;
        if( poCP->getNumInteriorRings() == 0 )
        {
            OGRCurve* poRing = poCP->getExteriorRingCurve();
            poRing->assignSpatialReference(poGeom->getSpatialReference());
            OGRwkbGeometryType eRingType = poRing->getGeometryType();
            OGRGeometry* poRingDup = poRing->clone();
            OGRGeometry* poRet = forceTo(poRingDup, eTargetType, papszOptions);
            if( poRet->getGeometryType() != eRingType )
            {
                delete poCP;
                return poRet;
            }
            else
                delete poRet;
        }
    }

    if( eTargetType == wkbLineString )
    {
        poGeom = forceToLineString(poGeom);
    }
    else if( eTargetType == wkbPolygon )
    {
        poGeom = forceToPolygon(poGeom);
    }
    else if( eTargetType == wkbMultiPolygon )
    {
        poGeom = forceToMultiPolygon(poGeom);
    }
    else if( eTargetType == wkbMultiLineString )
    {
        poGeom = forceToMultiLineString(poGeom);
    }
    else if( eTargetType == wkbMultiPoint )
    {
        poGeom = forceToMultiPoint(poGeom);
    }

    return poGeom;
}


/************************************************************************/
/*                          OGR_G_ForceTo()                             */
/************************************************************************/

/**
 * \brief Convert to another geometry type
 *
 * This function is the same as the C++ method OGRGeometryFactory::forceTo().
 *
 * @param hGeom the input geometry - ownership is passed to the method.
 * @param eTargetType target output geometry type.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 * @return new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometryH OGR_G_ForceTo( OGRGeometryH hGeom,
                            OGRwkbGeometryType eTargetType,
                            char** papszOptions )

{
    return (OGRGeometryH)
        OGRGeometryFactory::forceTo( (OGRGeometry *) hGeom, eTargetType,
                                     (const char* const*)papszOptions );
}


/************************************************************************/
/*                         GetCurveParmeters()                          */
/************************************************************************/

/**
 * \brief Returns the parameter of an arc circle.
 *
 * @param x0 x of first point
 * @param y0 y of first point
 * @param z0 z of first point
 * @param x1 x of intermediate point
 * @param y1 y of intermediate point
 * @param z1 z of intermediate point
 * @param x2 x of final point
 * @param y2 y of final point
 * @param z2 z of final point
 * @param R radius (output)
 * @param cx x of arc center (output)
 * @param cx y of arc center (output)
 * @param alpha0 angle between center and first point (output)
 * @param alpha1 angle between center and intermediate point (output)
 * @param alpha2 angle between center and final point (output)
 * @return TRUE if the points are not aligned and define an arc circle.
 *
 * @since GDAL 2.0
 */

#define DISTANCE(x1,y1,x2,y2) sqrt(((x2)-(x1))*((x2)-(x1))+((y2)-(y1))*((y2)-(y1)))

int OGRGeometryFactory::GetCurveParmeters(
    double x0, double y0, double x1, double y1, double x2, double y2,
    double& R, double& cx, double& cy, double& alpha0, double& alpha1, double& alpha2 )
{
    /* Circle */
    if( x0 == x2 && y0 == y2 && (x0 != x1 || y0 != y1) )
    {
        cx = (x0 + x1) / 2;
        cy = (y0 + y1) / 2;
        R = DISTANCE(cx,cy,x0,y0);
        /* Arbitrarily pick counter-clock-wise order (like PostGIS does) */
        alpha0 = atan2(y0 - cy, x0 - cx);
        alpha1 = alpha0 + M_PI;
        alpha2 = alpha0 + 2 * M_PI;
        return TRUE;
    }

    double dx01 = x1 - x0;
    double dy01 = y1 - y0;
    double dx12 = x2 - x1;
    double dy12 = y2 - y1;

    /* Normalize above values so as to make sure we don't end up with */
    /* computing a difference of too big values */
    double dfScale = fabs(dx01);
    if( fabs(dy01) > dfScale ) dfScale = fabs(dy01);
    if( fabs(dx12) > dfScale ) dfScale = fabs(dx12);
    if( fabs(dy12) > dfScale ) dfScale = fabs(dy12);
    double dfInvScale = 1.0 / dfScale;
    dx01 *= dfInvScale;
    dy01 *= dfInvScale;
    dx12 *= dfInvScale;
    dy12 *= dfInvScale;

    double det = dx01 * dy12 - dx12 * dy01;
    if( fabs(det) < 1e-8 )
    {
        return FALSE;
    }
    double x01_mid = (x0 + x1) * dfInvScale;
    double x12_mid = (x1 + x2) * dfInvScale;
    double y01_mid = (y0 + y1) * dfInvScale;
    double y12_mid = (y1 + y2) * dfInvScale;
    double c01 = dx01 * x01_mid + dy01 * y01_mid;
    double c12 = dx12 * x12_mid + dy12 * y12_mid;
    cx =  0.5 * dfScale * (c01 * dy12 - c12 * dy01) / det;
    cy =  0.5 * dfScale * (- c01 * dx12 + c12 * dx01) / det;

    alpha0 = atan2((y0 - cy) * dfInvScale, (x0 - cx) * dfInvScale);
    alpha1 = atan2((y1 - cy) * dfInvScale, (x1 - cx) * dfInvScale);
    alpha2 = atan2((y2 - cy) * dfInvScale, (x2 - cx) * dfInvScale);
    R = DISTANCE(cx,cy,x0,y0);

    /* if det is negative, the orientation if clockwise */
    if (det < 0)
    {
        if (alpha1 > alpha0)
            alpha1 -= 2 * M_PI;
        if (alpha2 > alpha1)
            alpha2 -= 2 * M_PI;
    }
    else
    {
        if (alpha1 < alpha0)
            alpha1 += 2 * M_PI;
        if (alpha2 < alpha1)
            alpha2 += 2 * M_PI;
    }

    CPLAssert((alpha0 <= alpha1 && alpha1 <= alpha2) ||
                (alpha0 >= alpha1 && alpha1 >= alpha2));

    return TRUE;
}


/************************************************************************/
/*                      OGRGeometryFactoryStrokeArc()                   */
/************************************************************************/

//#define ROUND_ANGLE_METHOD

static void OGRGeometryFactoryStrokeArc(OGRLineString* poLine,
                                        double cx, double cy, double R,
                                        double z0, double z1, int bHasZ,
                                        double alpha0, double alpha1,
                                        double dfStep,
                                        int bStealthConstraints)
{
    double alpha;

    int nSign = (dfStep > 0) ? 1 : -1;

#ifdef ROUND_ANGLE_METHOD
    /* Initial approach: no longer used */
    /* Discretize on angles that are multiple of dfStep so as to not */
    /* depend on winding order. */
    if (dfStep > 0 )
    {
        alpha = floor(alpha0  / dfStep) * dfStep;
        if( alpha <= alpha0 )
            alpha += dfStep;
    }
    else
    {
        alpha = ceil(alpha0  / dfStep) * dfStep;
        if( alpha >= alpha0 )
            alpha += dfStep;
    }
#else
    /* Constant angle between all points, so as to not depend on winding order */
    int nSteps = (int)(fabs((alpha1 - alpha0) / dfStep)+0.5);
    if( bStealthConstraints ) 
    {
        /* We need at least 6 intermediate vertex, and if more additional */
        /* multiples of 2 */
        if( nSteps < 1+6 )
            nSteps = 1+6;
        else
            nSteps = 1+6 + 2 * ((nSteps - (1+6) + (2-1)) / 2);
    }
    else if( nSteps < 4 )
        nSteps = 4;
    dfStep = nSign * fabs((alpha1 - alpha0) / nSteps);
    alpha = alpha0 + dfStep;
#endif

    for(; (alpha - alpha1) * nSign < -1e-8; alpha += dfStep)
    {
        double dfX = cx + R * cos(alpha), dfY = cy + R * sin(alpha);
        if( bHasZ )
        {
            double z = z0 + (z1 - z0) * (alpha - alpha0) / (alpha1 - alpha0);
            poLine->addPoint(dfX, dfY, z);
        }
        else
            poLine->addPoint(dfX, dfY);
    }
}

/************************************************************************/
/*                         OGRGF_SetHiddenValue()                       */
/************************************************************************/

#define HIDDEN_ALPHA_WIDTH        32
#define HIDDEN_ALPHA_SCALE        (GUInt32)((((GUIntBig)1) << HIDDEN_ALPHA_WIDTH)-2)
#define HIDDEN_ALPHA_HALF_WIDTH   (HIDDEN_ALPHA_WIDTH / 2)
#define HIDDEN_ALPHA_HALF_MASK    ((1 << HIDDEN_ALPHA_HALF_WIDTH)-1)

/* Encode 16-bit nValue in the 8-lsb of dfX and dfY */

#ifdef CPL_LSB
#define DOUBLE_LSB_OFFSET   0
#else
#define DOUBLE_LSB_OFFSET   7
#endif

static void OGRGF_SetHiddenValue(GUInt16 nValue, double& dfX, double &dfY)
{
    GByte abyData[8];

    memcpy(abyData, &dfX, sizeof(double));
    abyData[DOUBLE_LSB_OFFSET] = (GByte)(nValue & 0xFF);
    memcpy(&dfX, abyData, sizeof(double));

    memcpy(abyData, &dfY, sizeof(double));
    abyData[DOUBLE_LSB_OFFSET] = (GByte)(nValue >> 8);
    memcpy(&dfY, abyData, sizeof(double));
}

/************************************************************************/
/*                         OGRGF_GetHiddenValue()                       */
/************************************************************************/

/* Decode 16-bit nValue from the 8-lsb of dfX and dfY */
static GUInt16 OGRGF_GetHiddenValue(double dfX, double dfY)
{
    GUInt16 nValue;

    GByte abyData[8];
    memcpy(abyData, &dfX, sizeof(double));
    nValue = abyData[DOUBLE_LSB_OFFSET];
    memcpy(abyData, &dfY, sizeof(double));
    nValue |= (abyData[DOUBLE_LSB_OFFSET] << 8);

    return nValue;
}

/************************************************************************/
/*                     OGRGF_NeedSwithArcOrder()                        */
/************************************************************************/

/* We need to define a full ordering between starting point and ending point */
/* whatever it is */
static int OGRGF_NeedSwithArcOrder(double x0, double y0,
                                   double x2, double y2)
{
    return ( x0 < x2 || (x0 == x2 && y0 < y2) );
}

/************************************************************************/
/*                         curveToLineString()                          */
/************************************************************************/

/**
 * \brief Converts an arc circle into an approximate line string
 *
 * The arc circle is defined by a first point, an intermediate point and a
 * final point.
 *
 * The provided dfMaxAngleStepSizeDegrees is a hint. The discretization
 * algorithm may pick a slightly different value.
 *
 * So as to avoid gaps when rendering curve polygons that share common arcs,
 * this method is guaranteed to return a line with reversed vertex if called
 * with inverted first and final point, and identical intermediate point.
 *
 * @param x0 x of first point
 * @param y0 y of first point
 * @param z0 z of first point
 * @param x1 x of intermediate point
 * @param y1 y of intermediate point
 * @param z1 z of intermediate point
 * @param x2 x of final point
 * @param y2 y of final point
 * @param z2 z of final point
 * @param bHasZ TRUE if z must be taken into account
 * @param dfMaxAngleStepSizeDegrees  the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 * Recognized options:
 * <ul>
 * <li>ADD_INTERMEDIATE_POINT=STEALTH/YES/NO (Default to STEALTH).
 *         Determine if and how the intermediate point must be output in the linestring.
 *         If set to STEALTH, no explicit intermediate point is added but its
 *         properties are encoded in low significant bits of intermediate points
 *         and OGRGeometryFactory::curveFromLineString() can decode them.
 *         This is the best compromise for round-tripping in OGR and better results
 *         with PostGIS <a href="http://postgis.org/docs/ST_LineToCurve.html">ST_LineToCurve()</a>
 *         If set to YES, the intermediate point is explicitely added to the linestring.
 *         If set to NO, the intermediate point is not explicitely added.
 * </li>
 * </ul>
 *
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL 2.0
 */

OGRLineString* OGRGeometryFactory::curveToLineString(
                                            double x0, double y0, double z0,
                                            double x1, double y1, double z1,
                                            double x2, double y2, double z2,
                                            int bHasZ,
                                            double dfMaxAngleStepSizeDegrees,
                                            const char*const* papszOptions )
{
    double R, cx, cy, alpha0, alpha1, alpha2;

    /* So as to make sure the same curve followed in both direction results */
    /* in perfectly(=binary identical) symetrical points */
    if( OGRGF_NeedSwithArcOrder(x0,y0,x2,y2) )
    {
        OGRLineString* poLS = curveToLineString(x2,y2,z2,x1,y1,z1,x0,y0,z0,
                                                bHasZ, dfMaxAngleStepSizeDegrees,
                                                papszOptions);
        poLS->reversePoints();
        return poLS;
    }

    OGRLineString* poLine = new OGRLineString();
    int bIsArc = TRUE;
    if( !GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                           R, cx, cy, alpha0, alpha1, alpha2)) 
    {
        bIsArc = FALSE;
        cx = cy = R = alpha0 = alpha1 = alpha2 = 0.0;
    }

    int nSign = (alpha1 >= alpha0) ? 1 : -1;

    // support default arc step setting.
    if( dfMaxAngleStepSizeDegrees < 1e-6 )
    {
        dfMaxAngleStepSizeDegrees = OGRGF_GetDefaultStepSize();
    }

    double dfStep =
        dfMaxAngleStepSizeDegrees / 180 * M_PI;
    if (dfStep <= 0.01 / 180 * M_PI)
    {
        CPLDebug("OGR", "Too small arc step size: limiting to 0.01 degree.");
        dfStep = 0.01 / 180 * M_PI;
    }

    dfStep *= nSign;

    if( bHasZ )
        poLine->addPoint(x0, y0, z0);
    else
        poLine->addPoint(x0, y0);

    int bAddIntermediatePoint = FALSE;
    int bStealth = TRUE;
    for(const char* const* papszIter = papszOptions; papszIter && *papszIter; papszIter++)
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszKey != NULL && EQUAL(pszKey, "ADD_INTERMEDIATE_POINT") )
        {
            if( EQUAL(pszValue, "YES") || EQUAL(pszValue, "TRUE") || EQUAL(pszValue, "ON") )
            {
                bAddIntermediatePoint = TRUE;
                bStealth = FALSE;
            }
            else if( EQUAL(pszValue, "NO") || EQUAL(pszValue, "FALSE") || EQUAL(pszValue, "OFF") )
            {
                bAddIntermediatePoint = FALSE;
                bStealth = FALSE;
            }
            else if( EQUAL(pszValue, "STEALTH") )
            {
                /* default */
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported option: %s",
                        *papszIter);
        }
        CPLFree(pszKey);
    }

    if( !bIsArc || bAddIntermediatePoint )
    {
        OGRGeometryFactoryStrokeArc(poLine, cx, cy, R,
                                    z0, z1, bHasZ,
                                    alpha0, alpha1, dfStep,
                                    FALSE);

        if( bHasZ )
            poLine->addPoint(x1, y1, z1);
        else
            poLine->addPoint(x1, y1);

        OGRGeometryFactoryStrokeArc(poLine, cx, cy, R,
                                    z1, z2, bHasZ,
                                    alpha1, alpha2, dfStep,
                                    FALSE);
    }
    else
    {
        OGRGeometryFactoryStrokeArc(poLine, cx, cy, R,
                                    z0, z2, bHasZ,
                                    alpha0, alpha2, dfStep,
                                    bStealth);

        if( bStealth )
        {
            /* 'Hide' the angle of the intermediate point in the 8 low-significant */
            /* bits of the x,y of the first 2 computed points (so 32 bits), */
            /* then put 0xFF, and on the last couple points put again the */
            /* angle but in reverse order, so that overall the low-significant bits */
            /* of all the points are symetrical w.r.t the mid-point */
            double dfRatio = (alpha1 - alpha0) / (alpha2 - alpha0);
            GUInt32 nAlphaRatio = (GUInt32)(0.5 + HIDDEN_ALPHA_SCALE * dfRatio);
            GUInt16 nAlphaRatioLow = nAlphaRatio & HIDDEN_ALPHA_HALF_MASK;
            GUInt16 nAlphaRatioHigh = nAlphaRatio >> HIDDEN_ALPHA_HALF_WIDTH;
            /*printf("alpha0=%f, alpha1=%f, alpha2=%f, dfRatio=%f, nAlphaRatio = %u\n",
                   alpha0, alpha1, alpha2, dfRatio, nAlphaRatio);*/

            CPLAssert( ((poLine->getNumPoints()-1 - 6) % 2) == 0 );

            for(int i=1;i+1<poLine->getNumPoints();i+=2)
            {
                double dfX, dfY;
                GUInt16 nVal = 0xFFFF;

                dfX = poLine->getX(i);
                dfY = poLine->getY(i);
                if( i == 1 )
                    nVal = nAlphaRatioLow;
                else if( i == poLine->getNumPoints() - 2 )
                    nVal = nAlphaRatioHigh;
                OGRGF_SetHiddenValue(nVal, dfX, dfY);
                poLine->setPoint(i, dfX, dfY);

                dfX = poLine->getX(i+1);
                dfY = poLine->getY(i+1);
                if( i == 1 )
                    nVal = nAlphaRatioHigh;
                else if( i == poLine->getNumPoints() - 2 )
                    nVal = nAlphaRatioLow;
                OGRGF_SetHiddenValue(nVal, dfX, dfY);
                poLine->setPoint(i+1, dfX, dfY);
            }
        }
    }

    if( bHasZ )
        poLine->addPoint(x2, y2, z2);
    else
        poLine->addPoint(x2, y2);

    return poLine;
}

/************************************************************************/
/*                         OGRGF_FixAngle()                             */
/************************************************************************/

/* Fix dfAngle by offsets of 2 PI so that it lies between dfAngleStart and */
/* dfAngleStop, whatever their respective order. */
static double OGRGF_FixAngle(double dfAngleStart, double dfAngleStop, double dfAngle)
{
    if( dfAngleStart < dfAngleStop )
    {
        while( dfAngle <= dfAngleStart + 1e-8 )
            dfAngle += 2 * M_PI;
    }
    else
    {
        while( dfAngle >= dfAngleStart - 1e-8 )
            dfAngle -= 2 * M_PI;
    }
    return dfAngle;
}

/************************************************************************/
/*                         OGRGF_DetectArc()                            */
/************************************************************************/

//#define VERBOSE_DEBUG_CURVEFROMLINESTRING

static int OGRGF_DetectArc(const OGRLineString* poLS, int i,
                           OGRCompoundCurve*& poCC,
                           OGRCircularString*& poCS,
                           OGRLineString*& poLSNew)
{
    OGRPoint p0, p1, p2, p3;
    if( i + 3 >= poLS->getNumPoints() )
        return -1;

    poLS->getPoint(i, &p0);
    poLS->getPoint(i+1, &p1);
    poLS->getPoint(i+2, &p2);
    double R_1, cx_1, cy_1, alpha0_1, alpha1_1, alpha2_1;
    if( !(OGRGeometryFactory::GetCurveParmeters(p0.getX(), p0.getY(),
                            p1.getX(), p1.getY(),
                            p2.getX(), p2.getY(),
                            R_1, cx_1, cy_1,
                            alpha0_1, alpha1_1, alpha2_1) &&
          fabs(alpha2_1 - alpha0_1) < 2 * 20.0 / 180.0 * M_PI) )
    {
        return -1;
    }

    int j;
    double dfDeltaAlpha10 = alpha1_1 - alpha0_1;
    double dfDeltaAlpha21 = alpha2_1 - alpha1_1;
    double dfMaxDeltaAlpha = MAX(fabs(dfDeltaAlpha10),
                                    fabs(dfDeltaAlpha21));
    GUInt32 nAlphaRatioRef =
            OGRGF_GetHiddenValue(p1.getX(), p1.getY()) |
        (OGRGF_GetHiddenValue(p2.getX(), p2.getY()) << HIDDEN_ALPHA_HALF_WIDTH);
    int bFoundFFFFFFFFPattern = FALSE;
    int bFoundReversedAlphaRatioRef = FALSE;
    int bValidAlphaRatio = (nAlphaRatioRef > 0 && nAlphaRatioRef < 0xFFFFFFFF);
    int nCountValidAlphaRatio = 1;

    double dfScale = MAX(1, R_1);
    dfScale = MAX(dfScale, fabs(cx_1));
    dfScale = MAX(dfScale, fabs(cy_1));
    dfScale = pow(10.0, ceil(log10(dfScale)));
    double dfInvScale  = 1.0 / dfScale ;

    int bInitialConstantStep =
        (fabs(dfDeltaAlpha10 - dfDeltaAlpha21) / dfMaxDeltaAlpha) < 1e-4;
    double dfDeltaEpsilon = ( bInitialConstantStep ) ?
        dfMaxDeltaAlpha * 1e-4 : dfMaxDeltaAlpha/10;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("----------------------------\n");
    printf("Curve beginning at offset i = %d\n", i);
    printf("Initial alpha ratio = %u\n", nAlphaRatioRef);
    printf("Initial R = %.16g, cx = %.16g, cy = %.16g\n", R_1, cx_1, cy_1);
    printf("dfScale = %f\n", dfScale);
    printf("bInitialConstantStep = %d, "
            "fabs(dfDeltaAlpha10 - dfDeltaAlpha21)=%.8g, "
            "dfMaxDeltaAlpha = %.8f, "
            "dfDeltaEpsilon = %.8f (%.8f)\n",
            bInitialConstantStep,
            fabs(dfDeltaAlpha10 - dfDeltaAlpha21),
            dfMaxDeltaAlpha,
            dfDeltaEpsilon, 1.0 / 180.0 * M_PI);
#endif
    int iMidPoint = -1;
    double dfLastValidAlpha = alpha2_1;

    double dfLastLogRelDiff = 0;

    for(j = i + 1; j + 2 < poLS->getNumPoints(); j++ )
    {
        poLS->getPoint(j, &p1);
        poLS->getPoint(j+1, &p2);
        poLS->getPoint(j+2, &p3);
        double R_2, cx_2, cy_2, alpha0_2, alpha1_2, alpha2_2;
        /* Check that the new candidate arc shares the same */
        /* radius, center and winding order */
        if( !(OGRGeometryFactory::GetCurveParmeters(p1.getX(), p1.getY(),
                                p2.getX(), p2.getY(),
                                p3.getX(), p3.getY(),
                                R_2, cx_2, cy_2,
                                alpha0_2, alpha1_2, alpha2_2)) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d\n : straight line", j);
#endif
            break;
        }

        double dfRelDiffR = fabs(R_1 - R_2) * dfInvScale;
        double dfRelDiffCx = fabs(cx_1 - cx_2) * dfInvScale;
        double dfRelDiffCy = fabs(cy_1 - cy_2) * dfInvScale;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("j=%d: R = %.16g, cx = %.16g, cy = %.16g, "
                "rel_diff_R=%.8g rel_diff_cx=%.8g rel_diff_cy=%.8g\n",
                j, R_2, cx_2, cy_2, dfRelDiffR, dfRelDiffCx, dfRelDiffCy);
#endif

        if( //(dfRelDiffR > 1e-8 || dfRelDiffCx > 1e-8 || dfRelDiffCy > 1e-8) ||
            (dfRelDiffR > 1e-6 && dfRelDiffCx > 1e-6 && dfRelDiffCy > 1e-6) ||
            dfDeltaAlpha10 * (alpha1_2 - alpha0_2) < 0 )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d\n", j);
#endif
            break;
        }

        if( dfRelDiffR > 0 && dfRelDiffCx > 0 && dfRelDiffCy > 0 )
        {
            double dfLogRelDiff = MIN(MIN(fabs(log10(dfRelDiffR)),
                                          fabs(log10(dfRelDiffCx))),
                                      fabs(log10(dfRelDiffCy)));
            /*printf("dfLogRelDiff = %f, dfLastLogRelDiff=%f, "
                     "dfLogRelDiff - dfLastLogRelDiff=%f\n",
                     dfLogRelDiff, dfLastLogRelDiff,
                     dfLogRelDiff - dfLastLogRelDiff);*/
            if( dfLogRelDiff > 0 && dfLastLogRelDiff > 0 &&
                dfLastLogRelDiff >= 8 && dfLogRelDiff <= 8 &&
                dfLogRelDiff < dfLastLogRelDiff - 2 )
            {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf("End of curve at j=%d. Significant different in "
                       "relative error w.r.t previous points\n", j);
#endif
                break;
            }
            dfLastLogRelDiff = dfLogRelDiff;
        }

        double dfStep10 = fabs(alpha1_2 - alpha0_2);
        double dfStep21 = fabs(alpha2_2 - alpha1_2);
        /* Check that the angle step is consistant with the original */
        /* step. */
        if( !(dfStep10 < 2 * dfMaxDeltaAlpha && dfStep21 < 2 * dfMaxDeltaAlpha) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d: dfStep10=%f, dfStep21=%f, 2*dfMaxDeltaAlpha=%f\n",
                    j, dfStep10, dfStep21, 2 * dfMaxDeltaAlpha);
#endif
            break;
        }

        if( bValidAlphaRatio && j > i + 1 && (i % 2) != (j % 2 ) )
        {
            GUInt32 nAlphaRatioReversed =
                (OGRGF_GetHiddenValue(p1.getX(), p1.getY()) << HIDDEN_ALPHA_HALF_WIDTH) |
                (OGRGF_GetHiddenValue(p2.getX(), p2.getY()));
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("j=%d, nAlphaRatioReversed = %u\n",
                        j, nAlphaRatioReversed);
#endif
            if( !bFoundFFFFFFFFPattern && nAlphaRatioReversed == 0xFFFFFFFF )
            {
                bFoundFFFFFFFFPattern = TRUE;
                nCountValidAlphaRatio ++;
            }
            else if( bFoundFFFFFFFFPattern && !bFoundReversedAlphaRatioRef &&
                        nAlphaRatioReversed == 0xFFFFFFFF )
            {
                nCountValidAlphaRatio ++;
            }
            else if( bFoundFFFFFFFFPattern && !bFoundReversedAlphaRatioRef &&
                        nAlphaRatioReversed == nAlphaRatioRef )
            {
                bFoundReversedAlphaRatioRef = TRUE;
                nCountValidAlphaRatio ++;
            }
            else
            {
                if( bInitialConstantStep &&
                    fabs(dfLastValidAlpha - alpha0_1) >= M_PI &&
                    nCountValidAlphaRatio > 10 )
                {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                    printf("End of curve at j=%d: "
                            "fabs(dfLastValidAlpha - alpha0_1)=%f, "
                            "nCountValidAlphaRatio=%d\n",
                            j,
                            fabs(dfLastValidAlpha - alpha0_1),
                            nCountValidAlphaRatio);
#endif
                    if( dfLastValidAlpha - alpha0_1 > 0 )
                    {
                        while( dfLastValidAlpha - alpha0_1 - dfMaxDeltaAlpha - M_PI > -dfMaxDeltaAlpha/10 )
                        {
                            dfLastValidAlpha -= dfMaxDeltaAlpha;
                            j --;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf("--> corrected as fabs(dfLastValidAlpha - alpha0_1)=%f, j=%d\n",
                                fabs(dfLastValidAlpha - alpha0_1), j);
#endif
                        }
                    }
                    else
                    {
                        while( dfLastValidAlpha - alpha0_1 + dfMaxDeltaAlpha + M_PI < dfMaxDeltaAlpha/10 )
                        {
                            dfLastValidAlpha += dfMaxDeltaAlpha;
                            j --;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf("--> corrected as fabs(dfLastValidAlpha - alpha0_1)=%f, j=%d\n",
                                fabs(dfLastValidAlpha - alpha0_1), j);
#endif
                        }
                    }
                    poLS->getPoint(j+1, &p2);
                    break;
                }

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf("j=%d, nAlphaRatioReversed = %u --> unconsistant values accross arc. Don't use it\n",
                        j, nAlphaRatioReversed);
#endif
                bValidAlphaRatio = FALSE;
            }
        }

        /* Correct current end angle, consistently with start angle */
        dfLastValidAlpha = OGRGF_FixAngle(alpha0_1, alpha1_1, alpha2_2);

        /* Try to detect the precise intermediate point of the */
        /* arc circle by detecting irregular angle step */
        /* This is OK if we don't detect the right point or fail */
        /* to detect it */
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("j=%d A(0,1)-maxDelta=%.8f A(1,2)-maxDelta=%.8f "
                "x1=%.8f y1=%.8f x2=%.8f y2=%.8f x3=%.8f y3=%.8f\n",
                j, fabs(dfStep10 - dfMaxDeltaAlpha), fabs(dfStep21 - dfMaxDeltaAlpha),
                p1.getX(), p1.getY(), p2.getX(), p2.getY(), p3.getX(), p3.getY());
#endif
        if( j > i + 1 && iMidPoint < 0 && dfDeltaEpsilon < 1.0 / 180.0 * M_PI )
        {
            if( fabs(dfStep10 - dfMaxDeltaAlpha) > dfDeltaEpsilon )
                iMidPoint = j + ((bInitialConstantStep) ? 0 : 1);
            else if( fabs(dfStep21 - dfMaxDeltaAlpha) > dfDeltaEpsilon )
                iMidPoint = j + ((bInitialConstantStep) ? 1 : 2);
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            if( iMidPoint >= 0 )
            {
                OGRPoint pMid;
                poLS->getPoint(iMidPoint, &pMid);
                printf("Midpoint detected at j = %d, iMidPoint = %d, x=%.8f y=%.8f\n",
                        j, iMidPoint, pMid.getX(), pMid.getY());
            }
#endif
        }
    }

    /* Take a minimum threshold of consecutive points */
    /* on the arc to avoid false positives */
    if( j < i + 3 )
        return -1;

    bValidAlphaRatio &= (bFoundFFFFFFFFPattern && bFoundReversedAlphaRatioRef );

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("bValidAlphaRatio=%d bFoundFFFFFFFFPattern=%d, bFoundReversedAlphaRatioRef=%d\n",
            bValidAlphaRatio, bFoundFFFFFFFFPattern, bFoundReversedAlphaRatioRef);
    printf("alpha0_1=%f dfLastValidAlpha=%f\n",
            alpha0_1, dfLastValidAlpha);
#endif

    if( poLSNew != NULL )
    {
        double dfScale2 = MAX(1, fabs(p0.getX()));
        dfScale2 = MAX(dfScale, fabs(p0.getY()));
        /* Not strictly necessary, but helps having 'clean' lines without duplicated points */
        if( fabs(poLSNew->getX(poLSNew->getNumPoints()-1) - p0.getX()) / dfScale2 > 1e-8 ||
            fabs(poLSNew->getY(poLSNew->getNumPoints()-1) - p0.getY()) / dfScale2 > 1e-8 )
            poLSNew->addPoint(&p0);
        if( poLSNew->getNumPoints() >= 2 )
        {
            if( poCC == NULL )
                poCC = new OGRCompoundCurve();
            poCC->addCurveDirectly(poLSNew);
        }
        else
            delete poLSNew;
        poLSNew = NULL;
    }

    if( poCS == NULL )
    {
        poCS = new OGRCircularString();
        poCS->addPoint(&p0);
    }

    OGRPoint* poFinalPoint =
            ( j + 2 >= poLS->getNumPoints() ) ? &p3 : &p2;

    double dfXMid = 0.0, dfYMid = 0.0, dfZMid = 0.0;
    if( bValidAlphaRatio )
    {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Using alpha ratio...\n");
#endif
        double dfAlphaMid;
        if( OGRGF_NeedSwithArcOrder(p0.getX(),p0.getY(),
                                    poFinalPoint->getX(),
                                    poFinalPoint->getY()) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("Switching angles\n");
#endif
            dfAlphaMid = dfLastValidAlpha + nAlphaRatioRef *
                    (alpha0_1 - dfLastValidAlpha) / HIDDEN_ALPHA_SCALE;
            dfAlphaMid = OGRGF_FixAngle(alpha0_1, dfLastValidAlpha, dfAlphaMid);
        }
        else
        {
            dfAlphaMid = alpha0_1 + nAlphaRatioRef *
                    (dfLastValidAlpha - alpha0_1) / HIDDEN_ALPHA_SCALE;
        }

        dfXMid = cx_1 + R_1 * cos(dfAlphaMid);
        dfYMid = cy_1 + R_1 * sin(dfAlphaMid);

#define IS_ALMOST_INTEGER(x)  ((fabs((x)-floor((x)+0.5)))<1e-8)

        if( poLS->getCoordinateDimension() == 3 )
        {
            double dfLastAlpha = 0.0;
            double dfLastZ = 0.0;
            int k;
            for( k = i; k < j+2; k++ )
            {
                OGRPoint p;
                poLS->getPoint(k, &p);
                double dfAlpha = atan2(p.getY() - cy_1, p.getX() - cx_1);
                dfAlpha = OGRGF_FixAngle(alpha0_1, dfLastValidAlpha, dfAlpha);
                if( k > i && ((dfAlpha < dfLastValidAlpha && dfAlphaMid < dfAlpha) ||
                              (dfAlpha > dfLastValidAlpha && dfAlphaMid > dfAlpha)) )
                {
                    double dfRatio = ( dfAlphaMid - dfLastAlpha ) / ( dfAlpha - dfLastAlpha );
                    dfZMid = (1 - dfRatio) * dfLastZ + dfRatio * p.getZ();
                    break;
                }
                dfLastAlpha = dfAlpha;
                dfLastZ = p.getZ();
            }
            if( k == j + 2 )
                dfZMid = dfLastZ;
            if( IS_ALMOST_INTEGER(dfZMid) )
                dfZMid = (int)floor(dfZMid+0.5);
        }

        /* A few rounding strategies in case the mid point was at "exact" coordinates */
        if( R_1 > 1e-5 )
        {
            int bStartEndInteger = ( IS_ALMOST_INTEGER(p0.getX()) &&
                                     IS_ALMOST_INTEGER(p0.getY()) &&
                                     IS_ALMOST_INTEGER(poFinalPoint->getX()) &&
                                     IS_ALMOST_INTEGER(poFinalPoint->getY()) );
            if( bStartEndInteger &&
                fabs(dfXMid - floor(dfXMid+0.5)) / dfScale < 1e-4 &&
                fabs(dfYMid - floor(dfYMid+0.5)) / dfScale < 1e-4 )
            {
                dfXMid = (int)floor(dfXMid+0.5);
                dfYMid = (int)floor(dfYMid+0.5);
                // Sometimes rounding to closest is not best approach
                // Try neighbouring integers to look for the one that
                // minimize the error w.r.t to the arc center
                // But only do that if the radius is greater than
                // the magnitude of the delta that we will try !
                double dfBestRError = fabs(R_1 - DISTANCE(dfXMid,dfYMid,cx_1,cy_1));
                int iBestX = 0, iBestY = 0;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf("initial_error=%f\n", dfBestRError);
#endif
                if( dfBestRError > 0.001 && R_1 > 2 )
                {
                    int nSearchRadius = 1;
                    // Extend the search radius if the arc circle radius
                    // is much higher than the coordinate values
                    double dfMaxCoords = MAX(fabs(p0.getX()), fabs(p0.getY()));
                    dfMaxCoords = MAX(dfMaxCoords, poFinalPoint->getX());
                    dfMaxCoords = MAX(dfMaxCoords, poFinalPoint->getY());
                    dfMaxCoords = MAX(dfMaxCoords, dfXMid);
                    dfMaxCoords = MAX(dfMaxCoords, dfYMid);
                    if( R_1 > dfMaxCoords * 1000 )
                        nSearchRadius = 100;
                    else if( R_1 > dfMaxCoords * 10 )
                        nSearchRadius = 10;
                    for(int iY=-nSearchRadius;iY<=nSearchRadius;iY++)
                    {
                        for(int iX=-nSearchRadius;iX<=nSearchRadius;iX ++)
                        {
                            double dfCandidateX = dfXMid+iX;
                            double dfCandidateY = dfYMid+iY;
                            if( fabs(dfCandidateX - p0.getX()) < 1e-8 &&
                                fabs(dfCandidateY - p0.getY()) < 1e-8 )
                                continue;
                            if( fabs(dfCandidateX - poFinalPoint->getX()) < 1e-8 &&
                                fabs(dfCandidateY - poFinalPoint->getY()) < 1e-8 )
                                continue;
                            double dfRError = fabs(R_1 - DISTANCE(dfCandidateX,dfCandidateY,cx_1,cy_1));
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf("x=%d y=%d error=%f besterror=%f\n",
                                    (int)(dfXMid+iX),(int)(dfYMid+iY),dfRError,dfBestRError);
#endif
                            if( dfRError < dfBestRError )
                            {
                                iBestX = iX;
                                iBestY = iY;
                                dfBestRError = dfRError;
                            }
                        }
                    }
                }
                dfXMid += iBestX;
                dfYMid += iBestY;
            }
            else
            {
                /* Limit the number of significant figures in decimal representation */
                if( fabs(dfXMid) < 100000000 )
                {
                    dfXMid = ((GIntBig)floor(dfXMid * 100000000+0.5)) / 100000000.0;
                }
                if( fabs(dfYMid) < 100000000 )
                {
                    dfYMid = ((GIntBig)floor(dfYMid * 100000000+0.5)) / 100000000.0;
                }
            }
        }

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("dfAlphaMid=%f, x_mid = %f, y_mid = %f\n", dfLastValidAlpha, dfXMid, dfYMid);
#endif
    }

    /* If this is a full circle of a non-polygonal zone, we must */
    /* use a 5-point representation to keep the winding order */
    if( p0.Equals(poFinalPoint) &&
        !EQUAL(poLS->getGeometryName(), "LINEARRING") )
    {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Full circle of a non-polygonal zone\n");
#endif
        poLS->getPoint((i + j + 2) / 4, &p1);
        poCS->addPoint(&p1);
        if( bValidAlphaRatio )
        {
            p1.setX( dfXMid );
            p1.setY( dfYMid );
            if( poLS->getCoordinateDimension() == 3 )
                p1.setZ( dfZMid );
        }
        else
        {
            poLS->getPoint((i + j + 1) / 2, &p1);
        }
        poCS->addPoint(&p1);
        poLS->getPoint(3 * (i + j + 2) / 4, &p1);
        poCS->addPoint(&p1);
    }

    else if( bValidAlphaRatio )
    {
        p1.setX( dfXMid );
        p1.setY( dfYMid );
        if( poLS->getCoordinateDimension() == 3 )
            p1.setZ( dfZMid );
        poCS->addPoint(&p1);
    }

    /* If we have found a candidate for a precise intermediate */
    /* point, use it */
    else if( iMidPoint >= 1 && iMidPoint < j )
    {
        poLS->getPoint(iMidPoint, &p1);
        poCS->addPoint(&p1);
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Using detected midpoint...\n");
        printf("x_mid = %f, y_mid = %f\n", p1.getX(), p1.getY());
#endif
        }
        /* Otherwise pick up the mid point between both extremities */
        else
        {
            poLS->getPoint((i + j + 1) / 2, &p1);
            poCS->addPoint(&p1);
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("Pickup 'random' midpoint at index=%d...\n", (i + j + 1) / 2);
            printf("x_mid = %f, y_mid = %f\n", p1.getX(), p1.getY());
#endif
        }
        poCS->addPoint(poFinalPoint);

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("----------------------------\n");
#endif

    if( j + 2 >= poLS->getNumPoints() )
        return -2;
    return j + 1;
}

/************************************************************************/
/*                        curveFromLineString()                         */
/************************************************************************/

/**
 * \brief Try to convert a linestring approximating curves into a curve.
 *
 * This method can return a COMPOUNDCURVE, a CIRCULARSTRING or a LINESTRING.
 *
 * This method is the reverse of curveFromLineString().
 *
 * @param poLS handle to the geometry to convert.
 * @param papszOptions options as a null-terminated list of strings.
 *                     Unused for now. Must be set to NULL.
 *
 * @return the converted geometry (ownership to caller).
 *
 * @since GDAL 2.0
 */

OGRCurve* OGRGeometryFactory::curveFromLineString(const OGRLineString* poLS,
                                                  CPL_UNUSED const char*const* papszOptions)
{
    OGRCompoundCurve* poCC = NULL;
    OGRCircularString* poCS = NULL;
    OGRLineString* poLSNew = NULL;
    for(int i=0; i< poLS->getNumPoints(); /* nothing */)
    {
        int iNewI = OGRGF_DetectArc(poLS, i, poCC, poCS, poLSNew);
        if( iNewI == -2 )
            break;
        if( iNewI >= 0 )
        {
            i = iNewI;
            continue;
        }

        if( poCS != NULL )
        {
            if( poCC == NULL )
                poCC = new OGRCompoundCurve();
            poCC->addCurveDirectly(poCS);
            poCS = NULL;
        }

        OGRPoint p;
        poLS->getPoint(i, &p);
        if( poLSNew == NULL )
        {
            poLSNew = new OGRLineString();
            poLSNew->addPoint(&p);
        }
        /* Not strictly necessary, but helps having 'clean' lines without duplicated points */
        else
        {
            double dfScale = MAX(1, fabs(p.getX()));
            dfScale = MAX(dfScale, fabs(p.getY()));
            if( fabs(poLSNew->getX(poLSNew->getNumPoints()-1) - p.getX()) / dfScale > 1e-8 ||
                fabs(poLSNew->getY(poLSNew->getNumPoints()-1) - p.getY()) / dfScale > 1e-8 )
            {
                poLSNew->addPoint(&p);
            }
        }

        i++ ;
    }

    OGRCurve* poRet;

    if( poLSNew != NULL && poLSNew->getNumPoints() < 2 )
    {
        delete poLSNew;
        poLSNew = NULL;
        if( poCC != NULL )
        {
            if( poCC->getNumCurves() == 1 )
            {
                poRet = poCC->stealCurve(0);
                delete poCC;
                poCC = NULL;
            }
            else
                poRet = poCC;
        }
        else
            poRet = (OGRCurve*)poLS->clone();
    }
    else if( poCC != NULL )
    {
        poCC->addCurveDirectly(poLSNew ? (OGRCurve*)poLSNew : (OGRCurve*)poCS);
        poRet = poCC;
    }
    else if( poLSNew != NULL )
        poRet = poLSNew;
    else if( poCS != NULL )
        poRet = poCS;
    else
        poRet = (OGRCurve*)poLS->clone();

    poRet->assignSpatialReference( poLS->getSpatialReference() );

    return poRet;
}
