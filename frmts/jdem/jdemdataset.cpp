/******************************************************************************
 *
 * Project:  JDEM Reader
 * Purpose:  All code for Japanese DEM Reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <algorithm>

constexpr int HEADER_SIZE = 1011;

/************************************************************************/
/*                            JDEMGetField()                            */
/************************************************************************/

static int JDEMGetField(const char *pszField, int nWidth)

{
    char szWork[32] = {};
    CPLAssert(nWidth < static_cast<int>(sizeof(szWork)));

    strncpy(szWork, pszField, nWidth);
    szWork[nWidth] = '\0';

    return atoi(szWork);
}

/************************************************************************/
/*                            JDEMGetAngle()                            */
/************************************************************************/

static double JDEMGetAngle(const char *pszField)

{
    const int nAngle = JDEMGetField(pszField, 7);

    // Note, this isn't very general purpose, but it would appear
    // from the field widths that angles are never negative.  Nice
    // to be a country in the "first quadrant".

    const int nDegree = nAngle / 10000;
    const int nMin = (nAngle / 100) % 100;
    const int nSec = nAngle % 100;

    return nDegree + nMin / 60.0 + nSec / 3600.0;
}

/************************************************************************/
/* ==================================================================== */
/*                              JDEMDataset                             */
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand;

class JDEMDataset final : public GDALPamDataset
{
    friend class JDEMRasterBand;

    VSILFILE *m_fp = nullptr;
    GByte m_abyHeader[HEADER_SIZE];
    OGRSpatialReference m_oSRS{};

  public:
    JDEMDataset();
    ~JDEMDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);

    CPLErr GetGeoTransform(double *padfTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;
};

/************************************************************************/
/* ==================================================================== */
/*                            JDEMRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JDEMRasterBand final : public GDALPamRasterBand
{
    friend class JDEMDataset;

    int m_nRecordSize = 0;
    char *m_pszRecord = nullptr;
    bool m_bBufferAllocFailed = false;

  public:
    JDEMRasterBand(JDEMDataset *, int);
    ~JDEMRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                           JDEMRasterBand()                            */
/************************************************************************/

JDEMRasterBand::JDEMRasterBand(JDEMDataset *poDSIn, int nBandIn)
    :  // Cannot overflow as nBlockXSize <= 999.
      m_nRecordSize(poDSIn->GetRasterXSize() * 5 + 9 + 2)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          ~JDEMRasterBand()                            */
/************************************************************************/

JDEMRasterBand::~JDEMRasterBand()
{
    VSIFree(m_pszRecord);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JDEMRasterBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                  void *pImage)

{
    JDEMDataset *poGDS = cpl::down_cast<JDEMDataset *>(poDS);

    if (m_pszRecord == nullptr)
    {
        if (m_bBufferAllocFailed)
            return CE_Failure;

        m_pszRecord = static_cast<char *>(VSI_MALLOC_VERBOSE(m_nRecordSize));
        if (m_pszRecord == nullptr)
        {
            m_bBufferAllocFailed = true;
            return CE_Failure;
        }
    }

    CPL_IGNORE_RET_VAL(
        VSIFSeekL(poGDS->m_fp, 1011 + m_nRecordSize * nBlockYOff, SEEK_SET));

    if (VSIFReadL(m_pszRecord, m_nRecordSize, 1, poGDS->m_fp) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot read scanline %d",
                 nBlockYOff);
        return CE_Failure;
    }

    if (!EQUALN(reinterpret_cast<char *>(poGDS->m_abyHeader), m_pszRecord, 6))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JDEM Scanline corrupt.  Perhaps file was not transferred "
                 "in binary mode?");
        return CE_Failure;
    }

    if (JDEMGetField(m_pszRecord + 6, 3) != nBlockYOff + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JDEM scanline out of order, JDEM driver does not "
                 "currently support partial datasets.");
        return CE_Failure;
    }

    for (int i = 0; i < nBlockXSize; i++)
        static_cast<float *>(pImage)[i] =
            JDEMGetField(m_pszRecord + 9 + 5 * i, 5) * 0.1f;

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              JDEMDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            JDEMDataset()                             */
/************************************************************************/

JDEMDataset::JDEMDataset()
{
    std::fill_n(m_abyHeader, CPL_ARRAYSIZE(m_abyHeader), static_cast<GByte>(0));
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oSRS.importFromEPSG(4301);  // Tokyo geographic CRS
}

/************************************************************************/
/*                           ~JDEMDataset()                             */
/************************************************************************/

JDEMDataset::~JDEMDataset()

{
    FlushCache(true);
    if (m_fp != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(m_fp));
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JDEMDataset::GetGeoTransform(double *padfTransform)

{
    const char *psHeader = reinterpret_cast<const char *>(m_abyHeader);

    const double dfLLLat = JDEMGetAngle(psHeader + 29);
    const double dfLLLong = JDEMGetAngle(psHeader + 36);
    const double dfURLat = JDEMGetAngle(psHeader + 43);
    const double dfURLong = JDEMGetAngle(psHeader + 50);

    padfTransform[0] = dfLLLong;
    padfTransform[3] = dfURLat;
    padfTransform[1] = (dfURLong - dfLLLong) / GetRasterXSize();
    padfTransform[2] = 0.0;

    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * (dfURLat - dfLLLat) / GetRasterYSize();

    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *JDEMDataset::GetSpatialRef() const

{
    return &m_oSRS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int JDEMDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < HEADER_SIZE)
        return FALSE;

    // Confirm that the header has what appears to be dates in the
    // expected locations.
    // Check if century values seem reasonable.
    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    if ((!STARTS_WITH_CI(psHeader + 11, "19") &&
         !STARTS_WITH_CI(psHeader + 11, "20")) ||
        (!STARTS_WITH_CI(psHeader + 15, "19") &&
         !STARTS_WITH_CI(psHeader + 15, "20")) ||
        (!STARTS_WITH_CI(psHeader + 19, "19") &&
         !STARTS_WITH_CI(psHeader + 19, "20")))
    {
        return FALSE;
    }

    // Check the extent too. In particular, that we are in the first quadrant,
    // as this is only for Japan.
    const double dfLLLat = JDEMGetAngle(psHeader + 29);
    const double dfLLLong = JDEMGetAngle(psHeader + 36);
    const double dfURLat = JDEMGetAngle(psHeader + 43);
    const double dfURLong = JDEMGetAngle(psHeader + 50);
    if (dfLLLat > 90 || dfLLLat < 0 || dfLLLong > 180 || dfLLLong < 0 ||
        dfURLat > 90 || dfURLat < 0 || dfURLong > 180 || dfURLong < 0 ||
        dfLLLat > dfURLat || dfLLLong > dfURLong)
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JDEMDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Confirm that the header is compatible with a JDEM dataset.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The JDEM driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    // Check that the file pointer from GDALOpenInfo* is available.
    if (poOpenInfo->fpL == nullptr)
    {
        return nullptr;
    }

    // Create a corresponding GDALDataset.
    auto poDS = cpl::make_unique<JDEMDataset>();

    // Borrow the file pointer from GDALOpenInfo*.
    std::swap(poDS->m_fp, poOpenInfo->fpL);

    // Store the header (we have already checked it is at least HEADER_SIZE
    // byte large).
    memcpy(poDS->m_abyHeader, poOpenInfo->pabyHeader, HEADER_SIZE);

    const char *psHeader = reinterpret_cast<const char *>(poDS->m_abyHeader);
    poDS->nRasterXSize = JDEMGetField(psHeader + 23, 3);
    poDS->nRasterYSize = JDEMGetField(psHeader + 26, 3);
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    // Create band information objects.
    poDS->SetBand(1, new JDEMRasterBand(poDS.get(), 1));

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GDALRegister_JDEM()                         */
/************************************************************************/

void GDALRegister_JDEM()

{
    if (GDALGetDriverByName("JDEM") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("JDEM");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Japanese DEM (.mem)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jdem.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "mem");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = JDEMDataset::Open;
    poDriver->pfnIdentify = JDEMDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
