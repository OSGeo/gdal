/* Copyright (c) 1997
 * Atlantis Scientific Inc, 20 Colonnade, Suite 110
 * Nepean, Ontario, K2E 7M6, Canada
 *
 * All rights reserved.  Not to be used, reproduced
 * or disclosed without permission.
 */

/* +---------------------------------------------------------------------+
 * |@@@@@@@@@@    @@@| EASI/PACE V6.0, Copyright (c) 1997.               |
 * |@@@@@@ ***      @|                                                   |
 * |@@@  *******    @| PCI Inc., 50 West Wilmot Street,                  |
 * |@@  *********  @@| Richmond Hill, Ontario, L4B 1M5, Canada.          |
 * |@    *******  @@@|                                                   |
 * |@      *** @@@@@@| All rights reserved. Not to be used, reproduced   |
 * |@@@    @@@@@@@@@@| or disclosed without permission.                  |
 * +---------------------------------------------------------------------+
 */

#ifndef __CEOS_H
#define __CEOS_H

#include "gdb.h"

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


/* Function prototypes */

void InitEmptyCeosRecord(CeosRecord_t *record, int32 sequence, CeosTypeCode_t typecode, int32 length);

void InitCeosRecord(CeosRecord_t *record, uchar *buffer);

void InitCeosRecordWithHeader(CeosRecord_t *record, uchar *header, uchar *buffer);

int DetermineCeosRecordBodyLength(const uchar *header);

void DeleteCeosRecord(CeosRecord_t *record);

void GetCeosRecordStruct(const CeosRecord_t *record, void *struct_ptr);

void PutCeosRecordStruct(CeosRecord_t *record, const void *struct_ptr);

void GetCeosField(CeosRecord_t *record, int32 start_byte, char *format, void *value);

void SetCeosField(CeosRecord_t *record, int32 start_byte, char *format, void *value);

void SetIntCeosField(CeosRecord_t *record, int32 start_byte, int32 length, int32 value);

CeosRecord_t *FindCeosRecord(Link_t *record_list, CeosTypeCode_t typecode, int32 fileid, int32 flavour, int32 subsequence);

void SerializeCeosRecordsToFile(Link_t *record_list, FILE *fp);

void SerializeCeosRecordsFromFile( Link_t *record_list, FILE *fp );

void InitCeosSARVolume( CeosSARVolume_t *volume, int32 file_name_convention );

void GetCeosSARImageDesc( CeosSARVolume_t *volume );

void GetCeosSARImageDescInfo(CeosSARVolume_t *volume, CeosSARImageDescRecipe_t *recipe);

void CalcCeosSARImageFilePosition(CeosSARVolume_t *volume, int channel, int line, int *record, int *file_offset);

int32 GetCeosSARImageData(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, int channel, int xoff, int xsize, int bufsize, uchar *buffer);

void DetermineCeosSARPixelOrder(CeosSARVolume_t *volume, CeosRecord_t *record );

void GetCeosSAREmbeddedInfo(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, CeosSAREmbeddedInfo_t *info);

void DeleteCeosSARVolume(CeosSARVolume_t *volume);

void RegisterRecipes(void);

void AddRecipe( int ( *function )( CeosSARVolume_t *volume, void *token ),
		void *token );

int CeosDefaultRecipe( CeosSARVolume_t *volume, void *token );
int ScanSARRecipeFCN( CeosSARVolume_t *volume, void *token );

int GetCeosOrbitalData( CeosSARVolume_t *volume, EphemerisSeg_t *Orb, ProjInfo_t *Proj );

/* CEOS byte swapping stuff */

#if !defined(SEX_SWAPPED)
#define NativeToCeos(a,b,c,d) memcpy(a,b,c)
#define CeosToNative(a,b,c,d) memcpy(a,b,c)
#else
void NativeToCeos( void *dst, const void *src, const size_t len, const size_t swap_unit);
#define CeosToNative(a,b,c,d) NativeToCeos(a,b,c,d)
#endif

/* Recipe defines */


#endif
