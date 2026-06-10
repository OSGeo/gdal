.. _gdal_mdim_read:

.. program:: gdal_mdim_pipeline_read

================================================================================
``gdal mdim pipeline read``
================================================================================

.. versionadded:: 3.14

.. only:: html

   Read a multidimensional dataset (pipeline only)

.. Index:: gdal mdim pipeline read

Description
-----------

The ``read`` operation is for use in a :ref:`gdal_mdim_pipeline` only, and reads a single input
multidimensional dataset. This is the first step of a pipeline.

Synopsis
--------

.. program-output:: gdal mdim pipeline --help-doc=read

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Read a netCDF file

   .. code-block:: bash

        $ gdal mdim pipeline read input.nc ! ... [other commands here] ...
