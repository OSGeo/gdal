.. _gdal_driver_icechunk_list_tags:

.. program:: gdal_driver_icechunk_list_tags

================================================================================
``gdal driver icechunk list-tags``
================================================================================

.. versionadded:: 3.14

.. only:: html

    List tags of an Icechunk repository

.. Index:: gdal driver icechunk list-tags

Synopsis
--------

.. We are not using 'program-output:: gdal driver icechunk list-tags --help-doc' on purpose,
   because the Icechunk driver may not be built.

.. code-block::

    Usage: gdal driver icechunk list-tags [OPTIONS] <INPUT>

    List tags of an Icechunk repository

    Positional arguments:
      -i, --input <INPUT>     Input multidimensional raster dataset [required]

    Common Options:
      -h, --help              Display help message and exit
      --json-usage            Display usage as JSON document and exit
      --config <KEY>=<VALUE>  Configuration option [may be repeated]


Description
-----------

List the tags of an :ref:`Icechunk <raster.icechunk>` repository and their last commit message.

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::

   .. code-block:: bash

       gdal driver icechunk list-tags /vsicurl/https://raw.githubusercontent.com/earth-mover/icechunk/refs/heads/main/icechunk-python/tests/data/test-repo-v2/repo

   Output:

   .. code-block:: json

        [
          { "name": "it also works!", "commit_message": "some more structure" },
          { "name": "it works!", "commit_message": "delete a chunk" }
        ]

