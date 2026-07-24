:orphan:

.. _solution_format_conversion_vector:

Exercise solution for vector format conversion
==============================================

1. Convert to GeoJSON

    ::

        $ gdal vector convert timisoara.osm.pbf out_points.geojson --input-layer points
        $ gdal vector convert timisoara.osm.pbf out_lines.geojson --input-layer lines
        $ gdal vector convert timisoara.osm.pbf out_multilinestrings.geojson --input-layer multilinestrings
        $ gdal vector convert timisoara.osm.pbf out_multipolygons.geojson --input-layer multipolygons
        $ gdal vector convert timisoara.osm.pbf out_other_relations.geojson --input-layer other_relations


2. "Exploding" collections in single primitives (points, lines, polygons).

    ::

        $ gdal vector explode-collections timisoara.osm.pbf timisoara.gpkg --overwrite


3. Create a layer for each geometry type

    ::

        $ gdal vector filter timisoara.gpkg other_relations_points.gpkg \
            --input-layer other_relations  --where "ST_GeometryType(geom) = 'POINT'" --overwrite

        $ gdal vector filter timisoara.gpkg other_relations_lines.gpkg \
            --input-layer other_relations  --where "ST_GeometryType(geom) = 'LINESTRING'" --overwrite

        $ gdal vector filter timisoara.gpkg other_relations_polygons.gpkg \
            --input-layer other_relations  --where "ST_GeometryType(geom) = 'POLYGON'" --overwrite
