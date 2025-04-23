/******************************************************************************
 *
 * Project:  MSG Driver
 * Purpose:  GDALDataset driver for MSG translator for read support.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gdal_priv.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "msgcommand.h"

#include <string>
#include <fstream>

/************************************************************************/
/*                            MSGRasterBand                             */
/************************************************************************/

class MSGDataset;
class ReflectanceCalculator;

class MSGRasterBand final : public GDALRasterBand
{
    friend class MSGDataset;

  public:
    MSGRasterBand(MSGDataset *, int);
    virtual ~MSGRasterBand();
    virtual CPLErr IReadBlock(int, int, void *) override;

  private:
    double rRadiometricCorrection(unsigned int iDN, int iChannel, int iRow,
                                  int iCol, MSGDataset *poGDS) const;
    bool fScanNorth;
    int iLowerShift;  // nr of pixels that lower HRV image is shifted compared
                      // to upper
    int iSplitLine;   // line from top where the HRV image splits
    int iLowerWestColumnPlanned;
    int iSatellite;  // satellite number 1,2,3,4 for MSG1, MSG2, MSG3 and MSG4
    ReflectanceCalculator *m_rc;
    static const double rRTOA[12];
};

/************************************************************************/
/*                      MSGDataset                                       */
/************************************************************************/
class MSGDataset final : public GDALDataset
{
    friend class MSGRasterBand;

  public:
    MSGDataset();
    virtual ~MSGDataset();

    static GDALDataset *Open(GDALOpenInfo *);

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return &m_oSRS;
    }

    virtual CPLErr GetGeoTransform(double *padfTransform) override;

  private:
    MSGCommand command;
    double adfGeoTransform[6];  // Calculate and store once as GetGeoTransform
                                // may be called multiple times
    OGRSpatialReference m_oSRS{};
    OGRSpatialReference oLL;
    OGRCoordinateTransformation *poTransform = nullptr;
    double rCalibrationOffset[12];
    double rCalibrationSlope[12];
    int iCurrentSatellite =
        0;  // satellite number 1,2,3,4 for MSG1, MSG2, MSG3 and MSG4
    static int iCurrentSatelliteHint;  // hint for satellite number 1,2,3,4 for
                                       // MSG1, MSG2, MSG3 and MSG4
    static const double rCentralWvl[12];
    static const double rVc[12];
    static const double rA[12];
    static const double rB[12];
    static const int iCentralPixelVIS_IR;
    static const int iCentralPixelHRV;
    static const char *metadataDomain;
};
