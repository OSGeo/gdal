.. _rfc-88:

=============================================================
RFC 88: Use GoogleTest framework for C/C++ unit tests
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2022-Nov-16
Status:        Adopted, implemented
Target:        GDAL 3.7
============== =============================================

Summary
-------

The document proposes and describes conversion of the existing C/C++
autotest suite to use the `GoogleTest
framework <https://github.com/google/googletest>`__.

GoogleTest is a popular and maintained framework for C/C++ test
writing, that is a better replacement for the `TUT framework
<https://github.com/mrzechonek/tut-framework>`__ that we use currently.

Motivation
----------

The current TUT framework was adopted in 2006, and copied from the GEOS
project. While it works reasonably well, its use has the following drawbacks:

- TUT has little upstream activity. The last tagged release was apparently in
  2016.

- We have an internal copy in our repository that might have diverged.

- TUT is, to the best of our understanding, not very popular among C/C++ projects
  that have a regression test suite, which adds a (small) learning curve to
  new contributors.

- One of TUT main practical disadvantages is that it is not obvious to know which
  assertion has failed in a given test, unless adding a text string
  describing the assertion, and often redundant with it.
  Like ``ensure_equals("x = 2", x, 2);`` or some number like
  ``ensure_equals("(1)", x, 2);``. In GoogleTest, you just write
  ``EXPECT_EQ(x, 2);`` and if it fails the filename and line number will be
  printed in the error log, as well as the value of ``x``.
  With GoogleTest, you may also provide additional context that will be output
  when an assertion fails to help diagnose the reason of the failure, e.g.:
  ``EXPECT_EQ(x, 2) << "if that matters, y = " << y;``

- Another issue I stumbled upon a few months ago is that the default limit for a
  given test group is 50 steps. If you don't increase it explicitly, tests beyond
  the 50th one will be compiled but not run, without any compile or runtime
  failure. One might easily believe they are successful when they are not run at all!

A few GoogleTest advantages:

- It has been adopted by PROJ as its C/C++ test framework, and it makes sense
  to use the same technologies whenever it makes sense given the relationship of
  the 2 projects.

- It has extensive documentation: https://google.github.io/googletest/

- GoogleTest distinguishes assertions that must abort the test when failed
  (ASSERT_xxxx) from those where the test can continue (EXPECT_xxxx), which can
  be useful.

The only noticed inconvenience of GoogleTest-based tests is that they might be
quite slow to compile in optimized mode (probably related to
https://github.com/google/googletest/issues/1478). I've found that forcing
non-optimized mode when building tests reduces build time of tests to a reasonable
amount, while not having practical disadvantages: it still allows to test the
library built in optimized mode. This is what is done in the candidate
implementation.

This RFC doesn't change the logic of deciding when C/C++-based vs Python-based
tests should be written. My own position is that Python-based tests should be
preferred when possible, as productivity is higher in Python and there is no
associated compilation time (compilation time affects feedback received from
continuous integration).
C/C++-based test should be reserved for C++-specific aspects that cannot be tested
with the SWIG Python bindings, which use the C interface. For example testing
of C++ operators (copy/move constructors/assignment operators, iterator interfaces,
etc.) or C/C++ functionality not mapped to SWIG (e.g. CPL utility functions/classes)

Technical details
-----------------

GoogleTest >= 1.10 will be required. If available in the system, the system
GoogleTest library will be used. Otherwise the CMake ExternalProject_Add()
functionality is used to download and built it transparently. Use of system vs
downloaded framework can be controlled with the USE_EXTERNAL_GTEST=YES/NO CMake
variable. This logic is copied & pasted from PROJ.

All assertion based tests will be migrated to GoogleTest. The few performance
tests that don't have any assertions and do not use TUT will be kept unmodified
(at least for now).

The autotest/cpp/tut directory will be removed.

The migration from TUT to GoogleTest is mostly mechanical:

- TUT groups                   ==> GoogleTest test suite or fixture
- ``ensure_equals(a,b)``       ==> ``ASSERT_EQ(a,b)`` or ``EXPECT(a,b)``
- ``ensure(a)``                ==> ``ASSERT_TRUE(a)`` or ``EXPECT(a)``
- ``ensure_almost_equal(a,b)`` ==> ``ASSERT_NEAR(a,b,tolerance)`` or ``EXPECT_NEAR(a,b,tolerance)``
- ``ensure(a >= b)``           ==> ``ASSERT_GE(a,b)`` or ``EXPECT_GE(a,b)``
- etc.

Customized ensure logic like ``ensure_equal_geometries(geom1, geom2)`` can
also be migrated cleanly as functions that result an AssertionResult:
https://google.github.io/googletest/advanced.html#using-a-function-that-returns-an-assertionresult

``GTEST_SKIP() << "reason for the skip"`` can be used to skip tests for
which a build-time or run-time requirement is missing.

GoogleTest also offers capabilities for parametrized tests that were not existing
in TUT and can be adopted where it makes sense.

For example the following test in TUT that tested GDALDataTypeUnion() on
all potential combinations of (datatype1, datatype2)

.. code-block:: c++

    // Test GDALDataTypeUnion() on all (GDALDataType, GDALDataType) combinations
    template<> template<> void object::test<6>()
    {
        for(int i=GDT_Byte;i<GDT_TypeCount;i++)
        {
            for(int j=GDT_Byte;j<GDT_TypeCount;j++)
            {
                GDALDataType eDT1 = static_cast<GDALDataType>(i);
                GDALDataType eDT2 = static_cast<GDALDataType>(j);
                GDALDataType eDT = GDALDataTypeUnion(eDT1,eDT2 );
                ENSURE( eDT == GDALDataTypeUnion(eDT2,eDT1) );
                ENSURE( GDALGetDataTypeSize(eDT) >= GDALGetDataTypeSize(eDT1) );
                ENSURE( GDALGetDataTypeSize(eDT) >= GDALGetDataTypeSize(eDT2) );
                ENSURE( (GDALDataTypeIsComplex(eDT) && (GDALDataTypeIsComplex(eDT1) || GDALDataTypeIsComplex(eDT2))) ||
                        (!GDALDataTypeIsComplex(eDT) && !GDALDataTypeIsComplex(eDT1) && !GDALDataTypeIsComplex(eDT2)) );

                ENSURE( !(GDALDataTypeIsFloating(eDT1) || GDALDataTypeIsFloating(eDT2)) || GDALDataTypeIsFloating(eDT));
                ENSURE( !(GDALDataTypeIsSigned(eDT1) || GDALDataTypeIsSigned(eDT2)) || GDALDataTypeIsSigned(eDT));
            }
        }

can be written in GoogleTest as

.. code-block:: c++

    class DataTypeTupleFixture:
            public test_gdal,
            public ::testing::WithParamInterface<std::tuple<GDALDataType, GDALDataType>>
    {
    public:
        static std::vector<std::tuple<GDALDataType, GDALDataType>> GetTupleValues()
        {
            std::vector<std::tuple<GDALDataType, GDALDataType>> ret;
            for( GDALDataType eIn = GDT_Byte; eIn < GDT_TypeCount; eIn = static_cast<GDALDataType>(eIn + 1) )
            {
                for( GDALDataType eOut = GDT_Byte; eOut < GDT_TypeCount; eOut = static_cast<GDALDataType>(eOut + 1) )
                {
                    ret.emplace_back(std::make_tuple(eIn, eOut));
                }
            }
            return ret;
        }
    };

    // Test GDALDataTypeUnion() on all (GDALDataType, GDALDataType) combinations
    TEST_P(DataTypeTupleFixture, GDALDataTypeUnion_generic)
    {
        GDALDataType eDT1 = std::get<0>(GetParam());
        GDALDataType eDT2 = std::get<1>(GetParam());
        GDALDataType eDT = GDALDataTypeUnion(eDT1,eDT2 );
        EXPECT_EQ( eDT, GDALDataTypeUnion(eDT2,eDT1) );
        EXPECT_GE( GDALGetDataTypeSize(eDT), GDALGetDataTypeSize(eDT1) );
        EXPECT_GE( GDALGetDataTypeSize(eDT), GDALGetDataTypeSize(eDT2) );
        EXPECT_TRUE( (GDALDataTypeIsComplex(eDT) && (GDALDataTypeIsComplex(eDT1) || GDALDataTypeIsComplex(eDT2))) ||
                (!GDALDataTypeIsComplex(eDT) && !GDALDataTypeIsComplex(eDT1) && !GDALDataTypeIsComplex(eDT2)) );

        EXPECT_TRUE( !(GDALDataTypeIsFloating(eDT1) || GDALDataTypeIsFloating(eDT2)) || GDALDataTypeIsFloating(eDT));
        EXPECT_TRUE( !(GDALDataTypeIsSigned(eDT1) || GDALDataTypeIsSigned(eDT2)) || GDALDataTypeIsSigned(eDT));
    }

    INSTANTIATE_TEST_SUITE_P(
            test_gdal,
            DataTypeTupleFixture,
            ::testing::ValuesIn(DataTypeTupleFixture::GetTupleValues()),
            [](const ::testing::TestParamInfo<DataTypeTupleFixture::ParamType>& l_info) {
                GDALDataType eDT1 = std::get<0>(l_info.param);
                GDALDataType eDT2 = std::get<1>(l_info.param);
                return std::string(GDALGetDataTypeName(eDT1)) + "_" + GDALGetDataTypeName(eDT2);
            }
    );

While it is admittedly more verbose (which is an exception, as for simpler
tests, the GoogleTest way is generally smaller than the TUT way) but
much more expressive when looking at the test run output, where each combination
is run as a given named test, and thus if failure occurs, it is easy to spot
which combination failed, whereas with TUT you had to add manual instrumentation:


.. code-block::

    [----------] 196 tests from test_gdal/DataTypeTupleFixture
    [ RUN      ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Byte_Byte
    [       OK ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Byte_Byte (0 ms)
    [ RUN      ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Byte_UInt16
    [       OK ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Byte_UInt16 (0 ms)
    [.. snip ...]
    [ RUN      ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Int8_Int8
    [       OK ] test_gdal/DataTypeTupleFixture.GDALDataTypeUnion_generic/Int8_Int8 (0 ms)
    [----------] 196 tests from test_gdal/DataTypeTupleFixture (1 ms total)


Backward compatibility
----------------------

None. This doesn't affect the library, nor the release source code archive
which don't include the autotest/ directory.

Related tickets and PRs:
------------------------

Ticket: https://github.com/OSGeo/gdal/issues/3525

Implementation: https://github.com/OSGeo/gdal/pull/6732

Voting history
--------------

+1from PSC members MateuszL, HowardB, JukkaR, KurtS , FrankW, DanielM and EvenR
