/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Reader for ENVISAT format image data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Atlantis Scientific, Inc.
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
 * Revision 1.1  2001/02/13 18:29:04  warmerda
 * New
 *
 */

#include "frmts/raw/rawdataset.h"
#include "cpl_string.h"

CPL_C_START
#include "EnvisatFile.h"
CPL_C_END

static GDALDriver	*poEnvisatDriver = NULL;

CPL_C_START
void	GDALRegister_Envisat(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				EnvisatDataset				*/
/* ==================================================================== */
/************************************************************************/

class EnvisatDataset : public RawDataset
{
    EnvisatFile *hEnvisatFile;
    FILE	*fpImage;

  public:
    		EnvisatDataset();
    	        ~EnvisatDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*				EnvisatDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            EnvisatDataset()                             */
/************************************************************************/

EnvisatDataset::EnvisatDataset()
{
    hEnvisatFile = NULL;
}

/************************************************************************/
/*                            ~EnvisatDataset()                            */
/************************************************************************/

EnvisatDataset::~EnvisatDataset()

{
    if( hEnvisatFile != NULL )
        EnvisatFile_Close( hEnvisatFile );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *EnvisatDataset::Open( GDALOpenInfo * poOpenInfo )

{
    EnvisatFile	*hEnvisatFile;
    
/* -------------------------------------------------------------------- */
/*      Check the header.                                               */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 8 || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "PRODUCT=",8) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int		ds_index;

    if( EnvisatFile_Open( &hEnvisatFile, poOpenInfo->pszFilename, "r" ) 
        == FAILURE )
        return NULL;

    ds_index = EnvisatFile_GetDatasetIndex( hEnvisatFile, "MDS1" );
    if( ds_index == -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find \"MDS1\" datatset in Envisat file.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    EnvisatDataset 	*poDS;

    poDS = new EnvisatDataset();

    poDS->hEnvisatFile = hEnvisatFile;
    poDS->poDriver = poEnvisatDriver;

/* -------------------------------------------------------------------- */
/*      Setup image definition.                                         */
/* -------------------------------------------------------------------- */
    int		dsr_size, num_dsr, ds_offset, bNative;
    const char  *pszDataType, *pszSampleType;
    GDALDataType eDataType;

    EnvisatFile_GetDatasetInfo( hEnvisatFile, ds_index, 
                                NULL, NULL, NULL, &ds_offset, NULL, 
                                &num_dsr, &dsr_size );

    poDS->nRasterXSize = EnvisatFile_GetKeyValueAsInt( hEnvisatFile, SPH,
                                                       "LINE_LENGTH", 0 );
    poDS->nRasterYSize = num_dsr;
    poDS->nBands = 1;
    poDS->eAccess = GA_ReadOnly;

    pszDataType = EnvisatFile_GetKeyValueAsString( hEnvisatFile, SPH, 
                                                   "DATA_TYPE", "" );
    pszSampleType = EnvisatFile_GetKeyValueAsString( hEnvisatFile, SPH, 
                                                     "SAMPLE_TYPE", "" );
    if( EQUAL(pszDataType,"UWORD") )
        eDataType = GDT_UInt16;
    else if( EQUAL(pszDataType,"SWORD") && EQUALN(pszSampleType,"COMPLEX",7) )
        eDataType = GDT_CInt16;
    else if( EQUAL(pszDataType,"SWORD") )
        eDataType = GDT_Int16;
    else 
        eDataType = GDT_Byte;

#ifdef CPL_LSB 
    bNative = FALSE;
#else
    bNative = TRUE;
#endif

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */

    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Collect raw definitions of each channel and create              */
/*      corresponding bands.                                            */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, 
                   new RawRasterBand( poDS, 1, poDS->fpImage,
                                      ds_offset + 17, 
                                      GDALGetDataTypeSize(eDataType) / 8, 
                                      dsr_size, 
                                      eDataType, bNative ) );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_Envisat()                       */
/************************************************************************/

void GDALRegister_Envisat()

{
    GDALDriver	*poDriver;

    if( poEnvisatDriver == NULL )
    {
        poEnvisatDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "ESAT";
        poDriver->pszLongName = "Envisat Image Format (.N1)";
        
        poDriver->pfnOpen = EnvisatDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

