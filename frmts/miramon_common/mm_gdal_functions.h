#ifndef __MM_GDAL_FUNCTIONS_H
#define __MM_GDAL_FUNCTIONS_H
/* -------------------------------------------------------------------- */
/*      Constants used in GDAL and in MiraMon         */
/* -------------------------------------------------------------------- */

#include "mm_gdal_constants.h"       // MM_BYTE
#include "mm_gdal_structures.h"      // struct BASE_DADES_XP
#include "mm_gdal_driver_structs.h"  // struct MMAdmDatabase
CPL_C_START                          // Necessary for compiling in GDAL project

#define nullptr NULL

    // Log. It should be temporal
    extern const char *MM_pszLogFilename;

CPL_DLL void fclose_and_nullify(VSILFILE **pFunc);

CPL_DLL char *MM_oemansi(char *szcadena);
CPL_DLL char *MM_oemansi_n(char *szcadena, size_t n_bytes);
CPL_DLL char *MM_stristr(const char *haystack, const char *needle);
CPL_DLL void MM_InitializeField(struct MM_FIELD *camp);
CPL_DLL struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS ncamps);
CPL_DLL int MM_ISExtendedNameBD_XP(const char *nom_camp);
CPL_DLL MM_BYTE MM_CalculateBytesExtendedFieldName(struct MM_FIELD *camp);
CPL_DLL short int MM_ReturnValidClassicDBFFieldName(char *szChain);
CPL_DLL MM_FIRST_RECORD_OFFSET_TYPE
MM_GiveOffsetExtendedFieldName(const struct MM_FIELD *camp);
CPL_DLL struct MM_DATA_BASE_XP *MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
                                                   MM_BYTE nCharSet);
CPL_DLL void MM_ReleaseMainFields(struct MM_DATA_BASE_XP *data_base_XP);
CPL_DLL void MM_ReleaseDBFHeader(struct MM_DATA_BASE_XP **data_base_XP);
CPL_DLL MM_BOOLEAN MM_CreateAndOpenDBFFile(struct MM_DATA_BASE_XP *bd_xp,
                                           const char *NomFitxer);
CPL_DLL int MM_DuplicateFieldDBXP(struct MM_FIELD *camp_final,
                                  const struct MM_FIELD *camp_inicial);
CPL_DLL int MM_WriteNRecordsMMBD_XPFile(struct MM_DATA_BASE_XP *pMMBDXP);

CPL_DLL int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
    struct MM_FIELD *camp, struct MM_DATA_BASE_XP *bd_xp,
    MM_BOOLEAN no_modifica_descriptor, size_t mida_nom);

CPL_DLL int MMWritePreformatedNumberValueToRecordDBXP(
    struct MiraMonVectLayerInfo *hMiraMonLayer, char *registre,
    const struct MM_FIELD *camp, const char *valor);
CPL_DLL int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                     char *registre,
                                     const struct MM_FIELD *camp,
                                     const void *valor, MM_BOOLEAN is_64);
CPL_DLL int
MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                              MM_EXT_DBF_N_FIELDS *nStringCurrentLength);
CPL_DLL int MM_ChangeDBFWidthField(struct MM_DATA_BASE_XP *data_base_XP,
                                   MM_EXT_DBF_N_FIELDS quincamp,
                                   MM_BYTES_PER_FIELD_TYPE_DBF novaamplada,
                                   MM_BYTE nou_decimals);

// Strings
CPL_DLL char *MM_RemoveInitial_and_FinalQuotationMarks(char *cadena);
CPL_DLL char *MM_RemoveWhitespacesFromEndOfString(char *str);
CPL_DLL char *MM_RemoveLeadingWhitespaceOfString(char *cadena);
CPL_DLL int MMIsEmptyString(const char *string);

// DBF
CPL_DLL struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(
    VSILFILE *f, MM_EXT_DBF_N_RECORDS n_dbf,
    MM_FIRST_RECORD_OFFSET_TYPE offset_1era,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_per_fitxa,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_acumulats_id_grafic,
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_id_grafic, MM_BOOLEAN *isListField,
    MM_EXT_DBF_N_RECORDS *nMaxN);

CPL_DLL int MM_ReadExtendedDBFHeaderFromFile(const char *szFileName,
                                             struct MM_DATA_BASE_XP *pMMBDXP,
                                             const char *pszRelFile);

// READING/CREATING MIRAMON METADATA
CPL_DLL char *MMReturnValueFromSectionINIFile(const char *filename,
                                              const char *section,
                                              const char *key);

CPL_DLL int MMReturnCodeFromMM_m_idofic(const char *pMMSRS_or_pSRS,
                                        char *result, MM_BYTE direction);

#define EPSG_FROM_MMSRS 0
#define MMSRS_FROM_EPSG 1
#define ReturnEPSGCodeSRSFromMMIDSRS(pMMSRS, szResult)                         \
    MMReturnCodeFromMM_m_idofic((pMMSRS), (szResult), EPSG_FROM_MMSRS)
#define ReturnMMIDSRSFromEPSGCodeSRS(pSRS, szResult)                           \
    MMReturnCodeFromMM_m_idofic((pSRS), (szResult), MMSRS_FROM_EPSG)

CPL_DLL int MMCheck_REL_FILE(const char *szREL_file);
CPL_DLL void
MMGenerateFileIdentifierFromMetadataFileName(char *pMMFN,
                                             char *aFileIdentifier);
CPL_DLL int MMCheckSize_t(GUInt64 nCount, GUInt64 nSize);

CPL_C_END  // Necessary for compiling in GDAL project
#endif     //__MM_GDAL_FUNCTIONS_H
