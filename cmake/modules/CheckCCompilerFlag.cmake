# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckCCompilerFlag
------------------

Check whether the C compiler supports a given flag.

.. command:: check_c_compiler_flag

  ::

    check_c_compiler_flag(<flag> <var>)

  Check that the ``<flag>`` is accepted by the compiler without
  a diagnostic.  Stores the result in an internal cache entry
  named ``<var>``.

This command temporarily sets the ``CMAKE_REQUIRED_DEFINITIONS`` variable
and calls the ``check_c_source_compiles`` macro from the
:module:`CheckCSourceCompiles` module.  See documentation of that
module for a listing of variables that can otherwise modify the build.

A positive result from this check indicates only that the compiler did not
issue a diagnostic message when given the flag.  Whether the flag has any
effect or even a specific one is beyond the scope of this module.

.. note::
  Since the :command:`try_compile` command forwards flags from variables
  like :variable:`CMAKE_C_FLAGS <CMAKE_<LANG>_FLAGS>`, unknown flags
  in such variables may cause a false negative for this check.
#]=======================================================================]

include(CheckCSourceCompiles)
include(CMakeCheckCompilerFlagCommonPatterns)

macro (CHECK_C_COMPILER_FLAG _FLAG _RESULT)
   set(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
   set(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")

   if("${_FLAG}" STREQUAL "-mfma")
       # Compiling with FMA3 support may fail only at the assembler level.
       # In that case we need to have such an instruction in the test code
       set(_c_code "#include <immintrin.h>
          __m128 foo(__m128 x) { return _mm_fmadd_ps(x, x, x); }
          int main() { return 0; }")
   elseif("${_FLAG}" STREQUAL "-march=knl"
          OR "${_FLAG}" STREQUAL "-march=skylake-avx512"
          OR "${_FLAG}" STREQUAL "/arch:AVX512"
          OR "${_FLAG}" STREQUAL "/arch:KNL"
          OR "${_FLAG}" MATCHES "^-mavx512.")
       # Make sure the intrinsics are there
       set(_c_code "#include <immintrin.h>
      __m512 foo(__m256 v) {
        return _mm512_castpd_ps(_mm512_insertf64x4(_mm512_setzero_pd(), _mm256_castps_pd(v), 0x0));
      }
      __m512i bar() { return _mm512_setzero_si512(); }
      int main() { return 0; }")
   else()
       set(_c_code " int main() { return 0; }")
   endif()

   # Normalize locale during test compilation.
   set(_CheckCCompilerFlag_LOCALE_VARS LC_ALL LC_MESSAGES LANG)
   foreach(v ${_CheckCCompilerFlag_LOCALE_VARS})
     set(_CheckCCompilerFlag_SAVED_${v} "$ENV{${v}}")
     set(ENV{${v}} C)
   endforeach()
   CHECK_COMPILER_FLAG_COMMON_PATTERNS(_CheckCCompilerFlag_COMMON_PATTERNS)
   CHECK_C_SOURCE_COMPILES("${_c_code}" ${_RESULT}
     # Some compilers do not fail with a bad flag
     FAIL_REGEX "command line option .* is valid for .* but not for C" # GNU
     ${_CheckCCompilerFlag_COMMON_PATTERNS}
     )
   foreach(v ${_CheckCCompilerFlag_LOCALE_VARS})
     set(ENV{${v}} ${_CheckCCompilerFlag_SAVED_${v}})
     unset(_CheckCCompilerFlag_SAVED_${v})
   endforeach()
   unset(_CheckCCompilerFlag_LOCALE_VARS)
   unset(_CheckCCompilerFlag_COMMON_PATTERNS)

   set (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
endmacro ()
