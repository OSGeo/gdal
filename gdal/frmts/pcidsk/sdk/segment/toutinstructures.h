/******************************************************************************
 *
 * Purpose: Support for storing and manipulating Toutin information
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
#ifndef __INCLUDE_PCIDSK_TOUTIN_INFORMATION_H
#define __INCLUDE_PCIDSK_TOUTIN_INFORMATION_H

#include "segment/orbitstructures.h"

namespace PCIDSK
{
/* -------------------------------------------------------------------- */
/*      SRITInfo_t - Satellite Model structure.                         */
/* -------------------------------------------------------------------- */
#define AP_MDL   1
#define SRIT_MDL 2
#define RF_MDL   6
#define RTCS_MDL 7
#define ADS_MDL  9
#define SRITModele 0
#define SRITModele1A 1
#define SRITModele1B 2
#define SRITModeleSAR 3
#define SRITModele1AHR 4
#define SRITModeleEros 5

#define MAX_SPOT_LINES 30000

    /**
     * the SRITInfo_t struct contains all information
     * for the Toutin Math Model.
     */
    struct SRITInfo_t
    {
        /**
         * default constructor
         */
        SRITInfo_t()
        {
            OrbitPtr = NULL;
        }
        /**
         * destructor
         */
        ~SRITInfo_t()
        {
            delete OrbitPtr;
        }

        /**
         * Copy constructor.
         * @param oSI the SRITInfo_t to copy
         */
        SRITInfo_t(const SRITInfo_t& oSI)
        {
            OrbitPtr = NULL;
            Copy(oSI);
        }

        /**
         * Assignment operator
         * @param oSI the SRITInfo_t to assign
         */
        SRITInfo_t& operator=(const SRITInfo_t& oSI)
        {
            Copy(oSI);
            return *this;
        }

        /**
         * Copy function
         * @param oSI the SRITInfo_t to copy
         */
        void Copy(const SRITInfo_t& oSI)
        {
            if(this == &oSI)
            {
                return;
            }
            delete OrbitPtr;
            OrbitPtr = NULL;
            if(oSI.OrbitPtr)
            {
                OrbitPtr = new EphemerisSeg_t(*oSI.OrbitPtr);
            }

            for(int i=0 ; i<256 ; i++)
            {
                nGCPIds[i] = oSI.nGCPIds[i];
                nPixel[i] = oSI.nPixel[i];
                nLine[i] = oSI.nLine[i];
                dfElev[i] = oSI.dfElev[i];
            }

            N0x2 = oSI.N0x2;
            aa = oSI.aa;
            SmALPHA = oSI.SmALPHA;
            bb = oSI.bb;
            C0 = oSI.C0;
            cc = oSI.cc;
            COS_KHI = oSI.COS_KHI;
            DELTA_GAMMA = oSI.DELTA_GAMMA;
            GAMMA = oSI.GAMMA;
            K_1 = oSI.K_1;
            L0 = oSI.L0;
            P = oSI.P;
            Q = oSI.Q;
            TAU = oSI.TAU;
            THETA = oSI.THETA;
            THETA_SEC = oSI.THETA_SEC;
            X0 = oSI.X0;
            Y0 = oSI.Y0;
            delh = oSI.delh;
            COEF_Y2 = oSI.COEF_Y2;
            delT = oSI.delT;
            delL = oSI.delL;
            delTau = oSI.delTau;
            nDownSample = oSI.nDownSample;
            nGCPCount = oSI.nGCPCount;
            nEphemerisSegNo = oSI.nEphemerisSegNo;
            nAttitudeFlag = oSI.nAttitudeFlag;
            utmunit = oSI.utmunit;
            GCPUnit = oSI.GCPUnit;
            GCPMeanHtFlag = oSI.GCPMeanHtFlag;
            dfGCPMeanHt = oSI.dfGCPMeanHt;
            dfGCPMinHt = oSI.dfGCPMinHt;
            dfGCPMaxHt = oSI.dfGCPMaxHt;
            Qdeltar = oSI.Qdeltar;
            Hdeltat = oSI.Hdeltat;
            Sensor = oSI.Sensor;
            nSensor = oSI.nSensor;
            nModel = oSI.nModel;
            RawToGeo = oSI.RawToGeo;
            oProjectionInfo = oSI.oProjectionInfo;
        }

        double N0x2;
        double aa;
        double SmALPHA;
        double bb;
        double C0;
        double cc;
        double COS_KHI;
        double DELTA_GAMMA;
        double GAMMA;
        double K_1;
        double L0;
        double P;
        double Q;
        double TAU;
        double THETA;
        double THETA_SEC;
        double X0;
        double Y0;
        double delh;
        double COEF_Y2;
        double delT;
        double delL;
        double delTau;
        int    nDownSample;
        int    nGCPCount;
        int    nEphemerisSegNo;
        int    nAttitudeFlag;
        std::string   utmunit;
        std::string   GCPUnit;
        char   GCPMeanHtFlag;
        double dfGCPMeanHt;
        double dfGCPMinHt;
        double dfGCPMaxHt;
        int    nGCPIds[256];
        int    nPixel[256],nLine[256];
        double dfElev[256];
        std::vector<double> Qdeltar;
        std::vector<double> Hdeltat;
        std::string   Sensor;
        int    nSensor;
        int    nModel;
        EphemerisSeg_t *OrbitPtr;
        bool  RawToGeo;
        std::string oProjectionInfo;
    } ;
}

#endif // __INCLUDE_PCIDSK_TOUTIN_INFORMATION_H
