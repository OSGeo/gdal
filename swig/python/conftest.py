import ast
import doctest
import functools

import numpy as np
import pytest

from osgeo import gdal, ogr, osr


###
# Make modules available to doctests
#
# Adding modules to doctest_namespace allows us to avoid writing "from osgeo import gdal"
# in every example.
###
@pytest.fixture(scope="session", autouse=True)
def global_setup(doctest_namespace):
    doctest_namespace["gdal"] = gdal
    doctest_namespace["ogr"] = ogr
    doctest_namespace["osr"] = osr
    doctest_namespace["np"] = np
    doctest_namespace["pytest"] = pytest


###
# Custom output checker
#
# doctest relies on an OutputChecker object to determine if test outputs
# match their expected values. pytest has its own OutputChecker implementation
# but does not provide any hooks for further customizations. So we inject a
# custom checker directly into pytest's internals.
###

import _pytest.doctest

base_checker = _pytest.doctest._get_checker()


class GDALOutputChecker(doctest.OutputChecker):
    @staticmethod
    def _strip_comments(want):
        lines = want.split("\n")
        for i in range(len(lines)):
            comment_pos = lines[i].find("#")
            if comment_pos != -1:
                lines[i] = lines[i][:comment_pos]
        return "\n".join(lines)

    @staticmethod
    def _strip_warnings(want):
        lines = want.split("\n")
        return "\n".join(x for x in lines if not x.startswith("Warning"))

    def check_output(self, want, got, flags):
        want = self._strip_warnings(want)

        # custom checking method for arrays
        # pytest's built-in NUMBER flag is not suitable because
        # it only allows differences to the right of the decimal
        # point.
        if "rtol:" in want:
            pos = want.find("rtol:")
            rtol = float(want[(pos + 5) :].strip())
            want = np.array(ast.literal_eval(want))
            got = np.array(ast.literal_eval(got))
            return np.allclose(want, got, rtol=rtol)

        if "# random" in want:
            want = np.array(ast.literal_eval(want))
            got = np.array(ast.literal_eval(got))
            return np.array_equal(want.shape, got.shape)

        if "# no-check" in want:
            return True

        want = self._strip_comments(want)

        return base_checker.check_output(want, got, flags)


_pytest.doctest.CHECKER_CLASS = GDALOutputChecker

###
# Path substitutions
#
# We want to write concise documentation using short filenames like gdal.Open("byte.tif")
# Modifying the working directory before each doctest is tedious and error-prone.
# Instead we store a list of abbreviated filenames and monkey-patch GDAL functions to
# substitute the full path name before running the example.
###

files = {
    "byte.tif": "autotest/gcore/data/byte.tif",
    "my.tif": "autotest/gcore/data/byte.tif",
    "in.tif": "autotest/gcore/data/byte.tif",
    "rgbsmall.tif": "autotest/gcore/data/rgbsmall.tif",
    "poly.shp": "autotest/ogr/data/poly.shp",
    "testrat.tif": "autotest/gcore/data/gtiff/testrat.tif",
}


def nested_replace(x):
    if type(x) is int or type(x) is bool:
        return x
    if type(x) is str:
        return files.get(x, x)
    if type(x) is dict:
        return {k: nested_replace(v) for k, v in x.items()}
    if type(x) is tuple:
        return (nested_replace(v) for v in x)
    if type(x) is list:
        return [nested_replace(v) for v in x]
    if isinstance(x, (gdal.Algorithm, gdal.Dataset)):
        return x

    raise Exception(f"Unsupported argument type: {type(x)}")


def expand_dsn(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        return f(*nested_replace(args), **nested_replace(kwargs))

    return wrapper


@pytest.fixture(autouse=True)
def gdal_find_files(monkeypatch):

    monkeypatch.setattr(gdal, "Open", expand_dsn(gdal.Open))
    monkeypatch.setattr(gdal, "OpenEx", expand_dsn(gdal.OpenEx))
    monkeypatch.setattr(gdal, "Run", expand_dsn(gdal.Run))
    monkeypatch.setattr(
        gdal.Algorithm,
        "ParseCommandLineArguments",
        expand_dsn(gdal.Algorithm.ParseCommandLineArguments),
    )
    monkeypatch.setattr(
        gdal.Algorithm, "__setitem__", expand_dsn(gdal.Algorithm.__setitem__)
    )
