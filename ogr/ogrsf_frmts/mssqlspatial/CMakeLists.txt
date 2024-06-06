add_gdal_driver(
  TARGET ogr_MSSQLSpatial
  SOURCES ogrmssqlgeometryparser.cpp
          ogrmssqlgeometryvalidator.cpp
          ogrmssqlgeometrywriter.cpp
          ogrmssqlspatialdatasource.cpp
          ogrmssqlspatialdriver.cpp
          ogrmssqlspatiallayer.cpp
          ogrmssqlspatialselectlayer.cpp
          ogrmssqlspatialtablelayer.cpp
  CORE_SOURCES
          ogrmssqlspatialdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET ogr_MSSQLSpatial_core)
    if (GDAL_USE_MSSQL_ODBC)
      target_compile_definitions(ogr_MSSQLSpatial_core PRIVATE $<TARGET_PROPERTY:MSSQL_ODBC::MSSQL_ODBC,INTERFACE_COMPILE_DEFINITIONS>)
    elseif (GDAL_USE_MSSQL_NCLI)
      target_compile_definitions(ogr_MSSQLSpatial_core PRIVATE $<TARGET_PROPERTY:MSSQL_NCLI::MSSQL_NCLI,INTERFACE_COMPILE_DEFINITIONS>)
    endif ()
endif()

if(NOT TARGET ogr_MSSQLSpatial)
    return()
endif()

gdal_standard_includes(ogr_MSSQLSpatial)
gdal_target_link_libraries(ogr_MSSQLSpatial PRIVATE ODBC::ODBC)

if (GDAL_USE_MSSQL_ODBC)
  if (GDAL_USE_MSSQL_NCLI)
    message(WARNING "Both GDAL_USE_MSSQL_ODBC and GDAL_USE_MSSQL_NCLI are set. Ignoring GDAL_USE_MSSQL_NCLI")
  endif ()
  gdal_target_link_libraries(ogr_MSSQLSpatial PRIVATE MSSQL_ODBC::MSSQL_ODBC)
elseif (GDAL_USE_MSSQL_NCLI)
  gdal_target_link_libraries(ogr_MSSQLSpatial PRIVATE MSSQL_NCLI::MSSQL_NCLI)
endif ()
