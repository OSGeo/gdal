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
// Lerc2.cpp
//

#include "Lerc2.h"
#include <cassert>

using namespace std;

#ifdef DEBUG
void LERC_BRKPNT()
{
}
#endif

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

Lerc2::Lerc2()
{
  Init();
}

// -------------------------------------------------------------------------- ;

Lerc2::Lerc2(int nCols, int nRows, const Byte* pMaskBits)
{
  Init();
  Set(nCols, nRows, pMaskBits);
}

// -------------------------------------------------------------------------- ;

void Lerc2::Init()
{
  // Lerc 2 only works if size of int is 4 bytes
  assert(4 == sizeof(int));
  m_currentVersion    = 2;    // 2: added Huffman coding to 8 bit types DT_Char, DT_Byte;
  m_microBlockSize    = 8;
  m_maxValToQuantize  = 0;
  m_encodeMask        = true;
  m_writeDataOneSweep = false;

  m_headerInfo.RawInit();
  m_headerInfo.version = m_currentVersion;
  m_headerInfo.microBlockSize = m_microBlockSize;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::Set(int nCols, int nRows, const Byte* pMaskBits)
{
  if (!m_bitMask.SetSize(nCols, nRows))
    return false;

  if (pMaskBits)
  {
    memcpy(m_bitMask.Bits(), pMaskBits, m_bitMask.Size());
    m_headerInfo.numValidPixel = m_bitMask.CountValidBits();
  }
  else
  {
    m_headerInfo.numValidPixel = nCols * nRows;
    m_bitMask.SetAllValid();
  }

  m_headerInfo.nCols = nCols;
  m_headerInfo.nRows = nRows;

  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::Set(const BitMask2& bitMask)
{
  m_bitMask = bitMask;

  m_headerInfo.numValidPixel = m_bitMask.CountValidBits();
  m_headerInfo.nCols = m_bitMask.GetWidth();
  m_headerInfo.nRows = m_bitMask.GetHeight();

  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int Lerc2::ComputeNumBytesHeader()
{
  // header
  unsigned int numBytes = (unsigned int)FileKey().length();
  numBytes += 7 * sizeof(int);
  numBytes += 3 * sizeof(double);
  return numBytes;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::GetHeaderInfo(const Byte* pByte, size_t srcSize, struct HeaderInfo& headerInfo) const
{
  if (!pByte)
    return false;

  return ReadHeader(&pByte, srcSize, headerInfo);
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

bool Lerc2::WriteHeader(Byte** ppByte) const
{
  if (!ppByte)
    return false;

  string fileKey = FileKey();
  const HeaderInfo& hd = m_headerInfo;

  vector<int> intVec;
  intVec.push_back(m_currentVersion);
  intVec.push_back(hd.nRows);
  intVec.push_back(hd.nCols);
  intVec.push_back(hd.numValidPixel);
  intVec.push_back(hd.microBlockSize);
  intVec.push_back(hd.blobSize);
  intVec.push_back((int)hd.dt);

  vector<double> dblVec;
  dblVec.push_back(hd.maxZError);
  dblVec.push_back(hd.zMin);
  dblVec.push_back(hd.zMax);

  Byte* ptr = *ppByte;

  memcpy(ptr, fileKey.c_str(), fileKey.length());
  ptr += fileKey.length();

  memcpy( ptr, &intVec[0], intVec.size() * sizeof(int) );
  ptr += intVec.size() * sizeof(int);

  memcpy( ptr, &dblVec[0], dblVec.size() * sizeof(double) );
  ptr += dblVec.size() * sizeof(double);

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::ReadHeader(const Byte** ppByte, size_t &nRemainingSizeInOut, struct HeaderInfo& headerInfo) const
{
  if (!ppByte || !*ppByte)
    return false;

  const Byte* ptr = *ppByte;
  size_t nRemainingSize = nRemainingSizeInOut;

  string fileKey = FileKey();
  HeaderInfo& hd = headerInfo;
  hd.RawInit();

  if (nRemainingSize < fileKey.length())
    return false;
  if (0 != memcmp(ptr, fileKey.c_str(), fileKey.length()))
    return false;

  ptr += fileKey.length();
  nRemainingSize -= fileKey.length();

  if( nRemainingSize < sizeof(int) )
     return false;
  memcpy(&(hd.version), ptr, sizeof(int));
  ptr += sizeof(int);
  nRemainingSize -= sizeof(int);

  if (hd.version > m_currentVersion)    // this reader is outdated
    return false;

  std::vector<int>  intVec(7, 0);
  std::vector<double> dblVec(3, 0);

  if( nRemainingSize < sizeof(int) * (intVec.size() - 1) )
     return false;
  memcpy(&intVec[1], ptr, sizeof(int) * (intVec.size() - 1));
  ptr += sizeof(int) * (intVec.size() - 1);
  nRemainingSize -= sizeof(int) * (intVec.size() - 1);

  if( nRemainingSize < sizeof(double) * dblVec.size() )
     return false;
  memcpy(&dblVec[0], ptr, sizeof(double) * dblVec.size());
  ptr += sizeof(double) * dblVec.size();
  nRemainingSize -= sizeof(double) * dblVec.size();

  hd.nRows          = intVec[1];
  hd.nCols          = intVec[2];
  hd.numValidPixel  = intVec[3];
  hd.microBlockSize = intVec[4];
  hd.blobSize       = intVec[5];
  hd.dt             = static_cast<DataType>(intVec[6]);

  hd.maxZError      = dblVec[0];
  hd.zMin           = dblVec[1];
  hd.zMax           = dblVec[2];

  *ppByte = ptr;
  nRemainingSizeInOut = nRemainingSize;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::WriteMask(Byte** ppByte) const
{
  if (!ppByte)
    return false;

  int numValid = m_headerInfo.numValidPixel;
  int numTotal = m_headerInfo.nCols * m_headerInfo.nRows;

  bool needMask = numValid > 0 && numValid < numTotal;

  Byte* ptr = *ppByte;

  if (needMask && m_encodeMask)
  {
    Byte* pArrRLE;
    size_t numBytesRLE;
    RLE rle;
    if (!rle.compress((const Byte*)m_bitMask.Bits(), m_bitMask.Size(), &pArrRLE, numBytesRLE, false))
      return false;

    int numBytesMask = (int)numBytesRLE;
    memcpy(ptr, &numBytesMask, sizeof(int));    // num bytes for compressed mask
    ptr += sizeof(int);
    memcpy(ptr, pArrRLE, numBytesRLE);
    ptr += numBytesRLE;

    delete[] pArrRLE;
  }
  else
  {
    memset(ptr, 0, sizeof(int));
    ptr += sizeof(int);
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::ReadMask(const Byte** ppByte, size_t& nRemainingSizeInOut)
{
  if (!ppByte)
    return false;

  int numValid = m_headerInfo.numValidPixel;
  int w = m_headerInfo.nCols;
  int h = m_headerInfo.nRows;

  const Byte* ptr = *ppByte;
  size_t nRemainingSize = nRemainingSizeInOut;

  int numBytesMask;
  if( nRemainingSize < sizeof(int) )
  {
    LERC_BRKPNT();
    return false;
  }
  memcpy(&numBytesMask, ptr, sizeof(int));
  ptr += sizeof(int);
  nRemainingSize -= sizeof(int);

  if ((numValid == 0 || numValid == w * h) && (numBytesMask != 0))
    return false;

  if (!m_bitMask.SetSize(w, h))
    return false;

  if (numValid == 0)
    m_bitMask.SetAllInvalid();
  else if (numValid == w * h)
    m_bitMask.SetAllValid();
  else if (numBytesMask > 0)    // read it in
  {
    RLE rle;
    if( nRemainingSize < static_cast<size_t>(numBytesMask) )
    {
      LERC_BRKPNT();
      return false;
    }
    if (!rle.decompress(ptr, nRemainingSize, m_bitMask.Bits(), m_bitMask.Size()))
    {
      LERC_BRKPNT();
      return false;
    }

    ptr += numBytesMask;
    nRemainingSize -= numBytesMask;
  }
  // else use previous mask

  *ppByte = ptr;
  nRemainingSizeInOut = nRemainingSize;

  return true;
}

void Lerc2::SortQuantArray(const vector<unsigned int>& quantVec,
    vector<Quant>& sortedQuantVec)
{
    int numElem = (int)quantVec.size();
    sortedQuantVec.resize(numElem);

    for (int i = 0; i < numElem; i++) {
        sortedQuantVec[i].first = quantVec[i];
        sortedQuantVec[i].second = i;
    }
    sort(sortedQuantVec.begin(), sortedQuantVec.end());
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
