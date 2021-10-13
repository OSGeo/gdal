add_library(
  lerc STATIC
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
set_property(TARGET lerc PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET lerc PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE lerc)
