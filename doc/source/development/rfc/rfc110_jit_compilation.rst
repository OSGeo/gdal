.. _rfc-110:

=============================================================================
RFC 110: Just-In-Time (JIT) compilation of expressions (on hold, NOT adopted)
=============================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-11-25
Status:        Draft, on hold
Target:        GDAL 3.13
============== =============================================

.. warning::

    This RFC has been put on hold. Its candidate implementation has NOT been
    merged into the official GDAL source code.


Summary
-------

This RFC introduces the optional use of the `CLang <https://clang.llvm.org/>`__ and
`LLVM <https://llvm.org/>`__ embedded libraries to provide Just-In-Time (JIT) /
on-the-fly compilation of code, and its use to speed-up evaluation of
expressions, as used in VRTDerivedBand and :ref:`gdal_raster_calc`.

Motivation
----------

Currently our expression evaluation engines are interpreters, and thus do not
benefit from potential optimizations that a compiler can bring. By using the
on-the-fly compilation capabilities of CLang and LLVM, and in particular their
auto-vectorization optimizations when iterating over arrays, we can benefit from
the instruction set of the running CPU, in particular SIMD instructions, such as
SSE/AVX on Intel or Neon on ARM CPUs.

Technical details
-----------------

A new ``LLVM`` expression dialect is added. The aim is to keep it synchronized as
much as possible to the ``MuParser`` dialect, but acknowledging there are subtle
differences in some numeric results, particularly when ``Float32`` is used.

The LLVM/CLang dependency is optional, and does not make any assumption on which
compiler is used to build GDAL itself. It is perfectly fine to build GDAL with
GCC or Microsoft Visual C++ compiler.

The libraries are reasonably easy to find from packaging systems. Below a
few examples from the changes done in our Continuous Integration / Docker images

+-----------------+-------------------------------------+---------------------------------+
| Platform        | Build Requirements                  | Execution Requirements          |
+=================+=====================================+=================================+
|                 | ``llvm-${LLVM_VERSION}-dev``        | ``llvm-${LLVM_VERSION}``        |
| Ubuntu Linux    | ``libclang-${LLVM_VERSION}-dev``    | ``libclang-cpp${LLVM_VERSION}`` |
|                 | ``libclang-cpp${LLVM_VERSION}-dev`` |                                 |
+-----------------+-------------------------------------+---------------------------------+
| Alpine Linux    | ``llvm${LLVM_VERSION}-dev``         | ``llvm{LLVM_VERSION}-libs``     |
|                 | ``clang${LLVM_VERSION}-dev``        | ``clang${LLVM_VERSION}-libs``   |
+-----------------+-------------------------------------+---------------------------------+
| Fedora Linux    | ``llvm-devel`` ``clang-devel``      | ``llvm-libs`` ``clang-libs``    |
+-----------------+-------------------------------------+---------------------------------+
|                 | ``llvmdev`` ``clangdev``            | ``llvm`` ``clang``              |
| Conda-Forge     |                                     | + on Windows, optionally,       |
|                 |                                     | ``intel‐cmplr‐lib‐rt`` for SVML |
+-----------------+-------------------------------------+---------------------------------+

New files
+++++++++

The following files are added:

* cmake/modules/packages/FindLLVM.cmake:

  CMake Find module for LLVM and CLang. Borrowed from another project (BSD-2 License)
  with modifications to also look for the clang-cpp libraries, in addition to
  the LLVM libraries.

* gcore/gdal_jit_cpp.h:

  Public GDAL C++ API to compile C code and get a function pointer for an
  entry point in that C code. The API does not leak LLVM/CLang object.

  .. code-block:: cpp

        /** Returns an executable function from the provided C code.
         *
         * @param cCode Valid C code that has a function called functionName and
         *              whose signature must be FunctionSignature.
         *              The C code must not use any \#include statement.
         * @param functionName Entry point in the C code
         * @param papszOptions NULL-terminated list of options, or NULL. Unused for now.
         * @param[out] posDisassembledCode Pointer to a string that must receive the
         *                                 disassembly of the compiled code, or nullptr
         *                                 if not useful.
         * @param[out] pbHasVeclib Pointer to a boolean to indicate if a math vector lib
         *                         has been found, or nullptr if not useful.
         * @return a std::function of signature FunctionSignature corresponding to the
         *         entry point in the C code (may be invalid in case of error.)
         * @since 3.13
         */
        template <typename FunctionSignature>
        std::function<FunctionSignature> CPL_DLL GDALGetJITFunction(
            const std::string &cCode, const std::string &functionName,
            CSLConstList papszOptions = nullptr,
            std::string *posDisassembledCode = nullptr, bool *pbHasVeclib = nullptr);

  Note that we return a `std::function` and not a C callback because the
  code compiled by LLVM is owned by LLVM objects, and so we need to capture them
  in the lambda we return as the `std::function`, so as to keep the underlying
  C callable code valid as long as the `std::function` instance is valid.

  To be noted that the compilation process does not involve any access to on-disk files
  (verified with the ``strace`` utility). Only in-memory buffers are created.

  There is also a function to indicate if JIT capabilities are present:

  .. code-block:: cpp

        /** Return which JIT engines are available.
         *
         * At time of writing, the return value may be an empty vector or a vector
         * with "LLVM".
         *
         * @since 3.13
         */
        std::vector<std::string> CPL_DLL GDALGetJITEngines();

  That header file is installed, as it may be useful to other projects.

* gcore/gdal_jit.cpp:

  Implementation of the above header, using libLLVM and libclang-cpp C++
  semi-public/semi-private/unstable APIs.
  That implementation has been verified to build and work for all versions
  from LLVM 14 up to LLVM 22dev. There are a few #ifdef to account for breaking
  changes in the LLVM API, but not that much.

* gcore/gdal_c_expr.h:

  Definition of a C++ class ``GDAL_c_expr_node`` and a
  ``std::unique_ptr<GDAL_c_expr_node> CPL_DLL GDAL_c_expr_compile(const char *expr)``
  method that parses a C-like MuParser expression string and return the root node
  of the abstract syntax tree (AST) of the expression.
  The parser supports all MuParser operators and functions, as well as our
  ``nan``, ``isnan``, ``isnodata`` custom extensions.

* gcore/gdal_c_expr.cpp: Implementation of the above

* gcore/gdal_c_expr.y: Bison grammar for a C-like MuParser expression

* gcore/gdal_c_expr_parser.cpp/.h:

  Files generated from above file, and included in :file:`gdal_c_expr.cpp`


Modified files
++++++++++++++

The ``ExprPixelFunc`` function in `frmts/vrt/pixelfunctions.cpp`, which is
the method used to evaluate MuParser/ExprTK expressions is modified.

When the JIT is present and dialect LLVM is specific,
``GDAL_c_expr_compile`` is used to build an AST from it, and then a
``GDALCFunctionGenerator`` class translates that AST back into a C function
that iterates over all pixels.

For example, give an expression ``AVG(A)`` on a 3-band raster, in ``gdal raster calc``
flatten mode, where the compute data type is `int`, the following C code will
be generated in a in-memory string:

  .. code-block:: c

        typedef __SIZE_TYPE__ size_t;
        static inline double my_sum(const int* __restrict const* inArrays, size_t nSources, size_t iCol)
        {
          double res = 0;
          for (size_t iSrc = 0; iSrc < nSources; ++iSrc)
             res += inArrays[iSrc][iCol];
          return res;
        }
        double round(double);
        void computePixels(const int* __restrict const* const inArrays,
                           const double* __restrict const* const auxDoubleArrays,
                           int* __restrict const outArray,
                           size_t nSources)
        {
          (void)auxDoubleArrays;
          for(size_t i = 0; i < nSources; ++i)
          {
             outArray[i] = (int)round((my_sum(inArrays, 3, i) * (1.0 / 3)));
          }
        }


That C code is then passed to an internal ``JITCompute`` method that calls
``GDALGetJITFunction()`` and execute the returned function on the source buffers.
``JITCompute`` maintains a per-thread cache of JIT compiled functions.

Runtime Configuration options
+++++++++++++++++++++++++++++

The following (advanced) configuration options are added:

* GDAL_JIT_DEBUG=YES/NO (default: NO): when enabled, and :config:`CPL_DEBUG`
  configuration option is set to ``ON`` or ``GDAL_JIT``, debug messages specific
  to JIT usage, including generated C code and disassembly of the JIT code,
  will be emitted.

* GDAL_JIT_USE_VECLIB=YES/NO (default: YES): whether a mathematic library
  providing vectorized implementations of common
  transcendental functions, ``sin()``, ``cos()``, ``tan()``, ``exp()``, ``log()``, etc.
  can be used, when it is available. Such library is not linked to GDAL itself
  at GDAL compilation time, but is dynamically loaded by LLVM at runtime.

* GDAL_JIT_VECLIB_PATH=</path/to/dynamic_library>: path to the dynamic library
  providing vectorized implementations of transcendental functions.
  Defaults to the path of the ``Accelerate framework`` on Mac OS X, ``SVML``
  on Windows, or :file:`libmvec.so.1` on Linux systems for Intel or ARM64
  architectures, when they use a GNU C library (so not Alpine Linux for example)

* GDAL_JIT_VECLIB_TYPE=mvec|SVML|Accelerate|...: value supported by Clang
  `-fveclib=` option. Must be consistent with the library pointed by
  GDAL_JIT_VECLIB_PATH.
  See https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-fveclib
  for more details.

Normally, users should not have to bother with any of them.

Benchmarks
----------

On a Skylake x86_64 CPU, i.e. with AVX2 and Fused multiply add (FMA) extensions:

  .. code-block:: shell

    $ gdal raster resize --size 10000,5000 autotest/gdrivers/data/small_world.tif big_world.tif


Averaging all 3 bands (per pixel), with Byte output:

  .. code-block:: shell

    $ time gdal raster calc big_world.tif big_world_avg.tif \
        --flatten --calc "avg(X)" -q --dialect LLVM --overwrite \
        --output-data-type Byte

      real 0m0,283s

    $ time gdal raster calc big_world.tif big_world_avg.tif \
        --flatten --calc "avg(X)" -q --dialect muparser --overwrite \
        --output-data-type Byte

      real 0m1,255s

   $ time gdal raster calc big_world.tif big_world_mean.tif --dialect builtin \
        --flatten --calc "mean" -q --overwrite --output-data-type Byte

      real 0m0,246s


  .. note::

       The builtin "mean" method is slightly faster than the JIT'ed "avg(X)",
       since it has a manual ultra optimized SSE2 implementation.

Computing the maximum minus the minimum of all 3 bands (per pixel):

  .. code-block:: shell

    $ time gdal raster calc big_world.tif big_world_max_minus_min.tif \
        --flatten --calc "avg(X)" -q --dialect LLVM --overwrite \
        --output-data-type Byte

      real 0m0,248s

    $ time gdal raster calc big_world.tif big_world_max_minus_min.tif \
        --flatten --calc "avg(X)" -q --dialect muparser --overwrite \
        --output-data-type Byte

      real 0m1,838s


Computing the log10 of each pixel, per band, as Float64:

  .. code-block:: shell

    $ time gdal raster calc big_world.tif big_world_log10.tif \
        --calc "log10(X)" -q --dialect LLVM --overwrite

      real 0m2,850s

    $ time gdal raster calc big_world.tif big_world_log10.tif \
        --calc "log10(X)" -q --dialect muparser --overwrite

      real 0m5,631s

    $ time gdal raster calc big_world.tif big_world_log10_builtin.tif --dialect builtin \
        --calc "log10" -q --overwrite

      real 0m6,490s


Computing the log10 of each pixel, per band, as Float32:

  .. code-block:: shell

    $ time gdal raster calc big_world.tif big_world_log10.tif \
        --calc "log10(X)" -q --dialect LLVM --overwrite --output-data-type Float32

      real 0m1,842s

    $ time gdal raster calc big_world.tif big_world_log10.tif \
        --calc "log10(X)" -q --dialect muparser --overwrite --output-data-type Float32

      real 0m4,893s

    $ time gdal raster calc big_world.tif big_world_log10_builtin.tif --dialect builtin \
        --calc "log10" -q --overwrite --output-data-type Float32

      real 0m6,032s


Computing the modulus of a complex number from one band with the real part and another one with the imaginary part:

  .. code-block:: shell

    $ time gdal raster calc two_float_bands.tif modulus_jit.tif --flatten \
        --calc "sqrt(X[1]^2 + X[2]^2)" -q --overwrite --dialect LLVM --output-data-type Float32

      real 0m0,549s

    $ time gdal raster calc two_float_bands.tif modulus_nojit.tif --flatten \
        --calc "sqrt(X[1]^2 + X[2]^2)" -q --overwrite --dialect muparser --output-data-type Float32

      real 0m1,582s


.. warning::

    Above examples are best cases where the I/O cost is minimal. When slow I/O
    is involved (remote datasets and/or compression and/or suboptimal data
    arrangement), the CPU time can be negligible compared to I/O, and thus
    this enhancement will not bring any significant improvements.

Backward compatibility
----------------------

This enhancement is fully backwards compatible.

Security
--------

Executing the function returned by ``GDALGetJITFunction()`` can obviously lead
to arbitrary code execution depending on the content of the provided C code.

That said, in the VRTDerivedBand context, we are confident that there is no security
risk because the ``GDALCFunctionGenerator`` class composes the C code in a very
controlled way. Only recognized operators and functions are emitted, and none of
them involve risky operations. Furthermore the identifiers (names
referencing input arrays or variables) are never emitted verbatim from the
input but always translated from a controlled set.

Furthermore the impossibility to include headers from the C code provided to
``GDALGetJITFunction()`` avoids the risk that malicious content in an external
file would impact our own code.

Documentation
-------------

The new public methods are documented.

The https://gdal.org/en/stable/development/building_from_source.html#cmake-general-configure-options
paragraph will be extended to document the CMake variables needed to configure
with LLVM/CLang support: ``LLVM_ROOT``, ``LLVM_FIND_VERSION``, ``LLVM_CONFIG_EXECUTABLE``, ``LLVM_CLANG_LIBS``

Testing
-------

New tests have been added to test all the operators and functions.
A number of tests in :file:`autotest/gdrivers/vrtderived.py` and :file:`autotest/utilities/test_gdal_raster_calc.py`
are run both with MuParser and LLVM dialects.

The following continuous integration configurations are enhanced to add libLLVM/libclang-cpp
support to GDAL:

* Fedora Rawhide, x86_64 architecture
* Alpine Linux, x86_64 architecture
* Ubuntu 24.04, x86_64 architecture
* MacOSX, arm64 architecture, using Conda-Forge dependencies
* Windows, x86_64 architecture, using Conda-Forge dependencies
* (added) Ubuntu 25.10, arm64 architecture (to have glibc >= 3.40, which is the
  minimum version to be able to use libmvec on Linux ARM64)

Docker images
-------------

The ``osgeo/gdal:alpine-normal`` and ``osgeo/gdal:ubuntu-full`` Docker images
are modified to include the libclang-cpp and libLLVM

Related issues and PRs
----------------------

Candidate implementation: https://github.com/OSGeo/gdal/pull/13470

Potential further enhancements
------------------------------

The approach in the above branch currently makes libclang and libLLVM dependencies
of the GDAL core. The weight of those runtime libraries is typically more than
100 MB. We could potentially use the GDAL plugin architecture, so that people
building GDAL could instead create a ``gdal_LLVM`` pseudo-driver that could be
opt-in at installation time, typically provided by a ``gdal-LLVM`` sub-package.

The API in :file:`gdal_jit_cpp.h` is independent of VRT expressions and could
be re-used by other parts of GDAL, or external users, for other purposes.

Funding
-------

Funded by GDAL Sponsorship Program (GSP)

Voting history
--------------

TBD

.. below is an allow-list for spelling checker.

.. spelling:word-list::

    CLang
    FindLLVM
    glibc
    jit
    JIT'ed
    libLLVM
    libclang
    libmvec
    mathematic
    Skylake
