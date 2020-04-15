.. _raster.ace2:

================================================================================
ACE2 -- ACE2
================================================================================

.. shortname:: ACE2

.. built_in_by_default::

This is a convenience driver to read ACE2 DEMs. Those files contain raw
binary data. The georeferencing is entirely determined by the filename.
Quality, source and confidence layers are of Int16 type, whereas
elevation data is returned as Float32.

`ACE2 product
overview <http://tethys.eaprs.cse.dmu.ac.uk/ACE2/shared/overview>`__

NOTE: Implemented as ``gdal/frmts/raw/ace2dataset.cpp``.


Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
