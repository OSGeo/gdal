/******************************************************************************
 *
 * Project:  Sentinel SAFE products
 * Purpose:  Sentinel Products (manifest.safe) driver
 * Author:   Delfim Rego, delfimrego@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Delfim Rego <delfimrego@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef SAFEDATASET_H_INCLUDED
#define SAFEDATASET_H_INCLUDED

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include <set>
#include <map>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

/************************************************************************/
/* ==================================================================== */
/*                               SAFEDataset                            */
/* ==================================================================== */
/************************************************************************/

class SAFEDataset final : public GDALPamDataset
{
    CPLXMLTreeCloser psManifest{nullptr};

    int nGCPCount = 0;
    GDAL_GCP *pasGCPList = nullptr;
    OGRSpatialReference m_oGCPSRS{};
    char **papszSubDatasets = nullptr;
    double adfGeoTransform[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    bool bHaveGeoTransform = false;
    char **papszExtraFiles = nullptr;
    int m_nSubDSNum = 0;

  protected:
    virtual int CloseDependentDatasets() override;

    static const CPLXMLNode *GetMetaDataObject(const CPLXMLNode *,
                                               const char *);

    static const CPLXMLNode *GetDataObject(const CPLXMLNode *, const char *);
    static const CPLXMLNode *GetDataObject(const CPLXMLNode *,
                                           const CPLXMLNode *, const char *);

    void AddSubDataset(const CPLString &osName, const CPLString &osDesc);

  public:
    SAFEDataset();
    virtual ~SAFEDataset();

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual CPLErr GetGeoTransform(double *) override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;

    virtual char **GetFileList(void) override;

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);

    const CPLXMLNode *GetManifest()
    {
        return psManifest.get();
    }
};

/************************************************************************/
/* ==================================================================== */
/*                    SAFERasterBand                                    */
/* ==================================================================== */
/************************************************************************/

class SAFERasterBand final : public GDALPamRasterBand
{
    std::unique_ptr<GDALDataset> poBandFile{};

  public:
    SAFERasterBand(SAFEDataset *poDSIn, GDALDataType eDataTypeIn,
                   const CPLString &osSwath, const CPLString &osPol,
                   std::unique_ptr<GDALDataset> &&poBandFileIn);

    virtual CPLErr IReadBlock(int, int, void *) override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                    SAFESLCRasterBand                                 */
/* ==================================================================== */
/************************************************************************/

class SAFESLCRasterBand : public GDALPamRasterBand
{
  public:
    typedef enum BandType
    {
        COMPLEX = 0,
        INTENSITY
    } BandType;

    SAFESLCRasterBand(SAFEDataset *poDSIn, GDALDataType eDataTypeIn,
                      const CPLString &osSwath, const CPLString &osPol,
                      std::unique_ptr<GDALDataset> &&poBandFileIn,
                      BandType eBandType);

    virtual CPLErr IReadBlock(int, int, void *) override;
    static GDALDataset *Open(GDALOpenInfo *);

  private:
    std::unique_ptr<GDALDataset> poBandFile{};
    BandType m_eBandType = COMPLEX;
    GDALDataType m_eInputDataType = GDT_Unknown;
};

/************************************************************************/
/* ==================================================================== */
/*                   SAFECalibratedRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class SAFECalibratedRasterBand : public GDALPamRasterBand
{
  public:
    typedef enum CalibrationType
    {
        SIGMA_NOUGHT = 0,
        BETA_NOUGHT,
        GAMMA
    } CalibrationType;

    SAFECalibratedRasterBand(SAFEDataset *poDSIn, GDALDataType eDataTypeIn,
                             const CPLString &osSwath,
                             const CPLString &osPolarization,
                             std::unique_ptr<GDALDataset> &&poBandDatasetIn,
                             const char *pszCalibrationFilename,
                             CalibrationType eCalibrationType);

    virtual CPLErr IReadBlock(int, int, void *) override;

    bool ReadLUT();

    static GDALDataset *Open(GDALOpenInfo *);

  private:
    typedef std::chrono::system_clock::time_point TimePoint;

    std::unique_ptr<GDALDataset> poBandDataset{};
    GDALDataType m_eInputDataType = GDT_Unknown;
    std::vector<float> m_afTable;
    CPLString m_osCalibrationFilename;
    std::vector<int> m_anLineLUT;
    std::vector<int> m_anPixelLUT;
    TimePoint m_oStartTimePoint;
    TimePoint m_oStopTimePoint;
    int m_nNumPixels = 0;
    CPLStringList m_oAzimuthList;
    CalibrationType m_eCalibrationType = SIGMA_NOUGHT;

    static TimePoint getTimePoint(const char *pszTime);
    static double getTimeDiff(TimePoint oT1, TimePoint oT2);
    static TimePoint getazTime(TimePoint oStart, TimePoint oStop,
                               long nNumOfLines, int nOffset);

    int getCalibrationVectorIndex(int nLineNo);
    int getPixelIndex(int nPixelNo);
};

#endif
