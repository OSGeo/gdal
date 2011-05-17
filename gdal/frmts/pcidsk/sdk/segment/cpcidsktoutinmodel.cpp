/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKToutinModelSegment class.
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

#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "segment/cpcidsktoutinmodel.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

namespace
{
    /** 
     * Function to get the minimum value of two values.
     * 
     * @param a The first value.
     * @param b The second value.
     * 
     * @return The minimum value of the two specified values.
     */
    int MinFunction(int a,int b) 
    {
        return (a<b)?a:b;
    }
}

CPCIDSKToutinModelSegment::CPCIDSKToutinModelSegment(PCIDSKFile *file, 
                                                   int segment,
                                                   const char *segment_pointer) :
    CPCIDSKEphemerisSegment(file, segment, segment_pointer,false)
{
    loaded_ = false;
    mbModified = false;
    mpoInfo = NULL;
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
 * Synchronize the segement, if it was modified then
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
    int		i,j,k,l;
    SRITInfo_t 	*SRITModel;
    bool	bVersion9;

/* -------------------------------------------------------------------- */
/*	Read the header block						*/
/* -------------------------------------------------------------------- */    
    // We test the name of the binary segment before starting to read 
    // the buffer.
    if (std::strncmp(seg_data.buffer, "MODEL   ", 8)) 
    {
        seg_data.Put("MODEL   ",0,8);
        return NULL;
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
    
    SRITModel->GCPMeanHtFlag = 0;
    SRITModel->nDownSample = 1;
    if(std::strncmp(seg_data.Get(22,2) , "DS", 2)==0)
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
/*	Read the GCP information in Block 2     			*/
/* -------------------------------------------------------------------- */

    SRITModel->nGCPCount       = seg_data.GetInt(2*512,10); 
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
/*      Read the GCPs							*/
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
/*      Call BinaryToEphemeris to get the orbital data		        */
/* -------------------------------------------------------------------- */
    SRITModel->OrbitPtr =
        BinaryToEphemeris( 512*21 );
    
/* -------------------------------------------------------------------- */
/*      Pass the sensor back to SRITModel				*/
/* -------------------------------------------------------------------- */
    SRITModel->Sensor = SRITModel->OrbitPtr->SatelliteSensor;

/* -------------------------------------------------------------------- */
/*      Assign nSensor value						*/
/* -------------------------------------------------------------------- */

    SRITModel->nSensor = GetSensor (SRITModel->OrbitPtr);
    SRITModel->nModel  = GetModel (SRITModel->nSensor);

    if( SRITModel->nSensor == -999)
    {
        throw PCIDSKException("Invalid Sensor : %s.",
                              SRITModel->OrbitPtr->SatelliteSensor.c_str());
    }
    if( SRITModel->nModel == -999)
    {
	throw PCIDSKException("Invalid Model from sensor number: %d.",
                              SRITModel->nSensor);
    }

/* -------------------------------------------------------------------- */
/*      Get the attitude data for SPOT					*/
/* -------------------------------------------------------------------- */
    if (SRITModel->OrbitPtr->AttitudeSeg != NULL ||
        SRITModel->OrbitPtr->RadarSeg != NULL)
    {
        if (SRITModel->OrbitPtr->Type == OrbAttitude)
        {
            int  ndata;
            AttitudeSeg_t *attitudeSeg
                = SRITModel->OrbitPtr->AttitudeSeg;

	    ndata = SRITModel->OrbitPtr->AttitudeSeg->NumberOfLine;
       
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

    return SRITModel;
}

/************************************************************************/
/*                           SRITInfoToBinary()                         */
/************************************************************************/
/**
  * Translate a SRITInfo_t into binary data.
  * Translate a SRITInfo_t into the corresponding block of          
  * binary data.  This function is expected to be used by           
  * ranslators such as iisopen.c (VISTA) so that our satellite     
  * models can be converted into some opaque serialized form.       
  * Translate a RFInfo_t into the corresponding block of binary data. 
  *
  * @param  SRITModel	     Satellite Model structure.
  * @param  pnBinaryLength	Length of binary data.
  * @return Binary data for a  Satellite Model structure.
  */
void
CPCIDSKToutinModelSegment::SRITInfoToBinary( SRITInfo_t *SRITModel )

{
    int		i,j,k,l;
    double	dfminht,dfmaxht,dfmeanht;
    int nPos = 0;

/* -------------------------------------------------------------------- */
/*      Create the data array.                                          */
/* -------------------------------------------------------------------- */
    seg_data.SetSize(512 * 21);

    //clean the buffer
    memset( seg_data.buffer , ' ', 512 * 21 );
    
/* -------------------------------------------------------------------- */
/*	Initialize the header.						*/
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
/*      Find the min and max height					*/
/* -------------------------------------------------------------------- */
    nPos = 2*512;

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

    if (SRITModel->OrbitPtr->AttitudeSeg != NULL ||
        SRITModel->OrbitPtr->RadarSeg != NULL ||
        SRITModel->OrbitPtr->AvhrrSeg != NULL )
    {
        if (SRITModel->OrbitPtr->Type == OrbAttitude) 
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
/*      Write the projection parameter if necessary			*/
/* -------------------------------------------------------------------- */

    seg_data.Put(SRITModel->utmunit.c_str(),nPos+225,16);

    if(SRITModel->oProjectionInfo.size() > 0)
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
/*	Add the serialized form of the EphemerisSeg_t.			*/
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

    if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"AVHRR",5))
        nSensor = AVHRR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"PLA",3))
        nSensor = PLA_1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MLA",3))
        nSensor = MLA_1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ASTER",5))
        nSensor = ASTER;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"SAR",3))
    {
        nSensor = SAR;
        if (OrbitPtr->PixelRes == 6.25)
            nSensor = RSAT_FIN;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-1",6))
        nSensor = LISS_1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-2",6))
        nSensor = LISS_2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-3",6))
        nSensor = LISS_3;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-L3-L2",10))
        nSensor = LISS_L3_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-L3",7))
        nSensor = LISS_L3;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-L4-L2",10))
        nSensor = LISS_L4_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-L4",7))
        nSensor = LISS_L4;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-P3-L2",10))
        nSensor = LISS_P3_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-P3",7))
        nSensor = LISS_P3;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-W3-L2",10))
        nSensor = LISS_W3_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-W3",7))
        nSensor = LISS_W3;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-M3",7))
        nSensor = LISS_M3;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-AWF-L2",11))
        nSensor = LISS_AWF_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"LISS-AWF",8))
        nSensor = LISS_AWF;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"EOC",3))
        nSensor = EOC;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"IRS",3))
        nSensor = IRS_1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"TM",2))
    {
        nSensor = TM;
        if (OrbitPtr->PixelRes == 15)
            nSensor = ETM;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ETM",3))
        nSensor = ETM;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"IKO",3))
    {
        nSensor = IKO_PAN;
        if (OrbitPtr->PixelRes == 4)
            nSensor = IKO_MULTI;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ORBVIEW",7))
    {
        nSensor = ORBVIEW_PAN;
        if (OrbitPtr->PixelRes == 4)
            nSensor = ORBVIEW_MULTI;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV",2))
    {
        if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV3_PAN_BASIC",13))
            nSensor = OV3_PAN_BASIC;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV3_PAN_GEO",11))
            nSensor = OV3_PAN_GEO;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV3_MULTI_BASIC",15))
            nSensor = OV3_MULTI_BASIC;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV3_MULTI_GEO",13))
            nSensor = OV3_MULTI_GEO;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV5_PAN_BASIC",13))
            nSensor = OV5_PAN_BASIC;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV5_PAN_GEO",11))
            nSensor = OV5_PAN_GEO;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV5_MULTI_BASIC",15))
            nSensor = OV5_MULTI_BASIC;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"OV5_MULTI_GEO",13))
            nSensor = OV5_MULTI_GEO;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_PAN_STD",13))
        nSensor = QBIRD_PAN_STD; 	// this checking must go first
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_PAN_STH",13))
        nSensor = QBIRD_PAN_STH;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_PAN",9))
        nSensor = QBIRD_PAN;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_MULTI_STD",15))
        nSensor = QBIRD_MULTI_STD;	// this checking must go first
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_MULTI_STH",15))
        nSensor = QBIRD_MULTI_STH;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"QBIRD_MULTI",11))
        nSensor = QBIRD_MULTI;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW1_PAN_STD",14) ||
        EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW_PAN_STD",13))
        nSensor = WVIEW_PAN_STD; 	// this checking must go first
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW1_PAN",10) ||
        EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW_PAN",9))
        nSensor = WVIEW_PAN;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW_MULTI_STD",15))
        nSensor = WVIEW_MULTI_STD;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"WVIEW_MULTI",11))
        nSensor = WVIEW_MULTI;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"FORMOSAT",8))
    {
        if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"FORMOSAT_PAN_L2",15))
            nSensor = FORMOSAT_PAN_L2;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"FORMOSAT_MULTIL2",16))
            nSensor = FORMOSAT_MULTIL2;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"FORMOSAT_PAN",12))
            nSensor = FORMOSAT_PAN;
        else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"FORMOSAT_MULTI",14))
            nSensor = FORMOSAT_MULTI;
    }
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"SPOT5_PAN_2_5",13))
        nSensor = SPOT5_PAN_2_5;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"SPOT5_PAN_5",11))
        nSensor = SPOT5_PAN_5;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"SPOT5_HRS",9))
        nSensor = SPOT5_HRS;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"SPOT5_MULTI",11))
        nSensor = SPOT5_MULTI;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MERIS_FR",8))
        nSensor = MERIS_FR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MERIS_RR",8))
        nSensor = MERIS_RR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MERIS_LR",8))
        nSensor = MERIS_LR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ASAR",4))
        nSensor = ASAR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"EROS",4))
        nSensor = EROS;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MODIS_1000",10))
        nSensor = MODIS_1000;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MODIS_500",9))
        nSensor = MODIS_500;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"MODIS_250",9))
        nSensor = MODIS_250;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_HRC_L2",12))
        nSensor = CBERS_HRC_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_HRC",9))
        nSensor = CBERS_HRC;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_CCD_L2",12))
        nSensor = CBERS_CCD_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_CCD",9))
        nSensor = CBERS_CCD;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_IRM_80_L2",15))
        nSensor = CBERS_IRM_80_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_IRM_80",12))
        nSensor = CBERS_IRM_80;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_IRM_160_L2",16))
        nSensor = CBERS_IRM_160_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_IRM_160",13))
        nSensor = CBERS_IRM_160;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_WFI_L2",12))
        nSensor = CBERS_WFI_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CBERS_WFI",9))
        nSensor = CBERS_WFI;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CARTOSAT1_L1",12))
        nSensor = CARTOSAT1_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"CARTOSAT1_L2",12))
        nSensor = CARTOSAT1_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"DMC_1R",6))
        nSensor = DMC_1R;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"DMC_1T",6))
        nSensor = DMC_1T;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ALOS_PRISM_L1",13))
        nSensor = ALOS_PRISM_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ALOS_PRISM_L2",13))
        nSensor = ALOS_PRISM_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ALOS_AVNIR_L1",13))
        nSensor = ALOS_AVNIR_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"ALOS_AVNIR_L2",13))
        nSensor = ALOS_AVNIR_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"PALSAR",6))
        nSensor = PALSAR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"KOMPSAT2_PAN",12))
        nSensor = KOMPSAT2_PAN;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"KOMPSAT2_MULTI",14))
        nSensor = KOMPSAT2_MULTI;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"TERRASAR",8))
        nSensor = TERRASAR;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"RAPIDEYE",8))
        nSensor = RAPIDEYE_L1B;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"THEOS_PAN_L1",12))
        nSensor = THEOS_PAN_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"THEOS_PAN_L2",12))
        nSensor = THEOS_PAN_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"THEOS_MS_L1",11))
        nSensor = THEOS_MS_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"THEOS_MS_L2",11))
        nSensor = THEOS_MS_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"GOSAT_500_L1",12))
        nSensor = GOSAT_500_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"GOSAT_500_L2",12))
        nSensor = GOSAT_500_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"GOSAT_1500_L1",13))
        nSensor = GOSAT_1500_L1;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"GOSAT_1500_L2",13))
        nSensor = GOSAT_1500_L2;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"HJ_CCD_1A",9))
        nSensor = HJ_CCD_1A;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"HJ_CCD_1B",5))
        nSensor = HJ_CCD_1B;
    else if (EQUALN(OrbitPtr->SatelliteSensor.c_str(),"NEW",3))
        nSensor = NEW;
    else
    {
        throw PCIDSKException("Invalid Sensor %s",
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
    int  nModel;

    nModel = -999;

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
        nModel = SRITModele1A;
        break;

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
        nModel = SRITModeleSAR;
        break;

    case ORBVIEW_PAN:
    case ORBVIEW_MULTI:
    case QBIRD_PAN:
    case QBIRD_MULTI:
    case WVIEW_PAN:
    case WVIEW_MULTI:
    case SPOT5_PAN_2_5:
    case CARTOSAT1_L1:
    case ALOS_PRISM_L1:
    case KOMPSAT2_PAN:
    case KOMPSAT2_MULTI:
    case CBERS_HRC:
    case OV3_PAN_BASIC:
    case OV3_MULTI_BASIC:
    case OV5_PAN_BASIC:
    case OV5_MULTI_BASIC:
        nModel = SRITModele1AHR;
        break;

    case EROS:
    case QBIRD_PAN_STH:
    case QBIRD_MULTI_STH:
    case FORMOSAT_PAN:
    case FORMOSAT_MULTI:
        nModel = SRITModeleEros;
        break;

    default:
        throw PCIDSKException("Invalid sensor type.");
        break;
    }

    return (nModel);
}

