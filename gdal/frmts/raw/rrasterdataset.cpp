/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements R Raster Format.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"

#include "gdal_frmts.h"
#include "gdal_rat.h"
#include "gdal_priv.h"

#include "rawdataset.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <limits>
#include <memory>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                           RRASTERDataset                             */
/* ==================================================================== */
/************************************************************************/

class RRASTERDataset final: public RawDataset
{
    bool        m_bHeaderDirty = false;
    CPLString   m_osGriFilename{};
    bool        m_bGeoTransformValid = false;
    double      m_adfGeoTransform[6]{0,1,0,0,0,-1};
    VSILFILE   *m_fpImage = nullptr;
    CPLString   m_osProjection{};
    std::shared_ptr<GDALRasterAttributeTable> m_poRAT{};
    std::shared_ptr<GDALColorTable> m_poCT{};
    bool        m_bNativeOrder = true;
    CPLString   m_osCreator{};
    CPLString   m_osCreated{};
    CPLString   m_osBandOrder{};
    CPLString   m_osLegend{};
    bool        m_bInitRaster = false;

    static bool ComputeSpacings(const CPLString& osBandOrder,
                                int nCols,
                                int nRows,
                                int l_nBands,
                                GDALDataType eDT,
                                int& nPixelOffset,
                                int& nLineOffset,
                                vsi_l_offset& nBandOffset);
    void        RewriteHeader();

    CPL_DISALLOW_COPY_ASSIGN(RRASTERDataset)

  public:
    RRASTERDataset();
    ~RRASTERDataset() override;

    char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                      GDALDataset * poSrcDS,
                                      int bStrict, char ** papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void * pProgressData );

    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double *padfGeoTransform ) override;
    const char *_GetProjectionRef() override;
    CPLErr _SetProjection( const char *pszSRS ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;

    void SetHeaderDirty() { m_bHeaderDirty = true; }
    void InitImageIfNeeded();
};

/************************************************************************/
/* ==================================================================== */
/*                         RRASTERRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class RRASTERRasterBand final: public RawRasterBand
{
      friend class RRASTERDataset;

      bool      m_bHasNoDataValue = false;
      double    m_dfNoDataValue = 0.0;
      double    m_dfMin = std::numeric_limits<double>::infinity();
      double    m_dfMax = -std::numeric_limits<double>::infinity();
      std::shared_ptr<GDALRasterAttributeTable> m_poRAT{};
      std::shared_ptr<GDALColorTable> m_poCT{};

      CPL_DISALLOW_COPY_ASSIGN(RRASTERRasterBand)

  public:
      RRASTERRasterBand( GDALDataset *poDS, int nBand, VSILFILE * fpRaw,
                    vsi_l_offset nImgOffset, int nPixelOffset,
                    int nLineOffset,
                    GDALDataType eDataType, int bNativeOrder );

      void SetMinMax( double dfMin, double dfMax );
      double GetMinimum( int *pbSuccess = nullptr ) override;
      double GetMaximum( int *pbSuccess = nullptr ) override;

      double GetNoDataValue( int* pbSuccess = nullptr ) override;
      CPLErr SetNoDataValue( double dfNoData ) override;

      GDALColorTable *GetColorTable() override;
      CPLErr SetColorTable( GDALColorTable *poNewCT ) override;

      GDALRasterAttributeTable *GetDefaultRAT() override;
      CPLErr SetDefaultRAT( const GDALRasterAttributeTable * poRAT ) override;

      void SetDescription( const char *pszDesc ) override;

  protected:
      CPLErr IWriteBlock( int, int, void * ) override;
      CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                        void *, int, int, GDALDataType,
                        GSpacing nPixelSpace, GSpacing nLineSpace,
                        GDALRasterIOExtraArg* psExtraArg ) override;
};

/************************************************************************/
/*                           RRASTERDataset()                           */
/************************************************************************/

RRASTERRasterBand::RRASTERRasterBand( GDALDataset *poDSIn, int nBandIn,
                                      VSILFILE * fpRawIn,
                                      vsi_l_offset nImgOffsetIn,
                                      int nPixelOffsetIn,
                                      int nLineOffsetIn,
                                      GDALDataType eDataTypeIn,
                                      int bNativeOrderIn ) :
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                  nLineOffsetIn, eDataTypeIn, bNativeOrderIn,
                  RawRasterBand::OwnFP::NO)
{
}

/************************************************************************/
/*                             SetMinMax()                              */
/************************************************************************/

void RRASTERRasterBand::SetMinMax( double dfMin, double dfMax )
{
    m_dfMin = dfMin;
    m_dfMax = dfMax;
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double RRASTERRasterBand::GetMinimum( int *pbSuccess )
{
    if( m_dfMin <= m_dfMax )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMin;
    }
    return RawRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                            GetMaximum()                              */
/************************************************************************/

double RRASTERRasterBand::GetMaximum(int *pbSuccess )
{
    if( m_dfMin <= m_dfMax )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMax;
    }
    return RawRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable* RRASTERRasterBand::GetColorTable()
{
    return m_poCT.get();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr RRASTERRasterBand::SetColorTable( GDALColorTable *poNewCT )
{
    RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
    if( poGDS->GetAccess() != GA_Update )
        return CE_Failure;

    if( poNewCT == nullptr )
        m_poCT.reset();
    else
        m_poCT.reset(poNewCT->Clone());

    poGDS->SetHeaderDirty();

    return CE_None;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable* RRASTERRasterBand::GetDefaultRAT()
{
    return m_poRAT.get();
}

/************************************************************************/
/*                            SetDefaultRAT()                           */
/************************************************************************/

CPLErr RRASTERRasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )
{
    RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
    if( poGDS->GetAccess() != GA_Update )
        return CE_Failure;

    if( poRAT == nullptr )
        m_poRAT.reset();
    else
        m_poRAT.reset(poRAT->Clone());

    poGDS->SetHeaderDirty();

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void RRASTERRasterBand::SetDescription( const char *pszDesc )
{
    RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
    if( poGDS->GetAccess() != GA_Update )
        return;

    GDALRasterBand::SetDescription(pszDesc);

    poGDS->SetHeaderDirty();
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double RRASTERRasterBand::GetNoDataValue(int* pbSuccess)
{
    if( pbSuccess )
        *pbSuccess = m_bHasNoDataValue;
    return m_dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr RRASTERRasterBand::SetNoDataValue(double dfNoData)
{
    RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
    if( poGDS->GetAccess() != GA_Update )
        return CE_Failure;

    m_bHasNoDataValue = true;
    m_dfNoDataValue = dfNoData;
    poGDS->SetHeaderDirty();
    return CE_None;
}

/************************************************************************/
/*                             GetMinMax()                              */
/************************************************************************/

template<class T>
static void GetMinMax(const T* buffer, int nBufXSize, int nBufYSize,
                      GSpacing nPixelSpace, GSpacing nLineSpace,
                      double dfNoDataValue,
                      double& dfMin, double& dfMax)
{
    for( int iY = 0; iY < nBufYSize; iY++ )
    {
        for( int iX = 0; iX < nBufXSize; iX++ )
        {
            const double dfVal = buffer[iY * nLineSpace + iX * nPixelSpace];
            if( dfVal != dfNoDataValue && !CPLIsNan(dfVal) )
            {
                dfMin = std::min(dfMin, dfVal);
                dfMax = std::max(dfMax, dfVal);
            }
        }
    }
}

static void GetMinMax(const void* pBuffer, GDALDataType eDT,
                      bool bByteSigned,
                      int nBufXSize, int nBufYSize,
                      GSpacing nPixelSpace, GSpacing nLineSpace,
                      double dfNoDataValue,
                      double& dfMin, double& dfMax)
{
    switch( eDT )
    {
        case GDT_Byte:
            if( bByteSigned )
                GetMinMax( static_cast<const signed char*>(pBuffer),
                           nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                           dfNoDataValue,
                           dfMin, dfMax );
            else
                GetMinMax( static_cast<const GByte*>(pBuffer),
                           nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                           dfNoDataValue,
                           dfMin, dfMax );
            break;
        case GDT_UInt16:
            GetMinMax( static_cast<const GUInt16*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        case GDT_Int16:
            GetMinMax( static_cast<const GInt16*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        case GDT_UInt32:
            GetMinMax( static_cast<const GUInt32*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        case GDT_Int32:
            GetMinMax( static_cast<const GInt32*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        case GDT_Float32:
            GetMinMax( static_cast<const float*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        case GDT_Float64:
            GetMinMax( static_cast<const double*>(pBuffer),
                       nBufXSize, nBufYSize, nPixelSpace, nLineSpace,
                       dfNoDataValue,
                       dfMin, dfMax );
            break;
        default:
            CPLAssert(false);
            break;
    }
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RRASTERRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                       void * pImage )
{
    RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
    poGDS->InitImageIfNeeded();

    const char* pszPixelType =
        GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    bool bByteSigned = (eDataType == GDT_Byte && pszPixelType &&
                        EQUAL(pszPixelType, "SIGNEDBYTE"));
    int bGotNoDataValue = false;
    double dfNoDataValue = GetNoDataValue(&bGotNoDataValue);
    if( !bGotNoDataValue )
        dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
    GetMinMax(pImage, eDataType, bByteSigned,
              nBlockXSize, nBlockYSize, 1, nBlockXSize,
              dfNoDataValue,
              m_dfMin, m_dfMax);
    return RawRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr RRASTERRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                     int nXOff, int nYOff,
                                     int nXSize, int nYSize,
                                     void * pData, int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType,
                                     GSpacing nPixelSpace, GSpacing nLineSpace,
                                     GDALRasterIOExtraArg* psExtraArg )

{
    if( eRWFlag == GF_Write )
    {
        RRASTERDataset* poGDS = static_cast<RRASTERDataset*>(poDS);
        poGDS->InitImageIfNeeded();

        const char* pszPixelType =
            GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        bool bByteSigned = (eDataType == GDT_Byte && pszPixelType &&
                            EQUAL(pszPixelType, "SIGNEDBYTE"));
        const int nDTSize = std::max(1, GDALGetDataTypeSizeBytes(eDataType));
        int bGotNoDataValue = false;
        double dfNoDataValue = GetNoDataValue(&bGotNoDataValue);
        if( !bGotNoDataValue )
            dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
        GetMinMax(pData, eDataType, bByteSigned,
                  nBufXSize, nBufYSize,
                  nPixelSpace / nDTSize, nLineSpace / nDTSize,
                  dfNoDataValue,
                  m_dfMin, m_dfMax);
    }
    return RawRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                     nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg );
}

/************************************************************************/
/*                           RRASTERDataset()                           */
/************************************************************************/

RRASTERDataset::RRASTERDataset()
{
}

/************************************************************************/
/*                          ~RRASTERDataset()                           */
/************************************************************************/

RRASTERDataset::~RRASTERDataset()

{
    if( m_fpImage != nullptr )
    {
        InitImageIfNeeded();
        FlushCache();
        VSIFCloseL(m_fpImage);
    }
    if( m_bHeaderDirty )
        RewriteHeader();
}

/************************************************************************/
/*                        InitImageIfNeeded()                           */
/************************************************************************/

void RRASTERDataset::InitImageIfNeeded()
{
    CPLAssert(m_fpImage);
    if( !m_bInitRaster )
        return;
    m_bInitRaster = false;
    int bGotNoDataValue = false;
    double dfNoDataValue = GetRasterBand(1)->GetNoDataValue(&bGotNoDataValue);
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    if( dfNoDataValue == 0.0 )
    {
        VSIFTruncateL(m_fpImage,
            static_cast<vsi_l_offset>(nRasterXSize) * nRasterYSize * nBands *
            nDTSize);
    }
    else
    {
        GByte abyNoDataValue[16];
        GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                      abyNoDataValue, eDT, 0,
                      1);
        for( GUIntBig i = 0; i <
                static_cast<GUIntBig>(nRasterXSize) * nRasterYSize * nBands;
             i++ )
        {
            VSIFWriteL(abyNoDataValue, 1, nDTSize, m_fpImage);
        }
    }
}

/************************************************************************/
/*                           RewriteHeader()                            */
/************************************************************************/

void RRASTERDataset::RewriteHeader()
{
    VSILFILE* fp = VSIFOpenL(GetDescription(), "wb");
    if( !fp )
        return;

    VSIFPrintfL(fp, "[general]\n");
    if( !m_osCreator.empty() )
        VSIFPrintfL(fp, "creator=%s\n", m_osCreator.c_str());
    if( !m_osCreated.empty() )
        VSIFPrintfL(fp, "created=%s\n", m_osCreated.c_str());

    VSIFPrintfL(fp, "[georeference]\n");
    VSIFPrintfL(fp, "nrows=%d\n", nRasterYSize);
    VSIFPrintfL(fp, "ncols=%d\n", nRasterXSize);

    VSIFPrintfL(fp, "xmin=%.18g\n", m_adfGeoTransform[0]);
    VSIFPrintfL(fp, "ymin=%.18g\n",
            m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5]);
    VSIFPrintfL(fp, "xmax=%.18g\n",
            m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1]);
    VSIFPrintfL(fp, "ymax=%.18g\n", m_adfGeoTransform[3]);

    if( !m_osProjection.empty() )
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput(m_osProjection, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS);
        char* pszProj4 = nullptr;
        oSRS.exportToProj4(&pszProj4);
        if( pszProj4 )
        {
            VSIFPrintfL(fp, "projection=%s\n", pszProj4);
            VSIFree(pszProj4);
        }
    }

    VSIFPrintfL(fp, "[data]\n");
    GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    const char* pszPixelType =
        GetRasterBand(1)->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    VSIFPrintfL(fp, "datatype=%s\n",
        (eDT == GDT_Byte && pszPixelType && EQUAL(pszPixelType, "SIGNEDBYTE"))
                          ?        "INT1S" :
        (eDT == GDT_Byte) ?        "INT1U" :
        (eDT == GDT_UInt16) ?      "INT2U" :
        (eDT == GDT_UInt32) ?      "INT4U" :
        (eDT == GDT_Int16) ?       "INT2S" :
        (eDT == GDT_Int32) ?       "INT4S" :
        (eDT == GDT_Float32) ?     "FLT4S" :
        /*(eDT == GDT_Float64) ?*/ "FLT8S");

    int bGotNoDataValue = false;
    double dfNoDataValue = GetRasterBand(1)->GetNoDataValue(&bGotNoDataValue);
    if( bGotNoDataValue )
        VSIFPrintfL(fp, "nodatavalue=%.18g\n", dfNoDataValue);

#if CPL_IS_LSB
    VSIFPrintfL(fp, "byteorder=%s\n", m_bNativeOrder ? "little": "big");
#else
    VSIFPrintfL(fp, "byteorder=%s\n", !m_bNativeOrder ? "little": "big");
#endif
    VSIFPrintfL(fp, "nbands=%d\n", nBands);
    if( nBands > 1 )
        VSIFPrintfL(fp, "bandorder=%s\n", m_osBandOrder.c_str());
    CPLString osMinValue, osMaxValue;
    for( int i = 1; i <= nBands; i++ )
    {
        RRASTERRasterBand* poBand = static_cast<RRASTERRasterBand*>(
            GetRasterBand(i));
        if( i > 1 )
        {
            osMinValue += ":";
            osMaxValue += ":";
        }
        if( poBand->m_dfMin > poBand->m_dfMax )
        {
            osMinValue.clear();
            break;
        }
        osMinValue += CPLSPrintf("%.18g", poBand->m_dfMin);
        osMaxValue += CPLSPrintf("%.18g", poBand->m_dfMax);
    }
    if( !osMinValue.empty() )
    {
        VSIFPrintfL(fp, "minvalue=%s\n", osMinValue.c_str());
        VSIFPrintfL(fp, "maxvalue=%s\n", osMaxValue.c_str());
    }

    GDALColorTable* poCT = GetRasterBand(1)->GetColorTable();
    GDALRasterAttributeTable* poRAT = GetRasterBand(1)->GetDefaultRAT();
    if( poCT == nullptr && poRAT == nullptr )
    {
        VSIFPrintfL(fp, "categorical=FALSE\n");
    }
    else
    {
        VSIFPrintfL(fp, "categorical=TRUE\n");
        if( poCT && poRAT )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Both color table and raster attribute table defined. "
                     "Writing only the later");
        }
        if( poRAT )
        {
            CPLString osRatNames;
            CPLString osRatTypes;
            for( int i = 0; i < poRAT->GetColumnCount(); i++ )
            {
                if( !osRatNames.empty() )
                {
                    osRatNames += ":";
                    osRatTypes += ":";
                }
                osRatNames +=
                    CPLString(poRAT->GetNameOfCol(i)).replaceAll(':', '.');
                GDALRATFieldType eColType = poRAT->GetTypeOfCol(i);
                if( eColType == GFT_Integer )
                    osRatTypes += "integer";
                else if( eColType == GFT_Real )
                    osRatTypes += "numeric";
                else
                    osRatTypes += "character";
            }
            VSIFPrintfL(fp, "ratnames=%s\n", osRatNames.c_str());
            VSIFPrintfL(fp, "rattypes=%s\n", osRatTypes.c_str());
            CPLString osRatValues;
            for( int i = 0; i < poRAT->GetColumnCount(); i++ )
            {
                GDALRATFieldType eColType = poRAT->GetTypeOfCol(i);
                for( int j = 0; j < poRAT->GetRowCount(); j++ )
                {
                    if( i != 0 || j != 0 )
                        osRatValues += ":";
                    if( eColType == GFT_Integer )
                    {
                        osRatValues += CPLSPrintf("%d",
                                                 poRAT->GetValueAsInt(j, i));
                    }
                    else if( eColType == GFT_Real )
                    {
                        osRatValues += CPLSPrintf("%.18g",
                                                 poRAT->GetValueAsDouble(j, i));
                    }
                    else
                    {
                        const char* pszVal = poRAT->GetValueAsString(j, i);
                        if( pszVal )
                        {
                            osRatValues +=
                                CPLString(pszVal).replaceAll(':', '.');
                        }
                    }
                }
            }
            VSIFPrintfL(fp, "ratvalues=%s\n", osRatValues.c_str());
        }
        else
        {
            bool bNeedsAlpha = false;
            for( int i = 0; i < poCT->GetColorEntryCount(); i++ )
            {
                if( poCT->GetColorEntry(i)->c4 != 255 )
                {
                    bNeedsAlpha = true;
                    break;
                }
            }
            if( !bNeedsAlpha )
            {
                VSIFPrintfL(fp, "ratnames=%s\n",
                            "ID:red:green:blue");
                VSIFPrintfL(fp, "rattypes=%s\n",
                            "integer:integer:integer:integer");
            }
            else
            {
                VSIFPrintfL(fp, "ratnames=%s\n",
                            "ID:red:green:blue:alpha");
                VSIFPrintfL(fp, "rattypes=%s\n",
                            "integer:integer:integer:integer:integer");
            }

            CPLString osRatID;
            CPLString osRatR;
            CPLString osRatG;
            CPLString osRatB;
            CPLString osRatA;
            for( int i = 0; i < poCT->GetColorEntryCount(); i++ )
            {
                const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                if( i > 0 )
                {
                    osRatID += ":";
                    osRatR += ":";
                    osRatG += ":";
                    osRatB += ":";
                    osRatA += ":";
                }
                osRatID += CPLSPrintf("%d", i);
                osRatR += CPLSPrintf("%d", psEntry->c1);
                osRatG += CPLSPrintf("%d", psEntry->c2);
                osRatB += CPLSPrintf("%d", psEntry->c3);
                osRatA += CPLSPrintf("%d", psEntry->c4);
            }
            if( !bNeedsAlpha )
            {
                VSIFPrintfL(fp, "ratvalues=%s:%s:%s:%s\n",
                            osRatID.c_str(), osRatR.c_str(),
                            osRatG.c_str(), osRatB.c_str());
            }
            else
            {
                VSIFPrintfL(fp, "ratvalues=%s:%s:%s:%s:%s\n",
                            osRatID.c_str(), osRatR.c_str(),
                            osRatG.c_str(), osRatB.c_str(), osRatA.c_str());
            }
        }
    }

    if( !m_osLegend.empty() )
        VSIFPrintfL(fp, "[legend]\n%s", m_osLegend.c_str());

    CPLString osLayerName;
    bool bGotSignificantBandDesc = false;
    for( int i = 1; i <= nBands; i++ )
    {
        GDALRasterBand* poBand = GetRasterBand(i);
        const char* pszDesc = poBand->GetDescription();
        if( EQUAL(pszDesc, "") )
        {
            GDALColorInterp eInterp = poBand->GetColorInterpretation();
            if( eInterp == GCI_RedBand )
            {
                bGotSignificantBandDesc = true;
                pszDesc = "red";
            }
            else if( eInterp == GCI_GreenBand )
            {
                bGotSignificantBandDesc = true;
                pszDesc = "green";
            }
            else if( eInterp == GCI_BlueBand )
            {
                bGotSignificantBandDesc = true;
                pszDesc = "blue";
            }
            else if( eInterp == GCI_AlphaBand )
            {
                bGotSignificantBandDesc = true;
                pszDesc = "alpha";
            }
            else
            {
                pszDesc = CPLSPrintf("Band%d", i);
            }
        }
        else
        {
            bGotSignificantBandDesc = true;
        }
        if( i > 1 )
            osLayerName += ":";
        osLayerName += CPLString(pszDesc).replaceAll(':','.');
    }
    if( bGotSignificantBandDesc )
    {
        VSIFPrintfL(fp, "[description]\n");
        VSIFPrintfL(fp, "layername=%s\n", osLayerName.c_str());
    }

    VSIFCloseL(fp);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **RRASTERDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();

    papszFileList = CSLAddString(papszFileList, m_osGriFilename);

    return papszFileList;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RRASTERDataset::GetGeoTransform( double * padfGeoTransform )
{
    if( m_bGeoTransformValid )
    {
        memcpy( padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double) );
        return CE_None;
    }
    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr RRASTERDataset::SetGeoTransform( double *padfGeoTransform )

{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot set geotransform on a read-only dataset");
        return CE_Failure;
    }

    // We only support non-rotated images with info in the .HDR file.
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0 )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Rotated / skewed images not supported");
        return GDALPamDataset::SetGeoTransform(padfGeoTransform);
    }

    // Record new geotransform.
    m_bGeoTransformValid = true;
    memcpy(m_adfGeoTransform, padfGeoTransform, sizeof(double) * 6);
    SetHeaderDirty();

    return CE_None;
}

/************************************************************************/
/*                           GetProjectionRef()                         */
/************************************************************************/

const char * RRASTERDataset::_GetProjectionRef()
{
    return m_osProjection.c_str();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr RRASTERDataset::_SetProjection( const char *pszSRS )

{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot set projection on a read-only dataset");
        return CE_Failure;
    }

    m_osProjection = pszSRS ? pszSRS : "";
    SetHeaderDirty();

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr RRASTERDataset::SetMetadata( char ** papszMetadata,
                                    const char * pszDomain )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        m_osCreator = CSLFetchNameValueDef(papszMetadata, "CREATOR", "");
        m_osCreated = CSLFetchNameValueDef(papszMetadata, "CREATED", "");
        SetHeaderDirty();
    }
    return GDALDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr RRASTERDataset::SetMetadataItem( const char * pszName,
                                        const char * pszValue,
                                        const char * pszDomain )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        if( EQUAL(pszName, "CREATOR") )
        {
            m_osCreator = pszValue ? pszValue : "";
            SetHeaderDirty();
        }
        if( EQUAL(pszName, "CREATED") )
        {
            m_osCreated = pszValue ? pszValue : "";
            SetHeaderDirty();
        }
    }
    return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int RRASTERDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 40
        || poOpenInfo->fpL == nullptr
        || !EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "grd" )
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "ncols") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "nrows") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "xmin") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "ymin") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "xmax") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "ymax") == nullptr
        || strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader), "datatype") == nullptr )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          ComputeSpacing()                            */
/************************************************************************/

bool RRASTERDataset::ComputeSpacings(const CPLString& osBandOrder,
                                     int nCols,
                                     int nRows,
                                     int l_nBands,
                                     GDALDataType eDT,
                                     int& nPixelOffset,
                                     int& nLineOffset,
                                     vsi_l_offset& nBandOffset)
{
    nPixelOffset = 0;
    nLineOffset = 0;
    nBandOffset = 0;
    const int nPixelSize = GDALGetDataTypeSizeBytes( eDT );
    if( l_nBands == 1 || EQUAL( osBandOrder, "BIL" ) )
    {
        nPixelOffset = nPixelSize;
        if( l_nBands != 0 && nPixelSize != 0 &&
            nCols > INT_MAX / ( l_nBands * nPixelSize ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return false;
        }
        nLineOffset = nPixelSize * nCols * l_nBands;
        nBandOffset = static_cast<vsi_l_offset>(nPixelSize) * nCols;
    }
    else if( EQUAL( osBandOrder, "BIP" ) )
    {
        if( l_nBands != 0 && nPixelSize != 0 &&
            nCols > INT_MAX / ( l_nBands * nPixelSize ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return false;
        }
        nPixelOffset = nPixelSize * l_nBands;
        nLineOffset = nPixelSize * nCols * l_nBands;
        nBandOffset = nPixelSize;
    }
    else if( EQUAL( osBandOrder, "BSQ" ) )
    {
        if( nPixelSize != 0 && nCols > INT_MAX / nPixelSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return false;
        }
        nPixelOffset = nPixelSize;
        nLineOffset = nPixelSize * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else if( l_nBands > 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Unknown bandorder" );
        return false;
    }
    return true;
}

/************************************************************************/
/*                            CastToFloat()                             */
/************************************************************************/

static float CastToFloat(double dfVal)
{
    if( CPLIsInf(dfVal) || CPLIsNan(dfVal) ||
        (dfVal >= -std::numeric_limits<float>::max() &&
         dfVal <= std::numeric_limits<float>::max()) )
    {
        return static_cast<float>(dfVal);
    }
    return std::numeric_limits<float>::quiet_NaN();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RRASTERDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    const char* pszLine = nullptr;
    int nRows = 0;
    int nCols = 0;
    double dfXMin = 0.0;
    double dfYMin = 0.0;
    double dfXMax = 0.0;
    double dfYMax = 0.0;
    int l_nBands = 1;
    CPLString osDataType;
    CPLString osBandOrder;
    CPLString osProjection;
    CPLString osByteOrder;
    CPLString osNoDataValue("NA");
    CPLString osMinValue;
    CPLString osMaxValue;
    CPLString osCreator;
    CPLString osCreated;
    CPLString osLayerName;
    CPLString osRatNames;
    CPLString osRatTypes;
    CPLString osRatValues;
    bool bInLegend = false;
    CPLString osLegend;
    VSIRewindL(poOpenInfo->fpL);
    while( (pszLine = CPLReadLine2L(poOpenInfo->fpL, 1024 * 1024, nullptr)) != nullptr )
    {
        if( pszLine[0] == '[' )
        {
            bInLegend = EQUAL(pszLine, "[legend]");
            continue;
        }
        if( bInLegend )
        {
            osLegend += pszLine;
            osLegend += "\n";
        }
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
        if( pszKey && pszValue )
        {
            if( EQUAL(pszKey, "creator") )
                osCreator = pszValue;
            if( EQUAL(pszKey, "created") )
                osCreated = pszValue;
            else if( EQUAL(pszKey, "ncols") )
                nCols = atoi(pszValue);
            else if( EQUAL(pszKey, "nrows") )
                nRows = atoi(pszValue);
            else if( EQUAL(pszKey, "xmin") )
                dfXMin = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "ymin") )
                dfYMin = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "xmax") )
                dfXMax = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "ymax") )
                dfYMax = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "projection") )
                osProjection = pszValue;
            else if( EQUAL(pszKey, "nbands") )
                l_nBands = atoi(pszValue);
            else if( EQUAL(pszKey, "bandorder") )
                osBandOrder = pszValue;
            else if( EQUAL(pszKey, "datatype") )
                osDataType = pszValue;
            else if( EQUAL(pszKey, "byteorder") )
                osByteOrder = pszValue;
            else if( EQUAL(pszKey, "nodatavalue") )
                osNoDataValue = pszValue;
            else if( EQUAL(pszKey, "minvalue") )
                osMinValue = pszValue;
            else if( EQUAL(pszKey, "maxvalue") )
                osMaxValue = pszValue;
            else if( EQUAL(pszKey, "ratnames") )
                osRatNames = pszValue;
            else if( EQUAL(pszKey, "rattypes") )
                osRatTypes = pszValue;
            else if( EQUAL(pszKey, "ratvalues") )
                osRatValues = pszValue;
            else if( EQUAL(pszKey, "layername") )
                osLayerName = pszValue;
        }
        CPLFree(pszKey);
    }
    if( !GDALCheckDatasetDimensions(nCols, nRows) )
        return nullptr;
    if( !GDALCheckBandCount(l_nBands, FALSE) )
        return nullptr;

    GDALDataType eDT = GDT_Unknown;
    if( EQUAL(osDataType, "LOG1S") )
        eDT = GDT_Byte; // mapping TBC
    else if( EQUAL(osDataType, "INT1S") )
        eDT = GDT_Byte;
    else if( EQUAL(osDataType, "INT2S") )
        eDT = GDT_Int16;
    else if( EQUAL(osDataType, "INT4S") )
        eDT = GDT_Int32;
    //else if( EQUAL(osDataType, "INT8S") )
    //    eDT = GDT_UInt64; // unhandled
    else if( EQUAL(osDataType, "INT1U") )
        eDT = GDT_Byte;
    else if( EQUAL(osDataType, "INT2U") )
        eDT = GDT_UInt16;
    else if( EQUAL(osDataType, "INT4U") ) // Not documented
        eDT = GDT_UInt32;
    else if( EQUAL(osDataType, "FLT4S") )
        eDT = GDT_Float32;
    else if( EQUAL(osDataType, "FLT8S") )
        eDT = GDT_Float64;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled datatype=%s", osDataType.c_str() );
        return nullptr;
    }
    if( l_nBands > 1 && osBandOrder.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing 'bandorder'" );
        return nullptr;
    }

    bool bNativeOrder = true;
    if( EQUAL(osByteOrder, "little") )
    {
        bNativeOrder = CPL_TO_BOOL(CPL_IS_LSB);
    }
    else if( EQUAL(osByteOrder, "big") )
    {
        bNativeOrder = CPL_TO_BOOL(!CPL_IS_LSB);
    }
    else if( !EQUAL(osByteOrder, "") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unhandled byteorder=%s. Assuming native order",
                 osByteOrder.c_str() );
    }

    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    if( !ComputeSpacings(osBandOrder, nCols, nRows, l_nBands, eDT,
                         nPixelOffset, nLineOffset, nBandOffset) )
    {
        return nullptr;
    }

    CPLString osGriFilename;
    CPLString osDirname( CPLGetDirname(poOpenInfo->pszFilename) );
    CPLString osBasename( CPLGetBasename(poOpenInfo->pszFilename) );
    CPLString osGRDExtension( CPLGetExtension(poOpenInfo->pszFilename) );
    CPLString osGRIExtension( (osGRDExtension[0] == 'g') ? "gri" : "GRI" );
    char** papszSiblings = poOpenInfo->GetSiblingFiles();
    if( papszSiblings )
    {
        int iFile = CSLFindString(papszSiblings,
                            CPLFormFilename(nullptr, osBasename, osGRIExtension) );
        if( iFile < 0 )
            return nullptr;
        osGriFilename = CPLFormFilename( osDirname,
                                         papszSiblings[iFile], nullptr );
    }
    else
    {
        osGriFilename = CPLFormFilename( osDirname, osBasename, osGRIExtension );
    }

    VSILFILE* fpImage = VSIFOpenL( osGriFilename,
                            (poOpenInfo->eAccess == GA_Update) ? "rb+" : "rb" );
    if( fpImage == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot open %s", osGriFilename.c_str());
        return nullptr;
    }

    if( !RAWDatasetCheckMemoryUsage(
                        nCols, nRows, l_nBands,
                        GDALGetDataTypeSizeBytes(eDT),
                        nPixelOffset, nLineOffset, 0, nBandOffset,
                        fpImage) )
    {
        VSIFCloseL(fpImage);
        return nullptr;
    }

    RRASTERDataset* poDS = new RRASTERDataset;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->m_bGeoTransformValid = true;
    poDS->m_adfGeoTransform[0] = dfXMin;
    poDS->m_adfGeoTransform[1] = (dfXMax - dfXMin) / nCols;
    poDS->m_adfGeoTransform[2] = 0.0;
    poDS->m_adfGeoTransform[3] = dfYMax;
    poDS->m_adfGeoTransform[4] = 0.0;
    poDS->m_adfGeoTransform[5] = -(dfYMax - dfYMin) / nRows;
    poDS->m_osGriFilename = osGriFilename;
    poDS->m_fpImage = fpImage;
    poDS->m_bNativeOrder = bNativeOrder;
    poDS->m_osCreator = osCreator;
    poDS->m_osCreated = osCreated;
    poDS->m_osBandOrder = osBandOrder;
    poDS->m_osLegend = osLegend;

    if( !osProjection.empty() )
    {
        OGRSpatialReference oSRS;
        if( oSRS.importFromProj4( osProjection.c_str() ) == OGRERR_NONE )
        {
            char* pszWKT = nullptr;
            oSRS.exportToWkt( &pszWKT );
            if( pszWKT )
                poDS->m_osProjection = pszWKT;
            CPLFree( pszWKT );
        }
    }

    if( !osCreator.empty() )
        poDS->GDALDataset::SetMetadataItem("CREATOR", osCreator);

    if( !osCreated.empty() )
        poDS->GDALDataset::SetMetadataItem("CREATED", osCreated);

    // Instantiate RAT
    if( !osRatNames.empty() && !osRatTypes.empty() && !osRatValues.empty() )
    {
        CPLStringList aosRatNames(CSLTokenizeString2(osRatNames, ":", 0));
        CPLStringList aosRatTypes(CSLTokenizeString2(osRatTypes, ":", 0));
        CPLStringList aosRatValues(CSLTokenizeString2(osRatValues, ":", 0));
        if( aosRatNames.size() >= 1 &&
            aosRatNames.size() == aosRatTypes.size() &&
            (aosRatValues.size() % aosRatNames.size()) == 0 )
        {
            bool bIsCompatibleOfCT = false;
            const int nValues = aosRatValues.size() / aosRatNames.size();
            if( (aosRatNames.size() == 4 || aosRatNames.size() == 5) &&
                EQUAL(aosRatNames[1], "red") &&
                EQUAL(aosRatNames[2], "green") &&
                EQUAL(aosRatNames[3], "blue") &&
                (aosRatNames.size() == 4 || EQUAL(aosRatNames[4], "alpha")) &&
                EQUAL(aosRatTypes[0], "integer") &&
                EQUAL(aosRatTypes[1], "integer") &&
                EQUAL(aosRatTypes[2], "integer") &&
                EQUAL(aosRatTypes[3], "integer") &&
                (aosRatTypes.size() == 4 || EQUAL(aosRatTypes[4], "integer")) )
            {
                bIsCompatibleOfCT = true;
                poDS->m_poCT.reset(new GDALColorTable());
                for( int i = 0; i < nValues; i++ )
                {
                    const int nIndex = atoi(aosRatValues[i]);
                    if( nIndex >= 0 && nIndex < 65536 )
                    {
                        const int nRed = atoi(aosRatValues[nValues+i]);
                        const int nGreen = atoi(aosRatValues[2*nValues+i]);
                        const int nBlue = atoi(aosRatValues[3*nValues+i]);
                        const int nAlpha = aosRatTypes.size() == 4 ? 255 :
                                            atoi(aosRatValues[4*nValues+i]);
                        const GDALColorEntry oEntry =
                        {
                            static_cast<short>(nRed),
                            static_cast<short>(nGreen),
                            static_cast<short>(nBlue),
                            static_cast<short>(nAlpha)
                        };

                        poDS->m_poCT->SetColorEntry(nIndex, &oEntry);
                    }
                    else
                    {
                        bIsCompatibleOfCT = false;
                        poDS->m_poCT.reset();
                        break;
                    }
                }
            }

            // cppcheck-suppress knownConditionTrueFalse
            if( !bIsCompatibleOfCT )
            {
                poDS->m_poRAT.reset(new GDALDefaultRasterAttributeTable());
                for( int i = 0; i < aosRatNames.size(); i++ )
                {
                    poDS->m_poRAT->CreateColumn(
                        aosRatNames[i],
                        EQUAL(aosRatTypes[i], "integer") ?  GFT_Integer :
                        EQUAL(aosRatTypes[i], "numeric") ?  GFT_Real :
                                                            GFT_String,
                        EQUAL(aosRatNames[i], "red") ? GFU_Red :
                        EQUAL(aosRatNames[i], "green") ? GFU_Green :
                        EQUAL(aosRatNames[i], "blue") ? GFU_Blue :
                        EQUAL(aosRatNames[i], "alpha") ? GFU_Alpha :
                        EQUAL(aosRatNames[i], "name") ? GFU_Name :
                        EQUAL(aosRatNames[i], "pixelcount") ? GFU_PixelCount :
                                                             GFU_Generic
                    );
                }
                for( int i = 0; i < nValues; i++ )
                {
                    for( int j = 0; j < aosRatTypes.size(); j++ )
                    {
                        if( poDS->m_poRAT->GetTypeOfCol(j) == GFT_Integer )
                        {
                             poDS->m_poRAT->SetValue(i, j,
                                    atoi(aosRatValues[j * nValues + i]));
                        }
                        else if( poDS->m_poRAT->GetTypeOfCol(j) == GFT_Real )
                        {
                             poDS->m_poRAT->SetValue(i, j,
                                    CPLAtof(aosRatValues[j * nValues + i]));
                        }
                        else
                        {
                             poDS->m_poRAT->SetValue(i, j,
                                    aosRatValues[j * nValues + i]);
                        }
                    }
                }
            }
        }
    }

    CPLStringList aosMinValues(CSLTokenizeString2(osMinValue, ":", 0));
    CPLStringList aosMaxValues(CSLTokenizeString2(osMaxValue, ":", 0));

    CPLStringList aosLayerNames(CSLTokenizeString2(osLayerName, ":", 0));
    for( int i=1; i<=l_nBands; i++ )
    {
        RRASTERRasterBand* poBand = new RRASTERRasterBand(
                                  poDS, i, fpImage, nBandOffset * (i-1),
                                  nPixelOffset,
                                  nLineOffset, eDT, bNativeOrder );
        poDS->SetBand( i, poBand );
        if( EQUAL(osDataType, "INT1S") )
        {
            poDS->GetRasterBand(i)->GDALRasterBand::SetMetadataItem(
                    "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
        }
        if( !osNoDataValue.empty() && !EQUAL(osNoDataValue, "NA") )
        {
            double dfNoDataValue = CPLAtof(osNoDataValue);
            if( eDT == GDT_Float32 )
                dfNoDataValue = CastToFloat(dfNoDataValue);
            poBand->m_bHasNoDataValue = true;
            poBand->m_dfNoDataValue = dfNoDataValue;
        }
        if( i - 1 < static_cast<int>(aosMinValues.size()) &&
            i - 1 < static_cast<int>(aosMaxValues.size()) )
        {
            poBand->SetMinMax( CPLAtof(aosMinValues[i-1]),
                               CPLAtof(aosMaxValues[i-1]) );
        }
        if( i - 1 < static_cast<int>(aosLayerNames.size()) )
        {
            const CPLString osName(aosLayerNames[i-1]);
            poBand->GDALRasterBand::SetDescription(osName);
            if( EQUAL(osName, "red") )
                poBand->SetColorInterpretation( GCI_RedBand );
            else if( EQUAL(osName, "green") )
                poBand->SetColorInterpretation( GCI_GreenBand );
            else if( EQUAL(osName, "blue") )
                poBand->SetColorInterpretation( GCI_BlueBand );
            else if( EQUAL(osName, "alpha") )
                poBand->SetColorInterpretation( GCI_AlphaBand );
        }
        poBand->m_poRAT = poDS->m_poRAT;
        poBand->m_poCT = poDS->m_poCT;
        if( poBand->m_poCT )
            poBand->SetColorInterpretation(GCI_PaletteIndex);
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *RRASTERDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszOptions )

{
    // Verify input options.
    if (nBands <= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "RRASTER driver does not support %d bands.", nBands);
        return nullptr;
    }

    if( eType != GDT_Byte &&
        eType != GDT_UInt16 && eType != GDT_Int16 &&
        eType != GDT_Int32 && eType != GDT_UInt32 &&
        eType != GDT_Float32 && eType != GDT_Float64 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported data type (%s).",
                 GDALGetDataTypeName(eType));
        return nullptr;
    }

    CPLString osGRDExtension(CPLGetExtension(pszFilename));
    if( !EQUAL(osGRDExtension, "grd") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "RRASTER driver only supports grd extension");
        return nullptr;
    }

    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    CPLString osBandOrder(
        CSLFetchNameValueDef(papszOptions, "INTERLEAVE", "BIL"));
    if( !ComputeSpacings(osBandOrder,
                         nXSize, nYSize, nBands, eType,
                         nPixelOffset, nLineOffset, nBandOffset) )
    {
        return nullptr;
    }

    CPLString osGRIExtension( (osGRDExtension[0] == 'g') ? "gri" : "GRI" );
    CPLString osGriFilename( CPLResetExtension(pszFilename, osGRIExtension) );

    // Try to create the file.
    VSILFILE *fpImage = VSIFOpenL(osGriFilename, "wb+");

    if( fpImage == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.",
                 osGriFilename.c_str());
        return nullptr;
    }

    RRASTERDataset* poDS = new RRASTERDataset;
    poDS->eAccess = GA_Update;
    poDS->m_bHeaderDirty = true;
    poDS->m_osGriFilename = osGriFilename;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->m_fpImage = fpImage;
    poDS->m_bNativeOrder = true;
    poDS->m_osBandOrder = osBandOrder.toupper();
    poDS->m_bInitRaster = CPLFetchBool(papszOptions, "@INIT_RASTER", true);

    const char *pszPixelType = CSLFetchNameValue(papszOptions, "PIXELTYPE");
    const bool bByteSigned = (eType == GDT_Byte && pszPixelType &&
                        EQUAL(pszPixelType, "SIGNEDBYTE"));

    for( int i=1; i<=nBands; i++ )
    {
        RRASTERRasterBand* poBand = new RRASTERRasterBand(
                                  poDS, i, fpImage, nBandOffset * (i-1),
                                  nPixelOffset,
                                  nLineOffset, eType, true );
        poDS->SetBand( i, poBand );
        if( bByteSigned)
        {
            poBand->GDALRasterBand::SetMetadataItem(
                    "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
        }
    }

    return poDS;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *RRASTERDataset::CreateCopy( const char * pszFilename,
                                      GDALDataset * poSrcDS,
                                      int bStrict, char ** papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void * pProgressData )

{
    // Proceed with normal copying using the default createcopy  operators.
    GDALDriver *poDriver =
        reinterpret_cast<GDALDriver *>(GDALGetDriverByName("RRASTER"));

    char** papszAdjustedOptions = CSLDuplicate(papszOptions);
    papszAdjustedOptions =
        CSLSetNameValue(papszAdjustedOptions, "@INIT_RASTER", "NO");
    GDALDataset *poOutDS = poDriver->DefaultCreateCopy(
        pszFilename, poSrcDS, bStrict, papszAdjustedOptions, pfnProgress,
        pProgressData);
    CSLDestroy(papszAdjustedOptions);

    if( poOutDS != nullptr )
        poOutDS->FlushCache();

    return poOutDS;
}

/************************************************************************/
/*                   GDALRegister_RRASTER()                             */
/************************************************************************/

void GDALRegister_RRASTER()

{
    if( GDALGetDriverByName( "RRASTER" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "RRASTER" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "R Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/rraster.html" );
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Float32 Float64");

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to "
    "SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"   <Option name='INTERLEAVE' type='string-select' default='BIL'>"
"       <Value>BIP</Value>"
"       <Value>BIL</Value>"
"       <Value>BSQ</Value>"
"   </Option>"
"</CreationOptionList>" );

    poDriver->pfnOpen = RRASTERDataset::Open;
    poDriver->pfnIdentify = RRASTERDataset::Identify;
    poDriver->pfnCreate = RRASTERDataset::Create;
    poDriver->pfnCreateCopy = RRASTERDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
