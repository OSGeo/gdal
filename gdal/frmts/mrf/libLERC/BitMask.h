/*
Copyright 2015 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
A local copy of the license and additional notices are located with the
source distribution at:
http://github.com/Esri/lerc/
Contributors:  Thomas Maurer
               Lucian Plesea
*/

#ifndef BITMASK_H
#define BITMASK_H
#include "Defines.h"

NAMESPACE_LERC_START

/** BitMask - Convenient and fast access to binary mask bits
* includes RLE compression and decompression, in BitMask.cpp
*
*/

class BitMask
{
public:
  BitMask(int nCols, int nRows) : m_pBits(NULL), m_nRows(nRows), m_nCols(nCols)
  {
      m_pBits = new Byte[Size()];
      if (!m_pBits)
          m_nRows = m_nCols = 0;
      else
           m_pBits[Size() - 1] = 0; // Set potential pad bytes to zero
  }
  ~BitMask()                                  { if (m_pBits) delete[] m_pBits; }

  Byte  IsValid(int k) const                  { return (m_pBits[k >> 3] & Bit(k)) != 0; }
  void  SetValid(int k) const                 { m_pBits[k >> 3] |= Bit(k); }
  void  SetInvalid(int k) const               { m_pBits[k >> 3] &= ~Bit(k); }
  int   Size() const                          { return (m_nCols * m_nRows - 1) / 8 + 1; }

  // max RLE compressed size is n + 4 + 2 * (n - 1) / 32767
  // Returns encoded size
  int RLEcompress(Byte *aRLE) const;
  // current encoded size
  int RLEsize() const;
  // Decompress a RLE bitmask, bitmask size should be already set
  // Returns false if input seems wrong
  bool RLEdecompress(const Byte* src, size_t nRemainingBytes);

private:
  Byte*  m_pBits;
  int   m_nRows, m_nCols;

  static Byte  Bit(int k)                      { return (1 << 7) >> (k & 7); }

  // Disable assignment op, default and copy constructor
  BitMask();
  BitMask(const BitMask& copy);
  BitMask& operator=(const BitMask& m);
};

NAMESPACE_LERC_END
#endif
