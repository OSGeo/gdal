#ifndef __MM_GDAL_FUNCTIONS_H
#define __MM_GDAL_FUNCTIONS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon         */
/* -------------------------------------------------------------------- */

#ifdef GDAL_COMPILATION
#include "mm_gdal_constants.h"   // MM_BYTE
#include "mm_gdal_structures.h"   // struct BASE_DADES_XP
CPL_C_START // Necessary for compiling in GDAL project
#else
#include "mm_gdal\mm_gdal_constants.h"   // MM_BYTE
#include "mm_gdal\mm_gdal_structures.h"   // struct BASE_DADES_XP
#endif

#define MM_EscriuOffsetNomEstesBD_XP(bd_xp,i_camp,offset_nom_camp) \
			memcpy((bd_xp)->Camp[(i_camp)].reservat_2+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES,&(offset_nom_camp),4)

char *MM_strnzcpy(char *dest, const char *src, size_t maxlen);
void MM_InitializeField(struct MM_CAMP *camp);
struct MM_BASE_DADES_XP * MM_CreateDBFHeader(MM_NUMERATOR_DBF_FIELD_TYPE n_camps);
void MM_ReleaseDBFHeader(struct MM_BASE_DADES_XP * base_dades_XP);
MM_BOOLEAN MM_CreateDBFFile(struct MM_BASE_DADES_XP * bd_xp, const char *NomFitxer);
int MM_DuplicateFieldDBXP(struct MM_CAMP *camp_final, const struct MM_CAMP *camp_inicial);

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals);
size_t MM_DefineFirstArcFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals);
size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
size_t MM_DefineFirstPointFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp);
int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(struct MM_CAMP *camp,
			struct MM_BASE_DADES_XP * bd_xp, MM_BOOLEAN no_modifica_descriptor, size_t mida_nom);

void MM_WriteValueToRecordDBXP(char *registre, 
                                   const struct MM_CAMP *camp, 
                                   const void *valor,
                                   MM_BOOLEAN is_64);
int MM_ChangeDBFWidthField(struct MM_BASE_DADES_XP * base_dades_XP,
							MM_NUMERATOR_DBF_FIELD_TYPE quincamp,
							MM_TIPUS_BYTES_PER_CAMP_DBF novaamplada);
#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_GDAL_FUNCTIONS_H
