/******************************************************************************
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

#include "cpl_port.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geos.h"
#include "ogr_sfcgal.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#ifdef HAVE_GEOS
#include "geos_c.h"
#endif
#include "ogrgeojsonreader.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstddef>

#include <algorithm>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#ifndef HAVE_GEOS
#define UNUSED_IF_NO_GEOS CPL_UNUSED
#else
#define UNUSED_IF_NO_GEOS
#endif

CPL_CVSID("$Id$")

/************************************************************************/
/*                           createFromWkb()                            */
/************************************************************************/

/**
 * \brief Create a geometry object of the appropriate type from its
 * well known binary representation.
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
 * @param eWkbVariant WKB variant.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromWkb( const void *pabyData,
                                          OGRSpatialReference * poSR,
                                          OGRGeometry **ppoReturn,
                                          int nBytes,
                                          OGRwkbVariant eWkbVariant )

{
    int nBytesConsumedOutIgnored = -1;
    return createFromWkb( pabyData,
                          poSR,
                          ppoReturn,
                          nBytes,
                          eWkbVariant,
                          nBytesConsumedOutIgnored);
}

/**
 * \brief Create a geometry object of the appropriate type from its
 * well known binary representation.
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
 * @param eWkbVariant WKB variant.
 * @param nBytesConsumedOut output parameter. Number of bytes consumed.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 * @since GDAL 2.3
 */

OGRErr OGRGeometryFactory::createFromWkb( const void *pabyData,
                                          OGRSpatialReference * poSR,
                                          OGRGeometry **ppoReturn,
                                          int nBytes,
                                          OGRwkbVariant eWkbVariant,
                                          int& nBytesConsumedOut )

{
    const GByte* l_pabyData = static_cast<const GByte*>(pabyData);
    nBytesConsumedOut = -1;
    *ppoReturn = nullptr;

    if( nBytes < 9 && nBytes != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.  The extra tests are to work around    */
/*      bug sin the WKB of DB2 v7.2 as identified by Safe Software.     */
/* -------------------------------------------------------------------- */
    const int nByteOrder = DB2_V72_FIX_BYTE_ORDER(*l_pabyData);
    if( nByteOrder != wkbXDR && nByteOrder != wkbNDR )
    {
        CPLDebug( "OGR",
                  "OGRGeometryFactory::createFromWkb() - got corrupt data.\n"
                  "%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                  l_pabyData[0],
                  l_pabyData[1],
                  l_pabyData[2],
                  l_pabyData[3],
                  l_pabyData[4],
                  l_pabyData[5],
                  l_pabyData[6],
                  l_pabyData[7],
                  l_pabyData[8]);
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */

    OGRwkbGeometryType eGeometryType = wkbUnknown;
    const OGRErr err =
        OGRReadWKBGeometryType( l_pabyData, eWkbVariant, &eGeometryType );

    if( err != OGRERR_NONE )
        return err;

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry of the appropriate type, and             */
/*      initialize from the input stream.                               */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = createGeometry( eGeometryType );

    if( poGeom == nullptr )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Import from binary.                                             */
/* -------------------------------------------------------------------- */
    const OGRErr eErr = poGeom->importFromWkb( l_pabyData, nBytes, eWkbVariant,
                                               nBytesConsumedOut );
    if( eErr != OGRERR_NONE )
    {
        delete poGeom;
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */

    if( poGeom->hasCurveGeometry() &&
        CPLTestBool(CPLGetConfigOption("OGR_STROKE_CURVE", "FALSE")) )
    {
        OGRGeometry* poNewGeom = poGeom->getLinearGeometry();
        delete poGeom;
        poGeom = poNewGeom;
    }
    poGeom->assignSpatialReference( poSR );
    *ppoReturn = poGeom;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGR_G_CreateFromWkb()                         */
/************************************************************************/
/**
 * \brief Create a geometry object of the appropriate type from its
 * well known binary representation.
 *
 * Note that if nBytes is passed as zero, no checking can be done on whether
 * the pabyData is sufficient.  This can result in a crash if the input
 * data is corrupt.  This function returns no indication of the number of
 * bytes from the data source actually used to represent the returned
 * geometry object.  Use OGR_G_WkbSize() on the returned geometry to
 * establish the number of bytes it required in WKB format.
 *
 * The OGRGeometryFactory::createFromWkb() CPP method is the same as this
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

OGRErr CPL_DLL OGR_G_CreateFromWkb( const void *pabyData,
                                    OGRSpatialReferenceH hSRS,
                                    OGRGeometryH *phGeometry,
                                    int nBytes )

{
    return OGRGeometryFactory::createFromWkb(
        pabyData,
        OGRSpatialReference::FromHandle(hSRS),
        reinterpret_cast<OGRGeometry **>(phGeometry),
        nBytes );
}

/************************************************************************/
/*                           createFromWkt()                            */
/************************************************************************/

/**
 * \brief Create a geometry object of the appropriate type from its
 * well known text representation.
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
 * \code{.cpp}
 *    const char* wkt= "POINT(0 0)";
 *
 *    // cast because OGR_G_CreateFromWkt will move the pointer
 *    char* pszWkt = (char*) wkt;
 *    OGRSpatialReferenceH ref = OSRNewSpatialReference(NULL);
 *    OGRGeometryH new_geom;
 *    OSRSetAxisMappingStrategy(poSR, OAMS_TRADITIONAL_GIS_ORDER);
 *    OGRErr err = OGR_G_CreateFromWkt(&pszWkt, ref, &new_geom);
 * \endcode
 *
 *
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromWkt(const char **ppszData,
                                         OGRSpatialReference * poSR,
                                         OGRGeometry **ppoReturn )

{
    const char *pszInput = *ppszData;
    *ppoReturn = nullptr;

/* -------------------------------------------------------------------- */
/*      Get the first token, which should be the geometry type.         */
/* -------------------------------------------------------------------- */
    char szToken[OGR_WKT_TOKEN_MAX] = {};
    if( OGRWktReadToken( pszInput, szToken ) == nullptr )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry of the appropriate type.                 */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = nullptr;
    if( STARTS_WITH_CI(szToken, "POINT") )
    {
        poGeom = new OGRPoint();
    }
    else if( STARTS_WITH_CI(szToken, "LINESTRING") )
    {
        poGeom = new OGRLineString();
    }
    else if( STARTS_WITH_CI(szToken, "POLYGON") )
    {
        poGeom = new OGRPolygon();
    }
    else if( STARTS_WITH_CI(szToken,"TRIANGLE") )
    {
        poGeom = new OGRTriangle();
    }
    else if( STARTS_WITH_CI(szToken, "GEOMETRYCOLLECTION") )
    {
        poGeom = new OGRGeometryCollection();
    }
    else if( STARTS_WITH_CI(szToken, "MULTIPOLYGON") )
    {
        poGeom = new OGRMultiPolygon();
    }
    else if( STARTS_WITH_CI(szToken, "MULTIPOINT") )
    {
        poGeom = new OGRMultiPoint();
    }
    else if( STARTS_WITH_CI(szToken, "MULTILINESTRING") )
    {
        poGeom = new OGRMultiLineString();
    }
    else if( STARTS_WITH_CI(szToken, "CIRCULARSTRING") )
    {
        poGeom = new OGRCircularString();
    }
    else if( STARTS_WITH_CI(szToken, "COMPOUNDCURVE") )
    {
        poGeom = new OGRCompoundCurve();
    }
    else if( STARTS_WITH_CI(szToken, "CURVEPOLYGON") )
    {
        poGeom = new OGRCurvePolygon();
    }
    else if( STARTS_WITH_CI(szToken, "MULTICURVE") )
    {
        poGeom = new OGRMultiCurve();
    }
    else if( STARTS_WITH_CI(szToken, "MULTISURFACE") )
    {
        poGeom = new OGRMultiSurface();
    }

    else if( STARTS_WITH_CI(szToken,"POLYHEDRALSURFACE") )
    {
        poGeom = new OGRPolyhedralSurface();
    }

    else if( STARTS_WITH_CI(szToken,"TIN") )
    {
        poGeom = new OGRTriangulatedSurface();
    }

    else
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

/* -------------------------------------------------------------------- */
/*      Do the import.                                                  */
/* -------------------------------------------------------------------- */
    const OGRErr eErr = poGeom->importFromWkt( &pszInput );

/* -------------------------------------------------------------------- */
/*      Assign spatial reference system.                                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        if( poGeom->hasCurveGeometry() &&
            CPLTestBool(CPLGetConfigOption("OGR_STROKE_CURVE", "FALSE")) )
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

/**
 * \brief Create a geometry object of the appropriate type from its
 * well known text representation.
 *
 * The C function OGR_G_CreateFromWkt() is the same as this method.
 *
 * @param pszData input zero terminated string containing well known text
 *                representation of the geometry to be created.
 * @param poSR pointer to the spatial reference to be assigned to the
 *             created geometry object.  This may be NULL.
 * @param ppoReturn the newly created geometry object will be assigned to the
 *                  indicated pointer on return.  This will be NULL if the
 *                  method fails. If not NULL, *ppoReturn should be freed with
 *                  OGRGeometryFactory::destroyGeometry() after use.

 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 * @since GDAL 2.3
 */

OGRErr OGRGeometryFactory::createFromWkt(const char* pszData,
                                         OGRSpatialReference * poSR,
                                         OGRGeometry **ppoReturn )

{
    return createFromWkt(&pszData, poSR, ppoReturn);
}

/************************************************************************/
/*                        OGR_G_CreateFromWkt()                         */
/************************************************************************/
/**
 * \brief Create a geometry object of the appropriate type from its well known
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
    return OGRGeometryFactory::createFromWkt(
        const_cast<const char**>(ppszData),
        reinterpret_cast<OGRSpatialReference *>(hSRS),
        reinterpret_cast<OGRGeometry **>(phGeometry));
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
          return new (std::nothrow) OGRPoint();

      case wkbLineString:
          return new (std::nothrow) OGRLineString();

      case wkbPolygon:
          return new (std::nothrow) OGRPolygon();

      case wkbGeometryCollection:
          return new (std::nothrow) OGRGeometryCollection();

      case wkbMultiPolygon:
          return new (std::nothrow) OGRMultiPolygon();

      case wkbMultiPoint:
          return new (std::nothrow) OGRMultiPoint();

      case wkbMultiLineString:
          return new (std::nothrow) OGRMultiLineString();

      case wkbLinearRing:
          return new (std::nothrow) OGRLinearRing();

      case wkbCircularString:
          return new (std::nothrow) OGRCircularString();

      case wkbCompoundCurve:
          return new (std::nothrow) OGRCompoundCurve();

      case wkbCurvePolygon:
          return new (std::nothrow) OGRCurvePolygon();

      case wkbMultiCurve:
          return new (std::nothrow) OGRMultiCurve();

      case wkbMultiSurface:
          return new (std::nothrow) OGRMultiSurface();

      case wkbTriangle:
          return new (std::nothrow) OGRTriangle();

      case wkbPolyhedralSurface:
          return new (std::nothrow) OGRPolyhedralSurface();

      case wkbTIN:
          return new (std::nothrow) OGRTriangulatedSurface();

      default:
          return nullptr;
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
 * @return handle to the newly create geometry or NULL on failure. Should be
 *         freed with OGR_G_DestroyGeometry() after use.
 */

OGRGeometryH OGR_G_CreateGeometry( OGRwkbGeometryType eGeometryType )

{
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::createGeometry(eGeometryType));
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
    OGRGeometryFactory::destroyGeometry(reinterpret_cast<OGRGeometry *>(hGeom));
}

/************************************************************************/
/*                           forceToPolygon()                           */
/************************************************************************/

/**
 * \brief Convert to polygon.
 *
 * Tries to force the provided geometry to be a polygon. This effects a change
 * on multipolygons.
 * Starting with GDAL 2.0, curve polygons or closed curves will be changed to
 * polygons.  The passed in geometry is consumed and a new one returned (or
 * potentially the same one).
 *
 * Note: the resulting polygon may break the Simple Features rules for polygons,
 * for example when converting from a multi-part multipolygon.
 *
 * @param poGeom the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToPolygon( OGRGeometry *poGeom )

{
    if( poGeom == nullptr )
        return nullptr;

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

    if( eGeomType == wkbCurvePolygon )
    {
        OGRCurvePolygon *poCurve = poGeom->toCurvePolygon();

        if( !poGeom->hasCurveGeometry(TRUE) )
            return OGRSurface::CastToPolygon(poCurve);

        OGRPolygon* poPoly = poCurve->CurvePolyToPoly();
        delete poGeom;
        return poPoly;
    }

    // base polygon or triangle
    if( OGR_GT_IsSubClassOf( eGeomType, wkbPolygon ) )
    {
        return OGRSurface::CastToPolygon(poGeom->toSurface());
    }

    if( OGR_GT_IsCurve(eGeomType) )
    {
        OGRCurve* poCurve = poGeom->toCurve();
        if( poCurve->getNumPoints() >= 3 &&
            poCurve->get_IsClosed() )
        {
            OGRPolygon *poPolygon = new OGRPolygon();
            poPolygon->assignSpatialReference(poGeom->getSpatialReference());

            if( !poGeom->hasCurveGeometry(TRUE) )
            {
                poPolygon->addRingDirectly(
                    OGRCurve::CastToLinearRing(poCurve ));
            }
            else
            {
                OGRLineString* poLS = poCurve->CurveToLine();
                poPolygon->addRingDirectly( OGRCurve::CastToLinearRing(poLS) );
                delete poGeom;
            }
            return poPolygon;
        }
    }

    if( OGR_GT_IsSubClassOf(eGeomType, wkbPolyhedralSurface) )
    {
        OGRPolyhedralSurface* poPS = poGeom->toPolyhedralSurface();
        if( poPS->getNumGeometries() == 1 )
        {
            poGeom = OGRSurface::CastToPolygon(
              poPS->getGeometryRef(0)->clone()->toSurface());
            delete poPS;
            return poGeom;
        }
    }

    if( eGeomType != wkbGeometryCollection
        && eGeomType != wkbMultiPolygon
        && eGeomType != wkbMultiSurface )
        return poGeom;

    // Build an aggregated polygon from all the polygon rings in the container.
    OGRPolygon *poPolygon = new OGRPolygon();
    OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
    if( poGeom->hasCurveGeometry() )
    {
        OGRGeometryCollection *poNewGC =
            poGC->getLinearGeometry()->toGeometryCollection();
        delete poGC;
        poGeom = poNewGC;
        poGC = poNewGC;
    }

    poPolygon->assignSpatialReference(poGeom->getSpatialReference());

    for( int iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
    {
        if( wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType())
            != wkbPolygon )
            continue;

        OGRPolygon *poOldPoly = poGC->getGeometryRef(iGeom)->toPolygon();

        if( poOldPoly->getExteriorRing() == nullptr )
            continue;

        poPolygon->addRingDirectly( poOldPoly->stealExteriorRing() );

        for( int iRing = 0; iRing < poOldPoly->getNumInteriorRings(); iRing++ )
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceToPolygon(
            reinterpret_cast<OGRGeometry *>(hGeom)));
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
    if( poGeom == nullptr )
        return nullptr;

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
    if( eGeomType == wkbMultiSurface )
    {
        OGRMultiSurface* poMS = poGeom->toMultiSurface();
        if( !poMS->hasCurveGeometry(TRUE) )
        {
            return OGRMultiSurface::CastToMultiPolygon(poMS);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for the case of a geometrycollection that can be          */
/*      promoted to MultiPolygon.                                       */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbGeometryCollection ||
        eGeomType == wkbMultiSurface )
    {
        bool bAllPoly = true;
        OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
        if( poGeom->hasCurveGeometry() )
        {
            OGRGeometryCollection *poNewGC =
                poGC->getLinearGeometry()->toGeometryCollection();
            delete poGC;
            poGeom = poNewGC;
            poGC = poNewGC;
        }

        bool bCanConvertToMultiPoly = true;
        for( int iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            OGRwkbGeometryType eSubGeomType =
                wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
            if( eSubGeomType != wkbPolygon )
                bAllPoly = false;
            if( eSubGeomType != wkbMultiPolygon && eSubGeomType != wkbPolygon &&
                eSubGeomType != wkbPolyhedralSurface && eSubGeomType != wkbTIN )
            {
                bCanConvertToMultiPoly = false;
            }
        }

        if( !bCanConvertToMultiPoly )
            return poGeom;

        OGRMultiPolygon *poMP = new OGRMultiPolygon();
        poMP->assignSpatialReference(poGeom->getSpatialReference());

        while( poGC->getNumGeometries() > 0 )
        {
            OGRGeometry* poSubGeom = poGC->getGeometryRef(0);
            poGC->removeGeometry( 0, FALSE );
            if( bAllPoly )
            {
                poMP->addGeometryDirectly( poSubGeom );
            }
            else
            {
                poSubGeom = forceToMultiPolygon( poSubGeom );
                OGRMultiPolygon* poSubMP = poSubGeom->toMultiPolygon();
                while( poSubMP != nullptr && poSubMP->getNumGeometries() > 0 )
                {
                    poMP->addGeometryDirectly( poSubMP->getGeometryRef(0) );
                    poSubMP->removeGeometry( 0, FALSE );
                }
                delete poSubMP;
            }
        }

        delete poGC;

        return poMP;
    }

    if( eGeomType == wkbCurvePolygon )
    {
        OGRPolygon* poPoly = poGeom->toCurvePolygon()->CurvePolyToPoly();
        OGRMultiPolygon *poMP = new OGRMultiPolygon();
        poMP->assignSpatialReference(poGeom->getSpatialReference());
        poMP->addGeometryDirectly( poPoly );
        delete poGeom;
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If it is PolyhedralSurface or TIN, then pretend it is a         */
/*      multipolygon.                                                   */
/* -------------------------------------------------------------------- */
    if( OGR_GT_IsSubClassOf(eGeomType, wkbPolyhedralSurface) )
    {
        return OGRPolyhedralSurface::CastToMultiPolygon(
                                            poGeom->toPolyhedralSurface());
    }

    if( eGeomType == wkbTriangle )
    {
        return forceToMultiPolygon( forceToPolygon( poGeom ) );
    }

/* -------------------------------------------------------------------- */
/*      Eventually we should try to split the polygon into component    */
/*      island polygons.  But that is a lot of work and can be put off. */
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceToMultiPolygon(
            reinterpret_cast<OGRGeometry *>(hGeom)));
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
    if( poGeom == nullptr )
        return nullptr;

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
        OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
        for( auto& poMember: poGC )
        {
            if( wkbFlatten(poMember->getGeometryType()) != wkbPoint )
                return poGeom;
        }

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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceToMultiPoint(
            reinterpret_cast<OGRGeometry *>(hGeom)));
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
 * - curvepolygons will be approximated and changed to a collection of
 ( linestrings (one per ring).
 *
 * The passed in geometry is
 * consumed and a new one returned (or potentially the same one).
 *
 * @return new geometry.
 */

OGRGeometry *OGRGeometryFactory::forceToMultiLineString( OGRGeometry *poGeom )

{
    if( poGeom == nullptr )
        return nullptr;

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
        OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
        if( poGeom->hasCurveGeometry() )
        {
            OGRGeometryCollection *poNewGC = poGC->getLinearGeometry()->
                toGeometryCollection();
            delete poGC;
            poGeom = poNewGC;
            poGC = poNewGC;
        }

        for( auto&& poMember: poGC )
        {
            if( wkbFlatten(poMember->getGeometryType()) != wkbLineString )
            {
                return poGeom;
            }
        }

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
    if( OGR_GT_IsSubClassOf(eGeomType, wkbCurvePolygon ) )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRPolygon *poPoly = nullptr;
        if( OGR_GT_IsSubClassOf(eGeomType, wkbPolygon) )
            poPoly = poGeom->toPolygon();
        else
        {
            poPoly = poGeom->toCurvePolygon()->CurvePolyToPoly();
            delete poGeom;
            poGeom = poPoly;
        }

        poMP->assignSpatialReference(poGeom->getSpatialReference());

        for( int iRing = 0; iRing < poPoly->getNumInteriorRings()+1; iRing++ )
        {
            OGRLineString *poNewLS, *poLR;

            if( iRing == 0 )
            {
                poLR = poPoly->getExteriorRing();
                if( poLR == nullptr )
                    break;
            }
            else
                poLR = poPoly->getInteriorRing(iRing-1);

            if( poLR == nullptr || poLR->getNumPoints() == 0 )
                continue;

            poNewLS = new OGRLineString();
            poNewLS->addSubLineString( poLR );
            poMP->addGeometryDirectly( poNewLS );
        }

        delete poPoly;

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If it is PolyhedralSurface or TIN, then pretend it is a         */
/*      multipolygon.                                                   */
/* -------------------------------------------------------------------- */
    if( OGR_GT_IsSubClassOf(eGeomType, wkbPolyhedralSurface) )
    {
        poGeom = forceToMultiPolygon(poGeom);
        assert(poGeom);
        eGeomType = wkbMultiPolygon;
    }

/* -------------------------------------------------------------------- */
/*      Convert multi-polygons into a multilinestring.                  */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiPolygon || eGeomType == wkbMultiSurface )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        OGRMultiPolygon *poMPoly = nullptr;
        if( eGeomType == wkbMultiPolygon )
            poMPoly = poGeom->toMultiPolygon();
        else
        {
            poMPoly = poGeom->getLinearGeometry()->toMultiPolygon();
            delete poGeom;
            poGeom = poMPoly;
        }

        assert(poGeom);
        poMP->assignSpatialReference(poGeom->getSpatialReference());

        for( auto&& poPoly: poMPoly )
        {
            for( auto&& poLR: poPoly )
            {
                if( poLR->IsEmpty() )
                    continue;

                OGRLineString* poNewLS = new OGRLineString();
                poNewLS->addSubLineString( poLR );
                poMP->addGeometryDirectly( poNewLS );
            }
        }
        delete poMPoly;

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If it is a curve line, approximate it and wrap in a multilinestring */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbCircularString ||
        eGeomType == wkbCompoundCurve )
    {
        OGRMultiLineString *poMP = new OGRMultiLineString();
        poMP->assignSpatialReference(poGeom->getSpatialReference());
        poMP->addGeometryDirectly( poGeom->toCurve()->CurveToLine() );
        delete poGeom;
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      If this is already a MultiCurve with compatible content,        */
/*      just cast                                                       */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiCurve &&
        !poGeom->toMultiCurve()->hasCurveGeometry(TRUE) )
    {
        return OGRMultiCurve::CastToMultiLineString(poGeom->toMultiCurve());
    }

/* -------------------------------------------------------------------- */
/*      If it is a multicurve, call getLinearGeometry()                */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbMultiCurve )
    {
        OGRGeometry* poNewGeom = poGeom->getLinearGeometry();
        CPLAssert( wkbFlatten(poNewGeom->getGeometryType()) ==
                   wkbMultiLineString );
        delete poGeom;
        return poNewGeom->toMultiLineString();
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceToMultiLineString(
            reinterpret_cast<OGRGeometry *>(hGeom)));
}

/************************************************************************/
/*                      removeLowerDimensionSubGeoms()                  */
/************************************************************************/

/** \brief Remove sub-geometries from a geometry collection that do not have
 *         the maximum topological dimensionality of the collection.
 *
 * This is typically to be used as a cleanup phase after running OGRGeometry::MakeValid()
 *
 * For example, MakeValid() on a polygon can return a geometry collection of
 * polygons and linestrings. Calling this method will return either a polygon
 * or multipolygon by dropping those linestrings.
 *
 * On a non-geometry collection, this will return a clone of the passed geometry.
 *
 * @param poGeom input geometry
 * @return a new geometry.
 *
 * @since GDAL 3.1.0
 */
OGRGeometry* OGRGeometryFactory::removeLowerDimensionSubGeoms( const OGRGeometry* poGeom )
{
    if( poGeom == nullptr )
        return nullptr;
    if( wkbFlatten(poGeom->getGeometryType()) != wkbGeometryCollection ||
        poGeom->IsEmpty() )
    {
        return poGeom->clone();
    }
    const OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
    int nMaxDim = 0;
    OGRBoolean bHasCurve = FALSE;
    for( const auto poSubGeom: *poGC )
    {
        nMaxDim = std::max(nMaxDim, poSubGeom->getDimension());
        bHasCurve |= poSubGeom->hasCurveGeometry();
    }
    int nCountAtMaxDim = 0;
    const OGRGeometry* poGeomAtMaxDim = nullptr;
    for( const auto poSubGeom: *poGC )
    {
        if( poSubGeom->getDimension() == nMaxDim )
        {
            poGeomAtMaxDim = poSubGeom;
            nCountAtMaxDim ++;
        }
    }
    if( nCountAtMaxDim == 1 && poGeomAtMaxDim != nullptr )
    {
        return poGeomAtMaxDim->clone();
    }
    OGRGeometryCollection* poRet =
        (nMaxDim == 0) ?               static_cast<OGRGeometryCollection*>(new OGRMultiPoint()) :
        (nMaxDim == 1 && !bHasCurve) ? static_cast<OGRGeometryCollection*>(new OGRMultiLineString()) :
        (nMaxDim == 1 && bHasCurve) ?  static_cast<OGRGeometryCollection*>(new OGRMultiCurve()) :
        (nMaxDim == 2 && !bHasCurve) ? static_cast<OGRGeometryCollection*>(new OGRMultiPolygon()) :
                                       static_cast<OGRGeometryCollection*>(new OGRMultiSurface());
    for( const auto poSubGeom: *poGC )
    {
        if( poSubGeom->getDimension() == nMaxDim )
        {
            if( OGR_GT_IsSubClassOf(poSubGeom->getGeometryType(), wkbGeometryCollection) )
            {
                const OGRGeometryCollection* poSubGeomAsGC = poSubGeom->toGeometryCollection();
                for( const auto poSubSubGeom: *poSubGeomAsGC )
                {
                    if( poSubSubGeom->getDimension() == nMaxDim )
                    {
                        poRet->addGeometryDirectly(poSubSubGeom->clone());
                    }
                }
            }
            else
            {
                poRet->addGeometryDirectly(poSubGeom->clone());
            }
        }
    }
    return poRet;
}

/************************************************************************/
/*                  OGR_G_RemoveLowerDimensionSubGeoms()                */
/************************************************************************/

/** \brief Remove sub-geometries from a geometry collection that do not have
 *         the maximum topological dimensionality of the collection.
 *
 * This function is the same as the C++ method
 * OGRGeometryFactory::removeLowerDimensionSubGeoms().
 *
 * @param hGeom handle to the geometry to convert
 * @return a new geometry.
 *
 * @since GDAL 3.1.0
 */

OGRGeometryH OGR_G_RemoveLowerDimensionSubGeoms( const OGRGeometryH hGeom )

{
    return OGRGeometry::ToHandle(
        OGRGeometryFactory::removeLowerDimensionSubGeoms(
            OGRGeometry::FromHandle(hGeom)));
}

/************************************************************************/
/*                          organizePolygons()                          */
/************************************************************************/

struct sPolyExtended
{
    CPL_DISALLOW_COPY_ASSIGN(sPolyExtended)
    sPolyExtended() = default;
    sPolyExtended(sPolyExtended&&) = default;
    sPolyExtended& operator= (sPolyExtended&&) = default;

    OGRGeometry* poGeometry = nullptr;
    OGRCurvePolygon* poPolygon = nullptr;
    OGREnvelope     sEnvelope{};
    OGRCurve*  poExteriorRing = nullptr;
    OGRPoint        poAPoint{};
    int             nInitialIndex = 0;
    OGRCurvePolygon*     poEnclosingPolygon = nullptr;
    double          dfArea = 0.0;
    bool            bIsTopLevel = false;
    bool            bIsCW = false;
    bool            bIsPolygon = false;
};

static bool OGRGeometryFactoryCompareArea(const sPolyExtended& sPoly1, const sPolyExtended& sPoly2)
{
    return sPoly2.dfArea < sPoly1.dfArea;
}

static bool OGRGeometryFactoryCompareByIndex(const sPolyExtended& sPoly1, const sPolyExtended& sPoly2)
{
    return sPoly1.nInitialIndex < sPoly2.nInitialIndex;
}

constexpr int N_CRITICAL_PART_NUMBER = 100;

enum OrganizePolygonMethod
{
   METHOD_NORMAL,
   METHOD_SKIP,
   METHOD_ONLY_CCW,
   METHOD_CCW_INNER_JUST_AFTER_CW_OUTER
};

/**
 * \brief Organize polygons based on geometries.
 *
 * Analyse a set of rings (passed as simple polygons), and based on a
 * geometric analysis convert them into a polygon with inner rings,
 * (or a MultiPolygon if dealing with more than one polygon) that follow the
 * OGC Simple Feature specification.
 *
 * All the input geometries must be OGRPolygon/OGRCurvePolygon with only a valid exterior
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
 * the value of the METHOD option of papszOptions (useful to modify the behavior of the
 * shapefile driver)
 *
 * @param papoPolygons array of geometry pointers - should all be OGRPolygons.
 * Ownership of the geometries is passed, but not of the array itself.
 * @param nPolygonCount number of items in papoPolygons
 * @param pbIsValidGeometry value will be set TRUE if result is valid or
 * FALSE otherwise.
 * @param papszOptions a list of strings for passing options
 *
 * @return a single resulting geometry (either OGRPolygon, OGRCurvePolygon,
 * OGRMultiPolygon, OGRMultiSurface or OGRGeometryCollection). Returns a
 * POLYGON EMPTY in the case of nPolygonCount being 0.
 */

OGRGeometry* OGRGeometryFactory::organizePolygons( OGRGeometry **papoPolygons,
                                                   int nPolygonCount,
                                                   int *pbIsValidGeometry,
                                                   const char** papszOptions )
{
    if( nPolygonCount == 0 )
    {
        if( pbIsValidGeometry )
            *pbIsValidGeometry = TRUE;

        return new OGRPolygon();
    }

    OGRGeometry* geom = nullptr;
    OrganizePolygonMethod method = METHOD_NORMAL;
    bool bHasCurves = false;

/* -------------------------------------------------------------------- */
/*      Trivial case of a single polygon.                               */
/* -------------------------------------------------------------------- */
    if( nPolygonCount == 1 )
    {
        geom = papoPolygons[0];
        papoPolygons[0] = nullptr;

        if( pbIsValidGeometry )
            *pbIsValidGeometry = TRUE;

        return geom;
    }

    bool bUseFastVersion = true;
    if( CPLTestBool(CPLGetConfigOption("OGR_DEBUG_ORGANIZE_POLYGONS",
                                       "NO")) )
    {
        /* ------------------------------------------------------------------ */
        /*      A wee bit of a warning.                                       */
        /* ------------------------------------------------------------------ */
        static int firstTime = 1;
        // cppcheck-suppress knownConditionTrueFalse
        if( !haveGEOS() && firstTime )
        {
            CPLDebug(
                "OGR",
                "In OGR_DEBUG_ORGANIZE_POLYGONS mode, GDAL should be built "
                "with GEOS support enabled in order "
                "OGRGeometryFactory::organizePolygons to provide reliable "
                "results on complex polygons.");
            firstTime = 0;
        }
        // cppcheck-suppress knownConditionTrueFalse
        bUseFastVersion = !haveGEOS();
    }

/* -------------------------------------------------------------------- */
/*      Setup per polygon envelope and area information.                */
/* -------------------------------------------------------------------- */
    std::vector<sPolyExtended> asPolyEx(nPolygonCount);

    bool bValidTopology = true;
    bool bMixedUpGeometries = false;
    bool bNonPolygon = false;
    bool bFoundCCW = false;

    const char* pszMethodValue =
        CSLFetchNameValue( papszOptions, "METHOD" );
    const char* pszMethodValueOption =
        CPLGetConfigOption("OGR_ORGANIZE_POLYGONS", nullptr);
    if( pszMethodValueOption != nullptr && pszMethodValueOption[0] != '\0' )
        pszMethodValue = pszMethodValueOption;

    if( pszMethodValue != nullptr )
    {
        if( EQUAL(pszMethodValue, "SKIP") )
        {
            method = METHOD_SKIP;
            bMixedUpGeometries = true;
        }
        else if( EQUAL(pszMethodValue, "ONLY_CCW") )
        {
            method = METHOD_ONLY_CCW;
        }
        else if( EQUAL(pszMethodValue, "CCW_INNER_JUST_AFTER_CW_OUTER") )
        {
            method = METHOD_CCW_INNER_JUST_AFTER_CW_OUTER;
        }
        else if( !EQUAL(pszMethodValue, "DEFAULT") )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized value for METHOD option : %s",
                     pszMethodValue);
        }
    }

    int nCountCWPolygon = 0;
    int indexOfCWPolygon = -1;

    for( int i = 0; i < nPolygonCount; i++ )
    {
        asPolyEx[i].nInitialIndex = i;
        asPolyEx[i].poGeometry = papoPolygons[i];
        asPolyEx[i].poPolygon = papoPolygons[i]->toCurvePolygon();
        papoPolygons[i]->getEnvelope(&asPolyEx[i].sEnvelope);

        OGRwkbGeometryType eType =
            wkbFlatten(papoPolygons[i]->getGeometryType());
        if( eType == wkbCurvePolygon )
            bHasCurves = true;
        if( asPolyEx[i].poPolygon != nullptr
            && !asPolyEx[i].poPolygon->IsEmpty()
            && asPolyEx[i].poPolygon->getNumInteriorRings() == 0
            && asPolyEx[i].poPolygon->
                getExteriorRingCurve()->getNumPoints() >= 4)
        {
            if( method != METHOD_CCW_INNER_JUST_AFTER_CW_OUTER )
                asPolyEx[i].dfArea = asPolyEx[i].poPolygon->get_Area();
            asPolyEx[i].poExteriorRing =
                asPolyEx[i].poPolygon->getExteriorRingCurve();
            asPolyEx[i].poExteriorRing->StartPoint(&asPolyEx[i].poAPoint);
            if( eType == wkbPolygon )
            {
                asPolyEx[i].bIsCW =
                    CPL_TO_BOOL(asPolyEx[i].poExteriorRing->
                                        toLinearRing()->isClockwise());
                asPolyEx[i].bIsPolygon = true;
            }
            else
            {
                OGRLineString* poLS = asPolyEx[i].poExteriorRing->CurveToLine();
                OGRLinearRing oLR;
                oLR.addSubLineString(poLS);
                asPolyEx[i].bIsCW = CPL_TO_BOOL(oLR.isClockwise());
                asPolyEx[i].bIsPolygon = false;
                delete poLS;
            }
            if( asPolyEx[i].bIsCW )
            {
                indexOfCWPolygon = i;
                nCountCWPolygon++;
            }
            if( !bFoundCCW )
                bFoundCCW = !(asPolyEx[i].bIsCW);
        }
        else
        {
            if( !bMixedUpGeometries )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "organizePolygons() received an unexpected geometry.  "
                    "Either a polygon with interior rings, or a polygon "
                    "with less than 4 points, or a non-Polygon geometry.  "
                    "Return arguments as a collection." );
                bMixedUpGeometries = true;
            }
            if( eType != wkbPolygon && eType != wkbCurvePolygon )
                bNonPolygon = true;
        }
    }

    // If we are in ONLY_CCW mode and that we have found that there is only one
    // outer ring, then it is pretty easy : we can assume that all other rings
    // are inside.
    if( (method == METHOD_ONLY_CCW ||
         method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER) &&
        nCountCWPolygon == 1 && bUseFastVersion && !bNonPolygon )
    {
        OGRCurvePolygon* poCP = asPolyEx[indexOfCWPolygon].poPolygon;
        for( int i = 0; i < nPolygonCount; i++ )
        {
            if( i != indexOfCWPolygon )
            {
                poCP->addRingDirectly(
                    asPolyEx[i].poPolygon->stealExteriorRingCurve());
                delete asPolyEx[i].poPolygon;
            }
        }

        if( pbIsValidGeometry )
            *pbIsValidGeometry = TRUE;
        return poCP;
    }

    if( method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER &&
        !bNonPolygon && asPolyEx[0].bIsCW )
    {
        // Inner rings are CCW oriented and follow immediately the outer
        // ring (that is CW oriented) in which they are included.
        OGRMultiSurface* poMulti = nullptr;
        OGRCurvePolygon* poCur = asPolyEx[0].poPolygon;
        OGRGeometry* poRet = poCur;
        // We have already checked that the first ring is CW.
        OGREnvelope* psEnvelope = &(asPolyEx[0].sEnvelope);
        for( int i = 1; i < nPolygonCount; i++ )
        {
            if( asPolyEx[i].bIsCW )
            {
                if( poMulti == nullptr )
                {
                    if( bHasCurves )
                        poMulti = new OGRMultiSurface();
                    else
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
                poCur->addRingDirectly(
                    asPolyEx[i].poPolygon->stealExteriorRingCurve());
                if( !(asPolyEx[i].poAPoint.getX() >= psEnvelope->MinX &&
                      asPolyEx[i].poAPoint.getX() <= psEnvelope->MaxX &&
                      asPolyEx[i].poAPoint.getY() >= psEnvelope->MinY &&
                      asPolyEx[i].poAPoint.getY() <= psEnvelope->MaxY) )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Part %d does not respect "
                             "CCW_INNER_JUST_AFTER_CW_OUTER rule",
                             i);
                }
                delete asPolyEx[i].poPolygon;
            }
        }

        if( pbIsValidGeometry )
            *pbIsValidGeometry = TRUE;
        return poRet;
    }
    else if( method == METHOD_CCW_INNER_JUST_AFTER_CW_OUTER && !bNonPolygon )
    {
        method = METHOD_ONLY_CCW;
        for( int i = 0; i < nPolygonCount; i++ )
            asPolyEx[i].dfArea = asPolyEx[i].poPolygon->get_Area();
    }

    // Emits a warning if the number of parts is sufficiently big to anticipate
    // for very long computation time, and the user didn't specify an explicit
    // method.
    if( nPolygonCount > N_CRITICAL_PART_NUMBER &&
        method == METHOD_NORMAL && pszMethodValue == nullptr )
    {
        static int firstTime = 1;
        if( firstTime )
        {
            if( bFoundCCW )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "organizePolygons() received a polygon with more than %d "
                    "parts. The processing may be really slow.  "
                    "You can skip the processing by setting METHOD=SKIP, "
                    "or only make it analyze counter-clock wise parts by "
                    "setting METHOD=ONLY_CCW if you can assume that the "
                    "outline of holes is counter-clock wise defined",
                    N_CRITICAL_PART_NUMBER);
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "organizePolygons() received a polygon with more than %d "
                    "parts.  The processing may be really slow.  "
                    "You can skip the processing by setting METHOD=SKIP.",
                    N_CRITICAL_PART_NUMBER);
            }
            firstTime = 0;
        }
    }

    /* This a nulti-step algorithm :
       1) Sort polygons by descending areas
       2) For each polygon of rank i, find its smallest enclosing polygon
          among the polygons of rank [i-1 ... 0]. If there are no such polygon,
          this is a top-level polygon. Otherwise, depending on if the enclosing
          polygon is top-level or not, we can decide if we are top-level or not
       3) Re-sort the polygons to retrieve their initial order (nicer for
          some applications)
       4) For each non top-level polygon (= inner ring), add it to its
          outer ring
       5) Add the top-level polygons to the multipolygon

       Complexity : O(nPolygonCount^2)
    */

    /* Compute how each polygon relate to the other ones
       To save a bit of computation we always begin the computation by a test
       on the envelope. We also take into account the areas to avoid some
       useless tests.  (A contains B implies envelop(A) contains envelop(B)
       and area(A) > area(B)) In practice, we can hope that few full geometry
       intersection of inclusion test is done:
       * if the polygons are well separated geographically (a set of islands
       for example), no full geometry intersection or inclusion test is done.
       (the envelopes don't intersect each other)

       * if the polygons are 'lake inside an island inside a lake inside an
       area' and that each polygon is much smaller than its enclosing one,
       their bounding boxes are strictly contained into each other, and thus,
       no full geometry intersection or inclusion test is done
    */

    if( !bMixedUpGeometries )
    {
        // STEP 1: Sort polygons by descending area.
        std::sort(asPolyEx.begin(), asPolyEx.end(),
                  OGRGeometryFactoryCompareArea);
    }
    papoPolygons = nullptr;  // Just to use to avoid it afterwards.

/* -------------------------------------------------------------------- */
/*      Compute relationships, if things seem well structured.          */
/* -------------------------------------------------------------------- */

    // The first (largest) polygon is necessarily top-level.
    asPolyEx[0].bIsTopLevel = true;
    asPolyEx[0].poEnclosingPolygon = nullptr;

    int nCountTopLevel = 1;

    // STEP 2.
    for( int i = 1;
         !bMixedUpGeometries && bValidTopology && i<nPolygonCount;
         i++ )
    {
        if( method == METHOD_ONLY_CCW && asPolyEx[i].bIsCW )
        {
            nCountTopLevel++;
            asPolyEx[i].bIsTopLevel = true;
            asPolyEx[i].poEnclosingPolygon = nullptr;
            continue;
        }

        int j = i - 1;  // Used after for.
        for( ; bValidTopology && j >= 0; j-- )
        {
            bool b_i_inside_j = false;

            if( method == METHOD_ONLY_CCW && asPolyEx[j].bIsCW == false )
            {
                // In that mode, i which is CCW if we reach here can only be
                // included in a CW polygon.
                continue;
            }

            if( asPolyEx[j].sEnvelope.Contains(asPolyEx[i].sEnvelope) )
            {
                if( bUseFastVersion )
                {
                    if( method == METHOD_ONLY_CCW && j == 0 )
                    {
                        // We are testing if a CCW ring is in the biggest CW
                        // ring It *must* be inside as this is the last
                        // candidate, otherwise the winding order rules is
                        // broken.
                        b_i_inside_j = true;
                    }
                    else if( asPolyEx[i].bIsPolygon &&
                             asPolyEx[j].bIsPolygon &&
                             asPolyEx[j].poExteriorRing->toLinearRing()->
                                     isPointOnRingBoundary(
                                         &asPolyEx[i].poAPoint, FALSE) )
                    {
                        OGRLinearRing* poLR_i =
                            asPolyEx[i].poExteriorRing->toLinearRing();
                        OGRLinearRing* poLR_j =
                            asPolyEx[j].poExteriorRing->toLinearRing();

                        // If the point of i is on the boundary of j, we will
                        // iterate over the other points of i.
                        const int nPoints = poLR_i->getNumPoints();
                        int k = 1;  // Used after for.
                        OGRPoint previousPoint = asPolyEx[i].poAPoint;
                        for( ; k < nPoints; k++ )
                        {
                            OGRPoint point;
                            poLR_i->getPoint(k, &point);
                            if( point.getX() == previousPoint.getX() &&
                                point.getY() == previousPoint.getY() )
                            {
                                continue;
                            }
                            if( poLR_j->isPointOnRingBoundary(&point, FALSE) )
                            {
                                // If it is on the boundary of j, iterate again.
                            }
                            else if( poLR_j->isPointInRing(&point, FALSE) )
                            {
                                // If then point is strictly included in j, then
                                // i is considered inside j.
                                b_i_inside_j = true;
                                break;
                            }
                            else
                            {
                                // If it is outside, then i cannot be inside j.
                                break;
                            }
                            previousPoint = point;
                        }
                        if( !b_i_inside_j && k == nPoints && nPoints > 2 )
                        {
                            // All points of i are on the boundary of j.
                            // Take a point in the middle of a segment of i and
                            // test it against j.
                            poLR_i->getPoint(0, &previousPoint);
                            for( k = 1; k < nPoints; k++ )
                            {
                                OGRPoint point;
                                poLR_i->getPoint(k, &point);
                                if( point.getX() == previousPoint.getX() &&
                                    point.getY() == previousPoint.getY() )
                                {
                                    continue;
                                }
                                OGRPoint pointMiddle;
                                pointMiddle.setX((point.getX() +
                                                  previousPoint.getX()) / 2);
                                pointMiddle.setY((point.getY() +
                                                  previousPoint.getY()) / 2);
                                if( poLR_j->isPointOnRingBoundary(&pointMiddle,
                                                                  FALSE) )
                                {
                                    // If it is on the boundary of j, iterate
                                    // again.
                                }
                                else if( poLR_j->isPointInRing(&pointMiddle,
                                                               FALSE) )
                                {
                                    // If then point is strictly included in j,
                                    // then i is considered inside j.
                                    b_i_inside_j = true;
                                    break;
                                }
                                else
                                {
                                    // If it is outside, then i cannot be inside
                                    // j.
                                    break;
                                }
                                previousPoint = point;
                            }
                        }
                    }
                    // Note that isPointInRing only test strict inclusion in the
                    // ring.
                    else if( asPolyEx[i].bIsPolygon &&
                             asPolyEx[j].bIsPolygon &&
                             asPolyEx[j].poExteriorRing->toLinearRing()->
                                     isPointInRing(&asPolyEx[i].poAPoint,
                                                   FALSE) )
                    {
                        b_i_inside_j = true;
                    }
                }
                else if( asPolyEx[j].poPolygon->
                             Contains(asPolyEx[i].poPolygon) )
                {
                    b_i_inside_j = true;
                }
            }

            if( b_i_inside_j )
            {
                if( asPolyEx[j].bIsTopLevel )
                {
                    // We are a lake.
                    asPolyEx[i].bIsTopLevel = false;
                    asPolyEx[i].poEnclosingPolygon = asPolyEx[j].poPolygon;
                }
                else
                {
                    // We are included in a something not toplevel (a lake),
                    // so in OGCSF we are considered as toplevel too.
                    nCountTopLevel++;
                    asPolyEx[i].bIsTopLevel = true;
                    asPolyEx[i].poEnclosingPolygon = nullptr;
                }
                break;
            }
            // Use Overlaps instead of Intersects to be more
            // tolerant about touching polygons.
            else if( bUseFastVersion ||
                     !asPolyEx[i].sEnvelope.Intersects(asPolyEx[j].sEnvelope) ||
                     !asPolyEx[i].poPolygon->Overlaps(asPolyEx[j].poPolygon) )
            {

            }
            else
            {
                // Bad... The polygons are intersecting but no one is
                // contained inside the other one. This is a really broken
                // case. We just make a multipolygon with the whole set of
                // polygons.
                bValidTopology = false;
#ifdef DEBUG
                char* wkt1 = nullptr;
                char* wkt2 = nullptr;
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

        if( j < 0 )
        {
            // We come here because we are not included in anything.
            // We are toplevel.
            nCountTopLevel++;
            asPolyEx[i].bIsTopLevel = true;
            asPolyEx[i].poEnclosingPolygon = nullptr;
        }
    }

    if( pbIsValidGeometry )
        *pbIsValidGeometry = bValidTopology && !bMixedUpGeometries;

/* -------------------------------------------------------------------- */
/*      Things broke down - just turn everything into a multipolygon.   */
/* -------------------------------------------------------------------- */
    if( !bValidTopology || bMixedUpGeometries )
    {
        OGRGeometryCollection* poGC = nullptr;
        if( bNonPolygon )
            poGC = new OGRGeometryCollection();
        else if( bHasCurves )
            poGC = new OGRMultiSurface();
        else
            poGC = new OGRMultiPolygon();
        geom = poGC;

        for( int i = 0; i < nPolygonCount; i++ )
        {
            poGC->addGeometryDirectly( asPolyEx[i].poGeometry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to turn into one or more polygons based on the ring         */
/*      relationships.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        // STEP 3: Sort again in initial order.
        std::sort(asPolyEx.begin(), asPolyEx.end(),
                  OGRGeometryFactoryCompareByIndex);

        // STEP 4: Add holes as rings of their enclosing polygon.
        for( int i = 0; i < nPolygonCount; i++ )
        {
            if( asPolyEx[i].bIsTopLevel == false )
            {
                asPolyEx[i].poEnclosingPolygon->addRingDirectly(
                    asPolyEx[i].poPolygon->stealExteriorRingCurve());
                delete asPolyEx[i].poPolygon;
            }
            else if( nCountTopLevel == 1 )
            {
                geom = asPolyEx[i].poPolygon;
            }
        }

        // STEP 5: Add toplevel polygons.
        if( nCountTopLevel > 1 )
        {
            OGRGeometryCollection* poGC = nullptr;
            for( int i = 0; i < nPolygonCount; i++ )
            {
                if( asPolyEx[i].bIsTopLevel )
                {
                    if( poGC == nullptr )
                    {
                        if( bHasCurves )
                            poGC = new OGRMultiSurface();
                        else
                            poGC = new OGRMultiPolygon();
                    }
                    poGC->addGeometryDirectly(asPolyEx[i].poPolygon);
                }
            }
            geom = poGC;
        }
    }

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
 * @return a geometry on success, or NULL on error.
 */

OGRGeometry *OGRGeometryFactory::createFromGML( const char *pszData )

{
    OGRGeometryH hGeom;

    hGeom = OGR_G_CreateFromGML( pszData );

    return OGRGeometry::FromHandle(hGeom);
}

/************************************************************************/
/*                           createFromGEOS()                           */
/************************************************************************/

/** Builds a OGRGeometry* from a GEOSGeom.
 * @param hGEOSCtxt GEOS context
 * @param geosGeom GEOS geometry
 * @return a OGRGeometry*
 */
OGRGeometry *
OGRGeometryFactory::createFromGEOS(
    UNUSED_IF_NO_GEOS GEOSContextHandle_t hGEOSCtxt,
    UNUSED_IF_NO_GEOS GEOSGeom geosGeom )

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return nullptr;

#else

    size_t nSize = 0;
    unsigned char *pabyBuf = nullptr;
    OGRGeometry *poGeometry = nullptr;

    // Special case as POINT EMPTY cannot be translated to WKB.
    if( GEOSGeomTypeId_r(hGEOSCtxt, geosGeom) == GEOS_POINT &&
        GEOSisEmpty_r(hGEOSCtxt, geosGeom) )
        return new OGRPoint();

#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 3)
    // GEOSGeom_getCoordinateDimension only available in GEOS 3.3.0.
    const int nCoordDim =
        GEOSGeom_getCoordinateDimension_r(hGEOSCtxt, geosGeom);
    GEOSWKBWriter* wkbwriter = GEOSWKBWriter_create_r(hGEOSCtxt);
    GEOSWKBWriter_setOutputDimension_r(hGEOSCtxt, wkbwriter, nCoordDim);
    pabyBuf = GEOSWKBWriter_write_r(hGEOSCtxt, wkbwriter, geosGeom, &nSize );
    GEOSWKBWriter_destroy_r(hGEOSCtxt, wkbwriter);
#else
    pabyBuf = GEOSGeomToWKB_buf_r( hGEOSCtxt, geosGeom, &nSize );
#endif
    if( pabyBuf == nullptr || nSize == 0 )
    {
        return nullptr;
    }

    if( OGRGeometryFactory::createFromWkb( pabyBuf,
                                           nullptr, &poGeometry,
                                           static_cast<int>(nSize) )
        != OGRERR_NONE )
    {
        poGeometry = nullptr;
    }
    // Since GEOS 3.1.1, so we test 3.2.0.
#if GEOS_CAPI_VERSION_MAJOR >= 2 || \
    (GEOS_CAPI_VERSION_MAJOR == 1 && GEOS_CAPI_VERSION_MINOR >= 6)
    GEOSFree_r( hGEOSCtxt, pabyBuf );
#else
    free( pabyBuf );
#endif

    return poGeometry;

#endif  // HAVE_GEOS
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

bool OGRGeometryFactory::haveGEOS()

{
#ifndef HAVE_GEOS
    return false;
#else
    return true;
#endif
}

/************************************************************************/
/*                           createFromFgf()                            */
/************************************************************************/

/**
 * \brief Create a geometry object of the appropriate type from its FGF (FDO Geometry Format) binary representation.
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
 *                  of failure, but NULL might be a valid return for a NULL shape.
 * @param nBytes the number of bytes available in pabyData.
 * @param pnBytesConsumed if not NULL, it will be set to the number of bytes
 * consumed (at most nBytes).
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometryFactory::createFromFgf( const void* pabyData,
                                          OGRSpatialReference * poSR,
                                          OGRGeometry **ppoReturn,
                                          int nBytes,
                                          int *pnBytesConsumed )

{
    return createFromFgfInternal(static_cast<const GByte*>(pabyData),
                                 poSR, ppoReturn, nBytes,
                                 pnBytesConsumed, 0);
}

/************************************************************************/
/*                       createFromFgfInternal()                        */
/************************************************************************/

OGRErr OGRGeometryFactory::createFromFgfInternal( const unsigned char *pabyData,
                                                  OGRSpatialReference * poSR,
                                                  OGRGeometry **ppoReturn,
                                                  int nBytes,
                                                  int *pnBytesConsumed,
                                                  int nRecLevel )
{
    // Arbitrary value, but certainly large enough for reasonable usages.
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many recursion levels (%d) while parsing FGF geometry.",
                  nRecLevel );
        return OGRERR_CORRUPT_DATA;
    }

    *ppoReturn = nullptr;

    if( nBytes < 4 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Decode the geometry type.                                       */
/* -------------------------------------------------------------------- */
    GInt32 nGType = 0;
    memcpy( &nGType, pabyData + 0, 4 );
    CPL_LSBPTR32( &nGType );

    if( nGType < 0 || nGType > 13 )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Decode the dimensionality if appropriate.                       */
/* -------------------------------------------------------------------- */
    int          nTupleSize = 0;
    GInt32       nGDim = 0;

    // TODO: Why is this a switch?
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

    OGRGeometry *poGeom = nullptr;

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
        if( nBytes < nTupleSize * 8 + 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        double adfTuple[4] = { 0.0, 0.0, 0.0, 0.0 };
        memcpy( adfTuple, pabyData + 8, nTupleSize*8 );
#ifdef CPL_MSB
        for( int iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
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
        if( nBytes < 12 )
            return OGRERR_NOT_ENOUGH_DATA;

        GInt32 nPointCount = 0;
        memcpy( &nPointCount, pabyData + 8, 4 );
        CPL_LSBPTR32( &nPointCount );

        if( nPointCount < 0 || nPointCount > INT_MAX / (nTupleSize * 8) )
            return OGRERR_CORRUPT_DATA;

        if( nBytes - 12 < nTupleSize * 8 * nPointCount )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRLineString *poLS = new OGRLineString();
        poGeom = poLS;
        poLS->setNumPoints( nPointCount );

        for( int iPoint = 0; iPoint < nPointCount; iPoint++ )
        {
            double adfTuple[4] = { 0.0, 0.0, 0.0, 0.0 };
            memcpy( adfTuple, pabyData + 12 + 8*nTupleSize*iPoint,
                    nTupleSize*8 );
#ifdef CPL_MSB
            for( int iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
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
        if( nBytes < 12 )
            return OGRERR_NOT_ENOUGH_DATA;

        GInt32 nRingCount = 0;
        memcpy( &nRingCount, pabyData + 8, 4 );
        CPL_LSBPTR32( &nRingCount );

        if( nRingCount < 0 || nRingCount > INT_MAX / 4 )
            return OGRERR_CORRUPT_DATA;

        // Each ring takes at least 4 bytes.
        if( nBytes - 12 < nRingCount * 4 )
            return OGRERR_NOT_ENOUGH_DATA;

        int nNextByte = 12;

        OGRPolygon * poPoly = new OGRPolygon();
        poGeom = poPoly;

        for( int iRing = 0; iRing < nRingCount; iRing++ )
        {
            if( nBytes - nNextByte < 4 )
            {
                delete poGeom;
                return OGRERR_NOT_ENOUGH_DATA;
            }

            GInt32 nPointCount = 0;
            memcpy( &nPointCount, pabyData + nNextByte, 4 );
            CPL_LSBPTR32( &nPointCount );

            if( nPointCount < 0 || nPointCount > INT_MAX / (nTupleSize * 8) )
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

            OGRLinearRing *poLR = new OGRLinearRing();
            poLR->setNumPoints( nPointCount );

            for( int iPoint = 0; iPoint < nPointCount; iPoint++ )
            {
                double adfTuple[4] = { 0.0, 0.0, 0.0, 0.0 };
                memcpy( adfTuple, pabyData + nNextByte, nTupleSize*8 );
                nNextByte += nTupleSize * 8;

#ifdef CPL_MSB
                for( int iOrdinal = 0; iOrdinal < nTupleSize; iOrdinal++ )
                    CPL_SWAP64PTR( adfTuple + iOrdinal );
#endif
                if( nTupleSize > 2 )
                    poLR->setPoint( iPoint, adfTuple[0],
                                    adfTuple[1], adfTuple[2] );
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
    else if( nGType == 4       // MultiPoint
             || nGType == 5    // MultiLineString
             || nGType == 6    // MultiPolygon
             || nGType == 7 )  // MultiGeometry
    {
        if( nBytes < 8 )
            return OGRERR_NOT_ENOUGH_DATA;

        GInt32 nGeomCount = 0;
        memcpy( &nGeomCount, pabyData + 4, 4 );
        CPL_LSBPTR32( &nGeomCount );

        if( nGeomCount < 0 || nGeomCount > INT_MAX / 4 )
            return OGRERR_CORRUPT_DATA;

        // Each geometry takes at least 4 bytes.
        if( nBytes - 8 < 4 * nGeomCount )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRGeometryCollection *poGC = nullptr;
        if( nGType == 4 )
            poGC = new OGRMultiPoint();
        else if( nGType == 5 )
            poGC = new OGRMultiLineString();
        else if( nGType == 6 )
            poGC = new OGRMultiPolygon();
        else if( nGType == 7 )
            poGC = new OGRGeometryCollection();

        int nBytesUsed = 8;

        for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        {
            int nThisGeomSize = 0;
            OGRGeometry *poThisGeom = nullptr;

            const OGRErr eErr =
                createFromFgfInternal(pabyData + nBytesUsed, poSR, &poThisGeom,
                                      nBytes - nBytesUsed, &nThisGeomSize,
                                      nRecLevel + 1);
            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }

            nBytesUsed += nThisGeomSize;
            if( poThisGeom != nullptr )
            {
                const OGRErr eErr2 = poGC->addGeometryDirectly( poThisGeom );
                if( eErr2 != OGRERR_NONE )
                {
                    delete poGC;
                    delete poThisGeom;
                    return eErr2;
                }
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
    if( poGeom != nullptr && poSR )
        poGeom->assignSpatialReference( poSR );
    *ppoReturn = poGeom;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGR_G_CreateFromFgf()                         */
/************************************************************************/

/**
 * \brief Create a geometry object of the appropriate type from its FGF
 * (FDO Geometry Format) binary representation.
 *
 * See OGRGeometryFactory::createFromFgf() */
OGRErr CPL_DLL OGR_G_CreateFromFgf( const void* pabyData,
                                    OGRSpatialReferenceH hSRS,
                                    OGRGeometryH *phGeometry,
                                    int nBytes, int *pnBytesConsumed )

{
    return OGRGeometryFactory::createFromFgf( pabyData,
                                              OGRSpatialReference::FromHandle(hSRS),
                                              reinterpret_cast<OGRGeometry **>(phGeometry),
                                              nBytes, pnBytesConsumed );
}

/************************************************************************/
/*                SplitLineStringAtDateline()                           */
/************************************************************************/

static void SplitLineStringAtDateline(OGRGeometryCollection* poMulti,
                                      const OGRLineString* poLS,
                                      double dfDateLineOffset,
                                      double dfXOffset)
{
    const double dfLeftBorderX = 180 - dfDateLineOffset;
    const double dfRightBorderX = -180 + dfDateLineOffset;
    const double dfDiffSpace = 360 - dfDateLineOffset;

    const bool bIs3D = poLS->getCoordinateDimension() == 3;
    OGRLineString* poNewLS = new OGRLineString();
    poMulti->addGeometryDirectly(poNewLS);
    for( int i = 0; i < poLS->getNumPoints(); i++ )
    {
        const double dfX = poLS->getX(i) + dfXOffset;
        if( i > 0 && fabs(dfX - (poLS->getX(i-1) + dfXOffset)) > dfDiffSpace )
        {
            double dfX1 = poLS->getX(i-1) + dfXOffset;
            double dfY1 = poLS->getY(i-1);
            double dfZ1 = poLS->getY(i-1);
            double dfX2 = poLS->getX(i) + dfXOffset;
            double dfY2 = poLS->getY(i);
            double dfZ2 = poLS->getY(i);

            if( dfX1 > -180 && dfX1 < dfRightBorderX && dfX2 == 180 &&
                i+1 < poLS->getNumPoints() &&
                poLS->getX(i+1) + dfXOffset > -180 && poLS->getX(i+1) + dfXOffset < dfRightBorderX )
            {
                if( bIs3D )
                    poNewLS->addPoint(-180, poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(-180, poLS->getY(i));

                i++;

                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i) + dfXOffset, poLS->getY(i),
                                      poLS->getZ(i));
                else
                    poNewLS->addPoint(poLS->getX(i) + dfXOffset, poLS->getY(i));
                continue;
            }
            else if( dfX1 > dfLeftBorderX && dfX1 < 180 && dfX2 == -180 &&
                     i+1 < poLS->getNumPoints() &&
                     poLS->getX(i+1) + dfXOffset > dfLeftBorderX && poLS->getX(i+1) + dfXOffset < 180 )
            {
                if( bIs3D )
                    poNewLS->addPoint(180, poLS->getY(i), poLS->getZ(i));
                else
                    poNewLS->addPoint(180, poLS->getY(i));

                i++;

                if( bIs3D )
                    poNewLS->addPoint(poLS->getX(i) + dfXOffset, poLS->getY(i),
                                      poLS->getZ(i));
                else
                    poNewLS->addPoint(poLS->getX(i) + dfXOffset, poLS->getY(i));
                continue;
            }

            if( dfX1 < dfRightBorderX && dfX2 > dfLeftBorderX )
            {
                std::swap(dfX1, dfX2);
                std::swap(dfY1, dfY2);
                std::swap(dfZ1, dfZ2);
            }
            if( dfX1 > dfLeftBorderX && dfX2 < dfRightBorderX )
                dfX2 += 360;

            if( dfX1 <= 180 && dfX2 >= 180 && dfX1 < dfX2 )
            {
                const double dfRatio = (180 - dfX1) / (dfX2 - dfX1);
                const double dfY = dfRatio * dfY2 + (1 - dfRatio) * dfY1;
                const double dfZ = dfRatio * dfZ2 + (1 - dfRatio) * dfZ1;
                if( bIs3D )
                    poNewLS->addPoint(
                        poLS->getX(i-1) + dfXOffset > dfLeftBorderX ? 180 : -180, dfY, dfZ);
                else
                    poNewLS->addPoint(
                        poLS->getX(i-1) + dfXOffset > dfLeftBorderX ? 180 : -180, dfY);
                poNewLS = new OGRLineString();
                if( bIs3D )
                    poNewLS->addPoint(
                        poLS->getX(i-1) + dfXOffset > dfLeftBorderX ? -180 : 180, dfY, dfZ);
                else
                    poNewLS->addPoint(
                        poLS->getX(i-1) + dfXOffset > dfLeftBorderX ? -180 : 180, dfY);
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
static void FixPolygonCoordinatesAtDateLine(OGRPolygon* poPoly,
                                            double dfDateLineOffset)
{
    const double dfLeftBorderX = 180 - dfDateLineOffset;
    const double dfRightBorderX = -180 + dfDateLineOffset;
    const double dfDiffSpace = 360 - dfDateLineOffset;

    for( int iPart = 0; iPart < 1 + poPoly->getNumInteriorRings(); iPart++)
    {
        OGRLineString* poLS = (iPart == 0) ? poPoly->getExteriorRing() :
                                             poPoly->getInteriorRing(iPart-1);
        bool bGoEast = false;
        const bool bIs3D = poLS->getCoordinateDimension() == 3;
        for( int i = 1; i < poLS->getNumPoints(); i++ )
        {
            double dfX = poLS->getX(i);
            const double dfPrevX = poLS->getX(i-1);
            const double dfDiffLong = fabs(dfX - dfPrevX);
            if( dfDiffLong > dfDiffSpace )
            {
                if( (dfPrevX > dfLeftBorderX && dfX < dfRightBorderX) ||
                    (dfX < 0 && bGoEast) )
                {
                    dfX += 360;
                    bGoEast = true;
                    if( bIs3D )
                        poLS->setPoint(i, dfX, poLS->getY(i), poLS->getZ(i));
                    else
                        poLS->setPoint(i, dfX, poLS->getY(i));
                }
                else if( dfPrevX < dfRightBorderX && dfX > dfLeftBorderX )
                {
                    for( int j = i - 1; j >= 0; j-- )
                    {
                        dfX = poLS->getX(j);
                        if( dfX < 0 )
                        {
                            if( bIs3D )
                                poLS->setPoint(j, dfX + 360, poLS->getY(j),
                                               poLS->getZ(j));
                            else
                                poLS->setPoint(j, dfX + 360, poLS->getY(j));
                        }
                    }
                    bGoEast = false;
                }
                else
                {
                    bGoEast = false;
                }
            }
        }
    }
}
#endif

/************************************************************************/
/*                            AddOffsetToLon()                          */
/************************************************************************/

static void AddOffsetToLon( OGRGeometry* poGeom, double dfOffset )
{
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPolygon:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            const int nSubGeomCount =
                OGR_G_GetGeometryCount(reinterpret_cast<OGRGeometryH>(poGeom));
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                AddOffsetToLon(
                    reinterpret_cast<OGRGeometry*>(
                        OGR_G_GetGeometryRef(
                            reinterpret_cast<OGRGeometryH>(poGeom),
                            iGeom)),
                    dfOffset);
            }

            break;
        }

        case wkbLineString:
        {
            OGRLineString* poLineString = poGeom->toLineString();
            const int nPointCount = poLineString->getNumPoints();
            const int nCoordDim = poLineString->getCoordinateDimension();
            for( int iPoint = 0; iPoint < nPointCount; iPoint++)
            {
                if( nCoordDim == 2 )
                    poLineString->setPoint(iPoint,
                                     poLineString->getX(iPoint) + dfOffset,
                                     poLineString->getY(iPoint));
                else
                    poLineString->setPoint(iPoint,
                                     poLineString->getX(iPoint) + dfOffset,
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

#ifdef HAVE_GEOS
static void AddSimpleGeomToMulti( OGRGeometryCollection* poMulti,
                                  const OGRGeometry* poGeom )
{
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPolygon:
        case wkbLineString:
            poMulti->addGeometry(poGeom);
            break;

        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            // TODO(schwehr): Can the const_casts be removed or improved?
            const int nSubGeomCount =
                OGR_G_GetGeometryCount(reinterpret_cast<OGRGeometryH>(
                    const_cast<OGRGeometry *>(poGeom)));
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                OGRGeometry* poSubGeom =
                    reinterpret_cast<OGRGeometry *>(
                        OGR_G_GetGeometryRef(
                            reinterpret_cast<OGRGeometryH>(
                                const_cast<OGRGeometry *>(poGeom)),
                            iGeom));
                AddSimpleGeomToMulti(poMulti, poSubGeom);
            }
            break;
        }

        default:
            break;
    }
}
#endif // #ifdef HAVE_GEOS

/************************************************************************/
/*                 CutGeometryOnDateLineAndAddToMulti()                 */
/************************************************************************/

static void CutGeometryOnDateLineAndAddToMulti( OGRGeometryCollection* poMulti,
                                                const OGRGeometry* poGeom,
                                                double dfDateLineOffset )
{
    const OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());
    switch( eGeomType )
    {
        case wkbPolygon:
        case wkbLineString:
        {
            bool bSplitLineStringAtDateline = false;
            OGREnvelope oEnvelope;

            poGeom->getEnvelope(&oEnvelope);
            const bool bAroundMinus180 = (oEnvelope.MinX < -180.0);

            // Naive heuristics... Place to improve.
#ifdef HAVE_GEOS
            OGRGeometry* poDupGeom = nullptr;
            bool bWrapDateline = false;
#endif

            const double dfLeftBorderX = 180 - dfDateLineOffset;
            const double dfRightBorderX = -180 + dfDateLineOffset;
            const double dfDiffSpace = 360 - dfDateLineOffset;

            const double dfXOffset = (bAroundMinus180) ? 360.0 : 0.0;
            if( oEnvelope.MinX < -180 ||
                oEnvelope.MaxX > 180 ||
                (oEnvelope.MinX + dfXOffset > dfLeftBorderX &&
                 oEnvelope.MaxX + dfXOffset > 180) )
            {
#ifndef HAVE_GEOS
                CPLError( CE_Failure, CPLE_NotSupported,
                        "GEOS support not enabled." );
#else
                bWrapDateline = true;
#endif
            }
            else
            {
                auto poLS = eGeomType == wkbPolygon
                    ? poGeom->toPolygon()->getExteriorRing()
                    : poGeom->toLineString();
                if( poLS )
                {
                    double dfMaxSmallDiffLong = 0;
                    bool bHasBigDiff = false;
                    bool bOnlyAtPlusMinus180 = poLS->getNumPoints() > 0 &&
                        ( fabs(fabs(poLS->getX(0)) - 180) < 1e-10 );
                    // Detect big gaps in longitude.
                    for( int i = 1; i < poLS->getNumPoints(); i++ )
                    {
                        const double dfPrevX = poLS->getX(i-1) + dfXOffset;
                        const double dfX = poLS->getX(i) + dfXOffset;
                        const double dfDiffLong = fabs(dfX - dfPrevX);
                        if( fabs(fabs(poLS->getX(i)) - 180) > 1e-10 )
                            bOnlyAtPlusMinus180 = false;

                        if( dfDiffLong > dfDiffSpace &&
                            ((dfX > dfLeftBorderX &&
                              dfPrevX < dfRightBorderX) ||
                             (dfPrevX > dfLeftBorderX &&
                              dfX < dfRightBorderX)) )
                            bHasBigDiff = true;
                        else if( dfDiffLong > dfMaxSmallDiffLong )
                            dfMaxSmallDiffLong = dfDiffLong;
                    }
                    if( bHasBigDiff && !bOnlyAtPlusMinus180 &&
                        dfMaxSmallDiffLong < dfDateLineOffset )
                    {
                        if( eGeomType == wkbLineString )
                            bSplitLineStringAtDateline = true;
                        else
                        {
#ifndef HAVE_GEOS
                            CPLError( CE_Failure, CPLE_NotSupported,
                                    "GEOS support not enabled." );
#else
                            bWrapDateline = true;
                            poDupGeom = poGeom->clone();
                            FixPolygonCoordinatesAtDateLine(
                                poDupGeom->toPolygon(), dfDateLineOffset);
#endif
                        }
                    }
                }
            }

            if( bSplitLineStringAtDateline )
            {
                SplitLineStringAtDateline(poMulti, poGeom->toLineString(),
                                          dfDateLineOffset,
                                          ( bAroundMinus180 ) ? 360.0 : 0.0 );
            }
#ifdef HAVE_GEOS
            else if( bWrapDateline )
            {
                const OGRGeometry* poWorkGeom =
                    poDupGeom ? poDupGeom : poGeom;
                OGRGeometry* poRectangle1 = nullptr;
                OGRGeometry* poRectangle2 = nullptr;
                const char* pszWKT1 = !bAroundMinus180 ?
                    "POLYGON((-180 90,180 90,180 -90,-180 -90,-180 90))" :
                    "POLYGON((180 90,-180 90,-180 -90,180 -90,180 90))";
                const char* pszWKT2 = !bAroundMinus180 ?
                    "POLYGON((180 90,360 90,360 -90,180 -90,180 90))" :
                    "POLYGON((-180 90,-360 90,-360 -90,-180 -90,-180 90))";
                OGRGeometryFactory::createFromWkt(pszWKT1, nullptr,
                                                  &poRectangle1);
                OGRGeometryFactory::createFromWkt(pszWKT2, nullptr,
                                                  &poRectangle2);
                OGRGeometry* poGeom1 = poWorkGeom->Intersection(poRectangle1);
                OGRGeometry* poGeom2 = poWorkGeom->Intersection(poRectangle2);
                delete poRectangle1;
                delete poRectangle2;

                if( poGeom1 != nullptr && poGeom2 != nullptr )
                {
                    AddSimpleGeomToMulti(poMulti, poGeom1);
                    AddOffsetToLon(poGeom2, !bAroundMinus180 ? -360.0 : 360.0);
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
#endif
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
            // TODO(schwehr): Fix the const_cast.
            int nSubGeomCount =
              OGR_G_GetGeometryCount(reinterpret_cast<OGRGeometryH>(
                  const_cast<OGRGeometry *>(poGeom)));
            for( int iGeom = 0; iGeom < nSubGeomCount; iGeom++ )
            {
                OGRGeometry* poSubGeom =
                    reinterpret_cast<OGRGeometry *>(OGR_G_GetGeometryRef(
                        reinterpret_cast<OGRGeometryH>(
                            const_cast<OGRGeometry *>(poGeom)),
                        iGeom));
                CutGeometryOnDateLineAndAddToMulti(poMulti, poSubGeom,
                                                   dfDateLineOffset);
            }
            break;
        }

        default:
            break;
    }
}

#ifdef HAVE_GEOS

/************************************************************************/
/*                             RemovePoint()                            */
/************************************************************************/

static void RemovePoint(OGRGeometry* poGeom, OGRPoint* poPoint)
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch( eType )
    {
        case wkbLineString:
        {
            OGRLineString* poLS = poGeom->toLineString();
            const bool bIs3D = ( poLS->getCoordinateDimension() == 3 );
            int j = 0;
            for( int i = 0; i < poLS->getNumPoints(); i++ )
            {
                if( poLS->getX(i) != poPoint->getX() ||
                    poLS->getY(i) != poPoint->getY() )
                {
                    if( i > j )
                    {
                        if( bIs3D )
                        {
                            poLS->setPoint( j, poLS->getX(i), poLS->getY(i),
                                            poLS->getZ(i) );
                        }
                        else
                        {
                            poLS->setPoint( j, poLS->getX(i), poLS->getY(i) );
                        }
                    }
                    j++;
                }
            }
            poLS->setNumPoints(j);
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = poGeom->toPolygon();
            if( poPoly->getExteriorRing() != nullptr )
            {
                RemovePoint(poPoly->getExteriorRing(), poPoint);
                for( int i=0; i<poPoly->getNumInteriorRings(); ++i )
                {
                    RemovePoint(poPoly->getInteriorRing(i), poPoint);
                }
            }
            break;
        }

        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
            for( int i=0; i<poGC->getNumGeometries(); ++i )
            {
                RemovePoint(poGC->getGeometryRef(i), poPoint);
            }
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                              GetDist()                               */
/************************************************************************/

static double GetDist(double dfDeltaX, double dfDeltaY)
{
    return sqrt(dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY);
}

/************************************************************************/
/*                             AlterPole()                              */
/*                                                                      */
/* Replace and point at the pole by points really close to the pole,    */
/* but on the previous and later segments.                              */
/************************************************************************/

static void AlterPole(OGRGeometry* poGeom, OGRPoint* poPole,
                      bool bIsRing = false)
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch( eType )
    {
        case wkbLineString:
        {
            if( !bIsRing )
                return;
            OGRLineString* poLS = poGeom->toLineString();
            const int nNumPoints = poLS->getNumPoints();
            if( nNumPoints >= 4 )
            {
                const bool bIs3D = ( poLS->getCoordinateDimension() == 3 );
                std::vector<OGRRawPoint> aoPoints;
                std::vector<double> adfZ;
                bool bMustClose = false;
                for( int i = 0; i < nNumPoints; i++ )
                {
                    const double dfX = poLS->getX(i);
                    const double dfY = poLS->getY(i);
                    if( dfX == poPole->getX() && dfY == poPole->getY() )
                    {
                        // Replace the pole by points really close to it
                        if( i == 0 )
                            bMustClose = true;
                        if( i == nNumPoints - 1 )
                            continue;
                        const int iBefore = i > 0 ? i - 1: nNumPoints - 2;
                        double dfXBefore = poLS->getX(iBefore);
                        double dfYBefore = poLS->getY(iBefore);
                        double dfNorm = GetDist(dfXBefore - dfX,
                                                dfYBefore - dfY);
                        double dfXInterp =
                            dfX + (dfXBefore - dfX) / dfNorm * 1.0e-7;
                        double dfYInterp =
                            dfY + (dfYBefore - dfY) / dfNorm * 1.0e-7;
                        OGRRawPoint oPoint;
                        oPoint.x = dfXInterp;
                        oPoint.y = dfYInterp;
                        aoPoints.push_back(oPoint);
                        adfZ.push_back(poLS->getZ(i));

                        const int iAfter = i+1;
                        double dfXAfter = poLS->getX(iAfter);
                        double dfYAfter = poLS->getY(iAfter);
                        dfNorm = GetDist(dfXAfter - dfX, dfYAfter - dfY);
                        dfXInterp = dfX + (dfXAfter - dfX) / dfNorm * 1e-7;
                        dfYInterp = dfY + (dfYAfter - dfY) / dfNorm * 1e-7;
                        oPoint.x = dfXInterp;
                        oPoint.y = dfYInterp;
                        aoPoints.push_back(oPoint);
                        adfZ.push_back(poLS->getZ(i));
                    }
                    else
                    {
                        OGRRawPoint oPoint;
                        oPoint.x = dfX;
                        oPoint.y = dfY;
                        aoPoints.push_back(oPoint);
                        adfZ.push_back(poLS->getZ(i));
                    }
                }
                if( bMustClose )
                {
                    aoPoints.push_back(aoPoints[0]);
                    adfZ.push_back(adfZ[0]);
                }

                poLS->setPoints(static_cast<int>(aoPoints.size()),
                                &(aoPoints[0]),
                                bIs3D ? &adfZ[0] : nullptr);
            }
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = poGeom->toPolygon();
            if( poPoly->getExteriorRing() != nullptr )
            {
                AlterPole(poPoly->getExteriorRing(), poPole, true);
                for( int i=0; i<poPoly->getNumInteriorRings(); ++i )
                {
                    AlterPole(poPoly->getInteriorRing(i), poPole, true);
                }
            }
            break;
        }

        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
            for( int i=0; i<poGC->getNumGeometries(); ++i )
            {
                AlterPole(poGC->getGeometryRef(i), poPole);
            }
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                          IsPolarToWGS84()                            */
/*                                                                      */
/* Returns true if poCT transforms from a projection that includes one  */
/* of the pole in a continuous way.                                     */
/************************************************************************/

static bool IsPolarToWGS84( OGRCoordinateTransformation* poCT,
                            OGRCoordinateTransformation* poRevCT,
                            bool& bIsNorthPolarOut )
{
    bool bIsNorthPolar = false;
    bool bIsSouthPolar = false;
    double x = 0.0;
    double y = 90.0;

    const bool bBackupEmitErrors = poCT->GetEmitErrors();
    poRevCT->SetEmitErrors(false);
    poCT->SetEmitErrors(false);

    if( poRevCT->Transform( 1, &x, &y ) &&
        // Surprisingly, pole south projects correctly back &
        // forth for antarctic polar stereographic.  Therefore, check that
        // the projected value is not too big.
        fabs(x) < 1e10 && fabs(y) < 1e10 )
    {
        double x_tab[] = {x, x - 1e5, x + 1e5};
        double y_tab[] = {y, y - 1e5, y + 1e5};
        if( poCT->Transform(3, x_tab, y_tab) &&
            fabs(y_tab[0] - (90.0)) < 1e-10 &&
            fabs(x_tab[2] - x_tab[1]) > 170 &&
            fabs(y_tab[2] - y_tab[1]) < 1e-10 )
        {
            bIsNorthPolar = true;
        }
    }

    x = 0.0;
    y = -90.0;
    if( poRevCT->Transform( 1, &x, &y ) &&
        fabs(x) < 1e10 && fabs(y) < 1e10 )
    {
        double x_tab[] = {x, x - 1e5, x + 1e5};
        double y_tab[] = {y, y - 1e5, y + 1e5};
        if( poCT->Transform(3, x_tab, y_tab) &&
            fabs(y_tab[0] - (-90.0)) < 1e-10 &&
            fabs(x_tab[2] - x_tab[1]) > 170 &&
            fabs(y_tab[2] - y_tab[1]) < 1e-10 )
        {
            bIsSouthPolar = true;
        }
    }

    poCT->SetEmitErrors(bBackupEmitErrors);

    if( bIsNorthPolar && bIsSouthPolar )
    {
        bIsNorthPolar = false;
        bIsSouthPolar = false;
    }

    bIsNorthPolarOut = bIsNorthPolar;
    return bIsNorthPolar || bIsSouthPolar;
}

/************************************************************************/
/*                     TransformBeforePolarToWGS84()                    */
/*                                                                      */
/* Transform the geometry (by intersection), so as to cut each geometry */
/* that crosses the pole, in 2 parts. Do also tricks for geometries     */
/* that just touch the pole.                                            */
/************************************************************************/

static OGRGeometry* TransformBeforePolarToWGS84(
                                        OGRCoordinateTransformation* poRevCT,
                                        bool bIsNorthPolar,
                                        OGRGeometry* poDstGeom,
                                        bool& bNeedPostCorrectionOut )
{
    const int nSign = (bIsNorthPolar) ? 1 : -1;

    // Does the geometry fully contains the pole ? */
    double dfXPole = 0.0;
    double dfYPole = nSign * 90.0;
    poRevCT->Transform( 1, &dfXPole, &dfYPole );
    OGRPoint oPole(dfXPole, dfYPole);
    const bool bContainsPole =
                CPL_TO_BOOL(poDstGeom->Contains(&oPole));

    const double EPS = 1e-9;

    // Does the geometry touches the pole and intersects the antimeridian ?
    double dfNearPoleAntiMeridianX = 180.0;
    double dfNearPoleAntiMeridianY = nSign*(90.0 - EPS);
    poRevCT->Transform( 1,
                        &dfNearPoleAntiMeridianX,
                        &dfNearPoleAntiMeridianY );
    OGRPoint oNearPoleAntimeridian(dfNearPoleAntiMeridianX,
                                    dfNearPoleAntiMeridianY);
    const bool bContainsNearPoleAntimeridian = CPL_TO_BOOL(
        poDstGeom->Contains(&oNearPoleAntimeridian));

    // Does the geometry touches the pole (but not intersect the antimeridian) ?
    const bool bRegularTouchesPole =
        !bContainsPole &&
        !bContainsNearPoleAntimeridian &&
        CPL_TO_BOOL(poDstGeom->Touches(&oPole));

    // Create a polygon of nearly a full hemisphere, but excluding the anti
    // meridian and the pole.
    OGRPolygon oCutter;
    OGRLinearRing* poRing = new OGRLinearRing();
    poRing->addPoint(180.0 - EPS, 0);
    poRing->addPoint(180.0 - EPS, nSign*(90.0 - EPS));
    // If the geometry doesn't contain the pole, then we add it to the cutter
    // geometry, but will later remove it completely (geometry touching the
    // pole but intersecting the antimeridian), or will replace it by 2
    // close points (geometry touching the pole without intersecting the
    // antimeridian)
    if( !bContainsPole )
        poRing->addPoint(180.0, nSign*90);
    poRing->addPoint(-180.0 + EPS, nSign*(90.0 - EPS));
    poRing->addPoint(-180.0 + EPS, 0);
    poRing->addPoint(180.0 - EPS, 0);
    oCutter.addRingDirectly(poRing);

    if( oCutter.transform(poRevCT) == OGRERR_NONE &&
        // Check that longitudes +/- 180 are continuous
        // in the polar projection
        fabs(poRing->getX(0) -
                poRing->getX(poRing->getNumPoints()-2)) < 1 &&
        (bContainsPole || bContainsNearPoleAntimeridian ||
            bRegularTouchesPole) )
    {
        if( bContainsPole || bContainsNearPoleAntimeridian )
        {
            OGRGeometry* poNewGeom =
                            poDstGeom->Difference(&oCutter);
            if( poNewGeom )
            {
                if( bContainsNearPoleAntimeridian )
                    RemovePoint(poNewGeom, &oPole);
                delete poDstGeom;
                poDstGeom = poNewGeom;
            }
        }

        if( bRegularTouchesPole )
        {
            AlterPole(poDstGeom, &oPole);
        }

        bNeedPostCorrectionOut = true;
    }
    return poDstGeom;
}

/************************************************************************/
/*                        IsAntimeridianProjToWGS84()                   */
/*                                                                      */
/* Returns true if poCT transforms from a projection that includes the  */
/* antimeridian in a continuous way.                                    */
/************************************************************************/

static bool IsAntimeridianProjToWGS84( OGRCoordinateTransformation* poCT,
                                       OGRCoordinateTransformation* poRevCT,
                                       OGRGeometry* poDstGeometry )
{
    const bool bBackupEmitErrors = poCT->GetEmitErrors();
    poRevCT->SetEmitErrors(false);
    poCT->SetEmitErrors(false);

    // Find a reasonable latitude for the geometry
    OGREnvelope sEnvelope;
    poDstGeometry->getEnvelope(&sEnvelope);
    OGRPoint pMean( sEnvelope.MinX, (sEnvelope.MinY + sEnvelope.MaxY) / 2 );
    if( pMean.transform(poCT) != OGRERR_NONE )
    {
        poCT->SetEmitErrors(bBackupEmitErrors);
        return false;
    }
    const double dfMeanLat = pMean.getY();

    // Check that close points on each side of the antimeridian in (long, lat)
    // project to close points in the source projection, and check that they
    // roundtrip correctly.
    const double EPS = 1.0e-8;
    double x1 = 180 - EPS;
    double y1 = dfMeanLat;
    double x2 = -180 + EPS;
    double y2 = dfMeanLat;
    if( !poRevCT->Transform( 1, &x1, &y1 ) ||
        !poRevCT->Transform( 1, &x2, &y2 ) ||
        GetDist(x2-x1, y2-y1) > 1 ||
        !poCT->Transform( 1, &x1, &y1 ) ||
        !poCT->Transform( 1, &x2, &y2 ) ||
        GetDist(x1 - (180 - EPS), y1 - dfMeanLat) > 2 * EPS ||
        GetDist(x2 - (-180 + EPS), y2 - dfMeanLat) > 2 * EPS )
    {
        poCT->SetEmitErrors(bBackupEmitErrors);
        return false;
    }

    poCT->SetEmitErrors(bBackupEmitErrors);

    return true;
}

/************************************************************************/
/*                      CollectPointsOnAntimeridian()                   */
/*                                                                      */
/* Collect points that are the intersection of the lines of the geometry*/
/* with the antimeridian.                                               */
/************************************************************************/

static void CollectPointsOnAntimeridian(OGRGeometry* poGeom,
                                        OGRCoordinateTransformation* poCT,
                                        OGRCoordinateTransformation* poRevCT,
                                        std::vector<OGRRawPoint>& aoPoints )
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch( eType )
    {
        case wkbLineString:
        {
            OGRLineString* poLS = poGeom->toLineString();
            const int nNumPoints = poLS->getNumPoints();
            for( int i = 0; i < nNumPoints-1; i++ )
            {
                const double dfX = poLS->getX(i);
                const double dfY = poLS->getY(i);
                const double dfX2 = poLS->getX(i+1);
                const double dfY2 = poLS->getY(i+1);
                double dfXTrans = dfX;
                double dfYTrans = dfY;
                double dfX2Trans = dfX2;
                double dfY2Trans = dfY2;
                poCT->Transform(1, &dfXTrans, &dfYTrans);
                poCT->Transform(1, &dfX2Trans, &dfY2Trans);
                // Are we crossing the antimeridian ? (detecting by inversion of
                // sign of X)
                if( (dfX2 - dfX) * (dfX2Trans - dfXTrans) < 0 )
                {
                    double dfXStart = dfX;
                    double dfYStart = dfY;
                    double dfXEnd = dfX2;
                    double dfYEnd = dfY2;
                    double dfXStartTrans = dfXTrans;
                    double dfXEndTrans = dfX2Trans;
                    int iIter = 0;
                    const double EPS = 1e-8;
                    // Find point of the segment intersecting the antimeridian
                    // by dichotomy
                    for( ;
                         iIter < 50 &&
                           (fabs(fabs(dfXStartTrans) - 180) > EPS ||
                            fabs(fabs(dfXEndTrans) - 180) > EPS);
                         ++iIter )
                    {
                        double dfXMid = (dfXStart + dfXEnd) / 2;
                        double dfYMid = (dfYStart + dfYEnd) / 2;
                        double dfXMidTrans = dfXMid;
                        double dfYMidTrans = dfYMid;
                        poCT->Transform(1, &dfXMidTrans, &dfYMidTrans);
                        if( (dfXMid - dfXStart) *
                                        (dfXMidTrans - dfXStartTrans) < 0 )
                        {
                            dfXEnd = dfXMid;
                            dfYEnd = dfYMid;
                            dfXEndTrans = dfXMidTrans;
                        }
                        else
                        {
                            dfXStart = dfXMid;
                            dfYStart = dfYMid;
                            dfXStartTrans = dfXMidTrans;
                        }
                    }
                    if( iIter < 50 )
                    {
                        OGRRawPoint oPoint;
                        oPoint.x = (dfXStart + dfXEnd) / 2;
                        oPoint.y = (dfYStart + dfYEnd) / 2;
                        poCT->Transform(1, &(oPoint.x), &(oPoint.y));
                        oPoint.x = 180.0;
                        aoPoints.push_back(oPoint);
                    }
                }
            }
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = poGeom->toPolygon();
            if( poPoly->getExteriorRing() != nullptr )
            {
                CollectPointsOnAntimeridian(poPoly->getExteriorRing(),
                                            poCT, poRevCT, aoPoints);
                for( int i=0; i<poPoly->getNumInteriorRings(); ++i )
                {
                    CollectPointsOnAntimeridian(poPoly->getInteriorRing(i),
                                                poCT, poRevCT, aoPoints);
                }
            }
            break;
        }

        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
            for( int i=0; i<poGC->getNumGeometries(); ++i )
            {
                CollectPointsOnAntimeridian(poGC->getGeometryRef(i),
                                            poCT, poRevCT, aoPoints);
            }
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                         SortPointsByAscendingY()                     */
/************************************************************************/

struct SortPointsByAscendingY
{
    bool operator()(const OGRRawPoint& a, const OGRRawPoint& b)
    {
        return a.y < b.y;
    }
};

/************************************************************************/
/*                  TransformBeforeAntimeridianToWGS84()                */
/*                                                                      */
/* Transform the geometry (by intersection), so as to cut each geometry */
/* that crosses the antimeridian, in 2 parts.                           */
/************************************************************************/

static OGRGeometry* TransformBeforeAntimeridianToWGS84(
                                        OGRCoordinateTransformation* poCT,
                                        OGRCoordinateTransformation* poRevCT,
                                        OGRGeometry* poDstGeom,
                                        bool& bNeedPostCorrectionOut )
{
    OGREnvelope sEnvelope;
    poDstGeom->getEnvelope(&sEnvelope);
    OGRPoint pMean( sEnvelope.MinX, (sEnvelope.MinY + sEnvelope.MaxY) / 2 );
    pMean.transform(poCT);
    const double dfMeanLat = pMean.getY();
    pMean.setX( 180.0 );
    pMean.setY( dfMeanLat );
    pMean.transform(poRevCT);
    // Check if the antimeridian crosses the bbox of our geometry
    if( !(pMean.getX() >= sEnvelope.MinX && pMean.getY() >= sEnvelope.MinY &&
          pMean.getX() <= sEnvelope.MaxX && pMean.getY() <= sEnvelope.MaxY) )
    {
        return poDstGeom;
    }

    // Collect points that are the intersection of the lines of the geometry
    // with the antimeridian
    std::vector<OGRRawPoint> aoPoints;
    CollectPointsOnAntimeridian(poDstGeom, poCT, poRevCT, aoPoints);
    if( aoPoints.empty() )
        return poDstGeom;

    SortPointsByAscendingY sortFunc;
    std::sort( aoPoints.begin(), aoPoints.end(), sortFunc );

    const double EPS = 1e-9;

    // Build a very thin polygon cutting the antimeridian at our points
    OGRLinearRing* poLR = new OGRLinearRing;
    {
        double x = 180.0-EPS;
        double y = aoPoints[0].y-EPS;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    for( const auto& oPoint: aoPoints )
    {
        double x = 180.0-EPS;
        double y = oPoint.y;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    {
        double x = 180.0-EPS;
        double y = aoPoints.back().y+EPS;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    {
        double x = 180.0+EPS;
        double y = aoPoints.back().y+EPS;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    for( size_t i = aoPoints.size(); i > 0; )
    {
        --i;
        const OGRRawPoint& oPoint = aoPoints[i];
        double x = 180.0+EPS;
        double y = oPoint.y;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    {
        double x = 180.0+EPS;
        double y = aoPoints[0].y-EPS;
        poRevCT->Transform(1, &x, &y);
        poLR->addPoint( x, y );
    }
    poLR->closeRings();

    OGRPolygon oPolyToCut;
    oPolyToCut.addRingDirectly(poLR);

#if DEBUG_VERBOSE
    char* pszWKT = NULL;
    oPolyToCut.exportToWkt(&pszWKT);
    CPLDebug("OGR", "Geometry to cut: %s", pszWKT);
    CPLFree(pszWKT);
#endif

    // Get the geometry without the antimeridian
    OGRGeometry* poInter = poDstGeom->Difference(&oPolyToCut);
    if( poInter != nullptr )
    {
        delete poDstGeom;
        poDstGeom = poInter;
        bNeedPostCorrectionOut = true;
    }

    return poDstGeom;
}

/************************************************************************/
/*                 SnapCoordsCloseToLatLongBounds()                     */
/*                                                                      */
/* This function snaps points really close to the antimerdian or poles  */
/* to their exact longitudes/latitudes.                                 */
/************************************************************************/

static void SnapCoordsCloseToLatLongBounds(OGRGeometry* poGeom)
{
    const OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    switch( eType )
    {
        case wkbLineString:
        {
            OGRLineString* poLS = poGeom->toLineString();
            const double EPS = 1e-8;
            for( int i = 0; i < poLS->getNumPoints(); i++ )
            {
                OGRPoint p;
                poLS->getPoint(i, &p);
                if( fabs( p.getX() - 180.0 ) < EPS )
                {
                    p.setX(180.0);
                    poLS->setPoint(i, &p);
                }
                else if( fabs( p.getX() - -180.0 ) < EPS )
                {
                    p.setX(-180.0);
                    poLS->setPoint(i, &p);
                }

                if( fabs( p.getY() - 90.0 ) < EPS )
                {
                    p.setY(90.0);
                    poLS->setPoint(i, &p);
                }
                else if( fabs( p.getY() - -90.0 ) < EPS )
                {
                    p.setY(-90.0);
                    poLS->setPoint(i, &p);
                }
            }
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = poGeom->toPolygon();
            if( poPoly->getExteriorRing() != nullptr )
            {
                SnapCoordsCloseToLatLongBounds(poPoly->getExteriorRing());
                for( int i=0; i<poPoly->getNumInteriorRings(); ++i )
                {
                    SnapCoordsCloseToLatLongBounds(poPoly->getInteriorRing(i));
                }
            }
            break;
        }

        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
            for( int i=0; i<poGC->getNumGeometries(); ++i )
            {
                SnapCoordsCloseToLatLongBounds(poGC->getGeometryRef(i));
            }
            break;
        }

        default:
            break;
    }
}

#endif

/************************************************************************/
/*                  TransformWithOptionsCache::Private                  */
/************************************************************************/

struct OGRGeometryFactory::TransformWithOptionsCache::Private
{
    OGRCoordinateTransformation* poRevCT = nullptr;
    bool bIsPolar = false;
    bool bIsNorthPolar = false;

    ~Private()
    {
        delete poRevCT;
    }
};

/************************************************************************/
/*                     TransformWithOptionsCache()                      */
/************************************************************************/

OGRGeometryFactory::TransformWithOptionsCache::TransformWithOptionsCache(): d(new Private())
{
}

/************************************************************************/
/*                     ~TransformWithOptionsCache()                      */
/************************************************************************/

OGRGeometryFactory::TransformWithOptionsCache::~TransformWithOptionsCache()
{
}

/************************************************************************/
/*                       transformWithOptions()                         */
/************************************************************************/

/** Transform a geometry.
 * @param poSrcGeom source geometry
 * @param poCT coordinate transformation object, or NULL.
 * @param papszOptions options. Including WRAPDATELINE=YES and DATELINEOFFSET=.
 * @param cache Cache. May increase performance if persisted between invocations
 * @return (new) transformed geometry.
 */
OGRGeometry* OGRGeometryFactory::transformWithOptions(
    const OGRGeometry* poSrcGeom,
    OGRCoordinateTransformation *poCT,
    char** papszOptions,
    CPL_UNUSED const TransformWithOptionsCache& cache )
{
    OGRGeometry* poDstGeom = poSrcGeom->clone();
    if( poCT != nullptr )
    {
#ifdef HAVE_GEOS
        bool bNeedPostCorrection = false;

        if( poCT->GetSourceCS() != nullptr &&
            poCT->GetTargetCS() != nullptr )
        {
            OGRSpatialReference oSRSWGS84;
            oSRSWGS84.SetWellKnownGeogCS( "WGS84" );
            oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( poCT->GetTargetCS()->IsSame(&oSRSWGS84) )
            {
                if( cache.d->poRevCT == nullptr ||
                    !cache.d->poRevCT->GetTargetCS()->IsSame(poCT->GetSourceCS()) )
                {
                    delete cache.d->poRevCT;
                    cache.d->poRevCT =
                        OGRCreateCoordinateTransformation( &oSRSWGS84,
                                                       poCT->GetSourceCS() );
                    cache.d->bIsNorthPolar = false;
                    cache.d->bIsPolar = false;
                    if( cache.d->poRevCT &&
                        IsPolarToWGS84(poCT, cache.d->poRevCT, cache.d->bIsNorthPolar) )
                    {
                        cache.d->bIsPolar = true;
                    }
                }
                auto poRevCT = cache.d->poRevCT;
                if( poRevCT != nullptr )
                {
                    if( cache.d->bIsPolar )
                    {
                        poDstGeom = TransformBeforePolarToWGS84(
                                        poRevCT, cache.d->bIsNorthPolar,
                                        poDstGeom,
                                        bNeedPostCorrection);
                    }
                    else if( IsAntimeridianProjToWGS84(poCT, poRevCT,
                                                       poDstGeom) )
                    {
                        poDstGeom = TransformBeforeAntimeridianToWGS84(
                                        poCT, poRevCT, poDstGeom,
                                        bNeedPostCorrection);
                    }
                }
            }
        }
#endif
        OGRErr eErr = poDstGeom->transform(poCT);
        if( eErr != OGRERR_NONE )
        {
            delete poDstGeom;
            return nullptr;
        }
#ifdef HAVE_GEOS
        if( bNeedPostCorrection )
        {
            SnapCoordsCloseToLatLongBounds(poDstGeom);
        }
#endif
    }

    if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "WRAPDATELINE", "NO")) )
    {
        if( poDstGeom->getSpatialReference() &&
            !poDstGeom->getSpatialReference()->IsGeographic() )
        {
            static bool bHasWarned = false;
            if( !bHasWarned )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "WRAPDATELINE is without effect when reprojecting to a "
                        "non-geographic CRS");
                bHasWarned = true;
            }
            return poDstGeom;
        }
        // TODO and we should probably also test that the axis order + data axis mapping
        // is long-lat...

        const OGRwkbGeometryType eType =
            wkbFlatten(poDstGeom->getGeometryType());
        if( eType == wkbPoint )
        {
            OGRPoint* poDstPoint = poDstGeom->toPoint();
            if( poDstPoint->getX() > 180 )
            {
                poDstPoint->setX(fmod(poDstPoint->getX() + 180, 360) - 180);
            }
            else if( poDstPoint->getX() < -180 )
            {
                poDstPoint->setX(-(fmod(-poDstPoint->getX() + 180, 360) - 180));
            }
        }
        else
        {
            OGREnvelope sEnvelope;
            poDstGeom->getEnvelope(&sEnvelope);
            if( sEnvelope.MinX >= -360.0 && sEnvelope.MaxX <= -180.0 )
                AddOffsetToLon( poDstGeom, 360.0 );
            else if( sEnvelope.MinX >= 180.0 && sEnvelope.MaxX <= 360.0 )
                AddOffsetToLon( poDstGeom, -360.0 );
            else
            {
                OGRwkbGeometryType eNewType;
                if( eType == wkbPolygon || eType == wkbMultiPolygon )
                    eNewType = wkbMultiPolygon;
                else if( eType == wkbLineString || eType == wkbMultiLineString )
                    eNewType = wkbMultiLineString;
                else
                    eNewType = wkbGeometryCollection;

                OGRGeometry* poMultiGeom = createGeometry(eNewType);
                OGRGeometryCollection* poMulti = poMultiGeom->toGeometryCollection();

                double dfDateLineOffset =
                    CPLAtofM(CSLFetchNameValueDef(papszOptions,
                                                "DATELINEOFFSET", "10"));
                if( dfDateLineOffset <= 0.0 || dfDateLineOffset >= 360.0 )
                    dfDateLineOffset = 10.0;

                CutGeometryOnDateLineAndAddToMulti(poMulti, poDstGeom,
                                                dfDateLineOffset);

                if( poMulti->getNumGeometries() == 0 )
                {
                    delete poMultiGeom;
                }
                else if( poMulti->getNumGeometries() == 1 )
                {
                    delete poDstGeom;
                    poDstGeom = poMulti->getGeometryRef(0)->clone();
                    delete poMultiGeom;
                }
                else
                {
                    delete poDstGeom;
                    poDstGeom = poMultiGeom;
                }
            }
        }
    }

    return poDstGeom;
}

/************************************************************************/
/*                         OGRGeomTransformer()                         */
/************************************************************************/

struct OGRGeomTransformer
{
    std::unique_ptr<OGRCoordinateTransformation> poCT{};
    OGRGeometryFactory::TransformWithOptionsCache cache{};
    CPLStringList aosOptions{};

    OGRGeomTransformer() = default;
    OGRGeomTransformer(const OGRGeomTransformer&) = delete;
    OGRGeomTransformer& operator=(const OGRGeomTransformer&) = delete;
};

/************************************************************************/
/*                     OGR_GeomTransformer_Create()                     */
/************************************************************************/

/** Create a geometry transformer.
 *
 * This is a enhanced version of OGR_G_Transform().
 *
 * When reprojecting geometries from a Polar Stereographic projection or a
 * projection naturally crossing the antimeridian (like UTM Zone 60) to a
 * geographic CRS, it will cut geometries along the antimeridian. So a
 * LineString might be returned as a MultiLineString.
 *
 * The WRAPDATELINE=YES option might be specified for circumstances to correct
 * geometries that incorrectly go from a longitude on a side of the antimeridian
 * to the other side, like a LINESTRING(-179 0,179 0) will be transformed to
 * a MULTILINESTRING ((-179 0,-180 0),(180 0,179 0)). For that use case, hCT
 * might be NULL.
 *
 * @param hCT Coordinate transformation object (will be cloned) or NULL.
 * @param papszOptions NULL terminated list of options, or NULL.
 *                     Supported options are:
 *                     <ul>
 *                         <li>WRAPDATELINE=YES</li>
 *                         <li>DATELINEOFFSET=longitude_gap_in_degree. Defaults to 10.</li>
 *                     </ul>
 * @return transformer object to free with OGR_GeomTransformer_Destroy()
 * @since GDAL 3.1
 */
OGRGeomTransformerH OGR_GeomTransformer_Create( OGRCoordinateTransformationH hCT,
                                                CSLConstList papszOptions )
{
    OGRGeomTransformer* transformer = new OGRGeomTransformer;
    if( hCT )
    {
        transformer->poCT.reset(
            OGRCoordinateTransformation::FromHandle(hCT)->Clone());
    }
    transformer->aosOptions.Assign(CSLDuplicate(papszOptions));
    return transformer;
}

/************************************************************************/
/*                     OGR_GeomTransformer_Transform()                  */
/************************************************************************/

/** Transforms a geometry.
 *
 * @param hTransformer transformer object.
 * @param hGeom Source geometry.
 * @return a new geometry (or NULL) to destroy with OGR_G_DestroyGeometry()
 * @since GDAL 3.1
 */
OGRGeometryH OGR_GeomTransformer_Transform(OGRGeomTransformerH hTransformer,
                                           OGRGeometryH hGeom)
{
    VALIDATE_POINTER1( hTransformer, "OGR_GeomTransformer_Transform", nullptr );
    VALIDATE_POINTER1( hGeom, "OGR_GeomTransformer_Transform", nullptr );

    return OGRGeometry::ToHandle(
        OGRGeometryFactory::transformWithOptions(
            OGRGeometry::FromHandle(hGeom),
            hTransformer->poCT.get(),
            hTransformer->aosOptions.List(),
            hTransformer->cache));
}

/************************************************************************/
/*                      OGR_GeomTransformer_Destroy()                   */
/************************************************************************/

/** Destroy a geometry transformer allocated with OGR_GeomTransformer_Create()
 *
 * @param hTransformer transformer object.
 * @since GDAL 3.1
 */
void OGR_GeomTransformer_Destroy(OGRGeomTransformerH hTransformer)
{
    delete hTransformer;
}

/************************************************************************/
/*                       OGRGF_GetDefaultStepSize()                     */
/************************************************************************/

static double OGRGF_GetDefaultStepSize()
{
    // coverity[tainted_data]
    return CPLAtofM(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
}

/************************************************************************/
/*                              DISTANCE()                              */
/************************************************************************/

static inline double DISTANCE(double x1, double y1, double x2, double y2)
{
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
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
 * If the OGR_ARC_MAX_GAP configuration variable is set, the straight-line
 * distance between adjacent pairs of interpolated points will be limited to
 * the specified distance. If the distance between a pair of points exceeds
 * this maximum, additional points are interpolated between the two points.
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
 * @param bUseMaxGap Optional: whether to honor OGR_ARC_MAX_GAP.
 *
 * @return OGRLineString geometry representing an approximation of the arc.
 *
 * @since OGR 1.8.0
 */

OGRGeometry* OGRGeometryFactory::approximateArcAngles(
    double dfCenterX, double dfCenterY, double dfZ,
    double dfPrimaryRadius, double dfSecondaryRadius, double dfRotation,
    double dfStartAngle, double dfEndAngle,
    double dfMaxAngleStepSizeDegrees, const bool bUseMaxGap /* = false */ )

{
    OGRLineString *poLine = new OGRLineString();
    const double dfRotationRadians = dfRotation * M_PI / 180.0;

    // Support default arc step setting.
    if( dfMaxAngleStepSizeDegrees < 1e-6 )
    {
        dfMaxAngleStepSizeDegrees = OGRGF_GetDefaultStepSize();
    }

    // Determine maximum interpolation gap. This is the largest straight-line
    // distance allowed between pairs of interpolated points. Default zero,
    // meaning no gap.
    // coverity[tainted_data]
    const double dfMaxInterpolationGap = bUseMaxGap ?
        CPLAtofM(CPLGetConfigOption("OGR_ARC_MAX_GAP", "0")) :
        0.0;

    // Is this a full circle?
    const bool bIsFullCircle = fabs( dfEndAngle - dfStartAngle ) == 360.0;

    // Switch direction.
    dfStartAngle *= -1;
    dfEndAngle *= -1;

    // Figure out the number of slices to make this into.
    int nVertexCount = std::max(2, static_cast<int>(
        ceil(fabs(dfEndAngle - dfStartAngle)/dfMaxAngleStepSizeDegrees) + 1));
    const double dfSlice = (dfEndAngle-dfStartAngle)/(nVertexCount-1);

    // If it is a full circle we will work out the last point separately.
    if( bIsFullCircle )
    {
        nVertexCount--;
    }

/* -------------------------------------------------------------------- */
/*      Compute the interpolated points.                                */
/* -------------------------------------------------------------------- */
    double dfLastX = 0.0;
    double dfLastY = 0.0;
    int nTotalAddPoints = 0;
    for( int iPoint = 0; iPoint < nVertexCount; iPoint++ )
    {
        const double dfAngleOnEllipse =
            (dfStartAngle + iPoint * dfSlice) * M_PI / 180.0;

        // Compute position on the unrotated ellipse.
        const double dfEllipseX = cos(dfAngleOnEllipse) * dfPrimaryRadius;
        const double dfEllipseY = sin(dfAngleOnEllipse) * dfSecondaryRadius;

        // Is this point too far from the previous point?
        if( iPoint && dfMaxInterpolationGap != 0.0 )
        {
            const double dfDistFromLast =
                DISTANCE(dfLastX, dfLastY, dfEllipseX, dfEllipseY);

            if( dfDistFromLast > dfMaxInterpolationGap )
            {
                const int nAddPoints =
                    static_cast<int>( dfDistFromLast / dfMaxInterpolationGap );
                const double dfAddSlice = dfSlice / (nAddPoints + 1);

                // Interpolate additional points
                for( int iAddPoint = 0; iAddPoint < nAddPoints; iAddPoint++ )
                {
                    const double dfAddAngleOnEllipse = (dfStartAngle +
                        (iPoint - 1) * dfSlice +
                        (iAddPoint + 1) * dfAddSlice) * (M_PI / 180.0);

                    poLine->setPoint( iPoint + nTotalAddPoints + iAddPoint,
                        cos(dfAddAngleOnEllipse) * dfPrimaryRadius,
                        sin(dfAddAngleOnEllipse) * dfSecondaryRadius,
                        dfZ );
                }

                nTotalAddPoints += nAddPoints;
            }
        }

        poLine->setPoint( iPoint + nTotalAddPoints,
            dfEllipseX, dfEllipseY, dfZ );
        dfLastX = dfEllipseX;
        dfLastY = dfEllipseY;
    }

/* -------------------------------------------------------------------- */
/*      Rotate and translate the ellipse.                               */
/* -------------------------------------------------------------------- */
    nVertexCount = poLine->getNumPoints();
    for( int iPoint = 0; iPoint < nVertexCount; iPoint++ )
    {
        const double dfEllipseX = poLine->getX( iPoint );
        const double dfEllipseY = poLine->getY( iPoint );

        // Rotate this position around the center of the ellipse.
        const double dfArcX = dfCenterX
            + dfEllipseX * cos(dfRotationRadians)
            + dfEllipseY * sin(dfRotationRadians);
        const double dfArcY = dfCenterY
            - dfEllipseX * sin(dfRotationRadians)
            + dfEllipseY * cos(dfRotationRadians);

        poLine->setPoint( iPoint, dfArcX, dfArcY, dfZ );
    }

/* -------------------------------------------------------------------- */
/*      If we're asked to make a full circle, ensure the start and      */
/*      end points coincide exactly, in spite of any rounding error.    */
/* -------------------------------------------------------------------- */
    if( bIsFullCircle )
    {
        OGRPoint oPoint;
        poLine->getPoint( 0, &oPoint );
        poLine->setPoint( nVertexCount, &oPoint );
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::approximateArcAngles(
            dfCenterX, dfCenterY, dfZ,
            dfPrimaryRadius, dfSecondaryRadius, dfRotation,
            dfStartAngle, dfEndAngle, dfMaxAngleStepSizeDegrees));
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

OGRGeometry *OGRGeometryFactory::forceToLineString( OGRGeometry *poGeom,
                                                    bool bOnlyInOrder )

{
    if( poGeom == nullptr )
        return nullptr;

    const OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());

/* -------------------------------------------------------------------- */
/*      If this is already a LineString, nothing to do                  */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbLineString )
    {
        // Except if it is a linearring.
        poGeom = OGRCurve::CastToLineString(poGeom->toCurve());

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      If it is a polygon with a single ring, return it                 */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbPolygon || eGeomType == wkbCurvePolygon )
    {
        OGRCurvePolygon* poCP = poGeom->toCurvePolygon();
        if( poCP->getNumInteriorRings() == 0 )
        {
            OGRCurve* poRing = poCP->stealExteriorRingCurve();
            delete poCP;
            return forceToLineString(poRing);
        }
        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      If it is a curve line, call CurveToLine()                        */
/* -------------------------------------------------------------------- */
    if( eGeomType == wkbCircularString ||
        eGeomType == wkbCompoundCurve )
    {
        OGRGeometry* poNewGeom = poGeom->toCurve()->CurveToLine();
        delete poGeom;
        return poNewGeom;
    }

    if( eGeomType != wkbGeometryCollection
        && eGeomType != wkbMultiLineString
        && eGeomType != wkbMultiCurve )
        return poGeom;

    // Build an aggregated linestring from all the linestrings in the container.
    OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
    if( poGeom->hasCurveGeometry() )
    {
        OGRGeometryCollection *poNewGC =
            poGC->getLinearGeometry()->toGeometryCollection();
        delete poGC;
        poGC = poNewGC;
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

        OGRLineString *poLineString0 =
            poGC->getGeometryRef(iGeom0)->toLineString();
        if( poLineString0->getNumPoints() < 2 )
        {
            iGeom0++;
            continue;
        }

        OGRPoint pointStart0;
        poLineString0->StartPoint( &pointStart0 );
        OGRPoint pointEnd0;
        poLineString0->EndPoint( &pointEnd0 );

        int iGeom1 = iGeom0 + 1;  // Used after for.
        for( ; iGeom1 < poGC->getNumGeometries(); iGeom1++ )
        {
            if( wkbFlatten(poGC->getGeometryRef(iGeom1)->getGeometryType())
                != wkbLineString )
                continue;

            OGRLineString *poLineString1 =
                poGC->getGeometryRef(iGeom1)->toLineString();
            if( poLineString1->getNumPoints() < 2 )
                continue;

            OGRPoint pointStart1;
            poLineString1->StartPoint( &pointStart1 );
            OGRPoint pointEnd1;
            poLineString1->EndPoint( &pointEnd1 );

            if( !bOnlyInOrder &&
                (pointEnd0.Equals( &pointEnd1 ) ||
                 pointStart0.Equals( &pointStart1 )) )
            {
                poLineString1->reversePoints();
                poLineString1->StartPoint( &pointStart1 );
                poLineString1->EndPoint( &pointEnd1 );
            }

            if( pointEnd0.Equals( &pointStart1 ) )
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

        if( iGeom1 == poGC->getNumGeometries() )
        {
            iGeom0++;
        }
    }

    if( poGC->getNumGeometries() == 1 )
    {
        OGRGeometry *poSingleGeom = poGC->getGeometryRef(0);
        poGC->removeGeometry( 0, FALSE );
        delete poGC;

        return poSingleGeom;
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceToLineString(
            reinterpret_cast<OGRGeometry *>(hGeom)));
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
 * possibly approximating circular arcs they may contain.  Regarding conversion
 * from linear geometry types to curve geometry types, only "wrapping" will be
 * done. No attempt to retrieve potential circular arcs by de-approximating
 * stroking will be done. For that, OGRGeometry::getCurveGeometry() can be used.
 *
 * The passed in geometry is consumed and a new one returned (or potentially the
 * same one).
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
    if( poGeom == nullptr )
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

    if( OGR_GT_IsSubClassOf(eType, wkbPolyhedralSurface) &&
        (eTargetType == wkbMultiSurface ||
         eTargetType == wkbGeometryCollection) )
    {
        return forceTo( forceTo( poGeom, wkbMultiPolygon, papszOptions),
                        eTargetType, papszOptions );
    }

    if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) &&
        eTargetType == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
        return OGRGeometryCollection::CastToGeometryCollection(poGC);
    }

    if( eType == wkbTriangle && eTargetType == wkbPolyhedralSurface )
    {
        OGRPolyhedralSurface* poPS = new OGRPolyhedralSurface();
        poPS->assignSpatialReference( poGeom->getSpatialReference() );
        poPS->addGeometryDirectly( OGRTriangle::CastToPolygon(poGeom) );
        return poPS;
    }
    else if( eType == wkbPolygon && eTargetType == wkbPolyhedralSurface )
    {
        OGRPolyhedralSurface* poPS = new OGRPolyhedralSurface();
        poPS->assignSpatialReference( poGeom->getSpatialReference() );
        poPS->addGeometryDirectly( poGeom );
        return poPS;
    }
    else if( eType == wkbMultiPolygon && eTargetType == wkbPolyhedralSurface )
    {
        OGRMultiPolygon* poMP = poGeom->toMultiPolygon();
        OGRPolyhedralSurface* poPS = new OGRPolyhedralSurface();
        for( int i = 0; i < poMP->getNumGeometries(); ++i )
        {
            poPS->addGeometry( poMP->getGeometryRef(i) );
        }
        delete poGeom;
        return poPS;
    }
    else if( eType == wkbTIN && eTargetType == wkbPolyhedralSurface )
    {
        poGeom = OGRTriangulatedSurface::CastToPolyhedralSurface(
                    poGeom->toTriangulatedSurface());
    }
    else if( eType == wkbCurvePolygon && eTargetType == wkbPolyhedralSurface )
    {
        return forceTo( forceTo( poGeom, wkbPolygon, papszOptions ),
                        eTargetType, papszOptions );
    }
    else if( eType == wkbMultiSurface && eTargetType == wkbPolyhedralSurface )
    {
        return forceTo( forceTo( poGeom, wkbMultiPolygon, papszOptions ),
                        eTargetType, papszOptions );
    }

    else if( eType == wkbTriangle && eTargetType == wkbTIN )
    {
        OGRTriangulatedSurface* poTS = new OGRTriangulatedSurface();
        poTS->assignSpatialReference( poGeom->getSpatialReference() );
        poTS->addGeometryDirectly( poGeom );
        return poTS;
    }
    else if( eType == wkbPolygon && eTargetType == wkbTIN )
    {
        OGRPolygon* poPoly = poGeom->toPolygon();
        OGRLinearRing* poLR = poPoly->getExteriorRing();
        if( !(poLR != nullptr && poLR->getNumPoints() == 4 &&
                poPoly->getNumInteriorRings() == 0) )
        {
            return poGeom;
        }
        OGRErr eErr = OGRERR_NONE;
        OGRTriangle* poTriangle = new OGRTriangle(*poPoly, eErr);
        OGRTriangulatedSurface* poTS = new OGRTriangulatedSurface();
        poTS->assignSpatialReference( poGeom->getSpatialReference() );
        poTS->addGeometryDirectly( poTriangle );
        delete poGeom;
        return poTS;
    }
    else if( eType == wkbMultiPolygon && eTargetType == wkbTIN )
    {
        OGRMultiPolygon* poMP = poGeom->toMultiPolygon();
        for( const auto poPoly: *poMP )
        {
            const OGRLinearRing* poLR = poPoly->getExteriorRing();
            if( !(poLR != nullptr && poLR->getNumPoints() == 4 &&
                  poPoly->getNumInteriorRings() == 0) )
            {
                return poGeom;
            }
        }
        OGRTriangulatedSurface* poTS = new OGRTriangulatedSurface();
        poTS->assignSpatialReference( poGeom->getSpatialReference() );
        for( const auto poPoly: *poMP )
        {
            OGRErr eErr = OGRERR_NONE;
            poTS->addGeometryDirectly( new OGRTriangle(*poPoly, eErr) );
        }
        delete poGeom;
        return poTS;
    }
    else if( eType == wkbPolyhedralSurface && eTargetType == wkbTIN )
    {
        OGRPolyhedralSurface* poPS = poGeom->toPolyhedralSurface();
        for( const auto poPoly: *poPS )
        {
            const OGRLinearRing* poLR = poPoly->getExteriorRing();
            if( !(poLR != nullptr && poLR->getNumPoints() == 4 &&
                  poPoly->getNumInteriorRings() == 0) )
            {
                return poGeom;
            }
        }
        OGRTriangulatedSurface* poTS = new OGRTriangulatedSurface();
        poTS->assignSpatialReference( poGeom->getSpatialReference() );
        for( const auto poPoly: *poPS )
        {
            OGRErr eErr = OGRERR_NONE;
            poTS->addGeometryDirectly( new OGRTriangle(*poPoly, eErr) );
        }
        delete poGeom;
        return poTS;
    }

    else if( eType == wkbPolygon && eTargetType == wkbTriangle )
    {
        OGRPolygon* poPoly = poGeom->toPolygon();
        OGRLinearRing* poLR = poPoly->getExteriorRing();
        if( !(poLR != nullptr && poLR->getNumPoints() == 4 &&
                poPoly->getNumInteriorRings() == 0) )
        {
            return poGeom;
        }
        OGRErr eErr = OGRERR_NONE;
        OGRTriangle* poTriangle = new OGRTriangle(*poPoly, eErr);
        delete poGeom;
        return poTriangle;
    }

    if( eTargetType == wkbTriangle || eTargetType == wkbTIN ||
        eTargetType == wkbPolyhedralSurface )
    {
        OGRGeometry* poPoly = forceTo( poGeom, wkbPolygon, papszOptions );
        if( poPoly == poGeom )
            return poGeom;
        return forceTo( poPoly, eTargetType, papszOptions );
    }

    if( eType == wkbTriangle && eTargetType == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = new OGRGeometryCollection();
        poGC->assignSpatialReference(poGeom->getSpatialReference());
        poGC->addGeometryDirectly(poGeom);
        return poGC;
    }

    // Promote single to multi.
    if( !OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) &&
         OGR_GT_IsSubClassOf(OGR_GT_GetCollection(eType), eTargetType) )
    {
        OGRGeometry* poRet = createGeometry(eTargetType);
        if( poRet == nullptr)
        {
            delete poGeom;
            return nullptr;
        }
        poRet->assignSpatialReference(poGeom->getSpatialReference());
        if( eType == wkbLineString )
            poGeom = OGRCurve::CastToLineString( poGeom->toCurve() );
        poRet->toGeometryCollection()->addGeometryDirectly(poGeom);
        return poRet;
    }

    const bool bIsCurve = CPL_TO_BOOL(OGR_GT_IsCurve(eType));
    if( bIsCurve && eTargetType == wkbCompoundCurve )
    {
        return OGRCurve::CastToCompoundCurve(poGeom->toCurve());
    }
    else if( bIsCurve && eTargetType == wkbCurvePolygon )
    {
        OGRCurve* poCurve = poGeom->toCurve();
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
    else if( eType == wkbTriangle && eTargetType == wkbCurvePolygon )
    {
        return OGRSurface::CastToCurvePolygon(
            OGRTriangle::CastToPolygon(poGeom)->toSurface() );
    }
    else if( eType == wkbPolygon && eTargetType == wkbCurvePolygon )
    {
        return OGRSurface::CastToCurvePolygon(poGeom->toPolygon());
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) &&
             eTargetType == wkbCompoundCurve )
    {
        OGRCurvePolygon* poPoly = poGeom->toCurvePolygon();
        if( poPoly->getNumInteriorRings() == 0 )
        {
            OGRCurve* poRet = poPoly->stealExteriorRingCurve();
            if( poRet )
                poRet->assignSpatialReference(poGeom->getSpatialReference());
            delete poPoly;
            return forceTo(poRet, eTargetType, papszOptions);
        }
    }
    else if( eType == wkbMultiPolygon && eTargetType == wkbMultiSurface )
    {
        return OGRMultiPolygon::CastToMultiSurface(poGeom->toMultiPolygon());
    }
    else if( eType == wkbMultiLineString && eTargetType == wkbMultiCurve )
    {
        return
            OGRMultiLineString::CastToMultiCurve(poGeom->toMultiLineString());
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
        if( poGC->getNumGeometries() == 1 )
        {
            OGRGeometry* poSubGeom = poGC->getGeometryRef(0);
            if( poSubGeom )
                poSubGeom->assignSpatialReference(
                    poGeom->getSpatialReference());
            poGC->removeGeometry(0, FALSE);
            OGRGeometry* poRet = forceTo(poSubGeom, eTargetType, papszOptions);
            if( OGR_GT_IsSubClassOf(wkbFlatten(poRet->getGeometryType()),
                                    eTargetType) )
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
        OGRCurvePolygon* poCP = poGeom->toCurvePolygon();
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
            {
                delete poRet;
            }
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
    return reinterpret_cast<OGRGeometryH>(
        OGRGeometryFactory::forceTo(
            reinterpret_cast<OGRGeometry *>(hGeom), eTargetType,
            papszOptions));
}

/************************************************************************/
/*                         GetCurveParameters()                          */
/************************************************************************/

/**
 * \brief Returns the parameter of an arc circle.
 *
 * Angles are return in radians, with trigonometic convention (counter clock
 * wise)
 *
 * @param x0 x of first point
 * @param y0 y of first point
 * @param x1 x of intermediate point
 * @param y1 y of intermediate point
 * @param x2 x of final point
 * @param y2 y of final point
 * @param R radius (output)
 * @param cx x of arc center (output)
 * @param cy y of arc center (output)
 * @param alpha0 angle between center and first point, in radians (output)
 * @param alpha1 angle between center and intermediate point, in radians (output)
 * @param alpha2 angle between center and final point, in radians (output)
 * @return TRUE if the points are not aligned and define an arc circle.
 *
 * @since GDAL 2.0
 */

int OGRGeometryFactory::GetCurveParameters(
    double x0, double y0, double x1, double y1, double x2, double y2,
    double& R, double& cx, double& cy,
    double& alpha0, double& alpha1, double& alpha2 )
{
    if( CPLIsNan(x0) || CPLIsNan(y0) ||
        CPLIsNan(x1) || CPLIsNan(y1) ||
        CPLIsNan(x2) || CPLIsNan(y2) )
    {
        return FALSE;
    }

    // Circle.
    if( x0 == x2 && y0 == y2 )
    {
        if( x0 != x1 || y0 != y1 )
        {
            cx = (x0 + x1) / 2;
            cy = (y0 + y1) / 2;
            R = DISTANCE(cx, cy, x0, y0);
            // Arbitrarily pick counter-clock-wise order (like PostGIS does).
            alpha0 = atan2(y0 - cy, x0 - cx);
            alpha1 = alpha0 + M_PI;
            alpha2 = alpha0 + 2 * M_PI;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    double dx01 = x1 - x0;
    double dy01 = y1 - y0;
    double dx12 = x2 - x1;
    double dy12 = y2 - y1;

    // Normalize above values so as to make sure we don't end up with
    // computing a difference of too big values.
    double dfScale = fabs(dx01);
    if( fabs(dy01) > dfScale ) dfScale = fabs(dy01);
    if( fabs(dx12) > dfScale ) dfScale = fabs(dx12);
    if( fabs(dy12) > dfScale ) dfScale = fabs(dy12);
    const double dfInvScale = 1.0 / dfScale;
    dx01 *= dfInvScale;
    dy01 *= dfInvScale;
    dx12 *= dfInvScale;
    dy12 *= dfInvScale;

    const double det = dx01 * dy12 - dx12 * dy01;
    if( fabs(det) < 1.0e-8 || CPLIsNan(det) )
    {
        return FALSE;
    }
    const double x01_mid = (x0 + x1) * dfInvScale;
    const double x12_mid = (x1 + x2) * dfInvScale;
    const double y01_mid = (y0 + y1) * dfInvScale;
    const double y12_mid = (y1 + y2) * dfInvScale;
    const double c01 = dx01 * x01_mid + dy01 * y01_mid;
    const double c12 = dx12 * x12_mid + dy12 * y12_mid;
    cx = 0.5 * dfScale * (c01 * dy12 - c12 * dy01) / det;
    cy = 0.5 * dfScale * (-c01 * dx12 + c12 * dx01) / det;

    alpha0 = atan2((y0 - cy) * dfInvScale, (x0 - cx) * dfInvScale);
    alpha1 = atan2((y1 - cy) * dfInvScale, (x1 - cx) * dfInvScale);
    alpha2 = atan2((y2 - cy) * dfInvScale, (x2 - cx) * dfInvScale);
    R = DISTANCE(cx, cy, x0, y0);

    // If det is negative, the orientation if clockwise.
    if( det < 0 )
    {
        if( alpha1 > alpha0 )
            alpha1 -= 2 * M_PI;
        if( alpha2 > alpha1 )
            alpha2 -= 2 * M_PI;
    }
    else
    {
        if( alpha1 < alpha0 )
            alpha1 += 2 * M_PI;
        if( alpha2 < alpha1 )
            alpha2 += 2 * M_PI;
    }

    CPLAssert((alpha0 <= alpha1 && alpha1 <= alpha2) ||
              (alpha0 >= alpha1 && alpha1 >= alpha2));

    return TRUE;
}

/************************************************************************/
/*                      OGRGeometryFactoryStrokeArc()                   */
/************************************************************************/

static void OGRGeometryFactoryStrokeArc( OGRLineString* poLine,
                                         double cx, double cy, double R,
                                         double z0, double z1, int bHasZ,
                                         double alpha0, double alpha1,
                                         double dfStep,
                                         int bStealthConstraints )
{
    const int nSign = dfStep > 0 ? 1 : -1;

    // Constant angle between all points, so as to not depend on winding order.
    const double dfNumSteps = fabs((alpha1 - alpha0) / dfStep) + 0.5;
    if ( dfNumSteps >= std::numeric_limits<int>::max() ||
         dfNumSteps <= std::numeric_limits<int>::min() ||
         CPLIsNan(dfNumSteps) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "OGRGeometryFactoryStrokeArc: bogus steps: "
                 "%lf %lf %lf %lf", alpha0, alpha1, dfStep, dfNumSteps);
        return;
    }

    int nSteps = static_cast<int>(dfNumSteps);
    if( bStealthConstraints )
    {
        // We need at least 6 intermediate vertex, and if more additional
        // multiples of 2.
        if( nSteps < 1+6 )
            nSteps = 1+6;
        else
            nSteps = 1+6 + 2 * ((nSteps - (1+6) + (2-1)) / 2);
    }
    else if( nSteps < 4 )
    {
        nSteps = 4;
    }
    dfStep = nSign * fabs((alpha1 - alpha0) / nSteps);
    double alpha = alpha0 + dfStep;

    for( ; (alpha - alpha1) * nSign < -1e-8; alpha += dfStep )
    {
        const double dfX = cx + R * cos(alpha);
        const double dfY = cy + R * sin(alpha);
        if( bHasZ )
        {
            const double z =
                z0 + (z1 - z0) * (alpha - alpha0) / (alpha1 - alpha0);
            poLine->addPoint(dfX, dfY, z);
        }
        else
        {
            poLine->addPoint(dfX, dfY);
        }
    }
}

/************************************************************************/
/*                         OGRGF_SetHiddenValue()                       */
/************************************************************************/

// TODO(schwehr): Cleanup these static constants.
constexpr int HIDDEN_ALPHA_WIDTH = 32;
constexpr GUInt32 HIDDEN_ALPHA_SCALE =
    static_cast<GUInt32>((static_cast<GUIntBig>(1) << HIDDEN_ALPHA_WIDTH) - 2);
constexpr int HIDDEN_ALPHA_HALF_WIDTH = (HIDDEN_ALPHA_WIDTH / 2);
constexpr int HIDDEN_ALPHA_HALF_MASK = (1 << HIDDEN_ALPHA_HALF_WIDTH) - 1;

// Encode 16-bit nValue in the 8-lsb of dfX and dfY.

#ifdef CPL_LSB
constexpr int DOUBLE_LSB_OFFSET = 0;
#else
constexpr int DOUBLE_LSB_OFFSET = 7;
#endif

static void OGRGF_SetHiddenValue( GUInt16 nValue, double& dfX, double &dfY )
{
    GByte abyData[8] = {};

    memcpy(abyData, &dfX, sizeof(double));
    abyData[DOUBLE_LSB_OFFSET] = static_cast<GByte>(nValue & 0xFF);
    memcpy(&dfX, abyData, sizeof(double));

    memcpy(abyData, &dfY, sizeof(double));
    abyData[DOUBLE_LSB_OFFSET] = static_cast<GByte>(nValue >> 8);
    memcpy(&dfY, abyData, sizeof(double));
}

/************************************************************************/
/*                         OGRGF_GetHiddenValue()                       */
/************************************************************************/

// Decode 16-bit nValue from the 8-lsb of dfX and dfY.
static GUInt16 OGRGF_GetHiddenValue( double dfX, double dfY )
{
    GByte abyData[8] = {};
    memcpy(abyData, &dfX, sizeof(double));
    GUInt16 nValue = abyData[DOUBLE_LSB_OFFSET];
    memcpy(abyData, &dfY, sizeof(double));
    nValue |= (abyData[DOUBLE_LSB_OFFSET] << 8);

    return nValue;
}

/************************************************************************/
/*                     OGRGF_NeedSwithArcOrder()                        */
/************************************************************************/

// We need to define a full ordering between starting point and ending point
// whatever it is.
static bool OGRGF_NeedSwithArcOrder( double x0, double y0,
                                     double x2, double y2 )
{
    return x0 < x2 || (x0 == x2 && y0 < y2);
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
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 * Recognized options:
 * <ul>
 * <li>ADD_INTERMEDIATE_POINT=STEALTH/YES/NO (Default to STEALTH).
 *         Determine if and how the intermediate point must be output in the
 *         linestring.  If set to STEALTH, no explicit intermediate point is
 *         added but its properties are encoded in low significant bits of
 *         intermediate points and OGRGeometryFactory::curveFromLineString() can
 *         decode them.  This is the best compromise for round-tripping in OGR
 *         and better results with PostGIS
 *         <a href="http://postgis.org/docs/ST_LineToCurve.html">ST_LineToCurve()</a>
 *         If set to YES, the intermediate point is explicitly added to the
 *         linestring.
 *         If set to NO, the intermediate point is not explicitly added.
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
    // So as to make sure the same curve followed in both direction results
    // in perfectly(=binary identical) symmetrical points.
    if( OGRGF_NeedSwithArcOrder(x0, y0, x2, y2) )
    {
        OGRLineString* poLS =
            curveToLineString(x2, y2, z2, x1, y1, z1, x0, y0, z0,
                              bHasZ, dfMaxAngleStepSizeDegrees,
                              papszOptions);
        poLS->reversePoints();
        return poLS;
    }

    double R = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double alpha0 = 0.0;
    double alpha1 = 0.0;
    double alpha2 = 0.0;

    OGRLineString* poLine = new OGRLineString();
    bool bIsArc = true;
    if( !GetCurveParameters(x0, y0, x1, y1, x2, y2,
                           R, cx, cy, alpha0, alpha1, alpha2))
    {
        bIsArc = false;
        cx = 0.0;
        cy = 0.0;
        R = 0.0;
        alpha0 = 0.0;
        alpha1 = 0.0;
        alpha2 = 0.0;
    }

    const int nSign = alpha1 >= alpha0 ? 1 : -1;

    // support default arc step setting.
    if( dfMaxAngleStepSizeDegrees < 1e-6 )
    {
        dfMaxAngleStepSizeDegrees = OGRGF_GetDefaultStepSize();
    }

    double dfStep = dfMaxAngleStepSizeDegrees / 180 * M_PI;
    if( dfStep <= 0.01 / 180 * M_PI )
    {
        CPLDebug("OGR", "Too small arc step size: limiting to 0.01 degree.");
        dfStep = 0.01 / 180 * M_PI;
    }

    dfStep *= nSign;

    if( bHasZ )
        poLine->addPoint(x0, y0, z0);
    else
        poLine->addPoint(x0, y0);

    bool bAddIntermediatePoint = false;
    bool bStealth = true;
    for( const char* const* papszIter = papszOptions;
         papszIter && *papszIter;
         papszIter++ )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszKey != nullptr && EQUAL(pszKey, "ADD_INTERMEDIATE_POINT") )
        {
            if( EQUAL(pszValue, "YES") || EQUAL(pszValue, "TRUE") ||
                EQUAL(pszValue, "ON") )
            {
                bAddIntermediatePoint = true;
                bStealth = false;
            }
            else if( EQUAL(pszValue, "NO") || EQUAL(pszValue, "FALSE") ||
                     EQUAL(pszValue, "OFF") )
            {
                bAddIntermediatePoint = false;
                bStealth = false;
            }
            else if( EQUAL(pszValue, "STEALTH") )
            {
                // default.
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

        if( bStealth && poLine->getNumPoints() > 6 )
        {
            // 'Hide' the angle of the intermediate point in the 8
            // low-significant bits of the x, y of the first 2 computed points
            // (so 32 bits), then put 0xFF, and on the last couple points put
            // again the angle but in reverse order, so that overall the
            // low-significant bits of all the points are symmetrical w.r.t the
            // mid-point.
            const double dfRatio = (alpha1 - alpha0) / (alpha2 - alpha0);
            double dfAlphaRatio = 0.5 + HIDDEN_ALPHA_SCALE * dfRatio;
            if( dfAlphaRatio < 0.0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "AlphaRation < 0: %lf", dfAlphaRatio);
                dfAlphaRatio *= -1;
            }
            else if( dfAlphaRatio >= std::numeric_limits<GUInt32>::max() ||
                     CPLIsNan(dfAlphaRatio) )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "AlphaRatio too large: %lf", dfAlphaRatio);
                dfAlphaRatio = std::numeric_limits<GUInt32>::max();
            }
            const GUInt32 nAlphaRatio = static_cast<GUInt32>(dfAlphaRatio);
            const GUInt16 nAlphaRatioLow = nAlphaRatio & HIDDEN_ALPHA_HALF_MASK;
            const GUInt16 nAlphaRatioHigh =
                nAlphaRatio >> HIDDEN_ALPHA_HALF_WIDTH;
            // printf("alpha0=%f, alpha1=%f, alpha2=%f, dfRatio=%f, "/*ok*/
            //        "nAlphaRatio = %u\n",
            //        alpha0, alpha1, alpha2, dfRatio, nAlphaRatio);

            CPLAssert( ((poLine->getNumPoints()-1 - 6) % 2) == 0 );

            for( int i = 1; i + 1 < poLine->getNumPoints(); i += 2 )
            {
                GUInt16 nVal = 0xFFFF;

                double dfX = poLine->getX(i);
                double dfY = poLine->getY(i);
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

// Fix dfAngle by offsets of 2 PI so that it lies between dfAngleStart and
// dfAngleStop, whatever their respective order.
static double OGRGF_FixAngle( double dfAngleStart, double dfAngleStop,
                              double dfAngle )
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

static inline bool IS_ALMOST_INTEGER(double x)
{
    const double val = fabs(x - floor(x + 0.5));
    return val < 1.0e-8;
}

static int OGRGF_DetectArc( const OGRLineString* poLS, int i,
                            OGRCompoundCurve*& poCC,
                            OGRCircularString*& poCS,
                            OGRLineString*& poLSNew )
{
    if( i + 3 >= poLS->getNumPoints() )
        return -1;

    OGRPoint p0;
    OGRPoint p1;
    OGRPoint p2;
    poLS->getPoint(i, &p0);
    poLS->getPoint(i+1, &p1);
    poLS->getPoint(i+2, &p2);
    double R_1 = 0.0;
    double cx_1 = 0.0;
    double cy_1 = 0.0;
    double alpha0_1 = 0.0;
    double alpha1_1 = 0.0;
    double alpha2_1 = 0.0;
    if( !(OGRGeometryFactory::GetCurveParameters(p0.getX(), p0.getY(),
                            p1.getX(), p1.getY(),
                            p2.getX(), p2.getY(),
                            R_1, cx_1, cy_1,
                            alpha0_1, alpha1_1, alpha2_1) &&
          fabs(alpha2_1 - alpha0_1) < 2.0 * 20.0 / 180.0 * M_PI) )
    {
        return -1;
    }

    const double dfDeltaAlpha10 = alpha1_1 - alpha0_1;
    const double dfDeltaAlpha21 = alpha2_1 - alpha1_1;
    const double dfMaxDeltaAlpha = std::max(fabs(dfDeltaAlpha10),
                                            fabs(dfDeltaAlpha21));
    GUInt32 nAlphaRatioRef =
        OGRGF_GetHiddenValue(p1.getX(), p1.getY()) |
        (OGRGF_GetHiddenValue(p2.getX(), p2.getY()) << HIDDEN_ALPHA_HALF_WIDTH);
    bool bFoundFFFFFFFFPattern = false;
    bool bFoundReversedAlphaRatioRef = false;
    bool bValidAlphaRatio = nAlphaRatioRef > 0 && nAlphaRatioRef < 0xFFFFFFFF;
    int nCountValidAlphaRatio = 1;

    double dfScale = std::max(1.0, R_1);
    dfScale = std::max(dfScale, fabs(cx_1));
    dfScale = std::max(dfScale, fabs(cy_1));
    dfScale = pow(10.0, ceil(log10(dfScale)));
    const double dfInvScale = 1.0 / dfScale;

    const int bInitialConstantStep =
        (fabs(dfDeltaAlpha10 - dfDeltaAlpha21) / dfMaxDeltaAlpha) < 1.0e-4;
    const double dfDeltaEpsilon = bInitialConstantStep ?
        dfMaxDeltaAlpha * 1e-4 : dfMaxDeltaAlpha/10;

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("----------------------------\n");/*ok*/
    printf("Curve beginning at offset i = %d\n", i);/*ok*/
    printf("Initial alpha ratio = %u\n", nAlphaRatioRef);/*ok*/
    printf("Initial R = %.16g, cx = %.16g, cy = %.16g\n", R_1, cx_1, cy_1);/*ok*/
    printf("dfScale = %f\n", dfScale);/*ok*/
    printf("bInitialConstantStep = %d, "/*ok*/
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

    OGRPoint p3;
    int j = i + 1;  // Used after for.
    for( ; j + 2 < poLS->getNumPoints(); j++ )
    {
        poLS->getPoint(j, &p1);
        poLS->getPoint(j+1, &p2);
        poLS->getPoint(j+2, &p3);
        double R_2 = 0.0;
        double cx_2 = 0.0;
        double cy_2 = 0.0;
        double alpha0_2 = 0.0;
        double alpha1_2 = 0.0;
        double alpha2_2 = 0.0;
        // Check that the new candidate arc shares the same
        // radius, center and winding order.
        if( !(OGRGeometryFactory::GetCurveParameters(p1.getX(), p1.getY(),
                                p2.getX(), p2.getY(),
                                p3.getX(), p3.getY(),
                                R_2, cx_2, cy_2,
                                alpha0_2, alpha1_2, alpha2_2)) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d\n : straight line", j);/*ok*/
#endif
            break;
        }

        const double dfRelDiffR = fabs(R_1 - R_2) * dfInvScale;
        const double dfRelDiffCx = fabs(cx_1 - cx_2) * dfInvScale;
        const double dfRelDiffCy = fabs(cy_1 - cy_2) * dfInvScale;

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("j=%d: R = %.16g, cx = %.16g, cy = %.16g, "/*ok*/
               "rel_diff_R=%.8g rel_diff_cx=%.8g rel_diff_cy=%.8g\n",
               j, R_2, cx_2, cy_2, dfRelDiffR, dfRelDiffCx, dfRelDiffCy);
#endif

        if( (dfRelDiffR > 1.0e-6 &&
             dfRelDiffCx > 1.0e-6 &&
             dfRelDiffCy > 1.0e-6) ||
            dfDeltaAlpha10 * (alpha1_2 - alpha0_2) < 0.0 )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d\n", j);/*ok*/
#endif
            break;
        }

        if( dfRelDiffR > 0.0 && dfRelDiffCx > 0.0 && dfRelDiffCy > 0.0 )
        {
            const double dfLogRelDiff =
                std::min(std::min(fabs(log10(dfRelDiffR)),
                                  fabs(log10(dfRelDiffCx))),
                         fabs(log10(dfRelDiffCy)));
            // printf("dfLogRelDiff = %f, dfLastLogRelDiff=%f, "/*ok*/
            //        "dfLogRelDiff - dfLastLogRelDiff=%f\n",
            //         dfLogRelDiff, dfLastLogRelDiff,
            //         dfLogRelDiff - dfLastLogRelDiff);
            if( dfLogRelDiff > 0.0 &&
                dfLastLogRelDiff >= 8.0 && dfLogRelDiff <= 8.0 &&
                dfLogRelDiff < dfLastLogRelDiff - 2.0 )
            {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf("End of curve at j=%d. Significant different in "/*ok*/
                       "relative error w.r.t previous points\n", j);
#endif
                break;
            }
            dfLastLogRelDiff = dfLogRelDiff;
        }

        const double dfStep10 = fabs(alpha1_2 - alpha0_2);
        const double dfStep21 = fabs(alpha2_2 - alpha1_2);
        // Check that the angle step is consistent with the original step.
        if( !(dfStep10 < 2.0 * dfMaxDeltaAlpha &&
              dfStep21 < 2.0 * dfMaxDeltaAlpha) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("End of curve at j=%d: dfStep10=%f, dfStep21=%f, "/*ok*/
                   "2*dfMaxDeltaAlpha=%f\n",
                   j, dfStep10, dfStep21, 2 * dfMaxDeltaAlpha);
#endif
            break;
        }

        if( bValidAlphaRatio && j > i + 1 && (i % 2) != (j % 2 ) )
        {
            const GUInt32 nAlphaRatioReversed =
                (OGRGF_GetHiddenValue(p1.getX(),
                                      p1.getY()) << HIDDEN_ALPHA_HALF_WIDTH) |
                (OGRGF_GetHiddenValue(p2.getX(), p2.getY()));
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("j=%d, nAlphaRatioReversed = %u\n",/*ok*/
                   j, nAlphaRatioReversed);
#endif
            if( !bFoundFFFFFFFFPattern && nAlphaRatioReversed == 0xFFFFFFFF )
            {
                bFoundFFFFFFFFPattern = true;
                nCountValidAlphaRatio++;
            }
            else if( bFoundFFFFFFFFPattern && !bFoundReversedAlphaRatioRef &&
                        nAlphaRatioReversed == 0xFFFFFFFF )
            {
                nCountValidAlphaRatio++;
            }
            else if( bFoundFFFFFFFFPattern && !bFoundReversedAlphaRatioRef &&
                        nAlphaRatioReversed == nAlphaRatioRef )
            {
                bFoundReversedAlphaRatioRef = true;
                nCountValidAlphaRatio++;
            }
            else
            {
                if( bInitialConstantStep &&
                    fabs(dfLastValidAlpha - alpha0_1) >= M_PI &&
                    nCountValidAlphaRatio > 10 )
                {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                    printf("End of curve at j=%d: "/*ok*/
                           "fabs(dfLastValidAlpha - alpha0_1)=%f, "
                           "nCountValidAlphaRatio=%d\n",
                           j,
                           fabs(dfLastValidAlpha - alpha0_1),
                           nCountValidAlphaRatio);
#endif
                    if( dfLastValidAlpha - alpha0_1 > 0 )
                    {
                        while( dfLastValidAlpha - alpha0_1 - dfMaxDeltaAlpha -
                               M_PI > -dfMaxDeltaAlpha/10 )
                        {
                            dfLastValidAlpha -= dfMaxDeltaAlpha;
                            j--;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf("--> corrected as fabs(dfLastValidAlpha - "/*ok*/
                                   "alpha0_1)=%f, j=%d\n",
                                   fabs(dfLastValidAlpha - alpha0_1), j);
#endif
                        }
                    }
                    else
                    {
                        while( dfLastValidAlpha - alpha0_1 + dfMaxDeltaAlpha +
                               M_PI < dfMaxDeltaAlpha/10 )
                        {
                            dfLastValidAlpha += dfMaxDeltaAlpha;
                            j--;
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf( "--> corrected as fabs(dfLastValidAlpha - "/*ok*/
                                    "alpha0_1)=%f, j=%d\n",
                                    fabs(dfLastValidAlpha - alpha0_1), j);
#endif
                        }
                    }
                    poLS->getPoint(j+1, &p2);
                    break;
                }

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf( "j=%d, nAlphaRatioReversed = %u --> inconsistent "/*ok*/
                        "values across arc. Don't use it\n",
                        j, nAlphaRatioReversed);
#endif
                bValidAlphaRatio = false;
            }
        }

        // Correct current end angle, consistently with start angle.
        dfLastValidAlpha = OGRGF_FixAngle(alpha0_1, alpha1_1, alpha2_2);

        // Try to detect the precise intermediate point of the
        // arc circle by detecting irregular angle step
        // This is OK if we don't detect the right point or fail
        // to detect it.
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("j=%d A(0,1)-maxDelta=%.8f A(1,2)-maxDelta=%.8f "/*ok*/
               "x1=%.8f y1=%.8f x2=%.8f y2=%.8f x3=%.8f y3=%.8f\n",
               j, fabs(dfStep10 - dfMaxDeltaAlpha),
               fabs(dfStep21 - dfMaxDeltaAlpha),
               p1.getX(), p1.getY(), p2.getX(), p2.getY(),
               p3.getX(), p3.getY());
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
                printf("Midpoint detected at j = %d, iMidPoint = %d, "/*ok*/
                       "x=%.8f y=%.8f\n",
                       j, iMidPoint, pMid.getX(), pMid.getY());
            }
#endif
        }
    }

    // Take a minimum threshold of consecutive points
    // on the arc to avoid false positives.
    if( j < i + 3 )
        return -1;

    bValidAlphaRatio &= bFoundFFFFFFFFPattern && bFoundReversedAlphaRatioRef;

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("bValidAlphaRatio=%d bFoundFFFFFFFFPattern=%d, "/*ok*/
           "bFoundReversedAlphaRatioRef=%d\n",
           static_cast<int>(bValidAlphaRatio),
           static_cast<int>(bFoundFFFFFFFFPattern),
           static_cast<int>(bFoundReversedAlphaRatioRef));
    printf("alpha0_1=%f dfLastValidAlpha=%f\n",/*ok*/
            alpha0_1, dfLastValidAlpha);
#endif

    if( poLSNew != nullptr )
    {
        double dfScale2 = std::max(1.0, fabs(p0.getX()));
        dfScale2 = std::max(dfScale2, fabs(p0.getY()));
        // Not strictly necessary, but helps having 'clean' lines without
        // duplicated points.
        if( fabs(poLSNew->getX(poLSNew->getNumPoints()-1) -
                 p0.getX()) / dfScale2 > 1.0e-8 ||
            fabs(poLSNew->getY(poLSNew->getNumPoints()-1) -
                 p0.getY()) / dfScale2 > 1.0e-8 )
            poLSNew->addPoint(&p0);
        if( poLSNew->getNumPoints() >= 2 )
        {
            if( poCC == nullptr )
                poCC = new OGRCompoundCurve();
            poCC->addCurveDirectly(poLSNew);
        }
        else
            delete poLSNew;
        poLSNew = nullptr;
    }

    if( poCS == nullptr )
    {
        poCS = new OGRCircularString();
        poCS->addPoint(&p0);
    }

    OGRPoint* poFinalPoint =
            ( j + 2 >= poLS->getNumPoints() ) ? &p3 : &p2;

    double dfXMid = 0.0;
    double dfYMid = 0.0;
    double dfZMid = 0.0;
    if( bValidAlphaRatio )
    {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Using alpha ratio...\n");/*ok*/
#endif
        double dfAlphaMid = 0.0;
        if( OGRGF_NeedSwithArcOrder(p0.getX(), p0.getY(),
                                    poFinalPoint->getX(),
                                    poFinalPoint->getY()) )
        {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("Switching angles\n");/*ok*/
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

        if( poLS->getCoordinateDimension() == 3 )
        {
            double dfLastAlpha = 0.0;
            double dfLastZ = 0.0;
            int k = i;  // Used after for.
            for( ; k < j+2; k++ )
            {
                OGRPoint p;
                poLS->getPoint(k, &p);
                double dfAlpha = atan2(p.getY() - cy_1, p.getX() - cx_1);
                dfAlpha = OGRGF_FixAngle(alpha0_1, dfLastValidAlpha, dfAlpha);
                if( k > i &&
                    ((dfAlpha < dfLastValidAlpha && dfAlphaMid < dfAlpha) ||
                     (dfAlpha > dfLastValidAlpha && dfAlphaMid > dfAlpha)) )
                {
                    const double dfRatio =
                        (dfAlphaMid - dfLastAlpha) / (dfAlpha - dfLastAlpha);
                    dfZMid = (1 - dfRatio) * dfLastZ + dfRatio * p.getZ();
                    break;
                }
                dfLastAlpha = dfAlpha;
                dfLastZ = p.getZ();
            }
            if( k == j + 2 )
                dfZMid = dfLastZ;
            if( IS_ALMOST_INTEGER(dfZMid) )
                dfZMid = static_cast<int>(floor(dfZMid + 0.5));
        }

        // A few rounding strategies in case the mid point was at "exact"
        // coordinates.
        if( R_1 > 1e-5 )
        {
            const bool bStartEndInteger =
                IS_ALMOST_INTEGER(p0.getX()) &&
                IS_ALMOST_INTEGER(p0.getY()) &&
                IS_ALMOST_INTEGER(poFinalPoint->getX()) &&
                IS_ALMOST_INTEGER(poFinalPoint->getY());
            if( bStartEndInteger &&
                fabs(dfXMid - floor(dfXMid + 0.5)) / dfScale < 1e-4 &&
                fabs(dfYMid - floor(dfYMid + 0.5)) / dfScale < 1e-4 )
            {
                dfXMid = static_cast<int>(floor(dfXMid + 0.5));
                dfYMid = static_cast<int>(floor(dfYMid + 0.5));
                // Sometimes rounding to closest is not best approach
                // Try neighbouring integers to look for the one that
                // minimize the error w.r.t to the arc center
                // But only do that if the radius is greater than
                // the magnitude of the delta that we will try!
                double dfBestRError =
                    fabs(R_1 - DISTANCE(dfXMid, dfYMid, cx_1, cy_1));
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                printf("initial_error=%f\n", dfBestRError);/*ok*/
#endif
                int iBestX = 0;
                int iBestY = 0;
                if( dfBestRError > 0.001 && R_1 > 2 )
                {
                    int nSearchRadius = 1;
                    // Extend the search radius if the arc circle radius
                    // is much higher than the coordinate values.
                    double dfMaxCoords =
                        std::max(fabs(p0.getX()), fabs(p0.getY()));
                    dfMaxCoords = std::max(dfMaxCoords, poFinalPoint->getX());
                    dfMaxCoords = std::max(dfMaxCoords, poFinalPoint->getY());
                    dfMaxCoords = std::max(dfMaxCoords, dfXMid);
                    dfMaxCoords = std::max(dfMaxCoords, dfYMid);
                    if( R_1 > dfMaxCoords * 1000 )
                        nSearchRadius = 100;
                    else if( R_1 > dfMaxCoords * 10 )
                        nSearchRadius = 10;
                    for( int iY = -nSearchRadius; iY <= nSearchRadius; iY++ )
                    {
                        for( int iX = -nSearchRadius;
                             iX <= nSearchRadius;
                             iX++ )
                        {
                            const double dfCandidateX = dfXMid+iX;
                            const double dfCandidateY = dfYMid+iY;
                            if( fabs(dfCandidateX - p0.getX()) < 1e-8 &&
                                fabs(dfCandidateY - p0.getY()) < 1e-8 )
                                continue;
                            if( fabs(dfCandidateX -
                                     poFinalPoint->getX()) < 1e-8 &&
                                fabs(dfCandidateY -
                                     poFinalPoint->getY()) < 1e-8 )
                                continue;
                            const double dfRError =
                                fabs(R_1 - DISTANCE(dfCandidateX, dfCandidateY,
                                                    cx_1, cy_1));
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
                            printf("x=%d y=%d error=%f besterror=%f\n",/*ok*/
                                   static_cast<int>(dfXMid + iX),
                                   static_cast<int>(dfYMid + iY),
                                   dfRError, dfBestRError);
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
                // Limit the number of significant figures in decimal
                // representation.
                if( fabs(dfXMid) < 100000000.0 )
                {
                    dfXMid =
                        static_cast<GIntBig>(floor(dfXMid * 100000000+0.5)) / 100000000.0;
                }
                if( fabs(dfYMid) < 100000000.0 )
                {
                    dfYMid =
                        static_cast<GIntBig>(floor(dfYMid * 100000000+0.5)) / 100000000.0;
                }
            }
        }

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("dfAlphaMid=%f, x_mid = %f, y_mid = %f\n",/*ok*/
               dfLastValidAlpha, dfXMid, dfYMid);
#endif
    }

    // If this is a full circle of a non-polygonal zone, we must
    // use a 5-point representation to keep the winding order.
    if( p0.Equals(poFinalPoint) &&
        !EQUAL(poLS->getGeometryName(), "LINEARRING") )
    {
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Full circle of a non-polygonal zone\n");/*ok*/
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

    // If we have found a candidate for a precise intermediate
    // point, use it.
    else if( iMidPoint >= 1 && iMidPoint < j )
    {
        poLS->getPoint(iMidPoint, &p1);
        poCS->addPoint(&p1);
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
        printf("Using detected midpoint...\n");/*ok*/
        printf("x_mid = %f, y_mid = %f\n", p1.getX(), p1.getY());/*ok*/
#endif
        }
        // Otherwise pick up the mid point between both extremities.
        else
        {
            poLS->getPoint((i + j + 1) / 2, &p1);
            poCS->addPoint(&p1);
#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
            printf("Pickup 'random' midpoint at index=%d...\n",/*ok*/
                   (i + j + 1) / 2);
            printf("x_mid = %f, y_mid = %f\n", p1.getX(), p1.getY());/*ok*/
#endif
        }
        poCS->addPoint(poFinalPoint);

#ifdef VERBOSE_DEBUG_CURVEFROMLINESTRING
    printf("----------------------------\n");/*ok*/
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

OGRCurve* OGRGeometryFactory::curveFromLineString(
    const OGRLineString* poLS,
    CPL_UNUSED const char * const * papszOptions)
{
    OGRCompoundCurve* poCC = nullptr;
    OGRCircularString* poCS = nullptr;
    OGRLineString* poLSNew = nullptr;
    const int nLSNumPoints = poLS->getNumPoints();
    const bool bIsClosed = nLSNumPoints >= 4 && poLS->get_IsClosed();
    for( int i = 0; i < nLSNumPoints; /* nothing */ )
    {
        const int iNewI = OGRGF_DetectArc(poLS, i, poCC, poCS, poLSNew);
        if( iNewI == -2 )
            break;
        if( iNewI >= 0 )
        {
            i = iNewI;
            continue;
        }

        if( poCS != nullptr )
        {
            if( poCC == nullptr )
                poCC = new OGRCompoundCurve();
            poCC->addCurveDirectly(poCS);
            poCS = nullptr;
        }

        OGRPoint p;
        poLS->getPoint(i, &p);
        if( poLSNew == nullptr )
        {
            poLSNew = new OGRLineString();
            poLSNew->addPoint(&p);
        }
        // Not strictly necessary, but helps having 'clean' lines without
        // duplicated points.
        else
        {
            double dfScale = std::max(1.0, fabs(p.getX()));
            dfScale = std::max(dfScale, fabs(p.getY()));
            if (bIsClosed && i == nLSNumPoints - 1)
                dfScale = 0;
            if( fabs(poLSNew->getX(poLSNew->getNumPoints()-1) - p.getX()) >
                    1e-8 * dfScale ||
                fabs(poLSNew->getY(poLSNew->getNumPoints()-1) - p.getY()) >
                    1e-8 * dfScale )
            {
                poLSNew->addPoint(&p);
            }
        }

        i++;
    }

    OGRCurve* poRet = nullptr;

    if( poLSNew != nullptr && poLSNew->getNumPoints() < 2 )
    {
        delete poLSNew;
        poLSNew = nullptr;
        if( poCC != nullptr )
        {
            if( poCC->getNumCurves() == 1 )
            {
                poRet = poCC->stealCurve(0);
                delete poCC;
                poCC = nullptr;
            }
            else
                poRet = poCC;
        }
        else
            poRet = poLS->clone();
    }
    else if( poCC != nullptr )
    {
        if( poLSNew )
            poCC->addCurveDirectly(poLSNew);
        else
            poCC->addCurveDirectly(poCS);
        poRet = poCC;
    }
    else if( poLSNew != nullptr )
        poRet = poLSNew;
    else if( poCS != nullptr )
        poRet = poCS;
    else
        poRet = poLS->clone();

    poRet->assignSpatialReference( poLS->getSpatialReference() );

    return poRet;
}

/************************************************************************/
/*                   createFromGeoJson( const char* )                   */
/************************************************************************/

/**
 * @brief Create geometry from GeoJson fragment.
 * @param pszJsonString The GeoJSON fragment for the geometry.
 * @return a geometry on success, or NULL on error.
 * @since GDAL 2.3
 */
OGRGeometry* OGRGeometryFactory::createFromGeoJson( const char *pszJsonString )
{
    CPLJSONDocument oDocument;
    if( !oDocument.LoadMemory( reinterpret_cast<const GByte*>(pszJsonString)) )
    {
        return nullptr;
    }

    return createFromGeoJson( oDocument.GetRoot() );
}

/************************************************************************/
/*              createFromGeoJson( const CPLJSONObject& )               */
/************************************************************************/

/**
 * @brief Create geometry from GeoJson fragment.
 * @param oJsonObject The JSONObject class describes the GeoJSON geometry.
 * @return a geometry on success, or NULL on error.
 * @since GDAL 2.3
 */
OGRGeometry* OGRGeometryFactory::createFromGeoJson( const CPLJSONObject &oJsonObject )
{
    if( !oJsonObject.IsValid() )
    {
        return nullptr;
    }

    // TODO: Move from GeoJSON driver functions create geometry here, and replace
    // json-c specific json_object to CPLJSONObject
    return OGRGeoJSONReadGeometry(static_cast<json_object*>(
                                      oJsonObject.GetInternalHandle()));
}
