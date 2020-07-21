.. _raster_common_options:

================================================================================
Common options for raster programs
================================================================================

All GDAL command line programs support the following common options.

.. option:: --version

    Report the version of GDAL and exit.

.. _raster_common_options_formats:
.. option:: --formats

    List all raster formats supported by this GDAL build (read-only and read-write) and exit. The format support is indicated as follows: 'ro' is read-only driver; 'rw' is read or write (i.e. supports CreateCopy); 'rw+' is read, write and update (i.e. supports Create). A 'v' is appended for formats supporting virtual IO (/vsimem, /vsigzip, /vsizip, etc). A 's' is appended for formats supporting subdatasets. Note: The valid formats for the output of gdalwarp are formats that support the Create() method (marked as rw+), not just the CreateCopy() method.

.. option:: --format <format>

    List detailed information about a single format driver. The format should be the short name reported in the --formats list, such as GTiff.

.. _raster_common_options_optfile:
.. option:: --optfile <filename>

    Read the named file and substitute the contents into the command line options list. Lines beginning with # will be ignored. Multi-word arguments may be kept together with double quotes.

.. option:: --config <key> <value>

    Sets the named configuration keyword to the given value, as opposed to setting them as environment variables. Some common configuration keywords are GDAL_CACHEMAX (memory used internally for caching in megabytes) and :decl_configoption:`GDAL_DATA` (path of the GDAL "data" directory). Individual drivers may be influenced by other :ref:`configuration options <list_config_options>`.

.. option:: --debug <value>

    Control what debugging messages are emitted. A value of ON will enable all debug messages. A value of OFF will disable all debug messages. Another value will select only debug messages containing that string in the debug prefix code.

.. option:: --help-general

    Gives a brief usage message for the generic GDAL command line options and exit.

Creating new files
------------------

Access an existing file to read it is generally quite simple.
Just indicate the name of the file or dataset on the command line.
However, creating a file is more complicated. It may be necessary to
indicate the the format to create, various creation options affecting
how it will be created and perhaps a coordinate system to be assigned.
Many of these options are handled similarly by different GDAL utilities,
and are introduced here.

.. option:: -of <format>

    Select the format to create the new file as. The formats are
    assigned short names such as GTiff (for GeoTIFF) or HFA (for Erdas Imagine).
    The list of all format codes can be listed with the :option:`--formats` switch.
    Only formats list as ``(rw)`` (read-write) can be written.

    .. versionadded:: 2.3

        If not specified, the format is guessed from the extension.
        Previously, it was generally GTiff for raster, or ESRI Shapefile for vector.

.. include:: options/co.rst

.. option:: -a_srs <srs>
.. option:: -s_srs <srs>
.. option:: -t_srs <srs>

    Several utilities (e.g. :command:`gdal_translate` and :command:`gdalwarp`)
    include the ability to specify coordinate systems with command line options
    like :option:`-a_srs` (assign SRS to output), :option:`-s_srs` (source SRS)
    and :option:`-t_srs` (target SRS). These utilities allow the coordinate system
    (SRS = spatial reference system) to be assigned in a variety of formats.

    * ``NAD27|NAD83|WGS84|WGS72``

        These common geographic (lat/long) coordinate
        systems can be used directly by these names.

    * ``EPSG:n``

        Coordinate systems (projected or geographic) can be selected based on their
        EPSG codes. For instance, :samp:`EPSG:27700` is the British National Grid.
        A list of EPSG coordinate systems can be found in the GDAL data files
        :file:`gcs.csv` and :file:`pcs.csv`.

    * ``PROJ.4 definition``

        A PROJ.4 definition string can be used as a coordinate system.
        Take care to keep the proj.4 string together as a single argument to
        the command (usually by double quoting).

        For instance :samp:`+proj=utm +zone=11 +datum=WGS84`.

    * ``OpenGIS Well Known Text``

        The Open GIS Consortium has defined a textual format for describing
        coordinate systems as part of the Simple Features specifications.
        This format is the internal working format for coordinate systems
        used in GDAL. The name of a file containing a WKT coordinate system
        definition may be used a coordinate system argument, or the entire
        coordinate system itself may be used as a command line option (though
        escaping all the quotes in WKT is quite challenging).

    * ``ESRI Well Known Text``

        ESRI uses a slight variation on OGC WKT format in their ArcGIS product
        (ArcGIS :file:`.prj` files), and these may be used in a similar manner
        o WKT files, but the filename should be prefixed with ``ESRI::``.

        For example, `"ESRI::NAD 1927 StatePlane Wyoming West FIPS 4904.prj"`.

    * ``Spatial References from URLs``

        For example http://spatialreference.org/ref/user/north-pacific-albers-conic-equal-area/.

    * :file:`filename`

        File containing WKT, PROJ.4 strings, or XML/GML coordinate
        system definitions can be provided.
