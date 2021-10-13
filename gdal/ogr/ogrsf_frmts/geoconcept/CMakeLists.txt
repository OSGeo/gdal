add_gdal_driver(
  TARGET ogr_Geoconcept
  SOURCES geoconcept.h
          ogrgeoconceptdatasource.cpp
          ogrgeoconceptdriver.cpp
          ogrgeoconceptlayer.cpp
          geoconcept_syscoord.h
          ogrgeoconceptdatasource.h
          ogrgeoconceptdriver.h
          ogrgeoconceptlayer.h
          geoconcept.c
          geoconcept_syscoord.c)
gdal_standard_includes(ogr_Geoconcept)
if (USE_CPL)
  target_compile_definitions(ogr_Geoconcept PRIVATE -DUSE_CPL)
endif ()
