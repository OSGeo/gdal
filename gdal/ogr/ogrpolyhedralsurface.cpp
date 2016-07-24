/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRPolyhedralSurface geometry class.
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
/*                         OGRPolyhedralSurface()                       */
/************************************************************************/

OGRPolyhedralSurface::OGRPolyhedralSurface()

{ }

/************************************************************************/
/*         OGRPolyhedralSurface( const OGRPolyhedralSurface& )          */
/************************************************************************/

OGRPolyhedralSurface::OGRPolyhedralSurface( const OGRPolyhedralSurface& other ) :
    OGRSurface(other),
    oMP(other.oMP)
{ }

/************************************************************************/
/*                        ~OGRPolyhedralSurface()                       */
/************************************************************************/

OGRPolyhedralSurface::~OGRPolyhedralSurface()

{ }

/************************************************************************/
/*                 operator=( const OGRPolyhedralSurface&)              */
/************************************************************************/

OGRPolyhedralSurface& OGRPolyhedralSurface::operator=( const OGRPolyhedralSurface& other )
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

const char* OGRPolyhedralSurface::getGeometryName() const
{
    return "POLYHEDRALSURFACE" ;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRPolyhedralSurface::getGeometryType() const
{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbPolyhedralSurfaceZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbPolyhedralSurfaceM;
    else if( flags & OGR_G_3D )
        return wkbPolyhedralSurfaceZ;
    else
        return wkbPolyhedralSurface;
}

/************************************************************************/
/*                              WkbSize()                               */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

int OGRPolyhedralSurface::WkbSize() const
{
    int nSize = 9;
    for( int i = 0; i < oMP.nGeomCount; i++ )
        nSize += oMP.papoGeoms[i]->WkbSize();
    return nSize;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRPolyhedralSurface::getDimension() const
{
    return 2;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRPolyhedralSurface::empty()
{
    if( oMP.papoGeoms != NULL )
    {
        for( int i = 0; i < oMP.nGeomCount; i++ )
            delete oMP.papoGeoms[i];
        OGRFree(oMP.papoGeoms);
    }
    oMP.nGeomCount = 0;
    oMP.papoGeoms = NULL;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry* OGRPolyhedralSurface::clone() const
{
    OGRPolyhedralSurface *poNewPS;
    poNewPS = (OGRPolyhedralSurface*) OGRGeometryFactory::createGeometry(getGeometryType());
    if( poNewPS == NULL )
        return NULL;

    poNewPS->assignSpatialReference(getSpatialReference());
    poNewPS->flags = flags;

    for( int i = 0; i < oMP.nGeomCount; i++ )
    {
        if( poNewPS->oMP.addGeometry( oMP.papoGeoms[i] ) != OGRERR_NONE )
        {
            delete poNewPS;
            return NULL;
        }
    }

    return poNewPS;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPolyhedralSurface::getEnvelope( OGREnvelope * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRPolyhedralSurface::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                           importFromWkb()                            */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRPolyhedralSurface::importFromWkb ( unsigned char * pabyData,
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

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
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

OGRErr  OGRPolyhedralSurface::exportToWkb ( OGRwkbByteOrder eByteOrder,
                                            unsigned char * pabyData,
                                            OGRwkbVariant eWkbVariant ) const

{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
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
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse); /* yes we explicitly set wkb25DBit */
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
        oMP.papoGeoms[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset, wkbVariantIso );
        nOffset += oMP.papoGeoms[iGeom]->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/*              Instantiate from well known text format.                */
/************************************************************************/

OGRErr OGRPolyhedralSurface::importFromWkt( char ** ppszInput )

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

/* ==================================================================== */
/*      Read each surface in turn.  Note that we try to reuse the same  */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int          nMaxPoints = 0;
    double      *padfZ = NULL;

    do
    {

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        const char* pszInputBefore = pszInput;
        pszInput = OGRWktReadToken( pszInput, szToken );

        OGRSurface* poSurface;

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
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

OGRErr OGRPolyhedralSurface::exportToWkt ( char ** ppszDstText,
                                           CPL_UNUSED OGRwkbVariant eWkbVariant ) const
{
    return exportToWktInternal(ppszDstText, wkbVariantIso, "POLYGON");
}

OGRErr OGRPolyhedralSurface::exportToWktInternal ( char ** ppszDstText,
                                                   OGRwkbVariant eWkbVariant,
                                                   const char* pszSkipPrefix ) const
{
    char        **papszGeoms;
    int         iGeom;
    size_t      nCumulativeLength = 0;
    OGRErr      eErr;
    bool bMustWriteComma = false;

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    papszGeoms = (oMP.nGeomCount) ? (char **) CPLCalloc(sizeof(char *),oMP.nGeomCount) : NULL;

    for( iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        eErr = oMP.papoGeoms[iGeom]->exportToWkt( &(papszGeoms[iGeom]), eWkbVariant );
        if( eErr != OGRERR_NONE )
            goto error;

        size_t nSkip = 0;
        if( pszSkipPrefix != NULL &&
            EQUALN(papszGeoms[iGeom], pszSkipPrefix, strlen(pszSkipPrefix)) &&
            papszGeoms[iGeom][strlen(pszSkipPrefix)] == ' ' )
        {
            nSkip = strlen(pszSkipPrefix) + 1;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "ZM ") )
                nSkip += 3;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "M ") )
                nSkip += 2;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "Z ") )
                nSkip += 2;

            /* skip empty subgeoms */
            if( papszGeoms[iGeom][nSkip] != '(' )
            {
                CPLDebug( "OGR", "OGR%s::exportToWkt() - skipping %s.",getGeometryName(), papszGeoms[iGeom] );
                CPLFree( papszGeoms[iGeom] );
                papszGeoms[iGeom] = NULL;
                continue;
            }
        }
        else if( eWkbVariant != wkbVariantIso )
        {
            char *substr;
            if( (substr = strstr(papszGeoms[iGeom], " Z")) != NULL )
                memmove(substr, substr+strlen(" Z"), 1+strlen(substr+strlen(" Z")));
        }

        nCumulativeLength += strlen(papszGeoms[iGeom] + nSkip);
    }

/* -------------------------------------------------------------------- */
/*      Return XXXXXXXXXXXXXXX EMPTY if we get no valid line string.    */
/* -------------------------------------------------------------------- */
    if( nCumulativeLength == 0 )
    {
        CPLFree( papszGeoms );
        CPLString osEmpty;
        if( eWkbVariant == wkbVariantIso )
        {
            if( Is3D() && IsMeasured() )
                osEmpty.Printf("%s ZM EMPTY",getGeometryName());
            else if( IsMeasured() )
                osEmpty.Printf("%s M EMPTY",getGeometryName());
            else if( Is3D() )
                osEmpty.Printf("%s Z EMPTY",getGeometryName());
            else
                osEmpty.Printf("%s EMPTY",getGeometryName());
        }
        else
            osEmpty.Printf("%s EMPTY",getGeometryName());
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSI_MALLOC_VERBOSE(nCumulativeLength + oMP.nGeomCount + 26);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, getGeometryName() );
    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            strcat( *ppszDstText, " ZM" );
        else if( flags & OGR_G_3D )
            strcat( *ppszDstText, " Z" );
        else if( flags & OGR_G_MEASURED )
            strcat( *ppszDstText, " M" );
    }
    strcat( *ppszDstText, " (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        if( papszGeoms[iGeom] == NULL )
            continue;

        if( bMustWriteComma )
            (*ppszDstText)[nCumulativeLength++] = ',';
        bMustWriteComma = true;

        size_t nSkip = 0;
        if( pszSkipPrefix != NULL &&
            EQUALN(papszGeoms[iGeom], pszSkipPrefix, strlen(pszSkipPrefix)) &&
            papszGeoms[iGeom][strlen(pszSkipPrefix)] == ' ' )
        {
            nSkip = strlen(pszSkipPrefix) + 1;
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "ZM ") )
                nSkip += 3;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "M ") )
                nSkip += 2;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "Z ") )
                nSkip += 2;
        }

        size_t nGeomLength = strlen(papszGeoms[iGeom] + nSkip);
        memcpy( *ppszDstText + nCumulativeLength, papszGeoms[iGeom] + nSkip, nGeomLength );
        nCumulativeLength += nGeomLength;
        VSIFree( papszGeoms[iGeom] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszGeoms );

    return OGRERR_NONE;

error:
    for( iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
        CPLFree( papszGeoms[iGeom] );
    CPLFree( papszGeoms );
    return eErr;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRPolyhedralSurface::flattenTo2D()
{
    oMP.flattenTo2D();
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRPolyhedralSurface::transform( OGRCoordinateTransformation *poCT )
{
    return oMP.transform(poCT);
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

OGRSurfaceCasterToPolygon OGRPolyhedralSurface::GetCasterToPolygon() const
{
    return (OGRSurfaceCasterToPolygon) NULL;
}

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

OGRSurfaceCasterToCurvePolygon OGRPolyhedralSurface::GetCasterToCurvePolygon() const
{
    return (OGRSurfaceCasterToCurvePolygon) NULL;
}

/************************************************************************/
/*                               Equals()                               */
/************************************************************************/

OGRBoolean OGRPolyhedralSurface::Equals(OGRGeometry * poOther) const
{

    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if ( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    OGRPolyhedralSurface *poOMP = (OGRPolyhedralSurface *) poOther;
    if( oMP.getNumGeometries() != poOMP->oMP.getNumGeometries() )
        return FALSE;

    for( int iGeom = 0; iGeom < oMP.nGeomCount; iGeom++ )
    {
        if( !oMP.getGeometryRef(iGeom)->Equals(poOMP->oMP.getGeometryRef(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

double OGRPolyhedralSurface::get_Area() const
{
#ifndef HAVE_SFCGAL

    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return -1.0;

#else

    sfcgal_init();
    sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
    if (poThis == NULL)
        return -1.0;

    double area = sfcgal_geometry_area_3d(poThis);

    sfcgal_geometry_delete(poThis);

    return (area > 0)? area: -1.0;

#endif
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

OGRErr OGRPolyhedralSurface::PointOnSurface(OGRPoint *poPoint) const
{
    return PointOnSurfaceInternal(poPoint);
}

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

OGRMultiPolygon* OGRPolyhedralSurface::CastToMultiPolygon()
{
    OGRMultiPolygon *poMultiPolygon = new OGRMultiPolygon(this->oMP);
    return poMultiPolygon;
}

/************************************************************************/
/*                            addGeometry()                             */
/*      Add a new geometry to a collection.  Only a POLYGON can be      */
/*      added to a POLYHEDRALSURFACE.                                   */
/************************************************************************/

OGRErr OGRPolyhedralSurface::addGeometry (const OGRGeometry *poNewGeom)
{
    if (!EQUAL(poNewGeom->getGeometryName(),"POLYGON"))
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    OGRGeometry *poClone = poNewGeom->clone();
    OGRErr      eErr;

    if (poClone == NULL)
        return OGRERR_FAILURE;

    eErr = oMP.addGeometryDirectly(poClone);

    if( eErr != OGRERR_NONE )
        delete poClone;

    return eErr;
}

/************************************************************************/
/*                        addGeometryDirectly()                         */
/************************************************************************/

OGRErr OGRPolyhedralSurface::addGeometryDirectly (OGRGeometry *poNewGeom)
{
    if (!EQUAL(poNewGeom->getGeometryName(), "POLYGON"))
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    if( poNewGeom->Is3D() && !Is3D() )
        set3D(TRUE);

    if( poNewGeom->IsMeasured() && !IsMeasured() )
        setMeasured(TRUE);

    if( !poNewGeom->Is3D() && Is3D() )
        poNewGeom->set3D(TRUE);

    if( !poNewGeom->IsMeasured() && IsMeasured() )
        poNewGeom->setMeasured(TRUE);

    OGRGeometry** papoNewGeoms = (OGRGeometry **) VSI_REALLOC_VERBOSE( oMP.papoGeoms,
                                             sizeof(void*) * (oMP.nGeomCount+1) );
    if( papoNewGeoms == NULL )
        return OGRERR_FAILURE;

    oMP.papoGeoms = papoNewGeoms;
    oMP.papoGeoms[oMP.nGeomCount] = poNewGeom;
    oMP.nGeomCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          getNumGeometries()                          */
/************************************************************************/

int OGRPolyhedralSurface::getNumGeometries()
{
    return oMP.nGeomCount;
}

/************************************************************************/
/*                            getGeometry()                             */
/************************************************************************/

OGRGeometry* OGRPolyhedralSurface::getGeometry(int i)
{
    return (OGRGeometry *)oMP.papoGeoms[i];
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean  OGRPolyhedralSurface::IsEmpty() const
{
    return oMP.IsEmpty();
}

/************************************************************************/
/*                                 set3D()                              */
/************************************************************************/

void OGRPolyhedralSurface::set3D (OGRBoolean bIs3D)
{
    oMP.set3D(bIs3D);

    OGRGeometry::set3D( bIs3D );
}

/************************************************************************/
/*                             setMeasured()                            */
/************************************************************************/

void OGRPolyhedralSurface::setMeasured (OGRBoolean bIsMeasured)
{
    oMP.setMeasured(bIsMeasured);

    OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRPolyhedralSurface::setCoordinateDimension (int nNewDimension)
{
    oMP.setCoordinateDimension(nNewDimension);

    OGRGeometry::setCoordinateDimension( nNewDimension );
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRPolyhedralSurface::swapXY()
{
    oMP.swapXY();
}

/************************************************************************/
/*                             Distance3D()                             */
/*    Returns the 3D distance between the two geometries. The distance  */
/*    is expressed into the same unit as the coordinates of the         */
/*    geometries.                                                       */
/************************************************************************/

double OGRPolyhedralSurface::Distance3D(UNUSED_IF_NO_SFCGAL const OGRGeometry *poOtherGeom) const
{
    if (poOtherGeom == NULL)
    {
        CPLDebug( "OGR", "%s::Distance3D called with NULL geometry pointer", getGeometryName() );
        return -1.0;
    }

    if (!(poOtherGeom->Is3D() && this->Is3D()))
    {
        CPLDebug( "OGR", "%s::Distance3D called with two dimensional geometry(geometries)", getGeometryName() );
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
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRPolyhedralSurface::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
{
    return FALSE;
}

/************************************************************************/
/*                          removeGeometry()                            */
/************************************************************************/

OGRErr OGRPolyhedralSurface::removeGeometry(int iGeom, int bDelete)
{
    return this->oMP.removeGeometry(iGeom,bDelete);
}
