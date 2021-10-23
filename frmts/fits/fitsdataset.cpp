/******************************************************************************
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
 * Copyright (c) 2008-2020, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Chiara Marmo <chiara dot marmo at u-psud dot fr>
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

// So that OFF_T is 64 bits
#define _FILE_OFFSET_BITS 64

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

#include <string.h>
#include <fitsio.h>

#include <algorithm>
#include <string>
#include <cstring>
#include <set>
#include <vector>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              FITSDataset                             */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand;
class FITSLayer;

class FITSDataset final : public GDALPamDataset {

  friend class FITSRasterBand;
  friend class FITSLayer;

  fitsfile* m_hFITS = nullptr;

  int       m_hduNum = 0;
  GDALDataType m_gdalDataType = GDT_Unknown;   // GDAL code for the image type
  int m_fitsDataType = 0;   // FITS code for the image type

  bool m_isExistingFile = false;
  LONGLONG m_highestOffsetWritten = 0;  // How much of image has been written

  bool        m_bNoDataChanged = false;
  bool        m_bNoDataSet = false;
  double      m_dfNoDataValue = -9999.0;

  bool        m_bMetadataChanged = false;

  CPLStringList m_aosSubdatasets{};

  OGRSpatialReference m_oSRS{};

  double      m_adfGeoTransform[6];
  bool        m_bGeoTransformValid = false;

  bool        m_bFITSInfoChanged = false;

  std::vector<std::unique_ptr<FITSLayer>> m_apoLayers{};

  CPLErr Init(fitsfile* hFITS, bool isExistingFile, int hduNum);

  void        LoadGeoreferencing();
  void        LoadFITSInfo();
  void        WriteFITSInfo();
  void        LoadMetadata(GDALMajorObject* poTarget);

public:

  FITSDataset();     // Others should not call this constructor explicitly
  ~FITSDataset();

  static GDALDataset* Open( GDALOpenInfo* );
  static int          Identify( GDALOpenInfo* );
  static GDALDataset* Create( const char* pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              char** papszParamList );
  static CPLErr Delete( const char * pszFilename );

  const OGRSpatialReference* GetSpatialRef() const override;
  CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;
  virtual CPLErr GetGeoTransform( double * ) override;
  virtual CPLErr SetGeoTransform( double * ) override;
  char** GetMetadata(const char* papszDomain = nullptr) override;

  int GetLayerCount() override { return static_cast<int>(m_apoLayers.size()); }
  OGRLayer* GetLayer(int) override;

  OGRLayer* ICreateLayer(const char *pszName,
                         OGRSpatialReference* poSRS,
                         OGRwkbGeometryType eGType,
                         char ** papszOptions ) override;
  int         TestCapability( const char * pszCap ) override;

  bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

};

/************************************************************************/
/* ==================================================================== */
/*                            FITSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand final: public GDALPamRasterBand {

  friend class  FITSDataset;

  bool               m_bHaveOffsetScale = false;
  double             m_dfOffset = 0.0;
  double             m_dfScale = 1.0;

 protected:
    FITSDataset       *m_poFDS = nullptr;

    bool               m_bNoDataSet = false;
    double             m_dfNoDataValue = -9999.0;

 public:

  FITSRasterBand(FITSDataset*, int);
  virtual ~FITSRasterBand();

  virtual CPLErr IReadBlock( int, int, void * ) override;
  virtual CPLErr IWriteBlock( int, int, void * ) override;

  virtual double GetNoDataValue( int * ) override final;
  virtual CPLErr SetNoDataValue( double ) override final;
  virtual CPLErr DeleteNoDataValue() override final;

  virtual double GetOffset( int *pbSuccess = nullptr ) override final;
  virtual CPLErr SetOffset( double dfNewValue ) override final;
  virtual double GetScale( int *pbSuccess = nullptr ) override final;
  virtual CPLErr SetScale( double dfNewValue ) override final;

};

/************************************************************************/
/* ==================================================================== */
/*                              FITSLayer                               */
/* ==================================================================== */
/************************************************************************/
namespace
{
    struct ColDesc
    {
        std::string typechar{};
        int         iCol = 0; // numbering starting at 1
        int         iBit = 0; // numbering starting at 1
        int         nRepeat = 0;
        int         nItems = 1;
        double      dfOffset = 0;
        double      dfScale = 1;
        bool        bHasNull = false;
        LONGLONG    nNullValue = 0;
        int         nTypeCode = 0; // unset
    };
}

class FITSLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<FITSLayer>
{
    friend class FITSDataset;
    FITSDataset         *m_poDS = nullptr;
    int                  m_hduNum = 0;
    OGRFeatureDefn      *m_poFeatureDefn = nullptr;
    LONGLONG             m_nCurRow = 1;
    LONGLONG             m_nRows = 0;

    std::vector<ColDesc> m_aoColDescs;

    CPLStringList        m_aosCreationOptions{};

    std::vector<int>     m_anDeferredFieldsIndices{};

    OGRFeature*     GetNextRawFeature();
    void            SetActiveHDU();
    void            RunDeferredFieldCreation(const OGRFeature* poFeature = nullptr);
    bool            SetOrCreateFeature(const OGRFeature* poFeature, LONGLONG nRow);

public:

        FITSLayer(FITSDataset* poDS, int hduNum, const char* pszExtName);
        ~FITSLayer();

        OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }
        void            ResetReading() override;
        int             TestCapability(const char*) override;
        OGRFeature     *GetFeature(GIntBig) override;
        GIntBig         GetFeatureCount(int bForce) override;
        OGRErr          CreateField( OGRFieldDefn *poField, int bApproxOK ) override;
        OGRErr          ICreateFeature(OGRFeature* poFeature) override;
        OGRErr          ISetFeature(OGRFeature* poFeature) override;
        OGRErr          DeleteFeature(GIntBig nFID) override;

        DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(FITSLayer)

        void            SetCreationOptions(CSLConstList papszOptions) { m_aosCreationOptions = papszOptions; }
};

/************************************************************************/
/*                            FITSLayer()                               */
/************************************************************************/

FITSLayer::FITSLayer(FITSDataset* poDS, int hduNum, const char* pszExtName):
    m_poDS(poDS), m_hduNum(hduNum)
{
    if( pszExtName[0] != 0 )
        m_poFeatureDefn = new OGRFeatureDefn(pszExtName);
    else
        m_poFeatureDefn = new OGRFeatureDefn(CPLSPrintf("Table HDU %d", hduNum));
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);
    SetDescription(m_poFeatureDefn->GetName());

    SetActiveHDU();

    m_poDS->LoadMetadata(this);

    int status = 0;
    fits_get_num_rowsll(m_poDS->m_hFITS, &m_nRows, &status);
    if( status )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_get_num_rowsll() failed");
    }
    int nCols = 0;
    status = 0;
    fits_get_num_cols(m_poDS->m_hFITS, &nCols, &status);
    if( status )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_get_num_cols() failed");
    }

    std::vector<std::string> aosNames(nCols);
    std::vector<char*> apszNames(nCols);
    for( int i = 0; i < nCols; i++ )
    {
        aosNames[i].resize(80);
        apszNames[i] = &aosNames[i][0];
    }

    status = 0;
    fits_read_btblhdrll(m_poDS->m_hFITS, nCols, nullptr, nullptr,
                        &apszNames[0],
                        nullptr,
                        nullptr, nullptr, nullptr, &status);
    if( status )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_read_btblhdrll() failed");
    }

    for(int i = 0; i < nCols; i++ )
    {
        aosNames[i].resize(strlen(aosNames[i].c_str()));

        char typechar[80];
        LONGLONG nRepeat = 0;
        double dfScale = 0;
        double dfOffset = 0;
        status = 0;
        fits_get_bcolparmsll(m_poDS->m_hFITS, i+1,
                             nullptr, // column name
                             nullptr, // unit
                             typechar,
                             &nRepeat,
                             &dfScale,
                             &dfOffset,
                             nullptr, // nulval
                             nullptr, // tdisp
                             &status);
        if( status )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "fits_get_bcolparmsll() failed");
        }

        ColDesc oCol;

        status = 0;
        fits_read_key(m_poDS->m_hFITS, TLONGLONG, CPLSPrintf("TNULL%d", i+1),
                      &oCol.nNullValue, nullptr, &status);
        oCol.bHasNull = status == 0;

        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        if( typechar[0] == 'L' ) // Logical
        {
            eType = OFTInteger;
            eSubType = OFSTBoolean;
        }
        else if( typechar[0] == 'X' ) // Bit array
        {
            if( nRepeat > 128 )
            {
                CPLDebug("FITS", "Too large repetition count for column %s",
                         aosNames[i].c_str());
                continue;
            }
            for( int j = 1; j <= nRepeat; j++ )
            {
                OGRFieldDefn oFieldDefn(
                    (aosNames[i] + CPLSPrintf("_bit%d", j)).c_str(),
                    OFTInteger);
                m_poFeatureDefn->AddFieldDefn(&oFieldDefn);

                ColDesc oColBit;
                oColBit.typechar = typechar;
                oColBit.iCol = i + 1;
                oColBit.iBit = j;
                m_aoColDescs.emplace_back(oColBit);
            }
            continue;
        }
        else if( typechar[0] == 'B' ) // Unsigned byte
        {
            if( dfOffset == -128 && dfScale == 1 )
            {
                eType = OFTInteger; // signed byte
                oCol.nTypeCode = TSBYTE;
                // fits_read_col() automatically offsets numeric values
                dfOffset = 0;
            }
            else if( dfOffset != 0 || dfScale != 1 )
                eType = OFTReal;
            else
                eType = OFTInteger;
        }
        else if( typechar[0] == 'I' ) // 16-bit signed integer
        {
            if( dfOffset == 32768.0 && dfScale == 1 )
            {
                eType = OFTInteger; // unsigned 16-bit integer
                oCol.nTypeCode = TUSHORT;
                // fits_read_col() automatically offsets numeric values
                dfOffset = 0;
            }
            else if( dfOffset != 0 || dfScale != 1 )
                eType = OFTReal;
            else
            {
                eType = OFTInteger;
                eSubType = OFSTInt16;
            }
        }
        else if( typechar[0] == 'J' ) // 32-bit signed integer
        {
            if( dfOffset == 2147483648.0 && dfScale == 1 )
            {
                eType = OFTInteger64; // unsigned 32-bit integer --> needs to promote to 64 bits
                oCol.nTypeCode = TUINT;
                // fits_read_col() automatically offsets numeric values
                dfOffset = 0;
            }
            else if( dfOffset != 0 || dfScale != 1 )
                eType = OFTReal;
            else
                eType = OFTInteger;
        }
        else if( typechar[0] == 'K' ) // 64-bit signed integer
        {
            if( dfOffset != 0 || dfScale != 1 )
                eType = OFTReal;
            else
                eType = OFTInteger64;
        }
        else if( typechar[0] == 'A' ) // Character
        {

            status = 0;
            LONGLONG nWidth = 0;
            fits_get_coltypell(m_poDS->m_hFITS, i+1,
                            nullptr, // typecode
                            nullptr, // repeat
                            &nWidth,
                            &status);
            if( status )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "fits_get_coltypell() failed");
            }
            if( nRepeat >= 2 * nWidth && nWidth != 0)
            {
                oCol.nItems = static_cast<int>(nRepeat / nWidth);
                eType = OFTStringList;
                nRepeat = nWidth;
            }
            else
            {
                eType = OFTString;
            }
        }
        else if( typechar[0] == 'E' ) // IEEE754 32bit
        {
            eType = OFTReal;
            if( dfOffset == 0 && dfScale == 1 )
                eSubType = OFSTFloat32;
            // fits_read_col() automatically scales numeric values
            dfOffset = 0;
            dfScale = 1;
        }
        else if( typechar[0] == 'D' ) // IEEE754 64bit
        {
            eType = OFTReal;
            // fits_read_col() automatically scales numeric values
            dfOffset = 0;
            dfScale = 1;
        }
        else if( typechar[0] == 'C' ) // IEEE754 32bit complex
        {
            eType = OFTString;
            // fits_read_col() automatically scales numeric values
            dfOffset = 0;
            dfScale = 1;
        }
        else if( typechar[0] == 'M' ) // IEEE754 64bit complex
        {
            eType = OFTString;
            // fits_read_col() automatically scales numeric values
            dfOffset = 0;
            dfScale = 1;
        }
        else if( typechar[0] == 'P' || typechar[0] == 'Q' ) // Array
        {
            if( typechar[1] == 'L' )
            {
                nRepeat = 0;
                eType = OFTIntegerList;
                eSubType = OFSTBoolean;
            }
            else if( typechar[1] == 'B' )
            {
                nRepeat = 0;
                eType = OFTIntegerList;
            }
            else if( typechar[1] == 'I' )
            {
                nRepeat = 0;
                eType = OFTIntegerList;
                eSubType = OFSTInt16;
            }
            else if( typechar[1] == 'J' )
            {
                nRepeat = 0;
                eType = OFTIntegerList;
            }
            else if( typechar[1] == 'K' )
            {
                nRepeat = 0;
                eType = OFTInteger64List;
            }
            else if( typechar[1] == 'A' )
            {
                eType = OFTString;
            }
            else if( typechar[1] == 'E' )
            {
                nRepeat = 0;
                eType = OFTRealList;
                if( dfOffset == 0 && dfScale == 1 )
                    eSubType = OFSTFloat32;
                // fits_read_col() automatically scales numeric values
                dfOffset = 0;
                dfScale = 1;
            }
            else if( typechar[1] == 'D' )
            {
                nRepeat = 0;
                eType = OFTRealList;
                // fits_read_col() automatically scales numeric values
                dfOffset = 0;
                dfScale = 1;
            }
            else if( typechar[1] == 'C' )
            {
                nRepeat = 0;
                eType = OFTStringList;
                // fits_read_col() automatically scales numeric values
                dfOffset = 0;
                dfScale = 1;
            }
            else if( typechar[1] == 'M' )
            {
                nRepeat = 0;
                eType = OFTStringList;
                // fits_read_col() automatically scales numeric values
                dfOffset = 0;
                dfScale = 1;
            }
            else
            {
                CPLDebug("FITS", "Unhandled type %s", typechar);
                continue;
            }
        }
        else
        {
            CPLDebug("FITS", "Unhandled type %s", typechar);
            continue;
        }

        if( nRepeat > 1 && typechar[0] != 'A' )
        {
            if( eType == OFTInteger )
                eType = OFTIntegerList;
            else if( eType == OFTInteger64 )
                eType = OFTInteger64List;
            else if( eType == OFTReal )
                eType = OFTRealList;
            else if( eType == OFTString )
                eType = OFTStringList;
        }

        OGRFieldDefn oFieldDefn( aosNames[i].c_str(), eType );
        oFieldDefn.SetSubType(eSubType);
        if( typechar[0] == 'A' )
            oFieldDefn.SetWidth(static_cast<int>(nRepeat));
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);

        oCol.typechar = typechar;
        oCol.iCol = i + 1;
        oCol.nRepeat = static_cast<int>(nRepeat);
        oCol.dfOffset = dfOffset;
        oCol.dfScale = dfScale;
        m_aoColDescs.emplace_back(oCol);
    }
}

/************************************************************************/
/*                           ~FITSLayer()                               */
/************************************************************************/

FITSLayer::~FITSLayer()
{
    RunDeferredFieldCreation();

    for( int i = 0; i < m_aosCreationOptions.size(); i++ )
    {
        if( STARTS_WITH_CI(m_aosCreationOptions[i], "REPEAT_") )
        {
            char* pszKey = nullptr;
            CPL_IGNORE_RET_VAL(CPLParseNameValue(
                                    m_aosCreationOptions[i], &pszKey));
            if( pszKey && m_poFeatureDefn->GetFieldIndex(
                                    pszKey + strlen("REPEAT_")) < 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Creation option %s ignored as field does not exist",
                        m_aosCreationOptions[i]);
            }
            CPLFree(pszKey);
        }
    }

    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            SetActiveHDU()                            */
/************************************************************************/

void FITSLayer::SetActiveHDU()
{
    int status = 0;
    fits_movabs_hdu(m_poDS->m_hFITS, m_hduNum, nullptr, &status);
    if( status != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_movabs_hdu() failed: %d", status);
    }
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig FITSLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery == nullptr && m_poFilterGeom == nullptr )
        return m_nRows;
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void FITSLayer::ResetReading()
{
    m_nCurRow = 1;
}

/************************************************************************/
/*                              ReadCol                                 */
/************************************************************************/

template<typename T_FITS, typename T_GDAL, int TYPECODE> struct ReadCol
{
    static void Read(fitsfile* hFITS, const ColDesc& colDesc,
                     int iField, LONGLONG irow, OGRFeature* poFeature,
                     int nRepeat)
    {
        int status = 0;
        std::vector<T_FITS> x(nRepeat);
        fits_read_col(hFITS, TYPECODE,
                        colDesc.iCol, irow, 1, nRepeat, nullptr,
                        &x[0], nullptr, &status);
        if( nRepeat == 1 && colDesc.bHasNull &&
            x[0] == static_cast<T_FITS>(colDesc.nNullValue) )
        {
            poFeature->SetFieldNull(iField);
        }
        else if( colDesc.dfScale != 1.0 || colDesc.dfOffset != 0.0 )
        {
            std::vector<double> scaled;
            scaled.reserve(nRepeat);
            for( int i = 0; i < nRepeat; ++i )
            {
                scaled.push_back(static_cast<double>(x[i]) *
                                    colDesc.dfScale + colDesc.dfOffset);
            }
            poFeature->SetField(iField, nRepeat, &scaled[0]);
        }
        else if( nRepeat == 1 )
        {
            poFeature->SetField(iField, static_cast<T_GDAL>(x[0]));
        }
        else
        {
            std::vector<T_GDAL> xGDAL;
            xGDAL.reserve(nRepeat);
            for( int i = 0; i < nRepeat; i++ )
                xGDAL.push_back(x[i]);
            poFeature->SetField(iField, nRepeat, &xGDAL[0]);
        }
    }
};

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature* FITSLayer::GetNextRawFeature()
{
    auto poFeature = GetFeature(m_nCurRow);
    if( poFeature )
        m_nCurRow ++;
    return poFeature;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* FITSLayer::GetFeature(GIntBig nFID)
{
    LONGLONG nRow = static_cast<LONGLONG>(nFID);
    if( nRow <= 0 || nRow > m_nRows )
        return nullptr;

    RunDeferredFieldCreation();

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);

    SetActiveHDU();

    const auto ReadField = [this, &poFeature, nRow](const ColDesc& colDesc,
                                              int iField,
                                              char typechar,
                                              int nRepeat)
    {
        int status = 0;
        if( typechar == 'L' )
        {
            std::vector<char> x(nRepeat);
            fits_read_col(m_poDS->m_hFITS, TLOGICAL,
                          colDesc.iCol, nRow, 1, nRepeat, nullptr,
                          &x[0], nullptr, &status);
            if( nRepeat == 1 )
            {
                poFeature->SetField(iField, x[0] == '1' ? 1 : 0);
            }
            else
            {
                std::vector<int> intValues;
                intValues.reserve(nRepeat);
                for( int i = 0; i < nRepeat; ++i )
                {
                    intValues.push_back(x[i] == '1' ? 1 : 0);
                }
                poFeature->SetField(iField, nRepeat, &intValues[0]);
            }
        }
        else if( typechar == 'X' )
        {
            char x = 0;
            fits_read_col_bit(m_poDS->m_hFITS, colDesc.iCol, nRow,
                              colDesc.iBit, 1, &x, &status);
            poFeature->SetField(iField, x);
        }
        else if( typechar == 'B' )
        {
            if( colDesc.nTypeCode == TSBYTE )
            {
                ReadCol<signed char, int, TSBYTE>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
            else
            {
                ReadCol<GByte, int, TBYTE>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
        }
        else if( typechar == 'I' )
        {
            if( colDesc.nTypeCode == TUSHORT )
            {
                ReadCol<GUInt16, int, TUSHORT>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
            else
            {
                ReadCol<GInt16, int, TSHORT>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
        }
        else if( typechar == 'J' )
        {
            if( colDesc.nTypeCode == TUINT )
            {
                ReadCol<GUInt32, GIntBig, TUINT>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
            else
            {
                ReadCol<GInt32, int, TINT>::Read(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature, nRepeat);
            }
        }
        else if( typechar == 'K' )
        {
            ReadCol<GInt64, GIntBig, TLONGLONG>::Read(
                m_poDS->m_hFITS, colDesc, iField,
                nRow, poFeature, nRepeat);
        }
        else if( typechar == 'A' ) // Character
        {
            if( colDesc.nItems > 1 )
            {
                CPLStringList aosList;
                for(int iItem = 1; iItem <= colDesc.nItems; iItem++ )
                {
                    std::string osStr;
                    osStr.resize(nRepeat);
                    char* pszStr = &osStr[0];
                    fits_read_col_str(m_poDS->m_hFITS, colDesc.iCol, nRow, iItem, 1,
                                    nullptr, &pszStr, nullptr, &status);
                    aosList.AddString(pszStr);
                }
                poFeature->SetField(iField, aosList.List());
            }
            else
            {
                std::string osStr;
                osStr.resize(nRepeat);
                char* pszStr = &osStr[0];
                fits_read_col_str(m_poDS->m_hFITS, colDesc.iCol, nRow, 1, 1,
                                nullptr, &pszStr, nullptr, &status);
                poFeature->SetField(iField, osStr.c_str());
            }
        }
        else if( typechar == 'E' ) // IEEE754 32bit
        {
            ReadCol<float, double, TFLOAT>::Read(m_poDS->m_hFITS, colDesc, iField,
                                                 nRow, poFeature, nRepeat);
        }
        else if( typechar == 'D' ) // IEEE754 64bit
        {
            std::vector<double> x(nRepeat);
            fits_read_col(m_poDS->m_hFITS, TDOUBLE,
                            colDesc.iCol, nRow, 1, nRepeat, nullptr,
                            &x[0], nullptr, &status);
            if( nRepeat == 1 )
                poFeature->SetField(iField, x[0]);
            else
                poFeature->SetField(iField, nRepeat, &x[0]);
        }
        else if( typechar == 'C' ) // IEEE754 32bit complex
        {
            std::vector<float> x(2 * nRepeat);
            fits_read_col(m_poDS->m_hFITS, TCOMPLEX,
                          colDesc.iCol, nRow, 1, nRepeat, nullptr,
                          &x[0], nullptr, &status);
            CPLStringList aosList;
            for( int i = 0; i < nRepeat; ++i )
                aosList.AddString(CPLSPrintf("%.18g + %.18gj",
                                                x[2*i+0], x[2*i+1]));
            if( nRepeat == 1 )
                poFeature->SetField(iField, aosList[0]);
            else
                poFeature->SetField(iField, aosList.List());
        }
        else if( typechar == 'M' ) // IEEE754 64bit complex
        {
            std::vector<double> x(2 * nRepeat);
            fits_read_col(m_poDS->m_hFITS, TDBLCOMPLEX,
                          colDesc.iCol, nRow, 1, nRepeat, nullptr,
                          &x[0], nullptr, &status);
            CPLStringList aosList;
            for( int i = 0; i < nRepeat; ++i )
            {
                aosList.AddString(CPLSPrintf("%.18g + %.18gj",
                    x[2*i+0],
                    x[2*i+1]));
            }
            if( nRepeat == 1 )
                poFeature->SetField(iField, aosList[0]);
            else
                poFeature->SetField(iField, aosList.List());
        }
        else
        {
            CPLDebug("FITS", "Unhandled typechar %c", typechar);
        }
        if( status )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "fits_read_col() failed");
        }
    };

    const int nFieldCount = poFeature->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        const auto& colDesc = m_aoColDescs[iField];
        if( colDesc.typechar[0] == 'P' || colDesc.typechar[0] == 'Q' )
        {
            int status = 0;
            LONGLONG nRepeat = 0;
            fits_read_descriptll(m_poDS->m_hFITS,colDesc.iCol, nRow,
                                 &nRepeat, nullptr, &status);
            ReadField(colDesc, iField, colDesc.typechar[1],
                      static_cast<int>(nRepeat));
        }
        else
        {
            ReadField(colDesc, iField, colDesc.typechar[0], colDesc.nRepeat);
        }
    }
    poFeature->SetFID(nRow);
    return poFeature;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int FITSLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;

    if( EQUAL(pszCap, OLCRandomRead) )
        return true;

    if( EQUAL(pszCap, OLCCreateField) ||
        EQUAL(pszCap, OLCSequentialWrite) ||
        EQUAL(pszCap, OLCRandomWrite) ||
        EQUAL(pszCap, OLCDeleteFeature) )
    {
        return m_poDS->GetAccess() == GA_Update;
    }

    return false;
}

/************************************************************************/
/*                        RunDeferredFieldCreation()                    */
/************************************************************************/

void FITSLayer::RunDeferredFieldCreation(const OGRFeature* poFeature)
{
    if( m_anDeferredFieldsIndices.empty() )
        return;

    SetActiveHDU();

    CPLString            osPendingBitFieldName{};
    int                  nPendingBitFieldSize = 0;
    std::set<CPLString>  oSetBitFieldNames{};

    const auto FlushCreationPendingBitField =
        [this, &osPendingBitFieldName, &nPendingBitFieldSize, &oSetBitFieldNames]()
    {
        if( osPendingBitFieldName.empty() )
            return;

        const int iCol = m_aoColDescs.empty() ? 1 : m_aoColDescs.back().iCol + 1;
        for( int iBit = 1; iBit <= nPendingBitFieldSize; iBit++ )
        {
            ColDesc oCol;
            oCol.iCol = iCol;
            oCol.iBit = iBit;
            oCol.typechar = 'X';
            m_aoColDescs.emplace_back(oCol);
        }

        int status = 0;
        CPLString osTForm;
        osTForm.Printf("%dX", nPendingBitFieldSize);
        fits_insert_col(m_poDS->m_hFITS, iCol,
                        &osPendingBitFieldName[0], &osTForm[0], &status);
        if( status != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "fits_insert_col() failed: %d", status);
        }

        oSetBitFieldNames.insert(osPendingBitFieldName);
        osPendingBitFieldName.clear();
        nPendingBitFieldSize = 0;
    };

    const bool bRepeatFromFirstFeature =
        poFeature != nullptr &&
        EQUAL(m_aosCreationOptions.FetchNameValueDef(
            "COMPUTE_REPEAT", "AT_FIELD_CREATION"), "AT_FIRST_FEATURE_CREATION");

    char** papszMD = GetMetadata();
    bool bFirstMD = true;

    std::map<CPLString, std::map<CPLString, CPLString>> oMapColNameToMetadata;

    // Remap column related metadata (likely coming from source FITS) to
    // actual column numbers
    std::map<int, CPLString> oMapFITSMDColToName;
    for( auto papszIter = papszMD; papszIter && *papszIter; ++papszIter )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszKey && pszValue )
        {
            bool bIgnore = false;
            for( const char* pszPrefix : {
                    "TTYPE", "TFORM", "TUNIT", "TNULL", "TSCAL", "TZERO",
                    "TDISP", "TDIM", "TBCOL", "TCTYP", "TCUNI", "TCRPX",
                    "TCRVL", "TCDLT", "TRPOS" } )
            {
                if( STARTS_WITH(pszKey, pszPrefix) )
                {
                    const char* pszCol = pszKey + strlen(pszPrefix);
                    const int nCol = atoi(pszCol);
                    if( !EQUAL(pszPrefix, "TTYPE") )
                    {
                        auto oIter = oMapFITSMDColToName.find(nCol);
                        CPLString osColName;
                        if( oIter == oMapFITSMDColToName.end() )
                        {
                            const char* pszColName = CSLFetchNameValue(papszMD,
                                              (std::string("TTYPE") + pszCol).c_str());
                            if( pszColName )
                            {
                                osColName = pszColName;
                                osColName.Trim();
                                oMapFITSMDColToName[nCol] = osColName;
                            }
                        }
                        else
                        {
                            osColName = oIter->second;
                        }
                        if( !osColName.empty() )
                        {
                            oMapColNameToMetadata[osColName][pszPrefix] = CPLString(pszValue).Trim();
                        }
                    }
                    bIgnore = true;
                    break;
                }
            }

            if( !bIgnore && strlen(pszKey) <= 8 &&
                !EQUAL(pszKey, "TFIELDS") && !EQUAL(pszKey, "EXTNAME") )
            {
                if( bFirstMD )
                {
                    int status = 0;
                    fits_write_key_longwarn(m_poDS->m_hFITS, &status);
                    bFirstMD = false;
                }

                char* pszValueNonConst = const_cast<char*>(pszValue);
                int status = 0;
                fits_update_key_longstr(m_poDS->m_hFITS, pszKey,
                                        pszValueNonConst, nullptr, &status);
            }
        }
        CPLFree(pszKey);
    }

    for( const int nFieldIdx: m_anDeferredFieldsIndices )
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(nFieldIdx);
        const auto& oFieldDefn = *poFieldDefn;
        const char* pszFieldName = oFieldDefn.GetNameRef();
        const OGRFieldType eType = oFieldDefn.GetType();
        const OGRFieldSubType eSubType = oFieldDefn.GetSubType();

        if( eType == OFTInteger )
        {
            const char* pszBit = strstr(pszFieldName, "_bit");
            long iBit = 0;
            char* pszEndPtr = nullptr;
            if( pszBit && (iBit = strtol(pszBit + strlen("_bit"),
                                         &pszEndPtr, 10)) > 0 &&
                pszEndPtr && *pszEndPtr == '\0' )
            {
                CPLString osName;
                osName.assign( pszFieldName, pszBit - pszFieldName );
                if( oSetBitFieldNames.find(osName) == oSetBitFieldNames.end() )
                {
                    if( !osPendingBitFieldName.empty() &&
                        osPendingBitFieldName != osName )
                    {
                        FlushCreationPendingBitField();
                    }

                    if( osPendingBitFieldName.empty() )
                    {
                        osPendingBitFieldName = osName;
                        nPendingBitFieldSize = 1;
                        continue;
                    }
                    else if( iBit == nPendingBitFieldSize + 1 )
                    {
                        nPendingBitFieldSize ++;
                        continue;
                    }
                }
            }
        }

        FlushCreationPendingBitField();

        CPLString osTForm;
        ColDesc oCol;
        oCol.iCol = m_aoColDescs.empty() ? 1 : m_aoColDescs.back().iCol + 1;
        oCol.nRepeat = 1;

        CPLString osRepeat;
        const char* pszRepeat = m_aosCreationOptions.FetchNameValue(
            (CPLString("REPEAT_") + pszFieldName).c_str());

        const auto osTFormFromMD = oMapColNameToMetadata[pszFieldName]["TFORM"];

        // For fields of type list, determine if we can know if it has a fixed
        // number of elements
        if( eType == OFTIntegerList || eType == OFTInteger64List ||
            eType == OFTRealList )
        {
            // First take into account the REPEAT_{FIELD_NAME} creatin option
            if( pszRepeat )
            {
                osRepeat = pszRepeat;
                oCol.nRepeat = atoi(pszRepeat);
            }
            // Then if COMPUTE_REPEAT=AT_FIRST_FEATURE_CREATION was specified
            // and we have a feature, then look at the number of items in the
            // field
            else if( bRepeatFromFirstFeature &&
                     poFeature->IsFieldSetAndNotNull(nFieldIdx) )
            {
                int nCount = 0;
                if( eType == OFTIntegerList )
                {
                    CPL_IGNORE_RET_VAL(poFeature->GetFieldAsIntegerList(nFieldIdx, &nCount));
                }
                else if( eType == OFTInteger64List )
                {
                    CPL_IGNORE_RET_VAL(poFeature->GetFieldAsInteger64List(nFieldIdx, &nCount));
                }
                else if( eType == OFTRealList )
                {
                    CPL_IGNORE_RET_VAL(poFeature->GetFieldAsDoubleList(nFieldIdx, &nCount));
                }
                else
                {
                    CPLAssert(false);
                }
                osRepeat.Printf("%d", nCount);
                oCol.nRepeat = nCount;
            }
            else
            {
                if( !osTFormFromMD.empty() && osTFormFromMD[0] >= '1' && osTFormFromMD[0] <= '9' )
                {
                    oCol.nRepeat = atoi(osTFormFromMD.c_str());
                    osRepeat.Printf("%d", oCol.nRepeat);
                }
                else
                {
                    oCol.nRepeat = 0;
                    oCol.typechar = 'P';
                }
            }
        }
        else if( pszRepeat )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "%s ignored on a non-List data type",
                    (CPLString("REPEAT_") + pszFieldName).c_str());
        }

        switch( eType )
        {
            case OFTIntegerList:
            case OFTInteger:
                if( eSubType == OFSTInt16 )
                {
                    oCol.typechar += 'I';
                    oCol.nTypeCode = TSHORT;
                }
                else
                {
                    oCol.typechar += 'J';
                    oCol.nTypeCode = TINT;
                }
                break;

            case OFTInteger64List:
            case OFTInteger64:
                oCol.typechar += 'K';
                oCol.nTypeCode = TLONGLONG;
                break;

            case OFTRealList:
            case OFTReal:
                if( eSubType == OFSTFloat32 )
                {
                    oCol.typechar += 'E';
                    oCol.nTypeCode = TFLOAT;
                }
                else
                {
                    oCol.typechar += 'D';
                    oCol.nTypeCode = TDOUBLE;
                }
                break;

            case OFTString:
                if( osTFormFromMD == "C" )
                {
                    oCol.typechar = "C";
                    oCol.nTypeCode = TCOMPLEX;
                }
                else if( osTFormFromMD == "M" )
                {
                    oCol.typechar = "M";
                    oCol.nTypeCode = TDBLCOMPLEX;
                }
                else
                {
                    if( oFieldDefn.GetWidth() == 0 )
                    {
                        oCol.typechar = "PA";
                    }
                    else
                    {
                        oCol.typechar = "A";
                        oCol.nRepeat = oFieldDefn.GetWidth();
                        osTForm = CPLSPrintf("%dA", oCol.nRepeat);
                    }
                    oCol.nTypeCode = TSTRING;
                }
                break;

            default:
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Unsupported field type: should not happen");
                break;
        }

        CPLString osTType(pszFieldName);
        if( osTForm.empty() )
        {
            if( (eType == OFTIntegerList || eType == OFTInteger64List ||
                eType == OFTRealList) && !osRepeat.empty() )
            {
                osTForm = osRepeat;
                osTForm += oCol.typechar;
            }
            else
            {
                osTForm = oCol.typechar;
            }
        }
        int status = 0;
        fits_insert_col(m_poDS->m_hFITS, oCol.iCol,
                        &osTType[0], &osTForm[0], &status);
        if( status != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "fits_insert_col() failed: %d", status);
        }

        // Set unit from metadata
        auto osUnit = oMapColNameToMetadata[pszFieldName]["TUNIT"];
        if( !osUnit.empty() )
        {
            CPLString osKey;
            osKey.Printf("TUNIT%d", oCol.iCol);
            fits_update_key_longstr(m_poDS->m_hFITS, &osKey[0],
                                    &osUnit[0], nullptr, &status);
        }

        m_aoColDescs.emplace_back(oCol);
    }

    m_anDeferredFieldsIndices.clear();
    CPLAssert( static_cast<int>(m_aoColDescs.size()) == m_poFeatureDefn->GetFieldCount() );
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr FITSLayer::CreateField( OGRFieldDefn *poField, int /* bApproxOK */ )
{
    if( !TestCapability(OLCCreateField) )
        return OGRERR_FAILURE;
    if( m_poFeatureDefn->GetFieldIndex(poField->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "A field with name %s already exists",
                 poField->GetNameRef());
        return OGRERR_FAILURE;
    }
    if( poField->GetType() == OFTStringList )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Unsupported field type");
        return OGRERR_FAILURE;
    }

    m_anDeferredFieldsIndices.emplace_back(m_poFeatureDefn->GetFieldCount());
    m_poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                              WriteCol                                */
/************************************************************************/

template<class T> static T Round(double dfValue)
{
    return static_cast<T>(floor(dfValue + 0.5));
}

template<> double Round<double>(double dfValue)
{
    return dfValue;
}

template<> float Round<float>(double dfValue)
{
    return static_cast<float>(dfValue);
}

template<typename T_FITS, typename T_GDAL, int TYPECODE,
         T_GDAL (OGRFeature::*GetFieldMethod)(int) const,
         const T_GDAL* (OGRFeature::*GetFieldListMethod)(int, int*) const> struct WriteCol
{
    static int Write(fitsfile* hFITS, const ColDesc& colDesc,
                     int iField, LONGLONG irow, const OGRFeature* poFeature)
    {
        int status = 0;
        int nRepeat = colDesc.nRepeat;
        const auto poFieldDefn = poFeature->GetFieldDefnRef(iField);
        const auto eOGRType = poFieldDefn->GetType();
        int nCount = 0;
        // cppcheck-suppress constStatement
        const T_GDAL* panList =
          ( eOGRType == OFTIntegerList ||
            eOGRType == OFTInteger64List ||
            eOGRType == OFTRealList ) ?
                (poFeature->*GetFieldListMethod)(iField, &nCount) : nullptr;
        if( panList )
        {
            nRepeat = nRepeat == 0 ? nCount : std::min(nRepeat, nCount);
            if( nCount > nRepeat )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %s of feature " CPL_FRMT_GIB " had %d "
                         "elements, but had to be truncated to %d",
                         poFieldDefn->GetNameRef(),
                         static_cast<GIntBig>(irow),
                         nCount,
                         nRepeat);
            }
        }
        else
        {
            nRepeat = 1;
        }

        if( nRepeat == 0 )
            return 0;

        if( colDesc.bHasNull && nRepeat == 1 &&
            poFeature->IsFieldNull(iField) )
        {
            T_FITS x = static_cast<T_FITS>(colDesc.nNullValue);
            fits_write_col(hFITS, TYPECODE,
                           colDesc.iCol, irow, 1, nRepeat, &x, &status);
        }
        else if( nRepeat == 1 )
        {
            const auto val =
                panList ? panList[0] : (poFeature->*GetFieldMethod)(iField);
            if( colDesc.dfScale != 1.0 || colDesc.dfOffset != 0.0 )
            {
                T_FITS x = Round<T_FITS>(
                                (val - colDesc.dfOffset) / colDesc.dfScale);
                fits_write_col(hFITS, TYPECODE,
                               colDesc.iCol, irow, 1, nRepeat, &x, &status);
            }
            else
            {
                T_FITS x = static_cast<T_FITS>(val);
                fits_write_col(hFITS, TYPECODE,
                                colDesc.iCol, irow, 1, nRepeat, &x, &status);
            }
        }
        else
        {
            CPLAssert(panList);

            std::vector<T_FITS> x;
            x.reserve(nRepeat);
            if( colDesc.dfScale != 1.0 || colDesc.dfOffset != 0.0 )
            {
                for( int i = 0; i < nRepeat; i++ )
                {
                    x.push_back(Round<T_FITS>(
                        (panList[i] - colDesc.dfOffset) / colDesc.dfScale));
                }
                fits_write_col(hFITS, TYPECODE,
                               colDesc.iCol, irow, 1, nRepeat, &x[0], &status);
            }
            else
            {
                for( int i = 0; i < nRepeat; i++ )
                {
                    x.push_back(static_cast<T_FITS>(panList[i]));
                }
                fits_write_col(hFITS, TYPECODE,
                               colDesc.iCol, irow, 1, nRepeat, &x[0], &status);
            }
        }
        return status;
    }
};

/************************************************************************/
/*                            WriteComplex                              */
/************************************************************************/

template<typename T, int TYPECODE> struct WriteComplex
{
    static int Write(fitsfile* hFITS, const ColDesc& colDesc,
                     int iField, LONGLONG irow, const OGRFeature* poFeature)
    {
        int status = 0;
        const auto poFieldDefn = poFeature->GetFieldDefnRef(iField);
        if( poFieldDefn->GetType() == OFTStringList )
        {
            auto papszStrings = poFeature->GetFieldAsStringList(iField);
            const int nCount = CSLCount(papszStrings);
            int nRepeat = colDesc.nRepeat;
            nRepeat = nRepeat == 0 ? nCount : std::min(nRepeat, nCount);
            if( nRepeat > nCount )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Field %s of feature " CPL_FRMT_GIB " had %d "
                        "elements, but had to be truncated to %d",
                        poFieldDefn->GetNameRef(),
                        static_cast<GIntBig>(irow),
                        nRepeat,
                        nCount);
            }
            std::vector<T> x(2 * nRepeat);
            for( int i = 0; i < nRepeat; i ++ )
            {
                double re = 0;
                double im = 0;
                CPLsscanf(papszStrings[i], "%lf + %lfj", &re, &im);
                x[2 * i] = static_cast<T>(re);
                x[2 * i + 1] = static_cast<T>(im);
            }
            fits_write_col(hFITS, TYPECODE, colDesc.iCol, irow,
                           1, nRepeat, &x[0], &status);
        }
        else
        {
            std::vector<T> x(2);
            double re = 0;
            double im = 0;
            CPLsscanf(poFeature->GetFieldAsString(iField),
                        "%lf + %lfj", &re, &im);
            x[0] = static_cast<T>(re);
            x[1] = static_cast<T>(im);
            fits_write_col(hFITS, TYPECODE, colDesc.iCol, irow,
                           1, 1, &x[0], &status);
        }
        return status;
    }
};

/************************************************************************/
/*                        SetOrCreateFeature()                          */
/************************************************************************/

bool FITSLayer::SetOrCreateFeature(const OGRFeature* poFeature, LONGLONG nRow)
{
    SetActiveHDU();

    const auto WriteField = [this, &poFeature, nRow](int iField)
    {
        const auto poFieldDefn = poFeature->GetFieldDefnRef(iField);
        const auto& colDesc = m_aoColDescs[iField];
        const char typechar = (colDesc.typechar[0] == 'P' ||
                               colDesc.typechar[0] == 'Q') ?
                                    colDesc.typechar[1] : colDesc.typechar[0];
        int nRepeat = colDesc.nRepeat;
        int status = 0;
        if( typechar == 'L' )
        {
            const auto ToLogical = [](int x) -> char { return x ? '1' : '0'; };

            if( poFieldDefn->GetType() == OFTIntegerList )
            {
                int nCount = 0;
                const int* panVals = poFeature->GetFieldAsIntegerList(iField, &nCount);
                nRepeat = nRepeat == 0 ? nCount : std::min(nRepeat, nCount);
                if( nRepeat > nCount )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Field %s of feature " CPL_FRMT_GIB " had %d "
                            "elements, but had to be truncated to %d",
                            poFieldDefn->GetNameRef(),
                            static_cast<GIntBig>(nRow),
                            nRepeat,
                            nCount);
                }
                std::vector<char> x(nRepeat);
                for( int i = 0; i < nRepeat; i++ )
                {
                    x[i] = ToLogical(panVals[i]);
                }
                fits_write_col(m_poDS->m_hFITS, TLOGICAL,
                               colDesc.iCol, nRow, 1, nRepeat, &x[0],
                               &status);
            }
            else
            {
                char x = ToLogical(poFeature->GetFieldAsInteger(iField));
                fits_write_col(m_poDS->m_hFITS, TLOGICAL,
                               colDesc.iCol, nRow, 1, nRepeat, &x, &status);
            }
        }
        else if( typechar == 'X' ) // bit array
        {
            char flag = poFeature->GetFieldAsInteger(iField) ? 0x80 : 0;
            fits_write_col_bit(m_poDS->m_hFITS, colDesc.iCol, nRow,
                               colDesc.iBit, 1, &flag, &status);
        }
        else if( typechar == 'B' )
        {
            if( colDesc.nTypeCode == TSBYTE )
            {
                status = WriteCol<signed char, int, TSBYTE,
                                  &OGRFeature::GetFieldAsInteger,
                                  &OGRFeature::GetFieldAsIntegerList>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
            else
            {
                status = WriteCol<GByte, int, TBYTE,
                                  &OGRFeature::GetFieldAsInteger,
                                  &OGRFeature::GetFieldAsIntegerList>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
        }
        else if( typechar == 'I' )
        {
            if( colDesc.nTypeCode == TUSHORT )
            {
                status = WriteCol<GUInt16, int, TUSHORT,
                                  &OGRFeature::GetFieldAsInteger,
                                  &OGRFeature::GetFieldAsIntegerList>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
            else
            {
                status = WriteCol<GInt16, int, TSHORT,
                                  &OGRFeature::GetFieldAsInteger,
                                  &OGRFeature::GetFieldAsIntegerList>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
        }
        else if( typechar == 'J' )
        {
            if( colDesc.nTypeCode == TUINT )
            {
                status = WriteCol<GUInt32, GIntBig, TUINT,
                                  &OGRFeature::GetFieldAsInteger64,
                                  &OGRFeature::GetFieldAsInteger64List>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
            else
            {
                status = WriteCol<GInt32, int, TINT,
                                  &OGRFeature::GetFieldAsInteger,
                                  &OGRFeature::GetFieldAsIntegerList>::Write(
                    m_poDS->m_hFITS, colDesc, iField,
                    nRow, poFeature);
            }
        }
        else if( typechar == 'K' )
        {
            status = WriteCol<GInt64, GIntBig, TLONGLONG,
                              &OGRFeature::GetFieldAsInteger64,
                              &OGRFeature::GetFieldAsInteger64List>::Write(
                m_poDS->m_hFITS, colDesc, iField,
                nRow, poFeature);
        }
        else if( typechar == 'A' ) // Character
        {
            if( poFieldDefn->GetType() == OFTStringList )
            {
                auto papszStrings = poFeature->GetFieldAsStringList(iField);
                const int nStringCount = CSLCount(papszStrings);
                const int nItems = std::min(colDesc.nItems, nStringCount);
                if( nItems > nStringCount )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Field %s of feature " CPL_FRMT_GIB " had %d "
                            "elements, but had to be truncated to %d",
                            poFieldDefn->GetNameRef(),
                            static_cast<GIntBig>(nRow),
                            nItems,
                            nStringCount);
                }
                fits_write_col_str(m_poDS->m_hFITS, colDesc.iCol, nRow,
                                   1, nItems, papszStrings, &status);
            }
            else
            {
                char* pszStr = const_cast<char*>(
                    poFeature->GetFieldAsString(iField));
                fits_write_col_str(m_poDS->m_hFITS, colDesc.iCol, nRow,
                                   1, 1, &pszStr, &status);
            }
        }
        else if( typechar == 'E' ) // IEEE754 32bit
        {
            status = WriteCol<float, double, TFLOAT,
                              &OGRFeature::GetFieldAsDouble,
                              &OGRFeature::GetFieldAsDoubleList>::Write(
                m_poDS->m_hFITS, colDesc, iField, nRow, poFeature);
        }
        else if( typechar == 'D' ) // IEEE754 64bit
        {
            status = WriteCol<double, double, TDOUBLE,
                              &OGRFeature::GetFieldAsDouble,
                              &OGRFeature::GetFieldAsDoubleList>::Write(
                m_poDS->m_hFITS, colDesc, iField, nRow, poFeature);
        }
        else if( typechar == 'C' ) // IEEE754 32bit complex
        {
            status = WriteComplex<float, TCOMPLEX>::Write(
                m_poDS->m_hFITS, colDesc, iField, nRow, poFeature);
        }
        else if( typechar == 'M' ) // IEEE754 64bit complex
        {
            status = WriteComplex<double, TDBLCOMPLEX>::Write(
                m_poDS->m_hFITS, colDesc, iField, nRow, poFeature);
        }
        else
        {
            CPLDebug("FITS", "Unhandled typechar %c", typechar);
        }
        if( status )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "fits_write_col() failed");
        }
        return status == 0;
    };

    bool bOK = true;
    const int nFieldCount = poFeature->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( !WriteField(iField) )
            bOK = false;
    }
    return bOK;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr FITSLayer::ICreateFeature(OGRFeature* poFeature)
{
    if( !TestCapability(OLCSequentialWrite) )
        return OGRERR_FAILURE;

    RunDeferredFieldCreation(poFeature);

    m_nRows ++;
    SetActiveHDU();
    const bool bOK = SetOrCreateFeature(poFeature, m_nRows);
    poFeature->SetFID(m_nRows);

    return bOK ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                           ISetFeature()                              */
/************************************************************************/

OGRErr FITSLayer::ISetFeature(OGRFeature* poFeature)
{
    if( !TestCapability(OLCRandomWrite) )
        return OGRERR_FAILURE;

    RunDeferredFieldCreation();

    const GIntBig nRow = poFeature->GetFID();
    if( nRow <= 0 || nRow > m_nRows )
        return OGRERR_NON_EXISTING_FEATURE;

    SetActiveHDU();
    const bool bOK = SetOrCreateFeature(poFeature, nRow);
    return bOK ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr FITSLayer::DeleteFeature(GIntBig nFID)
{
    if( !TestCapability(OLCDeleteFeature) )
        return OGRERR_FAILURE;

    if( nFID <= 0 || nFID > m_nRows )
        return OGRERR_NON_EXISTING_FEATURE;

    SetActiveHDU();

    int status = 0;
    fits_delete_rows(m_poDS->m_hFITS, nFID, 1, &status);
    m_nRows --;
    return status == 0 ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                          FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::FITSRasterBand( FITSDataset *poDSIn, int nBandIn ) :
  m_poFDS(poDSIn)
{
  poDS = poDSIn;
  nBand = nBandIn;
  eDataType = poDSIn->m_gdalDataType;
  nBlockXSize = poDSIn->nRasterXSize;
  nBlockYSize = 1;
}

/************************************************************************/
/*                          ~FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::~FITSRasterBand()
{
    FlushCache(true);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr FITSRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                  void* pImage ) {
  // A FITS block is one row (we assume BSQ formatted data)
  FITSDataset* dataset = m_poFDS;
  fitsfile* hFITS = dataset->m_hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows
  CPLAssert(nBlockXOff == 0);
  CPLAssert(nBlockYOff < nRasterYSize);

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1 at the bottom left...
  LONGLONG offset = static_cast<LONGLONG>(nBand - 1) * nRasterXSize * nRasterYSize +
    (static_cast<LONGLONG>(nRasterYSize - 1 - nBlockYOff) * nRasterXSize + 1);
  long nElements = nRasterXSize;

  // If we haven't written this block to the file yet, then attempting
  // to read causes an error, so in this case, just return zeros.
  if (!dataset->m_isExistingFile && offset > dataset->m_highestOffsetWritten) {
    memset(pImage, 0, nBlockXSize * nBlockYSize
           * GDALGetDataTypeSize(eDataType) / 8);
    return CE_None;
  }

  // Otherwise read in the image data
  fits_read_img(hFITS, dataset->m_fitsDataType, offset, nElements,
                nullptr, pImage, nullptr, &status);

  // Capture special case of non-zero status due to data range
  // overflow Standard GDAL policy is to silently truncate, which is
  // what CFITSIO does, in addition to returning NUM_OVERFLOW (412) as
  // the status.
  if (status == NUM_OVERFLOW)
    status = 0;

  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't read image data from FITS file (%d).", status);
    return CE_Failure;
  }

  return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/************************************************************************/

CPLErr FITSRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                    void* pImage )
{
  FITSDataset* dataset = m_poFDS;
  fitsfile* hFITS = dataset->m_hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1 at the bottom left...
  LONGLONG offset = static_cast<LONGLONG>(nBand - 1) * nRasterXSize * nRasterYSize +
    (static_cast<LONGLONG>(nRasterYSize - 1 - nBlockYOff) * nRasterXSize + 1);
  long nElements = nRasterXSize;
  fits_write_img(hFITS, dataset->m_fitsDataType, offset, nElements,
                 pImage, &status);

  // Capture special case of non-zero status due to data range
  // overflow Standard GDAL policy is to silently truncate, which is
  // what CFITSIO does, in addition to returning NUM_OVERFLOW (412) as
  // the status.
  if (status == NUM_OVERFLOW)
    status = 0;

  // Check for other errors
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Error writing image data to FITS file (%d).", status);
    return CE_Failure;
  }

  // When we write a block, update the offset counter that we've written
  if (offset > dataset->m_highestOffsetWritten)
    dataset->m_highestOffsetWritten = offset;

  return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             FITSDataset                             */
/* ==================================================================== */
/************************************************************************/

// Some useful utility functions

// Simple static function to determine if FITS header keyword should
// be saved in meta data.
static const char* const ignorableFITSHeaders[] = {
  "SIMPLE", "BITPIX", "NAXIS", "NAXIS1", "NAXIS2", "NAXIS3", "END",
  "XTENSION", "PCOUNT", "GCOUNT", "EXTEND", "CONTINUE",
  "COMMENT", "", "LONGSTRN", "BZERO", "BSCALE", "BLANK",
  "CHECKSUM", "DATASUM",
};
static bool isIgnorableFITSHeader(const char* name) {
  for (const char* keyword: ignorableFITSHeaders) {
    if (strcmp(name, keyword) == 0)
      return true;
  }
  return false;
}

/************************************************************************/
/*                            FITSDataset()                            */
/************************************************************************/

FITSDataset::FITSDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_adfGeoTransform[0] = 0;
    m_adfGeoTransform[1] = 1;
    m_adfGeoTransform[2] = 0;
    m_adfGeoTransform[3] = 0;
    m_adfGeoTransform[4] = 0;
    m_adfGeoTransform[5] = 1;
}

/************************************************************************/
/*                           ~FITSDataset()                            */
/************************************************************************/

FITSDataset::~FITSDataset() {

  int status = 0;
  if( m_hFITS )
  {
    m_apoLayers.clear();

    if(m_hduNum > 0 && eAccess == GA_Update)
    {
      // Only do this if we've successfully opened the file and update
      // capability.  Write any meta data to the file that's compatible with
      // FITS.
      fits_movabs_hdu(m_hFITS, m_hduNum, nullptr, &status);
      fits_write_key_longwarn(m_hFITS, &status);
      if (status) {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Couldn't move to HDU %d in FITS file %s (%d).\n",
                 m_hduNum, GetDescription(), status);
      }
      char** metaData = FITSDataset::GetMetadata();
      int count = CSLCount(metaData);
      for (int i = 0; i < count; ++i) {
        const char* field = CSLGetField(metaData, i);
        if (strlen(field) == 0)
            continue;
        else {
            char* key = nullptr;
            const char* value = CPLParseNameValue(field, &key);
            // FITS keys must be less than 8 chars
            if (key != nullptr && strlen(key) <= 8 && !isIgnorableFITSHeader(key))
            {
                // Although FITS provides support for different value
                // types, the GDAL Metadata mechanism works only with
                // string values. Prior to about 2003-05-02, this driver
                // would attempt to guess the value type from the metadata
                // value string amd then would use the appropriate
                // type-specific FITS keyword update routine. This was
                // found to be troublesome (e.g. a numeric version string
                // with leading zeros would be interpreted as a number
                // and might get those leading zeros stripped), and so now
                // the driver writes every value as a string. In practice
                // this is not a problem since most FITS reading routines
                // will convert from strings to numbers automatically, but
                // if you want finer control, use the underlying FITS
                // handle. Note: to avoid a compiler warning we copy the
                // const value string to a non const one.
                char* valueCpy = CPLStrdup(value);
                fits_update_key_longstr(m_hFITS, key, valueCpy, nullptr, &status);
                CPLFree(valueCpy);

                // Check for errors.
                if (status)
                {
                    // Throw a warning with CFITSIO error status, then ignore status
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Couldn't update key %s in FITS file %s (%d).",
                             key, GetDescription(), status);
                    status = 0;
                    return;
                }
            }
            // Must free up key
            CPLFree(key);
        }
      }

      // Writing nodata value
      if (m_gdalDataType != GDT_Float32 && m_gdalDataType != GDT_Float64) {
        fits_update_key( m_hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BLANK in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
      }

      // Writing Scale and offset if defined
      int pbSuccess;
      GDALRasterBand* poSrcBand = GDALPamDataset::GetRasterBand(1);
      double dfScale = poSrcBand->GetScale(&pbSuccess);
      double dfOffset = poSrcBand->GetOffset(&pbSuccess);
      if (m_bMetadataChanged) {
        fits_update_key( m_hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BSCALE in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BZERO in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
      }

      // Copy georeferencing info to PAM if the profile is not FITS
      GDALPamDataset::SetSpatialRef(GDALPamDataset::GetSpatialRef());

      // Write geographic info
      if (m_bFITSInfoChanged) {
        WriteFITSInfo();
      }

      // Make sure we flush the raster cache before we close the file!
      FlushCache(true);
    }

    // Close the FITS handle
    fits_close_file(m_hFITS, &status);
    if( status != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_close_file() failed with %d", status);
    }
  }
}


/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool FITSDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    if( m_hduNum == 0 )
        return false;
    int status = 0;
    if( fits_is_compressed_image( m_hFITS, &status) )
        return false;
    GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    if( eDT == GDT_UInt16 || eDT == GDT_UInt32 )
        return false; // are supported as native signed with offset

    sLayout.osRawFilename = GetDescription();
    static_assert( sizeof(OFF_T) == 8, "OFF_T should be 64 bits !" );
    OFF_T headerstart = 0;
    OFF_T datastart = 0;
    OFF_T dataend = 0;
    fits_get_hduoff(m_hFITS, &headerstart, &datastart, &dataend, &status);
    if( nBands > 1 )
        sLayout.eInterleaving = RawBinaryLayout::Interleaving::BSQ;
    sLayout.eDataType = eDT;
    sLayout.bLittleEndianOrder = false;
    sLayout.nImageOffset = static_cast<GIntBig>(datastart);
    sLayout.nPixelOffset = GDALGetDataTypeSizeBytes(eDT);
    sLayout.nLineOffset = sLayout.nPixelOffset * nRasterXSize;
    sLayout.nBandOffset = sLayout.nLineOffset * nRasterYSize;
    return true;
}

/************************************************************************/
/*                           Init()                                     */
/************************************************************************/

CPLErr FITSDataset::Init(fitsfile* hFITS, bool isExistingFile, int hduNum) {

    m_hFITS = hFITS;
    m_isExistingFile = isExistingFile;

    int status = 0;
    double offset;

    int hduType = 0;
    fits_movabs_hdu(hFITS, hduNum, &hduType, &status);
    if (status)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't move to HDU %d in FITS file %s (%d).",
                hduNum, GetDescription(), status);
        return CE_Failure;
    }

    if( hduType != IMAGE_HDU )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "HDU %d is not an image.", hduNum);
        return CE_Failure;
    }

    // Get the image info for this dataset (note that all bands in a FITS dataset
    // have the same type)
    int bitpix = 0;
    int naxis = 0;
    const int maxdim = 3;
    long naxes[maxdim] = {0,0,0};
    fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
    if (status) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't determine image parameters of FITS file %s (%d)",
                GetDescription(), status);
        return CE_Failure;
    }

    m_hduNum = hduNum;

    fits_read_key(hFITS, TDOUBLE, "BZERO", &offset, nullptr, &status);
    if( status )
    {
        // BZERO is not mandatory offset defaulted to 0 if BZERO is missing
        status = 0;
        offset = 0.;
    }

    fits_read_key(hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
    m_bNoDataSet = !status;
    status = 0;

    // Determine data type and nodata value if BLANK keyword is absent
    if (bitpix == BYTE_IMG) {
        m_gdalDataType = GDT_Byte;
        m_fitsDataType = TBYTE;
    }
    else if (bitpix == SHORT_IMG) {
        if (offset == 32768.)
        {
            m_gdalDataType = GDT_UInt16;
            m_fitsDataType = TUSHORT;
        }
        else {
            m_gdalDataType = GDT_Int16;
            m_fitsDataType = TSHORT;
        }
    }
    else if (bitpix == LONG_IMG) {
        if (offset == 2147483648.)
        {
            m_gdalDataType = GDT_UInt32;
            m_fitsDataType = TUINT;
        }
        else {
            m_gdalDataType = GDT_Int32;
            m_fitsDataType = TINT;
        }
    }
    else if (bitpix == FLOAT_IMG) {
        m_gdalDataType = GDT_Float32;
        m_fitsDataType = TFLOAT;
    }
    else if (bitpix == DOUBLE_IMG) {
        m_gdalDataType = GDT_Float64;
        m_fitsDataType = TDOUBLE;
    }
    else {
        CPLError(CE_Failure, CPLE_AppDefined,
                "FITS file %s has unknown data type: %d.", GetDescription(),
                bitpix);
        return CE_Failure;
    }

    // Determine image dimensions - we assume BSQ ordering
    if (naxis == 2) {
        nRasterXSize = static_cast<int>(naxes[0]);
        nRasterYSize = static_cast<int>(naxes[1]);
        nBands = 1;
    }
    else if (naxis == 3) {
        nRasterXSize = static_cast<int>(naxes[0]);
        nRasterYSize = static_cast<int>(naxes[1]);
        nBands = static_cast<int>(naxes[2]);
    }
    else {
        CPLError(CE_Failure, CPLE_AppDefined,
                "FITS file %s does not have 2 or 3 dimensions.",
                GetDescription());
        return CE_Failure;
    }

    // Create the bands
    for (int i = 0; i < nBands; ++i)
        SetBand(i+1, new FITSRasterBand(this, i+1));

    return CE_None;
}

/************************************************************************/
/*                         LoadMetadata()                               */
/************************************************************************/

void FITSDataset::LoadMetadata(GDALMajorObject* poTarget)
{
    // Read header information from file and use it to set metadata
    // This process understands the CONTINUE standard for long strings.
    // We don't bother to capture header names that duplicate information
    // already captured elsewhere (e.g. image dimensions and type)
    int keyNum;
    char key[100];
    char value[100];
    CPLStringList aosMD;

    int nKeys = 0;
    int nMoreKeys = 0;
    int status = 0;
    fits_get_hdrspace(m_hFITS, &nKeys, &nMoreKeys, &status);
    for(keyNum = 1; keyNum <= nKeys; keyNum++)
    {
        fits_read_keyn(m_hFITS, keyNum, key, value, nullptr, &status);
        if (status) {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Error while reading key %d from FITS file %s (%d)",
                    keyNum, GetDescription(), status);
            return;
        }
        if (strcmp(key, "END") == 0) {
            // We should not get here in principle since the END
            // keyword shouldn't be counted in nKeys, but who knows.
            break;
        }
        else if (isIgnorableFITSHeader(key)) {
        // Ignore it
        }
        else {   // Going to store something, but check for long strings etc
            // Strip off leading and trailing quote if present
            char* newValue = value;
            if (value[0] == '\'' && value[strlen(value) - 1] == '\'')
            {
                newValue = value + 1;
                value[strlen(value) - 1] = '\0';
            }
            // Check for long string
            if (strrchr(newValue, '&') == newValue + strlen(newValue) - 1)
            {
                // Value string ends in "&", so use long string conventions
                char* longString = nullptr;
                fits_read_key_longstr(m_hFITS, key, &longString, nullptr, &status);
                // Note that read_key_longstr already strips quotes
                if( status )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Error while reading long string for key %s from "
                            "FITS file %s (%d)", key, GetDescription(), status);
                    return;
                }
                poTarget->SetMetadataItem(key, longString);
                free(longString);
            }
            else
            {  // Normal keyword
                poTarget->SetMetadataItem(key, newValue);
            }
        }
    }
}

/************************************************************************/
/*                           Identify()                                 */
/************************************************************************/

int FITSDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH(poOpenInfo->pszFilename, "FITS:") )
        return true;

    const char* fitsID = "SIMPLE  =                    T";  // Spaces important!
    const size_t fitsIDLen = strlen(fitsID);  // Should be 30 chars long

    if (static_cast<size_t>(poOpenInfo->nHeaderBytes) < fitsIDLen)
        return false;
    if (memcmp(poOpenInfo->pabyHeader, fitsID, fitsIDLen) != 0)
        return false;
    return true;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **FITSDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
    {
        return m_aosSubdatasets.List();
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* FITSDataset::GetLayer(int idx)
{
    if( idx < 0 || idx >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[idx].get();
}


/************************************************************************/
/*                            ICreateLayer()                            */
/************************************************************************/

OGRLayer* FITSDataset::ICreateLayer(const char *pszName,
                         OGRSpatialReference* /* poSRS */,
                         OGRwkbGeometryType eGType,
                         char ** papszOptions )
{
    if( !TestCapability(ODsCCreateLayer) )
        return nullptr;
    if( eGType != wkbNone )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Spatial tables not supported");
        return nullptr;
    }

    int status = 0;
    int numHDUs = 0;
    fits_get_num_hdus(m_hFITS, &numHDUs, &status);
    if( status != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fits_get_num_hdus() failed: %d", status);
        return nullptr;
    }

    fits_create_tbl(m_hFITS, BINARY_TBL,
                    0, // number of initial rows
                    0, // nfields,
                    nullptr, // ttype,
                    nullptr, // tform
                    nullptr, // tunits
                    pszName,
                    &status);
    if( status != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create layer");
        return nullptr;
    }

    // If calling fits_get_num_hdus() here on a freshly new created file,
    // it reports only one HDU, missing the initial dummy HDU
    if( numHDUs == 0 )
    {
        numHDUs = 2;
    }
    else
    {
        numHDUs++;
    }

    auto poLayer = new FITSLayer(this, numHDUs, pszName);
    poLayer->SetCreationOptions(papszOptions);
    m_apoLayers.emplace_back(std::unique_ptr<FITSLayer>(poLayer));
    return m_apoLayers.back().get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int FITSDataset::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return eAccess == GA_Update;
    return false;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* FITSDataset::Open(GDALOpenInfo* poOpenInfo) {

    if( !Identify(poOpenInfo) )
        return nullptr;

    CPLString osFilename(poOpenInfo->pszFilename);
    int iSelectedHDU = 0;
    if( STARTS_WITH(poOpenInfo->pszFilename, "FITS:") )
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                            CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES));
        if( aosTokens.size() != 3 )
        {
            return nullptr;
        }
        osFilename = aosTokens[1];
        iSelectedHDU = atoi(aosTokens[2]);
        if( iSelectedHDU <= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid HDU number");
            return nullptr;
        }
    }

    // Get access mode and attempt to open the file
    int status = 0;
    fitsfile* hFITS = nullptr;
    if (poOpenInfo->eAccess == GA_ReadOnly)
        fits_open_file(&hFITS, osFilename.c_str(), READONLY, &status);
    else
        fits_open_file(&hFITS, osFilename.c_str(), READWRITE, &status);
    if (status) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Error while opening FITS file %s (%d).\n",
                osFilename.c_str(), status);
        fits_close_file(hFITS, &status);
        return nullptr;
    }
    // Create a FITSDataset object
    auto dataset = cpl::make_unique<FITSDataset>();
    dataset->m_isExistingFile = true;
    dataset->m_hFITS = hFITS;
    dataset->eAccess = poOpenInfo->eAccess;
    dataset->SetPhysicalFilename(osFilename);

/* -------------------------------------------------------------------- */
/*      Iterate over HDUs                                               */
/* -------------------------------------------------------------------- */
    bool firstHDUIsDummy = false;
    int firstValidHDU = 0;
    CPLStringList aosSubdatasets;
    bool hasVector = false;
    if( iSelectedHDU == 0 )
    {
        int numHDUs = 0;
        fits_get_num_hdus(hFITS, &numHDUs, &status);
        if( numHDUs <= 0 )
        {
            return nullptr;
        }

        for( int iHDU = 1; iHDU <= numHDUs; iHDU++ )
        {
            int hduType = 0;
            fits_movabs_hdu(hFITS, iHDU, &hduType, &status);
            if (status)
            {
                continue;
            }

            char szExtname[81] = { 0 };
            fits_read_key(hFITS, TSTRING, "EXTNAME", szExtname, nullptr, &status);
            status = 0;
            int nExtVer = 0;
            fits_read_key(hFITS, TINT, "EXTVER", &nExtVer, nullptr, &status);
            status = 0;
            CPLString osExtname(szExtname);
            if( nExtVer > 0 )
                osExtname += CPLSPrintf(" %d", nExtVer);

            if( hduType == BINARY_TBL )
            {
                hasVector = true;
                if( (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 )
                {
                    dataset->m_apoLayers.push_back(std::unique_ptr<FITSLayer>(
                        new FITSLayer(dataset.get(), iHDU, osExtname.c_str())));
                }
            }

            if( hduType != IMAGE_HDU )
            {
                continue;
            }

            int bitpix = 0;
            int naxis = 0;
            const int maxdim = 3;
            long naxes[maxdim] = {0,0,0};
            fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
            if (status)
            {
                continue;
            }

            if( naxis != 2 && naxis != 3 )
            {
                if( naxis == 0 && iHDU == 1 )
                {
                    firstHDUIsDummy = true;
                }
                continue;
            }

            if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 )
            {
                const int nIdx = aosSubdatasets.size() / 2 + 1;
                aosSubdatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                    CPLSPrintf("FITS:\"%s\":%d", poOpenInfo->pszFilename, iHDU));
                CPLString osDesc(CPLSPrintf("HDU %d (%dx%d, %d band%s)", iHDU,
                            static_cast<int>(naxes[0]),
                            static_cast<int>(naxes[1]),
                            naxis == 3 ? static_cast<int>(naxes[2]) : 1,
                            (naxis == 3 && naxes[2] > 1) ? "s" : ""));
                if( !osExtname.empty() )
                {
                    osDesc += ", ";
                    osDesc += osExtname;
                }
                aosSubdatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nIdx),
                    osDesc);
            }

            if( firstValidHDU == 0 )
            {
                firstValidHDU = iHDU;
            }
        }
        if( aosSubdatasets.size() == 2 )
        {
            aosSubdatasets.Clear();
        }
    }
    else
    {
        if( iSelectedHDU != 1 )
        {
            int hduType = 0;
            fits_movabs_hdu(hFITS, 1, &hduType, &status);
            if( status == 0 )
            {
                int bitpix = 0;
                int naxis = 0;
                const int maxdim = 3;
                long naxes[maxdim] = {0,0,0};
                fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
                if( status == 0 && naxis == 0 )
                {
                    firstHDUIsDummy = true;
                }
            }
            status = 0;
        }
        firstValidHDU = iSelectedHDU;
    }

    const bool hasRaster = firstValidHDU > 0;
    const bool hasRasterAndIsAllowed = hasRaster &&
        (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0;

    if( !hasRasterAndIsAllowed &&
        (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
    {
        if( hasVector )
        {
            std::string osPath;
            osPath.resize(1024);
            if( CPLGetExecPath(&osPath[0], static_cast<int>(osPath.size())) )
            {
                osPath = CPLGetBasename(osPath.c_str());
            }
            if( osPath == "gdalinfo" )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "This FITS dataset does not contain any image, but "
                         "contains binary table(s) that could be opened "
                         "in vector mode with ogrinfo.");
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "This FITS dataset does not contain any image, but "
                         "contains binary table(s) that could be opened "
                         "in vector mode.");
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find HDU of image type with 2 or 3 axes.");
        }
        return nullptr;
    }

    if( dataset->m_apoLayers.empty() &&
        (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 )
    {
        if( hasRaster )
        {
            std::string osPath;
            osPath.resize(1024);
            if( CPLGetExecPath(&osPath[0], static_cast<int>(osPath.size())) )
            {
                osPath = CPLGetBasename(osPath.c_str());
            }
            if( osPath == "ogrinfo" )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "This FITS dataset does not contain any binary "
                         "table, but contains image(s) that could be opened "
                         "in raster mode with gdalinfo.");
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "This FITS dataset does not contain any binary "
                         "table, but contains image(s) that could be opened "
                         "in raster mode.");
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find binary table(s).");
        }
        return nullptr;
    }

    dataset->m_aosSubdatasets = aosSubdatasets;

    // Set up the description and initialize the dataset
    dataset->SetDescription(poOpenInfo->pszFilename);
    if( hasRasterAndIsAllowed )
    {
        if( aosSubdatasets.size() > 2 )
        {
            firstValidHDU = 0;
            int hduType = 0;
            fits_movabs_hdu(hFITS, 1, &hduType, &status);
        }
        else
        {
            if( firstValidHDU != 0 &&
                dataset->Init(hFITS, true, firstValidHDU) != CE_None) {
                return nullptr;
            }
        }
    }

    // If the first HDU is a dummy one, load its metadata first, and then
    // add/override it by the one of the image HDU
    if( firstHDUIsDummy && firstValidHDU > 1 )
    {
        int hduType = 0;
        status = 0;
        fits_movabs_hdu(hFITS, 1, &hduType, &status);
        if( status == 0 )
        {
            dataset->LoadMetadata(dataset.get());
        }
        status = 0;
        fits_movabs_hdu(hFITS, firstValidHDU, &hduType, &status);
        if( status ) {
            return nullptr;
        }
    }
    if( hasRasterAndIsAllowed )
    {
        dataset->LoadMetadata(dataset.get());
        dataset->LoadFITSInfo();
    }

/* -------------------------------------------------------------------- */
/*      Initialize any information.                                     */
/* -------------------------------------------------------------------- */
    dataset->SetDescription( poOpenInfo->pszFilename );
    dataset->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    dataset->oOvManager.Initialize( dataset.get(), poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return dataset.release();
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new FITS file.                                         */
/************************************************************************/

GDALDataset *FITSDataset::Create( const char* pszFilename,
                                  int nXSize, int nYSize,
                                  int nBands, GDALDataType eType,
                                  CPL_UNUSED char** papszParamList )
{
  int status = 0;

  if( nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown )
  {
      // Create the file - to force creation, we prepend the name with '!'
      CPLString extFilename("!");
      extFilename += pszFilename;
      fitsfile* hFITS = nullptr;
      fits_create_file(&hFITS, extFilename, &status);
      if (status) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't create FITS file %s (%d).\n", pszFilename, status);
        return nullptr;
      }

      // Likely vector creation mode
      FITSDataset* dataset = new FITSDataset();
      dataset->m_hFITS = hFITS;
      dataset->eAccess = GA_Update;
      dataset->SetDescription(pszFilename);
      return dataset;
  }

  // No creation options are defined. The BSCALE/BZERO options were
  // removed on 2002-07-02 by Simon Perkins because they introduced
  // excessive complications and didn't really fit into the GDAL
  // paradigm.
  // 2018 - BZERO BSCALE keywords are now set using SetScale() and
  // SetOffset() functions

  if( nXSize < 1 || nYSize < 1 || nBands < 1 )  {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Attempt to create %dx%dx%d raster FITS file, but width, height and bands"
            " must be positive.",
            nXSize, nYSize, nBands );

        return nullptr;
  }

  // Determine FITS type of image
  int bitpix;
  if (eType == GDT_Byte) {
    bitpix = BYTE_IMG;
  } else if (eType == GDT_UInt16) {
    bitpix = USHORT_IMG;
  } else if (eType == GDT_Int16) {
    bitpix = SHORT_IMG;
  } else if (eType == GDT_UInt32) {
    bitpix = ULONG_IMG;
  } else if (eType == GDT_Int32) {
    bitpix = LONG_IMG;
  } else if (eType == GDT_Float32)
    bitpix = FLOAT_IMG;
  else if (eType == GDT_Float64)
    bitpix = DOUBLE_IMG;
  else {
    CPLError(CE_Failure, CPLE_AppDefined,
             "GDALDataType (%d) unsupported for FITS", eType);
    return nullptr;
  }

  // Create the file - to force creation, we prepend the name with '!'
  CPLString extFilename("!");
  extFilename += pszFilename;
  fitsfile* hFITS = nullptr;
  fits_create_file(&hFITS, extFilename, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't create FITS file %s (%d).\n", pszFilename, status);
    return nullptr;
  }

  // Now create an image of appropriate size and type
  long naxes[3] = {nXSize, nYSize, nBands};
  int naxis = (nBands == 1) ? 2 : 3;
  fits_create_img(hFITS, bitpix, naxis, naxes, &status);

  // Check the status
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't create image within FITS file %s (%d).",
             pszFilename, status);
    fits_close_file(hFITS, &status);
    return nullptr;
  }

  FITSDataset* dataset = new FITSDataset();
  dataset->nRasterXSize = nXSize;
  dataset->nRasterYSize = nYSize;
  dataset->eAccess = GA_Update;
  dataset->SetDescription(pszFilename);

  // Init recalculates a lot of stuff we already know, but...
  if (dataset->Init(hFITS, false, 1) != CE_None) {
    delete dataset;
    return nullptr;
  }
  else {
    return dataset;
  }
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

CPLErr FITSDataset::Delete( const char * pszFilename )
{
    return VSIUnlink( pszFilename) == 0 ? CE_None : CE_Failure;
}

/************************************************************************/
/*                          WriteFITSInfo()                          */
/************************************************************************/

void FITSDataset::WriteFITSInfo()

{
  int status = 0;

  const double PI = std::atan(1.0)*4;
  const double DEG2RAD = PI / 180.;

  double falseEast = 0;
  double falseNorth = 0;

  double cfactor, mres, mapres, UpperLeftCornerX, UpperLeftCornerY;
  double crpix1, crpix2;

/* -------------------------------------------------------------------- */
/*      Write out projection definition.                                */
/* -------------------------------------------------------------------- */
    const bool bHasProjection = !m_oSRS.IsEmpty();
    if( bHasProjection )
    {

        // Set according to coordinate system (thanks to Trent Hare - USGS)

        std::string object, ctype1, ctype2;

        const char* target = m_oSRS.GetAttrValue("DATUM",0);
        if ( target ) {
            if ( strstr(target, "Moon") ) {
              object.assign("Moon");
              ctype1.assign("SE");
              ctype2.assign("SE");
            } else if ( strstr(target, "Mercury") ) {
              object.assign("Mercury");
              ctype1.assign("ME");
              ctype2.assign("ME");
            } else if ( strstr(target, "Venus") ) {
              object.assign("Venus");
              ctype1.assign("VE");
              ctype2.assign("VE");
            } else if ( strstr(target, "Mars") ) {
              object.assign("Mars");
              ctype1.assign("MA");
              ctype2.assign("MA");
            } else if ( strstr(target, "Jupiter") ) {
              object.assign("Jupiter");
              ctype1.assign("JU");
              ctype2.assign("JU");
            } else if ( strstr(target, "Saturn") ) {
              object.assign("Saturn");
              ctype1.assign("SA");
              ctype2.assign("SA");
            } else if ( strstr(target, "Uranus") ) {
              object.assign("Uranus");
              ctype1.assign("UR");
              ctype2.assign("UR");
            } else if ( strstr(target, "Neptune") ) {
              object.assign("Neptune");
              ctype1.assign("NE");
              ctype2.assign("NE");
            } else {
              object.assign("Earth");
              ctype1.assign("EA");
              ctype2.assign("EA");
            }

            fits_update_key( m_hFITS, TSTRING, "OBJECT",
                             const_cast<void*>(static_cast<const void*>(object.c_str())),
                             nullptr, &status);
        }

        double aradius = m_oSRS.GetSemiMajor();
        double bradius = aradius;
        double cradius = m_oSRS.GetSemiMinor();

        cfactor = aradius * DEG2RAD;

        fits_update_key( m_hFITS, TDOUBLE, "A_RADIUS", &aradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key A_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "B_RADIUS", &bradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key B_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "C_RADIUS", &cradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key C_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }

        const char* unit = m_oSRS.GetAttrValue("UNIT",0);

        ctype1.append("LN-");
        ctype2.append("LT-");

        // strcat(ctype1a, "PX-");
        // strcat(ctype2a, "PY-");

        std::string fitsproj;
        const char* projection = m_oSRS.GetAttrValue("PROJECTION",0);
        double centlon = 0, centlat = 0;

        if (projection) {
            if ( strstr(projection, "Sinusoidal") ) {
              fitsproj.assign("SFL");
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Equirectangular") ) {
              fitsproj.assign("CAR");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Orthographic") ) {
              fitsproj.assign("SIN");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Mercator_1SP") || strstr(projection, "Mercator") ) {
              fitsproj.assign("MER");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Polar_Stereographic") || strstr(projection, "Stereographic_South_Pole") || strstr(projection, "Stereographic_North_Pole") ) {
              fitsproj.assign("STG");
              centlat = m_oSRS.GetProjParm("latitude_of_origin", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            }

/*
                #Transverse Mercator is supported in FITS via specific MER parameters.
                # need some more testing...
                #if EQUAL(mapProjection,"Transverse_Mercator"):
                #    mapProjection = "MER"
                #    centLat = hSRS.GetProjParm('standard_parallel_1')
                #    centLon = hSRS.GetProjParm('central_meridian')
                #    TMscale = hSRS.GetProjParm('scale_factor')
                #    #Need to research when TM actually applies false values
                #    #but planetary is almost always 0.0
                #    falseEast =  hSRS.GetProjParm('false_easting')
                #    falseNorth =  hSRS.GetProjParm('false_northing')
*/

            ctype1.append(fitsproj);
            ctype2.append(fitsproj);

            fits_update_key( m_hFITS, TSTRING, "CTYPE1",
                             const_cast<void*>(
                                 static_cast<const void*>(ctype1.c_str())),
                             nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CTYPE1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TSTRING, "CTYPE2",
                             const_cast<void*>(
                                 static_cast<const void*>(ctype2.c_str())),
                             nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CTYPE2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }
        }


        UpperLeftCornerX = m_adfGeoTransform[0] - falseEast;
        UpperLeftCornerY = m_adfGeoTransform[3] - falseNorth;

        if ( centlon > 180. ) {
          centlon = centlon - 180.;
        }
        if ( strstr(unit, "metre") ) {
          // convert degrees/pixel to m/pixel
          mapres = 1. / m_adfGeoTransform[1] ; // mapres is pixel/meters
          mres = m_adfGeoTransform[1] / cfactor ; // mres is deg/pixel
          crpix1 = - (UpperLeftCornerX * mapres) + centlon / mres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mapres) + 0.5; // - (centlat / mres);
        } else if ( strstr(unit, "degree") ) {
          //convert m/pixel to pixel/degree
          mapres = 1. / m_adfGeoTransform[1] / cfactor; // mapres is pixel/deg
          mres = m_adfGeoTransform[1] ; // mres is meters/pixel
          crpix1 = - (UpperLeftCornerX * mres) + centlon / mapres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mres) + 0.5; // - (centlat / mapres);
        }

        /// Write WCS CRPIXia CRVALia CTYPEia here

        fits_update_key( m_hFITS, TDOUBLE, "CRVAL1", &centlon, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRVAL2", &centlat, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL2 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRPIX1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRPIX2 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }

/* -------------------------------------------------------------------- */
/*      Write geotransform if valid.                                    */
/* -------------------------------------------------------------------- */
        if( m_bGeoTransformValid )
        {

/* -------------------------------------------------------------------- */
/*      Write the transform.                                            */
/* -------------------------------------------------------------------- */

            /// Write WCS CDELTia and PCi_ja here

            double cd[4];
            cd[0] = m_adfGeoTransform[1] / cfactor;
            cd[1] = m_adfGeoTransform[2] / cfactor;
            cd[2] = m_adfGeoTransform[4] / cfactor;
            cd[3] = m_adfGeoTransform[5] / cfactor;

            double pc[4];
            pc[0] = 1.;
            pc[1] = cd[1] / cd[0];
            pc[2] = cd[2] / cd[3];
            pc[3] = - 1.;

            fits_update_key( m_hFITS, TDOUBLE, "CDELT1", &cd[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "CDELT2", &cd[3], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC2_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC2_2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }
        }
    }
}


/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference* FITSDataset::GetSpatialRef() const

{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr FITSDataset::SetSpatialRef( const OGRSpatialReference * poSRS )

{
    if( poSRS == nullptr || poSRS->IsEmpty() )
    {
        m_oSRS.Clear();
    }
    else
    {
        m_oSRS = *poSRS;
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    m_bFITSInfoChanged = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );

    if( !m_bGeoTransformValid )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::SetGeoTransform( double * padfTransform )

{
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double)*6 );
    m_bGeoTransformValid = true;

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double FITSRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetOffset( double dfNewValue )

{
    if( !m_bHaveOffsetScale || dfNewValue != m_dfOffset )
        m_poFDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double FITSRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetScale( double dfNewValue )

{
    if( !m_bHaveOffsetScale || dfNewValue != m_dfScale )
        m_poFDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double FITSRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_dfNoDataValue;
    }

    if( m_poFDS->m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_poFDS->m_dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::SetNoDataValue( double dfNoData )

{
    if( m_poFDS->m_bNoDataSet && m_poFDS->m_dfNoDataValue == dfNoData )
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = dfNoData;
        return CE_None;
    }

    m_poFDS->m_bNoDataSet = true;
    m_poFDS->m_dfNoDataValue = dfNoData;

    m_poFDS->m_bNoDataChanged = true;

    m_bNoDataSet = true;
    m_dfNoDataValue = dfNoData;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::DeleteNoDataValue()

{
    if( !m_poFDS->m_bNoDataSet )
        return CE_None;

    m_poFDS->m_bNoDataSet = false;
    m_poFDS->m_dfNoDataValue = -9999.0;

    m_poFDS->m_bNoDataChanged = true;

    m_bNoDataSet = false;
    m_dfNoDataValue = -9999.0;
    return CE_None;
}

/************************************************************************/
/*                         LoadGeoreferencing()                         */
/************************************************************************/

void FITSDataset::LoadGeoreferencing()
{
    int status = 0;
    double crpix1, crpix2, crval1, crval2, cdelt1, cdelt2, pc[4], cd[4];
    double aRadius, cRadius, invFlattening = 0.0;
    double falseEast = 0.0, falseNorth = 0.0, scale = 1.0;
    char target[81], ctype[81];
    std::string GeogName, DatumName, projName;

    const double PI = std::atan(1.0)*4;
    const double DEG2RAD = PI / 180.;

/* -------------------------------------------------------------------- */
/*      Get the transform from the FITS file.                           */
/* -------------------------------------------------------------------- */

    fits_read_key(m_hFITS, TSTRING, "OBJECT", target, nullptr, &status);
    if( status )
    {
        strncpy(target, "Undefined", 10);
        CPLDebug("FITS", "OBJECT keyword is missing");
        status = 0;
    }

    GeogName.assign("GCS_");
    GeogName.append(target);
    DatumName.assign("D_");
    DatumName.append(target);

    fits_read_key(m_hFITS, TDOUBLE, "A_RADIUS", &aRadius, nullptr, &status);
    if( status )
    {
        CPLDebug("FITS",
            "No Radii keyword available, metadata will not contain DATUM information.");
        return;
    } else {
        fits_read_key(m_hFITS, TDOUBLE, "C_RADIUS", &cRadius, nullptr, &status);
        if( status )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                "No polar radius keyword available, setting C_RADIUS = A_RADIUS");
            cRadius = aRadius;
            status = 0;
        }
        if( aRadius != cRadius )
        {
            invFlattening = aRadius / ( aRadius - cRadius );
        }
    }

    /* Waiting for linear keywords standardization only deg ctype are used */
    /* Check if WCS are there */
    fits_read_key(m_hFITS, TSTRING, "CTYPE1", ctype, nullptr, &status);
    if ( !status ) {
        /* Check if angular WCS are there */
        if ( strstr(ctype, "LN") )
        {
            /* Reading reference points */
            fits_read_key(m_hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRVAL1", &crval1, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRVAL2", &crval2, nullptr, &status);
            if( status )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                     "No CRPIX / CRVAL keyword available, the raster cannot be georeferenced.");
                status = 0;
            } else {
                /* Check for CDELT and PC matrix representation */
                fits_read_key(m_hFITS, TDOUBLE, "CDELT1", &cdelt1, nullptr, &status);
                if ( ! status ) {
                    fits_read_key(m_hFITS, TDOUBLE, "CDELT2", &cdelt2, nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
                    cd[0] = cdelt1 * pc[0];
                    cd[1] = cdelt1 * pc[1];
                    cd[2] = cdelt2 * pc[2];
                    cd[3] = cdelt2 * pc[3];
                    status = 0;
                } else {
                    /* Look for CD matrix representation */
                    fits_read_key(m_hFITS, TDOUBLE, "CD1_1", &cd[0], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD1_2", &cd[1], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD2_1", &cd[2], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD2_2", &cd[3], nullptr, &status);
                }

                double radfac = DEG2RAD * aRadius;

                m_adfGeoTransform[1] = cd[0] * radfac;
                m_adfGeoTransform[2] = cd[1] * radfac;
                m_adfGeoTransform[4] = cd[2] * radfac;
                m_adfGeoTransform[5] = - cd[3] * radfac ;
                if ( crval1 > 180. ) {
                    crval1 = crval1 - 180.;
                }

                /* NOTA BENE: FITS standard define pixel integers at the center of the pixel,
                   0.5 must be subtract to have UpperLeft corner */
                m_adfGeoTransform[0] = crval1 * radfac - m_adfGeoTransform[1] * (crpix1-0.5);
                // assuming that center latitude is also the origin of the coordinate
                // system: this is not always true.
                // More generic implementation coming soon
                m_adfGeoTransform[3] = - m_adfGeoTransform[5] * (crpix2-0.5);
                                                         //+ crval2 * radfac;
                m_bGeoTransformValid = true;
            }

            char* pstr = strrchr(ctype, '-');
            if( pstr ) {
                pstr += 1;

            /* Defining projection type
               Following http://www.gdal.org/ogr__srs__api_8h.html (GDAL)
               and http://www.aanda.org/component/article?access=bibcode&bibcode=&bibcode=2002A%2526A...395.1077CFUL (FITS)
            */

                /* Sinusoidal / SFL projection */
                if( strcmp(pstr,"SFL" ) == 0 ) {
                    projName.assign("Sinusoidal_");
                    m_oSRS.SetSinusoidal(crval1, falseEast, falseNorth);

                /* Mercator, Oblique (Hotine) Mercator, Transverse Mercator */
                /* Mercator / MER projection */
                } else if( strcmp(pstr,"MER" ) == 0 ) {
                    projName.assign("Mercator_");
                    m_oSRS.SetMercator(crval2, crval1, scale, falseEast, falseNorth);

                /* Equirectangular / CAR projection */
                } else if( strcmp(pstr,"CAR" ) == 0 ) {
                    projName.assign("Equirectangular_");
                /*
                The standard_parallel_1 defines where the local radius is calculated
                not the center of Y Cartesian system (which is latitude_of_origin)
                But FITS WCS only supports projections on the sphere
                we assume here that the local radius is the one computed at the projection center
                */
                    m_oSRS.SetEquirectangular2(crval2, crval1, crval2, falseEast, falseNorth);
                /* Lambert Azimuthal Equal Area / ZEA projection */
                } else if( strcmp(pstr,"ZEA" ) == 0 ) {
                    projName.assign("Lambert_Azimuthal_Equal_Area_");
                    m_oSRS.SetLAEA(crval2, crval1, falseEast, falseNorth);

                /* Lambert Conformal Conic 1SP / COO projection */
                } else if( strcmp(pstr,"COO" ) == 0 ) {
                    projName.assign("Lambert_Conformal_Conic_1SP_");
                    m_oSRS.SetLCC1SP (crval2, crval1, scale, falseEast, falseNorth);

                /* Orthographic / SIN projection */
                } else if( strcmp(pstr,"SIN" ) == 0 ) {
                    projName.assign("Orthographic_");
                    m_oSRS.SetOrthographic(crval2, crval1, falseEast, falseNorth);

                /* Point Perspective / AZP projection */
                } else if( strcmp(pstr,"AZP" ) == 0 ) {
                    projName.assign("perspective_point_height_");
                    m_oSRS.SetProjection(SRS_PP_PERSPECTIVE_POINT_HEIGHT);
                    /* # appears to need height... maybe center lon/lat */

                /* Polar Stereographic / STG projection */
                } else if( strcmp(pstr,"STG" ) == 0 ) {
                    projName.assign("Polar_Stereographic_");
                    m_oSRS.SetStereographic(crval2, crval1, scale, falseEast, falseNorth);
                } else {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
                }

                projName.append(target);
                m_oSRS.SetProjParm(SRS_PP_FALSE_EASTING,0.0);
                m_oSRS.SetProjParm(SRS_PP_FALSE_NORTHING,0.0);

                m_oSRS.SetNode("PROJCS",projName.c_str());

                m_oSRS.SetGeogCS(GeogName.c_str(), DatumName.c_str(), target, aRadius, invFlattening,
                    "Reference_Meridian", 0.0, "degree", 0.0174532925199433);
            }  else {
                CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
            }
        }
    } else {
        CPLError(CE_Warning, CPLE_AppDefined,
             "No CTYPE keywords: no geospatial information available.");
    }
}

/************************************************************************/
/*                     LoadFITSInfo()                                   */
/************************************************************************/

void FITSDataset::LoadFITSInfo()

{
    int status = 0;
    int bitpix;
    double dfScale, dfOffset;

    LoadGeoreferencing();

    CPLAssert(!m_bMetadataChanged);
    CPLAssert(!m_bNoDataChanged);

    m_bMetadataChanged = false;
    m_bNoDataChanged = false;

    bitpix = this->m_fitsDataType;
    FITSRasterBand *poBand = cpl::down_cast<FITSRasterBand*>(GetRasterBand(1));

    if (bitpix != TUSHORT && bitpix != TUINT)
    {
        fits_read_key(m_hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if( status )
        {
            status = 0;
            dfScale = 1.;
        }
        fits_read_key(m_hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
        if( status )
        {
            status = 0;
            dfOffset = 0.;
        }
        if ( dfScale != 1. || dfOffset != 0. )
        {
            poBand->m_bHaveOffsetScale = true;
            poBand->m_dfScale = dfScale;
            poBand->m_dfOffset = dfOffset;
        }
    }

    fits_read_key(m_hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
    m_bNoDataSet = !status;
}

/************************************************************************/
/*                          GDALRegister_FITS()                         */
/************************************************************************/

void GDALRegister_FITS()

{
    if( GDALGetDriverByName( "FITS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "FITS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Flexible Image Transport System" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/fits.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "fits" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String IntegerList "
                               "Integer64List RealList" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='REPEAT_*' type='int' description='Repeat value for fields of type List'/>"
"  <Option name='COMPUTE_REPEAT' type='string-select' description='Determine when the repeat value for fields is computed'>"
"    <Value>AT_FIELD_CREATION</Value>"
"    <Value>AT_FIRST_FEATURE_CREATION</Value>"
"  </Option>"
"</LayerCreationOptionList>");
    poDriver->pfnOpen = FITSDataset::Open;
    poDriver->pfnIdentify = FITSDataset::Identify;
    poDriver->pfnCreate = FITSDataset::Create;
    poDriver->pfnCreateCopy = nullptr;
    poDriver->pfnDelete = FITSDataset::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
