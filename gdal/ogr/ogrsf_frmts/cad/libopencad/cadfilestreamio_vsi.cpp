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
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "cadfilestreamio.h"

#include <string>

CADFileStreamIO::CADFileStreamIO(const char* pszFilePath) : CADFileIO(pszFilePath)
{

}

CADFileStreamIO::~CADFileStreamIO()
{

}

const char* CADFileStreamIO::ReadLine()
{
    // TODO: getline
    return nullptr;
}

bool CADFileStreamIO::Eof()
{
    return VSIFEof( m_oFileStream ) == 0 ? false : true;
}

bool CADFileStreamIO::Open(int mode)
{
    std::string sOpenMode = "r";
    if(mode & OpenMode::binary)
        sOpenMode = "r+b";

    if(mode & OpenMode::write)
        sOpenMode = "w+b";
        return false;

    m_oFileStream = VSIFOpen( m_pszFilePath, sOpenMode.c_str() );

    if( m_oFileStream != NULL )
        m_bIsOpened = true;

    return m_bIsOpened;
}

bool CADFileStreamIO::Close()
{
    return VSIFClose( m_oFileStream ) == 0 ? true : false;
}

int CADFileStreamIO::Seek(long offset, CADFileIO::SeekOrigin origin)
{
    int nWhence = 0;
    switch (origin) {
    case SeekOrigin::CUR:
        nWhence = SEEK_CUR;
        break;
    case SeekOrigin::END:
        nWhence = SEEK_END;
        break;
    case SeekOrigin::BEG:
        nWhence = SEEK_SET;
        break;
    }

    return VSIFSeek( m_oFileStream, offset, nWhence) == 0 ? 0 : 1;
}

long CADFileStreamIO::Tell()
{
    return VSIFTell( m_oFileStream );
}

size_t CADFileStreamIO::Read(void* ptr, size_t size)
{
    return VSIFRead( static_cast<char*>(ptr),
                     1,
                     size,
                     m_oFileStream );
}

size_t CADFileStreamIO::Write(void* /*ptr*/, size_t /*size*/)
{
    // unsupported
    return 0;
}

void CADFileStreamIO::Rewind()
{
    VSIRewind ( m_oFileStream );
}
