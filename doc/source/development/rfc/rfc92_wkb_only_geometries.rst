.. _rfc-92:

=============================================================
RFC 92: WKB Only geometries (on hold)
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault at spatialys.com
Started:       2023-Jan-31
Status:        On hold
Target:        N/A
============== =============================================

Summary
-------

This RFC provides shortcuts to avoid instantiation of full OGRGeometry instances
in scenarios where only the WKB representation of geometries is needed. The
hope is to save CPU time.

.. note:: This RFC is on hold.

Motivation
----------

Some consumers of the OGR feature API only use the WKB representation of
the geometry. This is typically the case in the QGIS OGR provider (a QGIS
"provider" ~= a GDAL driver) which creates the corresponding QgsGeometry
instance from WKB.

The format of geometries of some popular formats like GeoPackage or PostGIS is
based on WKB. Consequently the chain of processings done by QGIS reading GeoPackage
is:

GDAL side:

1. acquire GeoPackage geometry blob (WKB with a GeoPackage specific header) from
   the database
2. instantiate the relevant OGRGeometry subclass instance (OGRPoint, OGRLineString,
   OGRPolygon, etc.) from WKB
3. store it in a OGRFeature

QGIS side:

4. get the OGRGeometry from the OGRFeature
5. asks for its WKB representation, build from the subclass members (not the
   original WKB of step 2.)
6. instantiate QgsGeometry from WKB

One can see that if we were able to store the original WKB representation and
get it back we could save OGRGeometry subclass object creation, destruction and
WKB deserialization and serialization.

:ref:`rfc-86` has seen related performance boosts since ArrowArray batches store
the WKB representation of geometries and not OGRGeometry instances.
This RFC borrows the same idea, but applying it to the traditional feature API,
as switching to ArrowArray API is a significant undertaking for GDAL users.

That could also be used in the future for a generic implementation of a potential
OGRLayer::WriteArrowArray() method that would build temporary OGRFeature objects
from the rows of the array to call the ICreateFeature() implementation: the temporary
geometries could be OGRWKBOnlyGeometry instances for drivers such as GeoPackage
that use WKB natively.

Technical details
-----------------

The proposal mostly contains in adding a new subclass of OGRGeometry called
`OGRWKBOnlyGeometry`

.. code-block:: c++

    /**
     * Special OGRGeometry subclass that only holds its WKB representation.
     *
     * Used for optimizations when passing geometries between drivers or
     * application code that does not require to query details of the geometry,
     * such as structure in sub-components or vertex coordinates.
     */
    class CPL_DLL OGRWKBOnlyGeometry final: public OGRGeometry
    {
        std::vector<GByte> m_abyWKB{};
        OGREnvelope        m_sEnvelope{}; // optional

    public:
        OGRWKBOnlyGeometry(const void* pabyWKB, size_t nWKBSize);
        OGRWKBOnlyGeometry(const void* pabyWKB, size_t nWKBSize, const OGREnvelope& sEnvelope);

        /** Get WKB */
        const std::vector<GByte>& Wkb() const { return m_abyWKB; }

        /** Return a "real" OGRGeometry instantiated from the WKB */
        std::unique_ptr<OGRGeometry> Materialize() const;

        /** Returns the equivalent of Materialize()->getGeometryType() without
         * materializing */
        OGRwkbGeometryType getUnderlyingGeometryType() const override;

        /** Returns envelope stored at construction time, or "quickly"
         * determined by inspecting the WKB content */
        void getEnvelope(OGREnvelope *psEnvelope) const override;

        // Dummy implementation of all pure virtual methods of OGRGeometry
        // ==> all return an error
        // Typically getGeometryType() returns wkbUnknown, to avoid user code
        // to wrongly cast to a OGRPoint/OGRLineString/etc. instance.
    };


A new method is added to the OGRLayer class:

.. code-block:: c++

    /** If bRequestWKBOnlyGeometries is true, then the driver should return, in
     *  GetNextFeature(), geometries that are instance of OGRWKBOnlyGeometry.
     *  Only drivers for which TestCapability(OLCReadWKBGeometries) is true
     *  are capable of this. Other drivers will error out.
     */
    virtual OGRErr RequestWKBOnlyGeometries(bool bRequestWKBOnlyGeometries);

Two new capabilities are added at the OGRLayer level:

* OLCReadWKBGeometries: a layer must return TRUE for it when the layer can
  honour RequestWKBOnlyGeometries(true)
* OLCWriteWKBGeometries: a layer must return TRUE for it if its CreateFeature()
  and SetFeature() implementations support being passed OGRWKBOnlyGeometry
  instances.

ogr2ogr is modified to call RequestWKBOnlyGeometries(true) on the source layer:

* if the source layer advertises OLCReadWKBGeometries
* if the target layer advertises OLCWriteWKBGeometries
* if no command line switch requires a "materialized" geometry.

So basically this is for requests like
"ogr2ogr out.gpkg in.gpkg [layer or SQL request] [attribute filter] [spatial filter]"

To be noted that while bounding box intersection in the case of GeoPackage is
done at the SQLite RTree level, the GeoPackage driver currently does a "client-side"
post filtering using GEOSIntersects() (in situations where bounding box analysis
only cannot conclude), so geometry materialization is done in OGRLayer::FilterGeometry()
for a subset of features.

C API
-----

OGR_L_RequestWKBOnlyGeometries() is added.

Backward compatibility
----------------------

No issue. Only API additions

Benchmarking
------------

bench_ogr_c_api
~~~~~~~~~~~~~~~

The `bench_ogr_c_api <https://github.com/OSGeo/gdal/blob/master/perftests/bench_ogr_c_api.cpp>`_
benchmark utility which uses the C API to iterate over features and get their WKB
representation is enhanced with a `-wkb_only_geometry` switch to call
OGR_L_RequestWKBOnlyGeometries().

On a 1.6 GB GeoPackage (nz-building-outlines.gpkg) made of 3.2 million features
with simple polygons (typically quadrilaterals, building footprints) and 13
attributes:

- bench_ogr_c_api runs in 6.4 s
- bench_ogr_c_api -wkb_only_geometry runs in 5.0 s

==> 22% faster

Other synthetic benchmarks show that the maximum speed-up is about 30% on a
dataset with 10 millions polygonal features of 10 points each.

Conversely, the gain is much more modest, or close to null, with just a few
thousands of features that hold larger geometries (several thousands of points
each).

The gain is more in saving instantiation of OGRPolygon and OGRLinearRing
objects that in the size of their coordinate set.

ogr2ogr
~~~~~~~

::

    ogr2ogr /vsimem/out.gpkg nz-building-outlines.gpkg -lco spatial_index=no


runs in 15.8 second in WKBOnlyGeometry mode vs 19.1 second without it (master),
hence a 17% speed-up.

With spatial index creation enabled (multi-threaded), the wall clock time
difference is within measurement noise. And for singe threaded creation, the
WKBOnlyGeometry mode is 5% faster.

Discussion
----------

Is it a good idea... ?

The design of OGRWKBOnlyGeometry is admittedly a bit clunky, or at least at odds
with other OGRGeometry subclasses, but nothing more elegant, concise, performant
and that doesn't change the whole OGRGeometry API and driver implementations
comes to mind.

The scope is limited to a few drivers: GeoPackage, PostGIS (but the current
throughput of the driver is probably not sufficient for OGRGeometry overhead to
be noticeable), what else?

Should methods of OGRWKBOnlyGeometry that cannot work without materialization
of the real geometry return an error like done currently, or do the materialization
on-the-fly when needed ? The motivation for erroring out is to avoid silent
performance issues related to materialization.

Points raised during discussion
-------------------------------

Sean Gillies: Wouldn't it be possible for all OGRFeatures to carry WKB data by
default and add a method to provide it to callers?

Even: That involve massive code rewrites in all drivers and wouldn't be desirable
from a performance point of view, because most drivers can't generate WKB easily
(PostGIS and GPKG are the exceptions rather the norm). So either all other drivers
should be modified to compose WKB at hand (massive coding effort. Probably
several weeks of effort and significant risk of regressions). Or get it from the
ExportToWkb() method of the OGRGeometry instance they currently build, but then
you pay the price in memory and CPU time to generate WKB that might not be
consumed by users.

Sean Gillies: And only construct an OGRGeometry when it's asked for? Such as when
GetGeometryRef is called?

Even: we could both make GetGeometryRef() and GetGeomFieldRef() virtual methods
whose default implementation would be the same as currently, ie. return the value
of the corresponding member variable in the base OGRFeature class stored with
SetGeometry[Directly]()/SetGeomField[Directly]()

And add a new virtual method:

virtual GByte* OGRFeature::GetWKBGeometry(int iGeomField, size_t* pnOutSize) const

whose default implementation would just use GetGeomFieldRef(iGeomField)->ExportToWkb().

The few drivers that can provide a more efficient implementation (GPKG typically)
would create a derived class OGRFeatureGPKG with a specific implementation of
those new virtual methods to avoid systematic OGRGeometry instantiation. The only
drawback I see is that making GetGeometryRef() and GetGeomFieldRef() virtual would
have a slight performance impact, but probably small enough.

Dan Baston: I'm wondering about a more broad application of this. Would it be
helpful to have the ability to lazy-initialize an OGRGeometry from multiple
source types such as WKB and GEOS, initially storing only a reference to the
external data in WKB/GEOS/etc and actually materializing the geometry when
required? Then methods such as OGRGeometry::exportToWkb and
OGRGeometry::exportToGEOS could check the external data type and use it directly
if it is compatible, avoiding materialization. This would avoid multiple
conversions to/from GEOS in cases where operations are chained, as well as
allowing WKB to pass directly between input and output drivers that support it.
Relatedly, this ability could be used to cache external-format data when it is
generated for an OGRGeometry, avoiding inefficiencies such as two conversions
to GEOS when checking to see if two geometries intersect before calculating
their intersection.

Even: That's definitely something doable. At a minimum, you would have to
inspect the top geometry type to instantiate the appropriate OGRGeometry
subclass, and then its members could be lazy initialized, but that means that
all methods of OGRGeometry and its subclasses would have to do a check whether
the object has been fully initialized. There might be performance implications
for people doing for example lineString->getX(idx) to iterate on big geometries,
although branch predictors of modern CPUs are probably very good at repeatedly
evaluating stuff like "if (!materialized) materialize();". The main drawback is
that is a substantial & risky change that requires to revisit *all* methods of
the geometry classes. For setters, you would also have to make sure to invalidate
the potentially initial WKB / GEOS source.

Issues / pull requests
----------------------

https://github.com/OSGeo/gdal/compare/master...rouault:gdal:rfc92_implementation?expand=1
contains a preliminary candidate implementation.

Not all subtleties have been taken into account in the prototype implementation
(like doing OGRSQL and requesting the OGR_GEOMETRY special attribute).

OGRLayer::FilterGeometry() (used for spatial filter evaluation by GetNextFeature())
uses OGRWKBOnlyGeometry::Materialize() for convenience currently. This could be
improved for geometry types that are directly compatible of GEOS to pass directly
the WKB to GEOS.

Voting history
--------------

TBD
