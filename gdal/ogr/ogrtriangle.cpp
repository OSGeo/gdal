/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRTriangle geometry class.
 * Author:   Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Avyav Kumar Singh <avyavkumar at gmail dot com>
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
#include "ogr_sfcgal.h"
#include "ogr_geos.h"
#include "ogr_api.h"
#include "ogr_libs.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Constructor.
 *
 */

OGRTriangle::OGRTriangle()
{ }

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 */

OGRTriangle::OGRTriangle(const OGRTriangle& other) :
    OGRPolygon(other)
{ }

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Constructs an OGRTriangle from a valid OGRPolygon. In case of error, NULL is returned.
 *
 * @param other the Polygon we wish to construct a triangle from
 * @param eErr encapsulates an error code; contains OGRERR_NONE if the triangle is constructed successfully
 */

OGRTriangle::OGRTriangle(const OGRPolygon& other, OGRErr &eErr)
{
    // In case of Polygon, we have to check that it is a valid triangle -
    // closed and contains one external ring of four points
    // If not, then eErr will contain the error description
    if (other.getNumInteriorRings() == 0)
    {
        OGRCurve *poCurve = (OGRCurve *)other.getExteriorRingCurve();
        if (poCurve->get_IsClosed() && poCurve != NULL)
        {
            if (poCurve->getNumPoints() == 4)
            {
                // everything is fine
                eErr = this->addRing(poCurve);
                if (eErr != OGRERR_NONE)
                    CPLError( CE_Failure, CPLE_NotSupported, "Invalid Polygon");
            }
            else
            {
                eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                CPLError( CE_Failure, CPLE_NotSupported, "Invalid Polygon");
            }
        }
        else
        {
            eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
            CPLError( CE_Failure, CPLE_NotSupported, "Invalid Polygon");
        }
    }
    else
    {
        eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        CPLError( CE_Failure, CPLE_NotSupported, "Invalid Polygon");
    }
}

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Construct a triangle from points
 *
 * @param p Point 1
 * @param q Point 2
 * @param r Point 3
 */

OGRTriangle::OGRTriangle(const OGRPoint &p, const OGRPoint &q, const OGRPoint &r)
{
    OGRLinearRing *poCurve = new OGRLinearRing();
    OGRPoint *poPoint_1 = new OGRPoint(p);
    OGRPoint *poPoint_2 = new OGRPoint(q);
    OGRPoint *poPoint_3 = new OGRPoint(r);
    poCurve->addPoint(poPoint_1);
    poCurve->addPoint(poPoint_2);
    poCurve->addPoint(poPoint_3);
    poCurve->addPoint(poPoint_1);

    oCC.addCurveDirectly(poCurve, poCurve, TRUE);
    delete poPoint_1;
    delete poPoint_2;
    delete poPoint_3;
}

/************************************************************************/
/*                             ~OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Destructor
 *
 */

OGRTriangle::~OGRTriangle()
{
    if (!oCC.IsEmpty())
    {
        oCC.empty(this);
    }
}

/************************************************************************/
/*                    operator=( const OGRGeometry&)                    */
/************************************************************************/

/**
 * \brief Assignment operator
 *
 * @param other A triangle passed as a parameter
 *
 * @return OGRTriangle A copy of other
 *
 */

OGRTriangle& OGRTriangle::operator=( const OGRTriangle& other )
{
    if( this != &other)
    {
        OGRPolygon::operator=( other );
        oCC = other.oCC;
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

/**
 * \brief Returns the geometry name of the triangle
 *
 * @return "TRIANGLE"
 *
 */

const char* OGRTriangle::getGeometryName() const
{
    return "TRIANGLE";
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

/**
 * \brief Returns the WKB Type of Triangle
 *
 */

OGRwkbGeometryType OGRTriangle::getGeometryType() const
{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbTriangleZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbTriangleM;
    else if( flags & OGR_G_3D )
        return wkbTriangleZ;
    else
        return wkbTriangle;
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

/**
 * \brief Assign geometry from well known binary data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the binaries type.  This method is used
 * by the OGRGeometryFactory class, but not normally called by application
 * code.
 *
 * This method relates to the SFCOM IWks::ImportFromWKB() method.
 *
 * This method is the same as the C function OGR_G_ImportFromWkb().
 *
 * @param pabyData the binary input data.
 * @param nSize the size of pabyData in bytes, or zero if not known.
 * @param eWkbVariant if wkbVariantPostGIS1, special interpretation is done for curve geometries code
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRTriangle::importFromWkb( unsigned char *pabyData,
                                  int nSize,
                                  OGRwkbVariant eWkbVariant )
{
    OGRwkbByteOrder eByteOrder;
    int nDataOffset = 0;
    OGRErr eErr = oCC.importPreambuleFromWkb(this, pabyData, nSize, nDataOffset, eByteOrder, 4, eWkbVariant);
    if( eErr != OGRERR_NONE )
        return eErr;

    // get the individual LinearRing(s) and construct the triangle
    // an additional check is to make sure there are 4 points

    for(int iRing = 0; iRing < oCC.nCurveCount; iRing++)
    {
        OGRLinearRing* poLR = new OGRLinearRing();
        oCC.papoCurves[iRing] = poLR;
        eErr = poLR->_importFromWkb(eByteOrder, flags, pabyData + nDataOffset, nSize);
        if (eErr != OGRERR_NONE)
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return eErr;
        }

        OGRPoint *start_point = new OGRPoint();
        OGRPoint *end_point = new OGRPoint();

        poLR->getPoint(0,start_point);
        poLR->getPoint(poLR->getNumPoints()-1,end_point);

        if (poLR->getNumPoints() == 4)
        {
            // if both the start and end points are XYZ or XYZM
            if (start_point->Is3D() && end_point->Is3D())
            {
                if (start_point->getX() == end_point->getX())
                {
                    if (start_point->getY() == end_point->getY())
                    {
                        if (start_point->getZ() == end_point->getZ()) { }
                        else
                        {
                            delete oCC.papoCurves[iRing];
                            oCC.nCurveCount = iRing;
                            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                        }
                    }
                    else
                    {
                        delete oCC.papoCurves[iRing];
                        oCC.nCurveCount = iRing;
                        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                }
            }

            // if both the start and end points are XYM or XYZM
            else if (start_point->IsMeasured() && end_point->IsMeasured())
            {
                if (start_point->getX() == end_point->getX())
                {
                    if (start_point->getY() == end_point->getY())
                    {
                        if (start_point->getM() == end_point->getM()) { }
                        else
                        {
                            delete oCC.papoCurves[iRing];
                            oCC.nCurveCount = iRing;
                            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                        }
                    }
                    else
                    {
                        delete oCC.papoCurves[iRing];
                        oCC.nCurveCount = iRing;
                        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                }
            }

            // one point is XYZ or XYZM, other is XY or XYM
            // returns an error
            else if (((start_point->Is3D() & end_point->Is3D()) == 0) &&
                     ((start_point->Is3D() | end_point->Is3D()) == 1))
            {
                delete oCC.papoCurves[iRing];
                oCC.nCurveCount = iRing;
                return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
            }

            // one point is XYM or XYZM, other is XYZ or XY
            // returns an error
            else if (((start_point->IsMeasured() & end_point->IsMeasured()) == 0) &&
                     ((start_point->IsMeasured() | end_point->IsMeasured()) == 1))
            {
                delete oCC.papoCurves[iRing];
                oCC.nCurveCount = iRing;
                return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
            }

            // both points are XY
            else
            {
                if (start_point->getX() == end_point->getX())
                {
                    if (start_point->getY() == end_point->getY()) { }
                    else
                    {
                        delete oCC.papoCurves[iRing];
                        oCC.nCurveCount = iRing;
                        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
                }
            }
        }

        // there should be exactly four points
        // if there are not four points, then this falls under OGRPolygon and not OGRTriangle
        else
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        if (nSize != -1)
            nSize -= poLR->_WkbSize( flags );

        nDataOffset += poLR->_WkbSize( flags );
    }

    // rings must be 1 at all times
    if (oCC.nCurveCount != 1 )
        return OGRERR_CORRUPT_DATA;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/

/**
 * \brief Convert a geometry into well known binary format.
 *
 * This method relates to the SFCOM IWks::ExportToWKB() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkb() or OGR_G_ExportToIsoWkb(),
 * depending on the value of eWkbVariant.
 *
 * @param eByteOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyData a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGRGeometry::WkbSize() byte in size.
 * @param eWkbVariant What standard to use when exporting geometries with
 *                      three dimensions (or more). The default wkbVariantOldOgc is
 *                      the historical OGR variant. wkbVariantIso is the
 *                      variant defined in ISO SQL/MM and adopted by OGC
 *                      for SFSQL 1.2.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

OGRErr  OGRTriangle::exportToWkb( OGRwkbByteOrder eByteOrder,
                                 unsigned char * pabyData,
                                 OGRwkbVariant eWkbVariant ) const

{

    // Set the byte order according to machine (Big/Little Endian)
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

    // set the geometry type
    // returns wkbTriangle, wkbTriangleZ or wkbTriangleZM; getGeometryType() built within Triangle API
    GUInt32 nGType = getGeometryType();

    // check the variations of WKB formats
    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        // No need to modify wkbFlatten() as it is optimised for Triangle and other geometries
        nGType = wkbFlatten(nGType);
        if(Is3D())
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse);
        if(IsMeasured())
            nGType = (OGRwkbGeometryType)(nGType | 0x40000000);
    }

    else if ( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();

    // set the byte order
    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32(nGType);
    else
        nGType = CPL_MSBWORD32(nGType);

    memcpy( pabyData + 1, &nGType, 4 );

    // Copy in the count of the rings after setting the correct byte order
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;
        nCount = CPL_SWAP32( oCC.nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
        memcpy( pabyData+5, &oCC.nCurveCount, 4 );

    // cast every geometry into a LinearRing and attach it to the pabyData
    int nOffset = 9;

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        poLR->_exportToWkb( eByteOrder, flags, pabyData + nOffset );
        nOffset += poLR->_WkbSize(flags);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

/**
 * \brief Assign geometry from well known text data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the text type.  This method is used
 * by the OGRGeometryFactory class, but not normally called by application
 * code.
 *
 * This method relates to the SFCOM IWks::ImportFromWKT() method.
 *
 * This method is the same as the C function OGR_G_ImportFromWkt().
 *
 * @param ppszInput pointer to a pointer to the source text.  The pointer is
 *                    updated to pointer after the consumed text.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRTriangle::importFromWkt( char ** ppszInput )

{
    int bHasZ = FALSE, bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ )
        flags |= OGR_G_3D;
    if( bHasM )
        flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    OGRRawPoint *paoPoints = NULL;
    int          nMaxPoints = 0;
    double      *padfZ = NULL;

    eErr = importFromWKTListOnly(ppszInput, bHasZ, bHasM, paoPoints, nMaxPoints, padfZ);

    if (!oCC.papoCurves[0]->get_IsClosed())
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    CPLFree(paoPoints);
    CPLFree(padfZ);

    return eErr;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * \brief Convert a geometry into well known text format.
 *
 * This method relates to the SFCOM IWks::ExportToWKT() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkt().
 *
 * @param ppszDstText a text buffer is allocated by the program, and assigned
 *                    to the passed pointer. After use, *ppszDstText should be
 *                    freed with OGRFree().
 * @param eWkbVariant the specification that must be conformed too :
 *                    - wbkVariantOgc for old-style 99-402 extended dimension (Z) WKB types
 *                    - wbkVariantIso for SFSQL 1.2 and ISO SQL/MM Part 3
 *
 * @return Currently OGRERR_NONE is always returned.
 */

OGRErr OGRTriangle::exportToWkt( char ** ppszDstText,
                                OGRwkbVariant eWkbVariant ) const

{
    OGRErr      eErr;
    bool        bMustWriteComma = false;

    // If there is no LinearRing, then the Triangle is empty
    if (getExteriorRing() == NULL || getExteriorRing()->IsEmpty() )
    {
        if( eWkbVariant == wkbVariantIso )
        {
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                *ppszDstText = CPLStrdup("TRIANGLE ZM EMPTY");
            else if( flags & OGR_G_MEASURED )
                *ppszDstText = CPLStrdup("TRIANGLE M EMPTY");
            else if( flags & OGR_G_3D )
                *ppszDstText = CPLStrdup("TRIANGLE Z EMPTY");
            else
                *ppszDstText = CPLStrdup("TRIANGLE EMPTY");
        }
        else
            *ppszDstText = CPLStrdup("TRIANGLE EMPTY");
        return OGRERR_NONE;
    }

    // Build a list of strings containing the stuff for the ring.
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

    // allocate space for the ring
    *ppszDstText = (char *) VSI_MALLOC_VERBOSE(nCumulativeLength + nNonEmptyRings + 16);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

    // construct the string
    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            strcpy( *ppszDstText, "TRIANGLE ZM (" );
        else if( flags & OGR_G_MEASURED )
            strcpy( *ppszDstText, "TRIANGLE M (" );
        else if( flags & OGR_G_3D )
            strcpy( *ppszDstText, "TRIANGLE Z (" );
        else
            strcpy( *ppszDstText, "TRIANGLE (" );
    }
    else
        strcpy( *ppszDstText, "TRIANGLE (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        if( papszRings[iRing] == NULL )
        {
            CPLDebug( "OGR", "OGRTriangle::exportToWkt() - skipping empty ring.");
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
/*                              WkbSize()                               */
/************************************************************************/

/**
 * \brief Returns size of related binary representation.
 *
 * This method returns the exact number of bytes required to hold the
 * well known binary representation of this geometry object.
 *
 * This method relates to the SFCOM IWks::WkbSize() method.
 *
 * This method is the same as the C function OGR_G_WkbSize().
 *
 * @return size of binary representation in bytes.
 */

int OGRTriangle::WkbSize() const
{
    return 9+((OGRLinearRing*)oCC.papoCurves[0])->_WkbSize(flags);
}

/************************************************************************/
/*                             Distance3D()                             */
/************************************************************************/

/**
 * \brief Returns the 3D distance between
 *
 * The distance is expressed into the same unit as the coordinates of the geometries.
 *
 * This method is built on the SFCGAL library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the SFCGAL library, this method will always return
 * -1.0
 *
 * @return distance between the two geometries
 */

double OGRTriangle::Distance3D(UNUSED_IF_NO_SFCGAL const OGRGeometry *poOtherGeom) const
{
    if (poOtherGeom == NULL)
    {
        CPLDebug( "OGR", "OGRTriangle::Distance3D called with NULL geometry pointer" );
        return -1.0;
    }

    if (!(poOtherGeom->Is3D() && this->Is3D()))
    {
        CPLDebug( "OGR", "OGRGeometry::Distance3D called with two dimensional geometry(geometries)" );
        return -1.0;
    }

#ifndef HAVE_SFCGAL

    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return -1.0;

#else

    sfcgal_init();
    sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
    if (poThis == NULL)
        return -1.0;

    sfcgal_geometry_t *poOther = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
    if (poOther == NULL)
        return -1.0;

    double _distance = sfcgal_geometry_distance_3d(poThis, poOther);

    sfcgal_geometry_delete(poThis);
    sfcgal_geometry_delete(poOther);

    return (_distance > 0)? _distance: -1.0;

#endif
}

/************************************************************************/
/*                               addRing()                              */
/************************************************************************/

/**
 * \brief adds an exterior ring to the Triangle
 *
 * Checks if it is a valid ring (same start and end point; number of points should be four).
 *
 * If there is already a ring, then it doesn't add the new ring. The old ring must be deleted first.
 *
 * @return OGRErr The error code retuned. If the addition is successful, then OGRERR_NONE is returned.
 */

OGRErr OGRTriangle::addRing(OGRCurve *poNewRing)
{
    OGRCurve* poNewRingCloned = (OGRCurve* )poNewRing->clone();
    if( poNewRingCloned == NULL )
        return OGRERR_FAILURE;

    // check if the number of rings existing is 0
    if (oCC.nCurveCount > 0)
    {
        CPLDebug( "OGR", "OGRTriangle already contains a ring");
        return OGRERR_FAILURE;
    }

    // check if the ring to be added is valid
    if (!poNewRingCloned->get_IsClosed() || poNewRingCloned->getNumPoints() != 4)
    {
        // condition fails; cannot add this ring as it is not valid
        CPLDebug( "OGR", "Not a valid ring to add to a Triangle");
        return OGRERR_FAILURE;
    }

    OGRErr eErr = addRingDirectly(poNewRingCloned);

    if( eErr != OGRERR_NONE )
        delete poNewRingCloned;

    return eErr;
}

/************************************************************************/
/*                             SymDifference()                          */
/************************************************************************/

/**
 * \brief Generates a new geometry which is the symmetric difference of this geometry and the second geometry passed into the method.
 *
 * If there is already a ring, then it doesn't add the new ring. The old ring must be deleted first.
 *
 * @param poOtherGeom the other geometry to compute the symmetric difference against
 *
 * @return OGRGeometry* The computed geometry
 */

OGRGeometry *OGRTriangle::SymDifference( const OGRGeometry *poOtherGeom) const
{
    OGRGeometry* poGeom = this->Difference(poOtherGeom);
    if (poGeom == NULL)
        return NULL;

    OGRGeometry* poOther = poOtherGeom->Difference(this);
    if (poOther == NULL)
        return NULL;

    return this->Union(poOther);
}

/************************************************************************/
/*                              IsSimple()                              */
/************************************************************************/

/**
 * \brief Checks if it is a simple geometry
 *
 * The only self intersection points are the boundary points.
 *
 * Hence it is a simple geometry.
 *
 * @return TRUE
 */

OGRBoolean  OGRTriangle::IsSimple() const
{
    return TRUE;
}

/************************************************************************/
/*                             Boundary()                               */
/************************************************************************/

/**
 * \brief Returns the boundary of the geometry
 *
 * @return OGRGeometry* pointer to the boundary geometry
 */

OGRGeometry *OGRTriangle::Boundary() const
{
    return oCC.papoCurves[0];
}

/************************************************************************/
/*                             CastToPolygon()                          */
/************************************************************************/

/**
 * \brief Casts the OGRTriangle to an OGRPolygon
 *
 * @return OGRPolygon* pointer to the computed OGRPolygon
 */

OGRPolygon* OGRTriangle::CastToPolygon()
{
    OGRPolygon *poPolygon = new OGRPolygon();
    poPolygon->addRing((OGRCurve *)oCC.papoCurves[0]);
    poPolygon->assignSpatialReference(getSpatialReference());
    return poPolygon;
}
