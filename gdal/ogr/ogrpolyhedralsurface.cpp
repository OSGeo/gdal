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
