/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRGeometryCollection class.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.15  2002/03/05 14:25:14  warmerda
 * expand tabs
 *
 * Revision 1.14  2001/11/01 17:20:33  warmerda
 * added DISABLE_OGRGEOM_TRANSFORM macro
 *
 * Revision 1.13  2001/09/21 16:24:20  warmerda
 * added transform() and transformTo() methods
 *
 * Revision 1.12  2001/08/30 02:06:19  warmerda
 * fixed array overrun error in exportToWkt()
 *
 * Revision 1.11  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.10  2001/05/24 18:06:30  warmerda
 * use addGeometryDirectly when parsing WKT
 *
 * Revision 1.9  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.8  1999/11/04 16:26:29  warmerda
 * Implemented addGeometryDirectly().
 *
 * Revision 1.7  1999/09/01 11:50:40  warmerda
 * Fixed CPLAssert on legal geometry types.
 *
 * Revision 1.6  1999/07/27 00:48:11  warmerda
 * Added Equal() support
 *
 * Revision 1.5  1999/07/06 21:36:47  warmerda
 * tenatively added getEnvelope() and Intersect()
 *
 * Revision 1.4  1999/06/25 20:44:43  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.3  1999/05/31 20:43:04  warmerda
 * added empty method, implement createFromWkt(), added mline/mpoint
 *
 * Revision 1.2  1999/05/31 14:59:06  warmerda
 * added documentation
 *
 * Revision 1.1  1999/05/23 05:34:36  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRGeometryCollection()                        */
/************************************************************************/

/**
 * Create an empty geometry collection.
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
}


/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRGeometryCollection::clone()

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

OGRwkbGeometryType OGRGeometryCollection::getGeometryType()

{
    return wkbGeometryCollection;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRGeometryCollection::getDimension()

{
    return 2;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/*                                                                      */
/*      Returning 2 is a dubious solution.  This should at least be     */
/*      overridden by some of the subclasses.                           */
/************************************************************************/

int OGRGeometryCollection::getCoordinateDimension()

{
    return 2;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRGeometryCollection::getGeometryName()

{
    return "GEOMETRYCOLLECTION";
}

/************************************************************************/
/*                          getNumGeometries()                          */
/************************************************************************/

/**
 * Fetch number of geometries in container.
 *
 * This method relates to the SFCOM IGeometryCollect::get_NumGeometries()
 * method.
 *
 * @return count of children geometries.  May be zero.
 */

int OGRGeometryCollection::getNumGeometries()

{
    return nGeomCount;
}

/************************************************************************/
/*                           getGeometryRef()                           */
/************************************************************************/

/**
 * Fetch geometry from container.
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

/************************************************************************/
/*                            addGeometry()                             */
/*                                                                      */
/*      Add a new geometry to a collection.  Subclasses should          */
/*      override this to verify the type of the new geometry, and       */
/*      then call this method to actually add it.                       */
/************************************************************************/

/**
 * Add a geometry to the container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  The passed geometry is cloned
 * to make an internal copy.
 *
 * There is no SFCOM analog to this method.
 *
 * @param poNewGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of geometry container.
 */

OGRErr OGRGeometryCollection::addGeometry( OGRGeometry * poNewGeom )

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
 * Add a geometry directly to the container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  Ownership of the passed
 * geometry is taken by the container rather than cloning as addGeometry()
 * does.
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

    return OGRERR_NONE;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRGeometryCollection::WkbSize()

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
                                             int nBytesAvailable )

{
    OGRwkbByteOrder     eByteOrder;
    int                 nDataOffset;
    
    if( nBytesAvailable < 9 && nBytesAvailable != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = (OGRwkbByteOrder) *pabyData;
    CPLAssert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    OGRwkbGeometryType eGeometryType;
    
    if( eByteOrder == wkbNDR )
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
    else
        eGeometryType = (OGRwkbGeometryType) pabyData[4];

    CPLAssert( eGeometryType == wkbGeometryCollection
               || eGeometryType == wkbMultiPolygon 
               || eGeometryType == wkbMultiLineString 
               || eGeometryType == wkbMultiPoint );
#endif    

/* -------------------------------------------------------------------- */
/*      Do we already have some existing geometry objects?              */
/* -------------------------------------------------------------------- */
    if( nGeomCount != 0 )
    {
        for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
            delete papoGeoms[iGeom];

        OGRFree( papoGeoms );
        papoGeoms = NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the geometry count.                                         */
/* -------------------------------------------------------------------- */
    memcpy( &nGeomCount, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nGeomCount = CPL_SWAP32(nGeomCount);

    papoGeoms = (OGRGeometry **) OGRMalloc(sizeof(void*) * nGeomCount);

    nDataOffset = 9;
    if( nBytesAvailable != -1 )
        nBytesAvailable -= nDataOffset;

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        OGRErr  eErr;

        eErr = OGRGeometryFactory::
            createFromWkb( pabyData + nDataOffset, NULL,
                           papoGeoms + iGeom, nBytesAvailable );

        if( eErr != OGRERR_NONE )
        {
            nGeomCount = iGeom;
            return eErr;
        }

        if( nBytesAvailable != -1 )
            nBytesAvailable -= papoGeoms[iGeom]->WkbSize();

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
                                            unsigned char * pabyData )

{
    int         nOffset;
    
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = (unsigned char) eByteOrder;

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    if( eByteOrder == wkbNDR )
    {
        pabyData[1] = getGeometryType();
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = 0;
    }
    else
    {
        pabyData[1] = 0;
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = getGeometryType();
    }
    
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
    int         iGeom;

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    if( nGeomCount > 0 )
    {
        for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
            delete papoGeoms[iGeom];
        
        nGeomCount = 0;
        CPLFree( papoGeoms );
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the type keyword, and ensure it matches the     */
/*      actual type of this container.                                  */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      The next character should be a ( indicating the start of the    */
/*      list of objects.                                                */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );
    if( szToken[0] != '(' )
        return OGRERR_CORRUPT_DATA;

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

OGRErr OGRGeometryCollection::exportToWkt( char ** ppszReturn )

{
    char        **papszGeoms;
    int         iGeom, nCumulativeLength = 0;
    OGRErr      eErr;

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    papszGeoms = (char **) CPLCalloc(sizeof(char *),nGeomCount);

    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        eErr = papoGeoms[iGeom]->exportToWkt( &(papszGeoms[iGeom]) );
        if( eErr != OGRERR_NONE )
            return eErr;

        nCumulativeLength += strlen(papszGeoms[iGeom]);
    }
    
/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszReturn = (char *) VSIMalloc(nCumulativeLength + nGeomCount + 23);

    if( *ppszReturn == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszReturn, getGeometryName() );
    strcat( *ppszReturn, " (" );

    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {                                                           
        if( iGeom > 0 )
            strcat( *ppszReturn, "," );
        
        strcat( *ppszReturn, papszGeoms[iGeom] );
        VSIFree( papszGeoms[iGeom] );
    }

    strcat( *ppszReturn, ")" );

    CPLFree( papszGeoms );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRGeometryCollection::getEnvelope( OGREnvelope * poEnvelope )

{
    OGREnvelope         oGeomEnv;
    
    if( nGeomCount == 0 )
        return;

    papoGeoms[0]->getEnvelope( poEnvelope );

    for( int iGeom = 1; iGeom < nGeomCount; iGeom++ )
    {
        papoGeoms[iGeom]->getEnvelope( &oGeomEnv );

        if( poEnvelope->MinX > oGeomEnv.MinX )
            poEnvelope->MinX = oGeomEnv.MinX;
        if( poEnvelope->MinY > oGeomEnv.MinY )
            poEnvelope->MinY = oGeomEnv.MinY;
        if( poEnvelope->MaxX < oGeomEnv.MaxX )
            poEnvelope->MaxX = oGeomEnv.MaxX;
        if( poEnvelope->MaxY < oGeomEnv.MaxY )
            poEnvelope->MaxY = oGeomEnv.MaxY;
    }
}

/************************************************************************/
/*                               Equal()                                */
/************************************************************************/

OGRBoolean OGRGeometryCollection::Equal( OGRGeometry * poOther )

{
    OGRGeometryCollection *poOGC = (OGRGeometryCollection *) poOther;

    if( poOther == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( getNumGeometries() != poOGC->getNumGeometries() )
        return FALSE;
    
    // we should eventually test the SRS.

    for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {
        if( !getGeometryRef(iGeom)->Equal(poOGC->getGeometryRef(iGeom)) )
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

