add_gdal_driver(TARGET gdal_CEOS SOURCES ceosopen.h ceosdataset.cpp ceosopen.c)
gdal_standard_includes(gdal_CEOS)
add_executable(ceostest EXCLUDE_FROM_ALL ceostest.c)
