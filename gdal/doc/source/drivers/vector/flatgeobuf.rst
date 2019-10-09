.. _vector.flatgeobuf:

FlatGeobuf
==========

.. versionadded:: 3.1

.. shortname:: ``FlatGeobuf``

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

On creation, passing a filenam without a .fgb suffix will instruct the driver
to create a directory of that name, and create layers as .fgb files in that
directory.

Open options
------------

-  **VERIFY_BUFFERS=**\ *YES/NO*: Set the YES verify buffers when reading.
    This can provide some protection for invalid/corrupt data with a performance
    trade off. Defaults to YES.

Dataset Creation Options
------------------------

None

Layer Creation Options
----------------------

-  **SPATIAL_INDEX=**\ *YES/NO*: Set the YES to create a
   spatial index. Defaults to YES.

Examples
--------

-  Simple translation of a single shapefile into a FlatGeobuf file. The file
   'filename.fgb' will be created with the features from abc.shp and attributes
   from abc.dbf. The file ``filename.fgb`` must **not** already exist,
   as it will be created.

   ::

      % ogr2ogr -f FlatGeobuf filename.fgb abc.shp

-  Conversion of a Geopackage file with multiple layers:

    ::

      % ogr2ogr -f FlatGeobuf my_fgb_dataset input.gpkg

See Also
--------

-  `FlatGeobuf at GitHub <https://github.com/bjornharrtell/flatgeobuf>`__
