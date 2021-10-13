add_gdal_driver(TARGET gdal_BSB SOURCES bsb_read.h bsb_read.c bsbdataset.cpp)
gdal_standard_includes(gdal_BSB)
add_executable(bsb2raw EXCLUDE_FROM_ALL bsb2raw.c)
