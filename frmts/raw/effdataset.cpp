/******************************************************************************
 * $Id$
 *
 * Project:  Eosat Fast Format Driver
 * Purpose:  Implementation of Eosat Fast Format Driver
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.3  2000/08/30 18:53:24  warmerda
 * Removed unused variables.
 *
 * Revision 1.2  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.1  2000/06/20 17:35:28  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

static GDALDriver	*poEFFDriver = NULL;

CPL_C_START
void	GDALRegister_EFF(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				EFFDataset				*/
/* ==================================================================== */
/************************************************************************/

class EFFDataset : public RawDataset
{
    FILE	*afpBandImage[7];
    char         szHeader[1537];
    
  public:
    		EFFDataset();
    	        ~EFFDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            EFFDataset()                             */
/************************************************************************/

EFFDataset::EFFDataset()
{
}

/************************************************************************/
/*                            ~EFFDataset()                            */
/************************************************************************/

EFFDataset::~EFFDataset()

{
    for( int iBand = 0; iBand < nBands; iBand++ )
        VSIFClose( afpBandImage[iBand] );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *EFFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 100 && poOpenInfo->fp != NULL )
        return NULL;

    if( !EQUAL(CPLGetBasename(poOpenInfo->pszFilename),"HEADER") )
        return NULL;

    if( !EQUALN((char *) poOpenInfo->pabyHeader, "PRODUCT",7) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create dataset.                                                 */
/* -------------------------------------------------------------------- */
    EFFDataset 	*poDS;

    poDS = new EFFDataset();

    poDS->poDriver = poEFFDriver;
    
/* -------------------------------------------------------------------- */
/*      Read the entire header.dat file.                                */
/* -------------------------------------------------------------------- */
    VSIFSeek( poOpenInfo->fp, 0, SEEK_SET );
    if( VSIFRead( poDS->szHeader, 1, 1536, poOpenInfo->fp ) != 1536 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to read whole 1536 bytes of Eosat Fast Format\n"
                  "header file: %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }
    poDS->szHeader[1536] = '\0';

/* -------------------------------------------------------------------- */
/*      Extract field values of interest.                               */
/* -------------------------------------------------------------------- */
    const char * pszValue;
    int          nPixels=0, nLines=0, nRecordLength=0;

    pszValue = strstr(poDS->szHeader,"PIXELS PER LINE");
    if( pszValue != NULL )
        nPixels = atoi(pszValue + 16);

    pszValue = strstr(poDS->szHeader,"LINES PER IMAGE");
    if( pszValue != NULL )
        nLines = atoi(pszValue + 16);

    pszValue = strstr(poDS->szHeader,"RECORD LENGTH =");
    if( pszValue != NULL )
        nRecordLength = atoi(pszValue + 15);
    else
        nRecordLength = nPixels;

    if( nPixels == 0 || nLines == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing PIXELS PER LINE or LINES PER IMAGE in Eosat\n"
                  "Fast Format header %s.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    poDS->nRasterXSize = nPixels;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Test for, and add each of the raw bands.                        */
/* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    const char *pszAccess;

    if( poOpenInfo->eAccess == GA_Update )
        pszAccess = "r+";
    else 
        pszAccess = "r";

    for( int iBand = 1; iBand < 8; iBand++ )
    {
        char  szBasename[20];
        const char  *pszFullFilename = NULL;
        FILE  *fp;

        if( strcmp(CPLGetBasename(poOpenInfo->pszFilename),"HEADER") == 0 )
            sprintf( szBasename, "BAND%d.DAT", iBand );
        else
            sprintf( szBasename, "band%d.dat", iBand );

        pszFullFilename = CPLFormFilename( pszPath, szBasename, NULL );

        fp = VSIFOpen( pszFullFilename, pszAccess );
        if( fp == NULL )
            continue;

        poDS->afpBandImage[poDS->nBands] = fp;

        poDS->SetBand( poDS->nBands+1, 
            new RawRasterBand( poDS, poDS->nBands+1, fp,
                               0, 1, nRecordLength, GDT_Byte, FALSE ));
    }

    CPLFree( pszPath );

    if( poDS->nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find or open any band files associated with\n"
                  "Eosat Fast Format header %s.\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_EFF()                          */
/************************************************************************/

void GDALRegister_EFF()

{
    GDALDriver	*poDriver;

    if( poEFFDriver == NULL )
    {
        poEFFDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "EFF";
        poDriver->pszLongName = "Eosat Fast Format";
        poDriver->pszHelpTopic = "frmt_various.html#EFF";
        
        poDriver->pfnOpen = EFFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

