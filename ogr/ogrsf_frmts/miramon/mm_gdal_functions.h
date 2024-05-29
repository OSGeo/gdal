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

#define nullptr NULL

    // Log. It should be temporal
    extern const char *MM_pszLogFilename;

void fclose_and_nullify(FILE_TYPE **pFunc);

// MiraMon feature table descriptors
#define MM_MAX_IDENTIFIER_SIZE 50
#define MM_a_WITH_GRAVE 224
#define MM_a_WITH_ACUTE 225
#define MM_e_WITH_GRAVE 232
#define MM_e_WITH_ACUTE 233
#define MM_i_WITH_ACUTE 237
#define MM_o_WITH_GRAVE 242
#define MM_o_WITH_ACUTE 243
#define MM_u_WITH_ACUTE 250

#define MM_A_WITH_GRAVE 192
#define MM_A_WITH_ACUTE 193
#define MM_E_WITH_GRAVE 200
#define MM_E_WITH_ACUTE 201
#define MM_I_WITH_ACUTE 205
#define MM_O_WITH_GRAVE 210
#define MM_O_WITH_ACUTE 211
#define MM_U_WITH_ACUTE 218

// In case of diaeresis use "_WITH_DIAERESIS"
// In case of cedilla use "_WITH_CEDILLA"
// In case of tilde use "_WITH_TILDE"
// In case of middle dot use "_MIDDLE_DOT"

void MM_FillFieldDescriptorByLanguage(void);

extern char szInternalGraphicIdentifierEng[];
extern char szInternalGraphicIdentifierCat[];
extern char szInternalGraphicIdentifierSpa[];

extern char szNumberOfVerticesEng[];
extern char szNumberOfVerticesCat[];
extern char szNumberOfVerticesSpa[];

extern char szLengthOfAarcEng[];
extern char szLengthOfAarcCat[];
extern char szLengthOfAarcSpa[];

extern char szInitialNodeEng[];
extern char szInitialNodeCat[];
extern char szInitialNodeSpa[];

extern char szFinalNodeEng[];
extern char szFinalNodeCat[];
extern char szFinalNodeSpa[];

extern char szNumberOfArcsToNodeEng[];
extern char szNumberOfArcsToNodeCat[];
extern char szNumberOfArcsToNodeSpa[];

extern char szNodeTypeEng[];
extern char szNodeTypeCat[];
extern char szNodeTypeSpa[];

extern char szPerimeterOfThePolygonEng[];
extern char szPerimeterOfThePolygonCat[];
extern char szPerimeterOfThePolygonSpa[];

extern char szAreaOfThePolygonEng[];
extern char szAreaOfThePolygonCat[];
extern char szAreaOfThePolygonSpa[];

extern char szNumberOfArcsEng[];
extern char szNumberOfArcsCat[];
extern char szNumberOfArcsSpa[];

extern char szNumberOfElementaryPolygonsEng[];
extern char szNumberOfElementaryPolygonsCat[];
extern char szNumberOfElementaryPolygonsSpa[];

char *MM_oemansi(char *szcadena);
char *MM_oemansi_n(char *szcadena, size_t n_bytes);
char *MM_stristr(const char *haystack, const char *needle);
void MM_InitializeField(struct MM_FIELD *camp);
struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS ncamps);
MM_FIRST_RECORD_OFFSET_TYPE
MM_GiveOffsetExtendedFieldName(const struct MM_FIELD *camp);
struct MM_DATA_BASE_XP *MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
                                           MM_BYTE nCharSet);
void MM_ReleaseMainFields(struct MM_DATA_BASE_XP *data_base_XP);
void MM_ReleaseDBFHeader(struct MM_DATA_BASE_XP **data_base_XP);
MM_BOOLEAN MM_CreateAndOpenDBFFile(struct MM_DATA_BASE_XP *bd_xp,
                                   const char *NomFitxer);
int MM_DuplicateFieldDBXP(struct MM_FIELD *camp_final,
                          const struct MM_FIELD *camp_inicial);
int MM_WriteNRecordsMMBD_XPFile(struct MMAdmDatabase *MMAdmDB);

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                        MM_BYTE n_perimeter_decimals,
                                        MM_BYTE n_area_decimals_decimals);
size_t MM_DefineFirstArcFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                    MM_BYTE n_decimals);
size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp);
size_t MM_DefineFirstPointFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp);
int MM_SprintfDoubleSignifFigures(char *szChain, size_t size_szChain,
                                  int nSignifFigures, double nRealValue);
int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
    struct MM_FIELD *camp, struct MM_DATA_BASE_XP *bd_xp,
    MM_BOOLEAN no_modifica_descriptor, size_t mida_nom);

int MMWritePreformatedNumberValueToRecordDBXP(
    struct MiraMonVectLayerInfo *hMiraMonLayer, char *registre,
    const struct MM_FIELD *camp, const char *valor);
int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             char *registre, const struct MM_FIELD *camp,
                             const void *valor, MM_BOOLEAN is_64);
int MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                                  MM_EXT_DBF_N_FIELDS *nStringCurrentLength);
int MM_ChangeDBFWidthField(struct MM_DATA_BASE_XP *data_base_XP,
                           MM_EXT_DBF_N_FIELDS quincamp,
                           MM_BYTES_PER_FIELD_TYPE_DBF novaamplada,
                           MM_BYTE nou_decimals);

int MM_GetArcHeights(double *coord_z, FILE_TYPE *pF, MM_N_VERTICES_TYPE n_vrt,
                     struct MM_ZD *pZDescription, uint32_t flag);

// Strings
char *MM_RemoveInitial_and_FinalQuotationMarks(char *cadena);
char *MM_RemoveWhitespacesFromEndOfString(char *str);
char *MM_RemoveLeadingWhitespaceOfString(char *cadena);

// DBF
struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(
    FILE_TYPE *f, MM_EXT_DBF_N_RECORDS n_dbf,
    MM_FIRST_RECORD_OFFSET_TYPE offset_1era,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_per_fitxa,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_acumulats_id_grafic,
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_id_grafic, MM_BOOLEAN *isListField,
    MM_EXT_DBF_N_RECORDS *nMaxN);

int MM_ReadExtendedDBFHeaderFromFile(const char *szFileName,
                                     struct MM_DATA_BASE_XP *pMMBDXP,
                                     const char *pszRelFile);

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_GDAL_FUNCTIONS_H
