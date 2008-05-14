/******************************************************************************
 * $Id$
 *
 * Project:  NOAA Polar Orbiter Level 1b Dataset Reader (AVHRR)
 * Purpose:  Can read NOAA-9(F)-NOAA-17(M) AVHRR datasets
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 * Some format info at: http://www.sat.dundee.ac.uk/noaa1b.html
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "gdal_pam.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_L1B(void);
CPL_C_END

enum {          // Spacecrafts:
    TIROSN,     // TIROS-N
    NOAA6,      // NOAA-6(A)
    NOAAB,      // NOAA-B
    NOAA7,      // NOAA-7(C)
    NOAA8,      // NOAA-8(E)
    NOAA9,      // NOAA-9(F)
    NOAA10,     // NOAA-10(G)
    NOAA11,     // NOAA-11(H)
    NOAA12,     // NOAA-12(D)
    NOAA13,     // NOAA-13(I)
    NOAA14,     // NOAA-14(J)
    NOAA15,     // NOAA-15(K)
    NOAA16,     // NOAA-16(L)
    NOAA17,     // NOAA-17(M)
    NOAA18,     // NOAA-18(N)
    METOP2      // METOP-2(A)
};

enum {          // Product types
    HRPT,
    LAC,
    GAC,
    FRAC
};

enum {          // Data format
    PACKED10BIT,
    UNPACKED8BIT,
    UNPACKED16BIT
};

enum {          // Receiving stations names:
    DU,         // Dundee, Scotland, UK
    GC,         // Fairbanks, Alaska, USA (formerly Gilmore Creek)
    HO,         // Honolulu, Hawaii, USA
    MO,         // Monterey, California, USA
    WE,         // Western Europe CDA, Lannion, France
    SO,         // SOCC (Satellite Operations Control Center), Suitland, Maryland, USA
    WI,         // Wallops Island, Virginia, USA
    UNKNOWN_STATION
};

enum {          // Data processing centers:
    CMS,        // Centre de Meteorologie Spatiale - Lannion, France
    DSS,        // Dundee Satellite Receiving Station - Dundee, Scotland, UK
    NSS,        // NOAA/NESDIS - Suitland, Maryland, USA
    UKM,        // United Kingdom Meteorological Office - Bracknell, England, UK
    UNKNOWN_CENTER
};

enum {          // AVHRR Earth location indication
        ASCEND,
        DESCEND
};

/************************************************************************/
/*                      AVHRR band widths                               */
/************************************************************************/
static const char *apszBandDesc[] =
{
    // NOAA-7 -- METOP-2 channels
    "AVHRR Channel 1:  0.58  micrometers -- 0.68 micrometers",
    "AVHRR Channel 2:  0.725 micrometers -- 1.10 micrometers",
    "AVHRR Channel 3:  3.55  micrometers -- 3.93 micrometers",
    "AVHRR Channel 4:  10.3  micrometers -- 11.3 micrometers",
    "AVHRR Channel 5:  11.5  micrometers -- 12.5 micrometers",  // not in NOAA-6,-8,-10
    // NOAA-13
    "AVHRR Channel 5:  11.4  micrometers -- 12.4 micrometers",
    // NOAA-15 -- METOP-2
    "AVHRR Channel 3A: 1.58  micrometers -- 1.64 micrometers",
    "AVHRR Channel 3B: 3.55  micrometers -- 3.93 micrometers"
    };

#define TBM_HEADER_SIZE 122

#define DESIRED_GCPS_PER_LINE 11
#define DESIRED_LINES_OF_GCPS 20

/************************************************************************/
/* ==================================================================== */
/*                      TimeCode (helper class)                         */
/* ==================================================================== */
/************************************************************************/

#define L1B_TIMECODE_LENGTH 100
class TimeCode {
    long        lYear;
    long        lDay;
    long        lMillisecond;
    char        pszString[L1B_TIMECODE_LENGTH];

  public:
    void SetYear(long year)
    {
        lYear = year;
    }
    void SetDay(long day)
    {
        lDay = day;
    }
    void SetMillisecond(long millisecond)
    {
        lMillisecond = millisecond;
    }
    char* PrintTime()
    {
        snprintf(pszString, L1B_TIMECODE_LENGTH,
                 "year: %ld, day: %ld, millisecond: %ld",
                 lYear, lDay, lMillisecond);
        return pszString;
    }
};
#undef L1B_TIMECODE_LENGTH

/************************************************************************/
/* ==================================================================== */
/*                              L1BDataset                              */
/* ==================================================================== */
/************************************************************************/

class L1BDataset : public GDALPamDataset
{
    friend class L1BRasterBand;

    char        pszRevolution[6]; // Five-digit number identifying spacecraft revolution
    int         eSource;        // Source of data (receiving station name)
    int         eProcCenter;    // Data processing center
    TimeCode    sStartTime;
    TimeCode    sStopTime;

    GDAL_GCP    *pasGCPList;
    int         nGCPCount;
    int         iGCPOffset;
    int         iGCPCodeOffset;
    int         nGCPsPerLine;
    int         eLocationIndicator, iGCPStart, iGCPStep;

    int         nBufferSize;
    int         eSpacecraftID;
    int         eProductType;   // LAC, GAC, HRPT, FRAC
    int         iDataFormat;    // 10-bit packed or 16-bit unpacked
    int         nRecordDataStart;
    int         nRecordDataEnd;
    int         nDataStartOffset;
    int         nRecordSize;
    GUInt16     iInstrumentStatus;
    GUInt32     iChannels;

    char        *pszGCPProjection;

    FILE        *fp;

    void        ProcessRecordHeaders();
    void        FetchNOAA9GCPs(GDAL_GCP *pasGCPList, GInt16 *piRecordHeader, int iLine);
    void        FetchNOAA15GCPs(GDAL_GCP *pasGCPList, GInt32 *piRecordHeader, int iLine);
    void        FetchNOAA9TimeCode(TimeCode *psTime, GByte *piRecordHeader, int *iLocInd);
    void        FetchNOAA15TimeCode(TimeCode *psTime, GUInt16 *piRecordHeader, int *intLocInd);
    void        ProcessDatasetHeader();
    
  public:
                L1BDataset();
                ~L1BDataset();
    
    virtual int GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static int  Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );

};

/************************************************************************/
/* ==================================================================== */
/*                            L1BRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class L1BRasterBand : public GDALPamRasterBand
{
    friend class L1BDataset;

  public:

                L1BRasterBand( L1BDataset *, int );
    
//    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           L1BRasterBand()                            */
/************************************************************************/

L1BRasterBand::L1BRasterBand( L1BDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_UInt16;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr L1BRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    L1BDataset  *poGDS = (L1BDataset *) poDS;
    GUInt32     iword, jword;
    GUInt16     *iScan = NULL;          // Unpacked 16-bit scanline buffer
    int         iDataOffset, i, j;
            
/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    iDataOffset = (poGDS->eLocationIndicator == DESCEND)?
            poGDS->nDataStartOffset + nBlockYOff * poGDS->nRecordSize:
            poGDS->nDataStartOffset +
            (poGDS->GetRasterYSize() - nBlockYOff - 1) * poGDS->nRecordSize;
    VSIFSeekL(poGDS->fp, iDataOffset, SEEK_SET);

/* -------------------------------------------------------------------- */
/*      Read data into the buffer.                                      */
/* -------------------------------------------------------------------- */
    switch (poGDS->iDataFormat)
    {
        case PACKED10BIT:
            {
                // Read packed scanline
                GUInt32 *iRawScan = (GUInt32 *)CPLMalloc(poGDS->nRecordSize);
                VSIFReadL( iRawScan, 1, poGDS->nRecordSize, poGDS->fp );

                iScan = (GUInt16 *)CPLMalloc(poGDS->nBufferSize);
                j = 0;
                for(i = poGDS->nRecordDataStart / (int)sizeof(iRawScan[0]);
                    i < poGDS->nRecordDataEnd / (int)sizeof(iRawScan[0]); i++)
                {
                    iword = iRawScan[i];
#ifdef CPL_LSB
                    CPL_SWAP32PTR(&iword);
#endif
                    jword = iword & 0x3FF00000;
                    iScan[j++] = (GUInt16) (jword >> 20);
                    jword = iword & 0x000FFC00;
                    iScan[j++] = (GUInt16) (jword >> 10);
                    iScan[j++] = (GUInt16) (iword & 0x000003FF);
                }
                CPLFree(iRawScan);
            }
            break;
        case UNPACKED16BIT:
            {
                // Read unpacked scanline
                GUInt16 *iRawScan = (GUInt16 *)CPLMalloc(poGDS->nRecordSize);
                VSIFReadL( iRawScan, 1, poGDS->nRecordSize, poGDS->fp );

                iScan = (GUInt16 *)CPLMalloc(poGDS->GetRasterXSize()
                                             * poGDS->nBands * sizeof(GUInt16));
                for (i = 0; i < poGDS->GetRasterXSize() * poGDS->nBands; i++)
                {
                    iScan[i] = iRawScan[poGDS->nRecordDataStart
                        / (int)sizeof(iRawScan[0]) + i];
#ifdef CPL_LSB
                    CPL_SWAP16PTR(&iScan[i]);
#endif
                }
                CPLFree(iRawScan);
            }
            break;
        case UNPACKED8BIT:
            {
                // Read 8-bit unpacked scanline
                GByte   *byRawScan = (GByte *)CPLMalloc(poGDS->nRecordSize);
                VSIFReadL( byRawScan, 1, poGDS->nRecordSize, poGDS->fp );
                
                iScan = (GUInt16 *)CPLMalloc(poGDS->GetRasterXSize()
                                             * poGDS->nBands * sizeof(GUInt16));
                for (i = 0; i < poGDS->GetRasterXSize() * poGDS->nBands; i++)
                    iScan[i] = byRawScan[poGDS->nRecordDataStart
                        / (int)sizeof(byRawScan[0]) + i];
                CPLFree(byRawScan);
            }
            break;
        default: // NOTREACHED
            break;
    }
    
    int nBlockSize = nBlockXSize * nBlockYSize;
    if (poGDS->eLocationIndicator == DESCEND)
        for( i = 0, j = 0; i < nBlockSize; i++ )
        {
            ((GUInt16 *) pImage)[i] = iScan[j + nBand - 1];
            j += poGDS->nBands;
        }
    else
        for ( i = nBlockSize - 1, j = 0; i >= 0; i-- )
        {
            ((GUInt16 *) pImage)[i] = iScan[j + nBand - 1];
            j += poGDS->nBands;
        }
    
    CPLFree(iScan);
    return CE_None;
}

/************************************************************************/
/*                           L1BDataset()                               */
/************************************************************************/

L1BDataset::L1BDataset()

{
    fp = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",7043]],TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",6322]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",\"NORTH\"],AXIS[\"Long\",\"EAST\"],AUTHORITY[\"EPSG\",4322]]" );
    nBands = 0;
    eLocationIndicator = DESCEND; // XXX: should be initialised
    iChannels = 0;
    iInstrumentStatus = 0;
}

/************************************************************************/
/*                            ~L1BDataset()                             */
/************************************************************************/

L1BDataset::~L1BDataset()

{
    FlushCache();

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    if ( pszGCPProjection )
        CPLFree( pszGCPProjection );
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int L1BDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *L1BDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *L1BDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*      Fetch timecode from the record header (NOAA9-NOAA14 version)    */
/************************************************************************/

void L1BDataset::FetchNOAA9TimeCode( TimeCode *psTime, GByte *piRecordHeader,
                                     int *iLocInd )
{
    GUInt32 lTemp;

    lTemp = ((piRecordHeader[2] >> 1) & 0x7F);
    psTime->SetYear((lTemp > 77) ? 
        (lTemp + 1900) : (lTemp + 2000)); // Avoid `Year 2000' problem
    psTime->SetDay((GUInt32)(piRecordHeader[2] & 0x01) << 8
                   | (GUInt32)piRecordHeader[3]);
    psTime->SetMillisecond( ((GUInt32)(piRecordHeader[4] & 0x07) << 24)
        | ((GUInt32)piRecordHeader[5] << 16)
        | ((GUInt32)piRecordHeader[6] << 8)
        | (GUInt32)piRecordHeader[7] );
    *iLocInd = ((piRecordHeader[8] & 0x02) == 0) ? ASCEND : DESCEND;
}

/************************************************************************/
/*      Fetch timecode from the record header (NOAA15-METOP2 version)   */
/************************************************************************/

void L1BDataset::FetchNOAA15TimeCode( TimeCode *psTime,
                                      GUInt16 *piRecordHeader, int *iLocInd )
{
#ifdef CPL_LSB
    GUInt16 iTemp;
    GUInt32 lTemp;

    iTemp = piRecordHeader[1];
    psTime->SetYear(CPL_SWAP16(iTemp));
    iTemp = piRecordHeader[2];
    psTime->SetDay(CPL_SWAP16(iTemp));
    lTemp = (GUInt32)CPL_SWAP16(piRecordHeader[4]) << 16 |
        (GUInt32)CPL_SWAP16(piRecordHeader[5]);
    psTime->SetMillisecond(lTemp);
    *iLocInd = ((CPL_SWAP16(piRecordHeader[6]) & 0x8000) == 0)?ASCEND:DESCEND; // FIXME: hemisphere
#else
    psTime->SetYear(piRecordHeader[1]);
    psTime->SetDay(piRecordHeader[2]);
    psTime->SetMillisecond((GUInt32)piRecordHeader[4] << 16 | (GUInt32)piRecordHeader[5]);
    *iLocInd = ((piRecordHeader[6] & 0x8000) == 0)?ASCEND:DESCEND;
#endif
}

/************************************************************************/
/* Fetch the GCPs from the individual scanlines (NOAA9-NOAA14 version)  */
/************************************************************************/

void L1BDataset::FetchNOAA9GCPs( GDAL_GCP *pasGCPList,
                                 GInt16 *piRecordHeader, int iLine )
{
    int         nGoodGCPs, iGCPPos, j;
    double      dfPixel;
    
    nGoodGCPs = (*((GByte *)piRecordHeader + iGCPCodeOffset) <= nGCPsPerLine) ?
            *((GByte *)piRecordHeader + iGCPCodeOffset) : nGCPsPerLine;

#ifdef DEBUG
    CPLDebug( "L1B", "iGCPCodeOffset=%d, nGCPsPerLine=%d, nGoodGCPs=%d",
              iGCPCodeOffset, nGCPsPerLine, nGoodGCPs );
#endif

    // GCPs are located at center of pixel, so we will add a half pixel offset
    dfPixel = (eLocationIndicator == DESCEND) ?
        iGCPStart + 0.5 : (GetRasterXSize() - (iGCPStart + 0.5));
    j = iGCPOffset / (int)sizeof(piRecordHeader[0]);
    iGCPPos = iGCPOffset / (int)sizeof(piRecordHeader[0]) + 2 * nGoodGCPs;
    while ( j < iGCPPos )
    {
        GInt16  nRawY = piRecordHeader[j++];
        GInt16  nRawX = piRecordHeader[j++];

#ifdef CPL_LSB
        CPL_SWAP16PTR( &nRawX );
        CPL_SWAP16PTR( &nRawY );
#endif
        pasGCPList[nGCPCount].dfGCPY = nRawY / 128.0;
        pasGCPList[nGCPCount].dfGCPX = nRawX / 128.0;

        if (pasGCPList[nGCPCount].dfGCPX < -180
            || pasGCPList[nGCPCount].dfGCPX > 180
            || pasGCPList[nGCPCount].dfGCPY < -90
            || pasGCPList[nGCPCount].dfGCPY > 90)
            continue;

        pasGCPList[nGCPCount].dfGCPZ = 0.0;
        pasGCPList[nGCPCount].dfGCPPixel = dfPixel;
        dfPixel += (eLocationIndicator == DESCEND) ? iGCPStep : -iGCPStep;
        pasGCPList[nGCPCount].dfGCPLine =
            (double)((eLocationIndicator == DESCEND) ?
            iLine : GetRasterYSize() - iLine - 1) + 0.5;
        nGCPCount++;
    }
}

/************************************************************************/
/* Fetch the GCPs from the individual scanlines (NOAA15-METOP2 version) */
/************************************************************************/

void L1BDataset::FetchNOAA15GCPs( GDAL_GCP *pasGCPList,
                                  GInt32 *piRecordHeader, int iLine)
{
    int         j, iGCPPos;
    double      dfPixel;

    // GCPs are located at center of pixel, so we will add a half pixel offset
    dfPixel = (eLocationIndicator == DESCEND) ?
        iGCPStart + 0.5 : (GetRasterXSize() - (iGCPStart + 0.5));
    j = iGCPOffset / (int)sizeof(piRecordHeader[0]);
    iGCPPos = iGCPOffset / (int)sizeof(piRecordHeader[0]) + 2 * nGCPsPerLine;
    while ( j < iGCPPos )
    {
        GInt32  nRawY = piRecordHeader[j++];
        GInt32  nRawX = piRecordHeader[j++];

#ifdef CPL_LSB
        CPL_SWAP32PTR( &nRawX );
        CPL_SWAP32PTR( &nRawY );
#endif
        pasGCPList[nGCPCount].dfGCPY = nRawY / 10000.0;
        pasGCPList[nGCPCount].dfGCPX = nRawX / 10000.0;

        if ( pasGCPList[nGCPCount].dfGCPX < -180
             || pasGCPList[nGCPCount].dfGCPX > 180
             || pasGCPList[nGCPCount].dfGCPY < -90
             || pasGCPList[nGCPCount].dfGCPY > 90 )
        {
            continue;
        }
        pasGCPList[nGCPCount].dfGCPZ = 0.0;
        pasGCPList[nGCPCount].dfGCPPixel = dfPixel;
        dfPixel += (eLocationIndicator == DESCEND) ? iGCPStep : -iGCPStep;
        pasGCPList[nGCPCount].dfGCPLine =
            (double)((eLocationIndicator == DESCEND) ?
            iLine : GetRasterYSize() - iLine - 1) + 0.5;
        nGCPCount++;
    }
}

/************************************************************************/
/*                      ProcessRecordHeaders()                          */
/************************************************************************/

void L1BDataset::ProcessRecordHeaders()
{
    int         iLine, iLocInd;
    void        *piRecordHeader;

    piRecordHeader = CPLMalloc(nRecordDataStart);
    VSIFSeekL(fp, nDataStartOffset, SEEK_SET);
    VSIFReadL(piRecordHeader, 1, nRecordDataStart, fp);

    if (eSpacecraftID <= NOAA14)
        FetchNOAA9TimeCode(&sStartTime, (GByte *) piRecordHeader, &iLocInd);
    else
        FetchNOAA15TimeCode(&sStartTime, (GUInt16 *) piRecordHeader, &iLocInd);
    eLocationIndicator = iLocInd;
    VSIFSeekL( fp, nDataStartOffset + (GetRasterYSize() - 1) * nRecordSize,
              SEEK_SET);
    VSIFReadL( piRecordHeader, 1, nRecordDataStart, fp );
    if (eSpacecraftID <= NOAA14)
        FetchNOAA9TimeCode(&sStopTime, (GByte *) piRecordHeader, &iLocInd);
    else
        FetchNOAA15TimeCode(&sStopTime, (GUInt16 *) piRecordHeader, &iLocInd);

/* -------------------------------------------------------------------- */
/*      Pick a skip factor so that we will get roughly 20 lines         */
/*      worth of GCPs.  That should give respectible coverage on all    */
/*      but the longest swaths.                                         */
/* -------------------------------------------------------------------- */
    int nTargetLines = DESIRED_LINES_OF_GCPS;
    int nLineSkip = GetRasterYSize() / (nTargetLines-1);
    
/* -------------------------------------------------------------------- */
/*      Initialize the GCP list.                                        */
/* -------------------------------------------------------------------- */
    pasGCPList = (GDAL_GCP *)CPLCalloc( nTargetLines * nGCPsPerLine,
                                        sizeof(GDAL_GCP) );
    GDALInitGCPs( nTargetLines * nGCPsPerLine, pasGCPList );

/* -------------------------------------------------------------------- */
/*      Fetch the GCPs for each selected line.  We force the last       */
/*      line sampled to be the last line in the dataset even if that    */
/*      leaves a bigger than expected gap.                              */
/* -------------------------------------------------------------------- */
    int iStep;

    for( iStep = 0; iStep < nTargetLines; iStep++ )
    {
        int nOrigGCPs = nGCPCount;

        if( iStep == nTargetLines - 1 )
            iLine = GetRasterYSize() - 1;
        else
            iLine = nLineSkip * iStep;

        VSIFSeekL( fp, nDataStartOffset + iLine * nRecordSize, SEEK_SET );
        VSIFReadL( piRecordHeader, 1, nRecordDataStart, fp );

        if ( eSpacecraftID <= NOAA14 )
            FetchNOAA9GCPs( pasGCPList, (GInt16 *)piRecordHeader, iLine );
        else
            FetchNOAA15GCPs( pasGCPList, (GInt32 *)piRecordHeader, iLine );

/* -------------------------------------------------------------------- */
/*      We don't really want too many GCPs per line.  Downsample to     */
/*      11 per line.                                                    */
/* -------------------------------------------------------------------- */
        int iGCP;
        int nGCPsOnThisLine = nGCPCount - nOrigGCPs;
        int nDesiredGCPsPerLine = MIN(DESIRED_GCPS_PER_LINE,nGCPsOnThisLine);
        int nGCPStep = (nGCPsOnThisLine - 1) / (nDesiredGCPsPerLine-1);

        if( nGCPStep == 0 )
            nGCPStep = 1;

        for( iGCP = 0; iGCP < nDesiredGCPsPerLine; iGCP++ )
        {
            int iSrcGCP = nOrigGCPs + iGCP * nGCPStep;
            int iDstGCP = nOrigGCPs + iGCP;

            pasGCPList[iDstGCP].dfGCPX = pasGCPList[iSrcGCP].dfGCPX;
            pasGCPList[iDstGCP].dfGCPY = pasGCPList[iSrcGCP].dfGCPY;
            pasGCPList[iDstGCP].dfGCPPixel = pasGCPList[iSrcGCP].dfGCPPixel;
            pasGCPList[iDstGCP].dfGCPLine = pasGCPList[iSrcGCP].dfGCPLine;
        }

        nGCPCount = nOrigGCPs + nDesiredGCPsPerLine;
    }

    if( nGCPCount < nTargetLines * nGCPsPerLine )
    {
        GDALDeinitGCPs( nTargetLines * nGCPsPerLine - nGCPCount, 
                        pasGCPList + nGCPCount );
    }

    CPLFree( piRecordHeader );
}

/************************************************************************/
/*                      ProcessDatasetHeader()                          */
/************************************************************************/

void L1BDataset::ProcessDatasetHeader()
{
    GUInt16 *piHeader;
    piHeader = (GUInt16 *)CPLMalloc(nDataStartOffset);
    VSIFSeekL( fp, 0, SEEK_SET );
    VSIFReadL( piHeader, 1, nDataStartOffset, fp );
    if (eSpacecraftID > NOAA14)
    {
        iInstrumentStatus = (piHeader + 512)[58];
#ifdef CPL_LSB
        CPL_SWAP16PTR(&iInstrumentStatus);
#endif
    }
    CPLFree( piHeader );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int L1BDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->fp == NULL )
        return FALSE;

    // XXX: Signature is not very good
    if( !EQUALN((const char *) poOpenInfo->pabyHeader + 33, ".", 1) ||
        !EQUALN((const char *) poOpenInfo->pabyHeader + 38, ".", 1) || 
        !EQUALN((const char *) poOpenInfo->pabyHeader + 41, ".", 1) || 
        !EQUALN((const char *) poOpenInfo->pabyHeader + 48, ".", 1) || 
        !EQUALN((const char *) poOpenInfo->pabyHeader + 54, ".", 1) ||
        !EQUALN((const char *) poOpenInfo->pabyHeader + 60, ".", 1) ||
        !EQUALN((const char *) poOpenInfo->pabyHeader + 69, ".", 1) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *L1BDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if ( !Identify(poOpenInfo) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    L1BDataset  *poDS;
    VSIStatBuf  sStat;
    const char  *pszFilename = poOpenInfo->pszFilename;

    poDS = new L1BDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if ( !poDS->fp )
    {
        CPLDebug( "L1B", "Can't open file \"%s\".", poOpenInfo->pszFilename );
        goto bad;
    }
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    GByte       pabyTBMHeader[TBM_HEADER_SIZE];
    int         i;

    if ( VSIFSeekL( poDS->fp, 0, SEEK_SET ) < 0
         || VSIFReadL( pabyTBMHeader, 1, TBM_HEADER_SIZE, poDS->fp ) < TBM_HEADER_SIZE )
    {
        CPLDebug( "L1B", "Can't read TBM header." );
        goto bad;
    }

    // Determine processing center where the dataset was created
    if ( EQUALN((const char *) pabyTBMHeader + 30, "CMS", 3) )
         poDS->eProcCenter = CMS;
    else if ( EQUALN((const char *) pabyTBMHeader + 30, "DSS", 3) )
         poDS->eProcCenter = DSS;
    else if ( EQUALN((const char *) pabyTBMHeader + 30, "NSS", 3) )
         poDS->eProcCenter = NSS;
    else if ( EQUALN((const char *) pabyTBMHeader + 30, "UKM", 3) )
         poDS->eProcCenter = UKM;
    else
         poDS->eProcCenter = UNKNOWN_CENTER;
    
    // Determine spacecraft type
    if ( EQUALN((const char *)pabyTBMHeader + 39, "NA", 2) )
         poDS->eSpacecraftID = NOAA6;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NB", 2) )
         poDS->eSpacecraftID = NOAAB;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NC", 2) )
         poDS->eSpacecraftID = NOAA7;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NE", 2) )
         poDS->eSpacecraftID = NOAA8;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NF", 2) )
         poDS->eSpacecraftID = NOAA9;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NG", 2) )
         poDS->eSpacecraftID = NOAA10;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NH", 2) )
         poDS->eSpacecraftID = NOAA11;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "ND", 2) )
         poDS->eSpacecraftID = NOAA12;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NI", 2) )
         poDS->eSpacecraftID = NOAA13;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NJ", 2) )
         poDS->eSpacecraftID = NOAA14;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NK", 2) )
         poDS->eSpacecraftID = NOAA15;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NL", 2) )
         poDS->eSpacecraftID = NOAA16;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NM", 2) )
         poDS->eSpacecraftID = NOAA17;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "NN", 2) )
         poDS->eSpacecraftID = NOAA18;
    else if ( EQUALN((const char *)pabyTBMHeader + 39, "M2", 2) )
         poDS->eSpacecraftID = METOP2;
    else
    {
#ifdef DEBUG
        CPLDebug( "L1B", "Unknown spacecraft type \"%.2s\".",
                  pabyTBMHeader + 39 );
#endif
         goto bad;
    }
           
    // Determine product type
    if ( EQUALN((const char *)pabyTBMHeader + 34, "HRPT", 4) )
         poDS->eProductType = HRPT;
    else if ( EQUALN((const char *)pabyTBMHeader + 34, "LHRR", 4) )
         poDS->eProductType = LAC;
    else if ( EQUALN((const char *)pabyTBMHeader + 34, "GHRR", 4) )
         poDS->eProductType = GAC;
    else if ( EQUALN((const char *)pabyTBMHeader + 34, "FRAC", 4) )
         poDS->eProductType = FRAC;
    else
    {
#ifdef DEBUG
        CPLDebug( "L1B", "Unknown product type \"%.4s\".",
                  pabyTBMHeader + 34 );
#endif
         goto bad;
    }

    // Get revolution number as string, we don't need this value for processing
    memcpy(poDS->pszRevolution, pabyTBMHeader + 62, 5);
    poDS->pszRevolution[5] = '\0';

    // Get receiving station name
    if ( EQUALN((const char *)pabyTBMHeader + 70, "DU", 2) )
         poDS->eSource = DU;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "GC", 2) )
         poDS->eSource = GC;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "HO", 2) )
         poDS->eSource = HO;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "MO", 2) )
         poDS->eSource = MO;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "WE", 2) )
         poDS->eSource = WE;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "SO", 2) )
         poDS->eSource = SO;
    else if ( EQUALN((const char *)pabyTBMHeader + 70, "WI", 2) )
         poDS->eSource = WI;
    else
         poDS->eSource = UNKNOWN_STATION;

    // Determine number of bands
    for ( i = 97; i < 117; i++ )
    {
        if (pabyTBMHeader[i] == 1 || pabyTBMHeader[i] == 'Y')
        {
            poDS->nBands++;
            poDS->iChannels |= (1 << (i - 97));
        }
    }
    if (poDS->nBands == 0 || poDS->nBands > 5)
    {
        poDS->nBands = 5;
        poDS->iChannels = 0x1F;
    }

    // Determine data format (10-bit packed or 8/16-bit unpacked)
    if ( EQUALN((const char *)pabyTBMHeader + 117, "10", 2) ||
         EQUALN((const char *)pabyTBMHeader + 117, "  ", 2) )
        poDS->iDataFormat = PACKED10BIT;
    else if ( EQUALN((const char *)pabyTBMHeader + 117, "16", 2) )
        poDS->iDataFormat = UNPACKED16BIT;
    else if ( EQUALN((const char *)pabyTBMHeader + 117, "08", 2) )
        poDS->iDataFormat = UNPACKED8BIT;
    else
    {
#ifdef DEBUG
        CPLDebug( "L1B", "Unknown data format \"%.2s\".",
                  pabyTBMHeader + 117 );
#endif
        goto bad;
    }

    switch( poDS->eProductType )
    {
        case HRPT:
        case LAC:
        case FRAC:
            poDS->nRasterXSize = 2048;
            poDS->nBufferSize = 20484;
            poDS->iGCPStart = 25;
            poDS->iGCPStep = 40;
            poDS->nGCPsPerLine = 51;
            if (poDS->eSpacecraftID <= NOAA14)
            {
                if (poDS->iDataFormat == PACKED10BIT)
                {
                    poDS->nRecordSize = 14800;
                    poDS->nRecordDataEnd = 14104;
                }
                else if (poDS->iDataFormat == UNPACKED16BIT)
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 4544;
                        poDS->nRecordDataEnd = 4544;
                        break;
                        case 2:
                        poDS->nRecordSize = 8640;
                        poDS->nRecordDataEnd = 8640;
                        break;
                        case 3:
                        poDS->nRecordSize = 12736;
                        poDS->nRecordDataEnd = 12736;
                        break;
                        case 4:
                        poDS->nRecordSize = 16832;
                        poDS->nRecordDataEnd = 16832;
                        break;
                        case 5:
                        poDS->nRecordSize = 20928;
                        poDS->nRecordDataEnd = 20928;
                        break;
                    }
                }
                else // UNPACKED8BIT
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 2496;
                        poDS->nRecordDataEnd = 2496;
                        break;
                        case 2:
                        poDS->nRecordSize = 4544;
                        poDS->nRecordDataEnd = 4544;
                        break;
                        case 3:
                        poDS->nRecordSize = 6592;
                        poDS->nRecordDataEnd = 6592;
                        break;
                        case 4:
                        poDS->nRecordSize = 8640;
                        poDS->nRecordDataEnd = 8640;
                        break;
                        case 5:
                        poDS->nRecordSize = 10688;
                        poDS->nRecordDataEnd = 10688;
                        break;
                    }
                }
                poDS->nDataStartOffset = poDS->nRecordSize + 122;
                poDS->nRecordDataStart = 448;
                poDS->iGCPCodeOffset = 52;
                poDS->iGCPOffset = 104;
            }
            else if ( poDS->eSpacecraftID <= METOP2 )
            {
                if (poDS->iDataFormat == PACKED10BIT)
                {
                    poDS->nRecordSize = 15872;
                    poDS->nRecordDataEnd = 14920;
                }
                else if (poDS->iDataFormat == UNPACKED16BIT)
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 6144;
                        poDS->nRecordDataEnd = 5360;
                        break;
                        case 2:
                        poDS->nRecordSize = 10240;
                        poDS->nRecordDataEnd = 9456;
                        break;
                        case 3:
                        poDS->nRecordSize = 14336;
                        poDS->nRecordDataEnd = 13552;
                        break;
                        case 4:
                        poDS->nRecordSize = 18432;
                        poDS->nRecordDataEnd = 17648;
                        break;
                        case 5:
                        poDS->nRecordSize = 22528;
                        poDS->nRecordDataEnd = 21744;
                        break;
                    }
                }
                else // UNPACKED8BIT
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 4096;
                        poDS->nRecordDataEnd = 3312;
                        break;
                        case 2:
                        poDS->nRecordSize = 6144;
                        poDS->nRecordDataEnd = 5360;
                        break;
                        case 3:
                        poDS->nRecordSize = 8192;
                        poDS->nRecordDataEnd = 7408;
                        break;
                        case 4:
                        poDS->nRecordSize = 10240;
                        poDS->nRecordDataEnd = 9456;
                        break;
                        case 5:
                        poDS->nRecordSize = 12288;
                        poDS->nRecordDataEnd = 11504;
                        break;
                    }
                }
                poDS->nDataStartOffset = poDS->nRecordSize + 512;
                poDS->nRecordDataStart = 1264;
                poDS->iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                poDS->iGCPOffset = 640;
            }
            else
                goto bad;
        break;
        case GAC:
            poDS->nRasterXSize = 409;
            poDS->nBufferSize = 4092;
            poDS->iGCPStart = 5; // FIXME: depends of scan direction
            poDS->iGCPStep = 8;
            poDS->nGCPsPerLine = 51;
            if (poDS->eSpacecraftID <= NOAA14)
            {
                if (poDS->iDataFormat == PACKED10BIT)
                {
                    poDS->nRecordSize = 3220;
                    poDS->nRecordDataEnd = 3176;
                }
                else if (poDS->iDataFormat == UNPACKED16BIT)
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 1268;
                        poDS->nRecordDataEnd = 1266;
                        break;
                        case 2:
                        poDS->nRecordSize = 2084;
                        poDS->nRecordDataEnd = 2084;
                        break;
                        case 3:
                        poDS->nRecordSize = 2904;
                        poDS->nRecordDataEnd = 2902;
                        break;
                        case 4:
                        poDS->nRecordSize = 3720;
                        poDS->nRecordDataEnd = 3720;
                        break;
                        case 5:
                        poDS->nRecordSize = 4540;
                        poDS->nRecordDataEnd = 4538;
                        break;
                    }
                else // UNPACKED8BIT
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 860;
                        poDS->nRecordDataEnd = 858;
                        break;
                        case 2:
                        poDS->nRecordSize = 1268;
                        poDS->nRecordDataEnd = 1266;
                        break;
                        case 3:
                        poDS->nRecordSize = 1676;
                        poDS->nRecordDataEnd = 1676;
                        break;
                        case 4:
                        poDS->nRecordSize = 2084;
                        poDS->nRecordDataEnd = 2084;
                        break;
                        case 5:
                        poDS->nRecordSize = 2496;
                        poDS->nRecordDataEnd = 2494;
                        break;
                    }
                }
                poDS->nDataStartOffset = poDS->nRecordSize * 2 + 122;
                poDS->nRecordDataStart = 448;
                poDS->iGCPCodeOffset = 52;
                poDS->iGCPOffset = 104;
            }
            else if ( poDS->eSpacecraftID <= METOP2 )
            {
                if (poDS->iDataFormat == PACKED10BIT)
                {
                    poDS->nRecordSize = 4608;
                    poDS->nRecordDataEnd = 3992;
                }
                else if (poDS->iDataFormat == UNPACKED16BIT)
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 2360;
                        poDS->nRecordDataEnd = 2082;
                        break;
                        case 2:
                        poDS->nRecordSize = 3176;
                        poDS->nRecordDataEnd = 2900;
                        break;
                        case 3:
                        poDS->nRecordSize = 3992;
                        poDS->nRecordDataEnd = 3718;
                        break;
                        case 4:
                        poDS->nRecordSize = 4816;
                        poDS->nRecordDataEnd = 4536;
                        break;
                        case 5:
                        poDS->nRecordSize = 5632;
                        poDS->nRecordDataEnd = 5354;
                        break;
                    }
                }
                else // UNPACKED8BIT
                {
                    switch(poDS->nBands)
                    {
                        case 1:
                        poDS->nRecordSize = 1952;
                        poDS->nRecordDataEnd = 1673;
                        break;
                        case 2:
                        poDS->nRecordSize = 2360;
                        poDS->nRecordDataEnd = 2082;
                        break;
                        case 3:
                        poDS->nRecordSize = 2768;
                        poDS->nRecordDataEnd = 2491;
                        break;
                        case 4:
                        poDS->nRecordSize = 3176;
                        poDS->nRecordDataEnd = 2900;
                        break;
                        case 5:
                        poDS->nRecordSize = 3584;
                        poDS->nRecordDataEnd = 3309;
                        break;
                    }
                }
                poDS->nDataStartOffset = poDS->nRecordSize + 512;
                poDS->nRecordDataStart = 1264;
                poDS->iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                poDS->iGCPOffset = 640;
            }
            else
                goto bad;
        break;
        default:
            goto bad;
    }
    // Compute number of lines dinamycally, so we can read partially
    // downloaded files
    CPLStat(pszFilename, &sStat);
    poDS->nRasterYSize =
        (sStat.st_size - poDS->nDataStartOffset) / poDS->nRecordSize;

/* -------------------------------------------------------------------- */
/*      Load some info from header.                                     */
/* -------------------------------------------------------------------- */
    poDS->ProcessDatasetHeader();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int iBand;
    
    for( iBand = 1, i = 0; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new L1BRasterBand( poDS, iBand ));
        
        // Channels descriptions
        if ( poDS->eSpacecraftID >= NOAA6 && poDS->eSpacecraftID <= METOP2 )
        {
            if ( !(i & 0x01) && poDS->iChannels & 0x01 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[0] );
                i |= 0x01;
                continue;
            }
            if ( !(i & 0x02) && poDS->iChannels & 0x02 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[1] );
                i |= 0x02;
                continue;
            }
            if ( !(i & 0x04) && poDS->iChannels & 0x04 )
            {
                if ( poDS->eSpacecraftID >= NOAA15
                     && poDS->eSpacecraftID <= METOP2 )
                    if (poDS->iInstrumentStatus & 0x0400)
                        poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[7] );
                    else
                        poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[6] );
                else    
                    poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[2] );
                i |= 0x04;
                continue;
            }
            if ( !(i & 0x08) && poDS->iChannels & 0x08 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[3] );
                i |= 0x08;
                continue;
            }
            if ( !(i & 0x10) && poDS->iChannels & 0x10 )
            {
                if (poDS->eSpacecraftID == NOAA13)              // 5 NOAA-13
                    poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[5] );
                else if (poDS->eSpacecraftID == NOAA6 ||
                         poDS->eSpacecraftID == NOAA8 ||
                         poDS->eSpacecraftID == NOAA10)         // 4 NOAA-6,-8,-10
                    poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[3] );
                else
                    poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[4] );
                i |= 0x10;
                continue;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have GCPs?                                                */
/* -------------------------------------------------------------------- */
    if ( EQUALN((const char *)pabyTBMHeader + 96, "Y", 1) )
    {
        poDS->ProcessRecordHeaders();

#if 0
        Temporarily disabled.
        CPLString  osTMP;

        poDS->SetMetadataItem( "SRS", poDS->pszGCPProjection, "GEOLOCATION" );
        
        osTMP.Printf( "L1BGCPS:\"%s\"", pszFilename );
        poDS->SetMetadataItem( "X_DATASET", osTMP, "GEOLOCATION" );
        poDS->SetMetadataItem( "X_BAND", "1" , "GEOLOCATION" );
        poDS->SetMetadataItem( "Y_DATASET", osTMP, "GEOLOCATION" );
        poDS->SetMetadataItem( "Y_BAND", "2" , "GEOLOCATION" );

        osTMP.Printf( "%d", (poDS->eLocationIndicator == DESCEND) ?
            poDS->iGCPStart : (poDS->nRasterXSize - poDS->iGCPStart) );
        poDS->SetMetadataItem( "PIXEL_OFFSET", osTMP, "GEOLOCATION" );
        osTMP.Printf( "%d", (poDS->eLocationIndicator == DESCEND) ? poDS->iGCPStep : -poDS->iGCPStep );
        poDS->SetMetadataItem( "PIXEL_STEP", osTMP, "GEOLOCATION" );

        poDS->SetMetadataItem( "LINE_OFFSET", "0", "GEOLOCATION" );
        poDS->SetMetadataItem( "LINE_STEP", "1", "GEOLOCATION" );
#endif        
    }

/* -------------------------------------------------------------------- */
/*      Get and set other important information as metadata             */
/* -------------------------------------------------------------------- */
    const char *pszText;
    switch( poDS->eSpacecraftID )
    {
        case TIROSN:
            pszText = "TIROS-N";
        break;
        case NOAA6:
            pszText = "NOAA-6(A)";
        break;
        case NOAAB:
            pszText = "NOAA-B";
        break;
        case NOAA7:
            pszText = "NOAA-7(C)";
        break;
        case NOAA8:
            pszText = "NOAA-8(E)";
        break;
        case NOAA9:
            pszText = "NOAA-9(F)";
        break;
        case NOAA10:
            pszText = "NOAA-10(G)";
        break;
        case NOAA11:
            pszText = "NOAA-11(H)";
        break;
        case NOAA12:
            pszText = "NOAA-12(D)";
        break;
        case NOAA13:
            pszText = "NOAA-13(I)";
        break;
        case NOAA14:
            pszText = "NOAA-14(J)";
        break;
        case NOAA15:
            pszText = "NOAA-15(K)";
        break;
        case NOAA16:
            pszText = "NOAA-16(L)";
        break;
        case NOAA17:
            pszText = "NOAA-17(M)";
        break;
        case NOAA18:
            pszText = "NOAA-18(N)";
        break;
        case METOP2:
            pszText = "METOP-2(A)";
        break;
        default:
            pszText = "Unknown";
    }
    poDS->SetMetadataItem( "SATELLITE",  pszText );
    switch( poDS->eProductType )
    {
        case LAC:
            pszText = "AVHRR LAC";
        break;
        case HRPT:
            pszText = "AVHRR HRPT";
        break;
        case GAC:
            pszText = "AVHRR GAC";
        break;
        case FRAC:
            pszText = "AVHRR FRAC";
        break;
        default:
            pszText = "Unknown";
    }
    poDS->SetMetadataItem( "DATA_TYPE",  pszText );
    poDS->SetMetadataItem( "REVOLUTION",  poDS->pszRevolution );
    switch( poDS->eSource )
    {
        case DU:
            pszText = "Dundee, Scotland, UK";
        break;
        case GC:
            pszText = "Fairbanks, Alaska, USA (formerly Gilmore Creek)";
        break;
        case HO:
            pszText = "Honolulu, Hawaii, USA";
        break;
        case MO:
            pszText = "Monterey, California, USA";
        break;
        case WE:
            pszText = "Western Europe CDA, Lannion, France";
        break;
        case SO:
            pszText = "SOCC (Satellite Operations Control Center), Suitland, Maryland, USA";
        break;
        case WI:
            pszText = "Wallops Island, Virginia, USA";
        break;
        default:
            pszText = "Unknown receiving station";
    }
    poDS->SetMetadataItem( "SOURCE",  pszText );
    switch( poDS->eProcCenter )
    {
        case CMS:
            pszText = "Centre de Meteorologie Spatiale - Lannion, France";
        break;
        case DSS:
            pszText = "Dundee Satellite Receiving Station - Dundee, Scotland, UK";
        break;
        case NSS:
            pszText = "NOAA/NESDIS - Suitland, Maryland, USA";
        break;
        case UKM:
            pszText = "United Kingdom Meteorological Office - Bracknell, England, UK";
        break;
        default:
            pszText = "Unknown processing center";
    }
    poDS->SetMetadataItem( "PROCESSING_CENTER",  pszText );
    // Time of first scanline
    poDS->SetMetadataItem( "START",  poDS->sStartTime.PrintTime() );
    // Time of last scanline
    poDS->SetMetadataItem( "STOP",  poDS->sStopTime.PrintTime() );
    // AVHRR Earth location indication
    switch(poDS->eLocationIndicator)
    {
        case ASCEND:
            poDS->SetMetadataItem( "LOCATION", "Ascending" );
            break;
        case DESCEND:
        default:
            poDS->SetMetadataItem( "LOCATION", "Descending" );
            break;
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );

bad:
    delete poDS;
    return NULL;
}

/************************************************************************/
/*                        GDALRegister_L1B()                            */
/************************************************************************/

void GDALRegister_L1B()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "L1B" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "L1B" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NOAA Polar Orbiter Level 1b Data Set" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_l1b.html" );

        poDriver->pfnOpen = L1BDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

