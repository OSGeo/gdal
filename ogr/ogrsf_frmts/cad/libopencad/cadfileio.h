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
 ******************************************************************************/
#ifndef CADFILEIO_H
#define CADFILEIO_H

#include "opencad.h"

#include <cstddef>
#include <string>

/**
 * @brief The CADFileIO class provides in/out file operations as read, write,
 * seek, etc. This is abstract class.
 */
class OCAD_EXTERN CADFileIO
{
public:
    enum class SeekOrigin
    {
        BEG, /**< Begin of the file */
        CUR, /**< Current position of the pointer */
        END  /**< End of file */
    };

    enum OpenMode
    {
        binary      = 1L << 2,
        in          = 1L << 3,
        out         = 1L << 4
    };

public:
    explicit CADFileIO( const char * pszFileName );
    virtual                 ~CADFileIO();

    virtual const char * ReadLine() = 0;
    virtual bool     Eof() const                                = 0;
    virtual bool     Open( int mode )                           = 0;
    virtual bool     IsOpened() const;
    virtual bool     Close();
    virtual int      Seek( long int offset, SeekOrigin origin ) = 0;
    virtual long int Tell()                                     = 0;
    virtual size_t   Read( void * ptr, size_t size )            = 0;
    virtual size_t   Write( void * ptr, size_t size )           = 0;
    virtual void     Rewind()                                   = 0;
    const char * GetFilePath() const;

protected:
    std::string m_soFilePath;
    bool        m_bIsOpened;
};

#endif // CADFILEIO_H
