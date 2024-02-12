/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau, a.pau@creaf.uab.cat, based on the MiraMon codes, 
 *           mainly written by Xavier Pons, Joan Masó, Abel Pau, Núria Julià,
 *           Xavier Calaf, Lluís Pesquer and Alaitz Zabala, from CREAF and
 *           Universitat Autònoma de Barcelona. For a complete list of
 *           contributors: https://www.miramon.cat/USA/QuiSom.htm
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
#include "ogr_api.h"    // For CPL_C_START
#include "mm_gdal_functions.h"      // For MM_strnzcpy()
#include "mm_wrlayr.h" // For calloc_function(),...
#else
#include "CmptCmp.h"
#include "mm_gdal\mm_gdal_functions.h"      // For MM_strnzcpy()
#include "mm_gdal\mm_wrlayr.h" // For calloc_function(),...
#endif

#include "cpl_string.h"  // For CPL_ENC_UTF8

#ifdef GDAL_COMPILATION
CPL_C_START // Necessary for compiling in GDAL project
#endif

static int MM_max(int a, int b) {
    return (a > b) ? a : b;
}

// CREATING AN EXTENDED MIRAMON DBF
void MM_InitializeField(struct MM_CAMP *camp)
{
    memset(camp, '\0', sizeof(*camp));
    camp->TipusDeCamp='C';
    camp->TipusCampGeoTopo=MM_NO_ES_CAMP_GEOTOPO;
}

struct MM_CAMP *MM_CreateAllFields(int ncamps)
{
struct MM_CAMP *camp;
MM_EXT_DBF_N_FIELDS i;

    if ((camp=calloc_function(ncamps*sizeof(*camp)))==nullptr)
        return nullptr;
    
    for (i=0; i<(size_t)ncamps; i++)
        MM_InitializeField(camp+i);
    return camp;
}

static struct MM_BASE_DADES_XP * MM_CreateEmptyHeader(MM_EXT_DBF_N_FIELDS n_camps)
{
struct MM_BASE_DADES_XP *base_dades_XP;

    if ((base_dades_XP = (struct MM_BASE_DADES_XP *)
                        calloc_function(sizeof(struct MM_BASE_DADES_XP))) == nullptr)
        return nullptr;
    
    if (n_camps==0)
    {
        ;
    }
    else
    {
        base_dades_XP->Camp = (struct MM_CAMP *)MM_CreateAllFields(n_camps);
        if (!base_dades_XP->Camp)
        {
            free_function (base_dades_XP);
            return nullptr;
        }
    }
    base_dades_XP->ncamps=n_camps;
    return base_dades_XP;
}

struct MM_BASE_DADES_XP * MM_CreateDBFHeader(MM_EXT_DBF_N_FIELDS n_camps,
                        MM_BYTE charset)
{
struct MM_BASE_DADES_XP *bd_xp;
struct MM_CAMP *camp;
MM_EXT_DBF_N_FIELDS i;

    if (nullptr==(bd_xp=MM_CreateEmptyHeader(n_camps)))
        return nullptr;

    bd_xp->JocCaracters=charset;
    
    strcpy(bd_xp->ModeLectura,"a+b");

    bd_xp->CampIdGrafic=n_camps;
    bd_xp->CampIdEntitat=MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    bd_xp->versio_dbf=(MM_BYTE)((n_camps>MM_MAX_N_CAMPS_DBF_CLASSICA)?MM_MARCA_VERSIO_1_DBF_ESTESA:MM_MARCA_DBASE4);

    for(i=0, camp=bd_xp->Camp; i<n_camps; i++, camp++)
    {
        MM_InitializeField(camp);
        if (i<99999)
            sprintf(camp->NomCamp, "CAMP%05u", (unsigned)(i+1));
        else 
            sprintf(camp->NomCamp, "CM%u", (unsigned)(i+1));
        camp->TipusDeCamp='C';
        camp->DecimalsSiEsFloat=0;
        camp->BytesPerCamp=50;
    }
    return bd_xp;
}

MM_BYTE MM_DBFFieldTypeToVariableProcessing(MM_BYTE tipus_camp_DBF)
{
    switch(tipus_camp_DBF)
    {
        case 'N':
            return MM_CAMP_QUANTITATIU_CONTINU;
        case 'D':
        case 'C':
        case 'L':
            return MM_CAMP_CATEGORIC;
    }
    return MM_CAMP_CATEGORIC;
}

static MM_BYTE MM_GetDefaultDesiredDBFFieldWidth(const struct MM_CAMP *camp)
{
size_t a, b, c, d, e;

    b=strlen(camp->NomCamp);
    c=strlen(camp->DescripcioCamp[0]);

    if (camp->TipusDeCamp=='D')
    {
        d=(b>c?b:c);
        a=(size_t)camp->BytesPerCamp+2;
        return (MM_BYTE)(a>d?a:d);
    }
    a=camp->BytesPerCamp;
    d=(unsigned int)(b>c?b:c);
    e=(a>d?a:d);
    return (MM_BYTE)(e<80?e:80);
}

static MM_BOOLEAN MM_is_field_name_lowercase(const char *cadena)
{
const char *p;

    for (p=cadena; *p; p++)
    {
        if ((*p>='a' && *p<='z'))
            return TRUE;
    }
    return FALSE;
}

static MM_BOOLEAN MM_is_classical_field_DBF_name_or_lowercase(const char *cadena)
{
const char *p;

    for (p=cadena; *p; p++)
    {
        if ((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_')
            ;
        else
            return FALSE;
    }
    if (cadena[0]=='_')
        return FALSE;
    return TRUE;
}

static MM_BOOLEAN MM_Is_character_valid_for_extended_DBF_field_name(int valor, int *valor_substitut)
{
    if(valor_substitut)
    {
        switch(valor)
        {
            case 32:
                *valor_substitut='_';
                return FALSE;
            case 91:
                *valor_substitut='(';
                return FALSE;
            case 93:
                *valor_substitut=')';
                return FALSE;
            case 96:
                *valor_substitut='\'';
                return FALSE;
            case 127:
                *valor_substitut='_';
                return FALSE;
            case 168:
                *valor_substitut='-';
                return FALSE;
        }
    }
    else
    {
        if (valor<32 || valor==91 || valor==93 || valor==96 || valor==127 || valor==168)
           return FALSE;
    }
    return TRUE;
}

static int MM_ISExtendedNameBD_XP(const char *nom_camp)
{
GInt32 mida, j;

    mida=(GInt32)strlen(nom_camp);
    if(mida>=MM_MAX_LON_FIELD_NAME_DBF)
        return MM_NOM_DBF_NO_VALID;

    for(j=0;j<mida;j++)
    {
        if(!MM_Is_character_valid_for_extended_DBF_field_name((unsigned char)nom_camp[j], nullptr))
            return MM_NOM_DBF_NO_VALID;
    }

    if(mida>=MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF)
        return MM_NOM_DBF_ESTES_I_VALID;

    if(!MM_is_classical_field_DBF_name_or_lowercase(nom_camp))
        return MM_NOM_DBF_ESTES_I_VALID;
    
    if(MM_is_field_name_lowercase(nom_camp))
        return MM_NOM_DBF_MINUSCULES_I_VALID;
    
    return MM_NOM_DBF_CLASSICA_I_VALID;
}

static MM_BYTE MM_CalculateBytesExtendedFieldName(struct MM_CAMP *camp)
{
    camp->reservat_2[MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES]=(MM_BYTE)strlen(camp->NomCamp);
    return MM_DonaBytesNomEstesCamp(camp);
}

static MM_TIPUS_BYTES_ACUMULATS_DBF MM_CalculateBytesExtendedFieldNames(const struct MM_BASE_DADES_XP *bd_xp)
{
MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats=0;
MM_EXT_DBF_N_FIELDS i_camp;

    for(i_camp=0;i_camp<bd_xp->ncamps;i_camp++)
    {
        if (MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(bd_xp->Camp[i_camp].NomCamp))
            bytes_acumulats+=MM_CalculateBytesExtendedFieldName(bd_xp->Camp+i_camp);
    }

    return bytes_acumulats;
}

static MM_FIRST_RECORD_OFFSET_TYPE MM_CalculateBytesFirstRecordOffset(struct MM_BASE_DADES_XP *bd_xp)
{
    if(bd_xp)
        return (32+32*bd_xp->ncamps+1+MM_CalculateBytesExtendedFieldNames(bd_xp));
    return 0;    
}

static void MM_CheckDBFHeader(struct MM_BASE_DADES_XP * bd_xp)
{
struct MM_CAMP *camp;
MM_EXT_DBF_N_FIELDS i;
MM_BOOLEAN cal_DBF_estesa=FALSE;

    bd_xp->BytesPerFitxa=1;
    for(i=0, camp=bd_xp->Camp; i<bd_xp->ncamps; i++, camp++)
    {
        camp->BytesAcumulats=bd_xp->BytesPerFitxa;
        bd_xp->BytesPerFitxa+=camp->BytesPerCamp;
        if (camp->AmpleDesitjat==0)
            camp->AmpleDesitjat=camp->AmpleDesitjatOriginal=MM_GetDefaultDesiredDBFFieldWidth(camp); //camp->BytesPerCamp;
        if (camp->TipusDeCamp=='C' && camp->BytesPerCamp>MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA)
            cal_DBF_estesa=TRUE;
        if(MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(camp->NomCamp))
            cal_DBF_estesa=TRUE;
    }

    bd_xp->OffsetPrimeraFitxa=MM_CalculateBytesFirstRecordOffset(bd_xp);

    if (cal_DBF_estesa || bd_xp->ncamps>MM_MAX_N_CAMPS_DBF_CLASSICA
        || bd_xp->nRecords>UINT32_MAX)
        bd_xp->versio_dbf=(MM_BYTE)MM_MARCA_VERSIO_1_DBF_ESTESA;
    else
        bd_xp->versio_dbf=MM_MARCA_DBASE4;
}


static void MM_InitializeOffsetExtendedFieldNameFields(struct MM_BASE_DADES_XP *bd_xp, MM_EXT_DBF_N_FIELDS i_camp)
{
    memset((char*)(&bd_xp->Camp[i_camp].reservat_2)+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES, 0, 4);
}
static void MM_InitializeBytesExtendedFieldNameFields(struct MM_BASE_DADES_XP *bd_xp, MM_EXT_DBF_N_FIELDS i_camp)
{
    memset((char*)(&bd_xp->Camp[i_camp].reservat_2)+MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES, 0, 1);
}

static short int MM_return_common_valid_DBF_field_name_string(char *cadena)
{
char *p;
short int error_retornat=0;

    if(!cadena)
        return 0;
    //strupr(cadena);
    for (p=cadena; *p; p++)
    {
        (*p)=(char)toupper(*p);
        if ((*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_')
            ;
        else
        {
            *p='_';
            error_retornat|=MM_NOM_CAMP_CARACTER_INVALID;
        }
    }
    if (cadena[0]=='_')
    {
        cadena[0]='0';
        error_retornat|=MM_NOM_CAMP_PRIMER_CARACTER_;
    }
    return error_retornat;
}

static short int MM_ReturnValidClassicDBFFieldName(char *cadena)
{
size_t long_nom_camp;
short int error_retornat=0;

    long_nom_camp=strlen(cadena);
    if ( (long_nom_camp<1) || (long_nom_camp>=MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF) )
    {
        cadena[MM_MAX_LON_FIELD_NAME_DBF-1]='\0';
        error_retornat|=MM_NOM_CAMP_MASSA_LLARG;
    }
    error_retornat|=MM_return_common_valid_DBF_field_name_string(cadena);
    return error_retornat;
}

static MM_BOOLEAN MM_CheckClassicFieldNameEqual(const struct MM_BASE_DADES_XP * base_dades_XP,
                            const char *nom_camp_classic)
{
MM_EXT_DBF_N_FIELDS i;

    for(i=0; i<base_dades_XP->ncamps; i++)
    {                
        if((strcasecmp(base_dades_XP->Camp[i].NomCampDBFClassica, nom_camp_classic)) == 0 ||
            (strcasecmp(base_dades_XP->Camp[i].NomCamp, nom_camp_classic)) == 0)
            return TRUE;
    }
    return FALSE;
}

static char *MM_GiveNewStringWithCharacterAhead(const char *text, char caracter)
{
char *ptr;
size_t i;

    if(!text)
        return nullptr;

    i=strlen(text);
    if ((ptr=calloc_function(i+2)) == nullptr)
        return nullptr;

    *ptr=caracter;
    memcpy(ptr+1,text,i+1);
    return ptr;
}

static char *MM_SetSubIndexFieldNam(char *nom_camp, MM_EXT_DBF_N_FIELDS index, size_t ampladamax)
{
char *NomCamp_SubIndex;
char *_subindex;
char subindex[6];
size_t longsubindex;
size_t longnomcamp;
                        
    NomCamp_SubIndex = calloc_function(ampladamax*sizeof(char));
    if(!NomCamp_SubIndex)
        return nullptr;
    
    strcpy (NomCamp_SubIndex, nom_camp);

    sprintf(subindex, "%llu", (GUInt64)index);

    _subindex = MM_GiveNewStringWithCharacterAhead(subindex, '_');
    longsubindex = strlen(_subindex);
    longnomcamp = strlen(NomCamp_SubIndex);

    if (longnomcamp + longsubindex > ampladamax-1)
        memcpy(NomCamp_SubIndex + ((ampladamax-1) - longsubindex), _subindex,
               strlen(_subindex));
    else
        NomCamp_SubIndex = strcat(NomCamp_SubIndex, _subindex);

    free_function(_subindex);

    return NomCamp_SubIndex;
}

MM_FIRST_RECORD_OFFSET_TYPE MM_GiveOffsetExtendedFieldName(const struct MM_CAMP *camp)
{
MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;

    memcpy(&offset_nom_camp, (char*)(&camp->reservat_2)+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES, 4);
    return offset_nom_camp;
}

int MM_WriteNRecordsMMBD_XPFile(struct MMAdmDatabase *MMAdmDB)
{
    GUInt32 nRecords;
    if(!MMAdmDB->pMMBDXP)
        return 0;

    // Updating number of features in database
    fseek_function(MMAdmDB->pFExtDBF, MM_FIRST_OFFSET_to_N_RECORDS, SEEK_SET);

    //MMAdmDB->pMMBDXP->nRecords=939439764538373;
    if (MMAdmDB->pMMBDXP->nRecords > UINT32_MAX)
    {
        MMAdmDB->pMMBDXP->versio_dbf=MM_MARCA_VERSIO_1_DBF_ESTESA;

        if (fwrite_function(&MMAdmDB->pMMBDXP->nRecords, 4, 1,
                MMAdmDB->pFExtDBF) != 1)
            return FALSE;
    }
    else
    {
        MMAdmDB->pMMBDXP->versio_dbf=MM_MARCA_DBASE4;

        nRecords=(GUInt32)MMAdmDB->pMMBDXP->nRecords;
        if (fwrite_function(&nRecords, 4, 1, MMAdmDB->pFExtDBF) != 1)
            return FALSE;
    }

    fseek_function(MMAdmDB->pFExtDBF, MM_SECOND_OFFSET_to_N_RECORDS, SEEK_SET);
    if (MMAdmDB->pMMBDXP->versio_dbf == MM_MARCA_VERSIO_1_DBF_ESTESA)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        if (fwrite_function(((char *)(&MMAdmDB->pMMBDXP->nRecords))+4, 4, 1,
            MMAdmDB->pFExtDBF) != 1)
            return FALSE;

        /* from 20 to 27 */
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 8, 1,
            MMAdmDB->pFExtDBF) != 1)
            return FALSE;
    }
    else
    {
        if (fwrite_function(&(MMAdmDB->pMMBDXP->dbf_on_a_LAN), 12, 1,
            MMAdmDB->pFExtDBF) != 1)
            return FALSE;
    }
        
    return 0;
}

static MM_BOOLEAN MM_UpdateEntireHeader(struct MM_BASE_DADES_XP * base_dades_XP)
{
MM_BYTE variable_byte;
MM_EXT_DBF_N_FIELDS i, j=0;
const size_t max_n_zeros=11;
char *zero;
const MM_BYTE byte_zero=0;
char ModeLectura_previ[4]="";
MM_FIRST_RECORD_OFFSET_TYPE bytes_acumulats;
MM_BYTE mida_nom;
int estat;
char nom_camp[MM_MAX_LON_FIELD_NAME_DBF];
size_t retorn_fwrite;
MM_BOOLEAN cal_tancar_taula=FALSE;
GUInt32 nRecords;

    if ((zero=calloc_function(max_n_zeros))==nullptr)
        return FALSE;
    
    if (base_dades_XP->pfBaseDades == nullptr)
    {
        strcpy(ModeLectura_previ,base_dades_XP->ModeLectura);
        strcpy(base_dades_XP->ModeLectura, "wb");
        
        if ( (base_dades_XP->pfBaseDades =fopen_function(base_dades_XP->szNomFitxer,base_dades_XP->ModeLectura))==nullptr )
        {
            free_function(zero);
            return FALSE;
        }
        
        cal_tancar_taula = TRUE;
    }

    if((base_dades_XP->ncamps)>MM_MAX_N_CAMPS_DBF_CLASSICA)
        base_dades_XP->versio_dbf=MM_MARCA_VERSIO_1_DBF_ESTESA;
    else if((base_dades_XP->nRecords)>UINT32_MAX)
        base_dades_XP->versio_dbf=MM_MARCA_VERSIO_1_DBF_ESTESA;
    else
    {
        if (base_dades_XP->versio_dbf==MM_MARCA_VERSIO_1_DBF_ESTESA)
            base_dades_XP->versio_dbf=MM_MARCA_DBASE4;
        for (i = 0; i < base_dades_XP->ncamps; i++)
        {
            if (base_dades_XP->Camp[i].TipusDeCamp=='C' &&
                base_dades_XP->Camp[i].BytesPerCamp>MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA)
            {
                base_dades_XP->versio_dbf=MM_MARCA_VERSIO_1_DBF_ESTESA;
                break;
            }
            if (MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(base_dades_XP->Camp[i].NomCamp))
            {
                base_dades_XP->versio_dbf=MM_MARCA_VERSIO_1_DBF_ESTESA;
                break;
            }
        }
    }

    // Writting header
    fseek_function(base_dades_XP->pfBaseDades, 0, SEEK_SET);

    /* Byte 0 */
    if(fwrite_function(&(base_dades_XP->versio_dbf), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
    {
        free_function(zero);
        return FALSE;
    }

    /* MM_BYTE from 1 to 3 */
    variable_byte = (MM_BYTE)(base_dades_XP->any-1900);
    if(fwrite_function(&variable_byte, 1, 1, base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    if(fwrite_function(&(base_dades_XP->mes), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    if (fwrite_function(&(base_dades_XP->dia), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* from 4 a 7, position MM_FIRST_OFFSET_to_N_RECORDS */
    if (base_dades_XP->nRecords > UINT32_MAX)
    {
        if (fwrite_function(&base_dades_XP->nRecords, 4, 1,
                base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    else
    {
        nRecords=(GUInt32)base_dades_XP->nRecords;
        if (fwrite_function(&nRecords, 4, 1, base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }

    /* from 8 a 9, position MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA */
    if (fwrite_function(&(base_dades_XP->OffsetPrimeraFitxa), 2, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* from 10 to 11, & from 12 to 13 */
    if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf))
    {
        if (fwrite_function(&(base_dades_XP->BytesPerFitxa), sizeof(MM_TIPUS_BYTES_ACUMULATS_DBF), 1,
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    else
    {
        /* from 10 to 11 */
        if (fwrite_function(&(base_dades_XP->BytesPerFitxa), 2, 1,
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
        /* from 12 to 13 */
        if (fwrite_function(&(base_dades_XP->reservat_1), 2, 1,
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    /* byte 14 */
    if (fwrite_function(&(base_dades_XP->transaction_flag), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* byte 15 */
    if (fwrite_function(&(base_dades_XP->encryption_flag), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* from 16 to 27 */
    if (base_dades_XP->nRecords > UINT32_MAX)
    {
        /* from 16 to 19, position MM_SECOND_OFFSET_to_N_RECORDS */
        if (fwrite_function(((char *)(&base_dades_XP->nRecords))+4, 4, 1,
            base_dades_XP->pfBaseDades) != 1)
            return FALSE;

        /* from 20 to 27 */
        if (fwrite_function(&(base_dades_XP->dbf_on_a_LAN), 8, 1,
            base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    else
    {
        /* from 16 to 27 */
        if (fwrite_function(&(base_dades_XP->dbf_on_a_LAN), 12, 1,
            base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    /* byte 28 */
    if (fwrite_function(&(base_dades_XP->MDX_flag), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* Byte 29 */
    if (fwrite_function(&(base_dades_XP->JocCaracters), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* Bytes from 30 to 31, in position MM_SEGON_OFFSET_a_OFFSET_1a_FITXA */
    if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf))
    {
        if (fwrite_function(((char*)&(base_dades_XP->OffsetPrimeraFitxa))+2, 2, 1,
                base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    else
    {
        if (fwrite_function(&(base_dades_XP->reservat_2), 2, 1,
                base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    


    /* At 32th byte begins fields description    */
    /* Every description is 32 bytes long       */
    bytes_acumulats=32+32*(base_dades_XP->ncamps)+1;

    for (i = 0; i < base_dades_XP->ncamps; i++)
    {
        /* Bytes from 0 to 10    -> Field name, \0 finished */
        estat=MM_ISExtendedNameBD_XP(base_dades_XP->Camp[i].NomCamp);
        if(estat==MM_NOM_DBF_CLASSICA_I_VALID || estat==MM_NOM_DBF_MINUSCULES_I_VALID)
        {
            j = (short)strlen(base_dades_XP->Camp[i].NomCamp);
            
            retorn_fwrite=fwrite_function(&base_dades_XP->Camp[i].NomCamp, 1, j, base_dades_XP->pfBaseDades);
            if (retorn_fwrite != (size_t)j)
            {
                return FALSE;
            }
            MM_InitializeOffsetExtendedFieldNameFields(base_dades_XP, i);
            MM_InitializeBytesExtendedFieldNameFields(base_dades_XP, i);
        }
        else if(estat==MM_NOM_DBF_ESTES_I_VALID)
        {
            if(*(base_dades_XP->Camp[i].NomCampDBFClassica)=='\0')
            {
                char nom_temp[MM_MAX_LON_FIELD_NAME_DBF];

                MM_strnzcpy(nom_temp,base_dades_XP->Camp[i].NomCamp, MM_MAX_LON_FIELD_NAME_DBF);
                MM_ReturnValidClassicDBFFieldName(nom_temp);
                nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF-1]='\0';
                if ((MM_CheckClassicFieldNameEqual(base_dades_XP, nom_temp)) == TRUE)
                {
                    char *c;

                    c=MM_SetSubIndexFieldNam(nom_temp, i, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);

                    j = 0;
                    while (MM_CheckClassicFieldNameEqual(base_dades_XP, c) == TRUE && j < base_dades_XP->ncamps)
                    {
                        free_function(c);
                        c = MM_SetSubIndexFieldNam(nom_temp, ++j, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                    }

                    strcpy(base_dades_XP->Camp[i].NomCampDBFClassica, c);
                    free_function(c);
                }
                else
                    strcpy(base_dades_XP->Camp[i].NomCampDBFClassica, nom_temp);
            }
            j = (short)strlen(base_dades_XP->Camp[i].NomCampDBFClassica);

            retorn_fwrite=fwrite_function(&base_dades_XP->Camp[i].NomCampDBFClassica, 1, j, base_dades_XP->pfBaseDades);
            if (retorn_fwrite != (size_t)j)
            {
                free_function(zero);
                return FALSE;
            }

            mida_nom=MM_CalculateBytesExtendedFieldName(base_dades_XP->Camp+i);
            MM_EscriuOffsetNomEstesBD_XP(base_dades_XP, i, bytes_acumulats);
            bytes_acumulats+=mida_nom;
        }
        else
        {
            free_function(zero);
            return FALSE;
        }
        
        
        if (fwrite_function(zero, 1, 11-j, base_dades_XP->pfBaseDades) != 11-(size_t)j)
        {
            free_function(zero);
            return FALSE;
        }
        /* Byte 11, Field type */
        if (fwrite_function(&base_dades_XP->Camp[i].TipusDeCamp, 1, 1, base_dades_XP->pfBaseDades) != 1)
        {
            free_function(zero);
            return FALSE;
        }
        /* Bytes 12 to 15 --> Reserved */
        if (fwrite_function(&base_dades_XP->Camp[i].reservat_1, 4, 1, base_dades_XP->pfBaseDades) != 1)
        {
            free_function(zero);
            return FALSE;
        }
        /* Byte 16, or OFFSET_BYTESxCAMP_CAMP_CLASSIC --> BytesPerCamp */
        if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf) && base_dades_XP->Camp[i].TipusDeCamp=='C')
        {
            if (fwrite_function((void *)&byte_zero, 1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(&base_dades_XP->Camp[i].BytesPerCamp, 1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        /* 17th byte 17 --> In Fields, 'N' and 'F' indicate the decimals.*/
        if(base_dades_XP->Camp[i].TipusDeCamp == 'N' || base_dades_XP->Camp[i].TipusDeCamp == 'F')
        {
            if (fwrite_function(&base_dades_XP->Camp[i].DecimalsSiEsFloat, 1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        else
        {
            if (fwrite_function(zero, 1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf) && base_dades_XP->Camp[i].TipusDeCamp=='C')
        {
            /* Bytes from 18 to 20 --> Reserved */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2, 20-18+1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
            /* Bytes from 21 to 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C
                                    in extended DBF's */
            if (fwrite_function(&base_dades_XP->Camp[i].BytesPerCamp, sizeof(MM_TIPUS_BYTES_PER_CAMP_DBF), 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }

            /* Bytes from 25 to 30 --> Reserved */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2[25-18], 30-25+1, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        else
        {
            /* Bytes de 21 a 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, special fields, like C */
            memset(base_dades_XP->Camp[i].reservat_2+MM_OFFSET_RESERVAT2_BYTESxCAMP_CAMP_ESPECIAL, '\0', 4);
            /* Bytes from 18 to 30 --> Reserved */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2, 13, 1, base_dades_XP->pfBaseDades) != 1)
            {
                free_function(zero);
                return FALSE;
            }
        }
        /* Byte 31 --> MDX flag.    */
        if (fwrite_function(&base_dades_XP->Camp[i].MDX_camp_flag, 1, 1, base_dades_XP->pfBaseDades) != 1)
        {
            free_function(zero);
            return FALSE;
        }
    }

    free_function(zero);

    variable_byte = 13;
    if (fwrite_function(&variable_byte, 1, 1, base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    
    
    if(base_dades_XP->OffsetPrimeraFitxa!=bytes_acumulats)
        return FALSE;    

    // Extended fields
    for (i = 0; i < base_dades_XP->ncamps; i++)
    {
        if(MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(base_dades_XP->Camp[i].NomCamp))
        {
            bytes_acumulats=MM_GiveOffsetExtendedFieldName(base_dades_XP->Camp+i);
            mida_nom=MM_DonaBytesNomEstesCamp(base_dades_XP->Camp+i);

            fseek_function(base_dades_XP->pfBaseDades, bytes_acumulats, SEEK_SET);

            strcpy(nom_camp, base_dades_XP->Camp[i].NomCamp);
            //CanviaJocCaracPerEscriureDBF(nom_camp, JocCaracDBFaMM(base_dades_XP->JocCaracters, ParMM.JocCaracDBFPerDefecte));

            retorn_fwrite=fwrite_function(nom_camp, 1, mida_nom, base_dades_XP->pfBaseDades);

            if (retorn_fwrite != (size_t) mida_nom)
                return FALSE;
        }
    }

    if(cal_tancar_taula)
    {
        fclose_function(base_dades_XP->pfBaseDades);
        base_dades_XP->pfBaseDades = nullptr;
    }

    return TRUE;
} /* End of MM_UpdateEntireHeader() */

MM_BOOLEAN MM_CreateDBFFile(struct MM_BASE_DADES_XP * bd_xp, const char *NomFitxer)
{
    MM_CheckDBFHeader(bd_xp);
    if (NomFitxer)
        strcpy(bd_xp->szNomFitxer, NomFitxer);
    return MM_UpdateEntireHeader(bd_xp);
}

void MM_ReleaseMainFields(struct MM_BASE_DADES_XP * base_dades_XP)
{
MM_EXT_DBF_N_FIELDS i;
size_t j;
char **cadena;

    if (base_dades_XP->Camp)
    {
        for (i=0; i<base_dades_XP->ncamps; i++)
        {
            for (j=0; j<MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
            {
                cadena=base_dades_XP->Camp[i].separador;
                if (cadena[j])
                {
                    free_function(cadena[j]);
                    cadena[j]=nullptr;
                }
            }
        }
        free_function(base_dades_XP->Camp);
        base_dades_XP->Camp = nullptr;
        base_dades_XP->ncamps=0;
    }
    return;
}

// READING THE HEADER OF AN EXTENDED DBF
// Free with MM_ReleaseDBFHeader()
int MM_ReadExtendedDBFHeaderFromFile(const char * szFileName, struct MM_BASE_DADES_XP *pMMBDXP, const char * pszRelFile)
{
MM_BYTE  variable_byte;
FILE_TYPE *pf;
unsigned short int ushort;
MM_EXT_DBF_N_FIELDS nIField, j;
MM_FIRST_RECORD_OFFSET_TYPE offset_primera_fitxa;
MM_FIRST_RECORD_OFFSET_TYPE offset_fals=0;
MM_BOOLEAN grandaria_registre_incoherent=FALSE;
MM_BYTE un_byte;
MM_TIPUS_BYTES_PER_CAMP_DBF bytes_per_camp;
MM_BYTE tretze_bytes[13];
MM_FIRST_RECORD_OFFSET_TYPE offset_possible;
MM_BYTE n_queixes_estructura_incorrecta=0;
MM_FILE_OFFSET offset_reintent=0;
char cpg_file[MM_MAX_PATH];
const char *pszDesc;
char section[MM_MAX_LON_FIELD_NAME_DBF+25]; // TAULA_PRINCIPAL:field_name
GUInt32 nRecords;
char *pszString;

    if(!szFileName)
        return 1;

    strcpy(pMMBDXP->szNomFitxer, szFileName);
    strcpy(pMMBDXP->ModeLectura, "rb");

    if ((pMMBDXP->pfBaseDades=fopen_function(pMMBDXP->szNomFitxer,
         pMMBDXP->ModeLectura))==nullptr)
          return 1;

    pf=pMMBDXP->pfBaseDades;

    fseek_function(pf, 0, SEEK_SET);
    /* ====== Header reading (32 bytes) =================== */
    offset_primera_fitxa=0;

    if (1!=fread_function(&(pMMBDXP->versio_dbf), 1, 1, pf) ||
        1!=fread_function(&variable_byte, 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->mes), 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->dia), 1, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (1 != fread_function(&nRecords, 4, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (1!=fread_function(&offset_primera_fitxa, 2, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    pMMBDXP->any = (short)(1900+variable_byte);
    reintenta_lectura_per_si_error_CreaCampBD_XP:
    
    if (n_queixes_estructura_incorrecta>0)
    {
        if (!MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
        {
            offset_fals = offset_primera_fitxa;
            if ((offset_primera_fitxa-1)%32)
            {
                for (offset_fals=(offset_primera_fitxa-1); !((offset_fals-1)%32); offset_fals--)
                    ;
            }
        }
    }
    else
        offset_reintent=ftell_function(pf);

    if (1!=fread_function(&ushort, 2, 1, pf) ||
        1!=fread_function(&(pMMBDXP->reservat_1), 2, 1, pf) ||
        1!=fread_function(&(pMMBDXP->transaction_flag), 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->encryption_flag), 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
    {
        memcpy(&pMMBDXP->nRecords,&nRecords,4);
        memcpy(((char*)&pMMBDXP->nRecords)+4,&pMMBDXP->dbf_on_a_LAN,4);
    }
    else
        pMMBDXP->nRecords=nRecords;
            
    if (1 != fread_function(&(pMMBDXP->dbf_on_a_LAN), 8, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    if (1!=fread_function(&(pMMBDXP->MDX_flag), 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->JocCaracters), 1, 1, pf) ||
        1!=fread_function(&(pMMBDXP->reservat_2), 2, 1, pf))
    {
        fclose_function(pf);
        return 1;
    }

    // Checking for a cpg file
    if (pMMBDXP->JocCaracters==0)
    {
        FILE *f_cpg;

        strcpy(cpg_file, pMMBDXP->szNomFitxer);
        strcpy(cpg_file, reset_extension(cpg_file, ".cpg"));
        f_cpg=fopen(cpg_file, "rt");
        if(f_cpg)
        {
            char local_message[11];
            fseek(f_cpg, 0L, SEEK_SET);
            if(nullptr!=fgets(local_message, 10, f_cpg))
            {
                char *p=strstr(local_message, "UTF-8");
                if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_UTF8_DBF;
                p=strstr(local_message, "UTF8");
                if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_UTF8_DBF;
                p=strstr(local_message, "ISO-8859-1");
                if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_ANSI_DBASE;
            }
            fclose(f_cpg);
        }
    }
    if (MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
    {
        memcpy(&pMMBDXP->OffsetPrimeraFitxa,&offset_primera_fitxa,2);
        memcpy(((char*)&pMMBDXP->OffsetPrimeraFitxa)+2,&pMMBDXP->reservat_2,2);

        if (n_queixes_estructura_incorrecta>0)
            offset_fals=pMMBDXP->OffsetPrimeraFitxa;
        
        memcpy(&pMMBDXP->BytesPerFitxa, &ushort, 2);
        memcpy(((char*)&pMMBDXP->BytesPerFitxa)+2, &pMMBDXP->reservat_1, 2);
    }
    else
    {
        pMMBDXP->OffsetPrimeraFitxa=offset_primera_fitxa;
        pMMBDXP->BytesPerFitxa=ushort;
    }

    /* ====== Record structure ========================= */

    if (n_queixes_estructura_incorrecta>0)
        pMMBDXP->ncamps = (MM_EXT_DBF_N_FIELDS)(((offset_fals-1)-32)/32);
    else
    {
        MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats=1;

        pMMBDXP->ncamps=0;

        fseek_function(pf, 0, SEEK_END);
        if(32<(ftell_function(pf)-1))
        {
            fseek_function(pf, 32, SEEK_SET);
            do
            {
                bytes_per_camp=0;
                fseek_function(pf, 32+(MM_FILE_OFFSET)pMMBDXP->ncamps*32+(MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF+1+4), SEEK_SET);
                if (1!=fread_function(&bytes_per_camp, 1, 1, pf) ||
                    1!=fread_function(&un_byte, 1, 1, pf) ||
                    1!=fread_function(&tretze_bytes, 3+sizeof(bytes_per_camp), 1, pf))
                {
                    free(pMMBDXP->Camp);
                    fclose_function(pf);
                    return 1;
                }
                if (bytes_per_camp==0)
                    memcpy(&bytes_per_camp, (char*)(&tretze_bytes)+3, sizeof(bytes_per_camp));

                bytes_acumulats+=bytes_per_camp;
                pMMBDXP->ncamps++;
            }while(bytes_acumulats<pMMBDXP->BytesPerFitxa);
        }
    }

    if(pMMBDXP->ncamps != 0)
    {
        pMMBDXP->Camp = MM_CreateAllFields(pMMBDXP->ncamps);
        if (!pMMBDXP->Camp)
        {
            fclose_function(pf);
            return 1;
        }
    }
    else
        pMMBDXP->Camp = nullptr;

    fseek_function(pf, 32, SEEK_SET);
    for (nIField=0; nIField<pMMBDXP->ncamps; nIField++)
    {
        if (1!=fread_function(pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].TipusDeCamp), 1, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].reservat_1), 4, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].BytesPerCamp), 1, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].DecimalsSiEsFloat), 1, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].reservat_2), 13, 1, pf) ||
            1!=fread_function(&(pMMBDXP->Camp[nIField].MDX_camp_flag), 1, 1, pf))
        {
            free(pMMBDXP->Camp);
            fclose_function(pf);
            return 1;
        }

        #ifdef CODIFICATION_NEED_TO_BE_FINISHED
        MM_CanviaJocCaracLlegitDeDBF_CONSOLE(pMMBDXP->Camp[nIField].NomCamp, MM_JocCaracDBFaMM(pMMBDXP->JocCaracters, 850));
        #endif

        if(pMMBDXP->Camp[nIField].TipusDeCamp=='F')
            pMMBDXP->Camp[nIField].TipusDeCamp='N';

        pMMBDXP->Camp[nIField].NomCamp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF-1]='\0';
        if (EQUAL(pMMBDXP->Camp[nIField].NomCamp, "ID_GRAFIC"))
            pMMBDXP->CampIdGrafic=nIField;
        
        if (pMMBDXP->Camp[nIField].BytesPerCamp==0)
        {
            if (!MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
            {
                free(pMMBDXP->Camp);
                fclose_function(pf);
                return 1;
            }
            if (pMMBDXP->Camp[nIField].TipusDeCamp!='C')
            {
                free(pMMBDXP->Camp);
                fclose_function(pf);
                return 1;
            }

            memcpy(&pMMBDXP->Camp[nIField].BytesPerCamp, (char*)(&pMMBDXP->Camp[nIField].reservat_2)+3,
                        sizeof(MM_TIPUS_BYTES_PER_CAMP_DBF));
        }

        if (nIField)
            pMMBDXP->Camp[nIField].BytesAcumulats=
                (pMMBDXP->Camp[nIField-1].BytesAcumulats+
                    pMMBDXP->Camp[nIField-1].BytesPerCamp);
        else
            pMMBDXP->Camp[nIField].BytesAcumulats=1;

        for (j=0; j<MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
        {
            pMMBDXP->Camp[nIField].separador[j]=nullptr;

            if (pszRelFile)
            {
                sprintf(section, "TAULA_PRINCIPAL:%s", pMMBDXP->Camp[nIField].NomCamp);
                pszDesc = ReturnValueFromSectionINIFile(pszRelFile, section, "descriptor_eng");
                if (pszDesc)
                    MM_strnzcpy(pMMBDXP->Camp[nIField].DescripcioCamp[j], pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                else
                {
                    sprintf(section, "TAULA_PRINCIPAL:%s", pMMBDXP->Camp[nIField].NomCamp);
                    pszDesc = ReturnValueFromSectionINIFile(pszRelFile, section, "descriptor");
                    if (pszDesc)
                        MM_strnzcpy(pMMBDXP->Camp[nIField].DescripcioCamp[j], pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                    pMMBDXP->Camp[nIField].DescripcioCamp[j][0] = 0;
                }
            }
        }
    }

    if (!pMMBDXP->ncamps)
    {
        if (pMMBDXP->BytesPerFitxa)
            grandaria_registre_incoherent=TRUE;
    }
    else if(pMMBDXP->Camp[pMMBDXP->ncamps-1].BytesPerCamp+
            pMMBDXP->Camp[pMMBDXP->ncamps-1].BytesAcumulats
                >pMMBDXP->BytesPerFitxa)
        grandaria_registre_incoherent=TRUE;
    if (grandaria_registre_incoherent)
    {
        if (n_queixes_estructura_incorrecta==0)
        {
            grandaria_registre_incoherent=FALSE;
            fseek_function(pf, offset_reintent, SEEK_SET);
            n_queixes_estructura_incorrecta++;
            goto reintenta_lectura_per_si_error_CreaCampBD_XP;
        }
    }

    offset_possible=32+32*(pMMBDXP->ncamps)+1;

    if (!grandaria_registre_incoherent &&
        offset_possible!=pMMBDXP->OffsetPrimeraFitxa)
    {    // Extended names
        MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;
        int mida_nom;

        for(nIField=0; nIField<pMMBDXP->ncamps; nIField++)
        {
            offset_nom_camp=MM_GiveOffsetExtendedFieldName(pMMBDXP->Camp+nIField);
            mida_nom=MM_DonaBytesNomEstesCamp(pMMBDXP->Camp+nIField);
            if (mida_nom > 0 && mida_nom < MM_MAX_LON_FIELD_NAME_DBF &&
                offset_nom_camp >= offset_possible &&
                offset_nom_camp < pMMBDXP->OffsetPrimeraFitxa)
            {
                MM_strnzcpy(pMMBDXP->Camp[nIField].NomCampDBFClassica, pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
                fseek_function(pf, offset_nom_camp, SEEK_SET);
                if (1 != fread_function(pMMBDXP->Camp[nIField].NomCamp, mida_nom, 1, pf))
                {
                    free(pMMBDXP->Camp);
                    fclose_function(pf);
                    return 1;
                }
                pMMBDXP->Camp[nIField].NomCamp[mida_nom] = '\0';

                // All field names to UTF-8
                if (pMMBDXP->JocCaracters == MM_JOC_CARAC_ANSI_DBASE)
                {
                    pszString=CPLRecode_function(pMMBDXP->Camp[nIField].NomCamp, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    strncpy(pMMBDXP->Camp[nIField].NomCamp, pszString, MM_MAX_LON_FIELD_NAME_DBF);
                    CPLFree_function(pszString);
                }
                else if (pMMBDXP->JocCaracters == MM_JOC_CARAC_OEM850_DBASE)
                {
                    MM_oemansi(pMMBDXP->Camp[nIField].NomCamp);
                    pszString=CPLRecode_function(pMMBDXP->Camp[nIField].NomCamp, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    strncpy(pMMBDXP->Camp[nIField].NomCamp, pszString, MM_MAX_LON_FIELD_NAME_DBF);
                    CPLFree_function(pszString);
                }
            }
        }
    }
    
    pMMBDXP->CampIdEntitat=MM_MAX_EXT_DBF_N_FIELDS_TYPE;
    return 0;
} // End of MM_ReadExtendedDBFHeaderFromFile()

void MM_ReleaseDBFHeader(struct MM_BASE_DADES_XP * base_dades_XP)
{
    if (base_dades_XP)
    {
        MM_ReleaseMainFields(base_dades_XP);
        free_function(base_dades_XP);
    }
    return;
}

int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(struct MM_CAMP *camp,
            struct MM_BASE_DADES_XP * bd_xp, MM_BOOLEAN no_modifica_descriptor, size_t mida_nom)
{
MM_EXT_DBF_N_FIELDS i_camp;
unsigned n_digits_i=0, i;
int retorn=0;

    if (mida_nom==0)
        mida_nom=MM_MAX_LON_FIELD_NAME_DBF;

    for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
    {
        if (bd_xp->Camp+i_camp==camp)
            continue;
        if (!strcasecmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
            break;
    }
    if (i_camp<bd_xp->ncamps)
    {
        retorn=1;
        if (strlen(camp->NomCamp)>mida_nom-2)
            camp->NomCamp[mida_nom-2]='\0';
        strcat(camp->NomCamp, "0");
        for (i=2; i<(size_t)10; i++)
        {
            sprintf(camp->NomCamp+strlen(camp->NomCamp)-1, "%u", i);
            for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
            {
                if (bd_xp->Camp+i_camp==camp)
                    continue;
                if (!strcasecmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
                    break;
            }
            if (i_camp==bd_xp->ncamps)
            {
                n_digits_i=1;
                break;
            }
        }
        if (i==10)
        {
            camp->NomCamp[strlen(camp->NomCamp)-1]='\0';
            if (strlen(camp->NomCamp)>mida_nom-3)
                camp->NomCamp[mida_nom-3]='\0';
            strcat(camp->NomCamp, "00");
            for (i=10; i<(size_t)100; i++)
            {
                sprintf(camp->NomCamp+strlen(camp->NomCamp)-2, "%u", i);
                for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
                {
                    if (bd_xp->Camp+i_camp==camp)
                        continue;
                    if (!strcasecmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
                        break;
                }
                if (i_camp==bd_xp->ncamps)
                {
                    n_digits_i=2;
                    break;
                }
            }
            if (i==100)
            {
                camp->NomCamp[strlen(camp->NomCamp)-2]='\0';
                if (strlen(camp->NomCamp)>mida_nom-4)
                    camp->NomCamp[mida_nom-4]='\0';
                strcat(camp->NomCamp, "000");
                for (i=100; i<(size_t)256+2; i++)
                {
                    sprintf(camp->NomCamp+strlen(camp->NomCamp)-3, "%u", i);
                    for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
                    {
                        if (bd_xp->Camp+i_camp==camp)
                            continue;
                        if (!strcasecmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
                            break;
                    }
                    if (i_camp==bd_xp->ncamps)
                    {
                        n_digits_i=3;
                        break;
                    }
                }
                if (i==256)
                    return 2;
            }
        }
    }
    else
    {
        i=1;
    }

    if ((*(camp->DescripcioCamp[0])=='\0') || no_modifica_descriptor)
        return retorn;
        
    for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
    {
        if (bd_xp->Camp+i_camp==camp)
            continue;
        if (!strcasecmp(bd_xp->Camp[i_camp].DescripcioCamp[0], camp->DescripcioCamp[0]))
            break;
    }
    if (i_camp==bd_xp->ncamps)
        return retorn;
    
    if (retorn==1)
    {
        if (strlen(camp->DescripcioCamp[0])>MM_MAX_LON_DESCRIPCIO_CAMP_DBF-4-n_digits_i)
            camp->DescripcioCamp[0][mida_nom-4-n_digits_i]='\0';
        if(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]))
            sprintf(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]), " (%u)", i);
        for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
        {
            if (bd_xp->Camp+i_camp==camp)
                continue;
            if (!strcasecmp(bd_xp->Camp[i_camp].DescripcioCamp[0], camp->DescripcioCamp[0]))
                break;
        }
        if (i_camp==bd_xp->ncamps)
            return retorn;
    }
    
    retorn=1;
    if (strlen(camp->DescripcioCamp[0])>MM_MAX_LON_DESCRIPCIO_CAMP_DBF-4-n_digits_i)
        camp->DescripcioCamp[0][mida_nom-4-n_digits_i]='\0';
    camp->DescripcioCamp[0][strlen(camp->DescripcioCamp[0])-4-n_digits_i+1]='\0';
    if (strlen(camp->DescripcioCamp[0])>MM_MAX_LON_DESCRIPCIO_CAMP_DBF-7)
        camp->DescripcioCamp[0][mida_nom-7]='\0';
    for (i++; i<(size_t)256; i++)
    {
        if(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]))
            sprintf(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]), " (%u)", i);
        for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
        {
            if (bd_xp->Camp+i_camp==camp)
                continue;
            if (!strcasecmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
                break;
        }
        if (i_camp==bd_xp->ncamps)
            return retorn;
    }
    return 2;
} // End of MM_ModifyFieldNameAndDescriptorIfPresentBD_XP()

int MM_DuplicateMultilingualString(char *(cadena_final[MM_NUM_IDIOMES_MD_MULTIDIOMA]), 
                               const char * const (cadena_inicial[MM_NUM_IDIOMES_MD_MULTIDIOMA]))
{
size_t i;

    for (i=0; i<MM_NUM_IDIOMES_MD_MULTIDIOMA; i++)
    {
        if (cadena_inicial[i])
        {
            if (nullptr==(cadena_final[i]=strdup(cadena_inicial[i])))
                return 1;
        }
        else
            cadena_final[i]=nullptr;
    }
    return 0;
}

int MM_DuplicateFieldDBXP(struct MM_CAMP *camp_final, const struct MM_CAMP *camp_inicial)
{
    *camp_final=*camp_inicial;

    if(0!=MM_DuplicateMultilingualString(camp_final->separador, (const char * const(*))camp_inicial->separador))
        return 1;
    
    return 0;
}

char *MM_strnzcpy(char *dest, const char *src, size_t maxlen)
{
size_t i;
    if(!src)
    {
        *dest='\0';
        return dest;
    }

    if (!maxlen)
        i=0;
    else
        strncpy(dest, src, i=maxlen-1);

    dest[i]='\0';
    return dest;
}

char *MM_oemansi_n(char *szcadena, size_t n_bytes)
{
size_t u_i;
unsigned char *punter_bait;
unsigned char t_oemansi[128]=
    {    199, 252, 233, 226, 228, 224, 229, 231, 234, 235, 232, 239, 238, 236,
        196, 197, 201, 230, 198, 244, 246, 242, 251, 249, 255, 214, 220, 248,
        163, 216, 215, 131, 225, 237, 243, 250, 241, 209, 170, 186, 191, 174,
        172, 189, 188, 161, 171, 187, 164, 164, 164, 166, 166, 193, 194, 192,
        169, 166, 166, 164, 164, 162, 165, 164, 164, 164, 164, 164, 164, 164,
        227, 195, 164, 164, 164, 164, 166, 164, 164, 164, 240, 208, 202, 203,
        200, 180, 205, 206, 207, 164, 164, 164, 164, 166, 204, 164, 211, 223,
        212, 210, 245, 213, 181, 254, 222, 218, 219, 217, 253, 221, 175, 180,
        173, 177, 164, 190, 182, 167, 247, 184, 176, 168, 183, 185, 179, 178,
        164, 183
    };
    if (n_bytes == USHRT_MAX)
    {
        for (punter_bait = (unsigned char*)szcadena; *punter_bait; punter_bait++)
        {
            if (*punter_bait > 127)
                *punter_bait = t_oemansi[*punter_bait - 128];
        }
    }
    else
    {
        for (u_i = 0, punter_bait = (unsigned char*)szcadena;
            u_i < n_bytes; punter_bait++, u_i++)
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


static MM_BOOLEAN MM_FillFieldDB_XP(struct MM_CAMP *camp, const char *NomCamp, const char *DescripcioCamp, char TipusDeCamp,
                    MM_TIPUS_BYTES_PER_CAMP_DBF BytesPerCamp, MM_BYTE DecimalsSiEsFloat)
{
char nom_temp[MM_MAX_LON_FIELD_NAME_DBF];
int retorn_valida_nom_camp;

    if (NomCamp)
    {
        retorn_valida_nom_camp=MM_ISExtendedNameBD_XP(NomCamp);
        if(retorn_valida_nom_camp==MM_NOM_DBF_NO_VALID)
            return FALSE;
        MM_strnzcpy(camp->NomCamp, NomCamp, MM_MAX_LON_FIELD_NAME_DBF);

        if(retorn_valida_nom_camp==MM_NOM_DBF_ESTES_I_VALID)
        {
            MM_CalculateBytesExtendedFieldName(camp);
            MM_strnzcpy(nom_temp, NomCamp, MM_MAX_LON_FIELD_NAME_DBF);
            MM_ReturnValidClassicDBFFieldName(nom_temp);
            MM_strnzcpy(camp->NomCampDBFClassica, nom_temp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
        }
    }

    if (DescripcioCamp)
        strcpy(camp->DescripcioCamp[0], DescripcioCamp);
    else
        strcpy(camp->DescripcioCamp[0], "\0");
    camp->TipusDeCamp=TipusDeCamp;
    camp->DecimalsSiEsFloat=DecimalsSiEsFloat;
    camp->BytesPerCamp=BytesPerCamp;
    return TRUE;
}

#define szMMNomCampIdGraficDefecte    "ID_GRAFIC"
#define szMMNomCampPerimetreDefecte   "PERIMETRE"
#define szMMNomCampAreaDefecte        "AREA"
#define szMMNomCampLongitudArcDefecte "LONG_ARC"
#define szMMNomCampNodeIniDefecte     "NODE_INI"
#define szMMNomCampNodeFiDefecte      "NODE_FI"
#define szMMNomCampArcsANodeDefecte   "ARCS_A_NOD"
#define szMMNomCampTipusNodeDefecte   "TIPUS_NODE"
#define szMMNomCampNVertexsDefecte    "N_VERTEXS"
#define szMMNomCampNArcsDefecte       "N_ARCS"
#define szMMNomCampNPoligonsDefecte   "N_POLIG"

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp,
                MM_BYTE n_decimals)
{
MM_EXT_DBF_N_FIELDS i_camp=0;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampIdGraficDefecte,
            "Internal graphic identifier", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->CampIdGrafic=0;
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNVertexsDefecte,
            "Number of vertices", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampPerimetreDefecte,
            "Perimeter of the polygon", 'N', MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_PERIMETRE;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampAreaDefecte,
            "Area of the polygon", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_AREA;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNArcsDefecte,
            "Number of arcs", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_N_ARCS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNPoligonsDefecte,
            "Number of elemental polygons", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_N_POLIG;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstArcFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals)
{
MM_EXT_DBF_N_FIELDS i_camp;

    i_camp=0;
    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampIdGraficDefecte,
            "Internal graphic identifier", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->CampIdGrafic=0;
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNVertexsDefecte,
            "Number of vertices", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_N_VERTEXS;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampLongitudArcDefecte,
            "Lenght of arc", 'N', MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_LONG_ARC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNodeIniDefecte,
            "Initial node", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_NODE_INI;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampNodeFiDefecte,
            "Final node", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_NODE_FI;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp)
{
MM_EXT_DBF_N_FIELDS i_camp;

    i_camp=0;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampIdGraficDefecte,
            "Internal graphic identifier", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->CampIdGrafic=0;
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampArcsANodeDefecte,
            "Number of arcs to node", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_ARCS_A_NOD;
    i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampTipusNodeDefecte,
            "Node type", 'N',1, 0);
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_TIPUS_NODE;
    i_camp++;

    return i_camp;
}

size_t MM_DefineFirstPointFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp)
{
size_t i_camp=0;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szMMNomCampIdGraficDefecte,
            "Internal graphic identifier", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, 0);
    bd_xp->CampIdGrafic=0;
    (bd_xp->Camp+i_camp)->TipusCampGeoTopo=(MM_BYTE)MM_CAMP_ES_ID_GRAFIC;
    i_camp++;

    return i_camp;
}


const char MM_CadenaBuida[]={""};
#define MM_MarcaFinalDeCadena (*MM_CadenaBuida)
const char MM_CadenaEspai[]={" "};

static MM_BOOLEAN MM_EsNANDouble(double a)
{
GInt64 exp, mantissa;

    exp = *(GInt64*)&a& 0x7FF0000000000000ull;
    mantissa = *(GInt64*)&a & 0x000FFFFFFFFFFFFFull;
    if (exp == 0x7FF0000000000000ull && mantissa != 0)
        return TRUE;
    return FALSE;
}

#define MM_EsDoubleInfinit(a) (((*(GInt64*)&(a)&0x7FFFFFFFFFFFFFFFull)==0x7FF0000000000000ull)?TRUE:FALSE)

static int MM_SprintfDoubleAmplada(char * cadena, int amplada, int n_decimals, double valor_double,
                            MM_BOOLEAN *Error_sprintf_n_decimals)
{
#define VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E 1E+17
#define VALOR_MASSA_PETIT_PER_IMPRIMIR_f 1E-17
char cadena_treball[MM_CARACTERS_DOUBLE+1];
int retorn_printf;

    if (MM_EsNANDouble(valor_double)) 
    {
        if (amplada<3)
        {
            *cadena=*MM_CadenaBuida;
            return EOF;
        }
        return sprintf (cadena, "NAN");
    }
    if (MM_EsDoubleInfinit(valor_double))
    {
        if (amplada<3)
        {
            *cadena=*MM_CadenaBuida;
            return EOF;
        }
        return sprintf (cadena, "INF");
    }

    *Error_sprintf_n_decimals=FALSE;
    if (valor_double==0)
    {
        retorn_printf=sprintf (cadena_treball, "%*.*f", amplada, n_decimals, valor_double);
        if (retorn_printf==EOF)
        {
            *cadena=*MM_CadenaBuida;
            return retorn_printf;
        }
     
        if (retorn_printf>amplada)
        {
            int escurcament=retorn_printf-amplada;
            if (escurcament>n_decimals)
            {
                *cadena=*MM_CadenaBuida;
                return EOF;
            }
            *Error_sprintf_n_decimals=TRUE;
            n_decimals=n_decimals-escurcament;
            retorn_printf=sprintf (cadena, "%*.*f", amplada, n_decimals, valor_double);
        }
        else
            strcpy(cadena,cadena_treball);

        return retorn_printf;
    }

    if ( valor_double> VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E ||
         valor_double<-VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E ||
        (valor_double< VALOR_MASSA_PETIT_PER_IMPRIMIR_f &&
         valor_double>-VALOR_MASSA_PETIT_PER_IMPRIMIR_f) )
    {
        retorn_printf=sprintf (cadena_treball, "%*.*E", amplada, n_decimals, valor_double);
        if (retorn_printf==EOF)
        {
            *cadena=*MM_CadenaBuida;
            return retorn_printf;
        }
        if (retorn_printf>amplada)
        {
            int escurcament=retorn_printf-amplada;
            if (escurcament>n_decimals)
            {
                *cadena=*MM_CadenaBuida;
                return EOF;
            }
            *Error_sprintf_n_decimals=TRUE;
            n_decimals=n_decimals-escurcament;
            retorn_printf=sprintf (cadena, "%*.*E", amplada, n_decimals, valor_double);
        }
        else
            strcpy(cadena,cadena_treball);

        return retorn_printf;
    }

    retorn_printf=sprintf (cadena_treball, "%*.*f", amplada, n_decimals, valor_double);
    if (retorn_printf==EOF)
    {
        *cadena=*MM_CadenaBuida;
        return retorn_printf;
    }

    if (retorn_printf>amplada)
    {
        int escurcament=retorn_printf-amplada;
        if (escurcament>n_decimals)
        {
            *cadena=*MM_CadenaBuida;
            return EOF;
        }
        *Error_sprintf_n_decimals=TRUE;
        n_decimals=n_decimals-escurcament;
        retorn_printf=sprintf (cadena, "%*.*f", amplada, n_decimals, valor_double);
    }
    else
        strcpy(cadena,cadena_treball);

    return retorn_printf;

#undef VALOR_LIMIT_IMPRIMIR_EN_FORMAT_E
#undef VALOR_MASSA_PETIT_PER_IMPRIMIR_f
} // Fi de MM_SprintfDoubleAmplada()

static MM_BOOLEAN MM_EsCadenaDeBlancs(const char *cadena)
{
char *ptr;

    for (ptr=(char*)cadena; *ptr; ptr++)
        if (*ptr!=' ' && *ptr!='\t')
            return FALSE;

    return TRUE;
}

int MM_SecureCopyStringFieldValue(char **pszStringDst,
                                 const char *pszStringSrc,
                                 MM_EXT_DBF_N_FIELDS *nStringCurrentLenght)
{
    if(!pszStringSrc)
    {
        if(1>=*nStringCurrentLenght)
        {
            (*pszStringDst)=realloc_function(*pszStringDst, 2);
            if(!(*pszStringDst))
                return 1;
            *nStringCurrentLenght=(MM_EXT_DBF_N_FIELDS)2;
        }
        strcpy(*pszStringDst, "\0");
        return 0;
    }

    if(strlen(pszStringSrc)>=*nStringCurrentLenght)
    {
        (*pszStringDst)=realloc_function(*pszStringDst, strlen(pszStringSrc)+1);
        if(!(*pszStringDst))
            return 1;
        *nStringCurrentLenght=(MM_EXT_DBF_N_FIELDS)(strlen(pszStringSrc)+1);
    }
    strcpy(*pszStringDst, pszStringSrc);
    return 0;
}

// This function assumes that all the file is saved in disk and closed.
int MM_ChangeDBFWidthField(struct MM_BASE_DADES_XP * base_dades_XP,
                            MM_EXT_DBF_N_FIELDS nIField,
                            MM_TIPUS_BYTES_PER_CAMP_DBF nNewWidth,
                            MM_BYTE nNewPrecision,
                            MM_BYTE que_fer_amb_reformatat_decimals)
{
char *record, *whites=nullptr;
MM_TIPUS_BYTES_PER_CAMP_DBF l_glop1, l_glop2, i_glop2;
MM_EXT_DBF_N_RECORDS nfitx, i_reg;
int canvi_amplada;
GInt32 j;
MM_EXT_DBF_N_FIELDS i_camp;
size_t retorn_fwrite;
int retorn_TruncaFitxer;

MM_BOOLEAN error_sprintf_n_decimals=FALSE;
int retorn_printf;

    canvi_amplada = nNewWidth - base_dades_XP->Camp[nIField].BytesPerCamp;
    
    if (base_dades_XP->nRecords != 0)
    {
        l_glop1 = base_dades_XP->Camp[nIField].BytesAcumulats;
        i_glop2 = l_glop1 + base_dades_XP->Camp[nIField].BytesPerCamp;
        if (nIField == base_dades_XP->ncamps-1)
            l_glop2 = 0;
        else
            l_glop2 = base_dades_XP->BytesPerFitxa -
                    base_dades_XP->Camp[nIField + 1].BytesAcumulats;

        if ((record = calloc_function(base_dades_XP->BytesPerFitxa)) == nullptr)
            return 1;
        
        record[base_dades_XP->BytesPerFitxa-1]=MM_MarcaFinalDeCadena;

        if ((whites = (char *) calloc_function(nNewWidth)) == nullptr)
        {
            free_function (record);
            return 1;
        }
        memset(whites, ' ', nNewWidth);
        

        nfitx = base_dades_XP->nRecords;

        #ifdef _MSC_VER
        #pragma warning( disable : 4127 )
        #endif
        for (i_reg = (canvi_amplada<0 ? 0 : nfitx-1); TRUE; )
        #ifdef _MSC_VER
        #pragma warning( default : 4127 )
        #endif
        {
            if( 0!=fseek_function(base_dades_XP->pfBaseDades,
                           base_dades_XP->OffsetPrimeraFitxa +
                           (MM_FILE_OFFSET)i_reg * base_dades_XP->BytesPerFitxa,
                           SEEK_SET))
            {
                if (whites) free_function(whites);
                free_function (record);
                return 1;
            }

            if(1!=fread_function(record, base_dades_XP->BytesPerFitxa,
                            1, base_dades_XP->pfBaseDades))
            {
                if (whites) free_function(whites);
                free_function (record);
                return 1;
            }
            

            if(0!=fseek_function(base_dades_XP->pfBaseDades,
                            (MM_FILE_OFFSET)base_dades_XP->OffsetPrimeraFitxa +
                            i_reg * ((MM_FILE_OFFSET)base_dades_XP->BytesPerFitxa + canvi_amplada),
                            SEEK_SET))
            {
                if (whites) free_function(whites);
                free_function (record);
                return 1;
            }
            
            if(1!=fwrite_function(record, l_glop1, 1, base_dades_XP->pfBaseDades))
            {
                if (whites) free_function(whites);
                free_function (record);
                return 1;
            }

            switch (base_dades_XP->Camp[nIField].TipusDeCamp)
            {
                case 'C':
                case 'L':
                    memcpy(whites,
                       record + l_glop1,
                        ( canvi_amplada<0 ? nNewWidth :
                               base_dades_XP->Camp[nIField].BytesPerCamp));
                    retorn_fwrite=fwrite_function(whites, nNewWidth, 1, base_dades_XP->pfBaseDades);

                    if(1!=retorn_fwrite)
                    {
                        if (whites) free_function(whites);
                        free_function (record);
                        return 1;
                    }
                    break;
                case 'N':
                    if (nNewPrecision == base_dades_XP->Camp[nIField].DecimalsSiEsFloat ||
                        que_fer_amb_reformatat_decimals==MM_NOU_N_DECIMALS_NO_APLICA)
                        que_fer_amb_reformatat_decimals=MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS;
                    else if (que_fer_amb_reformatat_decimals==MM_PREGUNTA_SI_APLICAR_NOU_N_DECIM)
                        que_fer_amb_reformatat_decimals=MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS;
                    

                    if (que_fer_amb_reformatat_decimals==MM_NOMES_DOCUMENTAR_NOU_N_DECIMALS)
                    {
                        if (canvi_amplada>=0)
                        {
                            if( 1!=fwrite_function(whites, canvi_amplada, 1, base_dades_XP->pfBaseDades) ||
                                1!=fwrite_function(record + l_glop1,
                                        base_dades_XP->Camp[nIField].BytesPerCamp, 1,
                                        base_dades_XP->pfBaseDades))
                            {
                                if (whites) free_function(whites);
                                free_function (record);
                                return 1;
                            }
                        }
                        else if (canvi_amplada<0)
                        {

                            #ifdef _MSC_VER
                            #pragma warning( disable : 4127 )
                            #endif
                            for(j=(GInt32)(l_glop1 + (base_dades_XP->Camp[nIField].BytesPerCamp-1));TRUE;j--)
                            #ifdef _MSC_VER
                            #pragma warning( default : 4127 )
                            #endif
                            {
                                if(j<(GInt32)l_glop1 || record[j] ==  ' ')
                                {
                                    j++;
                                    break;
                                }
                            }

                            if((base_dades_XP->Camp[nIField].BytesPerCamp + l_glop1- j) < nNewWidth)
                                j -= (GInt32)(nNewWidth - (base_dades_XP->Camp[nIField].BytesPerCamp + l_glop1-j));

                            retorn_fwrite=fwrite_function(record+j, nNewWidth, 1, base_dades_XP->pfBaseDades);
                            if(1!=retorn_fwrite)
                            {
                                if (whites) free_function(whites);
                                free_function (record);
                                return 1;
                            }
                        }
                    }
                    else // MM_APLICAR_NOU_N_DECIMALS
                    {    
                        double valor;
                        char *sz_valor;

                        if ((sz_valor=calloc_function(MM_max(nNewWidth,base_dades_XP->Camp[nIField].BytesPerCamp)+1))==nullptr) // Sumo 1 per poder posar-hi el \0
                        {
                            if (whites) free_function(whites);
                            free_function (record);
                            return 1;
                        }
                        memcpy(sz_valor, record + l_glop1, base_dades_XP->Camp[nIField].BytesPerCamp);
                        sz_valor[base_dades_XP->Camp[nIField].BytesPerCamp]=0;

                        if(!MM_EsCadenaDeBlancs(sz_valor))
                        {
                            if (sscanf(sz_valor, "%lf", &valor)!=1)
                                memset(sz_valor, *MM_CadenaEspai, MM_max(nNewWidth,base_dades_XP->Camp[nIField].BytesPerCamp));
                            else
                            {
                                retorn_printf=MM_SprintfDoubleAmplada(sz_valor, nNewWidth, 
                                    nNewPrecision, valor, &error_sprintf_n_decimals);
                            }

                            retorn_fwrite=fwrite_function(sz_valor,nNewWidth, 1, base_dades_XP->pfBaseDades);
                            if(1!=retorn_fwrite)
                            {
                                if (whites) free_function(whites);
                                free_function(record);
                                free_function(sz_valor);
                                return 1;
                            }
                        }
                        else
                        {
                            memset(sz_valor, *MM_CadenaEspai, nNewWidth);
                            retorn_fwrite=fwrite_function(sz_valor, nNewWidth, 1, base_dades_XP->pfBaseDades);
                            if(1!=retorn_fwrite)
                            {
                                if (whites) free_function(whites);
                                free_function(record);
                                free_function(sz_valor);
                                return 1;
                            }
                        }
                        free_function(sz_valor);
                    }
                    break;
                default:
                    free_function (whites);
                    free_function (record);
                    return 1;
            }
            if(l_glop2)
            {
                retorn_fwrite=fwrite_function(record + i_glop2, l_glop2, 1, base_dades_XP->pfBaseDades);
                if(1!=retorn_fwrite)
                {
                    if (whites) free_function(whites);
                    free_function (record);
                    return 1;
                }
            }

            if (canvi_amplada<0)
            {
                if (i_reg+1 == nfitx)
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

        if (whites) free_function(whites);
        free_function(record);

        retorn_TruncaFitxer=TruncateFile_function(base_dades_XP->pfBaseDades,
                      (MM_FILE_OFFSET)base_dades_XP->OffsetPrimeraFitxa +
                      (MM_FILE_OFFSET)base_dades_XP->nRecords *
                      ((MM_FILE_OFFSET)base_dades_XP->BytesPerFitxa+canvi_amplada));
        if (canvi_amplada<0 && retorn_TruncaFitxer)
            return 1;
    } /* Fi de registres de != 0*/

    if (canvi_amplada!=0)
    {
        base_dades_XP->Camp[nIField].BytesPerCamp = nNewWidth;
        base_dades_XP->BytesPerFitxa+=canvi_amplada;
        for (i_camp=(MM_EXT_DBF_N_FIELDS)(nIField+1); i_camp< base_dades_XP->ncamps; i_camp++)
            base_dades_XP->Camp[i_camp].BytesAcumulats+=canvi_amplada;
    }
    base_dades_XP->Camp[nIField].DecimalsSiEsFloat = nNewPrecision;

    //DonaData(&(base_dades_XP->dia), &(base_dades_XP->mes), &(base_dades_XP->any));

    if ((MM_UpdateEntireHeader(base_dades_XP)) == FALSE)
        return 1;
    
    return 0;
} /* End of MMChangeCFieldWidthDBF() */


static void MM_AdoptaAlcada(double *desti, const double *proposta, unsigned long int flag)
{
    if (*proposta==MM_NODATA_COORD_Z)
        return;

    if (flag&MM_STRING_HIGHEST_ALTITUDE)
    {
        if (*desti==MM_NODATA_COORD_Z || *desti<*proposta)
            *desti=*proposta;
    }
    else if (flag&MM_STRING_LOWEST_ALTITUDE)
    {
        if (*desti==MM_NODATA_COORD_Z || *desti>*proposta)
            *desti=*proposta;
    }
    else
    {
        // First coordinate of this vertice
        if (*desti==MM_NODATA_COORD_Z)
            *desti=*proposta;
    }
}

int MM_GetArcHeights(double *coord_z, FILE_TYPE *pF, MM_N_VERTICES_TYPE n_vrt, struct MM_ZD *pZDescription, unsigned long int flag)
{
MM_N_HEIGHT_TYPE i;
MM_N_VERTICES_TYPE i_vrt;
double *pcoord_z;
MM_N_HEIGHT_TYPE n_alcada, n_h_total;
int tipus;
double *alcada=nullptr, *palcada, *palcada_i;
#define MM_N_ALCADA_LOCAL 50
double local_CinquantaAlcades[MM_N_ALCADA_LOCAL];

    for (i_vrt=0; i_vrt<n_vrt; i_vrt++)
        coord_z[i_vrt]=MM_NODATA_COORD_Z;

    tipus=MM_ARC_TIPUS_ALCADA(pZDescription->nZCount);
    n_alcada=MM_ARC_N_ALCADES(pZDescription->nZCount);
    if (n_vrt==0 || n_alcada==0)
        return 0;

    if (tipus==MM_ARC_ALCADA_PER_CADA_VERTEX)
        n_h_total=(MM_N_HEIGHT_TYPE)n_vrt*n_alcada;
    else
        n_h_total=n_alcada;

    if (n_h_total<=MM_N_ALCADA_LOCAL)
        palcada=local_CinquantaAlcades;
    else
    {
        if (nullptr==(palcada=alcada=malloc(n_vrt*sizeof(double)*n_alcada)))
            return 1;
    }

    if (fseek_function(pF, pZDescription->nOffsetZ, SEEK_SET))
    {
        if (alcada)
            free(alcada);
        return 1;
    }
    if (n_h_total!=(MM_N_VERTICES_TYPE)fread_function(palcada, sizeof(double), n_h_total, pF))
    {
        if (alcada)
            free(alcada);
        return 1;
    }

    if (tipus==MM_ARC_ALCADA_PER_CADA_VERTEX)
    {
        palcada_i=palcada;
        for (i=0; i<n_alcada; i++)
        {
            for (i_vrt=0, pcoord_z=coord_z; i_vrt<n_vrt; i_vrt++, pcoord_z++, palcada_i++)
                MM_AdoptaAlcada(pcoord_z, palcada_i, flag);
        }
    }
    else
    {
        palcada_i=palcada;
        pcoord_z=coord_z;
        for (i=0; i<n_alcada; i++, palcada_i++)
            MM_AdoptaAlcada(pcoord_z, palcada_i, flag);

        if (*pcoord_z!=MM_NODATA_COORD_Z)
        {
            /*Copio el mateix valor a totes les alcades.*/
            for (i_vrt=1, pcoord_z++; i_vrt<(size_t)n_vrt; i_vrt++, pcoord_z++)
                *pcoord_z=*coord_z;
        }
    }
    if (alcada)
        free(alcada);
    return 0;
} // End of MM_GetArcHeights()

static char *MM_l_TreuBlancsDeFinalDeCadena(char *punter,
                                            size_t l_cadena)
{
int longitud_cadena=(int)l_cadena;
    if (longitud_cadena-- == 0) 
        return punter;

    if (punter[longitud_cadena] != ' ' && punter[longitud_cadena] != '\t')
        return punter;
    longitud_cadena--;

    while ( longitud_cadena > -1 )
    {
        if (punter[longitud_cadena] != ' ' &&
            punter[longitud_cadena] != '\t')
        {
            break;
        }
        longitud_cadena--;
    }

    punter[++longitud_cadena]='\0';
    return  punter;
}

char * MM_RemoveInitial_and_FinalQuotationMarks(char *cadena)
{
char *ptr1, *ptr2;
char cometa='"';

    if (*cadena==cometa)
    {
        ptr1=cadena;
        ptr2=ptr1+1;
        if (*ptr2)
        {
            while (*ptr2)
            {
                *ptr1=*ptr2;
                ptr1++;
                ptr2++;
            }
            if (*ptr1==cometa)
                *(ptr1-1)=0;
            else
                *ptr1=0;
        }
    }
    return cadena;
} /* Fi de MM_RemoveInitial_and_FinalQuotationMarks() */

char *MM_RemoveLeadingWhitespaceOfString(char * cadena)
{
char *ptr;
char *ptr2;

    if (cadena == nullptr)
        return cadena;

    for (ptr=cadena; *ptr && (*ptr==' '||*ptr=='\t'); ptr++)
      continue;

    if (ptr!=cadena)
    {
        ptr2=cadena;
        while (*ptr)
        {
            *ptr2=*ptr;
             ptr2++;
             ptr++;
        }
        *ptr2=0;
    }
    return cadena;
}

char *MM_RemoveWhitespacesFromEndOfString(char * str)
{
const char *s;

  if (str == nullptr)
    return str;

  for (s = str; *s; ++s)
    continue;
  return MM_l_TreuBlancsDeFinalDeCadena(str, (s-str));
}

struct MM_ID_GRAFIC_MULTIPLE_RECORD *MMCreateExtendedDBFIndex(FILE_TYPE *f, MM_EXT_DBF_N_RECORDS n, MM_EXT_DBF_N_RECORDS n_dbf,
        MM_FIRST_RECORD_OFFSET_TYPE offset_1era, MM_TIPUS_BYTES_ACUMULATS_DBF bytes_per_fitxa,
        MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats_id_grafic,
        MM_TIPUS_BYTES_PER_CAMP_DBF bytes_id_grafic, MM_BOOLEAN *isListField, MM_EXT_DBF_N_RECORDS *nMaxN)
{
struct MM_ID_GRAFIC_MULTIPLE_RECORD *id;
MM_EXT_DBF_N_RECORDS i_dbf;
MM_EXT_DBF_SIGNED_N_RECORDS i, id_grafic;
char *fitxa;
MM_TIPUS_BYTES_PER_CAMP_DBF bytes_final_id_principi_id1=bytes_per_fitxa-bytes_id_grafic;

    *isListField=FALSE;
    if (nullptr==(id = (struct MM_ID_GRAFIC_MULTIPLE_RECORD *)calloc_function(n*sizeof(*id))))
        return nullptr;
    
    if (nullptr==( fitxa = (char *)calloc_function(bytes_id_grafic+1)))
    {
        free_function (id);
        return nullptr;
    }
    fitxa[bytes_id_grafic]='\0';

    fseek_function(f,(MM_FILE_OFFSET)offset_1era + (MM_FILE_OFFSET)bytes_acumulats_id_grafic, SEEK_SET);

    i_dbf=0;
    do
    {
        if (i_dbf==n_dbf || fread_function(fitxa, 1, bytes_id_grafic, f)!= (size_t)bytes_id_grafic)
        {
            free_function (id);
            free_function (fitxa);
            return nullptr;
        }
        i_dbf++;
    }while (1!=sscanf(fitxa, scanf_MM_EXT_DBF_SIGNED_N_RECORDS, &id_grafic) || id_grafic<0);
    i=0;

    #ifdef _MSC_VER
    #pragma warning( disable : 4127 )
    #endif
    while(TRUE)
    #ifdef _MSC_VER
    #pragma warning( default : 4127 )
    #endif        
    {
        if (i>id_grafic)
        {
            free_function(id);
            free_function (fitxa);
            return nullptr;
        }
        i=id_grafic;
        if (i>=(MM_EXT_DBF_SIGNED_N_RECORDS)n)
        {
            free_function (fitxa);
            return id;
        }
        id[(size_t)i].offset=(MM_FILE_OFFSET)offset_1era+(MM_FILE_OFFSET)(i_dbf-1)*bytes_per_fitxa;
        do
        {
            id[(size_t)i].nMR++;
            if(!(*isListField) && id[(size_t)i].nMR>1)
                *isListField=TRUE;
            if(*nMaxN<id[(size_t)i].nMR)
                *nMaxN=id[(size_t)i].nMR;

            if (i_dbf==n_dbf)
            {
                free_function (fitxa);
                return id;
            }
            fseek_function(f, bytes_final_id_principi_id1, SEEK_CUR);
            if (fread_function(fitxa, 1, bytes_id_grafic, f)!= (size_t)bytes_id_grafic)
            {
                free_function (id);
                free_function (fitxa);
                return nullptr;
            }
            if (1!=sscanf(fitxa,scanf_MM_EXT_DBF_SIGNED_N_RECORDS,&id_grafic) || id_grafic>=(MM_EXT_DBF_SIGNED_N_RECORDS)n)
            {
                free_function (fitxa);
                return id;
            }
            i_dbf++;
        }while (id_grafic==i);
    }
}// End of MMCreateExtendedDBFIndex()

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
