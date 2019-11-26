.. _raster.rdb:

================================================================================
RDB - *RIEGL* Database
================================================================================

.. shortname:: RDB

.. versionadded:: 3.1

.. build_dependencies:: rdblib >= 2.2.0.

GDAL can read \*.mpx files in the RDB format, the in-house format used by `RIEGL Laser Measurement Systems GmbH <http://www.riegl.com>`__ through the RDB library.

The driver relies on the RDB library, which can be downloaded `here <http://riegl.com/members-area/>`__. The minimum version required of the rdblib is 2.2.0.

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
| riegl.reflectance          | Band 4                  |
+----------------------------+-------------------------+
| riegl.amplitude            | Band 5                  |
+----------------------------+-------------------------+
| riegl.deviation            | Band 6                  |
+----------------------------+-------------------------+
| riegl.point_count          | Band 7                  |
+----------------------------+-------------------------+
| riegl.pca_thickness        | Band 8                  |
+----------------------------+-------------------------+
| riegl.std_dev              | Band 9                  |
+----------------------------+-------------------------+
| riegl.height_center        | Band 10                 |
+----------------------------+-------------------------+
| riegl.height_mean          | Band 11                 |
+----------------------------+-------------------------+
| riegl.height_min           | Band 12                 |
+----------------------------+-------------------------+
| riegl.height_max           | Band 13                 |
+----------------------------+-------------------------+
| pixel_linear_sums[0]       | Band 14                 |
|                            |                         |
| pixel_linear_sums[1]       | Band 15                 |
|                            |                         |
| pixel_linear_sums[2]       | Band 16                 |
+----------------------------+-------------------------+
| pixel_square_sums[0]       | Band 17                 |
|                            |                         |
| pixel_square_sums[1]       | Band 18                 |
|                            |                         |
| pixel_square_sums[2]       | Band 19                 |
|                            |                         |
| pixel_square_sums[3]       | Band 20                 |
|                            |                         |
| pixel_square_sums[4]       | Band 21                 |
|                            |                         |
| pixel_square_sums[5]       | Band 22                 |
+----------------------------+-------------------------+
| riegl.voxel_count          | Band 23                 |
+----------------------------+-------------------------+
| riegl.id                   | Band 24                 |
+----------------------------+-------------------------+
| riegl.point_count_grid_cell| Band 25                 |
+----------------------------+-------------------------+
