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
 * SPDX-License-Identifier: MIT
 *******************************************************************************/
#include "vsilfileio.h"

VSILFileIO::VSILFileIO(const char *pszFilePath)
    : CADFileIO(pszFilePath), m_oFileStream(nullptr)
{
}

VSILFileIO::~VSILFileIO()
{
    if (m_oFileStream)
        VSILFileIO::Close();
}

const char *VSILFileIO::ReadLine()
{
    // TODO: getline
    return nullptr;
}

bool VSILFileIO::Eof() const
{
    return VSIFEofL(m_oFileStream) || VSIFErrorL(m_oFileStream);
}

bool VSILFileIO::Open(int mode)
{
    // NOTE: now support only read mode
    if (mode & OpenMode::out)
        return false;

    std::string sOpenMode = "r";
    if (mode & OpenMode::binary)
        sOpenMode = "rb";

    m_oFileStream = VSIFOpenL(m_soFilePath.c_str(), sOpenMode.c_str());

    if (m_oFileStream != nullptr)
        m_bIsOpened = true;

    return m_bIsOpened;
}

bool VSILFileIO::Close()
{
    bool bRet = VSIFCloseL(m_oFileStream) == 0 ? true : false;
    m_oFileStream = nullptr;
    return bRet;
}

int VSILFileIO::Seek(long offset, CADFileIO::SeekOrigin origin)
{
    int nWhence = 0;
    switch (origin)
    {
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

    return VSIFSeekL(m_oFileStream, offset, nWhence) == 0 ? 0 : 1;
}

long int VSILFileIO::Tell()
{
    return static_cast<long>(VSIFTellL(m_oFileStream));
}

size_t VSILFileIO::Read(void *ptr, size_t size)
{
    return VSIFReadL(static_cast<char *>(ptr), 1, size, m_oFileStream);
}

size_t VSILFileIO::Write(void * /*ptr*/, size_t /*size*/)
{
    // unsupported
    return 0;
}

void VSILFileIO::Rewind()
{
    VSIRewindL(m_oFileStream);
}
