.. _gdal_vfs_command:

================================================================================
"gdal vfs" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for GDAL Virtual file system (VSI) commands

.. Index:: gdal vfs

The subcommands of :program:`gdal vs` allow manipulation of files located
on the :ref:`virtual_file_systems`.

Synopsis
--------

.. program-output:: gdal vfs --help-doc

Available sub-commands
----------------------

- :ref:`gdal_vfs_copy_subcommand`
- :ref:`gdal_vfs_delete_subcommand`
- :ref:`gdal_vfs_list_subcommand`

Examples
--------

.. example::
   :title: Listing recursively files in /vsis3/bucket with details

   .. code-block:: console

       $ gdal vfs list -lR --of=text /vsis3/bucket
