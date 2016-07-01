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
/*                     OGRPolygon( const OGRPolygon& )                  */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRPolygon::OGRPolygon( const OGRPolygon& other ) :
    OGRCurvePolygon(other)
{
}

/************************************************************************/
/*                            ~OGRPolygon()                             */
/************************************************************************/

OGRPolygon::~OGRPolygon()

{
}

/************************************************************************/
/*                     operator=( const OGRPolygon&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRPolygon& OGRPolygon::operator=( const OGRPolygon& other )
{
    if( this != &other)
    {
        OGRCurvePolygon::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolygon::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbPolygonZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbPolygonM;
    else if( flags & OGR_G_3D )
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
 * @param iRing internal ring index from 0 to getNumInteriorRings() - 1.
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
 * @param iRing internal ring index from 0 to getNumInteriorRings() - 1.
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

    for( int i = 0; i < oCC.nCurveCount; i++ )
    {
        nSize += ((OGRLinearRing*)oCC.papoCurves[i])->_WkbSize( flags );
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
    /* coverity[tainted_data] */
    OGRErr eErr = oCC.importPreambuleFromWkb(this, pabyData, nSize, nDataOffset,
                                             eByteOrder, 4, eWkbVariant);
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Get the rings.                                                  */
/* -------------------------------------------------------------------- */
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = new OGRLinearRing();
        oCC.papoCurves[iRing] = poLR;
        eErr = poLR->_importFromWkb( eByteOrder, flags,
                                                 pabyData + nDataOffset,
                                                 nSize );
        if( eErr != OGRERR_NONE )
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return eErr;
        }

        if( nSize != -1 )
            nSize -= poLR->_WkbSize( flags );

        nDataOffset += poLR->_WkbSize( flags );
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

/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();

    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        nGType = wkbFlatten(nGType);
        if( Is3D() )
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse); /* yes we explicitly set wkb25DBit */
        if( IsMeasured() )
            nGType = (OGRwkbGeometryType)(nGType | 0x40000000);
    }
    else if ( eWkbVariant == wkbVariantIso )
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

/* ==================================================================== */
/*      Serialize each of the rings.                                    */
/* ==================================================================== */
    int nOffset = 9;

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        poLR->_exportToWkb( eByteOrder, flags,
                                        pabyData + nOffset );

        nOffset += poLR->_WkbSize( flags );
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
    bool bIsEmpty = false;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

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
    double      *padfM = NULL;

    do
    {

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
        int nPoints = 0;
        int flagsFromInput = flags;
        if( flagsFromInput == 0 ) /* flags was not set, this is not called by us */
        {
            if( bHasM )
                flagsFromInput |= OGR_G_MEASURED;
            if( bHasZ )
                flagsFromInput |= OGR_G_3D;
        }

        pszInput = OGRWktReadPointsM( pszInput, &paoPoints, &padfZ, &padfM, &flagsFromInput,
                                      &nMaxPoints, &nPoints );
        if( pszInput == NULL || nPoints == 0 )
        {
            return OGRERR_CORRUPT_DATA;
        }
        if( (flagsFromInput & OGR_G_3D) && !(flags & OGR_G_3D) )
        {
            flags |= OGR_G_3D;
            bHasZ = TRUE;
        }
        if( (flagsFromInput & OGR_G_MEASURED) && !(flags & OGR_G_MEASURED) )
        {
            flags |= OGR_G_MEASURED;
            bHasM = TRUE;
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

        if (bHasM && bHasZ)
            poLR->setPoints( nPoints, paoPoints, padfZ, padfM );
        else if (bHasM)
            poLR->setPointsM( nPoints, paoPoints, padfM );
        else if (bHasZ)
            poLR->setPoints( nPoints, paoPoints, padfZ );
        else
            poLR->setPoints( nPoints, paoPoints, padfZ );

        oCC.nCurveCount++;

/* -------------------------------------------------------------------- */
/*      Read the delimiter following the ring.                          */
/* -------------------------------------------------------------------- */

        pszInput = OGRWktReadToken( pszInput, szToken );
    } while( szToken[0] == ',' );

    CPLFree( padfM );

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
/*      equivalent.  This could be made a lot more CPU efficient!        */
/************************************************************************/

OGRErr OGRPolygon::exportToWkt( char ** ppszDstText,
                                OGRwkbVariant eWkbVariant ) const

{
    OGRErr      eErr;
    bool        bMustWriteComma = false;

/* -------------------------------------------------------------------- */
/*      If we have no valid exterior ring, return POLYGON EMPTY.        */
/* -------------------------------------------------------------------- */
    if (getExteriorRing() == NULL ||
        getExteriorRing()->IsEmpty() )
    {
        if( eWkbVariant == wkbVariantIso )
        {
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                *ppszDstText = CPLStrdup("POLYGON ZM EMPTY");
            else if( flags & OGR_G_MEASURED )
                *ppszDstText = CPLStrdup("POLYGON M EMPTY");
            else if( flags & OGR_G_3D )
                *ppszDstText = CPLStrdup("POLYGON Z EMPTY");
            else
                *ppszDstText = CPLStrdup("POLYGON EMPTY");
        }
        else
            *ppszDstText = CPLStrdup("POLYGON EMPTY");
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */
    char **papszRings = (char **) CPLCalloc(sizeof(char *),oCC.nCurveCount);
    size_t nCumulativeLength = 0;
    size_t nNonEmptyRings = 0;
    size_t *pnRingBeginning = (size_t *) CPLCalloc(sizeof(size_t),oCC.nCurveCount);

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        //poLR->setFlags( getFlags() );
        poLR->set3D(Is3D());
        poLR->setMeasured(IsMeasured());
        if( poLR->getNumPoints() == 0 )
        {
            papszRings[iRing] = NULL;
            continue;
        }

        eErr = poLR->exportToWkt( &(papszRings[iRing]), eWkbVariant );
        if( eErr != OGRERR_NONE )
            goto error;

        if( STARTS_WITH_CI(papszRings[iRing], "LINEARRING ZM (") )
            pnRingBeginning[iRing] = 14;
        else if( STARTS_WITH_CI(papszRings[iRing], "LINEARRING M (") )
            pnRingBeginning[iRing] = 13;
        else if( STARTS_WITH_CI(papszRings[iRing], "LINEARRING Z (") )
            pnRingBeginning[iRing] = 13;
        else if( STARTS_WITH_CI(papszRings[iRing], "LINEARRING (") )
            pnRingBeginning[iRing] = 11;
        else
        {
            CPLAssert( 0 );
        }

        nCumulativeLength += strlen(papszRings[iRing] + pnRingBeginning[iRing]);

        nNonEmptyRings++;
    }

/* -------------------------------------------------------------------- */
/*      Allocate exactly the right amount of space for the              */
/*      aggregated string.                                              */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSI_MALLOC_VERBOSE(nCumulativeLength + nNonEmptyRings + 16);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            strcpy( *ppszDstText, "POLYGON ZM (" );
        else if( flags & OGR_G_MEASURED )
            strcpy( *ppszDstText, "POLYGON M (" );
        else if( flags & OGR_G_3D )
            strcpy( *ppszDstText, "POLYGON Z (" );
        else
            strcpy( *ppszDstText, "POLYGON (" );
    }
    else
        strcpy( *ppszDstText, "POLYGON (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        if( papszRings[iRing] == NULL )
        {
            CPLDebug( "OGR", "OGRPolygon::exportToWkt() - skipping empty ring.");
            continue;
        }

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = true;

        size_t nRingLen = strlen(papszRings[iRing] + pnRingBeginning[iRing]);
        memcpy( *ppszDstText + nCumulativeLength, papszRings[iRing] + pnRingBeginning[iRing], nRingLen );
        nCumulativeLength += nRingLen;
        VSIFree( papszRings[iRing] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszRings );
    CPLFree( pnRingBeginning );

    return OGRERR_NONE;

error:
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
        CPLFree(papszRings[iRing]);
    CPLFree(papszRings);
    return eErr;
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

OGRErr OGRPolygon::PointOnSurface( OGRPoint *poPoint ) const

{
    return PointOnSurfaceInternal(poPoint);
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
    bool bHasCurveGeometry = false;
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRCurve* poSubGeom = (OGRCurve* )oCC.papoCurves[iRing]->getCurveGeometry(papszOptions);
        if( wkbFlatten(poSubGeom->getGeometryType()) != wkbLineString )
            bHasCurveGeometry = true;
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
    poCP->set3D(poPoly->Is3D());
    poCP->setMeasured(poPoly->IsMeasured());
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
