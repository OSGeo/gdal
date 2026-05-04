:orphan:

.. _solution_zonal_stats:

Exercise solution for ``gdal raster zonal-stats``
=================================================

Using intermediate :file:`admin_1_around_timis.gpkg`

::

    $ gdal pipeline read  dem.tif ! \
        zonal-stats --stat max,max_center_x,max_center_y \
                    --zones admin_1_around_timis.gpkg  \
                    --include-field admin,name ! \
        make-point --x max_center_x --y max_center_y --output-crs EPSG:4326 ! \
        write dem_point_max_elev.gpkg


Starting from :file:`ne_10m_admin_1_states_provinces.zip`

::

    $ gdal pipeline read  dem.tif ! \
        zonal-stats --stat max,max_center_x,max_center_y \
                    --zones [ read /vsizip/ne_10m_admin_1_states_provinces.zip ! \
                              filter --bbox=19.6854167,45.0565278,22.4426389,46.9537500 ]  \
                    --include-field admin,name ! \
        make-point --x max_center_x --y max_center_y --output-crs EPSG:4326 ! \
        write dem_point_max_elev.gpkg
