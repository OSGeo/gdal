"""
Tests for exposing named arguments in GDAL Python bindings.
"""

import inspect

from osgeo import gdal


def test_OpenEx_named_arguments():
    sig = inspect.signature(gdal.OpenEx)
    params = list(sig.parameters.keys())

    assert params == [
        "utf8_path",
        "nOpenFlags",
        "allowed_drivers",
        "open_options",
        "sibling_files",
    ]
