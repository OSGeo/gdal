# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckCompilerSIMDFeature
-------------------------------

.. command:: check_compiler_simd_feature

   check_compiler_simd_feature(RESULT <result> FLAG <flag> QUERY <key>)

#]=======================================================================]

include(CheckCSourceCompiles)

function(check_compiler_simd_feature)
  set(_options)
  set(_oneValueArgs RESULT FLAG)
  set(_multiValueArgs)
  cmake_parse_arguments(_CCSF "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if(_CCSF_FLAG STREQUAL "AVX")
    set(CMAKE_REQUIRED_FLAGS ${_CCSF_FLAG})
    set(_c_source  "#ifdef __AVX__
#include <immintrin.h>
    int foo() { unsigned int nXCRLow, nXCRHigh;
    __asm__ (\"xgetbv\" : \"=a\" (nXCRLow), \"=d\" (nXCRHigh) : \"c\" (0));
   float fEpsilon = 0.0000000000001f;
   __m256 ymm_small = _mm256_set_ps(fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon);
   return (int)nXCRLow + _mm256_movemask_ps(ymm_small); }
   int main(int argc, char**) { if( argc == 0 ) return foo(); return 0; }
#else
   some_error
#endif")
    check_c_source_compiles(_c_source ${_CCSF_RESULT})
  elseif(_CCSF_FLAG STREQUAL "SSSE3")
    set(CMAKE_REQUIRED_FLAGS ${_CCSF_FLAG})
    set(_c_source  "#ifdef __SSSE3__
#include <tmmintrin.h>
    void foo() { __m128i xmm_i = _mm_set1_epi16(0); xmm_i = _mm_shuffle_epi8(xmm_i, xmm_i); }  int main() { return 0; }
#else
    some_error
#endif")
    check_c_source_compiles(_c_source ${_CCSF_RESULT})
  elseif(_CCSF_FLAG STREQUAL "SSE")
    set(_c_source "#ifdef __SSE__
#include <xmmintrin.h>
    void foo() { float fEpsilon = 0.0000000000001f; __m128 xmm_small = _mm_load1_ps(&fEpsilon); }  int main() { return 0; }
#else
    some_error
#endif")
    check_c_source_compiles(_c_source ${_CCSF_RESULT})
  endif()
endfunction()
