/******************************************************************************
 *
 * Project:  ELAS Translator
 * Purpose:  Complete implementation of ELAS translator module for GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <cmath>
#include <algorithm>
#include <limits>

using std::fill;

typedef struct ELASHeader
{
    ELASHeader();

    GInt32 NBIH;     /* bytes in header, normally 1024 */
    GInt32 NBPR;     /* bytes per data record (all bands of scanline) */
    GInt32 IL;       /* initial line - normally 1 */
    GInt32 LL;       /* last line */
    GInt32 IE;       /* initial element (pixel), normally 1 */
    GInt32 LE;       /* last element (pixel) */
    GInt32 NC;       /* number of channels (bands) */
    GUInt32 H4321;   /* header record identifier - always 4321. */
    char YLabel[4];  /* Should be "NOR" for UTM */
    GInt32 YOffset;  /* topleft pixel center northing */
    char XLabel[4];  /* Should be "EAS" for UTM */
    GInt32 XOffset;  /* topleft pixel center easting */
    float YPixSize;  /* height of pixel in georef units */
    float XPixSize;  /* width of pixel in georef units */
    float Matrix[4]; /* 2x2 transformation matrix.  Should be
                        1,0,0,1 for pixel/line, or
                        1,0,0,-1 for UTM */
    GByte IH19[4];   /* data type, and size flags */
    GInt32 IH20;     /* number of secondary headers */
    char unused1[8];
    GInt32 LABL; /* used by LABL module */
    char HEAD;   /* used by HEAD module */
    char Comment1[64];
    char Comment2[64];
    char Comment3[64];
    char Comment4[64];
    char Comment5[64];
    char Comment6[64];
    GUInt16 ColorTable[256]; /* RGB packed with 4 bits each */
    char unused2[32];
} _ELASHeader;

ELASHeader::ELASHeader()
    : NBIH(0), NBPR(0), IL(0), LL(0), IE(0), LE(0), NC(0), H4321(0), YOffset(0),
      XOffset(0), YPixSize(0.0), XPixSize(0.0), IH20(0), LABL(0), HEAD(0)
{
    fill(YLabel, YLabel + CPL_ARRAYSIZE(YLabel), static_cast<char>(0));
    fill(XLabel, XLabel + CPL_ARRAYSIZE(XLabel), static_cast<char>(0));
    fill(Matrix, Matrix + CPL_ARRAYSIZE(Matrix), 0.f);
    fill(IH19, IH19 + CPL_ARRAYSIZE(IH19), static_cast<GByte>(0));
    fill(unused1, unused1 + CPL_ARRAYSIZE(unused1), static_cast<char>(0));
    fill(Comment1, Comment1 + CPL_ARRAYSIZE(Comment1), static_cast<char>(0));
    fill(Comment2, Comment2 + CPL_ARRAYSIZE(Comment2), static_cast<char>(0));
    fill(Comment3, Comment3 + CPL_ARRAYSIZE(Comment3), static_cast<char>(0));
    fill(Comment4, Comment4 + CPL_ARRAYSIZE(Comment4), static_cast<char>(0));
    fill(Comment5, Comment5 + CPL_ARRAYSIZE(Comment5), static_cast<char>(0));
    fill(Comment6, Comment6 + CPL_ARRAYSIZE(Comment6), static_cast<char>(0));
    fill(ColorTable, ColorTable + CPL_ARRAYSIZE(ColorTable),
         static_cast<GUInt16>(0));
    fill(unused2, unused2 + CPL_ARRAYSIZE(unused2), static_cast<char>(0));
}

/************************************************************************/
/* ==================================================================== */
/*                              ELASDataset                             */
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand;

class ELASDataset final : public GDALPamDataset
{
    friend class ELASRasterBand;

    VSILFILE *fp;

    ELASHeader sHeader;

    GDALDataType eRasterDataType;

    int nLineOffset;
    int nBandOffset;  // Within a line.

    double adfGeoTransform[6];

  public:
    ELASDataset();
    ~ELASDataset() override;

    CPLErr GetGeoTransform(double *) override;

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                            ELASRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand final : public GDALPamRasterBand
{
    friend class ELASDataset;

  public:
    ELASRasterBand(ELASDataset *, int);

    // should override RasterIO eventually.

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                           ELASRasterBand()                            */
/************************************************************************/

ELASRasterBand::ELASRasterBand(ELASDataset *poDSIn, int nBandIn)

{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->eAccess;

    eDataType = poDSIn->eRasterDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ELASRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                  void *pImage)
{
    CPLAssert(nBlockXOff == 0);

    ELASDataset *poGDS = (ELASDataset *)poDS;

    int nDataSize =
        GDALGetDataTypeSizeBytes(eDataType) * poGDS->GetRasterXSize();
    long nOffset =
        poGDS->nLineOffset * nBlockYOff + 1024 + (nBand - 1) * nDataSize;

    /* -------------------------------------------------------------------- */
    /*      If we can't seek to the data, we will assume this is a newly    */
    /*      created file, and that the file hasn't been extended yet.       */
    /*      Just read as zeros.                                             */
    /* -------------------------------------------------------------------- */
    if (VSIFSeekL(poGDS->fp, nOffset, SEEK_SET) != 0 ||
        VSIFReadL(pImage, 1, nDataSize, poGDS->fp) != (size_t)nDataSize)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Seek or read of %d bytes at %ld failed.\n", nDataSize,
                 nOffset);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*      ELASDataset                                                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ELASDataset()                             */
/************************************************************************/

ELASDataset::ELASDataset()
    : fp(nullptr), eRasterDataType(GDT_Unknown), nLineOffset(0), nBandOffset(0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ELASDataset()                            */
/************************************************************************/

ELASDataset::~ELASDataset()

{
    ELASDataset::FlushCache(true);

    if (fp != nullptr)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    }
}

/************************************************************************/
/*                              Identify()                               */
/************************************************************************/

int ELASDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*  First we check to see if the file has the expected header           */
    /*  bytes.                                                               */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 256)
        return FALSE;

    if (CPL_MSBWORD32(*((GInt32 *)(poOpenInfo->pabyHeader + 0))) != 1024 ||
        CPL_MSBWORD32(*((GInt32 *)(poOpenInfo->pabyHeader + 28))) != 4321)
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ELASDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    ELASDataset *poDS = new ELASDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Read the header information.                                    */
    /* -------------------------------------------------------------------- */
    if (VSIFReadL(&(poDS->sHeader), 1024, 1, poDS->fp) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Attempt to read 1024 byte header filed on file %s\n",
                 poOpenInfo->pszFilename);
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract information of interest from the header.                */
    /* -------------------------------------------------------------------- */
    poDS->nLineOffset = CPL_MSBWORD32(poDS->sHeader.NBPR);

    int nStart = CPL_MSBWORD32(poDS->sHeader.IL);
    int nEnd = CPL_MSBWORD32(poDS->sHeader.LL);
    GIntBig nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;

    if (nDiff <= 0 || nDiff > std::numeric_limits<int>::max())
    {
        delete poDS;
        return nullptr;
    }
    poDS->nRasterYSize = static_cast<int>(nDiff);

    nStart = CPL_MSBWORD32(poDS->sHeader.IE);
    nEnd = CPL_MSBWORD32(poDS->sHeader.LE);
    nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;
    if (nDiff <= 0 || nDiff > std::numeric_limits<int>::max())
    {
        delete poDS;
        return nullptr;
    }
    poDS->nRasterXSize = static_cast<int>(nDiff);

    poDS->nBands = CPL_MSBWORD32(poDS->sHeader.NC);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, FALSE))
    {
        delete poDS;
        return nullptr;
    }

    const int nELASDataType = (poDS->sHeader.IH19[2] & 0x7e) >> 2;
    const int nBytesPerSample = poDS->sHeader.IH19[3];

    if (nELASDataType == 0 && nBytesPerSample == 1)
        poDS->eRasterDataType = GDT_Byte;
    else if (nELASDataType == 1 && nBytesPerSample == 1)
        poDS->eRasterDataType = GDT_Byte;
    else if (nELASDataType == 16 && nBytesPerSample == 4)
        poDS->eRasterDataType = GDT_Float32;
    else if (nELASDataType == 17 && nBytesPerSample == 8)
        poDS->eRasterDataType = GDT_Float64;
    else
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized image data type %d, with BytesPerSample=%d.\n",
                 nELASDataType, nBytesPerSample);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Band offsets are always multiples of 256 within a multi-band    */
    /*      scanline of data.                                               */
    /* -------------------------------------------------------------------- */
    if (GDALGetDataTypeSizeBytes(poDS->eRasterDataType) >
        (std::numeric_limits<int>::max() - 256) / poDS->nRasterXSize)
    {
        delete poDS;
        return nullptr;
    }
    poDS->nBandOffset =
        (poDS->nRasterXSize * GDALGetDataTypeSizeBytes(poDS->eRasterDataType));

    if (poDS->nBandOffset > 1000000)
    {
        VSIFSeekL(poDS->fp, 0, SEEK_END);
        if (VSIFTellL(poDS->fp) < static_cast<vsi_l_offset>(poDS->nBandOffset))
        {
            CPLError(CE_Failure, CPLE_FileIO, "File too short");
            delete poDS;
            return nullptr;
        }
    }

    if (poDS->nBandOffset % 256 != 0)
    {
        poDS->nBandOffset = poDS->nBandOffset - (poDS->nBandOffset % 256) + 256;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    /* coverity[tainted_data] */
    for (int iBand = 0; iBand < poDS->nBands; iBand++)
    {
        poDS->SetBand(iBand + 1, new ELASRasterBand(poDS, iBand + 1));
    }

    /* -------------------------------------------------------------------- */
    /*      Extract the projection coordinates, if present.                 */
    /* -------------------------------------------------------------------- */
    if (poDS->sHeader.XOffset != 0)
    {
        CPL_MSBPTR32(&(poDS->sHeader.XPixSize));
        CPL_MSBPTR32(&(poDS->sHeader.YPixSize));

        poDS->adfGeoTransform[0] = (GInt32)CPL_MSBWORD32(poDS->sHeader.XOffset);
        poDS->adfGeoTransform[1] = poDS->sHeader.XPixSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = (GInt32)CPL_MSBWORD32(poDS->sHeader.YOffset);
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -1.0 * std::abs(poDS->sHeader.YPixSize);

        CPL_MSBPTR32(&(poDS->sHeader.XPixSize));
        CPL_MSBPTR32(&(poDS->sHeader.YPixSize));

        poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
        poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for external overviews.                                   */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename,
                                poOpenInfo->GetSiblingFiles());

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ELASDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_ELAS()                         */
/************************************************************************/

void GDALRegister_ELAS()

{
    if (GDALGetDriverByName("ELAS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ELAS");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ELAS");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = ELASDataset::Open;
    poDriver->pfnIdentify = ELASDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
