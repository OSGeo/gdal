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
#include "cadfilestreamio.h"

CADFileStreamIO::CADFileStreamIO( const char * pszFilePath ) : CADFileIO( pszFilePath )
{
}

CADFileStreamIO::~CADFileStreamIO()
{
    if( CADFileStreamIO::IsOpened() )
        CADFileStreamIO::Close();
}

const char * CADFileStreamIO::ReadLine()
{
    // TODO: getline
    return nullptr;
}

bool CADFileStreamIO::Eof() const
{
    return m_oFileStream.eof();
}

bool CADFileStreamIO::Open( int mode )
{
    auto io_mode = std::ifstream::in; // As we use ifstream
    if( mode & OpenMode::binary )
        io_mode = std::ifstream::binary; // In set by default

    if( mode & OpenMode::out )
        //io_mode |= std::ifstream::out;
        return false;

    m_oFileStream.open( m_soFilePath, io_mode );

    if( m_oFileStream.is_open() )
        m_bIsOpened = true;

    return m_bIsOpened;
}

bool CADFileStreamIO::Close()
{
    m_oFileStream.close();
    return CADFileIO::Close();
}

int CADFileStreamIO::Seek( long offset, CADFileIO::SeekOrigin origin )
{
    std::ios_base::seekdir direction;
    switch( origin )
    {
        case SeekOrigin::CUR:
            direction = std::ios_base::cur;
            break;
        case SeekOrigin::END:
            direction = std::ios_base::end;
            break;
        default:
        case SeekOrigin::BEG:
            direction = std::ios_base::beg;
            break;
    }

    return m_oFileStream.seekg( offset, direction ).good() ? 0 : 1;
}

long int CADFileStreamIO::Tell()
{
    // FIXME? cast may cause potential issue on 32bit / Windows for large files
    return static_cast<long>(m_oFileStream.tellg());
}

size_t CADFileStreamIO::Read( void * ptr, size_t size )
{
    return static_cast<size_t>(m_oFileStream.read( static_cast<char *>(ptr), static_cast<long>(size) ).gcount());
}

size_t CADFileStreamIO::Write( void * /*ptr*/, size_t /*size*/ )
{
    // unsupported
    return 0;
}

void CADFileStreamIO::Rewind()
{
    m_oFileStream.seekg( 0, std::ios_base::beg );
}
