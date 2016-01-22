/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  CEOS field layout recipes.
 * Author:   Paul Lahaie, pjlahaie@atlsci.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc
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

#include "ceos.h"

CPL_CVSID("$Id$");

/* Array of Datatypes and their names/values */

typedef struct { 
    char *String;
    int Type;
} CeosStringType_t;

typedef struct { 
    int (*function)(CeosSARVolume_t *volume, void *token);
    void *token;
    const char *name;
} RecipeFunctionData_t;


CeosStringType_t CeosDataType[] = { { "IU1", __CEOS_TYP_UCHAR },
				    { "IU2", __CEOS_TYP_USHORT },
				    { "UI1", __CEOS_TYP_UCHAR },
				    { "UI2", __CEOS_TYP_USHORT },
				    { "CI*2", __CEOS_TYP_COMPLEX_CHAR },
				    { "CI*4", __CEOS_TYP_COMPLEX_SHORT },
				    { "CIS4", __CEOS_TYP_COMPLEX_SHORT },
				    { "CI*8", __CEOS_TYP_COMPLEX_LONG },
				    { "C*8", __CEOS_TYP_COMPLEX_FLOAT },
				    { "R*4", __CEOS_TYP_FLOAT },
				    { NULL, 0 } };

CeosStringType_t CeosInterleaveType[] = { { "BSQ", __CEOS_IL_BAND },
					  { " BSQ", __CEOS_IL_BAND },
					  { "BIL", __CEOS_IL_LINE },
					  { " BIL", __CEOS_IL_LINE },
					  { NULL, 0 } };

#define IMAGE_OPT { 63, 192, 18, 18 }
#define IMAGE_JERS_OPT { 50, 192, 18, 18 }    /* Some JERS data uses this instead of IMAGE_OPT */
#define PROC_DATA_REC { 50, 11, 18, 20 }
#define PROC_DATA_REC_ALT { 50, 11, 31, 20 }
#define PROC_DATA_REC_ALT2 { 50, 11, 31, 50 }  /* Some cases of ERS 1, 2 */
#define DATA_SET_SUMMARY { 18, 10, 18, 20 }

/* NOTE: This seems to be the generic recipe used for most things */
CeosRecipeType_t RadarSatRecipe[] =
{
    { __CEOS_REC_NUMCHANS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      233, 4, __CEOS_REC_TYP_I }, /* Number of channels */
    { __CEOS_REC_INTERLEAVE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      269, 4, __CEOS_REC_TYP_A }, /* Interleaving type */
    { __CEOS_REC_DATATYPE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      429, 4, __CEOS_REC_TYP_A }, /* Data type */
    { __CEOS_REC_BPR, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      0, 0, __CEOS_REC_TYP_A }, /* For Defeault CEOS, this is done using other vals */
    { __CEOS_REC_LINES, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      237, 8, __CEOS_REC_TYP_I }, /* How many lines */
    { __CEOS_REC_TBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      261, 4, __CEOS_REC_TYP_I },
    { __CEOS_REC_BBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      265, 4, __CEOS_REC_TYP_I }, /* Bottom border pixels */
    { __CEOS_REC_PPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      249, 8, __CEOS_REC_TYP_I }, /* Pixels per line */
    { __CEOS_REC_LBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      245, 4, __CEOS_REC_TYP_I }, /* Left Border Pixels */
    { __CEOS_REC_RBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      257, 4, __CEOS_REC_TYP_I }, /* Isn't available for RadarSAT */
    { __CEOS_REC_BPP, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      225, 4, __CEOS_REC_TYP_I }, /* Bytes Per Pixel */
    { __CEOS_REC_RPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      273, 2, __CEOS_REC_TYP_I }, /* Records per line */
    { __CEOS_REC_PPR, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Pixels Per Record -- need to fill record type */
    { __CEOS_REC_PDBPR, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      281, 8, __CEOS_REC_TYP_I }, /* pixel data bytes per record */
    { __CEOS_REC_IDS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      277, 4, __CEOS_REC_TYP_I }, /* Prefix data per record */
    { __CEOS_REC_FDL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      9, 4, __CEOS_REC_TYP_B }, /* Length of Imagry Options Header */
    { __CEOS_REC_PIXORD, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Must be calculated */
    { __CEOS_REC_LINORD, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Must be calculated */
    { __CEOS_REC_PRODTYPE, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      0, 0, __CEOS_REC_TYP_I },

    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */

    /* Some ERS-1 products use an alternate data record subtype2. */
    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC_ALT,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */

    /* Yet another ERS-1 and ERS-2 alternate data record subtype2. */
    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC_ALT2,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */

    { __CEOS_REC_SUFFIX_SIZE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      289, 4, __CEOS_REC_TYP_I }, /* Suffix data per record */
    { 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0 } /* Last record is Zero */
} ;


CeosRecipeType_t JersRecipe[] =
{
    { __CEOS_REC_NUMCHANS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      233, 4, __CEOS_REC_TYP_I }, /* Number of channels */
    { __CEOS_REC_INTERLEAVE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      269, 4, __CEOS_REC_TYP_A }, /* Interleaving type */
    { __CEOS_REC_DATATYPE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      429, 4, __CEOS_REC_TYP_A }, /* Data type */
    { __CEOS_REC_BPR, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      0, 0, __CEOS_REC_TYP_A }, /* For Defeault CEOS, this is done using other vals */
    { __CEOS_REC_LINES, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      237, 8, __CEOS_REC_TYP_I }, /* How many lines */
    { __CEOS_REC_TBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      261, 4, __CEOS_REC_TYP_I },
    { __CEOS_REC_BBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      265, 4, __CEOS_REC_TYP_I }, /* Bottom border pixels */
    { __CEOS_REC_PPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      249, 8, __CEOS_REC_TYP_I }, /* Pixels per line */
    { __CEOS_REC_LBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      245, 4, __CEOS_REC_TYP_I }, /* Left Border Pixels */
    { __CEOS_REC_RBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      257, 4, __CEOS_REC_TYP_I }, /* Isn't available for RadarSAT */
    { __CEOS_REC_BPP, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      225, 4, __CEOS_REC_TYP_I }, /* Bytes Per Pixel */
    { __CEOS_REC_RPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      273, 2, __CEOS_REC_TYP_I }, /* Records per line */
    { __CEOS_REC_PPR, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Pixels Per Record -- need to fill record type */
    { __CEOS_REC_PDBPR, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      281, 8, __CEOS_REC_TYP_I }, /* pixel data bytes per record */
    { __CEOS_REC_IDS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      277, 4, __CEOS_REC_TYP_I }, /* Prefix data per record */
    { __CEOS_REC_FDL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      9, 4, __CEOS_REC_TYP_B }, /* Length of Imagry Options Header */
    { __CEOS_REC_PIXORD, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Must be calculated */
    { __CEOS_REC_LINORD, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      0, 0, __CEOS_REC_TYP_I }, /* Must be calculated */
    { __CEOS_REC_PRODTYPE, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      0, 0, __CEOS_REC_TYP_I },

    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */

    { __CEOS_REC_SUFFIX_SIZE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_JERS_OPT,
      289, 4, __CEOS_REC_TYP_I }, /* Suffix data per record */
    { 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0 } /* Last record is Zero */
} ;

CeosRecipeType_t ScanSARRecipe[] =
{
    { __CEOS_REC_NUMCHANS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      233, 4, __CEOS_REC_TYP_I }, /* Number of channels */
    { __CEOS_REC_INTERLEAVE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      269, 4, __CEOS_REC_TYP_A }, /* Interleaving type */
    { __CEOS_REC_DATATYPE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      429, 4, __CEOS_REC_TYP_A }, /* Data type */
    { __CEOS_REC_LINES, 1, __CEOS_ANY_FILE, DATA_SET_SUMMARY,
      325, 8, __CEOS_REC_TYP_I }, /* How many lines */
    { __CEOS_REC_PPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      249, 8, __CEOS_REC_TYP_I }, /* Pixels per line */
    { __CEOS_REC_BPP, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      225, 4, __CEOS_REC_TYP_I }, /* Bytes Per Pixel */
    { __CEOS_REC_RPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      273, 2, __CEOS_REC_TYP_I }, /* Records per line */
    { __CEOS_REC_IDS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      277, 4, __CEOS_REC_TYP_I }, /* Prefix data per record */
    { __CEOS_REC_FDL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      9, 4, __CEOS_REC_TYP_B }, /* Length of Imagry Options Header */
    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */
    { __CEOS_REC_SUFFIX_SIZE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      289, 4, __CEOS_REC_TYP_I }, /* Suffix data per record */
    { 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0 } /* Last record is Zero */
} ;

CeosRecipeType_t SIRCRecipe[] =
{
    { __CEOS_REC_NUMCHANS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      233, 4, __CEOS_REC_TYP_I }, /* Number of channels */
    { __CEOS_REC_INTERLEAVE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      269, 4, __CEOS_REC_TYP_A }, /* Interleaving type */
    { __CEOS_REC_DATATYPE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      429, 4, __CEOS_REC_TYP_A }, /* Data type */
    { __CEOS_REC_LINES, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      237, 8, __CEOS_REC_TYP_I }, /* How many lines */
    { __CEOS_REC_TBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      261, 4, __CEOS_REC_TYP_I },
    { __CEOS_REC_BBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      265, 4, __CEOS_REC_TYP_I }, /* Bottom border pixels */
    { __CEOS_REC_PPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      249, 8, __CEOS_REC_TYP_I }, /* Pixels per line */
    { __CEOS_REC_LBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      245, 4, __CEOS_REC_TYP_I }, /* Left Border Pixels */
    { __CEOS_REC_RBP, 0, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      257, 4, __CEOS_REC_TYP_I }, /* Right Border Pixels */
    { __CEOS_REC_BPP, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      225, 4, __CEOS_REC_TYP_I }, /* Bytes Per Pixel */
    { __CEOS_REC_RPL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      273, 2, __CEOS_REC_TYP_I }, /* Records per line */
    { __CEOS_REC_IDS, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      277, 4, __CEOS_REC_TYP_I }, /* Prefix data per record */
    { __CEOS_REC_FDL, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      9, 4, __CEOS_REC_TYP_B }, /* Length of Imagry Options Header */
    { __CEOS_REC_RECORDSIZE, 1, __CEOS_IMAGRY_OPT_FILE, PROC_DATA_REC,
      9, 4, __CEOS_REC_TYP_B }, /* The processed image record size */
    { __CEOS_REC_SUFFIX_SIZE, 1, __CEOS_IMAGRY_OPT_FILE, IMAGE_OPT,
      289, 4, __CEOS_REC_TYP_I }, /* Suffix data per record */

    { 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0 } /* Last record is Zero */
} ;

#undef PROC_DATA_REC

static void ExtractInt( CeosRecord_t *record, int type, unsigned int offset, unsigned int length, int *value );

static char *ExtractString( CeosRecord_t *record, unsigned int offset, unsigned int length, char *string );

static int GetCeosStringType(const CeosStringType_t *CeosType, const char *string);

static int SIRCRecipeFCN( CeosSARVolume_t *volume, void *token );
static int PALSARRecipeFCN( CeosSARVolume_t *volume, void *token );

Link_t *RecipeFunctions = NULL;

void RegisterRecipes( void )
{

    AddRecipe( SIRCRecipeFCN, SIRCRecipe, "SIR-C" );
    AddRecipe( ScanSARRecipeFCN, ScanSARRecipe, "ScanSAR" );
    AddRecipe( CeosDefaultRecipe, RadarSatRecipe, "RadarSat" );
    AddRecipe( CeosDefaultRecipe, JersRecipe, "Jers" );
    AddRecipe( PALSARRecipeFCN, RadarSatRecipe, "PALSAR-ALOS" );
    /*  AddRecipe( CeosDefaultRecipe, AtlantisRecipe ); */
}

void FreeRecipes( void )

{
    Link_t *link;

    for( link = RecipeFunctions; link != NULL; link = link->next )
        HFree( link->object );

    DestroyList( RecipeFunctions );
    RecipeFunctions = NULL;
}

void AddRecipe( int (*function)(CeosSARVolume_t *volume,
				void *token),
		void *token,
                const char *name )
{

    RecipeFunctionData_t *TempData;

    Link_t *Link;

    TempData = HMalloc( sizeof( RecipeFunctionData_t ) );

    TempData->function = function;
    TempData->token = token;
    TempData->name = name;

    Link = ceos2CreateLink( TempData );

    if( RecipeFunctions == NULL)
    {
	RecipeFunctions = Link;
    } else {
	RecipeFunctions = InsertLink( RecipeFunctions, Link );
    }
}

int CeosDefaultRecipe( CeosSARVolume_t *volume, void *token )
{
    CeosRecipeType_t *recipe;
    CeosRecord_t *record;
    CeosTypeCode_t TypeCode;
    struct CeosSARImageDesc *ImageDesc = &(volume->ImageDesc);
    char temp_str[1024];
    int i /*, temp_int */;

#define DoExtractInt(a) ExtractInt( record, recipe[i].Type, recipe[i].Offset, recipe[i].Length, &a)

    if(token == NULL)
    {
	return 0;
    }

    memset(ImageDesc, 0, sizeof( struct CeosSARImageDesc ) );

/*    temp_imagerecipe = (CeosSARImageDescRecipe_t *) token;
    recipe = temp_imagerecipe->Recipe; */

    recipe = token;

    for(i = 0; recipe[i].ImageDescValue != 0; i++ )
    {
	if(recipe[i].Override)
	{
	    TypeCode.UCharCode.Subtype1 = recipe[i].TypeCode.Subtype1;
	    TypeCode.UCharCode.Type = recipe[i].TypeCode.Type;
	    TypeCode.UCharCode.Subtype2 = recipe[i].TypeCode.Subtype2;
	    TypeCode.UCharCode.Subtype3 = recipe[i].TypeCode.Subtype3;

	    record = FindCeosRecord( volume->RecordList, TypeCode, recipe[i].FileId, -1, -1 );

	    if(record == NULL)
	    {
              /* temp_int = 0; */
	    } else {

		switch( recipe[i].ImageDescValue )
		{
		case __CEOS_REC_NUMCHANS:
		   DoExtractInt( ImageDesc->NumChannels );
		   break;
		case __CEOS_REC_LINES:
		    DoExtractInt( ImageDesc->Lines );
		    break;
		case __CEOS_REC_BPP:
		    DoExtractInt( ImageDesc->BytesPerPixel );
		    break;
		case __CEOS_REC_RPL:
		    DoExtractInt( ImageDesc->RecordsPerLine );
		    break;
		case __CEOS_REC_PDBPR:
		    DoExtractInt( ImageDesc->PixelDataBytesPerRecord );
		    break;
		case __CEOS_REC_FDL:
		    DoExtractInt( ImageDesc->FileDescriptorLength );
		    break;
		case __CEOS_REC_IDS:
		    DoExtractInt( ImageDesc->ImageDataStart );
                    /* 
                    ** This is really reading the quantity of prefix data
                    ** per data record.  We want the offset from the very
                    ** beginning of the record to the data, so we add another
                    ** 12 to that.  I think some products incorrectly indicate
                    ** 192 (prefix+12) instead of 180 so if we see 192 assume
                    ** the 12 bytes of record start data has already been
                    ** added.  Frank Warmerdam.
                    */
		    if( ImageDesc->ImageDataStart != 192 )
                        ImageDesc->ImageDataStart += 12;
		    break;
		case __CEOS_REC_SUFFIX_SIZE:
		    DoExtractInt( ImageDesc->ImageSuffixData );
		    break;
		case __CEOS_REC_RECORDSIZE:
		    DoExtractInt( ImageDesc->BytesPerRecord );
		    break;
		case __CEOS_REC_PPL:
		    DoExtractInt( ImageDesc->PixelsPerLine );
		    break;
		case __CEOS_REC_TBP:
		    DoExtractInt( ImageDesc->TopBorderPixels );
		    break;
		case __CEOS_REC_BBP:
		    DoExtractInt( ImageDesc->BottomBorderPixels );
		    break;
		case __CEOS_REC_LBP:
		    DoExtractInt( ImageDesc->LeftBorderPixels );
		    break;
		case __CEOS_REC_RBP:
		    DoExtractInt( ImageDesc->RightBorderPixels );
		    break;
		case __CEOS_REC_INTERLEAVE:
		    ExtractString( record, recipe[i].Offset, recipe[i].Length, temp_str );

		    ImageDesc->ChannelInterleaving = GetCeosStringType( CeosInterleaveType, temp_str );
		    break;
		case __CEOS_REC_DATATYPE:
		    ExtractString( record, recipe[i].Offset, recipe[i].Length, temp_str );

		    ImageDesc->DataType = GetCeosStringType( CeosDataType, temp_str );
		    break;
		}
		
	    }
	}
    }

    /* Some files (Telaviv) don't record the number of pixel groups per line.
     * Try to derive it from the size of a data group, and the number of
     * bytes of pixel data if necessary.
     */

    if( ImageDesc->PixelsPerLine == 0 
        && ImageDesc->PixelDataBytesPerRecord != 0 
        && ImageDesc->BytesPerPixel != 0 )
    {
        ImageDesc->PixelsPerLine = 
            ImageDesc->PixelDataBytesPerRecord / ImageDesc->BytesPerPixel;
        CPLDebug( "SAR_CEOS", "Guessing PixelPerLine to be %d\n", 
                  ImageDesc->PixelsPerLine );
    }

    /* Some files don't have the BytesPerRecord stuff, so we calculate it if possible */

    if( ImageDesc->BytesPerRecord == 0 && ImageDesc->RecordsPerLine == 1 &&
	ImageDesc->PixelsPerLine > 0 && ImageDesc->BytesPerPixel > 0 )
    {
        CeosRecord_t *img_rec;

        ImageDesc->BytesPerRecord = ImageDesc->PixelsPerLine *
	  ImageDesc->BytesPerPixel + ImageDesc->ImageDataStart +
	  ImageDesc->ImageSuffixData ;

        TypeCode.UCharCode.Subtype1 = 0xed;
        TypeCode.UCharCode.Type = 0xed;
        TypeCode.UCharCode.Subtype2 = 0x12;
        TypeCode.UCharCode.Subtype3 = 0x12;
        
        img_rec = FindCeosRecord( volume->RecordList, TypeCode, 
                                  __CEOS_IMAGRY_OPT_FILE, -1, -1 );
        if( img_rec == NULL )
        {
            CPLDebug( "SAR_CEOS", 
                      "Unable to find imagery rec to check record length." );
            return 0;
        }

        if( img_rec->Length != ImageDesc->BytesPerRecord )
        {
            CPLDebug( "SAR_CEOS", 
                      "Guessed record length (%d) did not match\n"
                      "actual imagery record length (%d), recipe fails.", 
                      ImageDesc->BytesPerRecord, img_rec->Length );
            return 0;
        }
    }
    
    if( ImageDesc->PixelsPerRecord == 0 && 
	ImageDesc->BytesPerRecord != 0 && ImageDesc->BytesPerPixel != 0 )
    {
	ImageDesc->PixelsPerRecord = ( ( ImageDesc->BytesPerRecord -
					 (ImageDesc->ImageSuffixData +
					  ImageDesc->ImageDataStart )) /
				       ImageDesc->BytesPerPixel );

	if(ImageDesc->PixelsPerRecord > ImageDesc->PixelsPerLine)
	    ImageDesc->PixelsPerRecord = ImageDesc->PixelsPerLine;
    }

    /* If we didn't get a data type, try guessing. */
    if( ImageDesc->DataType == 0
        && ImageDesc->BytesPerPixel != 0
        && ImageDesc->NumChannels != 0 )
    {
        int  nDataTypeSize = ImageDesc->BytesPerPixel / ImageDesc->NumChannels;

        if( nDataTypeSize == 1 )
            ImageDesc->DataType = __CEOS_TYP_UCHAR;
        else if( nDataTypeSize == 2 )
            ImageDesc->DataType = __CEOS_TYP_USHORT;
    }

    /* Sanity checking */
    
    if( ImageDesc->PixelsPerLine == 0 || ImageDesc->Lines == 0 ||
	ImageDesc->RecordsPerLine == 0 || ImageDesc->ImageDataStart == 0 ||
	ImageDesc->FileDescriptorLength == 0 || ImageDesc->DataType == 0 ||
	ImageDesc->NumChannels == 0 || ImageDesc->BytesPerPixel == 0 ||
	ImageDesc->ChannelInterleaving == 0 || ImageDesc->BytesPerRecord == 0)
    {
	return 0;
    } else {
	
	ImageDesc->ImageDescValid = TRUE;
	return 1;
    }
}

int ScanSARRecipeFCN( CeosSARVolume_t *volume, void *token )
{
    struct CeosSARImageDesc *ImageDesc = &(volume->ImageDesc);

    memset( ImageDesc, 0, sizeof( struct CeosSARImageDesc ) );

    if( CeosDefaultRecipe( volume, token ) )
    {
	ImageDesc->Lines *= 2;
	return 1;
    }

    return 0;
}    

static int SIRCRecipeFCN( CeosSARVolume_t *volume, void *token )
{
    struct CeosSARImageDesc *ImageDesc = &(volume->ImageDesc);
    CeosTypeCode_t TypeCode;
    CeosRecord_t *record;
    char szSARDataFormat[29];

    memset( ImageDesc, 0, sizeof( struct CeosSARImageDesc ) );

/* -------------------------------------------------------------------- */
/*      First, we need to check if the "SAR Data Format Type            */
/*      identifier" is set to "COMPRESSED CROSS-PRODUCTS" which is      */
/*      pretty idiosyncratic to SIRC products.  It might also appear    */
/*      for some other similarly encoded Polarimetric data I suppose.    */
/* -------------------------------------------------------------------- */
    /* IMAGE_OPT */
    TypeCode.UCharCode.Subtype1 = 63;
    TypeCode.UCharCode.Type     = 192;
    TypeCode.UCharCode.Subtype2 = 18;
    TypeCode.UCharCode.Subtype3 = 18;

    record = FindCeosRecord( volume->RecordList, TypeCode, 
                             __CEOS_IMAGRY_OPT_FILE, -1, -1 );
    if( record == NULL )
        return 0;

    ExtractString( record, 401, 28, szSARDataFormat );
    if( !EQUALN( szSARDataFormat, "COMPRESSED CROSS-PRODUCTS", 25) )
        return 0;

/* -------------------------------------------------------------------- */
/*      Apply normal handling...                                        */
/* -------------------------------------------------------------------- */
    CeosDefaultRecipe( volume, token );

/* -------------------------------------------------------------------- */
/*      Make sure this looks like the SIRC product we are expecting.    */
/* -------------------------------------------------------------------- */
    if( ImageDesc->BytesPerPixel != 10 )
        return 0;

/* -------------------------------------------------------------------- */
/*      Then fix up a few values.                                       */
/* -------------------------------------------------------------------- */
    /* It seems the bytes of pixel data per record is just wrong.  Fix. */
    ImageDesc->PixelDataBytesPerRecord = 
        ImageDesc->BytesPerPixel * ImageDesc->PixelsPerLine;
    
    ImageDesc->DataType = __CEOS_TYP_CCP_COMPLEX_FLOAT;

/* -------------------------------------------------------------------- */
/*      Sanity checking                                                 */
/* -------------------------------------------------------------------- */
    if( ImageDesc->PixelsPerLine == 0 || ImageDesc->Lines == 0 ||
	ImageDesc->RecordsPerLine == 0 || ImageDesc->ImageDataStart == 0 ||
	ImageDesc->FileDescriptorLength == 0 || ImageDesc->DataType == 0 ||
	ImageDesc->NumChannels == 0 || ImageDesc->BytesPerPixel == 0 ||
	ImageDesc->ChannelInterleaving == 0 || ImageDesc->BytesPerRecord == 0)
    {
	return 0;
    } else {
	
	ImageDesc->ImageDescValid = TRUE;
	return 1;
    }
}    

static int PALSARRecipeFCN( CeosSARVolume_t *volume, void *token )
{
    struct CeosSARImageDesc *ImageDesc = &(volume->ImageDesc);
    CeosTypeCode_t TypeCode;
    CeosRecord_t *record;
    char szSARDataFormat[29], szProduct[32];

    memset( ImageDesc, 0, sizeof( struct CeosSARImageDesc ) );

/* -------------------------------------------------------------------- */
/*      First, we need to check if the "SAR Data Format Type            */
/*      identifier" is set to "COMPRESSED CROSS-PRODUCTS" which is      */
/*      pretty idiosyncratic to SIRC products.  It might also appear    */
/*      for some other similarly encoded Polarimetric data I suppose.    */
/* -------------------------------------------------------------------- */
    /* IMAGE_OPT */
    TypeCode.UCharCode.Subtype1 = 63;
    TypeCode.UCharCode.Type     = 192;
    TypeCode.UCharCode.Subtype2 = 18;
    TypeCode.UCharCode.Subtype3 = 18;

    record = FindCeosRecord( volume->RecordList, TypeCode, 
                             __CEOS_IMAGRY_OPT_FILE, -1, -1 );
    if( record == NULL )
        return 0;

    ExtractString( record, 401, 28, szSARDataFormat );
    if( !EQUALN( szSARDataFormat, "INTEGER*18                 ", 25) )
        return 0;

    ExtractString( record, 49, 16, szProduct );
    if( !EQUALN( szProduct, "ALOS-", 5 ) )
        return 0;

/* -------------------------------------------------------------------- */
/*      Apply normal handling...                                        */
/* -------------------------------------------------------------------- */
    CeosDefaultRecipe( volume, token );

/* -------------------------------------------------------------------- */
/*      Make sure this looks like the SIRC product we are expecting.    */
/* -------------------------------------------------------------------- */
    if( ImageDesc->BytesPerPixel != 18 )
        return 0;

/* -------------------------------------------------------------------- */
/*      Then fix up a few values.                                       */
/* -------------------------------------------------------------------- */
    ImageDesc->DataType = __CEOS_TYP_PALSAR_COMPLEX_SHORT;
    ImageDesc->NumChannels = 6;

/* -------------------------------------------------------------------- */
/*      Sanity checking                                                 */
/* -------------------------------------------------------------------- */
    if( ImageDesc->PixelsPerLine == 0 || ImageDesc->Lines == 0 ||
	ImageDesc->RecordsPerLine == 0 || ImageDesc->ImageDataStart == 0 ||
	ImageDesc->FileDescriptorLength == 0 || ImageDesc->DataType == 0 ||
	ImageDesc->NumChannels == 0 || ImageDesc->BytesPerPixel == 0 ||
	ImageDesc->ChannelInterleaving == 0 || ImageDesc->BytesPerRecord == 0)
    {
	return 0;
    } else {
	
	ImageDesc->ImageDescValid = TRUE;
	return 1;
    }
}    

void GetCeosSARImageDesc( CeosSARVolume_t *volume )
{
    Link_t *link;
    RecipeFunctionData_t *rec_data;
    int (*function)(CeosSARVolume_t *volume, void *token);

    if( RecipeFunctions == NULL )
    {
	RegisterRecipes();
    }

    if(RecipeFunctions == NULL )
    {
	return ;
    }

    for(link = RecipeFunctions; link != NULL; link = link->next)
    {
	if(link->object)
	{
	    rec_data = link->object;
            function = rec_data->function;
	    if(( *function )( volume, rec_data->token ) )
            {
                CPLDebug( "CEOS", "Using recipe '%s'.", 
                          rec_data->name );
		return;
            }
	}
    }

    return ;

}


static void ExtractInt(CeosRecord_t *record, int type, unsigned int offset, unsigned int length, int *value)
{
    void *buffer;
    char format[32];

    buffer = HMalloc( length + 1 );

    switch(type)
    {
    case __CEOS_REC_TYP_A:
	sprintf( format, "A%u", length );
	GetCeosField( record, offset, format,  buffer );
	*value = atoi( buffer );
	break;
    case __CEOS_REC_TYP_B:
	sprintf( format, "B%u", length );
#ifdef notdef
	GetCeosField( record, offset, format, buffer );
	if( length <= 4 )
	    CeosToNative( value, buffer, length, length );
	else
	    *value = 0;
#else
	GetCeosField( record, offset, format, value );
#endif
	break;
    case __CEOS_REC_TYP_I:
	sprintf( format, "I%u", length );
	GetCeosField( record, offset, format, value );
	break;
    }

    HFree( buffer );

}

static char *ExtractString( CeosRecord_t *record, unsigned int offset, unsigned int length, char *string )
{
    char format[12];

    if(string == NULL)
    {
	string = HMalloc( length + 1 );
    }

    sprintf( format, "A%u", length );

    GetCeosField( record, offset, format, string );

    return string;
}

static int GetCeosStringType(const CeosStringType_t *CeosStringType, const char *string)
{
    int i;

    for(i = 0;CeosStringType[i].String != NULL;i++)
    {
	if(strncmp(CeosStringType[i].String ,string, strlen( CeosStringType[i].String ) ) == 0 )
	{
	    return CeosStringType[i].Type;
	}
    }

    return 0;
}
