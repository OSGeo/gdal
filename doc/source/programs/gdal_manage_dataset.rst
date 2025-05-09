.. _gdal_manage_dataset:

================================================================================
``gdal manage-dataset``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Commands to manage datasets (identify, copy, rename, delete)

.. Index:: gdal manage-dataset

:program:`gdal manage-dataset` can perform various operations on datasets, depending
on the chosen operation. This includes identifying
drivers that can open them and deleting, renaming or copying the files.

Synopsis
--------

.. program-output:: gdal manage-dataset --help-doc


Available sub-commands
----------------------

- :ref:`gdal_manage_dataset_identify`: Identify driver opening dataset(s)
- :ref:`gdal_manage_dataset_copy`: Copy files of a dataset.
- :ref:`gdal_manage_dataset_rename`: Rename files of a dataset.
- :ref:`gdal_manage_dataset_delete`: Delete dataset(s)
