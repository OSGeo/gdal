message(STATUS "Building javadoc")

file(GLOB_RECURSE SOURCE_JAVA_FILES RELATIVE "${BUILD_DIR}" "${BUILD_DIR}/org/*.java")

# Duplicate org to ogr_patched/
file(REMOVE_RECURSE "${BUILD_DIR}/org_patched")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${BUILD_DIR}/org" "${BUILD_DIR}/org_patched/org")

# Patch the generated SWIG Java files to add the Javadoc into them
# thanks to the small utility add_javadoc
execute_process(COMMAND ${ADD_JAVADOC_EXE} "${SOURCE_DIR}/javadoc.java" "${BUILD_DIR}/org_patched" ${SOURCE_JAVA_FILES})

# Generate the HTML Javadoc
file(REMOVE_RECURSE "${BUILD_DIR}/java")

file(COPY "${SOURCE_DIR}/gdal-package-info.java" DESTINATION "${BUILD_DIR}/org_patched/org/gdal/gdal")
file(RENAME "${BUILD_DIR}/org_patched/org/gdal/gdal/gdal-package-info.java" "${BUILD_DIR}/org_patched/org/gdal/gdal/package-info.java")

file(COPY "${SOURCE_DIR}/gdalconst-package-info.java" DESTINATION "${BUILD_DIR}/org_patched/org/gdal/gdalconst")
file(RENAME "${BUILD_DIR}/org_patched/org/gdal/gdalconst/gdalconst-package-info.java" "${BUILD_DIR}/org_patched/org/gdal/gdalconst/package-info.java")

file(COPY "${SOURCE_DIR}/ogr-package-info.java" DESTINATION "${BUILD_DIR}/org_patched/org/gdal/ogr")
file(RENAME "${BUILD_DIR}/org_patched/org/gdal/ogr/ogr-package-info.java" "${BUILD_DIR}/org_patched/org/gdal/ogr/package-info.java")

file(COPY "${SOURCE_DIR}/osr-package-info.java" DESTINATION "${BUILD_DIR}/org_patched/org/gdal/osr")
file(RENAME "${BUILD_DIR}/org_patched/org/gdal/osr/osr-package-info.java" "${BUILD_DIR}/org_patched/org/gdal/osr/package-info.java")

execute_process(COMMAND ${Java_JAVADOC_EXECUTABLE}
                        -overview overview.html
                        -public
                        -d "${BUILD_DIR}/java"
                        -sourcepath "${BUILD_DIR}/org_patched"
                        -subpackages org.gdal
                        -link http://java.sun.com/javase/6/docs/api
                        -windowtitle "GDAL/OGR ${GDAL_VERSION} Java bindings API"
                WORKING_DIRECTORY "${BUILD_DIR}")

# Create a zip with the Javadoc
file(REMOVE "${BUILD_DIR}/javadoc.zip")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar "cfv" "javadoc.zip" --format=zip java
                WORKING_DIRECTORY "${BUILD_DIR}")
