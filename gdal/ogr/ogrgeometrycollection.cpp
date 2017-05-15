/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRGeometryCollection class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRGeometryCollection()                        */
/************************************************************************/

/**
 * \brief Create an empty geometry collection.
 */

OGRGeometryCollection::OGRGeometryCollection()

{
    nGeomCount = 0;
    papoGeoms = NULL;
}

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
    OGRGeometry(other),
    nGeomCount(0),
    papoGeoms(NULL)
{
    for( int i = 0; i < other.nGeomCount; i++ )
    {
        addGeometry( other.papoGeoms[i] );
    }
}

/************************************************************************/
/*                       ~OGRGeometryCollection()                       */
/************************************************************************/

OGRGeometryCollection::~OGRGeometryCollection()

{
    empty();
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
    if( papoGeoms != NULL )
    {
        for( int i = 0; i < nGeomCount; i++ )
        {
            delete papoGeoms[i];
        }
        CPLFree( papoGeoms );
    }

    nGeomCount = 0;
    papoGeoms = NULL;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRGeometryCollection::clone() const

{
    OGRGeometryCollection *poNewGC = dynamic_cast<OGRGeometryCollection *>(
        OGRGeometryFactory::createGeometry(getGeometryType()));
    if( poNewGC == NULL )
        return NULL;
    poNewGC->assignSpatialReference( getSpatialReference() );
    poNewGC->flags = flags;

    for( int i = 0; i < nGeomCount; i++ )
    {
        if( poNewGC->addGeometry( papoGeoms[i] ) != OGRERR_NONE )
        {
            delete poNewGC;
            return NULL;
        }
    }

    return poNewGC;
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
    for( int i = 0; i < nGeomCount; i++ )
    {
        int nSubGeomDimension = papoGeoms[i]->getDimension();
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
    for( int i = 0; i < nGeomCount; i++ )
        papoGeoms[i]->flattenTo2D();

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
        return NULL;

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
        return NULL;

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
    if( poClone == NULL )
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

    if( poNewGeom->Is3D() && !Is3D() )
        set3D(TRUE);

    if( poNewGeom->IsMeasured() && !IsMeasured() )
        setMeasured(TRUE);

    if( !poNewGeom->Is3D() && Is3D() )
        poNewGeom->set3D(TRUE);

    if( !poNewGeom->IsMeasured() && IsMeasured() )
        poNewGeom->setMeasured(TRUE);

    OGRGeometry** papoNewGeoms = static_cast<OGRGeometry **>(
        VSI_REALLOC_VERBOSE(papoGeoms, sizeof(void*) * (nGeomCount + 1)));
    if( papoNewGeoms == NULL )
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
    OGRErr eErr = importPreambuleOfCollectionFromWkb( pabyData,
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
    if( nGeomCount != 0 && papoGeoms == NULL )
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

        OGRGeometry* poSubGeom = NULL;
        int nSubGeomBytesConsumed = -1;
        if( OGR_GT_IsSubClassOf(eSubGeomType, wkbGeometryCollection) )
        {
            poSubGeom = OGRGeometryFactory::createGeometry( eSubGeomType );
            if( poSubGeom == NULL )
                eErr = OGRERR_FAILURE;
            else
                eErr = ((OGRGeometryCollection*)poSubGeom)->
                        importFromWkbInternal( pabySubData, nSize,
                                               nRecLevel + 1, eWkbVariant,
                                               nSubGeomBytesConsumed );
        }
        else
        {
            eErr = OGRGeometryFactory::
                createFromWkb( pabySubData, NULL,
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
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

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
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse);
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
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset,
                                       eWkbVariant );
        // Should normally not happen if everyone else does its job,
        // but has happened sometimes. (#6332)
        if( papoGeoms[iGeom]->getCoordinateDimension() !=
            getCoordinateDimension() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Sub-geometry %d has coordinate dimension %d, "
                     "but container has %d",
                     iGeom,
                     papoGeoms[iGeom]->getCoordinateDimension(),
                     getCoordinateDimension() );
        }

        nOffset += papoGeoms[iGeom]->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       importFromWktInternal()                        */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWktInternal( char ** ppszInput,
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
    OGRErr eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
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
        OGRGeometry *poGeom = NULL;

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        OGRWktReadToken( pszInput, szToken );

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        if( EQUAL(szToken, "GEOMETRYCOLLECTION") )
        {
            poGeom = new OGRGeometryCollection();
            eErr = ((OGRGeometryCollection*)poGeom)->
                        importFromWktInternal( (char **) &pszInput,
                                               nRecLevel + 1 );
        }
        else
            eErr = OGRGeometryFactory::createFromWkt( (char **) &pszInput,
                                                       NULL, &poGeom );

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

    *ppszInput = (char *) pszInput;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkt( char ** ppszInput )

{
    return importFromWktInternal(ppszInput, 0);
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivalent.  This could be made a lot more CPU efficient.       */
/************************************************************************/

OGRErr OGRGeometryCollection::exportToWkt( char ** ppszDstText,
                                           OGRwkbVariant eWkbVariant ) const
{
    return exportToWktInternal(ppszDstText, eWkbVariant, NULL);
}

//! @cond Doxygen_Suppress
OGRErr
OGRGeometryCollection::exportToWktInternal( char ** ppszDstText,
                                            OGRwkbVariant eWkbVariant,
                                            const char* pszSkipPrefix ) const

{
    size_t nCumulativeLength = 0;
    OGRErr eErr = OGRERR_NONE;
    bool bMustWriteComma = false;

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    char **papszGeoms =
        nGeomCount
        ? static_cast<char **>(CPLCalloc(sizeof(char *), nGeomCount))
        : NULL;

    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        eErr = papoGeoms[iGeom]->exportToWkt(&(papszGeoms[iGeom]), eWkbVariant);
        if( eErr != OGRERR_NONE )
            goto error;

        size_t nSkip = 0;
        if( pszSkipPrefix != NULL &&
            EQUALN(papszGeoms[iGeom], pszSkipPrefix, strlen(pszSkipPrefix)) &&
            papszGeoms[iGeom][strlen(pszSkipPrefix)] == ' ' )
        {
            nSkip = strlen(pszSkipPrefix) + 1;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "ZM ") )
                nSkip += 3;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "M ") )
                nSkip += 2;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "Z ") )
                nSkip += 2;

            // Skip empty subgeoms.
            if( papszGeoms[iGeom][nSkip] != '(' )
            {
                CPLDebug( "OGR",
                          "OGRGeometryCollection::exportToWkt() - skipping %s.",
                          papszGeoms[iGeom] );
                CPLFree( papszGeoms[iGeom] );
                papszGeoms[iGeom] = NULL;
                continue;
            }
        }
        else if( eWkbVariant != wkbVariantIso )
        {
            char *substr = NULL;
            // TODO(schwehr): Looks dangerous.  Cleanup.
            if( (substr = strstr(papszGeoms[iGeom], " Z")) != NULL )
                memmove(substr,
                        substr + strlen(" Z"),
                        1 + strlen(substr + strlen(" Z")));
        }

        nCumulativeLength += strlen(papszGeoms[iGeom] + nSkip);
    }

/* -------------------------------------------------------------------- */
/*      Return XXXXXXXXXXXXXXX EMPTY if we get no valid line string.    */
/* -------------------------------------------------------------------- */
    if( nCumulativeLength == 0 )
    {
        CPLFree( papszGeoms );
        CPLString osEmpty;
        if( eWkbVariant == wkbVariantIso )
        {
            if( Is3D() && IsMeasured() )
                osEmpty.Printf("%s ZM EMPTY", getGeometryName());
            else if( IsMeasured() )
                osEmpty.Printf("%s M EMPTY", getGeometryName());
            else if( Is3D() )
                osEmpty.Printf("%s Z EMPTY", getGeometryName());
            else
                osEmpty.Printf("%s EMPTY", getGeometryName());
        }
        else
        {
            osEmpty.Printf("%s EMPTY", getGeometryName());
        }
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = static_cast<char *>(
        VSI_MALLOC_VERBOSE(nCumulativeLength + nGeomCount + 26));

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, getGeometryName() );
    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            strcat( *ppszDstText, " ZM" );
        else if( flags & OGR_G_3D )
            strcat( *ppszDstText, " Z" );
        else if( flags & OGR_G_MEASURED )
            strcat( *ppszDstText, " M" );
    }
    strcat( *ppszDstText, " (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( papszGeoms[iGeom] == NULL )
            continue;

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = true;

        size_t nSkip = 0;
        if( pszSkipPrefix != NULL &&
            EQUALN(papszGeoms[iGeom], pszSkipPrefix, strlen(pszSkipPrefix)) &&
            papszGeoms[iGeom][strlen(pszSkipPrefix)] == ' ' )
        {
            nSkip = strlen(pszSkipPrefix) + 1;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "ZM ") )
                nSkip += 3;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "M ") )
                nSkip += 2;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "Z ") )
                nSkip += 2;
        }

        const size_t nGeomLength = strlen(papszGeoms[iGeom] + nSkip);
        memcpy( *ppszDstText + nCumulativeLength,
                papszGeoms[iGeom] + nSkip,
                nGeomLength );
        nCumulativeLength += nGeomLength;
        VSIFree( papszGeoms[iGeom] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszGeoms );

    return OGRERR_NONE;

error:
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        CPLFree( papszGeoms[iGeom] );
    CPLFree( papszGeoms );
    return eErr;
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
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( !papoGeoms[iGeom]->IsEmpty() )
        {
            bExtentSet = true;
            papoGeoms[iGeom]->getEnvelope( &oGeomEnv );
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

OGRBoolean OGRGeometryCollection::Equals( OGRGeometry * poOther ) const

{
    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    OGRGeometryCollection *poOGC = (OGRGeometryCollection *) poOther;
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
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        const OGRErr eErr = papoGeoms[iGeom]->transform( poCT );
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
    }

    assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRGeometryCollection::closeRings()

{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( wkbFlatten(papoGeoms[iGeom]->getGeometryType()) == wkbPolygon )
        {
            OGRPolygon *poPoly = dynamic_cast<OGRPolygon *>(papoGeoms[iGeom]);
            if( poPoly == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRPolygon.");
                return;
            }
            poPoly->closeRings();
        }
    }
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRGeometryCollection::setCoordinateDimension( int nNewDimension )

{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->setCoordinateDimension( nNewDimension );
    }

    OGRGeometry::setCoordinateDimension( nNewDimension );
}

void OGRGeometryCollection::set3D( OGRBoolean bIs3D )
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->set3D( bIs3D );
    }

    OGRGeometry::set3D( bIs3D );
}

void OGRGeometryCollection::setMeasured( OGRBoolean bIsMeasured )
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->setMeasured( bIsMeasured );
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
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRGeometry* geom = papoGeoms[iGeom];
        const OGRwkbGeometryType eType = wkbFlatten(geom->getGeometryType());
        if( OGR_GT_IsCurve(eType) )
        {
            OGRCurve *poCurve = dynamic_cast<OGRCurve *>(geom);
            if( poCurve == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRCurve.");
                return 0.0;
            }
            dfLength += poCurve->get_Length();
        }
        else if( OGR_GT_IsSubClassOf(eType, wkbMultiCurve) ||
                 eType == wkbGeometryCollection )
        {
            OGRGeometryCollection *poColl =
                dynamic_cast<OGRGeometryCollection *>(geom);
            if( poColl == NULL )
            {
                CPLError(
                    CE_Fatal, CPLE_AppDefined,
                    "dynamic_cast failed.  Expected OGRGeometryCollection.");
                return 0.0;
            }
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
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRGeometry* geom = papoGeoms[iGeom];
        OGRwkbGeometryType eType = wkbFlatten(geom->getGeometryType());
        if( OGR_GT_IsSurface(eType) )
        {
            OGRSurface *poSurface = dynamic_cast<OGRSurface *>(geom);
            if( poSurface == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRSurface.");
                return 0.0;
            }
            dfArea += poSurface->get_Area();
        }
        else if( OGR_GT_IsCurve(eType) )
        {
            OGRCurve *poCurve = dynamic_cast<OGRCurve *>(geom);
            if( poCurve == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRCurve.");
                return 0.0;
            }
            dfArea += poCurve->get_Area();
        }
        else if( OGR_GT_IsSubClassOf(eType, wkbMultiSurface) ||
                eType == wkbGeometryCollection )
        {
            dfArea += ((OGRGeometryCollection *) geom)->get_Area();
        }
    }

    return dfArea;
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRGeometryCollection::IsEmpty() const
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        if( papoGeoms[iGeom]->IsEmpty() == FALSE )
            return FALSE;
    return TRUE;
}

/************************************************************************/
/*              OGRGeometryCollection::segmentize()                     */
/************************************************************************/

void OGRGeometryCollection::segmentize( double dfMaxLength )
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        papoGeoms[iGeom]->segmentize(dfMaxLength);
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRGeometryCollection::swapXY()
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        papoGeoms[iGeom]->swapXY();
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
    OGRGeometryCollection* poGC = (OGRGeometryCollection*)
        OGRGeometryFactory::createGeometry(OGR_GT_GetLinear(getGeometryType()));
    if( poGC == NULL )
        return NULL;
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
    OGRGeometryCollection* poGC = (OGRGeometryCollection*)
        OGRGeometryFactory::createGeometry(OGR_GT_GetCurve(getGeometryType()));
    if( poGC == NULL )
        return NULL;
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
    poSrc->papoGeoms = NULL;
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
