.. _gdal_vsi_move:

================================================================================
``gdal vsi move``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Move/rename a file/directory located on GDAL Virtual System Interface (VSI)

.. Index:: gdal vsi move

Synopsis
--------

.. program-output:: gdal vsi move --help-doc

Description
-----------

:program:`gdal vsi move` move files and directories located on :ref:`virtual_file_systems`.

If the destination path is an existing directory, the file will be moved to it.

It can move files and directories between different virtual file systems,
but this will involve copying and deletion.

Note that for cloud object storage, moving/renaming a directory may involve
renaming all files it contains recursively, and is thus not an atomic
operation (and could be slow and expensive on directories with many files!)

This is implemented by :cpp:func:`VSIMove`.

This is an analog of the UNIX ``mv`` command, and ``gdal vsi mv`` is an
alias for ``gdal vsi move``.

Examples
--------

.. example::
   :title: Rename a file within the same virtual file system

   .. code-block:: console

       $ gdal vsi move /vsis3/bucket/my.tif /vsis3/bucket/new_name.tif

.. example::
   :title: Move a file into another directory within the same virtual file system

   .. code-block:: console

       $ gdal vsi move /vsis3/bucket/my.tif /vsis3/bucket/existing_subdir

.. example::
   :title: Move a directory between two different virtual file systems

   .. code-block:: console

       $ gdal vsi move /vsis3/bucket/my_directory /vsigs/bucket/
