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
#ifndef CADFILESTREAMIO_H
#define CADFILESTREAMIO_H

#include "cadfileio.h"

#include <fstream>

class CADFileStreamIO : public CADFileIO
{
public:
    explicit             CADFileStreamIO(const char* pszFilePath);
    ~CADFileStreamIO() override;

    const char* ReadLine() override;
    bool        Eof() const override;
    bool        Open(int mode) override;
    bool        Close() override;
    int         Seek(long int offset, SeekOrigin origin) override;
    long int    Tell() override;
    size_t      Read(void* ptr, size_t size) override;
    size_t      Write(void* ptr, size_t size) override;
    void        Rewind() override;
protected:
    std::ifstream       m_oFileStream;
};

#endif // CADFILESTREAMIO_H
