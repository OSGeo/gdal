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

#include "BitStuffer2.h"
#include <algorithm>
#include <cstring>

using namespace std;

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

// if you change Encode(...) / Decode(...), don't forget to update ComputeNumBytesNeeded(...)

bool BitStuffer2::EncodeSimple(Byte** ppByte, const vector<unsigned int>& dataVec)
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
    BitStuff(ppByte, dataVec, numBits);

  return true;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer2::EncodeLut(Byte** ppByte,
                            const vector<Quant>& sortedDataVec) const
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
    unsigned int prev = static_cast<unsigned int>(sortedDataVec[i - 1].first);
    m_tmpIndexVec[sortedDataVec[i - 1].second] = indexLut;

    if (sortedDataVec[i].first != prev)
    {
      m_tmpLutVec.push_back(static_cast<unsigned int>(sortedDataVec[i].first));
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

  BitStuff(ppByte, m_tmpLutVec, numBits);    // lut

  int nBitsLut = 0;
  while (nLut >> nBitsLut)
    nBitsLut++;

  BitStuff(ppByte, m_tmpIndexVec, nBitsLut);    // indexes

  return true;
}

// -------------------------------------------------------------------------- ;

// if you change Encode(...) / Decode(...), don't forget to update ComputeNumBytesNeeded(...)

bool BitStuffer2::Decode(const Byte** ppByte, size_t& nRemainingBytes, vector<unsigned int>& dataVec, size_t nMaxBufferVecElts) const
{
  if (!ppByte)
    return false;

  if( nRemainingBytes < 1 )
  {
     LERC_BRKPNT();
     return false;
  }
  Byte numBitsByte = **ppByte;
  (*ppByte)++;
  nRemainingBytes -= 1;

  int bits67 = numBitsByte >> 6;
  int n = (bits67 == 0) ? 4 : 3 - bits67;

  bool doLut = (numBitsByte & (1 << 5)) ? true : false;    // bit 5
  numBitsByte &= 31;    // bits 0-4;

  unsigned int numElements = 0;
  if (!DecodeUInt(ppByte, nRemainingBytes, numElements, n))
  {
    LERC_BRKPNT();
    return false;
  }
  // To avoid excessive memory allocation attempts
  if( numElements > nMaxBufferVecElts )
  {
    LERC_BRKPNT();
    return false;
  }

  int numBits = numBitsByte;
  dataVec.resize(numElements, 0);    // init with 0

  if (!doLut)
  {
    if (numBits > 0)    // numBits can be 0
      if( !BitUnStuff(ppByte, nRemainingBytes, dataVec, numElements, numBits) )
      {
        LERC_BRKPNT();
        return false;
      }
  }
  else
  {
    if( numBits == 0 )
    {
      LERC_BRKPNT();
      return false;
    }
    if( nRemainingBytes < 1 )
    {
      LERC_BRKPNT();
      return false;
    }
    Byte nLutByte = **ppByte;
    (*ppByte)++;
    nRemainingBytes -= 1;

    if( nLutByte == 0 )
    {
      LERC_BRKPNT();
      return false;
    }
    int nLut = nLutByte - 1;
    // unstuff lut w/o the 0
    if( !BitUnStuff(ppByte, nRemainingBytes, m_tmpLutVec, nLut, numBits) )
    {
        LERC_BRKPNT();
        return false;
    }

    int nBitsLut = 0;
    while (nLut >> nBitsLut)
      nBitsLut++;
    if( nBitsLut == 0 )
    {
      LERC_BRKPNT();
      return false;
    }

    // unstuff indexes
    if( !BitUnStuff(ppByte, nRemainingBytes, dataVec, numElements, nBitsLut) )
    {
      LERC_BRKPNT();
      return false;
    }

    // replace indexes by values
    m_tmpLutVec.insert(m_tmpLutVec.begin(), 0);    // put back in the 0
    for (unsigned int i = 0; i < numElements; i++)
    {
      if( dataVec[i] >= m_tmpLutVec.size() )
      {
        LERC_BRKPNT();
        return false;
      }
      dataVec[i] = m_tmpLutVec[dataVec[i]];
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int BitStuffer2::ComputeNumBytesNeededLut(const vector<Quant >& sortedDataVec,
                                                    bool& doLut)
{
  unsigned int maxElem = static_cast<unsigned int>(sortedDataVec.back().first);
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

void BitStuffer2::BitStuff(Byte** ppByte, const vector<unsigned int>& dataVec, int numBits)
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
      dstValue |= (*srcPtr  ) >> n;
      memcpy(dstPtr, &dstValue, sizeof(unsigned int));
      dstPtr ++;
      memcpy(&dstValue, dstPtr, sizeof(unsigned int));
      dstValue |= (*srcPtr++) << (32 - n);
      memcpy(dstPtr, &dstValue, sizeof(unsigned int));
      bitPos = n;
    }
  }

  // save the 0-3 bytes not used in the last UInt
  unsigned int numBytesNotNeeded = NumTailBytesNotNeeded(numElements, numBits);
  unsigned int n = numBytesNotNeeded;
  for (;n;--n)
  {
    unsigned int dstValue;
    memcpy(&dstValue, dstPtr, sizeof(unsigned int));
    dstValue >>= 8;
    memcpy(dstPtr, &dstValue, sizeof(unsigned int));
  }

  *ppByte += numBytes - numBytesNotNeeded;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer2::BitUnStuff(const Byte** ppByte, 
                             size_t& nRemainingBytes,
                             vector<unsigned int>& dataVec,
                             unsigned int numElements, int numBits) const
{
  try
  {
    dataVec.resize(numElements, 0);    // init with 0
  }
  catch( const std::bad_alloc& )
  {
    return false;
  }

  unsigned int numUInts = (numElements * numBits + 31) / 32;
  unsigned int numBytes = numUInts * sizeof(unsigned int);
  unsigned int* arr = (unsigned int*)(*ppByte);

  if( nRemainingBytes < numBytes )
  {
    LERC_BRKPNT();
    return false;
  }
  unsigned int* srcPtr = arr;
  srcPtr += numUInts;

  // needed to save the 0-3 bytes not used in the last UInt
  srcPtr--;
  unsigned int lastUInt;
  memcpy(&lastUInt, srcPtr, sizeof(unsigned int));
  unsigned int numBytesNotNeeded = NumTailBytesNotNeeded(numElements, numBits);
  unsigned int n = numBytesNotNeeded;
  for(;n;--n)
  {
    unsigned int srcValue;
    memcpy(&srcValue, srcPtr, sizeof(unsigned int));
    srcValue <<= 8;
    memcpy(srcPtr, &srcValue, sizeof(unsigned int));
  }

  // do the un-stuffing
  srcPtr = arr;
  unsigned int* dstPtr = &dataVec[0];
  int bitPos = 0;

  for (unsigned int i = 0; i < numElements; i++)
  {
    if (32 - bitPos >= numBits)
    {
      unsigned int srcValue;
      memcpy(&srcValue, srcPtr, sizeof(unsigned int));
      unsigned int n2 = srcValue << bitPos;
      *dstPtr++ = n2 >> (32 - numBits);
      bitPos += numBits;
      // cppcheck-suppress shiftTooManyBits
      if (bitPos == 32)    // shift >= 32 is undefined
      {
        bitPos = 0;
        srcPtr++;
      }
    }
    else
    {
      unsigned int srcValue;
      memcpy(&srcValue, srcPtr, sizeof(unsigned int));
      srcPtr ++;
      unsigned int n2 = srcValue << bitPos;
      *dstPtr = n2 >> (32 - numBits);
      bitPos -= (32 - numBits);
      memcpy(&srcValue, srcPtr, sizeof(unsigned int));
      *dstPtr++ |= srcValue >> (32 - bitPos);
    }
  }

  if (numBytesNotNeeded > 0)
  {
    memcpy(srcPtr, &lastUInt, sizeof(unsigned int));  // restore the last UInt
  }

  *ppByte += numBytes - numBytesNotNeeded;
  nRemainingBytes -= (numBytes - numBytesNotNeeded);
  return true;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
