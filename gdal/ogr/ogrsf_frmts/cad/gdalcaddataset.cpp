/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#include "cpl_conv.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_cad.h"
#include "vsilfileio.h"

class CADWrapperRasterBand : public GDALProxyRasterBand
{
  GDALRasterBand* poBaseBand;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() override { return poBaseBand; }

  public:
    explicit CADWrapperRasterBand( GDALRasterBand* poBaseBandIn ) :
                    poBaseBand( poBaseBandIn )
    {
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }
    virtual ~CADWrapperRasterBand() {}
};

GDALCADDataset::GDALCADDataset() :
    poCADFile(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    poRasterDS(nullptr),
    poSpatialReference(nullptr)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

GDALCADDataset::~GDALCADDataset()
{
    if( poRasterDS != nullptr )
    {
        GDALClose( poRasterDS );
        poRasterDS = nullptr;
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if( poSpatialReference)
        poSpatialReference->Release();

    if( poCADFile )
        delete poCADFile;
}

void GDALCADDataset::FillTransform(CADImage* pImage, double dfUnits)
{
    CADImage::ResolutionUnit eResUnits = pImage->getResolutionUnits();
    double dfMultiply = 1.0;

    switch( eResUnits ) // 0 == none, 2 == centimeters, 5 == inches;
    {
        case CADImage::ResolutionUnit::CENTIMETER:
            dfMultiply = 100.0 / dfUnits; // Meters to linear units
            break;
        case CADImage::ResolutionUnit::INCH:
            dfMultiply = 0.0254 / dfUnits;
            break;
        case CADImage::ResolutionUnit::NONE:
        default:
            dfMultiply = 1.0;
    }

    CADVector oSizePt = pImage->getImageSizeInPx();
    CADVector oInsPt = pImage->getVertInsertionPoint();
    CADVector oSizeUnitsPt = pImage->getPixelSizeInACADUnits();
    adfGeoTransform[0] = oInsPt.getX();
    adfGeoTransform[3] = oInsPt.getY() + oSizePt.getY() * oSizeUnitsPt.getX() *
    dfMultiply;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[4] = 0.0;

    adfGeoTransform[1] = oSizeUnitsPt.getX() * dfMultiply;
    adfGeoTransform[5] = -oSizeUnitsPt.getY() * dfMultiply;
}

int GDALCADDataset::Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO,
                          long nSubRasterLayer, long nSubRasterFID )
{
    osCADFilename = pFileIO->GetFilePath();
    SetDescription( poOpenInfo->pszFilename );

    const char * papszReadOptions = CSLFetchNameValueDef(
                             poOpenInfo->papszOpenOptions, "MODE", "READ_FAST" );
    const char * papszReadUnsupportedGeoms = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "ADD_UNSUPPORTED_GEOMETRIES_DATA", "NO");

    enum CADFile::OpenOptions openOpts = CADFile::READ_FAST;
    bool bReadUnsupportedGeometries = false;
    if( EQUAL( papszReadOptions, "READ_ALL" ) )
    {
        openOpts = CADFile::READ_ALL;
    }
    else if( EQUAL( papszReadOptions, "READ_FASTEST" ) )
    {
        openOpts = CADFile::READ_FASTEST;
    }

    if( EQUAL( papszReadUnsupportedGeoms, "YES") )
    {
        bReadUnsupportedGeometries = true;
    }

    poCADFile = OpenCADFile( pFileIO, openOpts, bReadUnsupportedGeometries );

    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n"
                  "Supported formats are:\n%s", GetVersionString(), GetCADFormats() );
        return FALSE;
    }

    if ( GetLastErrorCode() != CADErrorCodes::SUCCESS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\nSupported formats: %s",
                  GetVersionString(), GetCADFormats() );
        return FALSE;
    }

    OGRSpatialReference *poSpatialRef = GetSpatialReference();
    int nRasters = 1;

    if( nSubRasterLayer != -1 && nSubRasterFID != -1 )
    {
        // Indicates that subdataset from CAD layer number nSubRasterLayer and
        // FID nSubRasterFID is request
        nRasters = 2;
    }
    else
    {
        // Fill metadata
        const CADHeader& header = poCADFile->getHeader();
        for( size_t i = 0; i < header.getSize(); ++i )
        {
            short nCode = header.getCode( static_cast<int>( i ) );
            const CADVariant& oVal = header.getValue( nCode );
            GDALDataset::SetMetadataItem( header.getValueName( nCode ),
                                                        oVal.getString().c_str());
        }

        // Reading content of .prj file, or extracting it from CAD if not present
        nLayers = 0;
        // FIXME: We allocate extra memory, do we need more strict policy here?
        papoLayers = static_cast<OGRCADLayer**>( CPLMalloc( sizeof( OGRCADLayer* ) *
                                                  poCADFile->GetLayersCount() ) );

        int nEncoding = GetCadEncoding();
        for( size_t i = 0; i < poCADFile->GetLayersCount(); ++i )
        {
            CADLayer &oLayer = poCADFile->GetLayer( i );
            if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR &&
                oLayer.getGeometryCount() > 0 )
            {
                papoLayers[nLayers++] = new OGRCADLayer( oLayer, poSpatialRef,
                                                         nEncoding);
            }

            if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
            {
                for( size_t j = 0; j < oLayer.getImageCount(); ++j )
                {
                    nSubRasterLayer = static_cast<long>( i );
                    nSubRasterFID = static_cast<long>( j );
                    GDALDataset::SetMetadataItem( CPLSPrintf("SUBDATASET_%d_NAME",
                        nRasters),
                        CPLSPrintf("CAD:%s:%ld:%ld", osCADFilename.c_str(),
                            nSubRasterLayer, nSubRasterFID), "SUBDATASETS" );
                    GDALDataset::SetMetadataItem( CPLSPrintf("SUBDATASET_%d_DESC",
                        nRasters),
                        CPLSPrintf("%s - %ld", oLayer.getName().c_str(),
                                    nSubRasterFID), "SUBDATASETS" );
                    nRasters++;
                }
            }
        }
        // If nRasters == 2 we have the only one raster in CAD file
    }

    // The only one raster layer in dataset is present or subdataset is request
    if( nRasters == 2 )
    {
        CADLayer &oLayer = poCADFile->GetLayer( nSubRasterLayer );
        CADImage* pImage = oLayer.getImage( nSubRasterFID );
        if( pImage )
        {
            // TODO: Add support clipping region in neatline
            CPLString osImgFilename = pImage->getFilePath();
            CPLString osImgPath = CPLGetPath( osImgFilename );
            if( osImgPath.empty () )
            {
                osImgFilename = CPLFormFilename( CPLGetPath( osCADFilename ),
                                                 osImgFilename, nullptr );
            }

            if ( !CPLCheckForFile( const_cast<char *>( osImgFilename.c_str () ), nullptr) )
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;

            poRasterDS = reinterpret_cast<GDALDataset *>(
                                GDALOpen( osImgFilename, poOpenInfo->eAccess ) );
            if( poRasterDS == nullptr )
            {
                delete pImage;
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;
            }
            if( poRasterDS->GetRasterCount() == 0 )
            {
                delete pImage;
                GDALClose( poRasterDS );
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;
            }

            if( poRasterDS->GetGeoTransform( adfGeoTransform ) != CE_None )
            {
                // The external world file have priority
                double dfUnits = 1.0;
                if( nullptr != poSpatialRef )
                    dfUnits = poSpatialRef->GetLinearUnits();
                FillTransform( pImage, dfUnits );
            }
            delete pImage;

            nRasterXSize = poRasterDS->GetRasterXSize();
            nRasterYSize = poRasterDS->GetRasterYSize();
            if( !GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) )
            {
                GDALClose( poRasterDS );
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;
            }

            for( int iBand = 1; iBand <= poRasterDS->GetRasterCount(); iBand++ )
                SetBand( iBand,
                    new CADWrapperRasterBand( poRasterDS->GetRasterBand(
                        iBand )) );

            char** papszDomainList = poRasterDS->GetMetadataDomainList();
            while( papszDomainList )
            {
                char** papszMetadata = GetMetadata( *papszDomainList );
                char** papszRasterMetadata = poRasterDS->GetMetadata( *papszDomainList );
                if( nullptr == papszMetadata )
                    SetMetadata( papszRasterMetadata, *papszDomainList );
                else
                {
                    char** papszMD = CSLMerge( CSLDuplicate(papszMetadata), papszRasterMetadata );
                    SetMetadata( papszMD, *papszDomainList );
                    CSLDestroy( papszMD );
                }
                papszDomainList++;
            }
        }
    }

    return TRUE;
}

OGRLayer *GDALCADDataset::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

int GDALCADDataset::TestCapability( const char * pszCap )
{
    if ( EQUAL( pszCap,ODsCCreateLayer ) || EQUAL( pszCap,ODsCDeleteLayer ) )
         return FALSE;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    return FALSE;
}

char** GDALCADDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    /* duplicated papszFileList = CSLAddString( papszFileList, osCADFilename );*/
    const char * pszPRJFilename = GetPrjFilePath();
    if( nullptr != pszPRJFilename )
        papszFileList = CSLAddString( papszFileList, pszPRJFilename );

    for( size_t i = 0; i < poCADFile->GetLayersCount(); ++i )
    {
        CADLayer &oLayer = poCADFile->GetLayer( i );
        for( size_t j = 0; j < oLayer.getImageCount(); ++j )
        {
            CADImage* pImage = oLayer.getImage( j );
            if( pImage )
            {
                CPLString osImgFilename = pImage->getFilePath();
                if ( CPLCheckForFile (const_cast<char *>( osImgFilename.c_str()),
                        nullptr) == TRUE)
                    papszFileList = CSLAddString( papszFileList, osImgFilename );
            }
        }
    }

    if( nullptr != poRasterDS )
    {
        papszFileList = CSLMerge( papszFileList, poRasterDS->GetFileList() );
    }
    return papszFileList;
}

int GDALCADDataset::GetCadEncoding() const
{
    if( poCADFile == nullptr )
        return 0;
    const CADHeader & header = poCADFile->getHeader();
    return static_cast<int>( header.getValue(
                                        CADHeader::DWGCODEPAGE, 0 ).getDecimal() );
}

OGRSpatialReference *GDALCADDataset::GetSpatialReference()
{
    if( poSpatialReference )
        return poSpatialReference;

    if( poCADFile != nullptr )
    {
        CPLString sESRISpatRef;
        poSpatialReference = new OGRSpatialReference();
        poSpatialReference->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        CADDictionary oNOD = poCADFile->GetNOD();
        CPLString sESRISpatRefData = oNOD.getRecordByName("ESRI_PRJ");
        if( !sESRISpatRefData.empty() )
        {
            sESRISpatRef = sESRISpatRefData.substr( sESRISpatRefData.find("GEO") );
        }

        if( !sESRISpatRef.empty() )
        {
            char** papszPRJData = nullptr;
            papszPRJData = CSLAddString( papszPRJData, sESRISpatRef );
            if( poSpatialReference->importFromESRI( papszPRJData ) != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to parse PRJ section, ignoring." );
                delete poSpatialReference;
                poSpatialReference = nullptr;
            }

            CSLDestroy( papszPRJData );
        }
        else
        {
            const char * pszPRJFilename = GetPrjFilePath();
            if( pszPRJFilename && pszPRJFilename[0] ) // check if path exists
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                char **papszPRJData = CSLLoad( pszPRJFilename );
                CPLPopErrorHandler();

                if( poSpatialReference->importFromESRI( papszPRJData ) != OGRERR_NONE )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to parse PRJ file, ignoring." );
                    delete poSpatialReference;
                    poSpatialReference = nullptr;
                }

                if( papszPRJData )
                    CSLDestroy( papszPRJData );
            }
        }
    }

    if( poSpatialReference )
    {
        char *pszProjection = nullptr;
        poSpatialReference->exportToWkt( &pszProjection );
        soWKT = pszProjection;
        CPLFree( pszProjection );
    }
    return poSpatialReference;
}

const char* GDALCADDataset::GetPrjFilePath()
{
    const char * pszPRJFilename = CPLResetExtension( osCADFilename, "prj" );
    if ( CPLCheckForFile( (char*)pszPRJFilename, nullptr ) == TRUE )
        return pszPRJFilename;

    pszPRJFilename = CPLResetExtension( osCADFilename, "PRJ" );
    if ( CPLCheckForFile( (char*)pszPRJFilename, nullptr ) == TRUE )
        return pszPRJFilename;

    return "";
}

const char *GDALCADDataset::_GetProjectionRef(void)
{
    return soWKT;
}

CPLErr GDALCADDataset::GetGeoTransform( double* padfGeoTransform )
{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

int GDALCADDataset::GetGCPCount()
{
    if(nullptr == poRasterDS)
        return 0;
    return poRasterDS->GetGCPCount();
}

const OGRSpatialReference *GDALCADDataset::GetGCPSpatialRef() const
{
    if( nullptr == poRasterDS )
        return nullptr;
    return poRasterDS->GetGCPSpatialRef();
}

const GDAL_GCP *GDALCADDataset::GetGCPs()
{
    if( nullptr == poRasterDS )
        return nullptr;
    return poRasterDS->GetGCPs();
}

int GDALCADDataset::CloseDependentDatasets()
{
    int bRet = GDALDataset::CloseDependentDatasets();
    if ( poRasterDS != nullptr )
    {
        GDALClose( poRasterDS );
        poRasterDS = nullptr;
        bRet = TRUE;
    }
    return bRet;
}
