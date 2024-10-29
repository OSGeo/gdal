/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Ephemeris Segments
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_SEGMENT_PCIDSKEPHEMERIS_SEG_H
#define INCLUDE_PCIDSK_SEGMENT_PCIDSKEPHEMERIS_SEG_H

#include "pcidsk_ephemeris.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CPCIDSKEphemerisSegment : public PCIDSKEphemerisSegment,
                                    public CPCIDSKSegment
    {
    public:
        CPCIDSKEphemerisSegment(PCIDSKFile *file, int segment,const char *segment_pointer,bool bLoad=true);
        ~CPCIDSKEphemerisSegment();

        const EphemerisSeg_t& GetEphemeris() const override
        {
            return *mpoEphemeris;
        }
        void SetEphemeris(const EphemerisSeg_t& oEph) override
        {
            if(mpoEphemeris)
            {
                delete mpoEphemeris;
            }
            mpoEphemeris = new EphemerisSeg_t(oEph);
            mbModified = true;

            //we set loaded to true to trigger the Write during synchronize
            //else if the segment has just been created it will not be saved.
            this->loaded_ = true;
        }

        //synchronize the segment on disk.
        void Synchronize() override;
    private:

        // Helper housekeeping functions
        void Load();
        void Write();

        EphemerisSeg_t* mpoEphemeris;
    //functions to read/write binary information
    protected:
        // The raw segment data
        PCIDSKBuffer seg_data;
        bool loaded_;
        bool mbModified;
        void ReadAvhrrEphemerisSegment(int,
                                       EphemerisSeg_t *);
        void ReadAvhrrScanlineRecord(int nPos,
                                     AvhrrLine_t *psScanlineRecord);
        int  ReadAvhrrInt32(unsigned char* pbyBuf);
        void WriteAvhrrEphemerisSegment(int , EphemerisSeg_t *);
        void WriteAvhrrScanlineRecord(AvhrrLine_t *psScanlineRecord,
                                      int nPos);
        void WriteAvhrrInt32(int nValue, unsigned char* pbyBuf);
        EphemerisSeg_t *BinaryToEphemeris( int nStartBlock );
        void EphemerisToBinary( EphemerisSeg_t *, int );
        double ConvertDeg(double degree, int mode);
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKEPHEMERIS_SEG_H
