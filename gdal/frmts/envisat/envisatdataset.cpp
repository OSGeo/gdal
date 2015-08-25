/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Reader for ENVISAT format image data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Atlantis Scientific, Inc.
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "timedelta.hpp"
#include "adsrange.hpp"
#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "EnvisatFile.h"
#include "records.h"
CPL_C_END

CPL_C_START
void	GDALRegister_Envisat(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                        MerisL2FlagBand                         */
/* ==================================================================== */
/************************************************************************/
class MerisL2FlagBand : public GDALPamRasterBand
{
  public:
    MerisL2FlagBand( GDALDataset *, int, VSILFILE*, off_t, off_t );
    virtual ~MerisL2FlagBand();
    virtual CPLErr IReadBlock( int, int, void * );

  private:
    off_t nImgOffset;
    off_t nPrefixBytes;
    size_t nBytePerPixel;
    size_t nRecordSize;
    size_t nDataSize;
    GByte *pReadBuf;
    VSILFILE *fpImage;
};

/************************************************************************/
/*                        MerisL2FlagBand()                       */
/************************************************************************/
MerisL2FlagBand::MerisL2FlagBand( GDALDataset *poDS, int nBand,
                                  VSILFILE* fpImage, off_t nImgOffset,
                                  off_t nPrefixBytes )
{
    this->poDS = (GDALDataset *) poDS;
    this->nBand = nBand;

    this->fpImage = fpImage;
    this->nImgOffset = nImgOffset;
    this->nPrefixBytes = nPrefixBytes;

    eDataType = GDT_UInt32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    nBytePerPixel = 3;

    nDataSize = nBlockXSize * nBytePerPixel;
    nRecordSize = nPrefixBytes + nDataSize;

    pReadBuf = (GByte *) CPLMalloc( nRecordSize );
}


/************************************************************************/
/*                        ~MerisL2FlagBand()                       */
/************************************************************************/
MerisL2FlagBand::~MerisL2FlagBand()
{
    CPLFree( pReadBuf );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr MerisL2FlagBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                    int nBlockYOff,
                                    void * pImage )
{
    CPLAssert( nBlockXOff == 0 );
    CPLAssert( pReadBuf != NULL );

    off_t nOffset = nImgOffset + nPrefixBytes +
                    nBlockYOff * nBlockYSize * nRecordSize;

    if ( VSIFSeekL( fpImage, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Seek to %d for scanline %d failed.\n",
                  (int)nOffset, nBlockYOff );
        return CE_Failure;
    }

    if ( VSIFReadL( pReadBuf, 1, nDataSize, fpImage ) != nDataSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Read of %d bytes for scanline %d failed.\n",
                  (int)nDataSize, nBlockYOff );
        return CE_Failure;
    }

    unsigned iImg, iBuf;
    for( iImg = 0, iBuf = 0;
         iImg < nBlockXSize * sizeof(GDT_UInt32);
         iImg += sizeof(GDT_UInt32), iBuf += nBytePerPixel )
    {
#ifdef CPL_LSB
        ((GByte*) pImage)[iImg] = pReadBuf[iBuf + 2];
        ((GByte*) pImage)[iImg + 1] = pReadBuf[iBuf + 1];
        ((GByte*) pImage)[iImg + 2] = pReadBuf[iBuf];
        ((GByte*) pImage)[iImg + 3] = 0;
#else
        ((GByte*) pImage)[iImg] = 0;
        ((GByte*) pImage)[iImg + 1] = pReadBuf[iBuf];
        ((GByte*) pImage)[iImg + 2] = pReadBuf[iBuf + 1];
        ((GByte*) pImage)[iImg + 3] = pReadBuf[iBuf + 2];
#endif
    }

    return CE_None;
}


/************************************************************************/
/* ==================================================================== */
/*				EnvisatDataset				*/
/* ==================================================================== */
/************************************************************************/

class EnvisatDataset : public RawDataset
{
    EnvisatFile *hEnvisatFile;
    VSILFILE	*fpImage;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    char        **papszTempMD;

    void        ScanForGCPs_ASAR();
    void        ScanForGCPs_MERIS();

    void        UnwrapGCPs(); 

    void	CollectMetadata( EnvisatFile_HeaderFlag );
    void        CollectDSDMetadata();
    void        CollectADSMetadata();

  public:
    		EnvisatDataset();
    	        ~EnvisatDataset();

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual char      **GetMetadataDomainList();
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
        VSIFCloseL( fpImage );

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
        return SRS_WKT_WGS84;
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
/*                         UnwrapGCPs()                                 */
/************************************************************************/

/* external C++ implementation of the in-place unwrapper */
void EnvisatUnwrapGCPs( int nGCPCount, GDAL_GCP *pasGCPList ) ;

void  EnvisatDataset::UnwrapGCPs()
{ 
    EnvisatUnwrapGCPs( nGCPCount, pasGCPList ) ;
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
    int  	nRange=0, nSample, iGCP, nRangeOffset=0;
    GUInt32 	unValue;

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),(nNumDSR+1) * 11);

    for( iRecord = 0; iRecord < nNumDSR; iRecord++ )
    {
        if( EnvisatFile_ReadDatasetRecord( hEnvisatFile, nDatasetIndex,
                                           iRecord, abyRecord ) != SUCCESS )
            continue;

        memcpy( &unValue, abyRecord + 13, 4 );
        nRange = CPL_MSBWORD32( unValue ) + nRangeOffset;

        if((iRecord>1) && (int(pasGCPList[nGCPCount-1].dfGCPLine+0.5) > nRange))
        {
            int delta = (int) (pasGCPList[nGCPCount-1].dfGCPLine -
                               pasGCPList[nGCPCount-12].dfGCPLine);
            nRange = int(pasGCPList[nGCPCount-1].dfGCPLine+0.5) + delta;
            nRangeOffset = nRange-1;
        }

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
    int  nDatasetIndex, nNumDSR, nDSRSize;
    bool isBrowseProduct;

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

/* -------------------------------------------------------------------- */
/*      Find a Mesurement type dataset to use as a reference raster     */
/*      band.                                                           */
/* -------------------------------------------------------------------- */

    int		nMDSIndex;

    for( nMDSIndex = 0; TRUE; nMDSIndex++ )
    {
        char *pszDSType;
        if( EnvisatFile_GetDatasetInfo( hEnvisatFile, nMDSIndex,
            NULL, &pszDSType, NULL, NULL, NULL, NULL, NULL ) == FAILURE )
        {
            CPLDebug("EnvisatDataset",
                            "Unable to find MDS in Envisat file.") ;
            return ;
    }
        if( EQUAL(pszDSType,"M") ) break;
    }

/* -------------------------------------------------------------------- */
/*      Get subset of TP ADS records matching the MDS records	*/
/* -------------------------------------------------------------------- */

    /* get the MDS line sampling time interval */
    TimeDelta tdMDSSamplingInterval( 0 , 0 ,  
        EnvisatFile_GetKeyValueAsInt( hEnvisatFile, SPH,
                                      "LINE_TIME_INTERVAL", 0  ) );

    /* get range of TiePoint ADS records matching the measurements */
    ADSRangeLastAfter arTP( *hEnvisatFile , nDatasetIndex, 
        nMDSIndex , tdMDSSamplingInterval ) ; 
    
    /* check if there are any TPs to be used */ 
    if ( arTP.getDSRCount() <= 0 )
    {
        CPLDebug( "EnvisatDataset" , "No tiepoint covering "
            "the measurement records." ) ;
        return; /* No TPs - no extraction. */
    } 

    /* check if TPs cover the whole range of MDSRs */
    if(( arTP.getFirstOffset() < 0 )||( arTP.getLastOffset() < 0 ))
    {
        CPLDebug( "EnvisatDataset" , "The tiepoints do not cover "
            "whole range of measurement records." ) ;
        /* Not good but we can still extract some of the TPS, can we? */
    }

    /* check TP records spacing */
    if ((1+(arTP.getFirstOffset()+arTP.getLastOffset()+GetRasterYSize()-1) 
           / nLinesPerTiePoint ) != arTP.getDSRCount() )
    {
        CPLDebug( "EnvisatDataset", "Not enough tieponts per column! "
            "received=%d expected=%d", nTPPerColumn , 
                1 + (arTP.getFirstOffset()+arTP.getLastOffset()+
                      GetRasterYSize()-1) / nLinesPerTiePoint ) ; 
        return; /* That is far more serious - we risk missplaces TPs. */
    }

    if ( 50*nTPPerLine + 13 == nDSRSize ) /* regular product */
    {
        isBrowseProduct = false ; 
    } 
    else if ( 8*nTPPerLine + 13 == nDSRSize ) /* browse product */
    { 
        /* although BPs are rare there is no reason not to support them */
        isBrowseProduct = true ; 
    } 
    else 
    {
        CPLDebug( "EnvisatDataset", "Unexpectd size of 'Tie points ADS' !"
                " received=%d expected=%d or %d" , nDSRSize ,
                50*nTPPerLine+13, 8*nTPPerLine+13 ) ;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Collect the first GCP set from each record.			*/
/* -------------------------------------------------------------------- */

    GByte	*pabyRecord = (GByte *) CPLMalloc(nDSRSize-13);
    int  	iGCP;

    GUInt32 *tpLat = ((GUInt32*)pabyRecord) + nTPPerLine*0 ; /* latitude */  
    GUInt32 *tpLon = ((GUInt32*)pabyRecord) + nTPPerLine*1 ; /* longitude */  
    GUInt32 *tpLtc = ((GUInt32*)pabyRecord) + nTPPerLine*4 ; /* lat. DEM correction */
    GUInt32 *tpLnc = ((GUInt32*)pabyRecord) + nTPPerLine*5 ; /* lon. DEM correction */ 

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc( sizeof(GDAL_GCP), 
                                        arTP.getDSRCount() * nTPPerLine );

    for( int ir = 0 ; ir < arTP.getDSRCount() ; ir++ )
    {
        int iRecord = ir + arTP.getFirstIndex() ; 

        double dfGCPLine = 0.5 + 
            ( iRecord*nLinesPerTiePoint - arTP.getFirstOffset() ) ; 

        if( EnvisatFile_ReadDatasetRecordChunk( hEnvisatFile, nDatasetIndex,
                    iRecord , pabyRecord, 13 , -1 ) != SUCCESS )
            continue;

        for( iGCP = 0; iGCP < nTPPerLine; iGCP++ )
        {
            char	szId[128];

            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            CPLFree( pasGCPList[nGCPCount].pszId );

            sprintf( szId, "%d", nGCPCount+1 );
            pasGCPList[nGCPCount].pszId = CPLStrdup( szId );

            #define INT32(x)    ((GInt32)CPL_MSBWORD32(x)) 

            pasGCPList[nGCPCount].dfGCPX = 1e-6*INT32(tpLon[iGCP]) ; 
            pasGCPList[nGCPCount].dfGCPY = 1e-6*INT32(tpLat[iGCP]) ; 
            pasGCPList[nGCPCount].dfGCPZ = 0.0;

            if( !isBrowseProduct ) /* add DEM corrections */
            { 
                pasGCPList[nGCPCount].dfGCPX += 1e-6*INT32(tpLnc[iGCP]) ; 
                pasGCPList[nGCPCount].dfGCPY += 1e-6*INT32(tpLtc[iGCP]) ; 
            } 

            #undef INT32

            pasGCPList[nGCPCount].dfGCPLine = dfGCPLine ; 
            pasGCPList[nGCPCount].dfGCPPixel = iGCP*nSamplesPerTiePoint + 0.5;

            nGCPCount++;
        }
    }
    CPLFree( pabyRecord );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **EnvisatDataset::GetMetadataDomainList()
{
    return CSLAddString(GDALDataset::GetMetadataDomainList(), "envisat-ds-*-*");
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
    szDSName[sizeof(szDSName)-1] = 0;
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
/*                         CollectADSMetadata()                         */
/*                                                                      */
/*      Collect metadata from envisat ADS and GADS.                     */
/************************************************************************/

void EnvisatDataset::CollectADSMetadata()
{
    int nDSIndex, nNumDsr, nDSRSize;
    int nRecord;
    const char *pszDSName, *pszDSType, *pszDSFilename;
    const char *pszProduct;
    char *pszRecord;
    char szPrefix[128], szKey[128], szValue[1024];
    int i;
    CPLErr ret;
    const EnvisatRecordDescr *pRecordDescr = NULL;
    const EnvisatFieldDescr *pField;

    pszProduct = EnvisatFile_GetKeyValueAsString( hEnvisatFile, MPH,
                                                  "PRODUCT", "" );

    for( nDSIndex = 0;
         EnvisatFile_GetDatasetInfo( hEnvisatFile, nDSIndex,
                                     (char **) &pszDSName,
                                     (char **) &pszDSType,
                                     (char **) &pszDSFilename,
                                     NULL, NULL,
                                     &nNumDsr, &nDSRSize ) == SUCCESS;
         ++nDSIndex )
    {
        if( EQUALN(pszDSFilename,"NOT USED",8) || (nNumDsr <= 0) )
            continue;
        if( !EQUAL(pszDSType,"A") && !EQUAL(pszDSType,"G") )
            continue;

        for ( nRecord = 0; nRecord < nNumDsr; ++nRecord )
        {
            strncpy( szPrefix, pszDSName, sizeof(szPrefix) - 1);
            szPrefix[sizeof(szPrefix) - 1] = '\0';

            // strip trailing spaces
            for( i = strlen(szPrefix)-1; i && szPrefix[i] == ' '; --i )
                szPrefix[i] = '\0';

            // convert spaces into underscores
            for( i = 0; szPrefix[i] != '\0'; i++ )
            {
                if( szPrefix[i] == ' ' )
                    szPrefix[i] = '_';
            }

            pszRecord = (char *) CPLMalloc(nDSRSize+1);

            if( EnvisatFile_ReadDatasetRecord( hEnvisatFile, nDSIndex, nRecord,
                                               pszRecord ) == FAILURE )
            {
                CPLFree( pszRecord );
                return;
            }

            pRecordDescr = EnvisatFile_GetRecordDescriptor(pszProduct, pszDSName);
            if (pRecordDescr)
            {
                pField = pRecordDescr->pFields;
                while ( pField && pField->szName )
                {
                    ret = EnvisatFile_GetFieldAsString(pszRecord, nDSRSize,
                                           pField, szValue);
                    if ( ret == CE_None )
                    {
                        if (nNumDsr == 1)
                            sprintf(szKey, "%s_%s", szPrefix, pField->szName);
                        else
                            // sprintf(szKey, "%s_%02d_%s", szPrefix, nRecord,
                            sprintf(szKey, "%s_%d_%s", szPrefix, nRecord,
                                    pField->szName);
                        SetMetadataItem(szKey, szValue, "RECORDS");
                    }
                    // silently ignore conversion errors

                    ++pField;
                }
            }
            CPLFree( pszRecord );
        }
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
    if( poOpenInfo->nHeaderBytes < 8 )
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
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        EnvisatFile_Close( hEnvisatFile );
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The ENVISAT driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
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

    poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to collect GCPs.                                            */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Scan for all datasets matching the reference dataset.           */
/* -------------------------------------------------------------------- */
    int num_dsr2, dsr_size2, iBand = 0;
    const char *pszDSName;
    char szBandName[128];
    bool bMiltiChannel;

    for( ds_index = 0;
         EnvisatFile_GetDatasetInfo( hEnvisatFile, ds_index,
                                     (char **) &pszDSName, NULL, NULL,
                                     &ds_offset, NULL,
                                     &num_dsr2, &dsr_size2 ) == SUCCESS;
         ds_index++ )
    {
        if( !EQUAL(pszDSType,"M") || num_dsr2 != num_dsr )
            continue;

        if( EQUALN(pszProduct,"MER",3) && (pszProduct[8] == '2') &&
            ( (strstr(pszDSName, "MDS(16)") != NULL) ||
              (strstr(pszDSName, "MDS(19)") != NULL)) )
            bMiltiChannel = true;
        else
            bMiltiChannel = false;

        if( (dsr_size2 == dsr_size) && !bMiltiChannel )
        {
            poDS->SetBand( iBand+1,
                       new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                          ds_offset + nPrefixBytes,
                                          GDALGetDataTypeSize(eDataType) / 8,
                                          dsr_size,
                                          eDataType, bNative, TRUE ) );
            iBand++;

            poDS->GetRasterBand(iBand)->SetDescription( pszDSName );
        }
/* -------------------------------------------------------------------- */
/*       Handle MERIS Level 2 datasets with data type different from    */
/*       the one declared in the SPH                                    */
/* -------------------------------------------------------------------- */
        else if( EQUALN(pszProduct,"MER",3) &&
                 (strstr(pszDSName, "Flags") != NULL) )
        {
            if (pszProduct[8] == '1')
            {
                // Flags
                poDS->SetBand( iBand+1,
                           new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                              ds_offset + nPrefixBytes, 3,
                                              dsr_size, GDT_Byte, bNative, TRUE ) );
                iBand++;

                poDS->GetRasterBand(iBand)->SetDescription( pszDSName );

                // Detector indices
                poDS->SetBand( iBand+1,
                           new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                              ds_offset + nPrefixBytes + 1,
                                              3, dsr_size, GDT_Int16,
                                              bNative, TRUE ) );
                iBand++;

                const char *pszSuffix = strstr( pszDSName, "MDS" );
                if ( pszSuffix != NULL)
                    sprintf( szBandName, "Detector index %s", pszSuffix );
                else
                    sprintf( szBandName, "Detector index" );
                poDS->GetRasterBand(iBand)->SetDescription( szBandName );
            }
            else if ( (pszProduct[8] == '2') &&
                      (dsr_size2 >= 3 * poDS->nRasterXSize ) )
            {
                int nFlagPrefixBytes = dsr_size2 - 3 * poDS->nRasterXSize;

                poDS->SetBand( iBand+1,
                       new MerisL2FlagBand( poDS, iBand+1, poDS->fpImage,
                                            ds_offset, nFlagPrefixBytes ) );
                iBand++;

                poDS->GetRasterBand(iBand)->SetDescription( pszDSName );
            }
        }
        else if( EQUALN(pszProduct,"MER",3) && (pszProduct[8] == '2') )
        {
            int nPrefixBytes2, nSubBands, nSubBandIdx, nSubBandOffset;

            int nPixelSize = 1;
            GDALDataType eDataType2 = GDT_Byte;

            nSubBands = dsr_size2 / poDS->nRasterXSize;
            if( (nSubBands < 1) || (nSubBands > 3) )
                nSubBands = 0;

            nPrefixBytes2 = dsr_size2 -
                (nSubBands * nPixelSize * poDS->nRasterXSize);

            for (nSubBandIdx = 0; nSubBandIdx < nSubBands; ++nSubBandIdx)
            {
                nSubBandOffset =
                    ds_offset + nPrefixBytes2 + nSubBandIdx * nPixelSize;
                poDS->SetBand( iBand+1,
                        new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                           nSubBandOffset,
                                           nPixelSize * nSubBands,
                                           dsr_size2, eDataType2, bNative, TRUE ) );
                iBand++;

                if (nSubBands > 1)
                {
                    sprintf( szBandName, "%s (%d)", pszDSName, nSubBandIdx );
                    poDS->GetRasterBand(iBand)->SetDescription( szBandName );
                }
                else
                    poDS->GetRasterBand(iBand)->SetDescription( pszDSName );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    poDS->CollectMetadata( MPH );
    poDS->CollectMetadata( SPH );
    poDS->CollectDSDMetadata();
    poDS->CollectADSMetadata();

    if( EQUALN(pszProduct,"MER",3) )
        poDS->ScanForGCPs_MERIS();
    else
        poDS->ScanForGCPs_ASAR();

    /* unwrap GCPs for products crossing date border */
    poDS->UnwrapGCPs();

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
/*                         GDALRegister_Envisat()                       */
/************************************************************************/

void GDALRegister_Envisat()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ESAT" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "ESAT" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Envisat Image Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#Envisat" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "n1" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = EnvisatDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
