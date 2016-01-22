/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  CEOS library prototypes
 * Author:   Paul Lahaie, pjlahaie@atlsci.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc
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


#ifndef __CEOS_H
#define __CEOS_H

#include "cpl_conv.h"

CPL_C_START

#define int32 GInt32
#define TBool int
#define uchar unsigned char

typedef struct Link_t_struct 
{
  struct Link_t_struct	*next;
  void		*object;
} Link_t;

#define HMalloc CPLMalloc
#define HFree CPLFree
#define HCalloc CPLCalloc

Link_t *ceos2CreateLink( void * pObject );
Link_t *InsertLink(Link_t *psList, Link_t *psLink);
void    DestroyList( Link_t *psList );
Link_t *AddLink( Link_t *psList, Link_t *psLink );

/* Basic CEOS header defs */

#define __SEQUENCE_OFF 0
#define __TYPE_OFF 4
#define __LENGTH_OFF 8
#define __CEOS_HEADER_LENGTH 12

/* Defines for CEOS banding type */

#define __CEOS_IL_PIXEL 1
#define __CEOS_IL_LINE  2
#define __CEOS_IL_BAND  3

/* Defines for CEOS data types */

#define __CEOS_TYP_CHAR 1
#define __CEOS_TYP_UCHAR 2
#define __CEOS_TYP_SHORT 3
#define __CEOS_TYP_USHORT 4
#define __CEOS_TYP_LONG 5
#define __CEOS_TYP_ULONG 6
#define __CEOS_TYP_FLOAT 7
#define __CEOS_TYP_DOUBLE 8
#define __CEOS_TYP_COMPLEX_CHAR 9
#define __CEOS_TYP_COMPLEX_UCHAR 10
#define __CEOS_TYP_COMPLEX_SHORT 11
#define __CEOS_TYP_COMPLEX_USHORT 12
#define __CEOS_TYP_COMPLEX_LONG 13
#define __CEOS_TYP_COMPLEX_ULONG 14
#define __CEOS_TYP_COMPLEX_FLOAT 15
#define __CEOS_TYP_CCP_COMPLEX_FLOAT 16 /* COMPRESSED CROSS PRODUCT */
#define __CEOS_TYP_PALSAR_COMPLEX_SHORT 17 /* PALSAR - treat as COMPLEX SHORT*/

/* Defines for CEOS file names */

#define __CEOS_VOLUME_DIR_FILE 0
#define __CEOS_LEADER_FILE    1
#define __CEOS_IMAGRY_OPT_FILE 2
#define __CEOS_TRAILER_FILE    3
#define __CEOS_NULL_VOL_FILE   4
#define __CEOS_ANY_FILE -1

/* Defines for Recipe values */

#define __CEOS_REC_NUMCHANS 1
#define __CEOS_REC_INTERLEAVE 2
#define __CEOS_REC_DATATYPE 3
#define __CEOS_REC_BPR 4
#define __CEOS_REC_LINES 5
#define __CEOS_REC_TBP 6
#define __CEOS_REC_BBP 7
#define __CEOS_REC_PPL 8
#define __CEOS_REC_LBP 9
#define __CEOS_REC_RBP 10
#define __CEOS_REC_BPP 11
#define __CEOS_REC_RPL 12
#define __CEOS_REC_PPR 13
#define __CEOS_REC_IDS 14
#define __CEOS_REC_FDL 15
#define __CEOS_REC_PIXORD 16
#define __CEOS_REC_LINORD 17
#define __CEOS_REC_PRODTYPE 18
#define __CEOS_REC_RECORDSIZE 19
#define __CEOS_REC_SUFFIX_SIZE 20
#define __CEOS_REC_PDBPR 21

/* Defines for Recipe Types */

#define __CEOS_REC_TYP_A  1
#define __CEOS_REC_TYP_B  2
#define __CEOS_REC_TYP_I  3

/* Defines for SAR Embedded info */

#define __CEOS_SAR_ACQ_YEAR 1
#define __CEOS_SAR_ACQ_DAY  2
#define __CEOS_SAR_ACQ_MSEC 4
#define __CEOS_SAR_TRANS_POL 8
#define __CEOS_SAR_PULSE_REP 16
#define __CEOS_SAR_SLANT_FIRST 32
#define __CEOS_SAR_SLANT_MID 64
#define __CEOS_SAR_SLANT_LAST 128

/* Maximum size of LUT for Calibration records */
#define __CEOS_RADAR_MAX_LUT 512
#define __CEOS_RADAR_FLIP_DATE 19980101
#define __CEOS_RADAR_FACILITY "CDPF-RSAT"


typedef union
{
    int32          Int32Code;
    struct
    {
        uchar      Subtype1;
        uchar      Type;
        uchar      Subtype2;
        uchar      Subtype3;
    } UCharCode;
} CeosTypeCode_t;

typedef struct
{
    int32          Sequence;
    CeosTypeCode_t TypeCode;
    int32          Length;
    int32          Flavour;
    int32          Subsequence;
    int32          FileId;
    uchar *        Buffer;
} CeosRecord_t;

struct CeosSARImageDesc
{
    TBool ImageDescValid;
    int   NumChannels;
    int32 ChannelInterleaving;
    int32 DataType;
    int   BytesPerRecord;
    int   Lines;
    int   TopBorderPixels;
    int   BottomBorderPixels;
    int   PixelsPerLine;
    int   LeftBorderPixels;
    int   RightBorderPixels;
    int   BytesPerPixel;
    int   RecordsPerLine;
    int   PixelsPerRecord;
    int   ImageDataStart;
    int   ImageSuffixData;
    int   FileDescriptorLength;
    int32 PixelOrder;
    int32 LineOrder;
    int   PixelDataBytesPerRecord;
};

typedef struct
{
    int32          Flavour;
    int32          Sensor;
    int32          ProductType;
    int32          FileNamingConvention;
    TBool          VolumeDirectoryFile;
    TBool          SARLeaderFile;
    TBool          ImagryOptionsFile;
    TBool          SARTrailerFile;
    TBool          NullVolumeDirectoryFile;

    struct         CeosSARImageDesc     ImageDesc;

    Link_t *       RecordList;
} CeosSARVolume_t;

typedef struct 
{
    int            ImageDescValue;
    int            Override;
    int            FileId;
    struct         { unsigned char Subtype1,
                                   Type,
                                   Subtype2,
                                   Subtype3;
                   } TypeCode;
    int            Offset;
    int            Length;
    int            Type;
} CeosRecipeType_t;


typedef struct
{
    CeosRecipeType_t *Recipe;
} CeosSARImageDescRecipe_t;


typedef struct
{
    int32     ValidFields;
    TBool     SensorUpdate;
    int32     AcquisitionYear;
    int32     AcquisitionDay;
    int32     AcquisitionMsec;
    int32     TransmittedPolarization;
    int32     ReceivedPolarization;
    int32     PulsRepetitionFrequency;
    int32     SlantRangeFirstPixel;
    int32     SlantRangeMidPixel;
    int32     SlantRangeLastPixel;
} CeosSAREmbeddedInfo_t;

typedef struct
{
    double Slant[ 6 ];
    double Lut[ 512 ];
    double SemiMajorAxis;
    double PlatformLatitude;
    double CalibrationScale;
    int NumberOfSamples;
    int Increment;
    TBool PossiblyFlipped;
    
} CeosRadarCalibration_t;


/* Function prototypes */

void InitEmptyCeosRecord(CeosRecord_t *record, int32 sequence, CeosTypeCode_t typecode, int32 length);

void InitCeosRecord(CeosRecord_t *record, uchar *buffer);

void InitCeosRecordWithHeader(CeosRecord_t *record, uchar *header, uchar *buffer);

int DetermineCeosRecordBodyLength(const uchar *header);

void DeleteCeosRecord(CeosRecord_t *record);

void GetCeosRecordStruct(const CeosRecord_t *record, void *struct_ptr);

void PutCeosRecordStruct(CeosRecord_t *record, const void *struct_ptr);

void GetCeosField(CeosRecord_t *, int32, const char *, void *);

void SetCeosField(CeosRecord_t *record, int32 start_byte, char *format, void *value);

void SetIntCeosField(CeosRecord_t *record, int32 start_byte, int32 length, int32 value);

CeosRecord_t *FindCeosRecord(Link_t *record_list, CeosTypeCode_t typecode, int32 fileid, int32 flavour, int32 subsequence);

void SerializeCeosRecordsToFile(Link_t *record_list, VSILFILE *fp);

void SerializeCeosRecordsFromFile( Link_t *record_list, VSILFILE *fp );

void InitCeosSARVolume( CeosSARVolume_t *volume, int32 file_name_convention );

void GetCeosSARImageDesc( CeosSARVolume_t *volume );

void GetCeosSARImageDescInfo(CeosSARVolume_t *volume, CeosSARImageDescRecipe_t *recipe);

void CalcCeosSARImageFilePosition(CeosSARVolume_t *volume, int channel, int line, int *record, int *file_offset);

int32 GetCeosSARImageData(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, int channel, int xoff, int xsize, int bufsize, uchar *buffer);

void DetermineCeosSARPixelOrder(CeosSARVolume_t *volume, CeosRecord_t *record );

void GetCeosSAREmbeddedInfo(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, CeosSAREmbeddedInfo_t *info);

void DeleteCeosSARVolume(CeosSARVolume_t *volume);

void RegisterRecipes(void);
void FreeRecipes();

void AddRecipe( int ( *function )( CeosSARVolume_t *volume, void *token ),
		void *token, const char *name );

int CeosDefaultRecipe( CeosSARVolume_t *volume, void *token );
int ScanSARRecipeFCN( CeosSARVolume_t *volume, void *token );

/* ceoscalib.c function declarations */

CeosRadarCalibration_t *GetCeosRadarCalibration( CeosSARVolume_t *volume );

/* CEOS byte swapping stuff */

#if defined(CPL_MSB)
#define NativeToCeos(a,b,c,d) memcpy(a,b,c)
#define CeosToNative(a,b,c,d) memcpy(a,b,c)
#else
void NativeToCeos( void *dst, const void *src, const size_t len, const size_t swap_unit);
#define CeosToNative(a,b,c,d) NativeToCeos(a,b,c,d)
#endif

/* Recipe defines */

CPL_C_END

#endif
