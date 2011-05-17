/******************************************************************************
 *
 * Purpose: Support for storing and manipulating Orbit information
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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
#ifndef __INCLUDE_PCIDSK_ORBIT_INFORMATION_H
#define __INCLUDE_PCIDSK_ORBIT_INFORMATION_H

#include <string>
#include <vector>

namespace PCIDSK
{
/* -------------------------------------------------------------------- */
/*	Structure for ephemeris segment (ORBIT segment, type 160).	*/
/* -------------------------------------------------------------------- */
#define EPHEMERIS_BLK		8
#define EPHEMERIS_RADAR_BLK     10
#define EPHEMERIS_ATT_BLK	9
/* -------------------------------------------------------------------- */
/*	Structure for Satellite Radar segment.				*/
/* -------------------------------------------------------------------- */
#define ANC_DATA_PER_BLK	16
#define ANC_DATA_SIZE		32
    /**
     * Ancillary data structure.
     */
    struct AncillaryData_t
    {
        /**
         * Default constrcutor
         */
        AncillaryData_t()
        {
        }
        /**
         * Copy constructor
         * @param oAD the ancillary data to copy
         */
        AncillaryData_t(const AncillaryData_t& oAD)
        {
            Copy(oAD);
        }

        /**
         * assignement operator
         * @param oAD the ancillary data to assign
         */
        AncillaryData_t& operator=(const AncillaryData_t& oAD)
        {
            Copy(oAD);
            return *this;
        }

        /**
         * Copy function
         * @param oAD the ancillary data to copy
         */
        void Copy(const AncillaryData_t& oAD)
        {
            if(this == &oAD)
            {
                return;
            }
            SlantRangeFstPixel = oAD.SlantRangeFstPixel;	
            SlantRangeLastPixel = oAD.SlantRangeLastPixel;	
            FstPixelLat = oAD.FstPixelLat;		
            MidPixelLat = oAD.MidPixelLat;		
            LstPixelLat = oAD.LstPixelLat;		
            FstPixelLong = oAD.FstPixelLong;		
            MidPixelLong = oAD.MidPixelLong;		
            LstPixelLong = oAD.LstPixelLong;	
        }

        int   SlantRangeFstPixel;	/* Slant Range to First Pixel (m)	     */
        int   SlantRangeLastPixel;	/* Slant Range to Last Pixel (m)	     */
        float FstPixelLat;		/* First Pixel Latitude (millionths degrees) */
        float MidPixelLat;		/* Mid Pixel Latitude (millionths degrees)   */
        float LstPixelLat;		/* Last Pixel Latitude (millionths degrees)  */
        float FstPixelLong;		/* First Pixel Longitude (millionths degrees)*/
        float MidPixelLong;		/* Mid Pixel Longitude (millionths degrees)  */
        float LstPixelLong;		/* Last Pixel Longitude (millionths degrees) */
    } ;

    /**
     * Radar segment information
     */
    struct RadarSeg_t
    {
        /**
         * Default constrcutor
         */
        RadarSeg_t()
        {
        }
        /**
         * Copy constructor
         * @param oRS the radar segment to copy
         */
        RadarSeg_t(const RadarSeg_t& oRS)
        {
            Copy(oRS);
        }

        /**
         * assignement operator
         * @param oRS the radar segment to assign
         */
        RadarSeg_t& operator=(const RadarSeg_t& oRS)
        {
            Copy(oRS);
            return *this;
        }

        /**
         * Copy function
         * @param oRS the radar segment to copy
         */
        void Copy(const RadarSeg_t& oRS)
        {
            if(this == &oRS)
            {
                return;
            }
            Identifier = oRS.Identifier;	
            Facility = oRS.Facility;	
            Ellipsoid = oRS.Ellipsoid;	
            EquatorialRadius = oRS.EquatorialRadius;    
            PolarRadius = oRS.PolarRadius;		
            IncidenceAngle = oRS.IncidenceAngle;	
            PixelSpacing = oRS.PixelSpacing;	
            LineSpacing = oRS.LineSpacing;		
            ClockAngle = oRS.ClockAngle;		

            NumberBlockData = oRS.NumberBlockData;	
            NumberData = oRS.NumberData;		

            Line = oRS.Line;	
        }

        std::string   Identifier; /* Product identifier */
        std::string   Facility;	/* Processing facility */
        std::string   Ellipsoid; /* Ellipsoid designator */
        double EquatorialRadius; /* Equatorial radius of earth */
        double PolarRadius; /* Polar radius of earth */
        double IncidenceAngle; /* Incidence angle */
        double PixelSpacing; /* Nominal pixel spacing in metre */
        double LineSpacing; /* Nominal line spacing in metre */
        double ClockAngle; /* Clock angle in degree */

        int    NumberBlockData;	/* Number of blocks of ancillary data */
        int  NumberData; /* Number of ancillary data */

        std::vector<AncillaryData_t> Line; /* Pointer to ancillary line */
    } ;

/* -------------------------------------------------------------------- */
/*	Structure for Satellite attitude segment.			*/
/* -------------------------------------------------------------------- */
#define ATT_SEG_BLK		604
#define ATT_SEG_MAX_LINE	6000
#define ATT_SEG_LINE_PER_BLOCK  10

    /**
     * Attitude line information
     */
    struct AttitudeLine_t
    {
        /**
         * Default constrcutor
         */
        AttitudeLine_t()
        {
        }
        /**
         * Copy constructor
         * @param oAL the attitude line to copy
         */
        AttitudeLine_t(const AttitudeLine_t& oAL)
        {
            Copy(oAL);
        }

        /**
         * assignement operator
         * @param oAL the attitude line to assign
         */
        AttitudeLine_t& operator=(const AttitudeLine_t& oAL)
        {
            Copy(oAL);
            return *this;
        }

        /**
         * Copy function
         * @param oAL the attitude line to copy
         */
        void Copy(const AttitudeLine_t& oAL)
        {
            if(this == &oAL)
            {
                return;
            }
            ChangeInAttitude = oAL.ChangeInAttitude;
            ChangeEarthSatelliteDist = oAL.ChangeEarthSatelliteDist;
        }

        double ChangeInAttitude; /* Change in satellite attiutde (D22.16) */
        double ChangeEarthSatelliteDist; /* Change in earth-satellite distance
                                         (D22.16) */
    } ;

    /**
     * Attitude segment information
     */
    struct AttitudeSeg_t
    {
        /**
         * Default constrcutor
         */
        AttitudeSeg_t()
        {
        }
        /**
         * Copy constructor
         * @param oAS the attitude segment to copy
         */
        AttitudeSeg_t(const AttitudeSeg_t& oAS)
        {
            Copy(oAS);
        }

        /**
         * assignement operator
         * @param oAS the avhrr segment to assign
         */
        AttitudeSeg_t& operator=(const AttitudeSeg_t& oAS)
        {
            Copy(oAS);
            return *this;
        }

        /**
         * Copy function
         * @param oAS the avhrr segment to copy
         */
        void Copy(const AttitudeSeg_t& oAS)
        {
            if(this == &oAS)
            {
                return;
            }
            Roll = oAS.Roll;
            Pitch = oAS.Pitch;
            Yaw = oAS.Yaw;
            NumberOfLine = oAS.NumberOfLine;
            NumberBlockData = oAS.NumberBlockData;
            Line = oAS.Line;
        }

        double Roll; /* Roll (D22.16) */
        double Pitch; /* Pitch (D22.16) */
        double Yaw; /* Yaw (D22.16) */
        int  NumberOfLine; /* No. of Lines (I22) */

        int NumberBlockData; /* No. of block of data. */
        std::vector<AttitudeLine_t> Line;

    } ;

/* -------------------------------------------------------------------- */
/*	AVHRR orbit segment. Composed of 11 blocks plus extra blocks	*/
/*	for holding per-scanline information.				*/
/* -------------------------------------------------------------------- */
#define AVH_SEG_BASE_NUM_BLK	11

    /**
     * Avhrr line information
     */
    struct AvhrrLine_t
    {
        /**
         * Default constrcutor
         */
        AvhrrLine_t()
        {
        }
        /**
         * Copy constructor
         * @param oAL the avhrr line to copy
         */
        AvhrrLine_t(const AvhrrLine_t& oAL)
        {
            Copy(oAL);
        }

        /**
         * assignement operator
         * @param oAL the avhrr line to assign
         */
        AvhrrLine_t& operator=(const AvhrrLine_t& oAL)
        {
            Copy(oAL);
            return *this;
        }

        /**
         * Copy function
         * @param oAL the avhrr line to copy
         */
        void Copy(const AvhrrLine_t& oAL)
        {
            if(this == &oAL)
            {
                return;
            }
            nScanLineNum = oAL.nScanLineNum;
            nStartScanTimeGMTMsec = oAL.nStartScanTimeGMTMsec;
            for(int i=0 ; i < 10 ; i++)
                abyScanLineQuality[i] = oAL.abyScanLineQuality[i];
            for(int i=0 ; i < 5 ; i++)
            {
                aabyBadBandIndicators[i][0] = oAL.aabyBadBandIndicators[i][0];
                aabyBadBandIndicators[i][1] = oAL.aabyBadBandIndicators[i][1];
                anSpaceScanData[i] = oAL.anSpaceScanData[i];
            }
            for(int i=0 ; i < 8 ; i++)
                abySatelliteTimeCode[i] = oAL.abySatelliteTimeCode[i];
            for(int i=0 ; i < 3 ; i++)
            {
                anTargetTempData[i] = oAL.anTargetTempData[i];
                anTargetScanData[i] = oAL.anTargetScanData[i];
            }
        }

        /* For geocoding */
        int   nScanLineNum;
        int   nStartScanTimeGMTMsec;
        unsigned char abyScanLineQuality[10];
        unsigned char aabyBadBandIndicators[5][2];
        unsigned char abySatelliteTimeCode[8];

        /* For thermal/IR calibration */
        int   anTargetTempData[3];
        int   anTargetScanData[3];
        int   anSpaceScanData[5];

    } ;

    /**
     * Avhrr segment information.
     */
    struct AvhrrSeg_t
    {
        /**
         * Default constrcutor
         */
        AvhrrSeg_t()
        {
        }
        /**
         * Copy constructor
         * @param oAS the avhrr segment to copy
         */
        AvhrrSeg_t(const AvhrrSeg_t& oAS)
        {
            Copy(oAS);
        }

        /**
         * assignement operator
         * @param oAS the avhrr segment to assign
         */
        AvhrrSeg_t& operator=(const AvhrrSeg_t& oAS)
        {
            Copy(oAS);
            return *this;
        }

        /**
         * Copy function
         * @param oAS the avhrr segment to copy
         */
        void Copy(const AvhrrSeg_t& oAS)
        {
            if(this == &oAS)
            {
                return;
            }
            szImageFormat = oAS.szImageFormat;
            nImageXSize = oAS.nImageXSize;
            nImageYSize = oAS.nImageYSize;
            bIsAscending = oAS.bIsAscending;
            bIsImageRotated = oAS.bIsImageRotated;
            szOrbitNumber = oAS.szOrbitNumber;
            szAscendDescendNodeFlag = oAS.szAscendDescendNodeFlag;
            szEpochYearAndDay = oAS.szEpochYearAndDay;
            szEpochTimeWithinDay = oAS.szEpochTimeWithinDay;
            szTimeDiffStationSatelliteMsec = oAS.szTimeDiffStationSatelliteMsec;
            szActualSensorScanRate = oAS.szActualSensorScanRate;
            szIdentOfOrbitInfoSource = oAS.szIdentOfOrbitInfoSource;
            szInternationalDesignator = oAS.szInternationalDesignator;
            szOrbitNumAtEpoch = oAS.szOrbitNumAtEpoch;
            szJulianDayAscendNode = oAS.szJulianDayAscendNode;
            szEpochYear = oAS.szEpochYear;
            szEpochMonth = oAS.szEpochMonth;
            szEpochDay = oAS.szEpochDay;
            szEpochHour = oAS.szEpochHour;
            szEpochMinute = oAS.szEpochMinute;
            szEpochSecond = oAS.szEpochSecond;
            szPointOfAriesDegrees = oAS.szPointOfAriesDegrees;
            szAnomalisticPeriod = oAS.szAnomalisticPeriod;
            szNodalPeriod = oAS.szNodalPeriod;
            szEccentricity = oAS.szEccentricity;
            szArgumentOfPerigee = oAS.szArgumentOfPerigee;
            szRAAN = oAS.szRAAN;
            szInclination = oAS.szInclination;
            szMeanAnomaly = oAS.szMeanAnomaly;
            szSemiMajorAxis = oAS.szSemiMajorAxis;
            nRecordSize = oAS.nRecordSize;
            nBlockSize = oAS.nBlockSize;
            nNumRecordsPerBlock = oAS.nNumRecordsPerBlock;
            nNumBlocks = oAS.nNumBlocks;
            nNumScanlineRecords = oAS.nNumScanlineRecords;
            Line = oAS.Line;
        }

        /* Nineth Block Part 1 - General/header information */
        std::string  szImageFormat;
        int   nImageXSize;
        int   nImageYSize;
        bool bIsAscending;
        bool bIsImageRotated;

        /* Nineth Block Part 2 - Ephemeris information */
        std::string  szOrbitNumber;
        std::string  szAscendDescendNodeFlag;
        std::string  szEpochYearAndDay;
        std::string  szEpochTimeWithinDay;
        std::string  szTimeDiffStationSatelliteMsec;
        std::string  szActualSensorScanRate;
        std::string  szIdentOfOrbitInfoSource;
        std::string  szInternationalDesignator;
        std::string  szOrbitNumAtEpoch;
        std::string  szJulianDayAscendNode;
        std::string  szEpochYear;
        std::string  szEpochMonth;
        std::string  szEpochDay;
        std::string  szEpochHour;
        std::string  szEpochMinute;
        std::string  szEpochSecond;
        std::string  szPointOfAriesDegrees;
        std::string  szAnomalisticPeriod;
        std::string  szNodalPeriod;
        std::string  szEccentricity;
        std::string  szArgumentOfPerigee;
        std::string  szRAAN;
        std::string  szInclination;
        std::string  szMeanAnomaly;
        std::string  szSemiMajorAxis;

        /* 10th Block - Empty, reserved for future use */

        /* 11th Block - Needed for indexing 12th block onwards */
        int   nRecordSize;
        int   nBlockSize;
        int   nNumRecordsPerBlock;
        int   nNumBlocks;
        int   nNumScanlineRecords;

        /* 12th Block and onwards - Per-scanline records */
        std::vector<AvhrrLine_t> Line;

    } ;

    /**
     * Possible orbit types.
     */
    typedef enum 
    { 
        OrbNone, 
        OrbAttitude, 
        OrbLatLong, 
        OrbAvhrr 
    } OrbitType;

    /**
     * Ephemeris segment structure
     */
    struct EphemerisSeg_t
    {
        /**
         * Default constrcutor
         */
        EphemerisSeg_t()
        {
            AttitudeSeg = NULL;
            RadarSeg = NULL;
            AvhrrSeg = NULL;
        }

        /**
         * Destructor
         */
        ~EphemerisSeg_t()
        {
            delete AttitudeSeg;
            delete RadarSeg;
            delete AvhrrSeg;
        }

        /**
         * Copy constructor
         * @param oES the ephemeris segment to copy
         */
        EphemerisSeg_t(const EphemerisSeg_t& oES)
        {
            AttitudeSeg = NULL;
            RadarSeg = NULL;
            AvhrrSeg = NULL;
            Copy(oES);
        }

        /**
         * assignement operator
         * @param oES the ephemeris segment to assign
         */
        EphemerisSeg_t& operator=(const EphemerisSeg_t& oES)
        {
            Copy(oES);
            return *this;
        }

        /**
         * Copy function
         * @param oES the ephemeris segment to copy
         */
        void Copy(const EphemerisSeg_t& oES)
        {
            if(this == &oES)
            {
                return;
            }
            delete AttitudeSeg;
            delete RadarSeg;
            delete AvhrrSeg;
            AttitudeSeg = NULL;
            RadarSeg = NULL;
            AvhrrSeg = NULL;
            if(oES.AttitudeSeg)
                AttitudeSeg = new AttitudeSeg_t(*oES.AttitudeSeg);
            if(oES.RadarSeg)
                RadarSeg = new RadarSeg_t(*oES.RadarSeg);
            if(oES.AvhrrSeg)
                AvhrrSeg = new AvhrrSeg_t(*oES.AvhrrSeg);

            for(int i =0 ; i <39 ; i++)
                SPCoeff1B[i] = oES.SPCoeff1B[i];
            for(int i =0 ; i <4 ; i++)
                SPCoeffSg[i] = oES.SPCoeffSg[i];

            SatelliteDesc = oES.SatelliteDesc;
            SceneID = oES.SceneID;
            SatelliteSensor = oES.SatelliteSensor;
            SensorNo = oES.SensorNo;
            DateImageTaken = oES.DateImageTaken;
            SupSegExist = oES.SupSegExist;
            FieldOfView = oES.FieldOfView;
            ViewAngle = oES.ViewAngle;
            NumColCentre = oES.NumColCentre;
            RadialSpeed = oES.RadialSpeed;
            Eccentricity = oES.Eccentricity;
            Height = oES.Height;
            Inclination = oES.Inclination;
            TimeInterval = oES.TimeInterval;
            NumLineCentre = oES.NumLineCentre;
            LongCentre = oES.LongCentre;
            AngularSpd = oES.AngularSpd;
            AscNodeLong = oES.AscNodeLong;
            ArgPerigee = oES.ArgPerigee;
            LatCentre = oES.LatCentre;
            EarthSatelliteDist = oES.EarthSatelliteDist;
            NominalPitch = oES.NominalPitch;
            TimeAtCentre = oES.TimeAtCentre;
            SatelliteArg = oES.SatelliteArg;
            XCentre = oES.XCentre;
            YCentre = oES.YCentre;
            UtmYCentre = oES.UtmYCentre;
            UtmXCentre = oES.UtmXCentre;
            PixelRes = oES.PixelRes;
            LineRes = oES.LineRes;
            CornerAvail = oES.CornerAvail;
            MapUnit = oES.MapUnit;
            XUL = oES.XUL;
            YUL = oES.YUL;
            XUR = oES.XUR;
            YUR = oES.YUR;
            XLR = oES.XLR;
            YLR = oES.YLR;
            XLL = oES.XLL;
            YLL = oES.YLL;
            UtmYUL = oES.UtmYUL;
            UtmXUL = oES.UtmXUL;
            UtmYUR = oES.UtmYUR;
            UtmXUR = oES.UtmXUR;
            UtmYLR = oES.UtmYLR;
            UtmXLR = oES.UtmXLR;
            UtmYLL = oES.UtmYLL;
            UtmXLL = oES.UtmXLL;
            LatCentreDeg = oES.LatCentreDeg;
            LongCentreDeg = oES.LongCentreDeg;
            LatUL = oES.LatUL;
            LongUL = oES.LongUL;
            LatUR = oES.LatUR;
            LongUR = oES.LongUR;
            LatLR = oES.LatLR;
            LongLR = oES.LongLR;
            LatLL = oES.LatLL;
            LongLL = oES.LongLL;
            HtCentre = oES.HtCentre;
            HtUL = oES.HtUL;
            HtUR = oES.HtUR;
            HtLR = oES.HtLR;
            HtLL = oES.HtLL;
            ImageRecordLength = oES.ImageRecordLength;
            NumberImageLine = oES.NumberImageLine;
            NumberBytePerPixel = oES.NumberBytePerPixel;
            NumberSamplePerLine = oES.NumberSamplePerLine;
            NumberPrefixBytes = oES.NumberPrefixBytes;
            NumberSuffixBytes = oES.NumberSuffixBytes;
            SPNCoeff = oES.SPNCoeff;
            bDescending = oES.bDescending;
            Type = oES.Type;
        }

        /// Satellite description
        std::string SatelliteDesc;
        /// Scene ID
        std::string SceneID;

        /// Satellite sensor
        std::string SatelliteSensor;
        /// Satellite sensor no.
        std::string SensorNo;		
        /// Date of image taken
        std::string DateImageTaken;
        /// Flag to indicate supplemental segment
        bool SupSegExist;
        /// Scanner field of view (ALPHA)
        double FieldOfView;
        /// Viewing angle (BETA)
        double ViewAngle;
        /// Number of column at center (C0)
        double NumColCentre;
        /// Radial speed (DELIRO)
        double RadialSpeed;
        /// Eccentricity (ES)
        double Eccentricity;
        /// Height (H0)
        double Height;
        /// Inclination (I)
        double Inclination;
        /// Time interval (K)
        double TimeInterval;
        /// Number of line at center (L0)
        double NumLineCentre;
        /// Longitude of center (LAMBDA)
        double LongCentre;
        /// Angular speed (N)
        double AngularSpd;
        /// Ascending node Longitude (OMEGA-MAJ)
        double AscNodeLong; 
        /// Argument Perigee (OMEGA-MIN)
        double ArgPerigee;
        /// Latitude of center (PHI)
        double LatCentre;
        /// Earth Satellite distance (RHO)
        double EarthSatelliteDist;
        /// Nominal pitch (T)
        double NominalPitch;
        /// Time at centre (T0)
        double TimeAtCentre;
        /// Satellite argument (WP)
        double SatelliteArg;

        /// Scene center pixel coordinate
        double XCentre;
        /// Scene center line coordinate
        double YCentre;
        /// Scene centre UTM northing
        double UtmYCentre;
        /// Scene centre UTM easting
        double UtmXCentre;
        /// Pixel resolution in x direction
        double PixelRes;
        /// Pixel resolution in y direction
        double LineRes;
        /// Flag to tell corner coordinate available
        bool CornerAvail;
        /// Map units
        std::string MapUnit;
        /// Pixel coordinate of upper left corner
        double XUL;
        /// Line coordinate of upper left corner
        double YUL;
        /// Pixel coordinate of upper right corner
        double XUR;
        /// Line coordinate of upper right corner
        double YUR;
        /// Pixel coordinate of lower right corner
        double XLR;
        /// Line coordinate of lower right corner
        double YLR;
        /// Pixel coordinate of lower left corner
        double XLL;
        /// Line coordinate of lower left corner
        double YLL;
        /// UTM Northing of upper left corner
        double UtmYUL;
        /// UTM Easting of upper left corner
        double UtmXUL;
        /// UTM Northing of upper right corner
        double UtmYUR;
        /// UTM Easting of upper right corner
        double UtmXUR;
        /// UTM Northing of lower right corner
        double UtmYLR;
        /// UTM Easting of lower right corner
        double UtmXLR;
        /// Utm Northing of lower left corner
        double UtmYLL;
        /// Utm Easting of lower left corner
        double UtmXLL;

        /// Scene centre latitude (deg)
        double LatCentreDeg;
        /// Scene centre longitude (deg)
        double LongCentreDeg;
        /// Upper left latitude (deg)
        double LatUL;
        /// Upper left longitude (deg)
        double LongUL;
        /// Upper right latitude (deg)
        double LatUR;
        /// Upper right longitude (deg)
        double LongUR;
        /// Lower right latitude (deg)
        double LatLR;
        /// Lower right longitude (deg)
        double LongLR;
        /// Lower left latitude (deg)
        double LatLL;
        /// Lower left longitude (deg)
        double LongLL;
        /// Centre Height (m)
        double HtCentre;
        /// UL Height (m)
        double HtUL;
        /// UR Height (m)
        double HtUR;
        /// LR Height (m)
        double HtLR;
        /// LL Height (m)
        double HtLL;

        /// SPOT 1B coefficients
        double SPCoeff1B[39];
        /// SPOT 1B segment coefficients
        int    SPCoeffSg[4];

        /// Image record length
        int	   ImageRecordLength;
        /// Number of image line
        int	   NumberImageLine;
        /// Number of bytes per pixel
        int	   NumberBytePerPixel;
        /// Number of samples per line
        int	   NumberSamplePerLine;
        /// Number of prefix bytes
        int    NumberPrefixBytes;
        /// Number of suffix bytes
        int	   NumberSuffixBytes;
        /// Number of coefficients for SPOT 1B
        int	   SPNCoeff;

        /// Flag to indicate ascending or descending
        bool  bDescending;

        /// Orbit type: None, LatLong, Attitude, Avhrr
        OrbitType   Type;
        AttitudeSeg_t *AttitudeSeg;
        RadarSeg_t    *RadarSeg;
        AvhrrSeg_t    *AvhrrSeg;
    };

    /**
     * List of sensor type
     */
    typedef enum {PLA_1, MLA_1, PLA_2, MLA_2, PLA_3, MLA_3, PLA_4, MLA_4,
		ASTER, SAR, LISS_1, LISS_2, LISS_3, LISS_L3, LISS_L3_L2,
		LISS_L4, LISS_L4_L2, LISS_P3, LISS_P3_L2, LISS_W3, LISS_W3_L2,
		LISS_AWF, LISS_AWF_L2, LISS_M3, EOC, IRS_1, RSAT_FIN, 
		RSAT_STD, ERS_1, ERS_2, TM, ETM, IKO_PAN, IKO_MULTI, 
		ORBVIEW_PAN, ORBVIEW_MULTI, OV3_PAN_BASIC, OV3_PAN_GEO, 
		OV3_MULTI_BASIC, OV3_MULTI_GEO, OV5_PAN_BASIC, OV5_PAN_GEO, 
		OV5_MULTI_BASIC, OV5_MULTI_GEO, QBIRD_PAN, QBIRD_PAN_STD, 
		QBIRD_PAN_STH, QBIRD_MULTI, QBIRD_MULTI_STD, QBIRD_MULTI_STH, 
		FORMOSAT_PAN, FORMOSAT_MULTI, FORMOSAT_PAN_L2,
		FORMOSAT_MULTIL2, SPOT5_PAN_2_5, SPOT5_PAN_5, SPOT5_HRS,
		SPOT5_MULTI, MERIS_FR, MERIS_RR, MERIS_LR, ASAR, EROS, 
		MODIS_250, MODIS_500, MODIS_1000, CBERS_HRC, CBERS_HRC_L2,
		CBERS_CCD, CBERS_CCD_L2, CBERS_IRM_80, CBERS_IRM_80_L2, 
	        CBERS_IRM_160, CBERS_IRM_160_L2, CBERS_WFI, CBERS_WFI_L2, 
	 	CARTOSAT1_L1, CARTOSAT1_L2, ALOS_PRISM_L1, ALOS_PRISM_L2, 
		ALOS_AVNIR_L1, ALOS_AVNIR_L2, PALSAR, DMC_1R, DMC_1T, 
		KOMPSAT2_PAN, KOMPSAT2_MULTI, TERRASAR, WVIEW_PAN,
		WVIEW_PAN_STD, WVIEW_MULTI, WVIEW_MULTI_STD,
		RAPIDEYE_L1B, THEOS_PAN_L1, THEOS_PAN_L2,
		THEOS_MS_L1, THEOS_MS_L2, 
		GOSAT_500_L1, GOSAT_500_L2, GOSAT_1500_L1, GOSAT_1500_L2, 
		HJ_CCD_1A, HJ_CCD_1B, NEW, AVHRR} TypeDeCapteur;
}

#endif // __INCLUDE_PCIDSK_ORBIT_INFORMATION_H
