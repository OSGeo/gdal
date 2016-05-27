// TO BE INCORPORATED IN ogr_geometry.h
// Included in an external file for the sake of readability

class CPL_DLL OGROGRTriangle : public OGRPolygon
{
  private:
    virtual int checkRing( OGRCurve * poNewRing ) const;            // done
    OGRErr addRingDirectlyInternal( OGRCurve* poCurve, int bNeedRealloc );  //done

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
    virtual int WkbSize() const;    // done
    virtual OGRErr importFromWkb(unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc);    // done
    virtual OGRErr exportToWkb(OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc) const;     // done
    virtual OGRErr importFromWkt(char **);  // done
    virtual OGRErr exportToWkt(char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc) const;  // done

    virtual void empty() = 0;
    virtual OGRGeometry *clone(); // done
    virtual void getEnvelope(OGREnvelope * psEnvelope) const = 0;
    virtual void getEnvelope(OGREnvelope3D * psEnvelope) const = 0;

    // Need to throw an error if these are interfaced via OGRPolyhedralSurface + make them virtual in OGRGeometry
    virtual static GEOSContextHandle_t createGEOSContext(); // done
    virtual static void freeGEOSContext(GEOSContextHandle_t hGEOSCtxt);  // done
    virtual GEOSGeom exportToGEOS(GEOSContextHandle_t hGEOSCtxt) const CPL_WARN_UNUSED_RESULT;  // done

    // New methods interfaced through SFCGAL
    virtual OGRGeometry *Boundary();    // done
    virtual double Distance(const OGRGeometry *poOtherGeom) const;  // done
    virtual double Distance3D(const OGRGeometry *poOtherGeom) const;    // done
    void Reverse();

    // Methods inherited from OGRPolygon which need to be re-written in the implementation of OGRTriangle.
    // Another reason for such a bloated API is that most of the OGRPolygon functions are implemented directly
    // on top of GEOS; and since GEOS also doesn't provide support for Triangle API, most of the methods need to be
    // modified to ensure against any type mismatches. The following are modified due to GEOS incompatibility or a
    // re-write of methods inherited by OGRPolygon.
    // Of these, the functions which are not virtual in OGRPolygon will be made virtual
    virtual OGRErr addRing	(OGRCurve *poNewRing); // done
    virtual OGRErr addRingDirectly	(OGRCurve *poNewRing); // done
    virtual OGRBoolean Crosses (const OGRGeometry *poOtherGeom) const;   // done
    virtual OGRGeometry *ConvexHull() const CPL_WARN_UNUSED_RESULT;   // done
    virtual OGRGeometry *DelaunayTriangulation(double dfTolerance, int bOnlyEdges) const CPL_WARN_UNUSED_RESULT; // done
    virtual OGRGeometry *Difference( const OGRGeometry * ) const CPL_WARN_UNUSED_RESULT; // done
    OGRBoolean Disjoint	(const OGRGeometry *poOtherGeom) const; // done
    virtual OGRGeometry *Intersection( const OGRGeometry *) const CPL_WARN_UNUSED_RESULT; // done
    virtual OGRBoolean  IsValid() const;    // done
    virtual OGRBoolean  Overlaps( const OGRGeometry * ) const;  // done
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
