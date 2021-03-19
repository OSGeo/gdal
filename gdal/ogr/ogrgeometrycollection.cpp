/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRGeometryCollection class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRGeometryCollection()                        */
/************************************************************************/

/**
 * \brief Create an empty geometry collection.
 */

OGRGeometryCollection::OGRGeometryCollection() = default;

/************************************************************************/
/*         OGRGeometryCollection( const OGRGeometryCollection& )        */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRGeometryCollection::OGRGeometryCollection(
    const OGRGeometryCollection& other ) :
    OGRGeometry(other)
{
    // Do not use addGeometry() as it is virtual.
    papoGeoms = static_cast<OGRGeometry **>(
        VSI_CALLOC_VERBOSE(sizeof(void*), other.nGeomCount));
    if( papoGeoms )
    {
        nGeomCount = other.nGeomCount;
        for( int i = 0; i < other.nGeomCount; i++ )
        {
            papoGeoms[i] = other.papoGeoms[i]->clone();
        }
    }
}

/************************************************************************/
/*                       ~OGRGeometryCollection()                       */
/************************************************************************/

OGRGeometryCollection::~OGRGeometryCollection()

{
    OGRGeometryCollection::empty();
}

/************************************************************************/
/*               operator=( const OGRGeometryCollection&)               */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRGeometryCollection& OGRGeometryCollection::operator=(
    const OGRGeometryCollection& other )
{
    if( this != &other)
    {
        empty();

        OGRGeometry::operator=( other );

        for( int i = 0; i < other.nGeomCount; i++ )
        {
            addGeometry( other.papoGeoms[i] );
        }
    }
    return *this;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRGeometryCollection::empty()

{
    if( papoGeoms != nullptr )
    {
        for( auto&& poSubGeom: *this )
        {
            delete poSubGeom;
        }
        CPLFree( papoGeoms );
    }

    nGeomCount = 0;
    papoGeoms = nullptr;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometryCollection *OGRGeometryCollection::clone() const

{
    return new (std::nothrow) OGRGeometryCollection(*this);
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRGeometryCollection::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbGeometryCollectionZM;
    else if( flags & OGR_G_MEASURED )
        return wkbGeometryCollectionM;
    else if( flags & OGR_G_3D )
        return wkbGeometryCollection25D;
    else
        return wkbGeometryCollection;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRGeometryCollection::getDimension() const

{
    int nDimension = 0;
    // FIXME? Not sure if it is really appropriate to take the max in case
    // of geometries of different dimension.
    for( auto&& poSubGeom: *this )
    {
        int nSubGeomDimension = poSubGeom->getDimension();
        if( nSubGeomDimension > nDimension )
        {
            nDimension = nSubGeomDimension;
            if( nDimension == 2 )
                break;
        }
    }
    return nDimension;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRGeometryCollection::flattenTo2D()

{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->flattenTo2D();
    }

    flags &= ~OGR_G_3D;
    flags &= ~OGR_G_MEASURED;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRGeometryCollection::getGeometryName() const

{
    return "GEOMETRYCOLLECTION";
}

/************************************************************************/
/*                          getNumGeometries()                          */
/************************************************************************/

/**
 * \brief Fetch number of geometries in container.
 *
 * This method relates to the SFCOM IGeometryCollect::get_NumGeometries()
 * method.
 *
 * @return count of children geometries.  May be zero.
 */

int OGRGeometryCollection::getNumGeometries() const

{
    return nGeomCount;
}

/************************************************************************/
/*                           getGeometryRef()                           */
/************************************************************************/

/**
 * \brief Fetch geometry from container.
 *
 * This method returns a pointer to a geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid until the next change to the
 * geometry container.  Use IGeometry::clone() to make a copy.
 *
 * This method relates to the SFCOM IGeometryCollection::get_Geometry() method.
 *
 * @param i the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return pointer to requested geometry.
 */

OGRGeometry * OGRGeometryCollection::getGeometryRef( int i )

{
    if( i < 0 || i >= nGeomCount )
        return nullptr;

    return papoGeoms[i];
}

/**
 * \brief Fetch geometry from container.
 *
 * This method returns a pointer to a geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid until the next change to the
 * geometry container.  Use IGeometry::clone() to make a copy.
 *
 * This method relates to the SFCOM IGeometryCollection::get_Geometry() method.
 *
 * @param i the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return pointer to requested geometry.
 */

const OGRGeometry * OGRGeometryCollection::getGeometryRef( int i ) const

{
    if( i < 0 || i >= nGeomCount )
        return nullptr;

    return papoGeoms[i];
}

/************************************************************************/
/*                            addGeometry()                             */
/*                                                                      */
/*      Add a new geometry to a collection.  Subclasses should          */
/*      override this to verify the type of the new geometry, and       */
/*      then call this method to actually add it.                       */
/************************************************************************/

/**
 * \brief Add a geometry to the container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  The passed geometry is cloned
 * to make an internal copy.
 *
 * There is no SFCOM analog to this method.
 *
 * This method is the same as the C function OGR_G_AddGeometry().
 *
 * @param poNewGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of geometry container.
 */

OGRErr OGRGeometryCollection::addGeometry( const OGRGeometry * poNewGeom )

{
    OGRGeometry *poClone = poNewGeom->clone();
    if( poClone == nullptr )
        return OGRERR_FAILURE;

    const OGRErr eErr = addGeometryDirectly( poClone );
    if( eErr != OGRERR_NONE )
        delete poClone;

    return eErr;
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/*                                                                      */
/*      Add a new geometry to a collection.  Subclasses should          */
/*      override this to verify the type of the new geometry, and       */
/*      then call this method to actually add it.                       */
/************************************************************************/

/**
 * \brief Add a geometry directly to the container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  Ownership of the passed
 * geometry is taken by the container rather than cloning as addGeometry()
 * does.
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

OGRErr OGRGeometryCollection::addGeometryDirectly( OGRGeometry * poNewGeom )

{
    if( !isCompatibleSubType(poNewGeom->getGeometryType()) )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    HomogenizeDimensionalityWith(poNewGeom);

    OGRGeometry** papoNewGeoms = static_cast<OGRGeometry **>(
        VSI_REALLOC_VERBOSE(papoGeoms, sizeof(void*) * (nGeomCount + 1)));
    if( papoNewGeoms == nullptr )
        return OGRERR_FAILURE;

    papoGeoms = papoNewGeoms;
    papoGeoms[nGeomCount] = poNewGeom;

    nGeomCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           removeGeometry()                           */
/************************************************************************/

/**
 * \brief Remove a geometry from the container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * There is no SFCOM analog to this method.
 *
 * This method is the same as the C function OGR_G_RemoveGeometry().
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

OGRErr OGRGeometryCollection::removeGeometry( int iGeom, int bDelete )

{
    if( iGeom < -1 || iGeom >= nGeomCount )
        return OGRERR_FAILURE;

    // Special case.
    if( iGeom == -1 )
    {
        while( nGeomCount > 0 )
            removeGeometry( nGeomCount-1, bDelete );
        return OGRERR_NONE;
    }

    if( bDelete )
        delete papoGeoms[iGeom];

    memmove( papoGeoms + iGeom, papoGeoms + iGeom + 1,
             sizeof(void*) * (nGeomCount-iGeom-1) );

    nGeomCount--;

    return OGRERR_NONE;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRGeometryCollection::WkbSize() const

{
    int nSize = 9;

    for( int i = 0; i < nGeomCount; i++ )
    {
        nSize += papoGeoms[i]->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                       importFromWkbInternal()                        */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkbInternal( const unsigned char * pabyData,
                                                     int nSize, int nRecLevel,
                                                     OGRwkbVariant eWkbVariant,
                                                     int& nBytesConsumedOut )

{
    nBytesConsumedOut = -1;
    // Arbitrary value, but certainly large enough for reasonable use cases.
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many recursion levels (%d) while parsing WKB geometry.",
                  nRecLevel );
        return OGRERR_CORRUPT_DATA;
    }

    nGeomCount = 0;
    OGRwkbByteOrder eByteOrder = wkbXDR;
    int nDataOffset = 0;
    OGRErr eErr = importPreambleOfCollectionFromWkb( pabyData,
                                                      nSize,
                                                      nDataOffset,
                                                      eByteOrder,
                                                      9,
                                                      nGeomCount,
                                                      eWkbVariant );

    if( eErr != OGRERR_NONE )
        return eErr;

    // coverity[tainted_data]
    papoGeoms = static_cast<OGRGeometry **>(
        VSI_CALLOC_VERBOSE(sizeof(void*), nGeomCount));
    if( nGeomCount != 0 && papoGeoms == nullptr )
    {
        nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        // Parses sub-geometry.
        const unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eSubGeomType = wkbUnknown;
        eErr = OGRReadWKBGeometryType( pabySubData, eWkbVariant,
                                       &eSubGeomType );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !isCompatibleSubType(eSubGeomType) )
        {
            nGeomCount = iGeom;
            CPLDebug(
                "OGR",
                "Cannot add geometry of type (%d) to geometry of type (%d)",
                eSubGeomType, getGeometryType());
            return OGRERR_CORRUPT_DATA;
        }

        OGRGeometry* poSubGeom = nullptr;
        int nSubGeomBytesConsumed = -1;
        if( OGR_GT_IsSubClassOf(eSubGeomType, wkbGeometryCollection) )
        {
            poSubGeom = OGRGeometryFactory::createGeometry( eSubGeomType );
            if( poSubGeom == nullptr )
                eErr = OGRERR_FAILURE;
            else
                eErr = poSubGeom->toGeometryCollection()->
                        importFromWkbInternal( pabySubData, nSize,
                                               nRecLevel + 1, eWkbVariant,
                                               nSubGeomBytesConsumed );
        }
        else
        {
            eErr = OGRGeometryFactory::
                createFromWkb( pabySubData, nullptr,
                               &poSubGeom, nSize, eWkbVariant,
                               nSubGeomBytesConsumed );
        }

        if( eErr != OGRERR_NONE )
        {
            nGeomCount = iGeom;
            delete poSubGeom;
            return eErr;
        }

        papoGeoms[iGeom] = poSubGeom;

        if( papoGeoms[iGeom]->Is3D() )
            flags |= OGR_G_3D;
        if( papoGeoms[iGeom]->IsMeasured() )
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
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkb( const unsigned char * pabyData,
                                             int nSize,
                                             OGRwkbVariant eWkbVariant,
                                             int& nBytesConsumedOut )

{
    return importFromWkbInternal(pabyData, nSize, 0, eWkbVariant,
                                 nBytesConsumedOut);
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr OGRGeometryCollection::exportToWkb( OGRwkbByteOrder eByteOrder,
                                           unsigned char * pabyData,
                                           OGRwkbVariant eWkbVariant ) const

{
    if( eWkbVariant == wkbVariantOldOgc &&
        (wkbFlatten(getGeometryType()) == wkbMultiCurve ||
         wkbFlatten(getGeometryType()) == wkbMultiSurface) )
    {
        // Does not make sense for new geometries, so patch it.
        eWkbVariant = wkbVariantIso;
    }

/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER(static_cast<unsigned char>(eByteOrder));

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();

    if( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();
    else if( eWkbVariant == wkbVariantPostGIS1 )
    {
        const bool bIs3D = wkbHasZ(static_cast<OGRwkbGeometryType>(nGType));
        nGType = wkbFlatten(nGType);
        if( nGType == wkbMultiCurve )
            nGType = POSTGIS15_MULTICURVE;
        else if( nGType == wkbMultiSurface )
            nGType = POSTGIS15_MULTISURFACE;
        if( bIs3D )
            // Yes, explicitly set wkb25DBit.
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
        int nCount = CPL_SWAP32( nGeomCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nGeomCount, 4 );
    }

    int nOffset = 9;

/* ==================================================================== */
/*      Serialize each of the Geoms.                                    */
/* ==================================================================== */
    int iGeom = 0;
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->exportToWkb( eByteOrder, pabyData + nOffset,
                                       eWkbVariant );
        // Should normally not happen if everyone else does its job,
        // but has happened sometimes. (#6332)
        if( poSubGeom->getCoordinateDimension() !=
            getCoordinateDimension() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Sub-geometry %d has coordinate dimension %d, "
                     "but container has %d",
                     iGeom,
                     poSubGeom->getCoordinateDimension(),
                     getCoordinateDimension() );
        }

        nOffset += poSubGeom->WkbSize();
        iGeom ++;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       importFromWktInternal()                        */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWktInternal( const char ** ppszInput,
                                                     int nRecLevel )

{
    // Arbitrary value, but certainly large enough for reasonable usages.
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many recursion levels (%d) while parsing WKT geometry.",
                  nRecLevel );
        return OGRERR_CORRUPT_DATA;
    }

    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr eErr = importPreambleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    char szToken[OGR_WKT_TOKEN_MAX] = {};
    const char *pszInput = *ppszInput;

    // Skip first '('.
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each subgeometry in turn.                                  */
/* ==================================================================== */
    do
    {
        OGRGeometry *poGeom = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        OGRWktReadToken( pszInput, szToken );

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        if( STARTS_WITH_CI(szToken, "GEOMETRYCOLLECTION") )
        {
            OGRGeometryCollection* poGC = new OGRGeometryCollection();
            poGeom = poGC;
            eErr = poGC->importFromWktInternal( &pszInput,
                                               nRecLevel + 1 );
        }
        else
            eErr = OGRGeometryFactory::createFromWkt( &pszInput,
                                                       nullptr, &poGeom );

        if( eErr == OGRERR_NONE )
        {
            // If this has M, but not Z, it is an error if poGeom does
            // not have M.
            if( !Is3D() && IsMeasured() && !poGeom->IsMeasured() )
                eErr = OGRERR_CORRUPT_DATA;
            else
                eErr = addGeometryDirectly( poGeom );
        }
        if( eErr != OGRERR_NONE )
        {
            delete poGeom;
            return eErr;
        }

/* -------------------------------------------------------------------- */
/*      Read the delimiter following the ring.                          */
/* -------------------------------------------------------------------- */

        pszInput = OGRWktReadToken( pszInput, szToken );
    } while( szToken[0] == ',' );

/* -------------------------------------------------------------------- */
/*      freak if we don't get a closing bracket.                        */
/* -------------------------------------------------------------------- */
    if( szToken[0] != ')' )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = pszInput;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkt( const char ** ppszInput )

{
    return importFromWktInternal(ppszInput, 0);
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into its well known text format        */
/*      equivalent.                                                     */
/************************************************************************/

std::string OGRGeometryCollection::exportToWkt(const OGRWktOptions& opts,
                                               OGRErr *err) const
{
    return exportToWktInternal(opts, err);
}


//! @cond Doxygen_Suppress
std::string OGRGeometryCollection::exportToWktInternal(const OGRWktOptions& opts,
    OGRErr *err, std::string exclude) const
{
    bool first = true;
    const size_t excludeSize = exclude.size();
    std::string wkt(getGeometryName());
    wkt += wktTypeString(opts.variant);

    try
    {
        for (int i = 0; i < nGeomCount; ++i)
        {
            OGRGeometry *geom = papoGeoms[i];
            OGRErr subgeomErr = OGRERR_NONE;
            std::string tempWkt = geom->exportToWkt(opts, &subgeomErr);
            if (subgeomErr != OGRERR_NONE)
            {
                if (err)
                    *err = subgeomErr;
                return std::string();
            }

            // For some strange reason we exclude the typename leader when using
            // some geometries as part of a collection.
            if (excludeSize && (tempWkt.compare(0, excludeSize, exclude) == 0))
            {
                auto pos = tempWkt.find('(');
                // We won't have an opening paren if the geom is empty.
                if (pos == std::string::npos)
                    continue;
                tempWkt = tempWkt.substr(pos);
            }

            // Also strange, we allow the inclusion of ISO-only geometries (see
            // OGRPolyhedralSurface) in a non-iso geometry collection.  In order
            // to facilitate this, we need to rip the ISO bit from the string.
            if (opts.variant != wkbVariantIso)
            {
                std::string::size_type pos;
                if ((pos = tempWkt.find(" Z ")) != std::string::npos)
                    tempWkt.erase(pos + 1, 2);
                else if ((pos = tempWkt.find(" M ")) != std::string::npos)
                    tempWkt.erase(pos + 1, 2);
                else if ((pos = tempWkt.find(" ZM ")) != std::string::npos)
                    tempWkt.erase(pos + 1, 3);
            }

            if (first)
                wkt += '(';
            else
                wkt += ',';
            first = false;
            wkt += tempWkt;
        }

        if (err)
            *err = OGRERR_NONE;
        if (first)
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
/*                            getEnvelope()                             */
/************************************************************************/

void OGRGeometryCollection::getEnvelope( OGREnvelope * psEnvelope ) const

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

void OGRGeometryCollection::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    OGREnvelope3D oGeomEnv;
    bool bExtentSet = false;

    *psEnvelope = OGREnvelope3D();
    for( auto&& poSubGeom: *this )
    {
        if( !poSubGeom->IsEmpty() )
        {
            bExtentSet = true;
            poSubGeom->getEnvelope( &oGeomEnv );
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
/*                               Equals()                               */
/************************************************************************/

OGRBoolean OGRGeometryCollection::Equals( const OGRGeometry * poOther ) const

{
    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    auto poOGC = poOther->toGeometryCollection();
    if( getNumGeometries() != poOGC->getNumGeometries() )
        return FALSE;

    // TODO(schwehr): Should test the SRS.

    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( !getGeometryRef(iGeom)->Equals(poOGC->getGeometryRef(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRGeometryCollection::transform( OGRCoordinateTransformation *poCT )

{
    int iGeom  = 0;
    for( auto&& poSubGeom: *this )
    {
        const OGRErr eErr = poSubGeom->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iGeom != 0 )
            {
                CPLDebug("OGR",
                         "OGRGeometryCollection::transform() failed for a "
                         "geometry other than the first, meaning some "
                         "geometries are transformed and some are not." );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
        iGeom ++;
    }

    assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRGeometryCollection::closeRings()

{
    for( auto&& poSubGeom: *this )
    {
        if( OGR_GT_IsSubClassOf(
                     wkbFlatten(poSubGeom->getGeometryType()),
                                wkbCurvePolygon ) )
        {
            OGRCurvePolygon *poPoly = poSubGeom->toCurvePolygon();
            poPoly->closeRings();
        }
    }
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRGeometryCollection::setCoordinateDimension( int nNewDimension )

{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->setCoordinateDimension( nNewDimension );
    }

    OGRGeometry::setCoordinateDimension( nNewDimension );
}

void OGRGeometryCollection::set3D( OGRBoolean bIs3D )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->set3D( bIs3D );
    }

    OGRGeometry::set3D( bIs3D );
}

void OGRGeometryCollection::setMeasured( OGRBoolean bIsMeasured )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->setMeasured( bIsMeasured );
    }

    OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                              get_Length()                            */
/************************************************************************/

/**
 * \brief Compute the length of a multicurve.
 *
 * The length is computed as the sum of the length of all members
 * in this collection.
 *
 * @note No warning will be issued if a member of the collection does not
 *       support the get_Length method.
 *
 * @return computed length.
 */

double OGRGeometryCollection::get_Length() const
{
    double dfLength = 0.0;
    for( auto&& poSubGeom: *this )
    {
        const OGRwkbGeometryType eType = wkbFlatten(poSubGeom->getGeometryType());
        if( OGR_GT_IsCurve(eType) )
        {
            const OGRCurve *poCurve = poSubGeom->toCurve();
            dfLength += poCurve->get_Length();
        }
        else if( OGR_GT_IsSubClassOf(eType, wkbMultiCurve) ||
                 eType == wkbGeometryCollection )
        {
            const OGRGeometryCollection *poColl = poSubGeom->toGeometryCollection();
            dfLength += poColl->get_Length();
        }
    }

    return dfLength;
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

/**
 * \brief Compute area of geometry collection.
 *
 * The area is computed as the sum of the areas of all members
 * in this collection.
 *
 * @note No warning will be issued if a member of the collection does not
 *       support the get_Area method.
 *
 * @return computed area.
 */

double OGRGeometryCollection::get_Area() const
{
    double dfArea = 0.0;
    for( auto&& poSubGeom: *this )
    {
        OGRwkbGeometryType eType = wkbFlatten(poSubGeom->getGeometryType());
        if( OGR_GT_IsSurface(eType) )
        {
            const OGRSurface *poSurface = poSubGeom->toSurface();
            dfArea += poSurface->get_Area();
        }
        else if( OGR_GT_IsCurve(eType) )
        {
            const OGRCurve *poCurve = poSubGeom->toCurve();
            dfArea += poCurve->get_Area();
        }
        else if( OGR_GT_IsSubClassOf(eType, wkbMultiSurface) ||
                eType == wkbGeometryCollection )
        {
            dfArea += poSubGeom->toGeometryCollection()->get_Area();
        }
    }

    return dfArea;
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRGeometryCollection::IsEmpty() const
{
    for( auto&& poSubGeom: *this )
    {
        if( poSubGeom->IsEmpty() == FALSE )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

void OGRGeometryCollection::assignSpatialReference( OGRSpatialReference * poSR )
{
    OGRGeometry::assignSpatialReference(poSR);
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->assignSpatialReference(poSR);
    }
}

/************************************************************************/
/*              OGRGeometryCollection::segmentize()                     */
/************************************************************************/

void OGRGeometryCollection::segmentize( double dfMaxLength )
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->segmentize(dfMaxLength);
    }
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRGeometryCollection::swapXY()
{
    for( auto&& poSubGeom: *this )
    {
        poSubGeom->swapXY();
    }
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

/** Returns whether a geometry of the specified geometry type can be a
 * member of this collection.
 *
 * @param eSubType type of the potential member
 * @return TRUE or FALSE
 */

OGRBoolean OGRGeometryCollection::isCompatibleSubType(
    CPL_UNUSED OGRwkbGeometryType eSubType ) const
{
    // Accept all geometries as sub-geometries.
    return TRUE;
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRGeometryCollection::hasCurveGeometry(
    int bLookForNonLinear ) const
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( papoGeoms[iGeom]->hasCurveGeometry(bLookForNonLinear) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry* OGRGeometryCollection::getLinearGeometry(
    double dfMaxAngleStepSizeDegrees,
    const char* const* papszOptions ) const
{
    OGRGeometryCollection* poGC =
        OGRGeometryFactory::createGeometry(
            OGR_GT_GetLinear(getGeometryType()))->toGeometryCollection();
    if( poGC == nullptr )
        return nullptr;
    poGC->assignSpatialReference( getSpatialReference() );
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRGeometry* poSubGeom =
            papoGeoms[iGeom]->getLinearGeometry(dfMaxAngleStepSizeDegrees,
                                                papszOptions);
        poGC->addGeometryDirectly( poSubGeom );
    }
    return poGC;
}

/************************************************************************/
/*                             getCurveGeometry()                       */
/************************************************************************/

OGRGeometry* OGRGeometryCollection::getCurveGeometry(
    const char* const* papszOptions) const
{
    OGRGeometryCollection* poGC =
        OGRGeometryFactory::createGeometry(
            OGR_GT_GetCurve(getGeometryType()))->toGeometryCollection();
    if( poGC == nullptr )
        return nullptr;
    poGC->assignSpatialReference( getSpatialReference() );
    bool bHasCurveGeometry = false;
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRGeometry* poSubGeom =
            papoGeoms[iGeom]->getCurveGeometry(papszOptions);
        if( poSubGeom->hasCurveGeometry() )
            bHasCurveGeometry = true;
        poGC->addGeometryDirectly( poSubGeom );
    }
    if( !bHasCurveGeometry )
    {
        delete poGC;
        return clone();
    }
    return poGC;
}

/************************************************************************/
/*                      TransferMembersAndDestroy()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRGeometryCollection* OGRGeometryCollection::TransferMembersAndDestroy(
    OGRGeometryCollection* poSrc,
    OGRGeometryCollection* poDst )
{
    poDst->assignSpatialReference(poSrc->getSpatialReference());
    poDst->set3D(poSrc->Is3D());
    poDst->setMeasured(poSrc->IsMeasured());
    poDst->nGeomCount = poSrc->nGeomCount;
    poDst->papoGeoms = poSrc->papoGeoms;
    poSrc->nGeomCount = 0;
    poSrc->papoGeoms = nullptr;
    delete poSrc;
    return poDst;
}
//! @endcond

/************************************************************************/
/*                        CastToGeometryCollection()                    */
/************************************************************************/

/**
 * \brief Cast to geometry collection.
 *
 * This methods cast a derived class of geometry collection to a plain
 * geometry collection.
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure).
 *
 * @param poSrc the input geometry - ownership is passed to the method.
 * @return new geometry.
 * @since GDAL 2.2
 */

OGRGeometryCollection* OGRGeometryCollection::CastToGeometryCollection(
                                                OGRGeometryCollection* poSrc )
{
    if( wkbFlatten(poSrc->getGeometryType()) == wkbGeometryCollection )
        return poSrc;
    return TransferMembersAndDestroy(poSrc, new OGRGeometryCollection());
}
