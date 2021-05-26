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

Contributors:   Thomas Maurer
                Lucian Plesea (provided checksum code)
*/

#include "Defines.h"
#include "Lerc2.h"

USING_NAMESPACE_LERC
using namespace std;

static void ignore_ret_val(bool) {}

// -------------------------------------------------------------------------- ;

Lerc2::Lerc2()
{
  Init();
}

// -------------------------------------------------------------------------- ;

Lerc2::Lerc2(int nDim, int nCols, int nRows, const Byte* pMaskBits)
{
  Init();
  ignore_ret_val(Set(nDim, nCols, nRows, pMaskBits));
}

// -------------------------------------------------------------------------- ;

bool Lerc2::SetEncoderToOldVersion(int version)
{
  if (version < 2 || version > kCurrVersion)
    return false;

  if (version < 4 && m_headerInfo.nDim > 1)
    return false;

  m_headerInfo.version = version;

  return true;
}

// -------------------------------------------------------------------------- ;

void Lerc2::Init()
{
  m_microBlockSize    = 8;
  m_maxValToQuantize  = 0;
  m_encodeMask        = true;
  m_writeDataOneSweep = false;
  m_imageEncodeMode   = IEM_Tiling;

  m_headerInfo.RawInit();
  m_headerInfo.version = kCurrVersion;
  m_headerInfo.microBlockSize = m_microBlockSize;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::Set(int nDim, int nCols, int nRows, const Byte* pMaskBits)
{
  if (nDim > 1 && m_headerInfo.version < 4)
    return false;

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

  m_headerInfo.nDim  = nDim;
  m_headerInfo.nCols = nCols;
  m_headerInfo.nRows = nRows;

  return true;
}

// -------------------------------------------------------------------------- ;

//// if the Lerc2 header should ever shrink in size to less than below, then update it (very unlikely)
//
//unsigned int Lerc2::MinNumBytesNeededToReadHeader()
//{
//  unsigned int numBytes = (unsigned int)FileKey().length();
//  numBytes += 7 * sizeof(int);
//  numBytes += 3 * sizeof(double);
//  return numBytes;
//}

// -------------------------------------------------------------------------- ;

bool Lerc2::GetHeaderInfo(const Byte* pByte, size_t nBytesRemaining, struct HeaderInfo& hd)
{
  if (!pByte || !IsLittleEndianSystem())
    return false;

  return ReadHeader(&pByte, nBytesRemaining, hd);
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

unsigned int Lerc2::ComputeNumBytesHeaderToWrite(const struct HeaderInfo& hd)
{
  unsigned int numBytes = (unsigned int)FileKey().length();
  numBytes += 1 * sizeof(int);
  numBytes += (hd.version >= 3 ? 1 : 0) * sizeof(unsigned int);
  numBytes += (hd.version >= 4 ? 7 : 6) * sizeof(int);
  numBytes += 3 * sizeof(double);
  return numBytes;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::WriteHeader(Byte** ppByte, const struct HeaderInfo& hd)
{
  if (!ppByte)
    return false;

  Byte* ptr = *ppByte;

  string fileKey = FileKey();
  size_t len = fileKey.length();
  memcpy(ptr, fileKey.c_str(), len);
  ptr += len;

  memcpy(ptr, &hd.version, sizeof(int));
  ptr += sizeof(int);

  if (hd.version >= 3)
  {
    unsigned int checksum = 0;
    memcpy(ptr, &checksum, sizeof(unsigned int));    // place holder to be filled by the real check sum later
    ptr += sizeof(unsigned int);
  }

  vector<int> intVec;
  intVec.push_back(hd.nRows);
  intVec.push_back(hd.nCols);

  if (hd.version >= 4)
  {
    intVec.push_back(hd.nDim);
  }

  intVec.push_back(hd.numValidPixel);
  intVec.push_back(hd.microBlockSize);
  intVec.push_back(hd.blobSize);
  intVec.push_back((int)hd.dt);

  len = intVec.size() * sizeof(int);
  memcpy(ptr, &intVec[0], len);
  ptr += len;

  vector<double> dblVec;
  dblVec.push_back(hd.maxZError);
  dblVec.push_back(hd.zMin);
  dblVec.push_back(hd.zMax);

  len = dblVec.size() * sizeof(double);
  memcpy(ptr, &dblVec[0], len);
  ptr += len;

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::ReadHeader(const Byte** ppByte, size_t& nBytesRemainingInOut, struct HeaderInfo& hd)
{
  if (!ppByte || !*ppByte)
    return false;

  const Byte* ptr = *ppByte;
  size_t nBytesRemaining = nBytesRemainingInOut;

  string fileKey = FileKey();
  size_t keyLen = fileKey.length();

  hd.RawInit();

  if (nBytesRemaining < keyLen || memcmp(ptr, fileKey.c_str(), keyLen))
    return false;

  ptr += keyLen;
  nBytesRemaining -= keyLen;

  if (nBytesRemaining < sizeof(int) || !memcpy(&(hd.version), ptr, sizeof(int)))
    return false;

  ptr += sizeof(int);
  nBytesRemaining -= sizeof(int);

  if (hd.version > kCurrVersion)    // this reader is outdated
    return false;

  if (hd.version >= 3)
  {
    if (nBytesRemaining < sizeof(unsigned int) || !memcpy(&(hd.checksum), ptr, sizeof(unsigned int)))
      return false;

    ptr += sizeof(unsigned int);
    nBytesRemaining -= sizeof(unsigned int);
  }

  int nInts = (hd.version >= 4) ? 7 : 6;
  vector<int> intVec(nInts, 0);
  vector<double> dblVec(3, 0);

  size_t len = sizeof(int) * intVec.size();

  if (nBytesRemaining < len || !memcpy(&intVec[0], ptr, len))
    return false;

  ptr += len;
  nBytesRemaining -= len;

  len = sizeof(double) * dblVec.size();

  if (nBytesRemaining < len || !memcpy(&dblVec[0], ptr, len))
    return false;

  ptr += len;
  nBytesRemaining -= len;

  int i = 0;
  hd.nRows          = intVec[i++];
  hd.nCols          = intVec[i++];
  hd.nDim           = (hd.version >= 4) ? intVec[i++] : 1;
  hd.numValidPixel  = intVec[i++];
  hd.microBlockSize = intVec[i++];
  hd.blobSize       = intVec[i++];
  const int dt      = intVec[i++];
  if( dt < DT_Char || dt > DT_Undefined )
    return false;
  hd.dt             = static_cast<DataType>(dt);

  hd.maxZError      = dblVec[0];
  hd.zMin           = dblVec[1];
  hd.zMax           = dblVec[2];

  if (hd.nRows <= 0 || hd.nCols <= 0 || hd.nDim <= 0 || hd.numValidPixel < 0 || hd.microBlockSize <= 0 || hd.blobSize <= 0)
    return false;

  *ppByte = ptr;
  nBytesRemainingInOut = nBytesRemaining;

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
    memset(ptr, 0, sizeof(int));    // indicates no mask stored
    ptr += sizeof(int);
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::ReadMask(const Byte** ppByte, size_t& nBytesRemainingInOut)
{
  if (!ppByte)
    return false;

  int numValid = m_headerInfo.numValidPixel;
  int w = m_headerInfo.nCols;
  int h = m_headerInfo.nRows;

  const Byte* ptr = *ppByte;
  size_t nBytesRemaining = nBytesRemainingInOut;

  int numBytesMask;
  if (nBytesRemaining < sizeof(int) || !memcpy(&numBytesMask, ptr, sizeof(int)))
    return false;

  ptr += sizeof(int);
  nBytesRemaining -= sizeof(int);

  if (numValid == 0 || numValid == w * h)
  {
    if (numBytesMask != 0)
      return false;
  }

  if (!m_bitMask.SetSize(w, h))
    return false;

  if (numValid == 0)
    m_bitMask.SetAllInvalid();
  else if (numValid == w * h)
    m_bitMask.SetAllValid();
  else if (numBytesMask > 0)    // read it in
  {
    if (nBytesRemaining < static_cast<size_t>(numBytesMask))
      return false;

    RLE rle;
    if (!rle.decompress(ptr, nBytesRemaining, m_bitMask.Bits(), m_bitMask.Size()))
      return false;

    ptr += numBytesMask;
    nBytesRemaining -= numBytesMask;
  }
  // else use previous mask

  *ppByte = ptr;
  nBytesRemainingInOut = nBytesRemaining;

  return true;
}

// -------------------------------------------------------------------------- ;

bool Lerc2::DoChecksOnEncode(Byte* pBlobBegin, Byte* pBlobEnd) const
{
  if ((size_t)(pBlobEnd - pBlobBegin) != (size_t)m_headerInfo.blobSize)
    return false;

  if (m_headerInfo.version >= 3)
  {
    int blobSize = (int)(pBlobEnd - pBlobBegin);
    int nBytes = (int)(FileKey().length() + sizeof(int) + sizeof(unsigned int));    // start right after the checksum entry
    if (blobSize < nBytes)
      return false;
    unsigned int checksum = ComputeChecksumFletcher32(pBlobBegin + nBytes, blobSize - nBytes);

    nBytes -= sizeof(unsigned int);
    memcpy(pBlobBegin + nBytes, &checksum, sizeof(unsigned int));
  }

  return true;
}

// -------------------------------------------------------------------------- ;

// from  https://en.wikipedia.org/wiki/Fletcher's_checksum
// modified from ushorts to bytes (by Lucian Plesea)

unsigned int Lerc2::ComputeChecksumFletcher32(const Byte* pByte, int len)
{
  unsigned int sum1 = 0xffff, sum2 = 0xffff;
  unsigned int words = len / 2;

  while (words)
  {
    unsigned int tlen = (words >= 359) ? 359 : words;
    words -= tlen;
    do {
      sum1 += (*pByte++ << 8);
      sum2 += sum1 += *pByte++;
    } while (--tlen);

    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
  }

  // add the straggler byte if it exists
  if (len & 1)
    sum2 += sum1 += (*pByte << 8);

  // second reduction step to reduce sums to 16 bits
  sum1 = (sum1 & 0xffff) + (sum1 >> 16);
  sum2 = (sum2 & 0xffff) + (sum2 >> 16);

  return sum2 << 16 | sum1;
}

// -------------------------------------------------------------------------- ;

//struct MyLessThanOp
//{
//  inline bool operator() (const pair<unsigned int, unsigned int>& p0,
//                          const pair<unsigned int, unsigned int>& p1)  { return p0.first < p1.first; }
//};

// -------------------------------------------------------------------------- ;

void Lerc2::SortQuantArray(const vector<unsigned int>& quantVec, vector<pair<unsigned int, unsigned int> >& sortedQuantVec)
{
  int numElem = (int)quantVec.size();
  sortedQuantVec.resize(numElem);

  for (int i = 0; i < numElem; i++)
    sortedQuantVec[i] = pair<unsigned int, unsigned int>(quantVec[i], i);

  //std::sort(sortedQuantVec.begin(), sortedQuantVec.end(), MyLessThanOp());

  std::sort(sortedQuantVec.begin(), sortedQuantVec.end(),
    [](const pair<unsigned int, unsigned int>& p0,
       const pair<unsigned int, unsigned int>& p1) { return p0.first < p1.first; });
}

// -------------------------------------------------------------------------- ;

