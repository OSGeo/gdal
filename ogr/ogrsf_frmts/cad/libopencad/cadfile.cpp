/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
 *
  * SPDX-License-Identifier: MIT
 *******************************************************************************/
#include "cadfile.h"
#include "opencad_api.h"

#include <iostream>

CADFile::CADFile( CADFileIO * poFileIO ) :
    pFileIO( poFileIO ),
    bReadingUnsupportedGeometries( false )
{
}

CADFile::~CADFile()
{
    if( nullptr != pFileIO )
    {
        pFileIO->Close();
        delete pFileIO;
    }
}

const CADHeader& CADFile::getHeader() const
{
    return oHeader;
}

const CADClasses& CADFile::getClasses() const
{
    return oClasses;
}

const CADTables& CADFile::getTables() const
{
    return oTables;
}

int CADFile::ParseFile( enum OpenOptions eOptions, bool bReadUnsupportedGeometries )
{
    if( nullptr == pFileIO )
        return CADErrorCodes::FILE_OPEN_FAILED;

    if( !pFileIO->IsOpened() )
    {
        if( !pFileIO->Open( CADFileIO::in | CADFileIO::binary ) )
            return CADErrorCodes::FILE_OPEN_FAILED;
    }

    // Set flag which will tell CADLayer to skip/not skip unsupported geoms
    bReadingUnsupportedGeometries = bReadUnsupportedGeometries;

    int nResultCode;
    nResultCode = ReadSectionLocators();
    if( nResultCode != CADErrorCodes::SUCCESS )
        return nResultCode;
    nResultCode = ReadHeader( eOptions );
    if( nResultCode != CADErrorCodes::SUCCESS )
        return nResultCode;
    nResultCode = ReadClasses( eOptions );
    if( nResultCode != CADErrorCodes::SUCCESS )
        return nResultCode;
    nResultCode = CreateFileMap();
    if( nResultCode != CADErrorCodes::SUCCESS )
        return nResultCode;
    nResultCode = ReadTables( eOptions );
    if( nResultCode != CADErrorCodes::SUCCESS )
        return nResultCode;

    return CADErrorCodes::SUCCESS;
}

int CADFile::ReadTables( CADFile::OpenOptions /*eOptions*/ )
{
    // TODO: read other tables in ALL option mode

    int nResult = oTables.ReadTable( this, CADTables::LayersTable );
    return nResult;
}

size_t CADFile::GetLayersCount() const
{
    return oTables.GetLayerCount();
}

CADLayer& CADFile::GetLayer( size_t index )
{
    return oTables.GetLayer( index );
}

bool CADFile::isReadingUnsupportedGeometries()
{
    return bReadingUnsupportedGeometries;
}
