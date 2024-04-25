#ifndef __MM_CONSTANTS_H
#define __MM_CONSTANTS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon                           */
/* -------------------------------------------------------------------- */
#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling C in GDAL project
#else
#ifndef UINT32_MAX
#define UINT32_MAX _UI32_MAX
#endif
#endif  // GDAL_COMPILATION

#define MM_OFFSET_BYTESxCAMP_CAMP_CLASSIC 16
#define MM_OFFSET_BYTESxCAMP_CAMP_ESPECIAL 21
#define MM_MAX_LON_RESERVAT_1_CAMP_BD_XP 4
#define MM_OFFSET_RESERVAT2_BYTESxCAMP_CAMP_ESPECIAL 3
#define MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES 7
#define MM_OFFSET_RESERVED2_EXTENDED_NAME_SIZE 11
#define MM_MAX_LON_RESERVAT_2_CAMP_BD_XP 13

#define MM_ES_DBF_ESTESA(dbf_version)                                          \
    (((dbf_version) == MM_MARCA_VERSIO_1_DBF_ESTESA) ? TRUE : FALSE)

#define MM_UNDEFINED_STATISTICAL_VALUE (2.9E+301)
#define MM_CPL_PATH_BUF_SIZE 2048

// BIT 1
#define MM_BIT_1_ON 0x02  // Generated using MiraMon
// BIT 3
#define MM_BIT_3_ON 0x08  // Multipolygon
// BIT 4
#define MM_BIT_4_ON 0x10  // 3D
// BIT 5
#define MM_BIT_5_ON                                                            \
    0x20  // Explicital polygons (every polygon has only one arc)

#define MM_CREATED_USING_MIRAMON MM_BIT_1_ON
#define MM_LAYER_MULTIPOLYGON MM_BIT_3_ON
#define MM_LAYER_3D_INFO MM_BIT_4_ON

#define MM_BOOLEAN char
#define MM_HANDLE void *

#define MM_MESSAGE_LENGTH 512
#define MM_MAX_BYTES_FIELD_DESC 360
#define MM_MAX_BYTES_IN_A_FIELD_EXT (UINT32_MAX - 1)
#define MM_MAX_LON_FIELD_NAME_DBF 129
#define MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF 11
#define MM_MAX_LON_UNITATS 66
#define MM_MAX_LON_UNITATS_CAMP MM_MAX_LON_UNITATS

// Determines if an arc is external, the last one in a ring or
// if it has to be inverted to be consistent with other arcs
// in the ring.
#define MM_POL_EXTERIOR_SIDE 0x01
#define MM_POL_END_RING 0x02
#define MM_POL_REVERSE_ARC 0x04

// Z Part
#define MM_SELECT_FIRST_COORDZ 0
#define MM_SELECT_HIGHEST_COORDZ 1
#define MM_SELECT_LOWEST_COORDZ 2

#define MM_STRING_HIGHEST_ALTITUDE 0x0001
#define MM_STRING_LOWEST_ALTITUDE 0x0002

#define /*double*/ MM_NODATA_COORD_Z (-1.0E+300)

// General static variables
#define MM_MAX_LEN_LAYER_NAME 255
#define MM_MAX_LEN_LAYER_IDENTIFIER 255

#define MM_TYPICAL_NODE 0
#define MM_LINE_NODE 1
#define MM_RING_NODE 2
#define MM_FINAL_NODE 3

#define MM_MAX_ID_SNY 41

#ifndef GDAL_COMPILATION
    typedef unsigned int uint32_t;
typedef int int32_t;
#endif

// Extended DBF
// Type of the number of records of an extended DBF
#define MM_MAX_N_CAMPS_DBF_CLASSICA 255
#define MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA 254

#define MM_MARCA_VERSIO_1_DBF_ESTESA 0x90
#define MM_MARCA_DBASE4 0x03
#define MM_MAX_LON_RESERVAT_1_BASE_DADES_XP 2
#define MM_MAX_LON_DBF_ON_A_LAN_BASE_DADES_XP 12
#define MM_MAX_LON_RESERVAT_2_BASE_DADES_XP 2

#define MM_MAX_LON_DESCRIPCIO_CAMP_DBF MM_CPL_PATH_BUF_SIZE + 100
#define MM_NUM_IDIOMES_MD_MULTIDIOMA 4

#define MM_CAMP_NO_MOSTRABLE 0
#define MM_CAMP_MOSTRABLE 1
#define MM_CAMP_MOSTRABLE_QUAN_TE_CONTINGUT 4
#define MM_CAMP_QUE_MOSTRA_TESAURE 2
#define MM_CAMP_QUE_MOSTRA_TAULA_BDXP_O_BDODBC 3

#define MM_CAMP_INDETERMINAT 0
#define MM_CATEGORICAL_FIELD 1
#define MM_CAMP_ORDINAL 2
#define MM_QUANTITATIVE_CONTINUOUS_FIELD 3

#define MM_CAMP_NO_SIMBOLITZABLE 0
#define MM_CAMP_SIMBOLITZABLE 1

#define MM_NO_ES_CAMP_GEOTOPO 0
#define MM_CAMP_ES_ID_GRAFIC 1
#define MM_CAMP_ES_MAPA_X 2
#define MM_CAMP_ES_MAPA_Y 3
#define MM_CAMP_ES_MAPA_Z 17
#define MM_CAMP_ES_N_VERTEXS 4
#define MM_CAMP_ES_LONG_ARC 5
#define MM_CAMP_ES_LONG_ARCE 6
#define MM_CAMP_ES_NODE_INI 7
#define MM_CAMP_ES_NODE_FI 8
#define MM_CAMP_ES_ARCS_A_NOD 9
#define MM_CAMP_ES_TIPUS_NODE 10
#define MM_CAMP_ES_PERIMETRE 11
#define MM_CAMP_ES_PERIMETREE 12
#define MM_CAMP_ES_PERIMETRE_3D 18
#define MM_CAMP_ES_AREA 13
#define MM_CAMP_ES_AREAE 14
#define MM_CAMP_ES_AREA_3D 19
#define MM_CAMP_ES_N_ARCS 15
#define MM_CAMP_ES_N_POLIG 16
#define MM_CAMP_ES_PENDENT 20
#define MM_CAMP_ES_ORIENTACIO 21

#define MM_JOC_CARAC_ANSI_MM 1252
#define MM_JOC_CARAC_ANSI_DBASE 0x58
#define MM_JOC_CARAC_OEM850_MM 850
#define MM_JOC_CARAC_OEM850_DBASE 0x14
#define MM_JOC_CARAC_UTF8_DBF 0xFF
#define MM_JOC_CARAC_UTF8_MM 8

typedef unsigned char MM_BYTE;

#define MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA 8
#define MM_SEGON_OFFSET_a_OFFSET_1a_FITXA 30

#define MM_FIRST_OFFSET_to_N_RECORDS 4
#define MM_SECOND_OFFSET_to_N_RECORDS 16

#define MM_FIELD_NAME_TOO_LONG 0x01
#define MM_FIELD_NAME_CHARACTER_INVALID 0x02
#define MM_FIELD_NAME_FIRST_CHARACTER_ 0x04

#define MM_MAX_AMPLADA_CAMP_N_DBF 21
#define MM_MAX_AMPLADA_CAMP_C_DBF 254
#define MM_MAX_AMPLADA_CAMP_D_DBF 10

#define MM_PRIVATE_POINT_DB_FIELDS 1
#define MM_PRIVATE_ARC_DB_FIELDS 5
#define MM_PRIVATE_POLYGON_DB_FIELDS 6

#define MM_CHARACTERS_DOUBLE 40

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_CONSTANTS_H
