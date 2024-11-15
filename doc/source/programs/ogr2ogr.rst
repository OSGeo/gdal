.. _ogr2ogr:

================================================================================
ogr2ogr
================================================================================

.. only:: html

    Converts simple features data between file formats.

.. Index:: ogr2ogr

Synopsis
--------

.. code-block::

    ogr2ogr [--help] [--long-usage] [--help-general]
            [-of <output_format>]
            [-dsco <NAME>=<VALUE>]... [-lco <NAME>=<VALUE>]...
            [[-append]|[-upsert]|[-overwrite]]
            [-update] [-sql <statement>|@<filename>] [-dialect <dialect>]
            [-spat <xmin> <ymin> <xmax> <ymax>]
            [-where <restricted_where>|@<filename>] [-select <field_list>]
            [-nln <name>] [-nlt <type>]...
            [-s_srs <srs_def>]
            [[-a_srs <srs_def>]|[-t_srs <srs_def>]]
            <dst_dataset_name> <src_dataset_name> [<layer_name>]...

    Field related options:
           [-addfields] [-relaxedFieldNameMatch]
           [-fieldTypeToString All|<type1>[,<type2>]...]
           [-mapFieldType <srctype>|All=<dsttype>[,<srctype2>=<dsttype2>]...]
           [-fieldmap <field_1>[,<field_2>]...]
           [-splitlistfields] [-maxsubfields <n>] [-emptyStrAsNull]
           [-forceNullable] [-unsetFieldWidth]
           [-unsetDefault] [-resolveDomains]
           [-dateTimeTo UTC|UTC(+|-)<HH>|UTC(+|-)<HH>:<MM>] [-noNativeData]

    Advanced geometry and SRS related options:
           [-dim layer_dim|2|XY|3|XYZ|XYM|XYZM]
           [-s_coord_epoch <epoch>] [-a_coord_epoch <epoch>]
           [-t_coord_epoch <epoch>] [-ct <pipeline_def>]
           [-spat_srs <srs_def>] [-geomfield <name>]
           [-segmentize <max_dist>] [-simplify <tolerance>]
           [-makevalid] [-skipinvalid]
           [-wrapdateline] [-datelineoffset <val_in_degree>]
           [-clipsrc [<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>|spat_extent]
           [-clipsrcsql <sql_statement>] [-clipsrclayer <layername>]
           [-clipsrcwhere <expression>]
           [-clipdst [<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>]
           [-clipdstsql <sql_statement>] [-clipdstlayer <layername>]
           [-clipdstwhere <expression>]
           [-explodecollections] [-zfield <name>]
           [-gcp <ungeoref_x> <ungeoref_y> <georef_x> <georef_y> [<elevation>]]...
           [-tps] [-order 1|2|3]
           [-xyRes <val>[ m|mm|deg]] [-zRes <val>[ m|mm]] [-mRes <val>]
           [-unsetCoordPrecision]

    Other options:
           [--quiet] [-progress] [-if <format>]...
           [-oo <NAME>=<VALUE>]... [-doo <NAME>=<VALUE>]...
           [-fid <FID>] [-preserve_fid] [-unsetFid]
           [[-skipfailures]|[-gt <n>|unlimited]]
           [-limit <nb_features>] [-ds_transaction]
           [-mo <NAME>=<VALUE>]... [-nomd]

Description
-----------

:program:`ogr2ogr` can be used to convert simple features data between file
formats. It can also perform various operations during the process, such as
spatial or attribute selection, reducing the set of attributes, setting the
output coordinate system or even reprojecting the features during translation.

.. program:: ogr2ogr

.. include:: options/help_and_help_general.rst

.. include:: options/if.rst

.. option:: -of <format_name>, -f <format_name>

    Output file format name, e.g. ``ESRI Shapefile``, ``MapInfo File``,
    ``PostgreSQL``.  Starting with GDAL 2.3, if not specified, the format is
    guessed from the extension (previously was ESRI Shapefile).

.. option:: -append

    Append to existing layer instead of creating new. This option also enables
    :option:`-update`.

.. option:: -upsert

    .. versionadded:: 3.6

    Variant of :option:`-append` where the :cpp:func:`OGRLayer::UpsertFeature`
    operation is used to insert or update features instead of appending with
    :cpp:func:`OGRLayer::CreateFeature`.

    This is currently implemented only in a few drivers:
    :ref:`vector.gpkg` and :ref:`vector.mongodbv3`.

    The upsert operation uses the FID of the input feature, when it is set
    and is a "significant" (that is the FID column name is not the empty string),
    as the key to update existing features. It is crucial to make sure that
    the FID in the source and target layers are consistent.

    For the GPKG driver, it is also possible to upsert features whose FID is unset
    or non-significant (:option:`-unsetFid` can be used to ignore the FID from
    the source feature), when there is a UNIQUE column that is not the
    integer primary key.

.. option:: -overwrite

    Delete the output layer and recreate it empty

.. option:: -update

    Open existing output datasource in update mode rather than trying to create
    a new one

.. option:: -select <field_list>

    Comma-delimited list of fields from input layer to copy to the new layer.

    Starting with GDAL 3.9, field names with spaces, commas or double-quote
    should be surrounded with a starting and ending double-quote character, and
    double-quote characters in a field name should be escaped with backslash.

    Depending on the shell used, this might require further quoting. For example,
    to select ``regular_field``, ``a_field_with space, and comma`` and
    ``a field with " double quote`` with a Unix shell:

    .. code-block:: bash

        -select "regular_field,\"a_field_with space, and comma\",\"a field with \\\" double quote\""

    A field is only selected once, even if mentioned several times in the list
    and if the input layer has duplicate field names.

    Geometry fields can also be specified in the list.

    All fields are selected when -select is not specified. Specifying the
    empty string can be used to disable selecting any attribute field, and only
    keep geometries.

    Note this setting cannot be used together with ``-append``. To control the
    selection of fields when appending to a layer, use ``-fieldmap`` or ``-sql``.

.. option:: -progress

    Display progress on terminal. Only works if input layers have the "fast
    feature count" capability.

.. option:: -sql <sql_statement>|@<filename>

    SQL statement to execute. The resulting table/layer will be saved to the
    output. Starting with GDAL 2.1, the ``@filename`` syntax can be used to
    indicate that the content is in the pointed filename. (Cannot be used with :option:`-spat_srs`.)

.. option:: -dialect <dialect>

    SQL dialect. In some cases can be used to use the (unoptimized) :ref:`ogr_sql_dialect` instead
    of the native SQL of an RDBMS by passing the ``OGRSQL`` dialect value.
    The :ref:`sql_sqlite_dialect` dialect can be chosen with the ``SQLITE``
    and ``INDIRECT_SQLITE`` dialect values, and this can be used with any datasource.

.. option:: -where <restricted_where>

    Attribute query (like SQL WHERE). Starting with GDAL 2.1, the ``@filename``
    syntax can be used to indicate that the content is in the pointed filename.

.. option:: -skipfailures

    Continue after a failure, skipping the failed feature.

.. option:: -spat <xmin> <ymin> <xmax> <ymax>

    spatial query extents, in the SRS of the source layer(s) (or the one
    specified with ``-spat_srs``). Only features whose geometry intersects the
    extents will be selected. The geometries will not be clipped unless
    ``-clipsrc`` is specified.

.. option:: -spat_srs <srs_def>

    Override spatial filter SRS. (Cannot be used with :option:`-sql`.)

.. option:: -geomfield <field>

    Name of the geometry field on which the spatial filter operates on.

.. option:: -dsco <NAME>=<VALUE>

    Dataset creation option (format specific)

.. option:: -lco <NAME>=<VALUE>

    Layer creation option (format specific)

.. option:: -nln <name>

    Assign an alternate name to the new layer

.. option:: -nlt <type>

    Define the geometry type for the created layer. One of ``NONE``,
    ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``,
    ``GEOMETRYCOLLECTION``, ``MULTIPOINT``, ``MULTIPOLYGON``,
    ``MULTILINESTRING``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
    ``CURVEPOLYGON``, ``MULTICURVE``, and ``MULTISURFACE`` non-linear geometry
    types. Add ``Z``, ``M``, or ``ZM`` to the type name to specify coordinates
    with elevation, measure, or elevation and measure. ``PROMOTE_TO_MULTI`` can
    be used to automatically promote layers that mix polygon or multipolygons
    to multipolygons, and layers that mix linestrings or multilinestrings to
    multilinestrings. Can be useful when converting shapefiles to PostGIS and
    other target drivers that implement strict checks for geometry types.
    ``CONVERT_TO_LINEAR`` can be used to to convert non-linear geometry types
    into linear geometry types by approximating them, and ``CONVERT_TO_CURVE`` to
    promote a non-linear type to its generalized curve type (``POLYGON`` to
    ``CURVEPOLYGON``, ``MULTIPOLYGON`` to ``MULTISURFACE``, ``LINESTRING`` to
    ``COMPOUNDCURVE``, ``MULTILINESTRING`` to ``MULTICURVE``). Starting with
    version 2.1 the type can be defined as measured ("25D" remains as an alias for
    single "Z"). Some forced geometry conversions may result in invalid
    geometries, for example when forcing conversion of multi-part multipolygons
    with ``-nlt POLYGON``, the resulting polygon will break the Simple Features
    rules.

    Starting with GDAL 3.0.5, ``-nlt CONVERT_TO_LINEAR`` and ``-nlt PROMOTE_TO_MULTI``
    can be used simultaneously.

.. option:: -dim <val>

    Force the coordinate dimension to val (valid values are ``XY``, ``XYZ``,
    ``XYM``, and ``XYZM`` - for backwards compatibility ``2`` is an alias for
    ``XY`` and ``3`` is an alias for ``XYZ``). This affects both the layer
    geometry type, and feature geometries. The value can be set to ``layer_dim``
    to instruct feature geometries to be promoted to the coordinate dimension
    declared by the layer. Support for M was added in GDAL 2.1.

.. option:: -a_srs <srs_def>

    Assign an output SRS, but without reprojecting (use :option:`-t_srs`
    to reproject)

    .. include:: options/srs_def.rst

.. option:: -a_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the output SRS. Useful when the
    output SRS is a dynamic CRS. Only taken into account if :option:`-a_srs`
    is used.

.. option:: -t_srs <srs_def>

    Reproject/transform to this SRS on output, and assign it as output SRS.

    A source SRS must be available for reprojection to occur. The source SRS
    will be by default the one found in the source layer when it is available,
    or as overridden by the user with :option:`-s_srs`

    .. include:: options/srs_def.rst

.. option:: -t_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the output SRS. Useful when the
    output SRS is a dynamic CRS. Only taken into account if :option:`-t_srs`
    is used. It is also mutually exclusive with  :option:`-a_coord_epoch`.

    Before PROJ 9.4, :option:`-s_coord_epoch` and :option:`-t_coord_epoch` were
    mutually exclusive, due to lack of support for transformations between two dynamic CRS.

.. option:: -s_srs <srs_def>

    Override source SRS. If not specified the SRS found in the input layer will
    be used. This option has only an effect if used together with :option:`-t_srs`
    to reproject.

    .. include:: options/srs_def.rst

.. option:: -xyRes "<val>[ m|mm|deg]"

    .. versionadded:: 3.9

    Set/override the geometry X/Y coordinate resolution. If only a numeric value
    is specified, it is assumed to be expressed in the units of the target SRS.
    The m, mm or deg suffixes can be specified to indicate that the value must be
    interpreted as being in meter, millimeter or degree.

    When specifying this option, the :cpp:func:`OGRGeometry::SetPrecision`
    method is run on geometries (that are not curves) before passing them to the
    output driver, to avoid generating invalid geometries due to the potentially
    reduced precision (unless the :config:`OGR_APPLY_GEOM_SET_PRECISION`
    configuration option is set to ``NO``)

    If neither this option nor :option:`-unsetCoordPrecision` are specified, the
    coordinate resolution of the source layer, if available, is used.

.. option:: -zRes "<val>[ m|mm]"

    .. versionadded:: 3.9

    Set/override the geometry Z coordinate resolution. If only a numeric value
    is specified, it is assumed to be expressed in the units of the target SRS.
    The m or mm suffixes can be specified to indicate that the value must be
    interpreted as being in meter or millimeter.
    If neither this option nor :option:`-unsetCoordPrecision` are specified, the
    coordinate resolution of the source layer, if available, is used.

.. option:: -mRes <val>

    .. versionadded:: 3.9

    Set/override the geometry M coordinate resolution.
    If neither this option nor :option:`-unsetCoordPrecision` are specified, the
    coordinate resolution of the source layer, if available, is used.

.. option:: -unsetCoordPrecision

    .. versionadded:: 3.9

    Prevent the geometry coordinate resolution from being set on target layer(s).

.. option:: -s_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the source SRS. Useful when the
    source SRS is a dynamic CRS. Only taken into account if :option:`-s_srs`
    is used.

    Before PROJ 9.4, :option:`-s_coord_epoch` and :option:`-t_coord_epoch` were
    mutually exclusive, due to lack of support for transformations between two dynamic CRS.

.. option:: -ct <string>

    A PROJ string (single step operation or multiple step string starting with
    +proj=pipeline), a WKT2 string describing a CoordinateOperation, or a
    urn:ogc:def:coordinateOperation:EPSG::XXXX URN overriding the default
    transformation from the source to the target CRS.

    It must take into account the axis order of the source and target CRS, that
    is typically include a ``step proj=axisswap order=2,1`` at the beginning of
    the pipeline if the source CRS has northing/easting axis order, and/or at
    the end of the pipeline if the target CRS has northing/easting axis order.

    .. versionadded:: 3.0

.. option:: -preserve_fid

    Use the FID of the source features instead of letting the output driver
    automatically assign a new one (for formats that require a FID). If not
    in append mode, this behavior is the default if the output driver has
    a FID layer creation option, in which case the name of the source FID
    column will be used and source feature IDs will be attempted to be
    preserved. This behavior can be disabled by setting ``-unsetFid``.
    This option is not compatible with ``-explodecollections``.

.. option:: -fid <fid>

    If provided, only the feature with the specified feature id will be
    processed.  Operates exclusive of the spatial or attribute queries. Note: if
    you want to select several features based on their feature id, you can also
    use the fact the 'fid' is a special field recognized by OGR SQL. So,
    `-where "fid in (1,3,5)"` would select features 1, 3 and 5.

.. option:: -limit <nb_features>

    Limit the number of features per layer.

.. option:: -oo <NAME>=<VALUE>

    Input dataset open option (format specific).

.. option:: -doo <NAME>=<VALUE>

    Destination dataset open option (format specific), only valid in -update mode.

.. option:: -gt <n>

    Group n features per transaction (default 100 000). Increase the value for
    better performance when writing into DBMS drivers that have transaction
    support. ``n`` can be set to unlimited to load the data into a single
    transaction.

.. option:: -ds_transaction

    Force the use of a dataset level transaction (for drivers that support such
    mechanism), especially for drivers such as FileGDB that only support
    dataset level transaction in emulation mode.

.. option:: -clipsrc [<xmin> <ymin> <xmax> <ymax>]|WKT|<datasource>|spat_extent

    Clip geometries (before potential reprojection) to one of the following:

    * the specified bounding box (expressed in source SRS)
    * a WKT geometry (POLYGON or MULTIPOLYGON expressed in source SRS)
    * one or more geometries selected from a datasource
    * the spatial extent of the -spat option if you use the spat_extent keyword.

    When specifying a datasource, you will generally want to use -clipsrc in
    combination of the -clipsrclayer, -clipsrcwhere or -clipsrcsql options.

.. option:: -clipsrcsql <sql_statement>

    Select desired geometries from the source clip datasource using an SQL query.

.. option:: -clipsrclayer <layername>

    Select the named layer from the source clip datasource.

.. option:: -clipsrcwhere <expression>

    Restrict desired geometries from the source clip layer based on an attribute query.

.. option:: -clipdst [<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>

    Clip geometries (after potential reprojection) to one of the following:

    * the specified bounding box (expressed in destination SRS)
    * a WKT geometry (POLYGON or MULTIPOLYGON expressed in destination SRS)
    * one or more geometries selected from a datasource

    When specifying a datasource, you will generally want to use -clipdst in
    combination with the -clipdstlayer, -clipdstwhere or -clipdstsql options.

.. option:: -clipdstsql <sql_statement>

    Select desired geometries from the destination clip datasource using an SQL query.

.. option:: -clipdstlayer <layername>

    Select the named layer from the destination clip datasource.

.. option:: -clipdstwhere <expression>

    Restrict desired geometries from the destination clip layer based on an attribute query.

.. option:: -wrapdateline

    Split geometries crossing the dateline meridian (long. = +/- 180deg)

.. option:: -datelineoffset

    Offset from dateline in degrees (default long. = +/- 10deg, geometries
    within 170deg to -170deg will be split)

.. option:: -simplify <tolerance>

    Distance tolerance for simplification. Note: the algorithm used preserves
    topology per feature, in particular for polygon geometries, but not for a
    whole layer.

    The specified value of this option is the tolerance used to merge
    consecutive points of the output geometry using the
    :cpp:func:`OGRGeometry::Simplify` method
    The unit of the distance is in
    georeferenced units of the source vector dataset.
    This option is applied before the reprojection implied by :option:`-t_srs`

.. option:: -segmentize <max_dist>

    The specified value of this option is the maximum distance between two
    consecutive points of the output geometry before intermediate points are added.
    The unit of the distance is georeferenced units of the source raster.
    This option is applied before the reprojection implied by :option:`-t_srs`

.. option:: -makevalid

    Run the :cpp:func:`OGRGeometry::MakeValid` operation, followed by
    :cpp:func:`OGRGeometryFactory::removeLowerDimensionSubGeoms`, on geometries
    to ensure they are valid regarding the rules of the Simple Features specification.

    .. versionadded: 3.1 (requires GEOS)

.. option:: -skipinvalid

    Run the :cpp:func:`OGRGeometry::IsValid` operation on geometries to check if
    they are valid regarding the rules of the Simple Features specification.
    If they are not, the feature is skipped. This check is done after all other
    geometry operations.

    .. versionadded: 3.10 (requires GEOS)

.. option:: -fieldTypeToString All|<type1>[,<type2>]...

    Converts any field of the specified type to a field of type string in the
    destination layer. Valid types are : ``Integer``, ``Integer64``, ``Real``, ``String``,
    ``Date``, ``Time``, ``DateTime``, ``Binary``, ``IntegerList``, ``Integer64List``, ``RealList``,
    ``StringList``. Special value ``All`` can be used to convert all fields to strings.
    This is an alternate way to using the CAST operator of OGR SQL, that may
    avoid typing a long SQL query. Note that this does not influence the field
    types used by the source driver, and is only an afterwards conversion.
    Also note that this option is without effects on fields whose presence and
    type is hard-coded in the output driver (e.g KML, GPX)

.. option:: -mapFieldType {<srctype>|All=<dsttype>[,<srctype2>=<dsttype2>]...}

    Converts any field of the specified type to another type. Valid types are :
    ``Integer``, ``Integer64``, ``Real``, ``String``,
    ``Date``, ``Time``, ``DateTime``, ``Binary``, ``IntegerList``, ``Integer64List``, ``RealList``,
    ``StringList``. Types can also include
    subtype between parenthesis, such as ``Integer(Boolean)``, ``Real(Float32)``, ...
    Special value ``All`` can be used to convert all fields to another type. This
    is an alternate way to using the CAST operator of OGR SQL, that may avoid
    typing a long SQL query. This is a generalization of -fieldTypeToString.
    Note that this does not influence the field types used by the source
    driver, and is only an afterwards conversion.
    Also note that this option is without effects on fields whose presence and
    type is hard-coded in the output driver (e.g KML, GPX)

.. option:: -dateTimeTo {UTC|UTC(+|-)<HH>|UTC(+|-)<HH>:<MM>}

    .. versionadded: 3.7

    Converts date time values from the timezone specified in the source value
    to the target timezone expressed with :option:`-dateTimeTo`.
    Datetime whose timezone is unknown or localtime are not modified.

    HH must be in the [0,14] range and MM=00, 15, 30 or 45.

.. option:: -unsetFieldWidth

    Set field width and precision to 0.

.. option:: -splitlistfields

    Split fields of type StringList, RealList or IntegerList into as many
    fields of type String, Real or Integer as necessary.

.. option:: -maxsubfields <val>

    To be combined with ``-splitlistfields`` to limit the number of subfields
    created for each split field.

.. option:: -explodecollections

    Produce one feature for each geometry in any kind of geometry collection in
    the source file, applied after any ``-sql`` option. This options is not
    compatible with ``-preserve_fid`` but ``-sql "SELECT fid AS original_fid, * FROM ..."``
    can be used to store the original FID if needed.

.. option:: -zfield <field_name>

    Uses the specified field to fill the Z coordinate of geometries.

.. option:: -gcp <ungeoref_x> <ungeoref_y> <georef_x> <georef_y> [<elevation>]

    Use the indicated ground control point to compute a coordinate transformation.
    The transformation method can be selected by specifying the :option:`-order`
    or :option:`-tps` options.
    Note that unlike raster tools such as gdal_edit or gdal_translate, GCPs
    are not added to the output dataset.
    This option may be provided multiple times to provide a set of GCPs (at
    least 2 GCPs are needed).

.. option:: -order <n>

    Order of polynomial used for warping (1 to 3). The default is to select a
    polynomial order based on the number of GCPs.

.. option:: -tps

    Force use of thin plate spline transformer based on available GCPs.

.. option:: -fieldmap

    Specifies the list of field indexes to be copied from the source to the
    destination. The (n)th value specified in the list is the index of the
    field in the target layer definition in which the n(th) field of the source
    layer must be copied. Index count starts at zero. To omit a field, specify
    a value of -1. There must be exactly as many values in the list as the
    count of the fields in the source layer. We can use the 'identity' setting
    to specify that the fields should be transferred by using the same order.
    This setting should be used along with the ``-append`` setting.

.. option:: -addfields

    This is a specialized version of ``-append``. Contrary to ``-append``,
    ``-addfields`` has the effect of adding, to existing target layers, the new
    fields found in source layers. This option is useful when merging files
    that have non-strictly identical structures. This might not work for output
    formats that don't support adding fields to existing non-empty layers. Note
    that if you plan to use -addfields, you may need to combine it with
    -forceNullable, including for the initial import.

.. option:: -relaxedFieldNameMatch

    Do field name matching between source and existing target layer in a more
    relaxed way if the target driver has an implementation for it.

.. option:: -forceNullable

    Do not propagate not-nullable constraints to target layer if they exist in
    source layer.

.. option:: -unsetDefault

    Do not propagate default field values to target layer if they exist in
    source layer.

.. option:: -unsetFid

    Can be specified to prevent the name of the source FID column and source
    feature IDs from being re-used for the target layer. This option can for
    example be useful if selecting source features with a ORDER BY clause.

.. option:: -emptyStrAsNull

    .. versionadded:: 3.3

    Treat empty string values as null.

.. option:: -resolveDomains

    .. versionadded:: 3.3

    When this is specified, any selected field that is linked to a coded field
    domain will be accompanied by an additional field (``{dstfield}_resolved``),
    that will contain the description of the coded value.

.. option:: -nomd

    To disable copying of metadata from source dataset and layers into target
    dataset and layers, when supported by output driver.

.. option:: -mo <META-TAG>=<VALUE>

    Passes a metadata key and value to set on the output dataset, when
    supported by output driver.

.. option:: -noNativeData

    To disable copying of native data, i.e. details of source format not
    captured by OGR abstraction, that are otherwise preserved by some drivers
    (like GeoJSON) when converting to same format.

    .. versionadded:: 2.1

.. option:: <dst_dataset_name>

    Output dataset name.

.. option:: <src_dataset_name>

    Source dataset name.

.. option:: <layer_name>

    One or more source layer names to copy to the output dataset. If no layer
    names are passed, then all source layers are copied.


Performance Hints
-----------------

When writing into transactional DBMS (SQLite/PostgreSQL,MySQL, etc...), it
might be beneficial to increase the number of INSERT statements executed
between BEGIN TRANSACTION and COMMIT TRANSACTION statements. This number is
specified with the -gt option. For example, for SQLite, explicitly defining -gt
65536 ensures optimal performance while populating some table containing many
hundreds of thousands or millions of rows. However, note that -skipfailures
overrides -gt and sets the size of transactions to 1.

For PostgreSQL, the :config:`PG_USE_COPY` config option can be set to YES for a
significant insertion performance boost. See the PG driver documentation page.

More generally, consult the documentation page of the input and output drivers
for performance hints.

Known issues
------------

Starting with GDAL 3.8, ogr2ogr uses internally an Arrow array based API
(cf :ref:`rfc-86`) for some source formats (in particular GeoPackage or FlatGeoBuf),
and for the most basic types of operations, to improve performance.
This substantial change in the ogr2ogr internal logic has required a number of
fixes throughout the GDAL 3.8.x bugfix releases to fully stabilize it, and we believe
most issues are resolved with GDAL 3.9.
If you hit errors not met with earlier GDAL versions, you may specify
``--config OGR2OGR_USE_ARROW_API NO`` on the ogr2ogr command line to opt for the
classic algorithm using an iterative feature based approach. If that flag is
needed with GDAL >= 3.9, please file an issue on the
`GDAL issue tracker <https://github.com/OSGeo/gdal/issues>`__.

C API
-----

This utility is also callable from C with :cpp:func:`GDALVectorTranslate`.

.. versionadded::2.1

Examples
--------

* Basic conversion from Shapefile to GeoPackage:

    .. code-block:: bash

      ogr2ogr output.gpkg input.shp

* Change the coordinate reference system from ``EPSG:4326`` to ``EPSG:3857``:

    .. code-block:: bash

      ogr2ogr -s_srs EPSG:4326 -t_srs EPSG:3857 output.gpkg input.gpkg

* Example appending to an existing layer:

    .. code-block:: bash

        ogr2ogr -append -f PostgreSQL PG:dbname=warmerda abc.tab

* Clip input layer with a bounding box (<xmin> <ymin> <xmax> <ymax>):

    .. code-block:: bash

      ogr2ogr -spat -13.931 34.886 46.23 74.12 output.gpkg natural_earth_vector.gpkg

* Filter Features by a ``-where`` clause:

    .. code-block:: bash

      ogr2ogr -where "\"POP_EST\" < 1000000" \
        output.gpkg natural_earth_vector.gpkg ne_10m_admin_0_countries


More examples are given in the individual format pages.

Advanced examples
-----------------

* Reprojecting from ETRS_1989_LAEA_52N_10E to EPSG:4326 and clipping to a bounding box:

    .. code-block:: bash

        ogr2ogr -wrapdateline -t_srs EPSG:4326 -clipdst -5 40 15 55 france_4326.shp europe_laea.shp

* Using the ``-fieldmap`` setting. The first field of the source layer is
  used to fill the third field (index 2 = third field) of the target layer, the
  second field of the source layer is ignored, the third field of the source
  layer used to fill the fifth field of the target layer.

    .. code-block:: bash

        ogr2ogr -append -fieldmap 2,-1,4 dst.shp src.shp

* Outputting geometries with the CSV driver.

  By default, this driver does not preserve geometries on layer creation by
  default. An explicit layer creation option is needed:

    .. code-block:: bash

        ogr2ogr -lco GEOMETRY=AS_XYZ TrackWaypoint.csv TrackWaypoint.kml

* Extracting only geometries.

  There are different situations, depending if the input layer has a named geometry
  column, or not. First check, with ogrinfo if there is a reported geometry column.

    .. code-block:: bash

        ogrinfo -so CadNSDI.gdb.zip PLSSPoint | grep 'Geometry Column'
        Geometry Column = SHAPE

  In that situation where the input format is a FileGeodatabase, it is called SHAPE
  and can thus be referenced directly in a SELECT statement.

    .. code-block:: bash

        ogr2ogr -sql "SELECT SHAPE FROM PLSSPoint" \
          -lco GEOMETRY=AS_XY -f CSV /vsistdout/ CadNSDI.gdb.zip

  For a shapefile with a unnamed geometry column, ``_ogr_geometry_`` can be used as
  a special name to designate the implicit geometry column, when using the default
  :ref:`OGR SQL <ogr_sql_dialect>` dialect. The name begins with
  an underscore and SQL syntax requires that it must appear between double quotes.
  In addition the command line interpreter may require that double quotes are
  escaped and the final SELECT statement could look like:

    .. code-block:: bash

        ogr2ogr -sql "SELECT \"_ogr_geometry_\" FROM PLSSPoint" \
          -lco GEOMETRY=AS_XY -f CSV /vsistdout/ CadNSDI.shp

  If using the :ref:`SQL SQLite <sql_sqlite_dialect>` dialect, the special geometry
  name is ``geometry`` when the source geometry column has no name.

    .. code-block:: bash

        ogr2ogr -sql "SELECT geometry FROM PLSSPoint" -dialect SQLite \
          -lco GEOMETRY=AS_XY -f CSV /vsistdout/ CadNSDI.shp
