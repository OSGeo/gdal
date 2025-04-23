/******************************************************************************
 *
 * Purpose:  Interface of XRITHeaderParser class. Parse the header of
 *           the combined XRIT header/data files.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#if !defined(                                                                  \
    AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_)
#define AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#include <fstream>

class XRITHeaderParser
{
  public:
    explicit XRITHeaderParser(std::ifstream &ifile);

    virtual ~XRITHeaderParser();

    bool isValid() const
    {
        return m_isValid;
    }

    bool isPrologue() const
    {
        return m_isPrologue;
    }

    long dataSize() const
    {
        return m_dataSize;
    }

    int nrRows() const
    {
        return m_nrRows;
    }

    int nrColumns() const
    {
        return m_nrColumns;
    }

    int nrBitsPerPixel() const
    {
        return m_nrBitsPerPixel;
    }

    bool isScannedNorth() const
    {
        return m_scanNorth;
    }

  private:
    static int parseInt16(unsigned char *num);
    static long parseInt32(unsigned char *num);
    void parseHeader(unsigned char *buf, long totalHeaderLength);

    bool m_isValid;
    bool m_isPrologue;
    long m_dataSize;
    int m_nrBitsPerPixel;
    int m_nrColumns;
    int m_nrRows;
    bool m_scanNorth;
};

#endif  // !defined(AFX_XRITHEADERPARSER_H__D01CC599_96C2_4901_85B3_96169D757898__INCLUDED_)
