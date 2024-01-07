#ifndef __MM_GDAL_FUNCTIONS_H
#define __MM_GDAL_FUNCTIONS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon         */
/* -------------------------------------------------------------------- */

#ifdef GDAL_COMPILATION
#include "mm_gdal_constants.h"   // MM_BYTE
#include "mm_gdal_structures.h"   // struct BASE_DADES_XP
#include "mm_gdal_driver_structs.h"   // struct MMAdmDatabase
CPL_C_START // Necessary for compiling in GDAL project
#else
#include "mm_gdal\mm_gdal_constants.h"   // MM_BYTE
#include "mm_gdal\mm_gdal_structures.h"   // struct BASE_DADES_XP
#include "mm_gdal\mm_gdal_driver_structs.h"   // struct MMAdmDatabase
#endif

#define MM_EscriuOffsetNomEstesBD_XP(bd_xp,i_camp,offset_nom_camp) \
			memcpy((bd_xp)->Camp[(i_camp)].reservat_2+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES,&(offset_nom_camp),4)

char *MM_strnzcpy(char *dest, const char *src, size_t maxlen);
char *MM_PassaAMajuscules(char *linia);
void MM_InitializeField(struct MM_CAMP *camp);
struct MM_CAMP *MM_CreateAllFields(int ncamps);
MM_FIRST_RECORD_OFFSET_TYPE MM_GiveOffsetExtendedFieldName(const struct MM_CAMP *camp);
struct MM_BASE_DADES_XP * MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps, MM_BYTE nCharSet);
MM_BYTE MM_DBFFieldTypeToVariableProcessing(MM_BYTE tipus_camp_DBF);
void MM_ReleaseDBFHeader(struct MM_BASE_DADES_XP * base_dades_XP);
MM_BOOLEAN MM_CreateDBFFile(struct MM_BASE_DADES_XP * bd_xp, const char *NomFitxer);
int MM_DuplicateFieldDBXP(struct MM_CAMP *camp_final, const struct MM_CAMP *camp_inicial);

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals);
size_t MM_DefineFirstArcFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals);
size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
size_t MM_DefineFirstPointFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(struct MM_CAMP *camp,
			struct MM_BASE_DADES_XP * bd_xp, MM_BOOLEAN no_modifica_descriptor, size_t mida_nom);

int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            char *registre, 
                            const struct MM_CAMP *camp, 
                            const void *valor,
                            MM_BOOLEAN is_64);
int MM_SecureCopyStringFieldValue(char **pszStringDst,
                                 const char *pszStringSrc,
                                 MM_EXT_DBF_N_FIELDS *nStringCurrentLenght);
int MM_ChangeDBFWidthField(struct MM_BASE_DADES_XP * base_dades_XP,
							MM_EXT_DBF_N_FIELDS quincamp,
							MM_TIPUS_BYTES_PER_CAMP_DBF novaamplada,
                            MM_BYTE nou_decimals,
							MM_BYTE que_fer_amb_reformatat_decimals);

int MM_GetArcHeights(double *coord_z, FILE_TYPE *pF, MM_N_VERTICES_TYPE n_vrt, struct MM_ZD *pZDescription, unsigned long int flag);

// Strings
char *MM_TreuBlancsDeFinalDeCadena(char * str);

// DBF
struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(FILE_TYPE *f, size_t n, MM_EXT_DBF_N_RECORDS n_dbf,
        MM_FIRST_RECORD_OFFSET_TYPE offset_1era, MM_TIPUS_BYTES_ACUMULATS_DBF bytes_per_fitxa,
        MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats_id_grafic,
        MM_TIPUS_BYTES_PER_CAMP_DBF bytes_id_grafic, MM_BOOLEAN *isListField, MM_EXT_DBF_N_RECORDS *nMaxN);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_GDAL_FUNCTIONS_H
