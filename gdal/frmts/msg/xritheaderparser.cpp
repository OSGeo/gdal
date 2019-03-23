/******************************************************************************
 *
 * Purpose:  Implementation of XRITHeaderParser class. Parse the header
 *           of the combined XRIT header/data files.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
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
 ******************************************************************************/

 #include "cpl_port.h"  // Must be first.

#include "xritheaderparser.h"
#include <cstdlib> // malloc, free
#include <cstring> // memcpy

CPL_CVSID("$Id$")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//
// Upon successful parsing of a header in ifile, isValid() returns true
// and ifile is sought to the beginning of the image data
//////////////////////////////////////////////////////////////////////

XRITHeaderParser::XRITHeaderParser(std::ifstream & ifile)
: m_isValid(false)
, m_isPrologue(false)
, m_dataSize(0)
, m_nrBitsPerPixel(0)
, m_nrColumns(0)
, m_nrRows(0)
, m_scanNorth(false)
{
  const unsigned int probeSize = 8;

  unsigned char probeBuf[probeSize];
  ifile.read((char*)probeBuf, probeSize); // Probe file by reading first 8 bytes

  if (probeBuf[0] == 0 && probeBuf[1] == 0 && probeBuf[2] == 16) // Check for primary header record
  {
    long totalHeaderLength = parseInt32(&probeBuf[4]);
    if ((totalHeaderLength >= 10) && (totalHeaderLength <= 10000)) // Check for valid header length
    {
      unsigned char * buf = (unsigned char*)std::malloc(totalHeaderLength);
      std::memcpy(buf, probeBuf, probeSize); // save what we have already read when probing
      ifile.read((char*)buf + probeSize, totalHeaderLength - probeSize); // read the rest of the header section
      parseHeader(buf, totalHeaderLength);
      std::free(buf);

      m_isValid = true;
    }
  }

  if (!m_isValid) // seek back to original position
  {
    ifile.seekg(-probeSize, std::ios_base::cur);
  }
}

XRITHeaderParser::~XRITHeaderParser() {}

int XRITHeaderParser::parseInt16(unsigned char * num)
{
  return (num[0]<<8) | num[1];
}

long XRITHeaderParser::parseInt32(unsigned char * num)
{
    int i;
    memcpy(&i, num, 4);
    CPL_MSBPTR32(&i);
    return i;
}

void XRITHeaderParser::parseHeader(unsigned char * buf, long totalHeaderLength)
{
  int remainingHeaderLength = static_cast<int>(totalHeaderLength);

  while (remainingHeaderLength > 0)
  {
    int headerType = buf[0];
    int headerRecordLength = parseInt16(&buf[1]);
    if (headerRecordLength > remainingHeaderLength)
      break;

    switch(headerType)
    {
      case 0: // primary header
        {
          int fileTypeCode = buf[3]; // 0 = image data file, 128 = prologue
          if (fileTypeCode == 128)
            m_isPrologue = true;

          long dataFieldLengthH = parseInt32(&buf[8]); // length of data field in bits (High DWORD)
          long dataFieldLengthL = parseInt32(&buf[12]); // length of data field in bits (Low DWORD)
          m_dataSize = (dataFieldLengthH << 5) + (dataFieldLengthL >> 3); // combine and convert bits to bytes
        }
        break;
      case 1: // image structure
        m_nrBitsPerPixel = buf[3]; // NB, number of bits per pixel
        m_nrColumns = parseInt16(&buf[4]); // NC, number of columns
        m_nrRows = parseInt16(&buf[6]); // NL, number of lines
        break;
      case 2: // image navigation
        {
#if 0
          /*long cfac =*/ parseInt32(&buf[35]); // column scaling factor
#endif
          long lfac = parseInt32(&buf[39]); // line scaling factor
#if 0
          /*long coff =*/ parseInt32(&buf[43]); // column offset
          /*long loff =*/ parseInt32(&buf[47]); // line offset
#endif
          if (lfac >= 0)
            m_scanNorth = true;
          else
            m_scanNorth = false;
        }
        break;
      case 3: // image data function
      case 4: // annotation
      case 5: // time stamp
      case 6: // ancillary text
      case 7: // key header
      case 128: // image segment identification
      case 129: // encryption key message header
      case 130: // image compensation information header
      case 131: // image observation time header
      case 132: // image quality information header
        break;
      default: // ignore unknown header type
        break;
    }

    buf += headerRecordLength;
    remainingHeaderLength -= headerRecordLength;
  }
}
