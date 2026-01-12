.. _gdal_dataset:

================================================================================
``gdal dataset``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Commands to manage datasets (identify, copy, rename, delete)

.. Index:: gdal dataset

:program:`gdal dataset` can perform various operations on data
files, depending on the chosen operation. This includes identifying
data types and deleting, renaming or copying the files.

Synopsis
--------

.. program-output:: gdal dataset --help-doc


Available sub-commands
----------------------

- :ref:`gdal_dataset_identify`: Identify driver opening dataset(s)
- :ref:`gdal_dataset_check`: Check whether there are errors when reading the content of a dataset.
- :ref:`gdal_dataset_copy`: Copy files of a dataset.
- :ref:`gdal_dataset_rename`: Rename files of a dataset.
- :ref:`gdal_dataset_delete`: Delete dataset(s)
