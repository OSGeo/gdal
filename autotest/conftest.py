# coding: utf-8
from __future__ import absolute_import, division, print_function

import os
import sys

import pytest

from osgeo import gdal, ogr, osr

# Explicitly enable exceptions since autotest/ now assumes them to be
# enabled
gdal.UseExceptions()

# Put the pymod dir on the path, so modules can `import gdaltest`
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "pymod"))

# put the autotest dir on the path too. This lets us import all test modules
sys.path.insert(1, os.path.dirname(__file__))


# These files may be non-importable, and don't contain tests anyway.
# So we skip searching them during test collection.
collect_ignore = [
    "kml_generate_test_files.py",
    "gdrivers/netcdf_cfchecks.py",
    "gdrivers/generate_bag.py",
    "gdrivers/generate_fits.py",
]
collect_ignore_glob = ["pymod/*.py"]

if "APPLY_LOCALE" in os.environ:
    import locale

    locale.setlocale(locale.LC_ALL, "")


def setup_proj_search_paths():

    from osgeo import osr

    proj_grids_path = os.path.join(os.path.dirname(__file__), "proj_grids")
    assert os.path.exists(proj_grids_path)

    proj_db_tmpdir = os.path.join(
        os.path.dirname(__file__), "gcore", "tmp", "proj_db_tmpdir"
    )
    proj_db_tmpdir_filename = os.path.join(proj_db_tmpdir, "proj.db")
    src_proj_db_filename = None
    for path in osr.GetPROJSearchPaths():
        if os.path.exists(os.path.join(path, "proj.db")):
            src_proj_db_filename = os.path.join(path, "proj.db")
            break

    if src_proj_db_filename is None:
        print("Cannot find source proj.db")
        sys.exit(1)

    if (
        not os.path.exists(proj_db_tmpdir_filename)
        or os.stat(proj_db_tmpdir_filename).st_mtime
        < os.stat(src_proj_db_filename).st_mtime
        or os.stat(proj_db_tmpdir_filename).st_size
        != os.stat(src_proj_db_filename).st_size
    ):
        import shutil

        from filelock import FileLock

        # We need to do the copy of proj.db from its source directory to
        # gcore/tmp/proj_db_tmpdir under a lock to prevent pytest invocations
        # run concurrently to overwrite in parallel, leading to PROJ being
        # confused by the file being overwritten after opening, whereas PROJ
        # assumes it to be immutable.
        lock = FileLock(proj_db_tmpdir + ".lock")
        with lock:
            if (
                not os.path.exists(proj_db_tmpdir_filename)
                or os.stat(proj_db_tmpdir_filename).st_mtime
                < os.stat(src_proj_db_filename).st_mtime
                or os.stat(proj_db_tmpdir_filename).st_size
                != os.stat(src_proj_db_filename).st_size
            ):
                print("Copying %s to %s" % (src_proj_db_filename, proj_db_tmpdir))
                if not os.path.exists(proj_db_tmpdir):
                    os.mkdir(proj_db_tmpdir, 0o755)
                shutil.copy(src_proj_db_filename, proj_db_tmpdir)

    assert os.path.exists(proj_db_tmpdir_filename)
    osr.SetPROJSearchPaths([proj_db_tmpdir, proj_grids_path])


setup_proj_search_paths()


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
    # skip test with @ptest.mark.require_run_on_demand when RUN_ON_DEMAND is not set
    skip_run_on_demand_not_set = pytest.mark.skip("RUN_ON_DEMAND not set")
    import gdaltest
    import ogrtest

    drivers_checked = {}
    # Note: when adding a new custom marker, document it in cmake/template/pytest.ini.in
    for item in items:
        for mark in item.iter_markers("require_driver"):
            driver_name = mark.args[0]
            if driver_name not in drivers_checked:
                driver = gdal.GetDriverByName(driver_name)
                drivers_checked[driver_name] = bool(driver)
                if driver:
                    # Store the driver on gdaltest module so test functions can assume it's there.
                    setattr(gdaltest, "%s_drv" % driver_name.lower(), driver)
            if not drivers_checked[driver_name]:
                # skip tests with @pytest.mark.require_driver(name) when the driver isn't available
                item.add_marker(pytest.mark.skip(f"Driver {driver_name} not present"))
        if not gdal.GetConfigOption("RUN_ON_DEMAND"):
            for mark in item.iter_markers("require_run_on_demand"):
                item.add_marker(skip_run_on_demand_not_set)

        for mark in item.iter_markers("slow"):
            if not gdaltest.run_slow_tests():
                item.add_marker(pytest.mark.skip("GDAL_RUN_SLOW_TESTS not set"))

        for mark in item.iter_markers("require_creation_option"):

            driver, option = mark.args

            drv = gdal.GetDriverByName(driver)
            if drv is None:
                item.add_marker(
                    pytest.mark.skip(f"{driver} driver is not included in this build")
                )
            elif option not in drv.GetMetadata()["DMD_CREATIONOPTIONLIST"]:
                item.add_marker(
                    pytest.mark.skip(
                        f"{driver} creation option {option} not supported in this build"
                    )
                )

        for mark in item.iter_markers("require_geos"):
            if not ogrtest.have_geos():
                item.add_marker(pytest.mark.skip("GEOS not available"))

            required_version = (
                mark.args[0] if len(mark.args) > 0 else 0,
                mark.args[1] if len(mark.args) > 1 else 0,
                mark.args[2] if len(mark.args) > 2 else 0,
            )

            actual_version = (
                ogr.GetGEOSVersionMajor(),
                ogr.GetGEOSVersionMinor(),
                ogr.GetGEOSVersionMicro(),
            )

            if actual_version < required_version:
                item.add_marker(
                    pytest.mark.skip(
                        f"Requires GEOS >= {'.'.join(str(x) for x in required_version)}"
                    )
                )

        for mark in item.iter_markers("require_proj"):
            required_version = (
                mark.args[0],
                mark.args[1] if len(mark.args) > 1 else 0,
                mark.args[2] if len(mark.args) > 2 else 0,
            )

            actual_version = (
                osr.GetPROJVersionMajor(),
                osr.GetPROJVersionMinor(),
                osr.GetPROJVersionMicro(),
            )

            if actual_version < required_version:
                item.add_marker(
                    pytest.mark.skip(
                        f"Requires PROJ >= {'.'.join(str(x) for x in required_version)}"
                    )
                )

        for mark in item.iter_markers("require_curl"):
            if not gdaltest.built_against_curl():
                item.add_marker(pytest.mark.skip("curl support not available"))


def pytest_addoption(parser):
    parser.addini("gdal_version", "GDAL version for which pytest.ini was generated")


def pytest_configure(config):
    test_version = config.getini("gdal_version")
    lib_version = gdal.__version__

    if not lib_version.startswith(test_version):
        raise Exception(
            f"Attempting to run tests for GDAL {test_version} but library version is "
            f"{lib_version}. Do you need to run setdevenv.sh ?"
        )


def pytest_report_header(config):
    gdal_header_info = "GDAL Build Info:"
    for item in gdal.VersionInfo("BUILD_INFO").strip().split("\n"):
        gdal_header_info += "\n  " + item.replace("=", ": ")

    import gdaltest

    gdal_download_test_data = gdal.GetConfigOption("GDAL_DOWNLOAD_TEST_DATA")
    if gdal_download_test_data is None:
        gdal_download_test_data = "undefined"
    gdal_header_info += f"\nGDAL_DOWNLOAD_TEST_DATA: {gdal_download_test_data}"
    if not gdaltest.download_test_data():
        gdal_header_info += " (tests relying on downloaded data may be skipped)"

    gdal_run_slow_tests = gdal.GetConfigOption("GDAL_RUN_SLOW_TESTS")
    if gdal_run_slow_tests is None:
        gdal_run_slow_tests = "undefined"
    gdal_header_info += f"\nGDAL_RUN_SLOW_TESTS: {gdal_run_slow_tests}"
    if not gdaltest.run_slow_tests():
        gdal_header_info += ' (tests marked as "slow" will be skipped)'

    return gdal_header_info
