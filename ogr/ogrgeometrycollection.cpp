/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRGeometryCollection class.
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
#include "ogr_p.h"

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
/*                       ~OGRGeometryCollection()                       */
/************************************************************************/

OGRGeometryCollection::~OGRGeometryCollection()

{
    empty();
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
        OGRFree( papoGeoms );
    }

    nGeomCount = 0;
    papoGeoms = NULL;
    nCoordDimension = 2;
}


/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRGeometryCollection::clone() const

{
    OGRGeometryCollection       *poNewGC;

    poNewGC = new OGRGeometryCollection;
    poNewGC->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < nGeomCount; i++ )
    {
        poNewGC->addGeometry( papoGeoms[i] );
    }

    return poNewGC;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRGeometryCollection::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbGeometryCollection25D;
    else
        return wkbGeometryCollection;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRGeometryCollection::getDimension() const

{
    return 2; // This isn't strictly correct.  It should be based on members.
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRGeometryCollection::flattenTo2D()

{
    for( int i = 0; i < nGeomCount; i++ )
        papoGeoms[i]->flattenTo2D();

    nCoordDimension = 2;
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
 * This method returns a pointer to an geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid untill the next change to the
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
    else
        return papoGeoms[i];
}

const OGRGeometry * OGRGeometryCollection::getGeometryRef( int i ) const

{
    if( i < 0 || i >= nGeomCount )
        return NULL;
    else
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
    OGRErr      eErr;

    eErr = addGeometryDirectly( poClone );
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
    papoGeoms = (OGRGeometry **) OGRRealloc( papoGeoms,
                                             sizeof(void*) * (nGeomCount+1) );

    papoGeoms[nGeomCount] = poNewGeom;

    nGeomCount++;

    if( poNewGeom->getCoordinateDimension() == 3 )
        nCoordDimension = 3;

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
    int         nSize = 9;

    for( int i = 0; i < nGeomCount; i++ )
    {
        nSize += papoGeoms[i]->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkb( unsigned char * pabyData,
                                             int nSize )

{
    OGRwkbByteOrder     eByteOrder;
    int                 nDataOffset;
    
    if( nSize < 9 && nSize != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = DB2_V72_FIX_BYTE_ORDER((OGRwkbByteOrder) *pabyData);
    if (!( eByteOrder == wkbXDR || eByteOrder == wkbNDR ))
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    OGRwkbGeometryType eGeometryType;

    if( eByteOrder == wkbNDR )
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
    }
    else
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[4];
    }

    if (! ( eGeometryType == wkbGeometryCollection
               || eGeometryType == wkbMultiPolygon 
               || eGeometryType == wkbMultiLineString 
               || eGeometryType == wkbMultiPoint ))
        return OGRERR_CORRUPT_DATA;
#endif    

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    empty();
    
/* -------------------------------------------------------------------- */
/*      Get the geometry count.                                         */
/* -------------------------------------------------------------------- */
    memcpy( &nGeomCount, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nGeomCount = CPL_SWAP32(nGeomCount);

    if (nGeomCount < 0 || nGeomCount > INT_MAX / 9)
    {
        nGeomCount = 0;
        return OGRERR_CORRUPT_DATA;
    }

    /* Each geometry has a minimum of 9 bytes */
    if (nSize != -1 && nSize - 9 < nGeomCount * 9)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_DATA;
    }

    papoGeoms = (OGRGeometry **) VSIMalloc2(sizeof(void*), nGeomCount);
    if (nGeomCount != 0 && papoGeoms == NULL)
    {
        nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    nDataOffset = 9;
    if( nSize != -1 )
        nSize -= nDataOffset;

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRErr  eErr;

        eErr = OGRGeometryFactory::
            createFromWkb( pabyData + nDataOffset, NULL,
                           papoGeoms + iGeom, nSize );

        if( eErr != OGRERR_NONE )
        {
            nGeomCount = iGeom;
            return eErr;
        }

        if (papoGeoms[iGeom]->getCoordinateDimension() == 3)
            nCoordDimension = 3;

        if( nSize != -1 )
            nSize -= papoGeoms[iGeom]->WkbSize();

        nDataOffset += papoGeoms[iGeom]->WkbSize();
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRGeometryCollection::exportToWkb( OGRwkbByteOrder eByteOrder,
                                            unsigned char * pabyData ) const

{
    int         nOffset;
    
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();
    
    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );
    
/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nGeomCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nGeomCount, 4 );
    }
    
    nOffset = 9;
    
/* ==================================================================== */
/*      Serialize each of the Geoms.                                    */
/* ==================================================================== */
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset );

        nOffset += papoGeoms[iGeom]->WkbSize();
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRGeometryCollection::importFromWkt( char ** ppszInput )

{

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    empty();

/* -------------------------------------------------------------------- */
/*      Read and verify the type keyword, and ensure it matches the     */
/*      actual type of this container.                                  */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY ...                                             */
/* -------------------------------------------------------------------- */
    const char *pszPreScan;
    int bHasZ = FALSE, bHasM = FALSE;

    pszPreScan = OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        *ppszInput = (char *) pszPreScan;
        empty();
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Check for Z, M or ZM. Will ignore the Measure                   */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szToken,"Z") )
    {
        bHasZ = TRUE;
    }
    else if( EQUAL(szToken,"M") )
    {
        bHasM = TRUE;
    }
    else if( EQUAL(szToken,"ZM") )
    {
        bHasZ = TRUE;
        bHasM = TRUE;
    }

    if (bHasZ || bHasM)
    {
        pszInput = pszPreScan;
        pszPreScan = OGRWktReadToken( pszInput, szToken );
        if( EQUAL(szToken,"EMPTY") )
        {
            *ppszInput = (char *) pszPreScan;
            empty();
            /* FIXME?: In theory we should store the dimension and M presence */
            /* if we want to allow round-trip with ExportToWKT v1.2 */
            return OGRERR_NONE;
        }
    }

    if( !EQUAL(szToken,"(") )
        return OGRERR_CORRUPT_DATA;

    if ( !bHasZ && !bHasM )
    {
        /* Test for old-style GEOMETRYCOLLECTION(EMPTY) */
        pszPreScan = OGRWktReadToken( pszPreScan, szToken );
        if( EQUAL(szToken,"EMPTY") )
        {
            pszInput = OGRWktReadToken( pszPreScan, szToken );

            if( !EQUAL(szToken,")") )
                return OGRERR_CORRUPT_DATA;
            else
            {
                *ppszInput = (char *) pszInput;
                empty();
                return OGRERR_NONE;
            }
        }
    }

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each subgeometry in turn.                                  */
/* ==================================================================== */
    do
    {
        OGRGeometry *poGeom = NULL;
        OGRErr      eErr;

        eErr = OGRGeometryFactory::createFromWkt( (char **) &pszInput,
                                                  NULL, &poGeom );
        if( eErr != OGRERR_NONE )
            return eErr;

        addGeometryDirectly( poGeom );

/* -------------------------------------------------------------------- */
/*      Read the delimeter following the ring.                          */
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
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivelent.  This could be made alot more CPU efficient!        */
/************************************************************************/

OGRErr OGRGeometryCollection::exportToWkt( char ** ppszDstText ) const

{
    char        **papszGeoms;
    int         iGeom, nCumulativeLength = 0;
    OGRErr      eErr;

    if( getNumGeometries() == 0 )
    {
        *ppszDstText = CPLStrdup("GEOMETRYCOLLECTION EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    papszGeoms = (char **) CPLCalloc(sizeof(char *),nGeomCount);

    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        eErr = papoGeoms[iGeom]->exportToWkt( &(papszGeoms[iGeom]) );
        if( eErr != OGRERR_NONE )
            goto error;

        nCumulativeLength += strlen(papszGeoms[iGeom]);
    }
    
/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength + nGeomCount + 23);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, getGeometryName() );
    strcat( *ppszDstText, " (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {                                                           
        if( iGeom > 0 )
            (*ppszDstText)[nCumulativeLength++] = ',';
        
        int nGeomLength = strlen(papszGeoms[iGeom]);
        memcpy( *ppszDstText + nCumulativeLength, papszGeoms[iGeom], nGeomLength );
        nCumulativeLength += nGeomLength;
        VSIFree( papszGeoms[iGeom] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszGeoms );

    return OGRERR_NONE;

error:
    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
        CPLFree( papszGeoms[iGeom] );
    CPLFree( papszGeoms );
    return eErr;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRGeometryCollection::getEnvelope( OGREnvelope * psEnvelope ) const

{
    OGREnvelope         oGeomEnv;
    int                 bExtentSet = FALSE;

    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if (!papoGeoms[iGeom]->IsEmpty())
        {
            if (!bExtentSet)
            {
                papoGeoms[iGeom]->getEnvelope( psEnvelope );
                bExtentSet = TRUE;
            }
            else
            {
                papoGeoms[iGeom]->getEnvelope( &oGeomEnv );

                if( psEnvelope->MinX > oGeomEnv.MinX )
                    psEnvelope->MinX = oGeomEnv.MinX;
                if( psEnvelope->MinY > oGeomEnv.MinY )
                    psEnvelope->MinY = oGeomEnv.MinY;
                if( psEnvelope->MaxX < oGeomEnv.MaxX )
                    psEnvelope->MaxX = oGeomEnv.MaxX;
                if( psEnvelope->MaxY < oGeomEnv.MaxY )
                    psEnvelope->MaxY = oGeomEnv.MaxY;
            }
        }
    }

    if (!bExtentSet)
    {
        psEnvelope->MinX = psEnvelope->MinY = 0;
        psEnvelope->MaxX = psEnvelope->MaxY = 0;
    }
}

/************************************************************************/
/*                               Equals()                               */
/************************************************************************/

OGRBoolean OGRGeometryCollection::Equals( OGRGeometry * poOther ) const

{
    OGRGeometryCollection *poOGC = (OGRGeometryCollection *) poOther;

    if( poOGC == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( getNumGeometries() != poOGC->getNumGeometries() )
        return FALSE;
    
    // we should eventually test the SRS.

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
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRErr  eErr;

        eErr = papoGeoms[iGeom]->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iGeom != 0 )
            {
                CPLDebug("OGR", 
                         "OGRGeometryCollection::transform() failed for a geometry other\n"
                         "than the first, meaning some geometries are transformed\n"
                         "and some are not!\n" );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
    }

    assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRGeometryCollection::closeRings()

{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( wkbFlatten(papoGeoms[iGeom]->getGeometryType()) == wkbPolygon )
            ((OGRPolygon *) papoGeoms[iGeom])->closeRings();
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
        switch( wkbFlatten(geom->getGeometryType()) )
        {
            case wkbPolygon:
                dfArea += ((OGRPolygon *) geom)->get_Area();
                break;

            case wkbMultiPolygon:
                dfArea += ((OGRMultiPolygon *) geom)->get_Area();
                break;

            case wkbLinearRing:
            case wkbLineString:
                /* This test below is required to filter out wkbLineString geometries
                * not being of type of wkbLinearRing.
                */
                if( EQUAL( ((OGRGeometry*) geom)->getGeometryName(), "LINEARRING" ) )
                {
                    dfArea += ((OGRLinearRing *) geom)->get_Area();
                }
                break;

            case wkbGeometryCollection:
                dfArea +=((OGRGeometryCollection *) geom)->get_Area();
                break;

            default:
                break;
        }
    }

    return dfArea;
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRGeometryCollection::IsEmpty(  ) const
{
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
        if (papoGeoms[iGeom]->IsEmpty() == FALSE)
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
