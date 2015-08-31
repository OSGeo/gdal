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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * ----------------------------------------------------------------------------
 * Lagrange interpolation suitable for NOAA level 1B file formats.
 * Submitted by Andrew Brooks <arb@sat.dundee.ac.uk>
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
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_L1B(void);
CPL_C_END

typedef enum {                  // File formats
    L1B_NONE,           // Not a L1B format
    L1B_NOAA9,          // NOAA-9/14
    L1B_NOAA15,         // NOAA-15/METOP-2
    L1B_NOAA15_NOHDR    // NOAA-15/METOP-2 without ARS header
} L1BFileFormat;

typedef enum {          // Spacecrafts:
    TIROSN,     // TIROS-N
    // NOAA are given a letter before launch and a number after launch
    NOAA6,      // NOAA-6(A)
    NOAAB,      // NOAA-B
    NOAA7,      // NOAA-7(C)
    NOAA8,      // NOAA-8(E)
    NOAA9_UNKNOWN, // Some NOAA-18 and NOAA-19 HRPT are recognized like that...
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
    NOAA19,     // NOAA-19(N')
    // MetOp are given a number before launch and a letter after launch
    METOP2,     // METOP-A(2)
    METOP1,     // METOP-B(1)
    METOP3,     // METOP-C(3)
} L1BSpaceCraftdID;

typedef enum {          // Product types
    HRPT,
    LAC,
    GAC,
    FRAC
} L1BProductType;

typedef enum {          // Data format
    PACKED10BIT,
    UNPACKED8BIT,
    UNPACKED16BIT
} L1BDataFormat;

typedef enum {          // Receiving stations names:
    DU,         // Dundee, Scotland, UK
    GC,         // Fairbanks, Alaska, USA (formerly Gilmore Creek)
    HO,         // Honolulu, Hawaii, USA
    MO,         // Monterey, California, USA
    WE,         // Western Europe CDA, Lannion, France
    SO,         // SOCC (Satellite Operations Control Center), Suitland, Maryland, USA
    WI,         // Wallops Island, Virginia, USA
    SV,         // Svalbard, Norway
    UNKNOWN_STATION
} L1BReceivingStation;

typedef enum {          // Data processing centers:
    CMS,        // Centre de Meteorologie Spatiale - Lannion, France
    DSS,        // Dundee Satellite Receiving Station - Dundee, Scotland, UK
    NSS,        // NOAA/NESDIS - Suitland, Maryland, USA
    UKM,        // United Kingdom Meteorological Office - Bracknell, England, UK
    UNKNOWN_CENTER
} L1BProcessingCenter;

typedef enum {          // AVHRR Earth location indication
    ASCEND,
    DESCEND
} L1BAscendOrDescend;

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

/************************************************************************/
/*      L1B file format related constants                               */
/************************************************************************/

#define L1B_DATASET_NAME_SIZE       42  // Length of the string containing
                                        // dataset name
#define L1B_NOAA9_HEADER_SIZE       122 // Terabit memory (TBM) header length
#define L1B_NOAA9_HDR_NAME_OFF      30  // Dataset name offset
#define L1B_NOAA9_HDR_SRC_OFF       70  // Receiving station name offset
#define L1B_NOAA9_HDR_CHAN_OFF      97  // Selected channels map offset
#define L1B_NOAA9_HDR_CHAN_SIZE     20  // Length of selected channels map
#define L1B_NOAA9_HDR_WORD_OFF      117 // Sensor data word size offset

#define L1B_NOAA15_HEADER_SIZE      512 // Archive Retrieval System (ARS)
                                        // header
#define L1B_NOAA15_HDR_CHAN_OFF     97  // Selected channels map offset
#define L1B_NOAA15_HDR_CHAN_SIZE    20  // Length of selected channels map
#define L1B_NOAA15_HDR_WORD_OFF     117 // Sensor data word size offset

#define L1B_NOAA9_HDR_REC_SIZE      146 // Length of header record
                                        // filled with the data
#define L1B_NOAA9_HDR_REC_ID_OFF    0   // Spacecraft ID offset
#define L1B_NOAA9_HDR_REC_PROD_OFF  1   // Data type offset
#define L1B_NOAA9_HDR_REC_DSTAT_OFF 34  // DACS status offset

/* See http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/klm/html/c8/sec83132-2.htm */
#define L1B_NOAA15_HDR_REC_SIZE     992 // Length of header record
                                        // filled with the data
#define L1B_NOAA15_HDR_REC_SITE_OFF 0   // Dataset creation site ID offset
#define L1B_NOAA15_HDR_REC_FORMAT_VERSION_OFF      4  // NOAA Level 1b Format Version Number
#define L1B_NOAA15_HDR_REC_FORMAT_VERSION_YEAR_OFF 6  // Level 1b Format Version Year (e.g., 1999)
#define L1B_NOAA15_HDR_REC_FORMAT_VERSION_DAY_OFF  8  // Level 1b Format Version Day of Year (e.g., 365)
#define L1B_NOAA15_HDR_REC_LOGICAL_REC_LENGTH_OFF  10 // Logical Record Length of source Level 1b data set prior to processing
#define L1B_NOAA15_HDR_REC_BLOCK_SIZE_OFF          12 // Block Size of source Level 1b data set prior to processing
#define L1B_NOAA15_HDR_REC_HDR_REC_COUNT_OFF       14 // Count of Header Records in this Data Set
#define L1B_NOAA15_HDR_REC_NAME_OFF 22  // Dataset name
#define L1B_NOAA15_HDR_REC_ID_OFF   72  // Spacecraft ID offset
#define L1B_NOAA15_HDR_REC_PROD_OFF 76  // Data type offset
#define L1B_NOAA15_HDR_REC_STAT_OFF 116 // Instrument status offset
#define L1B_NOAA15_HDR_REC_DATA_RECORD_COUNT_OFF 128
#define L1B_NOAA15_HDR_REC_CALIBRATED_SCANLINE_COUNT_OFF 130
#define L1B_NOAA15_HDR_REC_MISSING_SCANLINE_COUNT_OFF 132
#define L1B_NOAA15_HDR_REC_SRC_OFF  154 // Receiving station name offset
#define L1B_NOAA15_HDR_REC_ELLIPSOID_OFF 328

/* This only apply if L1B_HIGH_GCP_DENSITY is explicitly set to NO */
/* otherwise we will report more GCPs */
#define DESIRED_GCPS_PER_LINE 11
#define DESIRED_LINES_OF_GCPS 20

// Fixed values used to scale GCPs coordinates in AVHRR records
#define L1B_NOAA9_GCP_SCALE     128.0
#define L1B_NOAA15_GCP_SCALE    10000.0

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
    long GetYear() { return lYear; }
    long GetDay() { return lDay; }
    long GetMillisecond() { return lMillisecond; }
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
class L1BGeolocDataset;
class L1BGeolocRasterBand;
class L1BSolarZenithAnglesDataset;
class L1BSolarZenithAnglesRasterBand;
class L1BNOAA15AnglesDataset;
class L1BNOAA15AnglesRasterBand;
class L1BCloudsDataset;
class L1BCloudsRasterBand;

class L1BDataset : public GDALPamDataset
{
    friend class L1BRasterBand;
    friend class L1BMaskBand;
    friend class L1BGeolocDataset;
    friend class L1BGeolocRasterBand;
    friend class L1BSolarZenithAnglesDataset;
    friend class L1BSolarZenithAnglesRasterBand;
    friend class L1BNOAA15AnglesDataset;
    friend class L1BNOAA15AnglesRasterBand;
    friend class L1BCloudsDataset;
    friend class L1BCloudsRasterBand;

    //char        pszRevolution[6]; // Five-digit number identifying spacecraft revolution
    L1BReceivingStation       eSource;        // Source of data (receiving station name)
    L1BProcessingCenter       eProcCenter;    // Data processing center
    TimeCode    sStartTime;
    TimeCode    sStopTime;

    int         bHighGCPDensityStrategy;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;
    int         iGCPOffset;
    int         iGCPCodeOffset;
    int         iCLAVRStart;
    int         nGCPsPerLine;
    int         eLocationIndicator, iGCPStart, iGCPStep;

    L1BFileFormat eL1BFormat;
    int         nBufferSize;
    L1BSpaceCraftdID eSpacecraftID;
    L1BProductType   eProductType;   // LAC, GAC, HRPT, FRAC
    L1BDataFormat    iDataFormat;    // 10-bit packed or 16-bit unpacked
    int         nRecordDataStart;
    int         nRecordDataEnd;
    int         nDataStartOffset;
    int         nRecordSize;
    int         nRecordSizeFromHeader;
    GUInt32     iInstrumentStatus;
    GUInt32     iChannelsMask;

    char        *pszGCPProjection;

    VSILFILE   *fp;

    int         bFetchGeolocation;
    int         bGuessDataFormat;

    int         bByteSwap;
    
    int             bExposeMaskBand;
    GDALRasterBand* poMaskBand;

    void        ProcessRecordHeaders();
    int         FetchGCPs( GDAL_GCP *, GByte *, int );
    void        FetchNOAA9TimeCode(TimeCode *, const GByte *, int *);
    void        FetchNOAA15TimeCode(TimeCode *, const GByte *, int *);
    void        FetchTimeCode( TimeCode *psTime, const void *pRecordHeader,
                               int *peLocationIndicator );
    CPLErr      ProcessDatasetHeader(const char* pszFilename);
    int         ComputeFileOffsets();
    
    void        FetchMetadata();
    void        FetchMetadataNOAA15();

    vsi_l_offset GetLineOffset(int nBlockYOff);

    GUInt16     GetUInt16(const void* pabyData);
    GInt16      GetInt16(const void* pabyData);
    GUInt32     GetUInt32(const void* pabyData);
    GInt32      GetInt32(const void* pabyData);

    static L1BFileFormat  DetectFormat( const char* pszFilename,
                              const GByte* pabyHeader, int nHeaderBytes );

  public:
                L1BDataset( L1BFileFormat );
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
    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
};

/************************************************************************/
/* ==================================================================== */
/*                            L1BMaskBand                               */
/* ==================================================================== */
/************************************************************************/

class L1BMaskBand: public GDALPamRasterBand
{
    friend class L1BDataset;

  public:

                L1BMaskBand( L1BDataset * );
    
    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                            L1BMaskBand()                             */
/************************************************************************/

L1BMaskBand::L1BMaskBand( L1BDataset *poDS )
{
    CPLAssert(poDS->eL1BFormat == L1B_NOAA15 ||
              poDS->eL1BFormat == L1B_NOAA15_NOHDR);

    this->poDS = poDS;
    eDataType = GDT_Byte;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr L1BMaskBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                int nBlockYOff,
                                void * pImage )
{
    L1BDataset  *poGDS = (L1BDataset *) poDS;

    VSIFSeekL( poGDS->fp, poGDS->GetLineOffset(nBlockYOff) + 24, SEEK_SET );

    GByte abyData[4];
    VSIFReadL( abyData, 1, 4, poGDS->fp );
    GUInt32 n32 = poGDS->GetUInt32(abyData);

    if( (n32 >> 31) != 0 ) /* fatal flag */
        memset(pImage, 0, nBlockXSize);
    else
        memset(pImage, 255, nBlockXSize);

    return CE_None;
}

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
/*                           GetMaskBand()                              */
/************************************************************************/

GDALRasterBand *L1BRasterBand::GetMaskBand()
{
    L1BDataset  *poGDS = (L1BDataset *) poDS;
    if( poGDS->poMaskBand )
        return poGDS->poMaskBand;
    return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int L1BRasterBand::GetMaskFlags()
{
    L1BDataset  *poGDS = (L1BDataset *) poDS;
    if( poGDS->poMaskBand )
        return GMF_PER_DATASET;
    return GDALPamRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr L1BRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    L1BDataset  *poGDS = (L1BDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poGDS->fp, poGDS->GetLineOffset(nBlockYOff), SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Read data into the buffer.                                      */
/* -------------------------------------------------------------------- */
    GUInt16     *iScan = NULL;          // Unpacked 16-bit scanline buffer
    int         i, j;

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
                    GUInt32 iWord1 = poGDS->GetUInt32( &iRawScan[i] );
                    GUInt32 iWord2 = iWord1 & 0x3FF00000;

                    iScan[j++] = (GUInt16) (iWord2 >> 20);
                    iWord2 = iWord1 & 0x000FFC00;
                    iScan[j++] = (GUInt16) (iWord2 >> 10);
                    iScan[j++] = (GUInt16) (iWord1 & 0x000003FF);
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
                    iScan[i] = poGDS->GetUInt16( &iRawScan[poGDS->nRecordDataStart
                        / (int)sizeof(iRawScan[0]) + i] );
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
    {
        for( i = 0, j = 0; i < nBlockSize; i++ )
        {
            ((GUInt16 *) pImage)[i] = iScan[j + nBand - 1];
            j += poGDS->nBands;
        }
    }
    else
    {
        for ( i = nBlockSize - 1, j = 0; i >= 0; i-- )
        {
            ((GUInt16 *) pImage)[i] = iScan[j + nBand - 1];
            j += poGDS->nBands;
        }
    }
    
    CPLFree(iScan);
    return CE_None;
}

/************************************************************************/
/*                           L1BDataset()                               */
/************************************************************************/

L1BDataset::L1BDataset( L1BFileFormat eL1BFormat )

{
    eSource = UNKNOWN_STATION;
    eProcCenter = UNKNOWN_CENTER;
    // sStartTime
    // sStopTime
    bHighGCPDensityStrategy = CSLTestBoolean(CPLGetConfigOption("L1B_HIGH_GCP_DENSITY", "TRUE"));
    pasGCPList = NULL;
    nGCPCount = 0;
    iGCPOffset = 0;
    iGCPCodeOffset = 0;
    iCLAVRStart = 0;
    nGCPsPerLine = 0;
    eLocationIndicator = DESCEND; // XXX: should be initialised
    iGCPStart = 0;
    iGCPStep = 0;
    this->eL1BFormat = eL1BFormat;
    nBufferSize = 0;
    eSpacecraftID = TIROSN;
    eProductType = HRPT;
    iDataFormat = PACKED10BIT;
    nRecordDataStart = 0;
    nRecordDataEnd = 0;
    nDataStartOffset = 0;
    nRecordSize = 0;
    nRecordSizeFromHeader = 0;
    iInstrumentStatus = 0;
    iChannelsMask = 0;
    pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",7043]],TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",6322]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AUTHORITY[\"EPSG\",4322]]" );
    fp = NULL;
    bFetchGeolocation = FALSE;
    bGuessDataFormat = FALSE;
    bByteSwap = CPL_IS_LSB; /* L1B is normally big-endian ordered, so byte-swap on little-endian CPU */
    bExposeMaskBand = FALSE;
    poMaskBand = NULL;
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
    delete poMaskBand;
}

/************************************************************************/
/*                          GetLineOffset()                             */
/************************************************************************/

vsi_l_offset L1BDataset::GetLineOffset(int nBlockYOff)
{
    return (eLocationIndicator == DESCEND) ?
        nDataStartOffset + (vsi_l_offset)nBlockYOff * nRecordSize :
        nDataStartOffset +
            (vsi_l_offset)(nRasterYSize - nBlockYOff - 1) * nRecordSize;
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
/*      Byte swapping helpers                                           */
/************************************************************************/

GUInt16 L1BDataset::GetUInt16(const void* pabyData)
{
    GUInt16 iTemp;
    memcpy(&iTemp, pabyData, 2);
    if( bByteSwap )
        return CPL_SWAP16(iTemp);
    return iTemp;
}

GInt16 L1BDataset::GetInt16(const void* pabyData)
{
    GInt16 iTemp;
    memcpy(&iTemp, pabyData, 2);
    if( bByteSwap )
        return CPL_SWAP16(iTemp);
    return iTemp;
}

GUInt32 L1BDataset::GetUInt32(const void* pabyData)
{
    GUInt32 lTemp;
    memcpy(&lTemp, pabyData, 4);
    if( bByteSwap )
        return CPL_SWAP32(lTemp);
    return lTemp;
}

GInt32 L1BDataset::GetInt32(const void* pabyData)
{
    GInt32 lTemp;
    memcpy(&lTemp, pabyData, 4);
    if( bByteSwap )
        return CPL_SWAP32(lTemp);
    return lTemp;

}

/************************************************************************/
/*      Fetch timecode from the record header (NOAA9-NOAA14 version)    */
/************************************************************************/

void L1BDataset::FetchNOAA9TimeCode( TimeCode *psTime,
                                     const GByte *piRecordHeader,
                                     int *peLocationIndicator )
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
    if ( peLocationIndicator )
    {
        *peLocationIndicator =
            ((piRecordHeader[8] & 0x02) == 0) ? ASCEND : DESCEND;
    }
}

/************************************************************************/
/*      Fetch timecode from the record header (NOAA15-METOP2 version)   */
/************************************************************************/

void L1BDataset::FetchNOAA15TimeCode( TimeCode *psTime,
                                      const GByte *pabyRecordHeader,
                                      int *peLocationIndicator )
{
    psTime->SetYear(GetUInt16(pabyRecordHeader + 2));
    psTime->SetDay(GetUInt16(pabyRecordHeader + 4));
    psTime->SetMillisecond(GetUInt32(pabyRecordHeader+8));
    if ( peLocationIndicator )
    {
        // FIXME: hemisphere
        *peLocationIndicator =
            ((GetUInt16(pabyRecordHeader + 12) & 0x8000) == 0) ? ASCEND : DESCEND;
    }
}
/************************************************************************/
/*                          FetchTimeCode()                             */
/************************************************************************/

void L1BDataset::FetchTimeCode( TimeCode *psTime,
                                const void *pRecordHeader,
                                int *peLocationIndicator )
{
    if (eSpacecraftID <= NOAA14)
    {
        FetchNOAA9TimeCode( psTime, (const GByte *) pRecordHeader,
                            peLocationIndicator );
    }
    else
    {
        FetchNOAA15TimeCode( psTime, (const GByte *) pRecordHeader,
                             peLocationIndicator );
    }
}

/************************************************************************/
/*      Fetch GCPs from the individual scanlines                        */
/************************************************************************/

int L1BDataset::FetchGCPs( GDAL_GCP *pasGCPListRow,
                           GByte *pabyRecordHeader, int iLine )
{
    // LAC and HRPT GCPs are tied to the center of pixel,
    // GAC ones are slightly displaced.
    double  dfDelta = (eProductType == GAC) ? 0.9 : 0.5;
    double  dfPixel = (eLocationIndicator == DESCEND) ?
        iGCPStart + dfDelta : (nRasterXSize - (iGCPStart + dfDelta));

    int     nGCPs;
    if ( eSpacecraftID <= NOAA14 )
    {
        // NOAA9-NOAA14 records have an indicator of number of working GCPs.
        // Number of good GCPs may be smaller than the total amount of points.
        nGCPs = (*(pabyRecordHeader + iGCPCodeOffset) < nGCPsPerLine) ?
            *(pabyRecordHeader + iGCPCodeOffset) : nGCPsPerLine;
#ifdef DEBUG_VERBOSE
        CPLDebug( "L1B", "iGCPCodeOffset=%d, nGCPsPerLine=%d, nGoodGCPs=%d",
                  iGCPCodeOffset, nGCPsPerLine, nGCPs );
#endif
    }
    else
        nGCPs = nGCPsPerLine;

    pabyRecordHeader += iGCPOffset;

    int nGCPCountRow = 0;
    while ( nGCPs-- )
    {
        if ( eSpacecraftID <= NOAA14 )
        {
            GInt16  nRawY = GetInt16( pabyRecordHeader );
            pabyRecordHeader += sizeof(GInt16);
            GInt16  nRawX = GetInt16( pabyRecordHeader );
            pabyRecordHeader += sizeof(GInt16);

            pasGCPListRow[nGCPCountRow].dfGCPY = nRawY / L1B_NOAA9_GCP_SCALE;
            pasGCPListRow[nGCPCountRow].dfGCPX = nRawX / L1B_NOAA9_GCP_SCALE;
        }
        else
        {
            GInt32  nRawY = GetInt32( pabyRecordHeader );
            pabyRecordHeader += sizeof(GInt32);
            GInt32  nRawX = GetInt32( pabyRecordHeader );
            pabyRecordHeader += sizeof(GInt32);

            pasGCPListRow[nGCPCountRow].dfGCPY = nRawY / L1B_NOAA15_GCP_SCALE;
            pasGCPListRow[nGCPCountRow].dfGCPX = nRawX / L1B_NOAA15_GCP_SCALE;
        }

        if ( pasGCPListRow[nGCPCountRow].dfGCPX < -180
             || pasGCPListRow[nGCPCountRow].dfGCPX > 180
             || pasGCPListRow[nGCPCountRow].dfGCPY < -90
             || pasGCPListRow[nGCPCountRow].dfGCPY > 90 )
            continue;

        pasGCPListRow[nGCPCountRow].dfGCPZ = 0.0;
        pasGCPListRow[nGCPCountRow].dfGCPPixel = dfPixel;
        dfPixel += (eLocationIndicator == DESCEND) ? iGCPStep : -iGCPStep;
        pasGCPListRow[nGCPCountRow].dfGCPLine =
            (double)( (eLocationIndicator == DESCEND) ?
                iLine : nRasterYSize - iLine - 1 ) + 0.5;
        nGCPCountRow++;
    }
    return nGCPCountRow;
}

/************************************************************************/
/*                      ProcessRecordHeaders()                          */
/************************************************************************/

void L1BDataset::ProcessRecordHeaders()
{
    void    *pRecordHeader = CPLMalloc( nRecordDataStart );

    VSIFSeekL(fp, nDataStartOffset, SEEK_SET);
    VSIFReadL(pRecordHeader, 1, nRecordDataStart, fp);

    FetchTimeCode( &sStartTime, pRecordHeader, &eLocationIndicator );

    VSIFSeekL( fp, nDataStartOffset + (nRasterYSize - 1) * nRecordSize,
              SEEK_SET);
    VSIFReadL( pRecordHeader, 1, nRecordDataStart, fp );

    FetchTimeCode( &sStopTime, pRecordHeader, NULL );

/* -------------------------------------------------------------------- */
/*      Pick a skip factor so that we will get roughly 20 lines         */
/*      worth of GCPs.  That should give respectible coverage on all    */
/*      but the longest swaths.                                         */
/* -------------------------------------------------------------------- */
    int nTargetLines;
    double dfLineStep;

    if( bHighGCPDensityStrategy )
    {
        if (nRasterYSize < nGCPsPerLine)
        {
            nTargetLines = nRasterYSize;
        }
        else
        {
            int nColStep;
            nColStep = nRasterXSize / nGCPsPerLine;
            if (nRasterYSize >= nRasterXSize)
            {
                dfLineStep = nColStep;
            }
            else
            {
                dfLineStep = nRasterYSize / nGCPsPerLine;
            }
            nTargetLines = nRasterYSize / dfLineStep;
        }
    }
    else
    {
        nTargetLines = MIN(DESIRED_LINES_OF_GCPS, nRasterYSize);
    }
    dfLineStep = 1.0 * (nRasterYSize - 1) / ( nTargetLines - 1 );

/* -------------------------------------------------------------------- */
/*      Initialize the GCP list.                                        */
/* -------------------------------------------------------------------- */
    pasGCPList = (GDAL_GCP *)VSICalloc( nTargetLines * nGCPsPerLine,
                                        sizeof(GDAL_GCP) );
    if (pasGCPList == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
        CPLFree( pRecordHeader );
        return;
    }
    GDALInitGCPs( nTargetLines * nGCPsPerLine, pasGCPList );

/* -------------------------------------------------------------------- */
/*      Fetch the GCPs for each selected line.  We force the last       */
/*      line sampled to be the last line in the dataset even if that    */
/*      leaves a bigger than expected gap.                              */
/* -------------------------------------------------------------------- */
    int iStep;
    int iPrevLine = -1;

    for( iStep = 0; iStep < nTargetLines; iStep++ )
    {
        int iLine;

        if( iStep == nTargetLines - 1 )
            iLine = nRasterYSize - 1;
        else
            iLine = (int)(dfLineStep * iStep);
        if( iLine == iPrevLine )
            continue;
        iPrevLine = iLine;

        VSIFSeekL( fp, nDataStartOffset + iLine * nRecordSize, SEEK_SET );
        VSIFReadL( pRecordHeader, 1, nRecordDataStart, fp );

        int nGCPsOnThisLine = FetchGCPs( pasGCPList + nGCPCount, (GByte *)pRecordHeader, iLine );

        if( !bHighGCPDensityStrategy )
        {
/* -------------------------------------------------------------------- */
/*      We don't really want too many GCPs per line.  Downsample to     */
/*      11 per line.                                                    */
/* -------------------------------------------------------------------- */

            int iGCP;
            int nDesiredGCPsPerLine = MIN(DESIRED_GCPS_PER_LINE,nGCPsOnThisLine);
            int nGCPStep = ( nDesiredGCPsPerLine > 1 ) ?
                ( nGCPsOnThisLine - 1 ) / ( nDesiredGCPsPerLine-1 ) : 1;
            int iSrcGCP = nGCPCount;
            int iDstGCP = nGCPCount;

            if( nGCPStep == 0 )
                nGCPStep = 1;

            for( iGCP = 0; iGCP < nDesiredGCPsPerLine; iGCP++ )
            {
                if( iGCP == nDesiredGCPsPerLine - 1 )
                    iSrcGCP = nGCPCount + nGCPsOnThisLine - 1;
                else
                    iSrcGCP += nGCPStep;
                iDstGCP ++;

                pasGCPList[iDstGCP].dfGCPX = pasGCPList[iSrcGCP].dfGCPX;
                pasGCPList[iDstGCP].dfGCPY = pasGCPList[iSrcGCP].dfGCPY;
                pasGCPList[iDstGCP].dfGCPPixel = pasGCPList[iSrcGCP].dfGCPPixel;
                pasGCPList[iDstGCP].dfGCPLine = pasGCPList[iSrcGCP].dfGCPLine;
            }

            nGCPCount += nDesiredGCPsPerLine;
        }
        else
        {
            nGCPCount += nGCPsOnThisLine;
        }
    }

    if( nGCPCount < nTargetLines * nGCPsPerLine )
    {
        GDALDeinitGCPs( nTargetLines * nGCPsPerLine - nGCPCount, 
                        pasGCPList + nGCPCount );
    }

    CPLFree( pRecordHeader );

/* -------------------------------------------------------------------- */
/*      Set fetched information as metadata records                     */
/* -------------------------------------------------------------------- */
    // Time of first scanline
    SetMetadataItem( "START",  sStartTime.PrintTime() );
    // Time of last scanline
    SetMetadataItem( "STOP",  sStopTime.PrintTime() );
    // AVHRR Earth location indication

    switch( eLocationIndicator )
    {
        case ASCEND:
            SetMetadataItem( "LOCATION", "Ascending" );
            break;
        case DESCEND:
        default:
            SetMetadataItem( "LOCATION", "Descending" );
            break;
    }

}

/************************************************************************/
/*                           FetchMetadata()                            */
/************************************************************************/

void L1BDataset::FetchMetadata()
{
    if( eL1BFormat != L1B_NOAA9 )
    {
        FetchMetadataNOAA15();
        return;
    }

    const char* pszDir = CPLGetConfigOption("L1B_METADATA_DIRECTORY", NULL);
    if( pszDir == NULL )
    {
        pszDir = CPLGetPath(GetDescription());
        if( pszDir[0] == '\0' )
            pszDir = ".";
    }
    CPLString osMetadataFile(CPLSPrintf("%s/%s_metadata.csv", pszDir, CPLGetFilename(GetDescription())));
    VSILFILE* fpCSV = VSIFOpenL(osMetadataFile, "wb");
    if( fpCSV == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create metadata file : %s",
                 osMetadataFile.c_str());
        return;
    }

    VSIFPrintfL(fpCSV, "SCANLINE,NBLOCKYOFF,YEAR,DAY,MS_IN_DAY,");
    VSIFPrintfL(fpCSV, "FATAL_FLAG,TIME_ERROR,DATA_GAP,DATA_JITTER,INSUFFICIENT_DATA_FOR_CAL,NO_EARTH_LOCATION,DESCEND,P_N_STATUS,");
    VSIFPrintfL(fpCSV, "BIT_SYNC_STATUS,SYNC_ERROR,FRAME_SYNC_ERROR,FLYWHEELING,BIT_SLIPPAGE,C3_SBBC,C4_SBBC,C5_SBBC,");
    VSIFPrintfL(fpCSV, "TIP_PARITY_FRAME_1,TIP_PARITY_FRAME_2,TIP_PARITY_FRAME_3,TIP_PARITY_FRAME_4,TIP_PARITY_FRAME_5,");
    VSIFPrintfL(fpCSV, "SYNC_ERRORS,");
    VSIFPrintfL(fpCSV, "CAL_SLOPE_C1,CAL_INTERCEPT_C1,CAL_SLOPE_C2,CAL_INTERCEPT_C2,CAL_SLOPE_C3,CAL_INTERCEPT_C3,CAL_SLOPE_C4,CAL_INTERCEPT_C4,CAL_SLOPE_C5,CAL_INTERCEPT_C5,");
    VSIFPrintfL(fpCSV, "NUM_SOLZENANGLES_EARTHLOCPNTS");
    VSIFPrintfL(fpCSV, "\n");

    GByte* pabyRecordHeader = (GByte*)CPLMalloc(nRecordDataStart);

    for( int nBlockYOff = 0; nBlockYOff < nRasterYSize; nBlockYOff ++ )
    {
/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
        VSIFSeekL( fp, GetLineOffset(nBlockYOff), SEEK_SET );

        VSIFReadL( pabyRecordHeader, 1, nRecordDataStart, fp );

        GUInt16 nScanlineNumber = GetUInt16(pabyRecordHeader);

        TimeCode timeCode;
        FetchTimeCode( &timeCode, pabyRecordHeader, NULL );

        VSIFPrintfL(fpCSV,
                    "%d,%d,%d,%d,%d,",
                    nScanlineNumber,
                    nBlockYOff,
                    (int)timeCode.GetYear(),
                    (int)timeCode.GetDay(),
                    (int)timeCode.GetMillisecond());
        VSIFPrintfL(fpCSV,
                    "%d,%d,%d,%d,%d,%d,%d,%d,",
                    (pabyRecordHeader[8] >> 7) & 1,
                    (pabyRecordHeader[8] >> 6) & 1,
                    (pabyRecordHeader[8] >> 5) & 1,
                    (pabyRecordHeader[8] >> 4) & 1,
                    (pabyRecordHeader[8] >> 3) & 1,
                    (pabyRecordHeader[8] >> 2) & 1,
                    (pabyRecordHeader[8] >> 1) & 1,
                    (pabyRecordHeader[8] >> 0) & 1);
        VSIFPrintfL(fpCSV,
                    "%d,%d,%d,%d,%d,%d,%d,%d,",
                    (pabyRecordHeader[9] >> 7) & 1,
                    (pabyRecordHeader[9] >> 6) & 1,
                    (pabyRecordHeader[9] >> 5) & 1,
                    (pabyRecordHeader[9] >> 4) & 1,
                    (pabyRecordHeader[9] >> 3) & 1,
                    (pabyRecordHeader[9] >> 2) & 1,
                    (pabyRecordHeader[9] >> 1) & 1,
                    (pabyRecordHeader[9] >> 0) & 1);
        VSIFPrintfL(fpCSV,
                    "%d,%d,%d,%d,%d,",
                    (pabyRecordHeader[10] >> 7) & 1,
                    (pabyRecordHeader[10] >> 6) & 1,
                    (pabyRecordHeader[10] >> 5) & 1,
                    (pabyRecordHeader[10] >> 4) & 1,
                    (pabyRecordHeader[10] >> 3) & 1);
        VSIFPrintfL(fpCSV, "%d,", pabyRecordHeader[11] >> 2);
        GInt32 i32;
        for(int i=0;i<10;i++)
        {
            i32 = GetInt32(pabyRecordHeader + 12 + 4 *i);
            /* Scales : http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/podug/html/c3/sec3-3.htm */
            if( (i % 2) == 0 )
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(2.0, 30.0));
            else
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(2.0, 22.0));
        }
        VSIFPrintfL(fpCSV, "%d", pabyRecordHeader[52]);
        VSIFPrintfL(fpCSV, "\n");
    }

    CPLFree(pabyRecordHeader);
    VSIFCloseL(fpCSV);
}

/************************************************************************/
/*                         FetchMetadataNOAA15()                        */
/************************************************************************/

void L1BDataset::FetchMetadataNOAA15()
{
    int i,j;
    const char* pszDir = CPLGetConfigOption("L1B_METADATA_DIRECTORY", NULL);
    if( pszDir == NULL )
    {
        pszDir = CPLGetPath(GetDescription());
        if( pszDir[0] == '\0' )
            pszDir = ".";
    }
    CPLString osMetadataFile(CPLSPrintf("%s/%s_metadata.csv", pszDir, CPLGetFilename(GetDescription())));
    VSILFILE* fpCSV = VSIFOpenL(osMetadataFile, "wb");
    if( fpCSV == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create metadata file : %s",
                 osMetadataFile.c_str());
        return;
    }

    VSIFPrintfL(fpCSV, "SCANLINE,NBLOCKYOFF,YEAR,DAY,MS_IN_DAY,SAT_CLOCK_DRIF_DELTA,SOUTHBOUND,SCANTIME_CORRECTED,C3_SELECT,");
    VSIFPrintfL(fpCSV, "FATAL_FLAG,TIME_ERROR,DATA_GAP,INSUFFICIENT_DATA_FOR_CAL,"
                       "NO_EARTH_LOCATION,FIRST_GOOD_TIME_AFTER_CLOCK_UPDATE,"
                       "INSTRUMENT_STATUS_CHANGED,SYNC_LOCK_DROPPED,"
                       "FRAME_SYNC_ERROR,FRAME_SYNC_DROPPED_LOCK,FLYWHEELING,"
                       "BIT_SLIPPAGE,TIP_PARITY_ERROR,REFLECTED_SUNLIGHT_C3B,"
                       "REFLECTED_SUNLIGHT_C4,REFLECTED_SUNLIGHT_C5,RESYNC,P_N_STATUS,");
    VSIFPrintfL(fpCSV, "BAD_TIME_CAN_BE_INFERRED,BAD_TIME_CANNOT_BE_INFERRED,"
                       "TIME_DISCONTINUITY,REPEAT_SCAN_TIME,");
    VSIFPrintfL(fpCSV, "UNCALIBRATED_BAD_TIME,CALIBRATED_FEWER_SCANLINES,"
                       "UNCALIBRATED_BAD_PRT,CALIBRATED_MARGINAL_PRT,"
                       "UNCALIBRATED_CHANNELS,");
    VSIFPrintfL(fpCSV, "NO_EARTH_LOC_BAD_TIME,EARTH_LOC_QUESTIONABLE_TIME,"
                       "EARTH_LOC_QUESTIONABLE,EARTH_LOC_VERY_QUESTIONABLE,");
    VSIFPrintfL(fpCSV, "C3B_UNCALIBRATED,C3B_QUESTIONABLE,C3B_ALL_BLACKBODY,"
                       "C3B_ALL_SPACEVIEW,C3B_MARGINAL_BLACKBODY,C3B_MARGINAL_SPACEVIEW,");
    VSIFPrintfL(fpCSV, "C4_UNCALIBRATED,C4_QUESTIONABLE,C4_ALL_BLACKBODY,"
                       "C4_ALL_SPACEVIEW,C4_MARGINAL_BLACKBODY,C4_MARGINAL_SPACEVIEW,");
    VSIFPrintfL(fpCSV, "C5_UNCALIBRATED,C5_QUESTIONABLE,C5_ALL_BLACKBODY,"
                       "C5_ALL_SPACEVIEW,C5_MARGINAL_BLACKBODY,C5_MARGINAL_SPACEVIEW,");
    VSIFPrintfL(fpCSV, "BIT_ERRORS,");
    for(i=0;i<3;i++)
    {
        const char* pszChannel = (i==0) ? "C1" : (i==1) ? "C2" : "C3A";
        for(j=0;j<3;j++)
        {
            const char* pszType = (j==0) ? "OP": (j==1) ? "TEST": "PRELAUNCH";
            VSIFPrintfL(fpCSV, "VIS_%s_CAL_%s_SLOPE_1,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "VIS_%s_CAL_%s_INTERCEPT_1,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "VIS_%s_CAL_%s_SLOPE_2,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "VIS_%s_CAL_%s_INTERCEPT_2,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "VIS_%s_CAL_%s_INTERSECTION,", pszType, pszChannel);
        }
    }
    for(i=0;i<3;i++)
    {
        const char* pszChannel = (i==0) ? "C3B" : (i==1) ? "C4" : "C5";
        for(j=0;j<2;j++)
        {
            const char* pszType = (j==0) ? "OP": "TEST";
            VSIFPrintfL(fpCSV, "IR_%s_CAL_%s_COEFF_1,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "IR_%s_CAL_%s_COEFF_2,", pszType, pszChannel);
            VSIFPrintfL(fpCSV, "IR_%s_CAL_%s_COEFF_3,", pszType, pszChannel);
        }
    }
    VSIFPrintfL(fpCSV, "EARTH_LOC_CORR_TIP_EULER,EARTH_LOC_IND,"
                       "SPACECRAFT_ATT_CTRL,ATT_SMODE,ATT_PASSIVE_WHEEL_TEST,"
                       "TIME_TIP_EULER,TIP_EULER_ROLL,TIP_EULER_PITCH,TIP_EULER_YAW,"
                       "SPACECRAFT_ALT");
    VSIFPrintfL(fpCSV, "\n");

    GByte* pabyRecordHeader = (GByte*)CPLMalloc(nRecordDataStart);
    GInt16 i16;
    GUInt16 n16;
    GInt32 i32;
    GUInt32 n32;

    for( int nBlockYOff = 0; nBlockYOff < nRasterYSize; nBlockYOff ++ )
    {
/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
        VSIFSeekL( fp, GetLineOffset(nBlockYOff), SEEK_SET );

        VSIFReadL( pabyRecordHeader, 1, nRecordDataStart, fp );

        GUInt16 nScanlineNumber = GetUInt16(pabyRecordHeader);

        TimeCode timeCode;
        FetchTimeCode( &timeCode, pabyRecordHeader, NULL );

        /* Clock drift delta */
        i16 = GetInt16(pabyRecordHeader + 6);
        /* Scanline bit field */
        n16 = GetInt16(pabyRecordHeader + 12);

        VSIFPrintfL(fpCSV,
                    "%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                    nScanlineNumber,
                    nBlockYOff,
                    (int)timeCode.GetYear(),
                    (int)timeCode.GetDay(),
                    (int)timeCode.GetMillisecond(),
                    i16,
                    (n16 >> 15) & 1,
                    (n16 >> 14) & 1,
                    (n16) & 3);

        n32 = GetUInt32(pabyRecordHeader + 24);
        VSIFPrintfL(fpCSV,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                    (n32 >> 31) & 1,
                    (n32 >> 30) & 1,
                    (n32 >> 29) & 1,
                    (n32 >> 28) & 1,
                    (n32 >> 27) & 1,
                    (n32 >> 26) & 1,
                    (n32 >> 25) & 1,
                    (n32 >> 24) & 1,
                    (n32 >> 23) & 1,
                    (n32 >> 22) & 1,
                    (n32 >> 21) & 1,
                    (n32 >> 20) & 1,
                    (n32 >> 8) & 1,
                    (n32 >> 6) & 3,
                    (n32 >> 4) & 3,
                    (n32 >> 2) & 3,
                    (n32 >> 1) & 1,
                    (n32 >> 0) & 1);

        n32 = GetUInt32(pabyRecordHeader + 28);
        VSIFPrintfL(fpCSV,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                    (n32 >> 23) & 1,
                    (n32 >> 22) & 1,
                    (n32 >> 21) & 1,
                    (n32 >> 20) & 1,
                    (n32 >> 15) & 1,
                    (n32 >> 14) & 1,
                    (n32 >> 13) & 1,
                    (n32 >> 12) & 1,
                    (n32 >> 11) & 1,
                    (n32 >> 7) & 1,
                    (n32 >> 6) & 1,
                    (n32 >> 5) & 1,
                    (n32 >> 4) & 1);

        for(i=0;i<3;i++)
        {
            n16 = GetUInt16(pabyRecordHeader + 32 + 2 * i);
            VSIFPrintfL(fpCSV,"%d,%d,%d,%d,%d,%d,",
                    (n16 >> 7) & 1,
                    (n16 >> 6) & 1,
                    (n16 >> 5) & 1,
                    (n16 >> 4) & 1,
                    (n16 >> 2) & 1,
                    (n16 >> 1) & 1);
        }

        /* Bit errors */
        n16 = GetUInt16(pabyRecordHeader + 38);
        VSIFPrintfL(fpCSV, "%d,", n16);

        int nOffset = 48;
        for(i=0;i<3;i++)
        {
            for(j=0;j<3;j++)
            {
                i32 = GetInt32(pabyRecordHeader + nOffset);
                nOffset += 4;
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(10.0, 7.0));
                i32 = GetInt32(pabyRecordHeader + nOffset);
                nOffset += 4;
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(10.0, 6.0));
                i32 = GetInt32(pabyRecordHeader + nOffset);
                nOffset += 4;
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(10.0, 7.0));
                i32 = GetInt32(pabyRecordHeader + nOffset);
                nOffset += 4;
                VSIFPrintfL(fpCSV, "%f,", i32 / pow(10.0, 6.0));
                i32 = GetInt32(pabyRecordHeader + nOffset);
                nOffset += 4;
                VSIFPrintfL(fpCSV, "%d,", i32);
            }
        }
        for(i=0;i<18;i++)
        {
            i32 = GetInt32(pabyRecordHeader + nOffset);
            nOffset += 4;
            VSIFPrintfL(fpCSV, "%f,", i32 / pow(10.0, 6.0));
        }

        n32 = GetUInt32(pabyRecordHeader + 312);
        VSIFPrintfL(fpCSV,"%d,%d,%d,%d,%d,",
                    (n32 >> 16) & 1,
                    (n32 >> 12) & 15,
                    (n32 >> 8) & 15,
                    (n32 >> 4) & 15,
                    (n32 >> 0) & 15);

        n32 = GetUInt32(pabyRecordHeader + 316);
        VSIFPrintfL(fpCSV,"%d,",n32);

        for(i=0;i<3;i++)
        {
            i16 = GetUInt16(pabyRecordHeader + 320 + 2 * i);
            VSIFPrintfL(fpCSV,"%f,",i16 / pow(10.0,3.0));
        }

        n16 = GetUInt16(pabyRecordHeader + 326);
        VSIFPrintfL(fpCSV,"%f",n16 / pow(10.0,1.0));

        VSIFPrintfL(fpCSV, "\n");
    }

    CPLFree(pabyRecordHeader);
    VSIFCloseL(fpCSV);
}

/************************************************************************/
/*                           EBCDICToASCII                              */
/************************************************************************/

static const GByte EBCDICToASCII[] =
{
0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F, 0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87, 0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
0x2D, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x5C, 0x00, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F,
};

/************************************************************************/
/*                      ProcessDatasetHeader()                          */
/************************************************************************/

CPLErr L1BDataset::ProcessDatasetHeader(const char* pszFilename)
{
    char    szDatasetName[L1B_DATASET_NAME_SIZE + 1];

    if ( eL1BFormat == L1B_NOAA9 )
    {
        GByte   abyTBMHeader[L1B_NOAA9_HEADER_SIZE];

        if ( VSIFSeekL( fp, 0, SEEK_SET ) < 0
             || VSIFReadL( abyTBMHeader, 1, L1B_NOAA9_HEADER_SIZE,
                           fp ) < L1B_NOAA9_HEADER_SIZE )
        {
            CPLDebug( "L1B", "Can't read NOAA-9/14 TBM header." );
            return CE_Failure;
        }

        // If dataset name in EBCDIC, decode it in ASCII
        if ( *(abyTBMHeader + 8 + 25) == 'K'
            && *(abyTBMHeader + 8 + 30) == 'K'
            && *(abyTBMHeader + 8 + 33) == 'K'
            && *(abyTBMHeader + 8 + 40) == 'K'
            && *(abyTBMHeader + 8 + 46) == 'K'
            && *(abyTBMHeader + 8 + 52) == 'K'
            && *(abyTBMHeader + 8 + 61) == 'K' )
        {
            for(int i=0;i<L1B_DATASET_NAME_SIZE;i++)
                abyTBMHeader[L1B_NOAA9_HDR_NAME_OFF+i] =
                    EBCDICToASCII[abyTBMHeader[L1B_NOAA9_HDR_NAME_OFF+i]];
        }

        // Fetch dataset name. NOAA-9/14 datasets contain the names in TBM
        // header only, so read it there.
        memcpy( szDatasetName, abyTBMHeader + L1B_NOAA9_HDR_NAME_OFF,
                L1B_DATASET_NAME_SIZE );
        szDatasetName[L1B_DATASET_NAME_SIZE] = '\0';

        // Deal with a few NOAA <= 9 datasets with no dataset name in TBM header
        if( memcmp(szDatasetName,
                    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", L1B_DATASET_NAME_SIZE) == 0 &&
            strlen(pszFilename) == L1B_DATASET_NAME_SIZE )
        {
            strcpy(szDatasetName, pszFilename);
        }

        // Determine processing center where the dataset was created
        if ( EQUALN(szDatasetName, "CMS", 3) )
             eProcCenter = CMS;
        else if ( EQUALN(szDatasetName, "DSS", 3) )
             eProcCenter = DSS;
        else if ( EQUALN(szDatasetName, "NSS", 3) )
             eProcCenter = NSS;
        else if ( EQUALN(szDatasetName, "UKM", 3) )
             eProcCenter = UKM;
        else
             eProcCenter = UNKNOWN_CENTER;

        // Determine number of bands
        int     i;
        for ( i = 0; i < L1B_NOAA9_HDR_CHAN_SIZE; i++ )
        {
            if ( abyTBMHeader[L1B_NOAA9_HDR_CHAN_OFF + i] == 1
                 || abyTBMHeader[L1B_NOAA9_HDR_CHAN_OFF + i] == 'Y' )
            {
                nBands++;
                iChannelsMask |= (1 << i);
            }
        }
        if ( nBands == 0 || nBands > 5 )
        {
            nBands = 5;
            iChannelsMask = 0x1F;
        }

        // Determine data format (10-bit packed or 8/16-bit unpacked)
        if ( EQUALN((const char *)abyTBMHeader + L1B_NOAA9_HDR_WORD_OFF,
                    "10", 2) )
            iDataFormat = PACKED10BIT;
        else if ( EQUALN((const char *)abyTBMHeader + L1B_NOAA9_HDR_WORD_OFF,
                         "16", 2) )
            iDataFormat = UNPACKED16BIT;
        else if ( EQUALN((const char *)abyTBMHeader + L1B_NOAA9_HDR_WORD_OFF,
                         "08", 2) )
            iDataFormat = UNPACKED8BIT;
        else if ( EQUALN((const char *)abyTBMHeader + L1B_NOAA9_HDR_WORD_OFF,
                         "  ", 2)
                  || abyTBMHeader[L1B_NOAA9_HDR_WORD_OFF] == '\0' )
            /* Empty string can be found in the following samples : 
                http://www2.ncdc.noaa.gov/docs/podug/data/avhrr/franh.1b (10 bit)
                http://www2.ncdc.noaa.gov/docs/podug/data/avhrr/frang.1b (10 bit)
                http://www2.ncdc.noaa.gov/docs/podug/data/avhrr/calfilel.1b (16 bit)
                http://www2.ncdc.noaa.gov/docs/podug/data/avhrr/rapnzg.1b (16 bit)
                ftp://ftp.sat.dundee.ac.uk/misc/testdata/noaa12/hrptnoaa1b.dat (10 bit)
            */
            bGuessDataFormat = TRUE;
        else
        {
#ifdef DEBUG
            CPLDebug( "L1B", "Unknown data format \"%.2s\".",
                      abyTBMHeader + L1B_NOAA9_HDR_WORD_OFF );
#endif
            return CE_Failure;
        }

        // Now read the dataset header record
        GByte   abyRecHeader[L1B_NOAA9_HDR_REC_SIZE];
        if ( VSIFSeekL( fp, L1B_NOAA9_HEADER_SIZE, SEEK_SET ) < 0
             || VSIFReadL( abyRecHeader, 1, L1B_NOAA9_HDR_REC_SIZE,
                           fp ) < L1B_NOAA9_HDR_REC_SIZE )
        {
            CPLDebug( "L1B", "Can't read NOAA-9/14 record header." );
            return CE_Failure;
        }

        // Determine the spacecraft name
        switch ( abyRecHeader[L1B_NOAA9_HDR_REC_ID_OFF] )
        {
            case 4:
                eSpacecraftID = NOAA7;
                break;
            case 6:
                eSpacecraftID = NOAA8;
                break;
            case 7:
                eSpacecraftID = NOAA9;
                break;
            case 8:
                eSpacecraftID = NOAA10;
                break;
            case 1:
            {
                /* We could also use the time code to determine TIROS-N */
                if( strlen(pszFilename) == L1B_DATASET_NAME_SIZE &&
                    strncmp(pszFilename + 8, ".TN.", 4) == 0 )
                    eSpacecraftID = TIROSN;
                else
                    eSpacecraftID = NOAA11;
                break;
            }
            case 5:
                eSpacecraftID = NOAA12;
                break;
            case 2:
            {
                /* We could also use the time code to determine NOAA6 */
                if( strlen(pszFilename) == L1B_DATASET_NAME_SIZE &&
                    strncmp(pszFilename + 8, ".NA.", 4) == 0 )
                    eSpacecraftID = NOAA6;
                else
                    eSpacecraftID = NOAA13;
                break;
            }
            case 3:
                eSpacecraftID = NOAA14;
                break;
            default:
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unknown spacecraft ID \"%d\".",
                          abyRecHeader[L1B_NOAA9_HDR_REC_ID_OFF] );

                eSpacecraftID = NOAA9_UNKNOWN;
                break;
        }

        // Determine the product data type
        int iWord = abyRecHeader[L1B_NOAA9_HDR_REC_PROD_OFF] >> 4;
        switch ( iWord )
        {
            case 1:
                eProductType = LAC;
                break;
            case 2:
                eProductType = GAC;
                break;
            case 3:
                eProductType = HRPT;
                break;
            default:
#ifdef DEBUG
                CPLDebug( "L1B", "Unknown product type \"%d\".", iWord );
#endif
                return CE_Failure;
        }

        // Determine receiving station name
        iWord = ( abyRecHeader[L1B_NOAA9_HDR_REC_DSTAT_OFF] & 0x60 ) >> 5;
        switch( iWord )
        {
            case 1:
                eSource = GC;
                break;
            case 2:
                eSource = WI;
                break;
            case 3:
                eSource = SO;
                break;
            default:
                eSource = UNKNOWN_STATION;
                break;
        }
    }

    else if ( eL1BFormat == L1B_NOAA15 || eL1BFormat == L1B_NOAA15_NOHDR )
    {
        if ( eL1BFormat == L1B_NOAA15 )
        {
            GByte   abyARSHeader[L1B_NOAA15_HEADER_SIZE];

            if ( VSIFSeekL( fp, 0, SEEK_SET ) < 0
                 || VSIFReadL( abyARSHeader, 1, L1B_NOAA15_HEADER_SIZE,
                               fp ) < L1B_NOAA15_HEADER_SIZE )
            {
                CPLDebug( "L1B", "Can't read NOAA-15 ARS header." );
                return CE_Failure;
            }

            // Determine number of bands
            int     i;
            for ( i = 0; i < L1B_NOAA15_HDR_CHAN_SIZE; i++ )
            {
                if ( abyARSHeader[L1B_NOAA15_HDR_CHAN_OFF + i] == 1
                     || abyARSHeader[L1B_NOAA15_HDR_CHAN_OFF + i] == 'Y' )
                {
                    nBands++;
                    iChannelsMask |= (1 << i);
                }
            }
            if ( nBands == 0 || nBands > 5 )
            {
                nBands = 5;
                iChannelsMask = 0x1F;
            }

            // Determine data format (10-bit packed or 8/16-bit unpacked)
            if ( EQUALN((const char *)abyARSHeader + L1B_NOAA15_HDR_WORD_OFF,
                        "10", 2) )
                iDataFormat = PACKED10BIT;
            else if ( EQUALN((const char *)abyARSHeader + L1B_NOAA15_HDR_WORD_OFF,
                             "16", 2) )
                iDataFormat = UNPACKED16BIT;
            else if ( EQUALN((const char *)abyARSHeader + L1B_NOAA15_HDR_WORD_OFF,
                             "08", 2) )
                iDataFormat = UNPACKED8BIT;
            else
            {
#ifdef DEBUG
                CPLDebug( "L1B", "Unknown data format \"%.2s\".",
                          abyARSHeader + L1B_NOAA9_HDR_WORD_OFF );
#endif
                return CE_Failure;
            }
        }
        else
        {
            nBands = 5;
            iChannelsMask = 0x1F;
            iDataFormat = PACKED10BIT;
        }

        // Now read the dataset header record
        GByte   abyRecHeader[L1B_NOAA15_HDR_REC_SIZE];
        if ( VSIFSeekL( fp,
                        (eL1BFormat == L1B_NOAA15) ? L1B_NOAA15_HEADER_SIZE : 0,
                        SEEK_SET ) < 0
             || VSIFReadL( abyRecHeader, 1, L1B_NOAA15_HDR_REC_SIZE,
                           fp ) < L1B_NOAA15_HDR_REC_SIZE )
        {
            CPLDebug( "L1B", "Can't read NOAA-9/14 record header." );
            return CE_Failure;
        }

        // Fetch dataset name
        memcpy( szDatasetName, abyRecHeader + L1B_NOAA15_HDR_REC_NAME_OFF,
                L1B_DATASET_NAME_SIZE );
        szDatasetName[L1B_DATASET_NAME_SIZE] = '\0';

        // Determine processing center where the dataset was created
        if ( EQUALN((const char *)abyRecHeader
                    + L1B_NOAA15_HDR_REC_SITE_OFF, "CMS", 3) )
             eProcCenter = CMS;
        else if ( EQUALN((const char *)abyRecHeader
                         + L1B_NOAA15_HDR_REC_SITE_OFF, "DSS", 3) )
             eProcCenter = DSS;
        else if ( EQUALN((const char *)abyRecHeader
                         + L1B_NOAA15_HDR_REC_SITE_OFF, "NSS", 3) )
             eProcCenter = NSS;
        else if ( EQUALN((const char *)abyRecHeader
                         + L1B_NOAA15_HDR_REC_SITE_OFF, "UKM", 3) )
             eProcCenter = UKM;
        else
             eProcCenter = UNKNOWN_CENTER;

        int nFormatVersionYear, nFormatVersionDayOfYear, nHeaderRecCount;

        /* Some products from NOAA-18 and NOAA-19 coming from 'ess' processing station */
        /* have little-endian ordering. Try to detect it with some consistency checks */
        for(int i=0;i<=2;i++)
        {
            nFormatVersionYear = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_FORMAT_VERSION_YEAR_OFF);
            nFormatVersionDayOfYear = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_FORMAT_VERSION_DAY_OFF);
            nHeaderRecCount = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_HDR_REC_COUNT_OFF);
            if( i == 2 )
                break;
            if( !(nFormatVersionYear >= 1980 && nFormatVersionYear <= 2100) &&
                !(nFormatVersionDayOfYear <= 366) &&
                !(nHeaderRecCount == 1) )
            {
                if( i == 0 )
                    CPLDebug("L1B", "Trying little-endian ordering");
                else
                    CPLDebug("L1B", "Not completely convincing... Returning to big-endian order");
                bByteSwap = !bByteSwap;
            }
            else
                break;
        }
        nRecordSizeFromHeader = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_LOGICAL_REC_LENGTH_OFF);
        int nFormatVersion = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_FORMAT_VERSION_OFF);
        CPLDebug("L1B", "NOAA Level 1b Format Version Number = %d", nFormatVersion);
        CPLDebug("L1B", "Level 1b Format Version Year = %d", nFormatVersionYear);
        CPLDebug("L1B", "Level 1b Format Version Day of Year = %d", nFormatVersionDayOfYear);
        CPLDebug("L1B", "Logical Record Length of source Level 1b data set prior to processing = %d", nRecordSizeFromHeader);
        int nBlockSize = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_BLOCK_SIZE_OFF);
        CPLDebug("L1B", "Block Size of source Level 1b data set prior to processing = %d", nBlockSize);
        CPLDebug("L1B", "Count of Header Records in this Data Set = %d", nHeaderRecCount);
        
        int nDataRecordCount = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_DATA_RECORD_COUNT_OFF);
        CPLDebug("L1B", "Count of Data Records = %d", nDataRecordCount);
        
        int nCalibratedScanlineCount = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_CALIBRATED_SCANLINE_COUNT_OFF);
        CPLDebug("L1B", "Count of Calibrated, Earth Located Scan Lines = %d", nCalibratedScanlineCount);
        
        int nMissingScanlineCount = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_MISSING_SCANLINE_COUNT_OFF);
        CPLDebug("L1B", "Count of Missing Scan Lines = %d", nMissingScanlineCount);
        if( nMissingScanlineCount != 0 )
            bExposeMaskBand = TRUE;

        char szEllipsoid[8+1];
        memcpy(szEllipsoid, abyRecHeader + L1B_NOAA15_HDR_REC_ELLIPSOID_OFF, 8);
        szEllipsoid[8] = '\0';
        CPLDebug("L1B", "Reference Ellipsoid Model ID = '%s'", szEllipsoid);
        if( EQUAL(szEllipsoid, "WGS-84  ") )
        {
            CPLFree(pszGCPProjection);
            pszGCPProjection = CPLStrdup(SRS_WKT_WGS84);
        }
        else if( EQUAL(szEllipsoid, "  GRS 80") )
        {
            CPLFree(pszGCPProjection);
            pszGCPProjection = CPLStrdup("GEOGCS[\"GRS 1980(IUGG, 1980)\",DATUM[\"unknown\",SPHEROID[\"GRS80\",6378137,298.257222101],TOWGS84[0,0,0,0,0,0,0]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]");
        }

        // Determine the spacecraft name
		// See http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/klm/html/c8/sec83132-2.htm
        int iWord = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_ID_OFF);
        switch ( iWord )
        {
            case 2:
                eSpacecraftID = NOAA16;
                break;
            case 4:
                eSpacecraftID = NOAA15;
                break;
            case 6:
                eSpacecraftID = NOAA17;
                break;
            case 7:
                eSpacecraftID = NOAA18;
                break;
            case 8:
                eSpacecraftID = NOAA19;
                break;
            case 11:
                eSpacecraftID = METOP1;
                break;
            case 12:
                eSpacecraftID = METOP2;
                break;
            // METOP3 is not documented yet
            case 13:
                eSpacecraftID = METOP3;
                break;
            case 14:
                eSpacecraftID = METOP3;
                break;
            default:
#ifdef DEBUG
                CPLDebug( "L1B", "Unknown spacecraft ID \"%d\".", iWord );
#endif
                return CE_Failure;
        }

        // Determine the product data type
        iWord = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_PROD_OFF);
        switch ( iWord )
        {
            case 1:
                eProductType = LAC;
                break;
            case 2:
                eProductType = GAC;
                break;
            case 3:
                eProductType = HRPT;
                break;
            case 4:     // XXX: documentation specifies the code '4'
            case 13:    // for FRAC but real datasets contain '13 here.'
                eProductType = FRAC;
                break;
            default:
#ifdef DEBUG
                CPLDebug( "L1B", "Unknown product type \"%d\".", iWord );
#endif
                return CE_Failure;
        }

        // Fetch hinstrument status. Helps to determine whether we have
        // 3A or 3B channel in the dataset.
        iInstrumentStatus = GetUInt32(abyRecHeader + L1B_NOAA15_HDR_REC_STAT_OFF);

        // Determine receiving station name
        iWord = GetUInt16(abyRecHeader + L1B_NOAA15_HDR_REC_SRC_OFF);
        switch( iWord )
        {
            case 1:
                eSource = GC;
                break;
            case 2:
                eSource = WI;
                break;
            case 3:
                eSource = SO;
                break;
            case 4:
                eSource = SV;
                break;
            case 5:
                eSource = MO;
                break;
            default:
                eSource = UNKNOWN_STATION;
                break;
        }
    }
    else
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Set fetched information as metadata records                     */
/* -------------------------------------------------------------------- */
    const char *pszText;

    SetMetadataItem( "DATASET_NAME",  szDatasetName );

    switch( eSpacecraftID )
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
        case NOAA9_UNKNOWN:
            pszText = "UNKNOWN";
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
        case NOAA19:
            pszText = "NOAA-19(N')";
            break;
        case METOP2:
            pszText = "METOP-A(2)";
            break;
        case METOP1:
            pszText = "METOP-B(1)";
            break;
        case METOP3:
            pszText = "METOP-C(3)";
            break;
        default:
            pszText = "Unknown";
            break;
    }
    SetMetadataItem( "SATELLITE",  pszText );

    switch( eProductType )
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
            break;
    }
    SetMetadataItem( "DATA_TYPE",  pszText );

    // Get revolution number as string, we don't need this value for processing
    char    szRevolution[6];
    memcpy( szRevolution, szDatasetName + 32, 5 );
    szRevolution[5] = '\0';
    SetMetadataItem( "REVOLUTION",  szRevolution );

    switch( eSource )
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
            break;
    }
    SetMetadataItem( "SOURCE",  pszText );

    switch( eProcCenter )
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
            break;
    }
    SetMetadataItem( "PROCESSING_CENTER",  pszText );
    
    return CE_None;
}

/************************************************************************/
/*                        ComputeFileOffsets()                          */
/************************************************************************/

int L1BDataset::ComputeFileOffsets()
{
    CPLDebug("L1B", "Data format = %s",
             (iDataFormat == PACKED10BIT) ? "Packed 10 bit" :
             (iDataFormat == UNPACKED16BIT) ? "Unpacked 16 bit" :
                                              "Unpacked 8 bit");

    switch( eProductType )
    {
        case HRPT:
        case LAC:
        case FRAC:
            nRasterXSize = 2048;
            nBufferSize = 20484;
            iGCPStart = 25 - 1; /* http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/klm/html/c2/sec2-4.htm */
            iGCPStep = 40;
            nGCPsPerLine = 51;
            if ( eL1BFormat == L1B_NOAA9 )
            {
                if (iDataFormat == PACKED10BIT)
                {
                    nRecordSize = 14800;
                    nRecordDataEnd = 14104;
                }
                else if (iDataFormat == UNPACKED16BIT)
                {
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 4544;
                        nRecordDataEnd = 4544;
                        break;
                        case 2:
                        nRecordSize = 8640;
                        nRecordDataEnd = 8640;
                        break;
                        case 3:
                        nRecordSize = 12736;
                        nRecordDataEnd = 12736;
                        break;
                        case 4:
                        nRecordSize = 16832;
                        nRecordDataEnd = 16832;
                        break;
                        case 5:
                        nRecordSize = 20928;
                        nRecordDataEnd = 20928;
                        break;
                    }
                }
                else // UNPACKED8BIT
                {
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 2496;
                        nRecordDataEnd = 2496;
                        break;
                        case 2:
                        nRecordSize = 4544;
                        nRecordDataEnd = 4544;
                        break;
                        case 3:
                        nRecordSize = 6592;
                        nRecordDataEnd = 6592;
                        break;
                        case 4:
                        nRecordSize = 8640;
                        nRecordDataEnd = 8640;
                        break;
                        case 5:
                        nRecordSize = 10688;
                        nRecordDataEnd = 10688;
                        break;
                    }
                }
                nDataStartOffset = nRecordSize + L1B_NOAA9_HEADER_SIZE;
                nRecordDataStart = 448;
                iGCPCodeOffset = 52;
                iGCPOffset = 104;
            }

            else if ( eL1BFormat == L1B_NOAA15
                      || eL1BFormat == L1B_NOAA15_NOHDR )
            {
                if (iDataFormat == PACKED10BIT)
                {
                    nRecordSize = 15872;
                    nRecordDataEnd = 14920;
                    iCLAVRStart = 14984;
                }
                else if (iDataFormat == UNPACKED16BIT)
                { /* Table 8.3.1.3.3.1-3 */
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 6144;
                        nRecordDataEnd = 5360;
                        iCLAVRStart = 5368 + 56; /* guessed but not verified */
                        break;
                        case 2:
                        nRecordSize = 10240;
                        nRecordDataEnd = 9456;
                        iCLAVRStart = 9464 + 56; /* guessed but not verified */
                        break;
                        case 3:
                        nRecordSize = 14336;
                        nRecordDataEnd = 13552;
                        iCLAVRStart = 13560 + 56; /* guessed but not verified */
                        break;
                        case 4:
                        nRecordSize = 18432;
                        nRecordDataEnd = 17648;
                        iCLAVRStart = 17656 + 56; /* guessed but not verified */
                        break;
                        case 5:
                        nRecordSize = 22528;
                        nRecordDataEnd = 21744;
                        iCLAVRStart = 21752 + 56;
                        break;
                    }
                }
                else // UNPACKED8BIT
                { /* Table 8.3.1.3.3.1-2 */
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 4096;
                        nRecordDataEnd = 3312;
                        iCLAVRStart = 3320 + 56; /* guessed but not verified */
                        break;
                        case 2:
                        nRecordSize = 6144;
                        nRecordDataEnd = 5360;
                        iCLAVRStart = 5368 + 56; /* guessed but not verified */
                        break;
                        case 3:
                        nRecordSize = 8192;
                        nRecordDataEnd = 7408;
                        iCLAVRStart = 7416 + 56; /* guessed but not verified */
                        break;
                        case 4:
                        nRecordSize = 10240;
                        nRecordDataEnd = 9456;
                        iCLAVRStart = 9464 + 56; /* guessed but not verified */
                        break;
                        case 5:
                        nRecordSize = 12288;
                        nRecordDataEnd = 11504;
                        iCLAVRStart = 11512 + 56; /* guessed but not verified */
                        break;
                    }
                }
                nDataStartOffset = ( eL1BFormat == L1B_NOAA15_NOHDR ) ?
                    nRecordDataEnd : nRecordSize + L1B_NOAA15_HEADER_SIZE;
                nRecordDataStart = 1264;
                iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                iGCPOffset = 640;
            }
            else
                return 0;
            break;

        case GAC:
            nRasterXSize = 409;
            nBufferSize = 4092;
            iGCPStart = 5 - 1; // FIXME: depends of scan direction
            iGCPStep = 8;
            nGCPsPerLine = 51;
            if (  eL1BFormat == L1B_NOAA9 )
            {
                if (iDataFormat == PACKED10BIT)
                {
                    nRecordSize = 3220;
                    nRecordDataEnd = 3176;
                }
                else if (iDataFormat == UNPACKED16BIT)
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 1268;
                        nRecordDataEnd = 1266;
                        break;
                        case 2:
                        nRecordSize = 2084;
                        nRecordDataEnd = 2084;
                        break;
                        case 3:
                        nRecordSize = 2904;
                        nRecordDataEnd = 2902;
                        break;
                        case 4:
                        nRecordSize = 3720;
                        nRecordDataEnd = 3720;
                        break;
                        case 5:
                        nRecordSize = 4540;
                        nRecordDataEnd = 4538;
                        break;
                    }
                else // UNPACKED8BIT
                {
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 860;
                        nRecordDataEnd = 858;
                        break;
                        case 2:
                        nRecordSize = 1268;
                        nRecordDataEnd = 1266;
                        break;
                        case 3:
                        nRecordSize = 1676;
                        nRecordDataEnd = 1676;
                        break;
                        case 4:
                        nRecordSize = 2084;
                        nRecordDataEnd = 2084;
                        break;
                        case 5:
                        nRecordSize = 2496;
                        nRecordDataEnd = 2494;
                        break;
                    }
                }
                nDataStartOffset = nRecordSize * 2 + L1B_NOAA9_HEADER_SIZE;
                nRecordDataStart = 448;
                iGCPCodeOffset = 52;
                iGCPOffset = 104;
            }

            else if ( eL1BFormat == L1B_NOAA15
                      || eL1BFormat == L1B_NOAA15_NOHDR )
            {
                if (iDataFormat == PACKED10BIT)
                {
                    nRecordSize = 4608;
                    nRecordDataEnd = 3992;
                    iCLAVRStart = 4056;
                }
                else if (iDataFormat == UNPACKED16BIT)
                { /* Table 8.3.1.4.3.1-3 */
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 2360;
                        nRecordDataEnd = 2082;
                        iCLAVRStart = 2088 + 56; /* guessed but not verified */
                        break;
                        case 2:
                        nRecordSize = 3176;
                        nRecordDataEnd = 2900;
                        iCLAVRStart = 2904 + 56; /* guessed but not verified */
                        break;
                        case 3:
                        nRecordSize = 3992;
                        nRecordDataEnd = 3718;
                        iCLAVRStart = 3720 + 56; /* guessed but not verified */
                        break;
                        case 4:
                        nRecordSize = 4816;
                        nRecordDataEnd = 4536;
                        iCLAVRStart = 4544 + 56; /* guessed but not verified */
                        break;
                        case 5:
                        nRecordSize = 5632;
                        nRecordDataEnd = 5354;
                        iCLAVRStart = 5360 + 56;
                        break;
                    }
                }
                else // UNPACKED8BIT
                { /* Table 8.3.1.4.3.1-2 but record length is wrong in the table ! */
                    switch(nBands)
                    {
                        case 1:
                        nRecordSize = 1952;
                        nRecordDataEnd = 1673;
                        iCLAVRStart = 1680 + 56; /* guessed but not verified */
                        break;
                        case 2:
                        nRecordSize = 2360;
                        nRecordDataEnd = 2082;
                        iCLAVRStart = 2088 + 56; /* guessed but not verified */
                        break;
                        case 3:
                        nRecordSize = 2768;
                        nRecordDataEnd = 2491;
                        iCLAVRStart = 2496 + 56; /* guessed but not verified */
                        break;
                        case 4:
                        nRecordSize = 3176;
                        nRecordDataEnd = 2900;
                        iCLAVRStart = 2904 + 56; /* guessed but not verified */
                        break;
                        case 5:
                        nRecordSize = 3584;
                        nRecordDataEnd = 3309;
                        iCLAVRStart = 3312 + 56; /* guessed but not verified */
                        break;
                    }
                }
                nDataStartOffset = ( eL1BFormat == L1B_NOAA15_NOHDR ) ?
                    nRecordDataEnd : nRecordSize + L1B_NOAA15_HEADER_SIZE;
                nRecordDataStart = 1264;
                iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                iGCPOffset = 640;
            }
            else
                return 0;
        break;
        default:
            return 0;
    }

    return 1;
}

/************************************************************************/
/*                       L1BGeolocDataset                               */
/************************************************************************/

class L1BGeolocDataset : public GDALDataset
{
    friend class L1BGeolocRasterBand;

    L1BDataset* poL1BDS;
    int bInterpolGeolocationDS;

    public:
                L1BGeolocDataset(L1BDataset* poMainDS,
                                 int bInterpolGeolocationDS);
       virtual ~L1BGeolocDataset();

       static GDALDataset* CreateGeolocationDS(L1BDataset* poL1BDS,
                                               int bInterpolGeolocationDS);
};

/************************************************************************/
/*                       L1BGeolocRasterBand                            */
/************************************************************************/

class L1BGeolocRasterBand: public GDALRasterBand
{
    public:
            L1BGeolocRasterBand(L1BGeolocDataset* poDS, int nBand);

            virtual CPLErr IReadBlock(int, int, void*);
            virtual double GetNoDataValue( int *pbSuccess = NULL );
};

/************************************************************************/
/*                        L1BGeolocDataset()                            */
/************************************************************************/

L1BGeolocDataset::L1BGeolocDataset(L1BDataset* poL1BDS,
                                   int bInterpolGeolocationDS)
{
    this->poL1BDS = poL1BDS;
    this->bInterpolGeolocationDS = bInterpolGeolocationDS;
    if( bInterpolGeolocationDS )
        nRasterXSize = poL1BDS->nRasterXSize;
    else
        nRasterXSize = poL1BDS->nGCPsPerLine;
    nRasterYSize = poL1BDS->nRasterYSize;
}

/************************************************************************/
/*                       ~L1BGeolocDataset()                            */
/************************************************************************/

L1BGeolocDataset::~L1BGeolocDataset()
{
    delete poL1BDS;
}

/************************************************************************/
/*                        L1BGeolocRasterBand()                         */
/************************************************************************/

L1BGeolocRasterBand::L1BGeolocRasterBand(L1BGeolocDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = GDT_Float64;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
    if( nBand == 1 )
        SetDescription("GEOLOC X");
    else
        SetDescription("GEOLOC Y");
}

/************************************************************************/
/*                         LagrangeInterpol()                           */
/************************************************************************/

/* ----------------------------------------------------------------------------
 * Perform a Lagrangian interpolation through the given x,y coordinates
 * and return the interpolated y value for the given x value.
 * The array size and thus the polynomial order is defined by numpt.
 * Input: x[] and y[] are of size numpt,
 *  x0 is the x value for which we calculate the corresponding y
 * Returns: y value calculated for given x0.
 */
static double LagrangeInterpol(const double x[],
                               const double y[], double x0, int numpt)
{
    int i, j;
    double L;
    double y0 = 0;

    for (i=0; i<numpt; i++)
    {
        L = 1.0;
        for (j=0; j<numpt; j++)
        {
            if (i == j)
                continue;
            L = L * (x0 - x[j]) / (x[i] - x[j]);
        }
        y0 = y0 + L * y[i];
    }
    return(y0);
}

/************************************************************************/
/*                         L1BInterpol()                                */
/************************************************************************/

/* ----------------------------------------------------------------------------
 * Interpolate an array of size numPoints where the only values set on input are
 * at knownFirst, and intervals of knownStep thereafter.
 * On return all the rest from 0..numPoints-1 will be filled in.
 * Uses the LagrangeInterpol() function to do the interpolation; 5-point for the
 * beginning and end of the array and 4-point for the rest.
 * To use this function for NOAA level 1B data extract the 51 latitude values
 * into their appropriate places in the vals array then call L1BInterpol to
 * calculate the rest of the values.  Do similarly for longitudes, solar zenith
 * angles, and any others which are present in the file.
 * Reference:
 *  http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/klm/html/c2/sec2-4.htm
 */

#define MIDDLE_INTERP_ORDER 4
#define END_INTERP_ORDER  5  /* Ensure this is an odd number, 5 is suitable.*/

/* Convert number of known point to its index in full array */
#define IDX(N) ((N)*knownStep+knownFirst)

static void
L1BInterpol(double vals[],
            int numKnown,   /* Number of known points (typically 51) */
            int knownFirst, /* Index in full array of first known point (24) */
            int knownStep,  /* Interval to next and subsequent known points (40) */
            int numPoints   /* Number of points in whole array (2048) */ )
{
    int i, j;
    double x[END_INTERP_ORDER];
    double y[END_INTERP_ORDER];

    /* First extrapolate first 24 points */
    for (i=0; i<END_INTERP_ORDER; i++)
    {
        x[i] = IDX(i);
        y[i] = vals[IDX(i)];
    }
    for (i=0; i<knownFirst; i++)
    {
        vals[i] = LagrangeInterpol(x, y, i, END_INTERP_ORDER);
    }

    /* Next extrapolate last 23 points */
    for (i=0; i<END_INTERP_ORDER; i++)
    {
        x[i] = IDX(numKnown-END_INTERP_ORDER+i);
        y[i] = vals[IDX(numKnown-END_INTERP_ORDER+i)];
    }
    for (i=IDX(numKnown-1); i<numPoints; i++)
    {
        vals[i] = LagrangeInterpol(x, y, i, END_INTERP_ORDER);
    }

    /* Interpolate all intermediate points using two before and two after */
    for (i=knownFirst; i<IDX(numKnown-1); i++)
    {
        double x[MIDDLE_INTERP_ORDER];
        double y[MIDDLE_INTERP_ORDER];
        int startpt;

        /* Find a suitable set of two known points before and two after */
        startpt = (i/knownStep)-MIDDLE_INTERP_ORDER/2;
        if (startpt<0)
            startpt=0;
        if (startpt+MIDDLE_INTERP_ORDER-1 >= numKnown)
            startpt = numKnown-MIDDLE_INTERP_ORDER;
        for (j=0; j<MIDDLE_INTERP_ORDER; j++)
        {
            x[j] = IDX(startpt+j);
            y[j] = vals[IDX(startpt+j)];
        }
        vals[i] = LagrangeInterpol(x, y, i, MIDDLE_INTERP_ORDER);
    }
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr L1BGeolocRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                       int nBlockYOff,
                                       void* pData)
{
    L1BGeolocDataset* poGDS = (L1BGeolocDataset*)poDS;
    L1BDataset* poL1BDS = poGDS->poL1BDS;
    GDAL_GCP* pasGCPList = (GDAL_GCP *)CPLCalloc( poL1BDS->nGCPsPerLine,
                                        sizeof(GDAL_GCP) );
    GDALInitGCPs( poL1BDS->nGCPsPerLine, pasGCPList );


    GByte* pabyRecordHeader = (GByte*)CPLMalloc(poL1BDS->nRecordSize);

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poL1BDS->fp, poL1BDS->GetLineOffset(nBlockYOff), SEEK_SET );

    VSIFReadL( pabyRecordHeader, 1, poL1BDS->nRecordDataStart, poL1BDS->fp );

    /* Fetch the GCPs for the row */
    int nGotGCPs = poL1BDS->FetchGCPs(pasGCPList, pabyRecordHeader, nBlockYOff );
    double* padfData = (double*)pData;
    int i;
    if( poGDS->bInterpolGeolocationDS )
    {
        /* Fill the known position */
        for(i=0;i<nGotGCPs;i++)
        {
            double dfVal = (nBand == 1) ? pasGCPList[i].dfGCPX : pasGCPList[i].dfGCPY;
            padfData[poL1BDS->iGCPStart + i * poL1BDS->iGCPStep] = dfVal;
        }

        if( nGotGCPs == poL1BDS->nGCPsPerLine )
        {
            /* And do Lagangian interpolation to fill the holes */
            L1BInterpol(padfData, poL1BDS->nGCPsPerLine,
                        poL1BDS->iGCPStart, poL1BDS->iGCPStep, nRasterXSize);
        }
        else
        {
            int iFirstNonValid = 0;
            if( nGotGCPs > 5 )
                iFirstNonValid = poL1BDS->iGCPStart + nGotGCPs * poL1BDS->iGCPStep + poL1BDS->iGCPStep / 2;
            for(i=iFirstNonValid; i<nRasterXSize; i++)
            {
                padfData[i] = GetNoDataValue(NULL);
            }
            if( iFirstNonValid > 0 )
            {
                L1BInterpol(padfData, poL1BDS->nGCPsPerLine,
                            poL1BDS->iGCPStart, poL1BDS->iGCPStep, iFirstNonValid);
            }
        }
    }
    else
    {
        for(i=0;i<nGotGCPs;i++)
        {
            padfData[i] = (nBand == 1) ? pasGCPList[i].dfGCPX : pasGCPList[i].dfGCPY;
        }
        for(i=nGotGCPs;i<nRasterXSize;i++)
            padfData[i] = GetNoDataValue(NULL);
    }

    if( poL1BDS->eLocationIndicator == ASCEND )
    {
        for(i=0;i<nRasterXSize/2;i++)
        {
            double dfTmp = padfData[i];
            padfData[i] = padfData[nRasterXSize-1-i];
            padfData[nRasterXSize-1-i] = dfTmp;
        }
    }

    CPLFree(pabyRecordHeader);
    GDALDeinitGCPs( poL1BDS->nGCPsPerLine, pasGCPList );
    CPLFree(pasGCPList);
    
    return CE_None;
}

/************************************************************************/
/*                        GetNoDataValue()                              */
/************************************************************************/

double L1BGeolocRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;
    return -200.0;
}

/************************************************************************/
/*                      CreateGeolocationDS()                           */
/************************************************************************/

GDALDataset* L1BGeolocDataset::CreateGeolocationDS(L1BDataset* poL1BDS,
                                             int bInterpolGeolocationDS)
{
    L1BGeolocDataset* poGeolocDS = new L1BGeolocDataset(poL1BDS, bInterpolGeolocationDS);
    for(int i=1;i<=2;i++)
    {
        poGeolocDS->SetBand(i, new L1BGeolocRasterBand(poGeolocDS, i));
    }
    return poGeolocDS;
}

/************************************************************************/
/*                    L1BSolarZenithAnglesDataset                       */
/************************************************************************/

class L1BSolarZenithAnglesDataset : public GDALDataset
{
    friend class L1BSolarZenithAnglesRasterBand;

    L1BDataset* poL1BDS;

    public:
                L1BSolarZenithAnglesDataset(L1BDataset* poMainDS);
       virtual ~L1BSolarZenithAnglesDataset();

       static GDALDataset* CreateSolarZenithAnglesDS(L1BDataset* poL1BDS);
};

/************************************************************************/
/*                  L1BSolarZenithAnglesRasterBand                      */
/************************************************************************/

class L1BSolarZenithAnglesRasterBand: public GDALRasterBand
{
    public:
            L1BSolarZenithAnglesRasterBand(L1BSolarZenithAnglesDataset* poDS, int nBand);

            virtual CPLErr IReadBlock(int, int, void*);
            virtual double GetNoDataValue( int *pbSuccess = NULL );
};

/************************************************************************/
/*                  L1BSolarZenithAnglesDataset()                       */
/************************************************************************/

L1BSolarZenithAnglesDataset::L1BSolarZenithAnglesDataset(L1BDataset* poL1BDS)
{
    this->poL1BDS = poL1BDS;
    nRasterXSize = 51;
    nRasterYSize = poL1BDS->nRasterYSize;
}

/************************************************************************/
/*                  ~L1BSolarZenithAnglesDataset()                      */
/************************************************************************/

L1BSolarZenithAnglesDataset::~L1BSolarZenithAnglesDataset()
{
    delete poL1BDS;
}

/************************************************************************/
/*                  L1BSolarZenithAnglesRasterBand()                    */
/************************************************************************/

L1BSolarZenithAnglesRasterBand::L1BSolarZenithAnglesRasterBand(L1BSolarZenithAnglesDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = GDT_Float32;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr L1BSolarZenithAnglesRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                                  int nBlockYOff,
                                                  void* pData)
{
    L1BSolarZenithAnglesDataset* poGDS = (L1BSolarZenithAnglesDataset*)poDS;
    L1BDataset* poL1BDS = poGDS->poL1BDS;
    int i;

    GByte* pabyRecordHeader = (GByte*)CPLMalloc(poL1BDS->nRecordSize);

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poL1BDS->fp, poL1BDS->GetLineOffset(nBlockYOff), SEEK_SET );

    VSIFReadL( pabyRecordHeader, 1, poL1BDS->nRecordSize, poL1BDS->fp );

    int nValidValues = MIN(nRasterXSize, pabyRecordHeader[poL1BDS->iGCPCodeOffset]);
    float* pafData = (float*)pData;

    int bHasFractional = ( poL1BDS->nRecordDataEnd + 20 <= poL1BDS->nRecordSize );

#ifdef notdef
    if( bHasFractional )
    {
        for(i=0;i<20;i++)
        {
            GByte val = pabyRecordHeader[poL1BDS->nRecordDataEnd + i];
            for(int j=0;j<8;j++)
                fprintf(stderr, "%c", ((val >> (7 -j)) & 1) ? '1' : '0');
            fprintf(stderr, " ");
        }
        fprintf(stderr, "\n");
    }
#endif

    for(i=0;i<nValidValues;i++)
    {
        pafData[i] = pabyRecordHeader[poL1BDS->iGCPCodeOffset + 1 + i] / 2.0;

        if( bHasFractional )
        {
            /* Cf http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/podug/html/l/app-l.htm#notl-2 */
            /* This is not very clear on if bits must be counted from MSB or LSB */
            /* but when testing on n12gac10bit.l1b, it appears that the 3 bits for i=0 are the 3 MSB bits */
            int nAddBitStart = i * 3;
            int nFractional;

#if 1
            if( (nAddBitStart % 8) + 3 <= 8 )
            {
                nFractional = (pabyRecordHeader[poL1BDS->nRecordDataEnd + (nAddBitStart / 8)] >> (8 - ((nAddBitStart % 8)+3))) & 0x7;
            }
            else
            {
                nFractional = (((pabyRecordHeader[poL1BDS->nRecordDataEnd + (nAddBitStart / 8)] << 8) |
                                pabyRecordHeader[poL1BDS->nRecordDataEnd + (nAddBitStart / 8) + 1]) >> (16 - ((nAddBitStart % 8)+3))) & 0x7;
            }
#else
            nFractional = (pabyRecordHeader[poL1BDS->nRecordDataEnd + (nAddBitStart / 8)] >> (nAddBitStart % 8)) & 0x7;
            if( (nAddBitStart % 8) + 3 > 8 )
                nFractional |= (pabyRecordHeader[poL1BDS->nRecordDataEnd + (nAddBitStart / 8) + 1] & ((1 << (((nAddBitStart % 8) + 3 - 8))) - 1)) << (3 - ((((nAddBitStart % 8) + 3 - 8))));*/
#endif
            if( nFractional > 4 )
            {
                CPLDebug("L1B", "For nBlockYOff=%d, i=%d, wrong fractional value : %d",
                         nBlockYOff, i, nFractional);
            }

            pafData[i] += nFractional / 10.0;
        }
    }

    for(;i<nRasterXSize;i++)
        pafData[i] = GetNoDataValue(NULL);

    if( poL1BDS->eLocationIndicator == ASCEND )
    {
        for(i=0;i<nRasterXSize/2;i++)
        {
            double fTmp = pafData[i];
            pafData[i] = pafData[nRasterXSize-1-i];
            pafData[nRasterXSize-1-i] = fTmp;
        }
    }

    CPLFree(pabyRecordHeader);

    return CE_None;
}

/************************************************************************/
/*                        GetNoDataValue()                              */
/************************************************************************/

double L1BSolarZenithAnglesRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;
    return -200.0;
}

/************************************************************************/
/*                      CreateSolarZenithAnglesDS()                     */
/************************************************************************/

GDALDataset* L1BSolarZenithAnglesDataset::CreateSolarZenithAnglesDS(L1BDataset* poL1BDS)
{
    L1BSolarZenithAnglesDataset* poGeolocDS = new L1BSolarZenithAnglesDataset(poL1BDS);
    for(int i=1;i<=1;i++)
    {
        poGeolocDS->SetBand(i, new L1BSolarZenithAnglesRasterBand(poGeolocDS, i));
    }
    return poGeolocDS;
}


/************************************************************************/
/*                     L1BNOAA15AnglesDataset                           */
/************************************************************************/

class L1BNOAA15AnglesDataset : public GDALDataset
{
    friend class L1BNOAA15AnglesRasterBand;

    L1BDataset* poL1BDS;

    public:
                L1BNOAA15AnglesDataset(L1BDataset* poMainDS);
       virtual ~L1BNOAA15AnglesDataset();

       static GDALDataset* CreateAnglesDS(L1BDataset* poL1BDS);
};

/************************************************************************/
/*                     L1BNOAA15AnglesRasterBand                        */
/************************************************************************/

class L1BNOAA15AnglesRasterBand: public GDALRasterBand
{
    public:
            L1BNOAA15AnglesRasterBand(L1BNOAA15AnglesDataset* poDS, int nBand);

            virtual CPLErr IReadBlock(int, int, void*);
};

/************************************************************************/
/*                       L1BNOAA15AnglesDataset()                       */
/************************************************************************/

L1BNOAA15AnglesDataset::L1BNOAA15AnglesDataset(L1BDataset* poL1BDS)
{
    this->poL1BDS = poL1BDS;
    nRasterXSize = 51;
    nRasterYSize = poL1BDS->nRasterYSize;
}

/************************************************************************/
/*                     ~L1BNOAA15AnglesDataset()                        */
/************************************************************************/

L1BNOAA15AnglesDataset::~L1BNOAA15AnglesDataset()
{
    delete poL1BDS;
}

/************************************************************************/
/*                      L1BNOAA15AnglesRasterBand()                     */
/************************************************************************/

L1BNOAA15AnglesRasterBand::L1BNOAA15AnglesRasterBand(L1BNOAA15AnglesDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = GDT_Float32;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
    if( nBand == 1 )
        SetDescription("Solar zenith angles");
    else if( nBand == 2 )
        SetDescription("Satellite zenith angles");
    else
        SetDescription("Relative azimuth angles");
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr L1BNOAA15AnglesRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                             int nBlockYOff,
                                             void* pData)
{
    L1BNOAA15AnglesDataset* poGDS = (L1BNOAA15AnglesDataset*)poDS;
    L1BDataset* poL1BDS = poGDS->poL1BDS;
    int i;

    GByte* pabyRecordHeader = (GByte*)CPLMalloc(poL1BDS->nRecordSize);

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poL1BDS->fp, poL1BDS->GetLineOffset(nBlockYOff), SEEK_SET );

    VSIFReadL( pabyRecordHeader, 1, poL1BDS->nRecordSize, poL1BDS->fp );

    float* pafData = (float*)pData;

    for(i=0;i<nRasterXSize;i++)
    {
        GInt16 i16 = poL1BDS->GetInt16(pabyRecordHeader + 328 + 6 * i + 2 * (nBand - 1));
        pafData[i] = i16 / 100.0;
    }

    if( poL1BDS->eLocationIndicator == ASCEND )
    {
        for(i=0;i<nRasterXSize/2;i++)
        {
            double fTmp = pafData[i];
            pafData[i] = pafData[nRasterXSize-1-i];
            pafData[nRasterXSize-1-i] = fTmp;
        }
    }

    CPLFree(pabyRecordHeader);

    return CE_None;
}

/************************************************************************/
/*                            CreateAnglesDS()                          */
/************************************************************************/

GDALDataset* L1BNOAA15AnglesDataset::CreateAnglesDS(L1BDataset* poL1BDS)
{
    L1BNOAA15AnglesDataset* poGeolocDS = new L1BNOAA15AnglesDataset(poL1BDS);
    for(int i=1;i<=3;i++)
    {
        poGeolocDS->SetBand(i, new L1BNOAA15AnglesRasterBand(poGeolocDS, i));
    }
    return poGeolocDS;
}

/************************************************************************/
/*                          L1BCloudsDataset                            */
/************************************************************************/

class L1BCloudsDataset : public GDALDataset
{
    friend class L1BCloudsRasterBand;

    L1BDataset* poL1BDS;

    public:
                L1BCloudsDataset(L1BDataset* poMainDS);
       virtual ~L1BCloudsDataset();

       static GDALDataset* CreateCloudsDS(L1BDataset* poL1BDS);
};

/************************************************************************/
/*                        L1BCloudsRasterBand                           */
/************************************************************************/

class L1BCloudsRasterBand: public GDALRasterBand
{
    public:
            L1BCloudsRasterBand(L1BCloudsDataset* poDS, int nBand);

            virtual CPLErr IReadBlock(int, int, void*);
};

/************************************************************************/
/*                         L1BCloudsDataset()                           */
/************************************************************************/

L1BCloudsDataset::L1BCloudsDataset(L1BDataset* poL1BDS)
{
    this->poL1BDS = poL1BDS;
    nRasterXSize = poL1BDS->nRasterXSize;
    nRasterYSize = poL1BDS->nRasterYSize;
}

/************************************************************************/
/*                        ~L1BCloudsDataset()                           */
/************************************************************************/

L1BCloudsDataset::~L1BCloudsDataset()
{
    delete poL1BDS;
}

/************************************************************************/
/*                         L1BCloudsRasterBand()                        */
/************************************************************************/

L1BCloudsRasterBand::L1BCloudsRasterBand(L1BCloudsDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = GDT_Byte;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr L1BCloudsRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                       int nBlockYOff,
                                       void* pData)
{
    L1BCloudsDataset* poGDS = (L1BCloudsDataset*)poDS;
    L1BDataset* poL1BDS = poGDS->poL1BDS;
    int i;

    GByte* pabyRecordHeader = (GByte*)CPLMalloc(poL1BDS->nRecordSize);

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poL1BDS->fp, poL1BDS->GetLineOffset(nBlockYOff), SEEK_SET );

    VSIFReadL( pabyRecordHeader, 1, poL1BDS->nRecordSize, poL1BDS->fp );

    GByte* pabyData = (GByte*)pData;

    for(i=0;i<nRasterXSize;i++)
    {
        pabyData[i] = ((pabyRecordHeader[poL1BDS->iCLAVRStart + (i / 4)] >> (8 - ((i%4)*2+2))) & 0x3);
    }

    if( poL1BDS->eLocationIndicator == ASCEND )
    {
        for(i=0;i<nRasterXSize/2;i++)
        {
            GByte byTmp = pabyData[i];
            pabyData[i] = pabyData[nRasterXSize-1-i];
            pabyData[nRasterXSize-1-i] = byTmp;
        }
    }

    CPLFree(pabyRecordHeader);

    return CE_None;
}

/************************************************************************/
/*                      CreateCloudsDS()                     */
/************************************************************************/

GDALDataset* L1BCloudsDataset::CreateCloudsDS(L1BDataset* poL1BDS)
{
    L1BCloudsDataset* poGeolocDS = new L1BCloudsDataset(poL1BDS);
    for(int i=1;i<=1;i++)
    {
        poGeolocDS->SetBand(i, new L1BCloudsRasterBand(poGeolocDS, i));
    }
    return poGeolocDS;
}


/************************************************************************/
/*                           DetectFormat()                             */
/************************************************************************/

L1BFileFormat L1BDataset::DetectFormat( const char* pszFilename,
                              const GByte* pabyHeader, int nHeaderBytes )

{
    if (pabyHeader == NULL || nHeaderBytes < L1B_NOAA9_HEADER_SIZE)
        return L1B_NONE;

    // We will try the NOAA-15 and later formats first
    if ( nHeaderBytes > L1B_NOAA15_HEADER_SIZE + 61
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 25) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 30) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 33) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 40) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 46) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 52) == '.'
         && *(pabyHeader + L1B_NOAA15_HEADER_SIZE + 61) == '.' )
        return L1B_NOAA15;

    // Next try the NOAA-9/14 formats
    if ( *(pabyHeader + 8 + 25) == '.'
         && *(pabyHeader + 8 + 30) == '.'
         && *(pabyHeader + 8 + 33) == '.'
         && *(pabyHeader + 8 + 40) == '.'
         && *(pabyHeader + 8 + 46) == '.'
         && *(pabyHeader + 8 + 52) == '.'
         && *(pabyHeader + 8 + 61) == '.' )
        return L1B_NOAA9;

    // Next try the NOAA-9/14 formats with dataset name in EBCDIC
    if ( *(pabyHeader + 8 + 25) == 'K'
         && *(pabyHeader + 8 + 30) == 'K'
         && *(pabyHeader + 8 + 33) == 'K'
         && *(pabyHeader + 8 + 40) == 'K'
         && *(pabyHeader + 8 + 46) == 'K'
         && *(pabyHeader + 8 + 52) == 'K'
         && *(pabyHeader + 8 + 61) == 'K' )
        return L1B_NOAA9;

    // Finally try the AAPP formats 
    if ( *(pabyHeader + 25) == '.'
         && *(pabyHeader + 30) == '.'
         && *(pabyHeader + 33) == '.'
         && *(pabyHeader + 40) == '.'
         && *(pabyHeader + 46) == '.'
         && *(pabyHeader + 52) == '.'
         && *(pabyHeader + 61) == '.' )
        return L1B_NOAA15_NOHDR;

    // A few NOAA <= 9 datasets with no dataset name in TBM header
    if( strlen(pszFilename) == L1B_DATASET_NAME_SIZE &&
        pszFilename[3] == '.' &&
        pszFilename[8] == '.' &&
        pszFilename[11] == '.' &&
        pszFilename[18] == '.' &&
        pszFilename[24] == '.' &&
        pszFilename[30] == '.' &&
        pszFilename[39] == '.' &&
        memcmp(pabyHeader + 30, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", L1B_DATASET_NAME_SIZE) == 0 &&
        (pabyHeader[75] == '+' || pabyHeader[75] == '-') &&
        (pabyHeader[78] == '+' || pabyHeader[78] == '-') &&
        (pabyHeader[81] == '+' || pabyHeader[81] == '-') &&
        (pabyHeader[85] == '+' || pabyHeader[85] == '-') )
        return L1B_NOAA9;

    return L1B_NONE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int L1BDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if ( EQUALN( poOpenInfo->pszFilename, "L1BGCPS:", strlen("L1BGCPS:") ) )
        return TRUE;
    if ( EQUALN( poOpenInfo->pszFilename, "L1BGCPS_INTERPOL:", strlen("L1BGCPS_INTERPOL:") ) )
        return TRUE;
    if ( EQUALN( poOpenInfo->pszFilename, "L1B_SOLAR_ZENITH_ANGLES:", strlen("L1B_SOLAR_ZENITH_ANGLES:") ) )
        return TRUE;
    if ( EQUALN( poOpenInfo->pszFilename, "L1B_ANGLES:", strlen("L1B_ANGLES:") ) )
        return TRUE;
    if ( EQUALN( poOpenInfo->pszFilename, "L1B_CLOUDS:", strlen("L1B_CLOUDS:") ) )
        return TRUE;

    if ( DetectFormat(CPLGetFilename(poOpenInfo->pszFilename),
                      poOpenInfo->pabyHeader,
                      poOpenInfo->nHeaderBytes) == L1B_NONE )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *L1BDataset::Open( GDALOpenInfo * poOpenInfo )

{
    GDALDataset* poOutDS;
    VSILFILE* fp = NULL;
    CPLString osFilename = poOpenInfo->pszFilename;
    int bAskGeolocationDS = FALSE;
    int bInterpolGeolocationDS = FALSE;
    int bAskSolarZenithAnglesDS = FALSE;
    int bAskAnglesDS = FALSE;
    int bAskCloudsDS = FALSE;
    L1BFileFormat eL1BFormat;

    if ( EQUALN( poOpenInfo->pszFilename, "L1BGCPS:", strlen("L1BGCPS:") ) ||
         EQUALN( poOpenInfo->pszFilename, "L1BGCPS_INTERPOL:", strlen("L1BGCPS_INTERPOL:") ) ||
         EQUALN( poOpenInfo->pszFilename, "L1B_SOLAR_ZENITH_ANGLES:", strlen("L1B_SOLAR_ZENITH_ANGLES:") )||
         EQUALN( poOpenInfo->pszFilename, "L1B_ANGLES:", strlen("L1B_ANGLES:") )||
         EQUALN( poOpenInfo->pszFilename, "L1B_CLOUDS:", strlen("L1B_CLOUDS:") ) )
    {
        GByte abyHeader[1024];
        const char* pszFilename;
        if( EQUALN( poOpenInfo->pszFilename, "L1BGCPS_INTERPOL:", strlen("L1BGCPS_INTERPOL:")) )
        {
            bAskGeolocationDS = TRUE;
            bInterpolGeolocationDS = TRUE;
            pszFilename = poOpenInfo->pszFilename + strlen("L1BGCPS_INTERPOL:");
        }
        else if (EQUALN( poOpenInfo->pszFilename, "L1BGCPS:", strlen("L1BGCPS:") ) )
        {
            bAskGeolocationDS = TRUE;
            pszFilename = poOpenInfo->pszFilename + strlen("L1BGCPS:");
        }
        else if (EQUALN( poOpenInfo->pszFilename, "L1B_SOLAR_ZENITH_ANGLES:", strlen("L1B_SOLAR_ZENITH_ANGLES:") ) )
        {
            bAskSolarZenithAnglesDS = TRUE;
            pszFilename = poOpenInfo->pszFilename + strlen("L1B_SOLAR_ZENITH_ANGLES:");
        }
        else if (EQUALN( poOpenInfo->pszFilename, "L1B_ANGLES:", strlen("L1B_ANGLES:") ) )
        {
            bAskAnglesDS = TRUE;
            pszFilename = poOpenInfo->pszFilename + strlen("L1B_ANGLES:");
        }
        else
        {
            bAskCloudsDS = TRUE;
            pszFilename = poOpenInfo->pszFilename + strlen("L1B_CLOUDS:");
        }
        if( pszFilename[0] == '"' )
            pszFilename ++;
        osFilename = pszFilename;
        if( osFilename.size() > 0 && osFilename[osFilename.size()-1] == '"' )
            osFilename.resize(osFilename.size()-1);
        fp = VSIFOpenL( osFilename, "rb" );
        if ( !fp )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Can't open file \"%s\".", osFilename.c_str() );
            return NULL;
        }
        VSIFReadL( abyHeader, 1, sizeof(abyHeader)-1, fp);
        abyHeader[sizeof(abyHeader)-1] = '\0';
        eL1BFormat = DetectFormat( CPLGetFilename(osFilename),
                                   abyHeader, sizeof(abyHeader) );
    }
    else
        eL1BFormat = DetectFormat( CPLGetFilename(osFilename),
                                   poOpenInfo->pabyHeader,
                                   poOpenInfo->nHeaderBytes );

    if ( eL1BFormat == L1B_NONE )
    {
        if( fp != NULL )
            VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The L1B driver does not support update access to existing"
                  " datasets.\n" );
        if( fp != NULL )
            VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    L1BDataset  *poDS;
    VSIStatBufL  sStat;

    poDS = new L1BDataset( eL1BFormat );

    if( fp == NULL )
        fp = VSIFOpenL( osFilename, "rb" );
    poDS->fp = fp;
    if ( !poDS->fp )
    {
        CPLDebug( "L1B", "Can't open file \"%s\".", osFilename.c_str() );
        goto bad;
    }
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    if ( poDS->ProcessDatasetHeader(CPLGetFilename(osFilename)) != CE_None )
    {
        CPLDebug( "L1B", "Error reading L1B record header." );
        goto bad;
    }

    VSIStatL(osFilename, &sStat);

    if( poDS->eL1BFormat == L1B_NOAA15_NOHDR &&
        poDS->nRecordSizeFromHeader == 22016 &&
        (sStat.st_size % poDS->nRecordSizeFromHeader) == 0 )
    {
        poDS->iDataFormat = UNPACKED16BIT;
        poDS->ComputeFileOffsets();
        poDS->nDataStartOffset = poDS->nRecordSizeFromHeader;
        poDS->nRecordSize = poDS->nRecordSizeFromHeader;
        poDS->iCLAVRStart = 0;
    }
    else if ( poDS->bGuessDataFormat )
    {
        int nTempYSize;
        GUInt16 nScanlineNumber;
        int j;

        /* If the data format is unspecified, try each one of the 3 known data formats */
        /* It is considered valid when the spacing between the first 5 scanline numbers */
        /* is a constant */

        for(j=0;j<3;j++)
        {
            poDS->iDataFormat = (L1BDataFormat) (PACKED10BIT + j);
            if (!poDS->ComputeFileOffsets())
                goto bad;

            nTempYSize = (sStat.st_size - poDS->nDataStartOffset) / poDS->nRecordSize;
            if (nTempYSize < 5)
                continue;

            int nLastScanlineNumber = 0;
            int nDiffLine = 0;
            int i;
            for (i=0;i<5;i++)
            {
                nScanlineNumber = 0;

                VSIFSeekL(poDS->fp, poDS->nDataStartOffset + i * poDS->nRecordSize, SEEK_SET);
                VSIFReadL(&nScanlineNumber, 1, 2, poDS->fp);
                nScanlineNumber = poDS->GetUInt16(&nScanlineNumber);

                if (i == 1)
                {
                    nDiffLine = nScanlineNumber - nLastScanlineNumber;
                    if (nDiffLine == 0)
                        break;
                }
                else if (i > 1)
                {
                    if (nDiffLine != nScanlineNumber - nLastScanlineNumber)
                        break;
                }

                nLastScanlineNumber = nScanlineNumber;
            }

            if (i == 5)
            {
                CPLDebug("L1B", "Guessed data format : %s",
                         (poDS->iDataFormat == PACKED10BIT) ? "10" :
                         (poDS->iDataFormat == UNPACKED8BIT) ? "08" : "16");
                break;
            }
        }

        if (j == 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not guess data format of L1B product");
            goto bad;
        }
    }
    else
    {
        if (!poDS->ComputeFileOffsets())
            goto bad;
    }

    CPLDebug("L1B", "nRecordDataStart = %d", poDS->nRecordDataStart);
    CPLDebug("L1B", "nRecordDataEnd = %d", poDS->nRecordDataEnd);
    CPLDebug("L1B", "nDataStartOffset = %d", poDS->nDataStartOffset);
    CPLDebug("L1B", "iCLAVRStart = %d", poDS->iCLAVRStart);
    CPLDebug("L1B", "nRecordSize = %d", poDS->nRecordSize);

    // Compute number of lines dinamycally, so we can read partially
    // downloaded files
    poDS->nRasterYSize =
        (sStat.st_size - poDS->nDataStartOffset) / poDS->nRecordSize;

/* -------------------------------------------------------------------- */
/*      Deal with GCPs                                                  */
/* -------------------------------------------------------------------- */
    poDS->ProcessRecordHeaders();

    if( bAskGeolocationDS )
    {
        return L1BGeolocDataset::CreateGeolocationDS(poDS, bInterpolGeolocationDS);
    }
    else if( bAskSolarZenithAnglesDS )
    {
        if( eL1BFormat == L1B_NOAA9 )
            return L1BSolarZenithAnglesDataset::CreateSolarZenithAnglesDS(poDS);
        else
        {
            delete poDS;
            return NULL;
        }
    }
    else if( bAskAnglesDS )
    {
        if( eL1BFormat != L1B_NOAA9 )
            return L1BNOAA15AnglesDataset::CreateAnglesDS(poDS);
        else
        {
            delete poDS;
            return NULL;
        }
    }
    else if( bAskCloudsDS )
    {
        if( poDS->iCLAVRStart > 0 )
            poOutDS = L1BCloudsDataset::CreateCloudsDS(poDS);
        else
        {
            delete poDS;
            return NULL;
        }
    }
    else
    {
        poOutDS = poDS;
    }

    {
        CPLString  osTMP;
        int bInterpol = CSLTestBoolean(CPLGetConfigOption("L1B_INTERPOL_GCPS", "TRUE"));

        poOutDS->SetMetadataItem( "SRS", poDS->pszGCPProjection, "GEOLOCATION" ); /* unused by gdalgeoloc.cpp */

        if( bInterpol )
            osTMP.Printf( "L1BGCPS_INTERPOL:\"%s\"", osFilename.c_str() );
        else
            osTMP.Printf( "L1BGCPS:\"%s\"", osFilename.c_str() );
        poOutDS->SetMetadataItem( "X_DATASET", osTMP, "GEOLOCATION" );
        poOutDS->SetMetadataItem( "X_BAND", "1" , "GEOLOCATION" );
        poOutDS->SetMetadataItem( "Y_DATASET", osTMP, "GEOLOCATION" );
        poOutDS->SetMetadataItem( "Y_BAND", "2" , "GEOLOCATION" );

        if( bInterpol )
        {
            poOutDS->SetMetadataItem( "PIXEL_OFFSET", "0", "GEOLOCATION" );
            poOutDS->SetMetadataItem( "PIXEL_STEP", "1", "GEOLOCATION" );
        }
        else
        {
            osTMP.Printf( "%d", poDS->iGCPStart);
            poOutDS->SetMetadataItem( "PIXEL_OFFSET", osTMP, "GEOLOCATION" );
            osTMP.Printf( "%d", poDS->iGCPStep);
            poOutDS->SetMetadataItem( "PIXEL_STEP", osTMP, "GEOLOCATION" );
        }

        poOutDS->SetMetadataItem( "LINE_OFFSET", "0", "GEOLOCATION" );
        poOutDS->SetMetadataItem( "LINE_STEP", "1", "GEOLOCATION" );
    }
    
    if( poOutDS != poDS )
        return poOutDS;

    if( eL1BFormat == L1B_NOAA9 )
    {
        char** papszSubdatasets = NULL;
        papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_1_NAME",
                                        CPLSPrintf("L1B_SOLAR_ZENITH_ANGLES:\"%s\"", osFilename.c_str()));
        papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_1_DESC",
                                        "Solar zenith angles");
        poDS->SetMetadata(papszSubdatasets, "SUBDATASETS");
        CSLDestroy(papszSubdatasets);
    }
    else
    {
        char** papszSubdatasets = NULL;
        papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_1_NAME",
                                        CPLSPrintf("L1B_ANGLES:\"%s\"", osFilename.c_str()));
        papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_1_DESC",
                                        "Solar zenith angles, satellite zenith angles and relative azimuth angles");

        if( poDS->iCLAVRStart > 0 )
        {
            papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_2_NAME",
                                            CPLSPrintf("L1B_CLOUDS:\"%s\"", osFilename.c_str()));
            papszSubdatasets = CSLSetNameValue(papszSubdatasets, "SUBDATASET_2_DESC",
                                            "Clouds from AVHRR (CLAVR)");
        }

        poDS->SetMetadata(papszSubdatasets, "SUBDATASETS");
        CSLDestroy(papszSubdatasets);
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int iBand, i;
    
    for( iBand = 1, i = 0; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new L1BRasterBand( poDS, iBand ));
        
        // Channels descriptions
        if ( poDS->eSpacecraftID >= NOAA6 && poDS->eSpacecraftID <= METOP3 )
        {
            if ( !(i & 0x01) && poDS->iChannelsMask & 0x01 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[0] );
                i |= 0x01;
                continue;
            }
            if ( !(i & 0x02) && poDS->iChannelsMask & 0x02 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[1] );
                i |= 0x02;
                continue;
            }
            if ( !(i & 0x04) && poDS->iChannelsMask & 0x04 )
            {
                if ( poDS->eSpacecraftID >= NOAA15
                     && poDS->eSpacecraftID <= METOP3 )
                    if ( poDS->iInstrumentStatus & 0x0400 )
                        poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[7] );
                    else
                        poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[6] );
                else    
                    poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[2] );
                i |= 0x04;
                continue;
            }
            if ( !(i & 0x08) && poDS->iChannelsMask & 0x08 )
            {
                poDS->GetRasterBand(iBand)->SetDescription( apszBandDesc[3] );
                i |= 0x08;
                continue;
            }
            if ( !(i & 0x10) && poDS->iChannelsMask & 0x10 )
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
    
    if( poDS->bExposeMaskBand )
        poDS->poMaskBand = new L1BMaskBand(poDS);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Fetch metadata in CSV file                                      */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption("L1B_FETCH_METADATA", "NO")) )
    {
        poDS->FetchMetadata();
    }

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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NOAA Polar Orbiter Level 1b Data Set" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_l1b.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

        poDriver->pfnOpen = L1BDataset::Open;
        poDriver->pfnIdentify = L1BDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
