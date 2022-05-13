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

#include <algorithm>
#include <cassert>
#include "Defines.h"
#include "BitStuffer2.h"

using namespace std;
USING_NAMESPACE_LERC

// -------------------------------------------------------------------------- ;

// if you change Encode(...) / Decode(...), don't forget to update ComputeNumBytesNeeded(...)

bool BitStuffer2::EncodeSimple(Byte** ppByte, const vector<unsigned int>& dataVec, int lerc2Version) const
{
  if (!ppByte || dataVec.empty())
    return false;

  unsigned int maxElem = *max_element(dataVec.begin(), dataVec.end());
  int numBits = 0;
  while ((numBits < 32) && (maxElem >> numBits))
    numBits++;

  if (numBits >= 32)
    return false;

  Byte numBitsByte = (Byte)numBits;
  unsigned int numElements = (unsigned int)dataVec.size();
  unsigned int numUInts = (numElements * numBits + 31) / 32;

  // use the upper 2 bits to encode the type used for numElements: Byte, ushort, or uint
  int n = NumBytesUInt(numElements);
  int bits67 = (n == 4) ? 0 : 3 - n;
  numBitsByte |= bits67 << 6;

  // bit5 = 0 means simple mode

  **ppByte = numBitsByte;
  (*ppByte)++;

  if (!EncodeUInt(ppByte, numElements, n))
    return false;

  if (numUInts > 0)    // numBits can be 0, then we only write the header
  {
    if (lerc2Version >= 3)
      BitStuff(ppByte, dataVec, numBits);
    else
      BitStuff_Before_Lerc2v3(ppByte, dataVec, numBits);
  }

  return true;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer2::EncodeLut(Byte** ppByte, const vector<pair<unsigned int, unsigned int> >& sortedDataVec, int lerc2Version) const
{
  if (!ppByte || sortedDataVec.empty())
    return false;

  if (sortedDataVec[0].first != 0)    // corresponds to min
    return false;

  // collect the different values for the lut
  unsigned int numElem = (unsigned int)sortedDataVec.size();
  unsigned int indexLut = 0;

  m_tmpLutVec.resize(0);    // omit the 0 throughout that corresponds to min
  m_tmpIndexVec.assign(numElem, 0);

  for (unsigned int i = 1; i < numElem; i++)
  {
    unsigned int prev = sortedDataVec[i - 1].first;
    m_tmpIndexVec[sortedDataVec[i - 1].second] = indexLut;

    if (sortedDataVec[i].first != prev)
    {
      m_tmpLutVec.push_back(sortedDataVec[i].first);
      indexLut++;
    }
  }
  m_tmpIndexVec[sortedDataVec[numElem - 1].second] = indexLut;    // don't forget the last one

  // write first 2 data elements same as simple, but bit5 set to 1
  unsigned int maxElem = m_tmpLutVec.back();
  int numBits = 0;
  while ((numBits < 32) && (maxElem >> numBits))
    numBits++;

  if (numBits >= 32)
    return false;

  Byte numBitsByte = (Byte)numBits;

  // use the upper 2 bits to encode the type used for numElem: byte, ushort, or uint
  int n = NumBytesUInt(numElem);
  int bits67 = (n == 4) ? 0 : 3 - n;
  numBitsByte |= bits67 << 6;

  numBitsByte |= (1 << 5);    // bit 5 = 1 means lut mode

  **ppByte = numBitsByte;
  (*ppByte)++;

  if (!EncodeUInt(ppByte, numElem, n))    // numElements = numIndexes to lut
    return false;

  unsigned int nLut = (unsigned int)m_tmpLutVec.size();
  if (nLut < 1 || nLut >= 255)
    return false;

  **ppByte = (Byte)nLut + 1;    // size of lut, incl the 0
  (*ppByte)++;

  if (lerc2Version >= 3)
    BitStuff(ppByte, m_tmpLutVec, numBits);    // lut
  else
    BitStuff_Before_Lerc2v3(ppByte, m_tmpLutVec, numBits);

  int nBitsLut = 0;
  while (nLut >> nBitsLut)    // indexes are in [0 .. nLut]
    nBitsLut++;

  if (lerc2Version >= 3)
    BitStuff(ppByte, m_tmpIndexVec, nBitsLut);    // indexes
  else
    BitStuff_Before_Lerc2v3(ppByte, m_tmpIndexVec, nBitsLut);

  return true;
}

// -------------------------------------------------------------------------- ;

// if you change Encode(...) / Decode(...), don't forget to update ComputeNumBytesNeeded(...)

bool BitStuffer2::Decode(const Byte** ppByte, size_t& nBytesRemaining, vector<unsigned int>& dataVec, size_t maxElementCount, int lerc2Version) const
{
  if (!ppByte || nBytesRemaining < 1)
    return false;

  Byte numBitsByte = **ppByte;
  (*ppByte)++;
  nBytesRemaining--;

  int bits67 = numBitsByte >> 6;
  int nb = (bits67 == 0) ? 4 : 3 - bits67;

  bool doLut = (numBitsByte & (1 << 5)) ? true : false;    // bit 5
  numBitsByte &= 31;    // bits 0-4;
  int numBits = numBitsByte;

  unsigned int numElements = 0;
  if (!DecodeUInt(ppByte, nBytesRemaining, numElements, nb))
    return false;
  if (numElements > maxElementCount)
    return false;

  if (!doLut)
  {
    if (numBits > 0)    // numBits can be 0
    {
      if (lerc2Version >= 3)
      {
        if (!BitUnStuff(ppByte, nBytesRemaining, dataVec, numElements, numBits))
          return false;
      }
      else
      {
        if (!BitUnStuff_Before_Lerc2v3(ppByte, nBytesRemaining, dataVec, numElements, numBits))
          return false;
      }
    }
  }
  else
  {
    if (numBits == 0)  // fail gracefully in case of corrupted blob for old version <= 2 which had no checksum
      return false;
    if (nBytesRemaining < 1)
      return false;

    Byte nLutByte = **ppByte;
    (*ppByte)++;
    nBytesRemaining--;

    int nLut = nLutByte - 1;

    // unstuff lut w/o the 0
    if (lerc2Version >= 3)
    {
      if (!BitUnStuff(ppByte, nBytesRemaining, m_tmpLutVec, nLut, numBits))
        return false;
    }
    else
    {
      if (!BitUnStuff_Before_Lerc2v3(ppByte, nBytesRemaining, m_tmpLutVec, nLut, numBits))
        return false;
    }

    int nBitsLut = 0;
    while (nLut >> nBitsLut)
      nBitsLut++;
    if (nBitsLut == 0)
      return false;

    if (lerc2Version >= 3)
    {
      // unstuff indexes
      if (!BitUnStuff(ppByte, nBytesRemaining, dataVec, numElements, nBitsLut))
        return false;

      // replace indexes by values
      m_tmpLutVec.insert(m_tmpLutVec.begin(), 0);    // put back in the 0
      for (unsigned int i = 0; i < numElements; i++)
      {
#ifdef GDAL_COMPILATION
        if (dataVec[i] >= m_tmpLutVec.size())
          return false;
#endif
        dataVec[i] = m_tmpLutVec[dataVec[i]];
      }
    }
    else
    {
      // unstuff indexes
      if (!BitUnStuff_Before_Lerc2v3(ppByte, nBytesRemaining, dataVec, numElements, nBitsLut))
        return false;

      // replace indexes by values
      m_tmpLutVec.insert(m_tmpLutVec.begin(), 0);    // put back in the 0
      for (unsigned int i = 0; i < numElements; i++)
      {
        if (dataVec[i] >= m_tmpLutVec.size())
          return false;

        dataVec[i] = m_tmpLutVec[dataVec[i]];
      }
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int BitStuffer2::ComputeNumBytesNeededLut(const vector<pair<unsigned int, unsigned int> >& sortedDataVec, bool& doLut)
{
  unsigned int maxElem = sortedDataVec.back().first;
  unsigned int numElem = (unsigned int)sortedDataVec.size();

  int numBits = 0;
  while ((numBits < 32) && (maxElem >> numBits))
    numBits++;
  unsigned int numBytes = 1 + NumBytesUInt(numElem) + ((numElem * numBits + 7) >> 3);

  // go through and count how often the value changes
  int nLut = 0;
  for (unsigned int i = 1; i < numElem; i++)
    if (sortedDataVec[i].first != sortedDataVec[i - 1].first)
      nLut++;

  int nBitsLut = 0;
  while (nLut >> nBitsLut)
    nBitsLut++;

  unsigned int numBitsTotalLut = nLut * numBits;    // num bits w/o the 0
  unsigned int numBytesLut = 1 + NumBytesUInt(numElem) + 1 + ((numBitsTotalLut + 7) >> 3) + ((numElem * nBitsLut + 7) >> 3);

  doLut = numBytesLut < numBytes;
  return min(numBytesLut, numBytes);
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

void BitStuffer2::BitStuff_Before_Lerc2v3(Byte** ppByte, const vector<unsigned int>& dataVec, int numBits)
{
  unsigned int numElements = (unsigned int)dataVec.size();
  unsigned int numUInts = (numElements * numBits + 31) / 32;
  unsigned int numBytes = numUInts * sizeof(unsigned int);
  unsigned int* arr = (unsigned int*)(*ppByte);

  memset(arr, 0, numBytes);

  // do the stuffing
  const unsigned int* srcPtr = &dataVec[0];
  unsigned int* dstPtr = arr;
  int bitPos = 0;

  for (unsigned int i = 0; i < numElements; i++)
  {
    if (32 - bitPos >= numBits)
    {
      unsigned int dstValue;
      memcpy(&dstValue, dstPtr, sizeof(unsigned int));
      dstValue |= (*srcPtr++) << (32 - bitPos - numBits);
      memcpy(dstPtr, &dstValue, sizeof(unsigned int));
      bitPos += numBits;
      if (bitPos == 32)    // shift >= 32 is undefined
      {
        bitPos = 0;
        dstPtr++;
      }
    }
    else
    {
      unsigned int dstValue;
      int n = numBits - (32 - bitPos);
      memcpy(&dstValue, dstPtr, sizeof(unsigned int));
      dstValue |= (*srcPtr) >> n;
      memcpy(dstPtr, &dstValue, sizeof(unsigned int));
      dstPtr++;
      memcpy(&dstValue, dstPtr, sizeof(unsigned int));
      dstValue |= (*srcPtr++) << (32 - n);
      memcpy(dstPtr, &dstValue, sizeof(unsigned int));
      bitPos = n;
    }
  }

  // save the 0-3 bytes not used in the last UInt
  unsigned int numBytesNotNeeded = NumTailBytesNotNeeded(numElements, numBits);
  unsigned int n = numBytesNotNeeded;
  for (; n; --n)
  {
    unsigned int dstValue;
    memcpy(&dstValue, dstPtr, sizeof(unsigned int));
    dstValue >>= 8;
    memcpy(dstPtr, &dstValue, sizeof(unsigned int));
  }

  *ppByte += numBytes - numBytesNotNeeded;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer2::BitUnStuff_Before_Lerc2v3(const Byte** ppByte, size_t& nBytesRemaining,
    vector<unsigned int>& dataVec, unsigned int numElements, int numBits) const
{
  if (numElements == 0 || numBits >= 32)
    return false;
  unsigned long long numUIntsLL = ((unsigned long long)numElements * numBits + 31) / 32;
  unsigned long long numBytesLL = numUIntsLL * sizeof(unsigned int);
  size_t numBytes = (size_t)numBytesLL; // could theoretically overflow on 32 bit system
  size_t numUInts = (size_t)numUIntsLL;
  unsigned int ntbnn = NumTailBytesNotNeeded(numElements, numBits);
  if (numBytes != numBytesLL || nBytesRemaining + ntbnn < numBytes)
    return false;

  try
  {
    dataVec.resize(numElements, 0);    // init with 0
    m_tmpBitStuffVec.resize(numUInts);
  }
  catch( const std::exception& )
  {
    return false;
  }

  m_tmpBitStuffVec[numUInts - 1] = 0;    // set last uint to 0

  unsigned int nBytesToCopy = (numElements * numBits + 7) / 8;
  memcpy(&m_tmpBitStuffVec[0], *ppByte, nBytesToCopy);

  unsigned int* pLastULong = &m_tmpBitStuffVec[numUInts - 1];
  while (ntbnn)
  {
    -- ntbnn;
    *pLastULong <<= 8;
  }

  unsigned int* srcPtr = &m_tmpBitStuffVec[0];
  unsigned int* dstPtr = &dataVec[0];
  int bitPos = 0;

  for (unsigned int i = 0; i < numElements; i++)
  {
    if (32 - bitPos >= numBits)
    {
      unsigned int val;
      memcpy(&val, srcPtr, sizeof(unsigned int));
      unsigned int n = val << bitPos;
      *dstPtr++ = n >> (32 - numBits);
      bitPos += numBits;

      if (bitPos == 32)    // shift >= 32 is undefined
      {
        bitPos = 0;
        srcPtr++;
      }
    }
    else
    {
      unsigned int val;
      memcpy(&val, srcPtr, sizeof(unsigned int));
      srcPtr++;
      unsigned int n = val << bitPos;
      *dstPtr = n >> (32 - numBits);
      bitPos -= (32 - numBits);
      memcpy(&val, srcPtr, sizeof(unsigned int));
      *dstPtr++ |= val >> (32 - bitPos);
    }
  }

  *ppByte += nBytesToCopy;
  nBytesRemaining -= nBytesToCopy;

  return true;
}

// -------------------------------------------------------------------------- ;

// starting with version Lerc2v3: integer >> into local uint buffer, plus final memcpy

void BitStuffer2::BitStuff(Byte** ppByte, const vector<unsigned int>& dataVec, int numBits) const
{
  unsigned int numElements = (unsigned int)dataVec.size();
  unsigned int numUInts = (numElements * numBits + 31) / 32;
  unsigned int numBytes = numUInts * sizeof(unsigned int);

  m_tmpBitStuffVec.resize(numUInts);
  unsigned int* dstPtr = &m_tmpBitStuffVec[0];

  memset(dstPtr, 0, numBytes);

  // do the stuffing
  const unsigned int* srcPtr = &dataVec[0];
  int bitPos = 0;
  assert(numBits <= 32); // to avoid coverity warning about large shift a bit later, when doing (*srcPtr++) >> (32 - bitPos)

  for (unsigned int i = 0; i < numElements; i++)
  {
    if (32 - bitPos >= numBits)
    {
      *dstPtr |= (*srcPtr++) << bitPos;
      bitPos += numBits;
      if (bitPos == 32)    // shift >= 32 is undefined
      {
        dstPtr++;
        bitPos = 0;
      }
    }
    else
    {
      *dstPtr++ |= (*srcPtr) << bitPos;
      *dstPtr |= (*srcPtr++) >> (32 - bitPos);
      bitPos += numBits - 32;
    }
  }

  // copy the bytes to the outgoing byte stream
  size_t numBytesUsed = numBytes - NumTailBytesNotNeeded(numElements, numBits);
#ifdef CSA_BUILD
  assert( numElements );
#endif
  memcpy(*ppByte, m_tmpBitStuffVec.data(), numBytesUsed);

  *ppByte += numBytesUsed;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer2::BitUnStuff(const Byte** ppByte, size_t& nBytesRemaining, vector<unsigned int>& dataVec,
  unsigned int numElements, int numBits) const
{
  if (numElements == 0 || numBits >= 32)
    return false;
  unsigned long long numUIntsLL = ((unsigned long long)numElements * numBits + 31) / 32;
  unsigned long long numBytesLL = numUIntsLL * sizeof(unsigned int);
  size_t numBytes = (size_t)numBytesLL; // could theoretically overflow on 32 bit system
  if (numBytes != numBytesLL)
    return false;
  size_t numUInts = (size_t)numUIntsLL;

  // copy the bytes from the incoming byte stream
  const size_t numBytesUsed = numBytes - NumTailBytesNotNeeded(numElements, numBits);

  if (nBytesRemaining < numBytesUsed)
    return false;

  try
  {
    dataVec.resize(numElements);
  }
  catch( const std::exception& )
  {
    return false;
  }

  try
  {
    m_tmpBitStuffVec.resize(numUInts);
  }
  catch( const std::exception& )
  {
    return false;
  }

  m_tmpBitStuffVec[numUInts - 1] = 0;    // set last uint to 0

  memcpy(&m_tmpBitStuffVec[0], *ppByte, numBytesUsed);

  // do the un-stuffing
  unsigned int* srcPtr = &m_tmpBitStuffVec[0];
  unsigned int* dstPtr = &dataVec[0];
  int bitPos = 0;
  int nb = 32 - numBits;

  for (unsigned int i = 0; i < numElements; i++)
  {
    if (nb - bitPos >= 0)
    {
      *dstPtr++ = ((*srcPtr) << (nb - bitPos)) >> nb;
      bitPos += numBits;
      if (bitPos == 32)    // shift >= 32 is undefined
      {
        srcPtr++;
        bitPos = 0;
      }
    }
    else
    {
      *dstPtr = (*srcPtr++) >> bitPos;
      *dstPtr++ |= ((*srcPtr) << (64 - numBits - bitPos)) >> nb;
      bitPos -= nb;
    }
  }

  *ppByte += numBytesUsed;
  nBytesRemaining -= numBytesUsed;
  return true;
}

// -------------------------------------------------------------------------- ;

