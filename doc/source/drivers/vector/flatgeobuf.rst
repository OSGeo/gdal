.. _vector.flatgeobuf:

FlatGeobuf
==========

.. versionadded:: 3.1

.. shortname:: FlatGeobuf

.. built_in_by_default::

This driver implements read/write support for access to features encoded
in `FlatGeobuf <https://github.com/bjornharrtell/flatgeobuf>`__ format, a
performant binary encoding for geographic data based on flatbuffers that
can hold a collection of Simple Features.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Multi layer support
-------------------

A single .fgb file only contains one single layer. For multiple layer support,
it is possible to put several .fgb files in a directory, and use that directory
name as the connection string.

On creation, passing a filename without a .fgb suffix will instruct the driver
to create a directory of that name, and create layers as .fgb files in that
directory.

Open options
------------

-  **VERIFY_BUFFERS=**\ *YES/NO*: Set to YES to verify buffers when reading.
   This can provide some protection for invalid/corrupt data with a performance
   trade off. Defaults to YES.

Dataset Creation Options
------------------------

None

Layer Creation Options
----------------------

-  **SPATIAL_INDEX=**\ *YES/NO*: Set to YES to create a
   spatial index. Defaults to YES.
-  **TEMPORARY_DIR=**\ path: Path to an existing directory where temporary
   files should be created. Only used if SPATIAL_INDEX=YES. If not specified,
   the directory of the output file will be used for regular filenames. For
   other VSI file systems, the temporary directory will be the one decided by
   the :cpp:func:`CPLGenerateTempFilename` function.
   "/vsimem/" can be used for in-memory temporary files.

Examples
--------

-  Simple translation of a single shapefile into a FlatGeobuf file. The file
   'filename.fgb' will be created with the features from abc.shp and attributes
   from abc.dbf. The file ``filename.fgb`` must **not** already exist,
   as it will be created.

   ::

      ogr2ogr -f FlatGeobuf filename.fgb abc.shp

-  Conversion of a Geopackage file with multiple layers:

   ::

      ogr2ogr -f FlatGeobuf my_fgb_dataset input.gpkg

See Also
--------

-  `FlatGeobuf at GitHub <https://github.com/bjornharrtell/flatgeobuf>`__
