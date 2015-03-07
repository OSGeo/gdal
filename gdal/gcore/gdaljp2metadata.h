/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  JP2 Box Reader (and GMLJP2 Interpreter)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "gdal.h"

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

public:
                GDALJP2Box( VSILFILE * = NULL );
                ~GDALJP2Box();

    int         SetOffset( GIntBig nNewOffset );
    int         ReadBox();

    int         ReadFirst();
    int         ReadNext();

    int         ReadFirstChild( GDALJP2Box *poSuperBox );
    int         ReadNextChild( GDALJP2Box *poSuperBox );

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
                                      int nDataSize, GByte *pabyData );
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

public:
    char  **papszGMLMetadata;
    
    int     bHaveGeoTransform;
    double  adfGeoTransform[6];
    int     bPixelIsPoint;

    char   *pszProjection;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    char  **papszMetadata;
    char   *pszXMPMetadata;

public:
            GDALJP2Metadata();
            ~GDALJP2Metadata();

    int     ReadBoxes( VSILFILE * fpVSIL );

    int     ParseJP2GeoTIFF();
    int     ParseMSIG();
    int     ParseGMLCoverageDesc();

    int     ReadAndParse( VSILFILE * fpVSIL );
    int     ReadAndParse( const char *pszFilename );

    // Write oriented. 
    void    SetProjection( const char *pszWKT );
    void    SetGeoTransform( double * );
    void    SetGCPs( int, const GDAL_GCP * );
    
    GDALJP2Box *CreateJP2GeoTIFF();
    GDALJP2Box *CreateGMLJP2( int nXSize, int nYSize );
};

#endif /* ndef GDAL_JP2READER_H_INCLUDED */
