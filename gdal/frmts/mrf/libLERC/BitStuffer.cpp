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

#include <cstring>
#include "BitStuffer.h"

using namespace std;
using namespace LercNS;

// -------------------------------------------------------------------------- ;

bool BitStuffer::read(Byte** ppByte, size_t& nRemainingBytes, vector<unsigned int>& dataVec, size_t nMaxBufferVecElts)
{
  if (!ppByte)
    return false;

  Byte numBitsByte = **ppByte;
  if( nRemainingBytes < 1 )
  {
    LERC_BRKPNT();
    return false;
  }
  (*ppByte)++;
  nRemainingBytes -= 1;

  int bits67 = numBitsByte >> 6;
  int n = (bits67 == 0) ? 4 : 3 - bits67;

  numBitsByte &= 63;    // bits 0-5;

  unsigned int numElements = 0;
  if (!readUInt(ppByte, nRemainingBytes, numElements, n))
  {
    LERC_BRKPNT();
    return false;
  }
  if( numElements > nMaxBufferVecElts )
  {
    LERC_BRKPNT();
    return false;
  }

  if (numBitsByte >= 32)
    return false;

  int numBits = numBitsByte;
  unsigned int numUInts = (numElements * numBits + 31) / 32;
  dataVec.resize(numElements, 0);    // init with 0

  if (numUInts > 0)    // numBits can be 0
  {
    unsigned int numBytes = numUInts * sizeof(unsigned int);
    if( nRemainingBytes < numBytes )
    {
      LERC_BRKPNT();
      return false;
    }
    unsigned int* arr = (unsigned int*)(*ppByte);

    unsigned int* srcPtr = arr;
    for (unsigned int i = 0; i < numUInts; i++)
    {
      SWAP_4(*srcPtr);
      srcPtr++;
    }

    // needed to save the 0-3 bytes not used in the last UInt
    srcPtr--;
    unsigned int lastUInt;
    memcpy(&lastUInt, srcPtr, sizeof(unsigned int));
    unsigned int numBytesNotNeeded = numTailBytesNotNeeded(numElements, numBits);
    unsigned int n2 = numBytesNotNeeded;
    for (; n2; --n2)
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

    size_t nRemainingBytesTmp = nRemainingBytes;
    for (unsigned int i = 0; i < numElements; i++)
    {
      if (32 - bitPos >= numBits)
      {
        unsigned int srcValue;
        if( nRemainingBytesTmp < sizeof(unsigned) )
        {
          LERC_BRKPNT();
          return false;
        }
        memcpy(&srcValue, srcPtr, sizeof(unsigned int));
        unsigned int n3 = srcValue << bitPos;
        *dstPtr++ = n3 >> (32 - numBits);
        bitPos += numBits;
        // cppcheck-suppress shiftTooManyBits
        if (bitPos == 32)    // shift >= 32 is undefined
        {
          bitPos = 0;
          srcPtr++;
          nRemainingBytesTmp -= sizeof(unsigned);
        }
      }
      else
      {
        unsigned int srcValue;
        if( nRemainingBytesTmp < sizeof(unsigned) )
        {
          LERC_BRKPNT();
          return false;
        }
        memcpy(&srcValue, srcPtr, sizeof(unsigned int));
        srcPtr ++;
        nRemainingBytesTmp -= sizeof(unsigned);
        unsigned int n3 = srcValue << bitPos;
        *dstPtr = n3 >> (32 - numBits);
        bitPos -= (32 - numBits);
        if( nRemainingBytesTmp < sizeof(unsigned) )
        {
          LERC_BRKPNT();
          return false;
        }
        memcpy(&srcValue, srcPtr, sizeof(unsigned int));
        *dstPtr++ |= srcValue >> (32 - bitPos);
      }
    }

    if (numBytesNotNeeded > 0)
      memcpy(srcPtr, &lastUInt, sizeof(unsigned int)); // restore the last UInt

    if( nRemainingBytes < numBytes - numBytesNotNeeded )
    {
      LERC_BRKPNT();
      return false;
    }
    *ppByte += numBytes - numBytesNotNeeded;
     nRemainingBytes -= numBytes - numBytesNotNeeded;
  }

  return true;
}

// -------------------------------------------------------------------------- ;

bool BitStuffer::readUInt(Byte** ppByte, size_t& nRemainingBytes, unsigned int& k, int numBytes)
{
  Byte* ptr = *ppByte;

  if (numBytes == 1)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    k = *ptr;
  }
  else if (numBytes == 2)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    unsigned short s;
    memcpy(&s, ptr, sizeof(unsigned short));
    SWAP_2(s);
    k = s;
  }
  else if (numBytes == 4)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    memcpy(&k, ptr, sizeof(unsigned int));
    SWAP_4(k);
  }
  else
    return false;

  *ppByte = ptr + numBytes;
  nRemainingBytes -= numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int BitStuffer::numTailBytesNotNeeded(unsigned int numElem, int numBits)
{
  int numBitsTail = (numElem * numBits) & 31;
  int numBytesTail = (numBitsTail + 7) >> 3;
  return (numBytesTail > 0) ? 4 - numBytesTail : 0;
}

// -------------------------------------------------------------------------- ;
