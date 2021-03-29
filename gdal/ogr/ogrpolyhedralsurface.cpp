/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolyhedralSurface geometry class.
 * Author:   Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Avyav Kumar Singh <avyavkumar at gmail dot com>
 * Copyright (c) 2016, Even Rouault <even.roauult at spatialys.com>
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
#include "ogr_p.h"
#include "ogr_sfcgal.h"
#include "ogr_api.h"
#include "ogr_libs.h"

#include <new>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRPolyhedralSurface()                       */
/************************************************************************/

/**
 * \brief Create an empty PolyhedralSurface
 */

OGRPolyhedralSurface::OGRPolyhedralSurface() = default;

/************************************************************************/
/*         OGRPolyhedralSurface( const OGRPolyhedralSurface& )          */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 */

OGRPolyhedralSurface::OGRPolyhedralSurface( const OGRPolyhedralSurface& ) = default;

/************************************************************************/
/*                        ~OGRPolyhedralSurface()                       */
/************************************************************************/

/**
 * \brief Destructor
 *
 */

OGRPolyhedralSurface::~OGRPolyhedralSurface() = default;

/************************************************************************/
/*                 operator=( const OGRPolyhedralSurface&)              */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 */

OGRPolyhedralSurface& OGRPolyhedralSurface::operator=(
                                            const OGRPolyhedralSurface& other )
{
    if( this != &other)
    {
        OGRSurface::operator=( other );
        oMP = other.oMP;
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRPolyhedralSurface *OGRPolyhedralSurface::clone() const

{
    return new (std::nothrow) OGRPolyhedralSurface(*this);
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char* OGRPolyhedralSurface::getGeometryName() const
{
    return "POLYHEDRALSURFACE" ;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

/**
 * \brief Returns the WKB Type of PolyhedralSurface
 *
 */

OGRwkbGeometryType OGRPolyhedralSurface::getGeometryType() const
{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbPolyhedralSurfaceZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbPolyhedralSurfaceM;
    else if( flags & OGR_G_3D )
        return wkbPolyhedralSurfaceZ;
    else
        return wkbPolyhedralSurface;
}

/************************************************************************/
/*                              WkbSize()                               */
/************************************************************************/

int OGRPolyhedralSurface::WkbSize() const
{
    int nSize = 9;
    for( auto&& poSubGeom: *this )
    {
        nSize += poSubGeom->WkbSize();
    }
    return nSize;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRPolyhedralSurface::getDimension() const
{
    return 2;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRPolyhedralSurface::empty()
{
    if( oMP.papoGeoms != nullptr )
    {
        for( auto&& poSubGeom: *this )
        {
            delete poSubGeom;
        }
        CPLFree(oMP.papoGeoms);
    }
    oMP.nGeomCount = 0;
    oMP.papoGeoms = nullptr;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/


void OGRPolyhedralSurface::getEnvelope( OGREnvelope * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPolyhedralSurface::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRPolyhedralSurface::importFromWkb ( const unsigned char * pabyData,
                                             int nSize,
                                             OGRwkbVariant eWkbVariant,
                                             int& nBytesConsumedOut )
{
    nBytesConsumedOut = -1;
    oMP.nGeomCount = 0;
    OGRwkbByteOrder eByteOrder = wkbXDR;
    int nDataOffset = 0;
    OGRErr eErr = importPreambleOfCollectionFromWkb( pabyData,
                                                      nSize,
                                                      nDataOffset,
                                                      eByteOrder,
                                                      9,
                                                      oMP.nGeomCount,
                                                      eWkbVariant );

    if( eErr != OGRERR_NONE )
        return eErr;


    oMP.papoGeoms = reinterpret_cast<OGRGeometry **>(
                            VSI_CALLOC_VERBOSE(sizeof(void*), oMP.nGeomCount));
    if (oMP.nGeomCount != 0 && oMP.papoGeoms == nullptr)
    {
        oMP.nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    for( int iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        // Parse the polygons
        const unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eSubGeomType;
        eErr = OGRReadWKBGeometryType( pabySubData, eWkbVariant, &eSubGeomType );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !isCompatibleSubType(eSubGeomType) )
        {
            oMP.nGeomCount = iGeom;
            CPLDebug("OGR", "Cannot add geometry of type (%d) to "
                     "geometry of type (%d)",
                     eSubGeomType, getGeometryType());
            return OGRERR_CORRUPT_DATA;
        }

        OGRGeometry* poSubGeom = nullptr;
        int nSubGeomBytesConsumed = -1;
        eErr = OGRGeometryFactory::createFromWkb( pabySubData, nullptr,
                                                  &poSubGeom, nSize,
                                                  eWkbVariant,
                                                  nSubGeomBytesConsumed );

        if( eErr != OGRERR_NONE )
        {
            oMP.nGeomCount = iGeom;
            delete poSubGeom;
            return eErr;
        }

        oMP.papoGeoms[iGeom] = poSubGeom;

        if (oMP.papoGeoms[iGeom]->Is3D())
            flags |= OGR_G_3D;
        if (oMP.papoGeoms[iGeom]->IsMeasured())
            flags |= OGR_G_MEASURED;

        CPLAssert( nSubGeomBytesConsumed > 0 );
        if( nSize != -1 )
        {
            CPLAssert( nSize >= nSubGeomBytesConsumed );
            nSize -= nSubGeomBytesConsumed;
        }

        nDataOffset += nSubGeomBytesConsumed;
    }
    nBytesConsumedOut = nDataOffset;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/

OGRErr  OGRPolyhedralSurface::exportToWkb ( OGRwkbByteOrder eByteOrder,
                                            unsigned char * pabyData,
                                            OGRwkbVariant /*eWkbVariant*/ ) const

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER(static_cast<unsigned char>(eByteOrder));

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getIsoGeometryType();

    if( OGR_SWAP( eByteOrder ) )
    {
        nGType = CPL_SWAP32(nGType);
    }

    memcpy( pabyData + 1, &nGType, 4 );

    // Copy the raw data
    if( OGR_SWAP( eByteOrder ) )
    {
        int nCount = CPL_SWAP32( oMP.nGeomCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
        memcpy( pabyData+5, &oMP.nGeomCount, 4 );

    int nOffset = 9;

    // serialize each of the geometries
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->exportToWkb( eByteOrder, pabyData + nOffset,
                                           wkbVariantIso );
        nOffset += poSubGeom->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*              Instantiate from well known text format.                */
/************************************************************************/

OGRErr OGRPolyhedralSurface::importFromWkt( const char ** ppszInput )
{
    int bHasZ = FALSE, bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr      eErr = importPreambleFromWkt(
                                        ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each surface in turn.  Note that we try to reuse the same  */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = nullptr;
    int          nMaxPoints = 0;
    double      *padfZ = nullptr;

    do
    {

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        const char* pszInputBefore = pszInput;
        pszInput = OGRWktReadToken( pszInput, szToken );

        OGRSurface* poSurface = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        if (EQUAL(szToken,"("))
        {
            OGRPolygon *poPolygon =  OGRGeometryFactory::createGeometry(
                                            getSubGeometryType())->toPolygon();
            poSurface = poPolygon;
            pszInput = pszInputBefore;
            eErr = poPolygon->importFromWKTListOnly(
                            &pszInput, bHasZ, bHasM,
                            paoPoints, nMaxPoints, padfZ );
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected token : %s", szToken);
            eErr = OGRERR_CORRUPT_DATA;
            break;
        }

        if( eErr == OGRERR_NONE )
            eErr = oMP._addGeometryDirectlyWithExpectedSubGeometryType(
                                                      poSurface,
                                                      getSubGeometryType() );
        if( eErr != OGRERR_NONE )
        {
            delete poSurface;
            break;
        }

        // Read the delimiter following the surface.
        pszInput = OGRWktReadToken( pszInput, szToken );

    } while( szToken[0] == ',' && eErr == OGRERR_NONE );

    CPLFree( paoPoints );
    CPLFree( padfZ );

    // Check for a closing bracket
    if( eErr != OGRERR_NONE )
        return eErr;

    if( szToken[0] != ')' )
        return OGRERR_CORRUPT_DATA;

    set3D(oMP.Is3D());
    setMeasured(oMP.IsMeasured());

    *ppszInput = pszInput;
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/


std::string OGRPolyhedralSurface::exportToWkt(const OGRWktOptions& opts,
                                              OGRErr *err) const
{
    OGRWktOptions optsModified(opts);
    optsModified.variant = wkbVariantIso;
    return exportToWktInternal(optsModified, err);
}

//! @cond Doxygen_Suppress
std::string OGRPolyhedralSurface::exportToWktInternal(const OGRWktOptions& opts,
                                                      OGRErr *err) const
{
    try
    {
        std::string wkt(getGeometryName());
        wkt += wktTypeString(opts.variant);
        bool first = true;

        for (int i = 0; i < oMP.nGeomCount; ++i)
        {
            OGRGeometry *geom = oMP.papoGeoms[i];

            OGRErr subgeomErr = OGRERR_NONE;
            std::string tempWkt = geom->exportToWkt(opts, &subgeomErr);
            if (subgeomErr != OGRERR_NONE)
            {
                if( err )
                    *err = subgeomErr;
                return std::string();
            }

            auto pos = tempWkt.find('(');

            // Skip empty geoms
            if (pos == std::string::npos)
                continue;
            if (first)
                wkt += '(';
            else
                wkt += ',';
            first = false;

            // Extract the '( ... )' part of the child geometry.
            wkt += tempWkt.substr(pos);
        }

        if (err)
            *err = OGRERR_NONE;
        if( first )
            wkt += "EMPTY";
        else
            wkt += ')';
        return wkt;
    }
    catch( const std::bad_alloc& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        if (err)
            *err = OGRERR_FAILURE;
        return std::string();
    }
}
//! @endcond

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRPolyhedralSurface::flattenTo2D()
{
    oMP.flattenTo2D();

    flags &= ~OGR_G_3D;
    flags &= ~OGR_G_MEASURED;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRPolyhedralSurface::transform( OGRCoordinateTransformation *poCT )
{
    return oMP.transform(poCT);
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
static OGRPolygon* CasterToPolygon(OGRSurface* poGeom)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "%s found. Conversion impossible", poGeom->getGeometryName());
    delete poGeom;
    return nullptr;
}

OGRSurfaceCasterToPolygon OGRPolyhedralSurface::GetCasterToPolygon() const
{
    return ::CasterToPolygon;
}
//! @endcond

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

//! @cond Doxygen_Suppress
static OGRCurvePolygon* CasterToCurvePolygon(OGRSurface* poGeom)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "%s found. Conversion impossible", poGeom->getGeometryName());
    delete poGeom;
    return nullptr;
}

OGRSurfaceCasterToCurvePolygon OGRPolyhedralSurface::GetCasterToCurvePolygon() const
{
    return ::CasterToCurvePolygon;
}
//! @endcond


/************************************************************************/
/*                         isCompatibleSubType()                        */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRBoolean OGRPolyhedralSurface::isCompatibleSubType( OGRwkbGeometryType eSubType ) const
{
    return wkbFlatten( eSubType ) == wkbPolygon;
}
//! @endcond

/************************************************************************/
/*                         getSubGeometryName()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
const char* OGRPolyhedralSurface::getSubGeometryName() const
{
    return "POLYGON";
}
//! @endcond

/************************************************************************/
/*                         getSubGeometryType()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRwkbGeometryType OGRPolyhedralSurface::getSubGeometryType() const
{
    return wkbPolygon;
}
//! @endcond

/************************************************************************/
/*                               Equals()                               */
/************************************************************************/

OGRBoolean OGRPolyhedralSurface::Equals(const OGRGeometry * poOther) const
{

    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if ( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    auto poOMP = poOther->toPolyhedralSurface();
    if( oMP.getNumGeometries() != poOMP->oMP.getNumGeometries() )
        return FALSE;

    for( int iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        if( !oMP.getGeometryRef(iGeom)->Equals(poOMP->oMP.getGeometryRef(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

/**
 * \brief Returns the area enclosed
 *
 * This method is built on the SFCGAL library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the SFCGAL library, this method will always return
 * -1.0
 *
 * @return area enclosed by the PolyhedralSurface
 */

double OGRPolyhedralSurface::get_Area() const
{
#ifndef HAVE_SFCGAL

    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return -1.0;

#else

    sfcgal_init();
    sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL(this);
    if (poThis == nullptr)
        return -1.0;

    double area = sfcgal_geometry_area_3d(poThis);

    sfcgal_geometry_delete(poThis);

    return (area > 0)? area: -1.0;

#endif
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

OGRErr OGRPolyhedralSurface::PointOnSurface(OGRPoint *poPoint) const
{
    return PointOnSurfaceInternal(poPoint);
}

/************************************************************************/
/*                     GetCasterToMultiPolygon()                        */
/************************************************************************/
//! @cond Doxygen_Suppress
OGRPolyhedralSurfaceCastToMultiPolygon
                            OGRPolyhedralSurface::GetCasterToMultiPolygon() const
{
    return OGRPolyhedralSurface::CastToMultiPolygonImpl;
}

/************************************************************************/
/*                      CastToMultiPolygonImpl()                        */
/************************************************************************/

OGRMultiPolygon* OGRPolyhedralSurface::CastToMultiPolygonImpl(
                                                    OGRPolyhedralSurface* poPS)
{
    OGRMultiPolygon *poMultiPolygon = new OGRMultiPolygon(poPS->oMP);
    poMultiPolygon->assignSpatialReference(poPS->getSpatialReference());
    delete poPS;
    return poMultiPolygon;
}
//! @endcond

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

/**
 * \brief Casts the OGRPolyhedralSurface to an OGRMultiPolygon
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poPS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */


OGRMultiPolygon* OGRPolyhedralSurface::CastToMultiPolygon(
                                                    OGRPolyhedralSurface* poPS)
{
    OGRPolyhedralSurfaceCastToMultiPolygon pfn =
                                            poPS->GetCasterToMultiPolygon();
    return pfn(poPS);
}


/************************************************************************/
/*                            addGeometry()                             */
/************************************************************************/

/**
 * \brief Add a new geometry to a collection.
 *
 * Only a POLYGON can be added to a POLYHEDRALSURFACE.
 *
 * @return OGRErr OGRERR_NONE if the polygon is successfully added
 */

OGRErr OGRPolyhedralSurface::addGeometry (const OGRGeometry *poNewGeom)
{
    if (!isCompatibleSubType(poNewGeom->getGeometryType()))
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    OGRGeometry *poClone = poNewGeom->clone();
    OGRErr      eErr;

    if (poClone == nullptr)
        return OGRERR_FAILURE;

    eErr = addGeometryDirectly(poClone);

    if( eErr != OGRERR_NONE )
        delete poClone;

    return eErr;
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/************************************************************************/

/**
 * \brief Add a geometry directly to the container.
 *
 * This method is the same as the C function OGR_G_AddGeometryDirectly().
 *
 * There is no SFCOM analog to this method.
 *
 * @param poNewGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of geometry container.
 */

OGRErr OGRPolyhedralSurface::addGeometryDirectly (OGRGeometry *poNewGeom)
{
    if (!isCompatibleSubType(poNewGeom->getGeometryType()))
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    HomogenizeDimensionalityWith(poNewGeom);

    OGRGeometry** papoNewGeoms = static_cast<OGRGeometry **>(
        VSI_REALLOC_VERBOSE( oMP.papoGeoms,
                                        sizeof(void*) * (oMP.nGeomCount+1) ));
    if( papoNewGeoms == nullptr )
        return OGRERR_FAILURE;

    oMP.papoGeoms = papoNewGeoms;
    oMP.papoGeoms[oMP.nGeomCount] = poNewGeom;
    oMP.nGeomCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          getNumGeometries()                          */
/************************************************************************/

/**
 * \brief Fetch number of geometries in PolyhedralSurface
 *
 * @return count of children geometries.  May be zero.
 */

int OGRPolyhedralSurface::getNumGeometries() const
{
    return oMP.nGeomCount;
}

/************************************************************************/
/*                         getGeometryRef()                             */
/************************************************************************/

/**
 * \brief Fetch geometry from container.
 *
 * This method returns a pointer to an geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid until the next change to the
 * geometry container.  Use IGeometry::clone() to make a copy.
 *
 * @param i the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return pointer to requested geometry.
 */

OGRPolygon* OGRPolyhedralSurface::getGeometryRef(int i)
{
    return oMP.papoGeoms[i]->toPolygon();
}

/************************************************************************/
/*                         getGeometryRef()                             */
/************************************************************************/

/**
 * \brief Fetch geometry from container.
 *
 * This method returns a pointer to an geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid until the next change to the
 * geometry container.  Use IGeometry::clone() to make a copy.
 *
 * @param i the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return pointer to requested geometry.
 */

const OGRPolygon* OGRPolyhedralSurface::getGeometryRef(int i) const
{
    return oMP.papoGeoms[i]->toPolygon();
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

/**
 * \brief Checks if the PolyhedralSurface is empty
 *
 * @return TRUE if the PolyhedralSurface is empty, FALSE otherwise
 */

OGRBoolean  OGRPolyhedralSurface::IsEmpty() const
{
    return oMP.IsEmpty();
}

/************************************************************************/
/*                                 set3D()                              */
/************************************************************************/

/**
 * \brief Set the type as 3D geometry
 */

void OGRPolyhedralSurface::set3D (OGRBoolean bIs3D)
{
    oMP.set3D(bIs3D);

    OGRGeometry::set3D( bIs3D );
}

/************************************************************************/
/*                             setMeasured()                            */
/************************************************************************/

/**
 * \brief Set the type as Measured
 */

void OGRPolyhedralSurface::setMeasured (OGRBoolean bIsMeasured)
{
    oMP.setMeasured(bIsMeasured);

    OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

/**
 * \brief Set the coordinate dimension.
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values.
 * This will also remove the M dimension if present before this call.
 *
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGRPolyhedralSurface::setCoordinateDimension (int nNewDimension)
{
    oMP.setCoordinateDimension(nNewDimension);

    OGRGeometry::setCoordinateDimension( nNewDimension );
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

/**
 * \brief Swap x and y coordinates.
 */

void OGRPolyhedralSurface::swapXY()
{
    oMP.swapXY();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/


OGRBoolean OGRPolyhedralSurface::hasCurveGeometry(int) const
{
    return FALSE;
}

/************************************************************************/
/*                          removeGeometry()                            */
/************************************************************************/

/**
 * \brief Remove a geometry from the container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * @param iGeom the index of the geometry to delete.  A value of -1 is a
 * special flag meaning that all geometries should be removed.
 *
 * @param bDelete if TRUE the geometry will be deallocated, otherwise it will
 * not.  The default is TRUE as the container is considered to own the
 * geometries in it.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is
 * out of range.
 */

OGRErr OGRPolyhedralSurface::removeGeometry(int iGeom, int bDelete)
{
    return oMP.removeGeometry(iGeom,bDelete);
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

void OGRPolyhedralSurface::assignSpatialReference( OGRSpatialReference * poSR )
{
    OGRGeometry::assignSpatialReference(poSR);
    oMP.assignSpatialReference(poSR);
}
