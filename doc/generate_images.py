import os
import pathlib

import geopandas as gpd
import matplotlib.colors
import matplotlib.patheffects as pe
import matplotlib.pyplot as pyplot
import numpy as np
import pytest

from osgeo import gdal, gdal_array, ogr, osr

IMAGE_ROOT = os.path.join(os.path.dirname(__file__), "images")
DATA_DIR = pathlib.Path(os.path.join(os.path.dirname(__file__), "data"))

GDAL_GREEN_BLUE = matplotlib.colors.ListedColormap(["#71c9f1", "#359946"])


gdal.UseExceptions()
matplotlib.use("Agg")  # use a non-GUI backend


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


def wkt_ds(fname, wkts, *, geom_type=None, epsg=None):

    ds = gdal.GetDriverByName("ESRI Shapefile").CreateVector(fname)

    lyr = ds.CreateLayer(
        "polys",
        osr.SpatialReference(epsg=epsg) if epsg else None,
        geom_type=geom_type if geom_type else ogr.wkbUnknown,
    )

    if type(wkts) is str:
        wkts = [wkts]

    for i, wkt in enumerate(wkts):
        f = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(wkt)
        assert geom
        f.SetGeometry(geom)
        f.SetFID(i + 1)
        lyr.CreateFeature(f)

    return ds


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


def test_gdal_raster_neighbors():
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
        "raster neighbors",
        input=ds,
        output_format="MEM",
        method="sum",
        size=3,
        kernel="equal",
    )

    data_out = alg["output"].GetDataset().ReadAsMaskedArray()

    alg.Finalize()

    ax.pcolor(data, cmap=pyplot.get_cmap("viridis"))
    print_cell_values(ax, data)
    ax2.pcolor(data_out, cmap=pyplot.get_cmap("viridis"))
    print_cell_values(ax2, data_out)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_neighbors.svg",
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

    data_out = alg["output"].GetDataset().ReadAsMaskedArray()
    alg.Finalize()

    ax.pcolor(data, cmap=pyplot.get_cmap("Purples"))
    print_cell_values(ax, data)
    ax2.pcolor(data_out, cmap=pyplot.get_cmap("Set3", 3))
    print_cell_values(ax2, data_out)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_reclassify.svg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_raster_zonal_stats(tmp_path):

    raster_fname = DATA_DIR / "fortune.tif"
    vector_fname = DATA_DIR / "fortune_subd.geojson"

    output_fname = tmp_path / "out.dbf"

    alg = gdal.Run(
        "raster zonal-stats",
        input=raster_fname,
        zones=vector_fname,
        output=output_fname,
        stat="mean",
        include_field="csduid",
    )
    alg.Finalize()

    with gdal.Open(raster_fname) as ds:
        fortune = ds.ReadAsMaskedArray()
        xmin, dx, _, ymax, _, dy = ds.GetGeoTransform()
        nx = ds.RasterXSize
        ny = ds.RasterYSize
        extent = [xmin, xmin + nx * dx, ymax + dy * ny, ymax]

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(12, 5), sharex=True, sharey=True)
    ax.set_axis_off()
    ax2.set_axis_off()

    vmin, vmax = np.quantile(fortune.compressed(), [0.01, 0.99])

    ax.imshow(fortune, extent=extent, vmin=vmin, vmax=vmax)

    vector_gdf = gpd.read_file(vector_fname)
    vector_gdf.boundary.plot(ax=ax, edgecolor="black", linewidth=1, aspect="equal")

    stats_df = gpd.read_file(output_fname)
    stats_df.drop("geometry", axis=1, inplace=True, errors="ignore")

    vector_gdf.merge(stats_df, on="csduid").plot(
        ax=ax2, aspect="equal", column="mean", vmin=vmin, vmax=vmax
    )
    vector_gdf.boundary.plot(ax=ax2, edgecolor="black", linewidth=1, aspect="equal")

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_raster_zonal_stats.jpg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_vector_buffer_points(tmp_path):

    opts = [
        {"quadrant-segments": 4, "distance": 5},
        {"quadrant-segments": 20, "distance": 5},
    ]
    src_fname = tmp_path / "points.geojson"

    wkt_ds(
        src_fname,
        [
            "POINT (0 0)",
            "POINT (5 5)",
        ],
    )

    for i, opt in enumerate(opts):
        opt["input"] = src_fname
        opt["output"] = tmp_path / f"{i}_buffers.geojson"
        alg = gdal.Run("vector buffer", opt)
        alg.Finalize()

    plt, ax = pyplot.subplots(1, len(opts), figsize=(10, 4))

    src_gdf = gpd.read_file(src_fname)

    for i in range(len(opts)):

        src_gdf.plot(ax=ax[i], alpha=0.5, color="black")

        dst_gdf = gpd.read_file(tmp_path / f"{i}_buffers.geojson")
        dst_gdf.plot(ax=ax[i], alpha=0.5, edgecolor="black", cmap=GDAL_GREEN_BLUE)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_buffer_points.svg",
        bbox_inches="tight",
        transparent=True,
    )


@pytest.mark.parametrize(
    "parameter,opts",
    [
        (
            "endcap",
            [
                {"endcap-style": "round", "distance": 2},
                {"endcap-style": "flat", "distance": 2},
                {"endcap-style": "square", "distance": 2},
            ],
        ),
        (
            "side",
            [
                {"side": "both", "distance": 2},
                {"side": "left", "distance": 2},
                {"side": "right", "distance": 2},
            ],
        ),
        (
            "join",
            [
                {"join-style": "round", "distance": 2},
                {"join-style": "mitre", "distance": 2},
                {"join-style": "bevel", "distance": 2},
            ],
        ),
        (
            "mitre",
            [
                {"join-style": "round", "distance": 2, "mitre-limit": 5},
                {"join-style": "round", "distance": 2, "mitre-limit": 1},
            ],
        ),
    ],
)
def test_gdal_vector_buffer_lines(tmp_path, parameter, opts):
    src_fname = tmp_path / "lines.geojson"

    wkt_ds(
        src_fname,
        ["LINESTRING (30 10, 40 20, 30 30, 40 40)"],
    )

    for i, opt in enumerate(opts):
        opt["input"] = src_fname
        opt["output"] = tmp_path / f"{i}_{parameter}.geojson"
        alg = gdal.Run("vector buffer", opt)
        alg.Finalize()

    plt, ax = pyplot.subplots(1, len(opts), figsize=(10, 4))

    src_gdf = gpd.read_file(src_fname)

    for i in range(len(opts)):
        ax[i].set_axis_off()

        src_gdf.plot(ax=ax[i], alpha=0.5, color="black", linewidth=2)

        dst_gdf = gpd.read_file(tmp_path / f"{i}_{parameter}.geojson")
        dst_gdf.plot(
            ax=ax[i], alpha=0.5, linewidth=0.5, edgecolor="gray", cmap=GDAL_GREEN_BLUE
        )

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_buffer_lines_{parameter}.svg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_vector_check_coverage(tmp_path):

    src_fname = tmp_path / "src.shp"
    dst_fname = tmp_path / "dst.shp"

    wkt_ds(
        src_fname,
        [
            "POLYGON ((0 0, 5 0, 5 4, 4.5 5, 5 9, 5 10, 0 10, 0 0))",
            "POLYGON ((5 0, 10 0, 10 10, 5.1 10, 5 9, 5 3, 4.5 0, 5 0))",
        ],
    )

    alg = gdal.Run("vector check-coverage", input=src_fname, output=dst_fname)
    alg.Finalize()

    plt, (ax, ax2) = pyplot.subplots(1, 2, figsize=(12, 5))

    ax.set_axis_off()
    ax2.set_axis_off()

    src_gdf = gpd.read_file(src_fname)
    src_gdf.plot(
        ax=ax, alpha=0.5, edgecolor="black", column="FID", cmap=GDAL_GREEN_BLUE
    )

    dst_gdf = gpd.read_file(dst_fname)
    dst_gdf.plot(ax=ax2, aspect="equal", alpha=0.5, column="FID", cmap=GDAL_GREEN_BLUE)

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_check_coverage.svg",
        bbox_inches="tight",
        transparent=True,
    )


def test_gdal_vector_check_geometry(tmp_path):

    cases = {
        "poly": "POLYGON ((0 0, 5 0, 0 10, 5 10, 0 0))",
        "multipoly": "MULTIPOLYGON (((0 0, 5 0, 5 3, 0 3, 0 0)), ((2 2, 3 2, 3 10, 2 10, 2 2)))",
        "line": "LINESTRING (5 10, 0 5, 5 0, 2.5 0, 2.5 10)",
    }

    for k, v in cases.items():
        src_fname = tmp_path / f"{k}.gpkg"
        dst_fname = tmp_path / f"{k}_out.gpkg"

        wkt_ds(src_fname, [v])

        alg = gdal.Run("vector check-geometry", input=src_fname, output=dst_fname)
        alg.Finalize()

    plt, ax = pyplot.subplots(1, len(cases), figsize=(12, 5))

    for i, (k, v) in enumerate(cases.items()):
        ax[i].set_axis_off()

        src_gdf = gpd.read_file(tmp_path / f"{k}.gpkg")
        src_gdf.plot(ax=ax[i], alpha=0.5, cmap=GDAL_GREEN_BLUE)

        dst_gdf = gpd.read_file(tmp_path / f"{k}_out.gpkg")
        dst_gdf.plot(ax=ax[i], alpha=1.0, color="black")

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_check_geometry.svg",
        bbox_inches="tight",
        transparent=True,
    )


@pytest.mark.parametrize(
    "operation",
    ("erase", "identity", "intersection", "sym-difference", "union", "update", "clip"),
)
def test_gdal_vector_layer_algebra(tmp_path, operation):

    squares_fname = DATA_DIR / "squares.geojson"
    circle_fname = DATA_DIR / "circle.geojson"
    result_fname = tmp_path / "out.geojson"

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


@pytest.mark.parametrize(
    "case,opts",
    [
        ("close_gaps", {"maximum-gap-width": 1}),
        ("merge_max_area", {"merge-strategy": "max-area"}),
        ("snap_distance", {"snapping-distance": 0.1}),
    ],
)
def test_gdal_vector_clean_coverage(tmp_path, case, opts):

    src_fname = tmp_path / "src.shp"
    dst1_fname = tmp_path / "dst1.shp"
    dst2_fname = tmp_path / "dst2.shp"

    wkt_ds(
        src_fname,
        [
            "POLYGON ((0 0, 5 0, 5 4, 4.5 5, 5 9, 5 10, 0 10, 0 0))",
            "POLYGON ((5 0, 10 0, 10 10, 5.1 10, 5 9, 5 3, 4.5 0, 5 0))",
        ],
    )

    # run with default options
    alg = gdal.Run("vector clean-coverage", input=src_fname, output=dst1_fname)

    opts["input"] = src_fname
    opts["output"] = dst2_fname

    alg = gdal.Run("vector clean-coverage", opts)
    alg.Finalize()

    plt, (ax, ax2, ax3) = pyplot.subplots(1, 3, figsize=(12, 5))

    src_gdf = gpd.read_file(src_fname)
    src_gdf.plot(
        ax=ax, alpha=0.5, edgecolor="black", column="FID", cmap=GDAL_GREEN_BLUE
    )

    dst1_gdf = gpd.read_file(dst1_fname)
    dst1_gdf.plot(
        ax=ax2, alpha=0.5, edgecolor="black", column="FID", cmap=GDAL_GREEN_BLUE
    )

    dst2_gdf = gpd.read_file(dst2_fname)
    dst2_gdf.plot(
        ax=ax3, alpha=0.5, edgecolor="black", column="FID", cmap=GDAL_GREEN_BLUE
    )

    plt.savefig(
        f"{IMAGE_ROOT}/programs/gdal_vector_clean_coverage_{case}.svg",
        bbox_inches="tight",
        transparent=True,
    )
    pyplot.close()


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


@pytest.mark.parametrize(
    "operator",
    (
        "src-over",
        "hsv-value",
        "multiply",
        "overlay",
        "screen",
        "color-dodge",
        "color-burn",
        "hard-light",
    ),
)
def test_gdal_raster_blend_1band_overlay(operator):

    input_fname = DATA_DIR / "hypsometric.jpg"
    overlay_fname = DATA_DIR / "hillshade.jpg"

    output_fname = f"{IMAGE_ROOT}/programs/gdal_raster_blend/{operator}.jpg"

    alg = gdal.Run(
        "raster",
        "blend",
        input=input_fname,
        overlay=overlay_fname,
        output=output_fname,
        operator=operator,
        overwrite=True,
    )
    alg.Finalize()


@pytest.mark.parametrize(
    "operator",
    (
        "lighten",
        "darken",
    ),
)
def test_gdal_raster_blend_3band_overlay(operator):

    input_fname = DATA_DIR / "hypsometric.jpg"
    overlay_fname = DATA_DIR / "hillshade_rgb.jpg"

    output_fname = f"{IMAGE_ROOT}/programs/gdal_raster_blend/{operator}_3band.jpg"

    alg = gdal.Run(
        "raster",
        "blend",
        input=input_fname,
        overlay=overlay_fname,
        output=output_fname,
        operator=operator,
        overwrite=True,
    )
    alg.Finalize()
