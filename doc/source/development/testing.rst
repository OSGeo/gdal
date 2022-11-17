.. _testing:

.. include:: ../substitutions.rst

================================================================================
Automated testing
================================================================================

GDAL includes a comprehensive test suite, implemented using a combination of Python (via pytest) and C++ (via TUT).

After building GDAL using CMake, the complete test suite can be run using ``ctest -v --output-on-failure``. This will automatically set environment variables so that tests are run on the
the built version of GDAL, rather than an installed system copy.

Running a subset of tests using ``ctest``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The complete set of test suites known to ``ctest`` can be viewed running ``ctest -N``.

A subset of tests can be run using the ``-R`` argument to ``ctest``, which selects tests using a provided regular expression.
For example, ``ctest -R autotest`` would run the Python-based tests.

The ``-E`` argument can be used to exclude tests using a regular expression. For example, ``ctest -E performance`` would exclude (slow) performance benchmarks.

Running a subset of tests using ``pytest``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The test subsets exposed by ``ctest`` are still rather large and some may take several minutes to run. 
If a higher level of specificity is needed, ``pytest`` can be called directly to run groups of tests or individual tests.
Before running ``pytest``, it is important to set environment variables so that the development build of GDAL is tested,
rather than a system version. This can be done by sourcing the following from the build directory:

.. code-block:: bash

    . ../scripts/setdevenv.sh

(with adjustments to the above path if the build directory is not a subdirectory of the GDAL source root).
To verify that environment variables were set correctly, you can check the version of a GDAL binary:

.. code-block:: bash

    gdalinfo --version
    # GDAL 3.7.0dev-5327c149f5-dirty, released 2018/99/99 (debug build)

and the Python bindings:

.. code-block:: bash

    python3 -c 'from osgeo import gdal; print(gdal.__version__)'
    # 3.7.0dev-5327c149f5-dirty

List tests containing "tiff" in the name:

.. code-block:: bash

    pytest --collect-only autotest -k tiff

Running an individual test file

.. code-block:: bash

    pytest autotest/gcore/vrt_read.py

Running an individual test

.. code-block:: bash

    pytest autotest/gcore/vrt_read.py -k test_vrt_read_non_existing_source

.. warning:: Not all Python tests can be run independently; some tests depend on state set by a previous tests in the same file.