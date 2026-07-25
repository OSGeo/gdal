import os

from gdalgviz import generate_diagram

IMAGE_ROOT = os.path.join(os.path.dirname(__file__), "images")
WORKSHOP_IMAGE_ROOT = os.path.join(
    os.path.dirname(__file__), "source", "workshop", "images"
)


def test_gdal_pipeline_input_nested():
    pipeline = """
gdal pipeline read n43.tif
! color-map --color-map color_file.txt
! blend --operator=hsv-value --overlay [ read n43.tif ! hillshade -z 30 ]
! write out.tif --overwrite
"""
    output_fn = f"{IMAGE_ROOT}/programs/gdal_pipeline_input_nested.svg"
    generate_diagram(pipeline, output_fn, docs_root="../programs")


def test_gdal_pipeline_ouput_nested():
    pipeline = """
gdal raster pipeline
! read n43.tif
! color-map --color-map color_file.txt
! tee
    [ write colored.tif --overwrite ] 
! blend --operator=hsv-value --overlay
    [
        read n43.tif
        ! hillshade -z 30
        ! tee
            [
                write hillshade.tif --overwrite
            ]
    ]
! write colored-hillshade.tif --overwrite
"""
    output_fn = f"{IMAGE_ROOT}/programs/gdal_pipeline_output_nested.svg"
    generate_diagram(pipeline, output_fn, docs_root="../programs")


def test_gdal_vector_pipeline():
    pipeline = """
gdal vector pipeline
    ! read natural_earth_vector.gpkg --layer ne_110m_populated_places_simple
    ! filter --where "worldcity = 1"
    ! select --fields "_ogr_geometry_,name"
    ! reproject --dst-crs=ESRI:53009
    ! write worldcity_53009.geojson --overwrite
"""
    output_fn = f"{IMAGE_ROOT}/programs/gdal_pipeline_vector_example.svg"
    generate_diagram(pipeline, output_fn, docs_root="../programs", vertical=True)


def test_gdal_vector_pipeline_nested():
    pipeline = """
gdal vector pipeline
    ! read natural_earth_vector.gpkg --layer "ne_10m_rivers_europe"
    ! reproject --output-crs="EPSG:3844"
    ! clip --like [ read natural_earth_vector.gpkg --layer "ne_50m_admin_0_countries" ! filter --where "ADMIN='Romania'" ! reproject --output-crs="EPSG:3844" ]
    ! set-geom-type --geometry-type="MULTILINESTRING"
    ! write romania-rivers.gpkg --overwrite
"""
    output_fn = f"{IMAGE_ROOT}/programs/gdal_pipeline_vector_nested_example.svg"
    generate_diagram(pipeline, output_fn, docs_root="../programs", vertical=True)


def test_gdal_mixed_pipeline_nested():
    pipeline = """
gdal pipeline
    ! read "NE2_50M_SR_W.tif"
    ! clip --like [ read natural_earth_vector.gpkg --layer "ne_50m_admin_0_countries" ! filter --where "ADMIN='Romania'" ! buffer --distance=1 ]
    ! resize --size=70%,70% -r average
    ! write romania.png --overwrite
"""
    output_fn = f"{IMAGE_ROOT}/programs/gdal_mixed_pipeline_nested.svg"
    generate_diagram(pipeline, output_fn, docs_root="../programs", vertical=True)


# workshop pipeline images


def test_tile_merging():
    pipeline = """
gdal pipeline \
            mosaic SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TES_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            ! \
            select --band 1,2,3 \
            ! \
            scale --input-min 400 \
                  --input-max 2400 \
                  --output-data-type uint8 \
            ! \
            tile --min-zoom 10 s2_tiled_min_zoom10 --format WEBP
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/tile_merging.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=True,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )


def test_dem():
    pipeline = """
gdal raster pipeline \
    read dem.tif ! \
    color-map --color-map test.cpt ! \
    blend [ read dem.tif ! hillshade ] --operator hsv-value ! \
    write dem_pipeline.gdalg.json
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/dem.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=False,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )


def test_solution_dem1():
    pipeline = """
gdal raster pipeline \
    read dem.tif ! \
    hillshade ! \
    blend --input [ read dem.tif ! color-map --color-map test.cpt ] --overlay _PIPE_ --operator hsv-value ! \
    write out.tif --overwrite
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/solution_dem1.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=False,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )


def test_solution_dem2():
    pipeline = """
gdal raster pipeline \
    read dem.tif ! \
    hillshade ! \
    blend --input [ read dem.tif color-map --color-map test.cpt ] --overlay _PIPE_ --operator hsv-value ! \
    write out.tif --overwrite
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/solution_dem2.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=False,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )


def test_solution_materialize():
    pipeline = """
gdal raster pipeline \
        mosaic SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TES_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        ! \
        select --band 1,2,3 \
        ! \
        scale --input-min 400 \
              --input-max 2400 \
              --output-data-type uint8 \
        ! \
        materialize --output=mosaic.tif \
        ! \
        tile --min-zoom 10 s2_tiled_min_zoom10 --format WEBP
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/solution_materialize.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=True,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )


def test_pixel_operations():
    pipeline = """
gdal vector pipeline read /vsizip/ne_10m_admin_1_states_provinces.zip ! \
        filter --bbox=19.6854167,45.0565278,22.4426389,46.9537500 ! \
        set-geom-type --geometry-type MULTIPOLYGON ! \
        write admin_1_around_timis.gpkg --overwrite
"""
    output_fn = f"{WORKSHOP_IMAGE_ROOT}/pixel_operations.svg"
    generate_diagram(
        pipeline,
        output_fn,
        vertical=False,
        header_color="#EEFFCC",
        graph_attr={
            "bgcolor": "transparent",
        },
        node_attr={"fontname": "Courier", "fontsize": "12"},
    )
