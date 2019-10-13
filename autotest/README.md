# GDAL test suite


## How to run tests

1. You need to install `pytest` to run the test suite. This should do it:

```bash
cd autotest
pip install -r ./requirements.txt
```

2. Then, run tests with:

```bash
pytest
```

3. Some quick usage tips:

```bash
# get more verbose output; don't capture stdout/stdin
pytest -vvs

# run all the gcore tests
pytest gcore/

# run a particular module only
pytest gcore/basic_test.py

# run a particular test case in a module
pytest gcore/basic_test.py::test_basic_test_1
```

Full documentation of pytest at https://docs.pytest.org/en/latest/

## GDAL's tests are not independent

GDAL's test functions are not currently independent of each other. In particular, running individual test functions from a given module may not work. Most tests were originally written with the assumption that entire modules will be run at once.

Practically, this means that you should avoid using:


* pytest's `--last-failed` / `--lf` option (since it runs only failed tests, not the whole module)
* test specifiers that run individual tests (e.g. `pytest gcore/basic_test.py::test_basic_test_1` )
* the xunit plugin to run tests in parallel, unless you also use `--dist=loadfile`. (This may have other issues; untested)

This will hopefully be addressed in the future. When writing new tests, please try to make them independent of each other.

## Writing tests

Tests are automatically discovered in all files under `autotest/`. Tests should be functions with names starting with `test_`.

A few helpers are available in `conftest.py`. In particular, `require_driver` and `require_ogr_driver` skip tests when GDAL is not built with the given driver:

```python
import pytest
import gdaltest
import ogrtest

@pytest.mark.require_driver('JPIPKAK')
def test_jpipkak_driver_1():
    # this test will only run if GDAL is built with the JPIPKAK driver.
    # The driver itself becomes available on the gdaltest or ogrtest module
    # (name is lowercased and spaces replaced with underscores)
    assert gdaltest.jpipkak_drv

@pytest.mark.require_ogr_driver('Mapinfo File')
def test_mapinfo_file_1():
    # (name is lowercased and spaces replaced with underscores)
    assert ogrtest.mapinfo_file_drv
```

This can also be done for an entire test module by setting the `pytestmark` property.

```python
import pytest

pytestmark = pytest.mark.require_driver('JPIPKAK')
```

For more information, see the [pytest docs](https://docs.pytest.org/en/latest/)

## Notes about availability of GDAL sample and test data

The GDAL Team makes every effort to assure that all sample data files
available from GDAL download server (http://download.osgeo.org/gdal/data/) and
test data files used in GDAL Autotest package (https://github.com/OSGeo/gdal/tree/master/autotest)
are available as public and freely redistributable geodata.



--

http://gdal.org/
