/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  JP2 Box Reader (and GMLJP2 Interpreter)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_JP2READER_H_INCLUDED
#define GDAL_JP2READER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

/************************************************************************/
/*                              GDALJP2Box                              */
/************************************************************************/

class CPL_DLL GDALJP2Box
{

    VSILFILE   *fpVSIL;

    char        szBoxType[5];

    GIntBig     nBoxOffset;
    GIntBig     nBoxLength;

    GIntBig     nDataOffset;

    GByte       abyUUID[16];

    GByte      *pabyData;

    CPL_DISALLOW_COPY_ASSIGN(GDALJP2Box)

public:
    explicit    GDALJP2Box( VSILFILE * = nullptr );
                ~GDALJP2Box();

    int         SetOffset( GIntBig nNewOffset );
    int         ReadBox();

    int         ReadFirst();
    int         ReadNext();

    int         ReadFirstChild( GDALJP2Box *poSuperBox );
    int         ReadNextChild( GDALJP2Box *poSuperBox );

    GIntBig     GetBoxOffset() const { return nBoxOffset; }
    GIntBig     GetBoxLength() const { return nBoxLength; }

    GIntBig     GetDataOffset() const { return nDataOffset; }
    GIntBig     GetDataLength();

    const char *GetType() { return szBoxType; }

    GByte      *ReadBoxData();

    int         IsSuperBox();

    int         DumpReadable( FILE *, int nIndentLevel = 0 );

    VSILFILE   *GetFILE() { return fpVSIL; }

    const GByte *GetUUID() { return abyUUID; }

    // write support
    void        SetType( const char * );
    void        SetWritableData( int nLength, const GByte *pabyData );
    void        AppendWritableData( int nLength, const void *pabyDataIn );
    void        AppendUInt32( GUInt32 nVal );
    void        AppendUInt16( GUInt16 nVal );
    void        AppendUInt8( GByte nVal );
    const GByte*GetWritableData() { return pabyData; }

    // factory methods.
    static GDALJP2Box *CreateSuperBox( const char* pszType,
                                       int nCount, GDALJP2Box **papoBoxes );
    static GDALJP2Box *CreateAsocBox( int nCount, GDALJP2Box **papoBoxes );
    static GDALJP2Box *CreateLblBox( const char *pszLabel );
    static GDALJP2Box *CreateLabelledXMLAssoc( const char *pszLabel,
                                               const char *pszXML );
    static GDALJP2Box *CreateUUIDBox( const GByte *pabyUUID,
                                      int nDataSize, const GByte *pabyData );
};

/************************************************************************/
/*                           GDALJP2Metadata                            */
/************************************************************************/

typedef struct _GDALJP2GeoTIFFBox GDALJP2GeoTIFFBox;

class CPL_DLL GDALJP2Metadata

{
private:
    void    CollectGMLData( GDALJP2Box * );
    int     GMLSRSLookup( const char *pszURN );

    int    nGeoTIFFBoxesCount;
    GDALJP2GeoTIFFBox  *pasGeoTIFFBoxes;

    int    nMSIGSize;
    GByte  *pabyMSIGData;

    int      GetGMLJP2GeoreferencingInfo( int& nEPSGCode,
                                          double adfOrigin[2],
                                          double adfXVector[2],
                                          double adfYVector[2],
                                          const char*& pszComment,
                                          CPLString& osDictBox,
                                          int& bNeedAxisFlip );
    static CPLXMLNode* CreateGDALMultiDomainMetadataXML(
                                       GDALDataset* poSrcDS,
                                       int bMainMDDomainOnly );

    CPL_DISALLOW_COPY_ASSIGN(GDALJP2Metadata)

public:
    char  **papszGMLMetadata;

    bool    bHaveGeoTransform;
    double  adfGeoTransform[6];
    bool    bPixelIsPoint;

    OGRSpatialReference m_oSRS{};

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    char **papszRPCMD;

    char  **papszMetadata; /* TIFFTAG_?RESOLUTION* for now from resd box */
    char   *pszXMPMetadata;
    char   *pszGDALMultiDomainMetadata; /* as serialized XML */
    char   *pszXMLIPR; /* if an IPR box with XML content has been found */

public:
            GDALJP2Metadata();
            ~GDALJP2Metadata();

    int     ReadBoxes( VSILFILE * fpVSIL );

    int     ParseJP2GeoTIFF();
    int     ParseMSIG();
    int     ParseGMLCoverageDesc();

    int     ReadAndParse( VSILFILE * fpVSIL,
                          int nGEOJP2Index = 0, int nGMLJP2Index = 1,
                          int nMSIGIndex = 2, int *pnIndexUsed = nullptr );
    int     ReadAndParse( const char *pszFilename, int nGEOJP2Index = 0,
                          int nGMLJP2Index = 1, int nMSIGIndex = 2,
                          int nWorldFileIndex = 3, int *pnIndexUsed = nullptr );

    // Write oriented.
    void    SetSpatialRef( const OGRSpatialReference *poSRS );
    void    SetGeoTransform( double * );
    void    SetGCPs( int, const GDAL_GCP * );
    void    SetRPCMD( char** papszRPCMDIn );

    GDALJP2Box *CreateJP2GeoTIFF();
    GDALJP2Box *CreateGMLJP2( int nXSize, int nYSize );
    GDALJP2Box *CreateGMLJP2V2( int nXSize, int nYSize,
                                const char* pszDefFilename,
                                GDALDataset* poSrcDS );

    static GDALJP2Box* CreateGDALMultiDomainMetadataXMLBox(
                                       GDALDataset* poSrcDS,
                                       int bMainMDDomainOnly );
    static GDALJP2Box** CreateXMLBoxes( GDALDataset* poSrcDS,
                                        int* pnBoxes );
    static GDALJP2Box *CreateXMPBox ( GDALDataset* poSrcDS );
    static GDALJP2Box *CreateIPRBox ( GDALDataset* poSrcDS );
    static int   IsUUID_MSI(const GByte *abyUUID);
    static int   IsUUID_XMP(const GByte *abyUUID);
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* ndef GDAL_JP2READER_H_INCLUDED */
