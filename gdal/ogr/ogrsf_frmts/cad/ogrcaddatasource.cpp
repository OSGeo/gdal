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

OGRCADDataSource::OGRCADDataSource() : papoLayers(NULL),
    nLayers(0), poCADFile(NULL)
{

}

OGRCADDataSource::~OGRCADDataSource()
{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    if(poCADFile)
        delete( poCADFile );
}


int OGRCADDataSource::Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO )
{
    size_t i, j;
    SetDescription( poOpenInfo->pszFilename );
   
    if ( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                 "Update access not supported by CAD Driver." );
        return( FALSE );
    }
    
    CPLString osFilename( poOpenInfo->pszFilename );
    short nSubdatasetIndex = -1, nSubdatasetTableIndex = -1;    
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "CAD:") )
    {
        char** papszTokens = CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        if( CSLCount(papszTokens) != 4 )
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }

        osFilename = papszTokens[1];
        nSubdatasetTableIndex = atoi(papszTokens[2]);
        nSubdatasetIndex = atoi(papszTokens[3]);

        CSLDestroy(papszTokens);
    }
    
    poCADFile = OpenCADFile( pFileIO, CADFile::OpenOptions::READ_FAST );

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
    
    CPLString sESRISpatRef = poCADFile->getESRISpatialRef();
    if(sESRISpatRef.empty())
    {
        // TODO: do we need *.PRJ too?
        const char * pszPRJFilename = CPLResetExtension(poOpenInfo->pszFilename, "prj");
        CPLPushErrorHandler( CPLQuietErrorHandler );
        char **cabyPRJData = CSLLoad(pszPRJFilename);
        CPLPopErrorHandler();
        if( cabyPRJData && CSLCount(cabyPRJData) > 0 )
        {
            sESRISpatRef = cabyPRJData[0];
        }
        
        if(cabyPRJData)
            CSLDestroy( cabyPRJData );
    }  
    
    // get raster by index
    if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER && nSubdatasetIndex > -1 && 
        nSubdatasetTableIndex > -1)
    {
        CADLayer &oLayer = poCADFile->getLayer(nSubdatasetTableIndex);
        CADImage* pImage = oLayer.getImage(nSubdatasetIndex);
        if(pImage)
        {
            // TODO: open raster
        
            delete pImage;
            return TRUE;
        }
        
        return FALSE;
    } 
    
    nLayers = 0;
    int nRasters = 0;
    // FIXME: we allocate extra data, do we need more strict policy here?
    papoLayers = ( OGRCADLayer** ) CPLMalloc(sizeof(OGRCADLayer*) * 
                                                poCADFile->getLayersCount());

    for(i = 0; i < poCADFile->getLayersCount(); ++i)
    {
        CADLayer &oLayer = poCADFile->getLayer( i );
        if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR && oLayer.getGeometryCount() > 0)
        {
            papoLayers[nLayers++] = new OGRCADLayer( oLayer, sESRISpatRef );
        }

        if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
        {
            //DEBUG: CPLError( CE_Failure, CPLE_NotSupported, "Layer %d, raster count %d, vector count %d", i, oLayer.getImageCount(), oLayer.getGeometryCount());      
            for( j = 0; j < oLayer.getImageCount(); ++j )
            {
                SetMetadataItem(CPLSPrintf("SUBDATASET_%d_NAME", nRasters),
                    CPLSPrintf("CAD:%s:%ld:%ld", poOpenInfo->pszFilename, i, j), 
                        "SUBDATASETS");
                SetMetadataItem(CPLSPrintf("SUBDATASET_%d_DESC", nRasters),
                    CPLSPrintf("%s - %ld", oLayer.getName().c_str(), j), 
                        "SUBDATASETS");
                CPLError( CE_Failure, CPLE_NotSupported, "Add raster %d",  nRasters);      
                        
                nRasters++;
            }
        }
    }   
    
    return( TRUE );
}

OGRLayer *OGRCADDataSource::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return( NULL );
    else
        return( papoLayers[iLayer] );
}

int OGRCADDataSource::TestCapability( const char * pszCap )
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

