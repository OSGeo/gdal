/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRLineString geometry class.
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
 * Revision 1.41  2004/01/16 21:57:16  warmerda
 * fixed up EMPTY support
 *
 * Revision 1.40  2004/01/16 21:20:00  warmerda
 * Added EMPTY support
 *
 * Revision 1.39  2003/08/27 15:40:37  warmerda
 * added support for generating DB2 V7.2 compatible WKB
 *
 * Revision 1.38  2003/06/09 13:48:54  warmerda
 * added DB2 V7.2 byte order hack
 *
 * Revision 1.37  2003/05/28 19:16:43  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.36  2003/03/07 21:28:56  warmerda
 * support 0x8000 style 3D WKB flags
 *
 * Revision 1.35  2003/01/06 17:43:13  warmerda
 * fixed buffer sizing problem for 3D geometries
 *
 * Revision 1.34  2003/01/06 16:18:40  warmerda
 * getEnvlope() crash fix if there are no points
 *
 * Revision 1.33  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.32  2002/08/19 14:11:35  warmerda
 * correct second setPoints() method properly.
 *
 * Revision 1.31  2002/08/19 03:24:51  warmerda
 * Fixed serious bug with 3D points in setPoints() methods.
 *
 * Revision 1.30  2002/07/15 13:31:07  warmerda
 * patch from Graeme for exportToWkb() with swapping code
 *
 * Revision 1.29  2002/05/02 19:45:36  warmerda
 * added flattenTo2D() method
 *
 * Revision 1.28  2002/04/17 21:43:09  warmerda
 * Free z array in descructor.
 *
 * Revision 1.27  2002/04/08 17:50:09  warmerda
 * Ensure Equal operator tests first vertex too.
 *
 * Revision 1.26  2002/03/05 14:25:14  warmerda
 * expand tabs
 *
 * Revision 1.25  2002/02/22 22:24:08  warmerda
 * clarify setPoints code
 *
 * Revision 1.24  2001/11/14 14:44:06  warmerda
 * fixed problem with WKT translation
 *
 * Revision 1.23  2001/11/01 17:20:33  warmerda
 * added DISABLE_OGRGEOM_TRANSFORM macro
 *
 * Revision 1.22  2001/11/01 17:01:28  warmerda
 * pass output buffer into OGRMakeWktCoordinate
 *
 * Revision 1.21  2001/09/21 16:24:20  warmerda
 * added transform() and transformTo() methods
 *
 * Revision 1.20  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.19  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.18  2001/05/24 18:06:06  warmerda
 * fixed comment
 *
 * Revision 1.17  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.16  2000/01/26 21:20:47  warmerda
 * fixed crash assigning 2D points when already 3D
 *
 * Revision 1.15  1999/12/23 14:47:16  warmerda
 * improved 2D/3D handling to avoid 3D unless padfZ != 0.0
 *
 * Revision 1.14  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.13  1999/11/17 19:38:22  warmerda
 * Further performance tweaks to exportToWkt().
 *
 * Revision 1.12  1999/11/04 18:31:32  warmerda
 * Improved efficiency of exportToWkt() for large line strings.
 *
 * Revision 1.11  1999/09/13 14:34:07  warmerda
 * updated to use wkbZ of 0x8000 instead of 0x80000000
 *
 * Revision 1.10  1999/09/13 02:27:33  warmerda
 * incorporated limited 2.5d support
 *
 * Revision 1.9  1999/07/27 00:48:11  warmerda
 * Added Equal() support
 *
 * Revision 1.8  1999/07/06 21:36:47  warmerda
 * tenatively added getEnvelope() and Intersect()
 *
 * Revision 1.7  1999/06/25 20:44:43  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.6  1999/05/31 20:43:34  warmerda
 * added empty(), and another setPoints()
 *
 * Revision 1.5  1999/05/31 15:01:59  warmerda
 * OGRCurve now an abstract base class with essentially no implementation.
 * Everything moved down to OGRLineString where it belongs.  Also documented
 * classes.
 *
 * Revision 1.4  1999/05/23 05:34:40  warmerda
 * added support for clone(), multipolygons and geometry collections
 *
 * Revision 1.3  1999/05/20 14:35:44  warmerda
 * added support for well known text format
 *
 * Revision 1.2  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRLineString()                            */
/************************************************************************/

/**
 * Create an empty line string.
 */

OGRLineString::OGRLineString()

{
    nPointCount = 0;
    paoPoints = NULL;
    padfZ = NULL;
}

/************************************************************************/
/*                           ~OGRLineString()                           */
/************************************************************************/

OGRLineString::~OGRLineString()

{
    if( paoPoints != NULL )
        OGRFree( paoPoints );
    if( padfZ != NULL )
        OGRFree( padfZ );
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRLineString::getGeometryType()

{
    if( getCoordinateDimension() == 3 )
        return wkbLineString25D;
    else
        return wkbLineString;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRLineString::flattenTo2D()

{
    Make2D();
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRLineString::getGeometryName()

{
    return "LINESTRING";
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRLineString::clone()

{
    OGRLineString       *poNewLineString;

    poNewLineString = new OGRLineString();

    poNewLineString->assignSpatialReference( getSpatialReference() );
    poNewLineString->setPoints( nPointCount, paoPoints, padfZ );

    // notdef: not 3D

    return poNewLineString;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRLineString::empty()

{
    setNumPoints( 0 );
}


/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRLineString::getDimension()

{
    return 1;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/

int OGRLineString::getCoordinateDimension()

{
    if( padfZ != NULL )
        return 3;
    else
        return 2;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRLineString::WkbSize()

{
    return 5 + 4 + 8 * nPointCount * getCoordinateDimension();
}

/************************************************************************/
/*                               Make2D()                               */
/************************************************************************/

void OGRLineString::Make2D()

{
    if( padfZ != NULL )
    {
        OGRFree( padfZ );
        padfZ = NULL;
    }
}

/************************************************************************/
/*                               Make3D()                               */
/************************************************************************/

void OGRLineString::Make3D()

{
    if( padfZ == NULL )
    {
        if( nPointCount == 0 )
            padfZ = (double *) OGRCalloc(sizeof(double),1);
        else
            padfZ = (double *) OGRCalloc(sizeof(double),nPointCount);
    }
}

/************************************************************************/
/*                              getPoint()                              */
/************************************************************************/

/**
 * Fetch a point in line string.
 *
 * This method relates to the SFCOM ILineString::get_Point() method.
 *
 * @param i the vertex to fetch, from 0 to getNumPoints()-1.
 * @param poPoint a point to initialize with the fetched point.
 */

void    OGRLineString::getPoint( int i, OGRPoint * poPoint )

{
    assert( i >= 0 );
    assert( i < nPointCount );
    assert( poPoint != NULL );

    poPoint->setX( paoPoints[i].x );
    poPoint->setY( paoPoints[i].y );

    if( getCoordinateDimension() == 3 )
        poPoint->setZ( padfZ[i] );
}

/************************************************************************/
/*                                getZ()                                */
/************************************************************************/

double OGRLineString::getZ( int i )

{
    if( padfZ != NULL && i >= 0 && i < nPointCount )
        return( padfZ[i] );
    else
        return 0.0;
}

/************************************************************************/
/*                            setNumPoints()                            */
/************************************************************************/

/**
 * Set number of points in geometry.
 *
 * This method primary exists to preset the number of points in a linestring
 * geometry before setPoint() is used to assign them to avoid reallocating
 * the array larger with each call to addPoint(). 
 *
 * This method has no SFCOM analog.
 *
 * @param nNewPointCount the new number of points for geometry.
 */

void OGRLineString::setNumPoints( int nNewPointCount )

{
    if( nNewPointCount == 0 )
    {
        OGRFree( paoPoints );
        paoPoints = NULL;
        
        OGRFree( padfZ );
        padfZ = NULL;
        
        nPointCount = 0;
        return;
    }

    if( nNewPointCount > nPointCount )
    {
        paoPoints = (OGRRawPoint *)
            OGRRealloc(paoPoints, sizeof(OGRRawPoint) * nNewPointCount);

        assert( paoPoints != NULL );
        
        memset( paoPoints + nPointCount,
                0, sizeof(OGRRawPoint) * (nNewPointCount - nPointCount) );
        
        if( getCoordinateDimension() == 3 )
        {
            padfZ = (double *)
                OGRRealloc( padfZ, sizeof(double)*nNewPointCount );
            memset( padfZ + nPointCount, 0,
                    sizeof(double) * (nNewPointCount - nPointCount) );
        }
    }

    nPointCount = nNewPointCount;
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accomodate the request.
 *
 * There is no SFCOM analog to this method.
 * 
 * @param iPoint the index of the vertex to assign (zero based).
 * @param poPoint the value to assign to the vertex.
 */

void OGRLineString::setPoint( int iPoint, OGRPoint * poPoint )

{
    setPoint( iPoint, poPoint->getX(), poPoint->getY(), poPoint->getZ() );
}

/************************************************************************/
/*                              setPoint()                              */
/************************************************************************/

/**
 * Set the location of a vertex in line string.
 *
 * If iPoint is larger than the number of necessary the number of existing
 * points in the line string, the point count will be increased to
 * accomodate the request.
 * 
 * There is no SFCOM analog to this method.
 *
 * @param iPoint the index of the vertex to assign (zero based).
 * @param xIn input X coordinate to assign.
 * @param yIn input Y coordinate to assign.
 * @param zIn input Z coordinate to assign (defaults to zero).
 */

void OGRLineString::setPoint( int iPoint, double xIn, double yIn, double zIn )

{
    if( iPoint >= nPointCount )
        setNumPoints( iPoint+1 );

    paoPoints[iPoint].x = xIn;
    paoPoints[iPoint].y = yIn;

    if( zIn != 0.0 )
    {
        Make3D();
        padfZ[iPoint] = zIn;
    }
    else if( getCoordinateDimension() == 3 )
    {
        padfZ[iPoint] = 0.0;
    }
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param poPoint the point to assign to the new vertex.
 */

void OGRLineString::addPoint( OGRPoint * poPoint )

{
    setPoint( nPointCount, poPoint->getX(), poPoint->getY(), poPoint->getZ() );
}

/************************************************************************/
/*                              addPoint()                              */
/************************************************************************/

/**
 * Add a point to a line string.
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * There is no SFCOM analog to this method.
 *
 * @param x the X coordinate to assign to the new point.
 * @param y the Y coordinate to assign to the new point.
 * @param z the Z coordinate to assign to the new point (defaults to zero).
 */

void OGRLineString::addPoint( double x, double y, double z )

{
    setPoint( nPointCount, x, y, z );
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * Assign all points in a line string.
 *
 * This method clears any existing points assigned to this line string,
 * and assigns a whole new set.  It is the most efficient way of assigning
 * the value of a line string.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in paoPointsIn
 * @param paoPointsIn list of points being assigned.
 * @param padfZ the Z values that go with the points (optional, may be NULL).
 */

void OGRLineString::setPoints( int nPointsIn, OGRRawPoint * paoPointsIn,
                               double * padfZ )

{
    setNumPoints( nPointsIn );
    memcpy( paoPoints, paoPointsIn, sizeof(OGRRawPoint) * nPointsIn);

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZ != NULL )
    {
        int     i, bIs3D = FALSE;

        for( i = 0; i < nPointsIn && !bIs3D; i++ )
        {
            if( padfZ[i] != 0.0 )
                bIs3D = TRUE;
        }

        if( !bIs3D )
            padfZ = NULL;
    }

    if( padfZ == NULL )
    {
        if( this->padfZ != NULL )
            Make2D();
    }
    else
    {
        Make3D();
        memcpy( this->padfZ, padfZ, sizeof(double) * nPointsIn );
    }
}

/************************************************************************/
/*                             setPoints()                              */
/************************************************************************/

/**
 * Assign all points in a line string.
 *
 * This method clear any existing points assigned to this line string,
 * and assigns a whole new set.
 *
 * There is no SFCOM analog to this method.
 *
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param padfX list of X coordinates of points being assigned.
 * @param padfY list of Y coordinates of points being assigned.
 * @param padfZ list of Z coordinates of points being assigned (defaults to
 * NULL for 2D objects).
 */

void OGRLineString::setPoints( int nPointsIn, double * padfX, double * padfY,
                               double * padfZ )

{
    int         i;

/* -------------------------------------------------------------------- */
/*      Check 2D/3D.                                                    */
/* -------------------------------------------------------------------- */
    if( padfZ != NULL )
    {
        int     bIs3D = FALSE;

        for( i = 0; i < nPointsIn && !bIs3D; i++ )
        {
            if( padfZ[i] != 0.0 )
                bIs3D = TRUE;
        }

        if( !bIs3D )
            padfZ = NULL;
    }

    if( padfZ == NULL )
        Make2D();
    else
        Make3D();
    
/* -------------------------------------------------------------------- */
/*      Assign values.                                                  */
/* -------------------------------------------------------------------- */
    setNumPoints( nPointsIn );

    for( i = 0; i < nPointsIn; i++ )
    {
        paoPoints[i].x = padfX[i];
        paoPoints[i].y = padfY[i];
    }

    if( this->padfZ != NULL )
        memcpy( this->padfZ, padfZ, sizeof(double) * nPointsIn );
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRLineString::importFromWkb( unsigned char * pabyData,
                                     int nSize )

{
    OGRwkbByteOrder     eByteOrder;
    
    if( nSize < 21 && nSize != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = DB2_V72_FIX_BYTE_ORDER((OGRwkbByteOrder) *pabyData);
    assert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.  For now we assume that          */
/*      geometry type is between 0 and 255 so we only have to fetch     */
/*      one byte.                                                       */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType eGeometryType;
    int                bIs3D;

    if( eByteOrder == wkbNDR )
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[1];
        bIs3D = pabyData[4] & 0x80 || pabyData[2] & 0x80;
    }
    else
    {
        eGeometryType = (OGRwkbGeometryType) pabyData[4];
        bIs3D = pabyData[1] & 0x80 || pabyData[3] & 0x80;
    }

    CPLAssert( eGeometryType == wkbLineString );

/* -------------------------------------------------------------------- */
/*      Get the vertex count.                                           */
/* -------------------------------------------------------------------- */
    int         nNewNumPoints;
    
    memcpy( &nNewNumPoints, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nNewNumPoints = CPL_SWAP32(nNewNumPoints);

    setNumPoints( nNewNumPoints );
    
    if( bIs3D )
        Make3D();
    else
        Make2D();
    
/* -------------------------------------------------------------------- */
/*      Get the vertex.                                                 */
/* -------------------------------------------------------------------- */
    int         i;
    
    if( bIs3D )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( paoPoints + i, pabyData + 9 + i*24, 16 );
            memcpy( padfZ + i, pabyData + 9 + 16 + i*24, 8 );
        }
    }
    else
    {
        memcpy( paoPoints, pabyData + 9, 16 * nPointCount );
    }
    
/* -------------------------------------------------------------------- */
/*      Byte swap if needed.                                            */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            CPL_SWAPDOUBLE( &(paoPoints[i].x) );
            CPL_SWAPDOUBLE( &(paoPoints[i].y) );
        }

        if( bIs3D )
        {
            for( i = 0; i < nPointCount; i++ )
            {
                CPL_SWAPDOUBLE( padfZ + i );
            }
        }
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRLineString::exportToWkb( OGRwkbByteOrder eByteOrder,
                               unsigned char * pabyData )

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();
    
    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );
    
/* -------------------------------------------------------------------- */
/*      Copy in the data count.                                         */
/* -------------------------------------------------------------------- */
    memcpy( pabyData+5, &nPointCount, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    int         i;
    
    if( getCoordinateDimension() == 3 )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            memcpy( pabyData + 9 + 24*i, paoPoints+i, 16 );
            memcpy( pabyData + 9 + 16 + 24*i, padfZ+i, 8 );
        }
    }
    else
        memcpy( pabyData+9, paoPoints, 16 * nPointCount );

/* -------------------------------------------------------------------- */
/*      Swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nPointCount );
        memcpy( pabyData+5, &nCount, 4 );

        for( i = getCoordinateDimension() * nPointCount - 1; i >= 0; i-- )
        {
            CPL_SWAP64PTR( pabyData + 9 + 8 * i );
        }
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `LINESTRING ( x y, x y, ...)',                                  */
/************************************************************************/

OGRErr OGRLineString::importFromWkt( char ** ppszInput )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

    if( paoPoints != NULL )
    {
        nPointCount = 0;

        CPLFree( paoPoints );
        paoPoints = NULL;
        
        CPLFree( padfZ );
        padfZ = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the ``LINESTRING'' keyword token.               */
/* -------------------------------------------------------------------- */
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( !EQUAL(szToken,getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY ... but treat like a point at 0,0.              */
/* -------------------------------------------------------------------- */
    const char *pszPreScan;

    pszPreScan = OGRWktReadToken( pszInput, szToken );
    if( !EQUAL(szToken,"(") )
        return OGRERR_CORRUPT_DATA;
    
    pszPreScan = OGRWktReadToken( pszPreScan, szToken );
    if( EQUAL(szToken,"EMPTY") )
    {
        pszPreScan = OGRWktReadToken( pszPreScan, szToken );

        *ppszInput = (char *) pszPreScan;
        
        if( !EQUAL(szToken,")") )
            return OGRERR_CORRUPT_DATA;
        else
            return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Read the point list.                                            */
/* -------------------------------------------------------------------- */
    int                 nMaxPoint = 0;

    nPointCount = 0;

    pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoint,
                                 &nPointCount );
    if( pszInput == NULL )
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

OGRErr OGRLineString::exportToWkt( char ** ppszDstText )

{
    int         nMaxString = nPointCount * 20 * 3 + 20;
    int         nRetLen = 0;

/* -------------------------------------------------------------------- */
/*      Handle special empty case.                                      */
/* -------------------------------------------------------------------- */
    if( nPointCount == 0 )
    {
        *ppszDstText = CPLStrdup("LINESTRING(EMPTY)");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      General case.                                                   */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc( nMaxString );
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    sprintf( *ppszDstText, "%s (", getGeometryName() );

    for( int i = 0; i < nPointCount; i++ )
    {
        if( nMaxString <= (int) strlen(*ppszDstText+nRetLen) + 32 + nRetLen )
        {
            CPLDebug( "OGR", 
                      "OGRLineString::exportToWkt() ... buffer overflow.\n"
                      "nMaxString=%d, strlen(*ppszDstText) = %d, i=%d\n"
                      "*ppszDstText = %s", 
                      nMaxString, strlen(*ppszDstText), i, *ppszDstText );

            VSIFree( *ppszDstText );
            *ppszDstText = NULL;
            return OGRERR_NOT_ENOUGH_MEMORY;
        }
        
        if( i > 0 )
            strcat( *ppszDstText + nRetLen, "," );

        nRetLen += strlen(*ppszDstText + nRetLen);
        if( getCoordinateDimension() == 3 )
            OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                                  paoPoints[i].x,
                                  paoPoints[i].y,
                                  padfZ[i] );
        else
            OGRMakeWktCoordinate( *ppszDstText + nRetLen,
                                  paoPoints[i].x,
                                  paoPoints[i].y,
                                  0.0 );

        nRetLen += strlen(*ppszDstText + nRetLen);
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidian 2D distance.               */
/************************************************************************/

double OGRLineString::get_Length()

{
    double      dfLength = 0;
    int         i;

    for( i = 0; i < nPointCount-1; i++ )
    {
        double      dfDeltaX, dfDeltaY;

        dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);
    }
    
    return dfLength;
}

/************************************************************************/
/*                             StartPoint()                             */
/************************************************************************/

void OGRLineString::StartPoint( OGRPoint * poPoint )

{
    getPoint( 0, poPoint );
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRLineString::EndPoint( OGRPoint * poPoint )

{
    getPoint( nPointCount-1, poPoint );
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRLineString::Value( double dfDistance, OGRPoint * poPoint )

{
    double      dfLength = 0;
    int         i;

    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }

    for( i = 0; i < nPointCount-1; i++ )
    {
        double      dfDeltaX, dfDeltaY, dfSegLength;

        dfDeltaX = paoPoints[i+1].x - paoPoints[i].x;
        dfDeltaY = paoPoints[i+1].y - paoPoints[i].y;
        dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

        if( dfLength <= dfDistance && dfLength + dfSegLength >= dfDistance )
        {
            double      dfRatio;

            dfRatio = (dfDistance - dfLength) / dfSegLength;

            poPoint->setX( paoPoints[i].x * (1 - dfRatio)
                           + paoPoints[i+1].x * dfRatio );
            poPoint->setY( paoPoints[i].y * (1 - dfRatio)
                           + paoPoints[i+1].y * dfRatio );

            if( getCoordinateDimension() == 3 )
                poPoint->setZ( padfZ[i] * (1 - dfRatio)
                               + padfZ[i] * dfRatio );
                
            return;
        }
    }
    
    EndPoint( poPoint );
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRLineString::getEnvelope( OGREnvelope * psEnvelope )

{
    double      dfMinX, dfMinY, dfMaxX, dfMaxY;

    if( nPointCount == 0 )
        return;
    
    dfMinX = dfMaxX = paoPoints[0].x;
    dfMinY = dfMaxY = paoPoints[0].y;

    for( int iPoint = 1; iPoint < nPointCount; iPoint++ )
    {
        if( dfMaxX < paoPoints[iPoint].x )
            dfMaxX = paoPoints[iPoint].x;
        if( dfMaxY < paoPoints[iPoint].y )
            dfMaxY = paoPoints[iPoint].y;
        if( dfMinX > paoPoints[iPoint].x )
            dfMinX = paoPoints[iPoint].x;
        if( dfMinY > paoPoints[iPoint].y )
            dfMinY = paoPoints[iPoint].y;
    }

    psEnvelope->MinX = dfMinX;
    psEnvelope->MaxX = dfMaxX;
    psEnvelope->MinY = dfMinY;
    psEnvelope->MaxY = dfMaxY;
}

/************************************************************************/
/*                               Equal()                                */
/************************************************************************/

OGRBoolean OGRLineString::Equal( OGRGeometry * poOther )

{
    OGRLineString       *poOLine = (OGRLineString *) poOther;
    
    if( poOther == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    // we should eventually test the SRS.

    if( getNumPoints() != poOLine->getNumPoints() )
        return FALSE;

    for( int iPoint = 0; iPoint < getNumPoints(); iPoint++ )
    {
        if( getX(iPoint) != poOLine->getX(iPoint)
            || getY(iPoint) != poOLine->getY(iPoint) 
            || getZ(iPoint) != poOLine->getZ(iPoint) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRLineString::transform( OGRCoordinateTransformation *poCT )

{
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    double      *xyz;
    int         i;

/* -------------------------------------------------------------------- */
/*      Because we don't want to partially transform this geometry      */
/*      (if some points fail after some have succeeded) we will         */
/*      instead make a copy of the points to operate on.                */
/* -------------------------------------------------------------------- */
    xyz = (double *) CPLMalloc(sizeof(double) * nPointCount * 3);
    if( xyz == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    for( i = 0; i < nPointCount; i++ )
    {
        xyz[i  ] = paoPoints[i].x;
        xyz[i+nPointCount] = paoPoints[i].y;
        if( padfZ )
            xyz[i+nPointCount*2] = padfZ[i];
        else
            xyz[i+nPointCount*2] = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Transform and reapply.                                          */
/* -------------------------------------------------------------------- */
    if( !poCT->Transform( nPointCount, xyz, xyz + nPointCount, 
                          xyz+nPointCount*2 ) )
    {
        CPLFree( xyz );
        return OGRERR_FAILURE;
    }
    else
    {
        setPoints( nPointCount, xyz, xyz+nPointCount, xyz+nPointCount*2 );
        CPLFree( xyz );

        assignSpatialReference( poCT->GetTargetCS() );

        return OGRERR_NONE;
    }
#endif
}
