/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolygon geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_geos.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             OGRPolygon()                             */
/************************************************************************/

/**
 * \brief Create an empty polygon.
 */

OGRPolygon::OGRPolygon()

{
}

/************************************************************************/
/*                            ~OGRPolygon()                             */
/************************************************************************/

OGRPolygon::~OGRPolygon()

{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolygon::getGeometryType() const

{
    if( nCoordDimension == 3 )
        return wkbPolygon25D;
    else
        return wkbPolygon;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRPolygon::getGeometryName() const

{
    return "POLYGON";
}

/************************************************************************/
/*                          getExteriorRing()                           */
/************************************************************************/

/**
 * \brief Fetch reference to external polygon ring.
 *
 * Note that the returned ring pointer is to an internal data object of
 * the OGRPolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_ExteriorRing() method.
 *
 * @return pointer to external ring.  May be NULL if the OGRPolygon is empty.
 */

OGRLinearRing *OGRPolygon::getExteriorRing()

{
    if( oCC.nCurveCount > 0 )
        return (OGRLinearRing*) oCC.papoCurves[0];
    else
        return NULL;
}

const OGRLinearRing *OGRPolygon::getExteriorRing() const

{
    if( oCC.nCurveCount > 0 )
        return (OGRLinearRing*) oCC.papoCurves[0];
    else
        return NULL;
}

/************************************************************************/
/*                          stealExteriorRing()                         */
/************************************************************************/

/**
 * \brief "Steal" reference to external polygon ring.
 *
 * After the call to that function, only call to stealInteriorRing() or
 * destruction of the OGRPolygon is valid. Other operations may crash.
 *
 * @return pointer to external ring.  May be NULL if the OGRPolygon is empty.
 */

OGRLinearRing *OGRPolygon::stealExteriorRing()
{
    return (OGRLinearRing*)stealExteriorRingCurve();
}

/************************************************************************/
/*                          getInteriorRing()                           */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned ring pointer is to an internal data object of
 * the OGRPolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_InternalRing() method.
 *
 * @param iRing internal ring index from 0 to getNumInternalRings() - 1.
 *
 * @return pointer to interior ring.  May be NULL.
 */

OGRLinearRing *OGRPolygon::getInteriorRing( int iRing )

{
    if( iRing < 0 || iRing >= oCC.nCurveCount-1 )
        return NULL;
    else
        return (OGRLinearRing*) oCC.papoCurves[iRing+1];
}

const OGRLinearRing *OGRPolygon::getInteriorRing( int iRing ) const

{
    if( iRing < 0 || iRing >= oCC.nCurveCount-1 )
        return NULL;
    else
        return (OGRLinearRing*) oCC.papoCurves[iRing+1];
}

/************************************************************************/
/*                          stealInteriorRing()                         */
/************************************************************************/

/**
 * \brief "Steal" reference to indicated interior ring.
 *
 * After the call to that function, only call to stealInteriorRing() or
 * destruction of the OGRPolygon is valid. Other operations may crash.
 *
 * @param iRing internal ring index from 0 to getNumInternalRings() - 1.
 * @return pointer to interior ring.  May be NULL.
 */

OGRLinearRing *OGRPolygon::stealInteriorRing(int iRing)
{
    if( iRing < 0 || iRing >= oCC.nCurveCount-1 )
        return NULL;
    OGRLinearRing *poRet = (OGRLinearRing*) oCC.papoCurves[iRing+1];
    oCC.papoCurves[iRing+1] = NULL;
    return poRet;
}

/************************************************************************/
/*                            checkRing()                               */
/************************************************************************/

int OGRPolygon::checkRing( OGRCurve * poNewRing ) const
{
    if ( !(EQUAL(poNewRing->getGeometryName(), "LINEARRING")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong curve type. Expected LINEARRING.");
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPolygon::WkbSize() const

{
    int         nSize = 9;
    int         b3D = getCoordinateDimension() == 3;

    for( int i = 0; i < oCC.nCurveCount; i++ )
    {
        nSize += ((OGRLinearRing*)oCC.papoCurves[i])->_WkbSize( b3D );
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
                                  int nSize,
                                  OGRwkbVariant eWkbVariant )

{
    OGRwkbByteOrder eByteOrder;
    int nDataOffset = 0;
    OGRErr eErr = oCC.importPreambuleFromWkb(this, pabyData, nSize, nDataOffset,
                                             eByteOrder, 4, eWkbVariant);
    if( eErr >= 0 )
        return eErr;

    int b3D = (nCoordDimension == 3);
/* -------------------------------------------------------------------- */
/*      Get the rings.                                                  */
/* -------------------------------------------------------------------- */
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRErr  eErr;
        
        OGRLinearRing* poLR = new OGRLinearRing();
        oCC.papoCurves[iRing] = poLR;
        eErr = poLR->_importFromWkb( eByteOrder, b3D,
                                                 pabyData + nDataOffset,
                                                 nSize );
        if( eErr != OGRERR_NONE )
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return eErr;
        }

        if( nSize != -1 )
            nSize -= poLR->_WkbSize( b3D );

        nDataOffset += poLR->_WkbSize( b3D );
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRPolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                 unsigned char * pabyData,
                                 OGRwkbVariant eWkbVariant ) const

{
    int         nOffset;
    int         b3D = getCoordinateDimension() == 3;
    
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();

    if ( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();
    
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

        nCount = CPL_SWAP32( oCC.nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &oCC.nCurveCount, 4 );
    }
    
    nOffset = 9;
    
/* ==================================================================== */
/*      Serialize each of the rings.                                    */
/* ==================================================================== */
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        poLR->_exportToWkb( eByteOrder, b3D,
                                        pabyData + nOffset );

        nOffset += poLR->_WkbSize(b3D);
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `POLYGON ((x y, x y, ...),(x y, ...),...)'.                     */
/************************************************************************/

OGRErr OGRPolygon::importFromWkt( char ** ppszInput )

{
    int bHasZ = FALSE, bHasM = FALSE;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM);
    if( eErr >= 0 )
        return eErr;

    OGRRawPoint *paoPoints = NULL;
    int          nMaxPoints = 0;
    double      *padfZ = NULL;

    eErr = importFromWKTListOnly( ppszInput, bHasZ, bHasM,
                                  paoPoints, nMaxPoints, padfZ );

    CPLFree( paoPoints );
    CPLFree( padfZ );

    return eErr;
}

/************************************************************************/
/*                        importFromWKTListOnly()                       */
/*                                                                      */
/*      Instantiate from "((x y, x y, ...),(x y, ...),...)"             */
/************************************************************************/

OGRErr OGRPolygon::importFromWKTListOnly( char ** ppszInput, int bHasZ, int bHasM,
                                          OGRRawPoint*& paoPoints, int& nMaxPoints,
                                          double*& padfZ )

{
    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken, "EMPTY") )
    {
        *ppszInput = (char*) pszInput;
        return OGRERR_NONE;
    }
    if( !EQUAL(szToken, "(") )
        return OGRERR_CORRUPT_DATA;

/* ==================================================================== */
/*      Read each ring in turn.  Note that we try to reuse the same     */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    int         nMaxRings = 0;

    nCoordDimension = 2;
    
    do
    {
        int     nPoints = 0;

        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if (EQUAL(szToken,"EMPTY"))
        {
/* -------------------------------------------------------------------- */
/*      Do we need to grow the ring array?                              */
/* -------------------------------------------------------------------- */
            if( oCC.nCurveCount == nMaxRings )
            {
                nMaxRings = nMaxRings * 2 + 1;
                oCC.papoCurves = (OGRCurve **)
                    CPLRealloc(oCC.papoCurves, nMaxRings * sizeof(OGRLinearRing*));
            }
            oCC.papoCurves[oCC.nCurveCount] = new OGRLinearRing();
            oCC.nCurveCount++;

            pszInput = OGRWktReadToken( pszNext, szToken );
            if ( !EQUAL(szToken, ",") )
                break;

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Read points for one ring from input.                            */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadPoints( pszInput, &paoPoints, &padfZ, &nMaxPoints,
                                     &nPoints );

        if( pszInput == NULL || nPoints == 0 )
        {
            return OGRERR_CORRUPT_DATA;
        }
        
/* -------------------------------------------------------------------- */
/*      Do we need to grow the ring array?                              */
/* -------------------------------------------------------------------- */
        if( oCC.nCurveCount == nMaxRings )
        {
            nMaxRings = nMaxRings * 2 + 1;
            oCC.papoCurves = (OGRCurve **)
                CPLRealloc(oCC.papoCurves, nMaxRings * sizeof(OGRLinearRing*));
        }

/* -------------------------------------------------------------------- */
/*      Create the new ring, and assign to ring list.                   */
/* -------------------------------------------------------------------- */
        OGRLinearRing* poLR = new OGRLinearRing();
        oCC.papoCurves[oCC.nCurveCount] = poLR;
        /* Ignore Z array when we have a POLYGON M */
        if (bHasM && !bHasZ)
            poLR->setPoints( nPoints, paoPoints, NULL );
        else
            poLR->setPoints( nPoints, paoPoints, padfZ );

        oCC.nCurveCount++;

        if( padfZ && !(bHasM && !bHasZ) )
            nCoordDimension = 3;

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

OGRErr OGRPolygon::exportToWkt( char ** ppszDstText,
                                OGRwkbVariant eWkbVariant ) const

{
    char        **papszRings;
    int         iRing, nCumulativeLength = 0, nNonEmptyRings = 0;
    OGRErr      eErr;
    int         bMustWriteComma = FALSE;

/* -------------------------------------------------------------------- */
/*      If we have no valid exterior ring, return POLYGON EMPTY.        */
/* -------------------------------------------------------------------- */
    if (getExteriorRing() == NULL || 
        getExteriorRing()->IsEmpty() )
    {
        if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
            *ppszDstText = CPLStrdup("POLYGON Z EMPTY");
        else
            *ppszDstText = CPLStrdup("POLYGON EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    papszRings = (char **) CPLCalloc(sizeof(char *),oCC.nCurveCount);

    for( iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        poLR->setCoordinateDimension( getCoordinateDimension() );
        if( poLR->getNumPoints() == 0 )
        {
            papszRings[iRing] = NULL;
            continue;
        }

        eErr = poLR->exportToWkt( &(papszRings[iRing]) );
        if( eErr != OGRERR_NONE )
            goto error;

        CPLAssert( EQUALN(papszRings[iRing],"LINEARRING (", 12) );
        nCumulativeLength += strlen(papszRings[iRing] + 11);

        nNonEmptyRings++;
    }

/* -------------------------------------------------------------------- */
/*      Allocate exactly the right amount of space for the              */
/*      aggregated string.                                              */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength + nNonEmptyRings + 15);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    if( getCoordinateDimension() == 3 && eWkbVariant == wkbVariantIso )
        strcpy( *ppszDstText, "POLYGON Z (" );
    else
        strcpy( *ppszDstText, "POLYGON (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {                                                           
        if( papszRings[iRing] == NULL )
        {
            CPLDebug( "OGR", "OGRPolygon::exportToWkt() - skipping empty ring.");
            continue;
        }

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = TRUE;
        
        int nRingLen = strlen(papszRings[iRing] + 11);
        memcpy( *ppszDstText + nCumulativeLength, papszRings[iRing] + 11, nRingLen );
        nCumulativeLength += nRingLen;
        VSIFree( papszRings[iRing] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszRings );

    return OGRERR_NONE;

error:
    for( iRing = 0; iRing < oCC.nCurveCount; iRing++ )
        CPLFree(papszRings[iRing]);
    CPLFree(papszRings);
    return eErr;
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

int OGRPolygon::PointOnSurface( OGRPoint *poPoint ) const

{
    if( poPoint == NULL || poPoint->IsEmpty() )
        return OGRERR_FAILURE;

    OGRGeometryH hInsidePoint = OGR_G_PointOnSurface( (OGRGeometryH) this );
    if( hInsidePoint == NULL )
        return OGRERR_FAILURE;

    OGRPoint *poInsidePoint = (OGRPoint *) hInsidePoint;
    if( poInsidePoint->IsEmpty() )
        poPoint->empty();
    else
    {
        poPoint->setX( poInsidePoint->getX() );
        poPoint->setY( poInsidePoint->getY() );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           IsPointOnSurface()                           */
/************************************************************************/

OGRBoolean OGRPolygon::IsPointOnSurface( const OGRPoint * pt) const
{
    if ( NULL == pt)
        return 0;

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        if ( ((OGRLinearRing*)oCC.papoCurves[iRing])->isPointInRing(pt) )
        {
            return 1;
        }
    }

    return 0;
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRPolygon::closeRings()

{
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
        oCC.papoCurves[iRing]->closeRings();
}

/************************************************************************/
/*                           CurvePolyToPoly()                          */
/************************************************************************/

OGRPolygon* OGRPolygon::CurvePolyToPoly(CPL_UNUSED double dfMaxAngleStepSizeDegrees,
                                        CPL_UNUSED const char* const* papszOptions) const
{
    return (OGRPolygon*) clone();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRPolygon::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
{
    return FALSE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry* OGRPolygon::getLinearGeometry(double dfMaxAngleStepSizeDegrees,
                                             const char* const* papszOptions) const
{
    return OGRGeometry::getLinearGeometry(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                             getCurveGeometry()                       */
/************************************************************************/

OGRGeometry* OGRPolygon::getCurveGeometry(const char* const* papszOptions) const
{
    OGRCurvePolygon* poCC = new OGRCurvePolygon();
    poCC->assignSpatialReference( getSpatialReference() );
    int bHasCurveGeometry = FALSE;
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRCurve* poSubGeom = (OGRCurve* )oCC.papoCurves[iRing]->getCurveGeometry(papszOptions);
        if( wkbFlatten(poSubGeom->getGeometryType()) != wkbLineString )
            bHasCurveGeometry = TRUE;
        poCC->addRingDirectly( poSubGeom );
    }
    if( !bHasCurveGeometry )
    {
        delete poCC;
        return clone();
    }
    return poCC;
}

/************************************************************************/
/*                        CastToCurvePolygon()                          */
/************************************************************************/

/**
 * \brief Cast to curve polygon.
 *
 * The passed in geometry is consumed and a new one returned .
 * 
 * @param poPoly the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRCurvePolygon* OGRPolygon::CastToCurvePolygon(OGRPolygon* poPoly)
{
    OGRCurvePolygon* poCP = new OGRCurvePolygon();
    poCP->setCoordinateDimension(poPoly->getCoordinateDimension());
    poCP->assignSpatialReference(poPoly->getSpatialReference());
    poCP->oCC.nCurveCount = poPoly->oCC.nCurveCount;
    poCP->oCC.papoCurves = poPoly->oCC.papoCurves;
    poPoly->oCC.nCurveCount = 0;
    poPoly->oCC.papoCurves = NULL;

    for( int iRing = 0; iRing < poCP->oCC.nCurveCount; iRing++ )
    {
        poCP->oCC.papoCurves[iRing] = OGRLinearRing::CastToLineString(
                                    (OGRLinearRing*)poCP->oCC.papoCurves[iRing] );
    }

    delete poPoly;
    return poCP;
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

OGRSurfaceCasterToPolygon OGRPolygon::GetCasterToPolygon() const {
    return (OGRSurfaceCasterToPolygon) OGRGeometry::CastToIdentity;
}

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

OGRSurfaceCasterToCurvePolygon OGRPolygon::GetCasterToCurvePolygon() const {
    return (OGRSurfaceCasterToCurvePolygon) OGRPolygon::CastToCurvePolygon;
}
