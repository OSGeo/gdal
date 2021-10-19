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
          geoconcept_syscoord.c
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(ogr_Geoconcept)
if (MSVC)
  target_compile_options(ogr_Geoconcept PRIVATE /wd4706)
endif ()
