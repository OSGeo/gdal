.. _gdal_manage_dataset_copy:

================================================================================
``gdal manage-dataset copy``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Copy files of a dataset

.. Index:: gdal manage-dataset copy

:program:`gdal manage-dataset copy` creates a copy of the dataset file(s),
including potential side-car/associated files.

Synopsis
--------

.. program-output:: gdal manage-dataset copy --help-doc

Options
-------

.. option:: --source <FILENAME>

    Source file name or directory name. Required.

.. option:: --destination <FILENAME>

    Destination file name or directory name. Required.

.. option:: -f, ---format <FORMAT>

    Dataset format. Helps if automatic detection does not work.

.. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Copy a dataset

   .. code-block:: console

       $ gdal manage-dataset copy source.tif destination.tif
