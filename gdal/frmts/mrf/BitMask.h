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

#pragma once
#include "Defines.h"


/** BitMask - Convenient and fast access to binary mask bits
* includes RLE compression and decompression, in BitMask.cpp
*
*/

class BitMask
{
public:
  BitMask(long nCols, long nRows) : m_pBits(0), m_nRows(nRows), m_nCols(nCols)
  {
      m_pBits = new Byte[Size()];
      if (!m_pBits)
	  m_nRows = m_nCols = 0;
      else
	  m_pBits[Size() - 1] = 0; // Set potential pad bytes to zero
  }
  ~BitMask()				       { if (m_pBits) delete[] m_pBits; }

  Byte  IsValid(long k) const                  { return (m_pBits[k >> 3] & Bit(k)) != 0; }
  void  SetValid(long k) const                 { m_pBits[k >> 3] |= Bit(k); }
  void  SetInvalid(long k) const               { m_pBits[k >> 3] &= ~Bit(k); }
  long  Size() const			       { return (m_nCols * m_nRows - 1) / 8 + 1; }

  // the max RLE compressed size is n + 2 + 2 * (n + 1) / 32767
  // Returns encoded size
  long RLEcompress(Byte *aRLE) const;
  // current encoded size
  long RLEsize() const;
  // Decompress a RLE bitmask, bitmask size should be already set
  // Returns false if input seems wrong
  bool RLEdecompress(const Byte* src);

private:
  Byte*  m_pBits;
  long   m_nRows, m_nCols;

  Byte  Bit(long k) const                      { return (1 << 7) >> (k & 7); }

  // Disable assignment op, default and copy constructor
  BitMask();
  BitMask(const BitMask& copy);
  BitMask& operator=(const BitMask& m);
};
