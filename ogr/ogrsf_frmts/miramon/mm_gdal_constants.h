#ifndef __MM_GDAL_CONSTANTS_H
#define __MM_GDAL_CONSTANTS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon                           */
/* -------------------------------------------------------------------- */
#include "cpl_port.h"  // For GUInt64

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling C in GDAL project
#endif       // GDAL_COMPILATION

#if defined(_WIN32) && !defined(strcasecmp)
#define strcasecmp stricmp
#endif

    // Common types
    // Type of the Feature ID: determines the maximum number of features in a layer.
    typedef GUInt64 MM_INTERNAL_FID;
// Offset to the coordinates of the Features.
typedef GUInt64 MM_FILE_OFFSET;

// Type of the coordinates of a Point, Arc or Polygons points.
typedef double MM_COORD_TYPE;

// Points

// StringLines (or Arcs)
typedef GUInt64 MM_N_VERTICES_TYPE;  // size_t in MiraMon

// Polygons (or polypolygons)
typedef GUInt64 MM_POLYGON_ARCS_COUNT;
typedef GUInt64 MM_POLYGON_RINGS_COUNT;

// Z Part
typedef int MM_SELEC_COORDZ_TYPE;

// Extended DBF
// Type of the number of fields of an extended DBF
typedef GUInt32
    MM_EXT_DBF_N_FIELDS;  //(TIPUS_NUMERADOR_CAMP in MiraMon internal code)
#define MM_MAX_EXT_DBF_N_FIELDS_TYPE UINT32_MAX

#define MM_TIPUS_BYTES_PER_CAMP_DBF GUInt32
#define MM_TIPUS_BYTES_ACUMULATS_DBF GUInt32

// Type of the number of records of an extended DBF
typedef GUInt32 MM_EXT_DBF_N_MULTIPLE_RECORDS;
typedef GUInt64 MM_EXT_DBF_N_RECORDS;
typedef GInt64 MM_EXT_DBF_SIGNED_N_RECORDS;
#define scanf_MM_EXT_DBF_SIGNED_N_RECORDS "%lld"
typedef GInt32 MM_FIRST_RECORD_OFFSET_TYPE;

typedef GInt32 MM_N_HEIGHT_TYPE;

#define MM_ARC_ALCADA_PER_CADA_VERTEX 1
#define MM_ARC_ALCADA_CONSTANT -1
#define MM_ARC_TIPUS_ALCADA(n)                                                 \
    (((n) < 0) ? MM_ARC_ALCADA_CONSTANT : MM_ARC_ALCADA_PER_CADA_VERTEX)
#define MM_ARC_N_ALCADES(n) (((n) < 0) ? -n : n)
#define MM_ARC_N_TOTAL_ALCADES_DISC(n, n_vrt)                                  \
    (((n) < 0) ? -n : (n) * (MM_N_HEIGHT_TYPE)(n_vrt))

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_GDAL_CONSTANTS_H
