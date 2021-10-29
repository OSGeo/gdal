# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckCompilerMachineOption
-------------------------------

  Check compiler flag for machine optimization. Returns compiler flag
  for specified feature such as SSE AVX2 if available.
  if not available, return "".

.. command:: check_compiler_machine_option

   compiler_machine_option(<output variable> <feature>)

#]=======================================================================]


include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(CheckIncludeFile)

function(check_compiler_machine_option outvar feature)
  macro(__check_compiler_flag _flag _result)
    if(CMAKE_CXX_COMPILER_LOADED)
      check_cxx_compiler_flag("${_flag}" ${_result})
    elseif(CMAKE_C_COMPILER_LOADED)
      check_c_compiler_flag("${_flag}" ${_result})
    endif()
  endmacro()

  set(_FLAGS)
  if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86|AMD64)")
    if(MSVC AND (${feature} MATCHES "SSE"))
      # SSE2 and SSE are default on
	  set(_FLAGS " ")
    elseif(MSVC)
      # Only Visual Studio 2017 version 15.3 / Visual C++ 19.11 & up have support for AVX-512.
      # https://blogs.msdn.microsoft.com/vcblog/2017/07/11/microsoft-visual-studio-2017-supports-intel-avx-512/
      set(MSVC_FLAG_MAP AVX512 "-arch:AVX512" AVX2 "-arch:AVX2" AVX "-arch:AVX")
      list(FIND MSVC_FLAG_MAP ${feature} _found)
      if(_found GREATER -1)
        math(EXPR index "${_found}+1")
        list(GET MSVC_FLAG_MAP ${index} _flag)
        __check_compiler_flag("${_flag}" test_${feature})
        if(test_${feature})
          set(_FLAGS "${_flag}")
        endif()
      endif()
    elseif(CMAKE_CXX_COMPILER MATCHES "/(icpc|icc)$") # ICC (on Linux)
      set(map_flag AVX2 "-xCORE_AVX2" AVX "-xAVX" SSE4_2 "-xSSE4.2" SSE4_1 "-xSSE4.1" SSSE3 "-xSSSE3"
            AVX512 "-xCORE-AVX512;-xMIC-AVX512" )
      list(FIND map_flag "${feature}" _found)
      if(_found GREATER -1)
        math(EXPR index "${_found}+1")
        list(GET map_flag ${index} _flag)
        foreach(flag IN ITEMS ${_flag})
          __check_compiler_flag(${_flag} _ok)
          if(_ok)
            set(_FLAGS ${_flag})
            break()
          endif()
        endforeach()
      endif()
    else() # not MSVC and not ICC => GCC, Clang, Open64
      string(TOLOWER ${feature} _flag)
	  string(REPLACE "_" "." _flag "${_flag}")
      __check_compiler_flag("-m${_flag}" test_${_flag})
      if(test_${_flag})
        set(header_table "sse3" "pmmintrin.h" "ssse3" "tmmintrin.h" "sse4.1" "smmintrin.h"
            "sse4.2" "smmintrin.h" "sse4a" "ammintrin.h" "avx" "immintrin.h"
            "avx2" "immintrin.h" "fma4" "x86intrin.h" "xop" "x86intrin.h")
        set(_header FALSE)
        list(FIND header_table ${_flag} _found)
        if(_found GREATER -1)
          math(EXPR index "${_found} + 1")
          list(GET header_table ${index} _header)
        endif()
        set(_resultVar "HAVE_${_header}")
        string(REPLACE "." "_" _resultVar "${_resultVar}")
        if(_header)
          if(CMAKE_CXX_COMPILER_LOADED)
            check_include_file_cxx("${_header}" ${_resultVar} "-m${_flag}")
          elseif(CMAKE_C_COMPILER_LOADED)
            check_include_file("${_header}" ${_resultVar} "-m${_flag}")
          endif()
          if(NOT _header OR ${_resultVar})
            set(_FLAGS "-m${_flag}")
          endif()
        endif()
      endif()
    endif()
  elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(ARM|aarch64)")
    if(MSVC)
      # TODO implement me
    elseif(CMAKE_CXX_COMPILER MATCHES "/(icpc|icc)$") # ICC (on Linux)
      # TODO implement me
    else() # not MSVC and not ICC => GCC, Clang, Open64
      string(TOLOWER ${feature} _flag)
      __check_compiler_flag("-m${_flag}" test_${_flag})
      if(test_${_flag})
        set(_FLAGS "-m${_flag}")
      endif()
    endif()
  endif()
  set(${outvar} "${_FLAGS}" PARENT_SCOPE)
endfunction()
