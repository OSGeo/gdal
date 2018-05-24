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
#include <algorithm>
#include "CntZImage.h"
#include "BitStuffer.h"
#include "BitMask.h"
#include "Defines.h"
#include "RLE.h"

using namespace std;
using namespace LercNS;

// -------------------------------------------------------------------------- ;

CntZImage::CntZImage()
{
  type_                    = CNT_Z;
  m_bDecoderCanIgnoreMask  = false;

  memset(&m_infoFromComputeNumBytes, 0, sizeof(m_infoFromComputeNumBytes));
}

// -------------------------------------------------------------------------- ;

bool CntZImage::resizeFill0(int width, int height)
{
  if (!resize(width, height))
    return false;

  memset(getData(), 0, width * height * sizeof(CntZ));
  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int CntZImage::computeNumBytesNeededToReadHeader()
{
  CntZImage zImg;
  unsigned int cnt = (unsigned int)zImg.getTypeString().length();  // "CntZImage ", 10 bytes
  cnt += 4 * sizeof(int);       // version, type, width, height
  cnt += 1 * sizeof(double);    // maxZError
  cnt += 3 * sizeof(int) + sizeof(float);    // cnt part
  cnt += 3 * sizeof(int) + sizeof(float);    // z part
  cnt += 1;
  return cnt;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::read(Byte** ppByte,
                     size_t& nRemainingBytes,
                     double maxZError,
                     bool onlyHeader,
                     bool onlyZPart)
{
  assert(ppByte && *ppByte);

  size_t len = getTypeString().length();
  string typeStr(len, '0');

  if( nRemainingBytes < len )
  {
    LERC_BRKPNT();
    return false; 
  }
  memcpy(&typeStr[0], *ppByte, len);
  *ppByte += len;
  nRemainingBytes -= len;

  if (typeStr != getTypeString())
  {
    return false;
  }

  int version = 0, type = 0;
  int width = 0, height = 0;
  double maxZErrorInFile = 0;

  if( nRemainingBytes < 4 * sizeof(int) + sizeof(double) )
  {
    LERC_BRKPNT();
    return false;
  }
  Byte* ptr = *ppByte;

  memcpy(&version, ptr, sizeof(int));  ptr += sizeof(int);
  memcpy(&type, ptr, sizeof(int));  ptr += sizeof(int);
  memcpy(&height, ptr, sizeof(int));  ptr += sizeof(int);
  memcpy(&width, ptr, sizeof(int));  ptr += sizeof(int);
  memcpy(&maxZErrorInFile, ptr, sizeof(double));  ptr += sizeof(double);

  *ppByte = ptr;
  nRemainingBytes -= 4 * sizeof(int) + sizeof(double);

  SWAP_4(version);
  SWAP_4(type);
  SWAP_4(height);
  SWAP_4(width);
  SWAP_8(maxZErrorInFile);

  if (version != 11 || type != type_)
    return false;

  if (width <= 0 || width > 20000 || height <= 0 || height > 20000)
    return false;
  // To avoid excessive memory allocation attempts
  if (width * height > 1800 * 1000 * 1000 / static_cast<int>(sizeof(CntZ)))
    return false;

  if (maxZErrorInFile > maxZError)
    return false;

  if (onlyHeader)
    return true;

  if (!onlyZPart && !resizeFill0(width, height))    // init with (0,0), used below
    return false;

  m_bDecoderCanIgnoreMask = false;

  for (int iPart = 0; iPart < 2; iPart++)
  {
    bool zPart = iPart ? true : false;    // first cnt part, then z part

    if (!zPart && onlyZPart)
      continue;

    int numTilesVert = 0, numTilesHori = 0, numBytes = 0;
    float maxValInImg = 0;

    if( nRemainingBytes < 3 * sizeof(int) + sizeof(float) )
    {
      LERC_BRKPNT();
      return false;
    }
    ptr = *ppByte;
    memcpy(&numTilesVert, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&numTilesHori, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&numBytes, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&maxValInImg, ptr, sizeof(float));  ptr += sizeof(float);

    *ppByte = ptr;
    nRemainingBytes -= 3 * sizeof(int) + sizeof(float);
    Byte *bArr = ptr;

    SWAP_4(numTilesVert);
    SWAP_4(numTilesHori);
    SWAP_4(numBytes);
    SWAP_4(maxValInImg);

    if (!zPart && numTilesVert == 0 && numTilesHori == 0)    // no tiling for this cnt part
    {
      if (numBytes == 0)    // cnt part is const
      {
        CntZ* dstPtr = getData();
        for (int i = 0; i < height_; i++)
          for (int j = 0; j < width_; j++)
          {
            dstPtr->cnt = maxValInImg;
            dstPtr++;
          }

        if (maxValInImg > 0)
          m_bDecoderCanIgnoreMask = true;
      }

      if (numBytes > 0)    // cnt part is binary mask, use fast RLE class
      {
        // decompress to bit mask
        BitMask bitMask(width_, height_);
        RLE rle;
        if (!rle.decompress(bArr, nRemainingBytes, (Byte*)bitMask.Bits(), bitMask.Size()))
          return false;

        CntZ* dstPtr = getData();
        for (int k = 0, i = 0; i < height_; i++)
          for (int j = 0; j < width_; j++, k++, dstPtr++)
            dstPtr->cnt = bitMask.IsValid(k) ? 1.0f : 0.0f;
      }
    }
    else if (!readTiles(zPart, maxZErrorInFile, numTilesVert, numTilesHori, maxValInImg, bArr, nRemainingBytes))
    {
      LERC_BRKPNT();
      return false;
    }

    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    *ppByte += numBytes;
    nRemainingBytes -= numBytes;
  }

  m_tmpDataVec.clear();
  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readTiles(bool zPart, double maxZErrorInFile,
                          int numTilesVert, int numTilesHori, float maxValInImg,
                          Byte* bArr, size_t nRemainingBytes)
{
  Byte* ptr = bArr;

  for (int iTile = 0; iTile <= numTilesVert; iTile++)
  {
    int tileH = static_cast<int>(height_ / numTilesVert);
    int i0 = iTile * tileH;
    if (iTile == numTilesVert)
      tileH = height_ % numTilesVert;

    if (tileH == 0)
      continue;

    for (int jTile = 0; jTile <= numTilesHori; jTile++)
    {
      int tileW = static_cast<int>(width_ / numTilesHori);
      int j0 = jTile * tileW;
      if (jTile == numTilesHori)
        tileW = width_ % numTilesHori;

      if (tileW == 0)
        continue;

      bool rv = zPart ? readZTile(  &ptr, nRemainingBytes, i0, i0 + tileH, j0, j0 + tileW, maxZErrorInFile, maxValInImg) :
                        readCntTile(&ptr, nRemainingBytes, i0, i0 + tileH, j0, j0 + tileW);

      if (!rv)
        return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readCntTile(Byte** ppByte, size_t& nRemainingBytesInOut, int i0, int i1, int j0, int j1)
{
  size_t nRemainingBytes = nRemainingBytesInOut;
  Byte* ptr = *ppByte;
  int numPixel = (i1 - i0) * (j1 - j0);

  if( nRemainingBytes < 1 )
  {
    LERC_BRKPNT();
    return false;
  }
  Byte comprFlag = *ptr++;
  nRemainingBytes -= 1;

  if (comprFlag == 2)    // entire tile is constant 0 (invalid)
  {                      // here we depend on resizeFill0()
    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
  }

  if (comprFlag == 3 || comprFlag == 4)    // entire tile is constant -1 (invalid) or 1 (valid)
  {
    CntZ cz1m = {-1, 0};
    CntZ cz1p = { 1, 0};
    CntZ cz1 = (comprFlag == 3) ? cz1m : cz1p;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
        *dstPtr++ = cz1;
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
  }

  if ((comprFlag & 63) > 4)
    return false;

  if (comprFlag == 0)
  {
    // read cnt's as flt arr uncompressed
    const float* srcPtr = (const float*)ptr;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if( nRemainingBytes < sizeof(float) )
        {
          LERC_BRKPNT();
          return false;
        }
        dstPtr->cnt = *srcPtr++;
        nRemainingBytes -= sizeof(float);
        SWAP_4(dstPtr->cnt);
        dstPtr++;
      }
    }

    ptr += numPixel * sizeof(float);
  }
  else
  {
    // read cnt's as int arr bit stuffed
    int bits67 = comprFlag >> 6;
    int n = (bits67 == 0) ? 4 : 3 - bits67;

    float offset = 0;
    if (!readFlt(&ptr, nRemainingBytes, offset, n))
    {
      LERC_BRKPNT();
      return false;
    }

    vector<unsigned int>& dataVec = m_tmpDataVec;
    BitStuffer bitStuffer;
    size_t nMaxElts =
            static_cast<size_t>(i1-i0) * static_cast<size_t>(j1-j0);
    if (!bitStuffer.read(&ptr, nRemainingBytes, dataVec, nMaxElts))
    {
      LERC_BRKPNT();
      return false;
    }

    size_t dataVecIdx = 0;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if( dataVecIdx == dataVec.size() )
        {
          LERC_BRKPNT();
          return false;
        }
        dstPtr->cnt = offset + (float)dataVec[dataVecIdx];
        dataVecIdx ++;
        dstPtr++;
      }
    }
  }

  *ppByte = ptr;
  nRemainingBytesInOut = nRemainingBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readZTile(Byte** ppByte, size_t& nRemainingBytesInOut,
                          int i0, int i1, int j0, int j1,
                          double maxZErrorInFile, float maxZInImg)
{
  size_t nRemainingBytes = nRemainingBytesInOut;
  Byte* ptr = *ppByte;
  int numPixel = 0;

  if( nRemainingBytes < 1 )
  {
    LERC_BRKPNT();
    return false;
  }
  Byte comprFlag = *ptr++;
  nRemainingBytes -= 1;
  int bits67 = comprFlag >> 6;
  comprFlag &= 63;

  if (comprFlag == 2)    // entire zTile is constant 0 (if valid or invalid doesn't matter)
  {
    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if (dstPtr->cnt > 0)
          dstPtr->z = 0;
        dstPtr++;
      }
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
  }

  if (comprFlag > 3)
    return false;

  if (comprFlag == 0)
  {
    // read z's as flt arr uncompressed
    const float* srcPtr = (const float*)ptr;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if (dstPtr->cnt > 0)
        {
          if( nRemainingBytes < sizeof(float) )
          {
            LERC_BRKPNT();
            return false;
          }
          dstPtr->z = *srcPtr++;
          nRemainingBytes -= sizeof(float);
          SWAP_4(dstPtr->z);
          numPixel++;
        }
        dstPtr++;
      }
    }

    ptr += numPixel * sizeof(float);
  }
  else
  {
    // read z's as int arr bit stuffed
    int n = (bits67 == 0) ? 4 : 3 - bits67;
    float offset = 0;
    if (!readFlt(&ptr, nRemainingBytes, offset, n))
    {
      LERC_BRKPNT();
      return false;
    }

    if (comprFlag == 3)
    {
      for (int i = i0; i < i1; i++)
      {
        CntZ* dstPtr = getData() + i * width_ + j0;
        for (int j = j0; j < j1; j++)
        {
          if (dstPtr->cnt > 0)
            dstPtr->z = offset;
          dstPtr++;
        }
      }
    }
    else
    {
      vector<unsigned int>& dataVec = m_tmpDataVec;
      BitStuffer bitStuffer;
      size_t nMaxElts =
            static_cast<size_t>(i1-i0) * static_cast<size_t>(j1-j0);
      if (!bitStuffer.read(&ptr, nRemainingBytes, dataVec, nMaxElts))
      {
        LERC_BRKPNT();
        return false;
      }

      double invScale = 2 * maxZErrorInFile;
      size_t nDataVecIdx = 0;

      if (m_bDecoderCanIgnoreMask)
      {
        if( static_cast<size_t>(i1 -i0) * (j1 -j0) < dataVec.size() )
          return false;
        for (int i = i0; i < i1; i++)
        {
          CntZ* dstPtr = getData() + i * width_ + j0;

          for (int j = j0; j < j1; j++)
          {
            float z = (float)(offset + dataVec[nDataVecIdx] * invScale);
            nDataVecIdx ++;
            dstPtr->z = std::min(z, maxZInImg);    // make sure we stay in the orig range
            dstPtr++;
          }
        }
      }
      else
      {
        for (int i = i0; i < i1; i++)
        {
          CntZ* dstPtr = getData() + i * width_ + j0;
          for (int j = j0; j < j1; j++)
          {
            if (dstPtr->cnt > 0)
            {
              if( nDataVecIdx == dataVec.size() )
                 return false;
              float z = (float)(offset + dataVec[nDataVecIdx] * invScale);
              nDataVecIdx ++;
              dstPtr->z = std::min(z, maxZInImg);    // make sure we stay in the orig range
            }
            dstPtr++;
          }
        }
      }
    }
  }

  *ppByte = ptr;
  nRemainingBytesInOut = nRemainingBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

int CntZImage::numBytesFlt(float z)
{
  short s = (short)z;
  signed char c = static_cast<signed char>(s);
  return ((float)c == z) ? 1 : ((float)s == z) ? 2 : 4;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readFlt(Byte** ppByte, size_t& nRemainingBytes, float& z, int numBytes)
{
  Byte* ptr = *ppByte;

  if (numBytes == 1)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    char c = *((char*)ptr);
    z = c;
  }
  else if (numBytes == 2)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    short s = *((short*)ptr);
    SWAP_2(s);
    z = s;
  }
  else if (numBytes == 4)
  {
    if( nRemainingBytes < static_cast<size_t>(numBytes) )
    {
      LERC_BRKPNT();
      return false;
    }
    z = *((float*)ptr);
    SWAP_4(z);
  }
  else
    return false;

  *ppByte = ptr + numBytes;
  nRemainingBytes -= numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

