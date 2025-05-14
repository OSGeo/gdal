.. _gdal_dataset_rename:

================================================================================
``gdal dataset rename``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Rename files of a dataset

.. Index:: gdal dataset rename

:program:`gdal dataset rename` rename the dataset file(s), including
potential side-car/associated files.

Synopsis
--------

.. program-output:: gdal dataset rename --help-doc

Options
-------

.. option:: --source <FILENAME>

    Source file name or directory name. Required.

.. option:: --destination <FILENAME>

    Destination file name or directory name. Required.

.. option:: -f, ---format <FORMAT>

    Dataset format. Overrides the automatic format detection.

.. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Rename a dataset

   .. code-block:: console

       $ gdal dataset rename old_name.tif new_name.tif
