#ifndef __MM_GDAL_STRUCTURES_H
#define __MM_GDAL_STRUCTURES_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon                           */
/* -------------------------------------------------------------------- */
#ifdef GDAL_COMPILATION
#include "cpl_conv.h"  // For FILE_TYPE
#include "mm_gdal_constants.h"
#else
#include "F64_str.h"  // For FILE_64
#include "mm_gdal\mm_gdal_constants.h"
#endif

#include "mm_constants.h"

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling in GDAL project
#endif

#ifdef GDAL_COMPILATION
#define FILE_TYPE VSILFILE
#else
#define FILE_TYPE FILE_64
#endif

    /* Internal field of an extended DBF. It is a copy of a MiraMon internal
structure but translated to be understood by anyone who wants to
review the code of the driver.
*/

    struct MM_FIELD  // In MiraMon code: MM_CAMP
{
    // Name of the field
    char FieldName[MM_MAX_LON_FIELD_NAME_DBF];  // In MiraMon code: NomCamp

    // Name of the field in dBASEIII
    char ClassicalDBFFieldName
        [MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF];  // In MiraMon code:
                                                // NomCampDBFClassica

    // Type of the field C, N, D, L, M, F, G and B
    char FieldType;   // In MiraMon code: TipusDeCamp
    MM_BOOLEAN Is64;  // Is an signed 64 bit integer

    // Number of decimal places if it is a float
    MM_BYTE DecimalsIfFloat;  // In MiraMon code: DecimalsSiEsFloat

    // Number of bytes of a field
    MM_BYTES_PER_FIELD_TYPE_DBF
    BytesPerField;  // In MiraMon code: MM_TIPUS_BYTES_PER_CAMP_DBF BytesPerCamp

    // Accumulated bytes before a field starts
    MM_ACCUMULATED_BYTES_TYPE_DBF
    AccumulatedBytes;  // In MiraMon code:
                       // MM_TIPUS_BYTES_ACUMULATS_DBF BytesAcumulats

    // Not used in GDAL
    char
        *Separator[MM_NUM_IDIOMES_MD_MULTIDIOMA];  // In MiraMon code: separador

    // Description of the field (alternative name)
    char FieldDescription
        [MM_NUM_IDIOMES_MD_MULTIDIOMA]
        [MM_MAX_LON_DESCRIPCIO_CAMP_DBF];  // In MiraMon code: DescripcioCamp

    MM_BYTE DesiredWidth;          // In MiraMon code: AmpleDesitjat
    MM_BYTE OriginalDesiredWidth;  // In MiraMon code: AmpleDesitjatOriginal

    MM_BYTE reserved_1
        [MM_MAX_LON_RESERVAT_1_CAMP_BD_XP];  // In MiraMon code: reservat_1

    MM_BYTE reserved_2
        [MM_MAX_LON_RESERVAT_2_CAMP_BD_XP];  // In MiraMon code: reservat_2
    MM_BYTE MDX_field_flag;                  // In MiraMon code: MDX_camp_flag
    MM_BYTE GeoTopoTypeField;  // In MiraMon code: TipusCampGeoTopo
};

struct MM_DATA_BASE_XP  // MiraMon table Structure
{
    // Extended DBF file name
    char szFileName[MM_CPL_PATH_BUF_SIZE];  // In MiraMon code: szNomFitxer

    FILE_TYPE *pfDataBase;  // In MiraMon code: pfBaseDades

    // Charset of the DBF
    MM_BYTE CharSet;

    char ReadingMode[4];            // In MiraMon code: ModeLectura
    MM_EXT_DBF_N_RECORDS nRecords;  // In MiraMon code: n_fitxes
    MM_ACCUMULATED_BYTES_TYPE_DBF
    BytesPerRecord;               // In MiraMon code: BytesPerFitxa
    MM_EXT_DBF_N_FIELDS nFields;  // In MiraMon code: ncamps
    struct MM_FIELD *pField;      // In MiraMon code: Camp
    MM_FIRST_RECORD_OFFSET_TYPE
    FirstRecordOffset;                  // In MiraMon code: OffsetPrimeraFitxa
    MM_EXT_DBF_N_FIELDS IdGraficField;  // In MiraMon code: CampIdGrafic
    MM_EXT_DBF_N_FIELDS IdEntityField;  // In MiraMon code: CampIdEntitat
    short int year;                     // In MiraMon code: any
    MM_BYTE month;                      // In MiraMon code: mes
    MM_BYTE day;                        // In MiraMon code: dia

    MM_BYTE dbf_version;  // In MiraMon code: versio_dbf

    MM_BYTE reserved_1  // Used in extended DBF format to recompose BytesPerRecord
        [MM_MAX_LON_RESERVAT_1_BASE_DADES_XP];  // In MiraMon code: reservat_1
    MM_BYTE transaction_flag;
    MM_BYTE encryption_flag;
    MM_BYTE dbf_on_a_LAN[MM_MAX_LON_DBF_ON_A_LAN_BASE_DADES_XP];
    MM_BYTE MDX_flag;
    MM_BYTE reserved_2  // Used in extended DBF format to recompose BytesPerRecord
        [MM_MAX_LON_RESERVAT_2_BASE_DADES_XP];  // In MiraMon code: reservat_2
};
#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_GDAL_STRUCTURES_H
