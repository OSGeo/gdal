.. _raster.r:

================================================================================
R -- R Object Data Store
================================================================================

.. shortname:: R

.. built_in_by_default::

The R Object File Format is supported for write access, and limited read
access by GDAL. This format is the native format R uses for objects
saved with the *save* command and loaded with the *load* command. GDAL
supports writing a dataset as an array object in this format, and
supports reading files with simple rasters in essentially the same
organization. It will not read most R object files.

Currently there is no support for reading or writing georeferencing
information.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Creation Options
----------------

-  .. oo:: ASCII
      :choices: YES, NO
      :default: NO

      Produce an ASCII formatted file, instead of binary,
      if set to YES.

-  .. oo:: COMPRESS
      :choices: YES, NO
      :default: YES

      Produces a compressed file if YES, otherwise an
      uncompressed file.

See Also:

-  `R Project <http://www.r-project.org/>`__
