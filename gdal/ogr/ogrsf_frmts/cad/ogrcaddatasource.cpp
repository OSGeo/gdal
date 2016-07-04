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

OGRCADDataSource::OGRCADDataSource()
{
    papoLayers = nullptr;
    nLayers    = 0;
}

OGRCADDataSource::~OGRCADDataSource()
{
    for( size_t i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}


int OGRCADDataSource::Open( GDALOpenInfo* poOpenInfo )
{
    SetDescription( poOpenInfo->pszFilename );
   
    if ( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                 "Update access not supported by CAD Driver." );
        return( FALSE );
    }

    spoCADFile = std::unique_ptr<CADFile>(
                                          OpenCADFile( poOpenInfo->pszFilename, CADFile::OpenOptions::READ_FAST ) );

    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n"
                  "Supported formats are:\n%s", GetVersionString(), GetCADFormats() );
        return( FALSE );
    }
    
    // fill metadata
    //spoCADFile
    
    // get layers

    nLayers = spoCADFile->getLayersCount();
    papoLayers = ( OGRCADLayer** ) CPLMalloc( sizeof( void* ) );

    // Reading content of .prj file, or extracting it from CAD if not present
    std::string sESRISpatRef = "";
    size_t nFilenameLength = strlen( poOpenInfo->pszFilename );
    char * pszPRJFilename = new char( nFilenameLength );
    pszPRJFilename[nFilenameLength-1] = 'j';
    pszPRJFilename[nFilenameLength-2] = 'r';
    pszPRJFilename[nFilenameLength-3] = 'p';
    VSILFILE *fpPRJ = VSIFOpenL( pszPRJFilename, "r" );
    if( fpPRJ != NULL )
    {
        const char * cabyPRJData = CPLReadLineL( fpPRJ );
        for( size_t i = 0; i < strlen(cabyPRJData); ++i )
        {
            sESRISpatRef.push_back( cabyPRJData[i] );
        }
    }
    else // extract .prj from CAD
    {
        sESRISpatRef = spoCADFile->getESRISpatialRef();
    }
    

    for ( size_t iIndex = 0; iIndex < nLayers; ++iIndex )
    {
        papoLayers[iIndex] = new OGRCADLayer( spoCADFile->getLayer( iIndex ), sESRISpatRef );
    }
    
    return( TRUE );
}

OGRLayer *OGRCADDataSource::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return( nullptr );
    else
        return papoLayers[iLayer];
}
