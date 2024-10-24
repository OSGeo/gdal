/******************************************************************************
 *
 * Project:  DRDC Ottawa GEOINT
 * Purpose:  Radarsat Constellation Mission - XML Products (product.xml) driver
 * Author:   Roberto Caron, MDA
 *           on behalf of DRDC Ottawa
 *
 ******************************************************************************
 * Copyright (c) 2020, DRDC Ottawa
 *
 * Based on the RS2 Dataset Class
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_RCM_H_INCLUDED
#define GDAL_RCM_H_INCLUDED

#include "gdal_pam.h"

typedef enum eCalibration_t
{
    Sigma0 = 0,
    Gamma,
    Beta0,
    Uncalib,
    None
} eCalibration;

/************************************************************************/
/* ==================================================================== */
/*                               RCMDataset                             */
/* ==================================================================== */
/************************************************************************/

class RCMDataset final : public GDALPamDataset
{
    CPLXMLTreeCloser psProduct{nullptr};

    int nGCPCount = 0;
    GDAL_GCP *pasGCPList = nullptr;
    OGRSpatialReference m_oSRS{};
    OGRSpatialReference m_oGCPSRS{};
    char **papszSubDatasets = nullptr;
    double adfGeoTransform[6];
    bool bHaveGeoTransform = false;
    bool bPerPolarizationScaling = false;
    bool isComplexData = false;
    int magnitudeBits = 16;
    int realBitsComplexData = 32;
    int imaginaryBitsComplexData = 32;
    char **papszExtraFiles = nullptr;
    double *m_nfIncidenceAngleTable = nullptr;
    int m_IncidenceAngleTableSize = 0;

    CPL_DISALLOW_COPY_ASSIGN(RCMDataset)

  protected:
    virtual int CloseDependentDatasets() override;

  public:
    RCMDataset();
    virtual ~RCMDataset();

    virtual int GetGCPCount() override;

    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(double *) override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual char **GetFileList(void) override;

    static GDALDataset *Open(GDALOpenInfo *);

    CPLXMLNode *GetProduct()
    {
        return psProduct.get();
    }

    /* If False, this is Magnitude,   True, Complex data with Real and
     * Imaginary*/
    bool IsComplexData()
    {
        return isComplexData;
    }

    /* These 2 variables are used in case of Complex Data */
    int GetRealBitsComplexData()
    {
        return realBitsComplexData;
    }

    int GetImaginaryBitsComplexData()
    {
        return imaginaryBitsComplexData;
    }

    /* This variable is used in case of Magnitude */
    int GetMagnitudeBits()
    {
        return magnitudeBits;
    }

    /* This variable is used to hold the Incidence Angle */
    double *GetIncidenceAngle()
    {
        return m_nfIncidenceAngleTable;
    }

    /* This variable is used to hold the Incidence Angle Table Size */
    int GetIncidenceAngleSize()
    {
        return m_IncidenceAngleTableSize;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                          RCMRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class RCMRasterBand final : public GDALPamRasterBand
{
  private:
    eCalibration m_eCalib = eCalibration::Uncalib;
    GDALDataset *poBandFile = nullptr;
    RCMDataset *poRCMDataset = nullptr;
    GDALDataset *m_poBandDataset = nullptr;

    double *m_nfTable = nullptr;
    int m_nTableSize = 0;
    double m_nfOffset = 0;
    char *m_pszLUTFile = nullptr;
    int pixelFirstLutValue = 0;
    int stepSize = 0;
    int numberOfValues = 0;
    GDALRasterBand *poBand = nullptr;

    // 2 bands representing I+Q -> one complex band
    // otherwise poBandFile is passed straight through
    bool twoBandComplex = false;

    bool isOneFilePerPol = false;
    bool isNITF = false;

    CPL_DISALLOW_COPY_ASSIGN(RCMRasterBand)

  public:
    RCMRasterBand(RCMDataset *poDSIn, int nBandIn, GDALDataType eDataTypeIn,
                  const char *pszPole, GDALDataset *poBandFile,
                  bool bTwoBandComplex, bool isOneFilePerPol, bool isNITF);

    virtual ~RCMRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                         RCMCalibRasterBand                           */
/* ==================================================================== */
/************************************************************************/
/* Returns data that has been calibrated to sigma nought, gamma         */
/* or beta nought.                                                      */
/************************************************************************/

class RCMCalibRasterBand final : public GDALPamRasterBand
{
  private:
    eCalibration m_eCalib = eCalibration::Uncalib;
    GDALDataset *m_poBandDataset = nullptr;
    /* data type that used to be before transformation */
    GDALDataType m_eOriginalType = GDT_Unknown;

    double *m_nfTable = nullptr;
    int m_nTableSize = 0;
    double m_nfOffset = 0;
    char *m_pszLUTFile = nullptr;
    int pixelFirstLutValue = 0;
    int stepSize = 0;
    int numberOfValues = 0;

    char *m_pszNoiseLevelsFile = nullptr;
    double *m_nfTableNoiseLevels = nullptr;
    int pixelFirstLutValueNoiseLevels = 0;
    int stepSizeNoiseLevels = 0;
    int numberOfValuesNoiseLevels = 0;
    int m_nTableNoiseLevelsSize = 0;

    void ReadLUT();
    void ReadNoiseLevels();

    CPL_DISALLOW_COPY_ASSIGN(RCMCalibRasterBand)

  public:
    RCMCalibRasterBand(RCMDataset *poDataset, const char *pszPolarization,
                       GDALDataType eType, GDALDataset *poBandDataset,
                       eCalibration eCalib, const char *pszLUT,
                       const char *pszNoiseLevels, GDALDataType eOriginalType);
    ~RCMCalibRasterBand();

    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;
};

#endif /* ndef GDAL_RCM_H_INCLUDED */
