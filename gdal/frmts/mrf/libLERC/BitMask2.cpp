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
*/

//
// BitMask2.cpp
//

#include "BitMask2.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cassert>

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

BitMask2::BitMask2() : m_pBits(NULL), m_nCols(0), m_nRows(0)
{
}

// -------------------------------------------------------------------------- ;

BitMask2::BitMask2(int nCols, int nRows) : m_pBits(NULL), m_nCols(0), m_nRows(0)
{
  SetSize(nCols, nRows);
}

// -------------------------------------------------------------------------- ;

BitMask2::BitMask2(const BitMask2& src) : m_pBits(NULL), m_nCols(0), m_nRows(0)
{
  SetSize(src.m_nCols, src.m_nRows);
  if (m_pBits && src.m_pBits)
    memcpy(m_pBits, src.m_pBits, Size());
}

// -------------------------------------------------------------------------- ;

BitMask2& BitMask2::operator= (const BitMask2& src)
{
  if (this == &src) return *this;

  SetSize(src.m_nCols, src.m_nRows);
  if (src.m_pBits)
    memcpy(m_pBits, src.m_pBits, Size());

  return *this;
}

// -------------------------------------------------------------------------- ;

bool BitMask2::SetSize(int nCols, int nRows)
{
  if (nCols != m_nCols || nRows != m_nRows)
  {
    Clear();
    m_nCols = nCols;
    m_nRows = nRows;
    m_pBits = new Byte[Size()];
  }
  return m_pBits != NULL;
}

// -------------------------------------------------------------------------- ;
//
// Count of set bits in a byte, done in 7 32bit instructions
// Adds every two bits sideways, makes four copies, masks each of the four results
// the last multiplication adds the four results together in the top nibble
// This is potentially slower for input data
//
LERC_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static inline int csb(unsigned int v) {
    return ((((v - ((v >> 1) & 0x55)) * 0x1010101) & 0x30c00c03) * 0x10040041) >> 0x1c;
}

// Number of bits set.
// We really only need to know if full, empty or in between
int BitMask2::CountValidBits() const
{
  assert(Size());
  const Byte* ptr = m_pBits;
  int sum = 0;

  // The first loop is in multiples of four, to let the compiler optimize
  for (int i = 0; i < (Size()/4) * 4; i++)
      sum += csb(*ptr++);
  // Second loop for the leftover bytes, up to three
  for (int i = 0; i < Size() % 4; i++)
      sum += csb(*ptr++);

  // Subtract the defined bits potentially contained in the last byte
  // Number of undefined bytes is (C*R)%8
  // The undefined bits are at the low bit position, use a mask for them
  sum -= csb((*--ptr) & ((1 << ((m_nCols * m_nRows) % 8)) -1));

  return sum;
}

// -------------------------------------------------------------------------- ;

void BitMask2::Clear()
{
  delete[] m_pBits;
  m_pBits = NULL;
  m_nCols = 0;
  m_nRows = 0;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
