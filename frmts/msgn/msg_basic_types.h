/******************************************************************************
 * $Id$
 *
 * Project:  MSG Native Reader
 * Purpose:  Basic types implementation.
 * Author:   Frans van den Bergh, fvdbergh@csir.co.za
 *
 ******************************************************************************
 * Copyright (c) 2005, Frans van den Bergh <fvdbergh@csir.co.za>
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

#ifndef MSG_BASIC_TYPES
#define MSG_BASIC_TYPES

#include <math.h>

namespace msg_native_format
{

const unsigned int SATELLITESTATUS_RECORD_LENGTH = 60134;
const unsigned int IMAGEACQUISITION_RECORD_LENGTH = 700;
const unsigned int CELESTIALEVENTS_RECORD_LENGTH =
    326058;  // should be 56258 according to ICD105 ??
const unsigned int IMAGEDESCRIPTION_RECORD_LENGTH = 101;

const unsigned int RADIOMETRICPROCESSING_RECORD_OFFSET =
    SATELLITESTATUS_RECORD_LENGTH + IMAGEACQUISITION_RECORD_LENGTH +
    CELESTIALEVENTS_RECORD_LENGTH + IMAGEDESCRIPTION_RECORD_LENGTH;

typedef int INTEGER;            // 32 bits
typedef unsigned int UNSIGNED;  // 32 bits
typedef unsigned short USHORT;  // 16 bits
typedef unsigned char TIME_CDS_EXPANDED[10];
typedef unsigned char EBYTE;  // enumerated byte
typedef unsigned char UBYTE;  // enumerated byte
typedef float REAL;

typedef unsigned short int GP_SC_ID;  // 16 bits, enumerated
typedef unsigned char GP_SC_CHAN_ID;  // 8 bits,  enumerated
typedef unsigned char GP_FAC_ID;      // 8 bits,  enumerated
typedef unsigned char GP_FAC_ENV;     // 8 bits,  enumerated
typedef unsigned int GP_SU_ID;        // 32 bits, interval partition
typedef unsigned char GP_SVCE_TYPE;   // 8 bits, enumerated

// all structures must be packed on byte boundaries
#pragma pack(1)

typedef struct
{
    USHORT day;
    UNSIGNED ms;
} TIME_CDS_SHORT;

typedef struct
{
    unsigned char qualifier1;
    unsigned char qualifier2;
    unsigned char qualifier3;
    unsigned char qualifier4;
} GP_CPU_ID;

typedef struct
{
    char name[30];
    char value[50];
} PH_DATA;

typedef struct
{
    char name[30];
    char size[16];
    char address[16];
} PH_DATA_ID;

// 1.1.3.3 in MSG_native_format_1_5.doc
typedef struct
{
    PH_DATA formatName;
    PH_DATA formatDocumentName;
    PH_DATA formatDocumentMajorVersion;
    PH_DATA formatDocumentMinorVersion;
    PH_DATA creationDateTime;
    PH_DATA creatingCentre;
    PH_DATA_ID dataSetIdentification[5];
    UBYTE slack[1364];  // what is this? This is not in the documentation?
    PH_DATA totalFileSize;
    PH_DATA gort;
    PH_DATA asti;
    PH_DATA llos;
    PH_DATA snit;
    PH_DATA aiid;
    PH_DATA ssbt;
    PH_DATA ssst;
    PH_DATA rrcc;
    PH_DATA rrbt;
    PH_DATA rrst;
    PH_DATA pprc;
    PH_DATA ppdt;
    PH_DATA gplv;
    PH_DATA apnm;
    PH_DATA aarf;
    PH_DATA uudt;
    PH_DATA qqov;
    PH_DATA udsp;
} MAIN_PROD_HEADER;

// 1.1.3.4
typedef struct
{
    PH_DATA abid;
    PH_DATA smod;
    PH_DATA apxs;
    PH_DATA avpa;
    PH_DATA lscd;
    PH_DATA lmap;
    PH_DATA qdlc;
    PH_DATA qdlp;
    PH_DATA qqai;
    PH_DATA selectedBandIds;
    PH_DATA southLineSelectedRectangle;
    PH_DATA northLineSelectedRectangle;
    PH_DATA eastColumnSelectedRectangle;
    PH_DATA westColumnSelectedRectangle;
} SECONDARY_PROD_HEADER;

// 1.1.3.7 and 1.1.3.8
typedef struct
{
    UBYTE visirlineVersion;
    GP_SC_ID satelliteId;
    TIME_CDS_EXPANDED trueRepeatCycleStart;
    INTEGER lineNumberInVisirGrid;
    GP_SC_CHAN_ID channelId;
    TIME_CDS_SHORT l10LineMeanAcquisitionTime;
    EBYTE lineValidity;
    EBYTE lineRadiometricQuality;
    EBYTE lineGeometricQuality;
    // actual line data not represented here
} SUB_VISIRLINE;

typedef struct
{
    UBYTE headerVersionNo;
    EBYTE packetType;     // 2 = mission data
    EBYTE subHeaderType;  // 0 = no subheader, 1 = GP_PK_SH1, 2 = GP_PK_SH2
    GP_FAC_ID sourceFacilityId;
    GP_FAC_ENV sourceEnvId;
    UBYTE sourceInstanceId;
    GP_SU_ID sourceSUId;
    GP_CPU_ID sourceCPUId;
    GP_FAC_ID destFacilityId;
    GP_FAC_ENV destEnvId;
    USHORT sequenceCount;
    UNSIGNED packetLength;
} GP_PK_HEADER;

typedef struct
{
    UBYTE subHeaderVersionNo;
    EBYTE checksumFlag;
    UBYTE acknowledgement[4];
    GP_SVCE_TYPE serviceType;
    UBYTE serviceSubType;
    TIME_CDS_SHORT packetTime;
    GP_SC_ID spacecraftId;
} GP_PK_SH1;

// 7.2.5 in image data format
typedef struct
{
    double cal_slope;
    double cal_offset;
} CALIBRATION;

typedef struct
{
    EBYTE radianceLinearisation[12];
    EBYTE detectorEqualisation[12];
    EBYTE onboardCalibrationResult[12];
    EBYTE MPEFCalFeedback[12];
    EBYTE MTFAdaption[12];
    EBYTE straylightCorrectionFlag[12];
    CALIBRATION level1_5ImageCalibration[12];
    // rest of structure omitted for now
} RADIOMETRIC_PROCESSING_RECORD;

// 7.2.4
typedef struct
{
    INTEGER numberOfLines;
    INTEGER numberOfColumns;
    REAL lineDirGridStep;
    REAL columnDirGridStep;
    EBYTE gridOrigin;
} REFERENCEGRID_VISIR;

typedef struct
{
    INTEGER southernLinePlanned;
    INTEGER northernLinePlanned;
    INTEGER easternColumnPlanned;
    INTEGER westernColumnPlanned;
} PLANNED_COVERAGE_VISIR;

typedef struct
{
    INTEGER lowerSouthLinePlanned;
    INTEGER lowerNorthLinePlanned;
    INTEGER lowerEastColumnPlanned;
    INTEGER lowerWestColumnPlanned;
    INTEGER upperSouthLinePlanned;
    INTEGER upperNorthLinePlanned;
    INTEGER upperEastColumnPlanned;
    INTEGER upperWestColumnPlanned;
} PLANNED_COVERAGE_HRV;

typedef struct
{
    EBYTE typeOfProjection;
    REAL longitudeOfSSP;
    REFERENCEGRID_VISIR referencegrid_visir;
    REFERENCEGRID_VISIR referencegrid_hrv;
    PLANNED_COVERAGE_VISIR plannedCoverage_visir;
    PLANNED_COVERAGE_HRV plannedCoverage_hrv;
    // rest of record omitted, for now
} IMAGE_DESCRIPTION_RECORD;

typedef struct
{
    UBYTE nominalImageScanning;
    UBYTE reducedScan;
    TIME_CDS_SHORT forwardScanStart;
    TIME_CDS_SHORT forwardScanEnd;
} ACTUAL_SCANNING_SUMMARY_RECORD;

typedef struct
{
    UBYTE nominalBehavior;
    UBYTE radScanIrregularity;
    UBYTE radStoppage;
    UBYTE repeatCycleNotCompleted;
    UBYTE gainChangeTookPlace;
    UBYTE decontaminationTookPlace;
    UBYTE noBBCalibrationAchieved;
    UBYTE incorrectTemperature;
    UBYTE invalidBBData;
    UBYTE invalidAuxOrHKTMData;
    UBYTE refocusingMechanismActuated;
    UBYTE mirrorBackToReferencePos;
} RADIOMETER_BEHAVIOR_RECORD;

typedef struct
{
    UNSIGNED plannedNumberOfL10Lines[12];
    UNSIGNED numberOfMissingL10Lines[12];
    UNSIGNED numberOfCorruptedL10Lines[12];
    UNSIGNED numberOfReplacedL10Lines[12];
} RECEPTION_SUMMARY_STATS_RECORD;

typedef struct
{
    UBYTE nominalImage;
    UBYTE nonNominalBecauseIncomplete;
    UBYTE nonNominalRadiometricQuality;
    UBYTE nonNominalGeometricQuality;
    UBYTE nonNominalTimeliness;
    UBYTE incompleteL15;
} L15_IMAGE_VALIDITY_RECORD;

typedef struct
{
    INTEGER southernLineActual;
    INTEGER northernLineActual;
    INTEGER easternColumnActual;
    INTEGER westernColumnActual;
} ACTUAL_L15_COVERAGE_VISIR_RECORD;

typedef struct
{
    INTEGER lowerSouthLineActual;
    INTEGER lowerNorthLineActual;
    INTEGER lowerEastColumnActual;
    INTEGER lowerWestColumnActual;
    INTEGER upperSouthLineActual;
    INTEGER upperNorthLineActual;
    INTEGER upperEastColumnActual;
    INTEGER upperWestColumnActual;
} ACTUAL_L15_COVERAGE_HRV_RECORD;

typedef struct
{
    GP_SC_ID satelliteId;
    ACTUAL_SCANNING_SUMMARY_RECORD actualScanningSummary;
    RADIOMETER_BEHAVIOR_RECORD radiometerBehavior;
    RECEPTION_SUMMARY_STATS_RECORD receptionSummaryStats;
    L15_IMAGE_VALIDITY_RECORD l15ImageValidity[12];
    ACTUAL_L15_COVERAGE_VISIR_RECORD actualL15CoverageVisir;
    ACTUAL_L15_COVERAGE_HRV_RECORD actualL15CoverageHrv;
} IMAGE_PRODUCTION_STATS_RECORD;

typedef struct
{
    UBYTE trailerHeaderVersion;
    IMAGE_PRODUCTION_STATS_RECORD imageProductionStats;
    // skip the rest - only after actual line/col values
} TRAILER;
// disable byte-packing
#pragma pack()

// endian conversion routines
void to_native(GP_PK_HEADER &h);
void to_native(GP_PK_SH1 &h);
void to_native(SUB_VISIRLINE &v);
void to_native(RADIOMETRIC_PROCESSING_RECORD &r);
void to_native(IMAGE_DESCRIPTION_RECORD &r);
void to_native(ACTUAL_L15_COVERAGE_VISIR_RECORD &r);
void to_native(ACTUAL_L15_COVERAGE_HRV_RECORD &r);

// utility function, alters string fields permanently
void to_string(PH_DATA &d);

// unit tests on structures, returns true on success
// bool perform_type_size_check(void);

class Conversions
{
  public:
    static void convert_pixel_to_geo(double line, double column,
                                     double &longitude, double &latitude);
    static void convert_geo_to_pixel(double longitude, double latitude,
                                     unsigned int &line, unsigned int &column);

    static void compute_pixel_xyz(double line, double column, double &x,
                                  double &y, double &z);
    static double compute_pixel_area_sqkm(double line, double column);

    static const double altitude;  // from origin
    static const double req;       // earth equatorial radius
    static const double rpol;      // earth polar radius
    static const double
        dtp2;  // square of the distance to the equatorial tangent point
    static const double oblate;         // oblateness of earth
    static const double eccentricity2;  // eccentricity squared
    static const double ratio2;         // (rpol/req)^2
    static const double deg_to_rad;
    static const double rad_to_deg;
    static const double step;    // pixel / line step in degrees
    static const double nlines;  // number of lines in an image

    static const int CFAC;            // Column scale factor
    static const int LFAC;            // Line scale factor
    static const int COFF;            // Column offset
    static const int LOFF;            // Line offset
    static const double CFAC_scaled;  // divided by 2^16
    static const double LFAC_scaled;
};

}  // namespace msg_native_format

#endif
