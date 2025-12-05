.. _gdal_vsi:

================================================================================
``gdal vsi``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for GDAL Virtual System Interface (VSI) commands

.. Index:: gdal vsi

The subcommands of :program:`gdal vsi` allow manipulation of files located
on the :ref:`virtual_file_systems`.

Synopsis
--------

.. program-output:: gdal vsi --help-doc

Available sub-commands
----------------------

- :ref:`gdal_vsi_copy`
- :ref:`gdal_vsi_delete`
- :ref:`gdal_vsi_list`
- :ref:`gdal_vsi_move`
- :ref:`gdal_vsi_sync`
- :ref:`gdal_vsi_sozip`

Examples
--------

.. example::
   :title: Listing recursively files in /vsis3/bucket with details

   .. code-block:: console

       $ gdal vsi list -lR /vsis3/bucket
