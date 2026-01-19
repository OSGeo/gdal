.. _gdal_dataset_check:

================================================================================
``gdal dataset check``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Check whether there are errors when reading the content of a dataset.

.. Index:: gdal dataset check

:program:`gdal dataset check` check whether there are errors when reading the content
of a dataset and return 1 as the program exit code (or in the ``return-code`` output parameter
of the algorithm) if that happens, or 0 when no errors are detected.

.. warning::

    Most drivers or data formats do not have a built-in integrity mechanism.
    Thus a successful run of that program does not necessarily mean that the dataset
    is identical to the state it had just after being generated. The checks performed
    are generally sufficient to detect whether the dataset is truncated compared to
    its expected size, but this is not even necessarily guaranteed for some formats
    (for example the CSV driver may not be able to detect truncation).


Synopsis
--------

.. program-output:: gdal dataset check --help-doc

Options
-------

.. option:: --input <FILENAME>

    Input vector, raster or multidimensional dataset. Required.

Examples
--------

.. example::
   :title: Check whether there are errors when reading a dataset

   .. code-block:: console

       $ gdal dataset check NE1_50M_SR_W.tif
