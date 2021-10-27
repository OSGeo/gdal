/******************************************************************************
 *
 * Project:  Sentinel SAFE products
 * Purpose:  Sentinel Products (manifest.safe) driver
 * Author:   Delfim Rego, delfimrego@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Delfim Rego <delfimrego@gmail.com>
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

class SAFEDataset final: public GDALPamDataset
{
    CPLXMLNode *psManifest = nullptr;

    int           nGCPCount = 0;
    GDAL_GCP     *pasGCPList = nullptr;
    char         *pszGCPProjection = nullptr;
    char        **papszSubDatasets = nullptr;
    char         *pszProjection = nullptr;
    double        adfGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    bool          bHaveGeoTransform = false;
    char        **papszExtraFiles = nullptr;

  protected:
    virtual int         CloseDependentDatasets() override;

    static CPLXMLNode * GetMetaDataObject(CPLXMLNode *, const char *);

    static CPLXMLNode * GetDataObject(CPLXMLNode *, const char *);
    static CPLXMLNode * GetDataObject(CPLXMLNode *, CPLXMLNode *, const char *);

    static void AddSubDataset(SAFEDataset *poDS, int iDSNum,
                              const CPLString &osName, const CPLString &osDesc);

  public:
    SAFEDataset() = default;
    virtual ~SAFEDataset();

    virtual int    GetGCPCount() override;
    virtual const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    virtual const GDAL_GCP *GetGCPs() override;

    virtual const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual CPLErr GetGeoTransform( double * ) override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char * pszDomain = "" ) override;

    virtual char **GetFileList(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLXMLNode *GetManifest() { return psManifest; }
};

/************************************************************************/
/* ==================================================================== */
/*                    SAFERasterBand                                    */
/* ==================================================================== */
/************************************************************************/

class SAFERasterBand final: public GDALPamRasterBand
{
    std::unique_ptr<GDALDataset> poBandFile{};

  public:
             SAFERasterBand(SAFEDataset *poDSIn,
                            GDALDataType eDataTypeIn,
                            const CPLString &osSwath,
                            const CPLString &osPol,
                            std::unique_ptr<GDALDataset>&& poBandFileIn);

    virtual CPLErr IReadBlock( int, int, void * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
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
                      std::unique_ptr<GDALDataset>&& poBandFileIn, BandType eBandType);

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
                             const CPLString &osSwath, const CPLString &osPolarization,
                             std::unique_ptr<GDALDataset>&& poBandDatasetIn,
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

    static TimePoint getTimePoint(const char * pszTime);
    double getTimeDiff(TimePoint oT1, TimePoint oT2);
    TimePoint getazTime(TimePoint oStart, TimePoint oStop, long nNumOfLines, int nOffset);

    int getCalibrationVectorIndex(int nLineNo);
    int getPixelIndex(int nPixelNo);
};

#endif
