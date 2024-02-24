.. _rfc-99:

===================================================================
RFC 99: Geometry coordinate precision
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2024-Feb-24
Status:        In development
Target:        GDAL 3.9
============== =============================================

Summary
-------

This RFC aims at introducing optional metadata to specify the coordinate
precision of geometries, to be able to round appropriately coordinates and limit
the number of decimals when exporting to text-based formats or nullify
least-significant bits for binary formats. That metadata will be stored into
and read from formats that can support it.

Motivation
----------

The aim is multiple:

- reducing file size. For text-based formats, rounding and truncating to the
  specified precision directly reduce file size. For binary formats, using that
  information to zero least-significant bits can increase the potential when
  applying afterwards lossless compression methods (typically zipping a file).

- presenting the user with hints on the precision of the data he/she accesses.
  This can be used by user interfaces build on top of GDAL to display geometry
  coordinates with an appropriate number of decimals.

- a few drivers (GeoJSON, JSONFG, OpenFileGDB) have layer creation options to
  specify coordinate precision, but there is currently no driver agnostic way
  of specifying it.

For example, currently, when exporting a file to GML, 15 significant decimal
digits (ie the total of digits for the integral and decimal parts) are used,
which corresponds to a 0.1 micrometre precision for geography coordinates.
The same holds for regular GeoJSON export, unless the RFC 7946 variant is
selected, in which case only 7 decimal digits after decimal separators are used.
However this is a layer creation option, which means that it is no longer
remembered when data is edited/appended to an existing layer
(see https://github.com/qgis/QGIS/issues/56335).

For binary formats using IEEE-754 double-precision encoding of real numbers,
one can show that at least the last 16 least-significants bits (ie the last
2 bytes of 8) of a coordinate can be set to zero while keeping a 1 mm precision
(which corresponds to about 8.9e-9 degree).
On a test dataset, setting a 1 mm precision reduced the size of the .zip of the
.gpkg file from 766 MB to 667 MB (13% size decrease).
If only a 1 metre precision is wished, this increases to 26 useless least-significant bits.

.. code-block:: python

    >>> import math
    >>> earth_radius_in_metre = 6378137
    >>> one_degree_in_metre = earth_radius_in_metre * math.pi / 180.0
    >>> mm_prec_in_degree = 1e-3 / one_degree_in_metre
    >>> print(mm_prec_in_degree)
    8.983152841195215e-09
    >>> max_integer_part = 180  # for coordinates in range [-180,180]
    >>> significant_bits_needed = math.ceil(math.log2(max_integer_part)) + math.ceil(math.log2(1 / mm_prec_in_degree)) + 1
    >>> mantissa_bit_size = 52
    >>> unused_bits = mantissa_bit_size - significant_bits_needed
    >>> print(unused_bits)
    16


C and C++ API extensions and changes
------------------------------------

A new ``OGRGeomCoordinatePrecision`` class is introduced:

.. code-block:: c++

    /** Geometry coordinate precision.
     *
     * This may affect how many decimal digits (for text-based output) or bits
     * (for binary encodings) are used to encode geometries.
     *
     * It is important to note that the coordinate precision has no direct
     * relationship with the "physical" accuracy. It is generally advised that
     * the resolution (precision) be at least 10 times smaller than the accuracy.
     */
    struct CPL_DLL OGRGeomCoordinatePrecision
    {
        static constexpr double UNKNOWN = 0;

        /** Resolution for the coordinate precision of the X and Y coordinates.
         * Expressed in the units of the X and Y axis of the SRS.
         * For example for a projected SRS with X,Y axis unit in metre, a value
         * of 1e-3 corresponds to a 1 mm precision.
         * For a geographic SRS (on Earth) with axis unit in degree, a value
         * of 8.9e-9 (degree) also corresponds to a 1 mm precision.
         * Set to 0 if unknown.
         */
        double dfXYResolution = UNKNOWN;

        /** Resolution for the coordinate precision of the Z coordinate.
         * Expressed in the units of the Z axis of the SRS.
         * Set to 0 if unknown.
         */
        double dfZResolution = UNKNOWN;

        /** Resolution for the coordinate precision of the M coordinate.
         * Set to 0 if unknown.
         */
        double dfMResolution = UNKNOWN;

        /** Map from a format name to a list of format specific options.
         *
         * This can be for example used to store FileGeodatabase
         * xytolerance, xorigin, yorigin, etc. coordinate precision grids
         * options, which can be help to maximize preservation of coordinates in
         * FileGDB -> FileGDB conversion processes.
         */
        std::map<std::string, CPLStringList> oFormatSpecificOptions{};

        /** Helper to specify X,Y,Z precision in metre.
         * Note: the stored values in the structure will be converted from their
         * specified values in metre to the equivalent values expressed in the
         * SRS units.
         */
        void SetFromMetres(const OGRSpatialReference *poSRS,
                           double dfXYMetreResolution,
                           double dfZMetreResolution, double dfMResolution);
    }


The existing :cpp:class:`OGRGeomFieldDefn` is extented with a new
OGRGeomCoordinatePrecision member, and associated getter and setter methods.

.. code-block:: c++

    class OGRGeomFieldDefn
    {
        public:
            const OGRGeomCoordinatePrecision& GetCoordinatePrecision() const;

            void SetCoordinatePrecision(const OGRGeomCoordinatePrecision& prec);

        private:
            OGRGeomCoordinatePrecision m_oCoordPrecision{};
    };


Corresponding additions at the C API level:

.. code-block:: c

    typedef struct OGRGeomCoordinatePrecision *OGRGeomCoordinatePrecisionH;

    OGRGeomCoordinatePrecisionH CPL_DLL OGRGeomCoordinatePrecisionCreate(void);
    void CPL_DLL OGRGeomCoordinatePrecisionDestroy(OGRGeomCoordinatePrecisionH);
    double CPL_DLL OGRGeomCoordinatePrecisionGetXYResolution(OGRGeomCoordinatePrecisionH);
    double CPL_DLL OGRGeomCoordinatePrecisionGetZResolution(OGRGeomCoordinatePrecisionH);
    double CPL_DLL OGRGeomCoordinatePrecisionGetMResolution(OGRGeomCoordinatePrecisionH);
    void CPL_DLL OGRGeomCoordinatePrecisionSet(OGRGeomCoordinatePrecisionH,
                                               double dfXYResolution,
                                               double dfZResolution,
                                               double dfMResolution);
    void CPL_DLL OGRGeomCoordinatePrecisionSetFromMetric(OGRGeomCoordinatePrecisionH,
                                                       OGRSpatialReferenceH hSRS,
                                                       double dfXYMetricResolution,
                                                       double dfZMetricResolution,
                                                       double dfMResolution);

    const OGRGeomCoordinatePrecisionH CPL_DLL OGR_GFld_GetCoordinatePrecision(OGRGeomFieldDefnH);
    void CPL_DLL OGR_GFld_SetCoordinatePrecision(OGRGeomFieldDefnH, const OGRGeomCoordinatePrecisionH);


WKB export methods will be modified in a similar way as in the prototype
https://github.com/OSGeo/gdal/pull/6974 to nullify least significant bits from
the precision specifications.

The signature of the current :cpp:func:`OGRFieldDefn::ICreateLayer()` protected
method (implemented by drivers) will be changed from

.. code-block:: c++

    virtual OGRLayer *ICreateLayer(
            const char *pszName, const OGRSpatialReference *poSpatialRef = nullptr,
            OGRwkbGeometryType eGType = wkbUnknown, char **papszOptions = nullptr);

to

.. code-block:: c++

    virtual OGRLayer *ICreateLayer(
            const char *pszName,
            const OGRGeomFieldDefn* poGeomFieldDefn = nullptr,
            CSLConstList papszOptions = nullptr);

This will require changes to out-of-tree drivers that implement it.

A corresponding non-virtual public method will also be added:

.. code-block:: c++

    OGRLayer *CreateLayer(
            const char *pszName,
            const OGRGeomFieldDefn* poGeomFieldDefn = nullptr,
            CSLConstList papszOptions = nullptr);

And the current CreateLayer() signature will be adapted to call the modified
ICreateLayer().

And for the C API:

.. code-block:: c

    OGRLayerH CPL_DLL GDALDatasetCreateLayerEx(GDALDatasetH, const char *,
                                               OGRGeomFieldDefnH hGeomFieldDefn,
                                               CSLConstList);


A new ``GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION`` driver capability will be added
to advertize that a driver honours OGRGeomFieldDefn::GetCoordinatePrecision()
when writing geometries. This may be useul for user interfaces that could offer
an option to the user to specify the coordinate precision. Note however that
the driver may not be able to store that precision in the dataset metadata.

Driver changes
--------------

The following drivers will be modified to honour ``GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION``

GeoJSON
+++++++

The driver will compute the number of decimal digits after the decimal point
to write as ``ceil(1. / resolution)``

The driver will be able to store and retrieve the coordinate precision metadata
in the files it generates, by adding ``xy_coordinate_resolution`` and
``z_coordinate_resolution`` members at the FeatureCollection level.

The existing COORDINATE_PRECISION layer creation option, if specified, will
take precedence over the settings coming from OGRGeomFieldDefn::GetCoordinatePrecision().

GeoJSONSeq
++++++++++

The driver will compute the number of decimal digits after the decimal point
to write as ``ceil(1. / resolution)``

It will *not* be able to store it in its metadata.

JSONFG
++++++

Similar to GeoJSON. One subtelty is that this driver may write both the "place"
geometry (generally in a non-WGS84 CRS) and the GeoJSON RFC7946 WGS84 "geometry".

The OGRGeomFieldDefn::GetCoordinatePrecision() will qualify the "place" geometry.
The coordinate precision of the WGS84 "geometry" will be derived from the one
of the "place" geometry with appropriate geographic/projected CRS and axis unit
changes.

The coordinate precision metadata of the "place" member will be stored in
``xy_coordinate_resolution_place`` and ``z_coordinate_resolution_place``
members at the FeatureCollection level.

For the "geometry" member, the same ``xy_coordinate_resolution`` and
``z_coordinate_resolution`` members as the GeoJSON driver will be used.

The existing COORDINATE_PRECISION_PLACE or COORDINATE_PRECISION_GEOMETRY layer
creation option, if specified, will take precedence over the settings coming
from OGRGeomFieldDefn::GetCoordinatePrecision().

GML
+++

The driver will compute the number of decimal digits after the decimal point
to write as ``ceil(1. / resolution)``

The driver will be able to store the coordinate precision metadata in the XML
schema it generates by adding a ``xs:annotation/xs:appinfo`` element in the
declaration of the geometry property, and with ``ogr:xy_coordinate_resolution``,
``ogr:z_coordinate_resolution`` and ``ogr:m_coordinate_resolution`` sub-elements.
This should hopefully be ignored by readers that don't recognize
that metadata (this will be the case of GDAL < 3.9)

.. code-block:: xml

        <xs:element name="wkb_geometry" type="gml:SurfacePropertyType" nillable="true" minOccurs="0" maxOccurs="1">
            <xs:annotation>
              <xs:appinfo source="http://ogr.maptools.org/">
                <ogr:xy_coordinate_resolution>8.9e-9</ogr:xy_coordinate_resolution>
                <ogr:z_coordinate_resolution>1e-3</ogr:z_coordinate_resolution>
                <ogr:m_coordinate_resolution>1e-3</ogr:m_coordinate_resolution>
              </xs:appinfo>
            </xs:annotation>
        </xs:element>

CSV
+++

The driver will compute the number of decimal digits after the decimal point
to write as ``ceil(1. / resolution)``

It will *not* be able to store it in its metadata. The possibility of storing
the coordinate metadata in the .csvt side-car file has been considered, but it
would not be backwards-compatible.

GeoPackage
++++++++++

The driver will use the resolution to nullify useless least-significant bits
in its binary-based geometry (WKB-based) binary encoding.

And it will be able to store and retrieve the coordinate precision metadata in
the files it generates, by adding a ``<CoordinatePrecision>`` sub-element to the
``<GDALMultiDomainMetadata>`` metadata.
That ``<CoordinatePrecision>`` sub-element will be ignored by GDAL < 3.9.

.. code-block:: sql

    INSERT INTO gpkg_metadata VALUES(1,'dataset','http://gdal.org','text/xml',
        '<GDALMultiDomainMetadata><CoordinatePrecision geometry_column="geom" xy_resolution="8.9e-9" z_resolution="1e-3" m_resolution="1e-3"></CoordinatePrecision></GDALMultiDomainMetadata>');
    INSERT INTO gpkg_metadata_reference VALUES('table','poly',NULL,NULL,'2023-10-22T21:13:43.282Z',1,NULL);

OpenFileGDB
+++++++++++

OGRGeomCoordinatePrecision::dfXYResolution (resp. dfZResolution, dfMResolution)
directly map to 1. / xyscale (resp. 1 / zscale, 1 / mscale) in the declaration
of the coordinate grid precision options of the FileGeodatabase format
(cf https://help.arcgis.com/en/sdk/10.0/java_ao_adf/conceptualhelp/engine/index.html#//00010000037m000000).

Consequently the OpenFileGDB driver can be modified in reading and writing to
fully honour OGRGeomCoordinatePrecision.

The driver will also get and set other coordinate grid precision options, such
as the origin and tolerance, values in the
``OGRGeomCoordinatePrecision::oFormatSpecificOptions`` member.

The existing ``XYSCALE``, ``ZSCALE`` and ``MSCALE`` layer creation options,
if specified, will take precedence over the settings coming from
OGRGeomFieldDefn::GetCoordinatePrecision().


Utilities
---------

ogrinfo
+++++++

ogrinfo will be modified to honour OGRGeomCoordinatePrecision when outputing
WKT geometries (or GeoJSON geometries for the -json output)

ogr2ogr
+++++++

ogr2ogr will forward by default the OGRGeomCoordinatePrecision of the input
layer to the output layer, but of course it will only have effects for drivers
honouring ``GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION``.

When reprojection occurs, the coordinate precision will be adjusted to take into
account geographic vs projected CRS changes and unit changes.

The following options will be added:

- ``-xyRes <val>``: XY coordinate resolution. Nominally in the unit of the X and
  Y SRS axis.
  Appending a ``m``, ``mm`` or ``deg`` suffix will be also supported.
  A warning will be emitted if the user specifies this option when creating a
  new layer for a driver that does not advertize
  ``GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION``.

- ``-zRes <val>``: Z coordinate resolution. Nominally in the unit of the Z SRS
  axis. Appending a ``m`` or ``mm`` suffix will be also supported.

- ``-mRes <val>``: M coordinate resolution.

- ``-unsetCoordPrecision``: to disable automatic propagation of the input
  coordinate precision to the output.

Out of scope
------------

While there is an obvious logical connection with GEOS' PrecisionModel
(https://libgeos.org/doxygen/classgeos_1_1geom_1_1PrecisionModel.html),
this RFC does not tie the introduced OGR coordinate precision metadata with it.
Tying both would require either adding a reference to a
OGRGeomCoordinatePrecision as a member of the OGRGeometry class (which would
have some extra RAM usage implications), or as a parameter in OGRGeometry GEOS
related methods.

Quantization of raster pixel values (e.g. the ``DISCARD_LSB`` creation option
of the GeoTIFF driver) is also slightly connected.

Backward compatibility
----------------------

The C and C++ API are extended.

The change of the ICreateLayer() virtual method is an ABI change, and will
require source code changes to out-of-tree drivers implementing it.

MIGRATION_GUIDE.TXT will mention that and point to this RFC.

Design discussion
-----------------

This paragraph discusses a number of thoughts that arose during the writing of
this RFC but were not kept.

While changing ICreateLayer() prototype, which requires the tedious process of
changing it in more than 50 drivers, I've also considered introducing
an additional OGRLayerCreationContext argument, but I've decided against if,
as it is unclear if it would be that useful. For example, in most ogr2ogr
scenarios, the final extent and feature count is unknown at the start of the
process.

.. code-block:: c++

    struct OGRLayerCreationContext
    {
        OGRExtent3D sExtent;
        int64_t     nFeatureCount;
    }

    OGRLayer *ICreateLayer(
            const char *pszName, const OGRGeomFieldDefn* poFieldDefn = nullptr,
            const OGRLayerCreationContext& sContext = OGRLayerCreationContext(),
            CSLConstList papszOptions = nullptr);


Related issues and PRs
----------------------

- A prior implementation with a different and reduced scope was done last year
  in https://github.com/OSGeo/gdal/pull/6974.
  Tthe GeoPackage driver specific creation options of this pull request will no
  longer be needed in the implementation of this RFC.

- https://github.com/qgis/QGIS/issues/56335

Voting history
--------------

TBD
