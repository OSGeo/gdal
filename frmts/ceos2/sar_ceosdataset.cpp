/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  GDALDataset driver for CEOS translator.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2000/03/31 13:32:49  warmerda
 * New
 *
 */

#include "ceos.h"
#include "gdal_priv.h"
#include "../raw/rawdataset.h"

static GDALDriver	*poCEOSDriver = NULL;

CPL_C_START
void	GDALRegister_SAR_CEOS(void);
CPL_C_END

char *CeosExtension[7][5] = { 
{ "vol", "led", "img", "trl", "nul" },
{ "vol", "lea", "img", "trl", "nul" },
{ "vol", "led", "img", "tra", "nul" },
{ "vol", "lea", "img", "tra", "nul" },
{ NULL, NULL, NULL, NULL, NULL },
{ "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_vdf" },
{ NULL, NULL, NULL, NULL, NULL } 
};

#define RSAT 5

/************************************************************************/
/*                            ProcessData()                             */
/************************************************************************/
static int 
ProcessData( FILE *fp, int fileid, CeosSARVolume_t *sar, int max_records, 
             int max_bytes )

{
    unsigned char      temp_buffer[__CEOS_HEADER_LENGTH];
    unsigned char      *temp_body = NULL;
    int                start = 0;
    int                CurrentBodyLength = 0;
    int                CurrentType = 0;
    int                CurrentSequence = 0;
    Link_t             *TheLink;
    CeosRecord_t       *record;

    while(max_records != 0 && max_bytes != 0)
    {
	record = (CeosRecord_t *) CPLMalloc( sizeof( CeosRecord_t ) );
        VSIFSeek( fp, start, SEEK_SET );
        VSIFRead( temp_buffer, 1, __CEOS_HEADER_LENGTH, fp );
	record->Length = DetermineCeosRecordBodyLength( temp_buffer );

	if( record->Length > CurrentBodyLength )
	{
	    if(CurrentBodyLength == 0 )
		temp_body = (unsigned char *) CPLMalloc( record->Length );
	    else
	    {
		temp_body = (unsigned char *) 
                    CPLRealloc( temp_body, record->Length );
                CurrentBodyLength = record->Length;
            }
	}

        VSIFRead( temp_body, 1, record->Length - __CEOS_HEADER_LENGTH, fp );

	InitCeosRecordWithHeader( record, temp_buffer, temp_body );

	if( CurrentType == record->TypeCode.Int32Code )
	    record->Subsequence = ++CurrentSequence;
	else {
	    CurrentType = record->TypeCode.Int32Code;
	    record->Subsequence = CurrentSequence = 0;
	}

	record->FileId = fileid;

	TheLink = CreateLink( record );

	if( sar->RecordList == NULL )
	    sar->RecordList = TheLink;
	else
	    sar->RecordList = InsertLink( sar->RecordList, TheLink );

	start += record->Length;

	if(max_records > 0)
	    max_records--;
	if(max_bytes > 0)
        {
	    max_bytes -= record->Length;
            if(max_bytes < 0)
                max_bytes = 0;
        }
    }

    CPLFree(temp_body);

    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*				SAR_CEOSDataset				*/
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSDataset : public GDALDataset
{
    CeosSARVolume_t sVolume;

    FILE	*fpImage;

  public:
                ~SAR_CEOSDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                          ~SAR_CEOSDataset()                          */
/************************************************************************/

SAR_CEOSDataset::~SAR_CEOSDataset()

{
    if( fpImage != NULL )
        VSIFClose( fpImage );

    if( sVolume.RecordList )
    {
        Link_t	*Links;

        for(Links = sVolume.RecordList; Links != NULL; Links = Links->next)
        {
            if(Links->object)
            {
                DeleteCeosRecord( (CeosRecord_t *) Links->object );
                Links->object = NULL;
            }
        }
        DestroyList( sVolume.RecordList );
    }
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAR_CEOSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNative;
    
/* -------------------------------------------------------------------- */
/*      Does this appear to be a valid ceos leader record?              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL 
        || poOpenInfo->nHeaderBytes < __CEOS_HEADER_LENGTH )
        return NULL;

    if( (poOpenInfo->pabyHeader[4] != 0x3f
         && poOpenInfo->pabyHeader[4] != 0x32)
        || poOpenInfo->pabyHeader[5] != 0xc0
        || poOpenInfo->pabyHeader[6] != 0x12
        || poOpenInfo->pabyHeader[7] != 0x12 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SAR_CEOSDataset 	*poDS;
    CeosSARVolume_t     *psVolume;

    poDS = new SAR_CEOSDataset();

    poDS->poDriver = poCEOSDriver;

    psVolume = &(poDS->sVolume);
    InitCeosSARVolume( psVolume, 0 );

/* -------------------------------------------------------------------- */
/*      Try to read the current file as an imagery file.                */
/* -------------------------------------------------------------------- */
    psVolume->ImagryOptionsFile = TRUE;
    if( ProcessData( poOpenInfo->fp, __CEOS_IMAGRY_OPT_FILE, psVolume, 4, -1) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Corrupted or unknown CEOS format:\n%s", 
                  poOpenInfo->pszFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      If it has an extension, process using RadarSAT naming scheme.   */
/* -------------------------------------------------------------------- */
    if( CPLGetExtension( poOpenInfo->pszFilename ) != NULL
        && strlen(CPLGetExtension( poOpenInfo->pszFilename )) > 0 )
    {
	int num = atoi( CPLGetExtension( poOpenInfo->pszFilename ) );
        char *pszPath;
        char *pszExtension; 

        pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        pszExtension = CPLStrdup(CPLGetExtension(poOpenInfo->pszFilename));

	for( i = 0; i < 5; i++ )
	{
            char temp_str[24];
            const char *pszFilename;
            FILE	*process_fp;

            /* we have already done the imagery file */
	    if( i == 2)
                continue;

            sprintf( temp_str, CeosExtension[RSAT][i], num );
            
            pszFilename = CPLFormFilename(pszPath,temp_str,pszExtension);
            process_fp = VSIFOpen( pszFilename, "rb" );

            if( process_fp != NULL )
            {
                VSIFSeek( process_fp, 0, SEEK_END );
                
                if( ProcessData( process_fp, i, psVolume, -1, 
                                 VSIFTell( process_fp ) ) == 0 )
                {
                    switch( i )
                    {
                      case 0: psVolume->VolumeDirectoryFile = TRUE;
                        break;
                      case 1: psVolume->SARLeaderFile = TRUE;
                        break;
                      case 3: psVolume->SARTrailerFile = TRUE;
                        break;
                      case 4: psVolume->NullVolumeDirectoryFile = TRUE;
                        break;
                    }
                }
                
                VSIFClose( process_fp );
            }
	}
        
        CPLFree( pszPath );
        CPLFree( pszExtension );
    }

/* -------------------------------------------------------------------- */
/*      Non-Radardsat naming convention.                                */
/* -------------------------------------------------------------------- */
    else
    {
        char *pszPath;
        char *pszBasename;

        pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

	for( i = 0; i < 5;i++ )
	{
            int	e;

            /* skip image file ... we already did it */
	    if( i == 2 )
                continue;

            e = 0;
            while( CeosExtension[e][i] != NULL )
            {
                FILE	*process_fp;
                const char *pszFilename;

                pszFilename = 
                    CPLFormFilename(pszPath,pszBasename,CeosExtension[e][i]);

                process_fp = VSIFOpen( pszFilename, "rb" );
                if( process_fp != NULL )
                {
                    VSIFSeek( process_fp, 0, SEEK_END );
                    if( ProcessData( process_fp, i, psVolume, -1, 
                                     VSIFTell( process_fp ) ) == 0 )
                    {
                        switch( i )
                        {
                          case 0: psVolume->VolumeDirectoryFile = TRUE;
                            break;
                          case 1: psVolume->SARLeaderFile = TRUE;
                            break;
                          case 3: psVolume->SARTrailerFile = TRUE;
                            break;
                          case 4: psVolume->NullVolumeDirectoryFile = TRUE;
                            break;
                        }

                        VSIFClose( process_fp );
                        break; /* Exit the while loop, we have this data type*/
                    }
                    
                    VSIFClose( process_fp );
                }
                e++;
            }
	}

        CPLFree( pszPath );
        CPLFree( pszBasename );
    }

/* -------------------------------------------------------------------- */
/*      Check that we have an image description.                        */
/* -------------------------------------------------------------------- */
    struct CeosSARImageDesc   *psImageDesc;

    GetCeosSARImageDesc( psVolume );
    psImageDesc = &(psVolume->ImageDesc);
    if( !psImageDesc->ImageDescValid )
    {
        delete poDS;

        CPLDebug( "CEOS", 
                  "Unable to extract CEOS image description\n"
                  "from %s.", 
                  poOpenInfo->pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish image type.                                           */
/* -------------------------------------------------------------------- */
    GDALDataType eType;
    int		 StartData;

    switch( psImageDesc->DataType )
    {
      case __CEOS_TYP_CHAR:
      case __CEOS_TYP_UCHAR:
      case __CEOS_TYP_COMPLEX_CHAR:
      case __CEOS_TYP_COMPLEX_UCHAR:
        eType = GDT_Byte;
        break;

      case __CEOS_TYP_SHORT:
      case __CEOS_TYP_COMPLEX_SHORT:
        eType = GDT_Int16;
        break;

      case __CEOS_TYP_USHORT:
      case __CEOS_TYP_COMPLEX_USHORT:
        eType = GDT_UInt16;
        break;

      case __CEOS_TYP_LONG:
      case __CEOS_TYP_ULONG:
      case __CEOS_TYP_COMPLEX_LONG:
      case __CEOS_TYP_COMPLEX_ULONG:
      case __CEOS_TYP_FLOAT:
      case __CEOS_TYP_DOUBLE:
        eType = GDT_Float32;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported CEOS image data type %d.\n", 
                  psImageDesc->DataType );
        delete poDS;
        return NULL;
    }

    
    CalcCeosSARImageFilePosition( psVolume, 1, 1, NULL, &StartData );
    
    StartData += psImageDesc->ImageDataStart;

    if( psImageDesc->DataType >= __CEOS_TYP_COMPLEX_CHAR )
        poDS->nBands = 2 * psImageDesc->NumChannels;
    else
        poDS->nBands = psImageDesc->NumChannels;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psImageDesc->PixelsPerLine;
    poDS->nRasterYSize = psImageDesc->Lines;

#ifdef CPL_LSB
    bNative = FALSE;
#else
    bNative = TRUE;
#endif
    
/* -------------------------------------------------------------------- */
/*      Roll our own ...                                                */
/* -------------------------------------------------------------------- */
    if( psImageDesc->RecordsPerLine > 1
        || psImageDesc->DataType == __CEOS_TYP_CHAR
        || psImageDesc->DataType == __CEOS_TYP_LONG
        || psImageDesc->DataType == __CEOS_TYP_ULONG
        || psImageDesc->DataType == __CEOS_TYP_DOUBLE
        || psImageDesc->DataType >= __CEOS_TYP_COMPLEX_CHAR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                 "Support for ceos files with unusual data types, and\n"
                 "scanlines split over multiple records not yet implemented.");

        delete poDS;

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Use raw services for well behaved files.                        */
/* -------------------------------------------------------------------- */
    else
    {
        int	nLineSize, nLineSize2;

        CalcCeosSARImageFilePosition( psVolume, 1, 1, NULL, &nLineSize );
        CalcCeosSARImageFilePosition( psVolume, 1, 2, NULL, &nLineSize2 );

        nLineSize = nLineSize2 - nLineSize;
        
        for( int iBand = 0; iBand < psImageDesc->NumChannels; iBand++ )
        {
            RawRasterBand *poBand = NULL;

            if( psImageDesc->ChannelInterleaving == __CEOS_IL_PIXEL )
            {
                int	nStartData;

                CalcCeosSARImageFilePosition(psVolume,1,1,NULL,&nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nStartData += psImageDesc->BytesPerPixel * iBand;
                
                poBand = 
                    new RawRasterBand( 
                        poDS, iBand+1, poOpenInfo->fp, nStartData, 
                        psImageDesc->BytesPerPixel * psImageDesc->NumChannels,
                        nLineSize, eType, bNative );
            }
            else if( psImageDesc->ChannelInterleaving == __CEOS_IL_LINE )
            {
                int	nStartData;

                CalcCeosSARImageFilePosition(psVolume, iBand+1, 1, NULL,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                
                poBand = 
                    new RawRasterBand( 
                        poDS, iBand+1, poOpenInfo->fp, nStartData, 
                        psImageDesc->BytesPerPixel,
                        nLineSize * psImageDesc->NumChannels, 
                        eType, bNative );
            }
            else if( psImageDesc->ChannelInterleaving == __CEOS_IL_BAND )
            {
                int	nStartData;

                CalcCeosSARImageFilePosition(psVolume, iBand+1, 1, NULL,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                
                poBand = 
                    new RawRasterBand( 
                        poDS, iBand+1, poOpenInfo->fp, nStartData, 
                        psImageDesc->BytesPerPixel,
                        nLineSize, eType, bNative );
            }
            else
            {
                CPLAssert( FALSE );
                return NULL;
            }

            poDS->SetBand( iBand+1, poBand );
        }
    }

    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    return( poDS );
}

/************************************************************************/
/*                       GDALRegister_SAR_CEOS()                        */
/************************************************************************/

void GDALRegister_SAR_CEOS()

{
    GDALDriver	*poDriver;

    if( poCEOSDriver == NULL )
    {
        poCEOSDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "SAR_CEOS";
        poDriver->pszLongName = "CEOS SAR Image";
        
        poDriver->pfnOpen = SAR_CEOSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
