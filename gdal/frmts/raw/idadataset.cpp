/******************************************************************************
 *
 * Project:  IDA Raster Driver
 * Purpose:  Implemenents IDA driver/dataset/rasterband.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_priv.h"  // Must be first.

#include "gdal_frmts.h"
#include "gdal_rat.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                tp2c()                                */
/*                                                                      */
/*      convert a Turbo Pascal real into a double                       */
/************************************************************************/

static double tp2c( GByte *r )
{
    // Handle 0 case.
    if( r[0] == 0 )
        return 0.0;

    // Extract sign: bit 7 of byte 5.
    const int sign = (r[5] & 0x80) ? -1 : 1;

    // Extract mantissa from first bit of byte 1 to bit 7 of byte 5.
    double mant = 0.0;
    for ( int i = 1; i < 5; i++ )
      mant = (r[i] + mant) / 256;
    mant = (mant + (r[5] & 0x7F)) / 128 + 1;

    // Extract exponent.
    const int exp = r[0] - 129;

    // Compute the damned number.
    return sign * ldexp(mant, exp);
}

/************************************************************************/
/*                                c2tp()                                */
/*                                                                      */
/*      convert a double into a Turbo Pascal real                       */
/************************************************************************/

static void c2tp( double x, GByte *r )
{
    // Handle 0 case.
    if (x == 0.0)
    {
      // TODO(schwehr): memset.
      for (int i = 0; i < 6; r[i++] = 0);
      return;
    }

    // Compute mantissa, sign and exponent.
    int exp = 0;
    double mant = frexp(x, &exp) * 2 - 1;
    exp--;
    int negative = 0;
    if( mant < 0 )
    {
      mant = -mant;
      negative = 1;
    }

    // Stuff mantissa into Turbo Pascal real.
    double temp = 0.0;
    mant = modf(mant * 128, &temp);
    r[5] = static_cast<unsigned char>( static_cast<int>(temp) & 0xff);
    for( int i = 4; i >= 1; i-- )
    {
      mant = modf(mant * 256, &temp);
      r[i] = static_cast<unsigned char>( temp );
    }

    // Add sign.
    if( negative )
    r[5] |= 0x80;

    // Put exponent.
    r[0] = static_cast<GByte>( exp + 129 );
}

/************************************************************************/
/* ==================================================================== */
/*                              IDADataset                              */
/* ==================================================================== */
/************************************************************************/

class IDADataset final: public RawDataset
{
    friend class IDARasterBand;

    int         nImageType;
    int         nProjection;
    char        szTitle[81];
    double      dfLatCenter;
    double      dfLongCenter;
    double      dfXCenter;
    double      dfYCenter;
    double      dfDX;
    double      dfDY;
    double      dfParallel1;
    double      dfParallel2;
    int         nMissing;
    double      dfM;
    double      dfB;

    VSILFILE   *fpRaw;

    OGRSpatialReference* m_poSRS = nullptr;
    double      adfGeoTransform[6];

    void        ProcessGeoref();

    GByte       abyHeader[512];
    bool        bHeaderDirty;

    void        ReadColorTable();

    CPL_DISALLOW_COPY_ASSIGN(IDADataset)

  public:
    IDADataset();
    ~IDADataset() override;

    void FlushCache() override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char ** /* papszParamList */ );
};

/************************************************************************/
/* ==================================================================== */
/*                              IDARasterBand                           */
/* ==================================================================== */
/************************************************************************/

class IDARasterBand final: public RawRasterBand
{
    friend class IDADataset;

    GDALRasterAttributeTable *poRAT;
    GDALColorTable       *poColorTable;

    CPL_DISALLOW_COPY_ASSIGN(IDARasterBand)

  public:
    IDARasterBand( IDADataset *poDSIn, VSILFILE *fpRaw, int nXSize );
    ~IDARasterBand() override;

    GDALRasterAttributeTable *GetDefaultRAT() override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    double GetOffset( int *pbSuccess = nullptr ) override;
    CPLErr SetOffset( double dfNewValue ) override;
    double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double dfNewValue ) override;
    double GetNoDataValue( int *pbSuccess = nullptr ) override;
};

/************************************************************************/
/*                            IDARasterBand                             */
/************************************************************************/

IDARasterBand::IDARasterBand( IDADataset *poDSIn,
                              VSILFILE *fpRawIn, int nXSize ) :
    RawRasterBand( poDSIn, 1, fpRawIn, 512, 1, nXSize,
                   GDT_Byte, FALSE, RawRasterBand::OwnFP::NO ),
    poRAT(nullptr),
    poColorTable(nullptr)
{}

/************************************************************************/
/*                           ~IDARasterBand()                           */
/************************************************************************/

IDARasterBand::~IDARasterBand()

{
    if( poColorTable )
        delete poColorTable;
    if( poRAT )
        delete poRAT;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IDARasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    return reinterpret_cast<IDADataset *>( poDS )->nMissing;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double IDARasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    return reinterpret_cast<IDADataset *>( poDS )->dfB;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr IDARasterBand::SetOffset( double dfNewValue )

{
    IDADataset *poIDS = reinterpret_cast<IDADataset *>( poDS );

    if( dfNewValue == poIDS->dfB )
        return CE_None;

    if( poIDS->nImageType != 200 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Setting explicit offset only support for image type 200.");
        return CE_Failure;
    }

    poIDS->dfB = dfNewValue;
    c2tp( dfNewValue, poIDS->abyHeader + 177 );
    poIDS->bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double IDARasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    return reinterpret_cast<IDADataset *>( poDS )->dfM;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr IDARasterBand::SetScale( double dfNewValue )

{
    IDADataset *poIDS = reinterpret_cast<IDADataset *>( poDS );

    if( dfNewValue == poIDS->dfM )
        return CE_None;

    if( poIDS->nImageType != 200 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Setting explicit scale only support for image type 200." );
        return CE_Failure;
    }

    poIDS->dfM = dfNewValue;
    c2tp( dfNewValue, poIDS->abyHeader + 171 );
    poIDS->bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *IDARasterBand::GetColorTable()

{
    if( poColorTable )
        return poColorTable;

    return RawRasterBand::GetColorTable();
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp IDARasterBand::GetColorInterpretation()

{
    if( poColorTable )
        return GCI_PaletteIndex;

    return RawRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *IDARasterBand::GetDefaultRAT()

{
    if( poRAT )
        return poRAT;

    return RawRasterBand::GetDefaultRAT();
}

/************************************************************************/
/* ==================================================================== */
/*                              IDADataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             IDADataset()                             */
/************************************************************************/

IDADataset::IDADataset() :
    nImageType(0),
    nProjection(0),
    dfLatCenter(0.0),
    dfLongCenter(0.0),
    dfXCenter(0.0),
    dfYCenter(0.0),
    dfDX(0.0),
    dfDY(0.0),
    dfParallel1(0.0),
    dfParallel2(0.0),
    nMissing(0),
    dfM(0.0),
    dfB(0.0),
    fpRaw(nullptr),
    bHeaderDirty(false)
{
    memset( szTitle, 0, sizeof(szTitle) );
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    memset( abyHeader, 0, sizeof(abyHeader) );
}

/************************************************************************/
/*                            ~IDADataset()                             */
/************************************************************************/

IDADataset::~IDADataset()

{
    IDADataset::FlushCache();

    if( fpRaw != nullptr )
    {
        if( VSIFCloseL( fpRaw ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        }
    }
    if( m_poSRS )
        m_poSRS->Release();
}

/************************************************************************/
/*                           ProcessGeoref()                            */
/************************************************************************/

void IDADataset::ProcessGeoref()

{
    OGRSpatialReference oSRS;

    if( nProjection == 3 )
    {
        oSRS.SetWellKnownGeogCS( "WGS84" );
    }
    else if( nProjection == 4 )
    {
        oSRS.SetLCC( dfParallel1, dfParallel2,
                     dfLatCenter, dfLongCenter,
                     0.0, 0.0 );
        oSRS.SetGeogCS( "Clarke 1866", "Clarke 1866", "Clarke 1866",
                        6378206.4, 293.97869821389662 );
    }
    else if( nProjection == 6 )
    {
        oSRS.SetLAEA( dfLatCenter, dfLongCenter, 0.0, 0.0 );
        oSRS.SetGeogCS( "Sphere", "Sphere", "Sphere",
                        6370997.0, 0.0 );
    }
    else if( nProjection == 8 )
    {
        oSRS.SetACEA( dfParallel1, dfParallel2,
                      dfLatCenter, dfLongCenter,
                      0.0, 0.0 );
        oSRS.SetGeogCS( "Clarke 1866", "Clarke 1866", "Clarke 1866",
                        6378206.4, 293.97869821389662 );
    }
    else if( nProjection == 9 )
    {
        oSRS.SetGH( dfLongCenter, 0.0, 0.0 );
        oSRS.SetGeogCS( "Sphere", "Sphere", "Sphere",
                        6370997.0, 0.0 );
    }

    if( !oSRS.IsEmpty() )
    {
        if( m_poSRS )
            m_poSRS->Release();
        m_poSRS = oSRS.Clone();
    }

    adfGeoTransform[0] = 0 - dfDX * dfXCenter;
    adfGeoTransform[1] = dfDX;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] =  dfDY * dfYCenter;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfDY;

    if( nProjection == 3 )
    {
        adfGeoTransform[0] += dfLongCenter;
        adfGeoTransform[3] += dfLatCenter;
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void IDADataset::FlushCache()

{
    RawDataset::FlushCache();

    if( bHeaderDirty )
    {
        CPL_IGNORE_RET_VAL(VSIFSeekL( fpRaw, 0, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFWriteL( abyHeader, 512, 1, fpRaw ));
        bHeaderDirty = false;
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr IDADataset::GetGeoTransform( double *padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr IDADataset::SetGeoTransform( double *padfGeoTransform )

{
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0 )
        return GDALPamDataset::SetGeoTransform( padfGeoTransform );

    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double) * 6 );
    bHeaderDirty = true;

    dfDX = adfGeoTransform[1];
    dfDY = -adfGeoTransform[5];
    dfXCenter = -adfGeoTransform[0] / dfDX;
    dfYCenter = adfGeoTransform[3] / dfDY;

    c2tp( dfDX, abyHeader + 144 );
    c2tp( dfDY, abyHeader + 150 );
    c2tp( dfXCenter, abyHeader + 132 );
    c2tp( dfYCenter, abyHeader + 138 );

    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *IDADataset::GetSpatialRef() const

{
    return m_poSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr IDADataset::SetSpatialRef( const OGRSpatialReference* poSRS )

{
    if( !poSRS || (!poSRS->IsGeographic() && !poSRS->IsProjected()) )
        return GDALPamDataset::SetSpatialRef( poSRS );

/* -------------------------------------------------------------------- */
/*      Clear projection parameters.                                    */
/* -------------------------------------------------------------------- */
    dfParallel1 = 0.0;
    dfParallel2 = 0.0;
    dfLatCenter = 0.0;
    dfLongCenter = 0.0;

/* -------------------------------------------------------------------- */
/*      Geographic.                                                     */
/* -------------------------------------------------------------------- */
    if( poSRS->IsGeographic() )
    {
        // If no change, just return.
        if( nProjection == 3 )
            return CE_None;

        nProjection = 3;
    }

/* -------------------------------------------------------------------- */
/*      Verify we don't have a false easting or northing as these       */
/*      will be ignored for the projections we do support.              */
/* -------------------------------------------------------------------- */
    if( poSRS->GetProjParm( SRS_PP_FALSE_EASTING ) != 0.0
        || poSRS->GetProjParm( SRS_PP_FALSE_NORTHING ) != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to set a projection on an IDA file with a non-zero "
                  "false easting and/or northing.  This is not supported." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Lambert Conformal Conic.  Note that we don't support false      */
/*      eastings or nothings.                                           */
/* -------------------------------------------------------------------- */
    const char *l_pszProjection = poSRS->GetAttrValue( "PROJECTION" );

    if( l_pszProjection == nullptr )
    {
        /* do nothing - presumably geographic  */;
    }
    else if( EQUAL(l_pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        nProjection = 4;
        dfParallel1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        dfParallel2 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        dfLatCenter = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(l_pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        nProjection = 6;
        dfLatCenter = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(l_pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        nProjection = 8;
        dfParallel1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        dfParallel2 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        dfLatCenter = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(l_pszProjection, SRS_PT_GOODE_HOMOLOSINE) )
    {
        nProjection = 9;
        dfLongCenter = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
    }
    else
    {
        return GDALPamDataset::SetSpatialRef( poSRS );
    }

/* -------------------------------------------------------------------- */
/*      Update header and mark it as dirty.                             */
/* -------------------------------------------------------------------- */
    bHeaderDirty = true;

    abyHeader[23] = static_cast<GByte>( nProjection );
    c2tp( dfLatCenter, abyHeader + 120 );
    c2tp( dfLongCenter, abyHeader + 126 );
    c2tp( dfParallel1, abyHeader + 156 );
    c2tp( dfParallel2, abyHeader + 162 );

    return CE_None;
}

/************************************************************************/
/*                           ReadColorTable()                           */
/************************************************************************/

void IDADataset::ReadColorTable()

{
/* -------------------------------------------------------------------- */
/*      Decide what .clr file to look for and try to open.              */
/* -------------------------------------------------------------------- */
    CPLString osCLRFilename = CPLGetConfigOption( "IDA_COLOR_FILE", "" );
    if( osCLRFilename.empty() )
        osCLRFilename = CPLResetExtension(GetDescription(), "clr" );

    VSILFILE *fp = VSIFOpenL( osCLRFilename, "r" );
    if( fp == nullptr )
    {
        osCLRFilename = CPLResetExtension(osCLRFilename, "CLR" );
        fp = VSIFOpenL( osCLRFilename, "r" );
    }

    if( fp == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Skip first line, with the column titles.                        */
/* -------------------------------------------------------------------- */
    CPLReadLineL( fp );

/* -------------------------------------------------------------------- */
/*      Create a RAT to populate.                                       */
/* -------------------------------------------------------------------- */
    GDALRasterAttributeTable *poRAT = new GDALDefaultRasterAttributeTable();

    poRAT->CreateColumn( "FROM", GFT_Integer, GFU_Min );
    poRAT->CreateColumn( "TO", GFT_Integer, GFU_Max );
    poRAT->CreateColumn( "RED", GFT_Integer, GFU_Red );
    poRAT->CreateColumn( "GREEN", GFT_Integer, GFU_Green );
    poRAT->CreateColumn( "BLUE", GFT_Integer, GFU_Blue );
    poRAT->CreateColumn( "LEGEND", GFT_String, GFU_Name );

/* -------------------------------------------------------------------- */
/*      Apply lines.                                                    */
/* -------------------------------------------------------------------- */
    const char *pszLine = CPLReadLineL( fp );
    int iRow = 0;

    while( pszLine != nullptr )
    {
        char **papszTokens =
            CSLTokenizeStringComplex( pszLine, " \t", FALSE, FALSE );

        if( CSLCount( papszTokens ) >= 5 )
        {
            poRAT->SetValue( iRow, 0, atoi(papszTokens[0]) );
            poRAT->SetValue( iRow, 1, atoi(papszTokens[1]) );
            poRAT->SetValue( iRow, 2, atoi(papszTokens[2]) );
            poRAT->SetValue( iRow, 3, atoi(papszTokens[3]) );
            poRAT->SetValue( iRow, 4, atoi(papszTokens[4]) );

            // Find name, first nonspace after 5th token.
            const char *pszName = pszLine;

            // Skip from.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // Skip to.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // Skip red.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // Skip green.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // Skip blue.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // Skip pre-name white space.
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;

            poRAT->SetValue( iRow, 5, pszName );

            iRow++;
        }

        CSLDestroy( papszTokens );
        pszLine = CPLReadLineL( fp );
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Attach RAT to band.                                             */
/* -------------------------------------------------------------------- */
    reinterpret_cast<IDARasterBand *>( GetRasterBand( 1 ) )->poRAT = poRAT;

/* -------------------------------------------------------------------- */
/*      Build a conventional color table from this.                     */
/* -------------------------------------------------------------------- */
    reinterpret_cast<IDARasterBand *>( GetRasterBand( 1 ) )->poColorTable =
        poRAT->TranslateToColorTable();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IDADataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this an IDA file?                                            */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fpL == nullptr )
        return nullptr;

    if( poOpenInfo->nHeaderBytes < 512 )
        return nullptr;

    // Projection legal?
    if( poOpenInfo->pabyHeader[23] > 10 )
        return nullptr;

    // Image type legal?
    if( (poOpenInfo->pabyHeader[22] > 14
         && poOpenInfo->pabyHeader[22] < 100)
        || (poOpenInfo->pabyHeader[22] > 114
            && poOpenInfo->pabyHeader[22] != 200 ) )
        return nullptr;

    const int nXSize
        = poOpenInfo->pabyHeader[30] + poOpenInfo->pabyHeader[31] * 256;
    const int nYSize
        = poOpenInfo->pabyHeader[32] + poOpenInfo->pabyHeader[33] * 256;

    if( nXSize == 0 || nYSize == 0 )
        return nullptr;

    // The file just be exactly the image size + header size in length.
    const vsi_l_offset nExpectedFileSize =
        static_cast<vsi_l_offset>(nXSize) * nYSize + 512;

    CPL_IGNORE_RET_VAL(VSIFSeekL( poOpenInfo->fpL, 0, SEEK_END ));
    const vsi_l_offset nActualFileSize = VSIFTellL( poOpenInfo->fpL );
    VSIRewindL( poOpenInfo->fpL );

    if( nActualFileSize != nExpectedFileSize )
        return nullptr;

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("IDA") )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    IDADataset *poDS = new IDADataset();

    memcpy( poDS->abyHeader, poOpenInfo->pabyHeader, 512 );

/* -------------------------------------------------------------------- */
/*      Parse various values out of the header.                         */
/* -------------------------------------------------------------------- */
    poDS->nImageType = poOpenInfo->pabyHeader[22];
    poDS->nProjection = poOpenInfo->pabyHeader[23];

    poDS->nRasterYSize = poOpenInfo->pabyHeader[30]
        + poOpenInfo->pabyHeader[31] * 256;
    poDS->nRasterXSize = poOpenInfo->pabyHeader[32]
        + poOpenInfo->pabyHeader[33] * 256;

    strncpy( poDS->szTitle,
             reinterpret_cast<char *>( poOpenInfo->pabyHeader+38 ),
             80 );
    poDS->szTitle[80] = '\0';

    int nLastTitleChar = static_cast<int>(strlen(poDS->szTitle))-1;
    while( nLastTitleChar > -1
           && (poDS->szTitle[nLastTitleChar] == 10
               || poDS->szTitle[nLastTitleChar] == 13
               || poDS->szTitle[nLastTitleChar] == ' ') )
        poDS->szTitle[nLastTitleChar--] = '\0';

    poDS->dfLatCenter = tp2c( poOpenInfo->pabyHeader + 120 );
    poDS->dfLongCenter = tp2c( poOpenInfo->pabyHeader + 126 );
    poDS->dfXCenter = tp2c( poOpenInfo->pabyHeader + 132 );
    poDS->dfYCenter = tp2c( poOpenInfo->pabyHeader + 138 );
    poDS->dfDX = tp2c( poOpenInfo->pabyHeader + 144 );
    poDS->dfDY = tp2c( poOpenInfo->pabyHeader + 150 );
    poDS->dfParallel1 = tp2c( poOpenInfo->pabyHeader + 156 );
    poDS->dfParallel2 = tp2c( poOpenInfo->pabyHeader + 162 );

    poDS->ProcessGeoref();

    poDS->SetMetadataItem( "TITLE", poDS->szTitle );

/* -------------------------------------------------------------------- */
/*      Handle various image types.                                     */
/* -------------------------------------------------------------------- */

// GENERIC = 0
// GENERIC DIFF = 100

    poDS->nMissing = 0;

    switch( poDS->nImageType )
    {
      case 1:
        poDS->SetMetadataItem( "IMAGETYPE", "1, FEWS NDVI" );
        poDS->dfM = 1/256.0;
        poDS->dfB = -82/256.0;
        break;

      case 6:
        poDS->SetMetadataItem( "IMAGETYPE", "6, EROS NDVI" );
        poDS->dfM = 1/100.0;
        poDS->dfB = -100/100.0;
        break;

      case 10:
        poDS->SetMetadataItem( "IMAGETYPE", "10, ARTEMIS CUTOFF" );
        poDS->dfM = 1.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 11:
        poDS->SetMetadataItem( "IMAGETYPE", "11, ARTEMIS RECODE" );
        poDS->dfM = 4.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 12: /* ANDVI */
        poDS->SetMetadataItem( "IMAGETYPE", "12, ARTEMIS NDVI" );
        poDS->dfM = 4/500.0;
        poDS->dfB = -3/500.0 - 1.0;
        poDS->nMissing = 254;
        break;

      case 13: /* AFEWS */
        poDS->SetMetadataItem( "IMAGETYPE", "13, ARTEMIS FEWS" );
        poDS->dfM = 1/256.0;
        poDS->dfB = -82/256.0;
        poDS->nMissing = 254;
        break;

      case 14: /* NEWNASA */
        poDS->SetMetadataItem( "IMAGETYPE", "13, ARTEMIS NEWNASA" );
        poDS->dfM = 0.75/250.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 101: /* NDVI_DIFF (FEW S) */
        poDS->dfM = 1/128.0;
        poDS->dfB = -1.0;
        poDS->nMissing = 0;
        break;

      case 106: /* EROS_DIFF NDVI? */
        poDS->dfM = 1/50.0;
        poDS->dfB = -128/50.0;
        poDS->nMissing = 0;
        break;

      case 110: /* CUTOFF_DIFF */
        poDS->dfM = 2.0;
        poDS->dfB = -128*2;
        poDS->nMissing = 254;
        break;

      case 111: /* RECODE_DIFF */
        poDS->dfM = 8;
        poDS->dfB = -128*8;
        poDS->nMissing = 254;
        break;

      case 112: /* ANDVI_DIFF */
        poDS->dfM = 8/1000.0;
        poDS->dfB = (-128*8)/1000.0;
        poDS->nMissing = 254;
        break;

      case 113: /* AFEWS_DIFF */
        poDS->dfM = 1/128.0;
        poDS->dfB = -1;
        poDS->nMissing = 254;
        break;

      case 114: /* NEWNASA_DIFF */
        poDS->dfM = 0.75/125.0;
        poDS->dfB = -128*poDS->dfM;
        poDS->nMissing = 254;
        break;

      case 200:
        // Calculated.
        // We use the values from the header.
        poDS->dfM = tp2c( poOpenInfo->pabyHeader + 171 );
        poDS->dfB = tp2c( poOpenInfo->pabyHeader + 177 );
        poDS->nMissing = poOpenInfo->pabyHeader[170];
        break;

      default:
        poDS->dfM = 1.0;
        poDS->dfB = 0.0;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpRaw = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    poDS->SetBand( 1, new IDARasterBand( poDS, poDS->fpRaw,
                                         poDS->nRasterXSize ) );

/* -------------------------------------------------------------------- */
/*      Check for a color table.                                        */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->ReadColorTable();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *IDADataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParamList */ )

{
    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("IDA") )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte || nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Only 1 band, Byte datasets supported for IDA format." );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE *fp = VSIFOpen( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Prepare formatted header.                                       */
/* -------------------------------------------------------------------- */
    GByte abyHeader[512] = { 0 } ;
    abyHeader[22] = 200;  // Image type - CALCULATED.
    abyHeader[23] = 0;  // Projection - NONE.
    abyHeader[30] = nYSize % 256;
    abyHeader[31] = static_cast<GByte>( nYSize / 256 );
    abyHeader[32] = nXSize % 256;
    abyHeader[33] = static_cast<GByte>( nXSize / 256 );

    abyHeader[170] = 255;  // Missing = 255
    c2tp( 1.0, abyHeader + 171 );  // Slope = 1.0
    c2tp( 0.0, abyHeader + 177 );  // Offset = 0
    abyHeader[168] = 0;  // Lower limit.
    abyHeader[169] = 254;  // Upper limit.

    // Pixel size = 1.0
    c2tp( 1.0, abyHeader + 144 );
    c2tp( 1.0, abyHeader + 150 );

    if( VSIFWrite( abyHeader, 1, 512, fp ) != 512 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "IO error writing %s.\n%s",
                  pszFilename, VSIStrerror( errno ) );
        CPL_IGNORE_RET_VAL(VSIFClose( fp ));
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Now we need to extend the file to just the right number of      */
/*      bytes for the data we have to ensure it will open again         */
/*      properly.                                                       */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( fp, nXSize * nYSize - 1, SEEK_CUR ) != 0
        || VSIFWrite( abyHeader, 1, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "IO error writing %s.\n%s",
                  pszFilename, VSIStrerror( errno ) );
        VSIFClose( fp );
        return nullptr;
    }

    if( VSIFClose( fp ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "IO error writing %s.\n%s",
                  pszFilename, VSIStrerror( errno ) );
        return nullptr;
    }

    return static_cast<GDALDataset *>( GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                         GDALRegister_IDA()                           */
/************************************************************************/

void GDALRegister_IDA()

{
    if( GDALGetDriverByName( "IDA" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "IDA" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Image Data and Analysis" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/ida.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = IDADataset::Open;
    poDriver->pfnCreate = IDADataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
