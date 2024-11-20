
# ######################################################################################################################
# Detect available warning flags

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Do that check now, since we need the result of HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT for cpl_config.h

set(GDAL_C_WARNING_FLAGS)
set(GDAL_CXX_WARNING_FLAGS)

if (MSVC)
  # 1. conditional expression is constant
  # 2. 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
  # 3. non DLL-interface classkey 'identifier' used as base for DLL-interface classkey 'identifier'
  # 4. ??????????
  # 5. 'identifier' : unreferenced formal parameter
  # 6. 'conversion' : conversion from 'type1' to 'type2', signed/unsigned mismatch
  # 7. nonstandard extension used : translation unit is empty (only applies to C source code)
  # 8. new behavior: elements of array 'array' will be default initialized (needed for
  #    https://trac.osgeo.org/gdal/changeset/35593)
  # 9. interaction between '_setjmp' and C++ object destruction is non-portable
  #
  set(GDAL_C_WARNING_FLAGS
      /W4
      /wd4127
      /wd4251
      /wd4275
      /wd4786
      /wd4100
      /wd4245
      /wd4206
      /wd4351
      /wd4611)
  set(GDAL_CXX_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS})
  add_compile_options(/EHsc)

  # The following are extra disables that can be applied to external source not under our control that we wish to use
  # less stringent warnings with.
  set(GDAL_SOFTWARNFLAGS
      /wd4244
      /wd4702
      /wd4701
      /wd4013
      /wd4706
      /wd4057
      /wd4210
      /wd4305)

else ()

  set(GDAL_SOFTWARNFLAGS "")

  macro (detect_and_set_c_warning_flag flag_name)
    string(TOUPPER ${flag_name} flag_name_upper)
    string(REPLACE "-" "_" flag_name_upper "${flag_name_upper}")
    string(REPLACE "=" "_" flag_name_upper "${flag_name_upper}")
    check_c_compiler_flag(-W${flag_name} "HAVE_WFLAG_${flag_name_upper}")
    if (HAVE_WFLAG_${flag_name_upper})
      set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -W${flag_name})
    endif ()
  endmacro ()

  macro (detect_and_set_cxx_warning_flag flag_name)
    string(TOUPPER ${flag_name} flag_name_upper)
    string(REPLACE "-" "_" flag_name_upper "${flag_name_upper}")
    string(REPLACE "=" "_" flag_name_upper "${flag_name_upper}")
    check_cxx_compiler_flag(-W${flag_name} "HAVE_WFLAG_${flag_name_upper}")
    if (HAVE_WFLAG_${flag_name_upper})
      set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -W${flag_name})
    endif ()
  endmacro ()

  macro (detect_and_set_c_and_cxx_warning_flag flag_name)
    string(TOUPPER ${flag_name} flag_name_upper)
    string(REPLACE "-" "_" flag_name_upper "${flag_name_upper}")
    string(REPLACE "=" "_" flag_name_upper "${flag_name_upper}")
    check_c_compiler_flag(-W${flag_name} "HAVE_WFLAG_${flag_name_upper}")
    if (HAVE_WFLAG_${flag_name_upper})
      set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -W${flag_name})
      set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -W${flag_name})
    endif ()
  endmacro ()

  detect_and_set_c_and_cxx_warning_flag(all)
  detect_and_set_c_and_cxx_warning_flag(extra)
  detect_and_set_c_and_cxx_warning_flag(init-self)
  detect_and_set_c_and_cxx_warning_flag(unused-parameter)
  detect_and_set_c_warning_flag(missing-prototypes)
  detect_and_set_c_and_cxx_warning_flag(missing-declarations)
  detect_and_set_c_and_cxx_warning_flag(shorten-64-to-32)
  detect_and_set_c_and_cxx_warning_flag(logical-op)
  detect_and_set_c_and_cxx_warning_flag(shadow)
  detect_and_set_cxx_warning_flag(shadow-field) # CLang only for now
  detect_and_set_c_and_cxx_warning_flag(missing-include-dirs)
  check_c_compiler_flag("-Wformat -Werror=format-security -Wno-format-nonliteral" HAVE_WFLAG_FORMAT_SECURITY)
  if (HAVE_WFLAG_FORMAT_SECURITY)
    set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -Wformat -Werror=format-security -Wno-format-nonliteral)
    set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -Wformat -Werror=format-security -Wno-format-nonliteral)
  else ()
    detect_and_set_c_and_cxx_warning_flag(format)
  endif ()
  detect_and_set_c_and_cxx_warning_flag(error=vla)
  detect_and_set_c_and_cxx_warning_flag(no-clobbered)
  detect_and_set_c_and_cxx_warning_flag(date-time)
  detect_and_set_c_and_cxx_warning_flag(null-dereference)
  detect_and_set_c_and_cxx_warning_flag(duplicate-cond)
  detect_and_set_cxx_warning_flag(extra-semi)
  detect_and_set_c_and_cxx_warning_flag(comma)
  detect_and_set_c_and_cxx_warning_flag(float-conversion)
  check_c_compiler_flag("-Wdocumentation -Wno-documentation-deprecated-sync" HAVE_WFLAG_DOCUMENTATION_AND_NO_DEPRECATED)
  if (HAVE_WFLAG_DOCUMENTATION_AND_NO_DEPRECATED)
    set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -Wdocumentation -Wno-documentation-deprecated-sync)
    set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -Wdocumentation -Wno-documentation-deprecated-sync)
  endif ()
  detect_and_set_cxx_warning_flag(unused-private-field)
  detect_and_set_cxx_warning_flag(non-virtual-dtor)
  detect_and_set_cxx_warning_flag(overloaded-virtual)
  detect_and_set_cxx_warning_flag(suggest-override)
  # clang specific only. Avoids that foo("bar") goes to foo(bool) instead of foo(const std::string&)
  detect_and_set_cxx_warning_flag(string-conversion)

  check_cxx_compiler_flag(-fno-operator-names HAVE_FLAG_NO_OPERATOR_NAMES)
  if (HAVE_FLAG_NO_OPERATOR_NAMES)
    set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -fno-operator-names)
  endif ()

  check_cxx_compiler_flag(-Wzero-as-null-pointer-constant HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT)
  if (HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT)
    set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -Wzero-as-null-pointer-constant)
  endif ()

  # Detect -Wold-style-cast but do not add it by default, as not all targets support it
  check_cxx_compiler_flag(-Wold-style-cast HAVE_WFLAG_OLD_STYLE_CAST)
  if (HAVE_WFLAG_OLD_STYLE_CAST)
    set(WFLAG_OLD_STYLE_CAST -Wold-style-cast)
  endif ()

  # Detect Weffc++ but do not add it by default, as not all targets support it
  check_cxx_compiler_flag(-Weffc++ HAVE_WFLAG_EFFCXX)
  if (HAVE_WFLAG_EFFCXX)
    set(WFLAG_EFFCXX -Weffc++)
  endif ()

  if (CMAKE_BUILD_TYPE MATCHES Debug)
    check_c_compiler_flag(-ftrapv HAVE_FTRAPV)
    if (HAVE_FTRAPV)
      set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -ftrapv)
      set(GDAL_CXX_WARNING_FLAGS ${GDAL_CXX_WARNING_FLAGS} -ftrapv)
    endif ()
  endif ()

endif ()

add_compile_definitions($<$<CONFIG:DEBUG>:DEBUG>)

# message(STATUS "GDAL_C_WARNING_FLAGS: ${GDAL_C_WARNING_FLAGS}") message(STATUS "GDAL_CXX_WARNING_FLAGS: ${GDAL_CXX_WARNING_FLAGS}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  check_cxx_compiler_flag(-fno-finite-math-only HAVE_FLAG_NO_FINITE_MATH_ONLY)
  if (HAVE_FLAG_NO_FINITE_MATH_ONLY)
    # Intel CXX compiler based on clang defaults to -ffinite-math-only, which breaks std::isinf(), std::isnan(), etc.
    add_compile_options("-fno-finite-math-only")
  endif ()

  set(TEST_LINK_STDCPP_SOURCE_CODE
      "#include <string>
    int main(){
      std::string s;
      s += \"x\";
      return 0;
    }")
  check_cxx_source_compiles("${TEST_LINK_STDCPP_SOURCE_CODE}" _TEST_LINK_STDCPP)
  if( NOT _TEST_LINK_STDCPP )
      message(WARNING "Cannot link code using standard C++ library. Automatically adding -lstdc++ to CMAKE_EXE_LINKER_FLAGS, CMAKE_SHARED_LINKER_FLAGS and CMAKE_MODULE_LINKER_FLAGS")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lstdc++")
      set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -lstdc++")

      check_cxx_source_compiles("${TEST_LINK_STDCPP_SOURCE_CODE}" _TEST_LINK_STDCPP_AGAIN)
      if( NOT _TEST_LINK_STDCPP_AGAIN )
          message(FATAL_ERROR "Cannot link C++ program")
      endif()
  endif()

  check_c_compiler_flag(-wd188 HAVE_WD188) # enumerated type mixed with another type
  if( HAVE_WD188 )
    set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -wd188)
  endif()
  check_c_compiler_flag(-wd2259 HAVE_WD2259) # non-pointer conversion from ... may lose significant bits
  if( HAVE_WD2259 )
    set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -wd2259)
  endif()
  check_c_compiler_flag(-wd2312 HAVE_WD2312) # pointer cast involving 64-bit pointed-to type
  if( HAVE_WD2259 )
    set(GDAL_C_WARNING_FLAGS ${GDAL_C_WARNING_FLAGS} -wd2312)
  endif()
endif ()

# Default definitions during build
add_definitions(-DGDAL_COMPILATION)

if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-DNOMINMAX)
endif ()

if (MINGW)
  if (TARGET_CPU MATCHES "x86_64")
    add_definitions(-m64)
  endif ()
  # Workaround for export too large error - force problematic large file to be optimized to prevent string table
  # overflow error Used -Os instead of -O2 as previous issues had mentioned, since -Os is roughly speaking -O2,
  # excluding any optimizations that take up extra space. Given that the issue is a string table overflowing, -Os seemed
  # appropriate. Solves issue of https://github.com/OSGeo/gdal/issues/4706 with for example x86_64-w64-mingw32-gcc-posix
  # (GCC) 9.3-posix 20200320
  if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE STREQUAL "")
    add_compile_options(-Os)
  endif ()
endif ()
