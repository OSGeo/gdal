#ifndef __MM_GDAL_FUNCTIONS_H
#define __MM_GDAL_FUNCTIONS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon         */
/* -------------------------------------------------------------------- */

#ifdef GDAL_COMPILATION
#include "mm_gdal_constants.h"       // MM_BYTE
#include "mm_gdal_structures.h"      // struct BASE_DADES_XP
#include "mm_gdal_driver_structs.h"  // struct MMAdmDatabase
CPL_C_START                          // Necessary for compiling in GDAL project
#else
#include "mm_constants.h"                    // MM_BYTE
#include "mm_gdal\mm_gdal_structures.h"      // struct BASE_DADES_XP
#include "mm_gdal\mm_gdal_driver_structs.h"  // struct MMAdmDatabase
#endif

    char *
    MM_strnzcpy(char *dest, const char *src, size_t maxlen);
char *MM_oemansi(char *szcadena);
char *MM_oemansi_n(char *szcadena, size_t n_bytes);
void MM_InitializeField(struct MM_FIELD *camp);
struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS ncamps);
MM_FIRST_RECORD_OFFSET_TYPE
MM_GiveOffsetExtendedFieldName(const struct MM_FIELD *camp);
struct MM_BASE_DADES_XP *MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
                                            MM_BYTE nCharSet);
MM_BYTE MM_DBFFieldTypeToVariableProcessing(MM_BYTE tipus_camp_DBF);
void MM_ReleaseMainFields(struct MM_BASE_DADES_XP *base_dades_XP);
void MM_ReleaseDBFHeader(struct MM_BASE_DADES_XP *base_dades_XP);
MM_BOOLEAN MM_CreateDBFFile(struct MM_BASE_DADES_XP *bd_xp,
                            const char *NomFitxer);
int MM_DuplicateFieldDBXP(struct MM_FIELD *camp_final,
                          const struct MM_FIELD *camp_inicial);
int MM_WriteNRecordsMMBD_XPFile(struct MMAdmDatabase *MMAdmDB);

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp,
                                        MM_BYTE n_decimals);
size_t MM_DefineFirstArcFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp,
                                    MM_BYTE n_decimals);
size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
size_t MM_DefineFirstPointFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
    struct MM_FIELD *camp, struct MM_BASE_DADES_XP *bd_xp,
    MM_BOOLEAN no_modifica_descriptor, size_t mida_nom);

int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             char *registre, const struct MM_FIELD *camp,
                             const void *valor, MM_BOOLEAN is_64);
int MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                                  MM_EXT_DBF_N_FIELDS *nStringCurrentLenght);
int MM_ChangeDBFWidthField(struct MM_BASE_DADES_XP *base_dades_XP,
                           MM_EXT_DBF_N_FIELDS quincamp,
                           MM_BYTES_PER_FIELD_TYPE_DBF novaamplada,
                           MM_BYTE nou_decimals,
                           MM_BYTE que_fer_amb_reformatat_decimals);

int MM_GetArcHeights(double *coord_z, FILE_TYPE *pF, MM_N_VERTICES_TYPE n_vrt,
                     struct MM_ZD *pZDescription, unsigned long int flag);

// Strings
char *MM_RemoveInitial_and_FinalQuotationMarks(char *cadena);
char *MM_RemoveWhitespacesFromEndOfString(char *str);
char *MM_RemoveLeadingWhitespaceOfString(char *cadena);

// DBF
struct MM_ID_GRAFIC_MULTIPLE_RECORD *
MMCreateExtendedDBFIndex(FILE_TYPE *f, MM_EXT_DBF_N_RECORDS n,
                         MM_EXT_DBF_N_RECORDS n_dbf,
                         MM_FIRST_RECORD_OFFSET_TYPE offset_1era,
                         MM_ACUMULATED_BYTES_TYPE_DBF bytes_per_fitxa,
                         MM_ACUMULATED_BYTES_TYPE_DBF bytes_acumulats_id_grafic,
                         MM_BYTES_PER_FIELD_TYPE_DBF bytes_id_grafic,
                         MM_BOOLEAN *isListField, MM_EXT_DBF_N_RECORDS *nMaxN);

int MM_ReadExtendedDBFHeaderFromFile(const char *szFileName,
                                     struct MM_BASE_DADES_XP *pMMBDXP,
                                     const char *pszRelFile);

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_GDAL_FUNCTIONS_H
