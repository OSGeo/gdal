r"""
pip install pytest
pip install gdalgviz
$GVIZ_PATH = "C:\Program Files\Graphviz\bin"
$env:PATH = "$GVIZ_PATH;$env:PATH"
pytest ./scripts/generate_pipeline_images.py
"""

import os

from gdalgviz import generate_diagram

IMAGE_ROOT = os.path.join(os.path.dirname(__file__), "../images")


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
    output_fn = f"{IMAGE_ROOT}/tile_merging.svg"
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
    output_fn = f"{IMAGE_ROOT}/dem.svg"
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
    output_fn = f"{IMAGE_ROOT}/solution_dem1.svg"
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
    output_fn = f"{IMAGE_ROOT}/solution_dem2.svg"
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
    output_fn = f"{IMAGE_ROOT}/solution_materialize.svg"
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
    output_fn = f"{IMAGE_ROOT}/pixel_operations.svg"
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
