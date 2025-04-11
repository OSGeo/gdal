.. _gdal_fs_command:

================================================================================
"gdal fs" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for GDAL Virtual file system (VSI) commands

.. Index:: gdal fs

The subcommands of :program:`gdal vs` allow manipulation of files located
on the :ref:`virtual_file_systems`.

Synopsis
--------

.. program-output:: gdal fs --help-doc

Available sub-commands
----------------------

- :ref:`gdal_fs_ls_subcommand`

Examples
--------

.. example::
   :title: Listing recursively files in /vsis3/bucket with details

   .. code-block:: console

       $ gdal fs ls -lR --of=text /vsis3/bucket
