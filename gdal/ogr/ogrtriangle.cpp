#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_sfcgal.h"
#include "ogr_geos.h"
#include "ogr_api.h"

#ifndef HAVE_GEOS
#define UNUSED_IF_NO_GEOS CPL_UNUSED
#else
#define UNUSED_IF_NO_GEOS
#endif

#ifndef HAVE_SFCGAL
#define UNUSED_IF_NO_SFCGAL CPL_UNUSED
#else
#define UNUSED_IF_NO_SFCGAL
#endif

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

OGRTriangle::OGRTriangle()
{ }

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

OGRTriangle::OGRTriangle(const OGRTriangle& other) :
    OGRPolygon(other)
{ }

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

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

OGRTriangle::~OGRTriangle()
{
    if (!oCC.IsEmpty())
    {
        oCC.empty(this);
    }
}

/************************************************************************/
/*                    operator=( const OGRGeometry&)                    */
/*                         Assignment operator                          */
/************************************************************************/

OGRTriangle& OGRTriangle::operator=( const OGRTriangle& other )
{
    if( this != &other)
    {
        OGRPolygon::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char* OGRTriangle::getGeometryName() const
{
    return "TRIANGLE";
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

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
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

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
/*      Build a well known binary representation of this object.        */
/************************************************************************/

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
/*      Instantiate from well known text format. Currently this is      */
/*      of the form 'TRIANGLE ((x y, x y, x y, x y))' or other          */
/*      varieties of the same (including Z and/or M)                    */
/************************************************************************/

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
/*            Translate this structure into it's WKT format             */
/************************************************************************/

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
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRTriangle::WkbSize() const
{
    return 9+((OGRLinearRing*)oCC.papoCurves[0])->_WkbSize(flags);
}

/************************************************************************/
/*                             Distance3D()                             */
/*       Returns the 3D distance between the two geometries. The        */
/*    distance is expressed into the same unit as the coordinates of    */
/*    the geometries.                                                   */
/************************************************************************/

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
/*    Checks if it is a valid ring (same start and end point; number    */
/*    of points should be four). If there is already a ring, then it    */
/*    doesn't add the new ring. The old ring must be deleted first.     */
/************************************************************************/

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
    OGRPoint *poStart = new OGRPoint();
    OGRPoint *poEnd = new OGRPoint();

    poNewRingCloned->StartPoint(poStart);
    poNewRingCloned->EndPoint(poEnd);

    if (!poNewRingCloned->get_IsClosed() || poNewRingCloned->getNumPoints() != 4)
    {
        // condition fails; cannot add this ring as it is not valid
        CPLDebug( "OGR", "Not a valid ring to add to a Triangle");
        return OGRERR_FAILURE;
    }

    OGRErr eErr = addRingDirectly(poNewRingCloned);

    if( eErr != OGRERR_NONE )
        delete poNewRingCloned;

    // free poStart and poEnd as we don't need them
    delete poStart;
    delete poEnd;

    return eErr;
}

/************************************************************************/
/*                          PointOnSurface()                            */
/************************************************************************/

OGRErr OGRTriangle::PointOnSurface(OGRPoint * poPoint ) const
{
    // cast the triangle as a polygon and use the GEOS method on it
    OGRPolygon poPolygon(*((OGRPolygon *)this));
    return poPolygon.PointOnSurface(poPoint);
}

/************************************************************************/
/*                             SymDifference()                          */
/*  Generates a new geometry which is the symmetric difference of this  */
/*  geometry and the second geometry passed into the method.            */
/************************************************************************/

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
/*      The only self intersection points are the boundary points.      */
/*      Hence it is a simple geometry.                                  */
/************************************************************************/

OGRBoolean  OGRTriangle::IsSimple() const
{
    return TRUE;
}

/************************************************************************/
/*                               IsRing()                               */
/************************************************************************/

OGRBoolean  OGRTriangle::IsRing() const
{
    return TRUE;
}

/************************************************************************/
/*                             Boundary()                               */
/************************************************************************/

OGRGeometry *OGRTriangle::Boundary() const
{
    return oCC.papoCurves[0];
}
