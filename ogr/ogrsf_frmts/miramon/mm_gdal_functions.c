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
#include "mm_gdal_functions.h"  // For MM_strnzcpy()
#include "mm_wrlayr.h"          // For calloc_function()...
#else
#include "CmptCmp.h"
#include "mm_gdal\mm_gdal_functions.h"  // For MM_strnzcpy()
#include "mm_gdal\mm_wrlayr.h"          // For calloc_function()...
#endif                                  // GDAL_COMPILATION

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling in GDAL project
#endif       // GDAL_COMPILATION

#include "cpl_string.h"  // For CPL_ENC_UTF8

    static char local_message[MAX_LOCAL_MESSAGE];

char szInternalGraphicIdentifierEng[MM_MAX_IDENTIFIER_SIZE];
char szInternalGraphicIdentifierCat[MM_MAX_IDENTIFIER_SIZE];
char szInternalGraphicIdentifierEsp[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfVerticesEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfVerticesCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfVerticesEsp[MM_MAX_IDENTIFIER_SIZE];

char szLenghtOfAarcEng[MM_MAX_IDENTIFIER_SIZE];
char szLenghtOfAarcCat[MM_MAX_IDENTIFIER_SIZE];
char szLenghtOfAarcEsp[MM_MAX_IDENTIFIER_SIZE];

char szInitialNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szInitialNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szInitialNodeEsp[MM_MAX_IDENTIFIER_SIZE];

char szFinalNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szFinalNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szFinalNodeEsp[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfArcsToNodeEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsToNodeCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsToNodeEsp[MM_MAX_IDENTIFIER_SIZE];

char szNodeTypeEng[MM_MAX_IDENTIFIER_SIZE];
char szNodeTypeCat[MM_MAX_IDENTIFIER_SIZE];
char szNodeTypeEsp[MM_MAX_IDENTIFIER_SIZE];

char szPerimeterOfThePolygonEng[MM_MAX_IDENTIFIER_SIZE];
char szPerimeterOfThePolygonCat[MM_MAX_IDENTIFIER_SIZE];
char szPerimeterOfThePolygonEsp[MM_MAX_IDENTIFIER_SIZE];

char szAreaOfThePolygonEng[MM_MAX_IDENTIFIER_SIZE];
char szAreaOfThePolygonCat[MM_MAX_IDENTIFIER_SIZE];
char szAreaOfThePolygonEsp[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfArcsEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfArcsEsp[MM_MAX_IDENTIFIER_SIZE];

char szNumberOfElementaryPolygonsEng[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfElementaryPolygonsCat[MM_MAX_IDENTIFIER_SIZE];
char szNumberOfElementaryPolygonsEsp[MM_MAX_IDENTIFIER_SIZE];

void MM_FillFieldDescriptorByLanguage(void)
{
    MM_strnzcpy(szInternalGraphicIdentifierEng, "Internal Graphic identifier",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szInternalGraphicIdentifierCat, "Identificador Grafic intern",
                MM_MAX_IDENTIFIER_SIZE);
    szInternalGraphicIdentifierCat[16] = MM_a_WITH_GRAVE;
    MM_strnzcpy(szInternalGraphicIdentifierEsp, "Identificador Grafico interno",
                MM_MAX_IDENTIFIER_SIZE);
    szInternalGraphicIdentifierEsp[16] = MM_a_WITH_ACUTE;

    MM_strnzcpy(szNumberOfVerticesEng, "Number of vertices",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfVerticesCat, "Nombre de vertexs",
                MM_MAX_IDENTIFIER_SIZE);
    szNumberOfVerticesCat[11] = MM_e_WITH_GRAVE;
    MM_strnzcpy(szNumberOfVerticesEsp, "Numero de vertices",
                MM_MAX_IDENTIFIER_SIZE);
    szNumberOfVerticesEsp[1] = MM_u_WITH_ACUTE;
    szNumberOfVerticesEsp[11] = MM_e_WITH_ACUTE;

    MM_strnzcpy(szLenghtOfAarcEng, "Lenght of arc", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szLenghtOfAarcCat, "Longitud de l'arc", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szLenghtOfAarcEsp, "Longitud del arco", MM_MAX_IDENTIFIER_SIZE);

    MM_strnzcpy(szInitialNodeEng, "Initial node", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szInitialNodeCat, "Node inicial", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szInitialNodeEsp, "Nodo inicial", MM_MAX_IDENTIFIER_SIZE);

    MM_strnzcpy(szFinalNodeEng, "Final node", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szFinalNodeCat, "Node final", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szFinalNodeEsp, "Nodo final", MM_MAX_IDENTIFIER_SIZE);

    MM_strnzcpy(szNumberOfArcsToNodeEng, "Number of arcs to node",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfArcsToNodeCat, "Nombre d'arcs al node",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfArcsToNodeEsp, "Numero de arcos al nodo",
                MM_MAX_IDENTIFIER_SIZE);
    szNumberOfArcsToNodeEsp[1] = MM_u_WITH_ACUTE;

    MM_strnzcpy(szNodeTypeEng, "Node type", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNodeTypeCat, "Tipus de node", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNodeTypeEsp, "Tipo de nodo", MM_MAX_IDENTIFIER_SIZE);

    MM_strnzcpy(szPerimeterOfThePolygonEng, "Perimeter of the polygon",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szPerimeterOfThePolygonCat, "Perimetre del poligon",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szPerimeterOfThePolygonEsp, "Perimetro del poligono",
                MM_MAX_IDENTIFIER_SIZE);
    szPerimeterOfThePolygonCat[3] = MM_i_WITH_ACUTE;
    szPerimeterOfThePolygonEsp[3] = MM_i_WITH_ACUTE;
    szPerimeterOfThePolygonCat[17] = MM_i_WITH_ACUTE;
    szPerimeterOfThePolygonEsp[17] = MM_i_WITH_ACUTE;

    MM_strnzcpy(szAreaOfThePolygonEng, "Area of the polygon",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szAreaOfThePolygonCat, "Area del poligon",
                MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szAreaOfThePolygonEsp, "Area del poligono",
                MM_MAX_IDENTIFIER_SIZE);
    szAreaOfThePolygonCat[0] = MM_A_WITH_GRAVE;
    szAreaOfThePolygonEsp[0] = MM_A_WITH_ACUTE;
    szAreaOfThePolygonCat[12] = MM_i_WITH_ACUTE;
    szAreaOfThePolygonEsp[12] = MM_i_WITH_ACUTE;

    MM_strnzcpy(szNumberOfArcsEng, "Number of arcs", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfArcsCat, "Nombre d'arcs", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfArcsEsp, "Numero de arcos", MM_MAX_IDENTIFIER_SIZE);
    szNumberOfArcsEsp[1] = MM_u_WITH_ACUTE;

    MM_strnzcpy(szNumberOfElementaryPolygonsEng,
                "Number of elementary polygons", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfElementaryPolygonsCat,
                "Nombre de poligons elementals", MM_MAX_IDENTIFIER_SIZE);
    MM_strnzcpy(szNumberOfElementaryPolygonsEsp,
                "Numero de poligonos elementales", MM_MAX_IDENTIFIER_SIZE);
    szNumberOfElementaryPolygonsEsp[1] = MM_u_WITH_ACUTE;
    szNumberOfElementaryPolygonsCat[13] = MM_i_WITH_ACUTE;
    szNumberOfElementaryPolygonsEsp[13] = MM_i_WITH_ACUTE;
}

const char *MM_pszLogFilename = nullptr;

// Loging
const char *Log(const char *pszMsg, int nLineNumber)
{
    FILE *f;

    if (MM_pszLogFilename == nullptr)
        return pszMsg;
    f = fopen(MM_pszLogFilename, "at");
    if (f == nullptr)
        return pszMsg;
    fprintf(f, "%d: %s\n", nLineNumber, pszMsg);
    fclose(f);
    return pszMsg;
}

static const char MM_EmptyString[] = {""};
#define MM_SetEndOfString (*MM_EmptyString)
static const char MM_BlankString[] = {" "};

// CREATING AN EXTENDED MIRAMON DBF
void MM_InitializeField(struct MM_FIELD *pField)
{
    memset(pField, '\0', sizeof(*pField));
    pField->FieldType = 'C';
    pField->GeoTopoTypeField = MM_NO_ES_CAMP_GEOTOPO;
}

struct MM_FIELD *MM_CreateAllFields(MM_EXT_DBF_N_FIELDS nFields)
{
    struct MM_FIELD *camp;
    MM_EXT_DBF_N_FIELDS i;

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
            sprintf(camp->FieldName, "CAMP%05u", (unsigned)(i + 1));
        else
            sprintf(camp->FieldName, "CM%u", (unsigned)(i + 1));
        camp->FieldType = 'C';
        camp->DecimalsIfFloat = 0;
        camp->BytesPerField = 50;
    }
    return bd_xp;
}

MM_BYTE MM_DBFFieldTypeToVariableProcessing(MM_BYTE tipus_camp_DBF)
{
    switch (tipus_camp_DBF)
    {
        case 'N':
            return MM_QUANTITATIVE_CONTINUOUS_FIELD;
        case 'D':
        case 'C':
        case 'L':
            return MM_CATEGORICAL_FIELD;
    }
    return MM_CATEGORICAL_FIELD;
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

static MM_BOOLEAN MM_is_field_name_lowercase(const char *cadena)
{
    const char *p;

    for (p = cadena; *p; p++)
    {
        if ((*p >= 'a' && *p <= 'z'))
            return TRUE;
    }
    return FALSE;
}

static MM_BOOLEAN
MM_Is_classical_DBF_field_name_or_lowercase(const char *cadena)
{
    const char *p;

    for (p = cadena; *p; p++)
    {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '_')
            ;
        else
            return FALSE;
    }
    if (cadena[0] == '_')
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
    GInt32 mida, j;

    mida = (GInt32)strlen(nom_camp);
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

static MM_ACUMULATED_BYTES_TYPE_DBF
MM_CalculateBytesExtendedFieldNames(const struct MM_DATA_BASE_XP *bd_xp)
{
    MM_ACUMULATED_BYTES_TYPE_DBF bytes_acumulats = 0;
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
        camp->AcumulatedBytes = bd_xp->BytesPerRecord;
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

static short int MM_return_common_valid_DBF_field_name_string(char *cadena)
{
    char *p;
    short int error_retornat = 0;

    if (!cadena)
        return 0;
    //strupr(cadena);
    for (p = cadena; *p; p++)
    {
        (*p) = (char)toupper(*p);
        if ((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
            ;
        else
        {
            *p = '_';
            error_retornat |= MM_FIELD_NAME_CHARACTER_INVALID;
        }
    }
    if (cadena[0] == '_')
    {
        cadena[0] = '0';
        error_retornat |= MM_FIELD_NAME_FIRST_CHARACTER_;
    }
    return error_retornat;
}

static short int MM_ReturnValidClassicDBFFieldName(char *cadena)
{
    size_t long_nom_camp;
    short int error_retornat = 0;

    long_nom_camp = strlen(cadena);
    if ((long_nom_camp < 1) ||
        (long_nom_camp >= MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF))
    {
        cadena[MM_MAX_LON_FIELD_NAME_DBF - 1] = '\0';
        error_retornat |= MM_FIELD_NAME_TOO_LONG;
    }
    error_retornat |= MM_return_common_valid_DBF_field_name_string(cadena);
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
                                                  char caracter)
{
    char *ptr;
    size_t i;

    if (!text)
        return nullptr;

    i = strlen(text);
    if ((ptr = calloc_function(i + 2)) == nullptr)
        return nullptr;

    *ptr = caracter;
    memcpy(ptr + 1, text, i + 1);
    return ptr;
}

static char *MM_SetSubIndexFieldNam(char *nom_camp, MM_EXT_DBF_N_FIELDS index,
                                    size_t ampladamax)
{
    char *NomCamp_SubIndex;
    char *_subindex;
    char subindex[15];
    size_t sizet_subindex;
    size_t sizet_nomcamp;

    NomCamp_SubIndex = calloc_function(ampladamax * sizeof(char));
    if (!NomCamp_SubIndex)
        return nullptr;

    strcpy(NomCamp_SubIndex, nom_camp);

    sprintf(subindex, sprintf_UINT64, (GUInt64)index);

    _subindex = MM_GiveNewStringWithCharacterInFront(subindex, '_');
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
    GUInt32 nRecords;
    if (!MMAdmDB->pMMBDXP || !MMAdmDB->pFExtDBF)
        return 0;

    // Updating number of features in features table
    fseek_function(MMAdmDB->pFExtDBF, MM_FIRST_OFFSET_to_N_RECORDS, SEEK_SET);

    if (MMAdmDB->pMMBDXP->nRecords > UINT32_MAX)
    {
        MMAdmDB->pMMBDXP->dbf_version = MM_MARCA_VERSIO_1_DBF_ESTESA;

        if (fwrite_function(&MMAdmDB->pMMBDXP->nRecords, 4, 1,
                            MMAdmDB->pFExtDBF) != 1)
            return 1;
    }
    else
    {
        MMAdmDB->pMMBDXP->dbf_version = MM_MARCA_DBASE4;

        nRecords = (GUInt32)MMAdmDB->pMMBDXP->nRecords;
        if (fwrite_function(&nRecords, 4, 1, MMAdmDB->pFExtDBF) != 1)
            return 1;
    }

    fseek_function(MMAdmDB->pFExtDBF, MM_SECOND_OFFSET_to_N_RECORDS, SEEK_SET);
    if (MMAdmDB->pMMBDXP->dbf_version == MM_MARCA_VERSIO_1_DBF_ESTESA)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        if (fwrite_function(((char *)(&MMAdmDB->pMMBDXP->nRecords)) + 4, 4, 1,
                            MMAdmDB->pFExtDBF) != 1)
            return 1;

        /* from 20 to 27 */
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 8, 1,
                            MMAdmDB->pFExtDBF) != 1)
            return 1;
    }
    else
    {
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 12, 1,
                            MMAdmDB->pFExtDBF) != 1)
            return 1;
    }

    return 0;
}

static MM_BOOLEAN MM_UpdateEntireHeader(struct MM_DATA_BASE_XP *data_base_XP)
{
    MM_BYTE variable_byte;
    MM_EXT_DBF_N_FIELDS i, j = 0;
    const size_t max_n_zeros = 11;
    char *zero;
    const MM_BYTE byte_zero = 0;
    char ModeLectura_previ[4] = "";
    MM_FIRST_RECORD_OFFSET_TYPE bytes_acumulats;
    MM_BYTE name_size;
    int estat;
    char nom_camp[MM_MAX_LON_FIELD_NAME_DBF];
    size_t retorn_fwrite;
    MM_BOOLEAN table_should_be_closed = FALSE;
    GUInt32 nRecords;

    if ((zero = calloc_function(max_n_zeros)) == nullptr)
        return FALSE;

    if (data_base_XP->pfDataBase == nullptr)
    {
        strcpy(ModeLectura_previ, data_base_XP->ReadingMode);
        strcpy(data_base_XP->ReadingMode, "wb");

        if ((data_base_XP->pfDataBase =
                 fopen_function(data_base_XP->szFileName,
                                data_base_XP->ReadingMode)) == nullptr)
        {
            free_function(zero);
            return FALSE;
        }

        table_should_be_closed = TRUE;
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

    // Writting header
    fseek_function(data_base_XP->pfDataBase, 0, SEEK_SET);

    /* Byte 0 */
    if (fwrite_function(&(data_base_XP->dbf_version), 1, 1,
                        data_base_XP->pfDataBase) != 1)
    {
        free_function(zero);
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
    if (data_base_XP->nRecords > UINT32_MAX)
    {
        if (fwrite_function(&data_base_XP->nRecords, 4, 1,
                            data_base_XP->pfDataBase) != 1)
            return FALSE;
    }
    else
    {
        nRecords = (GUInt32)data_base_XP->nRecords;
        if (fwrite_function(&nRecords, 4, 1, data_base_XP->pfDataBase) != 1)
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
                            sizeof(MM_ACUMULATED_BYTES_TYPE_DBF), 1,
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
        if (fwrite_function(((char *)(&data_base_XP->nRecords)) + 4, 4, 1,
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

                MM_strnzcpy(nom_temp, data_base_XP->pField[i].FieldName,
                            MM_MAX_LON_FIELD_NAME_DBF);
                MM_ReturnValidClassicDBFFieldName(nom_temp);
                nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF - 1] = '\0';
                if ((MM_CheckClassicFieldNameEqual(data_base_XP, nom_temp)) ==
                    TRUE)
                {
                    char *c;

                    c = MM_SetSubIndexFieldNam(
                        nom_temp, i, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);

                    j = 0;
                    while (MM_CheckClassicFieldNameEqual(data_base_XP, c) ==
                               TRUE &&
                           j < data_base_XP->nFields)
                    {
                        free_function(c);
                        c = MM_SetSubIndexFieldNam(
                            nom_temp, ++j, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                    }

                    strcpy(data_base_XP->pField[i].ClassicalDBFFieldName, c);
                    free_function(c);
                }
                else
                    strcpy(data_base_XP->pField[i].ClassicalDBFFieldName,
                           nom_temp);
            }
            j = (short)strlen(data_base_XP->pField[i].ClassicalDBFFieldName);

            retorn_fwrite =
                fwrite_function(&data_base_XP->pField[i].ClassicalDBFFieldName,
                                1, j, data_base_XP->pfDataBase);
            if (retorn_fwrite != (size_t)j)
            {
                free_function(zero);
                return FALSE;
            }

            name_size =
                MM_CalculateBytesExtendedFieldName(data_base_XP->pField + i);
            MM_EscriuOffsetNomEstesBD_XP(data_base_XP, i, bytes_acumulats);
            bytes_acumulats += name_size;
        }
        else
        {
            free_function(zero);
            return FALSE;
        }

        if (fwrite_function(zero, 1, 11 - j, data_base_XP->pfDataBase) !=
            11 - (size_t)j)
        {
            free_function(zero);
            return FALSE;
        }
        /* Byte 11, Field type */
        if (fwrite_function(&data_base_XP->pField[i].FieldType, 1, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            free_function(zero);
            return FALSE;
        }
        /* Bytes 12 to 15 --> Reserved */
        if (fwrite_function(&data_base_XP->pField[i].reserved_1, 4, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            free_function(zero);
            return FALSE;
        }
        /* Byte 16, or OFFSET_BYTESxCAMP_CAMP_CLASSIC --> BytesPerField */
        if (MM_ES_DBF_ESTESA(data_base_XP->dbf_version) &&
            data_base_XP->pField[i].FieldType == 'C')
        {
            if (fwrite_function((void *)&byte_zero, 1, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(&data_base_XP->pField[i].BytesPerField, 1, 1,
                                data_base_XP->pfDataBase) != 1)
            {
                free_function(zero);
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
                free_function(zero);
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(zero, 1, 1, data_base_XP->pfDataBase) != 1)
            {
                free_function(zero);
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
                free_function(zero);
                return FALSE;
            }
            /* Bytes from 21 to 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C
                                    in extended DBF */
            if (fwrite_function(&data_base_XP->pField[i].BytesPerField,
                                sizeof(MM_BYTES_PER_FIELD_TYPE_DBF), 1,
                                data_base_XP->pfDataBase) != 1)
            {
                free_function(zero);
                return FALSE;
            }

            /* Bytes from 25 to 30 --> Reserved */
            if (fwrite_function(&data_base_XP->pField[i].reserved_2[25 - 18],
                                30 - 25 + 1, 1, data_base_XP->pfDataBase) != 1)
            {
                free_function(zero);
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
                free_function(zero);
                return FALSE;
            }
        }
        /* Byte 31 --> MDX flag.    */
        if (fwrite_function(&data_base_XP->pField[i].MDX_field_flag, 1, 1,
                            data_base_XP->pfDataBase) != 1)
        {
            free_function(zero);
            return FALSE;
        }
    }

    free_function(zero);

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

    if (table_should_be_closed)
    {
        fclose_function(data_base_XP->pfDataBase);
        data_base_XP->pfDataBase = nullptr;
    }

    return TRUE;
} /* End of MM_UpdateEntireHeader() */

MM_BOOLEAN MM_CreateDBFFile(struct MM_DATA_BASE_XP *bd_xp,
                            const char *NomFitxer)
{
    if (MMIsEmptyString(NomFitxer))
        return TRUE;  // No file no error. Just continue
    MM_CheckDBFHeader(bd_xp);
    if (NomFitxer)
        strcpy(bd_xp->szFileName, NomFitxer);
    return MM_UpdateEntireHeader(bd_xp);
}

void MM_ReleaseMainFields(struct MM_DATA_BASE_XP *data_base_XP)
{
    MM_EXT_DBF_N_FIELDS i;
    size_t j;
    char **cadena;

    if (data_base_XP->pField)
    {
        for (i = 0; i < data_base_XP->nFields; i++)
        {
            for (j = 0; j < MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
            {
                cadena = data_base_XP->pField[i].Separator;
                if (cadena[j])
                {
                    free_function(cadena[j]);
                    cadena[j] = nullptr;
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
    unsigned short int ushort;
    MM_EXT_DBF_N_FIELDS nIField, j;
    MM_FIRST_RECORD_OFFSET_TYPE offset_primera_fitxa;
    MM_FIRST_RECORD_OFFSET_TYPE offset_fals = 0;
    MM_BOOLEAN incoherent_record_size = FALSE;
    MM_BYTE un_byte;
    MM_BYTES_PER_FIELD_TYPE_DBF bytes_per_camp;
    MM_BYTE tretze_bytes[13];
    MM_FIRST_RECORD_OFFSET_TYPE offset_possible;
    MM_BYTE n_queixes_estructura_incorrecta = 0;
    MM_FILE_OFFSET offset_reintent = 0;  // For retrying
    char cpg_file[MM_CPL_PATH_BUF_SIZE];
    char *pszDesc;
    char section[MM_MAX_LON_FIELD_NAME_DBF + 25];  // TAULA_PRINCIPAL:field_name
    GUInt32 nRecords;
    char *pszString;

    if (!szFileName)
        return 1;

    strcpy(pMMBDXP->szFileName, szFileName);
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
        fclose_function(pf);
        return 1;
    }

    if (1 != fread_function(&nRecords, 4, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (1 != fread_function(&offset_primera_fitxa, 2, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    pMMBDXP->year = (short)(1900 + variable_byte);
reintenta_lectura_per_si_error_CreaCampBD_XP:

    if (n_queixes_estructura_incorrecta > 0)
    {
        if (!MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
        {
            offset_fals = offset_primera_fitxa;
            if ((offset_primera_fitxa - 1) % 32)
            {
                for (offset_fals = (offset_primera_fitxa - 1);
                     !((offset_fals - 1) % 32); offset_fals--)
                    ;
            }
        }
    }
    else
        offset_reintent = ftell_function(pf);

    if (1 != fread_function(&ushort, 2, 1, pf) ||
        1 != fread_function(&(pMMBDXP->reserved_1), 2, 1, pf) ||
        1 != fread_function(&(pMMBDXP->transaction_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->encryption_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
    {
        memcpy(&pMMBDXP->nRecords, &nRecords, 4);
        memcpy(((char *)&pMMBDXP->nRecords) + 4, &pMMBDXP->dbf_on_a_LAN, 4);
    }
    else
        pMMBDXP->nRecords = nRecords;

    if (1 != fread_function(&(pMMBDXP->MDX_flag), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->CharSet), 1, 1, pf) ||
        1 != fread_function(&(pMMBDXP->reserved_2), 2, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    // Checking for a cpg file
    if (pMMBDXP->CharSet == 0)
    {
        FILE_TYPE *f_cpg;

        strcpy(cpg_file, pMMBDXP->szFileName);
        strcpy(cpg_file, reset_extension(cpg_file, "cpg"));
        f_cpg = fopen_function(cpg_file, "r");
        if (f_cpg)
        {
            char *p;
            size_t read_bytes;
            fseek_function(f_cpg, 0L, SEEK_SET);
            if (11 > (read_bytes = fread_function(local_message, 1, 10, f_cpg)))
            {
                local_message[read_bytes] = '\0';
                p = strstr(local_message, "UTF-8");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_UTF8_DBF;
                p = strstr(local_message, "UTF8");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_UTF8_DBF;
                p = strstr(local_message, "ISO-8859-1");
                if (p)
                    pMMBDXP->CharSet = MM_JOC_CARAC_ANSI_DBASE;
            }
            fclose_function(f_cpg);
        }
    }
    if (MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
    {
        memcpy(&pMMBDXP->FirstRecordOffset, &offset_primera_fitxa, 2);
        memcpy(((char *)&pMMBDXP->FirstRecordOffset) + 2, &pMMBDXP->reserved_2,
               2);

        if (n_queixes_estructura_incorrecta > 0)
            offset_fals = pMMBDXP->FirstRecordOffset;

        memcpy(&pMMBDXP->BytesPerRecord, &ushort, 2);
        memcpy(((char *)&pMMBDXP->BytesPerRecord) + 2, &pMMBDXP->reserved_1, 2);
    }
    else
    {
        pMMBDXP->FirstRecordOffset = offset_primera_fitxa;
        pMMBDXP->BytesPerRecord = ushort;
    }

    /* ====== Record structure ========================= */

    if (n_queixes_estructura_incorrecta > 0)
        pMMBDXP->nFields = (MM_EXT_DBF_N_FIELDS)(((offset_fals - 1) - 32) / 32);
    else
    {
        MM_ACUMULATED_BYTES_TYPE_DBF bytes_acumulats = 1;

        pMMBDXP->nFields = 0;

        fseek_function(pf, 0, SEEK_END);
        if (32 < (ftell_function(pf) - 1))
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
                    free(pMMBDXP->pField);
                    fclose_function(pf);
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
        pMMBDXP->pField = MM_CreateAllFields(pMMBDXP->nFields);
        if (!pMMBDXP->pField)
        {
            fclose_function(pf);
            return 1;
        }
    }
    else
        pMMBDXP->pField = nullptr;

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
            free(pMMBDXP->pField);
            fclose_function(pf);
            return 1;
        }

        if (pMMBDXP->pField[nIField].FieldType == 'F')
            pMMBDXP->pField[nIField].FieldType = 'N';

        pMMBDXP->pField[nIField]
            .FieldName[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF - 1] = '\0';
        if (EQUAL(pMMBDXP->pField[nIField].FieldName,
                  szMMNomCampIdGraficDefecte))
            pMMBDXP->IdGraficField = nIField;

        if (pMMBDXP->pField[nIField].BytesPerField == 0)
        {
            if (!MM_ES_DBF_ESTESA(pMMBDXP->dbf_version))
            {
                free(pMMBDXP->pField);
                fclose_function(pf);
                return 1;
            }
            if (pMMBDXP->pField[nIField].FieldType != 'C')
            {
                free(pMMBDXP->pField);
                fclose_function(pf);
                return 1;
            }

            memcpy(&pMMBDXP->pField[nIField].BytesPerField,
                   (char *)(&pMMBDXP->pField[nIField].reserved_2) + 3,
                   sizeof(MM_BYTES_PER_FIELD_TYPE_DBF));
        }

        if (nIField)
            pMMBDXP->pField[nIField].AcumulatedBytes =
                (pMMBDXP->pField[nIField - 1].AcumulatedBytes +
                 pMMBDXP->pField[nIField - 1].BytesPerField);
        else
            pMMBDXP->pField[nIField].AcumulatedBytes = 1;

        for (j = 0; j < MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
        {
            pMMBDXP->pField[nIField].Separator[j] = nullptr;

            if (pszRelFile)
            {
                sprintf(section, "TAULA_PRINCIPAL:%s",
                        pMMBDXP->pField[nIField].FieldName);
                pszDesc = MMReturnValueFromSectionINIFile(pszRelFile, section,
                                                          "descriptor_eng");
                if (pszDesc)
                {
                    MM_strnzcpy(pMMBDXP->pField[nIField].FieldDescription[j],
                                pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                    free_function(pszDesc);
                }
                else
                {
                    sprintf(section, "TAULA_PRINCIPAL:%s",
                            pMMBDXP->pField[nIField].FieldName);
                    pszDesc = MMReturnValueFromSectionINIFile(
                        pszRelFile, section, "descriptor");
                    if (pszDesc)
                        MM_strnzcpy(
                            pMMBDXP->pField[nIField].FieldDescription[j],
                            pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                    pMMBDXP->pField[nIField].FieldDescription[j][0] = 0;
                    free_function(pszDesc);
                }
            }
        }
    }

    if (!pMMBDXP->nFields)
    {
        if (pMMBDXP->BytesPerRecord)
            incoherent_record_size = TRUE;
    }
    else if (pMMBDXP->pField[pMMBDXP->nFields - 1].BytesPerField +
                 pMMBDXP->pField[pMMBDXP->nFields - 1].AcumulatedBytes >
             pMMBDXP->BytesPerRecord)
        incoherent_record_size = TRUE;
    if (incoherent_record_size)
    {
        if (n_queixes_estructura_incorrecta == 0)
        {
            incoherent_record_size = FALSE;
            fseek_function(pf, offset_reintent, SEEK_SET);
            n_queixes_estructura_incorrecta++;
            goto reintenta_lectura_per_si_error_CreaCampBD_XP;
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
                MM_strnzcpy(pMMBDXP->pField[nIField].ClassicalDBFFieldName,
                            pMMBDXP->pField[nIField].FieldName,
                            MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                fseek_function(pf, offset_nom_camp, SEEK_SET);
                if (1 != fread_function(pMMBDXP->pField[nIField].FieldName,
                                        mida_nom, 1, pf))
                {
                    free(pMMBDXP->pField);
                    fclose_function(pf);
                    return 1;
                }
                pMMBDXP->pField[nIField].FieldName[mida_nom] = '\0';

                // All field names to UTF-8
                if (pMMBDXP->CharSet == MM_JOC_CARAC_ANSI_DBASE)
                {
                    pszString =
                        CPLRecode_function(pMMBDXP->pField[nIField].FieldName,
                                           CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    MM_strnzcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                                MM_MAX_LON_FIELD_NAME_DBF);
                    CPLFree_function(pszString);
                }
                else if (pMMBDXP->CharSet == MM_JOC_CARAC_OEM850_DBASE)
                {
                    MM_oemansi(pMMBDXP->pField[nIField].FieldName);
                    pszString =
                        CPLRecode_function(pMMBDXP->pField[nIField].FieldName,
                                           CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    MM_strnzcpy(pMMBDXP->pField[nIField].FieldName, pszString,
                                MM_MAX_LON_FIELD_NAME_DBF - 1);
                    CPLFree_function(pszString);
                }
            }
        }
    }

    pMMBDXP->IdEntityField = MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    return 0;
}  // End of MM_ReadExtendedDBFHeaderFromFile()

void MM_ReleaseDBFHeader(struct MM_DATA_BASE_XP *data_base_XP)
{
    if (data_base_XP)
    {
        MM_ReleaseMainFields(data_base_XP);
        free_function(data_base_XP);
    }
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
            sprintf(camp->FieldName + strlen(camp->FieldName) - 1, "%u", i);
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
                sprintf(camp->FieldName + strlen(camp->FieldName) - 2, "%u", i);
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
                    sprintf(camp->FieldName + strlen(camp->FieldName) - 3, "%u",
                            i);
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
        //if (camp->FieldDescription[0] + strlen(camp->FieldDescription[0]))
        sprintf(camp->FieldDescription[0] + strlen(camp->FieldDescription[0]),
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
        sprintf(camp->FieldDescription[0] + strlen(camp->FieldDescription[0]),
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
    char *(cadena_final[MM_NUM_IDIOMES_MD_MULTIDIOMA]),
    const char *const(cadena_inicial[MM_NUM_IDIOMES_MD_MULTIDIOMA]))
{
    size_t i;

    for (i = 0; i < MM_NUM_IDIOMES_MD_MULTIDIOMA; i++)
    {
        if (cadena_inicial[i])
        {
            if (nullptr == (cadena_final[i] = strdup(cadena_inicial[i])))
                return 1;
        }
        else
            cadena_final[i] = nullptr;
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

char *MM_strnzcpy(char *dest, const char *src, size_t maxlen)
{
    size_t i = 0;
    if (!src || maxlen == 0)
    {
        *dest = '\0';
        return dest;
    }

    for (; i < maxlen - 1 && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';

    return dest;
}
/*
char *MM_strnzcpy(char *dest, const char *src, size_t maxlen)
{
    size_t i;
    if (!src)
    {
        *dest = '\0';
        return dest;
    }

    if (!maxlen)
        i = 0;
    else
        strncpy(dest, src, i = maxlen - 1);

    dest[i] = '\0';
    return dest;
}*/

char *MM_oemansi_n(char *szcadena, size_t n_bytes)
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
    if (n_bytes == USHRT_MAX)
    {
        for (punter_bait = (unsigned char *)szcadena; *punter_bait;
             punter_bait++)
        {
            if (*punter_bait > 127)
                *punter_bait = t_oemansi[*punter_bait - 128];
        }
    }
    else
    {
        for (u_i = 0, punter_bait = (unsigned char *)szcadena; u_i < n_bytes;
             punter_bait++, u_i++)
        {
            if (*punter_bait > 127)
                *punter_bait = t_oemansi[*punter_bait - 128];
        }
    }
    return szcadena;
}

char *MM_oemansi(char *szcadena)
{
    return MM_oemansi_n(szcadena, USHRT_MAX);
}

static MM_BOOLEAN MM_FillFieldDB_XP(struct MM_FIELD *camp,
                                    const char *FieldName,
                                    const char *FieldDescription,
                                    char FieldType,
                                    MM_BYTES_PER_FIELD_TYPE_DBF BytesPerField,
                                    MM_BYTE DecimalsIfFloat)
{
    char nom_temp[MM_MAX_LON_FIELD_NAME_DBF];
    int retorn_valida_nom_camp;

    if (FieldName)
    {
        retorn_valida_nom_camp = MM_ISExtendedNameBD_XP(FieldName);
        if (retorn_valida_nom_camp == MM_DBF_NAME_NO_VALID)
            return FALSE;
        MM_strnzcpy(camp->FieldName, FieldName, MM_MAX_LON_FIELD_NAME_DBF);

        if (retorn_valida_nom_camp == MM_VALID_EXTENDED_DBF_NAME)
        {
            MM_CalculateBytesExtendedFieldName(camp);
            MM_strnzcpy(nom_temp, FieldName, MM_MAX_LON_FIELD_NAME_DBF);
            MM_ReturnValidClassicDBFFieldName(nom_temp);
            nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF] = '\0';
            MM_strnzcpy(camp->ClassicalDBFFieldName, nom_temp,
                        MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
        }
    }

    if (FieldDescription)
        strcpy(camp->FieldDescription[0], FieldDescription);
    else
        strcpy(camp->FieldDescription[0], "\0");
    camp->FieldType = FieldType;
    camp->DecimalsIfFloat = DecimalsIfFloat;
    camp->BytesPerField = BytesPerField;
    return TRUE;
}

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                        MM_BYTE n_decimals)
{
    MM_EXT_DBF_N_FIELDS i_camp = 0;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
                      szInternalGraphicIdentifierEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNVertexsDefecte,
                      szNumberOfVerticesEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampPerimetreDefecte,
                      szPerimeterOfThePolygonEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_PERIMETRE;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampAreaDefecte,
                      szAreaOfThePolygonEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF,
                      n_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_AREA;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNArcsDefecte,
                      szNumberOfArcsEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_ARCS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNPoligonsDefecte,
                      szNumberOfElementaryPolygonsEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_POLIG;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstArcFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp,
                                    MM_BYTE n_decimals)
{
    MM_EXT_DBF_N_FIELDS i_camp;

    i_camp = 0;
    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
                      szInternalGraphicIdentifierEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNVertexsDefecte,
                      szNumberOfVerticesEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampLongitudArcDefecte,
                      szLenghtOfAarcEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF,
                      n_decimals);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_LONG_ARC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNodeIniDefecte,
                      szInitialNodeEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_NODE_INI;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampNodeFiDefecte,
                      szFinalNodeEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_NODE_FI;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp)
{
    MM_EXT_DBF_N_FIELDS i_camp;

    i_camp = 0;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
                      szInternalGraphicIdentifierEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampArcsANodeDefecte,
                      szNumberOfArcsToNodeEng, 'N', MM_MAX_AMPLADA_CAMP_N_DBF,
                      0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ARCS_A_NOD;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampTipusNodeDefecte,
                      szNodeTypeEng, 'N', 1, 0);
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_TIPUS_NODE;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstPointFieldsDB_XP(struct MM_DATA_BASE_XP *bd_xp)
{
    size_t i_camp = 0;

    MM_FillFieldDB_XP(bd_xp->pField + i_camp, szMMNomCampIdGraficDefecte,
                      szInternalGraphicIdentifierEng, 'N',
                      MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->IdGraficField = 0;
    (bd_xp->pField + i_camp)->GeoTopoTypeField = (MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    return i_camp;
}

static int MM_SprintfDoubleWidth(char *cadena, int amplada, int n_decimals,
                                 double valor_double,
                                 MM_BOOLEAN *Error_sprintf_n_decimals)
{
#define VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E 1E+17
#define VALOR_MASSA_PETIT_PER_IMPRIMIR_f 1E-17
    char cadena_treball[MM_CARACTERS_DOUBLE + 1];
    int retorn_printf;

    if (MM_IsNANDouble(valor_double))
    {
        if (amplada < 3)
        {
            *cadena = *MM_EmptyString;
            return EOF;
        }
        return sprintf(cadena, "NAN");
    }
    if (MM_IsDoubleInfinit(valor_double))
    {
        if (amplada < 3)
        {
            *cadena = *MM_EmptyString;
            return EOF;
        }
        return sprintf(cadena, "INF");
    }

    *Error_sprintf_n_decimals = FALSE;
    if (valor_double == 0)
    {
        retorn_printf =
            sprintf(cadena_treball, "%*.*f", amplada, n_decimals, valor_double);
        if (retorn_printf == EOF)
        {
            *cadena = *MM_EmptyString;
            return retorn_printf;
        }

        if (retorn_printf > amplada)
        {
            int escurcament = retorn_printf - amplada;
            if (escurcament > n_decimals)
            {
                *cadena = *MM_EmptyString;
                return EOF;
            }
            *Error_sprintf_n_decimals = TRUE;
            n_decimals = n_decimals - escurcament;
            retorn_printf =
                sprintf(cadena, "%*.*f", amplada, n_decimals, valor_double);
        }
        else
            strcpy(cadena, cadena_treball);

        return retorn_printf;
    }

    if (valor_double > VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E ||
        valor_double < -VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E ||
        (valor_double < VALOR_MASSA_PETIT_PER_IMPRIMIR_f &&
         valor_double > -VALOR_MASSA_PETIT_PER_IMPRIMIR_f))
    {
        retorn_printf =
            sprintf(cadena_treball, "%*.*E", amplada, n_decimals, valor_double);
        if (retorn_printf == EOF)
        {
            *cadena = *MM_EmptyString;
            return retorn_printf;
        }
        if (retorn_printf > amplada)
        {
            int escurcament = retorn_printf - amplada;
            if (escurcament > n_decimals)
            {
                *cadena = *MM_EmptyString;
                return EOF;
            }
            *Error_sprintf_n_decimals = TRUE;
            n_decimals = n_decimals - escurcament;
            retorn_printf =
                sprintf(cadena, "%*.*E", amplada, n_decimals, valor_double);
        }
        else
            strcpy(cadena, cadena_treball);

        return retorn_printf;
    }

    retorn_printf =
        sprintf(cadena_treball, "%*.*f", amplada, n_decimals, valor_double);
    if (retorn_printf == EOF)
    {
        *cadena = *MM_EmptyString;
        return retorn_printf;
    }

    if (retorn_printf > amplada)
    {
        int escurcament = retorn_printf - amplada;
        if (escurcament > n_decimals)
        {
            *cadena = *MM_EmptyString;
            return EOF;
        }
        *Error_sprintf_n_decimals = TRUE;
        n_decimals = n_decimals - escurcament;
        retorn_printf =
            sprintf(cadena, "%*.*f", amplada, n_decimals, valor_double);
    }
    else
        strcpy(cadena, cadena_treball);

    return retorn_printf;

#undef VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E
#undef VALOR_MASSA_PETIT_PER_IMPRIMIR_f
}  // End of MM_SprintfDoubleWidth()

static MM_BOOLEAN MM_EmptyString_function(const char *cadena)
{
    char *ptr;

    for (ptr = (char *)cadena; *ptr; ptr++)
        if (*ptr != ' ' && *ptr != '\t')
            return FALSE;

    return TRUE;
}

int MM_SecureCopyStringFieldValue(char **pszStringDst, const char *pszStringSrc,
                                  MM_EXT_DBF_N_FIELDS *nStringCurrentLenght)
{
    if (!pszStringSrc)
    {
        if (1 >= *nStringCurrentLenght)
        {
            (*pszStringDst) = realloc_function(*pszStringDst, 2);
            if (!(*pszStringDst))
                return 1;
            *nStringCurrentLenght = (MM_EXT_DBF_N_FIELDS)2;
        }
        strcpy(*pszStringDst, "\0");
        return 0;
    }

    if (strlen(pszStringSrc) >= *nStringCurrentLenght)
    {
        (*pszStringDst) =
            realloc_function(*pszStringDst, strlen(pszStringSrc) + 1);
        if (!(*pszStringDst))
            return 1;
        *nStringCurrentLenght = (MM_EXT_DBF_N_FIELDS)(strlen(pszStringSrc) + 1);
    }
    strcpy(*pszStringDst, pszStringSrc);
    return 0;
}

// This function assumes that all the file is saved in disk and closed.
int MM_ChangeDBFWidthField(struct MM_DATA_BASE_XP *data_base_XP,
                           MM_EXT_DBF_N_FIELDS nIField,
                           MM_BYTES_PER_FIELD_TYPE_DBF nNewWidth,
                           MM_BYTE nNewPrecision,
                           MM_BYTE que_fer_amb_reformatat_decimals)
{
    char *record, *whites = nullptr;
    MM_BYTES_PER_FIELD_TYPE_DBF l_glop1, l_glop2, i_glop2;
    MM_EXT_DBF_N_RECORDS nfitx, i_reg;
    int canvi_amplada;  // change width
    GInt32 j;
    MM_EXT_DBF_N_FIELDS i_camp;
    size_t retorn_fwrite;
    int retorn_TruncaFitxer;

    MM_BOOLEAN error_sprintf_n_decimals = FALSE;

    canvi_amplada = nNewWidth - data_base_XP->pField[nIField].BytesPerField;

    if (data_base_XP->nRecords != 0)
    {
        l_glop1 = data_base_XP->pField[nIField].AcumulatedBytes;
        i_glop2 = l_glop1 + data_base_XP->pField[nIField].BytesPerField;
        if (nIField == data_base_XP->nFields - 1)
            l_glop2 = 0;
        else
            l_glop2 = data_base_XP->BytesPerRecord -
                      data_base_XP->pField[nIField + 1].AcumulatedBytes;

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

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#endif
        for (i_reg = (canvi_amplada < 0 ? 0 : nfitx - 1); TRUE;)
#ifdef _MSC_VER
#pragma warning(default : 4127)
#endif
        {
            if (0 != fseek_function(data_base_XP->pfDataBase,
                                    data_base_XP->FirstRecordOffset +
                                        (MM_FILE_OFFSET)i_reg *
                                            data_base_XP->BytesPerRecord,
                                    SEEK_SET))
            {
                if (whites)
                    free_function(whites);
                free_function(record);
                return 1;
            }

            if (1 != fread_function(record, data_base_XP->BytesPerRecord, 1,
                                    data_base_XP->pfDataBase))
            {
                if (whites)
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
                if (whites)
                    free_function(whites);
                free_function(record);
                return 1;
            }

            if (1 !=
                fwrite_function(record, l_glop1, 1, data_base_XP->pfDataBase))
            {
                if (whites)
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
                        if (whites)
                            free_function(whites);
                        free_function(record);
                        return 1;
                    }
                    break;
                case 'N':
                    if (nNewPrecision ==
                            data_base_XP->pField[nIField].DecimalsIfFloat ||
                        que_fer_amb_reformatat_decimals ==
                            MM_NOU_N_DECIMALS_NO_APLICA)
                        que_fer_amb_reformatat_decimals =
                            MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS;
                    else if (que_fer_amb_reformatat_decimals ==
                             MM_PREGUNTA_SI_APLICAR_NOU_N_DECIM)
                        que_fer_amb_reformatat_decimals =
                            MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS;

                    if (que_fer_amb_reformatat_decimals ==
                        MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS)
                    {
                        if (canvi_amplada >= 0)
                        {
                            if (1 !=
                                    fwrite_function(whites, canvi_amplada, 1,
                                                    data_base_XP->pfDataBase) ||
                                1 != fwrite_function(
                                         record + l_glop1,
                                         data_base_XP->pField[nIField]
                                             .BytesPerField,
                                         1, data_base_XP->pfDataBase))
                            {
                                if (whites)
                                    free_function(whites);
                                free_function(record);
                                return 1;
                            }
                        }
                        else if (canvi_amplada < 0)
                        {

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#endif
                            for (j = (GInt32)(l_glop1 +
                                              (data_base_XP->pField[nIField]
                                                   .BytesPerField -
                                               1));
                                 TRUE; j--)
#ifdef _MSC_VER
#pragma warning(default : 4127)
#endif
                            {
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

                            retorn_fwrite =
                                fwrite_function(record + j, nNewWidth, 1,
                                                data_base_XP->pfDataBase);
                            if (1 != retorn_fwrite)
                            {
                                if (whites)
                                    free_function(whites);
                                free_function(record);
                                return 1;
                            }
                        }
                    }
                    else  // MM_APLICAR_NOU_N_DECIMALS
                    {
                        double valor;
                        char *sz_valor;

                        if ((sz_valor = calloc_function(
                                 max_function(nNewWidth,
                                              data_base_XP->pField[nIField]
                                                  .BytesPerField) +
                                 1)) ==
                            nullptr)  // Sumo 1 per poder posar-hi el \0
                        {
                            if (whites)
                                free_function(whites);
                            free_function(record);
                            return 1;
                        }
                        memcpy(sz_valor, record + l_glop1,
                               data_base_XP->pField[nIField].BytesPerField);
                        sz_valor[data_base_XP->pField[nIField].BytesPerField] =
                            0;

                        if (!MM_EmptyString_function(sz_valor))
                        {
                            if (sscanf(sz_valor, "%lf", &valor) != 1)
                                memset(
                                    sz_valor, *MM_BlankString,
                                    max_function(nNewWidth,
                                                 data_base_XP->pField[nIField]
                                                     .BytesPerField));
                            else
                            {
                                MM_SprintfDoubleWidth(
                                    sz_valor, nNewWidth, nNewPrecision, valor,
                                    &error_sprintf_n_decimals);
                            }

                            retorn_fwrite =
                                fwrite_function(sz_valor, nNewWidth, 1,
                                                data_base_XP->pfDataBase);
                            if (1 != retorn_fwrite)
                            {
                                if (whites)
                                    free_function(whites);
                                free_function(record);
                                free_function(sz_valor);
                                return 1;
                            }
                        }
                        else
                        {
                            memset(sz_valor, *MM_BlankString, nNewWidth);
                            retorn_fwrite =
                                fwrite_function(sz_valor, nNewWidth, 1,
                                                data_base_XP->pfDataBase);
                            if (1 != retorn_fwrite)
                            {
                                if (whites)
                                    free_function(whites);
                                free_function(record);
                                free_function(sz_valor);
                                return 1;
                            }
                        }
                        free_function(sz_valor);
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
                    if (whites)
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

        if (whites)
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
            data_base_XP->pField[i_camp].AcumulatedBytes += canvi_amplada;
    }
    data_base_XP->pField[nIField].DecimalsIfFloat = nNewPrecision;

    //DonaData(&(data_base_XP->day), &(data_base_XP->month), &(data_base_XP->year));

    if ((MM_UpdateEntireHeader(data_base_XP)) == FALSE)
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

    tipus = MM_ARC_HEIGHT_TYPE(pZDescription->nZCount);
    n_alcada = MM_ARC_N_HEIGHTS(pZDescription->nZCount);
    if (n_vrt == 0 || n_alcada == 0)
        return 0;

    if (tipus == MM_ARC_HEIGHT_FOR_EACH_VERTEX)
        n_h_total = (MM_N_HEIGHT_TYPE)n_vrt * n_alcada;
    else
        n_h_total = n_alcada;

    if (n_h_total <= MM_N_ALCADA_LOCAL)
        palcada = local_CinquantaAlcades;
    else
    {
        if (MMCheckSize_t(n_vrt * sizeof(double) * n_alcada, 1))
            return 1;
        if (nullptr == (palcada = alcada = calloc_function(
                            (size_t)n_vrt * sizeof(double) * n_alcada)))
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
                                                   size_t l_cadena)
{
    int longitud_cadena = (int)l_cadena;
    if (longitud_cadena-- == 0)
        return punter;

    if (punter[longitud_cadena] != ' ' && punter[longitud_cadena] != '\t')
        return punter;
    longitud_cadena--;

    while (longitud_cadena > -1)
    {
        if (punter[longitud_cadena] != ' ' && punter[longitud_cadena] != '\t')
        {
            break;
        }
        longitud_cadena--;
    }

    punter[++longitud_cadena] = '\0';
    return punter;
}

char *MM_RemoveInitial_and_FinalQuotationMarks(char *cadena)
{
    char *ptr1, *ptr2;
    char cometa = '"';

    if (*cadena == cometa)
    {
        ptr1 = cadena;
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
    return cadena;
} /* End of MM_RemoveInitial_and_FinalQuotationMarks() */

char *MM_RemoveLeadingWhitespaceOfString(char *cadena)
{
    char *ptr;
    char *ptr2;

    if (cadena == nullptr)
        return cadena;

    for (ptr = cadena; *ptr && (*ptr == ' ' || *ptr == '\t'); ptr++)
        continue;

    if (ptr != cadena)
    {
        ptr2 = cadena;
        while (*ptr)
        {
            *ptr2 = *ptr;
            ptr2++;
            ptr++;
        }
        *ptr2 = 0;
    }
    return cadena;
}

char *MM_RemoveWhitespacesFromEndOfString(char *str)
{
    const char *s;

    if (str == nullptr)
        return str;

    for (s = str; *s; ++s)
        continue;
    return MM_l_RemoveWhitespacesFromEndOfString(str, (s - str));
}

struct MM_ID_GRAFIC_MULTIPLE_RECORD *
MMCreateExtendedDBFIndex(FILE_TYPE *f, MM_EXT_DBF_N_RECORDS nNumberOfRecords,
                         MM_FIRST_RECORD_OFFSET_TYPE offset_1era,
                         MM_ACUMULATED_BYTES_TYPE_DBF bytes_per_fitxa,
                         MM_ACUMULATED_BYTES_TYPE_DBF bytes_acumulats_id_grafic,
                         MM_BYTES_PER_FIELD_TYPE_DBF bytes_id_grafic,
                         MM_BOOLEAN *isListField, MM_EXT_DBF_N_RECORDS *nMaxN)
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

    if (MMCheckSize_t(nNumberOfRecords * sizeof(*id), 1))
        return nullptr;
    if (nullptr == (id = (struct MM_ID_GRAFIC_MULTIPLE_RECORD *)calloc_function(
                        (size_t)nNumberOfRecords * sizeof(*id))))
        return nullptr;

    if (MMCheckSize_t(bytes_id_grafic + 1, 1))
        return nullptr;
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

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#endif
    while (TRUE)
#ifdef _MSC_VER
#pragma warning(default : 4127)
#endif
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
