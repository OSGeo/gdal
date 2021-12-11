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
          PLUGIN_CAPABLE)
gdal_standard_includes(ogr_MSSQLSpatial)
gdal_target_link_libraries(TARGET ogr_MSSQLSpatial LIBRARIES ODBC::ODBC)

if (GDAL_USE_MSSQL_ODBC)
  if (GDAL_USE_MSSQL_NCLI)
    message(WARNING "Both GDAL_USE_MSSQL_ODBC and GDAL_USE_MSSQL_NCLI are set. Ignoring GDAL_USE_MSSQL_NCLI")
  endif ()
  gdal_target_link_libraries(TARGET ogr_MSSQLSpatial LIBRARIES MSSQL_ODBC::MSSQL_ODBC)
elseif (GDAL_USE_MSSQL_NCLI)
  gdal_target_link_libraries(TARGET ogr_MSSQLSpatial LIBRARIES MSSQL_NCLI::MSSQL_NCLI)
endif ()
