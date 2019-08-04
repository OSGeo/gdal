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

DO_RUN_ON_DEMAND = gdal.GetConfigOption('RUN_ON_DEMAND')


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


skip_driver_not_present = pytest.mark.skip("Driver not present")
skip_run_on_demand_not_set = pytest.mark.skip("RUN_ON_DEMAND not set")


_marker_behaviour = {}


def marker_with_behaviour(func):
    """
    Register a function to add behaviour to a given marker.
    The function is called during collection for each item iff
    the item has a mark with the same name as the function (with leading 'marker_' removed)
    """
    name = func.__name__
    assert name.startswith('marker_')
    name = name[7:]
    _marker_behaviour[name] = func
    return func


@marker_with_behaviour
def marker_require_driver(mark, item):
    """
    skip tests with @pytest.mark.require_driver(name) when the driver isn't available
    """
    import gdaltest
    drivers_checked = {}
    for driver_name in mark.args:
        if driver_name not in drivers_checked:
            driver = gdal.GetDriverByName(driver_name)
            if driver:
                # Store the driver on gdaltest module so test functions can assume it's there.
                setattr(gdaltest, '%s_drv' % driver_name.lower().replace(' ', '_'), driver)
            drivers_checked[driver_name] = bool(driver)
        if not drivers_checked[driver_name]:
            item.add_marker(skip_driver_not_present)


@marker_with_behaviour
def marker_require_ogr_driver(mark, item):
    """
    skip tests with @pytest.mark.require_ogr_driver(name) when the driver isn't available
    """
    import ogrtest
    drivers_checked = {}
    for driver_name in mark.args:
        if driver_name not in drivers_checked:
            driver = ogr.GetDriverByName(driver_name)
            if driver:
                # Store the driver on gdaltest module so test functions can assume it's there.
                setattr(ogrtest, '%s_drv' % driver_name.lower().replace(' ', '_'), driver)
            drivers_checked[driver_name] = bool(driver)
        if not drivers_checked[driver_name]:
            item.add_marker(skip_driver_not_present)


@marker_with_behaviour
def marker_require_run_on_demand(mark, item):
    """
    skip test with @pytest.mark.require_run_on_demand when RUN_ON_DEMAND is not set
    """
    if DO_RUN_ON_DEMAND:
        item.add_marker(skip_run_on_demand_not_set)


@marker_with_behaviour
def marker_config_options(mark, item):
    """
    Add the _apply_config_options fixture, which does all the actual work
    """
    item.fixturenames.append('_apply_config_options')


def pytest_collection_modifyitems(config, items):
    for item in items:
        for mark in item.iter_markers():
            if mark.name in _marker_behaviour:
                # Apply behaviour for our custom markers
                # (e.g. skip things with require_driver if the driver isn't present)
                _marker_behaviour[mark.name](mark, item)
