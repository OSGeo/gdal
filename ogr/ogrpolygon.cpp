/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolygon geometry class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstring>
#include <cstddef>
#include <new>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geos.h"
#include "ogr_sfcgal.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             OGRPolygon()                             */
/************************************************************************/

/**
 * \brief Create an empty polygon.
 */

OGRPolygon::OGRPolygon() = default;

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

OGRPolygon::OGRPolygon( const OGRPolygon& ) = default;

/************************************************************************/
/*                            ~OGRPolygon()                             */
/************************************************************************/

OGRPolygon::~OGRPolygon() = default;

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
/*                               clone()                                */
/************************************************************************/

OGRPolygon *OGRPolygon::clone() const

{
    return new (std::nothrow) OGRPolygon(*this);
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolygon::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbPolygonZM;
    else if( flags & OGR_G_MEASURED )
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
        return oCC.papoCurves[0]->toLinearRing();

    return nullptr;
}

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

const OGRLinearRing *OGRPolygon::getExteriorRing() const

{
    if( oCC.nCurveCount > 0 )
        return oCC.papoCurves[0]->toLinearRing();

    return nullptr;
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
    return stealExteriorRingCurve()->toLinearRing();
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
        return nullptr;

    return oCC.papoCurves[iRing+1]->toLinearRing();
}

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

const OGRLinearRing *OGRPolygon::getInteriorRing( int iRing ) const

{
    if( iRing < 0 || iRing >= oCC.nCurveCount-1 )
        return nullptr;

    return oCC.papoCurves[iRing+1]->toLinearRing();
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

OGRLinearRing *OGRPolygon::stealInteriorRing( int iRing )
{
    if( iRing < 0 || iRing >= oCC.nCurveCount-1 )
        return nullptr;
    OGRLinearRing *poRet = oCC.papoCurves[iRing+1]->toLinearRing();
    oCC.papoCurves[iRing+1] = nullptr;
    return poRet;
}

/*! @cond Doxygen_Suppress */
/************************************************************************/
/*                            checkRing()                               */
/************************************************************************/

int OGRPolygon::checkRing( OGRCurve * poNewRing ) const
{
    if( poNewRing == nullptr ||
        !(EQUAL(poNewRing->getGeometryName(), "LINEARRING")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong curve type. Expected LINEARRING.");
        return FALSE;
    }

    if( !poNewRing->IsEmpty() && !poNewRing->get_IsClosed() )
    {
        // This configuration option name must be the same as in OGRCurvePolygon::checkRing()
        const char* pszEnvVar = CPLGetConfigOption("OGR_GEOMETRY_ACCEPT_UNCLOSED_RING", nullptr);
        if( pszEnvVar != nullptr && !CPLTestBool(pszEnvVar) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Non closed ring detected.");
            return FALSE;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Non closed ring detected.%s",
                     pszEnvVar == nullptr ?
                         " To avoid accepting it, set the "
                         "OGR_GEOMETRY_ACCEPT_UNCLOSED_RING configuration "
                         "option to NO" : "");
        }
    }

    return TRUE;
}
/*! @endcond */

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

size_t OGRPolygon::WkbSize() const

{
    size_t nSize = 9;

    for( auto&& poRing: *this )
    {
        nSize += poRing->_WkbSize( flags );
    }

    return nSize;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPolygon::importFromWkb( const unsigned char * pabyData,
                                  size_t nSize,
                                  OGRwkbVariant eWkbVariant,
                                  size_t& nBytesConsumedOut )

{
    nBytesConsumedOut = 0;
    OGRwkbByteOrder eByteOrder = wkbNDR;
    size_t nDataOffset = 0;
    // coverity[tainted_data]
    OGRErr eErr = oCC.importPreambleFromWkb(this, pabyData, nSize, nDataOffset,
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
        size_t nBytesConsumedRing = 0;
        eErr = poLR->_importFromWkb( eByteOrder, flags,
                                     pabyData + nDataOffset,
                                     nSize,
                                     nBytesConsumedRing );
        if( eErr != OGRERR_NONE )
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return eErr;
        }

        CPLAssert( nBytesConsumedRing > 0 );
        if( nSize != static_cast<size_t>(-1) )
        {
            CPLAssert( nSize >= nBytesConsumedRing );
            nSize -= nBytesConsumedRing;
        }

        nDataOffset += nBytesConsumedRing;
    }
    nBytesConsumedOut = nDataOffset;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr OGRPolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                unsigned char * pabyData,
                                OGRwkbVariant eWkbVariant ) const

{

/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] =
        DB2_V72_UNFIX_BYTE_ORDER(static_cast<unsigned char>(eByteOrder));

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = getGeometryType();

    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        nGType = wkbFlatten(nGType);
        if( Is3D() )
            // Explicitly set wkb25DBit.
            nGType =
                static_cast<OGRwkbGeometryType>(nGType | wkb25DBitInternalUse);
        if( IsMeasured() )
            nGType = static_cast<OGRwkbGeometryType>(nGType | 0x40000000);
    }
    else if( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();

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
        const int nCount = CPL_SWAP32( oCC.nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &oCC.nCurveCount, 4 );
    }

/* ==================================================================== */
/*      Serialize each of the rings.                                    */
/* ==================================================================== */
    size_t nOffset = 9;

    for( auto&& poRing: *this )
    {
        poRing->_exportToWkb( eByteOrder, flags,
                            pabyData + nOffset );

        nOffset += poRing->_WkbSize( flags );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `POLYGON ((x y, x y, ...),(x y, ...),...)'.                     */
/************************************************************************/

OGRErr OGRPolygon::importFromWkt( const char ** ppszInput )

{
    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr eErr = importPreambleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    OGRRawPoint *paoPoints = nullptr;
    int nMaxPoints = 0;
    double *padfZ = nullptr;

    eErr = importFromWKTListOnly( ppszInput, bHasZ, bHasM,
                                  paoPoints, nMaxPoints, padfZ );

    CPLFree( paoPoints );
    CPLFree( padfZ );

    return eErr;
}

/*! @cond Doxygen_Suppress */
/************************************************************************/
/*                        importFromWKTListOnly()                       */
/*                                                                      */
/*      Instantiate from "((x y, x y, ...),(x y, ...),...)"             */
/************************************************************************/

OGRErr OGRPolygon::importFromWKTListOnly( const char ** ppszInput,
                                          int bHasZ, int bHasM,
                                          OGRRawPoint*& paoPoints,
                                          int& nMaxPoints,
                                          double*& padfZ )

{
    char szToken[OGR_WKT_TOKEN_MAX] = {};
    const char *pszInput = *ppszInput;

    // Skip first '('.
    pszInput = OGRWktReadToken( pszInput, szToken );
    if( EQUAL(szToken, "EMPTY") )
    {
        *ppszInput = pszInput;
        return OGRERR_NONE;
    }
    if( !EQUAL(szToken, "(") )
        return OGRERR_CORRUPT_DATA;

/* ==================================================================== */
/*      Read each ring in turn.  Note that we try to reuse the same     */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    int nMaxRings = 0;
    double *padfM = nullptr;

    do
    {
        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if( EQUAL(szToken, "EMPTY") )
        {
/* -------------------------------------------------------------------- */
/*      Do we need to grow the ring array?                              */
/* -------------------------------------------------------------------- */
            if( oCC.nCurveCount == nMaxRings )
            {
                nMaxRings = nMaxRings * 2 + 1;
                oCC.papoCurves = static_cast<OGRCurve **>(
                    CPLRealloc(oCC.papoCurves,
                               nMaxRings * sizeof(OGRLinearRing*)));
            }
            oCC.papoCurves[oCC.nCurveCount] = new OGRLinearRing();
            oCC.nCurveCount++;

            pszInput = OGRWktReadToken( pszNext, szToken );
            if( !EQUAL(szToken, ",") )
                break;

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Read points for one ring from input.                            */
/* -------------------------------------------------------------------- */
        int nPoints = 0;
        int flagsFromInput = flags;
        if( flagsFromInput == 0 )
        {
            // Flags was not set, this is not called by us.
            if( bHasM )
                flagsFromInput |= OGR_G_MEASURED;
            if( bHasZ )
                flagsFromInput |= OGR_G_3D;
        }

        pszInput = OGRWktReadPointsM( pszInput, &paoPoints, &padfZ, &padfM,
                                      &flagsFromInput,
                                      &nMaxPoints, &nPoints );
        if( pszInput == nullptr || nPoints == 0 )
        {
            CPLFree(padfM);
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
            oCC.papoCurves = static_cast<OGRCurve **>(
                CPLRealloc(oCC.papoCurves, nMaxRings * sizeof(OGRLinearRing*)));
        }

/* -------------------------------------------------------------------- */
/*      Create the new ring, and assign to ring list.                   */
/* -------------------------------------------------------------------- */
        OGRLinearRing* poLR = new OGRLinearRing();
        oCC.papoCurves[oCC.nCurveCount] = poLR;

        if( bHasM && bHasZ )
            poLR->setPoints(nPoints, paoPoints, padfZ, padfM);
        else if( bHasM )
            poLR->setPointsM(nPoints, paoPoints, padfM);
        else if( bHasZ )
            poLR->setPoints(nPoints, paoPoints, padfZ);
        else
            poLR->setPoints(nPoints, paoPoints);

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

    *ppszInput = pszInput;
    return OGRERR_NONE;
}
/*! @endcond */

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into its well known text format        */
/*      equivalent.  This could be made a lot more CPU efficient.       */
/************************************************************************/

std::string OGRPolygon::exportToWkt(const OGRWktOptions& opts,
                                    OGRErr *err) const
{
    std::string wkt;
/* -------------------------------------------------------------------- */
/*      If we have no valid exterior ring, return POLYGON EMPTY.        */
/* -------------------------------------------------------------------- */
    wkt = getGeometryName();
    wkt += wktTypeString(opts.variant);
    if( getExteriorRing() == nullptr || getExteriorRing()->IsEmpty() )
        wkt += "EMPTY";

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each ring.     */
/* -------------------------------------------------------------------- */

    else
    {
        try
        {
            bool first(true);
            wkt += '(';
            for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
            {
                OGRLinearRing* poLR = oCC.papoCurves[iRing]->toLinearRing();
                if( poLR->getNumPoints() )
                {
                    if (!first)
                        wkt += ',';
                    first = false;
                    OGRErr subgeomErr = OGRERR_NONE;
                    std::string tempWkt = poLR->exportToWkt(opts, &subgeomErr);
                    if ( subgeomErr != OGRERR_NONE )
                    {
                        if( err )
                            *err = subgeomErr;
                        return std::string();
                    }

                    // Remove leading "LINEARRING..."
                    wkt += tempWkt.substr(tempWkt.find_first_of('('));
                }
            }
            wkt += ')';
        }
        catch( const std::bad_alloc& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            if (err)
                *err = OGRERR_FAILURE;
            return std::string();
        }
    }

    if (err)
        *err = OGRERR_NONE;
    return wkt;
}

/************************************************************************/
/*                           IsPointOnSurface()                           */
/************************************************************************/

/** Return whether the point is on the surface.
 * @return TRUE or FALSE
 */
OGRBoolean OGRPolygon::IsPointOnSurface( const OGRPoint * pt ) const
{
    if( nullptr == pt)
        return FALSE;

    bool bOnSurface = false;
    // The point must be in the outer ring, but not in the inner rings
    int iRing = 0;
    for( auto&& poRing: *this )
    {
        if( poRing->isPointInRing(pt) )
        {
            if( iRing == 0 )
            {
                bOnSurface = true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if( iRing == 0 )
            {
                return false;
            }
        }
        iRing ++;
    }

    return bOnSurface;
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

void OGRPolygon::closeRings()

{
    for( auto&& poRing: *this )
        poRing->closeRings();
}

/************************************************************************/
/*                           CurvePolyToPoly()                          */
/************************************************************************/

OGRPolygon* OGRPolygon::CurvePolyToPoly(
    CPL_UNUSED double dfMaxAngleStepSizeDegrees,
    CPL_UNUSED const char* const* papszOptions ) const
{
    return clone();
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

OGRGeometry* OGRPolygon::getLinearGeometry(
    double dfMaxAngleStepSizeDegrees,
    const char* const* papszOptions ) const
{
    return
        OGRGeometry::getLinearGeometry(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                             getCurveGeometry()                       */
/************************************************************************/

OGRGeometry *
OGRPolygon::getCurveGeometry( const char* const* papszOptions ) const
{
    OGRCurvePolygon* poCC = new OGRCurvePolygon();
    poCC->assignSpatialReference( getSpatialReference() );
    bool bHasCurveGeometry = false;
    for( auto&& poRing: *this )
    {
        auto poSubGeom =
            poRing->getCurveGeometry(papszOptions);
        if( wkbFlatten(poSubGeom->getGeometryType()) != wkbLineString )
            bHasCurveGeometry = true;
        poCC->addRingDirectly( poSubGeom->toCurve() );
    }
    if( !bHasCurveGeometry )
    {
        delete poCC;
        return clone();
    }
    return poCC;
}

/*! @cond Doxygen_Suppress */
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

OGRCurvePolygon* OGRPolygon::CastToCurvePolygon( OGRPolygon* poPoly )
{
    OGRCurvePolygon* poCP = new OGRCurvePolygon();
    poCP->set3D(poPoly->Is3D());
    poCP->setMeasured(poPoly->IsMeasured());
    poCP->assignSpatialReference(poPoly->getSpatialReference());
    poCP->oCC.nCurveCount = poPoly->oCC.nCurveCount;
    poCP->oCC.papoCurves = poPoly->oCC.papoCurves;
    poPoly->oCC.nCurveCount = 0;
    poPoly->oCC.papoCurves = nullptr;

    for( auto&& poRing: *poCP )
    {
        poRing = OGRLinearRing::CastToLineString(poRing->toLinearRing());
    }

    delete poPoly;
    return poCP;
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

static OGRPolygon* CasterToPolygon(OGRSurface* poSurface)
{
    return poSurface->toPolygon();
}

OGRSurfaceCasterToPolygon OGRPolygon::GetCasterToPolygon() const
{
    return ::CasterToPolygon;
}

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

OGRCurvePolygon* OGRPolygon::CasterToCurvePolygon(OGRSurface* poSurface)
{
    return OGRPolygon::CastToCurvePolygon(poSurface->toPolygon());
}

OGRSurfaceCasterToCurvePolygon OGRPolygon::GetCasterToCurvePolygon() const
{
    return OGRPolygon::CasterToCurvePolygon;
}
/*! @endcond */
