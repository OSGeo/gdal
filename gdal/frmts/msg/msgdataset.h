/******************************************************************************
 * $Id$
 *
 * Project:  MSG Driver
 * Purpose:  GDALDataset driver for MSG translator for read support.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
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
 ******************************************************************************/

#include "gdal_priv.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "msgcommand.h"

#include <string>
#include <fstream>

CPL_C_START
void  GDALRegister_MSG(void);
CPL_C_END

/************************************************************************/
/*                            MSGRasterBand                             */
/************************************************************************/

class MSGDataset;
class ReflectanceCalculator;

class MSGRasterBand : public GDALRasterBand
{
  friend class MSGDataset;

  public:
    MSGRasterBand( MSGDataset *, int );
    virtual ~MSGRasterBand();
    virtual CPLErr IReadBlock( int, int, void * );

  private:
    double rRadiometricCorrection(unsigned int iDN, int iChannel, int iRow, int iCol, MSGDataset* poGDS);
    bool fScanNorth;
    int iLowerShift; // nr of pixels that lower HRV image is shifted compared to upper
    int iSplitLine; // line from top where the HRV image splits
    int iLowerWestColumnPlanned;
    int iSatellite; // satellite number 1,2,3,4 for MSG1, MSG2, MSG3 and MSG4
    ReflectanceCalculator* m_rc;
    static const double rRTOA[12];
};

/************************************************************************/
/*                      MSGDataset                                       */
/************************************************************************/
class MSGDataset : public GDALDataset
{
  friend class MSGRasterBand;
  
  public:
    MSGDataset();
    ~MSGDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * padfTransform );

  private:
    MSGCommand command;
    double adfGeoTransform[6]; // Calculate and store once as GetGeoTransform may be called multiple times
    char   *pszProjection;
    OGRSpatialReference oSRS;
    OGRSpatialReference oLL;
    OGRCoordinateTransformation *poTransform;
    double rCalibrationOffset[12];
    double rCalibrationSlope[12];
    static int iCurrentSatellite; // satellite number 1,2,3,4 for MSG1, MSG2, MSG3 and MSG4
    static const double rCentralWvl[12];
    static const double rVc[12];
    static const double rA[12];
    static const double rB[12];
};

