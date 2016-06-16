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

    /* coverity[tainted_data] */
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
    if( eWkbVariant == wkbVariantOldOgc &&
        (wkbFlatten(getGeometryType()) == wkbMultiCurve ||
         wkbFlatten(getGeometryType()) == wkbMultiSurface) ) /* does not make sense for new geometries, so patch it */
    {
        eWkbVariant = wkbVariantIso;
    }

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
        oMP.papoGeoms[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset, eWkbVariant );
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
        else if ((EQUAL(szToken,"POLYGON") ||
                  EQUAL(szToken,"CURVEPOLYGON")))
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
                                           OGRwkbVariant eWkbVariant ) const
{
    return exportToWktInternal(ppszDstText, eWkbVariant, "POLYGON");
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
                CPLDebug( "OGR", "OGRPolyhedralSurface::exportToWkt() - skipping %s.",
                          papszGeoms[iGeom] );
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
    // Will write function later
    if (poOther == NULL)
    {

    }
    return TRUE;
}

double OGRPolyhedralSurface::get_Area() const
{
    // Will write function later
    return -1.0;
}

OGRErr OGRPolyhedralSurface::PointOnSurface(OGRPoint *poPoint) const
{
    // Will write function later
    if (poPoint != NULL)
    {

    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

OGRMultiPolygon* OGRPolyhedralSurface::CastToMultiPolygon()
{
    OGRMultiPolygon *poMultiPolygon = new OGRMultiPolygon(this->oMP);
    return poMultiPolygon;
}
