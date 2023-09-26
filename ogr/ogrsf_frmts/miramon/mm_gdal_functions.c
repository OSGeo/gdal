/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 2023,  MiraMon
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
#include "gdal.h"			// Per a GDALDatasetH
#include "ogr_srs_api.h"	// Per a OSRGetAuthorityCode
#include "mm_gdal_constants.h"     // MM_STATISTICAL_UNDEFINED_VALUE
#include "mm_gdal_structures.h"    // struct MM_CAMP *camp
#include "mm_gdal_functions.h"      // JocCaracMMaDBF()
#include "mm_wrlayr.h" // fseek_function(),...
#else
#include "CmptCmp.h"
#include "mm_gdal\mm_gdal_constants.h"     // MM_STATISTICAL_UNDEFINED_VALUE
#include "mm_gdal\mm_gdal_structures.h"    // struct MM_CAMP *camp
#include "mm_gdal\mm_gdal_functions.h"      // JocCaracMMaDBF()
#include "mm_gdal\mm_wrlayr.h" // fseek_function(),...
#endif



#ifdef GDAL_COMPILATION
CPL_C_START // Necessary for compiling in GDAL project
#endif

static char local_message[MM_MESSAGE_LENGHT];

void MM_InitializeField(struct MM_CAMP *camp)
{
	memset(camp, '\0', sizeof(*camp));
	camp->TipusDeCamp='C';
	camp->mostrar_camp=MM_CAMP_MOSTRABLE;
    camp->simbolitzable=MM_CAMP_SIMBOLITZABLE;
	camp->CampDescHipervincle=MM_MAX_TIPUS_NUMERADOR_CAMP_DBF;	
    camp->TractamentVariable=MM_CAMP_INDETERMINAT;
	camp->TipusCampGeoTopo=MM_NO_ES_CAMP_GEOTOPO;
}//Fi de MM_InitializeField()

struct MM_CAMP *MM_CreateAllFields(int ncamps)
{
struct MM_CAMP *camp;
MM_NUMERATOR_DBF_FIELD_TYPE i;

    if ((camp=malloc(ncamps*sizeof(*camp)))==NULL)
        return NULL;
    
    for (i=0; i<(size_t)ncamps; i++)
    	MM_InitializeField(camp+i);
	return camp;
}//Fi de CreaTotsElsCamps()


struct MM_BASE_DADES_XP * MM_CreateEmptyHeader(MM_NUMERATOR_DBF_FIELD_TYPE n_camps)
{
struct MM_BASE_DADES_XP *base_dades_XP;

	if ((base_dades_XP = (struct MM_BASE_DADES_XP *)
						calloc(sizeof(struct MM_BASE_DADES_XP), 1)) == NULL)
	{
		//ErrorMsg(stb(0,No_mem));
		return NULL;
	}

	if (n_camps==0)
	{
		/* base_dades_XP->Camp =NULL; No cal perquè ja s'ha inicialitzat
						amb calloc() */
		;
	}
	else
	{
        //CreaTotsElsCamps inicialitza perfectament base_dades_XP->Camp (JM)
		base_dades_XP->Camp = (struct MM_CAMP *)MM_CreateAllFields(n_camps);
		if (!base_dades_XP->Camp)
		{
			free (base_dades_XP);
			//ErrorMsg(stb(0,No_mem));
			return NULL;
		}
	}
	base_dades_XP->ncamps=n_camps;
	return base_dades_XP;
}

struct MM_BASE_DADES_XP * MM_CreateDBFHeader(MM_NUMERATOR_DBF_FIELD_TYPE n_camps)
{
struct MM_BASE_DADES_XP *bd_xp;
struct MM_CAMP *camp;
MM_NUMERATOR_DBF_FIELD_TYPE i;

	if (NULL==(bd_xp=MM_CreateEmptyHeader(n_camps)))
    	return NULL;

    bd_xp->JocCaracters=MM_JOC_CARAC_UTF8_DBF;//MM_JOC_CARAC_ANSI_DBASE;
    
    strcpy(bd_xp->ModeLectura,"a+b");

	bd_xp->CampIdGrafic=n_camps;
    bd_xp->CampIdEntitat=MM_MAX_TIPUS_NUMERADOR_CAMP_DBF;
	bd_xp->versio_dbf=(MM_BYTE)((n_camps>MM_MAX_N_CAMPS_DBF_CLASSICA)?MM_MARCA_VERSIO_1_DBF_ESTESA:MM_MARCA_DBASE4);

    //bd_xp->CampQueTeFitxerObert=MM_MAX_TIPUS_NUMERADOR_CAMP_DBF;
    //bd_xp->CampRelacional=n_camps; //No hi ha camps relacional.

	for(i=0, camp=bd_xp->Camp; i<n_camps; i++, camp++)
    {
        MM_InitializeField(camp);
        if (i<99999)
			sprintf(camp->NomCamp, "CAMP%05u", (unsigned)(i+1));
        else // Pot arribar a 67108863 (MAX_N_CAMPS_DBF_ESTESA), per la qual cosa poso el nom CM########.
        	// També es podria escriu en hexadec. si es vol noms més curts o amb més espai per a la paraula camp
        	sprintf(camp->NomCamp, "CM%u", (unsigned)(i+1));
		camp->TipusDeCamp='C';
        camp->DecimalsSiEsFloat=0;
		camp->BytesPerCamp=50;
		camp->mostrar_camp=MM_CAMP_MOSTRABLE;
        //InitEstad_CAMP_BDXP_a_Indefinit(camp); No cal perquè MMInicialitzaCamp() ja ho ha fet.
	}
    return bd_xp;
} /* Fi de CreaCapcaleraDBF() */

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

MM_BYTE MM_GetDefaultDesiredDBFFieldWidth(const struct MM_CAMP *camp)
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

MM_BOOLEAN MM_is_field_name_lowercase(const char *cadena)
{
const char *p;

	for (p=cadena; *p; p++)
	{
		if ((*p>='a' && *p<='z'))
			return TRUE;
	}
	return FALSE;
}

MM_BOOLEAN MM_is_classical_field_DBF_name_or_lowercase(const char *cadena)
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

MM_BOOLEAN MM_Is_character_valid_for_extended_DBF_field_name(int valor, int *valor_substitut)
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

int MM_ISExtendedNameBD_XP(const char *nom_camp)
{
size_t mida, j;

	mida=strlen(nom_camp);
	if(mida>=MM_MAX_LON_FIELD_NAME_DBF)
		return MM_NOM_DBF_NO_VALID;

	// Retornem cas invàlid
	for(j=0;j<mida;j++)
	{
		if(!MM_Is_character_valid_for_extended_DBF_field_name((unsigned char)nom_camp[j], NULL))
			return MM_NOM_DBF_NO_VALID;
	}

	if(mida>=MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF)
		return MM_NOM_DBF_ESTES_I_VALID;

	// Retornem cas en que alguna lletra és típica de DBF estesa (un espai, un accent,...).
	// Les minúscules no es contemplen aquí
	if(!MM_is_classical_field_DBF_name_or_lowercase(nom_camp))
		return MM_NOM_DBF_ESTES_I_VALID;
	
	// Retornem el cas en que tenim alguna minúscula (ja hem descartat el cas en que hi hagi
	// caràcters de DBF estesa)
	if(MM_is_field_name_lowercase(nom_camp))
		return MM_NOM_DBF_MINUSCULES_I_VALID;
    
	// Només queda el cas clàssica ja que hem descartat tots els altres.
	return MM_NOM_DBF_CLASSICA_I_VALID;
}

MM_BYTE MM_CalculateBytesExtendedFieldName(struct MM_CAMP *camp)
{
    camp->reservat_2[MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES]=(MM_BYTE)strlen(camp->NomCamp);
    return MM_DonaBytesNomEstesCamp(camp);
}

MM_TIPUS_BYTES_ACUMULATS_DBF MM_CalculateBytesExtendedFieldNames(const struct MM_BASE_DADES_XP *bd_xp)
{
MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats=0;
MM_NUMERATOR_DBF_FIELD_TYPE i_camp;

	for(i_camp=0;i_camp<bd_xp->ncamps;i_camp++)
    {
    	if (MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(bd_xp->Camp[i_camp].NomCamp))
			bytes_acumulats+=MM_CalculateBytesExtendedFieldName(bd_xp->Camp+i_camp);
    }

	return bytes_acumulats;
}

MM_TIPUS_OFFSET_PRIMERA_FITXA MM_CalculateBytesFirstRecordOffset(struct MM_BASE_DADES_XP *bd_xp)
{
	if(bd_xp)
		return (32+32*bd_xp->ncamps+1+MM_CalculateBytesExtendedFieldNames(bd_xp));
	return 0;	
}//MM_CalculateBytesFirstRecordOffset()

void MM_CheckDBFHeader(struct MM_BASE_DADES_XP * bd_xp)
{
struct MM_CAMP *camp;
MM_NUMERATOR_DBF_FIELD_TYPE i;
MM_BOOLEAN cal_DBF_estesa=FALSE;

	bd_xp->BytesPerFitxa=1;
	for(i=0, camp=bd_xp->Camp; i<bd_xp->ncamps; i++, camp++)
    {
		camp->BytesAcumulats=bd_xp->BytesPerFitxa;
		bd_xp->BytesPerFitxa+=camp->BytesPerCamp;
        if (camp->TractamentVariable==0)
	        camp->TractamentVariable=MM_DBFFieldTypeToVariableProcessing(camp->TipusDeCamp);
        if (camp->AmpleDesitjat==0)
			camp->AmpleDesitjat=camp->AmpleDesitjatOriginal=MM_GetDefaultDesiredDBFFieldWidth(camp); //camp->BytesPerCamp;
        if (camp->TipusDeCamp=='C' && camp->BytesPerCamp>MM_MAX_AMPLADA_CAMP_C_DBF_CLASSICA)
            cal_DBF_estesa=TRUE;
		if(MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(camp->NomCamp))
			cal_DBF_estesa=TRUE;
	}

    bd_xp->OffsetPrimeraFitxa=MM_CalculateBytesFirstRecordOffset(bd_xp);

    if (cal_DBF_estesa || bd_xp->ncamps>MM_MAX_N_CAMPS_DBF_CLASSICA)
		bd_xp->versio_dbf=(MM_BYTE)MM_MARCA_VERSIO_1_DBF_ESTESA;
    else
        bd_xp->versio_dbf=MM_MARCA_DBASE4;
} // Fi de MM_CheckDBFHeader()


void MM_InitializeOffsetExtendedFieldNameFields(struct MM_BASE_DADES_XP *bd_xp, MM_NUMERATOR_DBF_FIELD_TYPE i_camp)
{
	memset((char*)(&bd_xp->Camp[i_camp].reservat_2)+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES, 0, 4);
}
void MM_InitializeBytesExtendedFieldNameFields(struct MM_BASE_DADES_XP *bd_xp, MM_NUMERATOR_DBF_FIELD_TYPE i_camp)
{
	memset((char*)(&bd_xp->Camp[i_camp].reservat_2)+MM_OFFSET_RESERVAT2_MIDA_NOM_ESTES, 0, 1);
}

short int MM_return_common_valid_DBF_field_name_string(char *cadena)
{
char *p;
short int error_retornat=0;

	strupr(cadena);
	for (p=cadena; *p; p++)
	{
		if ((*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_')
        	; // De moment és legal
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

short int MM_ReturnValidClassicDBFFieldName(char *cadena)
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

MM_BOOLEAN MM_CheckClassicFieldNameEqual(const struct MM_BASE_DADES_XP * base_dades_XP,
							const char *nom_camp_classic)
{
MM_NUMERATOR_DBF_FIELD_TYPE i;

	for(i=0; i<base_dades_XP->ncamps; i++)
    {				
		if((stricmp(base_dades_XP->Camp[i].NomCampDBFClassica, nom_camp_classic)) == 0 ||
			(stricmp(base_dades_XP->Camp[i].NomCamp, nom_camp_classic)) == 0)
        	return TRUE;
    }
    return FALSE;
}

char *MM_GiveNewStringWithCharacterAhead(const char *text, char caracter)
{
char *ptr;
size_t i;

	if(!text)
		return NULL;

    i=strlen(text);
	if ((ptr=malloc(i+2)) == NULL)
        return NULL;

    *ptr=caracter;
    memcpy(ptr+1,text,i+1);
	return ptr;
} // Fi de DonaNovaCadenaAmbCaracterDavant()

char *MM_SetSubIndexFieldNam(char *nom_camp, MM_NUMERATOR_DBF_FIELD_TYPE index, size_t ampladamax)
{
char *NomCamp_SubIndex;
char *_subindex;
char subindex[6];
size_t longsubindex;
size_t longnomcamp;
       					/* Faig un duplicat del nom del camp. */
						/* El duplicat ha de tenir de amplada size_t ampladamax */
    NomCamp_SubIndex = calloc(ampladamax, sizeof(char));
    if(!NomCamp_SubIndex)
        return NULL;
    
    strcpy (NomCamp_SubIndex, nom_camp);

    /* Convertir el subindex en caràcters.	*/
	//itoa((int)index, subindex, 10);
	sprintf(subindex, "%I64u", (unsigned __int64)index);

	_subindex = MM_GiveNewStringWithCharacterAhead(subindex, '_');
    longsubindex = strlen(_subindex);
    longnomcamp = strlen(NomCamp_SubIndex);

    if (longnomcamp + longsubindex > ampladamax-1)
	   	memcpy(NomCamp_SubIndex + ((ampladamax-1) - longsubindex), _subindex,
        	   strlen(_subindex));
    else
    	NomCamp_SubIndex = strcat(NomCamp_SubIndex, _subindex);

    free(_subindex);

    return NomCamp_SubIndex;
}

MM_TIPUS_OFFSET_PRIMERA_FITXA MM_GiveOffsetExtendedFieldName(const struct MM_CAMP *camp)
{
MM_TIPUS_OFFSET_PRIMERA_FITXA offset_nom_camp;

    memcpy(&offset_nom_camp, (char*)(&camp->reservat_2)+MM_OFFSET_RESERVAT2_OFFSET_NOM_ESTES, 4);
    return offset_nom_camp;
}


MM_BOOLEAN MM_UpdateEntireHeader(struct MM_BASE_DADES_XP * base_dades_XP)
{
MM_BYTE variable_byte;
MM_NUMERATOR_DBF_FIELD_TYPE i, j=0;  // Per a fer callar el compilador
const size_t max_n_zeros=11;
char *zero;
const MM_BYTE byte_zero=0;
char ModeLectura_previ[4]="";
MM_TIPUS_OFFSET_PRIMERA_FITXA bytes_acumulats;
MM_BYTE mida_nom;
int estat;
char nom_camp[MM_MAX_LON_FIELD_NAME_DBF];
size_t retorn_fwrite;
MM_BOOLEAN cal_tancar_taula=FALSE;

	if ((zero=calloc(1,max_n_zeros))==NULL)
		return FALSE;
	
    if (base_dades_XP->pfBaseDades == NULL)
	{
		strcpy(ModeLectura_previ,base_dades_XP->ModeLectura);
		strcpy(base_dades_XP->ModeLectura, "wb");
        
        if ( (base_dades_XP->pfBaseDades =fopen_function(base_dades_XP->szNomFitxer,base_dades_XP->ModeLectura))==NULL )
			return FALSE;
		
        cal_tancar_taula = TRUE;
	}

	if((base_dades_XP->ncamps)>MM_MAX_N_CAMPS_DBF_CLASSICA)
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
        return FALSE;

    /* MM_BYTE de 1 a 3 */
    variable_byte = (MM_BYTE)(base_dades_XP->any-1900);
    if(fwrite_function(&variable_byte, 1, 1, base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    if(fwrite_function(&(base_dades_XP->mes), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    if (fwrite_function(&(base_dades_XP->dia), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* de 4 a 7 */
    if (fwrite_function(&(base_dades_XP->nfitxes), 4, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* de 8 a 9, posició MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA */
    if (fwrite_function(&(base_dades_XP->OffsetPrimeraFitxa), 2, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* de 10 a 11, i de 12 a 13 */
    if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf))
    {
        if (fwrite_function(&(base_dades_XP->BytesPerFitxa), sizeof(MM_TIPUS_BYTES_ACUMULATS_DBF), 1,
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
    }
    else
    {
        /* de 10 a 11 */
        if (fwrite_function(&(base_dades_XP->BytesPerFitxa), 2, 1,
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
        /* de 12 a 13 */
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
    /* de 16 a 27 */
    if (fwrite_function(&(base_dades_XP->dbf_on_a_LAN), 12, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;
    /* byte 28 */
    if (fwrite_function(&(base_dades_XP->MDX_flag), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* Byte 29 */
    if (fwrite_function(&(base_dades_XP->JocCaracters), 1, 1,
                base_dades_XP->pfBaseDades) != 1)
        return FALSE;

    /* Bytes 30 a 31, a MM_SEGON_OFFSET_a_OFFSET_1a_FITXA */
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
    


	/* Al byte 32 comença la descripció	 */
	/* dels camps. Cada descripció ocupa */
	/* 32 bytes.						 */
    bytes_acumulats=32+32*(base_dades_XP->ncamps)+1;

	for (i = 0; i < base_dades_XP->ncamps; i++)
	{
		/* Bytes de 0 a 10	-> Nom del camp, acabat amb \0				*/
		estat=MM_ISExtendedNameBD_XP(base_dades_XP->Camp[i].NomCamp);
		if(estat==MM_NOM_DBF_CLASSICA_I_VALID || estat==MM_NOM_DBF_MINUSCULES_I_VALID)
        {
			j = (short)strlen(base_dades_XP->Camp[i].NomCamp);
        	
			// Fem aquesta traducció abans d'escriure
			strcpy(local_message, base_dades_XP->Camp[i].NomCamp);
			//CanviaJocCaracPerEscriureDBF(local_message, JocCaracDBFaMM(base_dades_XP->JocCaracters, ParMM.JocCaracDBFPerDefecte));

			retorn_fwrite=fwrite_function(&local_message, 1, j, base_dades_XP->pfBaseDades);
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

				strncpy(nom_temp,base_dades_XP->Camp[i].NomCamp, MM_MAX_LON_FIELD_NAME_DBF);
				MM_ReturnValidClassicDBFFieldName(nom_temp);
				nom_temp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF-1]='\0';
				if ((MM_CheckClassicFieldNameEqual(base_dades_XP, nom_temp)) == TRUE)
               	{
					char *c;

					// Modifico el nom a la base de dades !
					c=MM_SetSubIndexFieldNam(nom_temp, i, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);

					//el nom que acabo de construir, existeix a la base de dades?
					j = 0;
					while (MM_CheckClassicFieldNameEqual(base_dades_XP, c) == TRUE && j < base_dades_XP->ncamps)
					{
						free(c);
						c = MM_SetSubIndexFieldNam(nom_temp, ++j, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
					}

					// Deso el nom del camp a l'estructura
					strcpy(base_dades_XP->Camp[i].NomCampDBFClassica, c);
					free(c);
                }
				else
					strcpy(base_dades_XP->Camp[i].NomCampDBFClassica, nom_temp);
            }
        	j = (short)strlen(base_dades_XP->Camp[i].NomCampDBFClassica);

			// Fem aquesta traducció abans d'escriure
			strcpy(local_message, base_dades_XP->Camp[i].NomCampDBFClassica);
			//CanviaJocCaracPerEscriureDBF(local_message, JocCaracDBFaMM(base_dades_XP->JocCaracters, ParMM.JocCaracDBFPerDefecte));

            retorn_fwrite=fwrite_function(&local_message, 1, j, base_dades_XP->pfBaseDades);
        	if (retorn_fwrite != (size_t)j)
            {
				return FALSE;
			}

            mida_nom=MM_CalculateBytesExtendedFieldName(base_dades_XP->Camp+i);
			MM_EscriuOffsetNomEstesBD_XP(base_dades_XP, i, bytes_acumulats);
            bytes_acumulats+=mida_nom;
        }
		else
			return FALSE;
		
		
        if (fwrite_function(zero, 1, 11-j, base_dades_XP->pfBaseDades) != 11-(size_t)j)
            return FALSE;
        /* Byte 11, Tipus de Camp */
        if (fwrite_function(&base_dades_XP->Camp[i].TipusDeCamp, 1, 1, base_dades_XP->pfBaseDades) != 1)
            return FALSE;
        /* Bytes 12 a 15 --> Reservats */
        if (fwrite_function(&base_dades_XP->Camp[i].reservat_1, 4, 1, base_dades_XP->pfBaseDades) != 1)
            return FALSE;
        /* Byte 16, o OFFSET_BYTESxCAMP_CAMP_CLASSIC --> BytesPerCamp */
        if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf) && base_dades_XP->Camp[i].TipusDeCamp=='C')
        {
            if (fwrite_function((void *)&byte_zero, 1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
            // De moment he escrit un zero. A OFFSET_BYTESxCAMP_CAMP_ESPECIAL escriuré el que toca.
        }
        else
        {
            if (fwrite_function(&base_dades_XP->Camp[i].BytesPerCamp, 1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
        }
        /* Byte 17 --> En els camps 'N' i 'F' indica els decimals.*/
        if(base_dades_XP->Camp[i].TipusDeCamp == 'N' || base_dades_XP->Camp[i].TipusDeCamp == 'F')
        {
            if (fwrite_function(&base_dades_XP->Camp[i].DecimalsSiEsFloat, 1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
        }
        else
        {
            if (fwrite_function(zero, 1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
        }
        if (MM_ES_DBF_ESTESA(base_dades_XP->versio_dbf) && base_dades_XP->Camp[i].TipusDeCamp=='C')
        {
            /* Bytes de 18 a 20 --> Reservats */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2, 20-18+1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
            /* Bytes de 21 a 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, longitud de camps especials, com els C
                                    en DBF esteses */
            if (fwrite_function(&base_dades_XP->Camp[i].BytesPerCamp, sizeof(MM_TIPUS_BYTES_PER_CAMP_DBF), 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;

            /* Bytes de 25 a 30 --> Reservats */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2[25-18], 30-25+1, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
        }
        else
        {
            /* Bytes de 21 a 24 --> OFFSET_BYTESxCAMP_CAMP_ESPECIAL, longitud de camps especials, que ja no ho són,
                                    com els C en DBF esteses */
            memset(base_dades_XP->Camp[i].reservat_2+MM_OFFSET_RESERVAT2_BYTESxCAMP_CAMP_ESPECIAL, '\0', 4);
            /* Bytes de 18 a 30 --> Reservats */
            if (fwrite_function(&base_dades_XP->Camp[i].reservat_2, 13, 1, base_dades_XP->pfBaseDades) != 1)
                return FALSE;
        }
        /* Byte 31 --> MDX flag.	*/
        if (fwrite_function(&base_dades_XP->Camp[i].MDX_camp_flag, 1, 1, base_dades_XP->pfBaseDades) != 1)
            return FALSE;
	}

    variable_byte = 13;
	if (fwrite_function(&variable_byte, 1, 1, base_dades_XP->pfBaseDades) != 1)
	    return FALSE;
    
    // Cal veure que tinguem el lloc suficient per a col·locar els noms estesos
    // no fós cas que ara tinguéssin, no més camps, sinó més noms estesos
    // o noms més llargs que abans.
    
    #ifdef NOT_USED_IN_GDAL
    if(base_dades_XP->OffsetPrimeraFitxa!=bytes_acumulats)
    {
        if(TraslladaFragmentFinalDeFitxer_64(base_dades_XP->pfBaseDades,
                bytes_acumulats, base_dades_XP->OffsetPrimeraFitxa,
                base_dades_XP->szNomFitxer))
        {
            return FALSE;
        }
        base_dades_XP->OffsetPrimeraFitxa=bytes_acumulats;
        // Tornem a escriure l'offset de la primera fitxa
        /* de 8 a 9, posició MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA */
        offset_actual=ftell_function(base_dades_XP->pfBaseDades);
        fseek_function(base_dades_XP->pfBaseDades, MM_PRIMER_OFFSET_a_OFFSET_1a_FITXA, SEEK_SET);
        if (fwrite_function(&(base_dades_XP->OffsetPrimeraFitxa), 2, 1, 
                    base_dades_XP->pfBaseDades) != 1)
            return FALSE;
        /* Bytes 30 a 31, a MM_SEGON_OFFSET_a_OFFSET_1a_FITXA */
        fseek_function(base_dades_XP->pfBaseDades, MM_SEGON_OFFSET_a_OFFSET_1a_FITXA, SEEK_SET);
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
        fseek_function(base_dades_XP->pfBaseDades, offset_actual, SEEK_SET);
    }
    #else
    if(base_dades_XP->OffsetPrimeraFitxa!=bytes_acumulats)
        return FALSE;    
    #endif //NOT_USED_IN_GDAL

    // Guardem els noms estesos (en els casos que en tinguem).
	for (i = 0; i < base_dades_XP->ncamps; i++)
	{
		if(MM_NOM_DBF_ESTES_I_VALID==MM_ISExtendedNameBD_XP(base_dades_XP->Camp[i].NomCamp))
        {
            bytes_acumulats=MM_GiveOffsetExtendedFieldName(base_dades_XP->Camp+i);
			mida_nom=MM_DonaBytesNomEstesCamp(base_dades_XP->Camp+i);

            fseek_function(base_dades_XP->pfBaseDades, bytes_acumulats, SEEK_SET);

        	// No podem usar el NomCamp (per ser massa gran) i guardem el que ja teniem guardat
            // a NomCampDBFClassica.
            // Vigilem en quin joc de caràcters escrivim
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
        base_dades_XP->pfBaseDades = NULL;
    }

	return TRUE;
} /* Fi de MM_UpdateEntireHeader() */

MM_BOOLEAN MM_CreateDBFFile(struct MM_BASE_DADES_XP * bd_xp, const char *NomFitxer)
{
	MM_CheckDBFHeader(bd_xp);
    if (NomFitxer)
	    strcpy(bd_xp->szNomFitxer, NomFitxer);
	return MM_UpdateEntireHeader(bd_xp);
}

void MM_ReleaseMainFields(struct MM_BASE_DADES_XP * base_dades_XP)
{
MM_NUMERATOR_DBF_FIELD_TYPE i;
size_t j;
char **cadena;

	if (base_dades_XP->Camp) /* Si la capçalera no té cap camp el punter és NULL */
	{
		for (i=0; i<base_dades_XP->ncamps; i++)
        {
            for (j=0; j<MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
            {
                cadena=base_dades_XP->Camp[i].separador;
                if (cadena[j])
                {
                    free(cadena[j]);
                    cadena[j]=NULL;
                }
            }
        }
		free(base_dades_XP->Camp);
		base_dades_XP->Camp = NULL;
        base_dades_XP->ncamps=0;
	}
	return;
}

void MM_ReleaseDBFHeader(struct MM_BASE_DADES_XP * base_dades_XP)
{
	if (base_dades_XP)
	{
        MM_ReleaseMainFields(base_dades_XP);
		free(base_dades_XP);
	}
	return;
}


int MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(struct MM_CAMP *camp,
			struct MM_BASE_DADES_XP * bd_xp, MM_BOOLEAN no_modifica_descriptor, size_t mida_nom)
{
MM_NUMERATOR_DBF_FIELD_TYPE i_camp;
unsigned n_digits_i=0, i;
int retorn=0;

	if (mida_nom==0)
        mida_nom=MM_MAX_LON_FIELD_NAME_DBF;

	for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
	{
		if (bd_xp->Camp+i_camp==camp)
			continue;
		if (!stricmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
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
				if (!stricmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
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
					if (!stricmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
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
				camp->NomCamp[strlen(camp->NomCamp)-2]='\0';  //Trec els 2 dígits.
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
						if (!stricmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
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
		if (!stricmp(bd_xp->Camp[i_camp].DescripcioCamp[0], camp->DescripcioCamp[0]))
			break;
	}
	if (i_camp==bd_xp->ncamps)
		return retorn;
	
    if (retorn==1)
	{
		if (strlen(camp->DescripcioCamp[0])>MM_MAX_LON_DESCRIPCIO_CAMP_DBF-4-n_digits_i)
			camp->DescripcioCamp[0][mida_nom-4-n_digits_i]='\0';
		sprintf(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]), " (%u)", i);
		for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
		{
			if (bd_xp->Camp+i_camp==camp)
				continue;
			if (!stricmp(bd_xp->Camp[i_camp].DescripcioCamp[0], camp->DescripcioCamp[0]))
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
		sprintf(camp->DescripcioCamp[0]+strlen(camp->DescripcioCamp[0]), " (%u)", i);
		for (i_camp=0; i_camp<bd_xp->ncamps; i_camp++)
		{
			if (bd_xp->Camp+i_camp==camp)
				continue;
			if (!stricmp(bd_xp->Camp[i_camp].NomCamp, camp->NomCamp))
				break;
		}
		if (i_camp==bd_xp->ncamps)
			return retorn;
	}
	return 2;
} // Fi de MM_ModifyFieldNameAndDescriptorIfPresentBD_XP()

int MM_DuplicateMultilingualString(char *(cadena_final[MM_NUM_IDIOMES_MD_MULTIDIOMA]), 
                               const char * const (cadena_inicial[MM_NUM_IDIOMES_MD_MULTIDIOMA]))
{
size_t i;

    for (i=0; i<MM_NUM_IDIOMES_MD_MULTIDIOMA; i++)
    {
        if (cadena_inicial[i])
        {
            if (NULL==(cadena_final[i]=strdup(cadena_inicial[i])))
                return 1;
        }
		else
			cadena_final[i]=NULL;
    }
    return 0;
}//Fi de MM_DuplicateMultilingualString()

int MM_DuplicateFieldDBXP(struct MM_CAMP *camp_final, const struct MM_CAMP *camp_inicial)
{
    *camp_final=*camp_inicial;

	if(0!=MM_DuplicateMultilingualString(camp_final->separador, (const char * const(*))camp_inicial->separador))
		return 1;
    
	return 0;
} //Fi de MM_DuplicateFieldDBXP()

char *MM_strnzcpy(char *dest, const char *src, size_t maxlen)
{
size_t i;
    if (!maxlen)
    	i=0;
    else
		strncpy(dest, src, i=maxlen-1);

    dest[i]='\0';
	return dest;
}

MM_BOOLEAN MM_FillFieldDB_XP(struct MM_CAMP *camp, const char *NomCamp, const char *DescripcioCamp, char TipusDeCamp,
					MM_TIPUS_BYTES_PER_CAMP_DBF BytesPerCamp, MM_BYTE DecimalsSiEsFloat, MM_BYTE mostrar_camp)
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
			strncpy(nom_temp, NomCamp, MM_MAX_LON_FIELD_NAME_DBF);
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
	camp->mostrar_camp=mostrar_camp;
	return TRUE;
}

#define szNomCampIdGraficDefecte    "ID_GRAFIC"
#define szNomCampPerimetreDefecte   "PERIMETRE"
#define szNomCampAreaDefecte        "AREA"
#define szNomCampLongitudArcDefecte "LONG_ARC"
#define szNomCampNodeIniDefecte     "NODE_INI"
#define szNomCampNodeFiDefecte      "NODE_FI"
#define szNomCampArcsANodeDefecte   "ARCS_A_NOD"
#define szNomCampTipusNodeDefecte   "TIPUS_NODE"
#define szNomCampNVertexsDefecte    "N_VERTEXS"
#define szNomCampNArcsDefecte       "N_ARCS"
#define szNomCampNPoligonsDefecte   "N_POLIG"

size_t MM_DefineFirstPolygonFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp,
				MM_BYTE n_decimals)
{
MM_NUMERATOR_DBF_FIELD_TYPE i_camp=0;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampIdGraficDefecte,
			"Internal Graphic identifier", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	bd_xp->CampIdGrafic=0;

	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNVertexsDefecte,
			"Number of vertices", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);

	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampPerimetreDefecte,
			"Perimeter of the polygon", 'N', MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals,
			(MM_BOOLEAN)TRUE);

	i_camp++;

    MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampAreaDefecte,
            "Area of the polygon", 'N',
            MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals,
            (MM_BOOLEAN)TRUE);
    i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNArcsDefecte,
			"Number of arcs", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNPoligonsDefecte,
			"Number of elemental polygons", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	i_camp++;

	//OmpleGeoTopoPerDefecteBD_XP(bd_xp, unitats_linials);

	return i_camp;
}

size_t MM_DefineFirstArcFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp, MM_BYTE n_decimals)
{
MM_NUMERATOR_DBF_FIELD_TYPE i_camp;

	i_camp=0;
	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampIdGraficDefecte,
			"Internal Graphic identifier", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	bd_xp->CampIdGrafic=0;

	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNVertexsDefecte,
			"Number of vertices", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);

	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampLongitudArcDefecte,
			"Lenght of arc", 'N', MM_MAX_AMPLADA_CAMP_N_DBF, n_decimals,
			TRUE);

	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNodeIniDefecte,
			"Initial node", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampNodeFiDefecte,
			"Final node", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	i_camp++;

	//OmpleGeoTopoPerDefecteBD_XP(bd_xp, unitats_linials);

	return i_camp;
}

size_t MM_DefineFirstNodeFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp)
{
MM_NUMERATOR_DBF_FIELD_TYPE i_camp;

	i_camp=0;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampIdGraficDefecte,
			"Internal Graphic identifier", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	bd_xp->CampIdGrafic=0;
	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampArcsANodeDefecte,
			"Number of arcs to node", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			TRUE);
	i_camp++;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampTipusNodeDefecte,
			"Node type", 'N',1, 0,TRUE);
	i_camp++;
	
	//OmpleGeoTopoPerDefecteBD_XP(bd_xp, CadenaBuida);
	return i_camp;
}

size_t MM_DefineFirstPointFieldsDB_XP(struct MM_BASE_DADES_XP *bd_xp)
{
size_t i_camp=0;

	MM_FillFieldDB_XP(bd_xp->Camp+i_camp, szNomCampIdGraficDefecte,
			"Internal Graphic identifier", 'N',
			MM_MAX_AMPLADA_CAMP_N_DBF, 0,
			FALSE);
	bd_xp->CampIdGrafic=0;

	i_camp++;
	//OmpleGeoTopoPerDefecteBD_XP(bd_xp, "\0");
	return i_camp;
}

void MM_WriteValueToRecordDBXP(char *registre, 
                                   const struct MM_CAMP *camp, 
                                   const void *valor,
                                   MM_BOOLEAN is_64)
{
char *p;

	if(camp->BytesPerCamp<MM_MESSAGE_LENGHT)
		p=local_message;
	else
		p=calloc_function(camp->BytesPerCamp+10);

	if (camp->TipusDeCamp=='N')
    {
        if(!is_64)
        {
    	    sprintf(p,
        		    "%*.*f",
                    camp->BytesPerCamp,
                    camp->DecimalsSiEsFloat,
                    *(const double *)valor);
        }
        else
        {
            sprintf(p,
        		    "%*lld",
                    camp->BytesPerCamp,
                    *(const __int64 *)valor);
        }
    }
    else
    	sprintf(p,
        		"%-*s",
                camp->BytesPerCamp,
                (const char *)valor);

    memcpy(registre+camp->BytesAcumulats, p, camp->BytesPerCamp);

	if(p!=local_message)
		free_function(p);
}

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
