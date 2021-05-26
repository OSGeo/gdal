/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurveCollection class.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
#include "ogr_geometry.h"

#include <cstddef>
#include <cstring>
#include <new>

#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         OGRCurveCollection()                         */
/************************************************************************/

OGRCurveCollection::OGRCurveCollection() = default;

/************************************************************************/
/*             OGRCurveCollection( const OGRCurveCollection& )          */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurveCollection::OGRCurveCollection( const OGRCurveCollection& other )
{
    if( other.nCurveCount > 0 )
    {
        nCurveCount = other.nCurveCount;
        papoCurves = static_cast<OGRCurve **>(
            VSI_CALLOC_VERBOSE(sizeof(void*), nCurveCount));

        if( papoCurves )
        {
            for( int i = 0; i < nCurveCount; i++ )
            {
                papoCurves[i] = other.papoCurves[i]->clone();
            }
        }
    }
}

/************************************************************************/
/*                         ~OGRCurveCollection()                        */
/************************************************************************/

OGRCurveCollection::~OGRCurveCollection()

{
    empty(nullptr);
}

/************************************************************************/
/*                 operator=( const OGRCurveCollection& )               */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurveCollection&
OGRCurveCollection::operator=( const OGRCurveCollection& other )
{
    if( this != &other)
    {
        empty(nullptr);

        if( other.nCurveCount > 0 )
        {
            nCurveCount = other.nCurveCount;
            papoCurves = static_cast<OGRCurve **>(
                VSI_MALLOC2_VERBOSE(sizeof(void*), nCurveCount));

            if( papoCurves )
            {
                for( int i = 0; i < nCurveCount; i++ )
                {
                    papoCurves[i] = other.papoCurves[i]->clone();
                }
            }
        }
    }
    return *this;
}

/************************************************************************/
/*                              WkbSize()                               */
/************************************************************************/

size_t OGRCurveCollection::WkbSize() const
{
    size_t nSize = 9;

    for( auto&& poSubGeom: *this )
    {
        nSize += poSubGeom->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                          addCurveDirectly()                          */
/************************************************************************/

OGRErr OGRCurveCollection::addCurveDirectly( OGRGeometry* poGeom,
                                             OGRCurve* poCurve,
                                             int bNeedRealloc )
{
    poGeom->HomogenizeDimensionalityWith(poCurve);

    if( bNeedRealloc )
    {
        OGRCurve** papoNewCurves = static_cast<OGRCurve **>(
            VSI_REALLOC_VERBOSE(papoCurves,
                                sizeof(OGRCurve*) * (nCurveCount + 1)));
        if( papoNewCurves == nullptr )
            return OGRERR_FAILURE;
        papoCurves = papoNewCurves;
    }

    papoCurves[nCurveCount] = poCurve;

    nCurveCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        importPreambleFromWkb()                      */
/************************************************************************/

OGRErr OGRCurveCollection::importPreambleFromWkb( OGRGeometry* poGeom,
                                                   const unsigned char * pabyData,
                                                   size_t& nSize,
                                                   size_t& nDataOffset,
                                                   OGRwkbByteOrder& eByteOrder,
                                                   size_t nMinSubGeomSize,
                                                   OGRwkbVariant eWkbVariant )
{
    OGRErr eErr = poGeom->importPreambleOfCollectionFromWkb(
                                                        pabyData,
                                                        nSize,
                                                        nDataOffset,
                                                        eByteOrder,
                                                        nMinSubGeomSize,
                                                        nCurveCount,
                                                        eWkbVariant );
    if( eErr != OGRERR_NONE )
        return eErr;

    // coverity[tainted_data]
    papoCurves = static_cast<OGRCurve **>(
        VSI_CALLOC_VERBOSE(sizeof(void*), nCurveCount));
    if( nCurveCount != 0 && papoCurves == nullptr )
    {
        nCurveCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       importBodyFromWkb()                            */
/************************************************************************/

OGRErr OGRCurveCollection::importBodyFromWkb(
    OGRGeometry* poGeom,
    const unsigned char * pabyData,
    size_t nSize,
    bool bAcceptCompoundCurve,
    OGRErr (*pfnAddCurveDirectlyFromWkb)(OGRGeometry* poGeom,
                                         OGRCurve* poCurve),
    OGRwkbVariant eWkbVariant,
    size_t& nBytesConsumedOut )
{
    nBytesConsumedOut = 0;
/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    const int nIter = nCurveCount;
    nCurveCount = 0;
    size_t nDataOffset = 0;
    for( int iGeom = 0; iGeom < nIter; iGeom++ )
    {
        OGRGeometry* poSubGeom = nullptr;

        // Parses sub-geometry.
        const unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != static_cast<size_t>(-1) )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eFlattenSubGeomType = wkbUnknown;
        if( OGRReadWKBGeometryType( pabySubData, eWkbVariant,
                                    &eFlattenSubGeomType ) != OGRERR_NONE )
            return OGRERR_FAILURE;
        eFlattenSubGeomType = wkbFlatten(eFlattenSubGeomType);

        OGRErr eErr = OGRERR_NONE;
        size_t nSubGeomBytesConsumedOut = 0;
        if( (eFlattenSubGeomType != wkbCompoundCurve &&
             OGR_GT_IsCurve(eFlattenSubGeomType)) ||
            (bAcceptCompoundCurve && eFlattenSubGeomType == wkbCompoundCurve) )
        {
            eErr = OGRGeometryFactory::
                createFromWkb( pabySubData, nullptr,
                               &poSubGeom, nSize, eWkbVariant,
                               nSubGeomBytesConsumedOut );
        }
        else
        {
            CPLDebug(
                "OGR",
                "Cannot add geometry of type (%d) to geometry of type (%d)",
                eFlattenSubGeomType, poGeom->getGeometryType());
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        if( eErr == OGRERR_NONE )
        {
            CPLAssert( nSubGeomBytesConsumedOut > 0 );
            if( nSize != static_cast<size_t>(-1) )
            {
                CPLAssert( nSize >= nSubGeomBytesConsumedOut );
                nSize -= nSubGeomBytesConsumedOut;
            }

            nDataOffset += nSubGeomBytesConsumedOut;

            OGRCurve *poCurve = poSubGeom->toCurve();
            eErr = pfnAddCurveDirectlyFromWkb(poGeom, poCurve);
        }
        if( eErr != OGRERR_NONE )
        {
            delete poSubGeom;
            return eErr;
        }

    }
    nBytesConsumedOut = nDataOffset;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

std::string OGRCurveCollection::exportToWkt(const OGRGeometry *baseGeom,
    const OGRWktOptions& opts, OGRErr *err) const
{
    try
    {
        bool first = true;
        std::string wkt(baseGeom->getGeometryName());

        OGRWktOptions optsModified(opts);
        optsModified.variant = wkbVariantIso;
        wkt += baseGeom->wktTypeString(optsModified.variant);

        for (int i = 0; i < nCurveCount; ++i)
        {
            OGRGeometry *geom = papoCurves[i];

            OGRErr subgeomErr = OGRERR_NONE;
            std::string tempWkt = geom->exportToWkt(optsModified, &subgeomErr);
            if (subgeomErr != OGRERR_NONE)
            {
                if( err )
                    *err = subgeomErr;
                return std::string();
            }

            // A curve collection has a list of linestrings (OGRCompoundCurve),
            // which should have their leader removed, or a OGRCurvePolygon,
            // which has leaders for each of its sub-geometries that aren't
            // linestrings.
            if (tempWkt.compare(0, strlen("LINESTRING"), "LINESTRING") == 0)
            {
                auto pos = tempWkt.find('(');
                if (pos != std::string::npos)
                    tempWkt = tempWkt.substr(pos);
            }

            if (tempWkt.find("EMPTY") != std::string::npos)
                continue;

            if (first)
                wkt += '(';
            else
                wkt += ',';
            first = false;
            wkt += tempWkt;
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

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/

OGRErr OGRCurveCollection::exportToWkb( const OGRGeometry* poGeom,
                                        OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData,
                                        OGRwkbVariant eWkbVariant ) const
{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER(static_cast<unsigned char>(eByteOrder));

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = poGeom->getIsoGeometryType();
    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        const bool bIs3D = wkbHasZ(static_cast<OGRwkbGeometryType>(nGType));
        nGType = wkbFlatten(nGType);
        if( nGType == wkbCurvePolygon )
            nGType = POSTGIS15_CURVEPOLYGON;
        if( bIs3D )
            // Explicitly set wkb25DBit.
            nGType = static_cast<OGRwkbGeometryType>(nGType | wkb25DBitInternalUse);
    }

    if( OGR_SWAP( eByteOrder ) )
    {
        nGType = CPL_SWAP32(nGType);
    }

    memcpy( pabyData + 1, &nGType, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        const int nCount = CPL_SWAP32( nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nCurveCount, 4 );
    }

    // TODO(schwehr): Where do these 9 values come from?
    size_t nOffset = 9;

/* ==================================================================== */
/*      Serialize each of the Geoms.                                    */
/* ==================================================================== */
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->exportToWkb( eByteOrder, pabyData + nOffset,
                                        eWkbVariant );

        nOffset += poSubGeom->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRCurveCollection::empty( OGRGeometry* poGeom )
{
    if( papoCurves != nullptr )
    {
        for( auto&& poSubGeom: *this )
        {
            delete poSubGeom;
        }
        CPLFree( papoCurves );
    }

    nCurveCount = 0;
    papoCurves = nullptr;
    if( poGeom )
        poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope * psEnvelope ) const
{
    OGREnvelope3D oEnv3D;
    getEnvelope(&oEnv3D);
    psEnvelope->MinX = oEnv3D.MinX;
    psEnvelope->MinY = oEnv3D.MinY;
    psEnvelope->MaxX = oEnv3D.MaxX;
    psEnvelope->MaxY = oEnv3D.MaxY;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    OGREnvelope3D oGeomEnv;
    bool bExtentSet = false;

    *psEnvelope = OGREnvelope3D();
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !papoCurves[iGeom]->IsEmpty() )
        {
            bExtentSet = true;
            papoCurves[iGeom]->getEnvelope( &oGeomEnv );
            psEnvelope->Merge( oGeomEnv );
        }
    }

    if( !bExtentSet )
    {
        // To be backward compatible when called on empty geom
        psEnvelope->MinX = 0.0;
        psEnvelope->MinY = 0.0;
        psEnvelope->MinZ = 0.0;
        psEnvelope->MaxX = 0.0;
        psEnvelope->MaxY = 0.0;
        psEnvelope->MaxZ = 0.0;
    }
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCurveCollection::IsEmpty() const
{
    for( auto&& poSubGeom: *this )
    {
        if( !poSubGeom->IsEmpty() )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean OGRCurveCollection::Equals( const OGRCurveCollection *poOCC ) const
{
    if( getNumCurves() != poOCC->getNumCurves() )
        return FALSE;

    // Should eventually test the SRS.

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !getCurve(iGeom)->Equals(poOCC->getCurve(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCurveCollection::setCoordinateDimension( OGRGeometry* poGeom,
                                                 int nNewDimension )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->setCoordinateDimension( nNewDimension );
    }

    poGeom->OGRGeometry::setCoordinateDimension( nNewDimension );
}

void OGRCurveCollection::set3D( OGRGeometry* poGeom, OGRBoolean bIs3D )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->set3D( bIs3D );
    }

    poGeom->OGRGeometry::set3D( bIs3D );
}

void OGRCurveCollection::setMeasured( OGRGeometry* poGeom,
                                      OGRBoolean bIsMeasured )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->setMeasured( bIsMeasured );
    }

    poGeom->OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

void OGRCurveCollection::assignSpatialReference( OGRGeometry* poGeom,
                                                 OGRSpatialReference * poSR )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->assignSpatialReference( poSR );
    }
    poGeom->OGRGeometry::assignSpatialReference( poSR );
}

/************************************************************************/
/*                          getNumCurves()                              */
/************************************************************************/

int OGRCurveCollection::getNumCurves() const
{
    return nCurveCount;
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

OGRCurve *OGRCurveCollection::getCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return nullptr;
    return papoCurves[i];
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

const OGRCurve *OGRCurveCollection::getCurve( int i ) const
{
    if( i < 0 || i >= nCurveCount )
        return nullptr;
    return papoCurves[i];
}

/************************************************************************/
/*                           stealCurve()                               */
/************************************************************************/

OGRCurve* OGRCurveCollection::stealCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return nullptr;
    OGRCurve* poRet = papoCurves[i];
    if( i < nCurveCount - 1 )
    {
        memmove(papoCurves + i,
                papoCurves + i + 1,
                (nCurveCount - i - 1) * sizeof(OGRCurve*));
    }
    nCurveCount--;
    return poRet;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRCurveCollection::transform( OGRGeometry* poGeom,
                                      OGRCoordinateTransformation *poCT )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        const OGRErr eErr = papoCurves[iGeom]->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iGeom != 0 )
            {
                CPLDebug("OGR",
                         "OGRCurveCollection::transform() failed for a "
                         "geometry other than the first, meaning some "
                         "geometries are transformed and some are not!" );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
    }

    poGeom->assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRCurveCollection::flattenTo2D(OGRGeometry* poGeom)
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->flattenTo2D();
    }

    poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCurveCollection::segmentize(double dfMaxLength)
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->segmentize(dfMaxLength);
    }
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRCurveCollection::swapXY()
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->swapXY();
    }
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCurveCollection::hasCurveGeometry(int bLookForNonLinear) const
{
    for( auto&& poSubGeom: *this )
    {
        if( poSubGeom->hasCurveGeometry(bLookForNonLinear) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                           removeCurve()                              */
/************************************************************************/

/**
 * \brief Remove a geometry from the container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * @param iIndex the index of the geometry to delete.  A value of -1 is a
 * special flag meaning that all geometries should be removed.
 *
 * @param bDelete if true the geometry will be deallocated, otherwise it will
 * not.  The default is true as the container is considered to own the
 * geometries in it.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is
 * out of range.
 */

OGRErr OGRCurveCollection::removeCurve( int iIndex, bool bDelete )

{
    if( iIndex < -1 || iIndex >= nCurveCount )
        return OGRERR_FAILURE;

    // Special case.
    if( iIndex == -1 )
    {
        while( nCurveCount > 0 )
            removeCurve( nCurveCount-1, bDelete );
        return OGRERR_NONE;
    }

    if( bDelete )
        delete papoCurves[iIndex];

    memmove( papoCurves + iIndex, papoCurves + iIndex + 1,
             sizeof(void*) * (nCurveCount-iIndex-1) );

    nCurveCount--;

    return OGRERR_NONE;
}

//! @endcond
