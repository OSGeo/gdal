.. _raster.cphd:

================================================================================
CPHD - Compensated Phase History Data
================================================================================

.. shortname:: CPHD

.. versionadded:: 3.13

GDAL can read CPHD datasets using the `RAWDataset` driver.

Driver capabilities
-------------------

.. supports_virtualio::

Open options
------------

The following open options exist:

-  .. oo:: INCLUDE_XML
      :choices: YES, NO

      Whether to include CPHD XML in the output, default is YES.

Multidimensional API support
----------------------------

The CPHD driver supports the :ref:`multidim_raster_data_model` for reading operations.

The driver supports:
- reading groups and subgroups
- reading multidimensional dense arrays with a numeric data type
- reading numeric or string attributes in groups and arrays

See Also
--------

- `CPHD v1.1.0 standard <https://nsgreg.nga.mil/doc/view?i=5388>`__
- `CPHD v1.1.0 XML schema <https://nsgreg.nga.mil/doc/view?i=5421>`__

