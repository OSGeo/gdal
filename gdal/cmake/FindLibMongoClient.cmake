# find libmongodb-client package
# stolen ideas from FindMySQL.cmake

if (LIBMONGODB_CLIENT_INCLUDE_DIR)
  # Already in cache, be silent
  set(LIBMONGODB_CLIENT_FIND_QUIETLY TRUE)
endif ()

find_path(LIBMONGODB_CLIENT_INCLUDE_DIR dbclient.h
        /usr/local/include/mongodb/client/
        /usr/include/mongodb/client/
        /usr/include/libmongo-client/
)

find_library(LIBMONGODB_CLIENT_LIBRARY
  NAMES libmongo-client
  PATHS /usr/lib /usr/local/lib
)

if (LIBMONGODB_CLIENT_INCLUDE_DIR AND LIBMONGODB_CLIENT_LIBRARY)
  set(LIBMONGODB_CLIENT_FOUND TRUE)
  set(LIBMONGODB_CLIENT_LIBRARIES ${LIBMONGODB_CLIENT_LIBRARY})
else ()
  set(LIBMONGODB_CLIENT_FOUND FALSE)
  set(LIBMONGODB_CLIENT_LIBRARIES)
endif ()

if (LIBMONGODB_CLIENT_FOUND)
  if (NOT LIBMONGODB_CLIENT_FIND_QUIETLY)
    MESSAGE(STATUS "Found libmongodb-client: ${LIBMONGODB_CLIENT_LIBRARY}")
  endif ()
else ()
  if (LIBMONGODB_CLIENT_FIND_REQUIRED)
    MESSAGE(STATUS "Looked for libmongodb-client library.")
    MESSAGE(FATAL_ERROR "Could NOT find libmongodb-client library")
  endif ()
endif ()

MARK_AS_ADVANCED(
  LIBMONGODB_CLIENT_LIBRARY
  LIBMONGODB_CLIENT_INCLUDE_DIR
)