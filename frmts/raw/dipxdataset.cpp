/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation for ELAS DIPEx format variant.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>
#include <algorithm>

using std::fill;

/************************************************************************/
/* ==================================================================== */
/*                              DIPExDataset                            */
/* ==================================================================== */
/************************************************************************/

class DIPExDataset final : public GDALPamDataset
{
    struct DIPExHeader
    {
        GInt32 NBIH = {0};  /* bytes in header, normally 1024 */
        GInt32 NBPR = {0};  /* bytes per data record (all bands of scanline) */
        GInt32 IL = {0};    /* initial line - normally 1 */
        GInt32 LL = {0};    /* last line */
        GInt32 IE = {0};    /* initial element (pixel), normally 1 */
        GInt32 LE = {0};    /* last element (pixel) */
        GInt32 NC = {0};    /* number of channels (bands) */
        GInt32 H4322 = {0}; /* header record identifier - always 4322. */
        char unused1[40] = {0};
        GByte IH19[4] = {0}; /* data type, and size flags */
        GInt32 IH20 = {0};   /* number of secondary headers */
        GInt32 SRID = {0};
        char unused2[12] = {0};
        double YOffset = {0};
        double XOffset = {0};
        double YPixSize = {0};
        double XPixSize = {0};
        double Matrix[4] = {0};
        char unused3[344] = {0};
        GUInt16 ColorTable[256] = {0}; /* RGB packed with 4 bits each */
        char unused4[32] = {0};
    };

    VSILFILE *fp;
    OGRSpatialReference m_oSRS{};

    DIPExHeader sHeader{};

    GDALDataType eRasterDataType;

    double adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(DIPExDataset)

  public:
    DIPExDataset();
    ~DIPExDataset() override;

    CPLErr GetGeoTransform(double *) override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                             DIPExDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            DIPExDataset()                             */
/************************************************************************/

DIPExDataset::DIPExDataset() : fp(nullptr), eRasterDataType(GDT_Unknown)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~DIPExDataset()                            */
/************************************************************************/

DIPExDataset::~DIPExDataset()

{
    if (fp)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    fp = nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DIPExDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      First we check to see if the file has the expected header       */
    /*      bytes.                                                          */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 256 || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (CPL_LSBWORD32(
            *(reinterpret_cast<GInt32 *>(poOpenInfo->pabyHeader + 0))) != 1024)
        return nullptr;

    if (CPL_LSBWORD32(
            *(reinterpret_cast<GInt32 *>(poOpenInfo->pabyHeader + 28))) != 4322)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    DIPExDataset *poDS = new DIPExDataset();

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

    // To avoid cppcheck warnings about unused members
    CPL_IGNORE_RET_VAL(poDS->sHeader.NBIH);
    CPL_IGNORE_RET_VAL(poDS->sHeader.H4322);
    CPL_IGNORE_RET_VAL(poDS->sHeader.unused1);
    CPL_IGNORE_RET_VAL(poDS->sHeader.IH20);
    CPL_IGNORE_RET_VAL(poDS->sHeader.unused2);
    CPL_IGNORE_RET_VAL(poDS->sHeader.Matrix);
    CPL_IGNORE_RET_VAL(poDS->sHeader.unused3);
    CPL_IGNORE_RET_VAL(poDS->sHeader.ColorTable);
    CPL_IGNORE_RET_VAL(poDS->sHeader.unused4);

    /* -------------------------------------------------------------------- */
    /*      Extract information of interest from the header.                */
    /* -------------------------------------------------------------------- */
    const int nLineOffset = CPL_LSBWORD32(poDS->sHeader.NBPR);

    int nStart = CPL_LSBWORD32(poDS->sHeader.IL);
    int nEnd = CPL_LSBWORD32(poDS->sHeader.LL);
    GIntBig nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;
    if (nDiff <= 0 || nDiff > INT_MAX)
    {
        delete poDS;
        return nullptr;
    }
    poDS->nRasterYSize = static_cast<int>(nDiff);

    nStart = CPL_LSBWORD32(poDS->sHeader.IE);
    nEnd = CPL_LSBWORD32(poDS->sHeader.LE);
    nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;
    if (nDiff <= 0 || nDiff > INT_MAX)
    {
        delete poDS;
        return nullptr;
    }
    poDS->nRasterXSize = static_cast<int>(nDiff);

    const int nBands = CPL_LSBWORD32(poDS->sHeader.NC);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        delete poDS;
        return nullptr;
    }

    const int nDIPExDataType = (poDS->sHeader.IH19[1] & 0x7e) >> 2;
    const int nBytesPerSample = poDS->sHeader.IH19[0];

    if (nDIPExDataType == 0 && nBytesPerSample == 1)
        poDS->eRasterDataType = GDT_Byte;
    else if (nDIPExDataType == 1 && nBytesPerSample == 1)
        poDS->eRasterDataType = GDT_Byte;
    else if (nDIPExDataType == 16 && nBytesPerSample == 4)
        poDS->eRasterDataType = GDT_Float32;
    else if (nDIPExDataType == 17 && nBytesPerSample == 8)
        poDS->eRasterDataType = GDT_Float64;
    else
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized image data type %d, with BytesPerSample=%d.",
                 nDIPExDataType, nBytesPerSample);
        return nullptr;
    }

    if (nLineOffset <= 0 || nLineOffset > INT_MAX / nBands)
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid values: nLineOffset = %d, nBands = %d.", nLineOffset,
                 nBands);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    CPLErrorReset();
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        poDS->SetBand(iBand + 1,
                      new RawRasterBand(poDS, iBand + 1, poDS->fp,
                                        1024 + iBand * nLineOffset,
                                        nBytesPerSample, nLineOffset * nBands,
                                        poDS->eRasterDataType, CPL_IS_LSB,
                                        RawRasterBand::OwnFP::NO));
        if (CPLGetLastErrorType() != CE_None)
        {
            delete poDS;
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Extract the projection coordinates, if present.                 */
    /* -------------------------------------------------------------------- */
    CPL_LSBPTR64(&(poDS->sHeader.XPixSize));
    CPL_LSBPTR64(&(poDS->sHeader.YPixSize));
    CPL_LSBPTR64(&(poDS->sHeader.XOffset));
    CPL_LSBPTR64(&(poDS->sHeader.YOffset));

    if (poDS->sHeader.XOffset != 0)
    {
        poDS->adfGeoTransform[0] = poDS->sHeader.XOffset;
        poDS->adfGeoTransform[1] = poDS->sHeader.XPixSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = poDS->sHeader.YOffset;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -1.0 * std::abs(poDS->sHeader.YPixSize);

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
    /*      Look for SRID.                                                  */
    /* -------------------------------------------------------------------- */
    CPL_LSBPTR32(&(poDS->sHeader.SRID));

    if (poDS->sHeader.SRID > 0 && poDS->sHeader.SRID < 33000)
    {
        OGRSpatialReference oSR;
        oSR.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oSR.importFromEPSG(poDS->sHeader.SRID) == OGRERR_NONE)
        {
            poDS->m_oSRS = oSR;
        }
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

CPLErr DIPExDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_DIPEx()                        */
/************************************************************************/

void GDALRegister_DIPEx()

{
    if (GDALGetDriverByName("DIPEx") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DIPEx");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "DIPEx");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = DIPExDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
