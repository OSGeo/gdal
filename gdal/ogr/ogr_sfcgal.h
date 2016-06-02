#ifdef HAVE_SFCGAL

// Broad API
#include <SFCGAL/LineString.h>
#include <SFCGAL/MultiLineString.h>
#include <SFCGAL/MultiPoint.h>
#include <SFCGAL/MultiPolygon.h>
#include <SFCGAL/MultiSolid.h>
#include <SFCGAL/Point.h>
#include <SFCGAL/Polygon.h>
#include <SFCGAL/PolyhedralSurface.h>
#include <SFCGAL/Solid.h>
#include <SFCGAL/Surface.h>
#include <SFCGAL/Triangle.h>
#include <SFCGAL/TriangulatedSurface.h>

// API for SFCGAL::io::writeBinaryGeometry()
#include <SFCGAL/detail/io/Serialization.h>

// API for SFCGAL::algorithm::intersects3D()
#include <SFCGAL/algorithm/intersects.h>

// API for SFCGAL::algorithm::convexHull3D()
#include <SFCGAL/algorithm/convexHull.h>

// API for SFCGAL::algorithm::difference3D()
#include <SFCGAL/algorithm/difference.h>

// API for SFCGAL::algorithm::intersection3D()
#include <SFCGAL/algorithm/intersection.h>

// API for SFCGAL::detail::algorithm::coversPoints3D()
#include <SFCGAL/detail/algorithm/coversPoints.h>

// API for SFCGAL::algorithm::union3D()
#include <SFCGAL/detail/algorithm/union.h>

// API for SFCGAL::algorithm::area3D()
#include <SFCGAL/detail/algorithm/area.h>

#endif
