.. _gdal_driver_icechunk_list_branches:

.. program:: gdal_driver_icechunk_list_branches

================================================================================
``gdal driver icechunk list-branches``
================================================================================

.. versionadded:: 3.14

.. only:: html

    List branches of an Icechunk repository

.. Index:: gdal driver icechunk list-branches

Synopsis
--------

.. We are not using 'program-output:: gdal driver icechunk list-branches --help-doc' on purpose,
   because the Icechunk driver may not be built.

.. code-block::

    Usage: gdal driver icechunk list-branches [OPTIONS] <INPUT>

    List branches of an Icechunk repository

    Positional arguments:
      -i, --input <INPUT>     Input multidimensional raster dataset [required]

    Common Options:
      -h, --help              Display help message and exit
      --json-usage            Display usage as JSON document and exit
      --config <KEY>=<VALUE>  Configuration option [may be repeated]


Description
-----------

List the branches of an :ref:`Icechunk <raster.icechunk>` repository and their last commit message.

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::

   .. code-block:: bash

       gdal driver icechunk list-branches /vsis3/dynamical-ecmwf-aifs-single/ecmwf-aifs-single-forecast/v0.1.0.icechunk --config AWS_NO_SIGN_REQUEST=YES

   Output:

   .. code-block:: json

        [
          { "name": "main", "commit_message": "Update at 2026-06-05T12:22:32Z" }
        ]
