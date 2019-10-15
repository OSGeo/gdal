/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements IGN France height correction ASCII grids
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_pam.h"

#include <cassert>
#include <vector>

// format description (in French) at
// https://geodesie.ign.fr/contenu/fichiers/documentation/grilles/notices/Grilles-MNT-TXT_Formats.pdf

/************************************************************************/
/* ==================================================================== */
/*                      IGNFHeightASCIIGridDataset                      */
/* ==================================================================== */
/************************************************************************/

class IGNFHeightASCIIGridDataset final: public GDALPamDataset
{
        double adfGeoTransform[6]{0,1,0,0,0,1};
        int m_nArrangementOrder = 0;

        size_t getSampleIndex( size_t nBufferCount );

        static bool CheckExtentAndComputeRasterSize( double dfLongMin,
                                                     double dfLongMax,
                                                     double dfLatMin,
                                                     double dfLatMax,
                                                     double dfStepLong,
                                                     double dfStepLat,
                                                     double& dfRasterXSize,
                                                     double& dfRasterYSize );

        static bool ParseHeaderMNT(GDALOpenInfo* poOpenInfo,
                                double& dfLongMin,
                                double& dfLongMax,
                                double& dfLatMin,
                                double& dfLatMax,
                                double& dfStepLong,
                                double& dfStepLat,
                                double& dfRasterXSize,
                                double& dfRasterYSize,
                                int& nArrangementOrder,
                                int& nCoordinatesAtNode,
                                int& nPrecisionCode,
                                CPLString& osDesc);

        static bool ParseHeaderGRA(GDALOpenInfo* poOpenInfo,
                                double& dfLongMin,
                                double& dfLongMax,
                                double& dfLatMin,
                                double& dfLatMax,
                                double& dfStepLong,
                                double& dfStepLat,
                                double& dfRasterXSize,
                                double& dfRasterYSize);

    public:
        IGNFHeightASCIIGridDataset() = default;

        CPLErr GetGeoTransform(double* padfGeoTransform) override;
        const char* _GetProjectionRef() override { return SRS_WKT_WGS84_LAT_LONG; }
        const OGRSpatialReference* GetSpatialRef() const override {
            return GetSpatialRefFromOldGetProjectionRef();
        }

        static int IdentifyMNT(GDALOpenInfo* poOpenInfo);
        static int IdentifyGRA(GDALOpenInfo* poOpenInfo);
        static int Identify(GDALOpenInfo* poOpenInfo);
        static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                    IGNFHeightASCIIGridRasterBand                     */
/* ==================================================================== */
/************************************************************************/

class IGNFHeightASCIIGridRasterBand final: public GDALPamRasterBand
{
        friend class IGNFHeightASCIIGridDataset;

        std::vector<double> adfBuffer;
        double m_dfNoDataValue = 0.0;
        bool m_bHasNoDataValue = false;

    public:
        explicit IGNFHeightASCIIGridRasterBand(GDALDataset* poDS,
                                      std::vector<double>&& adfBufferIn);
        CPLErr IReadBlock(int, int, void*) override;

        const char* GetUnitType() override { return "m"; }
        double GetNoDataValue(int* pbHasNoDataValue) override;
};

/************************************************************************/
/*                   IGNFHeightASCIIGridRasterBand()                    */
/************************************************************************/

IGNFHeightASCIIGridRasterBand::IGNFHeightASCIIGridRasterBand(
                    GDALDataset* poDSIn, std::vector<double>&& adfBufferIn):
    adfBuffer(std::move(adfBufferIn))
{
    poDS = poDSIn;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    eDataType = GDT_Float64;
    CPLAssert(adfBuffer.size() == static_cast<size_t>(
        poDS->GetRasterXSize()) * poDS->GetRasterYSize());
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr IGNFHeightASCIIGridRasterBand:: IReadBlock(int, int nBlockYOff,
                                                  void* pData)
{
    memcpy( pData, &adfBuffer[ nBlockYOff * nBlockXSize ],
            sizeof(double) * nBlockXSize );
    return CE_None;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double IGNFHeightASCIIGridRasterBand::GetNoDataValue(int* pbHasNoDataValue)
{
    if( pbHasNoDataValue )
        *pbHasNoDataValue = m_bHasNoDataValue;
    return m_dfNoDataValue;
}

/************************************************************************/
/*                           GetGeoTransform()                          */
/************************************************************************/

CPLErr IGNFHeightASCIIGridDataset::GetGeoTransform(double* padfGeoTransform)
{
    memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                            IdentifyMNT()                             */
/************************************************************************/

int IGNFHeightASCIIGridDataset::IdentifyMNT(GDALOpenInfo* poOpenInfo)
{
    const GByte* pabyHeader = poOpenInfo->pabyHeader;
    int iPosFirstNewLine = -1;
    int nCountFields = 0;
    for(int i = 0; i < poOpenInfo->nHeaderBytes; i++ )
    {
        const GByte ch = pabyHeader[i];
        if( ch == ' ' )
        {
            continue;
        }
        if( ch == '\r' || ch == '\n' )
        {
            iPosFirstNewLine = i;
            break;
        }
        if( i == 0 || pabyHeader[i-1] == ' ' )
        {
            nCountFields ++;
        }
        if( nCountFields <= 11 )
        {
            if( !((ch >= '0' && ch <= '9') || ch == '-' || ch == '.') )
            {
                return FALSE;
            }
        }
        else if( ch == static_cast<GByte>('\xC3') &&
            i + 1 < poOpenInfo->nHeaderBytes &&
            (pabyHeader[i+1] == static_cast<GByte>('\xA9') || // eacute in UTF-8
             pabyHeader[i+1] == static_cast<GByte>('\xA8') || // egrave in UTF-8
             pabyHeader[i+1] == static_cast<GByte>('\xAF')  // i trema in UTF-8
             ))
        {
            i++;
        }
        else if( ch < 32 || (ch > 127 &&
                ch != static_cast<GByte>('\xE9') && // eacute LATIN-1
                ch != static_cast<GByte>('\xEF') // i trema LATIN-1
            ) )
        {
            return FALSE;
        }
    }
    if( iPosFirstNewLine < 0 || nCountFields < 12 )
    {
        return FALSE;
    }

    for( int i = iPosFirstNewLine + 1; i < poOpenInfo->nHeaderBytes; i++ )
    {
        const GByte ch = pabyHeader[i];
        if( !((ch >= '0' && ch <= '9') || isspace(ch) ||
              ch == '-' || ch == '.') )
        {
            return FALSE;
        }
    }

    double dfLongMin = 0.0;
    double dfLongMax = 0.0;
    double dfLatMin = 0.0;
    double dfLatMax = 0.0;
    double dfStepLong = 0.0;
    double dfStepLat = 0.0;
    double dfRasterXSize = 0.0;
    double dfRasterYSize = 0.0;
    int nArrangementOrder = 0;
    int nCoordinatesAtNode = 0;
    int nPrecisionCode = 0;
    CPLString osDesc;
    return ParseHeaderMNT(poOpenInfo, dfLongMin, dfLongMax, dfLatMin, dfLatMax,
                       dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize,
                       nArrangementOrder, nCoordinatesAtNode, nPrecisionCode,
                       osDesc);
}

/************************************************************************/
/*                            IdentifyGRA()                             */
/************************************************************************/

int IGNFHeightASCIIGridDataset::IdentifyGRA(GDALOpenInfo* poOpenInfo)
{
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GRA") )
        return FALSE;

    const GByte* pabyHeader = poOpenInfo->pabyHeader;
    int nCountFields = 0;
    int nLine = 1;
    for(int i = 0; i < poOpenInfo->nHeaderBytes; i++ )
    {
        const GByte ch = pabyHeader[i];
        if( ch == ' ' || ch == '\r' )
        {
            continue;
        }
        if( ch == '\n' )
        {
            if( nCountFields != 2 )
                return FALSE;
            if( nLine == 3 )
            {
                double dfLongMin = 0.0;
                double dfLongMax = 0.0;
                double dfLatMin = 0.0;
                double dfLatMax = 0.0;
                double dfStepLong = 0.0;
                double dfStepLat = 0.0;
                double dfRasterXSize = 0.0;
                double dfRasterYSize = 0.0;
                return ParseHeaderGRA(poOpenInfo,
                    dfLongMin, dfLongMax, dfLatMin, dfLatMax,
                    dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize);
            }
            nCountFields = 0;
            nLine ++;
            continue;
        }
        if( i == 0 || pabyHeader[i-1] == ' ' || pabyHeader[i-1] == '\n' )
        {
            nCountFields ++;
        }
        if( !((ch >= '0' && ch <= '9') || ch == '-' || ch == '.') )
        {
            return FALSE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int IGNFHeightASCIIGridDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    return (IdentifyMNT(poOpenInfo) == TRUE) ||
           (IdentifyGRA(poOpenInfo) == TRUE);
}

/************************************************************************/
/*                    CheckExtentAndComputeRasterSize()                 */
/************************************************************************/

bool IGNFHeightASCIIGridDataset::CheckExtentAndComputeRasterSize(
                                                        double dfLongMin,
                                                        double dfLongMax,
                                                        double dfLatMin,
                                                        double dfLatMax,
                                                        double dfStepLong,
                                                        double dfStepLat,
                                                        double& dfRasterXSize,
                                                        double& dfRasterYSize)
{
    if( !(dfLongMin >= -180.0 && dfLongMax <= 180.0 && dfLongMin < dfLongMax &&
          dfLatMin >= -90.0 && dfLatMax <= 90.0 && dfLatMin < dfLatMax) )
    {
        return false;
    }
    if( !(dfStepLong > 0 && dfStepLong < 360 &&
          dfStepLat > 0 && dfStepLat < 180) )
    {
        return false;
    }
    dfRasterXSize = (dfLongMax - dfLongMin) / dfStepLong;
    dfRasterYSize = (dfLatMax - dfLatMin) / dfStepLat;
    if( dfRasterXSize > 10000 || dfRasterYSize > 10000 ||
        dfRasterXSize * dfRasterYSize > 10e6 )
    {
        return false;
    }
    return true;
}

/************************************************************************/
/*                         ParseHeaderMNT()                             */
/************************************************************************/

bool IGNFHeightASCIIGridDataset::ParseHeaderMNT(GDALOpenInfo* poOpenInfo,
                                            double& dfLongMin,
                                            double& dfLongMax,
                                            double& dfLatMin,
                                            double& dfLatMax,
                                            double& dfStepLong,
                                            double& dfStepLat,
                                            double& dfRasterXSize,
                                            double& dfRasterYSize,
                                            int& nArrangementOrder,
                                            int& nCoordinatesAtNode,
                                            int& nPrecisionCode,
                                            CPLString& osDesc)
{
    std::string osHeader;
    osHeader.assign(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                    poOpenInfo->nHeaderBytes);
    const size_t nHeaderSize = osHeader.find_first_of("\r\n");
    CPLAssert(nHeaderSize != std::string::npos);
    osHeader.resize( nHeaderSize );
    CPLStringList aosTokens(CSLTokenizeString2(osHeader.c_str(), " ", 0));
    CPLAssert( aosTokens.size() >= 12 ); // from Identify
    dfLongMin = CPLAtof(aosTokens[0]);
    dfLongMax = CPLAtof(aosTokens[1]);
    dfLatMin = CPLAtof(aosTokens[2]);
    dfLatMax = CPLAtof(aosTokens[3]);
    dfStepLong = CPLAtof(aosTokens[4]);
    dfStepLat = CPLAtof(aosTokens[5]);
    if( !CheckExtentAndComputeRasterSize(
            dfLongMin, dfLongMax, dfLatMin, dfLatMax,
            dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize) )
    {
        return false;
    }

    nArrangementOrder = atoi(aosTokens[6]);
#ifdef DEBUG_VERBOSE
    CPLDebug("IGNFHeightASCIIGrid", "nArrangementOrder = %d", nArrangementOrder);
#endif
    if( nArrangementOrder < 1 || nArrangementOrder > 4 )
    {
        CPLDebug("IGNFHeightASCIIGrid", "Wrong value for nArrangementOrder = %d",
                 nArrangementOrder);
        return false;
    }
    nCoordinatesAtNode = atoi(aosTokens[7]);
    if( nCoordinatesAtNode != 0 && nCoordinatesAtNode != 1 )
    {
        CPLDebug("IGNFHeightASCIIGrid", "Wrong value for nCoordinatesAtNode = %d",
                 nCoordinatesAtNode);
        return false;
    }
    const int nValuesPerNode = atoi(aosTokens[8]);
    if( nValuesPerNode != 1 )
    {
        CPLDebug("IGNFHeightASCIIGrid", "Wrong value for nValuesPerNode = %d",
                 nValuesPerNode);
        return false;
    }
    nPrecisionCode = atoi(aosTokens[9]);
    if( nPrecisionCode != 0 && nPrecisionCode != 1 )
    {
        CPLDebug("IGNFHeightASCIIGrid", "Wrong value for nPrecisionCode = %d",
                 nPrecisionCode);
        return false;
    }
    const double dfTranslation = CPLAtof(aosTokens[10]);
    if( dfTranslation != 0.0 )
    {
        CPLDebug("IGNFHeightASCIIGrid", "Wrong value for dfTranslation = %f",
                 dfTranslation);
        return false;
    }

    osDesc = aosTokens[11];
    for( int i = 12; i < aosTokens.size(); ++i )
    {
        osDesc += " ";
        osDesc += aosTokens[i];
    }
    osDesc.replaceAll("\xE9", "e");
    osDesc.replaceAll("\xC3\xA9", "e");
    osDesc.replaceAll("\xC3\xA8", "e");
    osDesc.replaceAll("\xC3\xAF", "i");
    osDesc.replaceAll("\xEF", "i");

    return true;
}

/************************************************************************/
/*                         ParseHeaderGRA()                             */
/************************************************************************/

bool IGNFHeightASCIIGridDataset::ParseHeaderGRA(GDALOpenInfo* poOpenInfo,
                                            double& dfLongMin,
                                            double& dfLongMax,
                                            double& dfLatMin,
                                            double& dfLatMax,
                                            double& dfStepLong,
                                            double& dfStepLat,
                                            double& dfRasterXSize,
                                            double& dfRasterYSize)
{
    std::string osHeader;
    osHeader.assign(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                    poOpenInfo->nHeaderBytes);
    CPLStringList aosTokens(CSLTokenizeString2(osHeader.c_str(), " \r\n", 0));
    CPLAssert( aosTokens.size() >= 6 ); // from Identify
    dfLatMin = CPLAtof(aosTokens[0]);
    dfLatMax = CPLAtof(aosTokens[1]);
    dfLongMin = CPLAtof(aosTokens[2]);
    dfLongMax = CPLAtof(aosTokens[3]);
    dfStepLat = CPLAtof(aosTokens[4]);
    dfStepLong = CPLAtof(aosTokens[5]);
    return CheckExtentAndComputeRasterSize(
            dfLongMin, dfLongMax, dfLatMin, dfLatMax,
            dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize);
}

/************************************************************************/
/*                          getSampleIndex()                            */
/************************************************************************/

size_t IGNFHeightASCIIGridDataset::getSampleIndex( size_t nBufferCount )
{
    if( m_nArrangementOrder == 1 )
    {
        return (nRasterYSize - 1 - (nBufferCount % nRasterYSize)) *
            nRasterXSize + (nBufferCount / nRasterYSize);
    }
    else if( m_nArrangementOrder == 2 )
    {
        return nBufferCount;
    }
    else if( m_nArrangementOrder == 3 )
    {
        return (nBufferCount % nRasterYSize) *
            nRasterXSize + (nBufferCount / nRasterYSize);
    }
    else
    {
        return (nRasterYSize - 1 - (nBufferCount / nRasterXSize)) *
                    nRasterXSize + (nBufferCount % nRasterXSize);
    }
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

GDALDataset* IGNFHeightASCIIGridDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update )
    {
        return nullptr;
    }

    double dfLongMin = 0.0;
    double dfLongMax = 0.0;
    double dfLatMin = 0.0;
    double dfLatMax = 0.0;
    double dfStepLong = 0.0;
    double dfStepLat = 0.0;
    double dfRasterXSize = 0.0;
    double dfRasterYSize = 0.0;
    int nArrangementOrder = 0;
    int nCoordinatesAtNode = 0;
    int nPrecisionCode = 0;
    CPLString osDesc;

    bool isMNTFormat = false;
    if( IdentifyMNT(poOpenInfo) )
    {
        isMNTFormat = true;
        ParseHeaderMNT(poOpenInfo, dfLongMin, dfLongMax, dfLatMin, dfLatMax,
                       dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize,
                       nArrangementOrder, nCoordinatesAtNode, nPrecisionCode,
                       osDesc);
    }
    else if( IdentifyGRA(poOpenInfo) )
    {
        ParseHeaderGRA(poOpenInfo, dfLongMin, dfLongMax, dfLatMin, dfLatMax,
                       dfStepLong, dfStepLat, dfRasterXSize, dfRasterYSize);
        nArrangementOrder = 2;
    }
    else
    {
        return nullptr;
    }

    // Check file size
    VSIFSeekL( poOpenInfo->fpL, 0, SEEK_END );
    vsi_l_offset nFileLength = VSIFTellL(poOpenInfo->fpL);
    if( nFileLength > 10 * 1024 * 1024 )
    {
        return nullptr;
    }

    VSIFSeekL( poOpenInfo->fpL, 0, SEEK_SET );
    std::string osBuffer;
    osBuffer.resize( static_cast<size_t>(nFileLength) + 1 );
    osBuffer[osBuffer.size() - 1] = '\n';
    VSIFReadL( &osBuffer[0], 1, osBuffer.size() - 1, poOpenInfo->fpL);

    // Create dataset
    auto poDS = new IGNFHeightASCIIGridDataset();
    poDS->m_nArrangementOrder = nArrangementOrder;
    poDS->adfGeoTransform[0] = dfLongMin - 0.5 * dfStepLong;
    poDS->adfGeoTransform[1] = dfStepLong;
    poDS->adfGeoTransform[2] = 0;
    poDS->adfGeoTransform[3] = dfLatMax + 0.5 * dfStepLat;
    poDS->adfGeoTransform[4] = 0;
    poDS->adfGeoTransform[5] = -dfStepLat;
    poDS->nRasterXSize = static_cast<int>( dfRasterXSize + 0.5 + 1 );
    poDS->nRasterYSize = static_cast<int>( dfRasterYSize + 0.5 + 1 );
    if( !osDesc.empty() )
    {
        poDS->GDALDataset::SetMetadataItem("DESCRIPTION", osDesc);
    }

    // Parse values
    std::vector<double> adfBuffer;
    adfBuffer.resize( poDS->nRasterXSize * poDS->nRasterYSize );
    size_t nBufferCount = 0;
    size_t nHeaderSize;
    if( isMNTFormat )
    {
        nHeaderSize = osBuffer.find_first_of("\r\n");
    }
    else
    {
        nHeaderSize = osBuffer.find('\n');
        CPLAssert(nHeaderSize != std::string::npos);
        nHeaderSize = osBuffer.find('\n', nHeaderSize + 1);
        CPLAssert(nHeaderSize != std::string::npos);
        nHeaderSize = osBuffer.find('\n', nHeaderSize + 1);
    }
    CPLAssert(nHeaderSize != std::string::npos);
    size_t nLastPos = nHeaderSize + 1;
    int iValuePerNode = 0;
    bool lastWasSep = true;
    for( size_t i = nLastPos; i < osBuffer.size(); i++ )
    {
        if( isspace(osBuffer[i]) )
        {
            if( !lastWasSep  )
            {
                if( iValuePerNode == 0 )
                {
                    if( nBufferCount == adfBuffer.size() )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Too many values at file pos %d",
                                static_cast<int>(i));
                        delete poDS;
                        return nullptr;
                    }
                    if( nCoordinatesAtNode == 0 )
                    {
                        const size_t nBufferIdx =
                            poDS->getSampleIndex(nBufferCount);
                        adfBuffer[nBufferIdx] = CPLAtof(
                            osBuffer.substr(nLastPos, i - nLastPos).c_str());
                    }
                    if( nCoordinatesAtNode == 0 && nPrecisionCode == 0 )
                    {
                        nBufferCount ++;
                    }
                    else
                    {
                        iValuePerNode++;
                    }
                }
                else if( nCoordinatesAtNode && iValuePerNode < 2 )
                {
                    iValuePerNode ++;
                }
                else if( nCoordinatesAtNode && iValuePerNode == 2 )
                {
                    const size_t nBufferIdx =
                            poDS->getSampleIndex(nBufferCount);
                    adfBuffer[nBufferIdx] = CPLAtof(
                            osBuffer.substr(nLastPos, i - nLastPos).c_str());
                    if( nPrecisionCode == 0 )
                    {
                        nBufferCount ++;
                        iValuePerNode = 0;
                    }
                    else
                    {
                        iValuePerNode ++;
                    }
                }
                else
                {
                    iValuePerNode = 0;
                    nBufferCount ++;
                }
                nLastPos = i + 1;
            }
            lastWasSep = true;
        }
        else
        {
            lastWasSep = false;
        }
    }
    if( nBufferCount != adfBuffer.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not enough values. Got %d, expected %d",
                 static_cast<int>(nBufferCount),
                 static_cast<int>(adfBuffer.size()) );
        delete poDS;
        return nullptr;
    }

    auto poBand = new IGNFHeightASCIIGridRasterBand(poDS,
                                                       std::move(adfBuffer));
    if( !isMNTFormat )
    {
        poBand->m_bHasNoDataValue = true;
        poBand->m_dfNoDataValue = 9999;
    }
    poDS->SetBand(1, poBand);

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
/*                  GDALRegister_IGNFHeightASCIIGrid()                  */
/************************************************************************/

void GDALRegister_IGNFHeightASCIIGrid()

{
    if( GDALGetDriverByName("IGNFHeightASCIIGrid") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("IGNFHeightASCIIGrid");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "IGN France height correction ASCII Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "frmt_various.html#IGNFHeightASCIIGrid");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "mnt txt gra");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = IGNFHeightASCIIGridDataset::Open;
    poDriver->pfnIdentify = IGNFHeightASCIIGridDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
