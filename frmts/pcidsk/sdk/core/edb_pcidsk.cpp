/******************************************************************************
 *
 * Purpose:  Implementation of the EDB interface that works only for
 *           links to another PCIDSK database.  This is mostly useful
 *           for testing - practical use is minimal.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "pcidsk_edb.h"
#include "pcidsk.h"
#include <cassert>
#include <cstdio>

using namespace PCIDSK;

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK_EDBFile                            */
/* ==================================================================== */
/************************************************************************/

class PCIDSK_EDBFile final : public EDBFile
{
    mutable PCIDSKFile *file;

public:

    explicit PCIDSK_EDBFile( PCIDSKFile *file_in ) { file = file_in; }
    ~PCIDSK_EDBFile() { PCIDSK_EDBFile::Close(); }

    int Close() const override;
    int GetWidth() const override;
    int GetHeight() const override;
    int GetChannels() const override;
    int GetBlockWidth(int channel ) const override;
    int GetBlockHeight(int channel ) const override;
    eChanType GetType(int channel ) const override;
    int ReadBlock(int channel,
                  int block_index, void *buffer,
                  int win_xoff, int win_yoff,
                  int win_xsize, int win_ysize ) override;
    int WriteBlock( int channel, int block_index, void *buffer) override;
};

/************************************************************************/
/*                           DefaultOpenEDB()                           */
/************************************************************************/

EDBFile *PCIDSK::DefaultOpenEDB( const std::string& filename, const std::string& access )

{
    // it would be nice to be able to pass in an appropriate PCIDSKInterface!

    PCIDSKFile *file = PCIDSK::Open( filename, access, nullptr );

    return new PCIDSK_EDBFile( file );
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int PCIDSK_EDBFile::Close() const

{
    if( file != nullptr )
    {
        delete file;
        file = nullptr;
    }

    return 1;
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

int PCIDSK_EDBFile::GetWidth() const

{
    return file->GetWidth();
}

/************************************************************************/
/*                             GetHeight()                              */
/************************************************************************/

int PCIDSK_EDBFile::GetHeight() const

{
    return file->GetHeight();
}

/************************************************************************/
/*                            GetChannels()                             */
/************************************************************************/

int PCIDSK_EDBFile::GetChannels() const

{
    return file->GetChannels();
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/

int PCIDSK_EDBFile::GetBlockWidth( int channel ) const

{
    return file->GetChannel(channel)->GetBlockWidth();
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/

int PCIDSK_EDBFile::GetBlockHeight( int channel ) const

{
    return file->GetChannel(channel)->GetBlockHeight();
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

eChanType PCIDSK_EDBFile::GetType( int channel ) const
{
    return file->GetChannel(channel)->GetType();
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int PCIDSK_EDBFile::ReadBlock( int channel,
                               int block_index, void *buffer,
                               int win_xoff, int win_yoff,
                               int win_xsize, int win_ysize )

{
    return
        file->GetChannel(channel)->ReadBlock( block_index, buffer,
                                              win_xoff, win_yoff,
                                              win_xsize, win_ysize );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int PCIDSK_EDBFile::WriteBlock( int channel, int block_index, void *buffer)

{
    return file->GetChannel(channel)->WriteBlock( block_index, buffer );
}
