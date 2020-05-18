.. _raster.marfa:

================================================================================
MRF -- Meta Raster Format
================================================================================

.. shortname:: MRF

.. versionadded:: 2.1

.. built_in_by_default::

Access to a indexed heap of regular tiles (blocks). Controlled by an xml
file, usually organized as a pyramid of overviews, with level zero being
the full resolution image. None, PNG, JPEG, ZLIB tile packing are
implemented

For file creation options, see "gdalinfo --format MRF"

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Links
-----

-  `MRF User Guide <https://github.com/nasa-gibs/mrf/blob/master/doc/MUG.md>`__
-  `MRF Specification <https://github.com/nasa-gibs/mrf/blob/master/spec/mrf_spec.md>`__
-  `Source repository nasa-gibs/mrf <https://github.com/nasa-gibs/mrf>`__
