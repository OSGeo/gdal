/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements decoder of shapebin geometry for PGeo
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrpgeogeometry.h"
#include "ogr_p.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#define SHPP_TRISTRIP   0
#define SHPP_TRIFAN     1
#define SHPP_OUTERRING  2
#define SHPP_INNERRING  3
#define SHPP_FIRSTRING  4
#define SHPP_RING       5
#define SHPP_TRIANGLES  6 /* Multipatch 9.0 specific */


/************************************************************************/
/*                  OGRCreateFromMultiPatchPart()                       */
/************************************************************************/

void OGRCreateFromMultiPatchPart(OGRMultiPolygon *poMP,
                                 OGRPolygon*& poLastPoly,
                                 int nPartType,
                                 int nPartPoints,
                                 double* padfX,
                                 double* padfY,
                                 double* padfZ)
{
    nPartType &= 0xf;

    if( nPartType == SHPP_TRISTRIP )
    {
        int iBaseVert;

        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert++ )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else if( nPartType == SHPP_TRIFAN )
    {
        int iBaseVert;

        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert++ )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[0],
                            padfY[0],
                            padfZ[0] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[0],
                            padfY[0],
                            padfZ[0] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else if( nPartType == SHPP_OUTERRING
            || nPartType == SHPP_INNERRING
            || nPartType == SHPP_FIRSTRING
            || nPartType == SHPP_RING )
    {
        if( poLastPoly != NULL
            && (nPartType == SHPP_OUTERRING
                || nPartType == SHPP_FIRSTRING) )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        if( poLastPoly == NULL )
            poLastPoly = new OGRPolygon();

        OGRLinearRing *poRing = new OGRLinearRing;

        poRing->setPoints( nPartPoints,
                            padfX,
                            padfY,
                            padfZ );

        poRing->closeRings();

        poLastPoly->addRingDirectly( poRing );
    }
    else if ( nPartType == SHPP_TRIANGLES )
    {
        int iBaseVert;

        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert+=3 )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else
        CPLDebug( "OGR", "Unrecognised parttype %d, ignored.",
                nPartType );
}

/************************************************************************/
/*                     OGRCreateFromMultiPatch()                        */
/*                                                                      */
/*      Translate a multipatch representation to an OGR geometry        */
/*      Mostly copied from shape2ogr.cpp                                */
/************************************************************************/

static OGRGeometry* OGRCreateFromMultiPatch(int nParts,
                                            GInt32* panPartStart,
                                            GInt32* panPartType,
                                            int nPoints,
                                            double* padfX,
                                            double* padfY,
                                            double* padfZ)
{
    OGRMultiPolygon *poMP = new OGRMultiPolygon();
    int iPart;
    OGRPolygon *poLastPoly = NULL;

    for( iPart = 0; iPart < nParts; iPart++ )
    {
        int nPartPoints, nPartStart;

        // Figure out details about this part's vertex list.
        if( panPartStart == NULL )
        {
            nPartPoints = nPoints;
            nPartStart = 0;
        }
        else
        {

            if( iPart == nParts - 1 )
                nPartPoints =
                    nPoints - panPartStart[iPart];
            else
                nPartPoints = panPartStart[iPart+1]
                    - panPartStart[iPart];
            nPartStart = panPartStart[iPart];
        }

        OGRCreateFromMultiPatchPart(poMP,
                                    poLastPoly,
                                    panPartType[iPart],
                                    nPartPoints,
                                    padfX + nPartStart,
                                    padfY + nPartStart,
                                    padfZ + nPartStart);
    }

    if( poLastPoly != NULL )
    {
        poMP->addGeometryDirectly( poLastPoly );
        poLastPoly = NULL;
    }

    return poMP;
}


/************************************************************************/
/*                      OGRWriteToShapeBin()                            */
/*                                                                      */
/*      Translate OGR geometry to a shapefile binary representation     */
/************************************************************************/

OGRErr OGRWriteToShapeBin( OGRGeometry *poGeom, 
                           GByte **ppabyShape,
                           int *pnBytes )
{
    GUInt32 nGType = SHPT_NULL;
    int nShpSize = 4; /* All types start with integer type number */
    int nShpZSize = 0; /* Z gets tacked onto the end */
    GUInt32 nPoints = 0;
    GUInt32 nParts = 0;

/* -------------------------------------------------------------------- */
/*      Null or Empty input maps to SHPT_NULL.                          */
/* -------------------------------------------------------------------- */
    if ( ! poGeom || poGeom->IsEmpty() )
    {
        *ppabyShape = (GByte*)VSIMalloc(nShpSize);
        GUInt32 zero = SHPT_NULL;
        memcpy(*ppabyShape, &zero, nShpSize);
        *pnBytes = nShpSize;
        return OGRERR_NONE;
    }

    OGRwkbGeometryType nOGRType = wkbFlatten(poGeom->getGeometryType());
    int b3d = (poGeom->getGeometryType() & wkb25DBit);
    int nCoordDims = b3d ? 3 : 2;

/* -------------------------------------------------------------------- */
/*      Calculate the shape buffer size                                 */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbPoint )
    {
        nShpSize += 8 * nCoordDims;
    }
    else if ( nOGRType == wkbLineString )
    {
        OGRLineString *poLine = (OGRLineString*)poGeom;
        nPoints = poLine->getNumPoints();
        nParts = 1;
        nShpSize += 16 * nCoordDims; /* xy(z) box */ 
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4; /* parts[1] */
        nShpSize += 8 * nCoordDims * nPoints; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbPolygon )
    {
        poGeom->closeRings();
        OGRPolygon *poPoly = (OGRPolygon*)poGeom;
        nParts = poPoly->getNumInteriorRings() + 1;
        for ( GUInt32 i = 0; i < nParts; i++ )
        {
            OGRLinearRing *poRing;
            if ( i == 0 )
                poRing = poPoly->getExteriorRing();
            else
                poRing = poPoly->getInteriorRing(i-1);
            nPoints += poRing->getNumPoints();
        }
        nShpSize += 16 * nCoordDims; /* xy(z) box */ 
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints; /* points */    
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiPoint )
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;
        for ( int i = 0; i < poMPoint->getNumGeometries(); i++ )
        {
            OGRPoint *poPoint = (OGRPoint*)(poMPoint->getGeometryRef(i));
            if ( poPoint->IsEmpty() ) 
                continue;
            nPoints++;
        }
        nShpSize += 16 * nCoordDims; /* xy(z) box */ 
        nShpSize += 4; /* npoints */
        nShpSize += 8 * nCoordDims * nPoints; /* points */    
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiLineString )
    {
        OGRMultiLineString *poMLine = (OGRMultiLineString*)poGeom;
        for ( int i = 0; i < poMLine->getNumGeometries(); i++ )
        {
            OGRLineString *poLine = (OGRLineString*)(poMLine->getGeometryRef(i));
            /* Skip empties */
            if ( poLine->IsEmpty() ) 
                continue;
            nParts++;
            nPoints += poLine->getNumPoints();
        }
        nShpSize += 16 * nCoordDims; /* xy(z) box */ 
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints ; /* points */    
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiPolygon )
    {
        poGeom->closeRings();
        OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;
        for( int j = 0; j < poMPoly->getNumGeometries(); j++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(j));
            int nRings = poPoly->getNumInteriorRings() + 1;

            /* Skip empties */
            if ( poPoly->IsEmpty() ) 
                continue;

            nParts += nRings;
            for ( int i = 0; i < nRings; i++ )
            {
                OGRLinearRing *poRing;
                if ( i == 0 )
                    poRing = poPoly->getExteriorRing();
                else
                    poRing = poPoly->getInteriorRing(i-1);
                nPoints += poRing->getNumPoints();
            }
        }
        nShpSize += 16 * nCoordDims; /* xy(z) box */ 
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints ; /* points */  
        nShpZSize = 16 + 8 * nPoints;
    }
    else
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    /* Allocate our shape buffer */
    *ppabyShape = (GByte*)VSIMalloc(nShpSize);
    if ( ! *ppabyShape )
        return OGRERR_FAILURE;

    /* Fill in the output size. */
    *pnBytes = nShpSize;

    /* Set up write pointers */
    unsigned char *pabyPtr = *ppabyShape;
    unsigned char *pabyPtrZ = NULL;
    if ( b3d )
        pabyPtrZ = pabyPtr + nShpSize - nShpZSize;

/* -------------------------------------------------------------------- */
/*      Write in the Shape type number now                              */
/* -------------------------------------------------------------------- */
    switch(nOGRType)
    {
        case wkbPoint:
        {
            nGType = b3d ? SHPT_POINTZ : SHPT_POINT;
            break;
        }
        case wkbMultiPoint:
        {
            nGType = b3d ? SHPT_MULTIPOINTZ : SHPT_MULTIPOINT;
            break;
        }
        case wkbLineString:
        case wkbMultiLineString:
        {
            nGType = b3d ? SHPT_ARCZ : SHPT_ARC;
            break;
        }
        case wkbPolygon:
        case wkbMultiPolygon:
        {
            nGType = b3d ? SHPT_POLYGONZ : SHPT_POLYGON;
            break;
        }
        default:
        {
            return OGRERR_UNSUPPORTED_OPERATION;
        }
    }
    /* Write in the type number and advance the pointer */
    nGType = CPL_LSBWORD32( nGType );
    memcpy( pabyPtr, &nGType, 4 );
    pabyPtr += 4;


/* -------------------------------------------------------------------- */
/*      POINT and POINTZ                                                */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint*)poGeom;
        double x = poPoint->getX();
        double y = poPoint->getY();

        /* Copy in the raw data. */
        memcpy( pabyPtr, &x, 8 );
        memcpy( pabyPtr+8, &y, 8 );
        if( b3d )
        {
            double z = poPoint->getZ();
            memcpy( pabyPtr+8+8, &z, 8 );
        }

        /* Swap if needed. Shape doubles always LSB */
        if( OGR_SWAP( wkbNDR ) )
        {
            CPL_SWAPDOUBLE( pabyPtr );
            CPL_SWAPDOUBLE( pabyPtr+8 );
            if( b3d )
                CPL_SWAPDOUBLE( pabyPtr+8+8 );
        }

        return OGRERR_NONE;    
    }

/* -------------------------------------------------------------------- */
/*      All the non-POINT types require an envelope next                */
/* -------------------------------------------------------------------- */
    OGREnvelope3D envelope;
    poGeom->getEnvelope(&envelope);
    memcpy( pabyPtr, &(envelope.MinX), 8 );
    memcpy( pabyPtr+8, &(envelope.MinY), 8 );
    memcpy( pabyPtr+8+8, &(envelope.MaxX), 8 );
    memcpy( pabyPtr+8+8+8, &(envelope.MaxY), 8 );

    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( int i = 0; i < 4; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 32;

    /* Write in the Z bounds at the end of the XY buffer */
    if ( b3d )
    {
        memcpy( pabyPtrZ, &(envelope.MinZ), 8 );
        memcpy( pabyPtrZ+8, &(envelope.MaxZ), 8 );

        /* Swap Z bounds if necessary */
        if( OGR_SWAP( wkbNDR ) )
        {
            for ( int i = 0; i < 2; i++ )
                CPL_SWAPDOUBLE( pabyPtrZ + 8*i );
        } 
        pabyPtrZ += 16;
    }

/* -------------------------------------------------------------------- */
/*      LINESTRING and LINESTRINGZ                                      */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbLineString )
    {
        const OGRLineString *poLine = (OGRLineString*)poGeom;

        /* Write in the nparts (1) */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the npoints */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

        /* Write in the part index (0) */
        GUInt32 nPartIndex = 0;
        memcpy( pabyPtr, &nPartIndex, 4 );
        pabyPtr += 4;

        /* Write in the point data */
        poLine->getPoints((OGRRawPoint*)pabyPtr, (double*)pabyPtrZ);

        /* Swap if necessary */
        if( OGR_SWAP( wkbNDR ) )
        {
            for( GUInt32 k = 0; k < nPoints; k++ )
            {
                CPL_SWAPDOUBLE( pabyPtr + 16*k );
                CPL_SWAPDOUBLE( pabyPtr + 16*k + 8 );
                if( b3d )
                    CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
            }
        }
        return OGRERR_NONE;    

    }
/* -------------------------------------------------------------------- */
/*      POLYGON and POLYGONZ                                            */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbPolygon )
    {
        OGRPolygon *poPoly = (OGRPolygon*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each ring and write an index number into   */
/*      the parts list, and the coordinates into the points list.       */
/*      to do it in one pass, we will use three write pointers.         */
/*      pabyPtr writes the part indexes                                 */
/*      pabyPoints writes the xy coordinates                            */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts; 

        int nPointIndexCount = 0;

        for( GUInt32 i = 0; i < nParts; i++ )
        {
            /* Check our Ring and condition it */
            OGRLinearRing *poRing;
            if ( i == 0 ) 
            {
                poRing = poPoly->getExteriorRing();
                /* Outer ring must be clockwise */
                if ( ! poRing->isClockwise() )
                    poRing->reverseWindingOrder();
            }
            else
            {
                poRing = poPoly->getInteriorRing(i-1);
                /* Inner rings should be anti-clockwise */
                if ( poRing->isClockwise() )
                    poRing->reverseWindingOrder();
            }

            int nRingNumPoints = poRing->getNumPoints();

            /* Cannot write un-closed rings to shape */
            if( nRingNumPoints <= 2 || ! poRing->get_IsClosed() )
                return OGRERR_FAILURE;

            /* Write in the part index */
            GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
            memcpy( pabyPtr, &nPartIndex, 4 );

            /* Write in the point data */
            poRing->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                for( int k = 0; k < nRingNumPoints; k++ )
                {
                    CPL_SWAPDOUBLE( pabyPoints + 16*k );
                    CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                    if( b3d )
                        CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                }
            }

            nPointIndexCount += nRingNumPoints;
            /* Advance the write pointers */
            pabyPtr += 4;
            pabyPoints += 16 * nRingNumPoints;
            if ( b3d )
                pabyPtrZ += 8 * nRingNumPoints; 
        }

        return OGRERR_NONE;

    }
/* -------------------------------------------------------------------- */
/*      MULTIPOINT and MULTIPOINTZ                                      */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbMultiPoint )
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each point write it into the points list   */
/*      We will use two write pointers.                                 */
/*      pabyPtr writes the xy coordinates                               */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        for( GUInt32 i = 0; i < nPoints; i++ )
        {
            const OGRPoint *poPt = (OGRPoint*)(poMPoint->getGeometryRef(i));

            /* Skip empties */
            if ( poPt->IsEmpty() ) 
                continue;

            /* Write the coordinates */
            double x = poPt->getX();
            double y = poPt->getY();
            memcpy(pabyPtr, &x, 8);
            memcpy(pabyPtr+8, &y, 8);
            if ( b3d )
            {
                double z = poPt->getZ();
                memcpy(pabyPtrZ, &z, 8);
            }

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                CPL_SWAPDOUBLE( pabyPtr );
                CPL_SWAPDOUBLE( pabyPtr + 8 );
                if( b3d )
                    CPL_SWAPDOUBLE( pabyPtrZ );
            }

            /* Advance the write pointers */
            pabyPtr += 16;
            if ( b3d )
                pabyPtrZ += 8; 
        }    

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      MULTILINESTRING and MULTILINESTRINGZ                            */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbMultiLineString )
    {
        OGRMultiLineString *poMLine = (OGRMultiLineString*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts; 

        int nPointIndexCount = 0;

        for( GUInt32 i = 0; i < nParts; i++ )
        {
            const OGRLineString *poLine = (OGRLineString*)(poMLine->getGeometryRef(i));

            /* Skip empties */
            if ( poLine->IsEmpty() ) 
                continue;

            int nLineNumPoints = poLine->getNumPoints();

            /* Write in the part index */
            GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
            memcpy( pabyPtr, &nPartIndex, 4 );

            /* Write in the point data */
            poLine->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                for( int k = 0; k < nLineNumPoints; k++ )
                {
                    CPL_SWAPDOUBLE( pabyPoints + 16*k );
                    CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                    if( b3d )
                        CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                }
            }

            nPointIndexCount += nLineNumPoints;

            /* Advance the write pointers */
            pabyPtr += 4;
            pabyPoints += 16 * nLineNumPoints;
            if ( b3d )
                pabyPtrZ += 8 * nLineNumPoints; 
        }

        return OGRERR_NONE;      

    }
/* -------------------------------------------------------------------- */
/*      MULTIPOLYGON and MULTIPOLYGONZ                                  */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbMultiPolygon )
    {
        OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each ring and write an index number into   */
/*      the parts list, and the coordinates into the points list.       */
/*      to do it in one pass, we will use three write pointers.         */
/*      pabyPtr writes the part indexes                                 */
/*      pabyPoints writes the xy coordinates                            */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts; 

        int nPointIndexCount = 0;

        for( int i = 0; i < poMPoly->getNumGeometries(); i++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(i));

            /* Skip empties */
            if ( poPoly->IsEmpty() ) 
                continue;

            int nRings = 1 + poPoly->getNumInteriorRings();

            for( int j = 0; j < nRings; j++ )
            {
                /* Check our Ring and condition it */
                OGRLinearRing *poRing;
                if ( j == 0 )
                {
                    poRing = poPoly->getExteriorRing();
                    /* Outer ring must be clockwise */
                    if ( ! poRing->isClockwise() )
                        poRing->reverseWindingOrder();
                }
                else
                {
                    poRing = poPoly->getInteriorRing(j-1);
                    /* Inner rings should be anti-clockwise */
                    if ( poRing->isClockwise() )
                        poRing->reverseWindingOrder();
                }

                int nRingNumPoints = poRing->getNumPoints();

                /* Cannot write closed rings to shape */
                if( nRingNumPoints <= 2 || ! poRing->get_IsClosed() )
                    return OGRERR_FAILURE;

                /* Write in the part index */
                GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
                memcpy( pabyPtr, &nPartIndex, 4 );

                /* Write in the point data */
                poRing->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);

                /* Swap if necessary */
                if( OGR_SWAP( wkbNDR ) )
                {
                    for( int k = 0; k < nRingNumPoints; k++ )
                    {
                        CPL_SWAPDOUBLE( pabyPoints + 16*k );
                        CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                        if( b3d )
                            CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                    }
                }

                nPointIndexCount += nRingNumPoints;
                /* Advance the write pointers */
                pabyPtr += 4;
                pabyPoints += 16 * nRingNumPoints;
                if ( b3d )
                    pabyPtrZ += 8 * nRingNumPoints; 
            }
        }

        return OGRERR_NONE;

    }
    else
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

}  


/************************************************************************/
/*                   OGRWriteMultiPatchToShapeBin()                     */
/************************************************************************/

OGRErr OGRWriteMultiPatchToShapeBin( OGRGeometry *poGeom, 
                                     GByte **ppabyShape,
                                     int *pnBytes )
{
    if( wkbFlatten(poGeom->getGeometryType()) != wkbMultiPolygon )
        return OGRERR_UNSUPPORTED_OPERATION;
    
    poGeom->closeRings();
    OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;
    int nParts = 0;
    int* panPartStart = NULL;
    int* panPartType = NULL;
    int nPoints = 0;
    OGRRawPoint* poPoints = NULL;
    double* padfZ = NULL;
    int nBeginLastPart = 0;

    for( int j = 0; j < poMPoly->getNumGeometries(); j++ )
    {
        OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(j));
        int nRings = poPoly->getNumInteriorRings() + 1;

        /* Skip empties */
        if ( poPoly->IsEmpty() ) 
            continue;

        OGRLinearRing *poRing = poPoly->getExteriorRing();
        if( nRings == 1 && poRing->getNumPoints() == 4 )
        {
            if( nParts > 0 &&
                ((panPartType[nParts-1] == SHPP_TRIANGLES && nPoints - panPartStart[nParts-1] == 3) ||
                 panPartType[nParts-1] == SHPP_TRIFAN) &&
                poRing->getX(0) == poPoints[nBeginLastPart].x &&
                poRing->getY(0) == poPoints[nBeginLastPart].y &&
                poRing->getZ(0) == padfZ[nBeginLastPart] &&
                poRing->getX(1) == poPoints[nPoints-1].x &&
                poRing->getY(1) == poPoints[nPoints-1].y &&
                poRing->getZ(1) == padfZ[nPoints-1] )
            {
                panPartType[nParts-1] = SHPP_TRIFAN;

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                            (nPoints + 1) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 1) * sizeof(double));
                poPoints[nPoints].x = poRing->getX(2);
                poPoints[nPoints].y = poRing->getY(2);
                padfZ[nPoints] = poRing->getZ(2);
                nPoints ++;
            }
            else if( nParts > 0 &&
                ((panPartType[nParts-1] == SHPP_TRIANGLES && nPoints - panPartStart[nParts-1] == 3) ||
                 panPartType[nParts-1] == SHPP_TRISTRIP) &&
                poRing->getX(0) == poPoints[nPoints-2].x &&
                poRing->getY(0) == poPoints[nPoints-2].y &&
                poRing->getZ(0) == padfZ[nPoints-2] &&
                poRing->getX(1) == poPoints[nPoints-1].x &&
                poRing->getY(1) == poPoints[nPoints-1].y &&
                poRing->getZ(1) == padfZ[nPoints-1] )
            {
                panPartType[nParts-1] = SHPP_TRISTRIP;

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                            (nPoints + 1) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 1) * sizeof(double));
                poPoints[nPoints].x = poRing->getX(2);
                poPoints[nPoints].y = poRing->getY(2);
                padfZ[nPoints] = poRing->getZ(2);
                nPoints ++;
            }
            else
            {
                if( nParts == 0 || panPartType[nParts-1] != SHPP_TRIANGLES )
                {
                    nBeginLastPart = nPoints;

                    panPartStart = (int*)CPLRealloc(panPartStart, (nParts + 1) * sizeof(int));
                    panPartType = (int*)CPLRealloc(panPartType, (nParts + 1) * sizeof(int));
                    panPartStart[nParts] = nPoints;
                    panPartType[nParts] = SHPP_TRIANGLES;
                    nParts ++;
                }

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                        (nPoints + 3) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 3) * sizeof(double));
                for(int i=0;i<3;i++)
                {
                    poPoints[nPoints+i].x = poRing->getX(i);
                    poPoints[nPoints+i].y = poRing->getY(i);
                    padfZ[nPoints+i] = poRing->getZ(i);
                }
                nPoints += 3;
            }
        }
        else
        {
            panPartStart = (int*)CPLRealloc(panPartStart, (nParts + nRings) * sizeof(int));
            panPartType = (int*)CPLRealloc(panPartType, (nParts + nRings) * sizeof(int));

            for ( int i = 0; i < nRings; i++ )
            {
                panPartStart[nParts + i] = nPoints;
                if ( i == 0 )
                {
                    poRing = poPoly->getExteriorRing();
                    panPartType[nParts + i] = SHPP_OUTERRING;
                }
                else
                {
                    poRing = poPoly->getInteriorRing(i-1);
                    panPartType[nParts + i] = SHPP_INNERRING;
                }
                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                        (nPoints + poRing->getNumPoints()) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ,
                        (nPoints + poRing->getNumPoints()) * sizeof(double));
                for( int k = 0; k < poRing->getNumPoints(); k++ )
                {
                    poPoints[nPoints+k].x = poRing->getX(k);
                    poPoints[nPoints+k].y = poRing->getY(k);
                    padfZ[nPoints+k] = poRing->getZ(k);
                }
                nPoints += poRing->getNumPoints();
            }

            nParts += nRings;
        }
    }

    int nShpSize = 4; /* All types start with integer type number */
    nShpSize += 16 * 2; /* xy bbox */ 
    nShpSize += 4; /* nparts */
    nShpSize += 4; /* npoints */
    nShpSize += 4 * nParts; /* panPartStart[nparts] */
    nShpSize += 4 * nParts; /* panPartType[nparts] */
    nShpSize += 8 * 2 * nPoints; /* xy points */
    nShpSize += 16; /* z bbox */
    nShpSize += 8 * nPoints; /* z points */

    *pnBytes = nShpSize;
    *ppabyShape = (GByte*) CPLMalloc(nShpSize);

    GByte* pabyPtr = *ppabyShape;

    /* Write in the type number and advance the pointer */
    GUInt32 nGType = CPL_LSBWORD32( SHPT_MULTIPATCH );
    memcpy( pabyPtr, &nGType, 4 );
    pabyPtr += 4;

    OGREnvelope3D envelope;
    poGeom->getEnvelope(&envelope);
    memcpy( pabyPtr, &(envelope.MinX), 8 );
    memcpy( pabyPtr+8, &(envelope.MinY), 8 );
    memcpy( pabyPtr+8+8, &(envelope.MaxX), 8 );
    memcpy( pabyPtr+8+8+8, &(envelope.MaxY), 8 );

    int i;
    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( i = 0; i < 4; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 32;

    /* Write in the part count */
    GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
    memcpy( pabyPtr, &nPartsLsb, 4 );
    pabyPtr += 4;

    /* Write in the total point count */
    GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
    memcpy( pabyPtr, &nPointsLsb, 4 );
    pabyPtr += 4;

    for( i = 0; i < nParts; i ++ )
    {
        int nPartStart = CPL_LSBWORD32(panPartStart[i]);
        memcpy( pabyPtr, &nPartStart, 4 );
        pabyPtr += 4;
    }
    for( i = 0; i < nParts; i ++ )
    {
        int nPartType = CPL_LSBWORD32(panPartType[i]);
        memcpy( pabyPtr, &nPartType, 4 );
        pabyPtr += 4;
    }

    memcpy(pabyPtr, poPoints, 2 * 8 * nPoints);
    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( i = 0; i < 2 * nPoints; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 2 * 8 * nPoints;

    memcpy( pabyPtr, &(envelope.MinZ), 8 );
    memcpy( pabyPtr+8, &(envelope.MaxZ), 8 );
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( i = 0; i < 2; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 16;
    
    memcpy(pabyPtr, padfZ, 8 * nPoints);
    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( i = 0; i < nPoints; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr +=  8 * nPoints;
    
    CPLFree(panPartStart);
    CPLFree(panPartType);
    CPLFree(poPoints);
    CPLFree(padfZ);

    return OGRERR_NONE;
}

/************************************************************************/
/*                      OGRCreateFromShapeBin()                         */
/*                                                                      */
/*      Translate shapefile binary representation to an OGR             */
/*      geometry.                                                       */
/************************************************************************/

OGRErr OGRCreateFromShapeBin( GByte *pabyShape,
                              OGRGeometry **ppoGeom,
                              int nBytes )

{
    *ppoGeom = NULL;

    if( nBytes < 4 )
    {    
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Shape buffer size (%d) too small",
                 nBytes);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*  Detect zlib compressed shapes and uncompress buffer if necessary    */
/*  NOTE: this seems to be an undocumented feature, even in the         */
/*  extended_shapefile_format.pdf found in the FileGDB API documentation*/
/* -------------------------------------------------------------------- */
    if( nBytes >= 14 &&
        pabyShape[12] == 0x78 && pabyShape[13] == 0xDA /* zlib marker */)
    {
        GInt32 nUncompressedSize, nCompressedSize;
        memcpy( &nUncompressedSize, pabyShape + 4, 4 );
        memcpy( &nCompressedSize, pabyShape + 8, 4 );
        CPL_LSBPTR32( &nUncompressedSize );
        CPL_LSBPTR32( &nCompressedSize );
        if (nCompressedSize + 12 == nBytes &&
            nUncompressedSize > 0)
        {
            GByte* pabyUncompressedBuffer = (GByte*)VSIMalloc(nUncompressedSize);
            if (pabyUncompressedBuffer == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate %d bytes to uncompress zlib buffer",
                         nUncompressedSize);
                return OGRERR_FAILURE;
            }

            size_t nRealUncompressedSize = 0;
            if( CPLZLibInflate( pabyShape + 12, nCompressedSize,
                                pabyUncompressedBuffer, nUncompressedSize,
                                &nRealUncompressedSize ) == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "CPLZLibInflate() failed");
                VSIFree(pabyUncompressedBuffer);
                return OGRERR_FAILURE;
            }

            OGRErr eErr = OGRCreateFromShapeBin(pabyUncompressedBuffer,
                                                ppoGeom,
                                                nRealUncompressedSize);

            VSIFree(pabyUncompressedBuffer);

            return eErr;
        }
    }

    int nSHPType = pabyShape[0];

/* -------------------------------------------------------------------- */
/*      Return a NULL geometry when SHPT_NULL is encountered.           */
/*      Watch out, null return does not mean "bad data" it means        */
/*      "no geometry here". Watch the OGRErr for the error status       */
/* -------------------------------------------------------------------- */
    if ( nSHPType == SHPT_NULL )
    {
      *ppoGeom = NULL;
      return OGRERR_NONE;
    }

//    CPLDebug( "PGeo",
//              "Shape type read from PGeo data is nSHPType = %d",
//              nSHPType );

/* -------------------------------------------------------------------- */
/*      TODO: These types include additional attributes including       */
/*      non-linear segments and such. They should be handled.           */
/*      This is documented in the extended_shapefile_format.pdf         */
/*      from the FileGDB API                                            */
/* -------------------------------------------------------------------- */
    switch( nSHPType )
    {
      case SHPT_GENERALPOLYLINE:
        nSHPType = SHPT_ARC;
        break;
      case SHPT_GENERALPOLYGON:
        nSHPType = SHPT_POLYGON;
        break;
      case SHPT_GENERALPOINT:
        nSHPType = SHPT_POINT;
        break;
      case SHPT_GENERALMULTIPOINT:
        nSHPType = SHPT_MULTIPOINT;
        break;
      case SHPT_GENERALMULTIPATCH:
        nSHPType = SHPT_MULTIPATCH;
    }

/* ==================================================================== */
/*  Extract vertices for a Polygon or Arc.              */
/* ==================================================================== */
    if(    nSHPType == SHPT_ARC
        || nSHPType == SHPT_ARCZ
        || nSHPType == SHPT_ARCM
        || nSHPType == SHPT_ARCZM
        || nSHPType == SHPT_POLYGON
        || nSHPType == SHPT_POLYGONZ
        || nSHPType == SHPT_POLYGONM
        || nSHPType == SHPT_POLYGONZM
        || nSHPType == SHPT_MULTIPATCH
        || nSHPType == SHPT_MULTIPATCHM)
    {
        GInt32         nPoints, nParts;
        int            i, nOffset;
        GInt32         *panPartStart;
        GInt32         *panPartType = NULL;

        if (nBytes < 44)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nBytes=%d, nSHPType=%d", nBytes, nSHPType);
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Extract part/point count, and build vertex and part arrays      */
/*      to proper size.                                                 */
/* -------------------------------------------------------------------- */
        memcpy( &nPoints, pabyShape + 40, 4 );
        memcpy( &nParts, pabyShape + 36, 4 );

        CPL_LSBPTR32( &nPoints );
        CPL_LSBPTR32( &nParts );

        if (nPoints < 0 || nParts < 0 ||
            nPoints > 50 * 1000 * 1000 || nParts > 10 * 1000 * 1000)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Corrupted Shape : nPoints=%d, nParts=%d.",
                     nPoints, nParts);
            return OGRERR_FAILURE;
        }

        int bHasZ = (  nSHPType == SHPT_POLYGONZ
                    || nSHPType == SHPT_POLYGONZM
                    || nSHPType == SHPT_ARCZ
                    || nSHPType == SHPT_ARCZM
                    || nSHPType == SHPT_MULTIPATCH
                    || nSHPType == SHPT_MULTIPATCHM );

        int bIsMultiPatch = ( nSHPType == SHPT_MULTIPATCH || nSHPType == SHPT_MULTIPATCHM );

        /* With the previous checks on nPoints and nParts, */
        /* we should not overflow here and after */
        /* since 50 M * (16 + 8 + 8) = 1 600 MB */
        int nRequiredSize = 44 + 4 * nParts + 16 * nPoints;
        if ( bHasZ )
        {
            nRequiredSize += 16 + 8 * nPoints;
        }
        if( bIsMultiPatch )
        {
            nRequiredSize += 4 * nParts;
        }
        if (nRequiredSize > nBytes)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nPoints=%d, nParts=%d, nBytes=%d, nSHPType=%d, nRequiredSize=%d",
                     nPoints, nParts, nBytes, nSHPType, nRequiredSize);
            return OGRERR_FAILURE;
        }

        panPartStart = (GInt32 *) VSICalloc(nParts,sizeof(GInt32));
        if (panPartStart == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Not enough memory for shape (nPoints=%d, nParts=%d)", nPoints, nParts);
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Copy out the part array from the record.                        */
/* -------------------------------------------------------------------- */
        memcpy( panPartStart, pabyShape + 44, 4 * nParts );
        for( i = 0; i < nParts; i++ )
        {
            CPL_LSBPTR32( panPartStart + i );

            /* We check that the offset is inside the vertex array */
            if (panPartStart[i] < 0 ||
                panPartStart[i] >= nPoints)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Corrupted Shape : panPartStart[%d] = %d, nPoints = %d",
                        i, panPartStart[i], nPoints);
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }
            if (i > 0 && panPartStart[i] <= panPartStart[i-1])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Corrupted Shape : panPartStart[%d] = %d, panPartStart[%d] = %d",
                        i, panPartStart[i], i - 1, panPartStart[i - 1]);
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }
        }

        nOffset = 44 + 4*nParts;

/* -------------------------------------------------------------------- */
/*      If this is a multipatch, we will also have parts types.         */
/* -------------------------------------------------------------------- */
        if( bIsMultiPatch )
        {
            panPartType = (GInt32 *) VSICalloc(nParts,sizeof(GInt32));
            if (panPartType == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                        "Not enough memory for panPartType for shape (nPoints=%d, nParts=%d)", nPoints, nParts);
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }

            memcpy( panPartType, pabyShape + nOffset, 4*nParts );
            for( i = 0; i < nParts; i++ )
            {
                CPL_LSBPTR32( panPartType + i );
            }
            nOffset += 4*nParts;
        }

/* -------------------------------------------------------------------- */
/*      Copy out the vertices from the record.                          */
/* -------------------------------------------------------------------- */
        double *padfX = (double *) VSIMalloc(sizeof(double)*nPoints);
        double *padfY = (double *) VSIMalloc(sizeof(double)*nPoints);
        double *padfZ = (double *) VSICalloc(sizeof(double),nPoints);
        if (padfX == NULL || padfY == NULL || padfZ == NULL)
        {
            CPLFree( panPartStart );
            CPLFree( panPartType );
            CPLFree( padfX );
            CPLFree( padfY );
            CPLFree( padfZ );
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Not enough memory for shape (nPoints=%d, nParts=%d)", nPoints, nParts);
            return OGRERR_FAILURE;
        }

        for( i = 0; i < nPoints; i++ )
        {
            memcpy(padfX + i, pabyShape + nOffset + i * 16, 8 );
            memcpy(padfY + i, pabyShape + nOffset + i * 16 + 8, 8 );
            CPL_LSBPTR64( padfX + i );
            CPL_LSBPTR64( padfY + i );
        }

        nOffset += 16*nPoints;

/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( bHasZ )
        {
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( padfZ + i, pabyShape + nOffset + 16 + i*8, 8 );
                CPL_LSBPTR64( padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      Build corresponding OGR objects.                                */
/* -------------------------------------------------------------------- */
        if(    nSHPType == SHPT_ARC
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_ARCM
            || nSHPType == SHPT_ARCZM )
        {
/* -------------------------------------------------------------------- */
/*      Arc - As LineString                                             */
/* -------------------------------------------------------------------- */
            if( nParts == 1 )
            {
                OGRLineString *poLine = new OGRLineString();
                *ppoGeom = poLine;

                poLine->setPoints( nPoints, padfX, padfY, padfZ );
            }

/* -------------------------------------------------------------------- */
/*      Arc - As MultiLineString                                        */
/* -------------------------------------------------------------------- */
            else
            {
                OGRMultiLineString *poMulti = new OGRMultiLineString;
                *ppoGeom = poMulti;

                for( i = 0; i < nParts; i++ )
                {
                    OGRLineString *poLine = new OGRLineString;
                    int nVerticesInThisPart;

                    if( i == nParts-1 )
                        nVerticesInThisPart = nPoints - panPartStart[i];
                    else
                        nVerticesInThisPart =
                            panPartStart[i+1] - panPartStart[i];

                    poLine->setPoints( nVerticesInThisPart,
                                       padfX + panPartStart[i],
                                       padfY + panPartStart[i],
                                       padfZ + panPartStart[i] );

                    poMulti->addGeometryDirectly( poLine );
                }
            }
        } /* ARC */

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
        else if(    nSHPType == SHPT_POLYGON
                 || nSHPType == SHPT_POLYGONZ
                 || nSHPType == SHPT_POLYGONM
                 || nSHPType == SHPT_POLYGONZM )
        {
            if (nParts != 0)
            {
                if (nParts == 1)
                {
                    OGRPolygon *poOGRPoly = new OGRPolygon;
                    *ppoGeom = poOGRPoly;
                    OGRLinearRing *poRing = new OGRLinearRing;
                    int nVerticesInThisPart = nPoints - panPartStart[0];

                    poRing->setPoints( nVerticesInThisPart,
                                       padfX + panPartStart[0],
                                       padfY + panPartStart[0],
                                       padfZ + panPartStart[0] );

                    poOGRPoly->addRingDirectly( poRing );
                }
                else
                {
                    OGRGeometry *poOGR = NULL;
                    OGRPolygon** tabPolygons = new OGRPolygon*[nParts];

                    for( i = 0; i < nParts; i++ )
                    {
                        tabPolygons[i] = new OGRPolygon();
                        OGRLinearRing *poRing = new OGRLinearRing;
                        int nVerticesInThisPart;

                        if( i == nParts-1 )
                            nVerticesInThisPart = nPoints - panPartStart[i];
                        else
                            nVerticesInThisPart =
                                panPartStart[i+1] - panPartStart[i];

                        poRing->setPoints( nVerticesInThisPart,
                                           padfX + panPartStart[i],
                                           padfY + panPartStart[i],
                                           padfZ + panPartStart[i] );
                        tabPolygons[i]->addRingDirectly(poRing);
                    }

                    int isValidGeometry;
                    const char* papszOptions[] = { "METHOD=ONLY_CCW", NULL };
                    poOGR = OGRGeometryFactory::organizePolygons(
                        (OGRGeometry**)tabPolygons, nParts, &isValidGeometry, papszOptions );

                    if (!isValidGeometry)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Geometry of polygon cannot be translated to Simple Geometry. "
                                 "All polygons will be contained in a multipolygon.\n");
                    }

                    *ppoGeom = poOGR;
                    delete[] tabPolygons;
                }
            }
        } /* polygon */

/* -------------------------------------------------------------------- */
/*      Multipatch                                                      */
/* -------------------------------------------------------------------- */
        else if( bIsMultiPatch )
        {
            *ppoGeom = OGRCreateFromMultiPatch( nParts,
                                                panPartStart,
                                                panPartType,
                                                nPoints,
                                                padfX,
                                                padfY,
                                                padfZ );
        }

        CPLFree( panPartStart );
        CPLFree( panPartType );
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );

        if (*ppoGeom != NULL)
            (*ppoGeom)->setCoordinateDimension( bHasZ ? 3 : 2 );

        return OGRERR_NONE;
    }

/* ==================================================================== */
/*  Extract vertices for a MultiPoint.                  */
/* ==================================================================== */
    else if(    nSHPType == SHPT_MULTIPOINT
             || nSHPType == SHPT_MULTIPOINTM
             || nSHPType == SHPT_MULTIPOINTZ
             || nSHPType == SHPT_MULTIPOINTZM )
    {
      GInt32 nPoints;
      GInt32 nOffsetZ;
      int i;

      int bHasZ = (  nSHPType == SHPT_MULTIPOINTZ
                  || nSHPType == SHPT_MULTIPOINTZM );

                
      memcpy( &nPoints, pabyShape + 36, 4 );
      CPL_LSBPTR32( &nPoints );

      if (nPoints < 0 || nPoints > 50 * 1000 * 1000 )
      {
          CPLError(CE_Failure, CPLE_AppDefined, "Corrupted Shape : nPoints=%d.",
                   nPoints);
          return OGRERR_FAILURE;
      }

      nOffsetZ = 40 + 2*8*nPoints + 2*8;
    
      OGRMultiPoint *poMultiPt = new OGRMultiPoint;
      *ppoGeom = poMultiPt;

      for( i = 0; i < nPoints; i++ )
      {
          double x, y, z;
          OGRPoint *poPt = new OGRPoint;
        
          /* Copy X */
          memcpy(&x, pabyShape + 40 + i*16, 8);
          CPL_LSBPTR64(&x);
          poPt->setX(x);
        
          /* Copy Y */
          memcpy(&y, pabyShape + 40 + i*16 + 8, 8);
          CPL_LSBPTR64(&y);
          poPt->setY(y);
        
          /* Copy Z */
          if ( bHasZ )
          {
            memcpy(&z, pabyShape + nOffsetZ + i*8, 8);
            CPL_LSBPTR64(&z);
            poPt->setZ(z);
          }
        
          poMultiPt->addGeometryDirectly( poPt );
      }
      
      poMultiPt->setCoordinateDimension( bHasZ ? 3 : 2 );
      
      return OGRERR_NONE;
    }

/* ==================================================================== */
/*      Extract vertices for a point.                                   */
/* ==================================================================== */
    else if(    nSHPType == SHPT_POINT
             || nSHPType == SHPT_POINTM
             || nSHPType == SHPT_POINTZ
             || nSHPType == SHPT_POINTZM )
    {
        /* int nOffset; */
        double  dfX, dfY, dfZ = 0;

        int bHasZ = (nSHPType == SHPT_POINTZ || nSHPType == SHPT_POINTZM);

        if (nBytes < 4 + 8 + 8 + ((bHasZ) ? 8 : 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nBytes=%d, nSHPType=%d", nBytes, nSHPType);
            return OGRERR_FAILURE;
        }

        memcpy( &dfX, pabyShape + 4, 8 );
        memcpy( &dfY, pabyShape + 4 + 8, 8 );

        CPL_LSBPTR64( &dfX );
        CPL_LSBPTR64( &dfY );
        /* nOffset = 20 + 8; */

        if( bHasZ )
        {
            memcpy( &dfZ, pabyShape + 4 + 16, 8 );
            CPL_LSBPTR64( &dfZ );
        }

        *ppoGeom = new OGRPoint( dfX, dfY, dfZ );
        (*ppoGeom)->setCoordinateDimension( bHasZ ? 3 : 2 );

        return OGRERR_NONE;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Unsupported geometry type: %d",
              nSHPType );

    return OGRERR_FAILURE;
}
