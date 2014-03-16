/******************************************************************************
 * $Id$
 *
 * Project:  PALSAR JAXA imagery reader
 * Purpose:  Support for PALSAR L1.1/1.5 imagery and appropriate metadata from
 *           JAXA and JAXA-supported ground stations (ASF, ESA, etc.). This
 *           driver does not support ERSDAC products.
 * Author:   Philippe Vachon <philippe@cowpig.ca>
 *
 ******************************************************************************
 * Copyright (c) 2007, Philippe P. Vachon <philippe@cowpig.ca>
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
void	GDALRegister_PALSARJaxa(void);
CPL_C_END

#if defined(WIN32) || defined(WIN32CE)
#define SEP_STRING "\\"
#else
#define SEP_STRING "/"
#endif

/* read binary fields */
#ifdef CPL_LSB
#define READ_WORD(f, x) \
	do { \
		VSIFReadL( &(x), 4, 1, (f) ); \
		(x) = CPL_SWAP32( (x) ); \
	} while (0);
#define READ_SHORT(f, x) \
	do { \
		VSIFReadL( &(x), 2, 1, (f) ); \
		(x) = CPL_SWAP16( (x) ); \
	} while (0);
#else
#define READ_WORD(f, x) do { VSIFReadL( &(x), 4, 1, (f) ); } while (0);
#define READ_SHORT(f, x) do { VSIFReadL( &(x), 2, 1, (f) ); } while (0);
#endif /* def CPL_LSB */
#define READ_BYTE(f, x) do { VSIFReadL( &(x), 1, 1, (f) ); } while (0);

/* read floating point value stored as ASCII */
#define READ_CHAR_FLOAT(n, l, f) \
	do {\
		char psBuf[(l+1)]; \
		psBuf[(l)] = '\0'; \
		VSIFReadL( &psBuf, (l), 1, (f) );\
		(n) = CPLAtof( psBuf );\
	} while (0);

/* read numbers stored as ASCII */
#define READ_CHAR_VAL(x, n, f) \
	do { \
		char psBuf[(n+1)]; \
		psBuf[(n)] = '\0';\
		VSIFReadL( &psBuf, (n), 1, (f) ); \
		(x) = atoi(psBuf); \
	} while (0);

/* read string fields 
 * note: string must be size of field to be extracted + 1
 */
#define READ_STRING(s, n, f) \
	do { \
		VSIFReadL( &(s), 1, (n), (f) ); \
		(s)[(n)] = '\0'; \
	} while (0);

/*************************************************************************/
/* a few key offsets in the volume directory file */
#define VOL_DESC_RECORD_LENGTH 360
#define FILE_PTR_RECORD_LENGTH 360
#define NUM_RECORDS_OFFSET 160

/* a few key offsets and values within the File Pointer record */
#define REF_FILE_CLASS_CODE_OFFSET 66
#define REF_FILE_CLASS_CODE_LENGTH 4
#define FILE_NAME_OFFSET 310

/* some image option descriptor records */
#define BITS_PER_SAMPLE_OFFSET 216
#define BITS_PER_SAMPLE_LENGTH 4
#define SAMPLES_PER_GROUP_OFFSET 220
#define SAMPLES_PER_GROUP_LENGTH 4
#define NUMBER_LINES_OFFSET 236
#define NUMBER_LINES_LENGTH 8
#define SAR_DATA_RECORD_LENGTH_OFFSET 186
#define SAR_DATA_RECORD_LENGTH_LENGTH 6

#define IMAGE_OPT_DESC_LENGTH 720

#define SIG_DAT_REC_OFFSET 412
#define PROC_DAT_REC_OFFSET 192

/* metadata to be extracted from the leader file */
#define LEADER_FILE_DESCRIPTOR_LENGTH 720
#define DATA_SET_SUMMARY_LENGTH 4096

/* relative to end of leader file descriptor */
#define EFFECTIVE_LOOKS_AZIMUTH_OFFSET 1174 /* floating point text */
#define EFFECTIVE_LOOKS_AZIMUTH_LENGTH 16

/* relative to leader file descriptor + dataset summary length */
#define PIXEL_SPACING_OFFSET 92
#define LINE_SPACING_OFFSET 108
#define ALPHANUMERIC_PROJECTION_NAME_OFFSET 412
#define TOP_LEFT_LAT_OFFSET 1072
#define TOP_LEFT_LON_OFFSET 1088
#define TOP_RIGHT_LAT_OFFSET 1104
#define TOP_RIGHT_LON_OFFSET 1120
#define BOTTOM_RIGHT_LAT_OFFSET 1136
#define BOTTOM_RIGHT_LON_OFFSET 1152
#define BOTTOM_LEFT_LAT_OFFSET 1168
#define BOTTOM_LEFT_LON_OFFSET 1184

/* a few useful enums */
enum eFileType {
	level_11 = 0,
	level_15,
    level_10
};

enum ePolarization {
	hh = 0,
	hv,
	vh,
	vv
};

/************************************************************************/
/* ==================================================================== */
/*                        PALSARJaxaDataset                             */
/* ==================================================================== */
/************************************************************************/

class PALSARJaxaRasterBand;

class PALSARJaxaDataset : public GDALPamDataset {
    friend class PALSARJaxaRasterBand;
private:
    GDAL_GCP *pasGCPList;
    int nGCPCount;
    eFileType nFileType;
public:
    PALSARJaxaDataset();
    ~PALSARJaxaDataset();

    int GetGCPCount();
    const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static int Identify( GDALOpenInfo *poOpenInfo );
    static void ReadMetadata( PALSARJaxaDataset *poDS, VSILFILE *fp );
};

PALSARJaxaDataset::PALSARJaxaDataset()
{
    pasGCPList = NULL;
    nGCPCount = 0;
}

PALSARJaxaDataset::~PALSARJaxaDataset()
{
    if( nGCPCount > 0 ) 
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList ); 
        CPLFree( pasGCPList ); 
    }
}

/************************************************************************/
/* ==================================================================== */
/*                        PALSARJaxaRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class PALSARJaxaRasterBand : public GDALRasterBand {
    VSILFILE *fp;
    int nRasterXSize;
    int nRasterYSize;
    ePolarization nPolarization;
    eFileType nFileType;
    int nBitsPerSample;
    int nSamplesPerGroup;
    int nRecordSize;
public:
    PALSARJaxaRasterBand( PALSARJaxaDataset *poDS, int nBand, VSILFILE *fp );
    ~PALSARJaxaRasterBand();

    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
};

/************************************************************************/
/*                         PALSARJaxaRasterBand()                       */
/************************************************************************/

PALSARJaxaRasterBand::PALSARJaxaRasterBand( PALSARJaxaDataset *poDS, 
	int nBand, VSILFILE *fp )
{
    this->fp = fp;

    /* Read image options record to determine the type of data */
    VSIFSeekL( fp, BITS_PER_SAMPLE_OFFSET, SEEK_SET );
    nBitsPerSample = 0;
    nSamplesPerGroup = 0;
    READ_CHAR_VAL( nBitsPerSample, BITS_PER_SAMPLE_LENGTH, fp );
    READ_CHAR_VAL( nSamplesPerGroup, SAMPLES_PER_GROUP_LENGTH, fp );

    if (nBitsPerSample == 32 && nSamplesPerGroup == 2) {
        eDataType = GDT_CFloat32;
        nFileType = level_11;
    }
    else if (nBitsPerSample == 8 && nSamplesPerGroup == 2) {
        eDataType = GDT_CInt16; /* shuold be 2 x signed byte */
        nFileType = level_10;
    }
    else {
        eDataType = GDT_UInt16;
        nFileType = level_15;
    }

    poDS->nFileType = nFileType;

    /* Read number of range/azimuth lines */
    VSIFSeekL( fp, NUMBER_LINES_OFFSET, SEEK_SET );
    READ_CHAR_VAL( nRasterYSize, NUMBER_LINES_LENGTH, fp );
    VSIFSeekL( fp, SAR_DATA_RECORD_LENGTH_OFFSET, SEEK_SET );
    READ_CHAR_VAL( nRecordSize, SAR_DATA_RECORD_LENGTH_LENGTH, fp );
    nRasterXSize = (nRecordSize -
                    (nFileType != level_15 ? SIG_DAT_REC_OFFSET : PROC_DAT_REC_OFFSET))
        / ((nBitsPerSample / 8) * nSamplesPerGroup);

    poDS->nRasterXSize = nRasterXSize;
    poDS->nRasterYSize = nRasterYSize;

    /* Polarization */
    switch (nBand) {
      case 0:
        nPolarization = hh;
        SetMetadataItem( "POLARIMETRIC_INTERP", "HH" );
        break;
      case 1:
        nPolarization = hv;
        SetMetadataItem( "POLARIMETRIC_INTERP", "HV" );
        break;
      case 2:
        nPolarization = vh;
        SetMetadataItem( "POLARIMETRIC_INTERP", "VH" );
        break;
      case 3:
        nPolarization = vv;
        SetMetadataItem( "POLARIMETRIC_INTERP", "VV" );
        break;
    }
	
    /* size of block we can read */
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;

    /* set the file pointer to the first SAR data record */
    VSIFSeekL( fp, IMAGE_OPT_DESC_LENGTH, SEEK_SET );
}	

/************************************************************************/
/*                        ~PALSARJaxaRasterBand()                       */
/************************************************************************/

PALSARJaxaRasterBand::~PALSARJaxaRasterBand()
{
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PALSARJaxaRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
	void *pImage )
{
    int nNumBytes = 0;
    if (nFileType == level_11) {
        nNumBytes = 8;
    }
    else {
        nNumBytes = 2;
    }

    int nOffset = IMAGE_OPT_DESC_LENGTH + ((nBlockYOff - 1) * nRecordSize) + 
        (nFileType == level_11 ? SIG_DAT_REC_OFFSET : PROC_DAT_REC_OFFSET);

    VSIFSeekL( fp, nOffset, SEEK_SET );
    VSIFReadL( pImage, nNumBytes, nRasterXSize, fp );

#ifdef CPL_LSB
    if (nFileType == level_11)
        GDALSwapWords( pImage, 4, nBlockXSize * 2, 4 );
    else 
        GDALSwapWords( pImage, 2, nBlockXSize, 2 );
#endif

    return CE_None;
}


/************************************************************************/
/* ==================================================================== */
/* 			PALSARJaxaDataset			     	*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          ReadMetadata()                              */
/************************************************************************/

int PALSARJaxaDataset::GetGCPCount() {
    return nGCPCount;
}


/************************************************************************/
/*                             GetGCPs()                                */
/************************************************************************/

const GDAL_GCP *PALSARJaxaDataset::GetGCPs() {
    return pasGCPList;
}


/************************************************************************/
/*                            ReadMetadata()                            */
/************************************************************************/

void PALSARJaxaDataset::ReadMetadata( PALSARJaxaDataset *poDS, VSILFILE *fp ) {
    /* seek to the end fo the leader file descriptor */
    VSIFSeekL( fp, LEADER_FILE_DESCRIPTOR_LENGTH, SEEK_SET );
    if (poDS->nFileType == level_10) {
        poDS->SetMetadataItem( "PRODUCT_LEVEL", "1.0" );
        poDS->SetMetadataItem( "AZIMUTH_LOOKS", "1.0" );
    }
    else if (poDS->nFileType == level_11) {
        poDS->SetMetadataItem( "PRODUCT_LEVEL", "1.1" );
        poDS->SetMetadataItem( "AZIMUTH_LOOKS", "1.0" );
    }
    else {
        poDS->SetMetadataItem( "PRODUCT_LEVEL", "1.5" );
        /* extract equivalent number of looks */
        VSIFSeekL( fp, LEADER_FILE_DESCRIPTOR_LENGTH + 
                  EFFECTIVE_LOOKS_AZIMUTH_OFFSET, SEEK_SET );
        char pszENL[17];
        double dfENL;
        READ_CHAR_FLOAT(dfENL, 16, fp);
        sprintf( pszENL, "%-16.1f", dfENL );
        poDS->SetMetadataItem( "AZIMUTH_LOOKS", pszENL );

        /* extract pixel spacings */
        VSIFSeekL( fp, LEADER_FILE_DESCRIPTOR_LENGTH +
                  DATA_SET_SUMMARY_LENGTH + PIXEL_SPACING_OFFSET, SEEK_SET );
        double dfPixelSpacing;
        double dfLineSpacing;
        char pszPixelSpacing[33];
        char pszLineSpacing[33];
        READ_CHAR_FLOAT(dfPixelSpacing, 16, fp);
        READ_CHAR_FLOAT(dfLineSpacing, 16, fp);
        sprintf( pszPixelSpacing, "%-32.1f",dfPixelSpacing );
        sprintf( pszLineSpacing, "%-32.1f", dfLineSpacing );
        poDS->SetMetadataItem( "PIXEL_SPACING", pszPixelSpacing );
        poDS->SetMetadataItem( "LINE_SPACING", pszPixelSpacing );

        /* Alphanumeric projection name */
        VSIFSeekL( fp, LEADER_FILE_DESCRIPTOR_LENGTH +
                  DATA_SET_SUMMARY_LENGTH + ALPHANUMERIC_PROJECTION_NAME_OFFSET,
                  SEEK_SET );
        char pszProjName[33];
        READ_STRING(pszProjName, 32, fp);
        poDS->SetMetadataItem( "PROJECTION_NAME", pszProjName );
		
        /* Extract corner GCPs */
        poDS->nGCPCount = 4;
        poDS->pasGCPList = (GDAL_GCP *)CPLCalloc( sizeof(GDAL_GCP), 
                                                  poDS->nGCPCount );
        GDALInitGCPs( poDS->nGCPCount, poDS->pasGCPList );

        /* setup the GCPs */
        int i;
        for (i = 0; i < poDS->nGCPCount; i++) {
            char pszID[2];
            sprintf( pszID, "%d", i + 1);
            CPLFree(poDS->pasGCPList[i].pszId);
            poDS->pasGCPList[i].pszId = CPLStrdup( pszID );
            poDS->pasGCPList[i].dfGCPZ = 0.0;
        }

        double dfTemp = 0.0;
        /* seek to start of GCPs */
        VSIFSeekL( fp, LEADER_FILE_DESCRIPTOR_LENGTH +
                  DATA_SET_SUMMARY_LENGTH + TOP_LEFT_LAT_OFFSET, SEEK_SET );
		
        /* top-left GCP */
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[0].dfGCPY = dfTemp;
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[0].dfGCPX = dfTemp;
        poDS->pasGCPList[0].dfGCPLine = 0.5;
        poDS->pasGCPList[0].dfGCPPixel = 0.5;

        /* top right GCP */
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[1].dfGCPY = dfTemp;
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[1].dfGCPX = dfTemp;
        poDS->pasGCPList[1].dfGCPLine = 0.5;
        poDS->pasGCPList[1].dfGCPPixel = poDS->nRasterYSize - 0.5;

        /* bottom right GCP */
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[2].dfGCPY = dfTemp;
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[2].dfGCPX = dfTemp;
        poDS->pasGCPList[2].dfGCPLine = poDS->nRasterYSize - 0.5;
        poDS->pasGCPList[2].dfGCPPixel = poDS->nRasterYSize - 0.5;

        /* bottom left GCP */
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[3].dfGCPY = dfTemp;
        READ_CHAR_FLOAT(dfTemp, 16, fp);
        poDS->pasGCPList[3].dfGCPX = dfTemp;
        poDS->pasGCPList[3].dfGCPLine = poDS->nRasterYSize - 0.5;
        poDS->pasGCPList[3].dfGCPPixel = 0.5;
    }

    /* some generic metadata items */
    poDS->SetMetadataItem( "SENSOR_BAND", "L" ); /* PALSAR is L-band */
    poDS->SetMetadataItem( "RANGE_LOOKS", "1.0" );

    /* Check if this is a PolSAR dataset */
    if ( poDS->GetRasterCount() == 4 ) {
        /* PALSAR data is only available from JAXA in Scattering Matrix form */
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
    }

}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PALSARJaxaDataset::Identify( GDALOpenInfo *poOpenInfo ) {
    if ( poOpenInfo->nHeaderBytes < 360 )
        return 0;

    /* First, check that this is a PALSAR image indeed */
    if ( !EQUALN((char *)(poOpenInfo->pabyHeader + 60),"AL", 2) 
         || !EQUALN(CPLGetBasename((char *)(poOpenInfo->pszFilename)) + 4, 
                    "ALPSR", 5) )
    {
        return 0;
    }

    VSILFILE *fpL = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( fpL == NULL )
        return FALSE;

    /* Check that this is a volume directory file */
    int nRecordSeq = 0;
    int nRecordSubtype = 0;
    int nRecordType = 0;
    int nSecondSubtype = 0;
    int nThirdSubtype = 0;
    int nLengthRecord = 0;

    VSIFSeekL(fpL, 0, SEEK_SET);

    READ_WORD(fpL, nRecordSeq);
    READ_BYTE(fpL, nRecordSubtype);
    READ_BYTE(fpL, nRecordType);
    READ_BYTE(fpL, nSecondSubtype);
    READ_BYTE(fpL, nThirdSubtype);
    READ_WORD(fpL, nLengthRecord);

    VSIFCloseL( fpL );

    /* Check that we have the right record */
    if ( nRecordSeq == 1 && nRecordSubtype == 192 && nRecordType == 192 &&
         nSecondSubtype == 18 && nThirdSubtype == 18 && nLengthRecord == 360 )
    {
        return 1;
    }

    return 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *PALSARJaxaDataset::Open( GDALOpenInfo * poOpenInfo ) {
    /* Check that this actually is a JAXA PALSAR product */
    if ( !PALSARJaxaDataset::Identify(poOpenInfo) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The JAXAPALSAR driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    PALSARJaxaDataset *poDS = new PALSARJaxaDataset();

    /* Get the suffix of the filename, we'll need this */
    char *pszSuffix = VSIStrdup( (char *)
                                 (CPLGetFilename( poOpenInfo->pszFilename ) + 3) );

    /* Try to read each of the polarizations */
    char *pszImgFile = (char *)VSIMalloc( 
        strlen( CPLGetDirname( poOpenInfo->pszFilename ) ) + 
        strlen( pszSuffix ) + 8 );

    int nBandNum = 1;

    /* HH */
    VSILFILE *fpHH;
    sprintf( pszImgFile, "%s%sIMG-HH%s", 
             CPLGetDirname(poOpenInfo->pszFilename), SEP_STRING, pszSuffix );
    fpHH = VSIFOpenL( pszImgFile, "rb" );
    if (fpHH != NULL) { 
        poDS->SetBand( nBandNum, new PALSARJaxaRasterBand( poDS, 0, fpHH ) );
        nBandNum++;
    }

    /* HV */
    VSILFILE *fpHV;
    sprintf( pszImgFile, "%s%sIMG-HV%s", 
             CPLGetDirname(poOpenInfo->pszFilename), SEP_STRING, pszSuffix );
    fpHV = VSIFOpenL( pszImgFile, "rb" );
    if (fpHV != NULL) {
        poDS->SetBand( nBandNum, new PALSARJaxaRasterBand( poDS, 1, fpHV ) );
        nBandNum++;
    }

    /* VH */
    VSILFILE *fpVH;
    sprintf( pszImgFile, "%s%sIMG-VH%s", 
             CPLGetDirname(poOpenInfo->pszFilename), SEP_STRING, pszSuffix );
    fpVH = VSIFOpenL( pszImgFile, "rb" );
    if (fpVH != NULL) {
        poDS->SetBand( nBandNum, new PALSARJaxaRasterBand( poDS, 2, fpVH ) );
        nBandNum++;
    }

    /* VV */
    VSILFILE *fpVV;
    sprintf( pszImgFile, "%s%sIMG-VV%s",
             CPLGetDirname(poOpenInfo->pszFilename), SEP_STRING, pszSuffix );
    fpVV = VSIFOpenL( pszImgFile, "rb" );
    if (fpVV != NULL) {
        poDS->SetBand( nBandNum, new PALSARJaxaRasterBand( poDS, 3, fpVV ) );
        nBandNum++;
    }

    VSIFree( pszImgFile );

    /* did we get at least one band? */
    if (fpVV == NULL && fpVH == NULL && fpHV == NULL && fpHH == NULL) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find any image data. Aborting opening as PALSAR image.");
        delete poDS;
        VSIFree( pszSuffix );
        return NULL;
    }

    /* Level 1.0 products are not supported */
    if (poDS->nFileType == level_10) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ALOS PALSAR Level 1.0 products are not supported. Aborting opening as PALSAR image.");
        delete poDS;
        VSIFree( pszSuffix );
        return NULL;
    }

    /* read metadata from Leader file. */
    char *pszLeaderFilename = (char *)VSIMalloc( 
        strlen( CPLGetDirname( poOpenInfo->pszFilename ) ) + 
        strlen(pszSuffix) + 5 );
    sprintf( pszLeaderFilename, "%s%sLED%s", 
             CPLGetDirname( poOpenInfo->pszFilename ) , SEP_STRING, pszSuffix );

    VSILFILE *fpLeader = VSIFOpenL( pszLeaderFilename, "rb" );
    /* check if the leader is actually present */
    if (fpLeader != NULL) {
        ReadMetadata(poDS, fpLeader);
        VSIFCloseL(fpLeader);
    }

    VSIFree(pszLeaderFilename);

    VSIFree( pszSuffix );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                      GDALRegister_PALSARJaxa()                       */
/************************************************************************/

void GDALRegister_PALSARJaxa() {
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JAXAPALSAR" ) == NULL ) {
        poDriver = new GDALDriver();
        poDriver->SetDescription( "JAXAPALSAR" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JAXA PALSAR Product Reader (Level 1.1/1.5)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_palsar.html" );
        poDriver->pfnOpen = PALSARJaxaDataset::Open;
        poDriver->pfnIdentify = PALSARJaxaDataset::Identify;
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
