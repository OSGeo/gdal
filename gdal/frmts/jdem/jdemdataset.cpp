/******************************************************************************
 * $Id$
 *
 * Project:  JDEM Reader
 * Purpose:  All code for Japanese DEM Reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

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
/*                            JDEMGetAngle()                            */
/************************************************************************/

static double JDEMGetAngle( char *pszField )

{
    int		nAngle = JDEMGetField( pszField, 7 );
    int		nDegree, nMin, nSec;

    // Note, this isn't very general purpose, but it would appear
    // from the field widths that angles are never negative.  Nice
    // to be a country in the "first quadrant". 

    nDegree = nAngle / 10000;
    nMin = (nAngle / 100) % 100;
    nSec = nAngle % 100;
    
    return nDegree + nMin / 60.0 + nSec / 3600.0;
}

/************************************************************************/
/* ==================================================================== */
/*				JDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand;

class JDEMDataset : public GDALPamDataset
{
    friend class JDEMRasterBand;

    VSILFILE	*fp;
    GByte	abyHeader[1012];

  public:
                     JDEMDataset();
                    ~JDEMDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            JDEMRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand : public GDALPamRasterBand
{
    friend class JDEMDataset;

    int          nRecordSize;
    char*        pszRecord;
    int          bBufferAllocFailed;
    
  public:

    		JDEMRasterBand( JDEMDataset *, int );
                ~JDEMRasterBand();
    
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

    /* Cannot overflow as nBlockXSize <= 999 */
    nRecordSize = nBlockXSize*5 + 9 + 2;
    pszRecord = NULL;
    bBufferAllocFailed = FALSE;
}

/************************************************************************/
/*                          ~JDEMRasterBand()                            */
/************************************************************************/

JDEMRasterBand::~JDEMRasterBand()
{
    VSIFree(pszRecord);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JDEMRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    JDEMDataset *poGDS = (JDEMDataset *) poDS;
    int		i;
    
    if (pszRecord == NULL)
    {
        if (bBufferAllocFailed)
            return CE_Failure;

        pszRecord = (char *) VSIMalloc(nRecordSize);
        if (pszRecord == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate scanline buffer");
            bBufferAllocFailed = TRUE;
            return CE_Failure;
        }
    }

    VSIFSeekL( poGDS->fp, 1011 + nRecordSize*nBlockYOff, SEEK_SET );

    VSIFReadL( pszRecord, 1, nRecordSize, poGDS->fp );

    if( !EQUALN((char *) poGDS->abyHeader,pszRecord,6) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "JDEM Scanline corrupt.  Perhaps file was not transferred\n"
                  "in binary mode?" );
        return CE_Failure;
    }
    
    if( JDEMGetField( pszRecord + 6, 3 ) != nBlockYOff + 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "JDEM scanline out of order, JDEM driver does not\n"
                  "currently support partial datasets." );
        return CE_Failure;
    }

    for( i = 0; i < nBlockXSize; i++ )
        ((float *) pImage)[i] = (float)
            (JDEMGetField( pszRecord + 9 + 5 * i, 5) * 0.1);

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				JDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            JDEMDataset()                             */
/************************************************************************/

JDEMDataset::JDEMDataset() : fp(NULL)

{
    fp = NULL;
}

/************************************************************************/
/*                           ~JDEMDataset()                             */
/************************************************************************/

JDEMDataset::~JDEMDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JDEMDataset::GetGeoTransform( double * padfTransform )

{
    double	dfLLLat, dfLLLong, dfURLat, dfURLong;

    dfLLLat = JDEMGetAngle( (char *) abyHeader + 29 );
    dfLLLong = JDEMGetAngle( (char *) abyHeader + 36 );
    dfURLat = JDEMGetAngle( (char *) abyHeader + 43 );
    dfURLong = JDEMGetAngle( (char *) abyHeader + 50 );
    
    padfTransform[0] = dfLLLong;
    padfTransform[3] = dfURLat;
    padfTransform[1] = (dfURLong - dfLLLong) / GetRasterXSize();
    padfTransform[2] = 0.0;
        
    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * (dfURLat - dfLLLat) / GetRasterYSize();


    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JDEMDataset::GetProjectionRef()

{
    return( "GEOGCS[\"Tokyo\",DATUM[\"Tokyo\",SPHEROID[\"Bessel 1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",7004]],TOWGS84[-148,507,685,0,0,0,0],AUTHORITY[\"EPSG\",6301]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AUTHORITY[\"EPSG\",4301]]" );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int JDEMDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Confirm that the header has what appears to be dates in the     */
/*      expected locations.  Sadly this is a relatively weak test.      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 50 )
        return FALSE;

    /* check if century values seem reasonable */
    if( (!EQUALN((char *)poOpenInfo->pabyHeader+11,"19",2)
          && !EQUALN((char *)poOpenInfo->pabyHeader+11,"20",2))
        || (!EQUALN((char *)poOpenInfo->pabyHeader+15,"19",2)
             && !EQUALN((char *)poOpenInfo->pabyHeader+15,"20",2))
        || (!EQUALN((char *)poOpenInfo->pabyHeader+19,"19",2)
             && !EQUALN((char *)poOpenInfo->pabyHeader+19,"20",2)) )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JDEMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Confirm that the header is compatible with a JDEM dataset.      */
/* -------------------------------------------------------------------- */
    if (!Identify(poOpenInfo))
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The JDEM driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    /* Check that the file pointer from GDALOpenInfo* is available */
    if( poOpenInfo->fpL == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JDEMDataset 	*poDS;

    poDS = new JDEMDataset();

    /* Borrow the file pointer from GDALOpenInfo* */
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( poDS->abyHeader, 1, 1012, poDS->fp );

    poDS->nRasterXSize = JDEMGetField( (char *) poDS->abyHeader + 23, 3 );
    poDS->nRasterYSize = JDEMGetField( (char *) poDS->abyHeader + 26, 3 );
    if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid dimensions : %d x %d", 
                  poDS->nRasterXSize, poDS->nRasterYSize); 
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new JDEMRasterBand( poDS, 1 ));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_JDEM()                          */
/************************************************************************/

void GDALRegister_JDEM()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JDEM" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JDEM" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Japanese DEM (.mem)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#JDEM" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mem" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = JDEMDataset::Open;
        poDriver->pfnIdentify = JDEMDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
