/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * gdbdriver.cpp
 *
 * The GDB driver implemenation is the GDAL Driver for GeoGateway.
 * 
 * $Log$
 * Revision 1.2  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.1  1998/11/29 22:39:49  warmerda
 * New
 *
 */

#include "gdb.h"		// from PCI distribution.

#include "gdal_priv.h"

static GDALDriver	*poGDBDriver = NULL;

GDAL_C_START
void	GDALRegister_GDB(void);
GDAL_C_END


/************************************************************************/
/* ==================================================================== */
/*				GDBDataset				*/
/* ==================================================================== */
/************************************************************************/

class GDBRasterBand;

class GDBDataset : public GDALDataset
{
    friend	GDBRasterBand;
    
    FILE	*fp;
    
  public:
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GDBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GDBRasterBand : public GDALRasterBand
{
    friend	GDBDataset;
    
  public:

                   GDBRasterBand::GDBRasterBand( GDBDataset *, int );
    
    // should override RasterIO eventually.
    
    virtual GBSErr ReadBlock( int, int, void * );
    virtual GBSErr WriteBlock( int, int, void * ); 
};


/************************************************************************/
/*                           GDBRasterBand()                            */
/************************************************************************/

GDBRasterBand::GDBRasterBand( GDBDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    if( GDBChanType( poDS->fp, nBand ) == CHN_8U )
        eDataType = GDT_Byte;
    else if( GDBChanType( poDS->fp, nBand ) == CHN_16U )
        eDataType = GDT_UInt16;
    else if( GDBChanType( poDS->fp, nBand ) == CHN_16S )
        eDataType = GDT_Int16;
    else if( GDBChanType( poDS->fp, nBand ) == CHN_32R )
        eDataType = GDT_Float32;
    else
        eDataType = GDT_Unknown;

/* -------------------------------------------------------------------- */
/*      Set the access flag.  For now we set it the same as the         */
/*      whole dataset, but eventually this should take account of       */
/*      locked channels, or read-only secondary data files.             */
/* -------------------------------------------------------------------- */
    /* ... */
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

GBSErr GDBRasterBand::ReadBlock( int nBlockXOff, int nBlockYOff,
                                 void * pImage )

{
    GDBDataset	*poGDB_DS = (GDBDataset *) poDS;

    

    return GE_None;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

GBSErr GDBRasterBand::WriteBlock( int nBlockXOff, int nBlockYOff,
                                 void * pImage )

{
    GDBDataset	*poGDB_DS = (GDBDataset *) poDS;

    

    return GE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GDBDataset::Open( GDALOpenInfo * poOpenInfo )

{
    FILE	*fp;
    TBool	bOldFatal;
    static TBool IMP_Initialized = FALSE;

/* -------------------------------------------------------------------- */
/*      Ensure IMP is initialized.  We would like to pass real          */
/*      arguments in if we could.  It would also be nice if there       */
/*      was a preferred application name for GeoGateway using           */
/*      programs.                                                       */
/* -------------------------------------------------------------------- */
    if( !IMP_Initialized )
    {
        IMP_Initialized = TRUE;
        IMPInit( "fimport", 0, 0, NULL );

        ALLRegister();
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    bOldFatal = IMPFatal( FALSE );
    if( poOpenInfo->eAccess == GA_ReadOnly )
        fp = GDBOpen( poOpenInfo->pszFilename, "r" );
    else
        fp = GDBOpen( poOpenInfo->pszFilename, "r+" );
    IMPFatal( bOldFatal );

    if( fp == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GDBDataset 	*poDS;

    poDS = new GDBDataset();

    poDS->fp = fp;
    poDS->poDriver = poGDBDriver;
    poDS->nRasterXSize = GDBChanXSize( fp );
    poDS->nRasterYSize = GDBChanYSize( fp );
    poDS->nBands = GDBChanNum(fp);
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < GDBChanNum(fp); iBand++ )
    {
        poDS->SetBand( iBand+1, new GDBRasterBand( poDS, iBand+1 ) );
    }

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_GDB()                          */
/************************************************************************/

void GDALRegister_GDB()

{
    GDALDriver	*poDriver;

    if( poGDBDriver == NULL )
    {
        poGDBDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GDB";
        poDriver->pszLongName = "PCI GeoGateway Bridge";
        
        poDriver->pfnOpen = GDBDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

