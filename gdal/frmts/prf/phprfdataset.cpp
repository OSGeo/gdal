/******************************************************************************
 * Purpose:  Racurs PHOTOMOD tiled format reader (http://www.racurs.ru)
 * Author:   Andrew Sudorgin (drons [a] list dot ru)
 ******************************************************************************
 * Copyright (c) 2016, Andrew Sudorgin
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

#include <gdal.h>
#include <gdal_priv.h>
#include <gdal_proxy.h>
#include <cpl_minixml.h>
#include "../vrt/vrtdataset.h"

enum ph_format
{
    ph_megatiff,
    ph_xdem
};

#define PH_PRF_DRIVER       "PRF"
#define PH_PRF_EXT          "prf"
#define PH_DEM_EXT          "x-dem"
#define PH_GEOREF_SHIFT_Y   (1.0)

class PhPrfBand : public VRTSourcedRasterBand
{
    std::vector<GDALRasterBand*> osOverview;
public:
    PhPrfBand( GDALDataset* poDataset, int nBandCount, GDALDataType eType,
               int nXSize, int nYSize ) :
               VRTSourcedRasterBand( poDataset, nBandCount, eType, nXSize, nYSize )
    {
    }

    void AddOverview( GDALRasterBand* ov )
    {
        osOverview.push_back( ov );
    }

    int GetOverviewCount()
    {
        if( osOverview.size() > 0 )
        {
            return (int)osOverview.size();
        }
        else
        {
            return VRTSourcedRasterBand::GetOverviewCount();
        }
    }

    GDALRasterBand* GetOverview( int i )
    {
        size_t	n = (size_t)i;
        if( n < osOverview.size() )
        {
            return osOverview[ n ];
        }
        else
        {
            return VRTSourcedRasterBand::GetOverview( i );
        }
    }
};

class PhPrfDataset : public VRTDataset
{
public:
    PhPrfDataset( GDALAccess eAccess, int nSizeX, int nSizeY, int nBandCount, GDALDataType eType, const char* pszName );
    ~PhPrfDataset();
    int AddTile( const char* pszPartName, GDALAccess eAccess, int nWidth, int nHeight, int nOffsetX, int nOffsetY, int nScale );
    static int Identify( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo* poOpenInfo );
};

PhPrfDataset::PhPrfDataset( GDALAccess _eAccess, int nSizeX, int nSizeY, int nBandCount, GDALDataType eType, const char* pszName ) : VRTDataset( nSizeX, nSizeY )
{
    poDriver = (GDALDriver*)GDALGetDriverByName( PH_PRF_DRIVER );
    eAccess = _eAccess;
    SetWritable( FALSE );   //Avoid rewrite of *.prf file with 'vrt' file
    SetDescription( pszName );

    for( int i = 0; i != nBandCount; ++i )
    {
        PhPrfBand*	poBand = new PhPrfBand( this, i + 1, eType, nSizeX, nSizeY );
        SetBand( i + 1, poBand );
    }
}

PhPrfDataset::~PhPrfDataset()
{
}

int PhPrfDataset::AddTile( const char* pszPartName, GDALAccess eAccessType, int nWidth, int nHeight, int nOffsetX, int nOffsetY, int nScale )
{
    GDALProxyPoolDataset*   poTileDataset;
    poTileDataset = new GDALProxyPoolDataset( pszPartName, nWidth, nHeight,
                                              eAccessType, FALSE );

    if( poTileDataset == NULL )
    {
        return -1;
    }

    for( int nBand = 1; nBand != GetRasterCount() + 1; ++nBand )
    {
        PhPrfBand*      poBand = dynamic_cast<PhPrfBand*>( GetRasterBand( nBand ) );
        GDALRasterBand* poTileBand;

        if( poBand == NULL )
        {
            delete poTileDataset;
            return -1;
        }

        //! \todo What reason for nBlockXSize&nBlockYSize passed to AddSrcBandDescription
        poTileDataset->AddSrcBandDescription( poBand->GetRasterDataType(), 0, 0 );
        poTileBand = poTileDataset->GetRasterBand( nBand );

        if( 0 == nScale )
        {
            poBand->AddSimpleSource( poTileBand, 0, 0, nWidth, nHeight,
                                     nOffsetX, nOffsetY, nWidth, nHeight );
        }
        else
        {
            poBand->AddOverview( poTileBand );
        }
    }
    return 0;
}

int PhPrfDataset::Identify( GDALOpenInfo* poOpenInfo )
{
    if( EQUAL( CPLGetExtension( poOpenInfo->pszFilename ), PH_PRF_EXT ) )
    {
        return TRUE;
    }
    else
    if( EQUAL( CPLGetExtension( poOpenInfo->pszFilename ), PH_DEM_EXT ) )
    {
        return TRUE;
    }

    return FALSE;
}

static void GetXmlNameValuePair( const CPLXMLNode* psElt, CPLString& osName,
                                 CPLString& osValue )
{
    for( const CPLXMLNode* psAttr = psElt->psChild; psAttr != NULL; psAttr = psAttr->psNext )
    {
        if( psAttr->eType != CXT_Attribute ||
            psAttr->pszValue == NULL ||
            psAttr->psChild == NULL ||
            psAttr->psChild->pszValue == NULL )
        {
            continue;
        }
        if( EQUAL( psAttr->pszValue, "n" ) )
        {
            osName = psAttr->psChild->pszValue;
        }
        else
        if( EQUAL( psAttr->pszValue, "v" ) )
        {
            osValue = psAttr->psChild->pszValue;
        }
    }
}

static CPLString GetXmlAttribute( const CPLXMLNode* psElt, const CPLString& osAttrName,
                                  const CPLString& osDef = CPLString() )
{
    for( const CPLXMLNode* psAttr = psElt->psChild; psAttr != NULL; psAttr = psAttr->psNext )
    {
        if( psAttr->eType != CXT_Attribute ||
            psAttr->pszValue == NULL ||
            psAttr->psChild == NULL ||
            psAttr->psChild->pszValue == NULL )
        {
            continue;
        }
        if( EQUAL( psAttr->pszValue, osAttrName ) )
        {
            return psAttr->psChild->pszValue;
        }
    }
    return osDef;
}

static bool ParseGeoref( const CPLXMLNode* psGeorefElt, double* padfGeoTrans )
{
    bool                pabOk[6] = {false,false,false,false,false,false};
    static const char*  papszGeoKeys[6] = { "A_0", "A_1", "A_2",
                                            "B_0", "B_1", "B_2" };
    for( const CPLXMLNode* elt = psGeorefElt->psChild; elt != NULL;	elt = elt->psNext )
    {
        CPLString   osName;
        CPLString   osValue;
        GetXmlNameValuePair( elt, osName, osValue );
        for( int k = 0; k != 6; ++k )
        {
            if( EQUAL( osName, papszGeoKeys[k] ) )
            {
                padfGeoTrans[k] = CPLAtof( osValue );
                pabOk[k] = true;
            }
        }
    }

    for( int k = 0; k != 6; ++k )
    {
        if( !pabOk[k] )
        {
            break;
        }
        if( k == 5 )
        {
            padfGeoTrans[3] -= PH_GEOREF_SHIFT_Y * padfGeoTrans[4];
            padfGeoTrans[3] -= PH_GEOREF_SHIFT_Y * padfGeoTrans[5];
            return true;
        }
    }
    return false;
}

static GDALDataType ParseChannelsInfo( const CPLXMLNode* psElt )
{
    CPLString   osType;
    CPLString   osBytesPS;
    CPLString   osChannels;

    for( const CPLXMLNode* psChild = psElt->psChild; psChild != NULL; psChild = psChild->psNext )
    {
        if( psChild->eType != CXT_Element )
        {
            continue;
        }

        CPLString   osName;
        CPLString   osValue;

        GetXmlNameValuePair( psChild, osName, osValue );

        if( EQUAL( osName, "type" ) )
        {
            osType = osValue;
        }
        else
        if( EQUAL( osName, "bytes_ps" ) )
        {
            osBytesPS = osValue;
        }
        else
        if( EQUAL( osName, "channels" ) )
        {
            osChannels = osValue;
        }
    }

    int	nDataTypeSize = atoi( osBytesPS );
    if( osType == "U" )
    {
        switch( nDataTypeSize )
        {
        case 1:
            return GDT_Byte;
        case 2:
            return GDT_UInt16;
        case 4:
            return GDT_UInt32;
        default:
            CPLError( CE_Failure, CPLE_OpenFailed, "Unsupported datatype size %d\n", nDataTypeSize );
            return GDT_Unknown;
        }
    }
    else
    if( osType == "F" )
    {
        switch( nDataTypeSize )
        {
        case 4:
            return GDT_Float32;
        case 8:
            return GDT_Float64;
        default:
            CPLError( CE_Failure, CPLE_OpenFailed, "Unsupported datatype size %d\n", nDataTypeSize );
            return GDT_Unknown;
        }
    }

    return GDT_Unknown;
}

GDALDataset* PhPrfDataset::Open( GDALOpenInfo* poOpenInfo )
{
    ph_format   eFormat;

    if( EQUAL( CPLGetExtension(poOpenInfo->pszFilename), PH_PRF_EXT ) )
    {
        eFormat = ph_megatiff;
    }
    else
    if( EQUAL( CPLGetExtension(poOpenInfo->pszFilename), PH_DEM_EXT ) )
    {
        eFormat = ph_xdem;
    }
    else
    {
        return NULL;
    }

    CPLXMLTreeCloser	oDoc( CPLParseXMLFile( poOpenInfo->pszFilename ) );

    if( oDoc.get() == NULL )
    {
        return NULL;
    }

    const CPLXMLNode*   psPhIni( CPLSearchXMLNode( oDoc.get(), "=phini" ) );
    if( psPhIni == NULL )
    {
        return NULL;
    }

    int             nSizeX = 0;
    int             nSizeY = 0;
    int             nBandCount = 0;
    GDALDataType    eResultDatatype = GDT_Unknown;
    CPLString       osPartsBasePath( CPLGetPath( poOpenInfo->pszFilename ) );
    CPLString       osPartsPath( osPartsBasePath + "/" + CPLGetBasename( poOpenInfo->pszFilename ) );
    CPLString       osPartsExt;
    double          padfGeoTrans[6] = {0,0,0,0,0,0};
    bool            bGeoTransOk = false;

    if( eFormat == ph_megatiff )
    {
        osPartsExt = ".tif";
    }
    else
    if( eFormat == ph_xdem )
    {
        osPartsExt = ".demtif";
    }

    for( const CPLXMLNode* psElt = psPhIni->psChild; psElt != NULL; psElt = psElt->psNext )
    {
        if( !EQUAL(psElt->pszValue,"s") ||
            psElt->eType != CXT_Element )
        {
            continue;
        }

        CPLString   osName;
        CPLString   osValue;

        GetXmlNameValuePair( psElt, osName, osValue );

        if( EQUAL( osName, "parts_ext" ) )
        {
            osPartsExt = "." + osValue;
        }
    }

    for( const CPLXMLNode* psElt = psPhIni->psChild; psElt != NULL;	psElt = psElt->psNext )
    {
        CPLString   osName;
        CPLString   osValue;

        GetXmlNameValuePair( psElt, osName, osValue );

        if( EQUAL( osName, "ChannelsInfo" ) )
        {
            eResultDatatype = ParseChannelsInfo( psElt );
        }
        else
        if( EQUAL( osName, "Width" ) )
        {
            nSizeX = atoi( osValue );
        }
        else
        if( EQUAL( osName, "Height" ) )
        {
            nSizeY = atoi( osValue );
        }
        else
        if( EQUAL( osName, "QChans" ) )
        {
            nBandCount = atoi( osValue );
        }
        else
        if( EQUAL( osName, "GeoRef" ) )
        {
            bGeoTransOk = ParseGeoref( psElt, padfGeoTrans );
        }
    }

    if( eResultDatatype == GDT_Unknown )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "GDAL Dataset datatype not found\n" );
        return NULL;
    }

    if( nSizeX == 0 || nSizeY == 0 || nBandCount == 0 )
    {
        return NULL;
    }

    PhPrfDataset*   poDataset = new PhPrfDataset( GA_ReadOnly, nSizeX, nSizeY, nBandCount, eResultDatatype, poOpenInfo->pszFilename );

    if( !GDALCheckDatasetDimensions( poDataset->GetRasterXSize(), poDataset->GetRasterYSize() ) )
    {
        delete poDataset;
        return NULL;
    }

    for( const CPLXMLNode* psElt = psPhIni->psChild; psElt != NULL;	psElt = psElt->psNext )
    {
        int	nWidth = 0;
        int	nHeight = 0;
        int	nOffsetX = 0;
        int	nOffsetY = 0;
        int	nScale = 0;

        for( const CPLXMLNode* psItem = psElt->psChild; psItem != NULL;	psItem = psItem->psNext )
        {
            CPLString   osName;
            CPLString   osValue;

            GetXmlNameValuePair( psItem, osName, osValue );

            if( EQUAL( osName, "Width" ) )
            {
                nWidth = atoi( osValue );
            }
            else
            if( EQUAL( osName, "Height" ) )
            {
                nHeight = atoi( osValue );
            }
            else
            if( EQUAL( osName, "DispX" ) )
            {
                nOffsetX = atoi( osValue );
            }
            else
            if( EQUAL( osName, "DispY" ) )
            {
                nOffsetY = atoi( osValue );
            }
            else
            if( EQUAL( osName, "Scale" ) )
            {
                nScale = atoi( osValue );
            }
        }

        if( nWidth == 0 || nHeight == 0 )
        {
            continue;
        }

        CPLString	osPartName( osPartsPath + "/" +
                                GetXmlAttribute( psElt, "n" ) +
                                osPartsExt );

        if( 0 != poDataset->AddTile( osPartName, GA_ReadOnly, nWidth, nHeight, nOffsetX, nOffsetY, nScale ) )
        {
            delete poDataset;
            return NULL;
        }
    }

    if( eFormat == ph_megatiff && bGeoTransOk )
    {
        poDataset->SetGeoTransform( padfGeoTrans );
    }

    return poDataset;
}

void GDALRegister_PRF()
{
    GDALDriver* poDriver;

    if( GDALGetDriverByName( PH_PRF_DRIVER ) == NULL )
    {
        poDriver = new GDALDriver;
        
        poDriver->SetDescription( PH_PRF_DRIVER );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Racurs PHOTOMOD PRF" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "prf" );
        poDriver->pfnIdentify = PhPrfDataset::Identify;
        poDriver->pfnOpen = PhPrfDataset::Open;
        GDALRegisterDriver( (GDALDriverH)poDriver );
    }
}

