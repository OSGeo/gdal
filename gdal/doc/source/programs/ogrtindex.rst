.. _ogrtindex:

================================================================================
ogrtindex
================================================================================

.. only:: html

    Creates a tileindex.

.. Index:: ogrtindex

Synopsis
--------

.. code-block::

    ogrtindex [-lnum n]... [-lname name]... [-f output_format]
              [-write_absolute_path] [-skip_different_projection]
              [-t_srs target_srs]
              [-src_srs_name field_name] [-src_srs_format [AUTO|WKT|EPSG|PROJ]
              [-accept_different_schemas]
              <output_dataset> <src_dataset>...

Description
-----------

:program:`ogrtindex` program can be used to create a tileindex - a file
containing a list of the identities of a bunch of other files along with
their spatial extents. This is primarily intended to be used with
`MapServer <http://mapserver.org/>`__ for tiled access to layers using
the OGR connection type.

.. program:: ogrtindex

.. option:: -lnum <n>

    Add layer number ``n`` from each source file in the tile index.

.. option:: -lname <name>

    Add the layer named ``name`` from each source file in the tile index.

.. option:: -f <output_format>

    Select an output format name. The default is to create a shapefile.

.. option:: -tileindex <field_name>

    The name to use for the dataset name. Defaults to LOCATION.

.. option:: -write_absolute_path

    Filenames are written with absolute paths

.. option:: -skip_different_projection

    Only layers with same projection ref as layers already inserted in
    the tileindex will be inserted.

.. option:: -t_srs <target_srs>

    Extent of input files will be transformed to the desired target
    coordinate reference system. Using this option generates files that
    are not compatible with MapServer < 7.2. Default creates simple
    rectangular polygons in the same coordinate reference system as the
    input vector layers.

    .. versionadded:: 2.2.0

.. option:: -src_srs_name <field_name>

    The name of the field to store the SRS of each tile. This field name
    can be used as the value of the TILESRS keyword in MapServer >= 7.2.

    .. versionadded:: 2.2.0

.. option:: -src_srs_format <format>

    The format in which the SRS of each tile must be written.
    Available formats are: ``AUTO``, ``WKT``, ``EPSG``, ``PROJ``.

    .. versionadded:: 2.2.0

.. option:: -accept_different_schemas

    By default ogrtindex checks that all layers inserted into the index
    have the same attribute schemas. If you specify this option, this
    test will be disabled. Be aware that resulting index may be
    incompatible with MapServer!

If no :option:`-lnum` or :option:`-lname` arguments are given it is assumed
that all layers in source datasets should be added to the tile index as
independent records.

If the tile index already exists it will be appended to, otherwise it
will be created.

Example
-------

This example would create a shapefile (:file:`tindex.shp`) containing
a tile index of the ``BL2000_LINK`` layers in all the NTF files
in the :file:`wrk` directory:

.. code-block::

    ogrtindex tindex.shp wrk/*.NTF 1069148.900,419873.100 1069147.500,419870.200
      1069146.400,419862.100 1069143.000,419860 1069142,419854.900
      1069138.600,419850 1069135,419848.800 1069134.100,419843
      1069130,419836.200 1069127.600,419824.600 1069123.800,419820.200
      1069126.900,419815.500 1069126.900,419808.200 1069116.500,419798.700
      1069117.600,419794.100 1069115.100,419796.300 1069109.100,419801.800
      1069106.800,419805.000  1069107.300)
