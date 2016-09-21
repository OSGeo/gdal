/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Ephemeris Segments
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

        const EphemerisSeg_t& GetEphemeris() const
        {
            return *mpoEphemeris;
        };
        void SetEphemeris(const EphemerisSeg_t& oEph)
        {
            if(mpoEphemeris)
            {
                delete mpoEphemeris;
            }
            mpoEphemeris = new EphemerisSeg_t(oEph);
            mbModified = true;
        };

        //synchronize the segment on disk.
        void Synchronize();
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
