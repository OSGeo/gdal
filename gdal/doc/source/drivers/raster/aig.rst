.. _raster.aig:

================================================================================
AIG -- Arc/Info Binary Grid
================================================================================

.. shortname:: AIG

.. built_in_by_default::

Supported by GDAL for read access. This format is the internal binary
format for Arc/Info Grid, and takes the form of a coverage level
directory in an Arc/Info database. To open the coverage select the
coverage directory, or an .adf file (such as hdr.adf) from within it. If
the directory does not contain file(s) with names like w001001.adf then
it is not a grid coverage.

Support includes reading of an affine georeferencing transform, some
projections, and a color table (.clr) if available.

This driver is implemented based on a reverse engineering of the format.
See the :ref:`raster.arcinfo_grid_format` for more details.

The projections support (read if a prj.adf file is available) is quite
limited. Additional sample prj.adf files may be sent to the maintainer,
warmerdam@pobox.com.

NOTE: Implemented as ``gdal/frmts/aigrid/aigdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

.. toctree::
   :maxdepth: 1
   :hidden:

   arcinfo_grid_format
