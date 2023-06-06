.. _raster.isg:

================================================================================
ISG -- International Service for the Geoid
================================================================================

.. versionadded:: 3.1

.. shortname:: ISG

.. built_in_by_default::

Supports reading grids in the International Service for the Geoid text format, used
for number of geoid models at
http://www.isgeoid.polimi.it/Geoid/reg_list.html

Format specification is at https://www.isgeoid.polimi.it/Geoid/format_specs.html

NOTE: Implemented as :source_file:`frmts/aaigrid/aaigriddataset.cpp`.

Driver capabilities
-------------------

.. supports_georeferencing::

.. note::

    WGS84 will always be arbitrarily reported as the interpolation CRS of the
    grid. Consult grid documentation for exact CRS to apply.

.. supports_virtualio::
