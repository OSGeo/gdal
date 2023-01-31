.. _rfc-92:

=============================================================
RFC 92: WKB Only geometries
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault at spatialys.com
Started:       2023-Jan-31
Status:        Draft / discussion
Target:        GDAL 3.7 ?
============== =============================================

Summary
-------

This RFC provides shortcuts to avoid instantiation of full OGRGeometry instances
in scenarios where only the WKB representation of geometries is needed. The
hope is to save CPU time.

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
2. instanciate the relevant OGRGeometry subclass instance (OGRPoint, OGRLineString,
   OGRPolygon, etc.) from WKB
3. store it in a OGRFeature

QGIS side:

4. get the OGRGeometry from the OGRFeature
5. asks for its WKB representation, build from the subclass members (not the
   original WKB of step 2.)
6. instanciate QgsGeometry from WKB

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
