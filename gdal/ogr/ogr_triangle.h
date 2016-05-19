#include "ogr_geometry.h"
#include "ogr_sfcgal.h"
#include "ogr_p.h"

// TO BE INCORPORATED IN ogr_geometry.h
// Included in an external file for the sake of readability

class CPL_DLL OGROGRTriangle : public OGRPolygon
{

  protected:
    friend class OGRCurveCollection;
    virtual OGRBoolean isCompatibleSubType(OGRwkbGeometryType) const;

  public:
    OGROGRTriangle();
    OGROGRTriangle(const OGRPoint &p, const OGRPoint &q, const OGRPoint &r);
    OGRTriangle(const OGROGRTriangle &other);
    OGROGRTriangle& operator=(const OGROGRTriangle& other);
    virtual ~OGRTriangle();

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb(unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc);
    virtual OGRErr exportToWkb(OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc) const;
    virtual OGRErr importFromWkt(char **);
    virtual OGRErr exportToWkt(char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc) const;

    virtual void empty() = 0;
    virtual OGRGeometry *clone() const CPL_WARN_UNUSED_RESULT = 0;
    virtual void getEnvelope(OGREnvelope * psEnvelope) const = 0;
    virtual void getEnvelope(OGREnvelope3D * psEnvelope) const = 0;

    // SFCGAL interface methods
    // The interface mechanism is handled through WKT. An OGR WKT is passed as a parameter to
    // Geometry *SFCGAL::detail::io::WktReader::readGeometry() and a corresponding SFCGAL::Geometry is derived.
    // This is used in all the algorithms which act as a wrapper for SFCGAL.
    // If the resultant geometry is modified in any way, get the WKT of the geometry by using the method
    // std::string SFCGAL::Geometry::asText	(const int& numDecimals = -1) within the calling function.
    // To convert back to OGR, the WKT of the SFCGAL::Geometry is passed as a parameter to
    // virtual OGRErr importFromWkt(char **)  and an OGRGeometry is returned
    virtual OGRErr exportToSFCGAL (char **, void*) const CPL_WARN_UNUSED_RESULT;

    // Need to throw an error if these are interfaced via OGRPolyhedralSurface + make them virtual in OGRGeometry
    static GEOSContextHandle_t createGEOSContext();
    static void freeGEOSContext(GEOSContextHandle_t hGEOSCtxt);
    virtual GEOSGeom exportToGEOS(GEOSContextHandle_t hGEOSCtxt) const CPL_WARN_UNUSED_RESULT;

    // New methods interfaced through SFCGAL
    virtual OGRGeometry *Boundary();
    OGRGeometry* clone();
    virtual double Distance(const OGRGeometry *poOtherGeom) const;
    virtual double Distance3D(const OGRGeometry *poOtherGeom) const;
    void Reverse();
    virtual OGRBoolean IsEmpty() const;

    // Methods inherited from OGRPolygon which need to be re-written in the implementation of OGRTriangle.
    // Another reason for such a bloated API is that most of the OGRPolygon functions are implemented directly
    // on top of GEOS; and since GEOS also doesn't provide support for Triangle API, most of the methods need to be
    // modified to ensure against any type mismatches. The following are modified due to GEOS incompatibility or a
    // re-write of methods inherited by OGRPolygon.
    // Of these, the functions which are not virtual in OGRPolygon will be made virtual
    OGRErr addRing	(OGRCurve *poNewRing);
    OGRErr addRingDirectly	(OGRCurve *poNewRing);
    OGRBoolean Crosses (const OGRGeometry *poOtherGeom) const;
    OGRGeometry *ConvexHull	() const;
    OGRBoolean Crosses (const OGRGeometry *poOtherGeom) const;
    OGRGeometry *DelaunayTriangulation (double dfTolerance, int bOnlyEdges) const;
    OGRGeometry *Difference	(const OGRGeometry *poOtherGeom) const;
    OGRBoolean Disjoint	(const OGRGeometry *poOtherGeom) const;
    double Distance	(const OGRGeometry *poOtherGeom) const;
    OGRGeometry *Boundary () const;
    OGRGeometry *Intersection (const OGRGeometry *poOtherGeom)	const;
    OGRBoolean IsValid () const;
    OGRBoolean Overlaps (const OGRGeometry *poOtherGeom) const;
    OGRErr PointOnSurface	(OGRPoint *poPoint) const;
    OGRGeometry *Polygonize () const;
    OGRGeometry *Simplify (double dTolerance)	const;
    OGRGeometry *SimplifyPreserveTopology (double dTolerance) const;
    OGRGeometry *SymDifference	(const OGRGeometry *poOtherGeom) const;
    OGRBoolean Touches (const OGRGeometry *poOtherGeom)	const;
    OGRGeometry *Union (const OGRGeometry *poOtherGeom) const;
    OGRGeometry *UnionCascaded	() const;
    virtual double get_Area() const;
    virtual OGRGeometry* getCurveGeometry(const char *const *papszOptions=NULL) const;
    const char* getGeometryName();
    OGRLinearRing* stealExteriorRing();
    OGRCurve* stealExteriorRingCurve();
};
