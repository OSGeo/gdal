#ifndef __MM_GDAL_CONSTANTS_H
#define __MM_GDAL_CONSTANTS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon                           */
/* -------------------------------------------------------------------- */
#ifdef GDAL_COMPILATION
CPL_C_START // Necessary for compiling C in GDAL project
#else
typedef __int32             GInt32;
typedef unsigned __int32    GUInt32;
typedef __int64             GInt64;
typedef unsigned __int64    GUInt64;
#endif

#include <stddef.h> // For size_t 
#if defined(_WIN32)
#define strcasecmp stricmp
#endif

#define nullptr NULL

#define MM_UNDEFINED_STATISTICAL_VALUE (2.9E+301)

#define MM_CPL_PATH_BUF_SIZE 2048

// BIT 1
#define MM_BIT_1_ON     0x02    // Generated using MiraMon
// BIT 3
#define MM_BIT_3_ON     0x08    // Multipolygon
// BIT 4
#define MM_BIT_4_ON     0x10    // 3d
// BIT 5
#define MM_BIT_5_ON     0x20    // Explicital polygons (every polygon has only one arc)

#define MM_CREATED_USING_MIRAMON    MM_BIT_1_ON
#define MM_LAYER_MULTIPOLYGON       MM_BIT_3_ON
#define MM_LAYER_3D_INFO            MM_BIT_4_ON

#define MM_BOOLEAN  char
#define MM_HANDLE   void *

#define MM_MAX_PATH                         260
#define MM_MESSAGE_LENGHT                   512
#define MM_MAX_BYTES_FIELD_DESC             360
#define MM_MAX_BYTES_IN_A_FIELD_EXT         (UINT32_MAX-1)
#define MM_MAX_LON_FIELD_NAME_DBF           129
#define MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF 11
#define MM_MAX_LON_UNITATS                  66
#define MM_MAX_LON_UNITATS_CAMP MM_MAX_LON_UNITATS

// Types of all components of a MiraMon feature

// Common types
// Type of the Feature ID: determines the maximum number of features in a layer.
typedef GUInt64 MM_INTERNAL_FID;
// Offset to the coordinates of the Features.
typedef GUInt64 MM_FILE_OFFSET;

// Type of the coordinates of a Point, Arc or Polygons points.
typedef double MM_COORD_TYPE;

// Points

// StringLines (or Arcs)
typedef GUInt64 MM_N_VERTICES_TYPE; // size_t in MiraMon

// Polygons (or polypolygons)
typedef GUInt64 MM_POLYGON_ARCS_COUNT;
typedef GUInt64 MM_POLYGON_RINGS_COUNT; 

#define MM_POL_EXTERIOR_SIDE  0x01
#define MM_POL_END_RING       0x02
#define MM_POL_REVERSE_ARC    0x04


// Z Part
typedef int MM_SELEC_COORDZ_TYPE;
#define MM_SELECT_FIRST_COORDZ      0
#define MM_SELECT_HIGHEST_COORDZ    1
#define MM_SELECT_LOWEST_COORDZ     2

#define MM_STRING_HIGHEST_ALTITUDE  0x0001
#define MM_STRING_LOWEST_ALTITUDE   0x0002

#define /*double*/ MM_NODATA_COORD_Z (-1.0E+300)


// General static variables
#define MM_MAX_LEN_LAYER_NAME           255
#define MM_MAX_LEN_LAYER_IDENTIFIER     255

#define MM_TYPICAL_NODE                 0
#define MM_LINE_NODE                    1
#define MM_RING_NODE                    2
#define MM_FINAL_NODE                   3

#define MM_MAX_ID_SNY                  41


// Extended DBF
// Type of the number of fields of an extended DBF
typedef GUInt32 MM_EXT_DBF_N_FIELDS;  //(TIPUS_NUMERADOR_CAMP in MiraMon internal code) 
// Type of the number of records of an extended DBF
typedef GUInt32 MM_EXT_DBF_N_MULTIPLE_RECORDS;
typedef GUInt64 MM_EXT_DBF_N_RECORDS;
typedef GInt64 MM_EXT_DBF_SIGNED_N_RECORDS;
#define scanf_MM_EXT_DBF_SIGNED_N_RECORDS  "%lld"
typedef GInt32 MM_FIRST_RECORD_OFFSET_TYPE;

#define MM_MAX_EXT_DBF_N_FIELDS_TYPE            UINT32_MAX
#define MM_MAX_N_CAMPS_DBF_CLASSICA             255 
#define MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA      254 

#define MM_MARCA_VERSIO_1_DBF_ESTESA            0x90
#define MM_MARCA_DBASE4                         0x03
#define MM_MAX_LON_RESERVAT_1_BASE_DADES_XP     2
#define MM_MAX_LON_DBF_ON_A_LAN_BASE_DADES_XP   12
#define MM_MAX_LON_RESERVAT_2_BASE_DADES_XP     2


#define MM_TIPUS_BYTES_PER_CAMP_DBF     GUInt32
#define MM_TIPUS_BYTES_ACUMULATS_DBF    GUInt32

#define MM_MAX_LON_DESCRIPCIO_CAMP_DBF MM_CPL_PATH_BUF_SIZE+100
#define MM_NUM_IDIOMES_MD_MULTIDIOMA 4

#define MM_CAMP_NO_MOSTRABLE                    0
#define MM_CAMP_MOSTRABLE                       1
#define MM_CAMP_MOSTRABLE_QUAN_TE_CONTINGUT     4
#define MM_CAMP_QUE_MOSTRA_TESAURE              2
#define MM_CAMP_QUE_MOSTRA_TAULA_BDXP_O_BDODBC  3

#define MM_CAMP_INDETERMINAT        0 
#define MM_CAMP_CATEGORIC           1
#define MM_CAMP_ORDINAL             2
#define MM_CAMP_QUANTITATIU_CONTINU 3

#define MM_CAMP_NO_SIMBOLITZABLE    0
#define MM_CAMP_SIMBOLITZABLE       1


#define MM_NO_ES_CAMP_GEOTOPO       0
#define MM_CAMP_ES_ID_GRAFIC        1
#define MM_CAMP_ES_MAPA_X           2
#define MM_CAMP_ES_MAPA_Y           3
#define MM_CAMP_ES_MAPA_Z           17
#define MM_CAMP_ES_N_VERTEXS        4
#define MM_CAMP_ES_LONG_ARC         5
#define MM_CAMP_ES_LONG_ARCE        6
#define MM_CAMP_ES_NODE_INI         7
#define MM_CAMP_ES_NODE_FI          8
#define MM_CAMP_ES_ARCS_A_NOD       9
#define MM_CAMP_ES_TIPUS_NODE       10
#define MM_CAMP_ES_PERIMETRE        11
#define MM_CAMP_ES_PERIMETREE       12
#define MM_CAMP_ES_PERIMETRE_3D     18    
#define MM_CAMP_ES_AREA             13
#define MM_CAMP_ES_AREAE            14
#define MM_CAMP_ES_AREA_3D          19
#define MM_CAMP_ES_N_ARCS           15
#define MM_CAMP_ES_N_POLIG          16
#define MM_CAMP_ES_PENDENT          20
#define MM_CAMP_ES_ORIENTACIO       21

#define MM_JOC_CARAC_ANSI_MM        1252
#define MM_JOC_CARAC_ANSI_DBASE     0x58
#define MM_JOC_CARAC_OEM850_MM      850
#define MM_JOC_CARAC_OEM850_DBASE   0x14
#define MM_JOC_CARAC_UTF8_DBF       0xFF
#define MM_JOC_CARAC_UTF8_MM        8

enum MM_TipusNomCamp { MM_NOM_DBF_CLASSICA_I_VALID=0, MM_NOM_DBF_MINUSCULES_I_VALID, MM_NOM_DBF_ESTES_I_VALID, MM_NOM_DBF_NO_VALID};
#define MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES  11

#define MM_DonaBytesNomEstesCamp(camp)   ((MM_BYTE)((camp)->reservat_2[MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES]))

#define MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA 8
#define MM_SEGON_OFFSET_a_OFFSET_1a_FITXA  30

#define MM_FIRST_OFFSET_to_N_RECORDS    4
#define MM_SECOND_OFFSET_to_N_RECORDS   16

#define MM_NOM_CAMP_MASSA_LLARG         0x01
#define MM_NOM_CAMP_CARACTER_INVALID    0x02
#define MM_NOM_CAMP_PRIMER_CARACTER_    0x04

#define MM_MAX_AMPLADA_CAMP_N_DBF       21
#define MM_MAX_AMPLADA_CAMP_C_DBF       3//500  // But it can be modified on the fly
#define MM_MAX_AMPLADA_CAMP_D_DBF       10

#define MM_PRIVATE_POINT_DB_FIELDS      1
#define MM_PRIVATE_ARC_DB_FIELDS        5
#define MM_PRIVATE_POLYGON_DB_FIELDS    6

typedef unsigned char       MM_BYTE;
typedef GInt32      MM_N_HEIGHT_TYPE;

#define MM_NOU_N_DECIMALS_NO_APLICA          0
#define MM_APLICAR_NOU_N_DECIMALS            1
#define MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS   2
#define MM_PREGUNTA_SI_APLICAR_NOU_N_DECIM   3
#define MM_CARACTERS_DOUBLE                 40

#define MM_ARC_ALCADA_PER_CADA_VERTEX  1
#define MM_ARC_ALCADA_CONSTANT        -1
#define MM_ARC_TIPUS_ALCADA(n)        (((n)<0) ? MM_ARC_ALCADA_CONSTANT : MM_ARC_ALCADA_PER_CADA_VERTEX)
#define MM_ARC_N_ALCADES(n)           (((n)<0) ? -n: n)
#define MM_ARC_N_TOTAL_ALCADES_DISC(n,n_vrt) (((n)<0) ? -n : (n)*(MM_N_HEIGHT_TYPE)(n_vrt))

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_GDAL_CONSTANTS_H
