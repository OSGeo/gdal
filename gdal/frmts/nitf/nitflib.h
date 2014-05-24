/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Main GDAL independent include file for NITF support.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef NITFLIB_H_INCLUDED
#define NITFLIB_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "cpl_minixml.h"

CPL_C_START

typedef struct { 
    char szSegmentType[3]; /* one of "IM", ... */

    GUIntBig nSegmentHeaderStart;
    GUInt32 nSegmentHeaderSize;
    GUIntBig nSegmentStart;
    GUIntBig nSegmentSize;

    void *hAccess;

    /* extra info related to relative display */
    int     nDLVL;
    int     nALVL;
    int     nLOC_R;
    int     nLOC_C;
    int     nCCS_R;
    int     nCCS_C;
} NITFSegmentInfo;

typedef struct {
    VSILFILE  *fp;

    char    szVersion[10];

    int     nSegmentCount;
    NITFSegmentInfo *pasSegmentInfo;

    char    *pachHeader;

    int     nTREBytes;
    char    *pachTRE;

    char    **papszMetadata;

    CPLXMLNode *psNITFSpecNode;
    
} NITFFile;

/* -------------------------------------------------------------------- */
/*      File level prototypes.                                          */
/* -------------------------------------------------------------------- */
NITFFile CPL_DLL *NITFOpen( const char *pszFilename, int bUpdatable );
NITFFile *NITFOpenEx( VSILFILE *fp, const char *pszFilename );
void     CPL_DLL  NITFClose( NITFFile * );

int      CPL_DLL  NITFCreate( const char *pszFilename, 
                              int nPixels, int nLines, int nBands, 
                              int nBitsPerSample, const char *pszPVType,
                              char **papszOptions );

const char CPL_DLL *NITFFindTRE( const char *pszTREData, int nTREBytes, 
                                 const char *pszTag, int *pnFoundTRESize );
const char CPL_DLL *NITFFindTREByIndex( const char *pszTREData, int nTREBytes,
                                const char *pszTag, int nTreIndex,
                                int *pnFoundTRESize );

int CPL_DLL NITFCollectAttachments( NITFFile *psFile );
int CPL_DLL NITFReconcileAttachments( NITFFile *psFile );

/* -------------------------------------------------------------------- */
/*      Image level access.                                             */
/* -------------------------------------------------------------------- */
typedef struct {
    char      szIREPBAND[3];
    char      szISUBCAT[7];

    int       nSignificantLUTEntries;
    int       nLUTLocation;
    unsigned char *pabyLUT;

} NITFBandInfo;

typedef struct { 
    GUInt16 nLocId;
    GUInt32 nLocOffset;
    GUInt32 nLocSize;
} NITFLocation;

typedef struct
{
  unsigned short   tableId;
  unsigned int     nRecords;
  unsigned char    elementLength;
  unsigned short   histogramRecordLength;
  unsigned int     colorTableOffset;
  unsigned int     histogramTableOffset;
} NITFColormapRecord;


typedef struct {
    NITFFile  *psFile;
    int        iSegment;
    char      *pachHeader;

    int        nRows;
    int        nCols;
    int        nBands;
    int        nBitsPerSample;

    NITFBandInfo *pasBandInfo;
    
    char       chIMODE;

    int        nBlocksPerRow;
    int        nBlocksPerColumn;
    int        nBlockWidth;
    int        nBlockHeight;

    char       szPVType[4];
    char       szIREP[9];
    char       szICAT[9];
    int        nABPP; /* signficant bits per pixel */

    char       chICORDS;
    int        bHaveIGEOLO;

    int        nZone;
    double     dfULX;
    double     dfULY;
    double     dfURX;
    double     dfURY;
    double     dfLRX;
    double     dfLRY;
    double     dfLLX;
    double     dfLLY;
    int        bIsBoxCenterOfPixel;

    char       *pszComments;
    char       szIC[3];
    char       szCOMRAT[5];

    int        nILOCColumn;
    int        nILOCRow;
    int        nIALVL;
    int        nIDLVL;
    char       szIMAG[5];
    
    int        bNoDataSet;
    int        nNoDataValue;

    int     nTREBytes;
    char    *pachTRE;

    /* Internal information not for application use. */
    
    int        nWordSize;
    GUIntBig   nPixelOffset;
    GUIntBig   nLineOffset;
    GUIntBig   nBlockOffset;
    GUIntBig   nBandOffset;

    GUIntBig    *panBlockStart;

    char       **papszMetadata;
    
    GUInt32 *apanVQLUT[4];

    int     nLocCount;
    NITFLocation *pasLocations;

} NITFImage;

NITFImage CPL_DLL *NITFImageAccess( NITFFile *, int iSegment );
void      CPL_DLL  NITFImageDeaccess( NITFImage * );

int       CPL_DLL  NITFReadImageBlock( NITFImage *, int nBlockX, int nBlockY,
                                       int nBand, void *pData );
int       CPL_DLL  NITFReadImageLine( NITFImage *, int nLine, int nBand, 
                                      void *pData );
int       CPL_DLL  NITFWriteImageBlock( NITFImage *, int nBlockX, int nBlockY,
                                        int nBand, void *pData );
int       CPL_DLL  NITFWriteImageLine( NITFImage *, int nLine, int nBand, 
                                       void *pData );
int       CPL_DLL  NITFWriteLUT( NITFImage *psImage, int nBand, int nColors, 
                                 unsigned char *pabyLUT );
int       CPL_DLL  NITFWriteIGEOLO( NITFImage *psImage, char chICORDS,
                                    int nZone,
                                    double dfULX, double dfULY,
                                    double dfURX, double dfURY,
                                    double dfLRX, double dfLRY,
                                    double dfLLX, double dfLLY );
char      CPL_DLL **NITFReadCSEXRA( NITFImage *psImage );
char      CPL_DLL **NITFReadPIAIMC( NITFImage *psImage );
char      CPL_DLL **NITFReadUSE00A( NITFImage *psImage );
char      CPL_DLL **NITFReadSTDIDC( NITFImage *psImage );
char      CPL_DLL **NITFReadBLOCKA( NITFImage *psImage );

GUIntBig  CPL_DLL NITFIHFieldOffset( NITFImage *psImage, 
                                     const char *pszFieldName );

#define BLKREAD_OK    0
#define BLKREAD_NULL  1
#define BLKREAD_FAIL  2

int NITFUncompressARIDPCM( NITFImage *psImage,
                           GByte *pabyInputData, int nInputBytes,
                           GByte *pabyOutputImage );
int NITFUncompressBILEVEL( NITFImage *psImage, 
                           GByte *pabyInputData, int nInputBytes,
                           GByte *pabyOutputImage );

NITFLocation* NITFReadRPFLocationTable(VSILFILE* fp, int* pnLocCount);

/* -------------------------------------------------------------------- */
/*      DE segment access.                                              */
/* -------------------------------------------------------------------- */
typedef struct {
    NITFFile  *psFile;
    int        iSegment;
    char      *pachHeader;

    char       **papszMetadata;
} NITFDES;

NITFDES   CPL_DLL *NITFDESAccess( NITFFile *, int iSegment );
void      CPL_DLL  NITFDESDeaccess( NITFDES * );

int       CPL_DLL  NITFDESGetTRE(   NITFDES* psDES,
                                    int nOffset,
                                    char szTREName[7],
                                    char** ppabyTREData,
                                    int* pnFoundTRESize);
void      CPL_DLL  NITFDESFreeTREData( char* pabyTREData );

int       CPL_DLL  NITFDESExtractShapefile(NITFDES* psDES, const char* pszRadixFileName);

/* -------------------------------------------------------------------- */
/*      These are really intended to be private helper stuff for the    */
/*      library.                                                        */
/* -------------------------------------------------------------------- */
char *NITFGetField( char *pszTarget, const char *pszSource, 
                    int nStart, int nLength );
void NITFExtractMetadata( char ***ppapszMetadata, const char *pachHeader,
                          int nStart, int nLength, const char *pszName );

/* -------------------------------------------------------------------- */
/*      location ids from the location table (from MIL-STD-2411-1).     */
/* -------------------------------------------------------------------- */

typedef enum {
    LID_HeaderComponent = 128,
    LID_LocationComponent = 129,
    LID_CoverageSectionSubheader = 130,
    LID_CompressionSectionSubsection = 131,
    LID_CompressionLookupSubsection = 132,
    LID_CompressionParameterSubsection = 133,
    LID_ColorGrayscaleSectionSubheader = 134,
    LID_ColormapSubsection = 135,
    LID_ImageDescriptionSubheader = 136,
    LID_ImageDisplayParametersSubheader = 137,
    LID_MaskSubsection = 138,
    LID_ColorConverterSubsection = 139,
    LID_SpatialDataSubsection = 140,
    LID_AttributeSectionSubheader = 141,
    LID_AttributeSubsection = 142,
    LID_ExplicitArealCoverageTable = 143,
    LID_RelatedImagesSectionSubheader = 144,
    LID_RelatedImagesSubsection = 145,
    LID_ReplaceUpdateSectionSubheader = 146,
    LID_ReplaceUpdateTable = 147,
    LID_BoundaryRectangleSectionSubheader = 148,
    LID_BoundaryRectangleTable = 149,
    LID_FrameFileIndexSectionSubHeader = 150,
    LID_FrameFileIndexSubsection = 151,
    LID_ColorTableIndexSectionSubheader = 152,
    LID_ColorTableIndexRecord = 153
} NITFLocId;

/* -------------------------------------------------------------------- */
/*      RPC structure, and function to fill it.                         */
/* -------------------------------------------------------------------- */
typedef struct  {
    int			SUCCESS;

    double		ERR_BIAS;
    double      ERR_RAND;

    double      LINE_OFF;
    double      SAMP_OFF;
    double      LAT_OFF;
    double      LONG_OFF;
    double      HEIGHT_OFF;

    double      LINE_SCALE;
    double      SAMP_SCALE;
    double      LAT_SCALE;
    double      LONG_SCALE;
    double      HEIGHT_SCALE;

    double      LINE_NUM_COEFF[20];
    double      LINE_DEN_COEFF[20];
    double      SAMP_NUM_COEFF[20];
    double      SAMP_DEN_COEFF[20];
} NITFRPC00BInfo;

int CPL_DLL NITFReadRPC00B( NITFImage *psImage, NITFRPC00BInfo * );
int CPL_DLL NITFRPCGeoToImage(NITFRPC00BInfo *, double, double, double,
                              double *, double *);

/* -------------------------------------------------------------------- */
/*      ICHIP structure, and function to fill it.                         */
/* -------------------------------------------------------------------- */
typedef struct {
	int		XFRM_FLAG;
	double	SCALE_FACTOR;
	int		ANAMORPH_CORR;
	int		SCANBLK_NUM;

	double	OP_ROW_11;
	double	OP_COL_11;

	double	OP_ROW_12;
	double	OP_COL_12;

	double	OP_ROW_21;
	double	OP_COL_21;

	double	OP_ROW_22;
	double	OP_COL_22;

	double	FI_ROW_11;
	double	FI_COL_11;

	double	FI_ROW_12;
	double	FI_COL_12;

	double	FI_ROW_21;
	double	FI_COL_21;

	double	FI_ROW_22;
	double	FI_COL_22;

	int		FI_ROW;
	int		FI_COL;
} NITFICHIPBInfo;

int CPL_DLL NITFReadICHIPB( NITFImage *psImage, NITFICHIPBInfo * );

double CPL_DLL 
        NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( double dfLat );




typedef struct
{
    const char* code;
    const char* abbreviation;
    const char* scaleResolution;
    const char* name;
    const char* rpfDataType;
} NITFSeries;

/** Return not freeable (maybe NULL if no matching) */
const NITFSeries CPL_DLL *NITFGetSeriesInfo(const char* pszFilename);

/* -------------------------------------------------------------------- */
/*                           Internal use                               */
/* -------------------------------------------------------------------- */

char **NITFGenericMetadataRead(char **papszMD,
                               NITFFile* psFile,
                               NITFImage *psImage,
                               const char* pszSpecificTREName);

CPLXMLNode* NITFCreateXMLTre(NITFFile* psFile,
                             const char* pszTREName,
                             const char *pachTRE,
                             int nTRESize);

CPL_C_END

#endif /* ndef NITFLIB_H_INCLUDED */

