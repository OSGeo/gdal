include(GdalStandardIncludes)

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
  string(REPLACE "/WX " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
endif ()

function (GDAL_JAVA_WRAP)
  set(_options CXX)
  set(_oneValueArgs TARGET)
  set(_multiValueArgs ARGS)
  cmake_parse_arguments(_SWIG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  if (_SWIG_CXX)
    set(SWIG_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.cpp)
    add_custom_command(
      OUTPUT ${SWIG_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}
      COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal -c++ -java -o ${SWIG_OUTPUT}
              ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_TARGET}.i
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES} ${PROJECT_SOURCE_DIR}/swig/include/java/typemaps_java.i
              ${PROJECT_SOURCE_DIR}/swig/include/java/${_SWIG_TARGET}_java.i)
    set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
  else ()
    set(SWIG_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}/${_SWIG_TARGET}_wrap.c)
    add_custom_command(
      OUTPUT ${SWIG_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${_SWIG_TARGET}
      COMMAND ${SWIG_EXECUTABLE} ${_SWIG_ARGS} ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal -java -o ${SWIG_OUTPUT}
              ${PROJECT_SOURCE_DIR}/swig/include/${_SWIG_TARGET}.i
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${GDAL_SWIG_COMMON_INTERFACE_FILES} ${PROJECT_SOURCE_DIR}/swig/include/java/typemaps_java.i
              ${PROJECT_SOURCE_DIR}/swig/include/java/${_SWIG_TARGET}_java.i)
    set_source_files_properties(${SWIG_OUTPUT} PROPERTIES GENERATED 1)
  endif ()

endfunction ()

gdal_java_wrap(
  TARGET gdalconst
  ARGS -Wall
       -I${PROJECT_SOURCE_DIR}/swig/include
       -I${PROJECT_SOURCE_DIR}/swig/include/java
       -outdir
       ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gdalconst/
       -package
       "org.gdal.gdalconst")
foreach (tgt IN ITEMS gdal ogr osr gnm)
  gdal_java_wrap(
    TARGET ${tgt}
    CXX
    ARGS -Wall
         -I${PROJECT_SOURCE_DIR}/swig/include
         -I${PROJECT_SOURCE_DIR}/swig/include/java
         -outdir
         ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/${tgt}/
         -package
         "org.gdal.${tgt}")
endforeach ()

# Build jni library
set(GDALALLJNI_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gdal/gdal_wrap.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gdalconst/gdalconst_wrap.c
    ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/ogr/ogr_wrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/osr/osr_wrap.cpp)
if (ENABLE_GNM)
  list(APPEND GDALALLJNI_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/org/gdal/gnm/gnm_wrap.cpp)
endif ()
add_library(gdalalljni SHARED ${GDALALLJNI_SOURCES})
gdal_standard_includes(gdalalljni)
target_include_directories(gdalalljni PRIVATE ${JNI_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
if (ENABLE_GNM)
  target_include_directories(gdalalljni PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
endif ()
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (_isMultiConfig)
  set_target_properties(gdalalljni PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/debug
                                              LIBRARY_OUTPUT_DIRECTORY_REL ${CMAKE_CURRENT_BINARY_DIR})
else ()
  set_property(TARGET gdalalljni PROPERTY LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif ()
target_link_libraries(gdalalljni PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

# Build javadoc
file(STRINGS ${PROJECT_SOURCE_DIR}/VERSION GDAL_VERSION)

option(GDAL_JAVA_GENERATE_JAVADOC "Generate Javadoc of Java bindings" ON)
if (GDAL_JAVA_GENERATE_JAVADOC)
  add_executable(add_javadoc add_javadoc.c)
  set(JAVADOC_ZIP ${CMAKE_CURRENT_BINARY_DIR}/javadoc.zip)
  add_custom_command(
    OUTPUT ${JAVADOC_ZIP}
    COMMAND
      ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DJava_JAVADOC_EXECUTABLE=${Java_JAVADOC_EXECUTABLE}" "-DADD_JAVADOC_EXE=$<TARGET_FILE:add_javadoc>"
      "-DGDAL_VERSION=${GDAL_VERSION}" -P "${CMAKE_CURRENT_SOURCE_DIR}/build_java_doc.cmake"
    VERBATIM
    DEPENDS ${GDALALLJNI_SOURCES} add_javadoc)
endif ()

# Build gdal.jar, test programs and Maven artifacts
file(GLOB SOURCE_APPS "${CMAKE_CURRENT_SOURCE_DIR}/apps/*.java")
set(JAR_OUTPUT gdal.jar)

set(GDAL_VERSION_JAR ${CMAKE_CURRENT_BINARY_DIR}/build/maven/gdal-${GDAL_VERSION}.jar)
set(GDAL_VERSION_SOURCES_JAR ${CMAKE_CURRENT_BINARY_DIR}/build/maven/gdal-${GDAL_VERSION}-sources.jar)
set(GDAL_VERSION_POM ${CMAKE_CURRENT_BINARY_DIR}/build/maven/gdal-${GDAL_VERSION}.pom)
set(MAVEN_ARTIFACTS ${GDAL_VERSION_JAR} ${GDAL_VERSION_SOURCES_JAR} ${GDAL_VERSION_POM})
if (GDAL_JAVA_GENERATE_JAVADOC)
  set(GDAL_VERSION_JAVADOC_JAR ${CMAKE_CURRENT_BINARY_DIR}/build/maven/gdal-${GDAL_VERSION}-javadoc.jar)
  list(APPEND MAVEN_ARTIFACTS ${GDAL_VERSION_JAVADOC_JAR})
endif ()

# Variables used to sign artifacts and generate swig/java/build/maven/bundle.jar
set(GPG_KEY
    ""
    CACHE STRING "GPG key used to sign Java binding artifacts")
set(GPG_PASS
    ""
    CACHE STRING "GPG pass phrase used to sign Java binding artifacts")
mark_as_advanced(GPG_KEY GPG_PASS)
if (GPG_KEY)
  set(ANT_TARGET maven_sign)
  set(GPG_KEY_PROPERTY "-Dgpg.key=${GPG_KEY}")
  if (GPG_PASS)
    set(GPG_PASS_PROPERTY "-Dgpg.pass=${GPG_PASS}")
  endif ()
else ()
  set(ANT_TARGET maven)
endif ()

add_custom_command(
  OUTPUT ${JAR_OUTPUT} ${MAVEN_ARTIFACTS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/java"
  COMMAND
    ${ANT} ${ANT_TARGET} -f ${CMAKE_CURRENT_SOURCE_DIR}/build.xml ${GPG_KEY_PROPERTY} ${GPG_PASS_PROPERTY}
    -Dskip_make_doc=true -Dbuild_dir=${CMAKE_CURRENT_BINARY_DIR} -Dversion_filename=${PROJECT_SOURCE_DIR}/VERSION
    -Dsource_apps=${CMAKE_CURRENT_SOURCE_DIR}/apps
  VERBATIM
  DEPENDS ${JAVADOC_ZIP} ${GDALALLJNI_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/build.xml ${SOURCE_APPS})

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "org;org_patched;build;tmp_test;java")

# java_binding target
add_custom_target(java_binding ALL DEPENDS gdalalljni ${MAVEN_ARTIFACTS})

# Install
set(GDAL_JAVA_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/java"
    CACHE PATH "Installation sub-directory for Java bindings")

install(
  TARGETS gdalalljni
  COMPONENT java
  DESTINATION ${GDAL_JAVA_INSTALL_DIR})

install(
  FILES ${GDAL_VERSION_JAR} ${GDAL_VERSION_JAVADOC_JAR} ${GDAL_VERSION_SOURCES_JAR} ${GDAL_VERSION_POM}
  COMPONENT java
  DESTINATION ${GDAL_JAVA_INSTALL_DIR})

# Tests
if (Java_Runtime_FOUND)

  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/tmp_test)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tmp_test)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data/byte.tif DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tmp_test)

  if (WIN32)
    set(CLASSPATH_SEPARATOR "\\;")
  else ()
    set(CLASSPATH_SEPARATOR ":")
  endif ()

  include(GdalSetRuntimeEnv)
  gdal_set_runtime_env(TEST_ENV)

  set(JAVA_RUN ${Java_JAVA_EXECUTABLE} "-Djava.library.path=$<SHELL_PATH:$<TARGET_FILE_DIR:gdalalljni>>" -cp
               "gdal.jar${CLASSPATH_SEPARATOR}build/apps")
  add_test(NAME java_GDALOverviews COMMAND ${JAVA_RUN} GDALOverviews tmp_test/byte.tif "NEAREST" 2 4)
  add_test(NAME java_gdalinfo COMMAND ${JAVA_RUN} gdalinfo -checksum tmp_test/byte.tif)
  add_test(NAME java_ogr2ogr_1 COMMAND ${JAVA_RUN} ogr2ogr tmp_test/out.shp
                                       ${CMAKE_CURRENT_SOURCE_DIR}/test_data/poly.shp -progress -overwrite)
  add_test(NAME java_ogrinfo_1 COMMAND ${JAVA_RUN} ogrinfo -ro -al tmp_test/out.shp)
  set_tests_properties(java_ogrinfo_1 PROPERTIES DEPENDS java_ogr2ogr_1)
  add_test(NAME java_ogrinfo_2 COMMAND ${JAVA_RUN} ogrinfo -ro -al tmp_test/out.shp -fid 1)
  set_tests_properties(java_ogrinfo_1 PROPERTIES DEPENDS java_ogr2ogr_1)
  add_test(NAME java_ogr2ogr_2 COMMAND ${JAVA_RUN} ogr2ogr_new tmp_test/out2.shp
                                       ${CMAKE_CURRENT_SOURCE_DIR}/test_data/poly.shp -progress -overwrite)
  add_test(NAME java_ogr2ogr_3 COMMAND ${JAVA_RUN} ogr2ogr_new tmp_test/out2.shp
                                       ${CMAKE_CURRENT_SOURCE_DIR}/test_data/poly.shp -append)
  set_tests_properties(java_ogr2ogr_3 PROPERTIES DEPENDS java_ogr2ogr_2)
  add_test(NAME java_ogrinfo_3 COMMAND ${JAVA_RUN} ogrinfo -ro -al tmp_test/out2.shp)
  set_tests_properties(java_ogrinfo_3 PROPERTIES DEPENDS java_ogr2ogr_3)
  add_test(NAME java_OSRTransform COMMAND ${JAVA_RUN} OSRTransform)
  add_test(NAME java_gdalmajorobject COMMAND ${JAVA_RUN} gdalmajorobject)
  add_test(NAME java_GDALTestIO COMMAND ${JAVA_RUN} GDALTestIO)
  add_test(NAME java_GDALContour COMMAND ${JAVA_RUN} GDALContour -i 1 tmp_test/byte.tif tmp_test/contour.shp)
  add_test(NAME java_testgetpoints COMMAND ${JAVA_RUN} testgetpoints)
  add_test(NAME java_ogrtindex COMMAND ${JAVA_RUN} ogrtindex tmp_test/contour_index.shp tmp_test/contour.shp)
  set_tests_properties(java_ogrtindex PROPERTIES DEPENDS java_GDALContour)
  add_test(NAME java_OSRTest COMMAND ${JAVA_RUN} OSRTest)

  foreach (
    test_name IN
    ITEMS java_GDALOverviews
          java_gdalinfo
          java_ogr2ogr_1
          java_ogrinfo_1
          java_ogrinfo_2
          java_ogr2ogr_2
          java_ogr2ogr_3
          java_ogrinfo_3
          java_OSRTransform
          java_gdalmajorobject
          java_GDALTestIO
          java_GDALContour
          java_testgetpoints
          java_ogrtindex
          java_OSRTest)
    set_property(TEST ${test_name} PROPERTY ENVIRONMENT "${TEST_ENV}")
  endforeach ()

endif ()
