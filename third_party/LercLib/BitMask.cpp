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

#include "Defines.h"
#include "BitMask.h"
#include <cstring>

USING_NAMESPACE_LERC

// -------------------------------------------------------------------------- ;

BitMask::BitMask(const BitMask& src)
{
  SetSize(src.m_nCols, src.m_nRows);
  if (m_pBits && src.m_pBits)
    memcpy(m_pBits, src.m_pBits, Size());
}

// -------------------------------------------------------------------------- ;

BitMask& BitMask::operator= (const BitMask& src)
{
  if (this == &src) return *this;

  SetSize(src.m_nCols, src.m_nRows);
  if (m_pBits && src.m_pBits)
    memcpy(m_pBits, src.m_pBits, Size());

  return *this;
}

// -------------------------------------------------------------------------- ;

void BitMask::SetAllValid() const
{
  memset(m_pBits, 255, Size());
}

void BitMask::SetAllInvalid() const
{
  memset(m_pBits, 0, Size());
}

// -------------------------------------------------------------------------- ;

bool BitMask::SetSize(int nCols, int nRows)
{
  if (nCols != m_nCols || nRows != m_nRows)
  {
    Clear();
    m_pBits = new Byte[(nCols * nRows + 7) >> 3];
    m_nCols = nCols;
    m_nRows = nRows;
  }
  return m_pBits != nullptr;
}

// -------------------------------------------------------------------------- ;

int BitMask::CountValidBits() const
{
  const Byte numBitsHB[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
  const Byte* ptr = m_pBits;
  int sum = 0;
  int i = Size();
  while (i--)
  {
    sum += numBitsHB[*ptr & 15] + numBitsHB[*ptr >> 4];
    ptr++;
  }

  // subtract undefined bits potentially contained in the last byte
  for (int k = m_nCols * m_nRows; k < Size() * 8; k++)
    if (IsValid(k))
      sum--;

  return sum;
}

// -------------------------------------------------------------------------- ;

void BitMask::Clear()
{
  delete[] m_pBits;
  m_pBits = nullptr;
  m_nCols = 0;
  m_nRows = 0;
}

// -------------------------------------------------------------------------- ;

