/******************************************************************************
 *
 * Project:  ASI CEOS Translator
 * Purpose:  GDALDataset driver for CEOS translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc.
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ceos.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "rawdataset.h"
#include "ogr_srs_api.h"
#include "cpl_vsi_virtual.h"

#include <algorithm>
#include <cinttypes>

static GInt16 CastToGInt16(float val)
{
    if (val < -32768.0)
        val = -32768.0;

    if (val > 32767)
        val = 32767.0;

    return static_cast<GInt16>(val);
}

// CeosExtension[][i] ordered by i:
// CEOS_VOLUME_DIR_FILE = 0
// CEOS_LEADER_FILE     = 1
// CEOS_IMAGRY_OPT_FILE = 2
// CEOS_TRAILER_FILE    = 3
// CEOS_NULL_VOL_FILE   = 4
// and last item (idx 5) is the method to derive auxiliary filenames
static const char *const CeosExtension[][CEOS_FILE_COUNT + 1] = {
    {"vol", "led", "img", "trl", "nul", "ext"},
    {"vol", "lea", "img", "trl", "nul", "ext"},
    {"vol", "led", "img", "tra", "nul", "ext"},
    {"vol", "lea", "img", "tra", "nul", "ext"},
    {"vdf", "slf", "sdf", "stf", "nvd", "ext"},

    {"vdf", "ldr", "img", "tra", "nul", "ext2"},

    /* Jers from Japan- not sure if this is generalized as much as it could be
     */
    {"VOLD", "Sarl_01", "Imop_%02d", "Sart_01", "NULL", "base"},

    /* Radarsat: basename, not extension */
    {"vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_vdf", "base"},

    /* Ers-1: basename, not extension */
    {"vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_dat", "base"},

    /* Ers-2 from Telaviv */
    {"volume", "leader", "image", "trailer", "nul_dat", "whole"},

    /* Ers-1 from D-PAF */
    {"VDF", "LF", "SLC", "", "", "ext"},

    /* Radarsat-1 per #2051 */
    {"vol", "sarl", "sard", "sart", "nvol", "ext"},

    /* Radarsat-1 ASF */
    {"", "L", "D", "", "", "ext"},

    /* PALSAR-2 ALOS2 / PALSAR-3 ALOS4 */
    {"VOL", "LED", "", "TRL", "", "ALOS2-ALOS4"},

    /* end marker */
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}};

static int ProcessData(VSILFILE *fp, int fileid, CeosSARVolume_t *sar,
                       int max_records, vsi_l_offset max_bytes,
                       bool bSilentWrongRecordNumber);

static CeosTypeCode_t QuadToTC(int a, int b, int c, int d)
{
    CeosTypeCode_t abcd;

    abcd.UCharCode.Subtype1 = (unsigned char)a;
    abcd.UCharCode.Type = (unsigned char)b;
    abcd.UCharCode.Subtype2 = (unsigned char)c;
    abcd.UCharCode.Subtype3 = (unsigned char)d;

    return abcd;
}

#define LEADER_DATASET_SUMMARY_TC QuadToTC(18, 10, 18, 20)
#define LEADER_DATASET_SUMMARY_ERS2_TC QuadToTC(10, 10, 31, 20)
#define LEADER_RADIOMETRIC_COMPENSATION_TC QuadToTC(18, 51, 18, 20)
#define VOLUME_DESCRIPTOR_RECORD_TC QuadToTC(192, 192, 18, 18)
#define IMAGE_HEADER_RECORD_TC QuadToTC(63, 192, 18, 18)
#define LEADER_RADIOMETRIC_DATA_RECORD_TC QuadToTC(18, 50, 18, 20)
#define LEADER_MAP_PROJ_RECORD_TC QuadToTC(10, 20, 31, 20)

/* JERS from Japan has MAP_PROJ record with different identifiers */
/* see CEOS-SAR-CCT Iss/Rev: 2/0 February 10, 1989 */
#define LEADER_MAP_PROJ_RECORD_JERS_TC QuadToTC(18, 20, 18, 20)

/* Leader from ASF has different identifiers */
#define LEADER_MAP_PROJ_RECORD_ASF_TC QuadToTC(10, 20, 18, 20)
#define LEADER_DATASET_SUMMARY_ASF_TC QuadToTC(10, 10, 18, 20)
#define LEADER_FACILITY_ASF_TC QuadToTC(90, 210, 18, 61)

/* For ERS calibration and incidence angle information */
#define ERS_GENERAL_FACILITY_DATA_TC QuadToTC(10, 200, 31, 50)
#define ERS_GENERAL_FACILITY_DATA_ALT_TC QuadToTC(10, 216, 31, 50)

#define RSAT_PROC_PARAM_TC QuadToTC(18, 120, 18, 20)

/* PALSAR-2 ALOS2 */
// https://www.eorc.jaxa.jp/ALOS/en/alos-2/pdf/product_format_description/PALSAR-2_xx_Format_CEOS_E_g.pdf
// Table 3.2-3 Record Type of Each Record
#define LEADER_PLATFORM_POSITION_PALSAR_TC QuadToTC(18, 30, 18, 20)
#define LEADER_ATTITUDE_PALSAR_TC QuadToTC(18, 40, 18, 20)
#define LEADER_RADIOMETRIC_PALSAR_TC QuadToTC(18, 50, 18, 20)
#define LEADER_FACILITY_RELATED_PALSAR_TC QuadToTC(18, 200, 18, 70)
#define IMAGE_PROCESSED_DATA_RECORD_PALSAR_TC QuadToTC(50, 11, 18, 20)

/************************************************************************/
/* ==================================================================== */
/*                              SAR_CEOSDataset                         */
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand;
class CCPRasterBand;
class PALSARRasterBand;

class SAR_CEOSDataset final : public GDALPamDataset
{
    friend class SAR_CEOSRasterBand;
    friend class CCPRasterBand;
    friend class PALSARRasterBand;

    CeosSARVolume_t sVolume;

    VSILFILE *fpImage;

    char **papszTempMD;

    bool m_bHasScannedForGCP = false;
    OGRSpatialReference m_oSRS{};
    int nGCPCount;
    GDAL_GCP *pasGCPList;

    void ScanForGCPs();
    void ScanForMetadata();
    int ScanForMapProjection();
    char **papszExtraFiles;

  public:
    SAR_CEOSDataset();
    ~SAR_CEOSDataset() override;

    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;

    char **GetMetadataDomainList() override;
    CSLConstList GetMetadata(const char *pszDomain) override;

    static GDALDataset *Open(GDALOpenInfo *);
    char **GetFileList(void) override;
};

/************************************************************************/
/* ==================================================================== */
/*                          CCPRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class CCPRasterBand final : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

  public:
    CCPRasterBand(SAR_CEOSDataset *, int, GDALDataType);

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        PALSARRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class PALSARRasterBand final : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

  public:
    PALSARRasterBand(SAR_CEOSDataset *, int);

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/* ==================================================================== */
/*                       SAR_CEOSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SAR_CEOSRasterBand final : public GDALPamRasterBand
{
    friend class SAR_CEOSDataset;

  public:
    SAR_CEOSRasterBand(SAR_CEOSDataset *, int, GDALDataType);

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                         SAR_CEOSRasterBand()                         */
/************************************************************************/

SAR_CEOSRasterBand::SAR_CEOSRasterBand(SAR_CEOSDataset *poGDSIn, int nBandIn,
                                       GDALDataType eType)

{
    poDS = poGDSIn;
    nBand = nBandIn;

    eDataType = eType;

    nBlockXSize = poGDSIn->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAR_CEOSRasterBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                      void *pImage)
{
    SAR_CEOSDataset *poGDS = cpl::down_cast<SAR_CEOSDataset *>(poDS);

    struct CeosSARImageDesc *ImageDesc = &(poGDS->sVolume.ImageDesc);

    uint64_t offset = 0;
    CalcCeosSARImageFilePosition(&(poGDS->sVolume), nBand, nBlockYOff + 1,
                                 nullptr, &offset);

    offset += ImageDesc->ImageDataStart;

    /* -------------------------------------------------------------------- */
    /*      Load all the pixel data associated with this scanline.          */
    /*      Ensure we handle multiple record scanlines properly.            */
    /* -------------------------------------------------------------------- */
    int nPixelsRead = 0;

    GByte *pabyRecord =
        (GByte *)VSI_MALLOC2_VERBOSE(ImageDesc->BytesPerPixel, nBlockXSize);
    if (!pabyRecord)
        return CE_Failure;

    for (int iRecord = 0; iRecord < ImageDesc->RecordsPerLine; iRecord++)
    {
        int nPixelsToRead;

        if (nPixelsRead + ImageDesc->PixelsPerRecord > nBlockXSize)
            nPixelsToRead = nBlockXSize - nPixelsRead;
        else
            nPixelsToRead = ImageDesc->PixelsPerRecord;

        CPL_IGNORE_RET_VAL(VSIFSeekL(poGDS->fpImage, offset, SEEK_SET));
        CPL_IGNORE_RET_VAL(VSIFReadL(
            pabyRecord +
                static_cast<size_t>(nPixelsRead) * ImageDesc->BytesPerPixel,
            1, static_cast<size_t>(nPixelsToRead) * ImageDesc->BytesPerPixel,
            poGDS->fpImage));

        nPixelsRead += nPixelsToRead;
        offset += ImageDesc->BytesPerRecord;
    }

    /* -------------------------------------------------------------------- */
    /*      Copy the desired band out based on the size of the type, and    */
    /*      the interleaving mode.                                          */
    /* -------------------------------------------------------------------- */
    const int nBytesPerSample = GDALGetDataTypeSizeBytes(eDataType);

    if (ImageDesc->ChannelInterleaving == CEOS_IL_PIXEL)
    {
        GDALCopyWords(pabyRecord + (nBand - 1) * nBytesPerSample, eDataType,
                      ImageDesc->BytesPerPixel, pImage, eDataType,
                      nBytesPerSample, nBlockXSize);
    }
    else if (ImageDesc->ChannelInterleaving == CEOS_IL_LINE)
    {
        GDALCopyWords(pabyRecord + (nBand - 1) * nBytesPerSample * nBlockXSize,
                      eDataType, nBytesPerSample, pImage, eDataType,
                      nBytesPerSample, nBlockXSize);
    }
    else if (ImageDesc->ChannelInterleaving == CEOS_IL_BAND)
    {
        memcpy(pImage, pabyRecord,
               static_cast<size_t>(nBytesPerSample) * nBlockXSize);
    }

#ifdef CPL_LSB
    GDALSwapWords(pImage, nBytesPerSample, nBlockXSize, nBytesPerSample);
#endif

    CPLFree(pabyRecord);

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              CCPRasterBand                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           CCPRasterBand()                            */
/************************************************************************/

CCPRasterBand::CCPRasterBand(SAR_CEOSDataset *poGDSIn, int nBandIn,
                             GDALDataType eType)

{
    poDS = poGDSIn;
    nBand = nBandIn;

    eDataType = eType;

    nBlockXSize = poGDSIn->nRasterXSize;
    nBlockYSize = 1;

    if (nBand == 1)
        SetMetadataItem("POLARIMETRIC_INTERP", "HH");
    else if (nBand == 2)
        SetMetadataItem("POLARIMETRIC_INTERP", "HV");
    else if (nBand == 3)
        SetMetadataItem("POLARIMETRIC_INTERP", "VH");
    else if (nBand == 4)
        SetMetadataItem("POLARIMETRIC_INTERP", "VV");
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

CPLErr CCPRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                 void *pImage)
{
    SAR_CEOSDataset *poGDS = cpl::down_cast<SAR_CEOSDataset *>(poDS);

    struct CeosSARImageDesc *ImageDesc = &(poGDS->sVolume.ImageDesc);

    const vsi_l_offset offset =
        ImageDesc->FileDescriptorLength +
        static_cast<vsi_l_offset>(ImageDesc->BytesPerRecord) * nBlockYOff +
        ImageDesc->ImageDataStart;

    /* -------------------------------------------------------------------- */
    /*      Load all the pixel data associated with this scanline.          */
    /* -------------------------------------------------------------------- */
    const int nBytesToRead = ImageDesc->BytesPerPixel * nBlockXSize;

    GByte *pabyRecord = (GByte *)CPLMalloc(nBytesToRead);

    if (VSIFSeekL(poGDS->fpImage, offset, SEEK_SET) != 0 ||
        (int)VSIFReadL(pabyRecord, 1, nBytesToRead, poGDS->fpImage) !=
            nBytesToRead)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Error reading %d bytes of CEOS record data at offset %" PRIu64
                 ".\n"
                 "Reading file %s failed.",
                 nBytesToRead, static_cast<uint64_t>(offset),
                 poGDS->GetDescription());
        CPLFree(pabyRecord);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize our power table if this is our first time through.   */
    /* -------------------------------------------------------------------- */
    static float afPowTable[256];
    static bool bPowTableInitialized = false;

    if (!bPowTableInitialized)
    {
        bPowTableInitialized = true;

        for (int i = 0; i < 256; i++)
        {
            afPowTable[i] = (float)pow(2.0, i - 128);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy the desired band out based on the size of the type, and    */
    /*      the interleaving mode.                                          */
    /* -------------------------------------------------------------------- */
    for (int iX = 0; iX < nBlockXSize; iX++)
    {
        unsigned char *pabyGroup = pabyRecord + iX * ImageDesc->BytesPerPixel;
        signed char *Byte =
            (signed char *)pabyGroup - 1; /* A ones based alias */
        double dfReSHH, dfImSHH, dfReSHV, dfImSHV, dfReSVH, dfImSVH, dfReSVV,
            dfImSVV;

        const double dfScale =
            sqrt((Byte[2] / 254.0 + 1.5) * afPowTable[Byte[1] + 128]);

        if (nBand == 1)
        {
            dfReSHH = Byte[3] * dfScale / 127.0;
            dfImSHH = Byte[4] * dfScale / 127.0;

            ((float *)pImage)[iX * 2] = (float)dfReSHH;
            ((float *)pImage)[iX * 2 + 1] = (float)dfImSHH;
        }
        else if (nBand == 2)
        {
            dfReSHV = Byte[5] * dfScale / 127.0;
            dfImSHV = Byte[6] * dfScale / 127.0;

            ((float *)pImage)[iX * 2] = (float)dfReSHV;
            ((float *)pImage)[iX * 2 + 1] = (float)dfImSHV;
        }
        else if (nBand == 3)
        {
            dfReSVH = Byte[7] * dfScale / 127.0;
            dfImSVH = Byte[8] * dfScale / 127.0;

            ((float *)pImage)[iX * 2] = (float)dfReSVH;
            ((float *)pImage)[iX * 2 + 1] = (float)dfImSVH;
        }
        else if (nBand == 4)
        {
            dfReSVV = Byte[9] * dfScale / 127.0;
            dfImSVV = Byte[10] * dfScale / 127.0;

            ((float *)pImage)[iX * 2] = (float)dfReSVV;
            ((float *)pImage)[iX * 2 + 1] = (float)dfImSVV;
        }
    }

    CPLFree(pabyRecord);

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            PALSARRasterBand                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          PALSARRasterBand()                          */
/************************************************************************/

PALSARRasterBand::PALSARRasterBand(SAR_CEOSDataset *poGDSIn, int nBandIn)

{
    poDS = poGDSIn;
    nBand = nBandIn;

    eDataType = GDT_CInt16;

    nBlockXSize = poGDSIn->nRasterXSize;
    nBlockYSize = 1;

    if (nBand == 1)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_11");
    else if (nBand == 2)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_22");
    else if (nBand == 3)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_33");
    else if (nBand == 4)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_12");
    else if (nBand == 5)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_13");
    else if (nBand == 6)
        SetMetadataItem("POLARIMETRIC_INTERP", "Covariance_23");
}

/************************************************************************/
/*                             IReadBlock()                             */
/*                                                                      */
/*      Based on ERSDAC-VX-CEOS-004                                     */
/************************************************************************/

CPLErr PALSARRasterBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                    void *pImage)
{
    SAR_CEOSDataset *poGDS = cpl::down_cast<SAR_CEOSDataset *>(poDS);

    struct CeosSARImageDesc *ImageDesc = &(poGDS->sVolume.ImageDesc);

    const vsi_l_offset offset =
        ImageDesc->FileDescriptorLength +
        static_cast<vsi_l_offset>(ImageDesc->BytesPerRecord) * nBlockYOff +
        ImageDesc->ImageDataStart;

    /* -------------------------------------------------------------------- */
    /*      Load all the pixel data associated with this scanline.          */
    /* -------------------------------------------------------------------- */
    const int nBytesToRead = ImageDesc->BytesPerPixel * nBlockXSize;

    GByte *pabyRecord = (GByte *)CPLMalloc(nBytesToRead);

    if (VSIFSeekL(poGDS->fpImage, offset, SEEK_SET) != 0 ||
        (int)VSIFReadL(pabyRecord, 1, nBytesToRead, poGDS->fpImage) !=
            nBytesToRead)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Error reading %d bytes of CEOS record data at offset %" PRIu64
                 ".\n"
                 "Reading file %s failed.",
                 nBytesToRead, static_cast<uint64_t>(offset),
                 poGDS->GetDescription());
        CPLFree(pabyRecord);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Copy the desired band out based on the size of the type, and    */
    /*      the interleaving mode.                                          */
    /* -------------------------------------------------------------------- */
    if (nBand == 1 || nBand == 2 || nBand == 3)
    {
        // we need to pre-initialize things to set the imaginary component to 0
        memset(pImage, 0, nBlockXSize * 4);

        GDALCopyWords(pabyRecord + 4 * (nBand - 1), GDT_Int16, 18, pImage,
                      GDT_Int16, 4, nBlockXSize);
#ifdef CPL_LSB
        GDALSwapWords(pImage, 2, nBlockXSize, 4);
#endif
    }
    else
    {
        GDALCopyWords(pabyRecord + 6 + 4 * (nBand - 4), GDT_CInt16, 18, pImage,
                      GDT_CInt16, 4, nBlockXSize);
#ifdef CPL_LSB
        GDALSwapWords(pImage, 2, nBlockXSize * 2, 2);
#endif
    }
    CPLFree(pabyRecord);

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

    if (nBand == 2)
    {
        GInt16 *panLine = (GInt16 *)pImage;

        for (int i = 0; i < nBlockXSize * 2; i++)
        {
            panLine[i] = (GInt16)CastToGInt16((float)2.0 * panLine[i]);
        }
    }
    else if (nBand == 4)
    {
        const double sqrt_2 = pow(2.0, 0.5);
        GInt16 *panLine = (GInt16 *)pImage;

        for (int i = 0; i < nBlockXSize * 2; i++)
        {
            panLine[i] =
                (GInt16)CastToGInt16((float)floor(panLine[i] * sqrt_2 + 0.5));
        }
    }
    else if (nBand == 6)
    {
        GInt16 *panLine = (GInt16 *)pImage;
        const double sqrt_2 = pow(2.0, 0.5);

        // real portion - just multiple by sqrt(2)
        for (int i = 0; i < nBlockXSize * 2; i += 2)
        {
            panLine[i] =
                (GInt16)CastToGInt16((float)floor(panLine[i] * sqrt_2 + 0.5));
        }

        // imaginary portion - conjugate and multiply
        for (int i = 1; i < nBlockXSize * 2; i += 2)
        {
            panLine[i] =
                (GInt16)CastToGInt16((float)floor(-panLine[i] * sqrt_2 + 0.5));
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              SAR_CEOSDataset                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          SAR_CEOSDataset()                           */
/************************************************************************/

SAR_CEOSDataset::SAR_CEOSDataset()
    : fpImage(nullptr), papszTempMD(nullptr), nGCPCount(0), pasGCPList(nullptr),
      papszExtraFiles(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);

    sVolume.Flavor = 0;
    sVolume.Sensor = 0;
    sVolume.ProductType = 0;
    sVolume.FileNamingConvention = 0;

    sVolume.VolumeDirectoryFile = 0;
    sVolume.SARLeaderFile = 0;
    sVolume.ImagryOptionsFile = 0;
    sVolume.SARTrailerFile = 0;
    sVolume.NullVolumeDirectoryFile = 0;

    sVolume.ImageDesc.ImageDescValid = 0;
    sVolume.ImageDesc.NumChannels = 0;
    sVolume.ImageDesc.ChannelInterleaving = 0;
    sVolume.ImageDesc.DataType = 0;
    sVolume.ImageDesc.BytesPerRecord = 0;
    sVolume.ImageDesc.Lines = 0;
    sVolume.ImageDesc.TopBorderPixels = 0;
    sVolume.ImageDesc.BottomBorderPixels = 0;
    sVolume.ImageDesc.PixelsPerLine = 0;
    sVolume.ImageDesc.LeftBorderPixels = 0;
    sVolume.ImageDesc.RightBorderPixels = 0;
    sVolume.ImageDesc.BytesPerPixel = 0;
    sVolume.ImageDesc.RecordsPerLine = 0;
    sVolume.ImageDesc.PixelsPerRecord = 0;
    sVolume.ImageDesc.ImageDataStart = 0;
    sVolume.ImageDesc.ImageSuffixData = 0;
    sVolume.ImageDesc.FileDescriptorLength = 0;
    sVolume.ImageDesc.PixelOrder = 0;
    sVolume.ImageDesc.LineOrder = 0;
    sVolume.ImageDesc.PixelDataBytesPerRecord = 0;

    sVolume.RecordList = nullptr;
}

/************************************************************************/
/*                          ~SAR_CEOSDataset()                          */
/************************************************************************/

SAR_CEOSDataset::~SAR_CEOSDataset()

{
    FlushCache(true);

    CSLDestroy(papszTempMD);

    if (fpImage != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpImage));

    if (nGCPCount > 0)
    {
        GDALDeinitGCPs(nGCPCount, pasGCPList);
    }
    CPLFree(pasGCPList);

    if (sVolume.RecordList)
    {
        for (Link_t *Links = sVolume.RecordList; Links != nullptr;
             Links = Links->next)
        {
            if (Links->object)
            {
                DeleteCeosRecord((CeosRecord_t *)Links->object);
                Links->object = nullptr;
            }
        }
        DestroyList(sVolume.RecordList);
    }
    FreeRecipes();
    CSLDestroy(papszExtraFiles);
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int SAR_CEOSDataset::GetGCPCount()

{
    if (!m_bHasScannedForGCP)
        ScanForGCPs();
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *SAR_CEOSDataset::GetGCPSpatialRef() const

{
    if (!m_bHasScannedForGCP)
        const_cast<SAR_CEOSDataset *>(this)->ScanForGCPs();
    if (nGCPCount > 0)
        return &m_oSRS;

    return nullptr;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *SAR_CEOSDataset::GetGCPs()

{
    if (!m_bHasScannedForGCP)
        ScanForGCPs();
    return pasGCPList;
}

/************************************************************************/
/*                       GetMetadataDomainList()                        */
/************************************************************************/

char **SAR_CEOSDataset::GetMetadataDomainList()
{
    return CSLAddString(GDALDataset::GetMetadataDomainList(),
                        "ceos-FFF-n-n-n-n:r");
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
/*      accessible, and this normally means that most image records     */
/*      are not available.                                              */
/************************************************************************/

CSLConstList SAR_CEOSDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain == nullptr || !STARTS_WITH_CI(pszDomain, "ceos-"))
        return GDALDataset::GetMetadata(pszDomain);

    /* -------------------------------------------------------------------- */
    /*      Identify which file to fetch the file from.                     */
    /* -------------------------------------------------------------------- */
    int nFileId = -1;

    if (STARTS_WITH_CI(pszDomain, "ceos-vol"))
    {
        nFileId = CEOS_VOLUME_DIR_FILE;
    }
    else if (STARTS_WITH_CI(pszDomain, "ceos-lea"))
    {
        nFileId = CEOS_LEADER_FILE;
    }
    else if (STARTS_WITH_CI(pszDomain, "ceos-img"))
    {
        nFileId = CEOS_IMAGRY_OPT_FILE;
    }
    else if (STARTS_WITH_CI(pszDomain, "ceos-trl"))
    {
        nFileId = CEOS_TRAILER_FILE;
    }
    else if (STARTS_WITH_CI(pszDomain, "ceos-nul"))
    {
        nFileId = CEOS_NULL_VOL_FILE;
    }
    else
        return nullptr;

    pszDomain += 8;

    /* -------------------------------------------------------------------- */
    /*      Identify the record type.                                       */
    /* -------------------------------------------------------------------- */
    int a, b, c, d, nRecordIndex = -1;

    if (sscanf(pszDomain, "-%d-%d-%d-%d:%d", &a, &b, &c, &d, &nRecordIndex) !=
            5 &&
        sscanf(pszDomain, "-%d-%d-%d-%d", &a, &b, &c, &d) != 4)
    {
        return nullptr;
    }

    CeosTypeCode_t sTypeCode = QuadToTC(a, b, c, d);

    /* -------------------------------------------------------------------- */
    /*      Try to fetch the record.                                        */
    /* -------------------------------------------------------------------- */
    CeosRecord_t *record = FindCeosRecord(sVolume.RecordList, sTypeCode,
                                          nFileId, -1, nRecordIndex);

    if (record == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Massage the data into a safe textual format.  The RawRecord     */
    /*      just has zero bytes turned into spaces while the                */
    /*      EscapedRecord has regular backslash escaping applied to zero    */
    /*      chars, double quotes, and backslashes.                          */
    /*      just turn zero bytes into spaces.                               */
    /* -------------------------------------------------------------------- */

    CSLDestroy(papszTempMD);

    // Escaped version
    char *pszSafeCopy = CPLEscapeString((char *)record->Buffer, record->Length,
                                        CPLES_BackslashQuotable);
    papszTempMD = CSLSetNameValue(nullptr, "EscapedRecord", pszSafeCopy);
    CPLFree(pszSafeCopy);

    // Copy with '\0' replaced by spaces.

    pszSafeCopy = (char *)CPLCalloc(1, record->Length + 1);
    memcpy(pszSafeCopy, record->Buffer, record->Length);

    for (int i = 0; i < record->Length; i++)
        if (pszSafeCopy[i] == '\0')
            pszSafeCopy[i] = ' ';

    papszTempMD = CSLSetNameValue(papszTempMD, "RawRecord", pszSafeCopy);

    CPLFree(pszSafeCopy);

    return papszTempMD;
}

/************************************************************************/
/*                          ScanForMetadata()                           */
/************************************************************************/

void SAR_CEOSDataset::ScanForMetadata()

{
    /* -------------------------------------------------------------------- */
    /*      Get the volume id (with the sensor name)                        */
    /* -------------------------------------------------------------------- */
    const CeosRecord_t *record =
        FindCeosRecord(sVolume.RecordList, VOLUME_DESCRIPTOR_RECORD_TC,
                       CEOS_VOLUME_DIR_FILE, -1, -1);

    struct FieldDef
    {
        const char *pszMetadataItemName;
        int nOffsetInRecord;
        const char *pszFormat;
    };

    CPLString osVolId;
    char szField[128];
    szField[0] = '\0';
    if (record != nullptr)
    {
        GetCeosField(record, 61, "A16", szField);
        osVolId = szField;
        osVolId.Trim();
        SetMetadataItem("CEOS_LOGICAL_VOLUME_ID", osVolId.c_str());

        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_PROCESSING_FACILITY", 149, "A12"},
            {"CEOS_PROCESSING_AGENCY", 141, "A8"},
            {"CEOS_PROCESSING_COUNTRY", 129, "A12"},
            {"CEOS_SOFTWARE_ID", 33, "A12"},
            {"CEOS_PRODUCT_ID", 261, "A8"},
            {"CEOS_VOLSET_ID", 77, "A16"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
        }
    }

    /* ==================================================================== */
    /*      Dataset summary record.                                         */
    /* ==================================================================== */
    record = FindCeosRecord(sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                            CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
        record =
            FindCeosRecord(sVolume.RecordList, LEADER_DATASET_SUMMARY_ASF_TC,
                           CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
        record = FindCeosRecord(sVolume.RecordList, LEADER_DATASET_SUMMARY_TC,
                                CEOS_TRAILER_FILE, -1, -1);

    if (record == nullptr)
        record =
            FindCeosRecord(sVolume.RecordList, LEADER_DATASET_SUMMARY_ERS2_TC,
                           CEOS_LEADER_FILE, -1, -1);

    if (record != nullptr)
    {
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_ACQUISITION_TIME", 69, "A32"},
            {"CEOS_ASC_DES", 101, "A16"},  // Ascending/Descending (RSAT only)
            {"CEOS_TRUE_HEADING", 149, "A16"},  //  at least for ERS2
            {"CEOS_ELLIPSOID", 165, "A16"},
            {"CEOS_SEMI_MAJOR", 181, "A16"},
            {"CEOS_SEMI_MINOR", 197, "A16"},
            {"CEOS_SCENE_LENGTH_KM", 341, "A16"},
            {"CEOS_SCENE_WIDTH_KM", 357, "A16"},
            {"CEOS_MISSION_ID", 397, "A16"},
            {"CEOS_SENSOR_ID", 413, "A32"},
            {"CEOS_ORBIT_NUMBER", 445, "A8"},
            {"CEOS_PLATFORM_LATITUDE", 453, "A8"},
            {"CEOS_PLATFORM_LONGITUDE", 461, "A8"},
            {"CEOS_PLATFORM_HEADING", 469, "A8"},    //  at least for ERS2
            {"CEOS_SENSOR_CLOCK_ANGLE", 477, "A8"},  // Look Angle
            {"CEOS_INC_ANGLE", 485, "A8"},           // Incidence Angle
            {"CEOS_FACILITY", 1047, "A16"},
            {"CEOS_PIXEL_TIME_DIR", 1527,
             "A8"},  // Pixel time direction indicator
            {"CEOS_LINE_SPACING_METERS", 1687, "A16"},
            {"CEOS_PIXEL_SPACING_METERS", 1703, "A16"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty() &&
                (osVolId.find("RSAT") != std::string::npos ||
                 !EQUAL(sDef.pszMetadataItemName, "CEOS_ASC_DES")))
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Get the beam mode, for radarsat.                                */
    /* -------------------------------------------------------------------- */
    record =
        FindCeosRecord(sVolume.RecordList, LEADER_RADIOMETRIC_COMPENSATION_TC,
                       CEOS_LEADER_FILE, -1, -1);

    if (osVolId.find("RSAT") != std::string::npos && record != nullptr)
    {
        GetCeosField(record, 4189, "A16", szField);
        CPLString osField(szField);
        osField.Trim();

        SetMetadataItem("CEOS_BEAM_TYPE", osField.c_str());
    }

    /* ==================================================================== */
    /*      ERS calibration and incidence angle info                        */
    /* ==================================================================== */
    record = FindCeosRecord(sVolume.RecordList, ERS_GENERAL_FACILITY_DATA_TC,
                            CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
        record =
            FindCeosRecord(sVolume.RecordList, ERS_GENERAL_FACILITY_DATA_ALT_TC,
                           CEOS_LEADER_FILE, -1, -1);

    if (record != nullptr)
    {
        GetCeosField(record, 13, "A64", szField);
        CPLString osField(szField);
        osField.Trim();

        /* Avoid PCS records, which don't contain necessary info */
        if (osField.find("GENERAL") == std::string::npos)
            record = nullptr;
    }

    if (record != nullptr)
    {
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_INC_ANGLE_FIRST_RANGE", 583, "A16"},
            {"CEOS_INC_ANGLE_CENTRE_RANGE", 599, "A16"},
            {"CEOS_INC_ANGLE_LAST_RANGE", 615, "A16"},
            {"CEOS_CALIBRATION_CONSTANT_K", 663, "A16"},
            {"CEOS_GROUND_TO_SLANT_C0", 1855, "A20"},
            {"CEOS_GROUND_TO_SLANT_C1", 1875, "A20"},
            {"CEOS_GROUND_TO_SLANT_C2", 1895, "A20"},
            {"CEOS_GROUND_TO_SLANT_C3", 1915, "A20"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Detailed Processing Parameters (Radarsat)                       */
    /* -------------------------------------------------------------------- */
    record = FindCeosRecord(sVolume.RecordList, RSAT_PROC_PARAM_TC,
                            CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
        record = FindCeosRecord(sVolume.RecordList, RSAT_PROC_PARAM_TC,
                                CEOS_TRAILER_FILE, -1, -1);

    if (record != nullptr)
    {
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_PROC_START", 192, "A21"},
            {"CEOS_PROC_STOP", 213, "A21"},
            {"CEOS_EPH_ORB_DATA_0", 4649, "A16"},
            {"CEOS_EPH_ORB_DATA_1", 4665, "A16"},
            {"CEOS_EPH_ORB_DATA_2", 4681, "A16"},
            {"CEOS_EPH_ORB_DATA_3", 4697, "A16"},
            {"CEOS_EPH_ORB_DATA_4", 4713, "A16"},
            {"CEOS_EPH_ORB_DATA_5", 4729, "A16"},
            {"CEOS_EPH_ORB_DATA_6", 4745, "A16"},
            {"CEOS_GROUND_TO_SLANT_C0", 4908, "A16"},
            {"CEOS_GROUND_TO_SLANT_C1", 4924, "A16"},
            {"CEOS_GROUND_TO_SLANT_C2", 4940, "A16"},
            {"CEOS_GROUND_TO_SLANT_C3", 4956, "A16"},
            {"CEOS_GROUND_TO_SLANT_C4", 4972, "A16"},
            {"CEOS_GROUND_TO_SLANT_C5", 4988, "A16"},
            {"CEOS_INC_ANGLE_FIRST_RANGE", 7334, "A16"},
            {"CEOS_INC_ANGLE_LAST_RANGE", 7350, "A16"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Get process-to-raw data coordinate translation values.  These   */
    /*      are likely specific to Atlantis APP products.                   */
    /* -------------------------------------------------------------------- */
    record = FindCeosRecord(sVolume.RecordList, IMAGE_HEADER_RECORD_TC,
                            CEOS_IMAGRY_OPT_FILE, -1, -1);

    if (record != nullptr)
    {
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_DM_CORNER", 449, "A4"},
            {"CEOS_DM_TRANSPOSE", 453, "A4"},
            {"CEOS_DM_START_SAMPLE", 457, "A4"},
            {"CEOS_DM_START_PULSE", 461, "A5"},
            {"CEOS_DM_FAST_ALPHA", 466, "A16"},
            {"CEOS_DM_FAST_BETA", 482, "A16"},
            {"CEOS_DM_SLOW_ALPHA", 498, "A16"},
            {"CEOS_DM_SLOW_BETA", 514, "A16"},
            {"CEOS_DM_FAST_ALPHA_2", 530, "A16"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to find calibration information from Radiometric Data       */
    /*      Record.                                                         */
    /* -------------------------------------------------------------------- */
    record =
        FindCeosRecord(sVolume.RecordList, LEADER_RADIOMETRIC_DATA_RECORD_TC,
                       CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
        record = FindCeosRecord(sVolume.RecordList,
                                LEADER_RADIOMETRIC_DATA_RECORD_TC,
                                CEOS_TRAILER_FILE, -1, -1);

    if (record != nullptr)
    {
        GetCeosField(record, 8317, "A16", szField);
        CPLString osField(szField);
        osField.Trim();
        if (!osField.empty())
            SetMetadataItem("CEOS_CALIBRATION_OFFSET", osField.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      For ERS Standard Format Landsat scenes we pick up the           */
    /*      calibration offset and gain from the Radiometric Ancillary      */
    /*      Record.                                                         */
    /* -------------------------------------------------------------------- */
    record =
        FindCeosRecord(sVolume.RecordList, QuadToTC(0x3f, 0x24, 0x12, 0x09),
                       CEOS_LEADER_FILE, -1, -1);
    if (record != nullptr)
    {
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_OFFSET_A0", 29, "A20"},
            {"CEOS_GAIN_A1", 49, "A20"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      For ERS Standard Format Landsat scenes we pick up the           */
    /*      gain setting from the Scene Header Record.                      */
    /* -------------------------------------------------------------------- */
    record =
        FindCeosRecord(sVolume.RecordList, QuadToTC(0x12, 0x12, 0x12, 0x09),
                       CEOS_LEADER_FILE, -1, -1);
    if (record != nullptr)
    {
        GetCeosField(record, 1486, "A1", szField);
        szField[1] = '\0';

        if (szField[0] == 'H' || szField[0] == 'V')
            SetMetadataItem("CEOS_GAIN_SETTING", szField);
    }

    /* -------------------------------------------------------------------- */
    /*      PALSAR-2 ALOS2 Platform position data                           */
    /* -------------------------------------------------------------------- */
    record =
        FindCeosRecord(sVolume.RecordList, LEADER_PLATFORM_POSITION_PALSAR_TC,
                       CEOS_LEADER_FILE, -1, -1);
    if (record && record->Length > 387)
    {
        // Table 3.3-7 Platform position data records
        // of https://www.eorc.jaxa.jp/ALOS/en/alos-2/pdf/product_format_description/PALSAR-2_xx_Format_CEOS_E_g.pdf
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENTS_DESIGNATOR", 13, "A32"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_1", 45, "A16"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_2", 61, "A16"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_3", 77, "A16"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_4", 93, "A16"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_5", 109, "A16"},
            {"CEOS_PLATFORM_POS_ORBITAL_ELEMENT_6", 125, "A16"},
            {"CEOS_PLATFORM_POS_NUMBER_POINTS", 141, "A4"},
            {"CEOS_PLATFORM_POS_YEAR_POINT_1", 145, "A4"},
            {"CEOS_PLATFORM_POS_MONTH_POINT_1", 149, "A4"},
            {"CEOS_PLATFORM_POS_DAY_POINT_1", 153, "A4"},
            {"CEOS_PLATFORM_POS_DAY_IN_YEAR_POINT_1", 157, "A4"},
            {"CEOS_PLATFORM_POS_SECONDS_OF_DAY_POINT_1", 161, "A22"},
            {"CEOS_PLATFORM_POS_TIME_INTERVAL_SECONDS", 183, "A22"},
            {"CEOS_PLATFORM_POS_REF_COORD_SYS", 205, "A64"},
            {"CEOS_PLATFORM_POS_GREENWICH_MEAN_HOUR_ANGLE_DEG", 269, "A22"},
            {"CEOS_PLATFORM_POS_ALONG_TRACK_POS_ERROR_METERS", 291, "A16"},
            {"CEOS_PLATFORM_POS_ACROSS_TRACK_POS_ERROR_METERS", 307, "A16"},
            {"CEOS_PLATFORM_POS_RADIAL_POS_ERROR_METERS", 323, "A16"},
            {"CEOS_PLATFORM_POS_ALONG_TRACK_VELOCITY_ERROR_METERS_PER_SEC", 339,
             "A16"},
            {"CEOS_PLATFORM_POS_ACROSS_TRACK_VELOCITY_ERROR_METERS_PER_SEC",
             355, "A16"},
            {"CEOS_PLATFORM_POS_RADIAL_VELOCITY_ERROR_METERS_PER_SEC", 371,
             "A16"},
        };
        int nPoints = 0;
        constexpr int OFFSET_POINT_1 = 387;
        constexpr int POINT_SIZE = 6 * 22;
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
                if (EQUAL(sDef.pszMetadataItemName,
                          "CEOS_PLATFORM_POS_NUMBER_POINTS"))
                {
                    nPoints = std::clamp(atoi(osField), 0,
                                         (record->Length - OFFSET_POINT_1) /
                                             POINT_SIZE);
                }
            }
        }

        constexpr FieldDef asPointFieldDefs[] = {
            {"VECTOR_X_METERS", 0, "A22"},
            {"VECTOR_Y_METERS", 22, "A22"},
            {"VECTOR_Z_METERS", 44, "A22"},
            {"VECTOR_X_DERIV_METERS_PER_SEC", 66, "A22"},
            {"VECTOR_Y_DERIV_METERS_PER_SEC", 88, "A22"},
            {"VECTOR_Z_DERIV_METERS_PER_SEC", 110, "A22"},
        };
        for (int iPnt = 0; iPnt < nPoints; ++iPnt)
        {
            for (const auto &sDef : asPointFieldDefs)
            {
                GetCeosField(record,
                             sDef.nOffsetInRecord + OFFSET_POINT_1 +
                                 iPnt * POINT_SIZE,
                             sDef.pszFormat, szField);
                CPLString osField(szField);
                osField.Trim();
                if (!osField.empty())
                {
                    SetMetadataItem(std::string("CEOS_PLATFORM_POS_")
                                        .append(sDef.pszMetadataItemName)
                                        .append("_POINT_")
                                        .append(std::to_string(iPnt + 1))
                                        .c_str(),
                                    osField.c_str());
                }
            }
        }

        {
            GetCeosField(record, 4101, "A1", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem("CEOS_PLATFORM_POS_LEAP_SECOND",
                                osField.c_str());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      PALSAR-2 ALOS2 Attitude data                                    */
    /* -------------------------------------------------------------------- */
    record = FindCeosRecord(sVolume.RecordList, LEADER_ATTITUDE_PALSAR_TC,
                            CEOS_LEADER_FILE, -1, -1);
    if (record)
    {
        // Table 3.3-8 Attitude data records
        // of https://www.eorc.jaxa.jp/ALOS/en/alos-2/pdf/product_format_description/PALSAR-2_xx_Format_CEOS_E_g.pdf

        int nPoints = 0;
        constexpr int OFFSET_POINT_1 = 17;
        constexpr int POINT_SIZE = 120;
        {
            GetCeosField(record, 13, "A4", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem("CEOS_PLATFORM_ATT_NUMBER_POINTS",
                                osField.c_str());
                nPoints =
                    std::clamp(atoi(osField), 0,
                               (record->Length - OFFSET_POINT_1) / POINT_SIZE);
            }
        }

        constexpr FieldDef asFieldDefs[] = {
            {"DAY_OF_YEAR", 17, "A4"},
            {"MILLISECOND_OF_DAY", 21, "A8"},
            {"PITCH_QUALITY_FLAG", 29, "A4"},
            {"ROLL_QUALITY_FLAG", 33, "A4"},
            {"YAW_QUALITY_FLAG", 37, "A4"},
            {"PITCH_DEG", 41, "A14"},
            {"ROLL_DEG", 55, "A14"},
            {"YAW_DEG", 69, "A14"},
            {"PITCH_RATE_QUALITY_FLAG", 83, "A4"},
            {"ROL_RATE_QUALITY_FLAG", 87, "A4"},
            {"YAW_RATE_QUALITY_FLAG", 91, "A4"},
            {"PITCH_RATE", 95, "A14"},
            {"ROLL_RATE", 109, "A14"},
            {"YAW_RATE", 123, "A14"},
        };
        for (int iPnt = 0; iPnt < nPoints; ++iPnt)
        {
            for (const auto &sDef : asFieldDefs)
            {
                GetCeosField(record, sDef.nOffsetInRecord + iPnt * POINT_SIZE,
                             sDef.pszFormat, szField);
                CPLString osField(szField);
                osField.Trim();
                if (!osField.empty())
                {
                    SetMetadataItem(std::string("CEOS_PLATFORM_ATT_")
                                        .append(sDef.pszMetadataItemName)
                                        .append("_POINT_")
                                        .append(std::to_string(iPnt + 1))
                                        .c_str(),
                                    osField.c_str());
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      PALSAR-2 ALOS2 Radiometric data                                 */
    /* -------------------------------------------------------------------- */
    record = FindCeosRecord(sVolume.RecordList, LEADER_RADIOMETRIC_PALSAR_TC,
                            CEOS_LEADER_FILE, -1, -1);
    if (record)
    {
        // Table 3.3-9 Radiometric data records
        // of https://www.eorc.jaxa.jp/ALOS/en/alos-2/pdf/product_format_description/PALSAR-2_xx_Format_CEOS_E_g.pdf
        constexpr FieldDef asFieldDefs[] = {
            {"CEOS_RADIOMETRIC_CALIBRATION_FACTOR", 21, "A16"},
            {"CEOS_RADIOMETRIC_DT_1_1_REAL", 37, "A16"},
            {"CEOS_RADIOMETRIC_DT_1_1_IMAG", 53, "A16"},
            {"CEOS_RADIOMETRIC_DT_1_2_REAL", 69, "A16"},
            {"CEOS_RADIOMETRIC_DT_1_2_IMAG", 85, "A16"},
            {"CEOS_RADIOMETRIC_DT_2_1_REAL", 101, "A16"},
            {"CEOS_RADIOMETRIC_DT_2_1_IMAG", 117, "A16"},
            {"CEOS_RADIOMETRIC_DT_2_2_REAL", 133, "A16"},
            {"CEOS_RADIOMETRIC_DT_2_2_IMAG", 149, "A16"},
            {"CEOS_RADIOMETRIC_DR_1_1_REAL", 165, "A16"},
            {"CEOS_RADIOMETRIC_DR_1_1_IMAG", 181, "A16"},
            {"CEOS_RADIOMETRIC_DR_1_2_REAL", 197, "A16"},
            {"CEOS_RADIOMETRIC_DR_1_2_IMAG", 213, "A16"},
            {"CEOS_RADIOMETRIC_DR_2_1_REAL", 229, "A16"},
            {"CEOS_RADIOMETRIC_DR_2_1_IMAG", 245, "A16"},
            {"CEOS_RADIOMETRIC_DR_2_2_REAL", 261, "A16"},
            {"CEOS_RADIOMETRIC_DR_2_2_IMAG", 277, "A16"},
        };
        for (const auto &sDef : asFieldDefs)
        {
            GetCeosField(record, sDef.nOffsetInRecord, sDef.pszFormat, szField);
            CPLString osField(szField);
            osField.Trim();
            if (!osField.empty())
            {
                SetMetadataItem(sDef.pszMetadataItemName, osField.c_str());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      PALSAR-2 ALOS2 Facility related data                            */
    /* -------------------------------------------------------------------- */
    constexpr int FACILITY_RELATED_DATA_5_LAT_LONG_CONV_FACTORS = 5;
    record = FindCeosRecord(
        sVolume.RecordList, LEADER_FACILITY_RELATED_PALSAR_TC, CEOS_LEADER_FILE,
        -1, FACILITY_RELATED_DATA_5_LAT_LONG_CONV_FACTORS - 1);
    if (record && record->Length == 5000)
    {
        // Table 3-17 Facility related data records 5
        // of https://www.eorc.jaxa.jp/ALOS/en/alos-2/pdf/product_format_description/PALSAR-2_xx_Format_CEOS_E_g.pdf

        std::string coeffs;
        for (int i = 0; i < 10; ++i)
        {
            GetCeosField(record, 17 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 10 coefficients to convert from the map projection (E, N) to pixel (P)
        SetMetadataItem("CEOS_FACILITY_5_XY_PROJECTED_TO_PIXEL_COEFFICIENTS",
                        coeffs.c_str());

        coeffs.clear();
        for (int i = 0; i < 10; ++i)
        {
            GetCeosField(record, 17 + 10 * 20 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 10 coefficients to convert from the map projection (E, N) to line (L)
        SetMetadataItem("CEOS_FACILITY_5_XY_PROJECTED_TO_LINE_COEFFICIENTS",
                        coeffs.c_str());

        coeffs.clear();
        for (int i = 0; i < 25; ++i)
        {
            GetCeosField(record, 1025 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 25 coefficients of the 8th polynomial expression to convert from pixel (P) and line (L) to latitude (φ)
        SetMetadataItem("CEOS_FACILITY_5_PIXEL_LINE_TO_LAT_COEFFICIENTS",
                        coeffs.c_str());

        coeffs.clear();
        for (int i = 0; i < 25; ++i)
        {
            GetCeosField(record, 1525 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 25 coefficients of the 8th polynomial expression to convert from pixel (P) and line (L) to longitude (λ)
        SetMetadataItem("CEOS_FACILITY_5_PIXEL_LINE_TO_LON_COEFFICIENTS",
                        coeffs.c_str());

        {
            GetCeosField(record, 2025, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            SetMetadataItem("CEOS_FACILITY_5_ORIGIN_PIXEL", osField.c_str());
        }

        {
            GetCeosField(record, 2045, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            SetMetadataItem("CEOS_FACILITY_5_ORIGIN_LINE", osField.c_str());
        }

        coeffs.clear();
        for (int i = 0; i < 25; ++i)
        {
            GetCeosField(record, 2065 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 25 coefficients of the 8th polynomial expression to convert from latitude (Φ) and longitude (Λ) to pixel (p)
        SetMetadataItem("CEOS_FACILITY_5_LAT_LON_TO_PIXEL_COEFFICIENTS",
                        coeffs.c_str());

        coeffs.clear();
        for (int i = 0; i < 25; ++i)
        {
            GetCeosField(record, 2065 + 25 * 20 + i * 20, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            if (!coeffs.empty())
                coeffs += ' ';
            coeffs += osField;
        }
        // 25 coefficients of the 8th polynomial expression to convert from latitude (Φ) and longitude (Λ) to pixel (p)
        SetMetadataItem("CEOS_FACILITY_5_LAT_LON_TO_LINE_COEFFICIENTS",
                        coeffs.c_str());

        {
            GetCeosField(record, 3065, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            SetMetadataItem("CEOS_FACILITY_5_ORIGIN_LAT", osField.c_str());
        }

        {
            GetCeosField(record, 3085, "A20", szField);
            CPLString osField(szField);
            osField.Trim();
            SetMetadataItem("CEOS_FACILITY_5_ORIGIN_LON", osField.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      PALSAR Level 1.5/Level 3.1 processed record metadata            */
    /* -------------------------------------------------------------------- */
    record = FindCeosRecord(sVolume.RecordList,
                            IMAGE_PROCESSED_DATA_RECORD_PALSAR_TC,
                            CEOS_IMAGRY_OPT_FILE, -1, 2);
    constexpr int NEEDED_SIZE_IN_PROCESSED_DATA_RECORD_PALSAR = 128;
    if (record && record->Length >= NEEDED_SIZE_IN_PROCESSED_DATA_RECORD_PALSAR)
    {
        const auto ReadProcessedDataRecord =
            [this](const CeosRecord_t *curRecord,
                   const char *pszMDNameSuffix = "")
        {
            int32_t nInt32 = 0;

            GetCeosField(curRecord, 65, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_SLANT_RANGE_FIRST_PIXEL%s_METERS",
                                       pszMDNameSuffix),
                            CPLSPrintf("%d", nInt32));

            GetCeosField(curRecord, 69, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_SLANT_RANGE_MID_PIXEL%s_METERS",
                                       pszMDNameSuffix),
                            CPLSPrintf("%d", nInt32));

            GetCeosField(curRecord, 73, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_SLANT_RANGE_LAST_PIXEL%s_METERS",
                                       pszMDNameSuffix),
                            CPLSPrintf("%d", nInt32));

            GetCeosField(curRecord, 77, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_DOPPLER_CENTROID_FIRST_PIXEL%s_HZ",
                                       pszMDNameSuffix),
                            CPLSPrintf("%.3f", nInt32 / 1000.0));

            GetCeosField(curRecord, 81, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_DOPPLER_CENTROID_MID_PIXEL%s_HZ",
                                       pszMDNameSuffix),
                            CPLSPrintf("%.3f", nInt32 / 1000.0));

            GetCeosField(curRecord, 85, "B4", &nInt32);
            SetMetadataItem(CPLSPrintf("CEOS_DOPPLER_CENTROID_LAST_PIXEL%s_HZ",
                                       pszMDNameSuffix),
                            CPLSPrintf("%.3f", nInt32 / 1000.0));

            GetCeosField(curRecord, 89, "B4", &nInt32);
            SetMetadataItem(
                CPLSPrintf("CEOS_AZIMUTH_FM_RATE_FIRST_PIXEL%s_HZ_PER_MS",
                           pszMDNameSuffix),
                CPLSPrintf("%d", nInt32));

            GetCeosField(curRecord, 93, "B4", &nInt32);
            SetMetadataItem(
                CPLSPrintf("CEOS_AZIMUTH_FM_RATE_MID_PIXEL%s_HZ_PER_MS",
                           pszMDNameSuffix),
                CPLSPrintf("%d", nInt32));

            GetCeosField(curRecord, 97, "B4", &nInt32);
            SetMetadataItem(
                CPLSPrintf("CEOS_AZIMUTH_FM_RATE_LAST_PIXEL%s_HZ_PER_MS",
                           pszMDNameSuffix),
                CPLSPrintf("%d", nInt32));
        };

        ReadProcessedDataRecord(record);

        // The above values are per-record. In practice they seem to be constant
        // among all records, but I could not find any statement on that, so
        // also read and report them from the last record.
        // We cannot use FindCeosRecord() to fetch it because ProcessData() limits
        // to 4 records for the image file (for memory and performance reasons)
        struct CeosSARImageDesc *ImageDesc = &(sVolume.ImageDesc);
        const vsi_l_offset nOffsetToLastRecordStart =
            ImageDesc->FileDescriptorLength +
            static_cast<vsi_l_offset>(ImageDesc->BytesPerRecord) *
                (nRasterYSize - 1);
        CeosRecord_t lastRecord;
        memset(&lastRecord, 0, sizeof(lastRecord));
        std::vector<GByte> abyLeader(
            NEEDED_SIZE_IN_PROCESSED_DATA_RECORD_PALSAR);
        if (fpImage->Seek(nOffsetToLastRecordStart, SEEK_SET) == 0 &&
            fpImage->Read(abyLeader.data(), abyLeader.size()) ==
                abyLeader.size())
        {
            lastRecord.Buffer = abyLeader.data();
            lastRecord.Length = static_cast<int>(abyLeader.size());
            ReadProcessedDataRecord(&lastRecord, "_LAST_LINE");
        }
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
    /* -------------------------------------------------------------------- */
    /*      Find record, and try to determine if it has useful GCPs.        */
    /* -------------------------------------------------------------------- */

    CeosRecord_t *record =
        FindCeosRecord(sVolume.RecordList, LEADER_MAP_PROJ_RECORD_TC,
                       CEOS_LEADER_FILE, -1, -1);

    int gcp_ordering_mode = CEOS_STD_MAPREC_GCP_ORDER;
    /* JERS from Japan */
    if (record == nullptr)
        record =
            FindCeosRecord(sVolume.RecordList, LEADER_MAP_PROJ_RECORD_JERS_TC,
                           CEOS_LEADER_FILE, -1, -1);

    if (record == nullptr)
    {
        record =
            FindCeosRecord(sVolume.RecordList, LEADER_MAP_PROJ_RECORD_ASF_TC,
                           CEOS_LEADER_FILE, -1, -1);
        gcp_ordering_mode = CEOS_ASF_MAPREC_GCP_ORDER;
    }
    if (record == nullptr)
    {
        record = FindCeosRecord(sVolume.RecordList, LEADER_FACILITY_ASF_TC,
                                CEOS_LEADER_FILE, -1, -1);
        gcp_ordering_mode = CEOS_ASF_FACREC_GCP_ORDER;
    }

    if (record == nullptr)
        return FALSE;

    char szField[100];
    memset(szField, 0, 17);
    GetCeosField(record, 29, "A16", szField);

    int GCPFieldSize = 16;
    int GCPOffset = 1073;

    if (!STARTS_WITH_CI(szField, "Slant Range") &&
        !STARTS_WITH_CI(szField, "Ground Range") &&
        !STARTS_WITH_CI(szField, "GEOCODED"))
    {
        /* detect ASF map projection record */
        GetCeosField(record, 1079, "A7", szField);
        if (!STARTS_WITH_CI(szField, "Slant") &&
            !STARTS_WITH_CI(szField, "Ground"))
        {
            return FALSE;
        }
        else
        {
            GCPFieldSize = 17;
            GCPOffset = 157;
        }
    }

    char FieldSize[4];
    snprintf(FieldSize, sizeof(FieldSize), "A%d", GCPFieldSize);

    GetCeosField(record, GCPOffset, FieldSize, szField);
    if (STARTS_WITH_CI(szField, "        "))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Read corner points.                                             */
    /* -------------------------------------------------------------------- */
    nGCPCount = 4;
    pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP), nGCPCount);

    GDALInitGCPs(nGCPCount, pasGCPList);

    for (int i = 0; i < nGCPCount; i++)
    {
        char szId[32];

        snprintf(szId, sizeof(szId), "%d", i + 1);
        CPLFree(pasGCPList[i].pszId);
        pasGCPList[i].pszId = CPLStrdup(szId);

        GetCeosField(record, GCPOffset + (GCPFieldSize * 2) * i, FieldSize,
                     szField);
        pasGCPList[i].dfGCPY = CPLAtof(szField);
        GetCeosField(record, GCPOffset + GCPFieldSize + (GCPFieldSize * 2) * i,
                     FieldSize, szField);
        pasGCPList[i].dfGCPX = CPLAtof(szField);
        pasGCPList[i].dfGCPZ = 0.0;
    }

    /* Map Projection Record has the order UL UR LR LL
     ASF Facility Data Record has the order UL,LL,UR,LR
     ASF Map Projection Record has the order LL, LR, UR, UL */

    pasGCPList[0].dfGCPLine = 0.5;
    pasGCPList[0].dfGCPPixel = 0.5;

    switch (gcp_ordering_mode)
    {
        case CEOS_ASF_FACREC_GCP_ORDER:
            pasGCPList[1].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[1].dfGCPPixel = 0.5;

            pasGCPList[2].dfGCPLine = 0.5;
            pasGCPList[2].dfGCPPixel = nRasterXSize - 0.5;

            pasGCPList[3].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[3].dfGCPPixel = nRasterXSize - 0.5;
            break;
        case CEOS_STD_MAPREC_GCP_ORDER:
            pasGCPList[1].dfGCPLine = 0.5;
            pasGCPList[1].dfGCPPixel = nRasterXSize - 0.5;

            pasGCPList[2].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[2].dfGCPPixel = nRasterXSize - 0.5;

            pasGCPList[3].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[3].dfGCPPixel = 0.5;
            break;
        case CEOS_ASF_MAPREC_GCP_ORDER:
            pasGCPList[0].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[0].dfGCPPixel = 0.5;

            pasGCPList[1].dfGCPLine = nRasterYSize - 0.5;
            pasGCPList[1].dfGCPPixel = nRasterXSize - 0.5;

            pasGCPList[2].dfGCPLine = 0.5;
            pasGCPList[2].dfGCPPixel = nRasterXSize - 0.5;

            pasGCPList[3].dfGCPLine = 0.5;
            pasGCPList[3].dfGCPPixel = 0.5;
            break;
    }

    return TRUE;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void SAR_CEOSDataset::ScanForGCPs()

{
    m_bHasScannedForGCP = true;

    /* -------------------------------------------------------------------- */
    /*      Do we have a standard 180 bytes of prefix data (192 bytes       */
    /*      including the record marker information)?  If not, it is        */
    /*      unlikely that the GCPs are available.                           */
    /* -------------------------------------------------------------------- */
    if (sVolume.ImageDesc.ImageDataStart < 192)
    {
        ScanForMapProjection();
        return;
    }

    /* ASF L1 products do not have valid data
       in the lat/long first/mid/last fields */
    const char *pszValue = GetMetadataItem("CEOS_FACILITY");
    if ((pszValue != nullptr) && (strncmp(pszValue, "ASF", 3) == 0))
    {
        ScanForMapProjection();
        return;
    }

    if (GetRasterBand(1)->GetRasterDataType() == GDT_CFloat32)
    {
        const char *pszVolSetId = GetMetadataItem("CEOS_VOLSET_ID");
        if (pszVolSetId && (STARTS_WITH(pszVolSetId, "ALOS2  SAR") ||
                            STARTS_WITH(pszVolSetId, "ALOS4  SAR")))
        {
            // No GCPs in those products. Helps for performance when
            // reading in zip archives (particularly on network)
            return;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Just sample fix scanlines through the image for GCPs, to        */
    /*      return 15 GCPs.  That is an adequate coverage for most          */
    /*      purposes.  A GCP is collected from the beginning, middle and    */
    /*      end of each scanline.                                           */
    /* -------------------------------------------------------------------- */
    nGCPCount = 0;
    int nGCPMax = 15;
    pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP), nGCPMax);

    int nStep = (GetRasterYSize() - 1) / (nGCPMax / 3 - 1);
    for (int iScanline = 0; iScanline < GetRasterYSize(); iScanline += nStep)
    {
        if (nGCPCount > nGCPMax - 3)
            break;

        uint64_t nFileOffset;
        CalcCeosSARImageFilePosition(&sVolume, 1, iScanline + 1, nullptr,
                                     &nFileOffset);

        GInt32 anRecord[192 / 4];
        if (VSIFSeekL(fpImage, nFileOffset, SEEK_SET) != 0 ||
            VSIFReadL(anRecord, 1, 192, fpImage) != 192)
            break;

        /* loop over first, middle and last pixel gcps */

        for (int iGCP = 0; iGCP < 3; iGCP++)
        {
            const int nLat = CPL_MSBWORD32(anRecord[132 / 4 + iGCP]);
            const int nLong = CPL_MSBWORD32(anRecord[144 / 4 + iGCP]);

            if (nLat != 0 || nLong != 0)
            {
                GDALInitGCPs(1, pasGCPList + nGCPCount);

                CPLFree(pasGCPList[nGCPCount].pszId);

                char szId[32];
                snprintf(szId, sizeof(szId), "%d", nGCPCount + 1);
                pasGCPList[nGCPCount].pszId = CPLStrdup(szId);

                pasGCPList[nGCPCount].dfGCPX = nLong / 1000000.0;
                pasGCPList[nGCPCount].dfGCPY = nLat / 1000000.0;
                pasGCPList[nGCPCount].dfGCPZ = 0.0;

                pasGCPList[nGCPCount].dfGCPLine = iScanline + 0.5;

                if (iGCP == 0)
                    pasGCPList[nGCPCount].dfGCPPixel = 0.5;
                else if (iGCP == 1)
                    pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize() / 2.0;
                else
                    pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize() - 0.5;

                nGCPCount++;
            }
        }
    }
    /* If general GCP's were not found, look for Map Projection (e.g. JERS) */
    if (nGCPCount == 0)
    {
        CPLFree(pasGCPList);
        pasGCPList = nullptr;
        ScanForMapProjection();
        return;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAR_CEOSDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Does this appear to be a valid ceos leader record?              */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < CEOS_HEADER_LENGTH ||
        poOpenInfo->fpL == nullptr)
        return nullptr;

    if ((poOpenInfo->pabyHeader[4] != 0x3f &&
         poOpenInfo->pabyHeader[4] != 0x32) ||
        poOpenInfo->pabyHeader[5] != 0xc0 ||
        poOpenInfo->pabyHeader[6] != 0x12 || poOpenInfo->pabyHeader[7] != 0x12)
        return nullptr;

    // some products (#1862) have byte swapped record length/number
    // values and will blow stuff up -- explicitly ignore if record index
    // value appears to be little endian.
    if (poOpenInfo->pabyHeader[0] != 0)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("SAR_CEOS");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    auto poDS = std::make_unique<SAR_CEOSDataset>();
    std::swap(poDS->fpImage, poOpenInfo->fpL);

    CeosSARVolume_t *psVolume = &(poDS->sVolume);
    InitCeosSARVolume(psVolume, 0);

    /* -------------------------------------------------------------------- */
    /*      Try to read the current file as an imagery file.                */
    /* -------------------------------------------------------------------- */

    psVolume->ImagryOptionsFile = TRUE;
    if (ProcessData(poDS->fpImage, CEOS_IMAGRY_OPT_FILE, psVolume,
                    /* max_records = */ 4, VSI_L_OFFSET_MAX, false) != CE_None)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try the various filenames.                                      */
    /* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup(CPLGetPathSafe(poOpenInfo->pszFilename).c_str());
    char *pszBasename =
        CPLStrdup(CPLGetBasenameSafe(poOpenInfo->pszFilename).c_str());
    char *pszExtension =
        CPLStrdup(CPLGetExtensionSafe(poOpenInfo->pszFilename).c_str());

    int nBand;
    if (strlen(pszBasename) > 4)
        nBand = atoi(pszBasename + 4);
    else
        nBand = 0;

    const bool bIsALOS2Or4 =
        strlen(pszBasename) >= strlen("IMG-HH-ALOS2") &&
        EQUALN(pszBasename, "IMG-", strlen("IMG-")) &&
        (EQUALN(pszBasename + strlen("IMG-HH"), "-ALOS2", strlen("-ALOS2")) ||
         EQUALN(pszBasename + strlen("IMG-HH"), "-ALOS4", strlen("-ALOS4")));
    if (bIsALOS2Or4 && strlen(pszExtension) >= 3 &&
        pszExtension[strlen(pszExtension) - 3] == '-' &&
        pszExtension[strlen(pszExtension) - 2] == 'F')
    {
        pszExtension[strlen(pszExtension) - 3] = 0;
    }

    for (int iFile = 0; iFile < CEOS_FILE_COUNT; iFile++)
    {
        /* skip image file ... we already did it */
        if (iFile == CEOS_IMAGRY_OPT_FILE)
            continue;

        for (int e = 0; CeosExtension[e][iFile] != nullptr; ++e)
        {
            std::string osFilename;

            const char *const pszMethod = CeosExtension[e][CEOS_FILE_COUNT];
            const char *const pszFilePart = CeosExtension[e][iFile];

            /* build filename */
            if (EQUAL(pszMethod, "base"))
            {
                char szMadeBasename[32];

                snprintf(szMadeBasename, sizeof(szMadeBasename), pszFilePart,
                         nBand);
                osFilename =
                    CPLFormFilenameSafe(pszPath, szMadeBasename, pszExtension);
            }
            else if (EQUAL(pszMethod, "ext"))
            {
                osFilename =
                    CPLFormFilenameSafe(pszPath, pszBasename, pszFilePart);
            }
            else if (EQUAL(pszMethod, "whole"))
            {
                osFilename = CPLFormFilenameSafe(pszPath, pszFilePart, "");
            }

            // This is for SAR SLC as per the SAR Toolbox (from ASF).
            else if (EQUAL(pszMethod, "ext2"))
            {
                char szThisExtension[32];

                if (strlen(pszExtension) > 3)
                    snprintf(szThisExtension, sizeof(szThisExtension), "%s%s",
                             pszFilePart, pszExtension + 3);
                else
                    snprintf(szThisExtension, sizeof(szThisExtension), "%s",
                             pszFilePart);

                osFilename =
                    CPLFormFilenameSafe(pszPath, pszBasename, szThisExtension);
            }

            else if (EQUAL(pszMethod, "ALOS2-ALOS4"))
            {
                if (bIsALOS2Or4 && CeosExtension[e][iFile][0] != 0)
                {
                    osFilename = CPLFormFilenameSafe(
                        pszPath,
                        std::string(pszFilePart)
                            .append(pszBasename + strlen("IMG-HH"))
                            .c_str(),
                        pszExtension);
                }
                else
                {
                    continue;
                }
            }

            else
            {
                CPLError(CE_Fatal, CPLE_AppDefined, "should not happen");
            }

            /* try to open */
            VSILFILE *process_fp = VSIFOpenL(osFilename.c_str(), "rb");

            /* try upper case */
            if (process_fp == nullptr)
            {
                for (int i = static_cast<int>(osFilename.size()) - 1;
                     i >= 0 && osFilename[i] != '/' && osFilename[i] != '\\';
                     i--)
                {
                    if (osFilename[i] >= 'a' && osFilename[i] <= 'z')
                        osFilename[i] = osFilename[i] - 'a' + 'A';
                }

                process_fp = VSIFOpenL(osFilename.c_str(), "rb");
            }

            if (process_fp != nullptr)
            {
                CPLDebug("CEOS", "Opened %s.\n", osFilename.c_str());

                poDS->papszExtraFiles =
                    CSLAddString(poDS->papszExtraFiles, osFilename.c_str());

                CPL_IGNORE_RET_VAL(VSIFSeekL(process_fp, 0, SEEK_END));
                const bool bSilentWrongRecordNumber =
                    bIsALOS2Or4 && iFile == CEOS_TRAILER_FILE;
                if (ProcessData(process_fp, iFile, psVolume, -1,
                                VSIFTellL(process_fp),
                                bSilentWrongRecordNumber) == CE_None)
                {
                    switch (iFile)
                    {
                        case CEOS_VOLUME_DIR_FILE:
                            psVolume->VolumeDirectoryFile = TRUE;
                            break;
                        case CEOS_LEADER_FILE:
                            psVolume->SARLeaderFile = TRUE;
                            break;
                        case CEOS_TRAILER_FILE:
                            psVolume->SARTrailerFile = TRUE;
                            break;
                        case CEOS_NULL_VOL_FILE:
                            psVolume->NullVolumeDirectoryFile = TRUE;
                            break;
                    }

                    CPL_IGNORE_RET_VAL(VSIFCloseL(process_fp));
                    break; /* Exit the while loop, we have this data type*/
                }

                CPL_IGNORE_RET_VAL(VSIFCloseL(process_fp));
            }
        }
    }

    CPLFree(pszPath);
    CPLFree(pszBasename);
    CPLFree(pszExtension);

    /* -------------------------------------------------------------------- */
    /*      Check that we have an image description.                        */
    /* -------------------------------------------------------------------- */
    GetCeosSARImageDesc(psVolume);
    struct CeosSARImageDesc *psImageDesc = &(psVolume->ImageDesc);
    if (!psImageDesc->ImageDescValid)
    {
        CPLDebug("CEOS",
                 "Unable to extract CEOS image description\n"
                 "from %s.",
                 poOpenInfo->pszFilename);

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Establish image type.                                           */
    /* -------------------------------------------------------------------- */
    GDALDataType eType;

    switch (psImageDesc->DataType)
    {
        case CEOS_TYP_CHAR:
        case CEOS_TYP_UCHAR:
            eType = GDT_UInt8;
            break;

        case CEOS_TYP_SHORT:
            eType = GDT_Int16;
            break;

        case CEOS_TYP_COMPLEX_SHORT:
        case CEOS_TYP_PALSAR_COMPLEX_SHORT:
            eType = GDT_CInt16;
            break;

        case CEOS_TYP_USHORT:
            eType = GDT_UInt16;
            break;

        case CEOS_TYP_LONG:
            eType = GDT_Int32;
            break;

        case CEOS_TYP_ULONG:
            eType = GDT_UInt32;
            break;

        case CEOS_TYP_FLOAT:
            eType = GDT_Float32;
            break;

        case CEOS_TYP_DOUBLE:
            eType = GDT_Float64;
            break;

        case CEOS_TYP_COMPLEX_FLOAT:
        case CEOS_TYP_CCP_COMPLEX_FLOAT:
            eType = GDT_CFloat32;
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unsupported CEOS image data type %d.\n",
                     psImageDesc->DataType);
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psImageDesc->PixelsPerLine +
                         psImageDesc->LeftBorderPixels +
                         psImageDesc->RightBorderPixels;
    poDS->nRasterYSize = psImageDesc->Lines;

    /* -------------------------------------------------------------------- */
    /*      Special case for compressed cross products.                     */
    /* -------------------------------------------------------------------- */
    if (psImageDesc->DataType == CEOS_TYP_CCP_COMPLEX_FLOAT)
    {
        for (int iBand = 0; iBand < psImageDesc->NumChannels; iBand++)
        {
            poDS->SetBand(
                poDS->nBands + 1,
                new CCPRasterBand(poDS.get(), poDS->nBands + 1, eType));
        }

        /* mark this as a Scattering Matrix product */
        if (poDS->GetRasterCount() == 4)
        {
            poDS->SetMetadataItem("MATRIX_REPRESENTATION", "SCATTERING");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for PALSAR data.                                   */
    /* -------------------------------------------------------------------- */
    else if (psImageDesc->DataType == CEOS_TYP_PALSAR_COMPLEX_SHORT)
    {
        for (int iBand = 0; iBand < psImageDesc->NumChannels; iBand++)
        {
            poDS->SetBand(poDS->nBands + 1,
                          new PALSARRasterBand(poDS.get(), poDS->nBands + 1));
        }

        /* mark this as a Symmetrized Covariance product if appropriate */
        if (poDS->GetRasterCount() == 6)
        {
            poDS->SetMetadataItem("MATRIX_REPRESENTATION",
                                  "SYMMETRIZED_COVARIANCE");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Roll our own ...                                                */
    /* -------------------------------------------------------------------- */
    else if (psImageDesc->RecordsPerLine > 1 ||
             psImageDesc->DataType == CEOS_TYP_CHAR ||
             psImageDesc->DataType == CEOS_TYP_LONG ||
             psImageDesc->DataType == CEOS_TYP_ULONG ||
             psImageDesc->DataType == CEOS_TYP_DOUBLE)
    {
        for (int iBand = 0; iBand < psImageDesc->NumChannels; iBand++)
        {
            poDS->SetBand(
                poDS->nBands + 1,
                new SAR_CEOSRasterBand(poDS.get(), poDS->nBands + 1, eType));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Use raw services for well behaved files.                        */
    /* -------------------------------------------------------------------- */
    else
    {
        uint64_t StartData;
        CalcCeosSARImageFilePosition(psVolume, 1, 1, nullptr, &StartData);

        /*StartData += psImageDesc->ImageDataStart; */

        uint64_t nLineOff1, nLineOff2;
        CalcCeosSARImageFilePosition(psVolume, 1, 1, nullptr, &nLineOff1);
        CalcCeosSARImageFilePosition(psVolume, 1, 2, nullptr, &nLineOff2);

        const int nLineSize = static_cast<int>(nLineOff2 - nLineOff1);

        for (int iBand = 0; iBand < psImageDesc->NumChannels; iBand++)
        {
            uint64_t nStartData;
            int nPixelOffset, nLineOffset;

            if (psImageDesc->ChannelInterleaving == CEOS_IL_PIXEL)
            {
                CalcCeosSARImageFilePosition(psVolume, 1, 1, nullptr,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nStartData +=
                    cpl::fits_on<int>(psImageDesc->BytesPerPixel * iBand);

                nPixelOffset =
                    psImageDesc->BytesPerPixel * psImageDesc->NumChannels;
                nLineOffset = nLineSize;
            }
            else if (psImageDesc->ChannelInterleaving == CEOS_IL_LINE)
            {
                CalcCeosSARImageFilePosition(psVolume, iBand + 1, 1, nullptr,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nPixelOffset = psImageDesc->BytesPerPixel;
                nLineOffset = nLineSize * psImageDesc->NumChannels;
            }
            else if (psImageDesc->ChannelInterleaving == CEOS_IL_BAND)
            {
                CalcCeosSARImageFilePosition(psVolume, iBand + 1, 1, nullptr,
                                             &nStartData);

                nStartData += psImageDesc->ImageDataStart;
                nPixelOffset = psImageDesc->BytesPerPixel;
                nLineOffset = nLineSize;
            }
            else
            {
                CPLAssert(false);
                return nullptr;
            }

            auto poBand = RawRasterBand::Create(
                poDS.get(), poDS->nBands + 1, poDS->fpImage, nStartData,
                nPixelOffset, nLineOffset, eType,
                RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN,
                RawRasterBand::OwnFP::NO);
            if (!poBand)
                return nullptr;
            poDS->SetBand(poDS->nBands + 1, std::move(poBand));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Collect metadata.                                               */
    /* -------------------------------------------------------------------- */
    poDS->ScanForMetadata();

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Open overviews.                                                 */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                            ProcessData()                             */
/************************************************************************/
static int ProcessData(VSILFILE *fp, int fileid, CeosSARVolume_t *sar,
                       int max_records, vsi_l_offset max_bytes,
                       bool bSilentWrongRecordNumber)

{
    unsigned char temp_buffer[CEOS_HEADER_LENGTH];
    unsigned char *temp_body = nullptr;
    vsi_l_offset start = 0;
    int CurrentBodyLength = 0;
    int CurrentType = 0;
    int CurrentSequence = 0;
    int iThisRecord = 0;

    while (max_records != 0 && max_bytes != 0)
    {
        iThisRecord++;

        if (VSIFSeekL(fp, start, SEEK_SET) != 0 ||
            VSIFReadL(temp_buffer, 1, CEOS_HEADER_LENGTH, fp) !=
                CEOS_HEADER_LENGTH)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt CEOS File - cannot read record %d.", iThisRecord);
            CPLFree(temp_body);
            return CE_Failure;
        }
        CeosRecord_t *record = (CeosRecord_t *)CPLMalloc(sizeof(CeosRecord_t));
        record->Length = DetermineCeosRecordBodyLength(temp_buffer);

        CeosToNative(&(record->Sequence), temp_buffer, 4, 4);

        if (iThisRecord != record->Sequence)
        {
            if (fileid == CEOS_IMAGRY_OPT_FILE && iThisRecord == 2)
            {
                CPLDebug("SAR_CEOS",
                         "Ignoring CEOS file with wrong second record sequence "
                         "number - likely it has padded records.");
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Warning;
            }
            else if (bSilentWrongRecordNumber && iThisRecord == 2)
            {
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Warning;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Corrupted CEOS File - got record seq# %d instead of "
                         "the expected %d.",
                         record->Sequence, iThisRecord);
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Failure;
            }
        }

        if (record->Length <= CEOS_HEADER_LENGTH)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt CEOS File - cannot read record %d.", iThisRecord);
            CPLFree(record);
            CPLFree(temp_body);
            return CE_Failure;
        }

        if (record->Length > CurrentBodyLength)
        {
            unsigned char *temp_body_new =
                (unsigned char *)VSI_REALLOC_VERBOSE(temp_body, record->Length);
            if (temp_body_new == nullptr)
            {
                CPLFree(record);
                CPLFree(temp_body);
                return CE_Failure;
            }
            temp_body = temp_body_new;
            CurrentBodyLength = record->Length;
        }

        int nToRead = record->Length - CEOS_HEADER_LENGTH;
        if ((int)VSIFReadL(temp_body, 1, nToRead, fp) != nToRead)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt CEOS File - cannot read record %d.", iThisRecord);
            CPLFree(record);
            CPLFree(temp_body);
            return CE_Failure;
        }

        InitCeosRecordWithHeader(record, temp_buffer, temp_body);
        if (record->Length == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt CEOS File - invalid record %d.", iThisRecord);
            CPLFree(record);
            CPLFree(temp_body);
            return CE_Failure;
        }

        if (CurrentType == record->TypeCode.Int32Code)
            record->Subsequence = ++CurrentSequence;
        else
        {
            CurrentType = record->TypeCode.Int32Code;
            record->Subsequence = 0;
            CurrentSequence = 0;
        }

        record->FileId = fileid;

        Link_t *TheLink = ceos2CreateLink(record);

        if (sar->RecordList == nullptr)
            sar->RecordList = TheLink;
        else
            sar->RecordList = InsertLink(sar->RecordList, TheLink);

        start += record->Length;

        if (max_records > 0)
            max_records--;
        if (max_bytes > 0)
        {
            if ((vsi_l_offset)record->Length <= max_bytes)
                max_bytes -= record->Length;
            else
            {
                CPLDebug("SAR_CEOS",
                         "Partial record found.  %d > " CPL_FRMT_GUIB,
                         record->Length, max_bytes);
                max_bytes = 0;
            }
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
    if (GDALGetDriverByName("SAR_CEOS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SAR_CEOS");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "CEOS SAR Image");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/sar_ceos.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = SAR_CEOSDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SAR_CEOSDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLInsertStrings(papszFileList, -1, papszExtraFiles);

    return papszFileList;
}
