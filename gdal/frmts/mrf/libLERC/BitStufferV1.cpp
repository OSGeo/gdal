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

#include "BitStufferV1.h"
#include <cstring>
#include <algorithm>

using namespace std;
NAMESPACE_LERC_START

// see the old stream IO functions below on how to call.
// if you change write(...) / read(...), don't forget to update computeNumBytesNeeded(...).

bool BitStufferV1::write(Byte** ppByte, const vector<unsigned int>& dataVec)
{
    if (!ppByte || dataVec.empty())
        return false;

    unsigned int maxElem = *max_element(dataVec.begin(), dataVec.end());
    int numBits = 0; // 0 to 23
    while (maxElem >> numBits)
        numBits++;
    unsigned int numElements = (unsigned int)dataVec.size();

    // use the upper 2 bits to encode the type used for numElements: Byte, ushort, or uint
    // n is 1, 2  or 4
    int n = numBytesUInt(numElements);
    const Byte bits67[5] = { 0xff, 0x80, 0x40, 0xff, 0 };
    **ppByte = static_cast<Byte>(numBits | bits67[n]);
    (*ppByte)++;
    memcpy(*ppByte, &numElements, n);
    *ppByte += n;
    if (numBits == 0)
        return true;

    int bits = 32; // Available
    unsigned int acc = 0;   // Accumulator
    for (unsigned int val : dataVec) {
        if (bits >= numBits) { // no accumulator overflow
            acc |= val << (bits - numBits);
            bits -= numBits;
        }
        else { // accum overflowing
            acc |= val >> (numBits - bits);
            memcpy(*ppByte, &acc, sizeof(acc));
            *ppByte += sizeof(acc);
            bits = 32 - (numBits - bits); // under 32
            acc = val << bits;
        }
    }

    // There are between 1 and 31 bits left to write
    int nbytes = 4;
    while (bits >= 8) {
        acc >>= 8;
        bits -= 8;
        nbytes--;
    }
    memcpy(*ppByte, &acc, nbytes);
    *ppByte += nbytes;
    return true;
}

// -------------------------------------------------------------------------- ;

bool BitStufferV1::read(Byte** ppByte, size_t& nRemainingBytes, vector<unsigned int>& dataVec, size_t nMaxBufferVecElts)
{
  if (!ppByte)
    return false;

  Byte numBitsByte = **ppByte;
  if( nRemainingBytes < 1 )
    return false;
  *ppByte += 1;
  nRemainingBytes -= 1;

  int bits67 = numBitsByte >> 6;
  numBitsByte &= 63;    // bits 0-5;
  if (numBitsByte >= 32)
      return false;

  int n = (bits67 == 0) ? 4 : 3 - bits67;
  if (n != 1 && n != 2 && n != 4) // only these values are valid
      return false;
  if (nRemainingBytes < static_cast<size_t>(n))
      return false;
  unsigned int numElements = 0;
  memcpy(&numElements, *ppByte, n);
  *ppByte += n;
  nRemainingBytes -= n;

  if(numElements > nMaxBufferVecElts)
    return false;

  int numBits = numBitsByte;
  unsigned int numUInts = (numElements * numBits + 31) / 32;
  dataVec.resize(numElements, 0);    // init with 0

  if (numUInts > 0)    // numBits can be 0
  {
    unsigned int numBytes = numUInts * sizeof(unsigned int);
    if( nRemainingBytes < numBytes )
      return false;
    unsigned int* arr = (unsigned int*)(*ppByte);

    unsigned int* srcPtr = arr;
    srcPtr += numUInts;

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
          return false;
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
          return false;
        memcpy(&srcValue, srcPtr, sizeof(unsigned int));
        srcPtr ++;
        nRemainingBytesTmp -= sizeof(unsigned);
        unsigned int n3 = srcValue << bitPos;
        *dstPtr = n3 >> (32 - numBits);
        bitPos -= (32 - numBits);
        if( nRemainingBytesTmp < sizeof(unsigned) )
          return false;
        memcpy(&srcValue, srcPtr, sizeof(unsigned int));
        *dstPtr++ |= srcValue >> (32 - bitPos);
      }
    }

    if (numBytesNotNeeded > 0)
      memcpy(srcPtr, &lastUInt, sizeof(unsigned int)); // restore the last UInt

    if( nRemainingBytes < numBytes - numBytesNotNeeded )
      return false;
    *ppByte += numBytes - numBytesNotNeeded;
     nRemainingBytes -= numBytes - numBytesNotNeeded;
  }

  return true;
}

NAMESPACE_LERC_END
