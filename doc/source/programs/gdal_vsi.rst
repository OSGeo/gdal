.. _gdal_vsi_command:

================================================================================
"gdal vsi" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for GDAL Virtual System Interface (VSI) commands

.. Index:: gdal vsi

The subcommands of :program:`gdal vs` allow manipulation of files located
on the :ref:`virtual_file_systems`.

Synopsis
--------

.. program-output:: gdal vsi --help-doc

Available sub-commands
----------------------

- :ref:`gdal_vsi_copy_subcommand`
- :ref:`gdal_vsi_delete_subcommand`
- :ref:`gdal_vsi_list_subcommand`
- :ref:`gdal_vsi_sozip_subcommand`

Examples
--------

.. example::
   :title: Listing recursively files in /vsis3/bucket with details

   .. code-block:: console

       $ gdal vsi list -lR --of=text /vsis3/bucket
