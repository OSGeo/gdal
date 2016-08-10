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
 *  Copyright (c) 2016, NextGIS
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
#include "ogr_cad.h"
#include "cpl_conv.h"
#include "gdal_proxy.h"
#include "gdal_pam.h"

#include "vsilfileio.h"

class CADWrapperRasterBand : public GDALProxyRasterBand
{
  GDALRasterBand* poBaseBand;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() { return poBaseBand; }

  public:
    explicit CADWrapperRasterBand( GDALRasterBand* poBaseBandIn )
    {
        this->poBaseBand = poBaseBandIn;
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }
    ~CADWrapperRasterBand() {}
};

GDALCADDataset::GDALCADDataset() : poCADFile(NULL), papoLayers(NULL), nLayers(0),
    poRasterDS(NULL)
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
    if (poRasterDS != NULL)
    {
        GDALClose( poRasterDS );
        poRasterDS = NULL;
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    if(poCADFile)
        delete( poCADFile );
}

void GDALCADDataset::FillTransform(CADImage* pImage, double dfUnits)
{
    CADImage::ResolutionUnit eResUnits = pImage->getResolutionUnits();
    double dfMultiply(1);

    switch(eResUnits)// 0 == none, 2 == centimeters, 5 == inches;
    {
        case CADImage::ResolutionUnit::CENTIMETER:
            dfMultiply = 100 / dfUnits; // meters to linear units
            break;
        case CADImage::ResolutionUnit::INCH: 
            dfMultiply = 0.0254 / dfUnits;   
            break;
        case CADImage::ResolutionUnit::NONE:
        default:
            dfMultiply = 1;
    }

    CADVector oSizePt = pImage->getImageSizeInPx();
    CADVector oInsPt = pImage->getVertInsertionPoint();
    CADVector osSizeUnitsPt = pImage->getPixelSizeInACADUnits();
    adfGeoTransform[0] = oInsPt.getX();
    adfGeoTransform[3] = oInsPt.getY() + oSizePt.getY() * osSizeUnitsPt.getX() * dfMultiply;
    adfGeoTransform[2] = 0;
    adfGeoTransform[4] = 0;

    adfGeoTransform[1] = osSizeUnitsPt.getX() * dfMultiply;
    adfGeoTransform[5] = -osSizeUnitsPt.getY() * dfMultiply;
}

int GDALCADDataset::Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO,
                                    long nSubRasterLayer, long nSubRasterFID )
{
    size_t i, j;
    int nRasters = 1;

    osCADFilename = pFileIO->GetFilePath();
    SetDescription( poOpenInfo->pszFilename );

    const char * papszOpenOptions = CSLFetchNameValueDef( poOpenInfo->papszOpenOptions, "MODE", 
                                                                           "READ_FAST");
    /* const char * papszReadUnsupportedGeoms = CSLFetchNameValueDef( 
                poOpenInfo->papszOpenOptions, "ADD_UNSUPPORTED_GEOMETRIES_DATA",
                "NO"); */

    enum CADFile::OpenOptions openOpts = CADFile::READ_FAST;
    if( !strcmp( papszOpenOptions, "READ_ALL" ) )
    {
        openOpts = CADFile::READ_ALL;
    }
    else if( !strcmp( papszOpenOptions, "READ_FASTEST" ) )
    {
        openOpts = CADFile::READ_FASTEST;
    }

    poCADFile = OpenCADFile( pFileIO, openOpts );

    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n"
                  "Supported formats are:\n%s", GetVersionString(), GetCADFormats() );
        return( FALSE );
    }

    if ( GetLastErrorCode() != CADErrorCodes::SUCCESS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\nSupported formats: %s", 
                  GetVersionString(), GetCADFormats() );
        return( FALSE );
    }

    OGRSpatialReference *poSpatialRef = GetSpatialReference( );

    if( nSubRasterLayer != -1 && nSubRasterFID != -1 )
    {
        // indicate that subdataset from CAD layer number nSubRasterLayer and 
        // FID nSubRasterFID is request
        nRasters = 2;
    }
    else
    {
        // fill metadata
        const CADHeader& header = poCADFile->getHeader();
        for(i = 0; i < header.getSize(); ++i)
        {
            short nCode = header.getCode(static_cast<int>(i));
            const CADVariant& oVal = header.getValue(nCode);
            GDALDataset::SetMetadataItem(header.getValueName(nCode), oVal.getString().c_str());
        }

        // Reading content of .prj file, or extracting it from CAD if not present
        nLayers = 0;
        // FIXME: we allocate extra memory, do we need more strict policy here?
        papoLayers = ( OGRCADLayer** ) CPLMalloc(sizeof(OGRCADLayer*) * 
                                                poCADFile->getLayersCount());

        for(i = 0; i < poCADFile->getLayersCount(); ++i)
        {
            CADLayer &oLayer = poCADFile->getLayer( i );
            if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR && oLayer.getGeometryCount() > 0)
            {
                papoLayers[nLayers++] = new OGRCADLayer( oLayer, poSpatialRef );
            }

            if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
            {
                for( j = 0; j < oLayer.getImageCount(); ++j )
                {
                    nSubRasterLayer = i;
                    nSubRasterFID = j;
                    GDALDataset::SetMetadataItem(CPLSPrintf("SUBDATASET_%d_NAME", nRasters),
                        CPLSPrintf("CAD:%s:%ld:%ld", osCADFilename.c_str(), i, j), 
                        "SUBDATASETS");
                    GDALDataset::SetMetadataItem(CPLSPrintf("SUBDATASET_%d_DESC", nRasters),
                    CPLSPrintf("%s - %ld", oLayer.getName().c_str(), j), 
                        "SUBDATASETS");
                    nRasters++;
                }
            }
        }
        // if nRasters == 2 we have the only one raster in CAD file
    }

    // the only one raster layer in dataset is present or subdataset is request
    if( nRasters == 2 )
    {
        CADLayer &oLayer = poCADFile->getLayer( nSubRasterLayer );
        CADImage* pImage = oLayer.getImage(nSubRasterFID);
        if(pImage)
        {
            // TODO: add support clipping region in neatline
            CPLString osImgFilename = pImage->getFilePath();
            CPLString osImgPath = CPLGetPath(osImgFilename);
            if(osImgPath.empty ())
            {
                osImgFilename = CPLFormFilename(CPLGetPath(osCADFilename),
                                                osImgFilename, NULL);
            }

            if ( CPLCheckForFile ((char*)osImgFilename.c_str (), NULL) == FALSE)
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;

            poRasterDS = reinterpret_cast<GDALDataset *>(
                                GDALOpen( osImgFilename, poOpenInfo->eAccess ) );
            if(poRasterDS == NULL)
            {
                delete pImage;
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;
            }
            if(poRasterDS->GetRasterCount() == 0)
            {
                delete pImage;
                GDALClose( poRasterDS );
                return poOpenInfo->nOpenFlags & GDAL_OF_VECTOR;
            }

            if(poRasterDS->GetGeoTransform(adfGeoTransform) != CE_None)
            {
                // external world file have priority
                double dfUnits = 1;
                if(NULL != poSpatialRef)
                    dfUnits = poSpatialRef->GetLinearUnits();
                FillTransform(pImage, dfUnits);
            }
            delete pImage;

            nRasterXSize = poRasterDS->GetRasterXSize();
            nRasterYSize = poRasterDS->GetRasterYSize();
            if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
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
                char** papszMetadata = GetMetadata(*papszDomainList);
                char** papszRasterMetadata = poRasterDS->GetMetadata(*papszDomainList);
                if(NULL == papszMetadata)
                    SetMetadata(papszRasterMetadata, *papszDomainList);
                else
                    papszMetadata = CSLMerge(papszMetadata, papszRasterMetadata);    
                papszDomainList++;
            }
        }
    }

    return TRUE;
}

OGRLayer *GDALCADDataset::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return( NULL );
    else
        return( papoLayers[iLayer] );
}

int GDALCADDataset::TestCapability( const char * pszCap )
{
    if ( EQUAL(pszCap,ODsCCreateLayer) ||
         EQUAL(pszCap,ODsCDeleteLayer) )
    {
         return FALSE;
    }
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    return FALSE;
}

char** GDALCADDataset::GetFileList()
{
    size_t i, j;
    char **papszFileList = GDALDataset::GetFileList();

    /* duplicated papszFileList = CSLAddString( papszFileList, osCADFilename );*/
    const char * pszPRJFilename = GetPrjFilePath();
    if(NULL != pszPRJFilename)
        papszFileList = CSLAddString( papszFileList, pszPRJFilename );

    for(i = 0; i < poCADFile->getLayersCount(); ++i)
    {
        CADLayer &oLayer = poCADFile->getLayer( i );
        for( j = 0; j < oLayer.getImageCount(); ++j )
        {
            CADImage* pImage = oLayer.getImage(j);
            if(pImage)
            {
                CPLString osImgFilename = pImage->getFilePath(); 
                if ( CPLCheckForFile ((char*)osImgFilename.c_str(), NULL) == TRUE)
                    papszFileList = CSLAddString( papszFileList, osImgFilename );
            }
        }
    }

    if(NULL != poRasterDS)
    {
        papszFileList = CSLMerge(papszFileList, poRasterDS->GetFileList());
    }
    return papszFileList;
}

OGRSpatialReference *GDALCADDataset::GetSpatialReference()
{
    OGRSpatialReference *poSpatialRef = NULL;
    if( poCADFile != NULL )
    {
        CPLString sESRISpatRef;
        poSpatialRef = new OGRSpatialReference();
        CADDictionary oNOD = poCADFile->getNOD();
        for( size_t i = 0; i < oNOD.getRecordsCount(); ++i )
        {
            if( !strcmp( oNOD.getRecord(i).first.c_str(), "ESRI_PRJ" ) )
            {
                CADXRecord * poXRecord = ( CADXRecord* ) oNOD.getRecord(i).second;
                size_t dDataBegins = poXRecord->getRecordData().find("GEO");

                std::string sESRISpatRefData( poXRecord->getRecordData().begin() + dDataBegins,
                                              poXRecord->getRecordData().end() );
                sESRISpatRef = sESRISpatRefData;
                delete( poXRecord );
        }
    }

        if( !sESRISpatRef.empty() )
        {
            char** papszPRJData = NULL;
            papszPRJData = CSLAddString(papszPRJData, sESRISpatRef);
            if( poSpatialRef->importFromESRI( papszPRJData ) != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to parse PRJ section, ignoring." );
                delete( poSpatialRef );
                poSpatialRef = NULL;
            }

            CSLDestroy( papszPRJData );
        }
        else
        {
            const char * pszPRJFilename = GetPrjFilePath();
            if(NULL != pszPRJFilename)
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                char **papszPRJData = CSLLoad(pszPRJFilename);
                CPLPopErrorHandler();

                if( poSpatialRef->importFromESRI( papszPRJData ) != OGRERR_NONE )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to parse PRJ section, ignoring." );
                    delete( poSpatialRef );
                    poSpatialRef = NULL;
                }

                if( papszPRJData )
                    CSLDestroy( papszPRJData );
            }
        }
    }

    char *pszProjection = NULL;
    poSpatialRef->exportToWkt( &pszProjection );
    soWKT = pszProjection;
    CPLFree( pszProjection );
    return poSpatialRef;
}

const char* GDALCADDataset::GetPrjFilePath()
{
    const char * pszPRJFilename = CPLResetExtension(osCADFilename, "prj");
    if ( CPLCheckForFile ((char*)pszPRJFilename, NULL) == TRUE)
        return pszPRJFilename;

    pszPRJFilename = CPLResetExtension(osCADFilename, "PRJ");
    if ( CPLCheckForFile ((char*)pszPRJFilename, NULL) == TRUE)
        return pszPRJFilename;

    return NULL;
}

const char *GDALCADDataset::GetProjectionRef(void)
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
    if(NULL == poRasterDS)
        return 0;
    return poRasterDS->GetGCPCount();
}

const char *GDALCADDataset::GetGCPProjection()
{
    if(NULL == poRasterDS)
        return "";
    return poRasterDS->GetGCPProjection();
}

const GDAL_GCP *GDALCADDataset::GetGCPs()
{
    if(NULL == poRasterDS)
        return NULL;
    return poRasterDS->GetGCPs();
}

int GDALCADDataset::CloseDependentDatasets()
{
    int bRet = GDALDataset::CloseDependentDatasets();
    if (poRasterDS != NULL)
    {
        GDALClose( poRasterDS );
        poRasterDS = NULL;
        bRet = TRUE;
    }
    return bRet;
}
