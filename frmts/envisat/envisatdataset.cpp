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
 * Revision 1.19  2005/05/05 14:02:58  fwarmerdam
 * PAM Enable
 *
 * Revision 1.18  2003/03/24 14:16:38  warmerda
 * added fallback case for unrecognised products
 *
 * Revision 1.17  2002/12/04 21:15:19  warmerda
 * Hacked to support AATSR TOA Level 1 data.
 *
 * Revision 1.16  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.15  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.14  2002/06/07 14:21:55  warmerda
 * avoid paths in include directives
 *
 * Revision 1.13  2002/05/29 16:38:35  warmerda
 * dont use variable sized buffer on stack
 *
 * Revision 1.12  2002/05/21 21:29:58  warmerda
 * added support for MERIS tiepoints, and capture DS name as band desc
 *
 * Revision 1.11  2002/05/16 03:29:50  warmerda
 * ensure fpImage is closed on cleanup
 *
 * Revision 1.10  2002/04/26 14:53:22  warmerda
 * added EscapedRecord for metadata
 *
 * Revision 1.9  2002/04/16 17:53:33  warmerda
 * Initialize variables.
 *
 * Revision 1.8  2002/04/08 17:33:25  warmerda
 * now you can get dataset records via metadata api
 *
 * Revision 1.7  2001/09/26 19:23:27  warmerda
 * added CollectDSDMetadata
 *
 * Revision 1.6  2001/09/24 18:40:17  warmerda
 * Added MDS2 support, and metadata support
 *
 * Revision 1.5  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
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

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "EnvisatFile.h"
CPL_C_END

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

    char        **papszTempMD;
    
    void        ScanForGCPs_ASAR();
    void        ScanForGCPs_MERIS();

    void	CollectMetadata( EnvisatFile_HeaderFlag );
    void        CollectDSDMetadata();

  public:
    		EnvisatDataset();
    	        ~EnvisatDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual char **GetMetadata( const char * pszDomain );


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
    fpImage = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    papszTempMD = NULL;
}

/************************************************************************/
/*                            ~EnvisatDataset()                            */
/************************************************************************/

EnvisatDataset::~EnvisatDataset()

{
    FlushCache();

    if( hEnvisatFile != NULL )
        EnvisatFile_Close( hEnvisatFile );

    if( fpImage != NULL )
        VSIFClose( fpImage );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CSLDestroy( papszTempMD );
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
/*                          ScanForGCPs_ASAR()                          */
/************************************************************************/

void EnvisatDataset::ScanForGCPs_ASAR()

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
    int  	nRange=0, nSample, iGCP;
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
/*                         ScanForGCPs_MERIS()                          */
/************************************************************************/

void EnvisatDataset::ScanForGCPs_MERIS()

{
    int		nDatasetIndex, nNumDSR, nDSRSize, iRecord;

/* -------------------------------------------------------------------- */
/*      Do we have a meaningful geolocation grid?  Seach for a          */
/*      DS_TYPE=A and a name containing "geolocation" or "tie           */
/*      points".                                                        */
/* -------------------------------------------------------------------- */
    nDatasetIndex = EnvisatFile_GetDatasetIndex( hEnvisatFile, 
                                                 "Tie points ADS" );
    if( nDatasetIndex == -1 )
        return;

    if( EnvisatFile_GetDatasetInfo( hEnvisatFile, nDatasetIndex, 
                                    NULL, NULL, NULL, NULL, NULL, 
                                    &nNumDSR, &nDSRSize ) != SUCCESS )
        return;

    if( nNumDSR == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Figure out the tiepoint space, and how many we have.            */
/* -------------------------------------------------------------------- */
    int  nLinesPerTiePoint, nSamplesPerTiePoint;
    int  nTPPerLine, nTPPerColumn = nNumDSR;

    if( nNumDSR == 0 )
        return;

    nLinesPerTiePoint = 
        EnvisatFile_GetKeyValueAsInt( hEnvisatFile, SPH, 
                                      "LINES_PER_TIE_PT", 0 );
    nSamplesPerTiePoint =
        EnvisatFile_GetKeyValueAsInt( hEnvisatFile, SPH, 
                                      "SAMPLES_PER_TIE_PT", 0 );

    if( nLinesPerTiePoint == 0 || nSamplesPerTiePoint == 0 )
        return;

    nTPPerLine = (GetRasterXSize() + nSamplesPerTiePoint - 1) 
        / nSamplesPerTiePoint;

    if( (GetRasterXSize() + nSamplesPerTiePoint - 1) 
        / nSamplesPerTiePoint  != nTPPerColumn )
    {
        CPLDebug( "EnvisatDataset", "Got %d instead of %d nTPPerColumn.", 
                  (GetRasterXSize()+nSamplesPerTiePoint-1)/nSamplesPerTiePoint,
                  nTPPerColumn );
        return;
    }

    if( 50*nTPPerLine + 13 != nDSRSize )
    {
        CPLDebug( "EnvisatDataset", 
                  "DSRSize=%d instead of expected %d for tiepoints ADS.", 
                  nDSRSize, 50*nTPPerLine + 13 );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Collect the first GCP set from each record.			*/
/* -------------------------------------------------------------------- */
    GByte	*pabyRecord = (GByte *) CPLMalloc(nDSRSize);
    int  	iGCP;
    GUInt32 	unValue;

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) 
        CPLCalloc(sizeof(GDAL_GCP),nNumDSR * nTPPerLine);

    for( iRecord = 0; iRecord < nNumDSR; iRecord++ )
    {
        if( EnvisatFile_ReadDatasetRecord( hEnvisatFile, nDatasetIndex, 
                                           iRecord, pabyRecord ) != SUCCESS )
            continue;

        memcpy( &unValue, pabyRecord + 13, 4 );

        for( iGCP = 0; iGCP < nTPPerLine; iGCP++ )
        {
            char	szId[128];

            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            CPLFree( pasGCPList[nGCPCount].pszId );
            
            sprintf( szId, "%d", nGCPCount+1 );
            pasGCPList[nGCPCount].pszId = CPLStrdup( szId );

            memcpy( &unValue, pabyRecord + 13 + nTPPerLine*4 + iGCP*4, 4 );
            pasGCPList[nGCPCount].dfGCPX = 
                ((int)CPL_MSBWORD32(unValue))*0.000001;

            memcpy( &unValue, pabyRecord + 13 + iGCP*4, 4 );
            pasGCPList[nGCPCount].dfGCPY = 
                ((int)CPL_MSBWORD32(unValue))*0.000001;

            pasGCPList[nGCPCount].dfGCPZ = 0.0;

            pasGCPList[nGCPCount].dfGCPLine = iRecord*nLinesPerTiePoint + 0.5;
            pasGCPList[nGCPCount].dfGCPPixel = iGCP*nSamplesPerTiePoint + 0.5;
            
            nGCPCount++;
        }
    }
    CPLFree( pabyRecord );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **EnvisatDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain == NULL || !EQUALN(pszDomain,"envisat-ds-",11) )
        return GDALDataset::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      Get the dataset name and record number.                         */
/* -------------------------------------------------------------------- */
    char	szDSName[128];
    int         i, nRecord = -1;

    strncpy( szDSName, pszDomain+11, sizeof(szDSName) );
    for( i = 0; i < (int) sizeof(szDSName)-1; i++ )
    {
        if( szDSName[i] == '-' )
        {
            szDSName[i] = '\0';
            nRecord = atoi(szDSName+1);
            break;
        }
    }

    if( nRecord == -1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the dataset index and info.                                 */
/* -------------------------------------------------------------------- */
    int nDSIndex = EnvisatFile_GetDatasetIndex( hEnvisatFile, szDSName );
    int nDSRSize, nNumDSR;

    if( nDSIndex == -1 )
        return NULL;

    EnvisatFile_GetDatasetInfo( hEnvisatFile, nDSIndex, NULL, NULL, NULL,
                                NULL, NULL, &nNumDSR, &nDSRSize );

    if( nDSRSize == -1 || nRecord < 0 || nRecord >= nNumDSR )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read the requested record.                                      */
/* -------------------------------------------------------------------- */
    char  *pszRecord;

    pszRecord = (char *) CPLMalloc(nDSRSize+1);
    
    if( EnvisatFile_ReadDatasetRecord( hEnvisatFile, nDSIndex, nRecord, 
                                       pszRecord ) == FAILURE )
    {
        CPLFree( pszRecord );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Massage the data into a safe textual format.  For now we        */
/*      just turn zero bytes into spaces.                               */
/* -------------------------------------------------------------------- */
    char *pszEscapedRecord;

    CSLDestroy( papszTempMD );

    pszEscapedRecord = CPLEscapeString( pszRecord, nDSRSize, 
                                        CPLES_BackslashQuotable );
    papszTempMD = CSLSetNameValue( NULL, "EscapedRecord", pszEscapedRecord );
    CPLFree( pszEscapedRecord );

    for( i = 0; i < nDSRSize; i++ )
        if( pszRecord[i] == '\0' )
            pszRecord[i] = ' ';

    papszTempMD = CSLSetNameValue( papszTempMD, "RawRecord", pszRecord );

    CPLFree( pszRecord );

    return papszTempMD;
}

/************************************************************************/
/*                         CollectDSDMetadata()                         */
/*                                                                      */
/*      Collect metadata based on any DSD entries with filenames        */
/*      associated.                                                     */
/************************************************************************/

void EnvisatDataset::CollectDSDMetadata()

{
    char	*pszDSName, *pszFilename;
    int		iDSD;

    for( iDSD = 0; 
         EnvisatFile_GetDatasetInfo( hEnvisatFile, iDSD, &pszDSName, NULL, 
                             &pszFilename, NULL, NULL, NULL, NULL ) == SUCCESS;
         iDSD++ )
    {
        if( pszFilename == NULL 
            || strlen(pszFilename) == 0 
            || EQUALN(pszFilename,"NOT USED",8)
            || EQUALN(pszFilename,"        ",8))
            continue;

        char	szKey[128], szTrimmedName[128];
        int	i;
        
        strcpy( szKey, "DS_" );
        strcat( szKey, pszDSName );

        // strip trailing spaces. 
        for( i = strlen(szKey)-1; i && szKey[i] == ' '; i-- )
            szKey[i] = '\0';

        // convert spaces into underscores.
        for( i = 0; szKey[i] != '\0'; i++ )
        {
            if( szKey[i] == ' ' )
                szKey[i] = '_';
        }

        strcat( szKey, "_NAME" );

        strcpy( szTrimmedName, pszFilename );
        for( i = strlen(szTrimmedName)-1; i && szTrimmedName[i] == ' '; i--)
            szTrimmedName[i] = '\0';

        SetMetadataItem( szKey, szTrimmedName );
    }
}

/************************************************************************/
/*                          CollectMetadata()                           */
/*                                                                      */
/*      Collect metadata from the SPH or MPH header fields.             */
/************************************************************************/

void EnvisatDataset::CollectMetadata( EnvisatFile_HeaderFlag  eMPHOrSPH )

{
    int		iKey;
    
    for( iKey = 0; TRUE; iKey++ )
    {
        const char *pszValue, *pszKey;
        char  szHeaderKey[128];

        pszKey = EnvisatFile_GetKeyByIndex(hEnvisatFile, eMPHOrSPH, iKey);
        if( pszKey == NULL )
            break;

        pszValue = EnvisatFile_GetKeyValueAsString( hEnvisatFile, eMPHOrSPH,
                                                    pszKey, NULL );

        if( pszValue == NULL )
            continue;

        // skip some uninteresting structural information.
        if( EQUAL(pszKey,"TOT_SIZE")
            || EQUAL(pszKey,"SPH_SIZE")
            || EQUAL(pszKey,"NUM_DSD")
            || EQUAL(pszKey,"DSD_SIZE")
            || EQUAL(pszKey,"NUM_DATA_SETS") )
            continue;

        if( eMPHOrSPH == MPH )
            sprintf( szHeaderKey, "MPH_%s", pszKey );
        else
            sprintf( szHeaderKey, "SPH_%s", pszKey );

        SetMetadataItem( szHeaderKey, pszValue );
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

/* -------------------------------------------------------------------- */
/*      Find a Mesurement type dataset to use as our reference          */
/*      raster band.                                                    */
/* -------------------------------------------------------------------- */
    int		dsr_size, num_dsr, ds_offset, bNative;
    char        *pszDSType;

    for( ds_index = 0; TRUE; ds_index++ )
    {
        if( EnvisatFile_GetDatasetInfo( hEnvisatFile, ds_index, 
                                        NULL, &pszDSType, NULL, 
                                        &ds_offset, NULL, 
                                        &num_dsr, &dsr_size ) == FAILURE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to find \"MDS1\" measurement datatset in Envisat file." );
            EnvisatFile_Close( hEnvisatFile );
            return NULL;
        }

        /* Have we found what we are looking for?  A Measurement ds. */
        if( EQUAL(pszDSType,"M") )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    EnvisatDataset 	*poDS;

    poDS = new EnvisatDataset();

    poDS->hEnvisatFile = hEnvisatFile;

/* -------------------------------------------------------------------- */
/*      Setup image definition.                                         */
/* -------------------------------------------------------------------- */
    const char  *pszDataType, *pszSampleType, *pszProduct;
    GDALDataType eDataType;
    int          nPrefixBytes;

    EnvisatFile_GetDatasetInfo( hEnvisatFile, ds_index, 
                                NULL, NULL, NULL, &ds_offset, NULL, 
                                &num_dsr, &dsr_size );

    poDS->nRasterXSize = EnvisatFile_GetKeyValueAsInt( hEnvisatFile, SPH,
                                                       "LINE_LENGTH", 0 );
    poDS->nRasterYSize = num_dsr;
    poDS->eAccess = GA_ReadOnly;

    pszProduct = EnvisatFile_GetKeyValueAsString( hEnvisatFile, MPH,
                                                  "PRODUCT", "" );
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
    else if( EQUALN(pszProduct,"ATS_TOA_1",8) )
    {
        /* all 16bit data, no line length provided */
        eDataType = GDT_Int16;
        poDS->nRasterXSize = (dsr_size - 20) / 2;
    }
    else if( poDS->nRasterXSize == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Envisat product format not recognised.  Assuming 8bit\n"
                  "with no per-record prefix data.  Results may be useless!" );
        eDataType = GDT_Byte;
        poDS->nRasterXSize = dsr_size;
    }
    else
    {
        if( dsr_size >= 2 * poDS->nRasterXSize )
            eDataType = GDT_UInt16;
        else
            eDataType = GDT_Byte;
    }

#ifdef CPL_LSB 
    bNative = FALSE;
#else
    bNative = TRUE;
#endif

    nPrefixBytes = dsr_size - 
        ((GDALGetDataTypeSize(eDataType) / 8) * poDS->nRasterXSize);

/* -------------------------------------------------------------------- */
/*      Fail out if we didn't get non-zero sizes.                       */
/* -------------------------------------------------------------------- */
    if( poDS->nRasterXSize < 1 || poDS->nRasterYSize < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to determine organization of dataset.  It would\n"
                  "appear this is an Envisat dataset, but an unsupported\n"
                  "data product.  Unable to utilize." );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Try to collect GCPs.                                            */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Scan for all datasets matching the reference dataset.           */
/* -------------------------------------------------------------------- */
    int	num_dsr2, dsr_size2, iBand = 0;
    const char *pszDSName;

    for( ds_index = 0; 
         EnvisatFile_GetDatasetInfo( hEnvisatFile, ds_index, 
                                     (char **) &pszDSName, NULL, NULL, 
                                     &ds_offset, NULL, 
                                     &num_dsr2, &dsr_size2 ) == SUCCESS;
         ds_index++ )
    {
        if( EQUAL(pszDSType,"M") 
            && num_dsr2 == num_dsr && dsr_size2 == dsr_size )
        {
            poDS->SetBand( iBand+1,
                       new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                          ds_offset + nPrefixBytes,
                                          GDALGetDataTypeSize(eDataType) / 8, 
                                          dsr_size, 
                                          eDataType, bNative ) );
            iBand++;

            poDS->GetRasterBand(iBand)->SetDescription( pszDSName );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    poDS->CollectMetadata( MPH );
    poDS->CollectMetadata( SPH );
    poDS->CollectDSDMetadata();

    if( EQUALN(pszProduct,"MER",3) )
        poDS->ScanForGCPs_MERIS();
    else
        poDS->ScanForGCPs_ASAR();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_Envisat()                       */
/************************************************************************/

void GDALRegister_Envisat()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ESAT" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ESAT" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Envisat Image Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#Envisat" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "n1" );

        poDriver->pfnOpen = EnvisatDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

