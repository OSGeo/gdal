/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  GDALDataset driver for CEOS translator.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc.
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
 * Revision 1.14  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.13  2001/07/16 16:42:32  warmerda
 * Added radiometric data record type code.
 *
 * Revision 1.12  2001/01/24 22:35:34  warmerda
 * multi-record scanline support added to ceos2
 *
 * Revision 1.11  2000/11/22 19:14:16  warmerda
 * added CEOS_DM_* metadata items
 *
 * Revision 1.10  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.9  2000/07/13 14:41:36  warmerda
 * fixed byte order
 *
 * Revision 1.8  2000/06/12 15:24:24  warmerda
 * Fixed byte order handling.
 *
 * Revision 1.7  2000/06/05 17:24:06  warmerda
 * added real complex support
 *
 * Revision 1.6  2000/05/15 14:18:27  warmerda
 * added COMPLEX_INTERPRETATION metadata
 *
 * Revision 1.5  2000/04/21 21:59:04  warmerda
 * added overview support, updated metadata handling
 *
 * Revision 1.4  2000/04/17 21:51:42  warmerda
 * added metadata support
 *
 * Revision 1.3  2000/04/07 19:39:16  warmerda
 * added some metadata
 *
 * Revision 1.2  2000/04/04 22:25:18  warmerda
 * Added logic to read gcps if available.
 *
 * Revision 1.1  2000/03/31 13:32:49  warmerda
 * New
 */

#include "ceos.h"
#include "gdal_priv.h"
#include "../raw/rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static GDALDriver	*poCEOSDriver = NULL;

CPL_C_START
void	GDALRegister_SAR_CEOS(void);
CPL_C_END

char *CeosExtension[][5] = { 
{ "vol", "led", "img", "trl", "nul" },
{ "vol", "lea", "img", "trl", "nul" },
{ "vol", "led", "img", "tra", "nul" },
{ "vol", "lea", "img", "tra", "nul" },

/* Radarsat: basename, not extension */
{ "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_vdf"},

/* Ers-1: basename, not extension */
{ "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_dat"},

/* end marker */
{ NULL, NULL, NULL, NULL, NULL } 
};

static int 
ProcessData( FILE *fp, int fileid, CeosSARVolume_t *sar, int max_records, 
             int max_bytes );


static CeosTypeCode_t QuadToTC( int a, int b, int c, int d )
{
    CeosTypeCode_t   abcd;

    abcd.UCharCode.Subtype1 = a;
    abcd.UCharCode.Type = b;
    abcd.UCharCode.Subtype2 = c;
    abcd.UCharCode.Subtype3 = d;

    return abcd;
}

#define LEADER_DATASET_SUMMARY_TC          QuadToTC( 18, 10, 18, 20 )
#define LEADER_RADIOMETRIC_COMPENSATION_TC QuadToTC( 18, 51, 18, 20 )
#define VOLUME_DESCRIPTOR_RECORD_TC        QuadToTC( 192, 192, 18, 18 )
#define IMAGE_HEADER_RECORD_TC             QuadToTC( 63, 192, 18, 18 )
#define LEADER_RADIOMETRIC_DATA_RECORD_TC  QuadToTC( 18, 50, 18, 20 )

#define PROC_PARAM_RECORD_TYPECODE { 18, 120, 18, 20 }
#define RAD_MET_RECORD_TYPECODE    { 18, 50, 18, 20 }
#define MAP_PROJ_RECORD_TYPECODE   { 18, 20, 18, 20 }

/************************************************************************/
/* ==================================================================== */
/*				SAR_CEOSDataset				*/
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand;

class SAR_CEOSDataset : public GDALDataset
{
    friend	SAR_CEOSRasterBand;

    CeosSARVolume_t sVolume;

    FILE	*fpImage;

    
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ScanForGCPs();
    void        ScanForMetadata();

  public:
                SAR_CEOSDataset();
                ~SAR_CEOSDataset();

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                       SAR_CEOSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand : public GDALRasterBand
{
    friend	SAR_CEOSDataset;

  public:
                   SAR_CEOSRasterBand( SAR_CEOSDataset *, int, GDALDataType );

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                         SAR_CEOSRasterBand()                         */
/************************************************************************/

SAR_CEOSRasterBand::SAR_CEOSRasterBand( SAR_CEOSDataset *poGDS, int nBand,
                                        GDALDataType eType )

{
    this->poDS = poGDS;
    this->nBand = nBand;
    
    eDataType = eType;

    nBlockXSize = poGDS->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAR_CEOSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                       void * pImage )

{
    struct CeosSARImageDesc *ImageDesc;
    int	   offset;
    GByte  *pabyRecord;
    SAR_CEOSDataset *poGDS = (SAR_CEOSDataset *) poDS;

    ImageDesc = &(poGDS->sVolume.ImageDesc);

    CalcCeosSARImageFilePosition( &(poGDS->sVolume), nBand,
                                  nBlockYOff + 1, NULL, &offset );

    offset += ImageDesc->ImageDataStart;

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/*      Ensure we handle multiple record scanlines properly.            */
/* -------------------------------------------------------------------- */
    int		iRecord, nPixelsRead = 0;

    pabyRecord = (GByte *) CPLMalloc( ImageDesc->BytesPerPixel * nBlockXSize );
    
    for( iRecord = 0; iRecord < ImageDesc->RecordsPerLine; iRecord++ )
    {
        int	nPixelsToRead;

        if( nPixelsRead + ImageDesc->PixelsPerRecord > nBlockXSize )
            nPixelsToRead = nBlockXSize - nPixelsRead;
        else
            nPixelsToRead = ImageDesc->PixelsPerRecord;
        
        VSIFSeek( poGDS->fpImage, offset, SEEK_SET );
        VSIFRead( pabyRecord + nPixelsRead * ImageDesc->BytesPerPixel, 
                  1, nPixelsToRead * ImageDesc->BytesPerPixel, 
                  poGDS->fpImage );

        nPixelsRead += nPixelsToRead;
        offset += ImageDesc->BytesPerRecord;
    }
    
/* -------------------------------------------------------------------- */
/*      Copy the desired band out based on the size of the type, and    */
/*      the interleaving mode.                                          */
/* -------------------------------------------------------------------- */
    int		nBytesPerSample = GDALGetDataTypeSize( eDataType ) / 8;

    if( ImageDesc->ChannelInterleaving == __CEOS_IL_PIXEL )
    {
        GDALCopyWords( pabyRecord + (nBand-1) * nBytesPerSample, 
                       eDataType, ImageDesc->BytesPerPixel, 
                       pImage, eDataType, nBytesPerSample, 
                       nBlockXSize );
    }
    else if( ImageDesc->ChannelInterleaving == __CEOS_IL_LINE )
    {
        GDALCopyWords( pabyRecord + (nBand-1) * nBytesPerSample * nBlockXSize, 
                       eDataType, nBytesPerSample, 
                       pImage, eDataType, nBytesPerSample, 
                       nBlockXSize );
    }
    else if( ImageDesc->ChannelInterleaving == __CEOS_IL_BAND )
    {
        memcpy( pImage, pabyRecord, nBytesPerSample * nBlockXSize );
    }

#ifdef CPL_LSB
    GDALSwapWords( pImage, nBytesPerSample, nBlockXSize, nBytesPerSample );
#endif    

    CPLFree( pabyRecord );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				SAR_CEOSDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          SAR_CEOSDataset()                           */
/************************************************************************/

SAR_CEOSDataset::SAR_CEOSDataset()

{
    fpImage = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                          ~SAR_CEOSDataset()                          */
/************************************************************************/

SAR_CEOSDataset::~SAR_CEOSDataset()

{
    if( fpImage != NULL )
        VSIFClose( fpImage );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( sVolume.RecordList )
    {
        Link_t	*Links;

        for(Links = sVolume.RecordList; Links != NULL; Links = Links->next)
        {
            if(Links->object)
            {
                DeleteCeosRecord( (CeosRecord_t *) Links->object );
                Links->object = NULL;
            }
        }
        DestroyList( sVolume.RecordList );
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int SAR_CEOSDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *SAR_CEOSDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *SAR_CEOSDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          ScanForMetadata()                           */
/************************************************************************/

void SAR_CEOSDataset::ScanForMetadata() 

{
    char szField[128], szVolId[128];
    CeosRecord_t *record;

/* -------------------------------------------------------------------- */
/*      Get the volume id (with the sensor name)                        */
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, VOLUME_DESCRIPTOR_RECORD_TC,
                             __CEOS_VOLUME_DIR_FILE, -1, -1 );
    szVolId[0] = '\0';
    if( record != NULL )
    {
        szVolId[16] = '\0';

        GetCeosField( record, 61, "A16", szVolId );

        SetMetadataItem( "CEOS_LOGICAL_VOLUME_ID", szVolId );

/* -------------------------------------------------------------------- */
/*      Processing facility                                             */
/* -------------------------------------------------------------------- */
        szField[0] = '\0';
        szField[12] = '\0';

        GetCeosField( record, 149, "A12", szField );

        if( !EQUALN(szField,"            ",12) )
            SetMetadataItem( "CEOS_PROCESSING_FACILITY", szField );

/* -------------------------------------------------------------------- */
/*      Agency                                                          */
/* -------------------------------------------------------------------- */
        szField[8] = '\0';

        GetCeosField( record, 141, "A8", szField );

        if( !EQUALN(szField,"            ",8) )
            SetMetadataItem( "CEOS_PROCESSING_AGENCY", szField );

/* -------------------------------------------------------------------- */
/*      Country                                                         */
/* -------------------------------------------------------------------- */
        szField[12] = '\0';

        GetCeosField( record, 129, "A12", szField );

        if( !EQUALN(szField,"            ",12) )
            SetMetadataItem( "CEOS_PROCESSING_COUNTRY", szField );

/* -------------------------------------------------------------------- */
/*      software id.                                                    */
/* -------------------------------------------------------------------- */
        szField[12] = '\0';

        GetCeosField( record, 33, "A12", szField );

        if( !EQUALN(szField,"            ",12) )
            SetMetadataItem( "CEOS_SOFTWARE_ID", szField );
    }

/* ==================================================================== */
/*      Dataset summary record.                                         */
/* ==================================================================== */
    record = FindCeosRecord( sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                                 __CEOS_TRAILER_FILE, -1, -1 );

    if( record != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Get the acquisition date.                                       */
/* -------------------------------------------------------------------- */
        szField[0] = '\0';
        szField[32] = '\0';

        GetCeosField( record, 69, "A32", szField );

        SetMetadataItem( "CEOS_ACQUISITION_TIME", szField );

/* -------------------------------------------------------------------- */
/*      Look Angle.                                                     */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 477, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_SENSOR_CLOCK_ANGLE", szField );

/* -------------------------------------------------------------------- */
/*      Ascending/Descending                                            */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 101, "A16", szField );
        szField[16] = '\0';

        if( strstr(szVolId,"RSAT") != NULL 
            && !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_ASC_DES", szField );

/* -------------------------------------------------------------------- */
/*      Ellipsoid                                                       */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 165, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_ELLIPSOID", szField );

/* -------------------------------------------------------------------- */
/*      Semimajor, semiminor axis                                       */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 181, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_SEMI_MAJOR", szField );

        GetCeosField( record, 197, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_SEMI_MINOR", szField );
    }

/* -------------------------------------------------------------------- */
/*      Get the beam mode, for radarsat.                                */
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, 
                             LEADER_RADIOMETRIC_COMPENSATION_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( strstr(szVolId,"RSAT") != NULL && record != NULL )
    {
        szField[16] = '\0';

        GetCeosField( record, 4189, "A16", szField );

        papszMetadata = 
            CSLSetNameValue( papszMetadata, "CEOS_BEAM_TYPE", 
                             szField );
    }

/* -------------------------------------------------------------------- */
/*	Get process-to-raw data coordinate translation values.  These	*/
/*	are likely specific to Atlantis APP products.			*/
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, 
                             IMAGE_HEADER_RECORD_TC,
                             __CEOS_IMAGRY_OPT_FILE, -1, -1 );

    if( record != NULL )
    {
        GetCeosField( record, 449, "A4", szField );
        szField[4] = '\0';

        if( !EQUALN(szField,"    ",4 ) )
            SetMetadataItem( "CEOS_DM_CORNER", szField );


        GetCeosField( record, 453, "A4", szField );
        szField[4] = '\0';

        if( !EQUALN(szField,"    ",4 ) )
            SetMetadataItem( "CEOS_DM_TRANSPOSE", szField );


        GetCeosField( record, 457, "A4", szField );
        szField[4] = '\0';

        if( !EQUALN(szField,"    ",4 ) )
            SetMetadataItem( "CEOS_DM_START_SAMPLE", szField );


        GetCeosField( record, 461, "A5", szField );
        szField[5] = '\0';

        if( !EQUALN(szField,"     ",5 ) )
            SetMetadataItem( "CEOS_DM_START_PULSE", szField );


        GetCeosField( record, 466, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_DM_FAST_ALPHA", szField );


        GetCeosField( record, 482, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_DM_FAST_BETA", szField );


        GetCeosField( record, 498, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_DM_SLOW_ALPHA", szField );


        GetCeosField( record, 514, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_DM_SLOW_BETA", szField );


        GetCeosField( record, 530, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_DM_FAST_ALPHA_2", szField );

    }

/* -------------------------------------------------------------------- */
/*      Try to find calibration information from Radiometric Data       */
/*      Record.                                                         */
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, 
                             LEADER_RADIOMETRIC_DATA_RECORD_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record != NULL )
    {
        /* perhaps add something here eventually */
    }
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void SAR_CEOSDataset::ScanForGCPs()

{
    int    iScanline, nStep, nGCPMax = 15;

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPMax);

    nStep = (GetRasterYSize()-1) / (nGCPMax / 3 - 1);
    for( iScanline = 0; iScanline < GetRasterYSize(); iScanline += nStep )
    {
        int   nFileOffset, iGCP;
        GInt32 anRecord[192/4];

        if( nGCPCount > nGCPMax-3 )
            break;

        CalcCeosSARImageFilePosition( &sVolume, 1, iScanline+1, NULL, 
                                      &nFileOffset );

        if( VSIFSeek( fpImage, nFileOffset, SEEK_SET ) != 0 
            || VSIFRead( anRecord, 1, 192, fpImage ) != 192 )
            break;
        
        /* loop over first, middle and last pixel gcps */

        for( iGCP = 0; iGCP < 3; iGCP++ )
        {
            int nLat, nLong;

            nLat  = CPL_MSBWORD32( anRecord[132/4 + iGCP] );
            nLong = CPL_MSBWORD32( anRecord[144/4 + iGCP] );

            if( nLat != 0 || nLong != 0 )
            {
                char      szId[32];

                GDALInitGCPs( 1, pasGCPList + nGCPCount );

                CPLFree( pasGCPList[nGCPCount].pszId );

                sprintf( szId, "%d", nGCPCount+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szId );
                
                pasGCPList[nGCPCount].dfGCPX = nLong / 1000000.0;
                pasGCPList[nGCPCount].dfGCPY = nLat / 1000000.0;
                pasGCPList[nGCPCount].dfGCPZ = 0.0;

                pasGCPList[nGCPCount].dfGCPLine = iScanline + 0.5;

                if( iGCP == 0 )
                    pasGCPList[nGCPCount].dfGCPPixel = 0.5;
                else if( iGCP == 1 )
                    pasGCPList[nGCPCount].dfGCPPixel = 
                        GetRasterXSize() / 2.0;
                else 
                    pasGCPList[nGCPCount].dfGCPPixel = 
                        GetRasterXSize() - 0.5;

                nGCPCount++;
            }
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAR_CEOSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNative;
    
/* -------------------------------------------------------------------- */
/*      Does this appear to be a valid ceos leader record?              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL 
        || poOpenInfo->nHeaderBytes < __CEOS_HEADER_LENGTH )
        return NULL;

    if( (poOpenInfo->pabyHeader[4] != 0x3f
         && poOpenInfo->pabyHeader[4] != 0x32)
        || poOpenInfo->pabyHeader[5] != 0xc0
        || poOpenInfo->pabyHeader[6] != 0x12
        || poOpenInfo->pabyHeader[7] != 0x12 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SAR_CEOSDataset 	*poDS;
    CeosSARVolume_t     *psVolume;

    poDS = new SAR_CEOSDataset();

    poDS->poDriver = poCEOSDriver;

    psVolume = &(poDS->sVolume);
    InitCeosSARVolume( psVolume, 0 );

/* -------------------------------------------------------------------- */
/*      Try to read the current file as an imagery file.                */
/* -------------------------------------------------------------------- */
    psVolume->ImagryOptionsFile = TRUE;
    if( ProcessData( poOpenInfo->fp, __CEOS_IMAGRY_OPT_FILE, psVolume, 4, -1) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Corrupted or unknown CEOS format:\n%s", 
                  poOpenInfo->pszFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Try the various filenames.                                      */
/* -------------------------------------------------------------------- */
    char *pszPath;
    char *pszBasename;
    char *pszExtension;
    int  nBand, iFile;

    pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));
    pszExtension = CPLStrdup(CPLGetExtension(poOpenInfo->pszFilename));
    if( strlen(pszBasename) > 4 )
        nBand = atoi( pszBasename + 4 );
    else
        nBand = 0;

    for( iFile = 0; iFile < 5;iFile++ )
    {
        int	e;

        /* skip image file ... we already did it */
        if( iFile == 2 )
            continue;

        e = 0;
        while( CeosExtension[e][iFile] != NULL )
        {
            FILE	*process_fp;
            char *pszFilename;
            
            /* build filename */
            if( strlen(CeosExtension[e][iFile]) > 3 )
            {
                char    szMadeBasename[32];

                sprintf( szMadeBasename, CeosExtension[e][iFile], nBand );
                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,szMadeBasename, pszExtension));
            }
            else
            {
                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,pszBasename,
                                    CeosExtension[e][iFile]));
            }

            /* try to open */
            process_fp = VSIFOpen( pszFilename, "rb" );

            /* try upper case */
            if( process_fp == NULL )
            {
                for( i = strlen(pszFilename)-1; 
                     i >= 0 && pszFilename[i] != '/' && pszFilename[i] != '\\';
                     i-- )
                {
                    if( pszFilename[i] >= 'a' && pszFilename[i] <= 'z' )
                        pszFilename[i] = pszFilename[i] - 'a' + 'A';
                }

                process_fp = VSIFOpen( pszFilename, "rb" );
            }

            if( process_fp != NULL )
            {
                CPLDebug( "CEOS", "Opened %s.\n", pszFilename );

                VSIFSeek( process_fp, 0, SEEK_END );
                if( ProcessData( process_fp, iFile, psVolume, -1, 
                                 VSIFTell( process_fp ) ) == 0 )
                {
                    switch( iFile )
                    {
                      case 0: psVolume->VolumeDirectoryFile = TRUE;
                        break;
                      case 1: psVolume->SARLeaderFile = TRUE;
                        break;
                      case 3: psVolume->SARTrailerFile = TRUE;
                        break;
                      case 4: psVolume->NullVolumeDirectoryFile = TRUE;
                        break;
                    }

                    VSIFClose( process_fp );
                    break; /* Exit the while loop, we have this data type*/
                }
                    
                VSIFClose( process_fp );
            }

            CPLFree( pszFilename );

            e++;
        }
    }

    CPLFree( pszPath );
    CPLFree( pszBasename );
    CPLFree( pszExtension );

/* -------------------------------------------------------------------- */
/*      Check that we have an image description.                        */
/* -------------------------------------------------------------------- */
    struct CeosSARImageDesc   *psImageDesc;

    GetCeosSARImageDesc( psVolume );
    psImageDesc = &(psVolume->ImageDesc);
    if( !psImageDesc->ImageDescValid )
    {
        delete poDS;

        CPLDebug( "CEOS", 
                  "Unable to extract CEOS image description\n"
                  "from %s.", 
                  poOpenInfo->pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish image type.                                           */
/* -------------------------------------------------------------------- */
    GDALDataType eType;

    switch( psImageDesc->DataType )
    {
      case __CEOS_TYP_CHAR:
      case __CEOS_TYP_UCHAR:
        eType = GDT_Byte;
        break;

      case __CEOS_TYP_SHORT:
        eType = GDT_Int16;
        break;

      case __CEOS_TYP_COMPLEX_SHORT:
        eType = GDT_CInt16;
        break;

      case __CEOS_TYP_USHORT:
        eType = GDT_UInt16;
        break;

      case __CEOS_TYP_LONG:
        eType = GDT_Int32;
        break;

      case __CEOS_TYP_ULONG:
        eType = GDT_UInt32;
        break;

      case __CEOS_TYP_FLOAT:
        eType = GDT_Float32;
        break;

      case __CEOS_TYP_DOUBLE:
        eType = GDT_Float64;
        break;

      case __CEOS_TYP_COMPLEX_FLOAT:
        eType = GDT_CFloat32;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported CEOS image data type %d.\n", 
                  psImageDesc->DataType );
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psImageDesc->PixelsPerLine;
    poDS->nRasterYSize = psImageDesc->Lines;

#ifdef CPL_LSB
    bNative = FALSE;
#else
    bNative = TRUE;
#endif
    
/* -------------------------------------------------------------------- */
/*      Roll our own ...                                                */
/* -------------------------------------------------------------------- */
    if( psImageDesc->RecordsPerLine > 1
        || psImageDesc->DataType == __CEOS_TYP_CHAR
        || psImageDesc->DataType == __CEOS_TYP_LONG
        || psImageDesc->DataType == __CEOS_TYP_ULONG
        || psImageDesc->DataType == __CEOS_TYP_DOUBLE )
    {
        for( int iBand = 0; iBand < psImageDesc->NumChannels; iBand++ )
        {
            poDS->SetBand( poDS->nBands+1, 
                           new SAR_CEOSRasterBand( poDS, poDS->nBands+1, 
                                                   eType ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Use raw services for well behaved files.                        */
/* -------------------------------------------------------------------- */
    else
    {
        int	StartData;
        int	nLineSize, nLineSize2;

        CalcCeosSARImageFilePosition( psVolume, 1, 1, NULL, &StartData );
        
        StartData += psImageDesc->ImageDataStart;

        CalcCeosSARImageFilePosition( psVolume, 1, 1, NULL, &nLineSize );
        CalcCeosSARImageFilePosition( psVolume, 1, 2, NULL, &nLineSize2 );

        nLineSize = nLineSize2 - nLineSize;
        
        for( int iBand = 0; iBand < psImageDesc->NumChannels; iBand++ )
        {
            int           nStartData, nPixelOffset, nLineOffset;

            if( psImageDesc->ChannelInterleaving == __CEOS_IL_PIXEL )
            {
                CalcCeosSARImageFilePosition(psVolume,1,1,NULL,&nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nStartData += psImageDesc->BytesPerPixel * iBand;

                nPixelOffset = 
                    psImageDesc->BytesPerPixel * psImageDesc->NumChannels;
                nLineOffset = nLineSize;
            }
            else if( psImageDesc->ChannelInterleaving == __CEOS_IL_LINE )
            {
                CalcCeosSARImageFilePosition(psVolume, iBand+1, 1, NULL,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nPixelOffset = psImageDesc->BytesPerPixel;
                nLineOffset = nLineSize * psImageDesc->NumChannels;
            }
            else if( psImageDesc->ChannelInterleaving == __CEOS_IL_BAND )
            {
                CalcCeosSARImageFilePosition(psVolume, iBand+1, 1, NULL,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nPixelOffset = psImageDesc->BytesPerPixel;
                nLineOffset = nLineSize;
            }
            else
            {
                CPLAssert( FALSE );
                return NULL;
            }

            
            poDS->SetBand( poDS->nBands+1, 
                    new RawRasterBand( 
                        poDS, poDS->nBands+1, poOpenInfo->fp, 
                        nStartData, nPixelOffset, nLineOffset, 
                        eType, bNative ) );
        }
        
    }

/* -------------------------------------------------------------------- */
/*      Adopt the file pointer.                                         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    poDS->ScanForMetadata();

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    poDS->ScanForGCPs();
    
/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                            ProcessData()                             */
/************************************************************************/
static int 
ProcessData( FILE *fp, int fileid, CeosSARVolume_t *sar, int max_records, 
             int max_bytes )

{
    unsigned char      temp_buffer[__CEOS_HEADER_LENGTH];
    unsigned char      *temp_body = NULL;
    int                start = 0;
    int                CurrentBodyLength = 0;
    int                CurrentType = 0;
    int                CurrentSequence = 0;
    Link_t             *TheLink;
    CeosRecord_t       *record;

    while(max_records != 0 && max_bytes != 0)
    {
	record = (CeosRecord_t *) CPLMalloc( sizeof( CeosRecord_t ) );
        VSIFSeek( fp, start, SEEK_SET );
        VSIFRead( temp_buffer, 1, __CEOS_HEADER_LENGTH, fp );
	record->Length = DetermineCeosRecordBodyLength( temp_buffer );

	if( record->Length > CurrentBodyLength )
	{
	    if(CurrentBodyLength == 0 )
		temp_body = (unsigned char *) CPLMalloc( record->Length );
	    else
	    {
		temp_body = (unsigned char *) 
                    CPLRealloc( temp_body, record->Length );
                CurrentBodyLength = record->Length;
            }
	}

        VSIFRead( temp_body, 1, record->Length - __CEOS_HEADER_LENGTH, fp );

	InitCeosRecordWithHeader( record, temp_buffer, temp_body );

	if( CurrentType == record->TypeCode.Int32Code )
	    record->Subsequence = ++CurrentSequence;
	else {
	    CurrentType = record->TypeCode.Int32Code;
	    record->Subsequence = CurrentSequence = 0;
	}

	record->FileId = fileid;

	TheLink = CreateLink( record );

	if( sar->RecordList == NULL )
	    sar->RecordList = TheLink;
	else
	    sar->RecordList = InsertLink( sar->RecordList, TheLink );

	start += record->Length;

	if(max_records > 0)
	    max_records--;
	if(max_bytes > 0)
        {
	    max_bytes -= record->Length;
            if(max_bytes < 0)
                max_bytes = 0;
        }
    }

    CPLFree(temp_body);

    return 0;
}

/************************************************************************/
/*                       GDALRegister_SAR_CEOS()                        */
/************************************************************************/

void GDALRegister_SAR_CEOS()

{
    GDALDriver	*poDriver;

    if( poCEOSDriver == NULL )
    {
        poCEOSDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "SAR_CEOS";
        poDriver->pszLongName = "CEOS SAR Image";
        poDriver->pszHelpTopic = "frmt_various.html#SAR_CEOS";
        
        poDriver->pfnOpen = SAR_CEOSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
