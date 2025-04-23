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
 ******************************************************************************/
#include "cadfileio.h"

CADFileIO::CADFileIO( const char * pszFileName ) :
    m_soFilePath( pszFileName),
    m_bIsOpened (false)
{
}

CADFileIO::~CADFileIO()
{
}

bool CADFileIO::IsOpened() const
{
    return m_bIsOpened;
}

bool CADFileIO::Close()
{
    m_bIsOpened = false;
    return true;
}

const char * CADFileIO::GetFilePath() const
{
    return m_soFilePath.c_str();
}
