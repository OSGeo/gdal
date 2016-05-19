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

    // cast every geometry into a LinearRing and attach it to the data
    int nOffset = 9;

    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLinearRing* poLR = (OGRLinearRing*) oCC.papoCurves[iRing];
        poLR->_exportToWkb( eByteOrder, flags, pabyData + nOffset );
        nOffset += poLR->_WkbSize(flags);
    }

    return OGRERR_NONE;
}
