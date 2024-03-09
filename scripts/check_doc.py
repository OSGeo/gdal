#!/usr/bin/python
# Check that driver documentation pages are consistent with driver capabilities

import os

from osgeo import gdal

doc_source = os.path.join(os.path.dirname(os.path.dirname(__file__)), "doc", "source")

map_doc_caps = {}

for subdir in ("raster", "vector"):
    dirname = os.path.join(doc_source, "drivers", subdir)
    for f in os.listdir(dirname):
        filename = os.path.join(dirname, f)
        shortnames = []
        supports_create = False
        supports_createcopy = False
        for l in open(filename, "rt").readlines():
            if l.startswith(".. shortname:: "):
                shortnames.append(l[len(".. shortname:: ") : -1])
            elif "supports_create::" in l:
                supports_create = True
            elif "supports_createcopy::" in l:
                supports_createcopy = True
        for shortname in shortnames:
            d = {}
            if supports_create:
                d["supports_create"] = True
            if supports_createcopy:
                d["supports_createcopy"] = True
            if shortname.upper() in map_doc_caps:
                assert subdir == "vector"
                map_doc_caps["OGR_" + shortname.upper()] = d
            else:
                map_doc_caps[shortname.upper()] = d

for i in range(gdal.GetDriverCount()):
    drv = gdal.GetDriver(i)
    shortname = drv.ShortName
    if shortname in ("BIGGIF", "GTX", "NULL", "GNMFile", "GNMDatabase", "HTTP"):
        continue
    if shortname == "OGR_GMT":
        shortname = "GMT"
    if shortname == "OGR_OGDI":
        shortname = "OGDI"
    if shortname == "NWT_GRC":
        # mixed in same page as NWT_GRD, which confuses our logic
        continue
    if shortname == "SQLite":
        # sqlite.rst and rasterlite2.rst declare both SQLite driver name,
        # which confuses our logic
        continue
    if shortname in ("OpenFileGDB", "NGW"):
        # one page in vector and another one in raster, which confuses our logic
        continue

    assert shortname.upper() in map_doc_caps, drv.ShortName
    doc_caps = map_doc_caps[shortname.upper()]

    if drv.GetMetadataItem(gdal.DCAP_CREATE):
        if shortname == "PDF":
            continue  # Supports Create() but in a very specific mode, hence better not advertizing it
        assert (
            "supports_create" in doc_caps
        ), f"Driver {shortname} declares DCAP_CREATE but doc does not!"
    else:
        if shortname == "HDF4":
            continue  # This is actually the HDF4Image
        assert (
            "supports_create" not in doc_caps
        ), f"Driver {shortname} does not declare DCAP_CREATE but doc does!"

    if drv.GetMetadataItem(gdal.DCAP_CREATECOPY):
        if shortname == "WMTS":
            continue  # Supports CreateCopy() but in a very specific mode, hence better not advertizing it
        assert (
            "supports_createcopy" in doc_caps
        ), f"Driver {shortname} declares DCAP_CREATECOPY but doc does not!"
    else:
        if not drv.GetMetadataItem(gdal.DCAP_CREATE):
            if shortname == "JP2MrSID":
                continue  # build dependent
            assert (
                "supports_createcopy" not in doc_caps
            ), f"Driver {shortname} does not declare DCAP_CREATECOPY but doc does!"
