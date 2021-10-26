# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckCXXCompilerFlag
------------------------

Check whether the CXX compiler supports a given flag.

.. command:: check_cxx_compiler_flag

  ::

    check_cxx_compiler_flag(<flag> <var>)

  Check that the ``<flag>`` is accepted by the compiler without
  a diagnostic.  Stores the result in an internal cache entry
  named ``<var>``.

This command temporarily sets the ``CMAKE_REQUIRED_DEFINITIONS`` variable
and calls the ``check_cxx_source_compiles`` macro from the
:module:`CheckCXXSourceCompiles` module.  See documentation of that
module for a listing of variables that can otherwise modify the build.

A positive result from this check indicates only that the compiler did not
issue a diagnostic message when given the flag.  Whether the flag has any
effect or even a specific one is beyond the scope of this module.

.. note::
  Since the :command:`try_compile` command forwards flags from variables
  like :variable:`CMAKE_CXX_FLAGS <CMAKE_<LANG>_FLAGS>`, unknown flags
  in such variables may cause a false negative for this check.
#]=======================================================================]

include(CheckCXXSourceCompiles)
include(CMakeCheckCompilerFlagCommonPatterns)

macro (CHECK_CXX_COMPILER_FLAG _FLAG _RESULT)
   set(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
   set(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")
   if("${_FLAG}" STREQUAL "-mfma")
       # Compiling with FMA3 support may fail only at the assembler level.
       # In that case we need to have such an instruction in the test code
       set(_cxx_code "#include <immintrin.h>
          __m128 foo(__m128 x) { return _mm_fmadd_ps(x, x, x); }
          int main() { return 0; }")
   elseif("${_FLAG}" STREQUAL "-std=c++14" OR "${_FLAG}" STREQUAL "-std=c++1y")
       set(_cxx_code "#include <utility>
      struct A { friend auto f(); };
      template <int N> constexpr int var_temp = N;
      template <std::size_t... I> void foo(std::index_sequence<I...>) {}
      int main() { foo(std::make_index_sequence<4>()); return 0; }")
   elseif("${_FLAG}" STREQUAL "-std=c++17" OR "${_FLAG}" STREQUAL "-std=c++1z")
       set(_cxx_code "#include <functional>
      int main() { return 0; }")
   elseif("${_FLAG}" STREQUAL "-stdlib=libc++")
       # Compiling with libc++ not only requires a compiler that understands it, but also
       # the libc++ headers itself
       set(_cxx_code "#include <iostream>
      #include <cstdio>
      int main() { return 0; }")
   elseif("${_FLAG}" STREQUAL "-march=knl"
          OR "${_FLAG}" STREQUAL "-march=skylake-avx512"
          OR "${_FLAG}" STREQUAL "/arch:AVX512"
          OR "${_FLAG}" STREQUAL "/arch:KNL"
          OR "${_FLAG}" MATCHES "^-mavx512.")
       # Make sure the intrinsics are there
       set(_cxx_code "#include <immintrin.h>
      __m512 foo(__m256 v) {
        return _mm512_castpd_ps(_mm512_insertf64x4(_mm512_setzero_pd(), _mm256_castps_pd(v), 0x0));
      }
      __m512i bar() { return _mm512_setzero_si512(); }
      int main() { return 0; }")
   elseif("${_FLAG}" STREQUAL "-mno-sse" OR "${_FLAG}" STREQUAL "-mno-sse2")
       set(_cxx_code "#include <cstdio>
      #include <cstdlib>
      int main() { return std::atof(\"0\"); }")
   else()
       set(_cxx_code "#include <cstdio>
      int main() { return 0; }")
   endif()

   # Normalize locale during test compilation.
   set(_CheckCXXCompilerFlag_LOCALE_VARS LC_ALL LC_MESSAGES LANG)
   foreach(v ${_CheckCXXCompilerFlag_LOCALE_VARS})
     set(_CheckCXXCompilerFlag_SAVED_${v} "$ENV{${v}}")
     set(ENV{${v}} C)
   endforeach()
   CHECK_COMPILER_FLAG_COMMON_PATTERNS(_CheckCXXCompilerFlag_COMMON_PATTERNS)
   CHECK_CXX_SOURCE_COMPILES("${_cxx_code}" ${_RESULT}
     # Some compilers do not fail with a bad flag
     FAIL_REGEX "command line option .* is valid for .* but not for C\\\\+\\\\+" # GNU
     ${_CheckCXXCompilerFlag_COMMON_PATTERNS}
     )
   foreach(v ${_CheckCXXCompilerFlag_LOCALE_VARS})
     set(ENV{${v}} ${_CheckCXXCompilerFlag_SAVED_${v}})
     unset(_CheckCXXCompilerFlag_SAVED_${v})
   endforeach()
   unset(_CheckCXXCompilerFlag_LOCALE_VARS)
   unset(_CheckCXXCompilerFlag_COMMON_PATTERNS)

   set (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
endmacro ()

