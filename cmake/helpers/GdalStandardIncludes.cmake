# Distributed under the GDAL/OGR MIT/X style License.  See accompanying
# file LICENSE.TXT.

#[=======================================================================[.rst:
GdalStandardIncludes
--------------------

#]=======================================================================]

function(gdal_standard_includes _TARGET)
  target_include_directories(${_TARGET} PRIVATE
                             $<TARGET_PROPERTY:appslib,SOURCE_DIR>
                             $<TARGET_PROPERTY:alg,SOURCE_DIR>
                             $<TARGET_PROPERTY:gcore,SOURCE_DIR>
                             $<TARGET_PROPERTY:gcore,BINARY_DIR>
                             $<TARGET_PROPERTY:cpl,SOURCE_DIR> # port
                             $<TARGET_PROPERTY:cpl,BINARY_DIR>
                             $<TARGET_PROPERTY:ogr,SOURCE_DIR>
                             $<TARGET_PROPERTY:ogrsf_frmts,SOURCE_DIR> # ogr/ogrsf_frmts
                             $<TARGET_PROPERTY:gdal_frmts,SOURCE_DIR> # frmts
                             )
endfunction()
