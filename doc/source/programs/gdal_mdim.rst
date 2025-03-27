.. _gdal_mdim_command:

================================================================================
"gdal mdim" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for multidimensional commands

.. Index:: gdal mdim

Synopsis
--------

.. program-output:: gdal mdim --help-doc

Available sub-commands
----------------------

- :ref:`gdal_mdim_info_subcommand`
- :ref:`gdal_mdim_convert_subcommand`

Examples
--------

.. example::
   :title: Getting information on the file :file:`temperatures.nc` (with JSON output)

   .. code-block:: console

       $ gdal mdim info temperatures.nc

.. example::
   :title: Converting file :file:`temperatures.nc` to Zarr

   .. code-block:: console

       $ gdal mdim convert temperatures.nc temperatures.zarr
