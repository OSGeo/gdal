/******************************************************************************
 * $Id$
 *
 * Project:  JDEM Reader
 * Purpose:  All code for Japanese DEM Reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2000/11/27 19:03:26  warmerda
 * New
 *
 */

#include "gdal_priv.h"

static GDALDriver	*poJDEMDriver = NULL;

CPL_C_START
void	GDALRegister_JDEM(void);
CPL_C_END

/************************************************************************/
/*                            JDEMGetField()                            */
/************************************************************************/

static int JDEMGetField( char *pszField, int nWidth )

{
    char	szWork[32];

    CPLAssert( nWidth < (int) sizeof(szWork) );

    strncpy( szWork, pszField, nWidth );
    szWork[nWidth] = '\0';

    return atoi(szWork);
}

/************************************************************************/
/* ==================================================================== */
/*				JDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand;

class JDEMDataset : public GDALDataset
{
    friend	JDEMRasterBand;

    FILE	*fp;
    GByte	abyHeader[1012];

  public:
		~JDEMDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            JDEMRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand : public GDALRasterBand
{
    friend	JDEMDataset;
    
  public:

    		JDEMRasterBand( JDEMDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           JDEMRasterBand()                            */
/************************************************************************/

JDEMRasterBand::JDEMRasterBand( JDEMDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JDEMRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    JDEMDataset *poGDS = (JDEMDataset *) poDS;
    char	*pszRecord;
    int		nRecordSize = nBlockXSize*5 + 9 + 2;
    int		i;

    VSIFSeek( poGDS->fp, 1011 + nRecordSize*nBlockYOff, SEEK_SET );

    pszRecord = (char *) CPLMalloc(nRecordSize);
    VSIFRead( pszRecord, 1, nRecordSize, poGDS->fp );

    for( i = 0; i < nBlockXSize; i++ )
        ((float *) pImage)[i] = JDEMGetField( pszRecord + 9 + 5 * i, 5) * 0.1;

    CPLFree( pszRecord );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				JDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~JDEMDataset()                             */
/************************************************************************/

JDEMDataset::~JDEMDataset()

{
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JDEMDataset::GetGeoTransform( double * padfTransform )

{
    return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JDEMDataset::GetProjectionRef()

{
    return( "" );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JDEMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Before trying JDEMOpen() we first verify that there is at        */
/*      least one "\n#keyword" type signature in the first chunk of     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    /* check if century values seem reasonable */
    if( (!EQUALN((char *)poOpenInfo->pabyHeader+11,"19",2)
          && !EQUALN((char *)poOpenInfo->pabyHeader+11,"20",2))
        || (!EQUALN((char *)poOpenInfo->pabyHeader+15,"19",2)
             && !EQUALN((char *)poOpenInfo->pabyHeader+15,"20",2))
        || (!EQUALN((char *)poOpenInfo->pabyHeader+19,"19",2)
             && !EQUALN((char *)poOpenInfo->pabyHeader+19,"20",2)) )
    {
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JDEMDataset 	*poDS;

    poDS = new JDEMDataset();

    poDS->poDriver = poJDEMDriver;
    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 0, SEEK_SET );
    VSIFRead( poDS->abyHeader, 1, 1012, poDS->fp );

    poDS->nRasterXSize = JDEMGetField( (char *) poDS->abyHeader + 23, 3 );
    poDS->nRasterYSize = JDEMGetField( (char *) poDS->abyHeader + 26, 3 );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new JDEMRasterBand( poDS, 1 ));

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_JDEM()                          */
/************************************************************************/

void GDALRegister_JDEM()

{
    GDALDriver	*poDriver;

    if( poJDEMDriver == NULL )
    {
        poJDEMDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "JDEM";
        poDriver->pszLongName = "Japanese DEM (.mem)";
        
        poDriver->pfnOpen = JDEMDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
