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

#include "vsilfileio.h"

GDALCADDataset::GDALCADDataset() : papoLayers(NULL),
    nLayers(0), poCADFile(NULL)
{

}

GDALCADDataset::~GDALCADDataset()
{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    if(poCADFile)
        delete( poCADFile );
}

GDALDataset *GDALCADDataset::OpenRaster( const char * pszOpenPath )
{
    short nSubdatasetIndex = -1, nSubdatasetTableIndex = -1;    
    char** papszTokens = CSLTokenizeString2(pszOpenPath, ":", 0);
    if( CSLCount(papszTokens) != 4 )
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    
    osCADFilename = papszTokens[1];
    nSubdatasetTableIndex = atoi(papszTokens[2]);
    nSubdatasetIndex = atoi(papszTokens[3]);

    CSLDestroy(papszTokens);
    
    poCADFile = OpenCADFile( new VSILFileIO(osCADFilename), CADFile::OpenOptions::READ_FAST );
    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n"
                  "Supported formats are:\n%s", GetVersionString(), GetCADFormats() );
        return NULL;
    }
    
    if ( GetLastErrorCode() != CADErrorCodes::SUCCESS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n", 
                  GetVersionString(), GetCADFormats() );
        return NULL;
    }
    
    // Reading content of .prj file, or extracting it from CAD if not present
    OGRSpatialReference oSRS;
    // FIXME: set oSRS from cad file
    CPLString sESRISpatRef = poCADFile->getESRISpatialRef();
    if(sESRISpatRef.empty())
    {
        // TODO: do we need *.PRJ too?
        const char * pszPRJFilename = CPLResetExtension(osCADFilename, "prj");
        CPLPushErrorHandler( CPLQuietErrorHandler );
        char **cabyPRJData = CSLLoad(pszPRJFilename);
        CPLPopErrorHandler();
        oSRS.importFromESRI(cabyPRJData);
        
        if(cabyPRJData)
            CSLDestroy( cabyPRJData );
    }
    
    // get raster by index
    if( nSubdatasetIndex > -1 && nSubdatasetTableIndex > -1)
    {
        CADLayer &oLayer = poCADFile->getLayer(nSubdatasetTableIndex);
        CADImage* pImage = oLayer.getImage(nSubdatasetIndex);
        if(pImage)
        {
            GDALDataset* poRasterDataset = NULL;
            
            // TODO: add support clipping region
            CPLString osImgFilename = pImage->getFilePath();
            poRasterDataset = reinterpret_cast<GDALDataset *>(
                                        GDALOpen( osImgFilename, GA_ReadOnly ) );
            if(poRasterDataset == NULL)
            {
                return NULL;
            }
            if(poRasterDataset->GetRasterCount() == 0)
            {
                delete poRasterDataset;
                return NULL;
            }
            
            char *pszWKT = NULL; 
            oSRS.exportToWkt( &pszWKT );
            poRasterDataset->SetProjection( pszWKT );
            CPLFree( pszWKT );
            
            double adfGeoTransform[6];
            CADVector oInsPt = pImage->getVertInsertionPoint();
            adfGeoTransform[0] = oInsPt.getX();
            adfGeoTransform[3] = oInsPt.getY();
            adfGeoTransform[2] = 0;
            adfGeoTransform[4] = 0;
            /* not used CADVector oSizePt = pImage->getImageSizeInPx(); */
            unsigned char nResUnits = pImage->getResolutionUnits();
            CADVector osSizeUnitsPt = pImage->getPixelSizeInACADUnits();
            double dfMultiply(1);
            
            switch(nResUnits)// 0 == none, 2 == centimeters, 5 == inches;
            {
                case 2:
                    dfMultiply = 100 / oSRS.GetLinearUnits(); // meters to linear units
                case 5: 
                    dfMultiply = 0.0254 / oSRS.GetLinearUnits();   
                case 0:                
                default:
                    dfMultiply = 1;
            }
            
            adfGeoTransform[1] = osSizeUnitsPt.getX() * dfMultiply;
            adfGeoTransform[5] = -osSizeUnitsPt.getY() * dfMultiply;
            
            poRasterDataset->SetGeoTransform (adfGeoTransform);

            delete pImage;
            return poRasterDataset;
        }
    } 
    return NULL;
}


int GDALCADDataset::Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO )
{
    size_t i, j;
    SetDescription( poOpenInfo->pszFilename );
    osCADFilename = poOpenInfo->pszFilename;
    
    int nMode = atoi(CSLFetchNameValueDef( poOpenInfo->papszOpenOptions, "MODE", "2"));
    
    enum CADFile::OpenOptions openOpts = CADFile::OpenOptions::READ_FAST;
    if(nMode == 1)
    {
        openOpts = CADFile::OpenOptions::READ_ALL;
    }
    else if(nMode == 3)
    {
        openOpts = CADFile::OpenOptions::READ_FASTEST;
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
                  "libopencad %s does not support this version of CAD file.\n", 
                  GetVersionString(), GetCADFormats() );
        return( FALSE );
    }
    

    // fill metadata
    const CADHeader& header = poCADFile->getHeader();
    for(i = 0; i < header.getSize(); ++i)
    {
        short nCode = header.getCode(i);
        const CADVariant& oVal = header.getValue(nCode);
        SetMetadataItem(header.getValueName(nCode), oVal.getString().c_str());
    }

    // Reading content of .prj file, or extracting it from CAD if not present
    OGRSpatialReference *poSpatialRef = GetSpatialReference( poOpenInfo->pszFilename );
    
    nLayers = 0;
    int nRasters = 1;
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
                SetMetadataItem(CPLSPrintf("SUBDATASET_%d_NAME", nRasters),
                    CPLSPrintf("CAD:%s:%ld:%ld", poOpenInfo->pszFilename, i, j), 
                        "SUBDATASETS");
                SetMetadataItem(CPLSPrintf("SUBDATASET_%d_DESC", nRasters),
                    CPLSPrintf("%s - %ld", oLayer.getName().c_str(), j), 
                        "SUBDATASETS");     
                        
                nRasters++;
            }
        }
    }   
    
    return( TRUE );
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
    int i, j;
    char **papszFileList = GDALDataset::GetFileList();

    /* duplicated papszFileList = CSLAddString( papszFileList, osCADFilename );*/
    const char * pszPRJFilename = CPLResetExtension(osCADFilename, "prj");
    if ( CPLCheckForFile ((char*)pszPRJFilename, NULL) == TRUE)
        papszFileList = CSLAddString( papszFileList, pszPRJFilename );
    else
    {
        pszPRJFilename = CPLResetExtension(osCADFilename, "PRJ");
        if ( CPLCheckForFile ((char*)pszPRJFilename, NULL) == TRUE)
            papszFileList = CSLAddString( papszFileList, pszPRJFilename );
    }    
    
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
    
    return papszFileList;
}


OGRSpatialReference *GDALCADDataset::GetSpatialReference(const char * const pszFilename)
{
    OGRSpatialReference *poSpatialRef = NULL;
    if( poCADFile != NULL )
    {
        poSpatialRef = new OGRSpatialReference();
        CPLString sESRISpatRef = poCADFile->getESRISpatialRef();

        if( !sESRISpatRef.empty() )
        {
            char** papszPRJData = new char*[1];
            papszPRJData[0] = new char[sESRISpatRef.size()];
            std::copy( sESRISpatRef.begin(), sESRISpatRef.end(), papszPRJData[0] ); // FIXME: pretty dirty trick

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
            const char * pszPRJFilename = CPLResetExtension(pszFilename, "prj");
            CPLPushErrorHandler( CPLQuietErrorHandler );
            char **cabyPRJData = CSLLoad(pszPRJFilename);
            CPLPopErrorHandler();

            if( poSpatialRef->importFromESRI( cabyPRJData ) != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to parse PRJ section, ignoring." );
                delete( poSpatialRef );
                poSpatialRef = NULL;
            }

            if(cabyPRJData)
                CSLDestroy( cabyPRJData );
        }
    }

    return poSpatialRef;
}
