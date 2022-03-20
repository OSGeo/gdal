/******************************************************************************
 *
 * Project:  ISIS Version 3 Driver
 * Purpose:  Implementation of ISIS3Dataset
 * Author:   Trent Hare (thare@usgs.gov)
 *           Frank Warmerdam (warmerdam@pobox.com)
 *           Even Rouault (even.rouault at spatialys.com)
 *
 * NOTE: Original code authored by Trent and placed in the public domain as
 * per US government policy.  I have (within my rights) appropriated it and
 * placed it under the following license.  This is not intended to diminish
 * Trents contribution.
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2017 Hobu Inc
 * Copyright (c) 2017, Dmitry Baryshnikov <polimax@mail.ru>
 * Copyright (c) 2017, NextGIS <info@nextgis.com>
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

#include "cpl_json.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi_error.h"
#include "gdal_frmts.h"
#include "gdal_proxy.h"
#include "nasakeywordhandler.h"
#include "ogrgeojsonreader.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"
#include "vrtdataset.h"
#include "cpl_safemaths.hpp"

// For gethostname()
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <map>
#include <utility> // pair
#include <vector>

// Constants coming from ISIS3 source code
// in isis/src/base/objs/SpecialPixel/SpecialPixel.h

//There are several types of special pixels
//   *   Isis::Null Pixel has no data available
//   *   Isis::Lis Pixel was saturated on the instrument
//   *   Isis::His Pixel was saturated on the instrument
//   *   Isis::Lrs Pixel was saturated during a computation
//   *   Isis::Hrs Pixel was saturated during a computation

// 1-byte special pixel values
const unsigned char NULL1           = 0;
const unsigned char LOW_REPR_SAT1   = 0;
const unsigned char LOW_INSTR_SAT1  = 0;
const unsigned char HIGH_INSTR_SAT1 = 255;
const unsigned char HIGH_REPR_SAT1  = 255;

// 2-byte unsigned special pixel values
const unsigned short NULLU2           = 0;
const unsigned short LOW_REPR_SATU2   = 1;
const unsigned short LOW_INSTR_SATU2  = 2;
const unsigned short HIGH_INSTR_SATU2 = 65534;
const unsigned short HIGH_REPR_SATU2  = 65535;

// 2-byte signed special pixel values
const short NULL2           = -32768;
const short LOW_REPR_SAT2   = -32767;
const short LOW_INSTR_SAT2  = -32766;
const short HIGH_INSTR_SAT2 = -32765;
const short HIGH_REPR_SAT2  = -32764;

// Define 4-byte special pixel values for IEEE floating point
const float NULL4           = -3.4028226550889045e+38f; // 0xFF7FFFFB;
const float LOW_REPR_SAT4   = -3.4028228579130005e+38f; // 0xFF7FFFFC;
const float LOW_INSTR_SAT4  = -3.4028230607370965e+38f; // 0xFF7FFFFD;
const float HIGH_INSTR_SAT4 = -3.4028232635611926e+38f; // 0xFF7FFFFE;
const float HIGH_REPR_SAT4  = -3.4028234663852886e+38f; // 0xFF7FFFFF;

// Must be large enough to hold an integer
static const char* const pszSTARTBYTE_PLACEHOLDER = "!*^STARTBYTE^*!";
// Must be large enough to hold an integer
static const char* const pszLABEL_BYTES_PLACEHOLDER = "!*^LABEL_BYTES^*!";
// Must be large enough to hold an integer
static const char* const pszHISTORY_STARTBYTE_PLACEHOLDER =
                                                    "!*^HISTORY_STARTBYTE^*!";

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                             ISISDataset                              */
/* ==================================================================== */
/************************************************************************/

class ISIS3Dataset final: public RawDataset
{
    friend class ISIS3RawRasterBand;
    friend class ISISTiledBand;
    friend class ISIS3WrapperRasterBand;

    class NonPixelSection
    {
        public:
            CPLString    osSrcFilename;
            CPLString    osDstFilename; // empty for same file
            vsi_l_offset nSrcOffset;
            vsi_l_offset nSize;
            CPLString    osPlaceHolder; // empty if not same file
    };

    VSILFILE    *m_fpLabel;  // label file (only used for writing)
    VSILFILE    *m_fpImage;  // image data file. May be == fpLabel
    GDALDataset *m_poExternalDS; // external dataset (GeoTIFF)
    bool         m_bGeoTIFFAsRegularExternal; // creation only
    bool         m_bGeoTIFFInitDone; // creation only

    CPLString    m_osExternalFilename;
    bool         m_bIsLabelWritten; // creation only

    bool         m_bIsTiled;
    bool         m_bInitToNodata; // creation only

    NASAKeywordHandler m_oKeywords;

    bool        m_bGotTransform;
    double      m_adfGeoTransform[6];

    bool        m_bHasSrcNoData; // creation only
    double      m_dfSrcNoData; // creation only

    OGRSpatialReference m_oSRS;

    // creation only variables
    CPLString   m_osComment;
    CPLString   m_osLatitudeType;
    CPLString   m_osLongitudeDirection;
    CPLString   m_osTargetName;
    bool        m_bForce360;
    bool        m_bWriteBoundingDegrees;
    CPLString   m_osBoundingDegrees;

    CPLJSONObject m_oJSonLabel;
    CPLString     m_osHistory; // creation only
    bool          m_bUseSrcLabel; // creation only
    bool          m_bUseSrcMapping; // creation only
    bool          m_bUseSrcHistory; // creation only
    bool          m_bAddGDALHistory; // creation only
    CPLString     m_osGDALHistory; // creation only
    std::vector<NonPixelSection> m_aoNonPixelSections; // creation only
    CPLJSONObject m_oSrcJSonLabel; // creation only
    CPLStringList m_aosISIS3MD;
    CPLStringList m_aosAdditionalFiles;
    CPLString     m_osFromFilename; // creation only

    RawBinaryLayout m_sLayout{};

    const char *GetKeyword( const char *pszPath,
                            const char *pszDefault = "");

    double       FixLong( double dfLong );
    void         BuildLabel();
    void         BuildHistory();
    void         WriteLabel();
    void         InvalidateLabel();

    static CPLString SerializeAsPDL( const CPLJSONObject& oObj );
    static void SerializeAsPDL( VSILFILE* fp, const CPLJSONObject& oObj,
                                int nDepth = 0 );

public:
    ISIS3Dataset();
    virtual ~ISIS3Dataset();

    virtual int CloseDependentDatasets() override;

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual CPLErr SetGeoTransform( double * padfTransform ) override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    virtual char **GetFileList() override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char* pszDomain = "" ) override;
    virtual CPLErr SetMetadata( char** papszMD, const char* pszDomain = "" )
                                                                     override;

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBandsIn,
                                GDALDataType eType, char ** papszOptions );
    static GDALDataset* CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                             ISISTiledBand                            */
/* ==================================================================== */
/************************************************************************/

class ISISTiledBand final: public GDALPamRasterBand
{
        friend class ISIS3Dataset;

        VSILFILE *m_fpVSIL;
        GIntBig   m_nFirstTileOffset;
        GIntBig   m_nXTileOffset;
        GIntBig   m_nYTileOffset;
        int       m_bNativeOrder;
        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

  public:

                ISISTiledBand( GDALDataset *poDS, VSILFILE *fpVSIL,
                               int nBand, GDALDataType eDT,
                               int nTileXSize, int nTileYSize,
                               GIntBig nFirstTileOffset,
                               GIntBig nXTileOffset,
                               GIntBig nYTileOffset,
                               int bNativeOrder );
        virtual     ~ISISTiledBand() {}

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual double GetOffset( int *pbSuccess = nullptr ) override;
        virtual double GetScale( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        void    SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                        ISIS3RawRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ISIS3RawRasterBand final: public RawRasterBand
{
        friend class ISIS3Dataset;

        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

    public:
                 ISIS3RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                     VSILFILE * l_fpRaw,
                                     vsi_l_offset l_nImgOffset,
                                     int l_nPixelOffset,
                                     int l_nLineOffset,
                                     GDALDataType l_eDataType,
                                     int l_bNativeOrder );
        virtual ~ISIS3RawRasterBand() {}

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = nullptr ) override;
        virtual double GetScale( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        void    SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                         ISIS3WrapperRasterBand                       */
/*                                                                      */
/*      proxy for bands stored in other formats.                        */
/* ==================================================================== */
/************************************************************************/
class ISIS3WrapperRasterBand final: public GDALProxyRasterBand
{
        friend class ISIS3Dataset;

        GDALRasterBand* m_poBaseBand;
        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() const override
                                                    { return m_poBaseBand; }

  public:
            explicit ISIS3WrapperRasterBand( GDALRasterBand* poBaseBandIn );
            ~ISIS3WrapperRasterBand() {}

        void    InitFile();

        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = nullptr ) override;
        virtual double GetScale( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        int             GetMaskFlags() override { return nMaskFlags; }
        GDALRasterBand* GetMaskBand() override { return poMask; }
        void            SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                             ISISMaskBand                             */
/* ==================================================================== */

class ISISMaskBand final: public GDALRasterBand
{
    GDALRasterBand  *m_poBaseBand;
    void            *m_pBuffer;

  public:

                            explicit ISISMaskBand( GDALRasterBand* poBaseBand );
                           ~ISISMaskBand();

    virtual CPLErr          IReadBlock( int, int, void * ) override;

};

/************************************************************************/
/*                           ISISTiledBand()                            */
/************************************************************************/

ISISTiledBand::ISISTiledBand( GDALDataset *poDSIn, VSILFILE *fpVSILIn,
                              int nBandIn, GDALDataType eDT,
                              int nTileXSize, int nTileYSize,
                              GIntBig nFirstTileOffsetIn,
                              GIntBig nXTileOffsetIn,
                              GIntBig nYTileOffsetIn,
                              int bNativeOrderIn ) :
    m_fpVSIL(fpVSILIn),
    m_nFirstTileOffset(0),
    m_nXTileOffset(nXTileOffsetIn),
    m_nYTileOffset(nYTileOffsetIn),
    m_bNativeOrder(bNativeOrderIn),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nBlockXSize = nTileXSize;
    nBlockYSize = nTileYSize;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();

    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

    if( m_nXTileOffset == 0 && m_nYTileOffset == 0 )
    {
        m_nXTileOffset =
            static_cast<GIntBig>(GDALGetDataTypeSizeBytes(eDT)) *
            nTileXSize;
        if( m_nXTileOffset > GINTBIG_MAX / nTileYSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return;
        }
        m_nXTileOffset *= nTileYSize;

        if( m_nXTileOffset > GINTBIG_MAX / l_nBlocksPerRow )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return;
        }
        m_nYTileOffset = m_nXTileOffset * l_nBlocksPerRow;
    }

    m_nFirstTileOffset = nFirstTileOffsetIn;
    if( nBand > 1 )
    {
        if( m_nYTileOffset > GINTBIG_MAX / (nBand - 1) ||
            (nBand-1) * m_nYTileOffset > GINTBIG_MAX / l_nBlocksPerColumn ||
            m_nFirstTileOffset > GINTBIG_MAX - (nBand-1) * m_nYTileOffset * l_nBlocksPerColumn )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return;
        }
        m_nFirstTileOffset += (nBand-1) * m_nYTileOffset * l_nBlocksPerColumn;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISISTiledBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    const GIntBig  nOffset = m_nFirstTileOffset +
        nXBlock * m_nXTileOffset + nYBlock * m_nYTileOffset;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const size_t nBlockSize = static_cast<size_t>(nDTSize)
                                            * nBlockXSize * nBlockYSize;

    if( VSIFSeekL( m_fpVSIL, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to offset %d to read tile %d,%d.",
                  static_cast<int>( nOffset ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( VSIFReadL( pImage, 1, nBlockSize, m_fpVSIL ) != nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes for tile %d,%d.",
                  static_cast<int>( nBlockSize ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    return CE_None;
}

/************************************************************************/
/*                           RemapNoDataT()                             */
/************************************************************************/

template<class T> static void RemapNoDataT( T* pBuffer, int nItems,
                                            T srcNoData, T dstNoData )
{
    for( int i = 0; i < nItems; i++ )
    {
        if( pBuffer[i] == srcNoData )
            pBuffer[i] = dstNoData;
    }
}

/************************************************************************/
/*                            RemapNoData()                             */
/************************************************************************/

static void RemapNoData( GDALDataType eDataType,
                         void* pBuffer, int nItems, double dfSrcNoData,
                         double dfDstNoData )
{
    if( eDataType == GDT_Byte )
    {
        RemapNoDataT( reinterpret_cast<GByte*>(pBuffer),
                      nItems,
                      static_cast<GByte>(dfSrcNoData),
                      static_cast<GByte>(dfDstNoData) );
    }
    else if( eDataType == GDT_UInt16 )
    {
        RemapNoDataT( reinterpret_cast<GUInt16*>(pBuffer),
                      nItems,
                      static_cast<GUInt16>(dfSrcNoData),
                      static_cast<GUInt16>(dfDstNoData) );
    }
    else if( eDataType == GDT_Int16)
    {
        RemapNoDataT( reinterpret_cast<GInt16*>(pBuffer),
                      nItems,
                      static_cast<GInt16>(dfSrcNoData),
                      static_cast<GInt16>(dfDstNoData) );
    }
    else
    {
        CPLAssert( eDataType == GDT_Float32 );
        RemapNoDataT( reinterpret_cast<float*>(pBuffer),
                      nItems,
                      static_cast<float>(dfSrcNoData),
                      static_cast<float>(dfDstNoData) );
    }
}

/**
 * Get or create CPLJSONObject.
 * @param  oParent Parent CPLJSONObject.
 * @param  osKey  Key name.
 * @return         CPLJSONObject class instance.
 */
static CPLJSONObject GetOrCreateJSONObject(CPLJSONObject &oParent,
                                           const std::string &osKey)
{
    CPLJSONObject oChild = oParent[osKey];
    if( oChild.IsValid() && oChild.GetType() != CPLJSONObject::Type::Object )
    {
        oParent.Delete( osKey );
        oChild.Deinit();
    }

    if( !oChild.IsValid() )
    {
        oChild = CPLJSONObject();
        oParent.Add( osKey, oChild );
    }
    return oChild;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISISTiledBand::IWriteBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }

    const GIntBig  nOffset = m_nFirstTileOffset +
        nXBlock * m_nXTileOffset + nYBlock * m_nYTileOffset;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const size_t nBlockSize = static_cast<size_t>(nDTSize)
                                            * nBlockXSize * nBlockYSize;

    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

    // Pad partial blocks to nodata value
    if( nXBlock == l_nBlocksPerRow - 1 && (nRasterXSize % nBlockXSize) != 0 )
    {
        GByte* pabyImage = static_cast<GByte*>(pImage);
        int nXStart = nRasterXSize % nBlockXSize;
        for( int iY = 0; iY < nBlockYSize; iY++ )
        {
            GDALCopyWords( &m_dfNoData, GDT_Float64, 0,
                           pabyImage + (iY * nBlockXSize + nXStart) * nDTSize,
                           eDataType, nDTSize,
                           nBlockXSize - nXStart );
        }
    }
    if( nYBlock == l_nBlocksPerColumn - 1 &&
        (nRasterYSize % nBlockYSize) != 0 )
    {
        GByte* pabyImage = static_cast<GByte*>(pImage);
        for( int iY = nRasterYSize % nBlockYSize; iY < nBlockYSize; iY++ )
        {
            GDALCopyWords( &m_dfNoData, GDT_Float64, 0,
                           pabyImage + iY * nBlockXSize * nDTSize,
                           eDataType, nDTSize,
                           nBlockXSize );
        }
    }

    if( VSIFSeekL( m_fpVSIL, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to offset %d to read tile %d,%d.",
                  static_cast<int>( nOffset ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    if( VSIFWriteL( pImage, 1, nBlockSize, m_fpVSIL ) != nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %d bytes for tile %d,%d.",
                  static_cast<int>( nBlockSize ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    return CE_None;
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISISTiledBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISISTiledBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISISTiledBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISISTiledBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISISTiledBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISISTiledBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISISTiledBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    return CE_None;
}

/************************************************************************/
/*                       ISIS3RawRasterBand()                           */
/************************************************************************/

ISIS3RawRasterBand::ISIS3RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                        VSILFILE * l_fpRaw,
                                        vsi_l_offset l_nImgOffset,
                                        int l_nPixelOffset,
                                        int l_nLineOffset,
                                        GDALDataType l_eDataType,
                                        int l_bNativeOrder )
    : RawRasterBand(l_poDS, l_nBand, l_fpRaw, l_nImgOffset, l_nPixelOffset,
                    l_nLineOffset,
                    l_eDataType, l_bNativeOrder, RawRasterBand::OwnFP::NO),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }
    return RawRasterBand::IReadBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                        void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }

    return RawRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }
    if( eRWFlag == GF_Write &&
        poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if( eBufType == eDataType && nPixelSpace == nDTSize &&
            nLineSpace == nPixelSpace * nBufXSize )
        {
            RemapNoData( eDataType, pData, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
        }
        else
        {
            const GByte* pabySrc = reinterpret_cast<GByte*>(pData);
            GByte* pabyTemp = reinterpret_cast<GByte*>(
                VSI_MALLOC3_VERBOSE(nDTSize, nBufXSize, nBufYSize));
            for( int i = 0; i < nBufYSize; i++ )
            {
                GDALCopyWords( pabySrc + i * nLineSpace, eBufType,
                               static_cast<int>(nPixelSpace),
                               pabyTemp + i * nBufXSize * nDTSize,
                               eDataType, nDTSize,
                               nBufXSize );
            }
            RemapNoData( eDataType, pabyTemp, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
            CPLErr eErr = RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pabyTemp, nBufXSize, nBufYSize,
                                     eDataType,
                                     nDTSize, nDTSize*nBufXSize,
                                     psExtraArg );
            VSIFree(pabyTemp);
            return eErr;
        }
    }
    return RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISIS3RawRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISIS3RawRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISIS3RawRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISIS3RawRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    return CE_None;
}

/************************************************************************/
/*                        ISIS3WrapperRasterBand()                      */
/************************************************************************/

ISIS3WrapperRasterBand::ISIS3WrapperRasterBand( GDALRasterBand* poBaseBandIn ) :
    m_poBaseBand(poBaseBandIn),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
    eDataType = m_poBaseBand->GetRasterDataType();
    m_poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISIS3WrapperRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISIS3WrapperRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISIS3WrapperRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetOffset(dfNewOffset);

    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetScale(dfNewScale);

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISIS3WrapperRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetNoDataValue(dfNewNoData);

    return CE_None;
}

/************************************************************************/
/*                              InitFile()                              */
/************************************************************************/

void ISIS3WrapperRasterBand::InitFile()
{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_bGeoTIFFAsRegularExternal && !poGDS->m_bGeoTIFFInitDone )
    {
        poGDS->m_bGeoTIFFInitDone = true;

        const int nBands = poGDS->GetRasterCount();
        // We need to make sure that blocks are written in the right order
        for( int i = 0; i < nBands; i++ )
        {
            poGDS->m_poExternalDS->GetRasterBand(i+1)->Fill(m_dfNoData);
        }
        poGDS->m_poExternalDS->FlushCache(false);

        // Check that blocks are effectively written in expected order.
        const int nBlockSizeBytes = nBlockXSize * nBlockYSize *
                                        GDALGetDataTypeSizeBytes(eDataType);

        GIntBig nLastOffset = 0;
        bool bGoOn = true;
        const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);
        for( int i = 0; i < nBands && bGoOn; i++ )
        {
            for( int y = 0; y < l_nBlocksPerColumn && bGoOn; y++ )
            {
                for( int x = 0; x < l_nBlocksPerRow && bGoOn; x++ )
                {
                    const char* pszBlockOffset =  poGDS->m_poExternalDS->
                        GetRasterBand(i+1)->GetMetadataItem(
                            CPLSPrintf("BLOCK_OFFSET_%d_%d", x, y), "TIFF");
                    if( pszBlockOffset )
                    {
                        GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                        if( i != 0 || x != 0 || y != 0 )
                        {
                            if( nOffset != nLastOffset + nBlockSizeBytes )
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Block %d,%d band %d not at expected "
                                         "offset",
                                         x, y, i+1);
                                bGoOn = false;
                                poGDS->m_bGeoTIFFAsRegularExternal = false;
                            }
                        }
                        nLastOffset = nOffset;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                         "Block %d,%d band %d not at expected "
                                         "offset",
                                         x, y, i+1);
                        bGoOn = false;
                        poGDS->m_bGeoTIFFAsRegularExternal = false;
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                               Fill()                                 */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::Fill(double dfRealValue, double dfImaginaryValue)
{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData == dfRealValue )
    {
        dfRealValue = m_dfNoData;
    }
    if( poGDS->m_bGeoTIFFAsRegularExternal && !poGDS->m_bGeoTIFFInitDone )
    {
        InitFile();
    }

    return GDALProxyRasterBand::Fill( dfRealValue, dfImaginaryValue );
}

/************************************************************************/
/*                             IWriteBlock()                             */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                            void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }
    if( poGDS->m_bGeoTIFFAsRegularExternal && !poGDS->m_bGeoTIFFInitDone )
    {
        InitFile();
    }

    return GDALProxyRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( eRWFlag == GF_Write && poGDS->m_bGeoTIFFAsRegularExternal &&
        !poGDS->m_bGeoTIFFInitDone )
    {
        InitFile();
    }
    if( eRWFlag == GF_Write &&
        poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if( eBufType == eDataType && nPixelSpace == nDTSize &&
            nLineSpace == nPixelSpace * nBufXSize )
        {
            RemapNoData( eDataType, pData, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
        }
        else
        {
            const GByte* pabySrc = reinterpret_cast<GByte*>(pData);
            GByte* pabyTemp = reinterpret_cast<GByte*>(
                VSI_MALLOC3_VERBOSE(nDTSize, nBufXSize, nBufYSize));
            for( int i = 0; i < nBufYSize; i++ )
            {
                GDALCopyWords( pabySrc + i * nLineSpace, eBufType,
                               static_cast<int>(nPixelSpace),
                               pabyTemp + i * nBufXSize * nDTSize,
                               eDataType, nDTSize,
                               nBufXSize );
            }
            RemapNoData( eDataType, pabyTemp, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
            CPLErr eErr = GDALProxyRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pabyTemp, nBufXSize, nBufYSize,
                                     eDataType,
                                     nDTSize, nDTSize*nBufXSize,
                                     psExtraArg );
            VSIFree(pabyTemp);
            return eErr;
        }
    }
    return GDALProxyRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                            ISISMaskBand()                            */
/************************************************************************/

ISISMaskBand::ISISMaskBand( GDALRasterBand* poBaseBand )
    : m_poBaseBand(poBaseBand)
    , m_pBuffer(nullptr)
{
    eDataType = GDT_Byte;
    poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    nRasterXSize = poBaseBand->GetXSize();
    nRasterYSize = poBaseBand->GetYSize();
}

/************************************************************************/
/*                           ~ISISMaskBand()                            */
/************************************************************************/

ISISMaskBand::~ISISMaskBand()
{
    VSIFree(m_pBuffer);
}

/************************************************************************/
/*                             FillMask()                               */
/************************************************************************/

template<class T>
static void FillMask      (void* pvBuffer,
                           GByte* pabyDst,
                           int nReqXSize, int nReqYSize,
                           int nBlockXSize,
                           T NULL_VAL, T LOW_REPR_SAT, T LOW_INSTR_SAT,
                           T HIGH_INSTR_SAT, T HIGH_REPR_SAT)
{
    const T* pSrc = static_cast<T*>(pvBuffer);
    for( int y = 0; y < nReqYSize; y++ )
    {
        for( int x = 0; x < nReqXSize; x++ )
        {
            const T nSrc = pSrc[y * nBlockXSize + x];
            if( nSrc == NULL_VAL ||
                nSrc == LOW_REPR_SAT ||
                nSrc == LOW_INSTR_SAT ||
                nSrc == HIGH_INSTR_SAT ||
                nSrc == HIGH_REPR_SAT )
            {
                pabyDst[y * nBlockXSize + x] = 0;
            }
            else
            {
                pabyDst[y * nBlockXSize + x] = 255;
            }
        }
    }
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr ISISMaskBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    const GDALDataType eSrcDT = m_poBaseBand->GetRasterDataType();
    const int nSrcDTSize = GDALGetDataTypeSizeBytes(eSrcDT);
    if( m_pBuffer == nullptr )
    {
        m_pBuffer = VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, nSrcDTSize);
        if( m_pBuffer == nullptr )
            return CE_Failure;
    }

    int nXOff = nXBlock * nBlockXSize;
    int nReqXSize = nBlockXSize;
    if( nXOff + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nXOff;
    int nYOff = nYBlock * nBlockYSize;
    int nReqYSize = nBlockYSize;
    if( nYOff + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nYOff;

    if( m_poBaseBand->RasterIO( GF_Read,
                                nXOff, nYOff, nReqXSize, nReqYSize,
                                m_pBuffer,
                                nReqXSize, nReqYSize,
                                eSrcDT,
                                nSrcDTSize,
                                nSrcDTSize * nBlockXSize,
                                nullptr ) != CE_None )
    {
        return CE_Failure;
    }

    GByte* pabyDst = static_cast<GByte*>(pImage);
    if( eSrcDT == GDT_Byte )
    {
        FillMask<GByte>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL1, LOW_REPR_SAT1, LOW_INSTR_SAT1,
                        HIGH_INSTR_SAT1, HIGH_REPR_SAT1);
    }
    else if( eSrcDT == GDT_UInt16 )
    {
        FillMask<GUInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULLU2, LOW_REPR_SATU2, LOW_INSTR_SATU2,
                        HIGH_INSTR_SATU2, HIGH_REPR_SATU2);
    }
    else if( eSrcDT == GDT_Int16 )
    {
        FillMask<GInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL2, LOW_REPR_SAT2, LOW_INSTR_SAT2,
                        HIGH_INSTR_SAT2, HIGH_REPR_SAT2);
    }
    else
    {
        CPLAssert( eSrcDT == GDT_Float32 );
        FillMask<float>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL4, LOW_REPR_SAT4, LOW_INSTR_SAT4,
                        HIGH_INSTR_SAT4, HIGH_REPR_SAT4);
    }

    return CE_None;
}

/************************************************************************/
/*                            ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::ISIS3Dataset() :
    m_fpLabel(nullptr),
    m_fpImage(nullptr),
    m_poExternalDS(nullptr),
    m_bGeoTIFFAsRegularExternal(false),
    m_bGeoTIFFInitDone(true),
    m_bIsLabelWritten(true),
    m_bIsTiled(false),
    m_bInitToNodata(false),
    m_bGotTransform(false),
    m_bHasSrcNoData(false),
    m_dfSrcNoData(0.0),
    m_bForce360(false),
    m_bWriteBoundingDegrees(true),
    m_bUseSrcLabel(true),
    m_bUseSrcMapping(false),
    m_bUseSrcHistory(true),
    m_bAddGDALHistory(true)
{
    m_oKeywords.SetStripSurroundingQuotes(true);
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;

    // Deinit JSON objects
    m_oJSonLabel.Deinit();
    m_oSrcJSonLabel.Deinit();
}

/************************************************************************/
/*                           ~ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::~ISIS3Dataset()

{
    if( !m_bIsLabelWritten )
        WriteLabel();
    if( m_poExternalDS && m_bGeoTIFFAsRegularExternal && !m_bGeoTIFFInitDone )
    {
        reinterpret_cast<ISIS3WrapperRasterBand*>(GetRasterBand(1))->
            InitFile();
    }
    ISIS3Dataset::FlushCache(true);
    if( m_fpLabel != nullptr )
        VSIFCloseL( m_fpLabel );
    if( m_fpImage != nullptr && m_fpImage != m_fpLabel )
        VSIFCloseL( m_fpImage );

    ISIS3Dataset::CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int ISIS3Dataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( m_poExternalDS )
    {
        bHasDroppedRef = FALSE;
        delete m_poExternalDS;
        m_poExternalDS = nullptr;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISIS3Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( !m_osExternalFilename.empty() )
        papszFileList = CSLAddString( papszFileList, m_osExternalFilename );
    for( int i = 0; i < m_aosAdditionalFiles.Count(); ++i )
    {
        if( CSLFindString(papszFileList, m_aosAdditionalFiles[i]) < 0 )
        {
            papszFileList = CSLAddString( papszFileList,
                                          m_aosAdditionalFiles[i] );
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference* ISIS3Dataset::GetSpatialRef() const

{
    if( !m_oSRS.IsEmpty() )
        return &m_oSRS;

    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr ISIS3Dataset::SetSpatialRef( const OGRSpatialReference* poSRS )
{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetSpatialRef( poSRS );
    if( poSRS )
        m_oSRS = *poSRS;
    else
        m_oSRS.Clear();
    if( m_poExternalDS )
        m_poExternalDS->SetSpatialRef(poSRS);
    InvalidateLabel();
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS3Dataset::GetGeoTransform( double * padfTransform )

{
    if( m_bGotTransform )
    {
        memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS3Dataset::SetGeoTransform( double * padfTransform )

{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetGeoTransform( padfTransform );
    if( padfTransform[1] <= 0.0 || padfTransform[1] != -padfTransform[5] ||
        padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up geotransform with square pixels supported");
        return CE_Failure;
    }
    m_bGotTransform = true;
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double) * 6 );
    if( m_poExternalDS )
        m_poExternalDS->SetGeoTransform(padfTransform);
    InvalidateLabel();
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **ISIS3Dataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(
        nullptr, FALSE, "", "json:ISIS3", nullptr);
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **ISIS3Dataset::GetMetadata( const char* pszDomain )
{
    if( pszDomain != nullptr && EQUAL( pszDomain, "json:ISIS3" ) )
    {
        if( m_aosISIS3MD.empty() )
        {
            if( eAccess == GA_Update && !m_oJSonLabel.IsValid() )
            {
                BuildLabel();
            }
            CPLAssert( m_oJSonLabel.IsValid() );
            const CPLString osJson = m_oJSonLabel.Format(CPLJSONObject::PrettyFormat::Pretty);
            m_aosISIS3MD.InsertString(0, osJson.c_str());
        }
        return m_aosISIS3MD.List();
    }
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           InvalidateLabel()                          */
/************************************************************************/

void ISIS3Dataset::InvalidateLabel()
{
    m_oJSonLabel.Deinit();
    m_aosISIS3MD.Clear();
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr ISIS3Dataset::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( m_bUseSrcLabel && eAccess == GA_Update && pszDomain != nullptr &&
        EQUAL( pszDomain, "json:ISIS3" ) )
    {
        m_oSrcJSonLabel.Deinit();
        InvalidateLabel();
        if( papszMD != nullptr && papszMD[0] != nullptr )
        {
            CPLJSONDocument oJSONDocument;
            const GByte *pabyData = reinterpret_cast<const GByte *>(papszMD[0]);
            if( !oJSONDocument.LoadMemory( pabyData ) )
            {
                return CE_Failure;
            }

            m_oSrcJSonLabel = oJSONDocument.GetRoot();
            if( !m_oSrcJSonLabel.IsValid() )
            {
                return CE_Failure;
            }
        }
        return CE_None;
    }
    return GDALPamDataset::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int ISIS3Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fpL != nullptr &&
        poOpenInfo->pabyHeader != nullptr &&
        strstr((const char *)poOpenInfo->pabyHeader,"IsisCube") != nullptr )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool ISIS3Dataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    if( m_sLayout.osRawFilename.empty() )
        return false;
    sLayout = m_sLayout;
    return true;
}

/************************************************************************/
/*                           GetValueAndUnits()                         */
/************************************************************************/

static void GetValueAndUnits(const CPLJSONObject& obj,
                             std::vector<double>& adfValues,
                             std::vector<std::string>& aosUnits,
                             int nExpectedVals)
{
    if( obj.GetType() == CPLJSONObject::Type::Integer ||
        obj.GetType() == CPLJSONObject::Type::Double )
    {
        adfValues.push_back(obj.ToDouble());
    }
    else if( obj.GetType() == CPLJSONObject::Type::Object )
    {
        auto oValue = obj.GetObj("value");
        auto oUnit = obj.GetObj("unit");
        if( oValue.IsValid() &&
            (oValue.GetType() == CPLJSONObject::Type::Integer ||
                oValue.GetType() == CPLJSONObject::Type::Double ||
                oValue.GetType() == CPLJSONObject::Type::Array) &&
            oUnit.IsValid() && oUnit.GetType() == CPLJSONObject::Type::String )
        {
            if( oValue.GetType() == CPLJSONObject::Type::Array )
            {
                GetValueAndUnits(oValue, adfValues, aosUnits, nExpectedVals);
            }
            else
            {
                adfValues.push_back(oValue.ToDouble());
            }
            aosUnits.push_back(oUnit.ToString());
        }
    }
    else if( obj.GetType() == CPLJSONObject::Type::Array )
    {
        auto oArray = obj.ToArray();
        if( oArray.Size() == nExpectedVals )
        {
            for( int i = 0; i < nExpectedVals; i++ )
            {
                if( oArray[i].GetType() == CPLJSONObject::Type::Integer ||
                    oArray[i].GetType() == CPLJSONObject::Type::Double )
                {
                    adfValues.push_back(oArray[i].ToDouble());
                }
                else
                {
                    adfValues.clear();
                    return;
                }
            }
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS3Dataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this look like a CUBE dataset?                             */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    ISIS3Dataset *poDS = new ISIS3Dataset();

    if( ! poDS->m_oKeywords.Ingest( poOpenInfo->fpL, 0 ) )
    {
        VSIFCloseL( poOpenInfo->fpL );
        poOpenInfo->fpL = nullptr;
        delete poDS;
        return nullptr;
    }
    poDS->m_oJSonLabel = poDS->m_oKeywords.GetJsonObject();
    poDS->m_oJSonLabel.Add( "_filename", poOpenInfo->pszFilename );

    // Find additional files from the label
    for( const CPLJSONObject& oObj : poDS->m_oJSonLabel.GetChildren() )
    {
        if( oObj.GetType() == CPLJSONObject::Type::Object )
        {
            CPLString osContainerName = oObj.GetName();
            CPLJSONObject oContainerName = oObj.GetObj( "_container_name" );
            if( oContainerName.GetType() == CPLJSONObject::Type::String )
            {
                osContainerName = oContainerName.ToString();
            }

            CPLJSONObject oFilename = oObj.GetObj( "^" + osContainerName );
            if( oFilename.GetType() == CPLJSONObject::Type::String )
            {
                VSIStatBufL sStat;
                CPLString osFilename( CPLFormFilename(
                    CPLGetPath(poOpenInfo->pszFilename),
                    oFilename.ToString().c_str(),
                    nullptr ) );
                if( VSIStatL( osFilename, &sStat ) == 0 )
                {
                    poDS->m_aosAdditionalFiles.AddString(osFilename);
                }
                else
                {
                    CPLDebug("ISIS3", "File %s referenced but not foud",
                                osFilename.c_str());
                }
            }
        }
    }

    VSIFCloseL( poOpenInfo->fpL );
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/* Assume user is pointing to label (i.e. .lbl) file for detached option */
/* -------------------------------------------------------------------- */
    //  Image can be inline or detached and point to an image name
    //  the Format can be Tiled or Raw
    //  Object = Core
    //      StartByte   = 65537
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detatched.cub
    //      Format    = BandSequential
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detached_tiled.cub
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = some.tif
    //      Format    = GeoTIFF

/* -------------------------------------------------------------------- */
/*      What file contains the actual data?                             */
/* -------------------------------------------------------------------- */
    const char *pszCore = poDS->GetKeyword( "IsisCube.Core.^Core" );
    CPLString osQubeFile;

    if( EQUAL(pszCore,"") )
        osQubeFile = poOpenInfo->pszFilename;
    else
    {
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        osQubeFile = CPLFormFilename( osPath, pszCore, nullptr );
        poDS->m_osExternalFilename = osQubeFile;
    }

/* -------------------------------------------------------------------- */
/*      Check if file an ISIS3 header file?  Read a few lines of text   */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */

    /*************   Skipbytes     *****************************/
    int nSkipBytes = atoi(poDS->GetKeyword("IsisCube.Core.StartByte", "1"));
    if( nSkipBytes <= 1 )
        nSkipBytes = 0;
    else
        nSkipBytes -= 1;

    /*******   Grab format type (BandSequential, Tiled)  *******/
    CPLString osFormat = poDS->GetKeyword( "IsisCube.Core.Format" );

    int tileSizeX = 0;
    int tileSizeY = 0;

    if (EQUAL(osFormat,"Tile") )
    {
       poDS->m_bIsTiled = true;
       /******* Get Tile Sizes *********/
       tileSizeX = atoi(poDS->GetKeyword("IsisCube.Core.TileSamples"));
       tileSizeY = atoi(poDS->GetKeyword("IsisCube.Core.TileLines"));
       if (tileSizeX <= 0 || tileSizeY <= 0)
       {
           CPLError( CE_Failure, CPLE_OpenFailed,
                     "Wrong tile dimensions : %d x %d",
                     tileSizeX, tileSizeY);
           delete poDS;
           return nullptr;
       }
    }
    else if (!EQUAL(osFormat,"BandSequential") &&
             !EQUAL(osFormat,"GeoTIFF") )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s format not supported.", osFormat.c_str());
        delete poDS;
        return nullptr;
    }

    /***********   Grab samples lines band ************/
    const int nCols = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Samples"));
    const int nRows = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Lines"));
    const int nBands = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Bands"));

    /****** Grab format type - ISIS3 only supports 8,U16,S16,32 *****/
    GDALDataType eDataType = GDT_Byte;
    double dfNoData = 0.0;

    const char *itype = poDS->GetKeyword( "IsisCube.Core.Pixels.Type" );
    if (EQUAL(itype,"UnsignedByte") ) {
        eDataType = GDT_Byte;
        dfNoData = NULL1;
    }
    else if (EQUAL(itype,"UnsignedWord") ) {
        eDataType = GDT_UInt16;
        dfNoData = NULLU2;
    }
    else if (EQUAL(itype,"SignedWord") ) {
        eDataType = GDT_Int16;
        dfNoData = NULL2;
    }
    else if (EQUAL(itype,"Real") || EQUAL(itype,"") ) {
        eDataType = GDT_Float32;
        dfNoData = NULL4;
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s pixel type not supported.", itype);
        delete poDS;
        return nullptr;
    }

    /***********   Grab samples lines band ************/

    //default to MSB
    const bool bIsLSB = EQUAL(
            poDS->GetKeyword( "IsisCube.Core.Pixels.ByteOrder"),"Lsb");

    /***********   Grab Cellsize ************/
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    const char* pszRes = poDS->GetKeyword("IsisCube.Mapping.PixelResolution");
    if (strlen(pszRes) > 0 ) {
        dfXDim = CPLAtof(pszRes); /* values are in meters */
        dfYDim = -CPLAtof(pszRes);
    }

    /***********   Grab UpperLeftCornerY ************/
    double dfULYMap = 0.5;

    const char* pszULY = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerY");
    if (strlen(pszULY) > 0) {
        dfULYMap = CPLAtof(pszULY);
    }

    /***********   Grab UpperLeftCornerX ************/
    double dfULXMap = 0.5;

    const char* pszULX = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerX");
    if( strlen(pszULX) > 0 ) {
        dfULXMap = CPLAtof(pszULX);
    }

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. Mars ***/
    const char *target_name = poDS->GetKeyword("IsisCube.Mapping.TargetName");

#ifdef notdef
    const double dfLongitudeMulFactor =
        EQUAL(poDS->GetKeyword( "IsisCube.Mapping.LongitudeDirection", "PositiveEast"), "PositiveEast") ? 1 : -1;
#else
    const double dfLongitudeMulFactor = 1;
#endif

    /***********   Grab MAP_PROJECTION_TYPE ************/
     const char *map_proj_name =
        poDS->GetKeyword( "IsisCube.Mapping.ProjectionName");

    /***********   Grab SEMI-MAJOR ************/
    const double semi_major =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.EquatorialRadius"));

    /***********   Grab semi-minor ************/
    const double semi_minor =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.PolarRadius"));

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.CenterLatitude"));

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.CenterLongitude")) * dfLongitudeMulFactor;

    /***********   Grab 1st std parallel ************/
    const double first_std_parallel =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.FirstStandardParallel"));

    /***********   Grab 2nd std parallel ************/
    const double second_std_parallel =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.SecondStandardParallel"));

    /***********   Grab scaleFactor ************/
    const double scaleFactor =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.scaleFactor", "1.0"));

    /*** grab      LatitudeType = Planetographic ****/
    // Need to further study how ocentric/ographic will effect the gdal library
    // So far we will use this fact to define a sphere or ellipse for some
    // projections

    // Frank - may need to talk this over
    bool bIsGeographic = true;
    if (EQUAL( poDS->GetKeyword("IsisCube.Mapping.LatitudeType"),
               "Planetocentric" ))
        bIsGeographic = false;

    //Set oSRS projection and parameters
    //############################################################
    //ISIS3 Projection types
    //  Equirectangular
    //  LambertConformal
    //  Mercator
    //  ObliqueCylindrical
    //  Orthographic
    //  PolarStereographic
    //  SimpleCylindrical
    //  Sinusoidal
    //  TransverseMercator

#ifdef DEBUG
    CPLDebug( "ISIS3", "using projection %s", map_proj_name);
#endif

    OGRSpatialReference oSRS;
    bool bProjectionSet = true;

    if ((EQUAL( map_proj_name, "Equirectangular" )) ||
        (EQUAL( map_proj_name, "SimpleCylindrical" )) )  {
        oSRS.SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "Orthographic" )) {
        oSRS.SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Sinusoidal" )) {
        oSRS.SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Mercator" )) {
        oSRS.SetMercator ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "PolarStereographic" )) {
        oSRS.SetPS ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "TransverseMercator" )) {
        oSRS.SetTM ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "LambertConformal" )) {
        oSRS.SetLCC ( first_std_parallel, second_std_parallel, center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "PointPerspective" )) {
        // Distance parameter is the distance to the center of the body, and is given in km
        const double distance = CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.Distance")) * 1000.0;
        const double height_above_ground = distance - semi_major;
        oSRS.SetVerticalPerspective(center_lat, center_lon, 0, height_above_ground, 0, 0);
    } else if (EQUAL( map_proj_name, "ObliqueCylindrical" )) {
        const double poleLatitude = CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.PoleLatitude"));
        const double poleLongitude = CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.PoleLongitude")) * dfLongitudeMulFactor;
        const double poleRotation = CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.PoleRotation"));
        CPLString oProj4String;
        // ISIS3 rotated pole doesn't use the same conventions than PROJ ob_tran
        // Compare the sign difference in https://github.com/USGS-Astrogeology/ISIS3/blob/3.8.0/isis/src/base/objs/ObliqueCylindrical/ObliqueCylindrical.cpp#L244
        // and https://github.com/OSGeo/PROJ/blob/6.2/src/projections/ob_tran.cpp#L34
        // They can be compensated by modifying the poleLatitude to 180-poleLatitude
        // There's also a sign difference for the poleRotation parameter
        // The existence of those different conventions is acknowledged in
        // https://pds-imaging.jpl.nasa.gov/documentation/Cassini_BIDRSIS.PDF in the middle of page 10
        oProj4String.Printf(
            "+proj=ob_tran +o_proj=eqc +o_lon_p=%.18g +o_lat_p=%.18g +lon_0=%.18g",
            -poleRotation,
            180-poleLatitude,
            poleLongitude);
        oSRS.SetFromUserInput(oProj4String);
    } else {
        CPLDebug( "ISIS3",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name );
        bProjectionSet = false;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        CPLString osProjTargetName(map_proj_name);
        osProjTargetName += " ";
        osProjTargetName += target_name;
        oSRS.SetProjCS(osProjTargetName); //set ProjCS keyword

        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        CPLString osGeogName("GCS_");
        osGeogName += target_name;

        //The datum name will be the same basic name as the planet
        CPLString osDatumName("D_");
        osDatumName += target_name;

        CPLString osSphereName(target_name);
        //strcat(osSphereName, "_IAU_IAG");  //Might not be IAU defined so don't add

        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double iflattening = 0.0;
        if ((semi_major - semi_minor) < 0.0000001)
           iflattening = 0;
        else
           iflattening = semi_major / (semi_major - semi_minor);

        //Set the body size but take into consideration which proj is being used to help w/ proj4 compatibility
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS does it internally
        if ( ( (EQUAL( map_proj_name, "Stereographic" ) && (fabs(center_lat) == 90)) ) ||
             (EQUAL( map_proj_name, "PolarStereographic" )) )
         {
            if (bIsGeographic) {
                //Geograpraphic, so set an ellipse
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, iflattening,
                               "Reference_Meridian", 0.0 );
            } else {
              //Geocentric, so force a sphere using the semi-minor axis. I hope...
              osSphereName += "_polarRadius";
              oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                              semi_minor, 0.0,
                              "Reference_Meridian", 0.0 );
            }
        }
        else if ( (EQUAL( map_proj_name, "SimpleCylindrical" )) ||
                  (EQUAL( map_proj_name, "Orthographic" )) ||
                  (EQUAL( map_proj_name, "Stereographic" )) ||
                  (EQUAL( map_proj_name, "Sinusoidal" )) ||
                  (EQUAL( map_proj_name, "PointPerspective" )) ) {
            // ISIS uses the spherical equation for these projections
            // so force a sphere.
            oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else if  (EQUAL( map_proj_name, "Equirectangular" )) {
            //Calculate localRadius using ISIS3 simple elliptical method
            //  not the more standard Radius of Curvature method
            //PI = 4 * atan(1);
            const double radLat = center_lat * M_PI / 180;  // in radians
            const double meanRadius =
                sqrt( pow( semi_minor * cos( radLat ), 2)
                    + pow( semi_major * sin( radLat ), 2) );
            const double localRadius = ( meanRadius == 0.0 ) ?
                                0.0 : semi_major * semi_minor / meanRadius;
            osSphereName += "_localRadius";
            oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                            localRadius, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else {
            //All other projections: Mercator, Transverse Mercator, Lambert Conformal, etc.
            //Geographic, so set an ellipse
            if (bIsGeographic) {
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            } else {
                //Geocentric, so force a sphere. I hope...
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }

        // translate back into a projection string.
        poDS->m_oSRS = oSRS;
        poDS->m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

/* END ISIS3 Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( !GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, false) )
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(osFormat,"GeoTIFF") )
    {
        if( nSkipBytes != 0 )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Ignoring StartByte=%d for format=GeoTIFF",
                     1+nSkipBytes);
        }
        if( osQubeFile == poOpenInfo->pszFilename )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "A ^Core file must be set");
            delete poDS;
            return nullptr;
        }
        poDS->m_poExternalDS = reinterpret_cast<GDALDataset *>(
                                GDALOpen( osQubeFile, poOpenInfo->eAccess ) );
        if( poDS->m_poExternalDS == nullptr )
        {
            delete poDS;
            return nullptr;
        }
        if( poDS->m_poExternalDS->GetRasterXSize() != poDS->nRasterXSize ||
            poDS->m_poExternalDS->GetRasterYSize() != poDS->nRasterYSize ||
            poDS->m_poExternalDS->GetRasterCount() != nBands ||
            poDS->m_poExternalDS->GetRasterBand(1)->GetRasterDataType() !=
                                                                    eDataType )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s has incompatible characteristics with the ones "
                      "declared in the label.",
                      osQubeFile.c_str() );
            delete poDS;
            return nullptr;
        }
    }
    else
    {
        if( poOpenInfo->eAccess == GA_ReadOnly )
            poDS->m_fpImage = VSIFOpenL( osQubeFile, "r" );
        else
            poDS->m_fpImage = VSIFOpenL( osQubeFile, "r+" );

        if( poDS->m_fpImage == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Failed to open %s: %s.",
                    osQubeFile.c_str(),
                    VSIStrerror( errno ) );
            delete poDS;
            return nullptr;
        }

        // Sanity checks in case the external raw file appears to be a
        // TIFF file
        if( EQUAL(CPLGetExtension(osQubeFile), "tif") )
        {
            GDALDataset* poTIF_DS = reinterpret_cast<GDALDataset*>(
                GDALOpen(osQubeFile, GA_ReadOnly));
            if( poTIF_DS )
            {
                bool bWarned = false;
                if( poTIF_DS->GetRasterXSize() != poDS->nRasterXSize ||
                    poTIF_DS->GetRasterYSize() != poDS->nRasterYSize ||
                    poTIF_DS->GetRasterCount() != nBands ||
                    poTIF_DS->GetRasterBand(1)->GetRasterDataType() !=
                                                                eDataType ||
                    poTIF_DS->GetMetadataItem("COMPRESSION",
                                              "IMAGE_STRUCTURE") != nullptr )
                {
                    bWarned = true;
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "%s has incompatible characteristics with the ones "
                        "declared in the label.",
                        osQubeFile.c_str() );
                }
                int nBlockXSize = 1, nBlockYSize = 1;
                poTIF_DS->GetRasterBand(1)->GetBlockSize(&nBlockXSize,
                                                         &nBlockYSize);
                if( (poDS->m_bIsTiled && (nBlockXSize != tileSizeX ||
                                          nBlockYSize != tileSizeY) ) ||
                    (!poDS->m_bIsTiled && (nBlockXSize != nCols ||
                                        (nBands > 1 && nBlockYSize != 1))) )
                {
                    if( !bWarned )
                    {
                        bWarned = true;
                        CPLError( CE_Warning, CPLE_AppDefined,
                            "%s has incompatible characteristics with the ones "
                            "declared in the label.",
                            osQubeFile.c_str() );
                    }
                }
                // to please Clang Static Analyzer
                nBlockXSize = std::max(1, nBlockXSize);
                nBlockYSize = std::max(1, nBlockYSize);

                // Check that blocks are effectively written in expected order.
                const int nBlockSizeBytes = nBlockXSize * nBlockYSize *
                                        GDALGetDataTypeSizeBytes(eDataType);
                bool bGoOn = !bWarned;
                const int l_nBlocksPerRow =
                        DIV_ROUND_UP(nCols, nBlockXSize);
                const int l_nBlocksPerColumn =
                        DIV_ROUND_UP(nRows, nBlockYSize);
                int nBlockNo = 0;
                for( int i = 0; i < nBands && bGoOn; i++ )
                {
                    for( int y = 0; y < l_nBlocksPerColumn && bGoOn; y++ )
                    {
                        for( int x = 0; x < l_nBlocksPerRow && bGoOn; x++ )
                        {
                            const char* pszBlockOffset =  poTIF_DS->
                                GetRasterBand(i+1)->GetMetadataItem(
                                    CPLSPrintf("BLOCK_OFFSET_%d_%d", x, y),
                                    "TIFF");
                            if( pszBlockOffset )
                            {
                                GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                                if( nOffset != nSkipBytes + nBlockNo *
                                                            nBlockSizeBytes )
                                {
                                    //bWarned = true;
                                    CPLError( CE_Warning, CPLE_AppDefined,
                                        "%s has incompatible "
                                        "characteristics with the ones "
                                        "declared in the label.",
                                        osQubeFile.c_str() );
                                    bGoOn = false;
                                }
                            }
                            nBlockNo ++;
                        }
                    }
                }

                delete poTIF_DS;
            }
        }
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int nLineOffset = 0;
    int nPixelOffset = 0;
    vsi_l_offset nBandOffset = 0;

    if( EQUAL(osFormat,"BandSequential") )
    {
        const int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
        nPixelOffset = nItemSize;
        try
        {
            nLineOffset = (CPLSM(nPixelOffset) * CPLSM(nCols)).v();
        }
        catch( const CPLSafeIntOverflow& )
        {
            delete poDS;
            return nullptr;
        }
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;

        poDS->m_sLayout.osRawFilename = osQubeFile;
        if( nBands > 1 )
            poDS->m_sLayout.eInterleaving = RawBinaryLayout::Interleaving::BSQ;
        poDS->m_sLayout.eDataType = eDataType;
        poDS->m_sLayout.bLittleEndianOrder = bIsLSB;
        poDS->m_sLayout.nImageOffset = nSkipBytes;
        poDS->m_sLayout.nPixelOffset = nPixelOffset;
        poDS->m_sLayout.nLineOffset = nLineOffset;
        poDS->m_sLayout.nBandOffset = static_cast<GIntBig>(nBandOffset);
    }
    /* else Tiled or external */

/* -------------------------------------------------------------------- */
/*      Extract BandBin info.                                           */
/* -------------------------------------------------------------------- */
    std::vector<std::string> aosBandNames;
    std::vector<std::string> aosBandUnits;
    std::vector<double> adfWavelengths;
    std::vector<std::string> aosWavelengthsUnit;
    std::vector<double> adfBandwidth;
    std::vector<std::string> aosBandwidthUnit;
    const auto oBandBin = poDS->m_oJSonLabel.GetObj( "IsisCube/BandBin" );
    if( oBandBin.IsValid() && oBandBin.GetType() == CPLJSONObject::Type::Object )
    {
        for( const auto& child: oBandBin.GetChildren() )
        {
            if( CPLString(child.GetName()).ifind("name") != std::string::npos )
            {
                // Use "name" in priority
                if( EQUAL(child.GetName().c_str(), "name") )
                {
                    aosBandNames.clear();
                }
                else if( !aosBandNames.empty() )
                {
                    continue;
                }

                if( child.GetType() == CPLJSONObject::Type::String && nBands == 1 )
                {
                    aosBandNames.push_back(child.ToString());
                }
                else if( child.GetType() == CPLJSONObject::Type::Array )
                {
                    auto oArray = child.ToArray();
                    if( oArray.Size() == nBands )
                    {
                        for( int i = 0; i < nBands; i++ )
                        {
                            if( oArray[i].GetType() == CPLJSONObject::Type::String )
                            {
                                aosBandNames.push_back(oArray[i].ToString());
                            }
                            else
                            {
                                aosBandNames.clear();
                                break;
                            }
                        }
                    }
                }
            }
            else if( EQUAL(child.GetName().c_str(), "BandSuffixUnit") &&
                     child.GetType() == CPLJSONObject::Type::Array )
            {
                auto oArray = child.ToArray();
                if( oArray.Size() == nBands )
                {
                    for( int i = 0; i < nBands; i++ )
                    {
                        if( oArray[i].GetType() == CPLJSONObject::Type::String )
                        {
                            aosBandUnits.push_back(oArray[i].ToString());
                        }
                        else
                        {
                            aosBandUnits.clear();
                            break;
                        }
                    }
                }
            }
            else if( EQUAL(child.GetName().c_str(), "BandBinCenter") ||
                     EQUAL(child.GetName().c_str(), "Center") )
            {
                GetValueAndUnits(child, adfWavelengths, aosWavelengthsUnit,
                                 nBands);
            }
            else if( EQUAL(child.GetName().c_str(), "BandBinUnit") &&
                     child.GetType() == CPLJSONObject::Type::String )
            {
                CPLString unit(child.ToString());
                if( STARTS_WITH_CI(unit, "micromet") ||
                    EQUAL(unit, "um") ||
                    STARTS_WITH_CI(unit, "nanomet") ||
                    EQUAL(unit, "nm")  )
                {
                    aosWavelengthsUnit.push_back(child.ToString());
                }
            }
            else if( EQUAL(child.GetName().c_str(), "Width") )
            {
                GetValueAndUnits(child, adfBandwidth, aosBandwidthUnit,
                                 nBands);
            }
        }

        if( !adfWavelengths.empty() && aosWavelengthsUnit.size() == 1 )
        {
            for( int i = 1; i < nBands; i++ )
            {
                aosWavelengthsUnit.push_back(aosWavelengthsUnit[0]);
            }
        }
        if( !adfBandwidth.empty() && aosBandwidthUnit.size() == 1 )
        {
            for( int i = 1; i < nBands; i++ )
            {
                aosBandwidthUnit.push_back(aosBandwidthUnit[0]);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
#ifdef CPL_LSB
    const bool bNativeOrder = bIsLSB;
#else
    const bool bNativeOrder = !bIsLSB;
#endif

    for( int i = 0; i < nBands; i++ )
    {
        GDALRasterBand *poBand = nullptr;

        if( poDS->m_poExternalDS != nullptr )
        {
            ISIS3WrapperRasterBand* poISISBand =
                new ISIS3WrapperRasterBand(
                            poDS->m_poExternalDS->GetRasterBand( i+1 ) );
            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }
        else if( poDS->m_bIsTiled )
        {
            CPLErrorReset();
            ISISTiledBand* poISISBand =
                new ISISTiledBand( poDS, poDS->m_fpImage, i+1, eDataType,
                                        tileSizeX, tileSizeY,
                                        nSkipBytes, 0, 0,
                                        bNativeOrder );
            if( CPLGetLastErrorType() != CE_None )
            {
                delete poISISBand;
                delete poDS;
                return nullptr;
            }
            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }
        else
        {
            ISIS3RawRasterBand* poISISBand =
                new ISIS3RawRasterBand( poDS, i+1, poDS->m_fpImage,
                                   nSkipBytes + nBandOffset * i,
                                   nPixelOffset, nLineOffset, eDataType,
                                   bNativeOrder );

            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }

        if( i < static_cast<int>(aosBandNames.size()) )
        {
            poBand->SetDescription(aosBandNames[i].c_str());
        }
        if( i < static_cast<int>(adfWavelengths.size()) &&
            i < static_cast<int>(aosWavelengthsUnit.size()) )
        {
            poBand->SetMetadataItem("WAVELENGTH", CPLSPrintf("%f", adfWavelengths[i]));
            poBand->SetMetadataItem("WAVELENGTH_UNIT", aosWavelengthsUnit[i].c_str());
            if( i < static_cast<int>(adfBandwidth.size()) &&
                i < static_cast<int>(aosBandwidthUnit.size()) )
            {
                poBand->SetMetadataItem("BANDWIDTH", CPLSPrintf("%f", adfBandwidth[i]));
                poBand->SetMetadataItem("BANDWIDTH_UNIT", aosBandwidthUnit[i].c_str());
            }
        }
        if( i < static_cast<int>(aosBandUnits.size()) )
        {
            poBand->SetUnitType(aosBandUnits[i].c_str());
        }

        poBand->SetNoDataValue( dfNoData );

        // Set offset/scale values.
        const double dfOffset =
            CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Base","0.0"));
        const double dfScale =
            CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Multiplier","1.0"));
        if( dfOffset != 0.0 || dfScale != 1.0 )
        {
            poBand->SetOffset(dfOffset);
            poBand->SetScale(dfScale);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file. For ISIS3 I would like to keep this in   */
/* -------------------------------------------------------------------- */
    const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    const CPLString osName = CPLGetBasename(poOpenInfo->pszFilename);
    const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

    VSILFILE *fp = VSIFOpenL( pszPrjFile, "r" );
    if( fp != nullptr )
    {
        VSIFCloseL( fp );

        char **papszLines = CSLLoad( pszPrjFile );

        OGRSpatialReference oSRS2;
        if( oSRS2.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            poDS->m_aosAdditionalFiles.AddString( pszPrjFile );
            poDS->m_oSRS = oSRS2;
            poDS->m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }

        CSLDestroy( papszLines );
    }

    if( dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->m_bGotTransform = true;
        poDS->m_adfGeoTransform[0] = dfULXMap;
        poDS->m_adfGeoTransform[1] = dfXDim;
        poDS->m_adfGeoTransform[2] = 0.0;
        poDS->m_adfGeoTransform[3] = dfULYMap;
        poDS->m_adfGeoTransform[4] = 0.0;
        poDS->m_adfGeoTransform[5] = dfYDim;
    }

    if( !poDS->m_bGotTransform )
    {
        poDS->m_bGotTransform =
            CPL_TO_BOOL(GDALReadWorldFile( poOpenInfo->pszFilename, "cbw",
                               poDS->m_adfGeoTransform ));
        if( poDS->m_bGotTransform )
        {
            poDS->m_aosAdditionalFiles.AddString(
                        CPLResetExtension(poOpenInfo->pszFilename, "cbw") );
        }
    }

    if( !poDS->m_bGotTransform )
    {
        poDS->m_bGotTransform =
            CPL_TO_BOOL(GDALReadWorldFile( poOpenInfo->pszFilename, "wld",
                               poDS->m_adfGeoTransform ));
        if( poDS->m_bGotTransform )
        {
            poDS->m_aosAdditionalFiles.AddString(
                        CPLResetExtension(poOpenInfo->pszFilename, "wld") );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *ISIS3Dataset::GetKeyword( const char *pszPath,
                                      const char *pszDefault )

{
    return m_oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                              FixLong()                               */
/************************************************************************/

double ISIS3Dataset::FixLong( double dfLong )
{
    if( m_osLongitudeDirection == "PositiveWest" )
        dfLong = -dfLong;
    if( m_bForce360 && dfLong < 0 )
        dfLong += 360.0;
    return dfLong;
}

/************************************************************************/
/*                           BuildLabel()                               */
/************************************************************************/

void ISIS3Dataset::BuildLabel()
{
    CPLJSONObject oLabel = m_oSrcJSonLabel;
    if( !oLabel.IsValid() )
    {
        oLabel = CPLJSONObject();
    }
    // If we have a source label, then edit it directly
    CPLJSONObject oIsisCube = GetOrCreateJSONObject(oLabel, "IsisCube");
    oIsisCube.Set( "_type", "object");

    if( !m_osComment.empty() )
        oIsisCube.Set( "_comment", m_osComment );

    CPLJSONObject oCore = GetOrCreateJSONObject(oIsisCube, "Core");
    if( oCore.GetType() != CPLJSONObject::Type::Object )
    {
        oIsisCube.Delete( "Core" );
        oCore = CPLJSONObject();
        oIsisCube.Add("Core", oCore);
    }
    oCore.Set( "_type", "object" );

    if( !m_osExternalFilename.empty() )
    {
        if( m_poExternalDS && m_bGeoTIFFAsRegularExternal )
        {
            if( !m_bGeoTIFFInitDone )
            {
                reinterpret_cast<ISIS3WrapperRasterBand*>(GetRasterBand(1))->
                    InitFile();
            }

            const char* pszOffset = m_poExternalDS->GetRasterBand(1)->
                                GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF");
            if( pszOffset )
            {
                oCore.Set( "StartByte", 1 + atoi(pszOffset) );
            }
            else
            {
                // Shouldn't happen normally
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Missing BLOCK_OFFSET_0_0");
                m_bGeoTIFFAsRegularExternal = false;
                oCore.Set( "StartByte", 1 );
            }
        }
        else
        {
            oCore.Set( "StartByte", 1 );
        }
        if( !m_osExternalFilename.empty() )
        {
            const CPLString osExternalFilename = CPLGetFilename(m_osExternalFilename);
            oCore.Set( "^Core", osExternalFilename );
        }
    }
    else
    {
        oCore.Set( "StartByte", pszSTARTBYTE_PLACEHOLDER );
        oCore.Delete( "^Core" );
    }

    if( m_poExternalDS && !m_bGeoTIFFAsRegularExternal )
    {
        oCore.Set( "Format", "GeoTIFF" );
        oCore.Delete( "TileSamples" );
        oCore.Delete( "TileLines" );
    }
    else if( m_bIsTiled )
    {
        oCore.Set( "Format", "Tile");
        int nBlockXSize = 1, nBlockYSize = 1;
        GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
        oCore.Set( "TileSamples", nBlockXSize );
        oCore.Set( "TileLines", nBlockYSize );
    }
    else
    {
        oCore.Set( "Format", "BandSequential" );
        oCore.Delete( "TileSamples" );
        oCore.Delete( "TileLines" );
    }

    CPLJSONObject oDimensions = GetOrCreateJSONObject(oCore, "Dimensions");
    oDimensions.Set( "_type", "group" );
    oDimensions.Set( "Samples", nRasterXSize );
    oDimensions.Set( "Lines", nRasterYSize );
    oDimensions.Set( "Bands", nBands );

    CPLJSONObject oPixels = GetOrCreateJSONObject(oCore, "Pixels");
    oPixels.Set( "_type", "group" );
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    oPixels.Set( "Type",
        (eDT == GDT_Byte) ?   "UnsignedByte" :
        (eDT == GDT_UInt16) ? "UnsignedWord" :
        (eDT == GDT_Int16) ?  "SignedWord" :
                                "Real" );

    oPixels.Set( "ByteOrder", "Lsb" );
    oPixels.Set( "Base", GetRasterBand(1)->GetOffset() );
    oPixels.Set( "Multiplier", GetRasterBand(1)->GetScale() );

    const OGRSpatialReference& oSRS = m_oSRS;

    if( !m_bUseSrcMapping )
    {
        oIsisCube.Delete( "Mapping" );
    }

    CPLJSONObject oMapping = GetOrCreateJSONObject(oIsisCube, "Mapping");
    if( m_bUseSrcMapping && oMapping.IsValid() &&
        oMapping.GetType() == CPLJSONObject::Type::Object )
    {
        if( !m_osTargetName.empty() )
            oMapping.Set( "TargetName", m_osTargetName );
        if( !m_osLatitudeType.empty() )
            oMapping.Set( "LatitudeType", m_osLatitudeType );
        if( !m_osLongitudeDirection.empty() )
            oMapping.Set( "LongitudeDirection", m_osLongitudeDirection );
    }
    else if( !m_bUseSrcMapping && !m_oSRS.IsEmpty() )
    {
        oMapping.Add( "_type", "group" );

        if( oSRS.IsProjected() || oSRS.IsGeographic() )
        {
            const char* pszDatum = oSRS.GetAttrValue("DATUM");
            CPLString osTargetName( m_osTargetName );
            if( osTargetName.empty() )
            {
                if( pszDatum && STARTS_WITH(pszDatum, "D_") )
                {
                    osTargetName = pszDatum + 2;
                }
                else if( pszDatum )
                {
                    osTargetName = pszDatum;
                }
            }
            if( !osTargetName.empty() )
                oMapping.Add( "TargetName", osTargetName );

            oMapping.Add( "EquatorialRadius/value", oSRS.GetSemiMajor() );
            oMapping.Add( "EquatorialRadius/unit", "meters" );
            oMapping.Add( "PolarRadius/value", oSRS.GetSemiMinor() );
            oMapping.Add( "PolarRadius/unit", "meters" );

            if( !m_osLatitudeType.empty() )
                oMapping.Add( "LatitudeType", m_osLatitudeType );
            else
                oMapping.Add( "LatitudeType", "Planetocentric" );

            if( !m_osLongitudeDirection.empty() )
                oMapping.Add( "LongitudeDirection", m_osLongitudeDirection );
            else
                oMapping.Add( "LongitudeDirection", "PositiveEast" );

            double adfX[4] = {0};
            double adfY[4] = {0};
            bool bLongLatCorners = false;
            if( m_bGotTransform )
            {
                for( int i = 0; i < 4; i++ )
                {
                    adfX[i] = m_adfGeoTransform[0] + (i%2) *
                                    nRasterXSize * m_adfGeoTransform[1];
                    adfY[i] = m_adfGeoTransform[3] +
                            ( (i == 0 || i == 3) ? 0 : 1 ) *
                            nRasterYSize * m_adfGeoTransform[5];
                }
                if( oSRS.IsGeographic() )
                {
                    bLongLatCorners = true;
                }
                else
                {
                    OGRSpatialReference* poSRSLongLat = oSRS.CloneGeogCS();
                    if( poSRSLongLat )
                    {
                        poSRSLongLat->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                        OGRCoordinateTransformation* poCT =
                            OGRCreateCoordinateTransformation(&oSRS, poSRSLongLat);
                        if( poCT )
                        {
                            if( poCT->Transform(4, adfX, adfY) )
                            {
                                bLongLatCorners = true;
                            }
                            delete poCT;
                        }
                        delete poSRSLongLat;
                    }
                }
            }
            if( bLongLatCorners )
            {
                for( int i = 0; i < 4; i++ )
                {
                    adfX[i] = FixLong(adfX[i]);
                }
            }

            if( bLongLatCorners && (
                    m_bForce360 || adfX[0] <- 180.0 || adfX[3] > 180.0) )
            {
                oMapping.Add( "LongitudeDomain", 360 );
            }
            else
            {
                oMapping.Add( "LongitudeDomain", 180 );
            }

            if( m_bWriteBoundingDegrees && !m_osBoundingDegrees.empty() )
            {
                char** papszTokens =
                        CSLTokenizeString2(m_osBoundingDegrees, ",", 0);
                if( CSLCount(papszTokens) == 4 )
                {
                    oMapping.Add( "MinimumLatitude", CPLAtof(papszTokens[1]) );
                    oMapping.Add( "MinimumLongitude", CPLAtof(papszTokens[0]) );
                    oMapping.Add( "MaximumLatitude", CPLAtof(papszTokens[3]) );
                    oMapping.Add( "MaximumLongitude", CPLAtof(papszTokens[2]) );
                }
                CSLDestroy(papszTokens);
            }
            else if( m_bWriteBoundingDegrees && bLongLatCorners )
            {
                oMapping.Add( "MinimumLatitude", std::min(
                    std::min(adfY[0], adfY[1]), std::min(adfY[2],adfY[3])) );
                oMapping.Add( "MinimumLongitude", std::min(
                    std::min(adfX[0], adfX[1]), std::min(adfX[2],adfX[3])) );
                oMapping.Add( "MaximumLatitude", std::max(
                    std::max(adfY[0], adfY[1]), std::max(adfY[2],adfY[3])) );
                oMapping.Add( "MaximumLongitude", std::max(
                    std::max(adfX[0], adfX[1]), std::max(adfX[2],adfX[3])) );
            }

            const char* pszProjection = oSRS.GetAttrValue("PROJECTION");
            if( pszProjection == nullptr )
            {
                oMapping.Add( "ProjectionName", "SimpleCylindrical" );
                oMapping.Add( "CenterLongitude", 0.0 );
                oMapping.Add( "CenterLatitude", 0.0 );
                oMapping.Add( "CenterLatitudeRadius", oSRS.GetSemiMajor() );
            }
            else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
            {
                oMapping.Add( "ProjectionName", "Equirectangular" );
                if( oSRS.GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 )
                                                                    != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Ignoring %s. Only 0 value supported",
                             SRS_PP_LATITUDE_OF_ORIGIN);
                }
                oMapping.Add( "CenterLongitude",
                    FixLong(oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                const double dfCenterLat =
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                oMapping.Add( "CenterLatitude", dfCenterLat );

                  // in radians
                const double radLat = dfCenterLat * M_PI / 180;
                const double semi_major = oSRS.GetSemiMajor();
                const double semi_minor = oSRS.GetSemiMinor();
                const double localRadius
                        = semi_major * semi_minor
                        / sqrt( pow( semi_minor * cos( radLat ), 2)
                            + pow( semi_major * sin( radLat ), 2) );
                oMapping.Add( "CenterLatitudeRadius", localRadius );
            }

            else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
            {
                oMapping.Add( "ProjectionName", "Orthographic" );
                oMapping.Add( "CenterLongitude", FixLong(
                    oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) );
            }

            else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
            {
                oMapping.Add( "ProjectionName", "Sinusoidal" );
                oMapping.Add( "CenterLongitude", FixLong(
                    oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0)) );
            }

            else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
            {
                oMapping.Add( "ProjectionName", "Mercator" );
                oMapping.Add( "CenterLongitude", FixLong(
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) );
                oMapping.Add( "scaleFactor",
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0) );
            }

            else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
            {
                oMapping.Add( "ProjectionName", "PolarStereographic" );
                oMapping.Add( "CenterLongitude", FixLong(
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) );
                oMapping.Add( "scaleFactor",
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0) );
            }

            else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
            {
                oMapping.Add( "ProjectionName", "TransverseMercator" );
                oMapping.Add( "CenterLongitude", FixLong(
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) );
                oMapping.Add( "scaleFactor",
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0) );
            }

            else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
            {
                oMapping.Add( "ProjectionName", "LambertConformal" );
                oMapping.Add( "CenterLongitude", FixLong(
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) );
                oMapping.Add( "FirstStandardParallel",
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0) );
                oMapping.Add( "SecondStandardParallel",
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0) );
            }

            else if( EQUAL(pszProjection, "Vertical Perspective") ) // PROJ 7 required
            {
                oMapping.Add( "ProjectionName", "PointPerspective" );
                oMapping.Add( "CenterLongitude", FixLong(
                        oSRS.GetNormProjParm("Longitude of topocentric origin", 0.0)) );
                oMapping.Add( "CenterLatitude",
                        oSRS.GetNormProjParm("Latitude of topocentric origin", 0.0) );
                // ISIS3 value is the distance from center of ellipsoid, in km
                oMapping.Add( "Distance",
                        (oSRS.GetNormProjParm("Viewpoint height", 0.0) + oSRS.GetSemiMajor()) / 1000.0 );
            }

            else if( EQUAL(pszProjection, "custom_proj4") )
            {
                const char* pszProj4 = oSRS.GetExtension("PROJCS", "PROJ4", nullptr);
                if( pszProj4 && strstr(pszProj4, "+proj=ob_tran" ) &&
                    strstr(pszProj4, "+o_proj=eqc") )
                {
                    const auto FetchParam = [](const char* pszProj4Str, const char* pszKey)
                    {
                        CPLString needle;
                        needle.Printf("+%s=", pszKey);
                        const char* pszVal = strstr(pszProj4Str, needle.c_str());
                        if( pszVal )
                            return CPLAtof(pszVal+needle.size());
                        return 0.0;
                    };

                    double dfLonP = FetchParam(pszProj4, "o_lon_p");
                    double dfLatP = FetchParam(pszProj4, "o_lat_p");
                    double dfLon0 = FetchParam(pszProj4, "lon_0");
                    double dfPoleRotation = -dfLonP;
                    double dfPoleLatitude = 180 - dfLatP;
                    double dfPoleLongitude = dfLon0;
                    oMapping.Add( "ProjectionName", "ObliqueCylindrical" );
                    oMapping.Add( "PoleLatitude", dfPoleLatitude );
                    oMapping.Add( "PoleLongitude", FixLong(dfPoleLongitude) );
                    oMapping.Add( "PoleRotation", dfPoleRotation );
                }
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "Projection %s not supported",
                            pszProjection);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Projection %s not supported",
                         pszProjection);
            }


            if( oMapping["ProjectionName"].IsValid() )
            {
                if( oSRS.GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 ) != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Ignoring %s. Only 0 value supported",
                             SRS_PP_FALSE_EASTING);
                }
                if( oSRS.GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Ignoring %s. Only 0 value supported",
                             SRS_PP_FALSE_NORTHING);
                }
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "SRS not supported");
        }
    }

    if( !m_bUseSrcMapping && m_bGotTransform )
    {
        oMapping.Add( "_type", "group" );

        const double dfDegToMeter = oSRS.GetSemiMajor() * M_PI / 180.0;
        if( !m_oSRS.IsEmpty() && oSRS.IsProjected() )
        {
            const double dfLinearUnits = oSRS.GetLinearUnits();
            // Maybe we should deal differently with non meter units ?
            const double dfRes = m_adfGeoTransform[1] * dfLinearUnits;
            const double dfScale = dfDegToMeter / dfRes;
            oMapping.Add( "UpperLeftCornerX", m_adfGeoTransform[0] );
            oMapping.Add( "UpperLeftCornerY", m_adfGeoTransform[3] );
            oMapping.Add( "PixelResolution/value", dfRes );
            oMapping.Add( "PixelResolution/unit", "meters/pixel" );
            oMapping.Add( "Scale/value", dfScale );
            oMapping.Add( "Scale/unit", "pixels/degree" );
        }
        else if( !m_oSRS.IsEmpty() && oSRS.IsGeographic() )
        {
            const double dfScale = 1.0 / m_adfGeoTransform[1];
            const double dfRes = m_adfGeoTransform[1] * dfDegToMeter;
            oMapping.Add( "UpperLeftCornerX", m_adfGeoTransform[0] * dfDegToMeter );
            oMapping.Add( "UpperLeftCornerY", m_adfGeoTransform[3] * dfDegToMeter );
            oMapping.Add( "PixelResolution/value", dfRes );
            oMapping.Add( "PixelResolution/unit", "meters/pixel" );
            oMapping.Add( "Scale/value", dfScale );
            oMapping.Add( "Scale/unit", "pixels/degree" );
        }
        else
        {
            oMapping.Add( "UpperLeftCornerX", m_adfGeoTransform[0] );
            oMapping.Add( "UpperLeftCornerY", m_adfGeoTransform[3] );
            oMapping.Add( "PixelResolution", m_adfGeoTransform[1] );
        }
    }

    CPLJSONObject oLabelLabel = GetOrCreateJSONObject(oLabel, "Label");
    oLabelLabel.Set( "_type", "object" );
    oLabelLabel.Set( "Bytes", pszLABEL_BYTES_PLACEHOLDER );

    // Deal with History object
    BuildHistory();

    oLabel.Delete( "History" );
    if( !m_osHistory.empty() )
    {
        CPLJSONObject oHistory;
        oHistory.Add( "_type", "object" );
        oHistory.Add( "Name", "IsisCube" );
        if( m_osExternalFilename.empty() )
            oHistory.Add( "StartByte", pszHISTORY_STARTBYTE_PLACEHOLDER );
        else
            oHistory.Add( "StartByte", 1 );
        oHistory.Add( "Bytes", static_cast<GIntBig>(m_osHistory.size()) );
        if( !m_osExternalFilename.empty() )
        {
            CPLString osFilename(CPLGetBasename(GetDescription()));
            osFilename += ".History.IsisCube";
            oHistory.Add( "^History", osFilename );
        }
        oLabel.Add( "History", oHistory );
    }

    // Deal with other objects that have StartByte & Bytes
    m_aoNonPixelSections.clear();
    if( m_oSrcJSonLabel.IsValid() )
    {
        CPLString osLabelSrcFilename;
        CPLJSONObject oFilename = oLabel["_filename"];
        if( oFilename.GetType() == CPLJSONObject::Type::String )
        {
            osLabelSrcFilename = oFilename.ToString();
        }

        for( CPLJSONObject& oObj : oLabel.GetChildren() )
        {
            CPLString osKey = oObj.GetName();
            if( osKey == "History" )
            {
                continue;
            }

            CPLJSONObject oBytes = oObj.GetObj( "Bytes" );
            if( oBytes.GetType() != CPLJSONObject::Type::Integer ||
                oBytes.ToInteger() <= 0 )
            {
                continue;
            }

            CPLJSONObject oStartByte = oObj.GetObj( "StartByte" );
            if( oStartByte.GetType() != CPLJSONObject::Type::Integer ||
                oStartByte.ToInteger() <= 0 )
            {
                continue;
            }

            if( osLabelSrcFilename.empty() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Cannot find _filename attribute in "
                        "source ISIS3 metadata. Removing object "
                        "%s from the label.",
                        osKey.c_str());
                oLabel.Delete( osKey );
                continue;
            }

            NonPixelSection oSection;
            oSection.osSrcFilename = osLabelSrcFilename;
            oSection.nSrcOffset = static_cast<vsi_l_offset>(
                oObj.GetInteger("StartByte")) - 1U;
            oSection.nSize = static_cast<vsi_l_offset>(
                oObj.GetInteger("Bytes"));

            CPLString osName;
            CPLJSONObject oName = oObj.GetObj( "Name" );
            if( oName.GetType() == CPLJSONObject::Type::String )
            {
                osName = oName.ToString();
            }

            CPLString osContainerName(osKey);
            CPLJSONObject oContainerName = oObj.GetObj( "_container_name" );
            if( oContainerName.GetType() == CPLJSONObject::Type::String )
            {
                osContainerName = oContainerName.ToString();
            }

            const CPLString osKeyFilename( "^" + osContainerName );
            CPLJSONObject oFilenameCap = oObj.GetObj( osKeyFilename );
            if( oFilenameCap.GetType() == CPLJSONObject::Type::String )
            {
                VSIStatBufL sStat;
                const CPLString osSrcFilename( CPLFormFilename(
                    CPLGetPath(osLabelSrcFilename),
                    oFilenameCap.ToString().c_str(),
                    nullptr ) );
                if( VSIStatL( osSrcFilename, &sStat ) == 0 )
                {
                    oSection.osSrcFilename = osSrcFilename;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Object %s points to %s, which does "
                             "not exist. Removing this section "
                             "from the label",
                             osKey.c_str(),
                             osSrcFilename.c_str());
                    oLabel.Delete( osKey );
                    continue;
                }
            }

            if( !m_osExternalFilename.empty() )
            {
                oObj.Set( "StartByte", 1 );
            }
            else
            {
                CPLString osPlaceHolder;
                osPlaceHolder.Printf(
                    "!*^PLACEHOLDER_%d_STARTBYTE^*!",
                    static_cast<int>(m_aoNonPixelSections.size()) + 1);
                oObj.Set( "StartByte", osPlaceHolder );
                oSection.osPlaceHolder = osPlaceHolder;
            }

            if( !m_osExternalFilename.empty() )
            {
                CPLString osDstFilename( CPLGetBasename(GetDescription()) );
                osDstFilename += ".";
                osDstFilename += osContainerName;
                if( !osName.empty() )
                {
                    osDstFilename += ".";
                    osDstFilename += osName;
                }

                oSection.osDstFilename = CPLFormFilename(
                    CPLGetPath( GetDescription() ),
                    osDstFilename,
                    nullptr );

                oObj.Set( osKeyFilename, osDstFilename );
            }
            else
            {
                oObj.Delete( osKeyFilename );
            }

            m_aoNonPixelSections.push_back(oSection);
        }
    }
    m_oJSonLabel = oLabel;
}

/************************************************************************/
/*                         BuildHistory()                               */
/************************************************************************/

void ISIS3Dataset::BuildHistory()
{
    CPLString osHistory;

    if( m_oSrcJSonLabel.IsValid() && m_bUseSrcHistory )
    {
        vsi_l_offset nHistoryOffset = 0;
        int nHistorySize = 0;
        CPLString osSrcFilename;

        CPLJSONObject oFilename = m_oSrcJSonLabel["_filename"];
        if( oFilename.GetType() == CPLJSONObject::Type::String )
        {
            osSrcFilename = oFilename.ToString();
        }
        CPLString osHistoryFilename(osSrcFilename);
        CPLJSONObject oHistory = m_oSrcJSonLabel["History"];
        if( oHistory.GetType() == CPLJSONObject::Type::Object )
        {
            CPLJSONObject oHistoryFilename = oHistory["^History"];
            if( oHistoryFilename.GetType() == CPLJSONObject::Type::String )
            {
                osHistoryFilename =
                        CPLFormFilename( CPLGetPath(osSrcFilename),
                                         oHistoryFilename.ToString().c_str(),
                                         nullptr );
            }

            CPLJSONObject oStartByte = oHistory["StartByte"];
            if( oStartByte.GetType() == CPLJSONObject::Type::Integer )
            {
                if( oStartByte.ToInteger() > 0 )
                {
                    nHistoryOffset = static_cast<vsi_l_offset>(
                        oStartByte.ToInteger()) - 1U;
                }
            }

            CPLJSONObject oBytes = oHistory["Bytes"];
            if( oBytes.GetType() == CPLJSONObject::Type::Integer )
            {
                nHistorySize = static_cast<int>( oBytes.ToInteger() );
            }
        }

        if( osHistoryFilename.empty() )
        {
            CPLDebug("ISIS3", "Cannot find filename for source history");
        }
        else if( nHistorySize <= 0 || nHistorySize > 1000000 )
        {
            CPLDebug("ISIS3",
                     "Invalid or missing value for History.Bytes "
                     "for source history");
        }
        else
        {
            VSILFILE* fpHistory = VSIFOpenL(osHistoryFilename, "rb");
            if( fpHistory != nullptr )
            {
                VSIFSeekL(fpHistory, nHistoryOffset, SEEK_SET);
                osHistory.resize( nHistorySize );
                if( VSIFReadL( &osHistory[0], nHistorySize, 1,
                              fpHistory ) != 1 )
                {
                    CPLError(CE_Warning, CPLE_FileIO,
                             "Cannot read %d bytes at offset " CPL_FRMT_GUIB
                             "of %s: history will not be preserved",
                             nHistorySize, nHistoryOffset,
                             osHistoryFilename.c_str());
                    osHistory.clear();
                }
                VSIFCloseL(fpHistory);
            }
            else
            {
                CPLError(CE_Warning, CPLE_FileIO,
                         "Cannot open %s: history will not be preserved",
                         osHistoryFilename.c_str());
            }
        }
    }

    if( m_bAddGDALHistory && !m_osGDALHistory.empty() )
    {
        if( !osHistory.empty() )
            osHistory += "\n";
        osHistory += m_osGDALHistory;
    }
    else if( m_bAddGDALHistory )
    {
        if( !osHistory.empty() )
            osHistory += "\n";

        CPLJSONObject oHistoryObj;
        char szFullFilename[2048] = { 0 };
        if( !CPLGetExecPath(szFullFilename, sizeof(szFullFilename) - 1) )
            strcpy(szFullFilename, "unknown_program");
        const CPLString osProgram(CPLGetBasename(szFullFilename));
        const CPLString osPath(CPLGetPath(szFullFilename));

        CPLJSONObject oObj;
        oHistoryObj.Add( osProgram, oObj );

        oObj.Add( "_type", "object" );
        oObj.Add( "GdalVersion", GDALVersionInfo("RELEASE_NAME") );
        if( osPath != "." )
            oObj.Add( "ProgramPath", osPath );
        time_t nCurTime = time(nullptr);
        if( nCurTime != -1 )
        {
            struct tm mytm;
            CPLUnixTimeToYMDHMS(nCurTime, &mytm);
            oObj.Add( "ExecutionDateTime",
                CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02d",
                            mytm.tm_year + 1900,
                            mytm.tm_mon + 1,
                            mytm.tm_mday,
                            mytm.tm_hour,
                            mytm.tm_min,
                            mytm.tm_sec) );
        }
        char szHostname[256] = { 0 };
        if( gethostname(szHostname, sizeof(szHostname)-1) == 0 )
        {
            oObj.Add( "HostName", std::string(szHostname) );
        }
        const char* pszUsername = CPLGetConfigOption("USERNAME", nullptr);
        if( pszUsername == nullptr )
            pszUsername = CPLGetConfigOption("USER", nullptr);
        if( pszUsername != nullptr )
        {
            oObj.Add( "UserName", pszUsername );
        }
        oObj.Add( "Description", "GDAL conversion" );

        CPLJSONObject oUserParameters;
        oObj.Add( "UserParameters", oUserParameters );

        oUserParameters.Add( "_type", "group");
        if( !m_osFromFilename.empty() )
        {
            const CPLString osFromFilename = CPLGetFilename( m_osFromFilename );
            oUserParameters.Add( "FROM", osFromFilename );
        }
        if( nullptr != GetDescription() )
        {
            const CPLString osToFileName = CPLGetFilename( GetDescription() );
            oUserParameters.Add( "TO", osToFileName );
        }
        if( m_bForce360 )
            oUserParameters.Add( "Force_360", "true");

        osHistory += SerializeAsPDL( oHistoryObj );
    }

    m_osHistory = osHistory;
}

/************************************************************************/
/*                           WriteLabel()                               */
/************************************************************************/

void ISIS3Dataset::WriteLabel()
{
    m_bIsLabelWritten = true;

    if( !m_oJSonLabel.IsValid() )
        BuildLabel();

    // Serialize label
    CPLString osLabel( SerializeAsPDL(m_oJSonLabel) );
    osLabel += "End\n";
    if( m_osExternalFilename.empty() && osLabel.size() < 65536 )
    {
        // In-line labels have conventionally a minimize size of 65536 bytes
        // See #2741
        osLabel.resize(65536);
    }
    char *pszLabel = &osLabel[0];
    const int nLabelSize = static_cast<int>(osLabel.size());

    // Hack back StartByte value
    {
        char *pszStartByte = strstr(pszLabel, pszSTARTBYTE_PLACEHOLDER);
        if( pszStartByte != nullptr )
        {
            const char* pszOffset = CPLSPrintf("%d", 1 + nLabelSize);
            memcpy(pszStartByte, pszOffset, strlen(pszOffset));
            memset(pszStartByte + strlen(pszOffset), ' ',
                    strlen(pszSTARTBYTE_PLACEHOLDER) - strlen(pszOffset));
        }
    }

    // Hack back Label.Bytes value
    {
        char* pszLabelBytes = strstr(pszLabel, pszLABEL_BYTES_PLACEHOLDER);
        if( pszLabelBytes != nullptr )
        {
            const char* pszBytes = CPLSPrintf("%d", nLabelSize);
            memcpy(pszLabelBytes, pszBytes, strlen(pszBytes));
            memset(pszLabelBytes + strlen(pszBytes), ' ',
                    strlen(pszLABEL_BYTES_PLACEHOLDER) - strlen(pszBytes));
        }
    }

    const GDALDataType eType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eType);
    vsi_l_offset nImagePixels = 0;
    if( m_poExternalDS == nullptr )
    {
        if( m_bIsTiled )
        {
            int nBlockXSize = 1, nBlockYSize = 1;
            GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
            nImagePixels = static_cast<vsi_l_offset>(nBlockXSize) *
                            nBlockYSize * nBands *
                            DIV_ROUND_UP(nRasterXSize, nBlockXSize) *
                            DIV_ROUND_UP(nRasterYSize, nBlockYSize);
        }
        else
        {
            nImagePixels = static_cast<vsi_l_offset>(nRasterXSize) *
                            nRasterYSize * nBands;
        }
    }

    // Hack back History.StartBytes value
    char* pszHistoryStartBytes = strstr(pszLabel,
                                        pszHISTORY_STARTBYTE_PLACEHOLDER);

    vsi_l_offset nHistoryOffset = 0;
    vsi_l_offset nLastOffset = 0;
    if( pszHistoryStartBytes != nullptr )
    {
        CPLAssert( m_osExternalFilename.empty() );
        nHistoryOffset = nLabelSize + nImagePixels * nDTSize;
        nLastOffset = nHistoryOffset + m_osHistory.size();
        const char* pszStartByte = CPLSPrintf(CPL_FRMT_GUIB,
                                              nHistoryOffset + 1);
        CPLAssert(strlen(pszStartByte) <
                                    strlen(pszHISTORY_STARTBYTE_PLACEHOLDER));
        memcpy(pszHistoryStartBytes, pszStartByte, strlen(pszStartByte));
        memset(pszHistoryStartBytes + strlen(pszStartByte), ' ',
               strlen(pszHISTORY_STARTBYTE_PLACEHOLDER) - strlen(pszStartByte));
    }

    // Replace placeholders in other sections
    for( size_t i = 0; i < m_aoNonPixelSections.size(); ++i )
    {
        if( !m_aoNonPixelSections[i].osPlaceHolder.empty() )
        {
            char* pszPlaceHolder = strstr(pszLabel,
                            m_aoNonPixelSections[i].osPlaceHolder.c_str());
            CPLAssert( pszPlaceHolder != nullptr );
            const char* pszStartByte = CPLSPrintf(CPL_FRMT_GUIB,
                                                  nLastOffset + 1);
            nLastOffset += m_aoNonPixelSections[i].nSize;
            CPLAssert(strlen(pszStartByte) <
                            m_aoNonPixelSections[i].osPlaceHolder.size() );

            memcpy(pszPlaceHolder, pszStartByte, strlen(pszStartByte));
            memset(pszPlaceHolder + strlen(pszStartByte), ' ',
                m_aoNonPixelSections[i].osPlaceHolder.size() -
                                                            strlen(pszStartByte));
        }
    }

    // Write to final file
    VSIFSeekL( m_fpLabel, 0, SEEK_SET );
    VSIFWriteL( pszLabel, 1, osLabel.size(), m_fpLabel);

    if( m_osExternalFilename.empty() )
    {
        // Update image offset in bands
        if( m_bIsTiled )
        {
            for(int i=0;i<nBands;i++)
            {
                ISISTiledBand* poBand =
                    reinterpret_cast<ISISTiledBand*>(GetRasterBand(i+1));
                poBand->m_nFirstTileOffset += nLabelSize;
            }
        }
        else
        {
            for(int i=0;i<nBands;i++)
            {
                ISIS3RawRasterBand* poBand =
                    reinterpret_cast<ISIS3RawRasterBand*>(GetRasterBand(i+1));
                poBand->nImgOffset += nLabelSize;
            }
        }
    }

    if( m_bInitToNodata )
    {
        // Initialize the image to nodata
        const double dfNoData = GetRasterBand(1)->GetNoDataValue();
        if( dfNoData == 0.0 )
        {
            VSIFTruncateL( m_fpImage, VSIFTellL(m_fpImage) +
                                                nImagePixels * nDTSize );
        }
        else if( nDTSize != 0 ) // to make Coverity not warn about div by 0
        {
            const int nPageSize = 4096; // Must be multiple of 4 since
                                        // Float32 is the largest type
            CPLAssert( (nPageSize % nDTSize) == 0 );
            const int nMaxPerPage = nPageSize / nDTSize;
            GByte* pabyTemp = static_cast<GByte*>(CPLMalloc(nPageSize));
            GDALCopyWords( &dfNoData, GDT_Float64, 0,
                           pabyTemp, eType, nDTSize,
                           nMaxPerPage );
#ifdef CPL_MSB
            GDALSwapWords( pabyTemp, nDTSize, nMaxPerPage, nDTSize );
#endif
            for( vsi_l_offset i = 0; i < nImagePixels; i += nMaxPerPage )
            {
                int n;
                if( i + nMaxPerPage <= nImagePixels )
                    n = nMaxPerPage;
                else
                    n = static_cast<int>(nImagePixels - i);
                if( VSIFWriteL( pabyTemp, n * nDTSize, 1, m_fpImage ) != 1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                            "Cannot initialize imagery to null");
                    break;
                }
            }

            CPLFree( pabyTemp );
        }
    }

    // Write history
    if( !m_osHistory.empty() )
    {
        if( m_osExternalFilename.empty() )
        {
            VSIFSeekL( m_fpLabel, nHistoryOffset, SEEK_SET );
            VSIFWriteL( m_osHistory.c_str(), 1, m_osHistory.size(), m_fpLabel);
        }
        else
        {
            CPLString osFilename(CPLGetBasename(GetDescription()));
            osFilename += ".History.IsisCube";
            osFilename =
              CPLFormFilename(CPLGetPath(GetDescription()), osFilename, nullptr);
            VSILFILE* fp = VSIFOpenL(osFilename, "wb");
            if( fp )
            {
                m_aosAdditionalFiles.AddString(osFilename);

                VSIFWriteL( m_osHistory.c_str(), 1,
                            m_osHistory.size(), fp );
                VSIFCloseL(fp);
            }
            else
            {
                CPLError(CE_Warning, CPLE_FileIO,
                         "Cannot write %s", osFilename.c_str());
            }
        }
    }

    // Write other non pixel sections
    for( size_t i = 0; i < m_aoNonPixelSections.size(); ++i )
    {
        VSILFILE* fpSrc = VSIFOpenL(
                        m_aoNonPixelSections[i].osSrcFilename, "rb");
        if( fpSrc == nullptr )
        {
            CPLError(CE_Warning, CPLE_FileIO,
                        "Cannot open %s",
                        m_aoNonPixelSections[i].osSrcFilename.c_str());
            continue;
        }

        VSILFILE* fpDest = m_fpLabel;
        if( !m_aoNonPixelSections[i].osDstFilename.empty() )
        {
            fpDest = VSIFOpenL(m_aoNonPixelSections[i].osDstFilename, "wb");
            if( fpDest == nullptr )
            {
                CPLError(CE_Warning, CPLE_FileIO,
                         "Cannot create %s",
                         m_aoNonPixelSections[i].osDstFilename.c_str());
                VSIFCloseL(fpSrc);
                continue;
            }

            m_aosAdditionalFiles.AddString(
                                m_aoNonPixelSections[i].osDstFilename);
        }

        VSIFSeekL(fpSrc, m_aoNonPixelSections[i].nSrcOffset, SEEK_SET);
        GByte abyBuffer[4096];
        vsi_l_offset nRemaining = m_aoNonPixelSections[i].nSize;
        while( nRemaining )
        {
            size_t nToRead = 4096;
            if( nRemaining < nToRead )
                nToRead = static_cast<size_t>(nRemaining);
            size_t nRead = VSIFReadL( abyBuffer, 1, nToRead, fpSrc );
            if( nRead != nToRead )
            {
                CPLError(CE_Warning, CPLE_FileIO,
                         "Could not read " CPL_FRMT_GUIB " bytes from %s",
                         m_aoNonPixelSections[i].nSize,
                         m_aoNonPixelSections[i].osSrcFilename.c_str());
                break;
            }
            VSIFWriteL( abyBuffer, 1, nRead, fpDest );
            nRemaining -= nRead;
        }

        VSIFCloseL( fpSrc );
        if( fpDest != m_fpLabel )
            VSIFCloseL(fpDest);
    }
}

/************************************************************************/
/*                      SerializeAsPDL()                                */
/************************************************************************/

CPLString ISIS3Dataset::SerializeAsPDL( const CPLJSONObject &oObj )
{
    CPLString osTmpFile( CPLSPrintf("/vsimem/isis3_%p", oObj.GetInternalHandle()) );
    VSILFILE* fpTmp = VSIFOpenL( osTmpFile, "wb+" );
    SerializeAsPDL( fpTmp, oObj );
    VSIFCloseL( fpTmp );
    CPLString osContent( reinterpret_cast<char*>(
                            VSIGetMemFileBuffer( osTmpFile, nullptr, FALSE )) );
    VSIUnlink(osTmpFile);
    return osContent;
}

/************************************************************************/
/*                      SerializeAsPDL()                                */
/************************************************************************/

void ISIS3Dataset::SerializeAsPDL( VSILFILE* fp, const CPLJSONObject &oObj,
                                   int nDepth )
{
    CPLString osIndentation;
    for( int i = 0; i < nDepth; i++ )
        osIndentation += "  ";
    const size_t WIDTH = 79;

    std::vector<CPLJSONObject> aoChildren = oObj.GetChildren();
    size_t nMaxKeyLength = 0;
    for( const CPLJSONObject& oChild : aoChildren )
    {
        const CPLString osKey = oChild.GetName();
        if( EQUAL(osKey, "_type") ||
            EQUAL(osKey, "_container_name") ||
            EQUAL(osKey, "_filename") )
        {
            continue;
        }

        const auto eType = oChild.GetType();
        if( eType == CPLJSONObject::Type::String ||
            eType == CPLJSONObject::Type::Integer ||
            eType == CPLJSONObject::Type::Double ||
            eType == CPLJSONObject::Type::Array )
        {
            if( osKey.size() > nMaxKeyLength )
            {
                nMaxKeyLength = osKey.size();
            }
        }
        else if( eType == CPLJSONObject::Type::Object )
        {
            CPLJSONObject oValue = oChild.GetObj( "value" );
            CPLJSONObject oUnit = oChild.GetObj( "unit" );
            if( oValue.IsValid() && oUnit.GetType() == CPLJSONObject::Type::String )
            {
                if( osKey.size() > nMaxKeyLength )
                {
                    nMaxKeyLength = osKey.size();
                }
            }
        }
    }

    for( const CPLJSONObject& oChild : aoChildren )
    {
        const CPLString osKey = oChild.GetName();
        if( EQUAL(osKey, "_type") ||
            EQUAL(osKey, "_container_name") ||
            EQUAL(osKey, "_filename") )
        {
            continue;
        }
        if( STARTS_WITH(osKey, "_comment") )
        {
            if( oChild.GetType() == CPLJSONObject::Type::String )
            {
                VSIFPrintfL(fp, "#%s\n", oChild.ToString().c_str() );
            }
            continue;
        }
        CPLString osPadding;
        size_t nLen = osKey.size();
        if( nLen < nMaxKeyLength )
        {
            osPadding.append( nMaxKeyLength - nLen, ' ' );
        }

        const auto eType = oChild.GetType();
        if( eType == CPLJSONObject::Type::Object )
        {
            CPLJSONObject oType = oChild.GetObj( "_type" );
            CPLJSONObject oContainerName = oChild.GetObj( "_container_name" );
            CPLString osContainerName = osKey;
            if( oContainerName.GetType() == CPLJSONObject::Type::String )
            {
                osContainerName = oContainerName.ToString();
            }
            if( oType.GetType() == CPLJSONObject::Type::String )
            {
                const CPLString osType = oType.ToString();
                if( EQUAL(osType, "Object") )
                {
                    if( nDepth == 0 && VSIFTellL(fp) != 0 )
                        VSIFPrintfL(fp, "\n");
                    VSIFPrintfL(fp, "%sObject = %s\n",
                                osIndentation.c_str(), osContainerName.c_str());
                    SerializeAsPDL( fp, oChild, nDepth + 1 );
                    VSIFPrintfL(fp, "%sEnd_Object\n", osIndentation.c_str());
                }
                else if( EQUAL(osType, "Group") )
                {
                    VSIFPrintfL(fp, "\n");
                    VSIFPrintfL(fp, "%sGroup = %s\n",
                                osIndentation.c_str(), osContainerName.c_str());
                    SerializeAsPDL( fp, oChild, nDepth + 1 );
                    VSIFPrintfL(fp, "%sEnd_Group\n", osIndentation.c_str());
                }
            }
            else
            {
                CPLJSONObject oValue = oChild.GetObj( "value" );
                CPLJSONObject oUnit = oChild.GetObj( "unit" );
                if( oValue.IsValid() &&
                    oUnit.GetType() == CPLJSONObject::Type::String )
                {
                    const CPLString osUnit = oUnit.ToString();
                    const auto eValueType = oValue.GetType();
                    if( eValueType == CPLJSONObject::Type::Integer )
                    {
                        VSIFPrintfL(fp, "%s%s%s = %d <%s>\n",
                                    osIndentation.c_str(), osKey.c_str(),
                                    osPadding.c_str(),
                                    oValue.ToInteger(), osUnit.c_str());
                    }
                    else if( eValueType == CPLJSONObject::Type::Double )
                    {
                        const double dfVal = oValue.ToDouble();
                        if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                            static_cast<int>(dfVal) == dfVal )
                        {
                            VSIFPrintfL(fp, "%s%s%s = %d.0 <%s>\n",
                                        osIndentation.c_str(), osKey.c_str(),
                                        osPadding.c_str(),
                                        static_cast<int>(dfVal), osUnit.c_str());
                        }
                        else
                        {
                            VSIFPrintfL(fp, "%s%s%s = %.18g <%s>\n",
                                        osIndentation.c_str(), osKey.c_str(),
                                        osPadding.c_str(),
                                        dfVal, osUnit.c_str());
                        }
                    }
                }
            }
        }
        else if( eType == CPLJSONObject::Type::String )
        {
            CPLString osVal = oChild.ToString();
            const char* pszVal = osVal.c_str();
            if( pszVal[0] == '\0' ||
                strchr(pszVal, ' ') || strstr(pszVal, "\\n") ||
                strstr(pszVal, "\\r") )
            {
                osVal.replaceAll("\\n", "\n");
                osVal.replaceAll("\\r", "\r");
                VSIFPrintfL(fp, "%s%s%s = \"%s\"\n",
                            osIndentation.c_str(), osKey.c_str(),
                            osPadding.c_str(), osVal.c_str());
            }
            else
            {
                if( osIndentation.size() + osKey.size() + osPadding.size() +
                    strlen(" = ") + strlen(pszVal) > WIDTH &&
                    osIndentation.size() + osKey.size() + osPadding.size() +
                    strlen(" = ") < WIDTH )
                {
                    size_t nFirstPos = osIndentation.size() + osKey.size() +
                                     osPadding.size() + strlen(" = ");
                    VSIFPrintfL(fp, "%s%s%s = ",
                                osIndentation.c_str(), osKey.c_str(),
                                osPadding.c_str());
                    size_t nCurPos = nFirstPos;
                    for( int j = 0; pszVal[j] != '\0'; j++ )
                    {
                        nCurPos ++;
                        if( nCurPos == WIDTH && pszVal[j+1] != '\0' )
                        {
                            VSIFPrintfL( fp, "-\n" );
                            for( size_t k=0;k<nFirstPos;k++ )
                            {
                                const char chSpace = ' ';
                                VSIFWriteL(&chSpace, 1, 1, fp);
                            }
                            nCurPos = nFirstPos + 1;
                        }
                        VSIFWriteL( &pszVal[j], 1, 1, fp );
                    }
                    VSIFPrintfL(fp, "\n");
                }
                else
                {
                    VSIFPrintfL(fp, "%s%s%s = %s\n",
                                osIndentation.c_str(), osKey.c_str(),
                                osPadding.c_str(), pszVal);
                }
            }
        }
        else if( eType == CPLJSONObject::Type::Integer )
        {
            const int nVal = oChild.ToInteger();
            VSIFPrintfL(fp, "%s%s%s = %d\n",
                        osIndentation.c_str(), osKey.c_str(),
                        osPadding.c_str(), nVal);
        }
        else if( eType == CPLJSONObject::Type::Double )
        {
            const double dfVal = oChild.ToDouble();
            if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                static_cast<int>(dfVal) == dfVal )
            {
                VSIFPrintfL(fp, "%s%s%s = %d.0\n",
                        osIndentation.c_str(), osKey.c_str(),
                        osPadding.c_str(), static_cast<int>(dfVal));
            }
            else
            {
                VSIFPrintfL(fp, "%s%s%s = %.18g\n",
                            osIndentation.c_str(), osKey.c_str(),
                            osPadding.c_str(), dfVal);
            }
        }
        else if( eType == CPLJSONObject::Type::Array )
        {
            CPLJSONArray oArrayItem(oChild);
            const int nLength = oArrayItem.Size();
            size_t nFirstPos = osIndentation.size() + osKey.size() +
                                     osPadding.size() + strlen(" = (");
            VSIFPrintfL(fp, "%s%s%s = (",
                        osIndentation.c_str(), osKey.c_str(),
                        osPadding.c_str());
            size_t nCurPos = nFirstPos;
            for( int idx = 0; idx < nLength; idx++ )
            {
                CPLJSONObject oItem = oArrayItem[idx];
                const auto eArrayItemType = oItem.GetType();
                if( eArrayItemType == CPLJSONObject::Type::String )
                {
                    CPLString osVal = oItem.ToString();
                    const char* pszVal = osVal.c_str();
                    if( pszVal[0] == '\0' ||
                        strchr(pszVal, ' ') || strstr(pszVal, "\\n") ||
                        strstr(pszVal, "\\r") )
                    {
                        osVal.replaceAll("\\n", "\n");
                        osVal.replaceAll("\\r", "\r");
                        VSIFPrintfL(fp, "\"%s\"", osVal.c_str());
                    }
                    else if( nFirstPos < WIDTH && nCurPos + strlen(pszVal) > WIDTH )
                    {
                        if( idx > 0 )
                        {
                            VSIFPrintfL( fp, "\n" );
                            for( size_t j=0;j<nFirstPos;j++ )
                            {
                                const char chSpace = ' ';
                                VSIFWriteL(&chSpace, 1, 1, fp);
                            }
                            nCurPos = nFirstPos;
                        }

                        for( int j = 0; pszVal[j] != '\0'; j++ )
                        {
                            nCurPos ++;
                            if( nCurPos == WIDTH && pszVal[j+1] != '\0' )
                            {
                                VSIFPrintfL( fp, "-\n" );
                                for( size_t k=0;k<nFirstPos;k++ )
                                {
                                    const char chSpace = ' ';
                                    VSIFWriteL(&chSpace, 1, 1, fp);
                                }
                                nCurPos = nFirstPos + 1;
                            }
                            VSIFWriteL( &pszVal[j], 1, 1, fp );
                        }
                    }
                    else
                    {
                        VSIFPrintfL( fp, "%s", pszVal );
                        nCurPos += strlen(pszVal);
                    }
                }
                else if( eArrayItemType == CPLJSONObject::Type::Integer )
                {
                    const int nVal = oItem.ToInteger();
                    const char* pszVal = CPLSPrintf("%d", nVal);
                    const size_t nValLen = strlen(pszVal);
                    if( nFirstPos < WIDTH && idx > 0 &&
                        nCurPos + nValLen > WIDTH )
                    {
                        VSIFPrintfL( fp, "\n" );
                        for( size_t j=0;j<nFirstPos;j++ )
                        {
                            const char chSpace = ' ';
                            VSIFWriteL(&chSpace, 1, 1, fp);
                        }
                        nCurPos = nFirstPos;
                    }
                    VSIFPrintfL( fp, "%d", nVal );
                    nCurPos += nValLen;
                }
                else if( eArrayItemType == CPLJSONObject::Type::Double )
                {
                    const double dfVal = oItem.ToDouble();
                    CPLString osVal;
                    if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                        static_cast<int>(dfVal) == dfVal )
                    {
                        osVal = CPLSPrintf("%d.0", static_cast<int>(dfVal));
                    }
                    else
                    {
                        osVal = CPLSPrintf("%.18g", dfVal);
                    }
                    const size_t nValLen = osVal.size();
                    if( nFirstPos < WIDTH && idx > 0 &&
                        nCurPos + nValLen > WIDTH )
                    {
                        VSIFPrintfL( fp, "\n" );
                        for( size_t j=0;j<nFirstPos;j++ )
                        {
                            const char chSpace = ' ';
                            VSIFWriteL(&chSpace, 1, 1, fp);
                        }
                        nCurPos = nFirstPos;
                    }
                    VSIFPrintfL( fp, "%s", osVal.c_str() );
                    nCurPos += nValLen;
                }
                if( idx < nLength - 1 )
                {
                    VSIFPrintfL( fp, ", " );
                    nCurPos += 2;
                }
            }
            VSIFPrintfL(fp, ")\n" );
        }
    }
}

/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

GDALDataset *ISIS3Dataset::Create(const char* pszFilename,
                                  int nXSize, int nYSize, int nBandsIn,
                                  GDALDataType eType,
                                  char** papszOptions)
{
    if( eType != GDT_Byte && eType != GDT_UInt16 && eType != GDT_Int16 &&
        eType != GDT_Float32 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type");
        return nullptr;
    }
    if( nBandsIn == 0 || nBandsIn > 32767 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    const char* pszDataLocation = CSLFetchNameValueDef(papszOptions,
                                                       "DATA_LOCATION",
                                                       "LABEL");
    const bool bIsTiled = CPLFetchBool(papszOptions, "TILED", false);
    const int nBlockXSize = std::max(1,
            atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "256")));
    const int nBlockYSize = std::max(1,
            atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "256")));
    if( !EQUAL(pszDataLocation, "LABEL") &&
        !EQUAL( CPLGetExtension(pszFilename), "LBL") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "For DATA_LOCATION=%s, "
                    "the main filename should have a .lbl extension",
                    pszDataLocation);
        return nullptr;
    }

    VSILFILE* fp = VSIFOpenExL(pszFilename, "wb", true);
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Cannot create %s: %s",
                  pszFilename, VSIGetLastErrorMsg() );
        return nullptr;
    }
    VSILFILE* fpImage = nullptr;
    CPLString osExternalFilename;
    GDALDataset* poExternalDS = nullptr;
    bool bGeoTIFFAsRegularExternal = false;
    if( EQUAL(pszDataLocation, "EXTERNAL") )
    {
        osExternalFilename = CSLFetchNameValueDef(papszOptions,
                                        "EXTERNAL_FILENAME",
                                        CPLResetExtension(pszFilename, "cub"));
        fpImage = VSIFOpenExL(osExternalFilename, "wb", true);
        if( fpImage == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot create %s: %s",
                      osExternalFilename.c_str(), VSIGetLastErrorMsg() );
            VSIFCloseL(fp);
            return nullptr;
        }
    }
    else if( EQUAL(pszDataLocation, "GEOTIFF") )
    {
        osExternalFilename = CSLFetchNameValueDef(papszOptions,
                                        "EXTERNAL_FILENAME",
                                        CPLResetExtension(pszFilename, "tif"));
        GDALDriver* poDrv = static_cast<GDALDriver*>(
                                            GDALGetDriverByName("GTiff"));
        if( poDrv == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot find GTiff driver" );
            VSIFCloseL(fp);
            return nullptr;
        }
        char** papszGTiffOptions = nullptr;
        papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                            "ENDIANNESS", "LITTLE");
        if( bIsTiled )
        {
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "TILED", "YES");
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "BLOCKXSIZE",
                                                CPLSPrintf("%d", nBlockXSize));
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "BLOCKYSIZE",
                                                CPLSPrintf("%d", nBlockYSize));
        }
        const char* pszGTiffOptions = CSLFetchNameValueDef(papszOptions,
                                                    "GEOTIFF_OPTIONS", "");
        char** papszTokens = CSLTokenizeString2( pszGTiffOptions, ",", 0 );
        for( int i = 0; papszTokens[i] != nullptr; i++ )
        {
            papszGTiffOptions = CSLAddString(papszGTiffOptions,
                                             papszTokens[i]);
        }
        CSLDestroy(papszTokens);

        // If the user didn't specify any compression and
        // GEOTIFF_AS_REGULAR_EXTERNAL is set (or unspecified), then the
        // GeoTIFF file can be seen as a regular external raw file, provided
        // we make some provision on its organization.
        if( CSLFetchNameValue(papszGTiffOptions, "COMPRESS") == nullptr &&
            CPLFetchBool(papszOptions, "GEOTIFF_AS_REGULAR_EXTERNAL", true) )
        {
            bGeoTIFFAsRegularExternal = true;
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "INTERLEAVE", "BAND");
            // Will make sure that our blocks at nodata are not optimized
            // away but indeed well written
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                    "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "YES");
            if( !bIsTiled && nBandsIn > 1 )
            {
                papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                    "BLOCKYSIZE", "1");
            }
        }

        poExternalDS = poDrv->Create( osExternalFilename, nXSize, nYSize,
                                      nBandsIn,
                                      eType, papszGTiffOptions );
        CSLDestroy(papszGTiffOptions);
        if( poExternalDS == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot create %s",
                      osExternalFilename.c_str() );
            VSIFCloseL(fp);
            return nullptr;
        }
    }

    ISIS3Dataset* poDS = new ISIS3Dataset();
    poDS->SetDescription( pszFilename );
    poDS->eAccess = GA_Update;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->m_osExternalFilename = osExternalFilename;
    poDS->m_poExternalDS = poExternalDS;
    poDS->m_bGeoTIFFAsRegularExternal = bGeoTIFFAsRegularExternal;
    if( bGeoTIFFAsRegularExternal )
        poDS->m_bGeoTIFFInitDone = false;
    poDS->m_fpLabel = fp;
    poDS->m_fpImage = fpImage ? fpImage: fp;
    poDS->m_bIsLabelWritten = false;
    poDS->m_bIsTiled = bIsTiled;
    poDS->m_bInitToNodata = (poDS->m_poExternalDS == nullptr);
    poDS->m_osComment = CSLFetchNameValueDef(papszOptions, "COMMENT", "");
    poDS->m_osLatitudeType = CSLFetchNameValueDef(papszOptions,
                                                  "LATITUDE_TYPE", "");
    poDS->m_osLongitudeDirection = CSLFetchNameValueDef(papszOptions,
                                                  "LONGITUDE_DIRECTION", "");
    poDS->m_osTargetName = CSLFetchNameValueDef(papszOptions,
                                                  "TARGET_NAME", "");
    poDS->m_bForce360 = CPLFetchBool(papszOptions, "FORCE_360", false);
    poDS->m_bWriteBoundingDegrees = CPLFetchBool(papszOptions,
                                                 "WRITE_BOUNDING_DEGREES",
                                                 true);
    poDS->m_osBoundingDegrees = CSLFetchNameValueDef(papszOptions,
                                                     "BOUNDING_DEGREES", "");
    poDS->m_bUseSrcLabel = CPLFetchBool(papszOptions, "USE_SRC_LABEL", true);
    poDS->m_bUseSrcMapping =
                        CPLFetchBool(papszOptions, "USE_SRC_MAPPING", false);
    poDS->m_bUseSrcHistory =
                        CPLFetchBool(papszOptions, "USE_SRC_HISTORY", true);
    poDS->m_bAddGDALHistory =
                        CPLFetchBool(papszOptions, "ADD_GDAL_HISTORY", true);
    if( poDS->m_bAddGDALHistory )
    {
        poDS->m_osGDALHistory = CSLFetchNameValueDef(papszOptions,
                                                     "GDAL_HISTORY", "");
    }
    const double dfNoData = (eType == GDT_Byte)    ? NULL1:
                            (eType == GDT_UInt16)  ? NULLU2:
                            (eType == GDT_Int16)   ? NULL2:
                            /*(eType == GDT_Float32) ?*/ NULL4;

    for( int i = 0; i < nBandsIn; i++ )
    {
        GDALRasterBand *poBand = nullptr;

        if( poDS->m_poExternalDS != nullptr )
        {
            ISIS3WrapperRasterBand* poISISBand =
                new ISIS3WrapperRasterBand(
                            poDS->m_poExternalDS->GetRasterBand( i+1 ) );
            poBand = poISISBand;
        }
        else if( bIsTiled  )
        {
            ISISTiledBand* poISISBand =
                new ISISTiledBand( poDS, poDS->m_fpImage, i+1, eType,
                                   nBlockXSize, nBlockYSize,
                                   0, //nSkipBytes, to be hacked
                                   // afterwards for in-label imagery
                                   0, 0,
                                   CPL_IS_LSB );

            poBand = poISISBand;
        }
        else
        {
            const int nPixelOffset = GDALGetDataTypeSizeBytes(eType);
            const int nLineOffset = nPixelOffset * nXSize;
            const vsi_l_offset nBandOffset =
                static_cast<vsi_l_offset>(nLineOffset) * nYSize;
            ISIS3RawRasterBand* poISISBand =
                new ISIS3RawRasterBand( poDS, i+1, poDS->m_fpImage,
                                   nBandOffset * i, // nImgOffset, to be
                                   //hacked afterwards for in-label imagery
                                   nPixelOffset, nLineOffset, eType,
                                   CPL_IS_LSB );

            poBand = poISISBand;
        }
        poDS->SetBand( i+1, poBand );
        poBand->SetNoDataValue(dfNoData);
    }

    return poDS;
}

/************************************************************************/
/*                      GetUnderlyingDataset()                          */
/************************************************************************/

static GDALDataset* GetUnderlyingDataset( GDALDataset* poSrcDS )
{
    if( poSrcDS->GetDriver() != nullptr &&
        poSrcDS->GetDriver() == GDALGetDriverByName("VRT") )
    {
        VRTDataset* poVRTDS = reinterpret_cast<VRTDataset* >(poSrcDS);
        poSrcDS = poVRTDS->GetSingleSimpleSource();
    }

    return poSrcDS;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset* ISIS3Dataset::CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int /*bStrict*/,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
{
    const char* pszDataLocation = CSLFetchNameValueDef(papszOptions,
                                                       "DATA_LOCATION",
                                                       "LABEL");
    GDALDataset* poSrcUnderlyingDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcUnderlyingDS == nullptr )
        poSrcUnderlyingDS = poSrcDS;
    if( EQUAL(pszDataLocation, "GEOTIFF") &&
        strcmp(poSrcUnderlyingDS->GetDescription(),
               CSLFetchNameValueDef(papszOptions, "EXTERNAL_FILENAME",
                                    CPLResetExtension(pszFilename, "tif"))
              ) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Output file has same name as input file");
        return nullptr;
    }
    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    ISIS3Dataset *poDS = reinterpret_cast<ISIS3Dataset*>(
        Create( pszFilename, nXSize, nYSize, nBands, eType, papszOptions ));
    if( poDS == nullptr )
        return nullptr;
    poDS->m_osFromFilename = poSrcUnderlyingDS->GetDescription();

    double adfGeoTransform[6] = { 0.0 };
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        poDS->SetGeoTransform( adfGeoTransform );
    }

    auto poSrcSRS = poSrcDS->GetSpatialRef();
    if( poSrcSRS )
    {
        poDS->SetSpatialRef( poSrcSRS );
    }

    for(int i=1;i<=nBands;i++)
    {
        const double dfOffset = poSrcDS->GetRasterBand(i)->GetOffset();
        if( dfOffset != 0.0 )
            poDS->GetRasterBand(i)->SetOffset(dfOffset);

        const double dfScale = poSrcDS->GetRasterBand(i)->GetScale();
        if( dfScale != 1.0 )
            poDS->GetRasterBand(i)->SetScale(dfScale);
    }

    // Do we need to remap nodata ?
    int bHasNoData = FALSE;
    poDS->m_dfSrcNoData =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);
    poDS->m_bHasSrcNoData = CPL_TO_BOOL(bHasNoData);

    if( poDS->m_bUseSrcLabel )
    {
        char** papszMD_ISIS3 = poSrcDS->GetMetadata("json:ISIS3");
        if( papszMD_ISIS3 != nullptr )
        {
            poDS->SetMetadata( papszMD_ISIS3, "json:ISIS3" );
        }
    }

    // We don't need to initialize the imagery as we are going to copy it
    // completely
    poDS->m_bInitToNodata = false;
    CPLErr eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDS,
                                           nullptr, pfnProgress, pProgressData );
    poDS->FlushCache(false);
    poDS->m_bHasSrcNoData = false;
    if( eErr != CE_None )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_ISIS3()                         */
/************************************************************************/

void GDALRegister_ISIS3()

{
    if( GDALGetDriverByName( "ISIS3" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ISIS3" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "USGS Astrogeology ISIS cube (Version 3)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/isis3.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "lbl cub" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 Float32" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='DATA_LOCATION' type='string-select' "
                "description='Location of pixel data' default='LABEL'>"
"     <Value>LABEL</Value>"
"     <Value>EXTERNAL</Value>"
"     <Value>GEOTIFF</Value>"
"  </Option>"
"  <Option name='GEOTIFF_AS_REGULAR_EXTERNAL' type='boolean' "
    "description='Whether the GeoTIFF file, if uncompressed, should be "
    "registered as a regular raw file' default='YES'/>"
"  <Option name='GEOTIFF_OPTIONS' type='string' "
    "description='Comma separated list of KEY=VALUE tuples to forward "
    "to the GeoTIFF driver'/>"
"  <Option name='EXTERNAL_FILENAME' type='string' "
                "description='Override default external filename. "
                "Only for DATA_LOCATION=EXTERNAL or GEOTIFF'/>"
"  <Option name='TILED' type='boolean' "
        "description='Whether the pixel data should be tiled' default='NO'/>"
"  <Option name='BLOCKXSIZE' type='int' "
                            "description='Tile width' default='256'/>"
"  <Option name='BLOCKYSIZE' type='int' "
                            "description='Tile height' default='256'/>"
"  <Option name='COMMENT' type='string' "
    "description='Comment to add into the label'/>"
"  <Option name='LATITUDE_TYPE' type='string-select' "
    "description='Value of Mapping.LatitudeType' default='Planetocentric'>"
"     <Value>Planetocentric</Value>"
"     <Value>Planetographic</Value>"
"  </Option>"
"  <Option name='LONGITUDE_DIRECTION' type='string-select' "
    "description='Value of Mapping.LongitudeDirection' "
    "default='PositiveEast'>"
"     <Value>PositiveEast</Value>"
"     <Value>PositiveWest</Value>"
"  </Option>"
"  <Option name='TARGET_NAME' type='string' description='Value of "
    "Mapping.TargetName'/>"
"  <Option name='FORCE_360' type='boolean' "
    "description='Whether to force longitudes in [0,360] range' default='NO'/>"
"  <Option name='WRITE_BOUNDING_DEGREES' type='boolean' "
    "description='Whether to write Min/MaximumLong/Latitude values' "
    "default='YES'/>"
"  <Option name='BOUNDING_DEGREES' type='string' "
    "description='Manually set bounding box with the syntax "
    "min_long,min_lat,max_long,max_lat'/>"
"  <Option name='USE_SRC_LABEL' type='boolean' "
    "description='Whether to use source label in ISIS3 to ISIS3 conversions' "
    "default='YES'/>"
"  <Option name='USE_SRC_MAPPING' type='boolean' "
    "description='Whether to use Mapping group from source label in "
                 "ISIS3 to ISIS3 conversions' "
    "default='NO'/>"
"  <Option name='USE_SRC_HISTORY' type='boolean' "
    "description='Whether to use content pointed by the History object in "
                 "ISIS3 to ISIS3 conversions' "
    "default='YES'/>"
"  <Option name='ADD_GDAL_HISTORY' type='boolean' "
    "description='Whether to add GDAL specific history in the content pointed "
                 "by the History object in "
                 "ISIS3 to ISIS3 conversions' "
    "default='YES'/>"
"  <Option name='GDAL_HISTORY' type='string' "
    "description='Manually defined GDAL history. Must be formatted as ISIS3 "
    "PDL. If not specified, it is automatically composed.'/>"
"</CreationOptionList>"
    );

    poDriver->pfnOpen = ISIS3Dataset::Open;
    poDriver->pfnIdentify = ISIS3Dataset::Identify;
    poDriver->pfnCreate = ISIS3Dataset::Create;
    poDriver->pfnCreateCopy = ISIS3Dataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
