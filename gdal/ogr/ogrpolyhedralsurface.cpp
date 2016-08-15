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

/**
 * \brief Create an empty PolyhedralSurface
 */

OGRPolyhedralSurface::OGRPolyhedralSurface()

{ }

/************************************************************************/
/*         OGRPolyhedralSurface( const OGRPolyhedralSurface& )          */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 */

OGRPolyhedralSurface::OGRPolyhedralSurface( const OGRPolyhedralSurface& other ) :
    OGRSurface(other),
    oMP(other.oMP)
{ }

/************************************************************************/
/*                        ~OGRPolyhedralSurface()                       */
/************************************************************************/

/**
 * \brief Destructor
 *
 */

OGRPolyhedralSurface::~OGRPolyhedralSurface()

{ }

/************************************************************************/
/*                 operator=( const OGRPolyhedralSurface&)              */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 */

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

/**
 * \brief Returns the geometry name of the PolyhedralSurface
 *
 * @return "POLYHEDRALSURFACE"
 *
 */

const char* OGRPolyhedralSurface::getGeometryName() const
{
    return "POLYHEDRALSURFACE" ;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

/**
 * \brief Returns the WKB Type of PolyhedralSurface
 *
 */

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

/**
 * \brief Returns the dimension of OGRPolyhedralSurface
 *
 * @return int Returns 2
 */

int OGRPolyhedralSurface::getDimension() const
{
    return 2;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

/**
 * \brief Deletes all geometries contained within the PolyhedralSurface
 *
 */

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

/**
 * \brief Make a copy of this object.
 *
 * This method relates to the SFCOM IGeometry::clone() method.
 *
 * This method is the same as the C function OGR_G_Clone().
 *
 * @return a new object instance with the same geometry, and spatial
 * reference system as the original.
 */

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

/**
 * \brief Computes and returns the bounding envelope for this geometry in the passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope().
 *
 * @param psEnvelope the structure in which to place the results.
 */

void OGRPolyhedralSurface::getEnvelope( OGREnvelope * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

/**
 * \brief Computes and returns the bounding envelope for this geometry in the passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope().
 *
 * @param psEnvelope the structure in which to place the results.
 */

void OGRPolyhedralSurface::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    oMP.getEnvelope(psEnvelope);
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

OGRErr OGRPolyhedralSurface::exportToWkt ( char ** ppszDstText,
                                           CPL_UNUSED OGRwkbVariant eWkbVariant ) const
{
    return exportToWktInternal(ppszDstText, wkbVariantIso, "POLYGON");
}

//! @cond Doxygen_Suppress
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
//! @endcond

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

/**
 * \brief Convert geometry to strictly 2D.
 * In a sense this converts all Z coordinates
 * to 0.0.
 *
 * This method is the same as the C function OGR_G_FlattenTo2D().
 */

void OGRPolyhedralSurface::flattenTo2D()
{
    oMP.flattenTo2D();
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

/**
 * \brief Apply arbitrary coordinate transformation to geometry.
 *
 * This method will transform the coordinates of a geometry from
 * their current spatial reference system to a new target spatial
 * reference system.  Normally this means reprojecting the vectors,
 * but it could include datum shifts, and changes of units.
 *
 * Note that this method does not require that the geometry already
 * have a spatial reference system.  It will be assumed that they can
 * be treated as having the source spatial reference system of the
 * OGRCoordinateTransformation object, and the actual SRS of the geometry
 * will be ignored.  On successful completion the output OGRSpatialReference
 * of the OGRCoordinateTransformation will be assigned to the geometry.
 *
 * This method is the same as the C function OGR_G_Transform().
 *
 * @param poCT the transformation to apply.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRPolyhedralSurface::transform( OGRCoordinateTransformation *poCT )
{
    return oMP.transform(poCT);
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRSurfaceCasterToPolygon OGRPolyhedralSurface::GetCasterToPolygon() const
{
    return (OGRSurfaceCasterToPolygon) NULL;
}
//! @endcond

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRSurfaceCasterToCurvePolygon OGRPolyhedralSurface::GetCasterToCurvePolygon() const
{
    return (OGRSurfaceCasterToCurvePolygon) NULL;
}
//! @endcond

/************************************************************************/
/*                               Equals()                               */
/************************************************************************/

/**
 * \brief Returns TRUE if two geometries are equivalent.
 *
 * This method is the same as the C function OGR_G_Equals().
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

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

/**
 * \brief Returns the area enclosed
 *
 * This method is built on the SFCGAL library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the SFCGAL library, this method will always return
 * -1.0
 *
 * @return area enclosed by the PolyhedralSurface
 */

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

/**
 * \brief Checks if the point is on a surface
 *
 */

OGRErr OGRPolyhedralSurface::PointOnSurface(OGRPoint *poPoint) const
{
    return PointOnSurfaceInternal(poPoint);
}

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

/**
 * \brief Casts the OGRPolyhedralSurface to an OGRMultiPolygon
 *
 * @return OGRMultiPolygon* pointer to the computed OGRMultiPolygon
 */

OGRMultiPolygon* OGRPolyhedralSurface::CastToMultiPolygon()
{
    OGRMultiPolygon *poMultiPolygon = new OGRMultiPolygon(this->oMP);
    return poMultiPolygon;
}

/************************************************************************/
/*                            addGeometry()                             */
/************************************************************************/

/**
 * \brief Add a new geometry to a collection.
 *
 * Only a POLYGON can be added to a POLYHEDRALSURFACE.
 *
 * @return OGRErr OGRERR_NONE if the polygon is successfully added
 */

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

/**
 * \brief Add a geometry directly to the container.
 *
 * This method is the same as the C function OGR_G_AddGeometryDirectly().
 *
 * There is no SFCOM analog to this method.
 *
 * @param poNewGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of geometry container.
 */

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

/**
 * \brief Fetch number of geometries in PolyhedralSurface
 *
 * @return count of children geometries.  May be zero.
 */

int OGRPolyhedralSurface::getNumGeometries()
{
    return oMP.nGeomCount;
}

/************************************************************************/
/*                            getGeometry()                             */
/************************************************************************/

/**
 * \brief Fetch geometry from container.
 *
 * This method returns a pointer to an geometry within the container.  The
 * returned geometry remains owned by the container, and should not be
 * modified.  The pointer is only valid until the next change to the
 * geometry container.  Use IGeometry::clone() to make a copy.
 *
 * @param i the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return pointer to requested geometry.
 */

OGRGeometry* OGRPolyhedralSurface::getGeometry(int i)
{
    return (OGRGeometry *)oMP.papoGeoms[i];
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

/**
 * \brief Checks if the PolyhedralSurface is empty
 *
 * @return TRUE if the PolyhedralSurface is empty, FALSE otherwise
 */

OGRBoolean  OGRPolyhedralSurface::IsEmpty() const
{
    return oMP.IsEmpty();
}

/************************************************************************/
/*                                 set3D()                              */
/************************************************************************/

/**
 * \brief Set the type as 3D geometry
 */

void OGRPolyhedralSurface::set3D (OGRBoolean bIs3D)
{
    oMP.set3D(bIs3D);

    OGRGeometry::set3D( bIs3D );
}

/************************************************************************/
/*                             setMeasured()                            */
/************************************************************************/

/**
 * \brief Set the type as Measured
 */

void OGRPolyhedralSurface::setMeasured (OGRBoolean bIsMeasured)
{
    oMP.setMeasured(bIsMeasured);

    OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

/**
 * \brief Set the coordinate dimension.
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values.
 * This will also remove the M dimension if present before this call.
 *
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGRPolyhedralSurface::setCoordinateDimension (int nNewDimension)
{
    oMP.setCoordinateDimension(nNewDimension);

    OGRGeometry::setCoordinateDimension( nNewDimension );
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

/**
 * \brief Swap x and y coordinates.
 */

void OGRPolyhedralSurface::swapXY()
{
    oMP.swapXY();
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

/**
 * \brief Returns if this geometry is or has curve geometry.
 *
 * This method is the same as the C function OGR_G_HasCurveGeometry().
 *
 * @param bLookForNonLinear set it to TRUE to check if the geometry is or contains
 * a CIRCULARSTRING.
 *
 * @return TRUE if this geometry is or has curve geometry.
 *
 * @since GDAL 2.0
 */


OGRBoolean OGRPolyhedralSurface::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
{
    return FALSE;
}

/************************************************************************/
/*                          removeGeometry()                            */
/************************************************************************/

/**
 * \brief Remove a geometry from the container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * @param iGeom the index of the geometry to delete.  A value of -1 is a
 * special flag meaning that all geometries should be removed.
 *
 * @param bDelete if TRUE the geometry will be deallocated, otherwise it will
 * not.  The default is TRUE as the container is considered to own the
 * geometries in it.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is
 * out of range.
 */

OGRErr OGRPolyhedralSurface::removeGeometry(int iGeom, int bDelete)
{
    return this->oMP.removeGeometry(iGeom,bDelete);
}
