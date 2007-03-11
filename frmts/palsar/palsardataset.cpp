/******************************************************************************
 * $Id: palsardataset.cpp 2007-01-28 what_nick $
 *
 * Project:  PALSAR Reader
 * Purpose:  Code for Complex PALSAR-CEOs Data access
 * Author:   Tisham Dhar, tisham@apogee.com.au
 *
 ******************************************************************************
 * Copyright (c) 2007,
 * Tisham Dhar(Apogee Imaging International) <tisham@apogee.com.au>
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

#include "gdal_pam.h"

CPL_C_START
void	GDALRegister_PALSAR(void);
CPL_C_END


class PALSARDataset : public GDALDataset
{

friend class PALSARComplexRasterBand;

    FILE        *fp;
    GByte       abyHeader[720];

  public:
                ~PALSARDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};
/************************************************************************/
/* ==================================================================== */
/*                            PALSARRasterBand                          */
/* ==================================================================== */
/************************************************************************/
class PALSARComplexRasterBand : public GDALRasterBand
{
  public:
                PALSARComplexRasterBand( PALSARDataset *, int,long,long,long );
    virtual CPLErr IReadBlock( int, int, void * );
  private:
	long offset,prefix,bps;
};

/************************************************************************/
/*                           PALSARRasterBand()                         */
/************************************************************************/

PALSARComplexRasterBand::PALSARComplexRasterBand( PALSARDataset *poDS, int nBand ,
long offset,long prefix,long bps)

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_CFloat32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
	
    this->offset = offset;
    this->prefix = prefix;
    this->bps = bps;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PALSARComplexRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    PALSARDataset *poGDS = (PALSARDataset *) poDS;
    char	*pszRecord;
    long	nRecordSize = nBlockXSize*bps + prefix;

    VSIFSeek( poGDS->fp, offset + nRecordSize*nBlockYOff, SEEK_SET );

    pszRecord = (char *) CPLMalloc(nRecordSize);
    VSIFRead( pszRecord, 1, nRecordSize, poGDS->fp );
	
    //Build complex image
    GDALCopyWords( pszRecord+prefix, eDataType,GDALGetDataTypeSize(eDataType)/8,
                   pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                   nBlockXSize );
	
    return CE_None;
}


/************************************************************************/
/* ==================================================================== */
/*				PALSARDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~PALSARDataset()                          */
/************************************************************************/

PALSARDataset::~PALSARDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFClose( fp );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *PALSARDataset::Open( GDALOpenInfo * poOpenInfo )

{
// -------------------------------------------------------------------- 
//      Before trying PALSAROpen() we first verify that there is Correct
//		header info in the file    
// -------------------------------------------------------------------- 
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 720 )
        return NULL;
//----------------------------------------------------------------------
//		Check if the file names are correct and the number of bands
//
//----------------------------------------------------------------------
    //Get file base name
    /*
      printf("Dirname %s Filename %s Extension %s\n",
      CPLGetPath(poOpenInfo->pszFilename),
      CPLGetFilename(poOpenInfo->pszFilename),
      CPLGetExtension(poOpenInfo->pszFilename));
    */
    if(*(poOpenInfo->pabyHeader+55)==66 && 
       strncmp(CPLGetExtension(poOpenInfo->pszFilename),"1__A",4) == 0)
    {
        printf("Palsar Level 1.1 detected\n");
        long lines  = CPLScanLong((char*)poOpenInfo->pabyHeader+180,6);// nr of data lines
        long bps    = CPLScanLong((char*)poOpenInfo->pabyHeader+224,4); //bytes per sample
        long prefix = CPLScanLong((char*)poOpenInfo->pabyHeader+276,4); //prefix at beginning of line
        long pixels = (CPLScanLong((char*)poOpenInfo->pabyHeader+186,6)-prefix) /  bps  ; //nr of pixels per line  
		
		
        long offset = poOpenInfo->pabyHeader[11]+
            256l*poOpenInfo->pabyHeader[10]+
            65536l*poOpenInfo->pabyHeader[9]+
            16777216l*poOpenInfo->pabyHeader[8]+1 ;
		
        printf("File parameters lines %ld prefix %ld bps %ld pixels %ld offset %ld\n",
               lines,prefix,bps,pixels,offset);
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.  May need VSIOpenL()        */
/* -------------------------------------------------------------------- */
        PALSARDataset  *poDS;

        poDS = new PALSARDataset();

        poDS->fp = poOpenInfo->fp;
        poOpenInfo->fp = NULL;
		
        VSIFSeek( poDS->fp, 0, SEEK_SET );
        VSIFRead( poDS->abyHeader, 1, 720, poDS->fp );
/*------------------------------------------------------------------------*/
/*		Set Dataset Info					  */
/*------------------------------------------------------------------------*/
        poDS->nRasterXSize = pixels;
        poDS->nRasterYSize = lines;
		
/*------------------------------------------------------------------------*/
/*		Attach all detected bands				  */
/*------------------------------------------------------------------------*/
        poDS->SetBand( 1, new PALSARComplexRasterBand( poDS, 1 ,offset,prefix,bps));

        return( poDS );
    }
	
    return NULL;
}



/************************************************************************/
/*                          GDALRegister_PALSAR()                       */
/************************************************************************/

void GDALRegister_PALSAR()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PALSAR" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PALSAR" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "SLC ALOS-PALSAR Reader" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#PALSAR" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "1__A" );

        poDriver->pfnOpen = PALSARDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
