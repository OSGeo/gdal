/******************************************************************************
 *
 * Project:  eCognition
 * Purpose:  Implementation of Erdas .LAN / .GIS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>

#include <algorithm>

CPL_CVSID("$Id$")

/**

Erdas Header format: "HEAD74"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (i.e. HEAD74).
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands.
10         6     char      Unknown.
16         4    Int32      Width
20         4    Int32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area.
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

Erdas Header format: "HEADER"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (i.e. HEAD74).
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands.
10         6     char      Unknown.
16         4  Float32      Width
20         4  Float32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area.
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

All binary fields are in the same byte order but it may be big endian or
little endian depending on what platform the file was written on.  Usually
this can be checked against the number of bands though this test won't work
if there are more than 255 bands.

There is also some information on .STA and .TRL files at:

  http://www.pcigeomatics.com/cgi-bin/pcihlp/ERDASWR%7CTRAILER+FORMAT

**/

constexpr int ERD_HEADER_SIZE = 128;

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class LANDataset;

class LAN4BitRasterBand final: public GDALPamRasterBand
{
    GDALColorTable *poCT;
    GDALColorInterp eInterp;

    CPL_DISALLOW_COPY_ASSIGN(LAN4BitRasterBand)

  public:
                   LAN4BitRasterBand( LANDataset *, int );
    ~LAN4BitRasterBand() override;

    GDALColorTable *GetColorTable() override;
    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorTable( GDALColorTable * ) override;
    CPLErr SetColorInterpretation( GDALColorInterp ) override;

    CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                              LANDataset                              */
/* ==================================================================== */
/************************************************************************/

class LANDataset final: public RawDataset
{
    CPL_DISALLOW_COPY_ASSIGN(LANDataset)

  public:
    VSILFILE    *fpImage;  // Image data file.

    char        pachHeader[ERD_HEADER_SIZE];

    OGRSpatialReference* m_poSRS = nullptr;

    double      adfGeoTransform[6];

    CPLString   osSTAFilename{};
    void        CheckForStatistics(void);

    char **GetFileList() override;

  public:
                LANDataset();
    ~LANDataset() override;

    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double * padfTransform ) override;

    const OGRSpatialReference* GetSpatialRef() const override ;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBandsIn,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         LAN4BitRasterBand()                          */
/************************************************************************/

LAN4BitRasterBand::LAN4BitRasterBand( LANDataset *poDSIn, int nBandIn ) :
    poCT(nullptr),
    eInterp(GCI_Undefined)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                         ~LAN4BitRasterBand()                         */
/************************************************************************/

LAN4BitRasterBand::~LAN4BitRasterBand()

{
    if( poCT )
        delete poCT;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr LAN4BitRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                      int nBlockYOff,
                                      void * pImage )

{
    LANDataset *poLAN_DS = reinterpret_cast<LANDataset *>( poDS );
    CPLAssert( nBlockXOff == 0  );

/* -------------------------------------------------------------------- */
/*      Seek to profile.                                                */
/* -------------------------------------------------------------------- */
    const vsi_l_offset nOffset =
        ERD_HEADER_SIZE
        + ( static_cast<vsi_l_offset>(nBlockYOff) * nRasterXSize *
           poLAN_DS->GetRasterCount() ) / 2
        + ( static_cast<vsi_l_offset>(nBand - 1) * nRasterXSize ) / 2;

    if( VSIFSeekL( poLAN_DS->fpImage, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "LAN Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the profile.                                               */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( pImage, 1, nRasterXSize / 2, poLAN_DS->fpImage ) !=
        static_cast<size_t>( nRasterXSize ) / 2 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "LAN Read failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Convert 4bit to 8bit.                                           */
/* -------------------------------------------------------------------- */
    for( int i = nRasterXSize-1; i >= 0; i-- )
    {
        if( (i & 0x01) != 0 )
            reinterpret_cast<GByte *>( pImage )[i] =
                reinterpret_cast<GByte *>(pImage)[i/2] & 0x0f;
        else
            reinterpret_cast<GByte *>( pImage )[i] =
                (reinterpret_cast<GByte *>(pImage)[i/2] & 0xf0) / 16;
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorTable( GDALColorTable *poNewCT )

{
    if( poCT )
        delete poCT;
    if( poNewCT == nullptr )
        poCT = nullptr;
    else
        poCT = poNewCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *LAN4BitRasterBand::GetColorTable()

{
    if( poCT != nullptr )
        return poCT;

    return GDALPamRasterBand::GetColorTable();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorInterpretation( GDALColorInterp eNewInterp )

{
    eInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp LAN4BitRasterBand::GetColorInterpretation()

{
    return eInterp;
}

/************************************************************************/
/* ==================================================================== */
/*                              LANDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             LANDataset()                             */
/************************************************************************/

LANDataset::LANDataset() :
    fpImage(nullptr)
{
    memset( pachHeader, 0, sizeof(pachHeader) );
    adfGeoTransform[0] =  0.0;
    adfGeoTransform[1] =  0.0;  // TODO(schwehr): Should this be 1.0?
    adfGeoTransform[2] =  0.0;
    adfGeoTransform[3] =  0.0;
    adfGeoTransform[4] =  0.0;
    adfGeoTransform[5] =  0.0;  // TODO(schwehr): Should this be 1.0?
}

/************************************************************************/
/*                            ~LANDataset()                             */
/************************************************************************/

LANDataset::~LANDataset()

{
    FlushCache(true);

    if( fpImage != nullptr )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        }
    }

    if( m_poSRS )
        m_poSRS->Release();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LANDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header (.pcb) file.       */
/*      Does this appear to be a pcb file?                              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < ERD_HEADER_SIZE || poOpenInfo->fpL == nullptr )
        return nullptr;

    if( !STARTS_WITH_CI( reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                         "HEADER" )
        && !STARTS_WITH_CI( reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                            "HEAD74" ) )
        return nullptr;

    if( memcmp(poOpenInfo->pabyHeader + 16, "S LAT   ", 8) == 0 )
    {
        // NTV1 format
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LANDataset *poDS = new LANDataset();

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Do we need to byte swap the headers to local machine order?     */
/* -------------------------------------------------------------------- */
    int bBigEndian = poOpenInfo->pabyHeader[8] == 0;

    memcpy( poDS->pachHeader, poOpenInfo->pabyHeader, ERD_HEADER_SIZE );

#ifdef CPL_LSB
    const int bNeedSwap = bBigEndian;
#else
    const int bNeedSwap = !bBigEndian;
#endif

    if( bNeedSwap )
    {
        CPL_SWAP16PTR( poDS->pachHeader + 6 );
        CPL_SWAP16PTR( poDS->pachHeader + 8 );

        CPL_SWAP32PTR( poDS->pachHeader + 16 );
        CPL_SWAP32PTR( poDS->pachHeader + 20 );
        CPL_SWAP32PTR( poDS->pachHeader + 24 );
        CPL_SWAP32PTR( poDS->pachHeader + 28 );

        CPL_SWAP16PTR( poDS->pachHeader + 88 );
        CPL_SWAP16PTR( poDS->pachHeader + 90 );

        CPL_SWAP16PTR( poDS->pachHeader + 106 );
        CPL_SWAP32PTR( poDS->pachHeader + 108 );
        CPL_SWAP32PTR( poDS->pachHeader + 112 );
        CPL_SWAP32PTR( poDS->pachHeader + 116 );
        CPL_SWAP32PTR( poDS->pachHeader + 120 );
        CPL_SWAP32PTR( poDS->pachHeader + 124 );
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(poDS->pachHeader,"HEADER") )
    {
        float fTmp = 0.0;
        memcpy(&fTmp, poDS->pachHeader + 16, 4);
        poDS->nRasterXSize = static_cast<int>(fTmp);
        memcpy(&fTmp, poDS->pachHeader + 20, 4);
        poDS->nRasterYSize = static_cast<int>(fTmp);
    }
    else
    {
        GInt32 nTmp = 0;
        memcpy(&nTmp, poDS->pachHeader + 16, 4);
        poDS->nRasterXSize = nTmp;
        memcpy(&nTmp, poDS->pachHeader + 20, 4);
        poDS->nRasterYSize = nTmp;
    }

    GInt16 nTmp16 = 0;
    memcpy(&nTmp16, poDS->pachHeader + 6, 2);

    int nPixelOffset = 0;
    GDALDataType eDataType = GDT_Unknown;
    if( nTmp16 == 0 )
    {
        eDataType = GDT_Byte;
        nPixelOffset = 1;
    }
    else if( nTmp16 == 1 )  // 4 bit
    {
        eDataType = GDT_Byte;
        nPixelOffset = -1;
    }
    else if( nTmp16 == 2 )
    {
        nPixelOffset = 2;
        eDataType = GDT_Int16;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported pixel type (%d).",
                  nTmp16 );

        delete poDS;
        return nullptr;
    }

    memcpy(&nTmp16, poDS->pachHeader + 8, 2);
    const int nBandCount = nTmp16;

    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandCount, FALSE) )
    {
        delete poDS;
        return nullptr;
    }

    // cppcheck-suppress knownConditionTrueFalse
    if( nPixelOffset != -1 &&
        poDS->nRasterXSize > INT_MAX / (nPixelOffset * nBandCount) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Int overflow occurred." );
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    for( int iBand = 1; iBand <= nBandCount; iBand++ )
    {
        if( nPixelOffset == -1 ) /* 4 bit case */
            poDS->SetBand( iBand,
                           new LAN4BitRasterBand( poDS, iBand ) );
        else
            poDS->SetBand(
                iBand,
                new RawRasterBand( poDS, iBand, poDS->fpImage,
                                   ERD_HEADER_SIZE + (iBand-1)
                                   * nPixelOffset * poDS->nRasterXSize,
                                   nPixelOffset,
                                   poDS->nRasterXSize*nPixelOffset*nBandCount,
                                   eDataType, !bNeedSwap,
                                   RawRasterBand::OwnFP::NO ));
        if( CPLGetLastErrorType() != CE_None )
        {
            delete poDS;
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->CheckForStatistics();
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Try to interpret georeferencing.                                */
/* -------------------------------------------------------------------- */
    float fTmp = 0.0;

    memcpy(&fTmp, poDS->pachHeader + 112, 4);
    poDS->adfGeoTransform[0] = fTmp;
    memcpy(&fTmp, poDS->pachHeader + 120, 4);
    poDS->adfGeoTransform[1] = fTmp;
    poDS->adfGeoTransform[2] = 0.0;
    memcpy(&fTmp, poDS->pachHeader + 116, 4);
    poDS->adfGeoTransform[3] = fTmp;
    poDS->adfGeoTransform[4] = 0.0;
    memcpy(&fTmp, poDS->pachHeader + 124, 4);
    poDS->adfGeoTransform[5] = - fTmp;

    // adjust for center of pixel vs. top left corner of pixel.
    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;

/* -------------------------------------------------------------------- */
/*      If we didn't get any georeferencing, try for a worldfile.       */
/* -------------------------------------------------------------------- */
    if( poDS->adfGeoTransform[1] == 0.0
        || poDS->adfGeoTransform[5] == 0.0 )
    {
        if( !GDALReadWorldFile( poOpenInfo->pszFilename, nullptr,
                                poDS->adfGeoTransform ) )
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld",
                               poDS->adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Try to come up with something for the coordinate system.        */
/* -------------------------------------------------------------------- */
    memcpy(&nTmp16, poDS->pachHeader + 88, 2);
    int nCoordSys = nTmp16;

    poDS->m_poSRS = new OGRSpatialReference();
    poDS->m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( nCoordSys == 0 )
    {
        poDS->m_poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    }
    else if( nCoordSys == 1 )
    {
        poDS->m_poSRS->SetFromUserInput(
            "LOCAL_CS[\"UTM - Zone Unknown\",UNIT[\"Meter\",1]]" );
    }
    else if( nCoordSys == 2 )
    {
        poDS->m_poSRS->SetFromUserInput(
                "LOCAL_CS[\"State Plane - Zone Unknown\","
                "UNIT[\"US survey foot\",0.3048006096012192]]" );
    }
    else
    {
        poDS->m_poSRS->SetFromUserInput(
            "LOCAL_CS[\"Unknown\",UNIT[\"Meter\",1]]" );
    }

/* -------------------------------------------------------------------- */
/*      Check for a trailer file with a colormap in it.                 */
/* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup( CPLGetPath(poOpenInfo->pszFilename) );
    char *pszBasename = CPLStrdup( CPLGetBasename(poOpenInfo->pszFilename) );
    const char *pszTRLFilename =
        CPLFormCIFilename( pszPath, pszBasename, "trl" );
    VSILFILE *fpTRL = VSIFOpenL( pszTRLFilename, "rb" );
    if( fpTRL != nullptr )
    {
        char szTRLData[896] = { '\0' };

        CPL_IGNORE_RET_VAL(VSIFReadL( szTRLData, 1, 896, fpTRL ));
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpTRL ));

        GDALColorTable *poCT = new GDALColorTable();
        for( int iColor = 0; iColor < 256; iColor++ )
        {
            GDALColorEntry sEntry = { 0, 0, 0, 0};

            sEntry.c2 = reinterpret_cast<GByte *>(szTRLData)[iColor+128];
            sEntry.c1 = reinterpret_cast<GByte *>(szTRLData)[iColor+128+256];
            sEntry.c3 = reinterpret_cast<GByte *>(szTRLData)[iColor+128+512];
            sEntry.c4 = 255;
            poCT->SetColorEntry( iColor, &sEntry );

            // Only 16 colors in 4bit files.
            if( nPixelOffset == -1 && iColor == 15 )
                break;
        }

        poDS->GetRasterBand(1)->SetColorTable( poCT );
        poDS->GetRasterBand(1)->SetColorInterpretation( GCI_PaletteIndex );

        delete poCT;
    }

    CPLFree( pszPath );
    CPLFree( pszBasename );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::GetGeoTransform( double * padfTransform )

{
    if( adfGeoTransform[1] != 0.0 && adfGeoTransform[5] != 0.0 )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::SetGeoTransform( double * padfTransform )

{
    unsigned char abyHeader[128] = { '\0' };

    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFReadL( abyHeader, 128, 1, fpImage ));

    // Upper Left X.
    float f32Val = static_cast<float>(
        adfGeoTransform[0] + 0.5 * adfGeoTransform[1] );
    memcpy( abyHeader + 112, &f32Val, 4 );

    // Upper Left Y.
    f32Val = static_cast<float>(
        adfGeoTransform[3] + 0.5 * adfGeoTransform[5] );
    memcpy( abyHeader + 116, &f32Val, 4 );

    // Width of pixel.
    f32Val = static_cast<float>( adfGeoTransform[1] );
    memcpy( abyHeader + 120, &f32Val, 4 );

    // Height of pixel.
    f32Val = static_cast<float>( std::abs( adfGeoTransform[5] ) );
    memcpy( abyHeader + 124, &f32Val, 4 );

    if( VSIFSeekL( fpImage, 0, SEEK_SET ) != 0
        || VSIFWriteL( abyHeader, 128, 1, fpImage ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "File IO Error writing header with new geotransform." );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/*                                                                      */
/*      Use PAM coordinate system if available in preference to the     */
/*      generally poor value derived from the file itself.              */
/************************************************************************/

const OGRSpatialReference *LANDataset::GetSpatialRef() const

{
    const auto poSRS = GDALPamDataset::GetSpatialRef();
    if( poSRS )
        return poSRS;

    return m_poSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr LANDataset::SetSpatialRef( const OGRSpatialReference * poSRS )

{
    if( poSRS == nullptr )
        return GDALPamDataset::SetSpatialRef( poSRS );

    unsigned char abyHeader[128] = { '\0' };

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFReadL( abyHeader, 128, 1, fpImage ));

    GUInt16 nProjCode = 0;

    if( poSRS->IsGeographic() )
    {
        nProjCode = 0;
    }
    else if( poSRS->GetUTMZone() != 0 )
    {
        nProjCode = 1;
    }
    // Too bad we have no way of recognising state plane projections.
    else
    {
        const char *l_pszProjection = poSRS->GetAttrValue("PROJECTION");

        if( l_pszProjection == nullptr )
            ;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
            nProjCode = 3;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
            nProjCode = 4;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_MERCATOR_1SP) )
            nProjCode = 5;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_POLAR_STEREOGRAPHIC) )
            nProjCode = 6;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_POLYCONIC) )
            nProjCode = 7;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_EQUIDISTANT_CONIC) )
            nProjCode = 8;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_TRANSVERSE_MERCATOR) )
            nProjCode = 9;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_STEREOGRAPHIC) )
            nProjCode = 10;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
            nProjCode = 11;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_AZIMUTHAL_EQUIDISTANT) )
            nProjCode = 12;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_GNOMONIC) )
            nProjCode = 13;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_ORTHOGRAPHIC) )
            nProjCode = 14;
        // We do not have GVNP.
        else if( EQUAL(l_pszProjection,
                       SRS_PT_SINUSOIDAL) )
            nProjCode = 16;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_EQUIRECTANGULAR) )
            nProjCode = 17;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_MILLER_CYLINDRICAL) )
            nProjCode = 18;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_VANDERGRINTEN) )
            nProjCode = 19;
        else if( EQUAL(l_pszProjection,
                       SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
            nProjCode = 20;
    }

    memcpy( abyHeader + 88, &nProjCode, 2 );

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFWriteL( abyHeader, 128, 1, fpImage ));

    return GDALPamDataset::SetSpatialRef( poSRS );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **LANDataset::GetFileList()

{
    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    if( !osSTAFilename.empty() )
        papszFileList = CSLAddString( papszFileList, osSTAFilename );

    return papszFileList;
}

/************************************************************************/
/*                         CheckForStatistics()                         */
/************************************************************************/

void LANDataset::CheckForStatistics()

{
/* -------------------------------------------------------------------- */
/*      Do we have a statistics file?                                   */
/* -------------------------------------------------------------------- */
    osSTAFilename = CPLResetExtension(GetDescription(),"sta");

    VSILFILE *fpSTA = VSIFOpenL( osSTAFilename, "r" );

    if( fpSTA == nullptr && VSIIsCaseSensitiveFS(osSTAFilename) )
    {
        osSTAFilename = CPLResetExtension(GetDescription(),"STA");
        fpSTA = VSIFOpenL( osSTAFilename, "r" );
    }

    if( fpSTA == nullptr )
    {
        osSTAFilename = "";
        return;
    }

/* -------------------------------------------------------------------- */
/*      Read it one band at a time.                                     */
/* -------------------------------------------------------------------- */
    GByte abyBandInfo[1152] = { '\0' };

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if( VSIFReadL( abyBandInfo, 1152, 1, fpSTA ) != 1 )
            break;

        const int nBandNumber = abyBandInfo[7];
        GDALRasterBand *poBand = GetRasterBand(nBandNumber);
        if( poBand == nullptr )
            break;

        GInt16 nMin = 0;
        GInt16 nMax = 0;

        if( poBand->GetRasterDataType() != GDT_Byte )
        {
            memcpy( &nMin, abyBandInfo + 28, 2 );
            memcpy( &nMax, abyBandInfo + 30, 2 );
            CPL_LSBPTR16( &nMin );
            CPL_LSBPTR16( &nMax );
        }
        else
        {
            nMin = abyBandInfo[9];
            nMax = abyBandInfo[8];
        }

        float fMean = 0.0;
        float fStdDev = 0.0;
        memcpy( &fMean, abyBandInfo + 12, 4 );
        memcpy( &fStdDev, abyBandInfo + 24, 4 );
        CPL_LSBPTR32( &fMean );
        CPL_LSBPTR32( &fStdDev );

        poBand->SetStatistics( nMin, nMax, fMean, fStdDev );
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL( fpSTA ));
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *LANDataset::Create( const char * pszFilename,
                                 int nXSize,
                                 int nYSize,
                                 int nBandsIn,
                                 GDALDataType eType,
                                 char ** /* papszOptions */ )
{
    if( eType != GDT_Byte && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create .GIS file with unsupported data type '%s'.",
                  GDALGetDataTypeName( eType ) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    unsigned char abyHeader[128] = { '\0' };

    memset( abyHeader, 0, sizeof(abyHeader) );

    memcpy( abyHeader + 0, "HEAD74", 6 );

    // Pixel type.
    GInt16  n16Val = 0;
    if( eType == GDT_Byte ) // Do we want 4bit?
        n16Val = 0;
    else
        n16Val = 2;
    memcpy( abyHeader + 6, &n16Val, 2 );

    // Number of Bands.
    n16Val = static_cast<GInt16>( nBandsIn );
    memcpy( abyHeader + 8, &n16Val, 2 );

    // Unknown (6).

    // Width.
    GInt32  n32Val = nXSize;
    memcpy( abyHeader + 16, &n32Val, 4 );

    // Height.
    n32Val = nYSize;
    memcpy( abyHeader + 20, &n32Val, 4 );

    // X Start (4).
    // Y Start (4).

    // Unknown (56).

    // Coordinate System.
    n16Val = 0;
    memcpy( abyHeader + 88, &n16Val, 2 );

    // Classes in coverage.
    n16Val = 0;
    memcpy( abyHeader + 90, &n16Val, 2 );

    // Unknown (14).

    // Area Unit.
    n16Val = 0;
    memcpy( abyHeader + 106, &n16Val, 2 );

    // Pixel Area.
    float f32Val = 0.0f;
    memcpy( abyHeader + 108, &f32Val, 4 );

    // Upper Left X.
    f32Val = 0.5f;
    memcpy( abyHeader + 112, &f32Val, 4 );

    // Upper Left Y
    f32Val = static_cast<float>(nYSize - 0.5);
    memcpy( abyHeader + 116, &f32Val, 4 );

    // Width of pixel.
    f32Val = 1.0f;
    memcpy( abyHeader + 120, &f32Val, 4 );

    // Height of pixel.
    f32Val = 1.0f;
    memcpy( abyHeader + 124, &f32Val, 4 );

    CPL_IGNORE_RET_VAL(VSIFWriteL( abyHeader, sizeof(abyHeader), 1, fp ));

/* -------------------------------------------------------------------- */
/*      Extend the file to the target size.                             */
/* -------------------------------------------------------------------- */
    vsi_l_offset nImageBytes = 0;

    if( eType != GDT_Byte )
        nImageBytes = nXSize * static_cast<vsi_l_offset>( nYSize ) * 2;
    else
        nImageBytes = nXSize * static_cast<vsi_l_offset>( nYSize );

    memset( abyHeader, 0, sizeof(abyHeader) );

    while( nImageBytes > 0 )
    {
        const vsi_l_offset nWriteThisTime
            = std::min( static_cast<size_t>( nImageBytes ), sizeof(abyHeader) );

        if( VSIFWriteL( abyHeader, 1, static_cast<size_t>(nWriteThisTime), fp )
            != nWriteThisTime )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to write whole Istar file." );
            return nullptr;
        }
        nImageBytes -= nWriteThisTime;
    }

    if( VSIFCloseL( fp ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write whole Istar file." );
        return nullptr;
    }

    return static_cast<GDALDataset *>( GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                          GDALRegister_LAN()                          */
/************************************************************************/

void GDALRegister_LAN()

{
    if( GDALGetDriverByName( "LAN" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "LAN" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Erdas .LAN/.GIS" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/lan.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16" );

    poDriver->pfnOpen = LANDataset::Open;
    poDriver->pfnCreate = LANDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
