import os
import pathlib

import geopandas as gpd
import matplotlib.patheffects as pe
import matplotlib.pyplot as pyplot
import numpy as np
import pytest

from osgeo import gdal, gdal_array

IMAGE_ROOT = os.path.join(os.path.dirname(__file__), "images")
DATA_DIR = pathlib.Path(os.path.join(os.path.dirname(__file__), "data"))


def print_cell_values(ax, data):
    for y in range(data.shape[0]):
        for x in range(data.shape[1]):
            has_data = not data.mask[y][x]
            z = data[y][x]
            ax.text(
                x + 0.5,
                y + 0.5,
                "NoData" if not has_data else f"{z}",
                va="center",
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground="white")],
            )


def test_gdal_raster_footprint(tmp_path):
    fortune_fname = DATA_DIR / "fortune.tif"
    output_fname = tmp_path / "out.shp"

    with gdal.Open(fortune_fname) as ds:
        fortune = ds.ReadAsMaskedArray()
        xmin, dx, _, ymax, _, dy = ds.GetGeoTransform()
        nx = ds.RasterXSize
        ny = ds.RasterYSize
        extent = [xmin, xmin + nx * dx, ymax + dy * ny, ymax]

        plt, (ax, ax2) = pyplot.subplots(
            1, 2, figsize=(12, 5), sharex=True, sharey=True
        )
        ax.set_axis_off()
        ax2.set_axis_off()

        ax.imshow(fortune, extent=extent)

        alg = gdal.Run("raster footprint", input=fortune_fname, output=output_fname)
        alg.Finalize()

        out_gdf = gpd.read_file(output_fname)
        out_gdf.plot(ax=ax2, aspect="equal")

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_footprint.png",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_raster_polygonize(tmp_path):
    output_fname = tmp_path / "out.shp"

    nodata = -9

    data = np.array([[4, 1, 4], [nodata, 4, 2], [1, 2, 2]])
    data = np.ma.masked_array(data, data == nodata)

    plt, ax = pyplot.subplots(1, 3, figsize=(15, 6))

    for x in ax:
        x.set_aspect("equal", adjustable="box")
        x.set_xlim([0, data.shape[1]])
        x.set_ylim([0, data.shape[0]])
        x.invert_yaxis()
        x.axis("off")

    ds = gdal_array.OpenArray(data)
    ds.GetRasterBand(1).SetNoDataValue(nodata)

    ax[0].pcolor(data, cmap=pyplot.get_cmap("Set2"))
    ax[0].grid(color="black", linewidth=2)
    ax[0].set_xticks([0, 1, 2, 3], [])
    ax[0].set_yticks([0, 1, 2, 3], [])
    ax[0].axis("on")
    ax[0].tick_params(length=0)
    print_cell_values(ax[0], data)

    for connect_diagonals in (False, True):

        alg = gdal.Run(
            "raster polygonize",
            input=ds,
            output=output_fname,
            connect_diagonal_pixels=connect_diagonals,
            overwrite=True,
        )
        alg.Finalize()

        colors = ["#8dd3c7", "#ffffb3", "#bebada", "#fb8072", "#80b1d3", "#fdb462"]

        out_gdf = gpd.read_file(output_fname)
        out_gdf.sort_values(by="DN", inplace=True)
        out_gdf.plot(
            ax=ax[1 + connect_diagonals],
            edgecolor="black",
            linewidth=2,
            color=colors[: len(out_gdf)],
        )

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_polygonize.svg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_raster_reclassify():
    nodata = -999
    data_raw = np.arange(9, dtype=np.int16).reshape(3, 3)
    data_raw[2, 2] = nodata
    data = np.ma.masked_array(data_raw, data_raw == nodata)

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(15, 6))

    for x in (ax, ax2):
        x.set_aspect("equal", adjustable="box")
        x.set_xticks([], [])
        x.set_yticks([], [])
        x.grid(color="black", linewidth=2)
        x.invert_yaxis()

    ds = gdal_array.OpenArray(data)
    ds.GetRasterBand(1).SetNoDataValue(nodata)

    alg = gdal.Run(
        "raster reclassify",
        input=ds,
        output_format="MEM",
        mapping="[1,3]= 101; [4, 5)= 102; 7=102; NO_DATA=103; DEFAULT=NO_DATA",
    )
    alg.Finalize()

    data_out = alg["output"].GetDataset().ReadAsMaskedArray()

    ax.pcolor(data, cmap=pyplot.get_cmap("Purples"))
    print_cell_values(ax, data)
    ax2.pcolor(data_out, cmap=pyplot.get_cmap("Set3", 3))
    print_cell_values(ax2, data_out)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_reclassify.svg",
        bbox_inches="tight",
        transparent=True,
    )


@pytest.mark.parametrize(
    "operation",
    ("erase", "identity", "intersection", "sym-difference", "union", "update", "clip"),
)
def test_gdal_vector_layer_algebra(tmp_path, operation):

    squares_fname = DATA_DIR / "squares.shp"
    circle_fname = DATA_DIR / "circle.shp"
    result_fname = tmp_path / "out.shp"

    alg = gdal.Run(
        "vector layer-algebra",
        operation=operation,
        input=squares_fname,
        method=circle_fname,
        output=result_fname,
    )
    alg.Finalize()

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(12, 5), sharex=True, sharey=True)
    ax.set_axis_off()
    ax2.set_axis_off()

    squares = gpd.read_file(squares_fname)
    circle = gpd.read_file(circle_fname)

    squares.plot(ax=ax, facecolor="blue", alpha=0.5, edgecolor="black")
    circle.plot(ax=ax, facecolor="red", alpha=0.5, edgecolor="black")

    op = gpd.read_file(result_fname)
    op.boundary.plot(ax=ax2, facecolor="yellow", edgecolor="black", alpha=0.3)

    plt.savefig(
        f'{IMAGE_ROOT}/programs/gdal_vector_layer_algebra_{operation.replace("-", "_")}.svg',
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_vector_simplify(tmp_path):

    src_fname = DATA_DIR / "wells.geojson"
    dst_fname = tmp_path / "out.shp"

    alg = gdal.Run("vector simplify", input=src_fname, output=dst_fname, tolerance=3e-3)
    alg.Finalize()

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(12, 5))
    ax.set_axis_off()
    ax2.set_axis_off()

    src_gdf = gpd.read_file(src_fname)
    dst_gdf = gpd.read_file(dst_fname)

    src_gdf.plot(ax=ax, linewidth=1)
    dst_gdf.plot(ax=ax2, linewidth=1)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_simplify.svg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_vector_simplify_coverage(tmp_path):

    src_fname = "../autotest/ogr/data/poly.shp"
    dst_fname = tmp_path / "out.shp"

    alg = gdal.Run(
        "vector simplify-coverage", input=src_fname, output=dst_fname, tolerance=110
    )
    alg.Finalize()

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(12, 5))
    ax.set_axis_off()
    ax2.set_axis_off()

    src_gdf = gpd.read_file(src_fname)
    src_gdf.boundary.plot(ax=ax)

    dst_gdf = gpd.read_file(dst_fname)
    dst_gdf.boundary.plot(ax=ax2)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_simplify_coverage.svg",
        bbox_inches="tight",
        transparent=True,
    )
