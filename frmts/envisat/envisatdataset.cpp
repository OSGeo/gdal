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
 * Revision 1.4  2001/06/15 17:15:29  warmerda
 * use CPL_MSBWORD32 instead of CPL_SWAP32 to be cross platform
 *
 * Revision 1.3  2001/02/28 21:58:45  warmerda
 * added GCP collection
 *
 * Revision 1.2  2001/02/15 22:32:03  warmerda
 * Added FLT32 support.
 *
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

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ScanForGCPs();

  public:
    		EnvisatDataset();
    	        ~EnvisatDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

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
    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                            ~EnvisatDataset()                            */
/************************************************************************/

EnvisatDataset::~EnvisatDataset()

{
    if( hEnvisatFile != NULL )
        EnvisatFile_Close( hEnvisatFile );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int EnvisatDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *EnvisatDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *EnvisatDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void EnvisatDataset::ScanForGCPs()

{
    int		nDatasetIndex, nNumDSR, nDSRSize, iRecord;

/* -------------------------------------------------------------------- */
/*      Do we have a meaningful geolocation grid?                       */
/* -------------------------------------------------------------------- */
    nDatasetIndex = EnvisatFile_GetDatasetIndex( hEnvisatFile, 
                                                 "GEOLOCATION GRID ADS" );
    if( nDatasetIndex == -1 )
        return;

    if( EnvisatFile_GetDatasetInfo( hEnvisatFile, nDatasetIndex, 
                                    NULL, NULL, NULL, NULL, NULL, 
                                    &nNumDSR, &nDSRSize ) != SUCCESS )
        return;

    if( nNumDSR == 0 || nDSRSize != 521 )
        return;

/* -------------------------------------------------------------------- */
/*      Collect the first GCP set from each record.			*/
/* -------------------------------------------------------------------- */
    GByte	abyRecord[521];
    int  	nRange, nSample, iGCP;
    GUInt32 	unValue;

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),(nNumDSR+1) * 11);

    for( iRecord = 0; iRecord < nNumDSR; iRecord++ )
    {
        if( EnvisatFile_ReadDatasetRecord( hEnvisatFile, nDatasetIndex, 
                                           iRecord, abyRecord ) != SUCCESS )
            continue;

        memcpy( &unValue, abyRecord + 13, 4 );
        nRange = CPL_MSBWORD32( unValue );

        for( iGCP = 0; iGCP < 11; iGCP++ )
        {
            char	szId[128];

            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            CPLFree( pasGCPList[nGCPCount].pszId );
            
            sprintf( szId, "%d", nGCPCount+1 );
            pasGCPList[nGCPCount].pszId = CPLStrdup( szId );

            memcpy( &unValue, abyRecord + 25 + iGCP*4, 4 );
            nSample = CPL_MSBWORD32(unValue);

            memcpy( &unValue, abyRecord + 25 + 176 + iGCP*4, 4 );
            pasGCPList[nGCPCount].dfGCPX = ((int)CPL_MSBWORD32(unValue))*0.000001;

            memcpy( &unValue, abyRecord + 25 + 132 + iGCP*4, 4 );
            pasGCPList[nGCPCount].dfGCPY = ((int)CPL_MSBWORD32(unValue))*0.000001;

            pasGCPList[nGCPCount].dfGCPZ = 0.0;

            pasGCPList[nGCPCount].dfGCPLine = nRange - 0.5;
            pasGCPList[nGCPCount].dfGCPPixel = nSample - 0.5;
            
            nGCPCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      We also collect the bottom GCPs from the last granule.          */
/* -------------------------------------------------------------------- */
    memcpy( &unValue, abyRecord + 17, 4 );
    nRange = nRange + CPL_MSBWORD32( unValue ) - 1;

    for( iGCP = 0; iGCP < 11; iGCP++ )
    {
        char	szId[128];

        GDALInitGCPs( 1, pasGCPList + nGCPCount );

        CPLFree( pasGCPList[nGCPCount].pszId );
            
        sprintf( szId, "%d", nGCPCount+1 );
        pasGCPList[nGCPCount].pszId = CPLStrdup( szId );

        memcpy( &unValue, abyRecord + 279 + iGCP*4, 4 );
        nSample = CPL_MSBWORD32(unValue);

        memcpy( &unValue, abyRecord + 279 + 176 + iGCP*4, 4 );
        pasGCPList[nGCPCount].dfGCPX = ((int)CPL_MSBWORD32(unValue))*0.000001;

        memcpy( &unValue, abyRecord + 279 + 132 + iGCP*4, 4 );
        pasGCPList[nGCPCount].dfGCPY = ((int)CPL_MSBWORD32(unValue))*0.000001;

        pasGCPList[nGCPCount].dfGCPZ = 0.0;

        pasGCPList[nGCPCount].dfGCPLine = nRange - 0.5;
        pasGCPList[nGCPCount].dfGCPPixel = nSample - 0.5;
            
        nGCPCount++;
    }
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
    if( EQUAL(pszDataType,"FLT32") && EQUALN(pszSampleType,"COMPLEX",7))
        eDataType = GDT_CFloat32;
    else if( EQUAL(pszDataType,"FLT32") )
        eDataType = GDT_Float32;
    else if( EQUAL(pszDataType,"UWORD") )
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
/*      Try to collect GCPs.                                            */
/* -------------------------------------------------------------------- */
    poDS->ScanForGCPs();

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

