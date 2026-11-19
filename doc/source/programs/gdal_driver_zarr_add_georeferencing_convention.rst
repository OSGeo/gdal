.. _gdal_driver_zarr_add_georeferencing_convention:

.. program:: gdal_driver_zarr_add_georeferencing_convention

================================================================================
``gdal driver zarr add-georeferencing-convention``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Add a georeferencing convention to an existing ZARR dataset

.. Index:: gdal driver zarr add-georeferencing-convention

Synopsis
--------

.. We are not using 'program-output:: gdal driver zarr add-georeferencing-convention --help-doc' on purpose,
   because the Zar driver may not be built.

.. code-block::

    Usage: gdal driver zarr add-georeferencing-convention [OPTIONS] <INPUT> <CONVENTION>

    Add a georeferencing convention to an existing ZARR dataset

    Positional arguments:
      -i, --input <INPUT>        Input multidimensional raster dataset [required]
      --convention <CONVENTION>  Georeferencing convention. CONVENTION=GDAL|spatial_proj [required]

    Common Options:
      -h, --help                 Display help message and exit
      --json-usage               Display usage as JSON document and exit
      --config <KEY>=<VALUE>     Configuration option [may be repeated]

Description
-----------

Add (but not replace) a georeferencing convention to an existing ZARR dataset.

This has only effect on arrays for which georeferencing is recognized.

This can been used for example to add the Zarr `spatial <https://github.com/zarr-conventions/spatial>`__
and `geo-proj <https://github.com/zarr-conventions/geo-proj>`__ conventions
by specifying :option:`--convention` to ``spatial_proj``.

Program-Specific Options
------------------------

.. option:: --convention GDAL|spatial_proj

    Name of the georeferencing convention to add.

    ``GDAL`` uses a ``_CRS`` attribute (cf :ref:`raster_zarr.gdal.convention`).

    ``spatial_proj`` uses the Zarr `spatial <https://github.com/zarr-conventions/spatial>`__
    and `geo-proj <https://github.com/zarr-conventions/geo-proj>`__ conventions.


.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
    :title: Add the ``spatial_proj`` convention.

   .. code-block:: bash

       gdal driver zarr add-georeferencing-conventions my.zarr spatial_proj
