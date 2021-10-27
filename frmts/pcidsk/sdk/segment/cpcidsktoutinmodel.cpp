/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKToutinModelSegment class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "segment/cpcidsktoutinmodel.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <memory>

using namespace PCIDSK;

CPCIDSKToutinModelSegment::CPCIDSKToutinModelSegment(PCIDSKFile *fileIn,
                                                   int segmentIn,
                                                   const char *segment_pointer) :
    CPCIDSKEphemerisSegment(fileIn, segmentIn, segment_pointer,false)
{
    loaded_ = false;
    mbModified = false;
    mpoInfo = nullptr;
    Load();
}


CPCIDSKToutinModelSegment::~CPCIDSKToutinModelSegment()
{
    delete mpoInfo;
}

/**
 * Get the SRITInfo_t structure from read from the segment.
 * @return the Toutin information structure.
 */
SRITInfo_t CPCIDSKToutinModelSegment::GetInfo() const
{
    if (!mpoInfo)
    {
        const_cast<CPCIDSKToutinModelSegment *>(this)->Load();

        if (!mpoInfo)
        {
            ThrowPCIDSKException("Unable to load toutin segment.");
            return SRITInfo_t();
        }
    }

    return (*mpoInfo);
}

/**
 * Set the toutin information in the segment. The segment will be tag
 * as modified and will be synchronize on disk with the next call to
 * the function synchronize.
 * @param oInfo the toutin information.
 */
void CPCIDSKToutinModelSegment::SetInfo(const SRITInfo_t& oInfo)
{
    if(&oInfo == mpoInfo)
    {
        return ;
    }
    if(mpoInfo)
    {
        delete mpoInfo;
    }

    mpoInfo = new SRITInfo_t(oInfo);
    mbModified = true;
}

/**
 * Load the contents of the segment
 */
void CPCIDSKToutinModelSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }

    seg_data.SetSize((int)data_size - 1024);

    ReadFromFile(seg_data.buffer, 0, data_size - 1024);

    if(seg_data.buffer_size == 0)
    {
        return;
    }

    SRITInfo_t* poInfo = BinaryToSRITInfo();

    mpoInfo = poInfo;

    // We've now loaded the structure up with data. Mark it as being loaded
    // properly.
    loaded_ = true;
}

/**
 * Write the segment on disk
 */
void CPCIDSKToutinModelSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

    SRITInfoToBinary(mpoInfo);

    WriteToFile(seg_data.buffer,0,seg_data.buffer_size);

    mbModified = false;
}

/**
 * Synchronize the segment, if it was modified then
 * write it into disk.
 */
void CPCIDSKToutinModelSegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}

/************************************************************************/
/*                           BinaryToSRITInfo()                         */
/************************************************************************/
/**
  * Translate a block of binary data into a SRIT segment. the caller is
  * responsible to free the returned memory with delete.
  *
  * @return Rational Satellite Model structure.
  */
SRITInfo_t *
CPCIDSKToutinModelSegment::BinaryToSRITInfo()
{
    int             i,j,k,l;
    SRITInfo_t     *SRITModel;
    bool            bVersion9;

/* -------------------------------------------------------------------- */
/*      Read the header block                                           */
/* -------------------------------------------------------------------- */
    // We test the name of the binary segment before starting to read
    // the buffer.
    if (!STARTS_WITH(seg_data.buffer, "MODEL   "))
    {
        seg_data.Put("MODEL   ",0,8);
        return nullptr;
        // Something has gone terribly wrong!
        /*throw PCIDSKException("A segment that was previously "
            "identified as an RFMODEL "
            "segment does not contain the appropriate data. Found: [%s]",
            std::string(seg_data.buffer, 8).c_str());*/
    }

    bVersion9 = false;
    int nVersion = seg_data.GetInt(8,1);
    if (nVersion == 9)
    {
        bVersion9 = true;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the SRITModel.                                         */
/* -------------------------------------------------------------------- */
    SRITModel = new SRITInfo_t();

    std::unique_ptr<SRITInfo_t> oModelAutoPtr(SRITModel);

    SRITModel->GCPMeanHtFlag = 0;
    SRITModel->nDownSample = 1;
    if(STARTS_WITH(seg_data.Get(22,2) , "DS"))
    {
        SRITModel->nDownSample = seg_data.GetInt(24,3);
    }

/* -------------------------------------------------------------------- */
/*      Read the Block 1                                                */
/* -------------------------------------------------------------------- */

    SRITModel->N0x2        = seg_data.GetDouble(512,22);
    SRITModel->aa          = seg_data.GetDouble(512+22,22);
    SRITModel->SmALPHA     = seg_data.GetDouble(512+44,22);
    SRITModel->bb          = seg_data.GetDouble(512+66,22);
    SRITModel->C0          = seg_data.GetDouble(512+88,22);
    SRITModel->cc          = seg_data.GetDouble(512+110,22);
    SRITModel->COS_KHI     = seg_data.GetDouble(512+132,22);
    SRITModel->DELTA_GAMMA = seg_data.GetDouble(512+154,22);
    SRITModel->GAMMA       = seg_data.GetDouble(512+176,22);
    SRITModel->K_1         = seg_data.GetDouble(512+198,22);
    SRITModel->L0          = seg_data.GetDouble(512+220,22);
    SRITModel->P           = seg_data.GetDouble(512+242,22);
    SRITModel->Q           = seg_data.GetDouble(512+264,22);
    SRITModel->TAU         = seg_data.GetDouble(512+286,22);
    SRITModel->THETA       = seg_data.GetDouble(512+308,22);
    SRITModel->THETA_SEC   = seg_data.GetDouble(512+330,22);
    SRITModel->X0          = seg_data.GetDouble(512+352,22);
    SRITModel->Y0          = seg_data.GetDouble(512+374,22);
    SRITModel->delh        = seg_data.GetDouble(512+396,22);
    SRITModel->COEF_Y2     = seg_data.GetDouble(512+418,22);

    if (bVersion9)
    {
        SRITModel->delT        = seg_data.GetDouble(512+440,22);
        SRITModel->delL        = seg_data.GetDouble(512+462,22);
        SRITModel->delTau      = seg_data.GetDouble(512+484,22);
    }
    else
    {
        SRITModel->delT   = 0.0;
        SRITModel->delL   = 0.0;
        SRITModel->delTau = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Read the GCP information in Block 2                             */
/* -------------------------------------------------------------------- */

    SRITModel->nGCPCount       = seg_data.GetInt(2*512,10);
    if (SRITModel->nGCPCount > 256) SRITModel->nGCPCount = 256;
    SRITModel->nEphemerisSegNo = seg_data.GetInt(2*512+10,10);
    SRITModel->nAttitudeFlag   = seg_data.GetInt(2*512+20,10);
    SRITModel->GCPUnit = seg_data.Get(2*512+30,16);

    SRITModel->dfGCPMeanHt = seg_data.GetDouble(2*512+50,22);
    SRITModel->dfGCPMinHt  = seg_data.GetDouble(2*512+72,22);
    SRITModel->dfGCPMaxHt  = seg_data.GetDouble(2*512+94,22);

/* -------------------------------------------------------------------- */
/*      Initialize a simple GeoTransform.                               */
/* -------------------------------------------------------------------- */

    SRITModel->utmunit = seg_data.Get(2*512+225,16);

    if (std::strcmp(seg_data.Get(2*512+245,8),"ProjInfo")==0)
    {
        SRITModel->oProjectionInfo = seg_data.Get(2*512+255,256);
    }

/* -------------------------------------------------------------------- */
/*      Read the GCPs                                                   */
/* -------------------------------------------------------------------- */
    l = 0;
    k = 4;
    for (j=0; j<SRITModel->nGCPCount; j++)
    {
        SRITModel->nGCPIds[j] =
            seg_data.GetInt((k-1)*512+10*l,5);
        SRITModel->nPixel[j]  =
            seg_data.GetInt((k-1)*512+10*(l+1),5);
        SRITModel->nLine[j]   =
            seg_data.GetInt((k-1)*512+10*(l+1)+5,5);
        SRITModel->dfElev[j]  =
            seg_data.GetInt((k-1)*512+10*(l+2),10);
        l+=3;

        if (l<50)
            continue;

        k++;
        l = 0;
    }

/* -------------------------------------------------------------------- */
/*      Call BinaryToEphemeris to get the orbital data                  */
/* -------------------------------------------------------------------- */
    SRITModel->OrbitPtr =
        BinaryToEphemeris( 512*21 );

/* -------------------------------------------------------------------- */
/*      Pass the sensor back to SRITModel                               */
/* -------------------------------------------------------------------- */
    SRITModel->Sensor = SRITModel->OrbitPtr->SatelliteSensor;

/* -------------------------------------------------------------------- */
/*      Assign nSensor value                                            */
/* -------------------------------------------------------------------- */

    SRITModel->nSensor = GetSensor (SRITModel->OrbitPtr);
    SRITModel->nModel  = GetModel (SRITModel->nSensor);

    if( SRITModel->nSensor == -999)
    {
        return (SRITInfo_t*)ThrowPCIDSKExceptionPtr("Invalid Sensor : %s.",
                              SRITModel->OrbitPtr->SatelliteSensor.c_str());
    }
    if( SRITModel->nModel == -999)
    {
        return (SRITInfo_t*)ThrowPCIDSKExceptionPtr("Invalid Model from sensor number: %d.",
                              SRITModel->nSensor);
    }

/* -------------------------------------------------------------------- */
/*      Get the attitude data for SPOT                                  */
/* -------------------------------------------------------------------- */
    if (SRITModel->OrbitPtr->AttitudeSeg != nullptr ||
        SRITModel->OrbitPtr->RadarSeg != nullptr)
    {
        AttitudeSeg_t *attitudeSeg
            = SRITModel->OrbitPtr->AttitudeSeg;

        if (SRITModel->OrbitPtr->Type == OrbAttitude &&
            attitudeSeg != nullptr)
        {
            int  ndata;

            ndata = attitudeSeg->NumberOfLine;

            for (i=0; i<ndata; i++)
            {
                SRITModel->Hdeltat.push_back(
                    attitudeSeg->Line[i].ChangeInAttitude);
                SRITModel->Qdeltar.push_back(
                    attitudeSeg->Line[i].ChangeEarthSatelliteDist);
            }
        }
    }
    else
    {
        SRITModel->Qdeltar.clear();
        SRITModel->Hdeltat.clear();
    }

    oModelAutoPtr.release();

    return SRITModel;
}

/************************************************************************/
/*                           SRITInfoToBinary()                         */
/************************************************************************/
/**
  * Translate a SRITInfo_t into binary data.
  * Translate a SRITInfo_t into the corresponding block of
  * binary data.  This function is expected to be used by
  * translators such as iisopen.c (VISTA) so that our satellite
  * models can be converted into some opaque serialized form.
  * Translate a Rpc Model into the corresponding block of binary data.
  *
  * @param  SRITModel        Satellite Model structure.
  */
void
CPCIDSKToutinModelSegment::SRITInfoToBinary( SRITInfo_t *SRITModel )

{
    int         i,j,k,l;
    double      dfminht,dfmaxht,dfmeanht;
    int         nPos = 0;

/* -------------------------------------------------------------------- */
/*      Create the data array.                                          */
/* -------------------------------------------------------------------- */
    seg_data.SetSize(512 * 21);

    //clean the buffer
    memset( seg_data.buffer , ' ', 512 * 21 );

/* -------------------------------------------------------------------- */
/*      Initialize the header.                                          */
/* -------------------------------------------------------------------- */
    nPos = 512*0;
    seg_data.Put("MODEL   9.0",0,nPos+11);

    seg_data.Put("DS",nPos+22,2);
    seg_data.Put(SRITModel->nDownSample,nPos+24,3);

/* -------------------------------------------------------------------- */
/*      Write the model results to second segment                       */
/* -------------------------------------------------------------------- */
    nPos = 512*1;

    seg_data.Put(SRITModel->N0x2,nPos,22,"%22.14f");
    seg_data.Put(SRITModel->aa,nPos+22,22,"%22.14f");
    seg_data.Put(SRITModel->SmALPHA,nPos+22*2,22,"%22.14f");
    seg_data.Put(SRITModel->bb,nPos+22*3,22,"%22.14f");
    seg_data.Put(SRITModel->C0,nPos+22*4,22,"%22.14f");
    seg_data.Put(SRITModel->cc,nPos+22*5,22,"%22.14f");
    seg_data.Put(SRITModel->COS_KHI,nPos+22*6,22,"%22.14f");
    seg_data.Put(SRITModel->DELTA_GAMMA,nPos+22*7,22,"%22.14f");
    seg_data.Put(SRITModel->GAMMA,nPos+22*8,22,"%22.14f");
    seg_data.Put(SRITModel->K_1,nPos+22*9,22,"%22.14f");
    seg_data.Put(SRITModel->L0,nPos+22*10,22,"%22.14f");
    seg_data.Put(SRITModel->P,nPos+22*11,22,"%22.14f");
    seg_data.Put(SRITModel->Q,nPos+22*12,22,"%22.14f");
    seg_data.Put(SRITModel->TAU,nPos+22*13,22,"%22.14f");
    seg_data.Put(SRITModel->THETA,nPos+22*14,22,"%22.14f");
    seg_data.Put(SRITModel->THETA_SEC,nPos+22*15,22,"%22.14f");
    seg_data.Put(SRITModel->X0,nPos+22*16,22,"%22.14f");
    seg_data.Put(SRITModel->Y0,nPos+22*17,22,"%22.14f");
    seg_data.Put(SRITModel->delh,nPos+22*18,22,"%22.14f");
    seg_data.Put(SRITModel->COEF_Y2,nPos+22*19,22,"%22.14f");
    seg_data.Put(SRITModel->delT,nPos+22*20,22,"%22.14f");
    seg_data.Put(SRITModel->delL,nPos+22*21,22,"%22.14f");
    seg_data.Put(SRITModel->delTau,nPos+22*22,22,"%22.14f");

/* -------------------------------------------------------------------- */
/*      Find the min and max height                                     */
/* -------------------------------------------------------------------- */
    nPos = 2*512;

    if (SRITModel->nGCPCount > 256) SRITModel->nGCPCount = 256;
    if (SRITModel->nGCPCount != 0)
    {
        dfminht = 1.e38;
        dfmaxht = -1.e38;
        for (i=0; i<SRITModel->nGCPCount; i++)
        {
            if (SRITModel->dfElev[i] > dfmaxht)
                dfmaxht = SRITModel->dfElev[i];
            if (SRITModel->dfElev[i] < dfminht)
                dfminht = SRITModel->dfElev[i];
        }
    }
    else
    {
        dfminht = SRITModel->dfGCPMinHt;
        dfmaxht = 0;
    }

    dfmeanht = (dfminht + dfmaxht)/2.;

    seg_data.Put(SRITModel->nGCPCount,nPos,10);
    seg_data.Put("2",nPos+10,1);
    seg_data.Put("0",nPos+20,1);

    if (SRITModel->OrbitPtr->AttitudeSeg != nullptr ||
        SRITModel->OrbitPtr->RadarSeg != nullptr ||
        SRITModel->OrbitPtr->AvhrrSeg != nullptr )
    {
        if (SRITModel->OrbitPtr->Type == OrbAttitude &&
            SRITModel->OrbitPtr->AttitudeSeg != nullptr)
        {
            if (SRITModel->OrbitPtr->AttitudeSeg->NumberOfLine != 0)
                seg_data.Put("3",nPos+20,1);
        }
    }

    seg_data.Put(SRITModel->GCPUnit.c_str(),nPos+30,16);
    seg_data.Put("M",nPos+49,1);

    seg_data.Put(dfmeanht,nPos+50,22,"%22.14f");
    seg_data.Put(dfminht,nPos+72,22,"%22.14f");
    seg_data.Put(dfmaxht,nPos+94,22,"%22.14f");

    seg_data.Put("NEWGCP",nPos+116,6);

/* -------------------------------------------------------------------- */
/*      Write the projection parameter if necessary                     */
/* -------------------------------------------------------------------- */

    seg_data.Put(SRITModel->utmunit.c_str(),nPos+225,16);

    if(!SRITModel->oProjectionInfo.empty())
    {
        seg_data.Put("ProjInfo: ",nPos+245,10);
        seg_data.Put(SRITModel->oProjectionInfo.c_str(),
                             nPos+255,256);
    }

/* -------------------------------------------------------------------- */
/*      Write the GCP to third segment                                  */
/* -------------------------------------------------------------------- */
    nPos = 3*512;

    l = 0;
    k = 3;
    if (SRITModel->nGCPCount > 256) SRITModel->nGCPCount = 256;
    for (j=0; j<SRITModel->nGCPCount; j++)
    {
        if (j > 255)
            break;

        seg_data.Put(SRITModel->nGCPIds[j],nPos+10*l,5);
        seg_data.Put((int)(SRITModel->nPixel[j]+0.5),
                             nPos+10*(l+1),5);
        seg_data.Put((int)(SRITModel->nLine[j]+0.5),
                             nPos+10*(l+1)+5,5);
        seg_data.Put((int)SRITModel->dfElev[j],nPos+10*(l+2),10);

        l+=3;

        if (l<50)
            continue;

        k++;
        nPos = 512*k;
        l = 0;
    }

/* -------------------------------------------------------------------- */
/*      Add the serialized form of the EphemerisSeg_t.                  */
/* -------------------------------------------------------------------- */
    EphemerisToBinary( SRITModel->OrbitPtr , 512*21 );
}

/**
 * Get the sensor enum from the orbit segment.
 * @param OrbitPtr the orbit segment
 * @return the sensor type.
 */
int  CPCIDSKToutinModelSegment::GetSensor( EphemerisSeg_t *OrbitPtr)
{
    int  nSensor;

    nSensor = -999;

    if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "AVHRR"))
        nSensor = AVHRR;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLA"))
        nSensor = PLA_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MLA"))
        nSensor = MLA_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ASTER"))
        nSensor = ASTER;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SAR"))
    {
        nSensor = SAR;
        if (OrbitPtr->PixelRes == 6.25)
            nSensor = RSAT_FIN;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-1"))
            nSensor = LISS_1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-2"))
            nSensor = LISS_2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-3"))
            nSensor = LISS_3;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-L3-L2"))
            nSensor = LISS_L3_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-L3"))
            nSensor = LISS_L3;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-L4-L2"))
            nSensor = LISS_L4_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-L4"))
            nSensor = LISS_L4;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-P3-L2"))
            nSensor = LISS_P3_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-P3"))
            nSensor = LISS_P3;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-W3-L2"))
            nSensor = LISS_W3_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-W3"))
            nSensor = LISS_W3;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-M3"))
            nSensor = LISS_M3;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-AWF-L2"))
            nSensor = LISS_AWF_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "LISS-AWF"))
            nSensor = LISS_AWF;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "EOC"))
        nSensor = EOC;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "IRS"))
        nSensor = IRS_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MSS"))
        nSensor = MSS;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TM"))
    {
        nSensor = TM;
        if (OrbitPtr->PixelRes == 15)
            nSensor = ETM;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ETM"))
        nSensor = ETM;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "IKO"))
    {
        nSensor = IKO_PAN;
        if (OrbitPtr->PixelRes == 4)
            nSensor = IKO_MULTI;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ORBVIEW"))
    {
        nSensor = ORBVIEW_PAN;
        if (OrbitPtr->PixelRes == 4)
            nSensor = ORBVIEW_MULTI;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV3_PAN_BASIC"))
            nSensor = OV3_PAN_BASIC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV3_PAN_GEO"))
            nSensor = OV3_PAN_GEO;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV3_MULTI_BASIC"))
            nSensor = OV3_MULTI_BASIC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV3_MULTI_GEO"))
            nSensor = OV3_MULTI_GEO;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV5_PAN_BASIC"))
            nSensor = OV5_PAN_BASIC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV5_PAN_GEO"))
            nSensor = OV5_PAN_GEO;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV5_MULTI_BASIC"))
            nSensor = OV5_MULTI_BASIC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "OV5_MULTI_GEO"))
            nSensor = OV5_MULTI_GEO;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_PAN_STD"))
            nSensor = QBIRD_PAN_STD; // this checking must go first
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_PAN_STH"))
            nSensor = QBIRD_PAN_STH;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_PAN"))
            nSensor = QBIRD_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_MULTI_STD"))
            nSensor = QBIRD_MULTI_STD; // this checking must go first
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_MULTI_STH"))
            nSensor = QBIRD_MULTI_STH;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "QBIRD_MULTI"))
            nSensor = QBIRD_MULTI;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW1_PAN_STD") ||
            STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW_PAN_STD"))
            nSensor = WVIEW_PAN_STD; // this checking must go first
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW1_PAN") ||
            STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW_PAN"))
            nSensor = WVIEW_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW_MULTI_STD"))
            nSensor = WVIEW_MULTI_STD;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "WVIEW_MULTI"))
            nSensor = WVIEW_MULTI;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GEOEYE"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GEOEYE_PAN_STD"))
            nSensor = GEOEYE_PAN_STD; // this checking must go first
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GEOEYE_PAN"))
            nSensor = GEOEYE_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GEOEYE_MULTI_STD"))
            nSensor = GEOEYE_MULTI_STD; // this checking must go first
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GEOEYE_MULTI"))
            nSensor = GEOEYE_MULTI;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT_PAN_L2"))
            nSensor = FORMOSAT_PAN_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT_MULTIL2"))
            nSensor = FORMOSAT_MULTIL2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT_PAN"))
            nSensor = FORMOSAT_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT_MULTI"))
            nSensor = FORMOSAT_MULTI;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT5_PAN"))
            nSensor = FORMOSAT5_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "FORMOSAT5_MS"))
            nSensor = FORMOSAT5_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT5_PAN_2_5"))
            nSensor = SPOT5_PAN_2_5;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT5_PAN_5"))
            nSensor = SPOT5_PAN_5;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT5_HRS"))
            nSensor = SPOT5_HRS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT5_MULTI"))
            nSensor = SPOT5_MULTI;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT1_PAN"))
            nSensor = SPOT1_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT1_MS"))
            nSensor = SPOT1_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT2_PAN"))
            nSensor = SPOT2_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT2_MS"))
            nSensor = SPOT2_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT3_PAN"))
            nSensor = SPOT3_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT3_MS"))
            nSensor = SPOT3_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT4_PAN"))
            nSensor = SPOT4_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT4_MS"))
            nSensor = SPOT4_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT6_PAN"))
            nSensor = SPOT6_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT6_MS"))
            nSensor = SPOT6_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT6_PSH"))
            nSensor = SPOT6_PSH;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT7_PAN"))
            nSensor = SPOT7_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT7_MS"))
            nSensor = SPOT7_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SPOT7_PSH"))
            nSensor = SPOT7_PSH;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MERIS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MERIS_FR"))
           nSensor = MERIS_FR;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MERIS_RR"))
           nSensor = MERIS_RR;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MERIS_LR"))
           nSensor = MERIS_LR;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ASAR"))
        nSensor = ASAR;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "EROS"))
        nSensor = EROS;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MODIS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MODIS_1000"))
           nSensor = MODIS_1000;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MODIS_500"))
           nSensor = MODIS_500;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MODIS_250"))
           nSensor = MODIS_250;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_HRC_L2"))
           nSensor = CBERS_HRC_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_HRC"))
           nSensor = CBERS_HRC;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_CCD_L2"))
           nSensor = CBERS_CCD_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_CCD"))
           nSensor = CBERS_CCD;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_IRM_80_L2"))
           nSensor = CBERS_IRM_80_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_IRM_80"))
           nSensor = CBERS_IRM_80;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_IRM_160_L2"))
           nSensor = CBERS_IRM_160_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_IRM_160"))
           nSensor = CBERS_IRM_160;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_WFI_L2"))
           nSensor = CBERS_WFI_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS_WFI"))
           nSensor = CBERS_WFI;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_PAN_1"))
           nSensor = CBERS4_PAN_1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_MS_1"))
           nSensor = CBERS4_MS_1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_PAN_2"))
           nSensor = CBERS4_PAN_2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_MS_2"))
           nSensor = CBERS4_MS_2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_THM_1"))
           nSensor = CBERS4_THM_1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CBERS4_THM_2"))
           nSensor = CBERS4_THM_2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CARTOSAT"))
    {
       if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CARTOSAT1_L1"))
           nSensor = CARTOSAT1_L1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "CARTOSAT1_L2"))
           nSensor = CARTOSAT1_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DMC"))
    {
       if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DMC_1R"))
           nSensor = DMC_1R;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DMC_1T"))
           nSensor = DMC_1T;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALOS"))
    {
       if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALOS_PRISM_L1"))
           nSensor = ALOS_PRISM_L1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALOS_PRISM_L2"))
           nSensor = ALOS_PRISM_L2;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALOS_AVNIR_L1"))
           nSensor = ALOS_AVNIR_L1;
       else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALOS_AVNIR_L2"))
           nSensor = ALOS_AVNIR_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PALSAR"))
        nSensor = PALSAR;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT2_PAN"))
            nSensor = KOMPSAT2_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT2_MULTI"))
            nSensor = KOMPSAT2_MULTI;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3_PAN"))
            nSensor = KOMPSAT3_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3_PSH"))
            nSensor = KOMPSAT3_PSH;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3_MS"))
            nSensor = KOMPSAT3_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3A_PAN"))
            nSensor = KOMPSAT3A_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3A_PSH"))
            nSensor = KOMPSAT3A_PSH;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KOMPSAT3A_MS"))
            nSensor = KOMPSAT3A_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TERRASAR"))
        nSensor = TERRASAR;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "RAPIDEYE"))
        nSensor = RAPIDEYE_L1B;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "RESOURCESAT"))
        nSensor = RESOURCESAT;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "THEOS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "THEOS_PAN_L1"))
            nSensor = THEOS_PAN_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "THEOS_PAN_L2"))
            nSensor = THEOS_PAN_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "THEOS_MS_L1"))
            nSensor = THEOS_MS_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "THEOS_MS_L2"))
            nSensor = THEOS_MS_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOSAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOSAT_500_L1"))
            nSensor = GOSAT_500_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOSAT_500_L2"))
            nSensor = GOSAT_500_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOSAT_1500_L1"))
            nSensor = GOSAT_1500_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOSAT_1500_L2"))
            nSensor = GOSAT_1500_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ_CCD_1A") ||
             STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ1A"))
            nSensor = HJ_CCD_1A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ_CC" /* "HJ_CCD_1B" */) ||
             STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ1B"))
            nSensor = HJ_CCD_1B;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HJ1C"))
            nSensor = HJ1C;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "RASAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "RASAT_PAN"))
            nSensor = RASAT_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "RASAT_MS"))
            nSensor = RASAT_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLEIADES"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLEIADES_PAN_L1"))
            nSensor = PLEIADES_PAN_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLEIADES_MS_L1"))
            nSensor = PLEIADES_MS_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLEIADES_PAN_L2"))
            nSensor = PLEIADES_PAN_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PLEIADES_MS_L2"))
            nSensor = PLEIADES_MS_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TH01"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TH01_DGP"))
            nSensor = TH01_DGP;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TH01_GFB"))
            nSensor = TH01_GFB;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TH01_SXZ"))
            nSensor = TH01_SXZ;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY3_NAD"))
            nSensor = ZY3_NAD;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY3_FWD"))
            nSensor = ZY3_FWD;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY3_BWD"))
            nSensor = ZY3_BWD;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY3_MUX"))
            nSensor = ZY3_MUX;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY3_TLC"))
            nSensor = ZY3_TLC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY1_02C_HRC"))
            nSensor = ZY1_02C_HRC;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY1_02C_PMS_PAN"))
            nSensor = ZY1_02C_PMS_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ZY1_02C_PMS_MS"))
            nSensor = ZY1_02C_PMS_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GK2"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GK2_PAN"))
            nSensor = GK2_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GK2_MS"))
            nSensor = GK2_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MRC"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MRC_RED"))
            nSensor = MRC_RED;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MRC_GRN"))
            nSensor = MRC_GRN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MRC_BLU"))
            nSensor = MRC_BLU;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MRC_NIR"))
            nSensor = MRC_NIR;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HRC"))
        nSensor = HRC;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF1_PMS_PAN"))
            nSensor = GF1_PMS_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF2_PMS_PAN"))
            nSensor = GF2_PMS_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF1_PMS_MS"))
            nSensor = GF1_PMS_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF1_WFV"))
            nSensor = GF1_WFV;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF2_PMS_MS"))
            nSensor = GF2_PMS_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF4_PMS_MS"))
            nSensor = GF4_PMS_MS;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF4_PMI_Thermal"))
            nSensor = GF4_PMI_Thermal;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF6_PMS_PAN"))
            nSensor = GF6_PMS_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GF6_PMS_MS"))
            nSensor = GF6_PMS_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SJ9"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SJ9_PAN"))
            nSensor = SJ9_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SJ9_MUX"))
            nSensor = SJ9_MUX;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SJ9_PMS_PAN"))
            nSensor = SJ9_PMS_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SJ9_PMS_MS"))
            nSensor = SJ9_PMS_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "YG2_1"))
        nSensor = YG2_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "YG8_1"))
        nSensor = YG8_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "YG14_1"))
        nSensor = YG14_1;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "UAVSAR"))
        nSensor = UAVSAR;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SSOT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SSOT_PAN_L1"))
            nSensor = SSOT_PAN_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SSOT_MS_L1"))
            nSensor = SSOT_MS_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SSOT_PAN_L2"))
            nSensor = SSOT_PAN_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SSOT_MS_L2"))
            nSensor = SSOT_MS_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALSAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALSAT2_PAN_1A"))
            nSensor = ALSAT2_PAN_1A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALSAT2_MS_1A"))
            nSensor = ALSAT2_MS_1A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALSAT2_PAN_2A"))
            nSensor = ALSAT2_PAN_2A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "ALSAT2_MS_2A"))
            nSensor = ALSAT2_MS_2A;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DUBAISAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DUBAISAT2_PAN"))
            nSensor = DUBAISAT2_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DUBAISAT2_MS"))
            nSensor = DUBAISAT2_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT1_PAN_1A"))
            nSensor = KAZEOSAT1_PAN_1A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT1_MS_1A"))
            nSensor = KAZEOSAT1_MS_1A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT1_PAN_2A"))
            nSensor = KAZEOSAT1_PAN_2A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT1_MS_2A"))
            nSensor = KAZEOSAT1_MS_2A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "KAZEOSAT2_MS_1G"))
            nSensor = KAZEOSAT2_MS_1G;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DEIMOS"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DEIMOS1_MS_1R"))
            nSensor = DEIMOS1_MS_1R;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DEIMOS2_PAN_1B"))
            nSensor = DEIMOS2_PAN_1B;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DEIMOS2_MS_1B"))
            nSensor = DEIMOS2_MS_1B;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "DEIMOS2_PSH_1B"))
            nSensor = DEIMOS2_PSH_1B;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TRIPLESAT"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TRIPLESAT_PAN"))
            nSensor = TRIPLESAT_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "TRIPLESAT_MS"))
            nSensor = TRIPLESAT_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PER_"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PER_PAN_2A"))
            nSensor = PER_PAN_2A;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "PER_MS_2A"))
            nSensor = PER_MS_2A;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "JL"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "JL101A_PAN"))
            nSensor = JL101A_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "JL101A_MS"))
            nSensor = JL101A_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SV"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SV1_PAN_L1"))
            nSensor = SV1_PAN_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SV1_MS_L1"))
            nSensor = SV1_MS_L1;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SV1_PAN_L2"))
            nSensor = SV1_PAN_L2;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "SV1_MS_L2"))
            nSensor = SV1_MS_L2;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "GOKTURK1"))
    {
        if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "Gokturk1_PAN"))
            nSensor = GOKTURK1_PAN;
        else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "Gokturk1_MS"))
            nSensor = GOKTURK1_MS;
    }
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "HI_RES"))
        nSensor = HI_RES;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "MED_RES"))
        nSensor = MED_RES;
    else if (STARTS_WITH_CI(OrbitPtr->SatelliteSensor.c_str(), "NEW"))
        nSensor = NEW;
    else
    {
        return ThrowPCIDSKException(0, "Invalid Sensor %s",
            OrbitPtr->SatelliteSensor.c_str());
    }

    return (nSensor);
}
/**
 * Get the model of a sensor
 * @param nSensor the sensor
 * @return the model
 */
int CPCIDSKToutinModelSegment::GetModel( int nSensor )
{
    int nModel = -999;

    switch (nSensor)
    {
    case PLA_1:
    case PLA_2:
    case PLA_3:
    case PLA_4:
    case MLA_1:
    case MLA_2:
    case MLA_3:
    case MLA_4:
    case SPOT1_PAN:
    case SPOT1_MS:
    case SPOT2_PAN:
    case SPOT2_MS:
    case SPOT3_PAN:
    case SPOT3_MS:
    case SPOT4_PAN:
    case SPOT4_MS:
    case NEW:
        nModel = SRITModele;
        break;

    case ASTER:
    case CBERS_CCD:
    case CBERS_IRM_80:
    case CBERS_IRM_160:
    case CBERS_WFI:
    case IRS_1:
    case LISS_AWF:
    case LISS_1:
    case LISS_2:
    case LISS_3:
    case LISS_L3:
    case LISS_L4:
    case LISS_P3:
    case LISS_W3:
    case LISS_M3:
    case EOC:
    case SPOT5_PAN_5:
    case SPOT5_HRS:
    case SPOT5_MULTI:
    case MERIS_FR:
    case MERIS_RR:
    case MERIS_LR:
    case MODIS_1000:
    case MODIS_500:
    case MODIS_250:
    case ALOS_AVNIR_L1:
    case ALOS_AVNIR_L2:
    case RAPIDEYE_L1B:
    case THEOS_PAN_L1:
    case THEOS_MS_L1:
    case GOSAT_500_L1:
    case GOSAT_1500_L1:
    case HJ_CCD_1A:
    case RASAT_PAN:
    case RASAT_MS:
    case GK2_MS:
    case GF1_WFV:
    case YG8_1:
    case DEIMOS1_MS_1R:
    case MED_RES:
    case CBERS4_PAN_1:
    case CBERS4_MS_1:
    case CBERS4_THM_1:
    case SV1_PAN_L1:
    case SV1_MS_L1:
        nModel = SRITModele1A;
        break;

    case MSS:
    case TM:
    case ETM:
    case LISS_P3_L2:
    case LISS_L3_L2:
    case LISS_W3_L2:
    case LISS_L4_L2:
    case LISS_AWF_L2:
    case CBERS_IRM_80_L2:
    case CBERS_IRM_160_L2:
    case CBERS_WFI_L2:
    case CBERS_CCD_L2:
    case CBERS_HRC_L2:
    case DMC_1R:
    case DMC_1T:
    case ALOS_PRISM_L2:
    case THEOS_PAN_L2:
    case THEOS_MS_L2:
    case GOSAT_500_L2:
    case GOSAT_1500_L2:
    case HJ_CCD_1B:
    case PLEIADES_PAN_L2:
    case PLEIADES_MS_L2:
    case SSOT_PAN_L2:
    case SSOT_MS_L2:
    case ALSAT2_PAN_2A:
    case ALSAT2_MS_2A:
    case RESOURCESAT:
    case CBERS4_PAN_2:
    case CBERS4_MS_2:
    case CBERS4_THM_2:
    case SV1_PAN_L2:
    case SV1_MS_L2:
        nModel = SRITModele1B;
        break;

    case SAR:
    case RSAT_FIN:
    case RSAT_STD:
    case ERS_1:
    case ERS_2:
    case ASAR:
    case QBIRD_PAN_STD:
    case QBIRD_MULTI_STD:
    case WVIEW_PAN_STD:
    case WVIEW_MULTI_STD:
    case GEOEYE_PAN_STD:
    case GEOEYE_MULTI_STD:
    case IKO_PAN:
    case IKO_MULTI:
    case CARTOSAT1_L2:
    case PALSAR:
    case FORMOSAT_PAN_L2:
    case FORMOSAT_MULTIL2:
    case TERRASAR:
    case OV3_PAN_GEO:
    case OV3_MULTI_GEO:
    case OV5_PAN_GEO:
    case OV5_MULTI_GEO:
    case UAVSAR:
    case HJ1C:
        nModel = SRITModeleSAR;
        break;

    case ORBVIEW_PAN:
    case ORBVIEW_MULTI:
    case QBIRD_PAN:
    case QBIRD_MULTI:
    case WVIEW_PAN:
    case WVIEW_MULTI:
    case GEOEYE_PAN:
    case GEOEYE_MULTI:
    case SPOT5_PAN_2_5:
    case CARTOSAT1_L1:
    case ALOS_PRISM_L1:
    case KOMPSAT2_PAN:
    case KOMPSAT2_MULTI:
    case KOMPSAT3_PAN:
    case KOMPSAT3_PSH:
    case KOMPSAT3_MS:
    case KOMPSAT3A_PAN:
    case KOMPSAT3A_PSH:
    case KOMPSAT3A_MS:
    case CBERS_HRC:
    case OV3_PAN_BASIC:
    case OV3_MULTI_BASIC:
    case OV5_PAN_BASIC:
    case OV5_MULTI_BASIC:
    case PLEIADES_PAN_L1:
    case PLEIADES_MS_L1:
    case SPOT6_PAN:
    case SPOT6_MS:
    case SPOT6_PSH:
    case SPOT7_PAN:
    case SPOT7_MS:
    case SPOT7_PSH:
    case TH01_DGP:
    case TH01_GFB:
    case TH01_SXZ:
    case ZY1_02C_HRC:
    case ZY1_02C_PMS_PAN:
    case ZY1_02C_PMS_MS:
    case ZY3_NAD:
    case ZY3_FWD:
    case ZY3_BWD:
    case ZY3_MUX:
    case ZY3_TLC:
    case GK2_PAN:
    case MRC_RED:
    case MRC_GRN:
    case MRC_BLU:
    case MRC_NIR:
    case GF1_PMS_PAN:
    case GF1_PMS_MS:
    case GF2_PMS_PAN:
    case GF2_PMS_MS:
    case GF4_PMS_MS:
    case GF4_PMI_Thermal:
    case GF6_PMS_PAN:
    case GF6_PMS_MS:
    case SJ9_PAN:
    case SJ9_MUX:
    case SJ9_PMS_PAN:
    case SJ9_PMS_MS:
    case YG2_1:
    case YG14_1:
    case SSOT_PAN_L1:
    case SSOT_MS_L1:
    case ALSAT2_PAN_1A:
    case ALSAT2_MS_1A:
    case DUBAISAT2_PAN:
    case DUBAISAT2_MS:
    case KAZEOSAT1_PAN_1A:
    case KAZEOSAT1_MS_1A:
    case KAZEOSAT1_PAN_2A:
    case KAZEOSAT1_MS_2A:
    case KAZEOSAT2_MS_1G:
    case DEIMOS2_PAN_1B:
    case DEIMOS2_MS_1B:
    case DEIMOS2_PSH_1B:
    case TRIPLESAT_PAN:
    case TRIPLESAT_MS:
    case PER_PAN_2A:
    case PER_MS_2A:
    case JL101A_PAN:
    case JL101A_MS:
    case HI_RES:
    case FORMOSAT5_PAN:
    case FORMOSAT5_MS:
    case GOKTURK1_PAN:
    case GOKTURK1_MS:
        nModel = SRITModele1AHR;
        break;

    case EROS:
    case HRC:
    case QBIRD_PAN_STH:
    case QBIRD_MULTI_STH:
    case FORMOSAT_PAN:
    case FORMOSAT_MULTI:
        nModel = SRITModeleEros;
        break;

    default:
        return ThrowPCIDSKException(0, "Invalid sensor type.");
        break;
    }

    return (nModel);
}

