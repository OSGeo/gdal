file(GLOB PUBLIC_DECOMP_WT_SRC
     "${BINARY_DIR}/PublicDecompWT/COMP/WT/Src/*.cpp"
     "${BINARY_DIR}/PublicDecompWT/COMP/Src/*.cpp"
     "${BINARY_DIR}/PublicDecompWT/DISE/*.cpp")
set(PUBLIC_DECOMPTWT_ALL_H "// This is a generated file. Do not edit !
#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
// '<=': signed/unsigned mismatch
#pragma warning(disable : 4018)
#endif
")
foreach(_f IN LISTS PUBLIC_DECOMP_WT_SRC)
  if (NOT("${IS_WIN32}" AND (${_f} MATCHES "TimeSpan.cpp" OR ${_f} MATCHES "UTCTime.cpp")))
      string(APPEND PUBLIC_DECOMPTWT_ALL_H "#include \"${_f}\"
")
  endif()
endforeach()

string(APPEND PUBLIC_DECOMPTWT_ALL_H "
#ifdef _MSC_VER
#pragma warning(pop)
#endif
")

file(WRITE "${BINARY_DIR}/PublicDecompWT_all.h" "${PUBLIC_DECOMPTWT_ALL_H}")
