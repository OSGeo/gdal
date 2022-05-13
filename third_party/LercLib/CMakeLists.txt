add_library(
  lerc OBJECT
  BitMask.cpp
  BitMask.h
  BitStuffer2.cpp
  BitStuffer2.h
  Defines.h
  Huffman.cpp
  Huffman.h
  Lerc.cpp
  Lerc.h
  Lerc2.cpp
  Lerc2.h
  Lerc_c_api.h
  Lerc_c_api_impl.cpp
  Lerc_types.h
  RLE.cpp
  RLE.h)
set_property(TARGET lerc PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:lerc>)
target_compile_options(lerc PRIVATE ${GDAL_CXX_WARNING_FLAGS})
