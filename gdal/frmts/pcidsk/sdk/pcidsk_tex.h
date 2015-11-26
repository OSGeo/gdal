/******************************************************************************
 *
 * Purpose:  PCIDSK TEXt segment interface class.
 * 
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
        virtual	~PCIDSK_TEX() {}

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
