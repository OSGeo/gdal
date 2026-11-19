import os

import matplotlib
import matplotlib.pyplot as plt

from osgeo import gdal, ogr, osr

matplotlib.rcParams["svg.hashsalt"] = "gdal"  # make SVG ids deterministic

gdal.UseExceptions()

DOC_ROOT = os.path.join(os.path.dirname(__file__), os.pardir)

RST_DIR = os.path.join(DOC_ROOT, "source", "user")
IMAGE_DIR = os.path.join(DOC_ROOT, "images", "user", "geometry_validity")
IMAGE_RELPATH = os.path.relpath(IMAGE_DIR, RST_DIR)

GDAL_BLUE = "#71c9f1"
GDAL_GREEN = "#359946"

cases = [
    {
        "wkt": "POLYGON ((10 90, 90 10, 90 90, 10 10, 10 90))",
        "description": "Self-intersecting polygon",
    },
    {
        "wkt": "POLYGON ((10 10, 90 10, 90 40, 80 20, 70 40, 80 60, 90 40, 90 90, 10 90, 10 10))",
        "description": "Polygon with self-touching ring",
        "ref": "polygon_self_touching",
    },
    {
        "wkt": "POLYGON ((10 90, 50 90, 50 10, 10 10, 10 90), (60 80, 90 80, 90 20, 60 20, 60 80))",
        "description": "Polygon hole outside shell",
    },
    {
        "wkt": "POLYGON ((10 90, 60 90, 60 10, 10 10, 10 90), (30 70, 90 70, 90 30, 30 30, 30 70))",
        "description": "Hole partially outside polygon shell",
    },
    {
        "wkt": "POLYGON ((10 90, 90 90, 90 10, 10 10, 10 90), (10 90, 90 90, 90 10, 10 10, 10 90))",
        "description": "Polygon hole equal to shell",
    },
    {
        "wkt": "POLYGON ((10 90, 90 90, 90 10, 10 10, 10 90), (80 80, 80 30, 30 30, 30 80, 80 80), (20 20, 20 70, 70 70, 70 20, 20 20))",
        "description": "Polygon holes overlap",
    },
    {
        "wkt": "POLYGON ((30 70, 70 70, 70 30, 30 30, 30 70), (10 90, 90 90, 90 10, 10 10, 10 90))",
        "description": "Polygon shell inside hole",
    },
    {
        "wkt": "POLYGON ((10 70, 90 70, 90 50, 30 50, 30 30, 50 30, 50 90, 70 90, 70 10, 10 10, 10 70))",
        "description": "Self-crossing polygon shell",
    },
    {
        "wkt": "POLYGON ((10 90, 50 90, 50 30, 70 30, 70 50, 30 50, 30 70, 90 70, 90 10, 10 10, 10 90))",
        "description": "Self-overlapping polygon shell",
    },
    {
        "wkt": "MULTIPOLYGON (((30 70, 70 70, 70 30, 30 30, 30 70)), ((10 90, 90 90, 90 10, 10 10, 10 90)))",
        "description": "Nested MultiPolygons",
    },
    {
        "wkt": "MULTIPOLYGON (((10 90, 60 90, 60 10, 10 10, 10 90)), ((90 80, 90 20, 40 20, 40 80, 90 80)))",
        "description": "Overlapping MultiPolygons",
    },
    {
        "wkt": "MULTIPOLYGON (((90 90, 90 30, 30 30, 30 90, 90 90)), ((20 20, 20 80, 80 80, 80 20, 20 20)), ((10 10, 10 70, 70 70, 70 10, 10 10)))",
        "description": "MultiPolygon with multiple overlapping Polygons",
    },
    {
        "wkt": "MULTIPOLYGON (((10 90, 50 90, 50 10, 10 10, 10 90)), ((90 80, 90 20, 50 20, 50 80, 90 80)))",
        "description": "MultiPolygon with two adjacent Polygons",
    },
    {"wkt": "POLYGON ((70 30))", "description": "Single-point polygon"},
    {"wkt": "POLYGON ((10 10, 90 90))", "description": "Two-point polygon"},
    {"wkt": "POLYGON ((10 10, 90 10, 90 90, 10 90))", "description": "Non-closed ring"},
]


def wkt_ds(features, *, geom_type=None, epsg=None):

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    if type(features) is str:
        features = [features]

    lyr = ds.CreateLayer(
        "polys",
        osr.SpatialReference(epsg=epsg) if epsg else None,
        geom_type=geom_type if geom_type else ogr.wkbUnknown,
    )

    field_names = ["description", "ref"]
    for f in features:
        for name in f.keys():
            if name != "wkt" and name not in field_names:
                field_names.append(name)

    for field in field_names:
        lyr.CreateField(ogr.FieldDefn(field, ogr.OFTString))

    for i, feature in enumerate(features):
        f = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(feature["wkt"])
        assert geom
        f.SetGeometry(geom)
        f.SetFID(i + 1)

        for field in field_names:
            f[field] = feature.get(field)

        lyr.CreateFeature(f)

    return ds


src_ds = wkt_ds(cases)
check_geom_alg = gdal.alg.vector.check_geometry(src_ds, "", output_format="MEM")
make_valid_linework_alg = gdal.alg.vector.make_valid(
    src_ds, "", method="linework", output_format="MEM"
)
make_valid_structure_alg = gdal.alg.vector.make_valid(
    src_ds, "", method="structure", output_format="MEM"
)

error_ds = check_geom_alg.Output()
make_valid_linework_ds = make_valid_linework_alg.Output()
make_valid_structure_ds = make_valid_structure_alg.Output()

src_lyr = src_ds.GetLayer(0)

error_lyr = error_ds.GetLayer(0)
make_valid_linework_lyr = make_valid_linework_ds.GetLayer(0)
make_valid_structure_lyr = make_valid_structure_ds.GetLayer(0)

assert (
    src_lyr.GetFeatureCount()
    == error_lyr.GetFeatureCount()
    == make_valid_linework_lyr.GetFeatureCount()
    == make_valid_structure_lyr.GetFeatureCount()
)

results = []

for f_src, f_err, f_lw, f_s in zip(
    src_lyr, error_lyr, make_valid_linework_lyr, make_valid_structure_lyr
):
    result = {"description": f_src["description"], "ref": f_src["ref"]}

    result["source_wkt"] = f_src.GetGeometryRef().ExportToWkt()
    result["error"] = f_err["error"]

    g_err = f_err.GetGeometryRef()
    result["error_wkt"] = g_err.ExportToWkt() if g_err else ""

    g_lw = f_lw.GetGeometryRef()
    result["make_valid_linework_wkt"] = g_lw.ExportToWkt() if g_lw else ""

    g_s = f_s.GetGeometryRef()
    result["make_valid_structure_wkt"] = g_s.ExportToWkt() if g_s else ""

    results.append(result)


def collect_rings(geom, shells, holes):
    if geom.GetGeometryType() == ogr.wkbMultiPolygon:
        for i in range(geom.GetGeometryCount()):
            collect_rings(geom.GetGeometryRef(i), shells, holes)
    else:
        if geom.GetGeometryRef(0) is not None:
            shells.append(geom.GetGeometryRef(0))
        for i in range(1, geom.GetGeometryCount()):
            holes.append(geom.GetGeometryRef(i))


def interpolate(p0, p1, frac):
    return (p0[0] + frac * (p1[0] - p0[0]), p0[1] + frac * (p1[1] - p0[1]))


def plot_points(ax, points, **kwargs):
    x = [pt[0] for pt in points]
    y = [pt[1] for pt in points]
    ax.plot(x, y, **kwargs)


def plot_rings(ax, shells, holes, point_map):

    for is_hole in (False, True):
        rings = holes if is_hole else shells

        for ring_i, ring in enumerate(rings):
            points = ring.GetPoints()
            x = [pt[0] for pt in points]
            y = [pt[1] for pt in points]

            # background
            if is_hole:
                ax.fill(x, y, color="white")
            else:
                ax.fill(x, y, color=GDAL_BLUE, alpha=0.2)

            # collect point labels
            for i, point in enumerate(points[:-1]):
                if point not in point_map:
                    point_map[point] = set()

                point_map[point].add(i)

            # add direction arrows
            for p0, p1 in zip(points[:-1], points[1:]):
                tip = interpolate(p0, p1, 0.5)
                start = interpolate(p0, p1, 0.45)
                ax.annotate(
                    "",
                    xytext=start,
                    xy=tip,
                    arrowprops={
                        "facecolor": GDAL_BLUE,
                        "edgecolor": GDAL_BLUE,
                        "headwidth": 6,
                    },
                )

            # outline
            ax.plot(
                x,
                y,
                linestyle="dashed" if is_hole else "solid",
                marker="o",
                color=GDAL_BLUE,
            )

            # add part component labels
            # if not is_hole and len(shells) > 1:
            #     for p0, p1 in zip(points[:-1], points[1:]):
            #         #part_label_point = interpolate(p0, p1, 0.3)
            #         ax.annotate(f"poly{ring_i + 1}", xy=tip, xytext=tip)


def plot_point_labels(point_map):
    for point, indices in point_map.items():
        label = "/".join(str(i) for i in sorted(indices))
        ax.annotate(label, xy=point, xytext=[2, 2], textcoords="offset points")


rst_file = open(os.path.join(RST_DIR, "geometry_validity_examples.rst"), "w")

for case_i, case in enumerate(results):

    error_points = None
    error_geom = ogr.CreateGeometryFromWkt(case["error_wkt"])
    if error_geom:
        error_points = [
            error_geom.GetGeometryRef(i).GetPoint_2D()
            for i in range(error_geom.GetGeometryCount())
        ]

    if case.get("ref"):
        rst_file.write(f"""
.. _{case['ref']}:
    """)

    rst_file.write(f"""

{case['description']}
{'+' * len(case['description'])}

.. rst-class:: top-aligned-table

.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    """)

    for thing in ("source", "make_valid_linework", "make_valid_structure"):

        wkt = case[f"{thing}_wkt"]

        fig, ax = plt.subplots()

        if wkt:
            geom = ogr.CreateGeometryFromWkt(wkt)

            if geom.GetGeometryType() == ogr.wkbMultiPolygon:
                geoms = [geom.GetGeometryRef(i) for i in range(geom.GetGeometryCount())]
            else:
                geoms = [geom]

            point_map = dict()

            for geom in geoms:
                typ = ogr.GT_Flatten(geom.GetGeometryType())

                if typ in (ogr.wkbPolygon, ogr.wkbMultiPolygon):

                    shells = []
                    holes = []

                    collect_rings(geom, shells, holes)
                    plot_rings(ax, shells, holes, point_map)

                elif typ in (ogr.wkbPoint, ogr.wkbLineString):

                    points = [geom.GetPoint_2D(i) for i in range(geom.GetPointCount())]

                    plot_points(ax, points, marker="o", color=GDAL_BLUE)

                else:
                    raise Exception("unhandled geom type")

            plot_point_labels(point_map)

        if thing == "source" and error_points:
            plot_points(
                ax,
                error_points,
                marker="X",
                color="red",
                markersize=12,
                markerfacecolor="none",
            )

        fig_fname = f"geometry_{thing}_{case_i + 1}.svg"

        fig.savefig(
            os.path.join(IMAGE_DIR, fig_fname),
            transparent=True,
            format="svg",
            metadata={"Date": None},
        )

        print(f'{case["description"]}: saved {fig_fname}')

    rst_file.write(f"""
   * - .. image:: {IMAGE_RELPATH}/geometry_source_{case_i + 1}.svg

       Input geometry: ``{case['source_wkt']}``

       Error message: ``{case['error']}``

       Error geometry: ``{case['error_wkt']}``
""")

    if case["make_valid_linework_wkt"]:
        rst_file.write(f"""
     - .. image:: {IMAGE_RELPATH}/geometry_make_valid_linework_{case_i + 1}.svg

       ``{case['make_valid_linework_wkt']}``
""")
    else:
        rst_file.write("""
     -
 """)

    if case["make_valid_structure_wkt"]:
        rst_file.write(f"""
     - .. image:: {IMAGE_RELPATH}/geometry_make_valid_structure_{case_i + 1}.svg

       ``{case['make_valid_structure_wkt']}``
""")
    else:
        rst_file.write("""
     -
 """)
