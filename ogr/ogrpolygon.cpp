/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolygon geometry class.
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
 * Revision 1.2  1999/05/17 14:38:11  warmerda
 * Added new IPolygon style methods.
 *
 * Revision 1.1  1999/03/30 21:21:05  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

/************************************************************************/
/*                           OGRPolygon()                            */
/************************************************************************/

OGRPolygon::OGRPolygon()

{
    nRingCount = 0;
    papoRings = NULL;
}

/************************************************************************/
/*                           ~OGRPolygon()                           */
/************************************************************************/

OGRPolygon::~OGRPolygon()

{
    if( papoRings != NULL )
    {
        for( int i = 0; i < nRingCount; i++ )
        {
            delete papoRings[i];
        }
        OGRFree( papoRings );
    }
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolygon::getGeometryType()

{
    return wkbPolygon;
}

/************************************************************************/
/*                            dumpReadable()                            */
/************************************************************************/

void OGRPolygon::dumpReadable( FILE * fp, const char * pszPrefix )

{
    char	*pszPrefix2;
    
    if( pszPrefix == NULL )
        pszPrefix = "";

    fprintf( fp, "%sOGRPolygon: %d rings\n", pszPrefix, nRingCount );

    pszPrefix2 = (char *) OGRMalloc(strlen(pszPrefix)+3);
    strcpy( pszPrefix2, "  " );
    strcat( pszPrefix2, pszPrefix );

    for( int i = 0; i < nRingCount; i++ )
        papoRings[i]->dumpReadable( fp, pszPrefix2 );

    OGRFree( pszPrefix2 );
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRPolygon::getDimension()

{
    return 2;
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/

int OGRPolygon::getCoordinateDimension()

{
    return 2;
}

/************************************************************************/
/*                          getExteriorRing()                           */
/************************************************************************/

OGRLinearRing *OGRPolygon::getExteriorRing()

{
    if( nRingCount > 0 )
        return papoRings[0];
    else
        return NULL;
}

/************************************************************************/
/*                        getNumInteriorRings()                         */
/************************************************************************/

int OGRPolygon::getNumInteriorRings()

{
    if( nRingCount > 0 )
        return nRingCount-1;
    else
        return 0;
}

/************************************************************************/
/*                          getInteriorRing()                           */
/************************************************************************/

OGRLinearRing *OGRPolygon::getInteriorRing( int iRing )

{
    if( iRing < 0 || iRing >= nRingCount-1 )
        return NULL;
    else
        return papoRings[iRing+1];
}

/************************************************************************/
/*                              addRing()                               */
/*                                                                      */
/*      Add a ring to the polygon. Note we make a copy of the ring.     */
/*      The original is still the responsibility of the caller.         */
/************************************************************************/

void OGRPolygon::addRing( OGRLinearRing * poNewRing )

{
    papoRings = (OGRLinearRing **) OGRRealloc( papoRings,
                                               sizeof(void*) * (nRingCount+1));

    papoRings[nRingCount] = new OGRLinearRing( poNewRing );

    nRingCount++;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPolygon::WkbSize()

{
    int		nSize = 9;

    for( int i = 0; i < nRingCount; i++ )
    {
        nSize += papoRings[i]->_WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPolygon::importFromWkb( unsigned char * pabyData,
                                  int nBytesAvailable )

{
    OGRwkbByteOrder	eByteOrder;
    int			nDataOffset;
    
    if( nBytesAvailable < 21 && nBytesAvailable != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    eByteOrder = (OGRwkbByteOrder) *pabyData;
    assert( eByteOrder == wkbXDR || eByteOrder == wkbNDR );

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

    assert( eGeometryType == wkbPolygon );
#endif    

/* -------------------------------------------------------------------- */
/*      Do we already have some rings?                                  */
/* -------------------------------------------------------------------- */
    if( nRingCount != 0 )
    {
        for( int iRing = 0; iRing < nRingCount; iRing++ )
            delete papoRings[iRing];

        OGRFree( papoRings );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the ring count.                                             */
/* -------------------------------------------------------------------- */
    memcpy( &nRingCount, pabyData + 5, 4 );
    
    if( OGR_SWAP( eByteOrder ) )
        nRingCount = CPL_SWAP32(nRingCount);

    papoRings = (OGRLinearRing **) OGRMalloc(sizeof(void*) * nRingCount);

    nDataOffset = 9;
    if( nBytesAvailable != -1 )
        nBytesAvailable -= nDataOffset;

/* -------------------------------------------------------------------- */
/*      Get the rings.                                                  */
/* -------------------------------------------------------------------- */
    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        OGRErr	eErr;
        
        papoRings[iRing] = new OGRLinearRing();
        eErr = papoRings[iRing]->_importFromWkb( eByteOrder,
                                                 pabyData + nDataOffset,
                                                 nBytesAvailable );
        if( eErr != OGRERR_NONE )
        {
            nRingCount = iRing;
            return eErr;
        }

        if( nBytesAvailable != -1 )
            nBytesAvailable -= papoRings[iRing]->_WkbSize();

        nDataOffset += papoRings[iRing]->_WkbSize();
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr	OGRPolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                 unsigned char * pabyData )

{
    int		nOffset;
    
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = (unsigned char) eByteOrder;

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    if( eByteOrder == wkbNDR )
    {
        pabyData[1] = wkbPolygon;
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = 0;
    }
    else
    {
        pabyData[1] = 0;
        pabyData[2] = 0;
        pabyData[3] = 0;
        pabyData[4] = wkbPolygon;
    }
    
/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int	nCount;

        nCount = CPL_SWAP32( nRingCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nRingCount, 4 );
    }
    
    nOffset = 9;
    
/* ==================================================================== */
/*      Serialize each of the rings.                                    */
/* ==================================================================== */
    for( int iRing = 0; iRing < nRingCount; iRing++ )
    {
        papoRings[iRing]->_exportToWkb( eByteOrder,
                                        pabyData + nOffset );

        nOffset += papoRings[iRing]->_WkbSize();
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

double OGRPolygon::get_Area()

{
    // notdef ... correct later.
    
    return 0.0;
}

/************************************************************************/
/*                              Centroid()                              */
/************************************************************************/

int OGRPolygon::Centroid( OGRPoint * )

{
    // notdef ... not implemented yet.
    
    return 0;
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

int OGRPolygon::PointOnSurface( OGRPoint * )

{
    // notdef ... not implemented yet.
    
    return 0;
}
