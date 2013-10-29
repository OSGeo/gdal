/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  GDALDataset driver for CEOS translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "ceos.h"
#include "gdal_priv.h"
#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_SAR_CEOS(void);
CPL_C_END

static GInt16 CastToGInt16(float val);

static GInt16 CastToGInt16(float val)
{
    float temp;

    temp = val;

    if ( temp < -32768.0 )
        temp = -32768.0;

    if ( temp > 32767 )
        temp = 32767.0;

    return (GInt16) temp;    
}

static const char *CeosExtension[][6] = { 
{ "vol", "led", "img", "trl", "nul", "ext" },
{ "vol", "lea", "img", "trl", "nul", "ext" },
{ "vol", "led", "img", "tra", "nul", "ext" },
{ "vol", "lea", "img", "tra", "nul", "ext" },
{ "vdf", "slf", "sdf", "stf", "nvd", "ext" },

{ "vdf", "ldr", "img", "tra", "nul", "ext2" },

/* Jers from Japan- not sure if this is generalized as much as it could be */
{ "VOLD", "Sarl_01", "Imop_%02d", "Sart_01", "NULL", "base" },


/* Radarsat: basename, not extension */
{ "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_vdf", "base" },

/* Ers-1: basename, not extension */
{ "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_dat", "base" },

/* Ers-2 from Telaviv */
{ "volume", "leader", "image", "trailer", "nul_dat", "whole" },

/* Ers-1 from D-PAF */
{ "VDF", "LF", "SLC", "", "", "ext" },

/* Radarsat-1 per #2051 */
{ "vol", "sarl", "sard", "sart", "nvol", "ext" },

/* end marker */
{ NULL, NULL, NULL, NULL, NULL, NULL } 
};

static int 
ProcessData( VSILFILE *fp, int fileid, CeosSARVolume_t *sar, int max_records,
             vsi_l_offset max_bytes );


static CeosTypeCode_t QuadToTC( int a, int b, int c, int d )
{
    CeosTypeCode_t   abcd;

    abcd.UCharCode.Subtype1 = (unsigned char) a;
    abcd.UCharCode.Type = (unsigned char) b;
    abcd.UCharCode.Subtype2 = (unsigned char) c;
    abcd.UCharCode.Subtype3 = (unsigned char) d;

    return abcd;
}

#define LEADER_DATASET_SUMMARY_TC          QuadToTC( 18, 10, 18, 20 )
#define LEADER_DATASET_SUMMARY_ERS2_TC     QuadToTC( 10, 10, 31, 20 )
#define LEADER_RADIOMETRIC_COMPENSATION_TC QuadToTC( 18, 51, 18, 20 )
#define VOLUME_DESCRIPTOR_RECORD_TC        QuadToTC( 192, 192, 18, 18 )
#define IMAGE_HEADER_RECORD_TC             QuadToTC( 63, 192, 18, 18 )
#define LEADER_RADIOMETRIC_DATA_RECORD_TC  QuadToTC( 18, 50, 18, 20 )
#define LEADER_MAP_PROJ_RECORD_TC          QuadToTC( 10, 20, 31, 20 )

/* JERS from Japan has MAP_PROJ recond with different identifiers */
/* see CEOS-SAR-CCT Iss/Rev: 2/0 February 10, 1989 */
#define LEADER_MAP_PROJ_RECORD_JERS_TC         QuadToTC( 18, 20, 18, 20 )

/* For ERS calibration and incidence angle information */
#define ERS_GENERAL_FACILITY_DATA_TC  QuadToTC( 10, 200, 31, 50 )
#define ERS_GENERAL_FACILITY_DATA_ALT_TC QuadToTC( 10, 216, 31, 50 )


#define RSAT_PROC_PARAM_TC QuadToTC( 18, 120, 18, 20 )

/************************************************************************/
/* ==================================================================== */
/*				SAR_CEOSDataset				*/
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand;
class CCPRasterBand;
class PALSARRasterBand;

class SAR_CEOSDataset : public GDALPamDataset
{
    friend class SAR_CEOSRasterBand;
    friend class CCPRasterBand;
    friend class PALSARRasterBand;

    CeosSARVolume_t sVolume;

    VSILFILE	*fpImage;

    char        **papszTempMD;
    
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ScanForGCPs();
    void        ScanForMetadata();
    int         ScanForMapProjection();

  public:
                SAR_CEOSDataset();
                ~SAR_CEOSDataset();

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char * pszDomain );

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                          CCPRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class CCPRasterBand : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

  public:
                   CCPRasterBand( SAR_CEOSDataset *, int, GDALDataType );

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/* ==================================================================== */
/*                        PALSARRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class PALSARRasterBand : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

  public:
                   PALSARRasterBand( SAR_CEOSDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/* ==================================================================== */
/*                       SAR_CEOSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

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
        
        VSIFSeekL( poGDS->fpImage, offset, SEEK_SET );
        VSIFReadL( pabyRecord + nPixelsRead * ImageDesc->BytesPerPixel, 
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
/*				CCPRasterBand				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           CCPRasterBand()                            */
/************************************************************************/

CCPRasterBand::CCPRasterBand( SAR_CEOSDataset *poGDS, int nBand,
                              GDALDataType eType )

{
    this->poDS = poGDS;
    this->nBand = nBand;

    eDataType = eType;

    nBlockXSize = poGDS->nRasterXSize;
    nBlockYSize = 1;

    if( nBand == 1 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HH" );
    else if( nBand == 2 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HV" );
    else if( nBand == 3 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VH" );
    else if( nBand == 4 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VV" );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

/* From: http://southport.jpl.nasa.gov/software/dcomp/dcomp.html

ysca = sqrt{ [ (Byte(2) / 254 ) + 1.5] 2Byte(1) }

Re(SHH) = byte(3) ysca/127

Im(SHH) = byte(4) ysca/127

Re(SHV) = byte(5) ysca/127

Im(SHV) = byte(6) ysca/127

Re(SVH) = byte(7) ysca/127

Im(SVH) = byte(8) ysca/127

Re(SVV) = byte(9) ysca/127

Im(SVV) = byte(10) ysca/127

*/

CPLErr CCPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    struct CeosSARImageDesc *ImageDesc;
    int	   offset;
    GByte  *pabyRecord;
    SAR_CEOSDataset *poGDS = (SAR_CEOSDataset *) poDS;
    static float afPowTable[256];
    static int bPowTableInitialized = FALSE;

    ImageDesc = &(poGDS->sVolume.ImageDesc);

    offset = ImageDesc->FileDescriptorLength
        + ImageDesc->BytesPerRecord * nBlockYOff 
        + ImageDesc->ImageDataStart;

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/* -------------------------------------------------------------------- */
    int	        nBytesToRead = ImageDesc->BytesPerPixel * nBlockXSize;

    pabyRecord = (GByte *) CPLMalloc( nBytesToRead );
    
    if( VSIFSeekL( poGDS->fpImage, offset, SEEK_SET ) != 0 
        || (int) VSIFReadL( pabyRecord, 1, nBytesToRead, 
                           poGDS->fpImage ) != nBytesToRead )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Error reading %d bytes of CEOS record data at offset %d.\n"
                  "Reading file %s failed.", 
                  nBytesToRead, offset, poGDS->GetDescription() );
        CPLFree( pabyRecord );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Initialize our power table if this is our first time through.   */
/* -------------------------------------------------------------------- */
    if( !bPowTableInitialized )
    {
        int i;

        bPowTableInitialized = TRUE;

        for( i = 0; i < 256; i++ )
        {
            afPowTable[i] = (float)pow( 2.0, i-128 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy the desired band out based on the size of the type, and    */
/*      the interleaving mode.                                          */
/* -------------------------------------------------------------------- */
    int iX;

    for( iX = 0; iX < nBlockXSize; iX++ )
    {
        unsigned char *pabyGroup = pabyRecord + iX * ImageDesc->BytesPerPixel;
        signed char *Byte = (signed char*)pabyGroup-1; /* A ones based alias */
        double dfReSHH, dfImSHH, dfReSHV, dfImSHV, 
            dfReSVH, dfImSVH, dfReSVV, dfImSVV, dfScale;

        dfScale = sqrt( (Byte[2] / 254 + 1.5) * afPowTable[Byte[1] + 128] );
        
        if( nBand == 1 )
        {
            dfReSHH = Byte[3] * dfScale / 127.0;
            dfImSHH = Byte[4] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = (float)dfReSHH;
            ((float *) pImage)[iX*2+1] = (float)dfImSHH;
        }        
        else if( nBand == 2 )
        {
            dfReSHV = Byte[5] * dfScale / 127.0;
            dfImSHV = Byte[6] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = (float)dfReSHV;
            ((float *) pImage)[iX*2+1] = (float)dfImSHV;
        }
        else if( nBand == 3 )
        {
            dfReSVH = Byte[7] * dfScale / 127.0;
            dfImSVH = Byte[8] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = (float)dfReSVH;
            ((float *) pImage)[iX*2+1] = (float)dfImSVH;
        }
        else if( nBand == 4 )
        {
            dfReSVV = Byte[9] * dfScale / 127.0;
            dfImSVV = Byte[10]* dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = (float)dfReSVV;
            ((float *) pImage)[iX*2+1] = (float)dfImSVV;
        }
    }

    CPLFree( pabyRecord );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*			      PALSARRasterBand				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           PALSARRasterBand()                         */
/************************************************************************/

PALSARRasterBand::PALSARRasterBand( SAR_CEOSDataset *poGDS, int nBand )

{
    this->poDS = poGDS;
    this->nBand = nBand;

    eDataType = GDT_CInt16;

    nBlockXSize = poGDS->nRasterXSize;
    nBlockYSize = 1;

    if( nBand == 1 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_11" );
    else if( nBand == 2 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_22" );
    else if( nBand == 3 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_33" );
    else if( nBand == 4 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_12" );
    else if( nBand == 5 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_13" );
    else if( nBand == 6 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_23" );
}

/************************************************************************/
/*                             IReadBlock()                             */
/*                                                                      */
/*      Based on ERSDAC-VX-CEOS-004                                     */
/************************************************************************/

CPLErr PALSARRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    struct CeosSARImageDesc *ImageDesc;
    int	   offset;
    GByte  *pabyRecord;
    SAR_CEOSDataset *poGDS = (SAR_CEOSDataset *) poDS;

    ImageDesc = &(poGDS->sVolume.ImageDesc);

    offset = ImageDesc->FileDescriptorLength
        + ImageDesc->BytesPerRecord * nBlockYOff 
        + ImageDesc->ImageDataStart;

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/* -------------------------------------------------------------------- */
    int	        nBytesToRead = ImageDesc->BytesPerPixel * nBlockXSize;

    pabyRecord = (GByte *) CPLMalloc( nBytesToRead );
    
    if( VSIFSeekL( poGDS->fpImage, offset, SEEK_SET ) != 0 
        || (int) VSIFReadL( pabyRecord, 1, nBytesToRead, 
                           poGDS->fpImage ) != nBytesToRead )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Error reading %d bytes of CEOS record data at offset %d.\n"
                  "Reading file %s failed.", 
                  nBytesToRead, offset, poGDS->GetDescription() );
        CPLFree( pabyRecord );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Copy the desired band out based on the size of the type, and    */
/*      the interleaving mode.                                          */
/* -------------------------------------------------------------------- */
    if( nBand == 1 || nBand == 2 || nBand == 3 )
    {
        // we need to pre-initialize things to set the imaginary component to 0
        memset( pImage, 0, nBlockXSize * 4 );

        GDALCopyWords( pabyRecord + 4*(nBand - 1), GDT_Int16, 18, 
                       pImage, GDT_Int16, 4, 
                       nBlockXSize );
#ifdef CPL_LSB
        GDALSwapWords( pImage, 2, nBlockXSize, 4 );
#endif        
    }
    else
    {
        GDALCopyWords( pabyRecord + 6 + 4*(nBand - 4), GDT_CInt16, 18, 
                       pImage, GDT_CInt16, 4, 
                       nBlockXSize );
#ifdef CPL_LSB
        GDALSwapWords( pImage, 2, nBlockXSize*2, 2 );
#endif        
    }
    CPLFree( pabyRecord );

/* -------------------------------------------------------------------- */
/*      Convert the values into covariance form as per:                 */
/* -------------------------------------------------------------------- */
/*
** 1) PALSAR- adjust so that it reads bands as a covariance matrix, and 
** set polarimetric interpretation accordingly:
**
** Covariance_11=HH*conj(HH): already there
** Covariance_22=2*HV*conj(HV): need a factor of 2
** Covariance_33=VV*conj(VV): already there 
** Covariance_12=sqrt(2)*HH*conj(HV): need the sqrt(2) factor
** Covariance_13=HH*conj(VV): already there
** Covariance_23=sqrt(2)*HV*conj(VV): need to take the conjugate, then 
**               multiply by sqrt(2)
**
*/

    if( nBand == 2 )
    {
        int i;
        GInt16 *panLine = (GInt16 *) pImage;
        
        for( i = 0; i < nBlockXSize * 2; i++ )
        {
          panLine[i] = (GInt16) CastToGInt16((float)2.0 * panLine[i]);
        }
    }
    else if( nBand == 4 )
    {
        int i;
        double sqrt_2 = pow(2.0,0.5);
        GInt16 *panLine = (GInt16 *) pImage;
        
        for( i = 0; i < nBlockXSize * 2; i++ )
        {
          panLine[i] = (GInt16) CastToGInt16((float)floor(panLine[i] * sqrt_2 + 0.5));
        }
    }
    else if( nBand == 6 )
    {
        int i;
        GInt16 *panLine = (GInt16 *) pImage;
        double sqrt_2 = pow(2.0,0.5);
        
        // real portion - just multiple by sqrt(2)
        for( i = 0; i < nBlockXSize * 2; i += 2 )
        {
          panLine[i] = (GInt16) CastToGInt16((float)floor(panLine[i] * sqrt_2 + 0.5));
        }

        // imaginary portion - conjugate and multiply
        for( i = 1; i < nBlockXSize * 2; i += 2 )
        {
          panLine[i] = (GInt16) CastToGInt16((float)floor(-panLine[i] * sqrt_2 + 0.5));
        }
    }

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

    papszTempMD = NULL;
}

/************************************************************************/
/*                          ~SAR_CEOSDataset()                          */
/************************************************************************/

SAR_CEOSDataset::~SAR_CEOSDataset()

{
    FlushCache();

    CSLDestroy( papszTempMD );

    if( fpImage != NULL )
        VSIFCloseL( fpImage );

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
    FreeRecipes();
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
        return SRS_WKT_WGS84;
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
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **SAR_CEOSDataset::GetMetadataDomainList()
{
    return CSLAddString(GDALDataset::GetMetadataDomainList(), "ceos-FFF-n-n-n-n:r");
}

/************************************************************************/
/*                            GetMetadata()                             */
/*                                                                      */
/*      We provide our own GetMetadata() so that we can override        */
/*      behavior for some very specialized domain names intended to     */
/*      give us access to raw record data.                              */
/*                                                                      */
/*      The domain must look like:                                      */
/*        ceos-FFF-n-n-n-n:r                                            */
/*                                                                      */
/*        FFF - The file id - one of vol, lea, img, trl or nul.         */
/*        n-n-n-n - the record type code such as 18-10-18-20 for the    */
/*        dataset summary record in the leader file.                    */
/*        :r - The zero based record number to fetch (optional)         */
/*                                                                      */
/*      Note that only records that are pre-loaded will be              */
/*      accessable, and this normally means that most image records     */
/*      are not available.                                              */
/************************************************************************/

char **SAR_CEOSDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain == NULL || !EQUALN(pszDomain,"ceos-",5) )
        return GDALDataset::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      Identify which file to fetch the file from.                     */
/* -------------------------------------------------------------------- */
    int	nFileId = -1;
    
    if( EQUALN(pszDomain,"ceos-vol",8) )
    {
        nFileId = __CEOS_VOLUME_DIR_FILE;
    }
    else if( EQUALN(pszDomain,"ceos-lea",8) )
    {
        nFileId = __CEOS_LEADER_FILE;
    }
    else if( EQUALN(pszDomain,"ceos-img",8) )
    {
        nFileId = __CEOS_IMAGRY_OPT_FILE;
    }
    else if( EQUALN(pszDomain,"ceos-trl",8) )
    {
        nFileId = __CEOS_TRAILER_FILE;
    }
    else if( EQUALN(pszDomain,"ceos-nul",8) )
    {
        nFileId = __CEOS_NULL_VOL_FILE;
    }
    else
        return NULL;

    pszDomain += 8;

/* -------------------------------------------------------------------- */
/*      Identify the record type.                                       */
/* -------------------------------------------------------------------- */
    CeosTypeCode_t sTypeCode;
    int  a, b, c, d, nRecordIndex = -1;

    if( sscanf( pszDomain, "-%d-%d-%d-%d:%d", 
                &a, &b, &c, &d, &nRecordIndex ) != 5 
        && sscanf( pszDomain, "-%d-%d-%d-%d", 
                   &a, &b, &c, &d ) != 4 )
    {
        return NULL;
    }

    sTypeCode = QuadToTC( a, b, c, d );

/* -------------------------------------------------------------------- */
/*      Try to fetch the record.                                        */
/* -------------------------------------------------------------------- */
    CeosRecord_t *record;

    record = FindCeosRecord( sVolume.RecordList, sTypeCode, nFileId, 
                             -1, nRecordIndex );

    if( record == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Massage the data into a safe textual format.  The RawRecord     */
/*      just has zero bytes turned into spaces while the                */
/*      EscapedRecord has regular backslash escaping applied to zero    */
/*      chars, double quotes, and backslashes.                          */
/*      just turn zero bytes into spaces.                               */
/* -------------------------------------------------------------------- */
    char *pszSafeCopy;
    int  i;

    CSLDestroy( papszTempMD );

    // Escaped version
    pszSafeCopy = CPLEscapeString( (char *) record->Buffer, 
                                   record->Length, 
                                   CPLES_BackslashQuotable );
    papszTempMD = CSLSetNameValue( NULL, "EscapedRecord", pszSafeCopy );
    CPLFree( pszSafeCopy );


    // Copy with '\0' replaced by spaces.

    pszSafeCopy = (char *) CPLCalloc(1,record->Length+1);
    memcpy( pszSafeCopy, record->Buffer, record->Length );
    
    for( i = 0; i < record->Length; i++ )
        if( pszSafeCopy[i] == '\0' )
            pszSafeCopy[i] = ' ';
        
    papszTempMD = CSLSetNameValue( papszTempMD, "RawRecord", pszSafeCopy );

    CPLFree( pszSafeCopy );

    return papszTempMD;
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

/* -------------------------------------------------------------------- */
/*      product identifier.                                                    */
/* -------------------------------------------------------------------- */
        szField[8] = '\0';

        GetCeosField( record, 261, "A8", szField );

        if( !EQUALN(szField,"        ",8) )
            SetMetadataItem( "CEOS_PRODUCT_ID", szField );
    
/* -------------------------------------------------------------------- */
/*      volume identifier.                                                    */
/* -------------------------------------------------------------------- */
        szField[16] = '\0';

        GetCeosField( record, 77, "A16", szField );

        if( !EQUALN(szField,"                ",16) )
            SetMetadataItem( "CEOS_VOLSET_ID", szField );
    }

/* ==================================================================== */
/*      Dataset summary record.                                         */
/* ==================================================================== */
    record = FindCeosRecord( sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                                 __CEOS_TRAILER_FILE, -1, -1 );

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, 
                                 LEADER_DATASET_SUMMARY_ERS2_TC,
                                 __CEOS_LEADER_FILE, -1, -1 );

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
/*      Ascending/Descending                                            */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 101, "A16", szField );
        szField[16] = '\0';

        if( strstr(szVolId,"RSAT") != NULL 
            && !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_ASC_DES", szField );

/* -------------------------------------------------------------------- */
/*      True heading - at least for ERS2.                               */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 149, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_TRUE_HEADING", szField );

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

/* -------------------------------------------------------------------- */
/*      SCENE LENGTH KM                                                 */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 341, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_SCENE_LENGTH_KM", szField );

/* -------------------------------------------------------------------- */
/*      SCENE WIDTH KM                                                  */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 357, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_SCENE_WIDTH_KM", szField );

/* -------------------------------------------------------------------- */
/*      MISSION ID                                                      */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 397, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_MISSION_ID", szField );

/* -------------------------------------------------------------------- */
/*      SENSOR ID                                                      */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 413, "A32", szField );
        szField[32] = '\0';

        if( !EQUALN(szField,"                                ",32 ) )
            SetMetadataItem( "CEOS_SENSOR_ID", szField );


/* -------------------------------------------------------------------- */
/*      ORBIT NUMBER                                                    */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 445, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_ORBIT_NUMBER", szField );


/* -------------------------------------------------------------------- */
/*      Platform latitude                                               */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 453, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_PLATFORM_LATITUDE", szField );

/* -------------------------------------------------------------------- */
/*      Platform longitude                                               */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 461, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_PLATFORM_LONGITUDE", szField );

/* -------------------------------------------------------------------- */
/*      Platform heading - at least for ERS2.                           */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 469, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"                ",8 ) )
            SetMetadataItem( "CEOS_PLATFORM_HEADING", szField );

/* -------------------------------------------------------------------- */
/*      Look Angle.                                                     */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 477, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_SENSOR_CLOCK_ANGLE", szField );

/* -------------------------------------------------------------------- */
/*      Incidence angle                                                 */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 485, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"        ",8 ) )
            SetMetadataItem( "CEOS_INC_ANGLE", szField );

/* -------------------------------------------------------------------- */
/*      Pixel time direction indicator                                  */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 1527, "A8", szField );
        szField[8] = '\0';

        if( !EQUALN(szField,"                ",8 ) )
            SetMetadataItem( "CEOS_PIXEL_TIME_DIR", szField );

/* -------------------------------------------------------------------- */
/*      Line spacing                                                    */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 1687, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_LINE_SPACING_METERS", szField );
/* -------------------------------------------------------------------- */
/*      Pixel spacing                                                    */
/* -------------------------------------------------------------------- */
        GetCeosField( record, 1703, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_PIXEL_SPACING_METERS", szField );

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

        SetMetadataItem( "CEOS_BEAM_TYPE", szField );
    }

/* ==================================================================== */
/*      ERS calibration and incidence angle info                        */
/* ==================================================================== */
    record = FindCeosRecord( sVolume.RecordList, ERS_GENERAL_FACILITY_DATA_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, 
                                 ERS_GENERAL_FACILITY_DATA_ALT_TC,
                                 __CEOS_LEADER_FILE, -1, -1 );

    if( record != NULL )
    {   
        GetCeosField( record, 13 , "A64", szField );
        szField[64] = '\0';

        /* Avoid PCS records, which don't contain necessary info */
        if( strstr( szField, "GENERAL") == NULL )
            record = NULL;
    }

    if( record != NULL )
    {
        GetCeosField( record, 583 , "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_INC_ANGLE_FIRST_RANGE", szField );

        GetCeosField( record, 599 , "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_INC_ANGLE_CENTRE_RANGE", szField );

        GetCeosField( record, 615, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_INC_ANGLE_LAST_RANGE", szField );

        GetCeosField( record, 663, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_CALIBRATION_CONSTANT_K", szField );

        GetCeosField( record, 1855, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C0", szField );

        GetCeosField( record, 1875, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C1", szField );

        GetCeosField( record, 1895, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C2", szField );

        GetCeosField( record, 1915, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C3", szField );

    }
/* -------------------------------------------------------------------- */
/*	Detailed Processing Parameters (Radarsat)                       */
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, RSAT_PROC_PARAM_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, RSAT_PROC_PARAM_TC,
                             __CEOS_TRAILER_FILE, -1, -1 );

    if( record != NULL )
    {
        GetCeosField( record, 192, "A21", szField );
        szField[21] = '\0';

        if( !EQUALN(szField,"                     ",21 ) )
            SetMetadataItem( "CEOS_PROC_START", szField );
            
        GetCeosField( record, 213, "A21", szField );
        szField[21] = '\0';

        if( !EQUALN(szField,"                     ",21 ) )
            SetMetadataItem( "CEOS_PROC_STOP", szField );
            
        GetCeosField( record, 4649, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_0", szField );

        GetCeosField( record, 4665, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_1", szField );

        GetCeosField( record, 4681, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_2", szField );

        GetCeosField( record, 4697, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_3", szField );

        GetCeosField( record, 4713, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_4", szField );

        GetCeosField( record, 4729, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_5", szField );

        GetCeosField( record, 4745, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_EPH_ORB_DATA_6", szField );

        GetCeosField( record, 4908, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C0", szField );

        GetCeosField( record, 4924, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C1", szField );

        GetCeosField( record, 4940, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C2", szField );

        GetCeosField( record, 4956, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C3", szField );

        GetCeosField( record, 4972, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C4", szField );

        GetCeosField( record, 4988, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_GROUND_TO_SLANT_C5", szField );

        GetCeosField( record, 7334, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_INC_ANGLE_FIRST_RANGE", szField );

        GetCeosField( record, 7350, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ",16 ) )
            SetMetadataItem( "CEOS_INC_ANGLE_LAST_RANGE", szField );

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

    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, 
                             LEADER_RADIOMETRIC_DATA_RECORD_TC,
                             __CEOS_TRAILER_FILE, -1, -1 );

    if( record != NULL )
    {
        GetCeosField( record, 8317, "A16", szField );
        szField[16] = '\0';

        if( !EQUALN(szField,"                ", 16 ) )
            SetMetadataItem( "CEOS_CALIBRATION_OFFSET", szField );
    }

/* -------------------------------------------------------------------- */
/*      For ERS Standard Format Landsat scenes we pick up the           */
/*      calibration offset and gain from the Radiometric Ancillary      */
/*      Record.                                                         */
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, 
                             QuadToTC( 0x3f, 0x24, 0x12, 0x09 ),
                             __CEOS_LEADER_FILE, -1, -1 );
    if( record != NULL )
    {
        GetCeosField( record, 29, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_OFFSET_A0", szField );

        GetCeosField( record, 49, "A20", szField );
        szField[20] = '\0';

        if( !EQUALN(szField,"                    ", 20 ) )
            SetMetadataItem( "CEOS_GAIN_A1", szField );
    }

/* -------------------------------------------------------------------- */
/*      For ERS Standard Format Landsat scenes we pick up the           */
/*      gain setting from the Scene Header Record.			*/
/* -------------------------------------------------------------------- */
    record = FindCeosRecord( sVolume.RecordList, 
                             QuadToTC( 0x12, 0x12, 0x12, 0x09 ),
                             __CEOS_LEADER_FILE, -1, -1 );
    if( record != NULL )
    {
        GetCeosField( record, 1486, "A1", szField );
        szField[1] = '\0';

        if( szField[0] == 'H' || szField[0] == 'V' )
            SetMetadataItem( "CEOS_GAIN_SETTING", szField );
    }
}

/************************************************************************/
/*                        ScanForMapProjection()                        */
/*                                                                      */
/*      Try to find a map projection record, and read corner points     */
/*      from it.  This has only been tested with ERS products.          */
/************************************************************************/

int SAR_CEOSDataset::ScanForMapProjection()

{
    CeosRecord_t *record;
    char	 szField[100];
    int          i;

/* -------------------------------------------------------------------- */
/*      Find record, and try to determine if it has useful GCPs.        */
/* -------------------------------------------------------------------- */

    record = FindCeosRecord( sVolume.RecordList, 
                             LEADER_MAP_PROJ_RECORD_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    /* JERS from Japan */
    if( record == NULL )
        record = FindCeosRecord( sVolume.RecordList, 
                             LEADER_MAP_PROJ_RECORD_JERS_TC,
                             __CEOS_LEADER_FILE, -1, -1 );

    if( record == NULL )
        return FALSE;

    memset( szField, 0, 17 );
    GetCeosField( record, 29, "A16", szField );

    if( !EQUALN(szField,"Slant Range",11) && !EQUALN(szField,"Ground Range",12) 
        && !EQUALN(szField,"GEOCODED",8) )
        return FALSE;

    GetCeosField( record, 1073, "A16", szField );
    if( EQUALN(szField,"        ",8) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Read corner points.                                             */
/* -------------------------------------------------------------------- */
    nGCPCount = 4;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);

    GDALInitGCPs( nGCPCount, pasGCPList );

    for( i = 0; i < nGCPCount; i++ )
    {
        char         szId[32];

        sprintf( szId, "%d", i+1 );
        pasGCPList[i].pszId = CPLStrdup( szId );
    
        GetCeosField( record, 1073+32*i, "A16", szField );
        pasGCPList[i].dfGCPY = atof(szField);
        GetCeosField( record, 1089+32*i, "A16", szField );
        pasGCPList[i].dfGCPX = atof(szField);
        pasGCPList[i].dfGCPZ = 0.0;
    }
    
    pasGCPList[0].dfGCPLine = 0.5;
    pasGCPList[0].dfGCPPixel = 0.5;

    pasGCPList[1].dfGCPLine = 0.5;
    pasGCPList[1].dfGCPPixel = nRasterXSize-0.5;

    pasGCPList[2].dfGCPLine = nRasterYSize-0.5;
    pasGCPList[2].dfGCPPixel = nRasterXSize-0.5;

    pasGCPList[3].dfGCPLine = nRasterYSize-0.5;
    pasGCPList[3].dfGCPPixel = 0.5;

    return TRUE;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void SAR_CEOSDataset::ScanForGCPs()

{
    int    iScanline, nStep, nGCPMax = 15;

/* -------------------------------------------------------------------- */
/*      Do we have a standard 180 bytes of prefix data (192 bytes       */
/*      including the record marker information)?  If not, it is        */
/*      unlikely that the GCPs are available.                           */
/* -------------------------------------------------------------------- */
    if( sVolume.ImageDesc.ImageDataStart < 192 )
    {
        ScanForMapProjection();
        return;
    }

/* -------------------------------------------------------------------- */
/*      Just sample fix scanlines through the image for GCPs, to        */
/*      return 15 GCPs.  That is an adequate coverage for most          */
/*      purposes.  A GCP is collected from the beginning, middle and    */
/*      end of each scanline.                                           */
/* -------------------------------------------------------------------- */
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

        if( VSIFSeekL( fpImage, nFileOffset, SEEK_SET ) != 0 
            || VSIFReadL( anRecord, 1, 192, fpImage ) != 192 )
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
    /* If general GCP's weren't found, look for Map Projection (eg. JERS) */
    if( nGCPCount == 0 )
    {
        ScanForMapProjection();
        return;
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
    if( poOpenInfo->nHeaderBytes < __CEOS_HEADER_LENGTH )
        return NULL;

    if( (poOpenInfo->pabyHeader[4] != 0x3f
         && poOpenInfo->pabyHeader[4] != 0x32)
        || poOpenInfo->pabyHeader[5] != 0xc0
        || poOpenInfo->pabyHeader[6] != 0x12
        || poOpenInfo->pabyHeader[7] != 0x12 )
        return NULL;

    // some products (#1862) have byte swapped record length/number
    // values and will blow stuff up -- explicitly ignore if record index
    // value appears to be little endian.
    if( poOpenInfo->pabyHeader[0] != 0 )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The SAR_CEOS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SAR_CEOSDataset 	*poDS;
    CeosSARVolume_t     *psVolume;

    poDS = new SAR_CEOSDataset();

    psVolume = &(poDS->sVolume);
    InitCeosSARVolume( psVolume, 0 );

/* -------------------------------------------------------------------- */
/*      Try to read the current file as an imagery file.                */
/* -------------------------------------------------------------------- */
    
    psVolume->ImagryOptionsFile = TRUE;
    if( ProcessData( fp, __CEOS_IMAGRY_OPT_FILE, psVolume, 4, -1) != CE_None )
    {
        delete poDS;
        VSIFCloseL(fp);
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
            VSILFILE	*process_fp;
            char *pszFilename = NULL;
            
            /* build filename */
            if( EQUAL(CeosExtension[e][5],"base") )
            {
                char    szMadeBasename[32];

                sprintf( szMadeBasename, CeosExtension[e][iFile], nBand );
                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,szMadeBasename, pszExtension));
            }
            else if( EQUAL(CeosExtension[e][5],"ext") )
            {
                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,pszBasename,
                                    CeosExtension[e][iFile]));
            }
            else if( EQUAL(CeosExtension[e][5],"whole") )
            {
                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,CeosExtension[e][iFile],""));
            }
            
            // This is for SAR SLC as per the SAR Toolbox (from ASF).
            else if( EQUAL(CeosExtension[e][5],"ext2") )
            {
                char szThisExtension[32];

                if( strlen(pszExtension) > 3 )
                    sprintf( szThisExtension, "%s%s", 
                             CeosExtension[e][iFile], 
                             pszExtension+3 );
                else
                    sprintf( szThisExtension, "%s", 
                             CeosExtension[e][iFile] );

                pszFilename = CPLStrdup(
                    CPLFormFilename(pszPath,pszBasename,szThisExtension));
            }

            CPLAssert( pszFilename != NULL );
            if( pszFilename == NULL ) 
                return NULL;
 
            /* try to open */
            process_fp = VSIFOpenL( pszFilename, "rb" );

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

                process_fp = VSIFOpenL( pszFilename, "rb" );
            }

            if( process_fp != NULL )
            {
                CPLDebug( "CEOS", "Opened %s.\n", pszFilename );

                VSIFSeekL( process_fp, 0, SEEK_END );
                if( ProcessData( process_fp, iFile, psVolume, -1, 
                                 VSIFTellL( process_fp ) ) == 0 )
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

                    VSIFCloseL( process_fp );
                    CPLFree( pszFilename );
                    break; /* Exit the while loop, we have this data type*/
                }
                    
                VSIFCloseL( process_fp );
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

        VSIFCloseL(fp);

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
      case __CEOS_TYP_PALSAR_COMPLEX_SHORT:
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
      case __CEOS_TYP_CCP_COMPLEX_FLOAT:
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
/*      Special case for compressed cross products.                     */
/* -------------------------------------------------------------------- */
    if( psImageDesc->DataType == __CEOS_TYP_CCP_COMPLEX_FLOAT )
    {
        for( int iBand = 0; iBand < psImageDesc->NumChannels; iBand++ )
        {
            poDS->SetBand( poDS->nBands+1, 
                           new CCPRasterBand( poDS, poDS->nBands+1, eType ) );
        }

        /* mark this as a Scattering Matrix product */
        if ( poDS->GetRasterCount() == 4 ) {
            poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for PALSAR data.                                   */
/* -------------------------------------------------------------------- */
    else if( psImageDesc->DataType == __CEOS_TYP_PALSAR_COMPLEX_SHORT )
    {
        for( int iBand = 0; iBand < psImageDesc->NumChannels; iBand++ )
        {
            poDS->SetBand( poDS->nBands+1, 
                           new PALSARRasterBand( poDS, poDS->nBands+1 ) );
        }

        /* mark this as a Symmetrized Covariance product if appropriate */
        if ( poDS->GetRasterCount() == 6 ) {
            poDS->SetMetadataItem( "MATRIX_REPRESENTATION", 
                "SYMMETRIZED_COVARIANCE" );
        } 
    }

/* -------------------------------------------------------------------- */
/*      Roll our own ...                                                */
/* -------------------------------------------------------------------- */
    else if( psImageDesc->RecordsPerLine > 1
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
                        poDS, poDS->nBands+1, fp, 
                        nStartData, nPixelOffset, nLineOffset, 
                        eType, bNative, TRUE ) );
        }
        
    }

/* -------------------------------------------------------------------- */
/*      Adopt the file pointer.                                         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = fp;

/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    poDS->ScanForMetadata();

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    poDS->ScanForGCPs();
    
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

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
ProcessData( VSILFILE *fp, int fileid, CeosSARVolume_t *sar, int max_records,
             vsi_l_offset max_bytes )

{
    unsigned char      temp_buffer[__CEOS_HEADER_LENGTH];
    unsigned char      *temp_body = NULL;
    int                start = 0;
    int                CurrentBodyLength = 0;
    int                CurrentType = 0;
    int                CurrentSequence = 0;
    Link_t             *TheLink;
    CeosRecord_t       *record;
    int                iThisRecord = 0;

    while(max_records != 0 && max_bytes != 0)
    {
        record = (CeosRecord_t *) CPLMalloc( sizeof( CeosRecord_t ) );
        VSIFSeekL( fp, start, SEEK_SET );
        VSIFReadL( temp_buffer, 1, __CEOS_HEADER_LENGTH, fp );
        record->Length = DetermineCeosRecordBodyLength( temp_buffer );

        iThisRecord++;
        CeosToNative( &(record->Sequence), temp_buffer, 4, 4 );

        if( iThisRecord != record->Sequence )
        {
            if( fileid == __CEOS_IMAGRY_OPT_FILE && iThisRecord == 2 )
            {
                CPLDebug( "SAR_CEOS", "Ignoring CEOS file with wrong second record sequence number - likely it has padded records." );
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Warning;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Corrupt CEOS File - got record seq# %d instead of the expected %d.",
                          record->Sequence, iThisRecord );
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Failure;
            }
        }
        
        if( record->Length > CurrentBodyLength )
        {
            if(CurrentBodyLength == 0 )
            {
                temp_body = (unsigned char *) CPLMalloc( record->Length );
                CurrentBodyLength = record->Length;
            }
            else
            {
                temp_body = (unsigned char *) 
                    CPLRealloc( temp_body, record->Length );
                CurrentBodyLength = record->Length;
            }
        }

        VSIFReadL( temp_body, 1, MAX(0,record->Length-__CEOS_HEADER_LENGTH),fp);

        InitCeosRecordWithHeader( record, temp_buffer, temp_body );

        if( CurrentType == record->TypeCode.Int32Code )
            record->Subsequence = ++CurrentSequence;
        else {
            CurrentType = record->TypeCode.Int32Code;
            record->Subsequence = CurrentSequence = 0;
        }

        record->FileId = fileid;

        TheLink = ceos2CreateLink( record );

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

    return CE_None;
}

/************************************************************************/
/*                       GDALRegister_SAR_CEOS()                        */
/************************************************************************/

void GDALRegister_SAR_CEOS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "SAR_CEOS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SAR_CEOS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "CEOS SAR Image" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SAR_CEOS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = SAR_CEOSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

