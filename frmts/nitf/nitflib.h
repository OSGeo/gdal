/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Main GDAL independent include file for NITF support.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.19  2006/10/24 02:18:06  fwarmerdam
 * added image attachment metadata
 *
 * Revision 1.18  2006/10/13 02:53:48  fwarmerdam
 * various improvements to TRE and VQ LUT support for bug 1313
 *
 * Revision 1.17  2005/02/18 19:28:15  fwarmerdam
 * added NITFIHFieldOffset
 *
 * Revision 1.16  2004/12/21 04:57:36  fwarmerdam
 * added support for writing UTM ICORDS/IGEOLO values
 *
 * Revision 1.15  2004/12/10 21:35:00  fwarmerdam
 * preliminary support for writing JPEG2000 compressed data
 *
 * Revision 1.14  2004/12/10 21:27:24  fwarmerdam
 * Added ICHIPB support
 *
 * Revision 1.13  2004/05/06 14:58:06  warmerda
 * added USE00A and STDIDC parsing and reporting as metadata
 *
 * Revision 1.12  2004/04/28 15:19:00  warmerda
 * added geocentric to geodetic conversion
 *
 * Revision 1.11  2004/04/16 15:26:04  warmerda
 * completed metadata support
 *
 * Revision 1.10  2004/04/15 20:52:53  warmerda
 * added metadata support
 *
 * Revision 1.9  2004/04/02 20:44:37  warmerda
 * preserve APBB (actual bits per pixel) field as metadata
 *
 * Revision 1.8  2003/05/29 19:50:57  warmerda
 * added TRE in image, and RPC00B support
 *
 * Revision 1.7  2002/12/18 20:16:04  warmerda
 * support writing IGEOLO
 *
 * Revision 1.6  2002/12/18 06:35:15  warmerda
 * implement nodata support for mapped data
 *
 * Revision 1.5  2002/12/17 21:23:15  warmerda
 * implement LUT reading and writing
 *
 * Revision 1.4  2002/12/17 20:03:08  warmerda
 * added rudimentary NITF 1.1 support
 *
 * Revision 1.3  2002/12/17 05:26:26  warmerda
 * implement basic write support
 *
 * Revision 1.2  2002/12/03 04:43:54  warmerda
 * lots of work
 *
 * Revision 1.1  2002/12/02 06:09:29  warmerda
 * New
 *
 */

#ifndef NITFLIB_H_INCLUDED
#define NITFLIB_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"

CPL_C_START

typedef struct { 
    char szSegmentType[3]; /* one of "IM", ... */

    GUInt32 nSegmentHeaderStart;
    GUInt32 nSegmentHeaderSize;
    GUInt32 nSegmentStart;
    GUInt32 nSegmentSize;

    void *hAccess;
} NITFSegmentInfo;

typedef struct {
    FILE    *fp;

    char    szVersion[10];

    int     nSegmentCount;
    NITFSegmentInfo *pasSegmentInfo;

    char    *pachHeader;

    int     nTREBytes;
    char    *pachTRE;

    char    **papszMetadata;
    
} NITFFile;

/* -------------------------------------------------------------------- */
/*      File level prototypes.                                          */
/* -------------------------------------------------------------------- */
NITFFile CPL_DLL *NITFOpen( const char *pszFilename, int bUpdatable );
void     CPL_DLL  NITFClose( NITFFile * );

int      CPL_DLL  NITFCreate( const char *pszFilename, 
                              int nPixels, int nLines, int nBands, 
                              int nBitsPerSample, const char *pszPVType,
                              char **papszOptions );

const char CPL_DLL *NITFFindTRE( const char *pszTREData, int nTREBytes, 
                                 const char *pszTag, int *pnFoundTRESize );

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
    int	nLocId;
    int nLocOffset;
    int nLocSize;
} NITFLocation;

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
    int        nPixelOffset;
    int        nLineOffset;
    int        nBlockOffset;
    int        nBandOffset;

    GUInt32    *panBlockStart;

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
char      CPL_DLL **NITFReadUSE00A( NITFImage *psImage );
char      CPL_DLL **NITFReadSTDIDC( NITFImage *psImage );

GUInt32   CPL_DLL NITFIHFieldOffset( NITFImage *psImage, 
                                     const char *pszFieldName );

#define BLKREAD_OK    0
#define BLKREAD_NULL  1
#define BLKREAD_FAIL  2

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

CPL_C_END

#endif /* ndef NITFLIB_H_INCLUDED */

