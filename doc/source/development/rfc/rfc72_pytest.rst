.. _rfc-72:

===========================================
RFC 72: Update autotest suite to use pytest
===========================================

======== ===============================
Author:  Craig de Stigter
Contact: craig.destigter@koordinates.com
Started: 2018-Sep-27
Status:  *Implemented in GDAL 2.4*
======== ===============================

Summary
-------

The document proposes and describes conversion of the existing Python
autotest suite to use the `pytest
framework <https://docs.pytest.org/en/latest/>`__.

Using pytest provides significant productivity gains for writing,
reading and debugging python tests, compared with the current home-grown
approach.

Motivation
----------

The current autotest framework dates back to 2007 (at least), and while
reasonably comprehensive (and 186,000 lines of Python) is difficult for
developers to use and extend.

-  As a homegrown framework it'll never get any better than the effort
   GDAL developers put in. For example: reporting, test coverage,
   parallelisation, resumption, log/output handling, parameterisation.
-  Test failures are typically only as descriptive as "fail",
   determining the cause requires editing the tests.
-  It is difficult to run/rerun individual tests
-  The tests often assume a set of compile options that may not be valid
   for the local build.
-  Tests are patched/disabled in various CI environments by scripts
   outside the test tree. This is opaque to developers working locally.
-  Some tests depend on each other and a specific execution order,
   making it difficult to debug and extend.
-  Shared functionality is repeated across tests and modules
-  Tests are typically only written for new functionality, not
   regressions. (Crudely, from the 2663 commits in the last year only
   725 touched the autotest tree)

By adopting an OSS test framework in widespread use we can leverage the
ecosystem to provide GDAL with benefits and improvements going forward.
The utility of automated testing has been proven for GDAL, and we need
to make test writing as easy as possible.

Proposal
--------

Port the existing Python autotest suite to use the `pytest
framework <https://docs.pytest.org/en/latest/>`__. Why pytest? It's in
widespread use, has a wide set of features, is extensible via plugins,
and focuses on making writing and debugging tests as easy as possible -
minimising boilerplate code and maximising reuse. `This
presentation <http://thesoftjaguar.com/pres_pytest.html>`__ (despite
dating back to 2014) gives a brief overview of the key benefits.

Do the bulk of this port using automated code refactoring tools so the
autotest suite matches the preferred pytest approach. While pytest does
support all sorts of custom test collection and execution methods, in
order to increase the benefits to developers going forward we should do
a proper conversion. Initial goal is to get the tests ported, remove as
much boilerplate as feasible, all while keeping the existing CI green.
Future goals are to continue to reduce boilerplate code and increase
isolation between tests.

At a minimum we still need to preserve the existing ability to:

-  Run all existing CI tests in all environments using the existing
   configuration
-  Run individual test modules
-  Support existing subprocess/multiprocess tests
-  Support testing under Python 2.7 & Python 3
-  Stacktraces for assertion failures

The new test suite will be in place for the GDAL 2.4.0 release in
December 2018. Changes will not be backported to the 2.3.x or earlier
release branches.

References:

-  `issue #949 <https://github.com/OSGeo/gdal/issues/949>`__.
-  `gdal-dev
   post <https://lists.osgeo.org/pipermail/gdal-dev/2018-October/049081.html>`__,
   Oct 2018

Example
~~~~~~~

A typical existing GDAL python unit test:

::

   def test_gdaladdo_1():
       if test_cli_utilities.get_gdaladdo_path() is None:
           return 'skip'

       shutil.copy('../gcore/data/mfloat32.vrt', 'tmp/mfloat32.vrt')
       shutil.copy('../gcore/data/float32.tif', 'tmp/float32.tif')

       (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaladdo_path() + ' tmp/mfloat32.vrt 2 4')
       if not (err is None or err == ''):
           gdaltest.post_reason('got error/warning')
           print(err)
           return 'fail'

       ds = gdal.Open('tmp/mfloat32.vrt')
       ret = tiff_ovr.tiff_ovr_check(ds)
       ds = None

       os.remove('tmp/mfloat32.vrt')
       os.remove('tmp/mfloat32.vrt.ovr')
       os.remove('tmp/float32.tif')

       return ret

Could *eventually* become something like this

::

   @pytest.mark.require_files('gcore/data/mfloat32.vrt', 'gcore/data/float32.tif')
   def test_gdaladdo_1(gdaladdo):
       gdaladdo('gcore/data/mfloat32.vrt 2 4')
       assert os.path.exists('gcore/data/mfloat32.vrt.ovr')

       tiff_ovr.tiff_ovr_check(gdal.Open('mfloat32.vrt'))

It's a lot clearer what it is actually testing, and all support
functionality is handled by shared-use fixtures (``gdaladdo`` &
``require_files``), including cleanup and conditional-skipping.

Test output
~~~~~~~~~~~

Pytest out-of-the-box produces readable output, and is augmented by the
``pytest-sugar`` plugin which makes it even nicer:

-  Successful tests don't produce much output (a single ``.`` or ``✓``
   per test, by default)
-  Failed tests produce a traceback. Any logs, stdout and stderr
   produced by the failing tests are printed too. This is a great start
   for debugging the cause of the failure.
-  Any expressions used in failing asserts are printed.
-  Test output is clearly colourised (red/green) if the terminal
   supports it.

![](pytest-output-example.png, 626px, center)

Plan Phase 1
------------

Progress at `pull request
963 <https://github.com/OSGeo/gdal/pull/963>`__.

-  Using code automation, convert the existing Python autotest suite to
   use pytest-style assertions.

-  rename all tests to ``test_*()``. Pytest finds tests by matching
   names against a regex and this is the default regex.

-  generate assertions from ``post_reason()``/``return 'fail'`` calls
   where possible

-  replace all ``skip``/``fail``/``success`` return values

-  remove extra ``../pymod`` entries from ``sys.path``. All tests now
   run in a single process

-  remove ``__main__`` block and ``gdaltest_list`` from test files

-  these collectively achieve better test collection/selection, output
   capturing, and improved assertions and reporting

-  Manually convert the dynamically-generated tests to use
   `parametrization <https://docs.pytest.org/en/latest/parametrize.html>`__

-  Ensure the slow/internet tests are still marked as such and skipped
   by default.

-  Use `pytest-sugar <https://pivotfinland.com/pytest-sugar/>`__ to make
   test output pretty. Disable it in CI since it doesn't work well with
   travis CI's output buffering.

-  Move environment-specific test-skipping from CI to the test suite,
   possibly with additional tag/marks.

-  Ensure the existing CI tests pass & debug any failures

-  Add documentation and a straightforward install process for pytest
   itself

Notable changes and their implications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  tests are now run with ``cd autotest ; pytest``. (The first time you
   may need to ``pip install -r requirements.txt`` to install pytest)
-  All tests now run in a single process (they were previously forked
   for each test module). This means that:

   -  errors during test collection are now loud, and immediately fail
      the entire test run with a traceback. Previously things like
      syntax errors in files and errors at module level were easy to
      miss.
   -  a single segfault will kill the entire test run dead.

-  It's now possible to run individual tests, instead of just entire
   files. However, tests are *not yet independent of each other*. So
   that might cause the tests to behave differently than if you ran the
   whole module.
-  ``test_py_scripts.run_py_script`` was modified to always run the
   script as a subprocess. The stdout capturing of the original method
   did strange things with pytest. This change broke some tests that
   relied on passing files in the ``/vsimem/`` root to scripts, so those
   have been changed to use the ``tmp/`` root instead.
-  no test suite support for Python <2.7

.. _plan-phase-2--future-work:

Plan Phase 2 / Future Work
--------------------------

-  Improving test isolation, so running an entire module at a time isn't
   required.
-  Removing the global ``gdaltest.<drivername>_drv`` variables and
   replace them with pytest fixtures.
-  Use fixtures for temporary file handling and cleanup
-  More automated test skipping based on what's actually compiled.
-  Automated style cleanup using
   `Black <https://github.com/ambv/black>`__.
-  Consider parallelising test runs by default (there are several
   `plugins available <https://github.com/pytest-dev/pytest-xdist>`__
   for this)

Voting history
--------------

Adopted with the following votes from PSC members:

-  +1 from EvenR, DanielM, HowardB and KurtS
-  +0 from JukkaR
