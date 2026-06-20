.. _raster.icechunk:

================================================================================
Icechunk
================================================================================

.. versionadded:: 3.14

.. shortname:: Icechunk

.. build_dependencies:: libzstd and ZARR driver

`Icechunk <https://icechunk.io/en/stable/overview>`__ is an open-source
transactional storage engine for :ref:`raster.zarr`. Icechunk enables using
Zarr as a true database for array data.

The GDAL Icechunk driver supports datasets using Icechunk v1 and v2 storage
format, hosted on local or cloud storage (see :ref:`virtual_file_systems`).

It supports:

- "inline chunk references": Zarr chunks are stored directly in a "manifest" file.

- "native chunk references": chunks stored in a file under the :file:`chunks`
  sub-directory of an Icechunk repository

- and "virtual chunk references": chunk content is not directly stored in the
  Icechunk repository. A URL to a remote file, with an offset and length in it,
  is stored instead. This is a mechanism typically used to expose netCDF/HDF5
  files as Zarr, without duplicating actual pixel data. The URL of virtual
  references is automatically morphed into GDAL VSI prefixes (i.e.
  ``https://`` --> ``/vsicurl/https://``, ``s3://`` --> ``/vsis3/``, etc.)


Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_multidimensional::

.. supports_virtualio::


Dataset connection name
-----------------------

The nominal dataset connection name, as expected by :cpp:func:`GDALOpenEx`
or command line utilities, is the Icechunk directory name, under which the
:file:`repo` file and :file:`snapshots`, :file:`manifests` and :file:`transactions`
directory are found.

By default, GDAL opens the "main" branch of an Icechunk repository. It is
possible to select:

* an alternate branch by prepending ``ICECHUNK:`` and suffixing
  ``?branch=<branch-name>`` to the Icechunk directory

* a given tag by prepending ``ICECHUNK:`` and suffixing
  ``?tag=<tag-name>`` to the Icechunk directory

The ``ignore-timestamp-etag=yes`` key-value pair can also be appended to ignore
timestamp and ETag checks for chunk files.

It is also possible to point directly at the :file:`repo` file itself, or
prefix the connection name, typically when it is a directory, with ``ICECHUNK:``
to avoid any other driver's identification logic to trigger.

Listing branch and tags
-----------------------

See :ref:`gdal_driver_icechunk_list_branches` and :ref:`gdal_driver_icechunk_list_tags`.


``/vsiicechunk/`` virtual file system
-------------------------------------

When opening an Icechunk dataset, the Icechunk driver essentially prepends
``/vsiicechunk/{`` to the dataset name and appends ``}`` before passing this new
name to the Zarr driver.

It is possible to directly use ``/vsiicechunk/`` filenames for low-level inspection
of an Icechunk repository, e.g. with :ref:`gdal_vsi_list`.

::

    gdal vsi ls /vsiicechunk/{/vsis3/dynamical-ecmwf-aifs-single/ecmwf-aifs-single-forecast/v0.1.0.icechunk}

outputs:

::

    zarr.json
    dew_point_temperature_2m
    downward_long_wave_radiation_flux_surface
    downward_short_wave_radiation_flux_surface
    expected_forecast_length
    geopotential_height_500hpa
    geopotential_height_850hpa
    geopotential_height_925hpa
    ingested_forecast_length
    [...]


Examples
--------

* Listing content of an Icechunk repository:

  ::

        gdal mdim info /vsis3/dynamical-ecmwf-aifs-single/ecmwf-aifs-single-forecast/v0.1.0.icechunk --config AWS_NO_SIGN_REQUEST=YES

* Getting the branches of an Icechunk repository:

  ::

        gdal driver icechunk list-branches /vsis3/dynamical-ecmwf-aifs-single/ecmwf-aifs-single-forecast/v0.1.0.icechunk --config AWS_NO_SIGN_REQUEST=YES

  Output:

   .. code-block:: json

        [
          { "name": "main", "commit_message": "Update at 2026-06-05T12:22:32Z" }
        ]


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Icechunk
    prepends
