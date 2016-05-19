/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

OGRTriangle::OGRTriangle()
{
    oCC.nCurveCount = 1;        // only 1 linear ring at all times for a triangle
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
                            return eErr;
                        }
                    }
                    else
                    {
                        delete oCC.papoCurves[iRing];
                        oCC.nCurveCount = iRing;
                        return eErr;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return eErr;
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
                            return eErr;
                        }
                    }
                    else
                    {
                        delete oCC.papoCurves[iRing];
                        oCC.nCurveCount = iRing;
                        return eErr;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return eErr;
                }
            }

            // one point is XYZ or XYZM, other is XY or XYM
            // returns an error
            else if ((start_point->Is3D() & end_point->Is3D() == 0) &&
                     (start_point->Is3D() | end_point->Is3D() == 1))
            {
                delete oCC.papoCurves[iRing];
                oCC.nCurveCount = iRing;
                return eErr;
            }

            // one point is XYM or XYZM, other is XYZ or XY
            // returns an error
            else if ((start_point->IsMeasured() & end_point->IsMeasured() == 0) &&
                     (start_point->IsMeasured() | end_point->IsMeasured() == 1))
            {
                delete oCC.papoCurves[iRing];
                oCC.nCurveCount = iRing;
                return eErr;
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
                        return eErr;
                    }
                }
                else
                {
                    delete oCC.papoCurves[iRing];
                    oCC.nCurveCount = iRing;
                    return eErr;
                }
            }
        }

        // there should be exactly four points
        // if there are not four points, then this falls under OGRPolygon and not OGRTriangle
        else
        {
            delete oCC.papoCurves[iRing];
            oCC.nCurveCount = iRing;
            return eErr;
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

OGRErr OGRTriangle::exportToWkb( OGRwkbByteOrder eByteOrder,
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
