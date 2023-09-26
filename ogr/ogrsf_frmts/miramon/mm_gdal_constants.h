#ifndef __MM_GDAL_CONSTANTS_H
#define __MM_GDAL_CONSTANTS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon                           */
/* -------------------------------------------------------------------- */
#ifdef GDAL_COMPILATION
CPL_C_START // Necessary for compiling C in GDAL project
#endif

#define MM_STATISTICAL_UNDEFINED_VALUE (2.9E+301)

// BIT 1
#define MM_BIT_1_ON     0x02    // Generated using MiraMon
// BIT 3
#define MM_BIT_3_ON     0x08    // Multipolygon
// BIT 4
#define MM_BIT_4_ON     0x10    // 3d
// BIT 5
#define MM_BIT_5_ON     0x20	// Explicital polígons (every polígon has only one arc)

#define MM_CREATED_USING_MIRAMON    MM_BIT_1_ON
#define MM_LAYER_MULTIPOLYGON       MM_BIT_3_ON
#define MM_LAYER_3D_INFO            MM_BIT_4_ON

#define MM_BOOLEAN  char
#define MM_HANDLE   void *

#define huge

#define MM_MAX_PATH                         260
#define MM_MESSAGE_LENGHT                   512
#define MM_MAX_BYTES_FIELD_DESC             360
#define MM_MAX_BYTES_IN_A_FIELD_EXT         (_UI32_MAX-1)
#define MM_MAX_LON_FIELD_NAME_DBF              129
#define MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF    11
#define MM_MAX_LON_UNITATS                  66
#define MM_MAX_LON_UNITATS_CAMP MM_MAX_LON_UNITATS

#define MM_NUMERATOR_DBF_FIELD_TYPE             unsigned __int32   //(TIPUS_NUMERADOR_CAMP)
#define MM_NUMERATOR_RECORD                     unsigned __int32   // ·$· <-- 64
#define MM_FILE_OFFSET                          unsigned __int64 
//#define MM_TIPUS_MIDA_FITXER                    __int64  
typedef __int64 MM_TIPUS_MIDA_FITXER;
#define MM_TIPUS_OFFSET_PRIMERA_FITXA           __int32 
#define MM_TIPUS_NUMERADOR_TAULA_ASSOC          size_t 
#define MM_MAX_TIPUS_NUMERADOR_CAMP_DBF         _UI32_MAX
#define MM_MAX_N_CAMPS_DBF_CLASSICA             255 
#define MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA      254 

#define MM_MARCA_VERSIO_1_DBF_ESTESA            0x90
#define MM_MARCA_DBASE4                         0x03
#define MM_MAX_LON_RESERVAT_1_BASE_DADES_XP     2
#define MM_MAX_LON_DBF_ON_A_LAN_BASE_DADES_XP   12
#define MM_MAX_LON_RESERVAT_2_BASE_DADES_XP     2

#define MM_MAX_LEN_LAYER_NAME           255
#define MM_MAX_LEN_LAYER_IDENTIFIER     255

#define MM_TYPICAL_NODE                 0
#define MM_LINE_NODE                    1
#define MM_RING_NODE                    2
#define MM_FINAL_NODE                   3

#define MM_MAX_ID_SNY                    41

#define MM_BYTE                         unsigned char
#define MM_TIPUS_BYTES_PER_CAMP_DBF     unsigned __int32
#define MM_TIPUS_BYTES_ACUMULATS_DBF    unsigned __int32

#define MM_MAX_LON_DESCRIPCIO_CAMP_DBF 256>(_MAX_PATH+100)?256:(_MAX_PATH+100)
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

#define MM_CAMP_NO_SIMBOLITZABLE   0
#define MM_CAMP_SIMBOLITZABLE		1


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
#define MM_CAMP_ES_AREA_3D	        19
#define MM_CAMP_ES_N_ARCS           15
#define MM_CAMP_ES_N_POLIG          16
#define MM_CAMP_ES_PENDENT          20
#define MM_CAMP_ES_ORIENTACIO       21

#define MM_JOC_CARAC_ANSI_MM    1252
#define MM_JOC_CARAC_ANSI_DBASE 0x58
#define MM_JOC_CARAC_UTF8_DBF   0xFF

enum MM_TipusNomCamp { MM_NOM_DBF_CLASSICA_I_VALID=0, MM_NOM_DBF_MINUSCULES_I_VALID, MM_NOM_DBF_ESTES_I_VALID, MM_NOM_DBF_NO_VALID};
#define MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES  11

#define MM_DonaBytesNomEstesCamp(camp)   ((MM_BYTE)((camp)->reservat_2[MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES]))

#define MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA 8
#define MM_SEGON_OFFSET_a_OFFSET_1a_FITXA  30

#define MM_NOM_CAMP_MASSA_LLARG         0x01
#define MM_NOM_CAMP_CARACTER_INVALID    0x02
#define MM_NOM_CAMP_PRIMER_CARACTER_    0x04

#define MM_MAX_AMPLADA_CAMP_N_DBF       21

#define MM_PRIVATE_POINT_DB_FIELDS      1
#define MM_PRIVATE_ARC_DB_FIELDS        5
#define MM_PRIVATE_POLYGON_DB_FIELDS    5

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_GDAL_CONSTANTS_H
