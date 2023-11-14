/******************************************************************************
 *
 * Project:  GSC Geogrid format driver.
 * Purpose:  Implements support for reading and writing GSC Geogrid format.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "rawdataset.h"

#include <algorithm>

/************************************************************************/
/* ==================================================================== */
/*                              GSCDataset                              */
/* ==================================================================== */
/************************************************************************/

class GSCDataset final : public RawDataset
{
    VSILFILE *fpImage;  // image data file.

    double adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(GSCDataset)

    CPLErr Close() override;

  public:
    GSCDataset();
    ~GSCDataset();

    CPLErr GetGeoTransform(double *padfTransform) override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                            GSCDataset()                             */
/************************************************************************/

GSCDataset::GSCDataset() : fpImage(nullptr)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~GSCDataset()                             */
/************************************************************************/

GSCDataset::~GSCDataset()

{
    GSCDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr GSCDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (GSCDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (fpImage)
        {
            if (VSIFCloseL(fpImage) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
                eErr = CE_Failure;
            }
        }

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GSCDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GSCDataset::Open(GDALOpenInfo *poOpenInfo)

{

    /* -------------------------------------------------------------------- */
    /*      Does this plausible look like a GSC Geogrid file?               */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 20)
        return nullptr;

    if (poOpenInfo->pabyHeader[12] != 0x02 ||
        poOpenInfo->pabyHeader[13] != 0x00 ||
        poOpenInfo->pabyHeader[14] != 0x00 ||
        poOpenInfo->pabyHeader[15] != 0x00)
        return nullptr;

    int nRecordLen =
        CPL_LSBWORD32(reinterpret_cast<GInt32 *>(poOpenInfo->pabyHeader)[0]);
    const int nPixels =
        CPL_LSBWORD32(reinterpret_cast<GInt32 *>(poOpenInfo->pabyHeader)[1]);
    const int nLines =
        CPL_LSBWORD32(reinterpret_cast<GInt32 *>(poOpenInfo->pabyHeader)[2]);

    if (nPixels < 1 || nLines < 1 || nPixels > 100000 || nLines > 100000)
        return nullptr;

    if (nRecordLen != nPixels * 4)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The GSC driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    nRecordLen += 8;  // For record length markers.

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<GSCDataset>();

    poDS->nRasterXSize = nPixels;
    poDS->nRasterYSize = nLines;
    std::swap(poDS->fpImage, poOpenInfo->fpL);

    /* -------------------------------------------------------------------- */
    /*      Read the header information in the second record.               */
    /* -------------------------------------------------------------------- */
    float afHeaderInfo[8] = {0.0};

    if (VSIFSeekL(poDS->fpImage, nRecordLen + 12, SEEK_SET) != 0 ||
        VSIFReadL(afHeaderInfo, sizeof(float), 8, poDS->fpImage) != 8)
    {
        CPLError(
            CE_Failure, CPLE_FileIO,
            "Failure reading second record of GSC file with %d record length.",
            nRecordLen);
        return nullptr;
    }

    for (int i = 0; i < 8; i++)
    {
        CPL_LSBPTR32(afHeaderInfo + i);
    }

    poDS->adfGeoTransform[0] = afHeaderInfo[2];
    poDS->adfGeoTransform[1] = afHeaderInfo[0];
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = afHeaderInfo[5];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -afHeaderInfo[1];

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    auto poBand = RawRasterBand::Create(
        poDS.get(), 1, poDS->fpImage, nRecordLen * 2 + 4, sizeof(float),
        nRecordLen, GDT_Float32, RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN,
        RawRasterBand::OwnFP::NO);
    if (!poBand)
        return nullptr;
    poBand->SetNoDataValue(-1.0000000150474662199e+30);
    poDS->SetBand(1, std::move(poBand));

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GDALRegister_GSC()                          */
/************************************************************************/

void GDALRegister_GSC()

{
    if (GDALGetDriverByName("GSC") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GSC");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GSC Geogrid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gsc.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = GSCDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
