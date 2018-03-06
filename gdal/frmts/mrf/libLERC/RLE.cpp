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
#include "RLE.h"
#include <cstring>

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

size_t RLE::computeNumBytesRLE(const Byte* arr, size_t numBytes) const
{
  if (arr == nullptr || numBytes == 0)
    return 0;

  const Byte* ptr = arr;
  size_t sum = 0;
  size_t cntOdd = 0;
  size_t cntEven = 0;
  size_t cntTotal = 0;
  bool bOdd = true;

  while (cntTotal < numBytes - 1)
  {
    if (*ptr != *(ptr + 1))
    {
      if (bOdd)
      {
        cntOdd++;
      }
      else    // switch to odd mode
      {
        sum += 2 + 1;
        bOdd = true;
        cntOdd = 0;
        cntEven = 0;
      }
    }
    else
    {
      if (!bOdd)
      {
        cntEven++;
      }
      else
      {
        // count if we have enough equal bytes so it is worth switching to even
        bool foundEnough = false;
        if (cntTotal + m_minNumEven < numBytes)
        {
          int i = 1;
          while (i < m_minNumEven && ptr[i] == ptr[0]) i++;
          foundEnough = i < m_minNumEven ? false : true;
        }

        if (!foundEnough)    // stay in odd mode
        {
          cntOdd++;
        }
        else    // switch to even mode
        {
          if (cntOdd > 0)
            sum += 2 + cntOdd;
          bOdd = false;
          cntOdd = 0;
          cntEven = 0;
          cntEven++;
        }
      }
    }
    ptr++;
    cntTotal++;

    if (cntOdd == 32767)    // prevent short counters from overflow
    {
      sum += 2 + 32767;
      cntOdd = 0;
    }
    if (cntEven == 32767)
    {
      sum += 2 + 1;
      cntEven = 0;
    }
  }

  // don't forget the last byte
  if (bOdd)
  {
    cntOdd++;
    sum += 2 + cntOdd;
  }
  else
  {
    /* cntEven++; */
    sum += 2 + 1;
  }

  return sum + 2;    // EOF short
}

// -------------------------------------------------------------------------- ;

bool RLE::compress(const Byte* arr, size_t numBytes,
                   Byte** arrRLE, size_t& numBytesRLE, bool verify) const
{
  if (arr == nullptr || numBytes == 0)
    return false;

  numBytesRLE = computeNumBytesRLE(arr, numBytes);

  *arrRLE = new Byte[numBytesRLE];
  if (!*arrRLE)
    return false;

  const Byte* srcPtr = arr;
  Byte* cntPtr = *arrRLE;
  Byte* dstPtr = cntPtr + 2;
  size_t cntOdd = 0;
  size_t cntEven = 0;
  size_t cntTotal = 0;
  bool bOdd = true;

  while (cntTotal < numBytes - 1)
  {
    if (*srcPtr != *(srcPtr + 1))
    {
      *dstPtr++ = *srcPtr;
      if (bOdd)
      {
        cntOdd++;
      }
      else    // switch to odd mode
      {
        cntEven++;
        writeCount(-(short)cntEven, &cntPtr, &dstPtr);    // - sign for even cnts
        bOdd = true;
        cntOdd = 0;
        cntEven = 0;
      }
    }
    else
    {
      if (!bOdd)
      {
        cntEven++;
      }
      else
      {
        // count if we have enough equal bytes so it is worth switching to even
        bool foundEnough = false;
        if (cntTotal + m_minNumEven < numBytes)
        {
          int i = 1;
          while (i < m_minNumEven && srcPtr[i] == srcPtr[0]) i++;
          foundEnough = i < m_minNumEven ? false : true;
        }

        if (!foundEnough)    // stay in odd mode
        {
          *dstPtr++ = *srcPtr;
          cntOdd++;
        }
        else    // switch to even mode
        {
          if (cntOdd > 0)
          {
            writeCount((short)cntOdd, &cntPtr, &dstPtr);    // + sign for odd cnts
          }
          bOdd = false;
          cntOdd = 0;
          cntEven = 0;
          cntEven++;
        }
      }
    }

    if (cntOdd == 32767)    // prevent short counters from overflow
    {
      writeCount((short)cntOdd, &cntPtr, &dstPtr);
      cntOdd = 0;
    }
    if (cntEven == 32767)
    {
      *dstPtr++ = *srcPtr;
      writeCount(-(short)cntEven, &cntPtr, &dstPtr);
      cntEven = 0;
    }

    srcPtr++;
    cntTotal++;
  }

  // don't forget the last byte
  *dstPtr++ = *srcPtr;
  if (bOdd)
  {
    cntOdd++;
    writeCount((short)cntOdd, &cntPtr, &dstPtr);
  }
  else
  {
    cntEven++;
    writeCount(-(short)cntEven, &cntPtr, &dstPtr);
  }

  writeCount(-32768, &cntPtr, &dstPtr);    // write end of stream symbol

  if (verify)
  {
    Byte* arr2 = nullptr;
    size_t numBytes2 = 0;
    if (!decompress(*arrRLE, numBytesRLE, &arr2, numBytes2) || numBytes2 != numBytes)
    {
      delete[] arr2;
      return false;
    }
    int nCheck = memcmp(arr, arr2, numBytes);
    delete[] arr2;
    if (nCheck != 0)
      return false;
  }

  return true;
}

// -------------------------------------------------------------------------- ;

bool RLE::decompress(const Byte* arrRLE, size_t nRemainingSizeIn, Byte** arr, size_t& numBytes) const
{
  if (!arrRLE)
    return false;

  // first count the encoded bytes
  const Byte* srcPtr = arrRLE;
  size_t sum = 0;
  size_t nRemainingSize = nRemainingSizeIn;
  if( nRemainingSize < 2 )
  {
    LERC_BRKPNT();
    return false;
  }
  short cnt = readCount(&srcPtr);
  nRemainingSize -= 2;
  while (cnt != -32768)
  {
    sum += (cnt < 0) ? -cnt : cnt;
    size_t nInc = (cnt > 0) ? cnt : 1;
    if( nRemainingSize < nInc )
    {
        LERC_BRKPNT();
        return false;
    }
    srcPtr += nInc;
    nRemainingSize -= nInc;
    if( nRemainingSize < 2 )
    {
      LERC_BRKPNT();
      return false;
    }
    cnt = readCount(&srcPtr);
    nRemainingSize -= 2;
  }

  numBytes = sum;
  if( numBytes == 0 )
  {
    *arr = nullptr;
    return true;
  }
  *arr = new Byte[numBytes];
  if (!*arr)
    return false;

  return decompress(arrRLE, nRemainingSizeIn, *arr, numBytes);
}

// -------------------------------------------------------------------------- ;

bool RLE::decompress(const Byte* arrRLE, size_t nRemainingSize, Byte* arr, size_t arrSize)
{
  if (!arrRLE || !arr)
    return false;

  const Byte* srcPtr = arrRLE;
  size_t arrIdx = 0;
  if( nRemainingSize < 2 )
  {
    LERC_BRKPNT();
    return false;
  }
  short cnt = readCount(&srcPtr);
  nRemainingSize -= 2;
  while (cnt != -32768)
  {
    int i = (cnt < 0) ? -cnt: cnt ;
    if (cnt > 0)
    {
      if( nRemainingSize < static_cast<size_t>(i) )
      {
        LERC_BRKPNT();
        return false;
      }
      nRemainingSize -= i;
      if( arrIdx + i > arrSize )
      {
        LERC_BRKPNT();
        return false;
      }
      while (i--) arr[arrIdx++] = *srcPtr++;
    }
    else
    {
      if( nRemainingSize < 1 )
      {
        LERC_BRKPNT();
        return false;
      }
      nRemainingSize -= 1;
      Byte b = *srcPtr++;
      if( arrIdx + i > arrSize )
      {
        LERC_BRKPNT();
        return false;
      }
      while (i--) arr[arrIdx++] = b;
    }
    if( nRemainingSize < 2 )
    {
        LERC_BRKPNT();
        return false;
    }
    cnt = readCount(&srcPtr);
    nRemainingSize -= 2;
  }

  return true;
}

// -------------------------------------------------------------------------- ;

void RLE::writeCount(short cnt, Byte** ppCnt, Byte** ppDst)
{
  SWAP_2(cnt);    // write shorts in little endian byte order
  memcpy(*ppCnt, &cnt, sizeof(short));
  *ppCnt = *ppDst;
  *ppDst += 2;
}

// -------------------------------------------------------------------------- ;

short RLE::readCount(const Byte** ppCnt)
{
  short cnt;
  memcpy(&cnt, *ppCnt, sizeof(short));
  SWAP_2(cnt);
  *ppCnt += 2;
  return cnt;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
