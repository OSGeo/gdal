#!/usr/bin/env python3

import os
import time

import requests

TOKEN = open(os.path.expanduser("~/.rtd"), "r").read().strip()
HEADERS = {"Authorization": f"token {TOKEN}"}


def get_redirects():
    url = "https://readthedocs.org/api/v3/projects/gdal/redirects/"
    results = []

    while url:
        response = requests.get(url, headers=HEADERS).json()
        results += response["results"]
        url = response["next"]

    return results


def delete_redirect(redirect_id):
    url = f"https://readthedocs.org/api/v3/projects/gdal/redirects/{redirect_id}"
    res = requests.delete(url, headers=HEADERS)
    print(res.status_code)
    if res.status_code == 429:
        time.sleep(3)
        delete_redirect(redirect_id)


def create_redirect(data):
    url = "https://readthedocs.org/api/v3/projects/gdal/redirects/"
    res = requests.post(url, json=data, headers=HEADERS)
    print(res.status_code)
    if res.status_code == 429:
        time.sleep(3)
        create_redirect(data)


existing_redirects = get_redirects()

redirects = [
    {"from_url": "/gdal_vrttut.html", "to_url": "/drivers/raster/vrt.html"},
    {
        "from_url": "/gdal_virtual_file_systems.html",
        "to_url": "/user/virtual_file_systems.html",
    },
    {"from_url": "/ogr_feature_style.html", "to_url": "/user/ogr_feature_style.html"},
    {"from_url": "/python/index.html", "to_url": "/api/index.html#python-api"},
    {"from_url": "/ogr_apitut.html", "to_url": "/tutorials/vector_api_tut.html"},
    {"from_url": "/ogr/ogr_arch.html", "to_url": "/user/vector_data_model.html"},
    {"from_url": "/ogr_arch.html", "to_url": "/user/vector_data_model.html"},
    {
        "from_url": "build_hints.html",
        "to_url": "/development/building_from_source.html",
    },
    # driver indexes
    {"from_url": "/frmt_various.html", "to_url": "/drivers/raster"},
    {"from_url": "/formats_list.html", "to_url": "/drivers/raster"},
    {"from_url": "/ogr_formats.html", "to_url": "/drivers/vector"},
    # special cases to forward to driver pages
    {"from_url": "/drv_geopackage_raster.html", "to_url": "/drivers/raster/gpkg.html"},
    {
        "from_url": "/geopackage_aspatial.html",
        "to_url": "/drivers/vector/geopackage_aspatial.html",
    },
    {"from_url": "/drv_geopackage.html", "to_url": "/drivers/vector/gpkg.html"},
    {"from_url": "/drv_shape.html", "to_url": "/drivers/vector/shapefile.html"},
    {"from_url": "/drv_wfs3.html", "to_url": "/drivers/vector/oapif.html"},
    {
        "from_url": "/drivers/vector/geopackage.html",
        "to_url": "/drivers/vector/gpkg.html",
    },
    {"from_url": "/drivers/vector/wfs3.html", "to_url": "/drivers/vector/oapif.html"},
    # pdf download
    {"from_url": "/gdal.pdf", "to_url": "/_/downloads/en/latest/pdf/", "type": "exact"},
    # wildcards to forward to driver pages
    {"from_url": "/drv_*", "to_url": "/drivers/vector/:splat"},
    {"from_url": "/frmt_*", "to_url": "/drivers/raster/:splat"},
]

programs = [
    "gdal2tiles",
    "gdaladdo",
    "gdalbuildvrt",
    "gdal_calc",
    "gdalcompare",
    "gdal-config",
    "gdal_contour",
    "gdaldem",
    "gdal_edit",
    "gdal_fillnodata",
    "gdal_grid",
    "gdalinfo",
    "gdallocationinfo",
    "gdalmanage",
    "gdal_merge",
    "gdalmove",
    "gdal_pansharpen",
    "gdal_polygonize",
    "gdal_proximity",
    "gdal_rasterize",
    "gdal_retile",
    "gdal_sieve",
    "gdalsrsinfo",
    "gdaltindex",
    "gdaltransform",
    "gdal_translate",
    "gdalwarp",
    "nearblack",
    "rgb2pct",
    "pct2rgb",
    "ogr2ogr",
    "ogrinfo",
    "ogrlineref",
    "ogrmerge",
    "ogrtindex",
    "gnmanalyse",
    "gnmmanage",
]

for program in programs:
    redirects.append(
        {"from_url": f"/{program}.html", "to_url": f"/programs/{program}.html"}
    )

# prepend /en/latest/ to pre-RTD URLs
redirects.append({"from_url": "/*", "to_url": "/en/latest/:splat", "type:": "exact"})

for redirect in existing_redirects:
    print(f'Deleting redirect from {redirect["from_url"]} to {redirect["to_url"]}')
    delete_redirect(redirect["pk"])

for i, redirect in enumerate(redirects):
    data = redirect.copy()
    data["http_status"] = 301
    data["type"] = data.get("type", "page")
    data["position"] = i

    print(f'Creating redirect from {redirect["from_url"]} to {redirect["to_url"]}')
    create_redirect(data)
