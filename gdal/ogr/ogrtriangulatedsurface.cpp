/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRTriangulatedSurface geometry class.
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
/*                        OGRTriangulatedSurface()                      */
/************************************************************************/

OGRTriangulatedSurface::OGRTriangulatedSurface()

{ }

/************************************************************************/
/*        OGRTriangulatedSurface( const OGRTriangulatedSurface& )       */
/************************************************************************/

OGRTriangulatedSurface::OGRTriangulatedSurface( const OGRTriangulatedSurface& other ) :
    OGRPolyhedralSurface(other)
{ }

/************************************************************************/
/*                        ~OGRTriangulatedSurface()                     */
/************************************************************************/

OGRTriangulatedSurface::~OGRTriangulatedSurface()

{ }

/************************************************************************/
/*                 operator=( const OGRTriangulatedSurface&)            */
/************************************************************************/

OGRTriangulatedSurface& OGRTriangulatedSurface::operator=( const OGRTriangulatedSurface& other )
{
    if( this != &other)
    {
        OGRSurface::operator=( other );
        oMP = other.oMP;
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char* OGRTriangulatedSurface::getGeometryName() const
{
    return "TIN" ;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRTriangulatedSurface::getGeometryType() const
{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbTINZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbTINM;
    else if( flags & OGR_G_3D )
        return wkbTINZ;
    else
        return wkbTIN;
}

/************************************************************************/
/*                              WkbSize()                               */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRTriangulatedSurface::WkbSize() const
{
    int nSize = 9;
    for( int i = 0; i < oMP.nGeomCount; i++ )
        nSize += oMP.papoGeoms[i]->WkbSize();
    return nSize;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry* OGRTriangulatedSurface::clone() const
{
    OGRTriangulatedSurface *poNewTIN;
    poNewTIN = (OGRTriangulatedSurface*) OGRGeometryFactory::createGeometry(getGeometryType());
    if( poNewTIN == NULL )
        return NULL;

    poNewTIN->assignSpatialReference(getSpatialReference());
    poNewTIN->flags = flags;

    for( int i = 0; i < oMP.nGeomCount; i++ )
    {
        if( poNewTIN->oMP.addGeometry( oMP.papoGeoms[i] ) != OGRERR_NONE )
        {
            delete poNewTIN;
            return NULL;
        }
    }

    return poNewTIN;
}

/************************************************************************/
/*                           importFromWkb()                            */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRTriangulatedSurface::importFromWkb ( unsigned char * pabyData,
                                               int nSize,
                                               OGRwkbVariant eWkbVariant )

{
    oMP.nGeomCount = 0;
    OGRwkbByteOrder eByteOrder = wkbXDR;
    int nDataOffset = 0;
    OGRErr eErr = importPreambuleOfCollectionFromWkb( pabyData,
                                                      nSize,
                                                      nDataOffset,
                                                      eByteOrder,
                                                      9,
                                                      oMP.nGeomCount,
                                                      eWkbVariant );

    if( eErr != OGRERR_NONE )
        return eErr;

    oMP.papoGeoms = (OGRGeometry **) VSI_CALLOC_VERBOSE(sizeof(void*), oMP.nGeomCount);
    if (oMP.nGeomCount != 0 && oMP.papoGeoms == NULL)
    {
        oMP.nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    // for each geometry
    for( int iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        // Parse the polygons
        unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eSubGeomType;
        eErr = OGRReadWKBGeometryType( pabySubData, eWkbVariant, &eSubGeomType );
        if( eErr != OGRERR_NONE )
            return eErr;

        OGRGeometry* poSubGeom = NULL;
        eErr = OGRGeometryFactory::createFromWkb( pabySubData, NULL, &poSubGeom, nSize, eWkbVariant );

        if( eErr != OGRERR_NONE )
        {
            oMP.nGeomCount = iGeom;
            delete poSubGeom;
            return eErr;
        }

        oMP.papoGeoms[iGeom] = poSubGeom;

        if (oMP.papoGeoms[iGeom]->Is3D())
            flags |= OGR_G_3D;
        if (oMP.papoGeoms[iGeom]->IsMeasured())
            flags |= OGR_G_MEASURED;

        int nSubGeomWkbSize = oMP.papoGeoms[iGeom]->WkbSize();
        if( nSize != -1 )
            nSize -= nSubGeomWkbSize;

        nDataOffset += nSubGeomWkbSize;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRTriangulatedSurface::exportToWkb ( OGRwkbByteOrder eByteOrder,
                                              unsigned char * pabyData,
                                              OGRwkbVariant eWkbVariant ) const

{
    // Set the byte order
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

    GUInt32 nGType = getGeometryType();

    if ( eWkbVariant == wkbVariantIso )
        nGType = getIsoGeometryType();

    else if( eWkbVariant == wkbVariantPostGIS1 )
    {
        int bIs3D = wkbHasZ((OGRwkbGeometryType)nGType);
        nGType = wkbFlatten(nGType);
        if( nGType == wkbMultiCurve )
            nGType = POSTGIS15_MULTICURVE;
        else if( nGType == wkbMultiSurface )
            nGType = POSTGIS15_MULTISURFACE;
        if( bIs3D )
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse);
    }

    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );

    // Copy the raw data
    if( OGR_SWAP( eByteOrder ) )
    {
        int nCount = CPL_SWAP32( oMP.nGeomCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
        memcpy( pabyData+5, &oMP.nGeomCount, 4 );

    int nOffset = 9;

    // serialize each of the geometries
    for( int iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        oMP.papoGeoms[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset, eWkbVariant );
        nOffset += oMP.papoGeoms[iGeom]->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*              Instantiate from well known text format.                */
/************************************************************************/

OGRErr OGRTriangulatedSurface::importFromWkt( char ** ppszInput )

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

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    eErr = OGRERR_NONE;

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );

    // Read each surface
    OGRRawPoint *paoPoints = NULL;
    int          nMaxPoints = 0;
    double      *padfZ = NULL;

    do
    {

        // Get the first token
        const char* pszInputBefore = pszInput;
        pszInput = OGRWktReadToken( pszInput, szToken );

        OGRSurface* poSurface;

        // Start importing
        if (EQUAL(szToken,"("))
        {
            OGRPolygon      *poPolygon = new OGRPolygon();
            poSurface = poPolygon;
            pszInput = pszInputBefore;
            eErr = poPolygon->importFromWKTListOnly( (char**)&pszInput, bHasZ, bHasM,
                                                     paoPoints, nMaxPoints, padfZ );
        }
        else if (EQUAL(szToken, "EMPTY") )
        {
            poSurface = new OGRPolygon();
        }

        /* We accept POLYGON() but this is an extension to the BNF, also */
        /* accepted by PostGIS */
        else if (EQUAL(szToken,"POLYGON"))
        {
            OGRGeometry* poGeom = NULL;
            pszInput = pszInputBefore;
            eErr = OGRGeometryFactory::createFromWkt( (char **) &pszInput,
                                                       NULL, &poGeom );
            poSurface = (OGRSurface*) poGeom;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected token : %s", szToken);
            eErr = OGRERR_CORRUPT_DATA;
            break;
        }

        if( eErr == OGRERR_NONE )
            eErr = oMP.addGeometryDirectly( poSurface );
        if( eErr != OGRERR_NONE )
        {
            delete poSurface;
            break;
        }

        // Read the delimiter following the surface.
        pszInput = OGRWktReadToken( pszInput, szToken );

    } while( szToken[0] == ',' && eErr == OGRERR_NONE );

    CPLFree( paoPoints );
    CPLFree( padfZ );

    // Check for a closing bracket
    if( eErr != OGRERR_NONE )
        return eErr;

    if( szToken[0] != ')' )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = (char *) pszInput;
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*      Translate this structure into it's well known text format       */
/*      equivalent.                                                     */
/************************************************************************/

OGRErr OGRTriangulatedSurface::exportToWkt ( char ** ppszDstText,
                                           CPL_UNUSED OGRwkbVariant eWkbVariant ) const
{
    return exportToWktInternal(ppszDstText, wkbVariantIso, "POLYGON");
}

/************************************************************************/
/*                            addGeometry()                             */
/*      Add a new geometry to a TIN.  Only a POLYGON or a TRIANGLE      */
/*      can be added to a TRIANGULATEDSURFACE.                          */
/************************************************************************/

OGRErr OGRTriangulatedSurface::addGeometry (const OGRGeometry *poNewGeom)
{
    if (!(EQUAL(poNewGeom->getGeometryName(),"POLYGON") || EQUAL(poNewGeom->getGeometryName(),"TRIANGLE")))
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    // If it is a triangle, we can add it to the TIN without any hassle
    // However, we can only add it as a polygon, so we need to create a Polygon out of it
    if (EQUAL(poNewGeom->getGeometryName(), "TRIANGLE"))
    {
        OGRPolygon *poGeom = new OGRPolygon(*((OGRPolygon *)poNewGeom));
        OGRErr eErr = OGRPolyhedralSurface::addGeometry(poGeom);
        delete poGeom;
        return eErr;
    }

    // In case of Polygon, we have to check that it is a valid triangle -
    // closed and contains one external ring of four points
    OGRPolygon *poPolygon = (OGRPolygon *)poNewGeom;
    if (poPolygon->getNumInteriorRings() == 0)
    {
        OGRCurve *poCurve = poPolygon->getExteriorRingCurve();
        if (poCurve->get_IsClosed())
        {
            if (poCurve->getNumPoints() == 4)
            {
                // everything is fine, we will add this to the TIN
                return OGRPolyhedralSurface::addGeometry(poNewGeom);
            }
        }
    }

    return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
}
