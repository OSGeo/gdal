/******************************************************************************
 *
 * Purpose:  PCIDSK TEXt segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_TEX_H
#define INCLUDE_PCIDSK_TEX_H

#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSK_TEX                              */
/************************************************************************/

//! Interface to PCIDSK text segment.

    class PCIDSK_DLL PCIDSK_TEX
    {
    public:
        virtual ~PCIDSK_TEX() {}

/**
\brief Read a text segment (SEG_TEX).

All carriage returns in the file are converted to newlines during reading.  No other processing is done.

@return a string containing the entire contents of the text segment.

*/
        virtual std::string ReadText() = 0;

/**
\brief Write a text segment.

Writes the text to the text segment.  All newlines will be converted to
carriage controls for storage in the text segment per the normal text segment
conventions, and if missing a carriage return will be added to the end of the
file.

@param text the text to write to the segment.  May contain newlines, and other special characters but no embedded \0 characters.

*/
        virtual void WriteText( const std::string &text ) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_TEX_H
