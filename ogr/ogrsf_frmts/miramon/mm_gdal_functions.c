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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifdef GDAL_COMPILATION
#include "ogr_api.h"            // For CPL_C_START
#include "mm_gdal_functions.h"  // For CPLStrlcpy()
#include "mm_wrlayr.h"          // For calloc_function()...
#else
#include "CmptCmp.h"
#include "mm_gdal\mm_gdal_functions.h"  // For CPLStrlcpy()
#include "mm_gdal\mm_wrlayr.h"          // For calloc_function()...
#endif                                  // GDAL_COMPILATION

#ifdef GDAL_COMPILATION
CPL_C_START              // Necessary for compiling in GDAL project
#include "cpl_string.h"  // For CPL_ENC_UTF8
#else
#ifdef _WIN64
#include "gdal\release-1911-x64\cpl_string.h"  // For CPL_ENC_UTF8
#else
#include "gdal\release-1911-32\cpl_string.h"  // For CPL_ENC_UTF8
#endif
#endif

    char szInternalGraphicIdentifierEng[MM_MAX_IDENTIFIER_SIZE];
char szInternalGraphicIdentifierCat[MM_MAX_IDENTIFIER_SIZE];
char szInternalGraphicIdentifierSpa[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfVerticesEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfVerticesCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfVerticesSpa[MM_MAX_IDENTIFIER_SIZE];

char szLengthOfAarcEng[MM_MAX_IDENTIFIER_SIZE];
char szLengthOfAarcCat[MM_MAX_IDENTIFIER_SIZE];
char szLengthOfAarcSpa[MM_MAX_IDENTIFIER_SIZE];

char szInitialNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szInitialNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szInitialNodeSpa[MM_MAX_IDENTIFIER_SIZE];

char szFinalNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szFinalNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szFinalNodeSpa[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfArcsToNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsToNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsToNodeSpa[MM_MAX_IDENTIFIER_SIZE];

char szNodeTypeEng[MM_MAX_IDENTIFIER_SIZE];
char szNodeTypeCat[MM_MAX_IDENTIFIER_SIZE];
char szNodeTypeSpa[MM_MAX_IDENTIFIER_SIZE];

char szPerimeterOfThePolygonEng[MM_MAX_IDENTIFIER_SIZE];
char szPerimeterOfThePolygonCat[MM_MAX_IDENTIFIER_SIZE];
char szPerimeterOfThePolygonSpa[MM_MAX_IDENTIFIER_SIZE];

char szAreaOfThePolygonEng[MM_MAX_IDENTIFIER_SIZE];
char szAreaOfThePolygonCat[MM_MAX_IDENTIFIER_SIZE];
char szAreaOfThePolygonSpa[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfArcsEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsSpa[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfElementaryPolygonsEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfElementaryPolygonsCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfElementaryPolygonsSpa[MM_MAX_IDENTIFIER_SIZE];

void MM_FillFieldDescriptorByLanguage(void)
{
    CPLStrlcpy(szInternalGraphicIdentifierEng, "Internal Graphic identifier",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szInternalGraphicIdentifierCat, "Identificador Grafic intern",
               MM_MAX_IDENTIFIER_SIZE);
    *(unsigned char *)&szInternalGraphicIdentifierCat[16] = MM_a_WITH_GRAVE;
    CPLStrlcpy(szInternalGraphicIdentifierSpa, "Identificador Grafico interno",
               MM_MAX_IDENTIFIER_SIZE);
    *(unsigned char *)&szInternalGraphicIdentifierSpa[16] = MM_a_WITH_ACUTE;

    CPLStrlcpy(szNumberOfVerticesEng, "Number of vertices",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfVerticesCat, "Nombre de vertexs",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfVerticesSpa, "Numero de vertices",
               MM_MAX_IDENTIFIER_SIZE);
    *(unsigned char *)&szNumberOfVerticesSpa[1] = MM_u_WITH_ACUTE;
    *(unsigned char *)&szNumberOfVerticesSpa[11] = MM_e_WITH_ACUTE;

    CPLStrlcpy(szLengthOfAarcEng, "Length of arc", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szLengthOfAarcCat, "Longitud de l'arc", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szLengthOfAarcSpa, "Longitud del arco", MM_MAX_IDENTIFIER_SIZE);

    CPLStrlcpy(szInitialNodeEng, "Initial node", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szInitialNodeCat, "Node inicial", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szInitialNodeSpa, "Nodo inicial", MM_MAX_IDENTIFIER_SIZE);

    CPLStrlcpy(szFinalNodeEng, "Final node", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szFinalNodeCat, "Node final", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szFinalNodeSpa, "Nodo final", MM_MAX_IDENTIFIER_SIZE);

    CPLStrlcpy(szNumberOfArcsToNodeEng, "Number of arcs to node",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfArcsToNodeCat, "Nombre d'arcs al node",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfArcsToNodeSpa, "Numero de arcos al nodo",
               MM_MAX_IDENTIFIER_SIZE);
    *(unsigned char *)&szNumberOfArcsToNodeSpa[1] = MM_u_WITH_ACUTE;

    CPLStrlcpy(szNodeTypeEng, "Node type", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNodeTypeCat, "Tipus de node", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNodeTypeSpa, "Tipo de nodo", MM_MAX_IDENTIFIER_SIZE);

    CPLStrlcpy(szPerimeterOfThePolygonEng, "Perimeter of the polygon",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szPerimeterOfThePolygonCat, "Perimetre del poligon",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szPerimeterOfThePolygonSpa, "Perimetro del poligono",
               MM_MAX_IDENTIFIER_SIZE);

    *(unsigned char *)&szPerimeterOfThePolygonCat[3] = MM_i_WITH_ACUTE;
    *(unsigned char *)&szPerimeterOfThePolygonSpa[3] = MM_i_WITH_ACUTE;
    *(unsigned char *)&szPerimeterOfThePolygonCat[17] = MM_i_WITH_ACUTE;
    *(unsigned char *)&szPerimeterOfThePolygonSpa[17] = MM_i_WITH_ACUTE;

    CPLStrlcpy(szAreaOfThePolygonEng, "Area of the polygon",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szAreaOfThePolygonCat, "Area del poligon",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szAreaOfThePolygonSpa, "Area del poligono",
               MM_MAX_IDENTIFIER_SIZE);

    *(unsigned char *)&szAreaOfThePolygonCat[0] = MM_A_WITH_GRAVE;
    *(unsigned char *)&szAreaOfThePolygonSpa[0] = MM_A_WITH_ACUTE;
    *(unsigned char *)&szAreaOfThePolygonCat[12] = MM_i_WITH_ACUTE;
    *(unsigned char *)&szAreaOfThePolygonSpa[12] = MM_i_WITH_ACUTE;

    CPLStrlcpy(szNumberOfArcsEng, "Number of arcs", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfArcsCat, "Nombre d'arcs", MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfArcsSpa, "Numero de arcos", MM_MAX_IDENTIFIER_SIZE);

    *(unsigned char *)&szNumberOfArcsSpa[1] = MM_u_WITH_ACUTE;

    CPLStrlcpy(szNumberOfElementaryPolygonsEng, "Number of elementary polygons",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfElementaryPolygonsCat, "Nombre de poligons elementals",
               MM_MAX_IDENTIFIER_SIZE);
    CPLStrlcpy(szNumberOfElementaryPolygonsSpa,
               "Numero de poligonos elementales", MM_MAX_IDENTIFIER_SIZE);

    *(unsigned char *)&szNumberOfElementaryPolygonsSpa[1] = MM_u_WITH_ACUTE;
    *(unsigned char *)&szNumberOfElementaryPolygonsCat[13] = MM_i_WITH_ACUTE;
    *(unsigned char *)&szNumberOfElementaryPolygonsSpa[13] = MM_i_WITH_ACUTE;
}

const char *MM_pszLogFilename = nullptr;

static const char MM_EmptyString[] = {""};
#define MM_SetEndOfString (*MM_EmptyString)

void fclose_and_nullify(FILE_TYPE **pFunc)
{
    if (!pFunc || !(*pFunc))
        return;
    fclose_function(*pFunc);
    *pFunc = nullptr;
}

// CREATING AN EXTENDED MIRAMON DBF
void MM_InitializeField(struct MM_FIELD *pField)
{
    memset(pField, '\0', sizeof(*pField));
    pField->FieldType = 'C';
    pField->GeoTopoTypeField = MM_NO_ES_CAMP_GEOTOPO;
}

#define MM_ACCEPTABLE_NUMBER_OF_FIELDS 20000

struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS nFields)
{
    struct MM_FIELD *camp;
    MM_EXT_DBF_N_FIELDS i;

    // MiraMon could accept a number of fields 13.4 million
    // but GDAL prefers to limit that to 20.000 to avoid
    // too large memory allocation attempts with corrupted datasets
    if (nFields > MM_ACCEPTABLE_NUMBER_OF_FIELDS)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
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

    if ((camp = calloc_function(nFields * sizeof(*camp))) == nullptr)
        return nullptr;

    for (i = 0; i < nFields; i++)
        MM_InitializeField(camp + i);
    return camp;
}

static struct MM_DATA_BASE_XP *MM_CreateEmptyHeader(MM_EXT_DBF_N_FIELDS nFields)
{
    struct MM_DATA_BASE_XP *data_base_XP;

    if ((data_base_XP = (struct MM_DATA_BASE_XP *)calloc_function(
             sizeof(struct MM_DATA_BASE_XP))) == nullptr)
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
            free_function(data_base_XP);
            return nullptr;
        }
    }
    data_base_XP->nFields = nFields;
    return data_base_XP;
}

struct MM_DATA_BASE_XP *MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
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

static int MM_ISExtendedNameBD_XP(const char *nom_camp)
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

static MM_BYTE MM_CalculateBytesExtendedFieldName(struct MM_FIELD *camp)
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

static short int MM_ReturnValidClassicDBFFieldName(char *szChain)
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
    if ((ptr = calloc_function(i + 2)) == nullptr)
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

    NomCamp_SubIndex = calloc_function(ampladamax);
    if (!NomCamp_SubIndex)
        return nullptr;

    CPLStrlcpy(NomCamp_SubIndex, nom_camp, ampladamax);
    NomCamp_SubIndex[ampladamax - 1] = '\0';

    snprintf(subindex, sizeof(subindex), sprintf_UINT64, (GUInt64)index);

    _subindex = MM_GiveNewStringWithCharacterInFront(subindex, '_');
    if (!_subindex)
    {
        free_function(NomCamp_SubIndex);
        return nullptr;
    }

    sizet_subindex = strlen(_subindex);
    sizet_nomcamp = strlen(NomCamp_SubIndex);

    if (sizet_nomcamp + sizet_subindex > ampladamax - 1)
        memcpy(NomCamp_SubIndex + ((ampladamax - 1) - sizet_subindex),
               _subindex, strlen(_subindex));
    else
        NomCamp_SubIndex = strcat(NomCamp_SubIndex, _subindex);

    free_function(_subindex);

    return NomCamp_SubIndex;
}

MM_FIRST_RECORD_OFFSET_TYPE
MM_GiveOffsetExtendedFieldName(const struct MM_FIELD *camp)
{
    MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;

    memcpy(&offset_nom_camp,
           (char *)(&camp->reserved_2) + MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES,
           4);
    return offset_nom_camp;
}

int MM_WriteNRecordsMMBD_XPFile(struct MMAdmDatabase *MMAdmDB)
{
    if (!MMAdmDB->pMMBDXP || !MMAdmDB->pMMBDXP->pfDataBase)
        return 0;

    // Updating number of features in features table
    fseek_function(MMAdmDB->pMMBDXP->pfDataBase, MM_FIRST_OFFSET_to_N_RECORDS,
                   SEEK_SET);

    if (MMAdmDB->pMMBDXP->nRecords > UINT32_MAX)
    {
        MMAdmDB->pMMBDXP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;
    }
    else
    {
        MMAdmDB->pMMBDXP->dbf_version = MM_MARCA_DBASE4;
    }

    {
        GUInt32 nRecords32LowBits =
            (GUInt32)(MMAdmDB->pMMBDXP->nRecords & UINT32_MAX);
        if (fwrite_function(&nRecords32LowBits, 4, 1,
                            MMAdmDB->pMMBDXP->pfDataBase) != 1)
            return 1;
    }

    fseek_function(MMAdmDB->pMMBDXP->pfDataBase, MM_SECOND_OFFSET_to_N_RECORDS,
                   SEEK_SET);
    if (MMAdmDB->pMMBDXP->dbf_version == MM_MARCA_VERSIO_1_DBF_ESTESA)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        GUInt32 nRecords32HighBits =
            (GUInt32)(MMAdmDB->pMMBDXP->nRecords >> 32);
        if (fwrite_function(&nRecords32HighBits, 4, 1,
                            MMAdmDB->pMMBDXP->pfDataBase) != 1)
            return 1;

        /* from 20 to 27 */
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 8, 1,
                            MMAdmDB->pMMBDXP->pfDataBase) != 1)
            return 1;
    }
    else
    {
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 12, 1,
                            MMAdmDB->pMMBDXP->pfDataBase) != 1)
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

        if ((data_base_XP->pfDataBase =
                 fopen_function(data_base_XP->szFileName,
                                data_base_XP->ReadingMode)) == nullptr)
        {
            return FALSE;
        }
    }
    else
    {
        // If it's open we just update the header
        fseek_function(data_base_XP->pfDataBase, 0, SEEK_SET);
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
    fseek_function(data_base_XP->pfDataBase, 0, SEEK_SET);

    /* Byte 0 */
    if (fwrite_function(&(data_base_XP->dbf_version), 1, 1,
                        data_base_XP->pfDataBase) != 1)
    {
        return FALSE;
    }

    /* MM_BYTE from 1 to 3 */
    variable_byte = (MM_BYTE)(data_base_XP->year - 1900);
    if (fwrite_function(&variable_byte, 1, 1, data_base_XP->pfDataBase) != 1)
        return FALSE;
    if (fwrite_function(&(data_base_XP->month), 1, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;
    if (fwrite_function(&(data_base_XP->day), 1, 1, data_base_XP->pfDataBase) !=
        1)
        return FALSE;

    /* from 4 a 7, position MM_FIRST_OFFSET_to_N_RECORDS */
    {
        GUInt32 nRecords32LowBits =
            (GUInt32)(data_base_XP->nRecords & UINT32_MAX);
        if (fwrite_function(&nRecords32LowBits, 4, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }

    /* from 8 a 9, position MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA */
    if (fwrite_function(&(data_base_XP->FirstRecordOffset), 2, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;
    /* from 10 to 11, & from 12 to 13 */
    if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version))
    {
        if (fwrite_function(&(data_base_XP->BytesPerRecord),
                            sizeof(MM_ACCUMULATED_BYTES_TYPE_DBF), 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    else
    {
        /* from 10 to 11 */
        if (fwrite_function(&(data_base_XP->BytesPerRecord), 2, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
        /* from 12 to 13 */
        if (fwrite_function(&(data_base_XP->reserved_1), 2, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    /* byte 14 */
    if (fwrite_function(&(data_base_XP->transaction_flag), 1, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;
    /* byte 15 */
    if (fwrite_function(&(data_base_XP->encryption_flag), 1, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;

    /* from 16 to 27 */
    if (data_base_XP->nRecords > UINT32_MAX)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        GUInt32 nRecords32HighBits = (GUInt32)(data_base_XP->nRecords >> 32);
        if (fwrite_function(&nRecords32HighBits, 4, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;

        /* from 20 to 27 */
        if (fwrite_function(&(data_base_XP->dbf_on_a_LAN), 8, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    else
    {
        /* from 16 to 27 */
        if (fwrite_function(&(data_base_XP->dbf_on_a_LAN), 12, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    /* byte 28 */
    if (fwrite_function(&(data_base_XP->MDX_flag), 1, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;

    /* Byte 29 */
    if (fwrite_function(&(data_base_XP->CharSet), 1, 1,
                        data_base_XP->pfDataBase) != 1)
        return FALSE;

    /* Bytes from 30 to 31, in position MM_SEGON_OFFSET_a_OFFSET_1a_FITXA */
    if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version))
    {
        if (fwrite_function(((char *)&(data_base_XP->FirstRecordOffset)) + 2, 2,
                            1, data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    else
    {
        if (fwrite_function(&(data_base_XP->reserved_2), 2, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
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

            retorn_fwrite = fwrite_function(&data_base_XP->pField[i].FieldName,
                                            1, j, data_base_XP->pfDataBase);
            if (retorn_fwrite != (size_t)j)
            {
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
                            free_function(c);
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
                            free_function(c);
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
                fwrite_function(&data_base_XP->pField[i].ClassicalDBFFieldName,
                                1, j, data_base_XP->pfDataBase);
            if (retorn_fwrite != (size_t)j)
            {
                return FALSE;
            }

            name_size =
                MM_CalculateBytesExtendedFieldName(data_base_XP->pField + i);
            MM_EscriuOffsetNomEstesBD_XP(data_base_XP, i, bytes_acumulats);
            bytes_acumulats += name_size;
        }
        else
        {
            return FALSE;
        }

        if (fwrite_function(zero, 1, 11 - j, data_base_XP->pfDataBase) !=
            11 - (size_t)j)
        {
            return FALSE;
        }
        /* Byte 11, Field type */
        if (fwrite_function(&data_base_XP->pField[i].FieldType, 1, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            return FALSE;
        }
        /* Bytes 12 to 15 --> Reserved */
        if (fwrite_function(&data_base_XP->pField[i].reserved_1, 4, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            return FALSE;
        }
        /* Byte 16, or OFFSET_BYTESxCAMP_CAMP_CLASSIC --> BytesPerField */
        if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version) &&
            data_base_XP->pField[i].FieldType == 'C')
        {
            if (fwrite_function((void *)&byte_zero, 1, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(&data_base_XP->pField[i].BytesPerField, 1, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
        }
        /* 17th byte 17 --> In fields of type 'N' and 'F' indicates decimal places.*/
        if (data_base_XP->pField[i].FieldType == 'N' ||
            data_base_XP->pField[i].FieldType == 'F')
        {
            if (fwrite_function(&data_base_XP->pField[i].DecimalsIfFloat, 1, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(zero, 1, 1, data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
        }
        if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version) &&
            data_base_XP->pField[i].FieldType == 'C')
        {
            /* Bytes from 18 to 20 --> Reserved */
            if (fwrite_function(&data_base_XP->pField[i].reserved_2,
                                20 - 18 + 1, 1, data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
            /* Bytes from 21 to 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C
                                    in extended DBF */
            if (fwrite_function(&data_base_XP->pField[i].BytesPerField,
                                sizeof(MM_BYTES_PER_FIELD_TYPE_DBF), 1,
                                data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }

            /* Bytes from 25 to 30 --> Reserved */
            if (fwrite_function(&data_base_XP->pField[i].reserved_2[25 - 18],
                                30 - 25 + 1, 1, data_base_XP->pfDataBase) != 1)
            {
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
            if (fwrite_function(&data_base_XP->pField[i].reserved_2, 13, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                return FALSE;
            }
        }
        /* Byte 31 --> MDX flag.    */
        if (fwrite_function(&data_base_XP->pField[i].MDX_field_flag, 1, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            return FALSE;
        }
    }

    variable_byte = 13;
    if (fwrite_function(&variable_byte, 1, 1, data_base_XP->pfDataBase) != 1)
        return FALSE;

    if (data_base_XP->FirstRecordOffset != bytes_acumulats)
        return FALSE;

    // Extended fields
    for (i = 0; i < data_base_XP->nFields; i++)
    {
        if (MM_VALID_EXTENDED_DBF_NAME ==
            MM_ISExtendedNameBD_XP(data_base_XP->pField[i].FieldName))
        {
            bytes_acumulats =
                MM_GiveOffsetExtendedFieldName(data_base_XP->pField + i);
            name_size = MM_DonaBytesNomEstesCamp(data_base_XP->pField + i);

            fseek_function(data_base_XP->pfDataBase, bytes_acumulats, SEEK_SET);

            strcpy(nom_camp, data_base_XP->pField[i].FieldName);
            //CanviaJocCaracPerEscriureDBF(nom_camp, JocCaracDBFaMM(data_base_XP->CharSet, ParMM.JocCaracDBFPerDefecte));

            retorn_fwrite = fwrite_function(nom_camp, 1, name_size,
                                            data_base_XP->pfDataBase);

            if (retorn_fwrite != (size_t)name_size)
                return FALSE;
        }
    }

    return TRUE;
} /* End of MM_OpenIfNeededAndUpdateEntireHeader() */

MM_BOOLEAN MM_CreateAndOpenDBFFile(struct MM_DATA_BASE_XP *bd_xp,
                                   const char *NomFitxer)
{
    time_t currentTime;

    if (!NomFitxer || MMIsEmptyString(NomFitxer) || !bd_xp)
        return FALSE;

    MM_CheckDBFHeader(bd_xp);

    // Setting the current date
    currentTime = time(nullptr);
#ifdef GDAL_COMPILATION
    {
        struct tm ltime;
        VSILocalTime(&currentTime, &ltime);

        bd_xp->year = (short int)(ltime.tm_year + 1900);
        bd_xp->month = (MM_BYTE)(ltime.tm_mon + 1);
        bd_xp->day = (MM_BYTE)ltime.tm_mday;
    }
#else
    {
        struct tm *pLocalTime;
        pLocalTime = localtime(&currentTime);

        bd_xp->year = pLocalTime->tm_year + 1900;
        bd_xp->month = pLocalTime->tm_mon + 1;
        bd_xp->day = pLocalTime->tm_mday;
    }
#endif

    CPLStrlcpy(bd_xp->szFileName, NomFitxer, sizeof(bd_xp->szFileName));
    return MM_OpenIfNeededAndUpdateEntireHeader(bd_xp);
}

void MM_ReleaseMainFields(struct MM_DATA_BASE_XP *data_base_XP)
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
                    free_function(szChain[j]);
                    szChain[j] = nullptr;
                }
            }
        }
        free_function(data_base_XP->pField);
        data_base_XP->pField = nullptr;
        data_base_XP->nFields = 0;
    }
    return;
}

// READING THE HEADER OF AN EXTENDED DBF
// Free with MM_ReleaseDBFHeader()
int MM_ReadExtendedDBFHeaderFromFile(const char *szFileName,
                                     struct MM_DATA_BASE_XP *pMMBDXP,
                                     const char *pszRelFile)
{
    MM_BYTE variable_byte;
    FILE_TYPE *pf;
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

    if ((pMMBDXP->pfDataBase = fopen_function(pMMBDXP->szFileName,
                                              pMMBDXP->ReadingMode)) == nullptr)
        return 1;

    pf = pMMBDXP->pfDataBase;

    fseek_function(pf, 0, SEEK_SET);
    /* ====== Header reading (32 bytes) =================== */
    offset_primera_fitxa = 0;

    if (1 != fread_function(&(pMMBDXP->dbf_version), 1, 1, pf) ||
        1 != fread_function(&variable_byte, 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->month), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->day), 1, 1, pf))
    {
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    if (1 != fread_function(&nRecords32LowBits, 4, 1, pf))
    {
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    if (1 != fread_function(&offset_primera_fitxa, 2, 1, pf))
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
        offset_reintent = ftell_function(pf);

    if (1 != fread_function(&two_bytes, 2, 1, pf) ||
        1 != fread_function(&(pMMBDXP->reserved_1), 2, 1, pf) ||
        1 != fread_function(&(pMMBDXP->transaction_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->encryption_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pf))
    {
        free_function(pMMBDXP->pField);
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

    if (1 != fread_function(&(pMMBDXP->MDX_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->CharSet), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->reserved_2), 2, 1, pf))
    {
        free_function(pMMBDXP->pField);
        pMMBDXP->pField = nullptr;
        pMMBDXP->nFields = 0;
        fclose_and_nullify(&pMMBDXP->pfDataBase);
        return 1;
    }

    // Checking for a cpg file
    if (pMMBDXP->CharSet == 0)
    {
        FILE_TYPE *f_cpg;
        char charset_cpg[11];

        strcpy(cpg_file, pMMBDXP->szFileName);
        CPLStrlcpy(cpg_file, reset_extension(cpg_file, "cpg"),
                   sizeof(cpg_file));
        f_cpg = fopen_function(cpg_file, "r");
        if (f_cpg)
        {
            char *p;
            size_t read_bytes;
            fseek_function(f_cpg, 0L, SEEK_SET);
            if (11 > (read_bytes = fread_function(charset_cpg, 1, 10, f_cpg)))
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
            fclose_function(f_cpg);
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
            free_function(pMMBDXP->pField);
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

        fseek_function(pf, 0, SEEK_END);
        if (32 - 1 < ftell_function(pf))
        {
            fseek_function(pf, 32, SEEK_SET);
            do
            {
                bytes_per_camp = 0;
                fseek_function(
                    pf,
                    32 + (MM_FILE_OFFSET)pMMBDXP->nFields * 32 +
                        (MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF + 1 + 4),
                    SEEK_SET);
                if (1 != fread_function(&bytes_per_camp, 1, 1, pf) ||
                    1 != fread_function(&un_byte, 1, 1, pf) ||
                    1 != fread_function(&tretze_bytes,
                                        3 + sizeof(bytes_per_camp), 1, pf))
                {
                    free_function(pMMBDXP->pField);
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
        free_function(pMMBDXP->pField);
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
        free_function(pMMBDXP->pField);
        pMMBDXP->pField = nullptr;
    }

    fseek_function(pf, 32, SEEK_SET);
    for (nIField = 0; nIField < pMMBDXP->nFields; nIField++)
    {
        if (1 != fread_function(pMMBDXP->pField[nIField].FieldName,
                                MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF, 1, pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].FieldType), 1, 1,
                                pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].reserved_1), 4, 1,
                                pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].BytesPerField), 1, 1,
                                pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].DecimalsIfFloat), 1,
                                1, pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].reserved_2), 13, 1,
                                pf) ||
            1 != fread_function(&(pMMBDXP->pField[nIField].MDX_field_flag), 1,
                                1, pf))
        {
            free_function(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            fclose_function(pf);
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
            free_function(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            fclose_function(pf);
            pMMBDXP->pfDataBase = nullptr;
            return 1;
        }

        if (pMMBDXP->pField[nIField].BytesPerField == 0)
        {
            if (!MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
            {
                free_function(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                fclose_function(pf);
                pMMBDXP->pfDataBase = nullptr;
                return 1;
            }
            if (pMMBDXP->pField[nIField].FieldType != 'C')
            {
                free_function(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                fclose_function(pf);
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
                free_function(pMMBDXP->pField);
                pMMBDXP->pField = nullptr;
                pMMBDXP->nFields = 0;
                fclose_function(pf);
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

                free_function(pszDesc);
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
                free_function(pszDesc);
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

                free_function(pszDesc);
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

                free_function(pszDesc);
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
            free_function(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            fclose_function(pf);
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
            fseek_function(pf, offset_reintent, SEEK_SET);
            some_problems_when_reading++;
            /* Reset IdGraficField as it might no longer be valid */
            pMMBDXP->IdGraficField = 0;
            goto reintenta_lectura_per_si_error_CreaCampBD_XP;
        }
        else
        {
            free_function(pMMBDXP->pField);
            pMMBDXP->pField = nullptr;
            pMMBDXP->nFields = 0;
            fclose_function(pf);
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
                fseek_function(pf, offset_nom_camp, SEEK_SET);
                if (1 != fread_function(pMMBDXP->pField[nIField].FieldName,
                                        mida_nom, 1, pf))
                {
                    free_function(pMMBDXP->pField);
                    pMMBDXP->pField = nullptr;
                    pMMBDXP->nFields = 0;
                    fclose_function(pf);
                    pMMBDXP->pfDataBase = nullptr;
                    return 1;
                }
                pMMBDXP->pField[nIField].FieldName[mida_nom] = '\0';

                // All field names to UTF-8
                if (pMMBDXP->CharSet == MM_JOC_CARAC_ANSI_DBASE)
                {
                    pszString =
                        CPLRecode_function(pMMBDXP->pField[nIField].FieldName,
                                           CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    CPLStrlcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                               MM_MAX_LON_FIELD_NAME_DBF);
                    CPLFree_function(pszString);
                }
                else if (pMMBDXP->CharSet == MM_JOC_CARAC_OEM850_DBASE)
                {
                    MM_oemansi(pMMBDXP->pField[nIField].FieldName);
                    pszString =
                        CPLRecode_function(pMMBDXP->pField[nIField].FieldName,
                                           CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    CPLStrlcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                               MM_MAX_LON_FIELD_NAME_DBF - 1);
                    CPLFree_function(pszString);
                }
            }
        }
    }

    pMMBDXP->IdEntityField = MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    return 0;
}  // End of MM_ReadExtendedDBFHeaderFromFile()

void MM_ReleaseDBFHeader(struct MM_DATA_BASE_XP **data_base_XP)
{
    if (!data_base_XP)
        return;
    if (!*data_base_XP)
        return;

    MM_ReleaseMainFields(*data_base_XP);
    free_function(*data_base_XP);
    *data_base_XP = nullptr;

    return;
}

int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
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

int MM_DuplicateFieldDBXP(struct MM_FIELD *camp_final,
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
char *MM_oemansi_n(char *szszChain, size_t n_bytes)
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
char *MM_stristr(const char *haystack, const char *needle)
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

char *MM_oemansi(char *szszChain)
{
    return MM_oemansi_n(szszChain, SIZE_MAX);
}

static MM_BOOLEAN MM_FillFieldDB_XP(
    struct MM_FIELD *camp, const char *FieldName,
    const char *FieldDescriptionEng, const char *FieldDescriptionCat,
    const char *FieldDescriptionSpa, char FieldType,
    MM_BYTES_PER_FIELD_TYPE_DBF BytesPerField, MM_BYTE DecimalsIfFloat)
{
    char nom_temp[MM_MAX_LON_FIELD_NAME_DBF];
    int retorn_valida_nom_camp;

    if (FieldName)
    {
        retorn_valida_nom_camp = MM_ISExtendedNameBD_XP(FieldName);
        if (retorn_valida_nom_camp == MM_DBF_NAME_NO_VALID)
            return FALSE;
        CPLStrlcpy(camp->FieldName, FieldName, MM_MAX_LON_FIELD_NAME_DBF);

        if (retorn_valida_nom_camp == MM_VALID_EXTENDED_DBF_NAME)
        {
            MM_CalculateBytesExtendedFieldName(camp);
            CPLStrlcpy(nom_temp, FieldName, MM_MAX_LON_FIELD_NAME_DBF);
            MM_ReturnValidClassicDBFFieldName(nom_temp);
            nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF] = '\0';
            CPLStrlcpy(camp->ClassicalDBFFieldName, nom_temp,
                       MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
        }
    }

    if (FieldDescriptionEng)
        CPLStrlcpy(camp->FieldDescription[MM_DEF_LANGUAGE], FieldDescriptionEng,
                   sizeof(camp->FieldDescription[MM_DEF_LANGUAGE]));
    else
        strcpy(camp->FieldDescription[MM_DEF_LANGUAGE], "\0");

    if (FieldDescriptionEng)
        CPLStrlcpy(camp->FieldDescription[MM_ENG_LANGUAGE], FieldDescriptionEng,
                   sizeof(camp->FieldDescription[MM_ENG_LANGUAGE]));
    else
        strcpy(camp->FieldDescription[MM_ENG_LANGUAGE], "\0");

    if (FieldDescriptionCat)
        CPLStrlcpy(camp->FieldDescription[MM_CAT_LANGUAGE], FieldDescriptionCat,
                   sizeof(camp->FieldDescription[MM_CAT_LANGUAGE]));
    else
        strcpy(camp->FieldDescription[MM_CAT_LANGUAGE], "\0");

    if (FieldDescriptionSpa)
        CPLStrlcpy(camp->FieldDescription[MM_SPA_LANGUAGE], FieldDescriptionSpa,
                   sizeof(camp->FieldDescription[MM_SPA_LANGUAGE]));
    else
        strcpy(camp->FieldDescription[MM_SPA_LANGUAGE], "\0");

    camp->FieldType = FieldType;
    camp->DecimalsIfFloat = DecimalsIfFloat;
    camp->BytesPerField = BytesPerField;
    return TRUE;
}

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                        MM_BYTE n_perimeter_decimals,
                                        MM_BYTE n_area_decimals_decimals)
{
    MM_EXT_DBF_N_FIELDS i_camp = 0;

    MM_FillFieldDB_XP(
        bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
        szInternalGraphicIdentifierEng, szInternalGraphicIdentifierCat,
        szInternalGraphicIdentifierSpa, 'N', MM_MIN_WIDTH_ID_GRAFIC, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNVertexsDefecte,
                      szNumberOfVerticesEng, szNumberOfVerticesCat,
                      szNumberOfVerticesSpa, 'N', MM_MIN_WIDTH_N_VERTEXS, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampPerimetreDefecte,
                      szPerimeterOfThePolygonEng, szPerimeterOfThePolygonCat,
                      szPerimeterOfThePolygonSpa, 'N', MM_MIN_WIDTH_LONG,
                      n_perimeter_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_PERIMETRE;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampAreaDefecte,
                      szAreaOfThePolygonEng, szAreaOfThePolygonCat,
                      szAreaOfThePolygonSpa, 'N', MM_MIN_WIDTH_AREA,
                      n_area_decimals_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_AREA;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNArcsDefecte,
                      szNumberOfArcsEng, szNumberOfArcsCat, szNumberOfArcsSpa,
                      'N', MM_MIN_WIDTH_N_ARCS, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_ARCS;
    i_camp++;

    MM_FillFieldDB_XP(
        bd_xp->pField + i_camp, szMMNomCampNPoligonsDefecte,
        szNumberOfElementaryPolygonsEng, szNumberOfElementaryPolygonsCat,
        szNumberOfElementaryPolygonsSpa, 'N', MM_MIN_WIDTH_N_POLIG, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_POLIG;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstArcFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                    MM_BYTE n_decimals)
{
    MM_EXT_DBF_N_FIELDS i_camp;

    i_camp = 0;
    MM_FillFieldDB_XP(
        bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
        szInternalGraphicIdentifierEng, szInternalGraphicIdentifierCat,
        szInternalGraphicIdentifierSpa, 'N', MM_MIN_WIDTH_ID_GRAFIC, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNVertexsDefecte,
                      szNumberOfVerticesEng, szNumberOfVerticesCat,
                      szNumberOfVerticesSpa, 'N', MM_MIN_WIDTH_N_VERTEXS, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampLongitudArcDefecte,
                      szLengthOfAarcEng, szLengthOfAarcCat, szLengthOfAarcSpa,
                      'N', MM_MIN_WIDTH_LONG, n_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_LONG_ARC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNodeIniDefecte,
                      szInitialNodeEng, szInitialNodeCat, szInitialNodeSpa, 'N',
                      MM_MIN_WIDTH_INITIAL_NODE, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_NODE_INI;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNodeFiDefecte,
                      szFinalNodeEng, szFinalNodeCat, szFinalNodeSpa, 'N',
                      MM_MIN_WIDTH_FINAL_NODE, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_NODE_FI;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp)
{
    MM_EXT_DBF_N_FIELDS i_camp;

    i_camp = 0;

    MM_FillFieldDB_XP(
        bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
        szInternalGraphicIdentifierEng, szInternalGraphicIdentifierCat,
        szInternalGraphicIdentifierSpa, 'N', MM_MIN_WIDTH_ID_GRAFIC, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampArcsANodeDefecte,
                      szNumberOfArcsToNodeEng, szNumberOfArcsToNodeCat,
                      szNumberOfArcsToNodeSpa, 'N', MM_MIN_WIDTH_ARCS_TO_NODE,
                      0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ARCS_A_NOD;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampTipusNodeDefecte,
                      szNodeTypeEng, szNodeTypeCat, szNodeTypeSpa, 'N', 1, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_TIPUS_NODE;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstPointFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp)
{
    size_t i_camp = 0;

    MM_FillFieldDB_XP(
        bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
        szInternalGraphicIdentifierEng, szInternalGraphicIdentifierCat,
        szInternalGraphicIdentifierSpa, 'N', MM_MIN_WIDTH_ID_GRAFIC, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    return i_camp;
}

/*
    Controlling the number of significant figures is often crucial in science
    and technology, and the best option when nothing is known about the number
    to be printed (if it is really small (near to 0), or very large), and
    allows a good return (the same value) into memory when re-read from a text
    file with scanf() functions.
    If you need to print 0.00000000000000000000000000000000000000001 with %f
    you will need an extremely large string. If you print 1980.45 with %E you
    obtain 1.98045E+003, needing more space, and not being easy to interpret
    for some people. Moreover, “normal” users do not want to see 1.0 as
    1.0E+000 or 1.0E+00. The choice of the format specifier, and the integer
    to be passed to the ‘*’ is not always easy,
    and MM_SprintfDoubleSignifFigures() automatically uses a “fair” notation
    whenever it is possible, resulting in shorter strings, being them under
    control (the maximum length of the resulting string is always known).
    Moreover, it avoids some failures in compilers not expecting
    NAN or INF values.
*/
int MM_SprintfDoubleSignifFigures(char *szChain, size_t size_szChain,
                                  int nSignifFigures, double dfRealValue)
{
    double VALOR_LIMIT_PRINT_IN_FORMAT_E;
    double VALOR_TOO_SMALL_TO_PRINT_f;
    int retorn, exponent;
    char *ptr;

#define N_POWERS MM_MAX_XS_DOUBLE

    /* This expression ensures that no garbage is written in
    the non-significant digits of the integer part, i.e., requesting 9E20
    with 16 significant digits does not print 90000000000000004905, where
    "4905" is garbage generated by the print call with such a large value 
    and "%.16f", but rather writes 9.000000000000000E+20.
    At the same time, it ensures that 9000 requested with 4 significant
    digits is written as 9000 and that requested with 5 significant digits
    is written as 9000.0, but that requested with 3 significant digits is
    written as 9.00E+03. */
    double potencies_de_10[N_POWERS] = {
        1E+1,  1E+2,  1E+3,  1E+4,  1E+5,  1E+6,  1E+7,  1E+8, 1E+9,
        1E+10, 1E+11, 1E+12, 1E+13, 1E+14, 1E+15, 1E+16, 1E+17};

    /* This expression ensures that -9E-7 requested with 11 significant digits
    still uses "natural" notation and gives -0.0000009000000000, which still
    fits exactly within the 20 characters of a 'N' field in dBASE, while
    requested with 12 significant digits jumps to exponential notation and
    writes -9.00000000000E-07, which also fits (in this case, comfortably)
    within the 20 characters of dBASE.
    The expression could be replaced by: pow(10,-max(0,20-2-signif_digits)); */
    double fraccions_de_10[N_POWERS + 1] = {
        1E-1,  1E-2,  1E-3,  1E-4,  1E-5,  1E-6,  1E-7,  1E-8,  1E-9,
        1E-10, 1E-11, 1E-12, 1E-13, 1E-14, 1E-15, 1E-16, 1E-17, 1E-18};

    if (!szChain)
        return 0;

    if (size_szChain < 3)
        return 0;

    memset(szChain, '\0', size_szChain);

    if (MM_IsNANDouble(dfRealValue))
        return snprintf(szChain, size_szChain, "NAN");

    if (MM_IsDoubleInfinite(dfRealValue))
        return snprintf(szChain, size_szChain, "INF");

    if (dfRealValue == 0.0)
        return snprintf(szChain, size_szChain, "%.*f", nSignifFigures, 0.0);

    if (nSignifFigures < 1)
        return snprintf(szChain, size_szChain, "0.0");

    if (nSignifFigures > N_POWERS)
        nSignifFigures = N_POWERS;

    retorn = snprintf(szChain, size_szChain, "%.*E", nSignifFigures - 1,
                      dfRealValue);

    VALOR_LIMIT_PRINT_IN_FORMAT_E = potencies_de_10[nSignifFigures - 1];
    VALOR_TOO_SMALL_TO_PRINT_f =
        fraccions_de_10[MM_MAX_XS_DOUBLE - nSignifFigures];

    if (dfRealValue > VALOR_LIMIT_PRINT_IN_FORMAT_E ||
        dfRealValue < -VALOR_LIMIT_PRINT_IN_FORMAT_E ||
        (dfRealValue < VALOR_TOO_SMALL_TO_PRINT_f &&
         dfRealValue > -VALOR_TOO_SMALL_TO_PRINT_f))
        return retorn;

    ptr = strchr(szChain, 'E');
    if (!ptr)
        return 0;
    exponent = atoi(ptr + 1);

    return sprintf(szChain, "%.*f",
                   (nSignifFigures - exponent - 1) > 0
                       ? (nSignifFigures - exponent - 1)
                       : 0,
                   dfRealValue);
#undef N_POWERS
}  // End of SprintfDoubleXifSignif()

int MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                                  MM_EXT_DBF_N_FIELDS *nStringCurrentLength)
{

    if (!pszStringSrc)
    {
        if (1 >= *nStringCurrentLength)
        {
            void *new_ptr = realloc_function(*pszStringDst, 2);
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
        void *new_ptr =
            realloc_function(*pszStringDst, strlen(pszStringSrc) + 1);
        if (!new_ptr)
            return 1;
        (*pszStringDst) = new_ptr;
        *nStringCurrentLength = (MM_EXT_DBF_N_FIELDS)(strlen(pszStringSrc) + 1);
    }
    strcpy(*pszStringDst, pszStringSrc);
    return 0;
}

// This function assumes that all the file is saved in disk and closed.
int MM_ChangeDBFWidthField(struct MM_DATA_BASE_XP *data_base_XP,
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

        if ((record = calloc_function((size_t)data_base_XP->BytesPerRecord)) ==
            nullptr)
            return 1;

        record[data_base_XP->BytesPerRecord - 1] = MM_SetEndOfString;

        if ((whites = (char *)calloc_function((size_t)nNewWidth)) == nullptr)
        {
            free_function(record);
            return 1;
        }
        memset(whites, ' ', nNewWidth);

        nfitx = data_base_XP->nRecords;
        i_reg = (canvi_amplada < 0 ? 0 : nfitx - 1);
        while (TRUE)
        {
            if (0 != fseek_function(data_base_XP->pfDataBase,
                                    data_base_XP->FirstRecordOffset +
                                        (MM_FILE_OFFSET)i_reg *
                                            data_base_XP->BytesPerRecord,
                                    SEEK_SET))
            {
                free_function(whites);
                free_function(record);
                return 1;
            }

            if (1 != fread_function(record, data_base_XP->BytesPerRecord, 1,
                                    data_base_XP->pfDataBase))
            {
                free_function(whites);
                free_function(record);
                return 1;
            }

            if (0 !=
                fseek_function(
                    data_base_XP->pfDataBase,
                    (MM_FILE_OFFSET)data_base_XP->FirstRecordOffset +
                        i_reg * ((MM_FILE_OFFSET)data_base_XP->BytesPerRecord +
                                 canvi_amplada),
                    SEEK_SET))
            {
                free_function(whites);
                free_function(record);
                return 1;
            }

            if (1 !=
                fwrite_function(record, l_glop1, 1, data_base_XP->pfDataBase))
            {
                free_function(whites);
                free_function(record);
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
                    retorn_fwrite = fwrite_function(whites, nNewWidth, 1,
                                                    data_base_XP->pfDataBase);

                    if (1 != retorn_fwrite)
                    {
                        free_function(whites);
                        free_function(record);
                        return 1;
                    }
                    break;
                case 'N':

                    if (canvi_amplada >= 0)
                    {
                        if (1 != fwrite_function(whites, canvi_amplada, 1,
                                                 data_base_XP->pfDataBase) ||
                            1 !=
                                fwrite_function(
                                    record + l_glop1,
                                    data_base_XP->pField[nIField].BytesPerField,
                                    1, data_base_XP->pfDataBase))
                        {
                            free_function(whites);
                            free_function(record);
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

                        retorn_fwrite = fwrite_function(
                            record + j, nNewWidth, 1, data_base_XP->pfDataBase);
                        if (1 != retorn_fwrite)
                        {
                            free_function(whites);
                            free_function(record);
                            return 1;
                        }
                    }

                    break;
                default:
                    free_function(whites);
                    free_function(record);
                    return 1;
            }
            if (l_glop2)
            {
                retorn_fwrite = fwrite_function(record + i_glop2, l_glop2, 1,
                                                data_base_XP->pfDataBase);
                if (1 != retorn_fwrite)
                {
                    free_function(whites);
                    free_function(record);
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

        free_function(whites);
        free_function(record);

        retorn_TruncaFitxer = TruncateFile_function(
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

static void MM_AdoptHeight(double *desti, const double *proposta, uint32_t flag)
{
    if (*proposta == MM_NODATA_COORD_Z)
        return;

    if (flag & MM_STRING_HIGHEST_ALTITUDE)
    {
        if (*desti == MM_NODATA_COORD_Z || *desti < *proposta)
            *desti = *proposta;
    }
    else if (flag & MM_STRING_LOWEST_ALTITUDE)
    {
        if (*desti == MM_NODATA_COORD_Z || *desti > *proposta)
            *desti = *proposta;
    }
    else
    {
        // First coordinate of this vertice
        if (*desti == MM_NODATA_COORD_Z)
            *desti = *proposta;
    }
}

int MM_GetArcHeights(double *coord_z, FILE_TYPE *pF, MM_N_VERTICES_TYPE n_vrt,
                     struct MM_ZD *pZDescription, uint32_t flag)
{
    MM_N_HEIGHT_TYPE i;
    MM_N_VERTICES_TYPE i_vrt;
    double *pcoord_z;
    MM_N_HEIGHT_TYPE n_alcada, n_h_total;
    int tipus;
    double *alcada = nullptr, *palcada, *palcada_i;
#define MM_N_ALCADA_LOCAL 50  // Nr of local heights
    double local_CinquantaAlcades[MM_N_ALCADA_LOCAL];

    for (i_vrt = 0; i_vrt < n_vrt; i_vrt++)
        coord_z[i_vrt] = MM_NODATA_COORD_Z;

    if (pZDescription->nZCount == INT_MIN)
        return 0;
    tipus = MM_ARC_HEIGHT_TYPE(pZDescription->nZCount);
    n_alcada = MM_ARC_N_HEIGHTS(pZDescription->nZCount);
    if (n_vrt == 0 || n_alcada == 0)
        return 0;

    if (tipus == MM_ARC_HEIGHT_FOR_EACH_VERTEX)
    {
        if (n_vrt > (unsigned)(INT_MAX / n_alcada))
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory, "Integer overflow");
            return 1;
        }
        n_h_total = (MM_N_HEIGHT_TYPE)n_vrt * n_alcada;
    }
    else
        n_h_total = n_alcada;

    if (n_h_total <= MM_N_ALCADA_LOCAL)
        palcada = local_CinquantaAlcades;
    else
    {
        if (MMCheckSize_t(n_h_total, sizeof(double)))
            return 1;
        if (nullptr == (palcada = alcada = calloc_function((size_t)n_h_total *
                                                           sizeof(double))))
            return 1;
    }

    if (fseek_function(pF, pZDescription->nOffsetZ, SEEK_SET))
    {
        if (alcada)
            free_function(alcada);
        return 1;
    }
    if (n_h_total != (MM_N_HEIGHT_TYPE)fread_function(palcada, sizeof(double),
                                                      n_h_total, pF))
    {
        if (alcada)
            free_function(alcada);
        return 1;
    }

    if (tipus == MM_ARC_HEIGHT_FOR_EACH_VERTEX)
    {
        palcada_i = palcada;
        for (i = 0; i < n_alcada; i++)
        {
            for (i_vrt = 0, pcoord_z = coord_z; i_vrt < n_vrt;
                 i_vrt++, pcoord_z++, palcada_i++)
                MM_AdoptHeight(pcoord_z, palcada_i, flag);
        }
    }
    else
    {
        palcada_i = palcada;
        pcoord_z = coord_z;
        for (i = 0; i < n_alcada; i++, palcada_i++)
            MM_AdoptHeight(pcoord_z, palcada_i, flag);

        if (*pcoord_z != MM_NODATA_COORD_Z)
        {
            /*Copio el mateix valor a totes les alcades.*/
            for (i_vrt = 1, pcoord_z++; i_vrt < (size_t)n_vrt;
                 i_vrt++, pcoord_z++)
                *pcoord_z = *coord_z;
        }
    }
    if (alcada)
        free_function(alcada);
    return 0;
}  // End of MM_GetArcHeights()

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

char *MM_RemoveInitial_and_FinalQuotationMarks(char *szChain)
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

char *MM_RemoveLeadingWhitespaceOfString(char *szChain)
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

char *MM_RemoveWhitespacesFromEndOfString(char *str)
{
    if (str == nullptr)
        return str;
    return MM_l_RemoveWhitespacesFromEndOfString(str, strlen(str));
}

struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(
    FILE_TYPE *f, MM_EXT_DBF_N_RECORDS nNumberOfRecords,
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
    if (nullptr == (id = (struct MM_ID_GRAFIC_MULTIPLE_RECORD *)calloc_function(
                        (size_t)nNumberOfRecords * sizeof(*id))))
        return nullptr;

    if (bytes_id_grafic == UINT32_MAX)
    {
        free_function(id);
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Overflow in bytes_id_graphic");
        return nullptr;
    }

    if (nullptr ==
        (fitxa = (char *)calloc_function((size_t)bytes_id_grafic + 1)))
    {
        free_function(id);
        return nullptr;
    }
    fitxa[bytes_id_grafic] = '\0';

    fseek_function(f,
                   (MM_FILE_OFFSET)offset_1era +
                       (MM_FILE_OFFSET)bytes_acumulats_id_grafic,
                   SEEK_SET);

    i_dbf = 0;
    do
    {
        if (i_dbf == nNumberOfRecords ||
            fread_function(fitxa, 1, bytes_id_grafic, f) !=
                (size_t)bytes_id_grafic)
        {
            free_function(id);
            free_function(fitxa);
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
            free_function(id);
            free_function(fitxa);
            return nullptr;
        }
        i = id_grafic;
        if (i >= (MM_EXT_DBF_SIGNED_N_RECORDS)nNumberOfRecords)
        {
            free_function(fitxa);
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
                free_function(fitxa);
                return id;
            }
            fseek_function(f, bytes_final_id_principi_id1, SEEK_CUR);
            if (fread_function(fitxa, 1, bytes_id_grafic, f) !=
                (size_t)bytes_id_grafic)
            {
                free_function(id);
                free_function(fitxa);
                return nullptr;
            }
            if (1 != sscanf(fitxa, scanf_MM_EXT_DBF_SIGNED_N_RECORDS,
                            &id_grafic) ||
                id_grafic >= (MM_EXT_DBF_SIGNED_N_RECORDS)nNumberOfRecords)
            {
                free_function(fitxa);
                return id;
            }
            i_dbf++;
        } while (id_grafic == i);
    }
}  // End of MMCreateExtendedDBFIndex()

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
