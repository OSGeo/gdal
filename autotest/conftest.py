# coding: utf-8
from __future__ import absolute_import, division, print_function

import os
import sys

import pytest

from osgeo import gdal

# Put the pymod dir on the path, so modules can `import gdaltest`
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "pymod"))

# put the autotest dir on the path too. This lets us import all test modules
sys.path.insert(1, os.path.dirname(__file__))


# These files may be non-importable, and don't contain tests anyway.
# So we skip searching them during test collection.
collect_ignore = ["kml_generate_test_files.py",
                  "gdrivers/netcdf_cfchecks.py",
                  "gdrivers/generate_bag.py",
                  "gdrivers/generate_fits.py"]

# we set ECW to not resolve projection and datum strings to get 3.x behavior.
gdal.SetConfigOption("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION", "YES")

if 'APPLY_LOCALE' in os.environ:
    import locale
    locale.setlocale(locale.LC_ALL, '')


@pytest.fixture(scope="module", autouse=True)
def chdir_to_test_file(request):
    """
    Changes to the same directory as the test file.
    Also puts that directory at the start of sys.path,
    so that imports of other files in the same directory are easy.

    Tests have grown to expect this.

    NOTE: This happens when the test is *run*, not during collection.
    So test modules must not rely on it at module level.
    """
    old = os.getcwd()

    new_cwd = os.path.dirname(request.module.__file__)
    os.chdir(new_cwd)
    sys.path.insert(0, new_cwd)
    yield
    if sys.path and sys.path[0] == new_cwd:
        sys.path.pop(0)
    os.chdir(old)


def pytest_collection_modifyitems(config, items):
    # skip tests with @pytest.mark.require_driver(name) when the driver isn't available
    skip_driver_not_present = pytest.mark.skip("Driver not present")
    # skip test with @ptest.mark.require_run_on_demand when RUN_ON_DEMAND is not set
    skip_run_on_demand_not_set = pytest.mark.skip("RUN_ON_DEMAND not set")
    import gdaltest

    drivers_checked = {}
    for item in items:
        for mark in item.iter_markers('require_driver'):
            driver_name = mark.args[0]
            if driver_name not in drivers_checked:
                driver = gdal.GetDriverByName(driver_name)
                drivers_checked[driver_name] = bool(driver)
                if driver:
                    # Store the driver on gdaltest module so test functions can assume it's there.
                    setattr(gdaltest, '%s_drv' % driver_name.lower(), driver)
            if not drivers_checked[driver_name]:
                item.add_marker(skip_driver_not_present)
        if not gdal.GetConfigOption('RUN_ON_DEMAND'):
            for mark in item.iter_markers('require_run_on_demand'):
                item.add_marker(skip_run_on_demand_not_set)
