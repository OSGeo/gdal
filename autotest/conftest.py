# coding: utf-8
from __future__ import absolute_import, division, print_function

import contextlib
import os
import sys

import pytest

from osgeo import gdal, ogr

# Put the pymod dir on the path, so modules can `import gdaltest`
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "pymod"))

# put the autotest dir on the path too. This lets us import all test modules
sys.path.insert(1, os.path.dirname(__file__))


# These files may be non-importable, and don't contain tests anyway.
# So we skip searching them during test collection.
collect_ignore = ["kml_generate_test_files.py", "gdrivers/netcdf_cfchecks.py"]

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

    os.chdir(os.path.dirname(request.module.__file__))
    sys.path.insert(0, ".")
    yield
    if sys.path and sys.path[0] == ".":
        sys.path.pop(0)
    os.chdir(old)


@pytest.fixture
def _apply_config_options(request):
    """
    Applies config options, and reverses them afterwards.
    This fixture is applied by:

        @pytest.mark.config_options(...)
    """
    from gdaltest import config_options
    try:
        contextlib.ExitStack
    except AttributeError:
        # python <3.3  :(
        # (contextlib.nested *sort of* handles this in 2.7, but it's not in 3.2, so let's just hack it)
        contexts = []
        for mark in request.node.iter_markers('config_options'):
            c = config_options(*mark.args, **mark.kwargs)
            contexts.append(c)
            c.__enter__()
        try:
            yield
        finally:
            for c in contexts:
                c.__exit__(*sys.exc_info())
    else:
        with contextlib.ExitStack() as stack:
            for mark in request.node.iter_markers('config_options'):
                stack.enter_context(config_options(*mark.args, **mark.kwargs))
            yield


def pytest_collection_modifyitems(config, items):
    # skip tests with @pytest.mark.require_driver(name) when the driver isn't available
    skip_driver_not_present = pytest.mark.skip("Driver not present")
    # skip test with @pytest.mark.require_run_on_demand when RUN_ON_DEMAND is not set
    skip_run_on_demand_not_set = pytest.mark.skip("RUN_ON_DEMAND not set")

    import gdaltest
    import ogrtest

    for item in items:
        for mark_name in ('require_driver', 'require_ogr_driver'):
            drivers_checked = {}
            for mark in item.iter_markers(mark_name):
                driver_name = mark.args[0]
                if driver_name not in drivers_checked:
                    if mark_name == 'require_ogr_driver':
                        # ogr and gdal drivers are a bit different.
                        test_module = ogrtest
                        driver = ogr.GetDriverByName(driver_name)
                    else:
                        test_module = gdaltest
                        driver = gdal.GetDriverByName(driver_name)
                    if driver:
                        # Store the driver on gdaltest module so test functions can assume it's there.
                        setattr(test_module, '%s_drv' % driver_name.lower().replace(' ', '_'), driver)
                    drivers_checked[driver_name] = bool(driver)
                if not drivers_checked[driver_name]:
                    item.add_marker(skip_driver_not_present)
        if not gdal.GetConfigOption('RUN_ON_DEMAND'):
            for mark in item.iter_markers('require_run_on_demand'):
                item.add_marker(skip_run_on_demand_not_set)
        for mark in item.iter_markers('config_options'):
            item.fixturenames.append('_apply_config_options')
