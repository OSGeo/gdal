/******************************************************************************
 *
 * Purpose:  Interface of XRITHeaderParser class. Parse the header of
 *           the combined XRIT header/data files.
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

#if !defined(AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_)
#define AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <fstream>

class XRITHeaderParser
{
public:
  XRITHeaderParser(std::ifstream & ifile);

  virtual ~XRITHeaderParser();

  const bool isValid() {
    return m_isValid;
  }

  const bool isPrologue() {
    return m_isPrologue;
  }

  const long dataSize() {
    return m_dataSize;
  }

  const int nrRows() {
    return m_nrRows;
  }

  const int nrColumns() {
    return m_nrColumns;
  }

  const int nrBitsPerPixel() {
    return m_nrBitsPerPixel;
  }

  const bool isScannedNorth() {
    return m_scanNorth;
  }

private:
  int parseInt16(unsigned char * num);
  long parseInt32(unsigned char * num);
  void parseHeader(unsigned char * buf, long totalHeaderLength);

  bool m_isValid;
  bool m_isPrologue;
  long m_dataSize;
  int m_nrBitsPerPixel;
  int m_nrColumns;
  int m_nrRows;
  bool m_scanNorth;
};

#endif // !defined(AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_)
