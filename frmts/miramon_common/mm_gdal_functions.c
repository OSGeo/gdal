/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C MiraMon code adapted to be used in GDAL
 * Author:   Abel Pau, a.pau@creaf.uab.cat, based on the MiraMon codes,
 *           mainly written by Xavier Pons, Joan Maso (correctly written
 *           "Mas0xF3"), Abel Pau, Nuria Julia (N0xFAria Juli0xE0),
 *           Xavier Calaf, Lluis (Llu0xEDs) Pesquer and Alaitz Zabala, from
 *           CREAF and Universitat Autonoma (Aut0xF2noma) de Barcelona.
 *           For a complete list of contributors:
 *           https://www.miramon.cat/eng/QuiSom.htm
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_api.h"            // For CPL_C_START
#include "mm_gdal_functions.h"  // For CPLStrlcpy()
#ifdef EMBED_RESOURCE_FILES
#include "embedded_resources.h"
#endif

CPL_C_START              // Necessary for compiling in GDAL project
#include "cpl_string.h"  // For CPL_ENC_UTF8

    const char *MM_pszLogFilename = nullptr;

static const char MM_EmptyString[] = {""};
#define MM_SetEndOfString (*MM_EmptyString)

CPL_DLL void fclose_and_nullify(VSILFILE **pFunc)
{
    if (!pFunc || !(*pFunc))
        return;
    VSIFCloseL(*pFunc);
    *pFunc = nullptr;
}

// CREATING AN EXTENDED MIRAMON DBF
CPL_DLL void MM_InitializeField(struct MM_FIELD *pField)
{
    memset(pField, '\0', sizeof(*pField));
    pField->FieldType = 'C';
    pField->GeoTopoTypeField = MM_NO_ES_CAMP_GEOTOPO;
}

#define MM_ACCEPTABLE_NUMBER_OF_FIELDS 20000

CPL_DLL struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS nFields)
{
    struct MM_FIELD *camp;
    MM_EXT_DBF_N_FIELDS i;

    // MiraMon could accept a number of fields 13.4 million
    // but GDAL prefers to limit that to 20.000 to avoid
    // too large memory allocation attempts with corrupted datasets
    if (nFields > MM_ACCEPTABLE_NUMBER_OF_FIELDS)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "More than 20000 fields not accepted");
        return nullptr;
    }

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (nFields >= UINT32_MAX / sizeof(*camp))
        return nullptr;
#else
    if (nFields >= (1000U * 1000 * 1000) / sizeof(*camp))
        return nullptr;
#endif

    if ((camp = VSICalloc(nFields, sizeof(*camp))) == nullptr)
        return nullptr;

    for (i = 0; i < nFields; i++)
        MM_InitializeField(camp + i);
    return camp;
}

static struct MM_DATA_BASE_XP *MM_CreateEmptyHeader(MM_EXT_DBF_N_FIELDS nFields)
{
    struct MM_DATA_BASE_XP *data_base_XP;

    if ((data_base_XP = (struct MM_DATA_BASE_XP *)VSICalloc(
             1, sizeof(struct MM_DATA_BASE_XP))) == nullptr)
        return nullptr;

    if (nFields == 0)
    {
        ;
    }
    else
    {
        data_base_XP->pField = (struct MM_FIELD *)MM_CreateAllFields(nFields);
        if (!data_base_XP->pField)
        {
            VSIFree(data_base_XP);
            return nullptr;
        }
    }
    data_base_XP->nFields = nFields;
    return data_base_XP;
}

CPL_DLL struct MM_DATA_BASE_XP *MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
                                                   MM_BYTE charset)
{
    struct MM_DATA_BASE_XP *bd_xp;
    struct MM_FIELD *camp;
    MM_EXT_DBF_N_FIELDS i;

    if (nullptr == (bd_xp = MM_CreateEmptyHeader(n_camps)))
        return nullptr;

    bd_xp->CharSet = charset;

    strcpy(bd_xp->ReadingMode, "a+b");

    bd_xp->IdGraficField = n_camps;
    bd_xp->IdEntityField = MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    bd_xp->dbf_version = (MM_BYTE)((n_camps > MM_MAX_N_CAMPS_DBF_CLASSICA)
                                       ? MM_MARCA_VERSIO_1_DBF_ESTESA
                                       : MM_MARCA_DBASE4);

    for (i = 0, camp = bd_xp->pField; i < n_camps; i++, camp++)
    {
        MM_InitializeField(camp);
        if (i < 99999)
            snprintf(camp->FieldName, sizeof(camp->FieldName), "CAMP%05u",
                     (unsigned)(i + 1));
        else
            snprintf(camp->FieldName, sizeof(camp->FieldName), "CM%u",
                     (unsigned)(i + 1));
        camp->FieldType = 'C';
        camp->DecimalsIfFloat = 0;
        camp->BytesPerField = 50;
    }
    return bd_xp;
}

static MM_BYTE MM_GetDefaultDesiredDBFFieldWidth(const struct MM_FIELD *camp)
{
    size_t a, b, c, d, e;

    b = strlen(camp->FieldName);
    c = strlen(camp->FieldDescription[0]);

    if (camp->FieldType == 'D')
    {
        d = (b > c ? b : c);
        a = (size_t)camp->BytesPerField + 2;
        return (MM_BYTE)(a > d ? a : d);
    }
    a = camp->BytesPerField;
    d = (unsigned int)(b > c ? b : c);
    e = (a > d ? a : d);
    return (MM_BYTE)(e < 80 ? e : 80);
}

static MM_BOOLEAN MM_is_field_name_lowercase(const char *szChain)
{
    const char *p;

    for (p = szChain; *p; p++)
    {
        if ((*p >= 'a' && *p <= 'z'))
            return TRUE;
    }
    return FALSE;
}

static MM_BOOLEAN
MM_Is_classical_DBF_field_name_or_lowercase(const char *szChain)
{
    const char *p;

    for (p = szChain; *p; p++)
    {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '_')
            ;
        else
            return FALSE;
    }
    if (szChain[0] == '_')
        return FALSE;
    return TRUE;
}

static MM_BOOLEAN
MM_Is_character_valid_for_extended_DBF_field_name(int valor,
                                                  int *valor_substitut)
{
    if (valor_substitut)
    {
        switch (valor)
        {
            case 32:
                *valor_substitut = '_';
                return FALSE;
            case 91:
                *valor_substitut = '(';
                return FALSE;
            case 93:
                *valor_substitut = ')';
                return FALSE;
            case 96:
                *valor_substitut = '\'';
                return FALSE;
            case 127:
                *valor_substitut = '_';
                return FALSE;
            case 168:
                *valor_substitut = '-';
                return FALSE;
        }
    }
    else
    {
        if (valor < 32 || valor == 91 || valor == 93 || valor == 96 ||
            valor == 127 || valor == 168)
            return FALSE;
    }
    return TRUE;
}

CPL_DLL int MM_ISExtendedNameBD_XP(const char *nom_camp)
{
    size_t mida, j;

    mida = strlen(nom_camp);
    if (mida >= MM_MAX_LON_FIELD_NAME_DBF)
        return MM_DBF_NAME_NO_VALID;

    for (j = 0; j < mida; j++)
    {
        if (!MM_Is_character_valid_for_extended_DBF_field_name(
                (unsigned char)nom_camp[j], nullptr))
            return MM_DBF_NAME_NO_VALID;
    }

    if (mida >= MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF)
        return MM_VALID_EXTENDED_DBF_NAME;

    if (!MM_Is_classical_DBF_field_name_or_lowercase(nom_camp))
        return MM_VALID_EXTENDED_DBF_NAME;

    if (MM_is_field_name_lowercase(nom_camp))
        return MM_DBF_NAME_LOWERCASE_AND_VALID;

    return NM_CLASSICAL_DBF_AND_VALID_NAME;
}

CPL_DLL MM_BYTE MM_CalculateBytesExtendedFieldName(struct MM_FIELD *camp)
{
    camp->reserved_2[MM_OFFSET_RESERVED2_EXTENDED_NAME_SIZE] =
        (MM_BYTE)strlen(camp->FieldName);
    return MM_DonaBytesNomEstesCamp(camp);
}

static MM_ACCUMULATED_BYTES_TYPE_DBF
MM_CalculateBytesExtendedFieldNames(const struct MM_DATA_BASE_XP *bd_xp)
{
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_acumulats = 0;
    MM_EXT_DBF_N_FIELDS i_camp;

    for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
    {
        if (MM_VALID_EXTENDED_DBF_NAME ==
            MM_ISExtendedNameBD_XP(bd_xp->pField[i_camp].FieldName))
            bytes_acumulats +=
                MM_CalculateBytesExtendedFieldName(bd_xp->pField + i_camp);
    }

    return bytes_acumulats;
}

static MM_FIRST_RECORD_OFFSET_TYPE
MM_CalculateBytesFirstRecordOffset(struct MM_DATA_BASE_XP *bd_xp)
{
    if (bd_xp)
        return (32 + 32 * bd_xp->nFields + 1 +
                MM_CalculateBytesExtendedFieldNames(bd_xp));
    return 0;
}

static void MM_CheckDBFHeader(struct MM_DATA_BASE_XP *bd_xp)
{
    struct MM_FIELD *camp;
    MM_EXT_DBF_N_FIELDS i;
    MM_BOOLEAN cal_DBF_estesa = FALSE;

    bd_xp->BytesPerRecord = 1;
    for (i = 0, camp = bd_xp->pField; i < bd_xp->nFields; i++, camp++)
    {
        camp->AccumulatedBytes = bd_xp->BytesPerRecord;
        bd_xp->BytesPerRecord += camp->BytesPerField;
        if (camp->DesiredWidth == 0)
            camp->DesiredWidth = camp->OriginalDesiredWidth =
                MM_GetDefaultDesiredDBFFieldWidth(camp);  //camp->BytesPerField;
        if (camp->FieldType == 'C' &&
            camp->BytesPerField > MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA)
            cal_DBF_estesa = TRUE;
        if (MM_VALID_EXTENDED_DBF_NAME ==
            MM_ISExtendedNameBD_XP(camp->FieldName))
            cal_DBF_estesa = TRUE;
    }

    bd_xp->FirstRecordOffset = MM_CalculateBytesFirstRecordOffset(bd_xp);

    if (cal_DBF_estesa || bd_xp->nFields > MM_MAX_N_CAMPS_DBF_CLASSICA ||
        bd_xp->nRecords > UINT32_MAX)
        bd_xp->dbf_version = (MM_BYTE)MM_MARCA_VERSIO_1_DBF_ESTESA;
    else
        bd_xp->dbf_version = MM_MARCA_DBASE4;
}

static void
MM_InitializeOffsetExtendedFieldNameFields(struct MM_DATA_BASE_XP *bd_xp,
                                           MM_EXT_DBF_N_FIELDS i_camp)
{
    memset((char *)(&bd_xp->pField[i_camp].reserved_2) +
               MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES,
           0, 4);
}

static void
MM_InitializeBytesExtendedFieldNameFields(struct MM_DATA_BASE_XP *bd_xp,
                                          MM_EXT_DBF_N_FIELDS i_camp)
{
    memset((char *)(&bd_xp->pField[i_camp].reserved_2) +
               MM_OFFSET_RESERVED2_EXTENDED_NAME_SIZE,
           0, 1);
}

static short int MM_return_common_valid_DBF_field_name_string(char *szChain)
{
    char *p;
    short int error_retornat = 0;

    if (!szChain)
        return 0;
    //strupr(szChain);
    for (p = szChain; *p; p++)
    {
        (*p) = (char)toupper((unsigned char)*p);
        if ((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
            ;
        else
        {
            *p = '_';
            error_retornat |= MM_FIELD_NAME_CHARACTER_INVALID;
        }
    }
    if (szChain[0] == '_')
    {
        // To avoid having field names starting by '_' this case is
        // substituted by a 0 (not a '\0').
        szChain[0] = '0';
        error_retornat |= MM_FIELD_NAME_FIRST_CHARACTER_;
    }
    return error_retornat;
}

CPL_DLL short int MM_ReturnValidClassicDBFFieldName(char *szChain)
{
    size_t long_nom_camp;
    short int error_retornat = 0;

    long_nom_camp = strlen(szChain);
    if ((long_nom_camp < 1) ||
        (long_nom_camp >= MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF))
    {
        szChain[MM_MAX_LON_FIELD_NAME_DBF - 1] = '\0';
        error_retornat |= MM_FIELD_NAME_TOO_LONG;
    }
    error_retornat |= MM_return_common_valid_DBF_field_name_string(szChain);
    return error_retornat;
}

static MM_BOOLEAN
MM_CheckClassicFieldNameEqual(const struct MM_DATA_BASE_XP *data_base_XP,
                              const char *classical_name)
{
    MM_EXT_DBF_N_FIELDS i;

    for (i = 0; i < data_base_XP->nFields; i++)
    {
        if ((strcasecmp(data_base_XP->pField[i].ClassicalDBFFieldName,
                        classical_name)) == 0 ||
            (strcasecmp(data_base_XP->pField[i].FieldName, classical_name)) ==
                0)
            return TRUE;
    }
    return FALSE;
}

static char *MM_GiveNewStringWithCharacterInFront(const char *text,
                                                  char character)
{
    char *ptr;
    size_t i;

    if (!text)
        return nullptr;

    i = strlen(text);
    if ((ptr = VSICalloc(1, i + 2)) == nullptr)
        return nullptr;

    *ptr = character;
    memcpy(ptr + 1, text, i + 1);
    return ptr;
}

static char *MM_SetSubIndexFieldNam(const char *nom_camp,
                                    MM_EXT_DBF_N_FIELDS index,
                                    size_t ampladamax)
{
    char *NomCamp_SubIndex;
    char *_subindex;
    char subindex[19 + 1];
    size_t sizet_subindex;
    size_t sizet_nomcamp;

    NomCamp_SubIndex = VSICalloc(1, ampladamax);
    if (!NomCamp_SubIndex)
        return nullptr;

    CPLStrlcpy(NomCamp_SubIndex, nom_camp, ampladamax);
    NomCamp_SubIndex[ampladamax - 1] = '\0';

    snprintf(subindex, sizeof(subindex), sprintf_UINT64, (GUInt64)index);

    _subindex = MM_GiveNewStringWithCharacterInFront(subindex, '_');
    if (!_subindex)
    {
        VSIFree(NomCamp_SubIndex);
        return nullptr;
    }

    sizet_subindex = strlen(_subindex);
    sizet_nomcamp = strlen(NomCamp_SubIndex);

    if (sizet_nomcamp + sizet_subindex > ampladamax - 1)
        memcpy(NomCamp_SubIndex + ((ampladamax - 1) - sizet_subindex),
               _subindex, strlen(_subindex));
    else
        NomCamp_SubIndex = strcat(NomCamp_SubIndex, _subindex);

    VSIFree(_subindex);

    return NomCamp_SubIndex;
}

CPL_DLL MM_FIRST_RECORD_OFFSET_TYPE
MM_GiveOffsetExtendedFieldName(const struct MM_FIELD *camp)
{
    MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;

    memcpy(&offset_nom_camp,
           (char *)(&camp->reserved_2) + MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES,
           4);
    return offset_nom_camp;
}

CPL_DLL int MM_WriteNRecordsMMBD_XPFile(struct MM_DATA_BASE_XP *pMMBDXP)
{
    if (!pMMBDXP || !pMMBDXP->pfDataBase)
        return 0;

    // Updating number of features in features table
    VSIFSeekL(pMMBDXP->pfDataBase, MM_FIRST_OFFSET_to_N_RECORDS, SEEK_SET);

    if (pMMBDXP->nRecords > UINT32_MAX)
    {
        pMMBDXP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
    }
    else
    {
        pMMBDXP->dbf_version = MM_MARCA_DBASE4;
    }

    {
        GUInt32 nRecords32LowBits = (GUInt32)(pMMBDXP->nRecords & UINT32_MAX);
        if (VSIFWriteL(&nRecords32LowBits, 4, 1, pMMBDXP->pfDataBase) != 1)
            return 1;
    }

    VSIFSeekL(pMMBDXP->pfDataBase, MM_SECOND_OFFSET_to_N_RECORDS, SEEK_SET);
    if (pMMBDXP->dbf_version == MM_MARCA_VERSIO_1_DBF_ESTESA)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        GUInt32 nRecords32HighBits = (GUInt32)(pMMBDXP->nRecords >> 32);
        if (VSIFWriteL(&nRecords32HighBits, 4, 1, pMMBDXP->pfDataBase) != 1)
            return 1;

        /* from 20 to 27 */
        if (VSIFWriteL(&(pMMBDXP->dbf_on_a_LAN), 8, 1, pMMBDXP->pfDataBase) !=
            1)
            return 1;
    }
    else
    {
        if (VSIFWriteL(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pMMBDXP->pfDataBase) !=
            1)
            return 1;
    }

    return 0;
}

static MM_BOOLEAN
MM_OpenIfNeededAndUpdateEntireHeader(struct MM_DATA_BASE_XP *data_base_XP)
{
    MM_BYTE variable_byte;
    MM_EXT_DBF_N_FIELDS i, j = 0;
    char zero[11] = {0};
    const MM_BYTE byte_zero = 0;
    char ModeLectura_previ[4] = "";
    MM_FIRST_RECORD_OFFSET_TYPE bytes_acumulats;
    MM_BYTE name_size;
    int estat;
    char nom_camp[MM_MAX_LON_FIELD_NAME_DBF];
    size_t retorn_fwrite;

    if (!data_base_XP)
        return FALSE;

    if (data_base_XP->pfDataBase == nullptr)
    {
        strcpy(ModeLectura_previ, data_base_XP->ReadingMode);
        strcpy(data_base_XP->ReadingMode, "wb+");

        if ((data_base_XP->pfDataBase = VSIFOpenL(data_base_XP->szFileName,
                                                  data_base_XP->ReadingMode)) ==
            nullptr)
        {
            return FALSE;
        }
    }
    else
    {
        // If it's open we just update the header
        VSIFSeekL(data_base_XP->pfDataBase, 0, SEEK_SET);
    }

    if ((data_base_XP->nFields) > MM_MAX_N_CAMPS_DBF_CLASSICA)
        data_base_XP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
    else if ((data_base_XP->nRecords) > UINT32_MAX)
        data_base_XP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
    else
    {
        if (data_base_XP->dbf_version == MM_MARCA_VERSIO_1_DBF_ESTESA)
            data_base_XP->dbf_version = MM_MARCA_DBASE4;
        for (i = 0; i < data_base_XP->nFields; i++)
        {
            if (data_base_XP->pField[i].FieldType == 'C' &&
                data_base_XP->pField[i].BytesPerField >
                    MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA)
            {
                data_base_XP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
                break;
            }
            if (MM_VALID_EXTENDED_DBF_NAME ==
                MM_ISExtendedNameBD_XP(data_base_XP->pField[i].FieldName))
            {
                data_base_XP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
                break;
            }
        }
    }

    // Writing header
    VSIFSeekL(data_base_XP->pfDataBase, 0, SEEK_SET);

    /* Byte 0 */
    if (VSIFWriteL(&(data_base_XP->dbf_version), 1, 1,
                   data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    /* MM_BYTE from 1 to 3 */
    variable_byte = (MM_BYTE)(data_base_XP->year - 1900);
    if (VSIFWriteL(&variable_byte, 1, 1, data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }
    if (VSIFWriteL(&(data_base_XP->month), 1, 1, data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }
    if (VSIFWriteL(&(data_base_XP->day), 1, 1, data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    /* from 4 a 7, position MM_FIRST_OFFSET_to_N_RECORDS */
    {
        GUInt32 nRecords32LowBits =
            (GUInt32)(data_base_XP->nRecords & UINT32_MAX);
        if (VSIFWriteL(&nRecords32LowBits, 4, 1, data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }

    /* from 8 a 9, position MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA */
    if (VSIFWriteL(&(data_base_XP->FirstRecordOffset), 2, 1,
                   data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }
    /* from 10 to 11, & from 12 to 13 */
    if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version))
    {
        if (VSIFWriteL(&(data_base_XP->BytesPerRecord),
                       sizeof(MM_ACCUMULATED_BYTES_TYPE_DBF), 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }
    else
    {
        /* from 10 to 11 */
        if (VSIFWriteL(&(data_base_XP->BytesPerRecord), 2, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
        /* from 12 to 13 */
        if (VSIFWriteL(&(data_base_XP->reserved_1), 2, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }
    /* byte 14 */
    if (VSIFWriteL(&(data_base_XP->transaction_flag), 1, 1,
                   data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }
    /* byte 15 */
    if (VSIFWriteL(&(data_base_XP->encryption_flag), 1, 1,
                   data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    /* from 16 to 27 */
    if (data_base_XP->nRecords > UINT32_MAX)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        GUInt32 nRecords32HighBits = (GUInt32)(data_base_XP->nRecords >> 32);
        if (VSIFWriteL(&nRecords32HighBits, 4, 1, data_base_XP->pfDataBase) !=
            1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }

        /* from 20 to 27 */
        if (VSIFWriteL(&(data_base_XP->dbf_on_a_LAN), 8, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }
    else
    {
        /* from 16 to 27 */
        if (VSIFWriteL(&(data_base_XP->dbf_on_a_LAN), 12, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }
    /* byte 28 */
    if (VSIFWriteL(&(data_base_XP->MDX_flag), 1, 1, data_base_XP->pfDataBase) !=
        1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    /* Byte 29 */
    if (VSIFWriteL(&(data_base_XP->CharSet), 1, 1, data_base_XP->pfDataBase) !=
        1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    /* Bytes from 30 to 31, in position MM_SEGON_OFFSET_a_OFFSET_1a_FITXA */
    if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version))
    {
        if (VSIFWriteL(((char *)&(data_base_XP->FirstRecordOffset)) + 2, 2, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }
    else
    {
        if (VSIFWriteL(&(data_base_XP->reserved_2), 2, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }

    /* At 32th byte fields description begins   */
    /* Every description is 32 bytes long       */
    bytes_acumulats = 32 + 32 * (data_base_XP->nFields) + 1;

    for (i = 0; i < data_base_XP->nFields; i++)
    {
        /* Bytes from 0 to 10    -> Field name, \0 finished */
        estat = MM_ISExtendedNameBD_XP(data_base_XP->pField[i].FieldName);
        if (estat == NM_CLASSICAL_DBF_AND_VALID_NAME ||
            estat == MM_DBF_NAME_LOWERCASE_AND_VALID)
        {
            j = (short)strlen(data_base_XP->pField[i].FieldName);

            retorn_fwrite = VSIFWriteL(&data_base_XP->pField[i].FieldName, 1, j,
                                       data_base_XP->pfDataBase);
            if (retorn_fwrite != (size_t)j)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
            MM_InitializeOffsetExtendedFieldNameFields(data_base_XP, i);
            MM_InitializeBytesExtendedFieldNameFields(data_base_XP, i);
        }
        else if (estat == MM_VALID_EXTENDED_DBF_NAME)
        {
            if (*(data_base_XP->pField[i].ClassicalDBFFieldName) == '\0')
            {
                char nom_temp[MM_MAX_LON_FIELD_NAME_DBF];

                CPLStrlcpy(nom_temp, data_base_XP->pField[i].FieldName,
                           MM_MAX_LON_FIELD_NAME_DBF);
                MM_ReturnValidClassicDBFFieldName(nom_temp);
                nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF - 1] = '\0';
                if ((MM_CheckClassicFieldNameEqual(data_base_XP, nom_temp)) ==
                    TRUE)
                {
                    char *c;

                    c = MM_SetSubIndexFieldNam(
                        nom_temp, i, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);

                    if (c)
                    {
                        j = 0;
                        while (MM_CheckClassicFieldNameEqual(data_base_XP, c) ==
                                   TRUE &&
                               j < data_base_XP->nFields)
                        {
                            VSIFree(c);
                            c = MM_SetSubIndexFieldNam(
                                nom_temp, ++j,
                                MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                        }
                        if (c)
                        {
                            CPLStrlcpy(
                                data_base_XP->pField[i].ClassicalDBFFieldName,
                                c,
                                sizeof(data_base_XP->pField[i]
                                           .ClassicalDBFFieldName));
                            VSIFree(c);
                        }
                    }
                }
                else
                    CPLStrlcpy(
                        data_base_XP->pField[i].ClassicalDBFFieldName, nom_temp,
                        sizeof(data_base_XP->pField[i].ClassicalDBFFieldName));
            }

            // This is a 11-byte fixed size field consisting of the filename
            // and it's been padding calculated some next lines.
            j = (short)strlen(data_base_XP->pField[i].ClassicalDBFFieldName);

            retorn_fwrite =
                VSIFWriteL(&data_base_XP->pField[i].ClassicalDBFFieldName, 1, j,
                           data_base_XP->pfDataBase);
            if (retorn_fwrite != (size_t)j)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }

            name_size =
                MM_CalculateBytesExtendedFieldName(data_base_XP->pField + i);
            MM_EscriuOffsetNomEstesBD_XP(data_base_XP, i, bytes_acumulats);
            bytes_acumulats += name_size;
        }
        else
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }

        if (VSIFWriteL(zero, 1, 11 - j, data_base_XP->pfDataBase) !=
            11 - (size_t)j)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
        /* Byte 11, Field type */
        if (VSIFWriteL(&data_base_XP->pField[i].FieldType, 1, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
        /* Bytes 12 to 15 --> Reserved */
        if (VSIFWriteL(&data_base_XP->pField[i].reserved_1, 4, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
        /* Byte 16, or OFFSET_BYTESxCAMP_CAMP_CLASSIC --> BytesPerField */
        if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version) &&
            data_base_XP->pField[i].FieldType == 'C')
        {
            if (VSIFWriteL((void *)&byte_zero, 1, 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        else
        {
            if (VSIFWriteL(&data_base_XP->pField[i].BytesPerField, 1, 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        /* 17th byte 17 --> In fields of type 'N' and 'F' indicates decimal places.*/
        if (data_base_XP->pField[i].FieldType == 'N' ||
            data_base_XP->pField[i].FieldType == 'F')
        {
            if (VSIFWriteL(&data_base_XP->pField[i].DecimalsIfFloat, 1, 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        else
        {
            if (VSIFWriteL(zero, 1, 1, data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version) &&
            data_base_XP->pField[i].FieldType == 'C')
        {
            /* Bytes from 18 to 20 --> Reserved */
            if (VSIFWriteL(&data_base_XP->pField[i].reserved_2, 20 - 18 + 1, 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
            /* Bytes from 21 to 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C
                                    in extended DBF */
            if (VSIFWriteL(&data_base_XP->pField[i].BytesPerField,
                           sizeof(MM_BYTES_PER_FIELD_TYPE_DBF), 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }

            /* Bytes from 25 to 30 --> Reserved */
            if (VSIFWriteL(&data_base_XP->pField[i].reserved_2[25 - 18],
                           30 - 25 + 1, 1, data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        else
        {
            /* Bytes de 21 a 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C */
            memset(data_base_XP->pField[i].reserved_2 +
                       MM_OFFSET_RESERVAT2_BYTESxCAMP_CAMP_ESPECIAL,
                   '\0', 4);
            /* Bytes from 18 to 30 --> Reserved */
            if (VSIFWriteL(&data_base_XP->pField[i].reserved_2, 13, 1,
                           data_base_XP->pfDataBase) != 1)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
        /* Byte 31 --> MDX flag.    */
        if (VSIFWriteL(&data_base_XP->pField[i].MDX_field_flag, 1, 1,
                       data_base_XP->pfDataBase) != 1)
        {
            VSIFCloseL(data_base_XP->pfDataBase);
            data_base_XP->pfDataBase = nullptr;
            return FALSE;
        }
    }

    variable_byte = 13;
    if (VSIFWriteL(&variable_byte, 1, 1, data_base_XP->pfDataBase) != 1)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    if (data_base_XP->FirstRecordOffset != bytes_acumulats)
    {
        VSIFCloseL(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
        return FALSE;
    }

    // Extended fields
    for (i = 0; i < data_base_XP->nFields; i++)
    {
        if (MM_VALID_EXTENDED_DBF_NAME ==
            MM_ISExtendedNameBD_XP(data_base_XP->pField[i].FieldName))
        {
            bytes_acumulats =
                MM_GiveOffsetExtendedFieldName(data_base_XP->pField + i);
            name_size = MM_DonaBytesNomEstesCamp(data_base_XP->pField + i);

            VSIFSeekL(data_base_XP->pfDataBase, bytes_acumulats, SEEK_SET);

            strcpy(nom_camp, data_base_XP->pField[i].FieldName);
            //CanviaJocCaracPerEscriureDBF(nom_camp, JocCaracDBFaMM(data_base_XP->CharSet, ParMM.JocCaracDBFPerDefecte));

            retorn_fwrite =
                VSIFWriteL(nom_camp, 1, name_size, data_base_XP->pfDataBase);

            if (retorn_fwrite != (size_t)name_size)
            {
                VSIFCloseL(data_base_XP->pfDataBase);
                data_base_XP->pfDataBase = nullptr;
                return FALSE;
            }
        }
    }

    return TRUE;
} /* End of MM_OpenIfNeededAndUpdateEntireHeader() */

CPL_DLL MM_BOOLEAN MM_CreateAndOpenDBFFile(struct MM_DATA_BASE_XP *bd_xp,
                                           const char *NomFitxer)
{
    time_t currentTime;

    if (!NomFitxer || MMIsEmptyString(NomFitxer) || !bd_xp)
        return FALSE;

    MM_CheckDBFHeader(bd_xp);

    // Setting the current date
    currentTime = time(nullptr);

    struct tm ltime;
    VSILocalTime(&currentTime, &ltime);

    bd_xp->year = (short int)(ltime.tm_year + 1900);
    bd_xp->month = (MM_BYTE)(ltime.tm_mon + 1);
    bd_xp->day = (MM_BYTE)ltime.tm_mday;

    CPLStrlcpy(bd_xp->szFileName, NomFitxer, sizeof(bd_xp->szFileName));
    return MM_OpenIfNeededAndUpdateEntireHeader(bd_xp);
}

CPL_DLL void MM_ReleaseMainFields(struct MM_DATA_BASE_XP *data_base_XP)
{
    MM_EXT_DBF_N_FIELDS i;
    size_t j;
    char **szChain;

    if (data_base_XP->pField)
    {
        for (i = 0; i < data_base_XP->nFields; i++)
        {
            for (j = 0; j < MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
            {
                szChain = data_base_XP->pField[i].Separator;
                if (szChain[j])
                {
                    VSIFree(szChain[j]);
                    szChain[j] = nullptr;
                }
            }
        }
        VSIFree(data_base_XP->pField);
        data_base_XP->pField = nullptr;
        data_base_XP->nFields = 0;
    }
    return;
}

// READING THE HEADER OF AN EXTENDED DBF
// Free with MM_ReleaseDBFHeader()
CPL_DLL int MM_ReadExtendedDBFHeaderFromFile(const char *szFileName,
                                             struct MM_DATA_BASE_XP *pMMBDXP,
                                             const char *pszRelFile)
{
    MM_BYTE variable_byte;
    VSILFILE *pf;
    unsigned short int two_bytes;
    MM_EXT_DBF_N_FIELDS nIField;
    uint16_t offset_primera_fitxa;
    MM_FIRST_RECORD_OFFSET_TYPE offset_fals = 0;
    MM_BOOLEAN incoherent_record_size = FALSE;
    MM_BYTE un_byte;
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_per_camp;
    MM_BYTE tretze_bytes[13];
    MM_FIRST_RECORD_OFFSET_TYPE offset_possible;
    MM_BYTE some_problems_when_reading = 0;
    MM_FILE_OFFSET offset_reintent = 0;  // For retrying
    char cpg_file[MM_CPL_PATH_BUF_SIZE];
    char *pszDesc;
    char section[MM_MAX_LON_FIELD_NAME_DBF + 25];  // TAULA_PRINCIPAL:field_name
    GUInt32 nRecords32LowBits;
    char *pszString;

    if (!szFileName || !pMMBDXP)
        return 1;

    CPLStrlcpy(pMMBDXP->szFileName, szFileName, sizeof(pMMBDXP->szFileName));
    strcpy(pMMBDXP->ReadingMode, "rb");

    if ((pMMBDXP->pfDataBase =
             VSIFOpenL(pMMBDXP->szFileName, pMMBDXP->ReadingMode)) == nullptr)
        return 1;

    pf = pMMBDXP->pfDataBase;

    VSIFSeekL(pf, 0, SEEK_SET);
    /* ====== Header reading (32 bytes) =================== */
    offset_primera_fitxa = 0;

    if (1 != VSIFReadL(&(pMMBDXP->dbf_version), 1, 1, pf) ||
        1 != VSIFReadL(&variable_byte, 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->month), 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->day), 1, 1, pf))
    {
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    if (1 != VSIFReadL(&nRecords32LowBits, 4, 1, pf))
    {
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    if (1 != VSIFReadL(&offset_primera_fitxa, 2, 1, pf))
    {
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    pMMBDXP->year = (short)(1900 + variable_byte);
reintenta_lectura_per_si_error_CreaCampBD_XP:

    if (some_problems_when_reading > 0)
    {
        if (!MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
        {
            offset_fals =
                offset_primera_fitxa & (MM_FIRST_RECORD_OFFSET_TYPE)(~31);
        }
    }
    else
        offset_reintent = VSIFTellL(pf);

    if (1 != VSIFReadL(&two_bytes, 2, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->reserved_1), 2, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->transaction_flag), 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->encryption_flag), 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pf))
    {
        VSIFree(pMMBDXP->pField);
        pMMBDXP->pField = nullptr;
        pMMBDXP->nFields = 0;
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    if (MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
    {
        GUInt32 nRecords32HighBits;

        // Getting 4 bytes of the 8 bytes variable
        memcpy(&nRecords32HighBits, &pMMBDXP->dbf_on_a_LAN, 4);

        // Getting other 4 bytes of the 8 bytes variable
        // The cast to GUInt64 of the high 32 bits is important to
        // make sure the left bit shift is done correctly
        pMMBDXP->nRecords =
            ((GUInt64)nRecords32HighBits << 32) | nRecords32LowBits;
    }
    else
        pMMBDXP->nRecords = nRecords32LowBits;

    if (1 != VSIFReadL(&(pMMBDXP->MDX_flag), 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->CharSet), 1, 1, pf) ||
        1 != VSIFReadL(&(pMMBDXP->reserved_2), 2, 1, pf))
    {
        VSIFree(pMMBDXP->pField);
        pMMBDXP->pField = nullptr;
        pMMBDXP->nFields = 0;
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    // Checking for a cpg file
    if (pMMBDXP->CharSet == 0)
    {
        VSILFILE *f_cpg;
        char charset_cpg[11];

        strcpy(cpg_file, pMMBDXP->szFileName);
        CPLStrlcpy(cpg_file, CPLResetExtension(cpg_file, "cpg"),
                   sizeof(cpg_file));
        f_cpg = VSIFOpenL(cpg_file, "r");
        if (f_cpg)
        {
            char *p;
            size_t read_bytes;
            VSIFSeekL(f_cpg, 0L, SEEK_SET);
            if (11 > (read_bytes = VSIFReadL(charset_cpg, 1, 10, f_cpg)))
            {
                charset_cpg[read_bytes] = '\0';
                p = MM_stristr(charset_cpg, "UTF-8");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_UTF8_DBF;
                p = MM_stristr(charset_cpg, "UTF8");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_UTF8_DBF;
                p = MM_stristr(charset_cpg, "ISO-8859-1");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_ANSI_DBASE;
            }
            VSIFCloseL(f_cpg);
        }
    }
    if (MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
    {
        unsigned short FirstRecordOffsetLow16Bits;
        unsigned short FirstRecordOffsetHigh16Bits;
        GUInt32 nTmp;

        memcpy(&FirstRecordOffsetLow16Bits, &offset_primera_fitxa, 2);
        memcpy(&FirstRecordOffsetHigh16Bits, &pMMBDXP->reserved_2, 2);

        nTmp = ((GUInt32)FirstRecordOffsetHigh16Bits << 16) |
               FirstRecordOffsetLow16Bits;
        if (nTmp > INT32_MAX)
        {
            VSIFree(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            fclose_and_nullify(&pMMBDXP->pfDataBase);
            return 1;
        }
        pMMBDXP->FirstRecordOffset = (MM_FIRST_RECORD_OFFSET_TYPE)nTmp;

        if (some_problems_when_reading > 0)
            offset_fals = pMMBDXP->FirstRecordOffset;

        memcpy(&FirstRecordOffsetLow16Bits, &two_bytes, 2);
        memcpy(&FirstRecordOffsetHigh16Bits, &pMMBDXP->reserved_1, 2);

        pMMBDXP->BytesPerRecord = ((GUInt32)FirstRecordOffsetHigh16Bits << 16) |
                                  FirstRecordOffsetLow16Bits;
    }
    else
    {
        pMMBDXP->FirstRecordOffset = offset_primera_fitxa;
        pMMBDXP->BytesPerRecord = two_bytes;
    }

    /* ====== Record structure ========================= */

    if (some_problems_when_reading > 0)
    {
        if (offset_fals < 1 + 32)
            pMMBDXP->nFields = 0;
        else
            pMMBDXP->nFields =
                (MM_EXT_DBF_N_FIELDS)(((offset_fals - 1) - 32) / 32);
    }
    else
    {
        // There's a chance that bytes_acumulats could overflow if it's GUInt32.
        // For that reason it's better to promote to GUInt64.
        GUInt64 bytes_acumulats = 1;

        pMMBDXP->nFields = 0;

        VSIFSeekL(pf, 0, SEEK_END);
        if (32 - 1 < VSIFTellL(pf))
        {
            VSIFSeekL(pf, 32, SEEK_SET);
            do
            {
                bytes_per_camp = 0;
                VSIFSeekL(pf,
                          32 + (MM_FILE_OFFSET)pMMBDXP->nFields * 32 +
                              (MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF + 1 + 4),
                          SEEK_SET);
                if (1 != VSIFReadL(&bytes_per_camp, 1, 1, pf) ||
                    1 != VSIFReadL(&un_byte, 1, 1, pf) ||
                    1 != VSIFReadL(&tretze_bytes, 3 + sizeof(bytes_per_camp), 1,
                                   pf))
                {
                    VSIFree(pMMBDXP->pField);
                    pMMBDXP->pField = nullptr;
                    pMMBDXP->nFields = 0;
                    fclose_and_nullify(&pMMBDXP->pfDataBase);
                    return 1;
                }
                if (bytes_per_camp == 0)
                    memcpy(&bytes_per_camp, (char *)(&tretze_bytes) + 3,
                           sizeof(bytes_per_camp));

                bytes_acumulats += bytes_per_camp;
                pMMBDXP->nFields++;
            } while (bytes_acumulats < pMMBDXP->BytesPerRecord);
        }
    }

    if (pMMBDXP->nFields != 0)
    {
        VSIFree(pMMBDXP->pField);
        pMMBDXP->pField = MM_CreateAllFields(pMMBDXP->nFields);
        if (!pMMBDXP->pField)
        {
            pMMBDXP->nFields = 0;
            fclose_and_nullify(&pMMBDXP->pfDataBase);
            return 1;
        }
    }
    else
    {
        VSIFree(pMMBDXP->pField);
        pMMBDXP->pField = nullptr;
    }

    VSIFSeekL(pf, 32, SEEK_SET);
    for (nIField = 0; nIField < pMMBDXP->nFields; nIField++)
    {
        if (1 != VSIFReadL(pMMBDXP->pField[nIField].FieldName,
                           MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF, 1, pf) ||
            1 != VSIFReadL(&(pMMBDXP->pField[nIField].FieldType), 1, 1, pf) ||
            1 != VSIFReadL(&(pMMBDXP->pField[nIField].reserved_1), 4, 1, pf) ||
            1 != VSIFReadL(&(pMMBDXP->pField[nIField].BytesPerField), 1, 1,
                           pf) ||
            1 != VSIFReadL(&(pMMBDXP->pField[nIField].DecimalsIfFloat), 1, 1,
                           pf) ||
            1 != VSIFReadL(&(pMMBDXP->pField[nIField].reserved_2), 13, 1, pf) ||
            1 !=
                VSIFReadL(&(pMMBDXP->pField[nIField].MDX_field_flag), 1, 1, pf))
        {
            VSIFree(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            VSIFCloseL(pf);
            pMMBDXP->pfDataBase = nullptr;
            return 1;
        }

        if (pMMBDXP->pField[nIField].FieldType == 'F')
            pMMBDXP->pField[nIField].FieldType = 'N';

        pMMBDXP->pField[nIField]
            .FieldName[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF - 1] = '\0';
        if (EQUAL(pMMBDXP->pField[nIField].FieldName,
                  szMMNomCampIdGraficDefecte))
            pMMBDXP->IdGraficField = nIField;

        // Limit BytesPerField to avoid later integer overflows
        // We could potentially limit further...
        if (pMMBDXP->pField[nIField].BytesPerField > (uint32_t)(INT32_MAX - 1))
        {
            VSIFree(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            VSIFCloseL(pf);
            pMMBDXP->pfDataBase = nullptr;
            return 1;
        }

        if (pMMBDXP->pField[nIField].BytesPerField == 0)
        {
            if (!MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
            {
                VSIFree(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                VSIFCloseL(pf);
                pMMBDXP->pfDataBase = nullptr;
                return 1;
            }
            if (pMMBDXP->pField[nIField].FieldType != 'C')
            {
                VSIFree(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                VSIFCloseL(pf);
                pMMBDXP->pfDataBase = nullptr;
                return 1;
            }

            memcpy(&pMMBDXP->pField[nIField].BytesPerField,
                   (char *)(&pMMBDXP->pField[nIField].reserved_2) + 3,
                   sizeof(MM_BYTES_PER_FIELD_TYPE_DBF));
        }

        if (nIField)
        {
            // To avoid overflow
            if (pMMBDXP->pField[nIField - 1].AccumulatedBytes >
                UINT32_MAX - pMMBDXP->pField[nIField - 1].BytesPerField)
            {
                VSIFree(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                VSIFCloseL(pf);
                pMMBDXP->pfDataBase = nullptr;
                return 1;
            }

            pMMBDXP->pField[nIField].AccumulatedBytes =
                (pMMBDXP->pField[nIField - 1].AccumulatedBytes +
                 pMMBDXP->pField[nIField - 1].BytesPerField);
        }
        else
        {
            pMMBDXP->pField[nIField].AccumulatedBytes = 1;
        }

        if (pszRelFile)
        {
            // Usually, in multilingual MiraMon metadata files, the main
            // language is the default one and has no "_cat", "_spa", or
            // "_eng" suffix after the keyword. So, to retrieve all
            // languages in a multilingual file, first, we'll identify
            // the one with no suffix "_cat", "_spa", or "_eng", and then the
            // others. If one of them lacks a value, it gets the default value.
            snprintf(section, sizeof(section), "TAULA_PRINCIPAL:%s",
                     pMMBDXP->pField[nIField].FieldName);

            // MM_DEF_LANGUAGE
            pszDesc = MMReturnValueFromSectionINIFile(pszRelFile, section,
                                                      "descriptor");
            if (pszDesc)
            {
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_DEF_LANGUAGE],
                    pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);

                VSIFree(pszDesc);
            }
            else
                *pMMBDXP->pField[nIField].FieldDescription[MM_DEF_LANGUAGE] =
                    '\0';

            // MM_ENG_LANGUAGE
            pszDesc = MMReturnValueFromSectionINIFile(pszRelFile, section,
                                                      "descriptor_eng");
            if (pszDesc)
            {
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_ENG_LANGUAGE],
                    pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);

                if (*pMMBDXP->pField[nIField]
                         .FieldDescription[MM_DEF_LANGUAGE] == '\0')
                {
                    CPLStrlcpy(pMMBDXP->pField[nIField]
                                   .FieldDescription[MM_DEF_LANGUAGE],
                               pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                }
                VSIFree(pszDesc);
            }
            else
            {
                // If there is no value descriptor_eng it's because it's the
                // default one. So, it's taken from there.
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_ENG_LANGUAGE],
                    pMMBDXP->pField[nIField].FieldDescription[MM_DEF_LANGUAGE],
                    MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
            }

            // MM_CAT_LANGUAGE
            pszDesc = MMReturnValueFromSectionINIFile(pszRelFile, section,
                                                      "descriptor_cat");
            if (pszDesc)
            {
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_CAT_LANGUAGE],
                    pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);

                if (*pMMBDXP->pField[nIField]
                         .FieldDescription[MM_DEF_LANGUAGE] == '\0')
                {
                    CPLStrlcpy(pMMBDXP->pField[nIField]
                                   .FieldDescription[MM_DEF_LANGUAGE],
                               pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                }

                VSIFree(pszDesc);
            }
            else
            {
                // If there is no value descriptor_cat it's because it's the
                // default one. So, it's taken from there.
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_CAT_LANGUAGE],
                    pMMBDXP->pField[nIField].FieldDescription[MM_DEF_LANGUAGE],
                    MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
            }

            // MM_SPA_LANGUAGE
            pszDesc = MMReturnValueFromSectionINIFile(pszRelFile, section,
                                                      "descriptor_spa");
            if (pszDesc)
            {
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_SPA_LANGUAGE],
                    pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);

                if (*pMMBDXP->pField[nIField]
                         .FieldDescription[MM_DEF_LANGUAGE] == '\0')
                {
                    CPLStrlcpy(pMMBDXP->pField[nIField]
                                   .FieldDescription[MM_DEF_LANGUAGE],
                               pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                }

                VSIFree(pszDesc);
            }
            else
            {
                // If there is no value descriptor_spa it's because it's the
                // default one. So, it's taken from there.
                CPLStrlcpy(
                    pMMBDXP->pField[nIField].FieldDescription[MM_SPA_LANGUAGE],
                    pMMBDXP->pField[nIField].FieldDescription[MM_DEF_LANGUAGE],
                    MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
            }
        }
    }

    if (!pMMBDXP->nFields)
    {
        if (pMMBDXP->BytesPerRecord)
            incoherent_record_size = TRUE;
    }
    else
    {
        // To avoid overflow
        if (pMMBDXP->pField[pMMBDXP->nFields - 1].AccumulatedBytes >
            UINT32_MAX - pMMBDXP->pField[pMMBDXP->nFields - 1].BytesPerField)
        {
            VSIFree(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            VSIFCloseL(pf);
            pMMBDXP->pfDataBase = nullptr;
            return 1;
        }
        if (pMMBDXP->pField[pMMBDXP->nFields - 1].BytesPerField +
                pMMBDXP->pField[pMMBDXP->nFields - 1].AccumulatedBytes >
            pMMBDXP->BytesPerRecord)
            incoherent_record_size = TRUE;
    }
    if (incoherent_record_size)
    {
        if (some_problems_when_reading == 0)
        {
            incoherent_record_size = FALSE;
            VSIFSeekL(pf, offset_reintent, SEEK_SET);
            some_problems_when_reading++;
            /* Reset IdGraficField as it might no longer be valid */
            pMMBDXP->IdGraficField = 0;
            goto reintenta_lectura_per_si_error_CreaCampBD_XP;
        }
        else
        {
            VSIFree(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            VSIFCloseL(pf);
            pMMBDXP->pfDataBase = nullptr;
            return 1;
        }
    }

    offset_possible = 32 + 32 * (pMMBDXP->nFields) + 1;

    if (!incoherent_record_size &&
        offset_possible != pMMBDXP->FirstRecordOffset)
    {  // Extended names
        MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;
        int mida_nom;

        for (nIField = 0; nIField < pMMBDXP->nFields; nIField++)
        {
            offset_nom_camp =
                MM_GiveOffsetExtendedFieldName(pMMBDXP->pField + nIField);
            mida_nom = MM_DonaBytesNomEstesCamp(pMMBDXP->pField + nIField);
            if (mida_nom > 0 && mida_nom < MM_MAX_LON_FIELD_NAME_DBF &&
                offset_nom_camp >= offset_possible &&
                offset_nom_camp < pMMBDXP->FirstRecordOffset)
            {
                CPLStrlcpy(pMMBDXP->pField[nIField].ClassicalDBFFieldName,
                           pMMBDXP->pField[nIField].FieldName,
                           MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                VSIFSeekL(pf, offset_nom_camp, SEEK_SET);
                if (1 != VSIFReadL(pMMBDXP->pField[nIField].FieldName, mida_nom,
                                   1, pf))
                {
                    VSIFree(pMMBDXP->pField);
                    pMMBDXP->pField = nullptr;
                    pMMBDXP->nFields = 0;
                    VSIFCloseL(pf);
                    pMMBDXP->pfDataBase = nullptr;
                    return 1;
                }
                pMMBDXP->pField[nIField].FieldName[mida_nom] = '\0';

                // All field names to UTF-8
                if (pMMBDXP->CharSet == MM_JOC_CARAC_ANSI_DBASE)
                {
                    pszString = CPLRecode(pMMBDXP->pField[nIField].FieldName,
                                          CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    CPLStrlcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                               MM_MAX_LON_FIELD_NAME_DBF);
                    CPLFree(pszString);
                }
                else if (pMMBDXP->CharSet == MM_JOC_CARAC_OEM850_DBASE)
                {
                    MM_oemansi(pMMBDXP->pField[nIField].FieldName);
                    pszString = CPLRecode(pMMBDXP->pField[nIField].FieldName,
                                          CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    CPLStrlcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                               MM_MAX_LON_FIELD_NAME_DBF - 1);
                    CPLFree(pszString);
                }
            }
        }
    }

    pMMBDXP->IdEntityField = MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    return 0;
}  // End of MM_ReadExtendedDBFHeaderFromFile()

CPL_DLL void MM_ReleaseDBFHeader(struct MM_DATA_BASE_XP **data_base_XP)
{
    if (!data_base_XP)
        return;
    if (!*data_base_XP)
        return;

    MM_ReleaseMainFields(*data_base_XP);
    VSIFree(*data_base_XP);
    *data_base_XP = nullptr;

    return;
}

CPL_DLL int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
    struct MM_FIELD *camp, struct MM_DATA_BASE_XP *bd_xp,
    MM_BOOLEAN no_modifica_descriptor, size_t mida_nom)
{
    MM_EXT_DBF_N_FIELDS i_camp;
    unsigned n_digits_i = 0, i;
    int retorn = 0;

    if (mida_nom == 0)
        mida_nom = MM_MAX_LON_FIELD_NAME_DBF;

    for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
    {
        if (bd_xp->pField + i_camp == camp)
            continue;
        if (!strcasecmp(bd_xp->pField[i_camp].FieldName, camp->FieldName))
            break;
    }
    if (i_camp < bd_xp->nFields)
    {
        retorn = 1;
        if (strlen(camp->FieldName) > mida_nom - 2)
            camp->FieldName[mida_nom - 2] = '\0';
        strcat(camp->FieldName, "0");
        for (i = 2; i < (size_t)10; i++)
        {
            snprintf(camp->FieldName + strlen(camp->FieldName) - 1,
                     sizeof(camp->FieldName) - strlen(camp->FieldName) + 1,
                     "%u", i);
            for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
            {
                if (bd_xp->pField + i_camp == camp)
                    continue;
                if (!strcasecmp(bd_xp->pField[i_camp].FieldName,
                                camp->FieldName))
                    break;
            }
            if (i_camp == bd_xp->nFields)
            {
                n_digits_i = 1;
                break;
            }
        }
        if (i == 10)
        {
            camp->FieldName[strlen(camp->FieldName) - 1] = '\0';
            if (strlen(camp->FieldName) > mida_nom - 3)
                camp->FieldName[mida_nom - 3] = '\0';
            strcat(camp->FieldName, "00");
            for (i = 10; i < (size_t)100; i++)
            {
                snprintf(camp->FieldName + strlen(camp->FieldName) - 2,
                         sizeof(camp->FieldName) - strlen(camp->FieldName) + 2,
                         "%u", i);
                for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
                {
                    if (bd_xp->pField + i_camp == camp)
                        continue;
                    if (!strcasecmp(bd_xp->pField[i_camp].FieldName,
                                    camp->FieldName))
                        break;
                }
                if (i_camp == bd_xp->nFields)
                {
                    n_digits_i = 2;
                    break;
                }
            }
            if (i == 100)
            {
                camp->FieldName[strlen(camp->FieldName) - 2] = '\0';
                if (strlen(camp->FieldName) > mida_nom - 4)
                    camp->FieldName[mida_nom - 4] = '\0';
                strcat(camp->FieldName, "000");
                for (i = 100; i < (size_t)256 + 2; i++)
                {
                    snprintf(camp->FieldName + strlen(camp->FieldName) - 3,
                             sizeof(camp->FieldName) - strlen(camp->FieldName) +
                                 3,
                             "%u", i);
                    for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
                    {
                        if (bd_xp->pField + i_camp == camp)
                            continue;
                        if (!strcasecmp(bd_xp->pField[i_camp].FieldName,
                                        camp->FieldName))
                            break;
                    }
                    if (i_camp == bd_xp->nFields)
                    {
                        n_digits_i = 3;
                        break;
                    }
                }
                if (i == 256)
                    return 2;
            }
        }
    }
    else
    {
        i = 1;
    }

    if ((*(camp->FieldDescription[0]) == '\0') || no_modifica_descriptor)
        return retorn;

    for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
    {
        if (bd_xp->pField + i_camp == camp)
            continue;
        if (!strcasecmp(bd_xp->pField[i_camp].FieldDescription[0],
                        camp->FieldDescription[0]))
            break;
    }
    if (i_camp == bd_xp->nFields)
        return retorn;

    if (retorn == 1)
    {
        if (strlen(camp->FieldDescription[0]) >
            MM_MAX_LON_DESCRIPCIO_CAMP_DBF - 4 - n_digits_i)
            camp->FieldDescription[0][mida_nom - 4 - n_digits_i] = '\0';

        snprintf(camp->FieldDescription[0] + strlen(camp->FieldDescription[0]),
                 sizeof(camp->FieldDescription[0]) -
                     strlen(camp->FieldDescription[0]),
                 " (%u)", i);
        for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
        {
            if (bd_xp->pField + i_camp == camp)
                continue;
            if (!strcasecmp(bd_xp->pField[i_camp].FieldDescription[0],
                            camp->FieldDescription[0]))
                break;
        }
        if (i_camp == bd_xp->nFields)
            return retorn;
    }

    retorn = 1;
    if (strlen(camp->FieldDescription[0]) >
        MM_MAX_LON_DESCRIPCIO_CAMP_DBF - 4 - n_digits_i)
        camp->FieldDescription[0][mida_nom - 4 - n_digits_i] = '\0';
    camp->FieldDescription[0][strlen(camp->FieldDescription[0]) - 4 -
                              n_digits_i + 1] = '\0';
    if (strlen(camp->FieldDescription[0]) > MM_MAX_LON_DESCRIPCIO_CAMP_DBF - 7)
        camp->FieldDescription[0][mida_nom - 7] = '\0';
    for (i++; i < (size_t)256; i++)
    {
        //if (camp->FieldDescription[0] + strlen(camp->FieldDescription[0]))
        snprintf(camp->FieldDescription[0] + strlen(camp->FieldDescription[0]),
                 sizeof(camp->FieldDescription[0]) -
                     strlen(camp->FieldDescription[0]),
                 " (%u)", i);
        for (i_camp = 0; i_camp < bd_xp->nFields; i_camp++)
        {
            if (bd_xp->pField + i_camp == camp)
                continue;
            if (!strcasecmp(bd_xp->pField[i_camp].FieldName, camp->FieldName))
                break;
        }
        if (i_camp == bd_xp->nFields)
            return retorn;
    }
    return 2;
}  // End of MM_ModifyFieldNameAndDescriptorIfPresentBD_XP()

static int MM_DuplicateMultilingualString(
    char *(szChain_final[MM_NUM_IDIOMES_MD_MULTIDIOMA]),
    const char *const(szChain_inicial[MM_NUM_IDIOMES_MD_MULTIDIOMA]))
{
    size_t i;

    for (i = 0; i < MM_NUM_IDIOMES_MD_MULTIDIOMA; i++)
    {
        if (szChain_inicial[i])
        {
            if (nullptr == (szChain_final[i] = strdup(szChain_inicial[i])))
                return 1;
        }
        else
            szChain_final[i] = nullptr;
    }
    return 0;
}

CPL_DLL int MM_DuplicateFieldDBXP(struct MM_FIELD *camp_final,
                                  const struct MM_FIELD *camp_inicial)
{
    *camp_final = *camp_inicial;

    if (0 != MM_DuplicateMultilingualString(
                 camp_final->Separator,
                 (const char *const(*))camp_inicial->Separator))
        return 1;

    return 0;
}

// If n_bytes==SIZE_MAX, the parameter is ignored ant, then,
// it's assumed that szszChain is NUL terminated
CPL_DLL char *MM_oemansi_n(char *szszChain, size_t n_bytes)
{
    size_t u_i;
    unsigned char *punter_bait;
    unsigned char t_oemansi[128] = {
        199, 252, 233, 226, 228, 224, 229, 231, 234, 235, 232, 239, 238,
        236, 196, 197, 201, 230, 198, 244, 246, 242, 251, 249, 255, 214,
        220, 248, 163, 216, 215, 131, 225, 237, 243, 250, 241, 209, 170,
        186, 191, 174, 172, 189, 188, 161, 171, 187, 164, 164, 164, 166,
        166, 193, 194, 192, 169, 166, 166, 164, 164, 162, 165, 164, 164,
        164, 164, 164, 164, 164, 227, 195, 164, 164, 164, 164, 166, 164,
        164, 164, 240, 208, 202, 203, 200, 180, 205, 206, 207, 164, 164,
        164, 164, 166, 204, 164, 211, 223, 212, 210, 245, 213, 181, 254,
        222, 218, 219, 217, 253, 221, 175, 180, 173, 177, 164, 190, 182,
        167, 247, 184, 176, 168, 183, 185, 179, 178, 164, 183};
    if (n_bytes == SIZE_MAX)
    {
        for (punter_bait = (unsigned char *)szszChain; *punter_bait;
             punter_bait++)
        {
            if (*punter_bait > 127)
                *punter_bait = t_oemansi[*punter_bait - 128];
        }
    }
    else
    {
        for (u_i = 0, punter_bait = (unsigned char *)szszChain; u_i < n_bytes;
             punter_bait++, u_i++)
        {
            if (*punter_bait > 127)
                *punter_bait = t_oemansi[*punter_bait - 128];
        }
    }
    return szszChain;
}

// An implementation of non-sensitive strstr()
CPL_DLL char *MM_stristr(const char *haystack, const char *needle)
{
    if (!haystack)
        return nullptr;

    if (!needle)
        return nullptr;

    if (!*needle)
        return (char *)haystack;

    char *p1 = (char *)haystack;
    while (*p1 != '\0' && !EQUALN(p1, needle, strlen(needle)))
        p1++;

    if (*p1 == '\0')
        return nullptr;

    return p1;
}

CPL_DLL char *MM_oemansi(char *szszChain)
{
    return MM_oemansi_n(szszChain, SIZE_MAX);
}

CPL_DLL int
MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                              MM_EXT_DBF_N_FIELDS *nStringCurrentLength)
{

    if (!pszStringSrc)
    {
        if (1 >= *nStringCurrentLength)
        {
            void *new_ptr = VSIRealloc(*pszStringDst, 2);
            if (!new_ptr)
                return 1;
            *pszStringDst = new_ptr;
            *nStringCurrentLength = (MM_EXT_DBF_N_FIELDS)2;
        }
        strcpy(*pszStringDst, "\0");
        return 0;
    }

    if (strlen(pszStringSrc) >= *nStringCurrentLength)
    {
        void *new_ptr = VSIRealloc(*pszStringDst, strlen(pszStringSrc) + 1);
        if (!new_ptr)
            return 1;
        (*pszStringDst) = new_ptr;
        *nStringCurrentLength = (MM_EXT_DBF_N_FIELDS)(strlen(pszStringSrc) + 1);
    }
    strcpy(*pszStringDst, pszStringSrc);
    return 0;
}

// This function assumes that all the file is saved in disk and closed.
CPL_DLL int MM_ChangeDBFWidthField(struct MM_DATA_BASE_XP *data_base_XP,
                                   MM_EXT_DBF_N_FIELDS nIField,
                                   MM_BYTES_PER_FIELD_TYPE_DBF nNewWidth,
                                   MM_BYTE nNewPrecision)
{
    char *record, *whites = nullptr;
    MM_BYTES_PER_FIELD_TYPE_DBF l_glop1, l_glop2, i_glop2;
    MM_EXT_DBF_N_RECORDS nfitx, i_reg;
    int canvi_amplada;  // change width
    GInt32 j;
    MM_EXT_DBF_N_FIELDS i_camp;
    size_t retorn_fwrite;
    int retorn_TruncaFitxer;

    if (!data_base_XP)
        return 1;

    canvi_amplada = nNewWidth - data_base_XP->pField[nIField].BytesPerField;

    if (data_base_XP->nRecords != 0)
    {
        l_glop1 = data_base_XP->pField[nIField].AccumulatedBytes;
        i_glop2 = l_glop1 + data_base_XP->pField[nIField].BytesPerField;
        if (nIField == data_base_XP->nFields - 1)
            l_glop2 = 0;
        else
            l_glop2 = data_base_XP->BytesPerRecord -
                      data_base_XP->pField[nIField + 1].AccumulatedBytes;

        if ((record = VSICalloc(1, (size_t)data_base_XP->BytesPerRecord)) ==
            nullptr)
            return 1;

        record[data_base_XP->BytesPerRecord - 1] = MM_SetEndOfString;

        if ((whites = (char *)VSICalloc(1, (size_t)nNewWidth)) == nullptr)
        {
            VSIFree(record);
            return 1;
        }
        memset(whites, ' ', nNewWidth);

        nfitx = data_base_XP->nRecords;
        i_reg = (canvi_amplada < 0 ? 0 : nfitx - 1);
        while (TRUE)
        {
            if (0 != VSIFSeekL(data_base_XP->pfDataBase,
                               data_base_XP->FirstRecordOffset +
                                   (MM_FILE_OFFSET)i_reg *
                                       data_base_XP->BytesPerRecord,
                               SEEK_SET))
            {
                VSIFree(whites);
                VSIFree(record);
                return 1;
            }

            if (1 != VSIFReadL(record, data_base_XP->BytesPerRecord, 1,
                               data_base_XP->pfDataBase))
            {
                VSIFree(whites);
                VSIFree(record);
                return 1;
            }

            if (0 !=
                VSIFSeekL(
                    data_base_XP->pfDataBase,
                    (MM_FILE_OFFSET)data_base_XP->FirstRecordOffset +
                        i_reg * ((MM_FILE_OFFSET)data_base_XP->BytesPerRecord +
                                 canvi_amplada),
                    SEEK_SET))
            {
                VSIFree(whites);
                VSIFree(record);
                return 1;
            }

            if (1 != VSIFWriteL(record, l_glop1, 1, data_base_XP->pfDataBase))
            {
                VSIFree(whites);
                VSIFree(record);
                return 1;
            }

            switch (data_base_XP->pField[nIField].FieldType)
            {
                case 'C':
                case 'L':
                    memcpy(whites, record + l_glop1,
                           (canvi_amplada < 0
                                ? nNewWidth
                                : data_base_XP->pField[nIField].BytesPerField));
                    retorn_fwrite = VSIFWriteL(whites, nNewWidth, 1,
                                               data_base_XP->pfDataBase);

                    if (1 != retorn_fwrite)
                    {
                        VSIFree(whites);
                        VSIFree(record);
                        return 1;
                    }
                    break;
                case 'N':

                    if (canvi_amplada >= 0)
                    {
                        if (1 != VSIFWriteL(whites, canvi_amplada, 1,
                                            data_base_XP->pfDataBase) ||
                            1 !=
                                VSIFWriteL(
                                    record + l_glop1,
                                    data_base_XP->pField[nIField].BytesPerField,
                                    1, data_base_XP->pfDataBase))
                        {
                            VSIFree(whites);
                            VSIFree(record);
                            return 1;
                        }
                    }
                    else if (canvi_amplada < 0)
                    {
                        j = (GInt32)(l_glop1 + (data_base_XP->pField[nIField]
                                                    .BytesPerField -
                                                1));
                        while (TRUE)
                        {
                            j--;

                            if (j < (GInt32)l_glop1 || record[j] == ' ')
                            {
                                j++;
                                break;
                            }
                        }

                        if ((data_base_XP->pField[nIField].BytesPerField +
                             l_glop1 - j) < nNewWidth)
                            j -= (GInt32)(nNewWidth -
                                          (data_base_XP->pField[nIField]
                                               .BytesPerField +
                                           l_glop1 - j));

                        retorn_fwrite = VSIFWriteL(record + j, nNewWidth, 1,
                                                   data_base_XP->pfDataBase);
                        if (1 != retorn_fwrite)
                        {
                            VSIFree(whites);
                            VSIFree(record);
                            return 1;
                        }
                    }

                    break;
                default:
                    VSIFree(whites);
                    VSIFree(record);
                    return 1;
            }
            if (l_glop2)
            {
                retorn_fwrite = VSIFWriteL(record + i_glop2, l_glop2, 1,
                                           data_base_XP->pfDataBase);
                if (1 != retorn_fwrite)
                {
                    VSIFree(whites);
                    VSIFree(record);
                    return 1;
                }
            }

            if (canvi_amplada < 0)
            {
                if (i_reg + 1 == nfitx)
                    break;
                i_reg++;
            }
            else
            {
                if (i_reg == 0)
                    break;
                i_reg--;
            }
        }

        VSIFree(whites);
        VSIFree(record);

        retorn_TruncaFitxer = VSIFTruncateL(
            data_base_XP->pfDataBase,
            (MM_FILE_OFFSET)data_base_XP->FirstRecordOffset +
                (MM_FILE_OFFSET)data_base_XP->nRecords *
                    ((MM_FILE_OFFSET)data_base_XP->BytesPerRecord +
                     canvi_amplada));
        if (canvi_amplada < 0 && retorn_TruncaFitxer)
            return 1;
    } /* Fi de registres de != 0*/

    if (canvi_amplada != 0)
    {
        data_base_XP->pField[nIField].BytesPerField = nNewWidth;
        data_base_XP->BytesPerRecord += canvi_amplada;
        for (i_camp = (MM_EXT_DBF_N_FIELDS)(nIField + 1);
             i_camp < data_base_XP->nFields; i_camp++)
            data_base_XP->pField[i_camp].AccumulatedBytes += canvi_amplada;
    }
    data_base_XP->pField[nIField].DecimalsIfFloat = nNewPrecision;

    if ((MM_OpenIfNeededAndUpdateEntireHeader(data_base_XP)) == FALSE)
        return 1;

    return 0;
} /* End of MMChangeCFieldWidthDBF() */

static char *MM_l_RemoveWhitespacesFromEndOfString(char *punter,
                                                   size_t l_szChain)
{
    size_t longitud_szChain = l_szChain;
    while (longitud_szChain > 0)
    {
        longitud_szChain--;
        if (punter[longitud_szChain] != ' ' && punter[longitud_szChain] != '\t')
        {
            break;
        }
        punter[longitud_szChain] = '\0';
    }
    return punter;
}

CPL_DLL char *MM_RemoveInitial_and_FinalQuotationMarks(char *szChain)
{
    char *ptr1, *ptr2;
    char cometa = '"';

    if (*szChain == cometa)
    {
        ptr1 = szChain;
        ptr2 = ptr1 + 1;
        if (*ptr2)
        {
            while (*ptr2)
            {
                *ptr1 = *ptr2;
                ptr1++;
                ptr2++;
            }
            if (*ptr1 == cometa)
                *(ptr1 - 1) = 0;
            else
                *ptr1 = 0;
        }
    }
    return szChain;
} /* End of MM_RemoveInitial_and_FinalQuotationMarks() */

CPL_DLL char *MM_RemoveLeadingWhitespaceOfString(char *szChain)
{
    char *ptr;
    char *ptr2;

    if (szChain == nullptr)
        return szChain;

    for (ptr = szChain; *ptr && (*ptr == ' ' || *ptr == '\t'); ptr++)
        continue;

    if (ptr != szChain)
    {
        ptr2 = szChain;
        while (*ptr)
        {
            *ptr2 = *ptr;
            ptr2++;
            ptr++;
        }
        *ptr2 = 0;
    }
    return szChain;
}

// Checks if a string is empty
CPL_DLL int MMIsEmptyString(const char *string)
{
    const char *ptr = string;

    for (; *ptr; ptr++)
        if (*ptr != ' ' && *ptr != '\t')
            return 0;

    return 1;
}

CPL_DLL char *MM_RemoveWhitespacesFromEndOfString(char *str)
{
    if (str == nullptr)
        return str;
    return MM_l_RemoveWhitespacesFromEndOfString(str, strlen(str));
}

CPL_DLL struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(
    VSILFILE *f, MM_EXT_DBF_N_RECORDS nNumberOfRecords,
    MM_FIRST_RECORD_OFFSET_TYPE offset_1era,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_per_fitxa,
    MM_ACCUMULATED_BYTES_TYPE_DBF bytes_acumulats_id_grafic,
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_id_grafic, MM_BOOLEAN *isListField,
    MM_EXT_DBF_N_RECORDS *nMaxN)
{
    struct MM_ID_GRAFIC_MULTIPLE_RECORD *id;
    MM_EXT_DBF_N_RECORDS i_dbf;
    MM_EXT_DBF_SIGNED_N_RECORDS i, id_grafic;
    char *fitxa;
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_final_id_principi_id1 =
        bytes_per_fitxa - bytes_id_grafic;

    *isListField = FALSE;
    *nMaxN = 0;
    if (!nNumberOfRecords)
        return nullptr;  // No elements to read

    if (MMCheckSize_t(nNumberOfRecords, sizeof(*id)))
        return nullptr;
    if (nullptr == (id = (struct MM_ID_GRAFIC_MULTIPLE_RECORD *)VSICalloc(
                        (size_t)nNumberOfRecords, sizeof(*id))))
        return nullptr;

    if (bytes_id_grafic == UINT32_MAX)
    {
        VSIFree(id);
        CPLError(CE_Failure, CPLE_OutOfMemory, "Overflow in bytes_id_graphic");
        return nullptr;
    }

    if (nullptr == (fitxa = (char *)VSICalloc(1, (size_t)bytes_id_grafic + 1)))
    {
        VSIFree(id);
        return nullptr;
    }
    fitxa[bytes_id_grafic] = '\0';

    VSIFSeekL(f,
              (MM_FILE_OFFSET)offset_1era +
                  (MM_FILE_OFFSET)bytes_acumulats_id_grafic,
              SEEK_SET);

    i_dbf = 0;
    do
    {
        if (i_dbf == nNumberOfRecords ||
            VSIFReadL(fitxa, 1, bytes_id_grafic, f) != (size_t)bytes_id_grafic)
        {
            VSIFree(id);
            VSIFree(fitxa);
            return nullptr;
        }
        i_dbf++;
    } while (1 !=
                 sscanf(fitxa, scanf_MM_EXT_DBF_SIGNED_N_RECORDS, &id_grafic) ||
             id_grafic < 0);
    i = 0;

    while (TRUE)
    {
        if (i > id_grafic)
        {
            VSIFree(id);
            VSIFree(fitxa);
            return nullptr;
        }
        i = id_grafic;
        if (i >= (MM_EXT_DBF_SIGNED_N_RECORDS)nNumberOfRecords)
        {
            VSIFree(fitxa);
            return id;
        }
        id[(size_t)i].offset = (MM_FILE_OFFSET)offset_1era +
                               (MM_FILE_OFFSET)(i_dbf - 1) * bytes_per_fitxa;
        do
        {
            id[(size_t)i].nMR++;
            if (!(*isListField) && id[(size_t)i].nMR > 1)
                *isListField = TRUE;
            if (*nMaxN < id[(size_t)i].nMR)
                *nMaxN = id[(size_t)i].nMR;

            if (i_dbf == nNumberOfRecords)
            {
                VSIFree(fitxa);
                return id;
            }
            VSIFSeekL(f, bytes_final_id_principi_id1, SEEK_CUR);
            if (VSIFReadL(fitxa, 1, bytes_id_grafic, f) !=
                (size_t)bytes_id_grafic)
            {
                VSIFree(id);
                VSIFree(fitxa);
                return nullptr;
            }
            if (1 != sscanf(fitxa, scanf_MM_EXT_DBF_SIGNED_N_RECORDS,
                            &id_grafic) ||
                id_grafic >= (MM_EXT_DBF_SIGNED_N_RECORDS)nNumberOfRecords)
            {
                VSIFree(fitxa);
                return id;
            }
            i_dbf++;
        } while (id_grafic == i);
    }
}  // End of MMCreateExtendedDBFIndex()

// READING/CREATING MIRAMON METADATA
// Returns the value of an INI file. Used to read MiraMon metadata
CPL_DLL char *MMReturnValueFromSectionINIFile(const char *filename,
                                              const char *section,
                                              const char *key)
{
    char *value = nullptr;
    const char *pszLine;
    char *section_header = nullptr;
    size_t key_len = 0;

    VSILFILE *file = VSIFOpenL(filename, "rb");
    if (file == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Cannot open INI file %s.",
                 filename);
        return nullptr;
    }

    if (key)
        key_len = strlen(key);

    while ((pszLine = CPLReadLine2L(file, 10000, nullptr)) != nullptr)
    {
        char *pszString = CPLRecode(pszLine, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);

        // Skip comments and empty lines
        if (*pszString == ';' || *pszString == '#' || *pszString == '\n' ||
            *pszString == '\r')
        {
            VSIFree(pszString);
            // Move to next line
            continue;
        }

        // Check for section header
        if (*pszString == '[')
        {
            char *section_end = strchr(pszString, ']');
            if (section_end != nullptr)
            {
                *section_end = '\0';  // Terminate the string at ']'
                if (section_header)
                    VSIFree(section_header);
                section_header = CPLStrdup(pszString + 1);  // Skip the '['
            }
            VSIFree(pszString);
            continue;
        }

        if (key)
        {
            // If the current line belongs to the desired section
            if (section_header != nullptr &&
                strcmp(section_header, section) == 0)
            {
                // Check if the line contains the desired key
                if (strncmp(pszString, key, key_len) == 0 &&
                    pszString[key_len] == '=')
                {
                    // Extract the value
                    char *value_start = pszString + key_len + 1;
                    char *value_end = strstr(value_start, "\r\n");
                    if (value_end != nullptr)
                    {
                        *value_end =
                            '\0';  // Terminate the string at newline character if found
                    }
                    else
                    {
                        value_end = strstr(value_start, "\n");
                        if (value_end != nullptr)
                        {
                            *value_end =
                                '\0';  // Terminate the string at newline character if found
                        }
                        else
                        {
                            value_end = strstr(value_start, "\r");
                            if (value_end != nullptr)
                            {
                                *value_end =
                                    '\0';  // Terminate the string at newline character if found
                            }
                        }
                    }

                    VSIFree(value);
                    value = CPLStrdup(value_start);
                    VSIFCloseL(file);
                    VSIFree(section_header);  // Free allocated memory
                    VSIFree(pszString);
                    return value;
                }
            }
        }
        else
        {
            VSIFree(value);
            value = nullptr;
            if (section_header)
            {
                if (!strcmp(section_header, section))
                {
                    value = section_header;  // Freed out
                    section_header = nullptr;
                }
            }

            VSIFCloseL(file);
            VSIFree(pszString);
            VSIFree(section_header);
            return value;
        }
        VSIFree(pszString);
    }

    if (section_header)
        VSIFree(section_header);  // Free allocated memory
    VSIFCloseL(file);
    return value;
}

// Retrieves EPSG codes from a CSV file based on provided geodetic identifiers.
CPL_DLL int MMReturnCodeFromMM_m_idofic(const char *pMMSRS_or_pSRS,
                                        char *szResult, MM_BYTE direction)
{
    char *aMMIDDBFFile = nullptr;  //m_idofic.dbf
    VSILFILE *pfMMSRS = nullptr;
    const char *pszLine;
    size_t nLong;
    char *id_geodes, *psidgeodes, *epsg;

    if (!pMMSRS_or_pSRS)
    {
        return 1;
    }

    {
#ifdef USE_ONLY_EMBEDDED_RESOURCE_FILES
        const char *pszFilename = nullptr;
#else
        const char *pszFilename = CPLFindFile("gdal", "MM_m_idofic.csv");
#endif
#ifdef EMBED_RESOURCE_FILES
        if (!pszFilename || EQUAL(pszFilename, "MM_m_idofic.csv"))
        {
            pfMMSRS = VSIFileFromMemBuffer(
                nullptr, (GByte *)(MiraMonGetMM_m_idofic_csv()),
                (int)(strlen(MiraMonGetMM_m_idofic_csv())),
                /* bTakeOwnership = */ false);
        }
        else
#endif
            if (pszFilename)
        {
            aMMIDDBFFile = CPLStrdup(pszFilename);
        }
    }

#ifdef EMBED_RESOURCE_FILES
    if (!pfMMSRS)
#endif
    {
        if (!aMMIDDBFFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error opening data\\MM_m_idofic.csv.\n");
            return 1;
        }

        // Opening the file with SRS information
        if (nullptr == (pfMMSRS = VSIFOpenL(aMMIDDBFFile, "r")))
        {
            VSIFree(aMMIDDBFFile);
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error opening data\\MM_m_idofic.csv.\n");
            return 1;
        }
        VSIFree(aMMIDDBFFile);
    }

    // Checking the header of the csv file
    pszLine = CPLReadLine2L(pfMMSRS, 10000, nullptr);

    if (!pszLine)

    {
        VSIFCloseL(pfMMSRS);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    id_geodes = MM_stristr(pszLine, "ID_GEODES");
    if (!id_geodes)
    {
        VSIFCloseL(pfMMSRS);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    id_geodes[strlen("ID_GEODES")] = '\0';
    psidgeodes = MM_stristr(pszLine, "PSIDGEODES");
    if (!psidgeodes)
    {
        VSIFCloseL(pfMMSRS);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    psidgeodes[strlen("PSIDGEODES")] = '\0';

    // Is PSIDGEODES in first place?
    if (strncmp(pszLine, psidgeodes, strlen("PSIDGEODES")))
    {
        VSIFCloseL(pfMMSRS);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    // Is ID_GEODES after PSIDGEODES?
    if (strncmp(pszLine + strlen("PSIDGEODES") + 1, "ID_GEODES",
                strlen("ID_GEODES")))
    {
        VSIFCloseL(pfMMSRS);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }

    // Looking for the information.
    while ((pszLine = CPLReadLine2L(pfMMSRS, 10000, nullptr)) != nullptr)
    {
        id_geodes = strstr(pszLine, ";");
        if (!id_geodes)
        {
            VSIFCloseL(pfMMSRS);
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Wrong format in data\\MM_m_idofic.csv.\n");
            return 1;
        }

        psidgeodes = strstr(id_geodes + 1, ";");
        if (!psidgeodes)
        {
            VSIFCloseL(pfMMSRS);
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Wrong format in data\\MM_m_idofic.csv.\n");
            return 1;
        }

        id_geodes[(ptrdiff_t)psidgeodes - (ptrdiff_t)id_geodes] = '\0';
        psidgeodes = CPLStrdup(pszLine);
        psidgeodes[(ptrdiff_t)id_geodes - (ptrdiff_t)pszLine] = '\0';
        id_geodes++;

        if (direction == EPSG_FROM_MMSRS)
        {
            // I have pMMSRS and I want pSRS
            if (strcmp(pMMSRS_or_pSRS, id_geodes))
            {
                VSIFree(psidgeodes);
                continue;
            }

            epsg = strstr(psidgeodes, "EPSG:");
            nLong = strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg[nLong] != '\0')
                {
                    strcpy(szResult, epsg + nLong);
                    VSIFree(psidgeodes);
                    VSIFCloseL(pfMMSRS);
                    return 0;  // found
                }
                else
                {
                    VSIFCloseL(pfMMSRS);
                    *szResult = '\0';
                    VSIFree(psidgeodes);
                    return 1;  // not found
                }
            }
        }
        else
        {
            // I have pSRS and I want pMMSRS
            epsg = strstr(psidgeodes, "EPSG:");
            nLong = strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg[nLong] != '\0')
                {
                    if (!strcmp(pMMSRS_or_pSRS, epsg + nLong))
                    {
                        strcpy(szResult, id_geodes);
                        VSIFCloseL(pfMMSRS);
                        VSIFree(psidgeodes);
                        return 0;  // found
                    }
                }
            }
        }
        VSIFree(psidgeodes);
    }

    VSIFCloseL(pfMMSRS);
    return 1;  // not found
}

// Verifies the version of a MiraMon REL 4 file.
CPL_DLL int MMCheck_REL_FILE(const char *szREL_file)
{
    char *pszLine;
    VSILFILE *pF;

    // Does the REL file exist?
    pF = VSIFOpenL(szREL_file, "r");
    if (!pF)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "The file %s must exist.",
                 szREL_file);
        return 1;
    }
    VSIFCloseL(pF);

    // Does the REL file have VERSION?
    pszLine =
        MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, nullptr);
    if (!pszLine)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "The file \"%s\" must be REL4. "
                 "You can use ConvREL.exe from MiraMon software "
                 " or GeM+ "
                 "to convert this file to REL4.",
                 szREL_file);
        return 1;
    }
    VSIFree(pszLine);

    // Does the REL file have the correct VERSION?
    // Vers>=4?
    pszLine =
        MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_Vers);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_VERS)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "The file \"%s\" must have %s>=%d.", szREL_file, KEY_Vers,
                     MM_VERS);
            VSIFree(pszLine);
            return 1;
        }
        VSIFree(pszLine);
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "The file \"%s\" must have %s>=%d.", szREL_file, KEY_Vers,
                 MM_VERS);
        return 1;
    }

    // SubVers>=0?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_SubVers);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_SUBVERS_ACCEPTED)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "The file \"%s\" must have %s>=%d.", szREL_file,
                     KEY_SubVers, MM_SUBVERS_ACCEPTED);

            VSIFree(pszLine);
            return 1;
        }
        VSIFree(pszLine);
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "The file \"%s\" must have %s>=%d.", szREL_file, KEY_SubVers,
                 MM_SUBVERS_ACCEPTED);
        return 1;
    }

    // VersMetaDades>=4?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_VersMetaDades);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_VERS_METADADES_ACCEPTED)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "The file \"%s\" must have %s>=%d.", szREL_file,
                     KEY_VersMetaDades, MM_VERS_METADADES_ACCEPTED);
            VSIFree(pszLine);
            return 1;
        }
        VSIFree(pszLine);
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "The file \"%s\" must have %s>=%d.", szREL_file,
                 KEY_VersMetaDades, MM_VERS_METADADES_ACCEPTED);
        return 1;
    }

    // SubVersMetaDades>=0?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_SubVersMetaDades);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_SUBVERS_METADADES)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "The file \"%s\" must have %s>=%d.", szREL_file,
                     KEY_SubVersMetaDades, MM_SUBVERS_METADADES);
            VSIFree(pszLine);
            return 1;
        }
        VSIFree(pszLine);
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "The file \"%s\" must have %s>=%d.", szREL_file,
                 KEY_SubVersMetaDades, MM_SUBVERS_METADADES);
        return 1;
    }
    return 0;
}

// Generates an identifier that REL 4 MiraMon metadata needs.
CPL_DLL void MMGenerateFileIdentifierFromMetadataFileName(char *pMMFN,
                                                          char *aFileIdentifier)
{
    char aCharRand[8];
    static const char aCharset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i, len_charset;

    memset(aFileIdentifier, '\0', MM_MAX_LEN_LAYER_IDENTIFIER);

    aCharRand[0] = '_';
    len_charset = (int)strlen(aCharset);
    for (i = 1; i < 7; i++)
    {
#ifndef __COVERITY__
        aCharRand[i] = aCharset[rand() % (len_charset - 1)];
#else
        aCharRand[i] = aCharset[i % (len_charset - 1)];
#endif
    }
    aCharRand[7] = '\0';
    CPLStrlcpy(aFileIdentifier, pMMFN, MM_MAX_LEN_LAYER_IDENTIFIER - 7);
    strcat(aFileIdentifier, aCharRand);
    return;
}

/* -------------------------------------------------------------------- */
/*      Managing errors and warnings                                    */
/* -------------------------------------------------------------------- */

// Checks for potential arithmetic overflow when performing multiplication
// operations between two GUInt64 values and converting the result to size_t.
// Important for 32 vs. 64 bit compiling compatibility.
CPL_DLL int MMCheckSize_t(GUInt64 nCount, GUInt64 nSize)
{
    if ((size_t)nCount != nCount)
        return 1;

    if ((size_t)nSize != nSize)
        return 1;

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (nCount != 0 && nSize > SIZE_MAX / nCount)
#else
    if (nCount != 0 && nSize > (1000 * 1000 * 1000U) / nCount)
#endif
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Overflow in MMCheckSize_t()");
        return 1;
    }
    return 0;
}

CPL_C_END  // Necessary for compiling in GDAL project
