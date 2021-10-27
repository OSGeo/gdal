/*******************************************************************************
 *  Project: libopencad_api.cpp
 *  Purpose: libOpenCAD OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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
#include "opencad_api.h"
#include "cadfilestreamio.h"
#include "dwg/r2000.h"

#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <iostream>

static int gLastError = CADErrorCodes::SUCCESS;

/**
 * @brief Check CAD file
 * @param pCADFileIO CAD file reader pointer owned by function
 * @return returns and int, 0 if CAD file has unsupported format
 */
static int CheckCADFile(CADFileIO * pCADFileIO)
{
    if( pCADFileIO == nullptr )
        return 0;

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(OPENCAD_DISABLE_EXTENSION_CHECK)
    const char * pszFilePath = pCADFileIO->GetFilePath();
    size_t nPathLen = strlen( pszFilePath );

    if( nPathLen > 3 &&
        toupper( pszFilePath[nPathLen - 3] ) == 'D' &&
        toupper( pszFilePath[nPathLen - 2] ) == 'X' &&
        toupper( pszFilePath[nPathLen - 1] ) == 'F' )
    {
        //TODO: "AutoCAD Binary DXF"
        //std::cerr << "DXF ASCII and binary is not supported yet.";
        return 0;
    }
    if( ! ( nPathLen > 3 &&
            toupper( pszFilePath[nPathLen - 3] ) == 'D' &&
            toupper( pszFilePath[nPathLen - 2] ) == 'W' &&
            toupper( pszFilePath[nPathLen - 1] ) == 'G' ) )
    {
        return 0;
    }
#endif

    if( !pCADFileIO->IsOpened() )
        pCADFileIO->Open( CADFileIO::OpenMode::in | CADFileIO::OpenMode::binary );
    if( !pCADFileIO->IsOpened() )
        return 0;

    char pabyDWGVersion[DWG_VERSION_STR_SIZE + 1] = { 0 };
    pCADFileIO->Rewind ();
    pCADFileIO->Read( pabyDWGVersion, DWG_VERSION_STR_SIZE );
    return atoi( pabyDWGVersion + 2 );
}

/**
 * @brief Open CAD file
 * @param pCADFileIO CAD file reader pointer ownd by function
 * @param eOptions Open options
 * @param bReadUnsupportedGeometries Unsupported geoms will be returned as CADUnknown
 * @return CADFile pointer or NULL if failed. The pointer have to be freed by user
 */
CADFile * OpenCADFile( CADFileIO * pCADFileIO, enum CADFile::OpenOptions eOptions, bool bReadUnsupportedGeometries )
{
    int nCADFileVersion = CheckCADFile( pCADFileIO );
    CADFile * poCAD = nullptr;

    switch( nCADFileVersion )
    {
        case CADVersions::DWG_R2000:
            poCAD = new DWGFileR2000( pCADFileIO );
            break;
        default:
            gLastError = CADErrorCodes::UNSUPPORTED_VERSION;
            delete pCADFileIO;
            return nullptr;
    }

    gLastError = poCAD->ParseFile( eOptions, bReadUnsupportedGeometries );
    if( gLastError != CADErrorCodes::SUCCESS )
    {
        delete poCAD;
        return nullptr;
    }

    return poCAD;
}


/**
 * @brief Get library version number as major * 10000 + minor * 100 + rev
 * @return library version number
 */
int GetVersion()
{
    return OCAD_VERSION_NUM;
}

/**
 * @brief Get library version string
 * @return library version string
 */
const char * GetVersionString()
{
    return OCAD_VERSION;
}

/**
 * @brief Get last error code
 * @return last error code
 */
int GetLastErrorCode()
{
    return gLastError;
}

/**
 * @brief GetDefaultFileIO return default file in/out class.
 * @param pszFileName CAD file path
 * @return CADFileIO pointer or null if error. The pointer have to be freed by
 * user
 */
CADFileIO* GetDefaultFileIO( const char * pszFileName )
{
    return new CADFileStreamIO( pszFileName );
}

/**
 * @brief IdentifyCADFile
 * @param pCADFileIO pointer to file in/out class
 * @return positive number for dwg version, negative for dxf version, 0 if error
 * occurred
 */
int IdentifyCADFile( CADFileIO * pCADFileIO, bool bOwn )
{
    int result = CheckCADFile(pCADFileIO);
    if(bOwn)
    {
        delete pCADFileIO;
    }
    return result;
}

/**
 * @brief List supported CAD Formats
 * @return String describes supported CAD formats
 */
const char * GetCADFormats()
{
    return "DWG R2000 [ACAD1015]\n";
}

/**
 * @brief Open CAD file
 * @param pszFileName Path to CAD file
 * @param eOptions Open options
 * @return CADFile pointer or NULL if failed. The pointer have to be freed by user.
 */
CADFile * OpenCADFile( const char * pszFileName, enum CADFile::OpenOptions eOptions, bool bReadUnsupportedGeometries )
{
    return OpenCADFile( GetDefaultFileIO( pszFileName ), eOptions, bReadUnsupportedGeometries );
}

#ifdef _DEBUG
void DebugMsg( const char* format, ... )
#else
void DebugMsg( const char*, ... )
#endif
{
#ifdef _DEBUG
    va_list argptr;
    va_start( argptr, format );
    vfprintf( stdout, format, argptr );
    va_end( argptr );
#endif //_DEBUG
}
