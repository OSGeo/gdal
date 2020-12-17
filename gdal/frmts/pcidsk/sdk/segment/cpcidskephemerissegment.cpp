/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKEphemerisSegment class.
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
#include "segment/cpcidskephemerissegment.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <memory>

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

/**
 * Ephemeris Segment constructor
 * @param fileIn the PCIDSK file
 * @param segmentIn the segment index
 * @param segment_pointer the segment pointer
 * @param bLoad true to load the segment, else false (default true)
 */
CPCIDSKEphemerisSegment::CPCIDSKEphemerisSegment(PCIDSKFile *fileIn,
                                                   int segmentIn,
                                                   const char *segment_pointer,
                                                   bool bLoad) :
    CPCIDSKSegment(fileIn, segmentIn, segment_pointer),
    loaded_(false),mbModified(false)
{
    mpoEphemeris = nullptr;
    if(bLoad)
    {
        Load();
    }
}


CPCIDSKEphemerisSegment::~CPCIDSKEphemerisSegment()
{
    delete mpoEphemeris;
}

/**
 * Load the contents of the segment
 */
void CPCIDSKEphemerisSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }

    seg_data.SetSize((int)data_size - 1024);

    if(data_size == 1024)
        return;

    ReadFromFile(seg_data.buffer, 0, data_size - 1024);

    // We test the name of the binary segment before starting to read
    // the buffer.
    if (!STARTS_WITH(seg_data.buffer, "ORBIT   "))
    {
        seg_data.Put("ORBIT   ",0,8);
        loaded_ = true;
        return ;
    }

    mpoEphemeris = BinaryToEphemeris(0);

    // We've now loaded the structure up with data. Mark it as being loaded
    // properly.
    loaded_ = true;
}

/**
 * Write the segment on disk
 */
void CPCIDSKEphemerisSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

    EphemerisToBinary( mpoEphemeris, 0);

    seg_data.Put("ORBIT   ",0,8);

    WriteToFile(seg_data.buffer,0,seg_data.buffer_size);

    mbModified = false;
}

/**
 * Synchronize the segment, if it was modified then
 * write it into disk.
 */
void CPCIDSKEphemerisSegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}

/************************************************************************/
/*                              ConvertDeg()                            */
/************************************************************************/
/**
 * if mode is 0, convert angle from 0 to 360 to 0 to 180 and 0 to -180
 * if mode is 1, convert angle from 0 to 180 and 0 to -180 to 0 to 360
 *
 * @param degree the degree
 * @param mode  the mode
 */
double CPCIDSKEphemerisSegment::ConvertDeg(double degree, int mode)
{
    double result;

    if (mode == 0)
    {
/* -------------------------------------------------------------------- */
/*      degree is in range of 0 to 360                                  */
/* -------------------------------------------------------------------- */
        if (degree > 180)
            result = degree - 360;
        else
            result = degree;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      degree is in range of 0 to 180 and 0 to -180                    */
/* -------------------------------------------------------------------- */
        if (degree < 0)
            result = 360 + degree;
        else
            result = degree;
    }
    return (result);
}

/************************************************************************/
/*                      ReadAvhrrEphemerisSegment()                     */
/************************************************************************/
/**
 *  Read the contents of blocks 9, 11, and onwards from the orbit
 *  segment into the EphemerisSeg_t structure.
 * @param nStartBlock where to start to read in the buffer
 * @param psEphSegRec the structure to populate with information.
 */
void
CPCIDSKEphemerisSegment::ReadAvhrrEphemerisSegment(int nStartBlock,
                                         EphemerisSeg_t *psEphSegRec)
{
    int  nBlock = 0, nLine = 0;
    int nPos = 0;

    int nDataLength = seg_data.buffer_size;
/* -------------------------------------------------------------------- */
/*  Allocate the AVHRR segment portion of EphemerisSeg_t.               */
/* -------------------------------------------------------------------- */
    psEphSegRec->AvhrrSeg = new AvhrrSeg_t();
    AvhrrSeg_t* as = psEphSegRec->AvhrrSeg;

/* -------------------------------------------------------------------- */
/*  Read in the Ninth Block which contains general info + ephemeris     */
/*  info as well.                                                       */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 8*512;

    as->szImageFormat = seg_data.Get(nPos, 16);
    as->nImageXSize = seg_data.GetInt(nPos+16, 16);
    as->nImageYSize = seg_data.GetInt(nPos+32, 16);

    if ( STARTS_WITH(seg_data.Get(nPos+48,9), "ASCENDING") )
        as->bIsAscending = true;
    else
        as->bIsAscending = false;
    if ( STARTS_WITH(seg_data.Get(nPos+64,7), "ROTATED") )
        as->bIsImageRotated = true;
    else
        as->bIsImageRotated = false;

    as->szOrbitNumber = seg_data.Get(nPos+80, 16);
    as->szAscendDescendNodeFlag = seg_data.Get(nPos+96,16);
    as->szEpochYearAndDay = seg_data.Get(nPos+112,16);
    as->szEpochTimeWithinDay = seg_data.Get(nPos+128,16);
    as->szTimeDiffStationSatelliteMsec = seg_data.Get(nPos+144,16);
    as->szActualSensorScanRate = seg_data.Get(nPos+160,16);
    as->szIdentOfOrbitInfoSource = seg_data.Get(nPos+176,16);
    as->szInternationalDesignator = seg_data.Get(nPos+192,16);
    as->szOrbitNumAtEpoch = seg_data.Get(nPos+208,16);
    as->szJulianDayAscendNode = seg_data.Get(nPos+224,16);
    as->szEpochYear = seg_data.Get(nPos+240,16);
    as->szEpochMonth = seg_data.Get(nPos+256,16);
    as->szEpochDay = seg_data.Get(nPos+272,16);
    as->szEpochHour = seg_data.Get(nPos+288,16);
    as->szEpochMinute = seg_data.Get(nPos+304,16);
    as->szEpochSecond = seg_data.Get(nPos+320,16);
    as->szPointOfAriesDegrees = seg_data.Get(nPos+336,16);
    as->szAnomalisticPeriod = seg_data.Get(nPos+352,16);
    as->szNodalPeriod = seg_data.Get(nPos+368,16);
    as->szEccentricity = seg_data.Get(nPos+384,16);
    as->szArgumentOfPerigee = seg_data.Get(nPos+400,16);
    as->szRAAN = seg_data.Get(nPos+416,16);
    as->szInclination = seg_data.Get(nPos+432,16);
    as->szMeanAnomaly = seg_data.Get(nPos+448,16);
    as->szSemiMajorAxis = seg_data.Get(nPos+464,16);

/* -------------------------------------------------------------------- */
/*  Skip the 10th block which is reserved for future use.               */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*  Read in the 11th block, which contains indexing info.               */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*10;

    as->nRecordSize             = seg_data.GetInt(nPos,    16);
    as->nBlockSize              = seg_data.GetInt(nPos+16, 16);
    as->nNumRecordsPerBlock     = seg_data.GetInt(nPos+32, 16);
    as->nNumBlocks              = seg_data.GetInt(nPos+48, 16);
    as->nNumScanlineRecords     = seg_data.GetInt(nPos+64, 16);

/* -------------------------------------------------------------------- */
/*  Allocate the scanline records.                                      */
/* -------------------------------------------------------------------- */
    if ( as->nNumScanlineRecords == 0 )
        return;

/* -------------------------------------------------------------------- */
/*  Now read the 12th block and onward.                                 */
/* -------------------------------------------------------------------- */
    nBlock = 12;

    if ( as->nNumRecordsPerBlock == 0 )
        return;

    for(nLine = 0; nLine < as->nNumScanlineRecords;
                   nLine += as->nNumRecordsPerBlock)
    {
        int nNumRecords = MinFunction(as->nNumRecordsPerBlock,
                                      as->nNumScanlineRecords - nLine);
        nPos = nStartBlock + 512*(nBlock-1);
        if( nDataLength < 512*nBlock )
        {
            break;
        }

        for(int i = 0; i < nNumRecords; ++i)
        {
            AvhrrLine_t sLine;
            ReadAvhrrScanlineRecord(nPos+i*80, &sLine);
            as->Line.push_back(sLine);
        }

        ++nBlock;
    }
}

/************************************************************************/
/*                      ReadAvhrrScanlineRecord()                       */
/************************************************************************/
/**
 *  Read from a byte buffer in order to set a scanline record.
 * @param nPos position in buffer
 * @param psScanlineRecord the record to read.
 */
void
CPCIDSKEphemerisSegment::ReadAvhrrScanlineRecord(int nPos,
                                           AvhrrLine_t *psScanlineRecord)
{
    int i;
    AvhrrLine_t *sr = psScanlineRecord;

    sr->nScanLineNum = ReadAvhrrInt32((unsigned char*)seg_data.Get(nPos,4));
    sr->nStartScanTimeGMTMsec = ReadAvhrrInt32((unsigned char*)seg_data.Get(nPos+4,4));

    for(i = 0; i < 10; ++i)
        sr->abyScanLineQuality[i] = static_cast<unsigned char>(seg_data.GetInt(nPos+8+i,1));

    for(i = 0; i < 5; ++i)
    {
        sr->aabyBadBandIndicators[i][0] = static_cast<unsigned char>(seg_data.GetInt(nPos+18+2*i,1));
        sr->aabyBadBandIndicators[i][1] = static_cast<unsigned char>(seg_data.GetInt(nPos+18+2*i+1,1));
    }

    for(i = 0; i < 8; ++i)
        sr->abySatelliteTimeCode[i] = static_cast<unsigned char>(seg_data.GetInt(nPos+28+i,1));

    for(i = 0; i < 3; ++i)
        sr->anTargetTempData[i] = ReadAvhrrInt32((unsigned char*)seg_data.Get(nPos+36+i*4,4));
    for(i = 0; i < 3; ++i)
        sr->anTargetScanData[i] = ReadAvhrrInt32((unsigned char*)seg_data.Get(nPos+48+i*4,4));
    for(i = 0; i < 5; ++i)
        sr->anSpaceScanData[i]  = ReadAvhrrInt32((unsigned char*)seg_data.Get(nPos+60+i*4,4));
}

/************************************************************************/
/*                         ReadAvhrrInt32()                             */
/************************************************************************/
/**
 * Read an integer from a given buffer of at least 4 bytes.
 * @param pbyBuf the buffer that contains the value.
 * @return the value
 */
int
CPCIDSKEphemerisSegment::ReadAvhrrInt32(unsigned char* pbyBuf)
{
    int nValue = 0;
    unsigned char* b = pbyBuf;
    nValue = (int)((b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3]);

    return( nValue );
}

/************************************************************************/
/*                    WriteAvhrrEphemerisSegment()                      */
/************************************************************************/
/**
 *  Write the contents of blocks 9, 10, and onwards to the orbit
 *  segment from fields in the EphemerisSeg_t structure.
 * @param nStartBlock where to start to write the information in the buffer
 * @param psEphSegRec the information to write.
 */
void
CPCIDSKEphemerisSegment::WriteAvhrrEphemerisSegment(int nStartBlock,
                                           EphemerisSeg_t *psEphSegRec)
{
    int  nBlock = 0, nLine = 0;
    int nPos = 0;
/* -------------------------------------------------------------------- */
/*  Check that the AvhrrSeg is not NULL.                                */
/* -------------------------------------------------------------------- */
    AvhrrSeg_t* as = psEphSegRec->AvhrrSeg;

    if ( as == nullptr)
    {
        return ThrowPCIDSKException("The AvhrrSeg is NULL.");
    }

/* -------------------------------------------------------------------- */
/*      Realloc the data buffer large enough to hold all the AVHRR      */
/*      information, and zero it.                                       */
/* -------------------------------------------------------------------- */
    int nToAdd = 512 *
        (((as->nNumScanlineRecords + as->nNumRecordsPerBlock-1) /
                   as->nNumRecordsPerBlock)
        +4);
    seg_data.SetSize(seg_data.buffer_size + nToAdd);

    nPos = nStartBlock;
    memset(seg_data.buffer+nPos,' ',nToAdd);

/* -------------------------------------------------------------------- */
/*  Write the first avhrr Block.                                        */
/* -------------------------------------------------------------------- */

    seg_data.Put(as->szImageFormat.c_str(),nPos,16);

    seg_data.Put(as->nImageXSize,nPos+16,16);
    seg_data.Put(as->nImageYSize,nPos+32,16);

    if ( as->bIsAscending )
        seg_data.Put("ASCENDING",nPos+48,9);
    else
        seg_data.Put("DESCENDING",nPos+48,10);

    if ( as->bIsImageRotated )
        seg_data.Put("ROTATED",nPos+64,7);
    else
        seg_data.Put("NOT ROTATED",nPos+64,11);

    seg_data.Put(as->szOrbitNumber.c_str(),nPos+80,16);
    seg_data.Put(as->szAscendDescendNodeFlag.c_str(),nPos+96,16,true);
    seg_data.Put(as->szEpochYearAndDay.c_str(),nPos+112,16,true);
    seg_data.Put(as->szEpochTimeWithinDay.c_str(),nPos+128,16,true);
    seg_data.Put(as->szTimeDiffStationSatelliteMsec.c_str(),nPos+144,16,true);
    seg_data.Put(as->szActualSensorScanRate.c_str(),nPos+160,16,true);
    seg_data.Put(as->szIdentOfOrbitInfoSource.c_str(),nPos+176,16,true);
    seg_data.Put(as->szInternationalDesignator.c_str(),nPos+192,16,true);
    seg_data.Put(as->szOrbitNumAtEpoch.c_str(),nPos+208,16,true);
    seg_data.Put(as->szJulianDayAscendNode.c_str(),nPos+224,16,true);
    seg_data.Put(as->szEpochYear.c_str(),nPos+240,16,true);
    seg_data.Put(as->szEpochMonth.c_str(),nPos+256,16,true);
    seg_data.Put(as->szEpochDay.c_str(),nPos+272,16,true);
    seg_data.Put(as->szEpochHour.c_str(),nPos+288,16,true);
    seg_data.Put(as->szEpochMinute.c_str(),nPos+304,16,true);
    seg_data.Put(as->szEpochSecond.c_str(),nPos+320,16,true);
    seg_data.Put(as->szPointOfAriesDegrees.c_str(),nPos+336,16,true);
    seg_data.Put(as->szAnomalisticPeriod.c_str(),nPos+352,16,true);
    seg_data.Put(as->szNodalPeriod.c_str(),nPos+368,16,true);
    seg_data.Put(as->szEccentricity.c_str(), nPos+384,16,true);
    seg_data.Put(as->szArgumentOfPerigee.c_str(),nPos+400,16,true);
    seg_data.Put(as->szRAAN.c_str(),nPos+416,16,true);
    seg_data.Put(as->szInclination.c_str(),nPos+432,16,true);
    seg_data.Put(as->szMeanAnomaly.c_str(),nPos+448,16,true);
    seg_data.Put(as->szSemiMajorAxis.c_str(),nPos+464,16,true);

/* -------------------------------------------------------------------- */
/*  second avhrr block is all zeros.                                    */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*  Write the 3rd avhrr Block.                                          */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*2;

    seg_data.Put(as->nRecordSize,nPos,16);
    seg_data.Put(as->nBlockSize,nPos+16,16);
    seg_data.Put(as->nNumRecordsPerBlock,nPos+32,16);
    seg_data.Put(as->nNumBlocks,nPos+48,16);
    seg_data.Put(as->nNumScanlineRecords,nPos+64,16);

/* -------------------------------------------------------------------- */
/*  Write the fourth avhrr block onwards.                               */
/* -------------------------------------------------------------------- */
    if ( as->Line.empty() )
        return;

    nBlock = 4;

    if ( as->nNumRecordsPerBlock == 0 )
        return;

    for(nLine = 0; nLine < as->nNumScanlineRecords;
                   nLine += as->nNumRecordsPerBlock)
    {
        int nNumRecords = MinFunction(as->nNumRecordsPerBlock,
                                      as->nNumScanlineRecords - nLine);
        nPos = nStartBlock + (nBlock-1) * 512;

        for(int i = 0; i < nNumRecords; ++i)
        {
            WriteAvhrrScanlineRecord(&(as->Line[nLine+i]), nPos + i*80);
        }

        ++nBlock;
    }
}

/************************************************************************/
/*                       WriteAvhrrScanlineRecord()                     */
/************************************************************************/
/**
 * Write a scanline record to a byte buffer.
 * @param psScanlineRecord the record to write
 * @param nPos position in buffer
 */
void
CPCIDSKEphemerisSegment::WriteAvhrrScanlineRecord(
                                         AvhrrLine_t *psScanlineRecord,
                                         int nPos)
{
    int i;
    AvhrrLine_t *sr = psScanlineRecord;
    unsigned char* b = (unsigned char*)&(seg_data.buffer[nPos]);

    WriteAvhrrInt32(sr->nScanLineNum, b);
    WriteAvhrrInt32(sr->nStartScanTimeGMTMsec, b+4);

    for(i=0 ; i < 10 ; i++)
        seg_data.Put(sr->abyScanLineQuality[i],nPos+8+i,1);

    for(i = 0; i < 5; ++i)
    {
        seg_data.Put(sr->aabyBadBandIndicators[i][0],nPos+18+i*2,1);
        seg_data.Put(sr->aabyBadBandIndicators[i][1],nPos+18+i*2+1,1);
    }

    for(i=0 ; i < 8 ; i++)
        seg_data.Put(sr->abySatelliteTimeCode[i],nPos+28+i,1);

    for(i = 0; i < 3; ++i)
        WriteAvhrrInt32(sr->anTargetTempData[i], b+(36+i*4));
    for(i = 0; i < 3; ++i)
        WriteAvhrrInt32(sr->anTargetScanData[i], b+(48+i*4));
    for(i = 0; i < 5; ++i)
        WriteAvhrrInt32(sr->anSpaceScanData[i],  b+(60+i*4));

}

/************************************************************************/
/*                         WriteAvhrrInt32()                            */
/************************************************************************/
/**
 * Write an integer into a given buffer of at least 4 bytes.
 * @param nValue the value to write
 * @param pbyBuf the buffer to write into.
 */
void CPCIDSKEphemerisSegment::WriteAvhrrInt32(int nValue,
                                              unsigned char* pbyBuf)
{
    pbyBuf[0] = static_cast<unsigned char>((nValue & 0xff000000) >> 24);
    pbyBuf[1] = static_cast<unsigned char>((nValue & 0x00ff0000) >> 16);
    pbyBuf[2] = static_cast<unsigned char>((nValue & 0x0000ff00) >> 8);
    pbyBuf[3] = static_cast<unsigned char>(nValue & 0x000000ff);
}


/************************************************************************/
/*                        BinaryToEphemeris()                           */
/************************************************************************/
/**
 * Read binary information from a binary buffer to create an
 * EphemerisSeg_t structure. The caller is responsible to free the memory
 * of the returned structure with delete.
 *
 * @param nStartBlock where to start read the orbit info into the buffer.
 * @return the orbit information.
 */
EphemerisSeg_t *
CPCIDSKEphemerisSegment::BinaryToEphemeris( int nStartBlock )

{
    EphemerisSeg_t *l_segment;
    int             i;
    int nPos = nStartBlock;

    l_segment = new EphemerisSeg_t();

    std::unique_ptr<EphemerisSeg_t> oSegmentAutoPtr(l_segment);

/* -------------------------------------------------------------------- */
/*      Process first block.                                            */
/* -------------------------------------------------------------------- */

    l_segment->SatelliteDesc = seg_data.Get(nPos+8,32);
    l_segment->SceneID = seg_data.Get(nPos+40, 32);

/* -------------------------------------------------------------------- */
/*      Process the second block.                                       */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512;

    l_segment->SatelliteSensor = seg_data.Get(nPos, 16);
    for (i=0; i<16; i++)
    {
        if (l_segment->SatelliteSensor[i] == ' ')
        {
            l_segment->SatelliteSensor = l_segment->SatelliteSensor.substr(0,i);
            break;
        }
    }

    l_segment->SensorNo = seg_data.Get(nPos+22, 2);
    l_segment->DateImageTaken = seg_data.Get(nPos+44, 22);

    if (seg_data.buffer[nPos+66] == 'Y' ||
        seg_data.buffer[nPos+66] == 'y')
        l_segment->SupSegExist = true;
    else
        l_segment->SupSegExist = false;
    l_segment->FieldOfView = seg_data.GetDouble(nPos+88, 22);
    l_segment->ViewAngle = seg_data.GetDouble(nPos+110, 22);
    l_segment->NumColCentre = seg_data.GetDouble(nPos+132, 22);
    l_segment->RadialSpeed = seg_data.GetDouble(nPos+154, 22);
    l_segment->Eccentricity = seg_data.GetDouble(nPos+176, 22);
    l_segment->Height = seg_data.GetDouble(nPos+198, 22);
    l_segment->Inclination = seg_data.GetDouble(nPos+220, 22);
    l_segment->TimeInterval = seg_data.GetDouble(nPos+242, 22);
    l_segment->NumLineCentre = seg_data.GetDouble(nPos+264, 22);
    l_segment->LongCentre = seg_data.GetDouble(nPos+286, 22);
    l_segment->AngularSpd = seg_data.GetDouble(nPos+308, 22);
    l_segment->AscNodeLong = seg_data.GetDouble(nPos+330, 22);
    l_segment->ArgPerigee = seg_data.GetDouble(nPos+352, 22);
    l_segment->LatCentre = seg_data.GetDouble(nPos+374, 22);
    l_segment->EarthSatelliteDist = seg_data.GetDouble(nPos+396, 22);
    l_segment->NominalPitch = seg_data.GetDouble(nPos+418, 22);
    l_segment->TimeAtCentre = seg_data.GetDouble(nPos+440, 22);
    l_segment->SatelliteArg = seg_data.GetDouble(nPos+462, 22);
    l_segment->bDescending = true;
    if (seg_data.buffer[nPos+484] == 'A')
        l_segment->bDescending = false;

/* -------------------------------------------------------------------- */
/*      Process the third block.                                        */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 2*512;

    l_segment->XCentre = seg_data.GetDouble(nPos, 22);
    l_segment->YCentre = seg_data.GetDouble(nPos+22, 22);
    l_segment->UtmXCentre = seg_data.GetDouble(nPos+44, 22);
    l_segment->UtmYCentre = seg_data.GetDouble(nPos+66, 22);
    l_segment->PixelRes = seg_data.GetDouble(nPos+88, 22);
    l_segment->LineRes = seg_data.GetDouble(nPos+110, 22);
    if (seg_data.buffer[nPos+132] == 'Y' ||
        seg_data.buffer[nPos+132] == 'y')
        l_segment->CornerAvail = true;
    else
        l_segment->CornerAvail = false;
    l_segment->MapUnit = seg_data.Get(nPos+133, 16);

    l_segment->XUL = seg_data.GetDouble(nPos+149, 22);
    l_segment->YUL = seg_data.GetDouble(nPos+171, 22);
    l_segment->XUR = seg_data.GetDouble(nPos+193, 22);
    l_segment->YUR = seg_data.GetDouble(nPos+215, 22);
    l_segment->XLR = seg_data.GetDouble(nPos+237, 22);
    l_segment->YLR = seg_data.GetDouble(nPos+259, 22);
    l_segment->XLL = seg_data.GetDouble(nPos+281, 22);
    l_segment->YLL = seg_data.GetDouble(nPos+303, 22);
    l_segment->UtmXUL = seg_data.GetDouble(nPos+325, 22);
    l_segment->UtmYUL = seg_data.GetDouble(nPos+347, 22);
    l_segment->UtmXUR = seg_data.GetDouble(nPos+369, 22);
    l_segment->UtmYUR = seg_data.GetDouble(nPos+391, 22);
    l_segment->UtmXLR = seg_data.GetDouble(nPos+413, 22);
    l_segment->UtmYLR = seg_data.GetDouble(nPos+435, 22);
    l_segment->UtmXLL = seg_data.GetDouble(nPos+457, 22);
    l_segment->UtmYLL = seg_data.GetDouble(nPos+479, 22);

/* -------------------------------------------------------------------- */
/*      Process the 4th block (Corner lat/long coordinates)             */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 3*512;

    l_segment->LongCentreDeg = seg_data.GetDouble(nPos, 16);
    l_segment->LatCentreDeg = seg_data.GetDouble(nPos+16, 16);
    l_segment->LongUL =  seg_data.GetDouble(nPos+32, 16);
    l_segment->LatUL = seg_data.GetDouble(nPos+48, 16);
    l_segment->LongUR =  seg_data.GetDouble(nPos+64, 16);
    l_segment->LatUR = seg_data.GetDouble(nPos+80, 16);
    l_segment->LongLR = seg_data.GetDouble(nPos+96, 16);
    l_segment->LatLR = seg_data.GetDouble(nPos+112, 16);
    l_segment->LongLL = seg_data.GetDouble(nPos+128, 16);
    l_segment->LatLL = seg_data.GetDouble(nPos+144, 16);
    l_segment->HtCentre = seg_data.GetDouble(nPos+160, 16);
    l_segment->HtUL = seg_data.GetDouble(nPos+176, 16);
    l_segment->HtUR = seg_data.GetDouble(nPos+192, 16);
    l_segment->HtLR = seg_data.GetDouble(nPos+208, 16);
    l_segment->HtLL = seg_data.GetDouble(nPos+224, 16);

/* -------------------------------------------------------------------- */
/*      Process the 5th block.                                          */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*4;

    l_segment->ImageRecordLength = seg_data.GetInt(nPos, 16);
    l_segment->NumberImageLine = seg_data.GetInt(nPos+16, 16);
    l_segment->NumberBytePerPixel = seg_data.GetInt(nPos+32, 16);
    l_segment->NumberSamplePerLine = seg_data.GetInt(nPos+48, 16);
    l_segment->NumberPrefixBytes = seg_data.GetInt(nPos+64, 16);
    l_segment->NumberSuffixBytes = seg_data.GetInt(nPos+80, 16);

/* -------------------------------------------------------------------- */
/*      Process the 6th and 7th block.                                  */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 5*512;

    l_segment->SPNCoeff = 0;

    if(STARTS_WITH(seg_data.Get(nPos,8), "SPOT1BOD") ||
       STARTS_WITH(seg_data.Get(nPos,8), "SPOT1BNW"))
    {
        l_segment->SPNCoeff = seg_data.GetInt(nPos+22, 22);
        for (i=0; i<20; i++)
        {
            l_segment->SPCoeff1B[i] =
                seg_data.GetDouble(nPos+(i+2)*22, 22);
        }

        if (STARTS_WITH(seg_data.Get(nPos,8), "SPOT1BNW"))
        {
            nPos = nStartBlock + 6*512;

            for (i=0; i<19; i++)
            {
                l_segment->SPCoeff1B[i+20] =
                    seg_data.GetDouble(nPos+i*22, 22);
            }
            l_segment->SPCoeffSg[0] = seg_data.GetInt(nPos+418, 8);
            l_segment->SPCoeffSg[1] = seg_data.GetInt(nPos+426, 8);
            l_segment->SPCoeffSg[2] = seg_data.GetInt(nPos+434, 8);
            l_segment->SPCoeffSg[3] = seg_data.GetInt(nPos+442, 8);
        }
    }

/* -------------------------------------------------------------------- */
/*      6th and 7th block of ORBIT segment are blank.                   */
/*      Read in the 8th block.                                          */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 7*512;

    if (STARTS_WITH(seg_data.Get(nPos,8), "ATTITUDE"))
        l_segment->Type = OrbAttitude;
    else if (STARTS_WITH(seg_data.Get(nPos,8), "RADAR   "))
        l_segment->Type = OrbLatLong;
    else if (STARTS_WITH(seg_data.Get(nPos,8), "AVHRR   "))
        l_segment->Type = OrbAvhrr;
    else if (STARTS_WITH(seg_data.Get(nPos,8), "NO_DATA "))
        l_segment->Type = OrbNone;
    else
        return (EphemerisSeg_t*)ThrowPCIDSKExceptionPtr("Invalid Orbit type found: [%s]",
                              seg_data.Get(nPos,8));

/* -------------------------------------------------------------------- */
/*      Orbit segment is a Satellite Attitude Segment(ATTITUDE) only    */
/*      for SPOT 1A.                                                    */
/* -------------------------------------------------------------------- */
    if (l_segment->Type == OrbAttitude)
    {
        AttitudeSeg_t  *AttitudeSeg;
        int            nBlock, nData;

        AttitudeSeg = l_segment->AttitudeSeg = new AttitudeSeg_t();

/* -------------------------------------------------------------------- */
/*      Read in the 9th block.                                          */
/* -------------------------------------------------------------------- */
        nPos = nStartBlock + 512*8;

        AttitudeSeg->Roll = seg_data.GetDouble(nPos, 22);
        AttitudeSeg->Pitch = seg_data.GetDouble(nPos+22, 22);
        AttitudeSeg->Yaw = seg_data.GetDouble(nPos+44, 22);
        AttitudeSeg->NumberOfLine = seg_data.GetInt(nPos+88, 22);
        if (AttitudeSeg->NumberOfLine % ATT_SEG_LINE_PER_BLOCK != 0)
            AttitudeSeg->NumberBlockData = 1 +
                AttitudeSeg->NumberOfLine / ATT_SEG_LINE_PER_BLOCK;
        else
            AttitudeSeg->NumberBlockData =
                AttitudeSeg->NumberOfLine / ATT_SEG_LINE_PER_BLOCK;

/* -------------------------------------------------------------------- */
/*      Read in the line required.                                      */
/* -------------------------------------------------------------------- */
        for (nBlock=0, nData=0; nBlock<AttitudeSeg->NumberBlockData;
             nBlock++)
        {
/* -------------------------------------------------------------------- */
/*      Read in 10+nBlock th block as required.                         */
/* -------------------------------------------------------------------- */
            nPos = nStartBlock + 512*(9+nBlock);

/* -------------------------------------------------------------------- */
/*      Fill in the lines as required.                                  */
/* -------------------------------------------------------------------- */
            for (i=0;
                 i<ATT_SEG_LINE_PER_BLOCK
                    && nData < AttitudeSeg->NumberOfLine;
                 i++, nData++)
            {
                AttitudeLine_t oAttitudeLine;
                oAttitudeLine.ChangeInAttitude
                    = seg_data.GetDouble(nPos+i*44, 22);
                oAttitudeLine.ChangeEarthSatelliteDist
                    = seg_data.GetDouble(nPos+i*44+22, 22);
                AttitudeSeg->Line.push_back(oAttitudeLine);
            }
        }

        if (nData != AttitudeSeg->NumberOfLine)
        {
            return (EphemerisSeg_t*)ThrowPCIDSKExceptionPtr("Number of data line read (%d) "
                     "does not matches with what is specified in "
                     "the segment (%d).\n", nData,
                     AttitudeSeg->NumberOfLine);
        }
    }
/* -------------------------------------------------------------------- */
/*      Radar segment (LATLONG)                                         */
/* -------------------------------------------------------------------- */
    else if (l_segment->Type == OrbLatLong)
    {
        RadarSeg_t *RadarSeg;
        int         nBlock, nData;

        RadarSeg = l_segment->RadarSeg = new RadarSeg_t();
/* -------------------------------------------------------------------- */
/*      Read in the 9th block.                                          */
/* -------------------------------------------------------------------- */
        nPos = nStartBlock + 512*8;

        RadarSeg->Identifier = seg_data.Get(nPos, 16);
        RadarSeg->Facility = seg_data.Get(nPos+16, 16);
        RadarSeg->Ellipsoid = seg_data.Get(nPos+32, 16);

        RadarSeg->EquatorialRadius = seg_data.GetDouble(nPos+48, 16);
        RadarSeg->PolarRadius = seg_data.GetDouble(nPos+64, 16);
        RadarSeg->IncidenceAngle = seg_data.GetDouble(nPos+80, 16);
        RadarSeg->LineSpacing = seg_data.GetDouble(nPos+96, 16);
        RadarSeg->PixelSpacing = seg_data.GetDouble(nPos+112, 16);
        RadarSeg->ClockAngle = seg_data.GetDouble(nPos+128, 16);

/* -------------------------------------------------------------------- */
/*      Read in the 10th block.                                         */
/* -------------------------------------------------------------------- */
        nPos = nStartBlock + 9*512;

        RadarSeg->NumberBlockData = seg_data.GetInt(nPos, 8);
        RadarSeg->NumberData = seg_data.GetInt(nPos+8, 8);

/* -------------------------------------------------------------------- */
/*      Read in the 11-th through 11+RadarSeg->NumberBlockData th block */
/*      for the ancillary data present.                                 */
/* -------------------------------------------------------------------- */
        for (nBlock = 0, nData = 0;
             nBlock < RadarSeg->NumberBlockData; nBlock++)
        {
/* -------------------------------------------------------------------- */
/*      Read in one block of data.                                      */
/* -------------------------------------------------------------------- */
            nPos = nStartBlock + 512*(10+nBlock);

            for (i=0;
                 i<ANC_DATA_PER_BLK &&  nData < RadarSeg->NumberData;
                 i++, nData++)
            {
                int     offset;
                char   *currentindex;
                void   *currentptr;
                double  tmp;
                int32   tmpInt;
                const double million = 1000000.0;

/* -------------------------------------------------------------------- */
/*      Reading in one ancillary data at a time.                        */
/* -------------------------------------------------------------------- */
                AncillaryData_t oData;
                offset = i*ANC_DATA_SIZE;

                currentindex = (char *)seg_data.Get(nPos+offset,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                oData.SlantRangeFstPixel = tmpInt;

                currentindex = (char *)seg_data.Get(nPos+offset+4,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                oData.SlantRangeLastPixel = tmpInt;

                currentindex = (char *)seg_data.Get(nPos+offset+8,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.FstPixelLat
                    = (float) ConvertDeg(tmp, 0);

                currentindex = (char *)seg_data.Get(nPos+offset+12,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.MidPixelLat
                    = (float) ConvertDeg(tmp, 0);

                currentindex = (char *)seg_data.Get(nPos+offset+16,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.LstPixelLat
                    = (float) ConvertDeg(tmp, 0);

                currentindex = (char *)seg_data.Get(nPos+offset+20,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.FstPixelLong
                    = (float) ConvertDeg(tmp, 0);

                currentindex = (char *)seg_data.Get(nPos+offset+24,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.MidPixelLong
                    = (float) ConvertDeg(tmp, 0);

                currentindex = (char *)seg_data.Get(nPos+offset+28,4);
                currentptr = (char *) currentindex;
                SwapData(currentptr,4,1);
                tmpInt = *((int32 *) currentptr);
                tmp = (double) tmpInt / million;
                oData.LstPixelLong
                    = (float) ConvertDeg(tmp, 0);

                RadarSeg->Line.push_back(oData);
            }
        }

        if (RadarSeg->NumberData != nData)
        {
            return (EphemerisSeg_t*)ThrowPCIDSKExceptionPtr("Number "
                     "of data lines read (%d) does not match with"
                     "\nwhat is specified in segment (%d).\n", nData,
                     RadarSeg->NumberData);
        }
    }
/* -------------------------------------------------------------------- */
/*      AVHRR segment                                                   */
/* -------------------------------------------------------------------- */
    else if (l_segment->Type == OrbAvhrr)
    {
        ReadAvhrrEphemerisSegment( nStartBlock, l_segment);
    }

    oSegmentAutoPtr.release();

    return l_segment;
}

/************************************************************************/
/*                        EphemerisToBinary()                           */
/************************************************************************/
/**
 * Write an Orbit segment information into a binary buffer of size 4096.
 * The caller is responsible to free this memory with delete [].
 *
 * @param psOrbit the orbit information to write into the binary
 * @param nStartBlock where to start writing in the buffer.
 */
void
CPCIDSKEphemerisSegment::EphemerisToBinary( EphemerisSeg_t * psOrbit,
                                              int nStartBlock )

{
    int i,j;

/* -------------------------------------------------------------------- */
/*      The binary data must be at least 8 blocks (4096 bytes) long     */
/*      for the common information.                                     */
/* -------------------------------------------------------------------- */
    seg_data.SetSize(nStartBlock+4096);
    memset(seg_data.buffer+nStartBlock,' ',4096);

    int nPos = nStartBlock;

/* -------------------------------------------------------------------- */
/*      Write the first block                                           */
/* -------------------------------------------------------------------- */

    seg_data.Put("ORBIT   ",nPos,8);
    seg_data.Put(psOrbit->SatelliteDesc.c_str(), nPos+8,32,true);
    seg_data.Put(psOrbit->SceneID.c_str(), nPos+40,32,true);

/* -------------------------------------------------------------------- */
/*      Write the second block                                          */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 1*512;

    seg_data.Put(psOrbit->SatelliteSensor.c_str(), nPos,16);
    seg_data.Put(psOrbit->SensorNo.c_str(),nPos+22,2,true);
    seg_data.Put(psOrbit->DateImageTaken.c_str(), nPos+44,22,true);

    if (psOrbit->SupSegExist)
        seg_data.Put("Y",nPos+66,1);
    else
        seg_data.Put("N",nPos+66,1);

    seg_data.Put(psOrbit->FieldOfView,nPos+88,22,"%22.14f");
    seg_data.Put(psOrbit->ViewAngle,nPos+110,22,"%22.14f");
    seg_data.Put(psOrbit->NumColCentre,nPos+132,22,"%22.14f");
    seg_data.Put(psOrbit->RadialSpeed,nPos+154,22,"%22.14f");
    seg_data.Put(psOrbit->Eccentricity,nPos+176,22,"%22.14f");
    seg_data.Put(psOrbit->Height,nPos+198,22,"%22.14f");
    seg_data.Put(psOrbit->Inclination,nPos+220,22,"%22.14f");
    seg_data.Put(psOrbit->TimeInterval,nPos+242,22,"%22.14f");
    seg_data.Put(psOrbit->NumLineCentre,nPos+264,22,"%22.14f");
    seg_data.Put(psOrbit->LongCentre,nPos+286,22,"%22.14f");
    seg_data.Put(psOrbit->AngularSpd,nPos+308,22,"%22.14f");
    seg_data.Put(psOrbit->AscNodeLong,nPos+330,22,"%22.14f");
    seg_data.Put(psOrbit->ArgPerigee,nPos+352,22,"%22.14f");
    seg_data.Put(psOrbit->LatCentre,nPos+374,22,"%22.14f");
    seg_data.Put(psOrbit->EarthSatelliteDist,nPos+396,22,"%22.14f");
    seg_data.Put(psOrbit->NominalPitch,nPos+418,22,"%22.14f");
    seg_data.Put(psOrbit->TimeAtCentre,nPos+440,22,"%22.14f");
    seg_data.Put(psOrbit->SatelliteArg,nPos+462,22,"%22.14f");

    if (psOrbit->bDescending)
        seg_data.Put("DESCENDING",nPos+484,10);
    else
        seg_data.Put("ASCENDING ",nPos+484,10);

/* -------------------------------------------------------------------- */
/*      Write the third block                                           */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*2;

    seg_data.Put(psOrbit->XCentre,nPos,22,"%22.14f");
    seg_data.Put(psOrbit->YCentre,nPos+22,22,"%22.14f");
    seg_data.Put(psOrbit->UtmXCentre,nPos+44,22,"%22.14f");
    seg_data.Put(psOrbit->UtmYCentre,nPos+66,22,"%22.14f");
    seg_data.Put(psOrbit->PixelRes,nPos+88,22,"%22.14f");
    seg_data.Put(psOrbit->LineRes,nPos+110,22,"%22.14f");

    if (psOrbit->CornerAvail == true)
        seg_data.Put("Y",nPos+132,1);
    else
        seg_data.Put("N",nPos+132,1);

    seg_data.Put(psOrbit->MapUnit.c_str(),nPos+133,16,true);

    seg_data.Put(psOrbit->XUL,nPos+149,22,"%22.14f");
    seg_data.Put(psOrbit->YUL,nPos+171,22,"%22.14f");
    seg_data.Put(psOrbit->XUR,nPos+193,22,"%22.14f");
    seg_data.Put(psOrbit->YUR,nPos+215,22,"%22.14f");
    seg_data.Put(psOrbit->XLR,nPos+237,22,"%22.14f");
    seg_data.Put(psOrbit->YLR,nPos+259,22,"%22.14f");
    seg_data.Put(psOrbit->XLL,nPos+281,22,"%22.14f");
    seg_data.Put(psOrbit->YLL,nPos+303,22,"%22.14f");
    seg_data.Put(psOrbit->UtmXUL,nPos+325,22,"%22.14f");
    seg_data.Put(psOrbit->UtmYUL,nPos+347,22,"%22.14f");
    seg_data.Put(psOrbit->UtmXUR,nPos+369,22,"%22.14f");
    seg_data.Put(psOrbit->UtmYUR,nPos+391,22,"%22.14f");
    seg_data.Put(psOrbit->UtmXLR,nPos+413,22,"%22.14f");
    seg_data.Put(psOrbit->UtmYLR,nPos+435,22,"%22.14f");
    seg_data.Put(psOrbit->UtmXLL,nPos+457,22,"%22.14f");
    seg_data.Put(psOrbit->UtmYLL,nPos+479,22,"%22.14f");

/* -------------------------------------------------------------------- */
/*      Write the fourth block                                          */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*3;

    seg_data.Put(psOrbit->LongCentreDeg,nPos,22,"%16.7f");
    seg_data.Put(psOrbit->LatCentreDeg,nPos+16,22,"%16.7f");
    seg_data.Put(psOrbit->LongUL,nPos+32,22,"%16.7f");
    seg_data.Put(psOrbit->LatUL,nPos+48,22,"%16.7f");
    seg_data.Put(psOrbit->LongUR,nPos+64,22,"%16.7f");
    seg_data.Put(psOrbit->LatUR,nPos+80,22,"%16.7f");
    seg_data.Put(psOrbit->LongLR,nPos+96,22,"%16.7f");
    seg_data.Put(psOrbit->LatLR,nPos+112,22,"%16.7f");
    seg_data.Put(psOrbit->LongLL,nPos+128,22,"%16.7f");
    seg_data.Put(psOrbit->LatLL,nPos+144,22,"%16.7f");
    seg_data.Put(psOrbit->HtCentre,nPos+160,22,"%16.7f");
    seg_data.Put(psOrbit->HtUL,nPos+176,22,"%16.7f");
    seg_data.Put(psOrbit->HtUR,nPos+192,22,"%16.7f");
    seg_data.Put(psOrbit->HtLR,nPos+208,22,"%16.7f");
    seg_data.Put(psOrbit->HtLL,nPos+224,22,"%16.7f");

/* -------------------------------------------------------------------- */
/*      Write the fifth block                                           */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*4;

    seg_data.Put(psOrbit->ImageRecordLength,nPos,16);
    seg_data.Put(psOrbit->NumberImageLine,nPos+16,16);
    seg_data.Put(psOrbit->NumberBytePerPixel,nPos+32,16);
    seg_data.Put(psOrbit->NumberSamplePerLine,nPos+48,16);
    seg_data.Put(psOrbit->NumberPrefixBytes,nPos+64,16);
    seg_data.Put(psOrbit->NumberSuffixBytes,nPos+80,16);

/* -------------------------------------------------------------------- */
/*      Write the sixth and seventh block (blanks)                      */
/*      For SPOT it is not blank                                        */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*5;

    if (psOrbit->SPNCoeff > 0)
    {
        if (psOrbit->SPNCoeff == 20)
        {
            seg_data.Put("SPOT1BOD",nPos,8);
            seg_data.Put(psOrbit->SPNCoeff,nPos+22,22);

            j = 44;
            for (i=0; i<20; i++)
            {
                seg_data.Put(psOrbit->SPCoeff1B[i],
                                     nPos+j,22,"%22.14f");
                j += 22;
            }
        }
        else
        {
            seg_data.Put("SPOT1BNW",nPos,8);
            seg_data.Put(psOrbit->SPNCoeff,nPos+22,22);

            j = 44;
            for (i=0; i<20; i++)
            {
                seg_data.Put(psOrbit->SPCoeff1B[i],
                                     nPos+j,22,"%22.14f");
                j += 22;
            }

            nPos = nStartBlock + 512*6;

            j = 0;
            for (i=20; i<39; i++)
            {
                seg_data.Put(psOrbit->SPCoeff1B[i],
                                     nPos+j,22,"%22.14f");
                j += 22;
            }

            seg_data.Put(psOrbit->SPCoeffSg[0],nPos+418,8);
            seg_data.Put(psOrbit->SPCoeffSg[1],nPos+426,8);
            seg_data.Put(psOrbit->SPCoeffSg[2],nPos+434,8);
            seg_data.Put(psOrbit->SPCoeffSg[3],nPos+442,8);
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the eighth block.                                         */
/* -------------------------------------------------------------------- */
    nPos = nStartBlock + 512*7;

    if (psOrbit->Type == OrbAttitude)
        seg_data.Put("ATTITUDE",nPos,8);
    else if (psOrbit->Type == OrbLatLong)
        seg_data.Put("RADAR   ",nPos,8);
    else if (psOrbit->Type == OrbAvhrr)
        seg_data.Put("AVHRR   ",nPos,8);
    else if (psOrbit->Type == OrbNone)
        seg_data.Put("NO_DATA ",nPos,8);
    else
    {
        return ThrowPCIDSKException("Invalid Orbit type.");
    }

/* ==================================================================== */
/*      Orbit segment is a Satellite Attitude Segment(ATTITUDE) only    */
/*      for SPOT 1A.                                                    */
/* ==================================================================== */
    if (psOrbit->Type == OrbAttitude)
    {
        AttitudeSeg_t *AttitudeSeg;
        int             nBlock, nData;

        AttitudeSeg = psOrbit->AttitudeSeg;

        if (AttitudeSeg == nullptr)
        {
            return ThrowPCIDSKException("The AttitudeSeg is NULL.");
        }

/* -------------------------------------------------------------------- */
/*      Add one block                                                   */
/* -------------------------------------------------------------------- */
        seg_data.SetSize(seg_data.buffer_size + 512);

        nPos = nStartBlock + 512*8;
        memset(seg_data.buffer+nPos,' ',512);

/* -------------------------------------------------------------------- */
/*      Write the ninth block.                                          */
/* -------------------------------------------------------------------- */

        seg_data.Put(AttitudeSeg->Roll,nPos,22,"%22.14f");
        seg_data.Put(AttitudeSeg->Pitch,nPos+22,22,"%22.14f");
        seg_data.Put(AttitudeSeg->Yaw,nPos+44,22,"%22.14f");

        if (AttitudeSeg->NumberOfLine % ATT_SEG_LINE_PER_BLOCK != 0)
            AttitudeSeg->NumberBlockData = 1 +
                AttitudeSeg->NumberOfLine / ATT_SEG_LINE_PER_BLOCK;
        else
            AttitudeSeg->NumberBlockData =
                AttitudeSeg->NumberOfLine / ATT_SEG_LINE_PER_BLOCK;

        seg_data.Put(AttitudeSeg->NumberBlockData,nPos+66,22);
        seg_data.Put(AttitudeSeg->NumberOfLine,nPos+88,22);

/* -------------------------------------------------------------------- */
/*      Add NumberBlockData blocks to array.                            */
/* -------------------------------------------------------------------- */
        seg_data.SetSize(seg_data.buffer_size +
                                 512 * AttitudeSeg->NumberBlockData);

        nPos = nStartBlock + 512*9;
        memset(seg_data.buffer+nPos,' ',
               512 * AttitudeSeg->NumberBlockData);

/* -------------------------------------------------------------------- */
/*      Write out the line required.                                    */
/* -------------------------------------------------------------------- */
        for (nBlock=0, nData=0; nBlock<AttitudeSeg->NumberBlockData;
             nBlock++)
        {
            nPos = nStartBlock + 512*(nBlock + 9);

/* -------------------------------------------------------------------- */
/*      Fill in buffer as required.                                     */
/* -------------------------------------------------------------------- */
            for (i=0;
                i<ATT_SEG_LINE_PER_BLOCK
                    && nData < AttitudeSeg->NumberOfLine;
                i++, nData++)
            {
                seg_data.Put(
                    AttitudeSeg->Line[nData].ChangeInAttitude,
                    nPos+i*44,22,"%22.14f");
                seg_data.Put(
                    AttitudeSeg->Line[nData].ChangeEarthSatelliteDist,
                    nPos+i*44+22,22,"%22.14f");
            }
        }

        if (nData != AttitudeSeg->NumberOfLine)
        {
            return ThrowPCIDSKException("Number of data line written"
                    " (%d) does not match with\nwhat is specified "
                    " in the segment (%d).\n",
                    nData, AttitudeSeg->NumberOfLine);
        }
    }

/* ==================================================================== */
/*      Radar segment (LATLONG)                                         */
/* ==================================================================== */
    else if (psOrbit->Type == OrbLatLong)
    {
        RadarSeg_t *RadarSeg;
        int         nBlock, nData;

        RadarSeg = psOrbit->RadarSeg;

        if (RadarSeg == nullptr)
        {
            return ThrowPCIDSKException("The RadarSeg is NULL.");
        }

/* -------------------------------------------------------------------- */
/*      Add two blocks.                                                 */
/* -------------------------------------------------------------------- */
        seg_data.SetSize(seg_data.buffer_size + 512*2);

        nPos = nStartBlock + 512*8;
        memset(seg_data.buffer+nPos,' ', 512*2);

/* -------------------------------------------------------------------- */
/*      Write out the ninth block.                                      */
/* -------------------------------------------------------------------- */
        seg_data.Put(RadarSeg->Identifier.c_str(), nPos,16);
        seg_data.Put(RadarSeg->Facility.c_str(), nPos+16,16);
        seg_data.Put(RadarSeg->Ellipsoid.c_str(), nPos+32,16);

        seg_data.Put(RadarSeg->EquatorialRadius,nPos+48,16,"%16.7f");
        seg_data.Put(RadarSeg->PolarRadius,nPos+64,16,"%16.7f");
        seg_data.Put(RadarSeg->IncidenceAngle,nPos+80,16,"%16.7f");
        seg_data.Put(RadarSeg->LineSpacing,nPos+96,16,"%16.7f");
        seg_data.Put(RadarSeg->PixelSpacing,nPos+112,16,"%16.7f");
        seg_data.Put(RadarSeg->ClockAngle,nPos+128,16,"%16.7f");

/* -------------------------------------------------------------------- */
/*      Write out the tenth block.                                      */
/* -------------------------------------------------------------------- */
        nPos = nStartBlock + 512*9;

        seg_data.Put(RadarSeg->NumberBlockData,nPos,8);
        seg_data.Put(RadarSeg->NumberData,nPos+8,8);

/* -------------------------------------------------------------------- */
/*      Make room for all the following per-line data.                  */
/* -------------------------------------------------------------------- */
        seg_data.SetSize(seg_data.buffer_size +
                                 512 * RadarSeg->NumberBlockData);

        nPos = nStartBlock + 512*10;
        memset(seg_data.buffer+nPos,' ',
               512 * RadarSeg->NumberBlockData);

/* -------------------------------------------------------------------- */
/*      Write out the 11-th through 11+psOrbit->NumberBlockData  block  */
/*      for the ancillary data present.                                 */
/* -------------------------------------------------------------------- */
        for (nBlock = 0, nData = 0;
             nBlock < RadarSeg->NumberBlockData; nBlock++)
        {
            for (i=0;
                 i<ANC_DATA_PER_BLK &&  nData < RadarSeg->NumberData;
                 i++, nData++)
            {
                int             offset;
                char            *currentptr, *currentindex;
                double          tmp, tmpDouble;
                const double    million = 1000000.0;
                int32           tmpInt;

/* -------------------------------------------------------------------- */
/*      Point to correct block                                          */
/* -------------------------------------------------------------------- */
                nPos = nStartBlock + 512*(10+nBlock);

/* -------------------------------------------------------------------- */
/*      Writing out one ancillary data at a time.                       */
/* -------------------------------------------------------------------- */
                offset = i*ANC_DATA_SIZE;

                currentptr =
                    (char *) &(RadarSeg->Line[nData].SlantRangeFstPixel);
                SwapData(currentptr,4,1);
                currentindex = &(seg_data.buffer[nPos+offset]);
                std::memcpy((void *) currentindex,currentptr, 4);

                currentptr =
                    (char *) &(RadarSeg->Line[nData].SlantRangeLastPixel);
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].FstPixelLat, 1);
                tmpDouble =  tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].MidPixelLat, 1);
                tmpDouble =  tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].LstPixelLat, 1);
                tmpDouble =  tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].FstPixelLong, 1);
                tmpDouble =  tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].MidPixelLong, 1);
                tmpDouble = tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);

                tmp = ConvertDeg(RadarSeg->Line[nData].LstPixelLong, 1);
                tmpDouble =  tmp * million;
                tmpInt = (int32) tmpDouble;
                currentptr = (char *) &tmpInt;
                SwapData(currentptr,4,1);
                currentindex += 4;
                std::memcpy((void *) currentindex,currentptr, 4);
            }
        }
    }

/* ==================================================================== */
/*      AVHRR segment                                                   */
/* ==================================================================== */
    else if ( psOrbit->Type == OrbAvhrr &&
              psOrbit->AvhrrSeg->nNumRecordsPerBlock > 0 )
    {
        WriteAvhrrEphemerisSegment(nStartBlock + 8*512 , psOrbit);
    }
}
