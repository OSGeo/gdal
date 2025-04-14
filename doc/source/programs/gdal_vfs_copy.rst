.. _gdal_vfs_copy_subcommand:

================================================================================
"gdal vfs copy" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Copy files located on GDAL Virtual file systems (VSI)

.. Index:: gdal vfs copy

Synopsis
--------

.. program-output:: gdal vfs copy --help-doc

Description
-----------

:program:`gdal vfs copy` copy files and directories located on :ref:`virtual_file_systems`.

It can copy files and directories between different virtual file systems.

This is the equivalent of the UNIX ``cp`` command, and ``gdal vfs cp`` is an
alias for ``gdal vfs copy``.

Options
+++++++

.. option:: -r, --recursive

    Copy directories recursively.

.. option:: --skip-errors

    Skip errors that occur while while copying.

Examples
--------

.. example::
   :title: Copy recursively files from /vsis3/bucket/my_dir to local directory, creating a my_dir directory if it does not exist.

   .. code-block:: console

       $ gdal vfs copy -r /vsis3/bucket/my_dir .

.. example::
   :title: Copy recursively files from /vsis3/bucket/my_dir to local directory, *without* creating a my_dir directory, and with progress bar

   .. code-block:: console

       $ gdal vfs copy --progress -r /vsis3/bucket/my_dir/* .
