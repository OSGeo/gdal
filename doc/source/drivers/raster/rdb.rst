.. _raster.rdb:

================================================================================
RDB - *RIEGL* Database
================================================================================

.. shortname:: RDB

.. versionadded:: 3.1

.. build_dependencies:: rdblib >= 2.2.0.

GDAL can read \*.mpx files in the RDB format, the in-house format used by `RIEGL Laser Measurement Systems GmbH <http://www.riegl.com>`__ through the RDB library.

The driver relies on the RDB library, which can be downloaded `here <https://repository.riegl.com/software/libraries/rdblib>`__ . The minimum version required of the rdblib is 2.2.0.

Driver capabilities
-------------------

.. supports_georeferencing::

Provided Bands
-------------------

All attributes stored in the RDB, but the coordinates, are provided in bands. Vector attributes are split up into multiple bands.
The attributes are currently mapped as follows:

+----------------------------+-------------------------+
| RDB attribute              | GDAL Band               |
+============================+=========================+
| riegl.surface_normal[0],   | Band 1                  |
|                            |                         |
| riegl.surface_normal[1],   | Band 2                  |
|                            |                         |
| riegl.surface_normal[2]    | Band 3                  |
+----------------------------+-------------------------+
| riegl.timestamp_min        | Band 4                  |
+----------------------------+-------------------------+
| riegl.timestamp_max        | Band 5                  |
+----------------------------+-------------------------+
| riegl.reflectance          | Band 6                  |
+----------------------------+-------------------------+
| riegl.amplitude            | Band 7                  |
+----------------------------+-------------------------+
| riegl.deviation            | Band 8                  |
+----------------------------+-------------------------+
| riegl.height_center        | Band 9                  |
+----------------------------+-------------------------+
| riegl.height_mean          | Band 10                 |
+----------------------------+-------------------------+
| riegl.height_min           | Band 11                 |
+----------------------------+-------------------------+
| riegl.height_max           | Band 12                 |
+----------------------------+-------------------------+
| riegl.point_count          | Band 13                 |
+----------------------------+-------------------------+
| riegl.point_count_grid_cell| Band 14                 |
+----------------------------+-------------------------+
| riegl.pca_thickness        | Band 15                 |
+----------------------------+-------------------------+
| riegl.std_dev              | Band 16                 |
+----------------------------+-------------------------+
| riegl.voxel_count          | Band 17                 |
+----------------------------+-------------------------+
| riegl.id                   | Band 18                 |
+----------------------------+-------------------------+
