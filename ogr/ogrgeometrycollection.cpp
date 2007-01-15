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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.32  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.31  2005/07/20 01:43:51  fwarmerdam
 * upgraded OGR geometry dimension handling
 *
 * Revision 1.30  2005/07/12 17:34:00  fwarmerdam
 * updated to produce proper empty syntax and consume either
 *
 * Revision 1.29  2005/02/22 12:38:01  fwarmerdam
 * rename Equal/Intersect to Equals/Intersects
 *
 * Revision 1.28  2004/07/10 04:51:22  warmerda
 * added closeRings
 *
 * Revision 1.27  2004/02/22 09:52:04  dron
 * Fix compirison casting problems in OGRGeometryCollection::Equal().
 *
 * Revision 1.26  2004/02/21 15:36:14  warmerda
 * const correctness updates for geometry: bug 289
 *
 * Revision 1.25  2004/01/16 21:57:16  warmerda
 * fixed up EMPTY support
 *
 * Revision 1.24  2004/01/16 21:20:00  warmerda
 * Added EMPTY support
 *
 * Revision 1.23  2003/08/27 15:40:37  warmerda
 * added support for generating DB2 V7.2 compatible WKB
 *
 * Revision 1.22  2003/06/09 13:48:54  warmerda
 * added DB2 V7.2 byte order hack
 *
 * Revision 1.21  2003/05/28 19:16:42  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.20  2003/03/31 15:55:42  danmo
 * Added C API function docs
 *
 * Revision 1.19  2003/03/07 21:32:52  warmerda
 * fixed bug with coordinate dimension reading from WKB
 *
 * Revision 1.18  2003/01/07 16:44:27  warmerda
 * added removeGeometry
 *
 * Revision 1.17  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.16  2002/05/02 19:45:36  warmerda
 * added flattenTo2D() method
 *
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
    nCoordinateDimension = 2;
}

/************************************************************************/
/*                       ~OGRGeometryCollection()                       */
/************************************************************************/

OGRGeometryCollection::~OGRGeometryCollection()

{
    empty();
    nCoordinateDimension = 2;
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

    nCoordinateDimension = 2;
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
 * Fetch number of geometries in container.
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
 * Add a geometry to the container.
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
 * Add a geometry directly to the container.
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
        nCoordinateDimension = 3;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           removeGeometry()                           */
/************************************************************************/

/**
 * Remove a geometry from the container.
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
    CPLAssert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

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
    if( nSize != -1 )
        nSize -= nDataOffset;

    nCoordinateDimension = 0; // unknown

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

    if( EQUAL(szToken,"EMPTY") )
    {
        *ppszInput = (char *) pszInput;
        return OGRERR_NONE;
    }

    if( szToken[0] != '(' )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      If the next token is EMPTY, then verify that we have proper     */
/*      EMPTY format will a trailing closing bracket.                   */
/* -------------------------------------------------------------------- */
    OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        pszInput = OGRWktReadToken( pszInput, szToken );
        pszInput = OGRWktReadToken( pszInput, szToken );
        
        *ppszInput = (char *) pszInput;

        if( !EQUAL(szToken,")") )
            return OGRERR_CORRUPT_DATA;
        else
            return OGRERR_NONE;
    }

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
            return eErr;

        nCumulativeLength += strlen(papszGeoms[iGeom]);
    }
    
/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength + nGeomCount + 23);

    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, getGeometryName() );
    strcat( *ppszDstText, " (" );

    for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
    {                                                           
        if( iGeom > 0 )
            strcat( *ppszDstText, "," );
        
        strcat( *ppszDstText, papszGeoms[iGeom] );
        VSIFree( papszGeoms[iGeom] );
    }

    strcat( *ppszDstText, ")" );

    CPLFree( papszGeoms );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRGeometryCollection::getEnvelope( OGREnvelope * psEnvelope ) const

{
    OGREnvelope         oGeomEnv;
    
    if( nGeomCount == 0 )
        return;

    papoGeoms[0]->getEnvelope( psEnvelope );

    for( int iGeom = 1; iGeom < nGeomCount; iGeom++ )
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

