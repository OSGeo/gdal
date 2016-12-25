#----------------------------------------------
# installation path settings
#----------------------------------------------
if(WIN32)
  if(DEFINED ENV{OSGEO4W_ROOT})
    set(OSGEO4W_ROOT_DIR $ENV{OSGEO4W_ROOT})
  else()
    set(OSGEO4W_ROOT_DIR c:/OSGeo4W)
  endif()
  set(DEFAULT_PROJ_ROOT_DIR ${OSGEO4W_ROOT_DIR})
endif()
if(UNIX)
  set(DEFAULT_PROJ_ROOT_DIR "/usr/local/")
endif(UNIX)


IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	SET(CMAKE_INSTALL_PREFIX ${DEFAULT_PROJ_ROOT_DIR} CACHE PATH "Foo install
		 prefix" FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

#TODO 
# for data install testing the PROJ_LIB envVar

if(WIN32)
  set(DEFAULT_BIN_SUBDIR bin)
  set(DEFAULT_LIB_SUBDIR local/lib)
  set(DEFAULT_DATA_SUBDIR share)
  set(DEFAULT_INCLUDE_SUBDIR local/include)
  set(DEFAULT_DOC_SUBDIR share/doc/proj)
else()
  # Common locatoins for Unix and Mac OS X
  set(DEFAULT_BIN_SUBDIR bin)
  set(DEFAULT_LIB_SUBDIR lib)
  set(DEFAULT_DATA_SUBDIR share/proj)
  set(DEFAULT_DOC_SUBDIR doc/proj)
  set(DEFAULT_INCLUDE_SUBDIR include)
endif()

# Locations are changeable by user to customize layout of PDAL installation
# (default values are platform-specific)
set(PROJ_BIN_SUBDIR ${DEFAULT_BIN_SUBDIR} CACHE STRING
  "Subdirectory where executables will be installed")
set(PROJ_LIB_SUBDIR ${DEFAULT_LIB_SUBDIR} CACHE STRING
  "Subdirectory where libraries will be installed")
set(PROJ_INCLUDE_SUBDIR ${DEFAULT_INCLUDE_SUBDIR} CACHE STRING
  "Subdirectory where header files will be installed")
set(PROJ_DATA_SUBDIR ${DEFAULT_DATA_SUBDIR} CACHE STRING
  "Subdirectory where data will be installed")
set(PROJ_DOC_SUBDIR ${DEFAULT_DOC_SUBDIR} CACHE STRING
  "Subdirectory where data will be installed")

# Mark *DIR variables as advanced and dedicated to use by power-users only.
mark_as_advanced(PROJ_ROOT_DIR
                 PROJ_BIN_SUBDIR
                 PROJ_LIB_SUBDIR 
                 PROJ_INCLUDE_SUBDIR 
                 PROJ_DATA_SUBDIR
                 PROJ_DOC_SUBDIR )

set(DEFAULT_BINDIR "${PROJ_BIN_SUBDIR}")
set(DEFAULT_LIBDIR "${PROJ_LIB_SUBDIR}")
set(DEFAULT_DATADIR "${PROJ_DATA_SUBDIR}")
set(DEFAULT_DOCDIR "${PROJ_DOC_SUBDIR}")
set(DEFAULT_INCLUDEDIR "${PROJ_INCLUDE_SUBDIR}")


