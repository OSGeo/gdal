import os

from gdalgviz import generate_diagram

IMAGE_ROOT = os.path.join(os.path.dirname(__file__), "images")


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
