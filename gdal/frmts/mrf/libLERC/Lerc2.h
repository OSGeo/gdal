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

#ifndef LERC2_H
#define LERC2_H

#include "BitMask2.h"
#include "BitStuffer2.h"
#include "Huffman.h"
#include "RLE.h"
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <typeinfo>
#include <cfloat>
#include <cmath>
#include <limits>

NAMESPACE_LERC_START

#define TryHuffman

/**   Lerc2
 *
 *    -- allow for lossless compression of all common data types
 *    -- avoid data type conversions and copies
 *    -- optimized compression for segmented rasters (10-15x lossless)
 *    -- micro block is 8x8 fixed, only gets doubled to 16x16 if bit rate < 1 bpp
 *    -- cnt is replaced by bit mask
 *    -- Lerc blob header has data range [min, max]
 *    -- harden consistency checks to detect if the byte blob has been tampered with
 *    -- drop support for big endian, this is legacy now
 *
 */

class Lerc2
{
public:
  Lerc2();
  Lerc2(int nCols, int nRows, const Byte* pMaskBits = NULL);    // valid / invalid bits as byte array
  virtual ~Lerc2()  {}

  bool Set(int nCols, int nRows, const Byte* pMaskBits = NULL);
  bool Set(const BitMask2& bitMask);

  template<class T>
  unsigned int ComputeNumBytesNeededToWrite(const T* arr, double maxZError, bool encodeMask);

  static unsigned int ComputeNumBytesHeader();

  static unsigned int NumExtraBytesToAllocate()  { return BitStuffer2::NumExtraBytesToAllocate(); }

  /// does not allocate memory;  byte ptr is moved like a file pointer
  template<class T>
  bool Encode(const T* arr, Byte** ppByte) const;

  // data types supported by Lerc2
  enum DataType {DT_Char, DT_Byte, DT_Short, DT_UShort, DT_Int, DT_UInt, DT_Float, DT_Double, DT_Undefined};

  struct HeaderInfo
  {
    int version,
        nCols,
        nRows,
        numValidPixel,
        microBlockSize,
        blobSize;
    DataType dt;
    double  zMin,
            zMax,
            maxZError;

    void RawInit()  { memset(this, 0, sizeof(struct HeaderInfo)); }
  };

  bool GetHeaderInfo(const Byte* pByte, size_t srcSize, struct HeaderInfo& headerInfo) const;

  /// does not allocate memory;  byte ptr is moved like a file pointer
  template<class T>
  bool Decode(const Byte** ppByte, size_t& nRemainingBytes, T* arr, Byte* pMaskBits = 0);    // if mask ptr is not 0, mask bits are returned (even if all valid or same as previous)

private:
  int         m_currentVersion,
              m_microBlockSize,
              m_maxValToQuantize;
  BitMask2    m_bitMask;
  HeaderInfo  m_headerInfo;
  BitStuffer2 m_bitStuffer2;
  bool        m_encodeMask,
              m_writeDataOneSweep;

  mutable std::vector<std::pair<short, unsigned int> > m_huffmanCodes;    // <= 256 codes, 1.5 kB

private:
  static std::string FileKey() { return "Lerc2 "; }
  void Init();
  bool WriteHeader(Byte** ppByte) const;
  bool ReadHeader(const Byte** ppByte, size_t& nRemainingBytes, struct HeaderInfo& headerInfo) const;
  bool WriteMask(Byte** ppByte) const;
  bool ReadMask(const Byte** ppByte, size_t& nRemainingBytes);

  template<class T>
  bool WriteDataOneSweep(const T* data, Byte** ppByte) const;

  template<class T>
  bool ReadDataOneSweep(const Byte** ppByte, size_t& nRemainingBytes, T* data) const;

  template<class T>
  bool WriteTiles(const T* data, Byte** ppByte, int& numBytes, double& zMinA, double& zMaxA) const;

  template<class T>
  bool ReadTiles(const Byte** ppByte, size_t& nRemainingBytes, T* data) const;

  template<class T>
  bool ComputeStats(const T* data, int i0, int i1, int j0, int j1,
                    T& zMinA, T& zMaxA, int& numValidPixelA, bool& tryLutA) const;

  static double ComputeMaxVal(double zMin, double zMax, double maxZError);

  template<class T>
  bool NeedToQuantize(int numValidPixel, T zMin, T zMax) const;

  template<class T>
  bool Quantize(const T* data, int i0, int i1, int j0, int j1, T zMin, int numValidPixel,
                std::vector<unsigned int>& quantVec) const;

  template<class T>
  int NumBytesTile(int numValidPixel, T zMin, T zMax, bool& tryLut,
                   const std::vector<Quant >& sortedQuantVec) const;

  template<class T>
  bool WriteTile(const T* data, Byte** ppByte, int& numBytesWritten,
                 int i0, int i1, int j0, int j1, int numValidPixel, T zMin, T zMax,
                 const std::vector<unsigned int>& quantVec, bool doLut,
                 const std::vector<Quant >& sortedQuantVec) const;

  template<class T>
  bool ReadTile(const Byte** ppByte, size_t& nRemainingBytes,
                T* data, int i0, int i1, int j0, int j1,
                std::vector<unsigned int>& bufferVec) const;

  template<class T>
  int TypeCode(T z, DataType& dtUsed) const;

  DataType GetDataTypeUsed(int typeCode) const;

  static bool WriteVariableDataType(Byte** ppByte, double z, DataType dtUsed);

  static bool ReadVariableDataType(const Byte** ppByte, size_t& nRemainingBytes, DataType dtUsed, double* pdfOutVal);

  // cppcheck-suppress functionStatic
  template<class T> DataType GetDataType(T z) const;

  static unsigned int GetMaxValToQuantize(DataType dt);

  static void SortQuantArray(const std::vector<unsigned int>& quantVec,
                      std::vector<Quant>& sortedQuantVec);

  template<class T>
  bool ComputeHistoForHuffman(const T* data, std::vector<int>& histo) const;

  template<class T>
  bool EncodeHuffman(const T* data, Byte** ppByte, T& zMinA, T& zMaxA) const;

  template<class T>
  bool DecodeHuffman(const Byte** ppByte, size_t& nRemainingBytes, T* data) const;
};

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

template<class T>
unsigned int Lerc2::ComputeNumBytesNeededToWrite(const T* arr, double maxZError, bool encodeMask)
{
  if (!arr)
    return 0;

  // header
  unsigned int numBytes = (unsigned int)FileKey().length();
  numBytes += 7 * sizeof(int);
  numBytes += 3 * sizeof(double);

  // valid / invalid mask
  int numValid = m_headerInfo.numValidPixel;
  int numTotal = m_headerInfo.nCols * m_headerInfo.nRows;

  bool needMask = numValid > 0 && numValid < numTotal;

  m_encodeMask = encodeMask;

  numBytes += 1 * sizeof(int);    // the mask encode numBytes

  if (needMask && encodeMask)
  {
    RLE rle;
    size_t n = rle.computeNumBytesRLE((const Byte*)m_bitMask.Bits(), m_bitMask.Size());
    numBytes += (unsigned int)n;
  }

  m_headerInfo.dt = GetDataType(arr[0]);

  if (m_headerInfo.dt == DT_Undefined)
    return 0;

  if (m_headerInfo.dt < DT_Float)
    maxZError = std::max(0.5, std::floor(maxZError));

  m_headerInfo.maxZError = maxZError;
  m_headerInfo.zMin = 0;
  m_headerInfo.zMax = 0;
  m_headerInfo.microBlockSize = m_microBlockSize;
  m_headerInfo.blobSize = numBytes;

  if (numValid == 0)
    return numBytes;

  m_maxValToQuantize = GetMaxValToQuantize(m_headerInfo.dt);

  // data
  m_writeDataOneSweep = false;
  int nBytes = 0;
  Byte* ptr = NULL;    // only emulate the writing and just count the bytes needed

  if (!WriteTiles(arr, &ptr, nBytes, m_headerInfo.zMin, m_headerInfo.zMax))
    return 0;

  if (m_headerInfo.zMin == m_headerInfo.zMax)    // image is const
    return numBytes;

  bool bHuffmanLostRun1 = m_huffmanCodes.empty();

  int nBytesOneSweep = (int)(numValid * sizeof(T));

  // if resulting bit rate < x (1 bpp), try with double block size to reduce block header overhead
  if ((nBytes * 8 < numTotal * 1) && (nBytesOneSweep * 4 > nBytes))
  {
    m_headerInfo.microBlockSize = m_microBlockSize * 2;
    double zMin, zMax;
    int nBytes2 = 0;
    if (!WriteTiles(arr, &ptr, nBytes2, zMin, zMax))
      return 0;

    if (nBytes2 <= nBytes)
    {
      nBytes = nBytes2;
    }
    else
    {
      m_headerInfo.microBlockSize = m_microBlockSize;    // reset to orig

      if (bHuffmanLostRun1)
        m_huffmanCodes.resize(0);    // if huffman lost on first run, reset it
    }
  }

  if (nBytesOneSweep <= nBytes)
  {
    m_writeDataOneSweep = true;    // fallback: write data binary uncompressed in one sweep
    nBytes = nBytesOneSweep;
  }

  m_headerInfo.blobSize += nBytes + 1;    // nBytes + flag
  return m_headerInfo.blobSize;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::Encode(const T* arr, Byte** ppByte) const
{
  if (!arr || !ppByte)
    return false;

  if (!WriteHeader(ppByte))
    return false;

  if (!WriteMask(ppByte))
    return false;

  if (m_headerInfo.numValidPixel == 0)
    return true;

  if (m_headerInfo.zMin == m_headerInfo.zMax)    // image is const
    return true;

  if (!m_writeDataOneSweep)
  {
    **ppByte = 0;    // write flag
    (*ppByte)++;

    int numBytes = 0;
    double zMinA, zMaxA;
    if (!WriteTiles(arr, ppByte, numBytes, zMinA, zMaxA))
      return false;
  }
  else
  {
    **ppByte = 1;    // write flag
    (*ppByte)++;

    if (!WriteDataOneSweep(arr, ppByte))
      return false;
  }

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::Decode(const Byte** ppByte, size_t& nRemainingBytes, T* arr, Byte* pMaskBits)
{
  if (!arr || !ppByte)
    return false;

  if (!ReadHeader(ppByte, nRemainingBytes, m_headerInfo))
    return false;

  if (!ReadMask(ppByte, nRemainingBytes))
    return false;

  if (pMaskBits)    // return proper mask bits even if they were not stored
    memcpy(pMaskBits, m_bitMask.Bits(), m_bitMask.Size());

  memset(arr, 0, m_headerInfo.nCols * m_headerInfo.nRows * sizeof(T));

  if (m_headerInfo.numValidPixel == 0)
    return true;

  if (m_headerInfo.zMin == m_headerInfo.zMax)    // image is const
  {
    T z0 = (T)m_headerInfo.zMin;
    for (int i = 0; i < m_headerInfo.nRows; i++)
    {
      int k = i * m_headerInfo.nCols;
      for (int j = 0; j < m_headerInfo.nCols; j++, k++)
        if (m_bitMask.IsValid(k))
          arr[k] = z0;
    }
    return true;
  }

  if( nRemainingBytes < 1 )
  {
    LERC_BRKPNT();
    return false;
  }
  Byte readDataOneSweep = **ppByte;    // read flag
  (*ppByte)++;
  nRemainingBytes -= 1;

  if (!readDataOneSweep)
  {
    if (!ReadTiles(ppByte, nRemainingBytes, arr))
    {
      LERC_BRKPNT();
      return false;
    }
  }
  else
  {
    if (!ReadDataOneSweep(ppByte, nRemainingBytes, arr))
    {
      LERC_BRKPNT();
      return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteDataOneSweep(const T* data, Byte** ppByte) const
{
  T* dstPtr = (T*)(*ppByte);
  int cntPixel = 0;

  for (int i = 0; i < m_headerInfo.nRows; i++)
  {
    int k = i * m_headerInfo.nCols;
    for (int j = 0; j < m_headerInfo.nCols; j++, k++)
      if (m_bitMask.IsValid(k))
      {
        *dstPtr++ = data[k];
        cntPixel++;
      }
  }

  (*ppByte) += cntPixel * sizeof(T);
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadDataOneSweep(const Byte** ppByte, size_t& nRemainingBytesInOut, T* data) const
{
  const T* srcPtr = (const T*)(*ppByte);
  size_t nRemainingBytes = nRemainingBytesInOut;
  int cntPixel = 0;

  for (int i = 0; i < m_headerInfo.nRows; i++)
  {
    int k = i * m_headerInfo.nCols;
    for (int j = 0; j < m_headerInfo.nCols; j++, k++)
      if (m_bitMask.IsValid(k))
      {
        if( nRemainingBytes < sizeof(T) )
        {
          LERC_BRKPNT();
          return false;
        }
        data[k] = *srcPtr++;
        nRemainingBytes -= sizeof(T);
        cntPixel++;
      }
  }

  (*ppByte) += cntPixel * sizeof(T);
  nRemainingBytesInOut -= cntPixel * sizeof(T);
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteTiles(const T* data, Byte** ppByte, int& numBytes, double& zMinA, double& zMaxA) const
{
  if (!data || !ppByte)
    return false;

  numBytes = 0;
  int numBytesLerc = 0;
  int numBytesHuffman = 0;
  zMinA = DBL_MAX;
  zMaxA = -DBL_MAX;

#ifdef TryHuffman
  if ((m_headerInfo.dt == DT_Byte || m_headerInfo.dt == DT_Char)    // try Huffman coding
    && m_headerInfo.maxZError == 0.5)    // for lossless only, maybe later extend to lossy, but Byte and lossy is rare
  {
    numBytes += 1;    // flag Huffman / Lerc2

    if (!*ppByte)    // compute histo and numBytesHuffman
    {
      std::vector<int> histoHuffman;
      if (!ComputeHistoForHuffman(data, histoHuffman))
        return false;

      double avgBpp = 0;
      Huffman huffman;
      if (huffman.ComputeCodes(histoHuffman)
       && huffman.ComputeCompressedSize(histoHuffman, numBytesHuffman, avgBpp))
      {
        m_huffmanCodes = huffman.GetCodes();    // save Huffman codes for later use
      }
      else
      {
        m_huffmanCodes.resize(0);    // if huffman fails, go Lerc
      }
    }
    else if (!m_huffmanCodes.empty())   // encode Huffman, not Lerc2
    {
      **ppByte = 1;    // write out flag Huffman
      (*ppByte)++;

      Huffman huffman;
      if (!huffman.SetCodes(m_huffmanCodes) || !huffman.WriteCodeTable(ppByte))    // header and code table
        return false;

      T zMin = 0, zMax = 0;
      if (!EncodeHuffman(data, ppByte, zMin, zMax))    // data bit stuffed
        return false;

      zMinA = zMin;    // also update stats, to be clean
      zMaxA = zMax;

      return true;    // done.
    }
    else    // encode Lerc2, not Huffman
    {
      **ppByte = 0;    // write out flag Lerc2, proceed below with Lerc2 ...
      (*ppByte)++;
    }
  }
#endif  // TryHuffman

  std::vector<unsigned int> quantVec;
  std::vector<Quant> sortedQuantVec;

  int mbSize = m_headerInfo.microBlockSize;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;

  int numTilesVert = (height + mbSize - 1) / mbSize;
  int numTilesHori = (width + mbSize - 1) / mbSize;

  for (int iTile = 0; iTile < numTilesVert; iTile++)
  {
    int tileH = mbSize;
    int i0 = iTile * tileH;
    if (iTile == numTilesVert - 1)
      tileH = height - i0;

    for (int jTile = 0; jTile < numTilesHori; jTile++)
    {
      int tileW = mbSize;
      int j0 = jTile * tileW;
      if (jTile == numTilesHori - 1)
        tileW = width - j0;

      T zMin = 0, zMax = 0;
      int numValidPixel = 0;
      bool tryLut = false;

      if (!ComputeStats(data, i0, i0 + tileH, j0, j0 + tileW, zMin, zMax,
            numValidPixel, tryLut))
        return false;

      if (numValidPixel > 0)
      {
        zMinA = std::min(zMinA, (double)zMin);
        zMaxA = std::max(zMaxA, (double)zMax);
      }

      // if needed, quantize the data here once
      if ((*ppByte || tryLut) && NeedToQuantize(numValidPixel, zMin, zMax))
      {
        if (!Quantize(data, i0, i0 + tileH, j0, j0 + tileW, zMin, numValidPixel, quantVec))
          return false;

        if (tryLut)
          SortQuantArray(quantVec, sortedQuantVec);
      }

      int numBytesNeeded = NumBytesTile(numValidPixel, zMin, zMax, tryLut, sortedQuantVec);
      numBytesLerc += numBytesNeeded;

      if (*ppByte)    // if 0, just count the bytes needed
      {
        int numBytesWritten = 0;
        if (!WriteTile(data, ppByte, numBytesWritten, i0, i0 + tileH, j0, j0 + tileW, numValidPixel, zMin, zMax,
          quantVec, tryLut, sortedQuantVec))
        {
          return false;
        }
        if (numBytesWritten != numBytesNeeded)
        {
          return false;
        }
      }
    }
  }

#ifdef TryHuffman
  if ((m_headerInfo.dt == DT_Byte || m_headerInfo.dt == DT_Char)
    && m_headerInfo.maxZError == 0.5)    // for lossless only, maybe later extend to lossy, but Byte and lossy is rare
  {
    if (!m_huffmanCodes.empty() && numBytesHuffman < numBytesLerc)
    {
      numBytes += numBytesHuffman;
    }
    else
    {
      numBytes += numBytesLerc;
      m_huffmanCodes.resize(0);
    }
    return true;
  }
#endif

  numBytes = numBytesLerc;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadTiles(const Byte** ppByte, size_t& nRemainingBytes, T* data) const
{
  if (!data || !ppByte || !(*ppByte))
    return false;

#ifdef TryHuffman
  if (m_headerInfo.version > 1
    && (m_headerInfo.dt == DT_Byte || m_headerInfo.dt == DT_Char)    // try Huffman coding
    && m_headerInfo.maxZError == 0.5)    // for lossless only, maybe later extend to lossy, but Byte and lossy is rare
  {
    if (nRemainingBytes < 1 )
    {
      LERC_BRKPNT();
      return false;
    }
    Byte flag = **ppByte;    // read flag Huffman / Lerc2
    (*ppByte)++;
    nRemainingBytes --;

    if (flag == 1)    // decode Huffman
    {
      Huffman huffman;
      if (!huffman.ReadCodeTable(ppByte, nRemainingBytes))    // header and code table
        return false;

      m_huffmanCodes = huffman.GetCodes();

      if (!DecodeHuffman(ppByte, nRemainingBytes, data))    // data
        return false;

      return true;    // done.
    }
    // else decode Lerc2
  }
#endif  // TryHuffman

  std::vector<unsigned int> bufferVec;

  int mbSize = m_headerInfo.microBlockSize;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;

  if( mbSize <= 0 || height < 0 || width < 0 ||
      height > std::numeric_limits<int>::max() - (mbSize - 1) ||
      width > std::numeric_limits<int>::max() - (mbSize - 1) )
  {
    LERC_BRKPNT();
    return false;
  }
  int numTilesVert = height / mbSize + ((height % mbSize) != 0 ? 1 : 0);
  int numTilesHori = width / mbSize + ((width % mbSize) != 0 ? 1 : 0);

  for (int iTile = 0; iTile < numTilesVert; iTile++)
  {
    int tileH = mbSize;
    int i0 = iTile * tileH;
    if (iTile == numTilesVert - 1)
      tileH = height - i0;

    for (int jTile = 0; jTile < numTilesHori; jTile++)
    {
      int tileW = mbSize;
      int j0 = jTile * tileW;
      if (jTile == numTilesHori - 1)
        tileW = width - j0;

      if (!ReadTile(ppByte, nRemainingBytes, data, i0, i0 + tileH, j0, j0 + tileW, bufferVec))
      {
        LERC_BRKPNT();
        return false;
      }
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ComputeStats(const T* data, int i0, int i1, int j0, int j1,
                         T& zMinA, T& zMaxA, int& numValidPixelA, bool& tryLutA) const
{
  if (!data || i0 < 0 || j0 < 0 || i1 > m_headerInfo.nRows || j1 > m_headerInfo.nCols)
    return false;

  tryLutA = false;

  T zMin = 0, zMax = 0, prevVal = 0;
  int numValidPixel = 0;
  int cntSameVal = 0;

  for (int i = i0; i < i1; i++)
  {
    int k = i * m_headerInfo.nCols + j0;

    for (int j = j0; j < j1; j++, k++)
      if (m_bitMask.IsValid(k))
      {
        T val = data[k];
        if (numValidPixel > 0)
        {
          zMin = std::min(zMin, val);
          zMax = std::max(zMax, val);
        }
        else
          zMin = zMax = val;    // init

        numValidPixel++;

        if (val == prevVal)
          cntSameVal++;

        prevVal = val;
      }
  }

  if (numValidPixel > 0)
  {
    zMinA = zMin;
    zMaxA = zMax;
    tryLutA = (zMax > zMin) && (2 * cntSameVal > numValidPixel) && (numValidPixel > 4);
  }

  numValidPixelA = numValidPixel;
  return true;
}

// -------------------------------------------------------------------------- ;

inline double Lerc2::ComputeMaxVal(double zMin, double zMax, double maxZError)
{
  double fac = 1 / (2 * maxZError);
  return (zMax - zMin) * fac;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::NeedToQuantize(int numValidPixel, T zMin, T zMax) const
{
  if (numValidPixel == 0 || m_headerInfo.maxZError == 0)
    return false;

  double maxVal = ComputeMaxVal(zMin, zMax, m_headerInfo.maxZError);
  return !(maxVal > m_maxValToQuantize || (unsigned int)(maxVal + 0.5) == 0);
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::Quantize(const T* data, int i0, int i1, int j0, int j1, T zMin,
  int numValidPixel, std::vector<unsigned int>& quantVec) const
{
  if (!data || i0 < 0 || j0 < 0 || i1 > m_headerInfo.nRows || j1 > m_headerInfo.nCols)
    return false;

  quantVec.resize(numValidPixel);
  unsigned int* dstPtr = &quantVec[0];
  int cntPixel = 0;

  if (m_headerInfo.dt < DT_Float && m_headerInfo.maxZError == 0.5)    // int lossless
  {
    if ((i1 - i0) * (j1 - j0) == numValidPixel)    // all valid
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * m_headerInfo.nCols + j0;
        for (int j = j0; j < j1; j++, k++)
        {
          *dstPtr++ = (unsigned int)(data[k] - zMin);
          cntPixel++;
        }
      }
    }
    else    // not all valid
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * m_headerInfo.nCols + j0;
        for (int j = j0; j < j1; j++, k++)
          if (m_bitMask.IsValid(k))
          {
            *dstPtr++ = (unsigned int)(data[k] - zMin);
            cntPixel++;
          }
      }
    }
  }
  else    // float and/or lossy
  {
    double scale = 1 / (2 * m_headerInfo.maxZError);
    double zMinDbl = (double)zMin;

    if ((i1 - i0) * (j1 - j0) == numValidPixel)    // all valid
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * m_headerInfo.nCols + j0;
        for (int j = j0; j < j1; j++, k++)
        {
          *dstPtr++ = (unsigned int)(((double)data[k] - zMinDbl) * scale + 0.5);
          //*dstPtr++ = (unsigned int)(((double)(data[k] - zMin) * scale + 0.5));
          cntPixel++;
        }
      }
    }
    else    // not all valid
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * m_headerInfo.nCols + j0;
        for (int j = j0; j < j1; j++, k++)
          if (m_bitMask.IsValid(k))
          {
            *dstPtr++ = (unsigned int)(((double)data[k] - zMinDbl) * scale + 0.5);
            cntPixel++;
          }
      }
    }
  }

  return (cntPixel == numValidPixel);
}

// -------------------------------------------------------------------------- ;

template<class T>
int Lerc2::NumBytesTile(int numValidPixel, T zMin, T zMax, bool& tryLut,  // can get set to false by the function
                         const std::vector<Quant>& sortedQuantVec) const
{
  if (numValidPixel == 0 || (zMin == 0 && zMax == 0))
    return 1;

  double maxVal, maxZError = m_headerInfo.maxZError;

  if (maxZError == 0 || (maxVal = ComputeMaxVal(zMin, zMax, maxZError)) > m_maxValToQuantize)
  {
    return(int)(1 + numValidPixel * sizeof(T));
  }
  else
  {
    //enum DataType {DT_Char, DT_Byte, DT_Short, DT_UShort, DT_Int, DT_UInt, DT_Float, DT_Double};
    static const Byte sizeArr[] = {1, 1, 2, 2, 4, 4, 4, 8};
    DataType dtUsed;
    TypeCode(zMin, dtUsed); // Called to set dtUsed, return value is ignored
    int nBytesForMin = sizeArr[dtUsed];
    int nBytes = 1 + nBytesForMin;

    unsigned int maxElem = (unsigned int)(maxVal + 0.5);
    if (maxElem > 0)
    {
      nBytes += (!tryLut) ? m_bitStuffer2.ComputeNumBytesNeededSimple(numValidPixel, maxElem)
                          : m_bitStuffer2.ComputeNumBytesNeededLut(sortedQuantVec, tryLut);
    }

    return nBytes;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteTile(const T* data, Byte** ppByte, int& numBytesWritten,
                      int i0, int i1, int j0, int j1, int numValidPixel, T zMin, T zMax,
                      const std::vector<unsigned int>& quantVec, bool doLut,
                      const std::vector<Quant>& sortedQuantVec) const
{
  Byte* ptr = *ppByte;
  Byte comprFlag = ((j0 >> 3) & 15) << 2;    // use bits 2345 for integrity check
  int cntPixel = 0;

  if (numValidPixel == 0 || (zMin == 0 && zMax == 0))    // special cases
  {
    *ptr++ = comprFlag | 2;    // set compression flag to 2 to mark tile as constant 0
    numBytesWritten = 1;
    *ppByte = ptr;
    return true;
  }

  double maxVal, maxZError = m_headerInfo.maxZError;

  if (maxZError == 0 || (maxVal = ComputeMaxVal(zMin, zMax, maxZError)) > m_maxValToQuantize)
  {
    *ptr++ = comprFlag | 0;    // write z's binary uncompressed
    T* dstPtr = (T*)ptr;

    for (int i = i0; i < i1; i++)
    {
      int k = i * m_headerInfo.nCols + j0;
      for (int j = j0; j < j1; j++, k++)
        if (m_bitMask.IsValid(k))
        {
          *dstPtr++ = data[k];
          cntPixel++;
        }
    }

    if (cntPixel != numValidPixel)
      return false;

    ptr += numValidPixel * sizeof(T);
  }
  else
  {
    // write z's as int arr bit stuffed
    unsigned int maxElem = (unsigned int)(maxVal + 0.5);
    if (maxElem == 0)
      comprFlag |= 3;    // set compression flag to 3 to mark tile as constant zMin
    else
      comprFlag |= 1;    // use bit stuffing

    DataType dtUsed;
    int bits67 = TypeCode(zMin, dtUsed);
    comprFlag |= bits67 << 6;

    *ptr++ = comprFlag;

    if (!WriteVariableDataType(&ptr, (double)zMin, dtUsed))
      return false;

    if (maxElem > 0)
    {
      if (quantVec.size() != (size_t)numValidPixel)
        return false;

      if (!doLut)
      {
        if (!m_bitStuffer2.EncodeSimple(&ptr, quantVec))
          return false;
      }
      else
      {
        if (!m_bitStuffer2.EncodeLut(&ptr, sortedQuantVec))
          return false;
      }
    }
  }

  numBytesWritten = (int)(ptr - *ppByte);
  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadTile(const Byte** ppByte, size_t& nRemainingBytesInOut,
                     T* data, int i0, int i1, int j0, int j1,
                     std::vector<unsigned int>& bufferVec) const
{
  size_t nRemainingBytes = nRemainingBytesInOut;
  const Byte* ptr = *ppByte;
  int numPixel = 0;

  if( nRemainingBytes < 1 )
  {
    LERC_BRKPNT();
    return false;
  }
  Byte comprFlag = *ptr++;
  nRemainingBytes -= 1;
  int bits67 = comprFlag >> 6;
  //comprFlag &= 63;

  int testCode = (comprFlag >> 2) & 15;    // use bits 2345 for integrity check
  if (testCode != ((j0 >> 3) & 15))
    return false;

  comprFlag &= 3;

  if (comprFlag == 2)    // entire tile is constant 0 (if valid or invalid doesn't matter)
  {
    for (int i = i0; i < i1; i++)
    {
      int k = i * m_headerInfo.nCols + j0;
      for (int j = j0; j < j1; j++, k++)
        if (m_bitMask.IsValid(k))
          data[k] = 0;
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
  }

  else if (comprFlag == 0)    // read z's binary uncompressed
  {
    const T* srcPtr = (const T*)ptr;

    for (int i = i0; i < i1; i++)
    {
      int k = i * m_headerInfo.nCols + j0;
      for (int j = j0; j < j1; j++, k++)
        if (m_bitMask.IsValid(k))
        {
          if( nRemainingBytes < sizeof(T) )
          {
            LERC_BRKPNT();
            return false;
          }
          data[k] = *srcPtr++;
          nRemainingBytes -= sizeof(T);
          numPixel++;
        }
    }

    ptr += numPixel * sizeof(T);
  }
  else
  {
    // read z's as int arr bit stuffed
    DataType dtUsed = GetDataTypeUsed(bits67);
    double offset;
    if( !ReadVariableDataType(&ptr, nRemainingBytes, dtUsed, &offset) )
    {
      LERC_BRKPNT();
      return false;
    }

    if (comprFlag == 3)
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * m_headerInfo.nCols + j0;
        for (int j = j0; j < j1; j++, k++)
          if (m_bitMask.IsValid(k))
            data[k] = (T)offset;
      }
    }
    else
    {
      const size_t nMaxBufferVecElts = size_t((i1 - i0) * (j1 - j0));
      if (!m_bitStuffer2.Decode(&ptr, nRemainingBytes, bufferVec, nMaxBufferVecElts))
      {
        LERC_BRKPNT();
        return false;
      }

      double invScale = 2 * m_headerInfo.maxZError;    // for int types this is int
      size_t bufferVecIdx = 0;
      if (bufferVec.size() == nMaxBufferVecElts)    // all valid
      {
        for (int i = i0; i < i1; i++)
        {
          int k = i * m_headerInfo.nCols + j0;
          for (int j = j0; j < j1; j++, k++)
          {
            double z = offset + bufferVec[bufferVecIdx] * invScale;
            bufferVecIdx ++;
            data[k] = (T)std::min(z, m_headerInfo.zMax);    // make sure we stay in the orig range
          }
        }
      }
      else    // not all valid
      {
        for (int i = i0; i < i1; i++)
        {
          int k = i * m_headerInfo.nCols + j0;
          for (int j = j0; j < j1; j++, k++)
            if (m_bitMask.IsValid(k))
            {
              if( bufferVecIdx == bufferVec.size() )
              {
                LERC_BRKPNT();
                return false;
              }
              double z = offset + bufferVec[bufferVecIdx] * invScale;
              bufferVecIdx ++;
              data[k] = (T)std::min(z, m_headerInfo.zMax);    // make sure we stay in the orig range
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

template<class T>
int Lerc2::TypeCode(T z, DataType& dtUsed) const
{
  Byte b = (Byte)z;
  DataType dt = m_headerInfo.dt;
  switch (dt)
  {
    case DT_Short:
    {
      char c = (char)z;
      int tc = (T)c == z ? 2 : (T)b == z ? 1 : 0;
      dtUsed = (DataType)(dt - tc);
      return tc;
    }
    case DT_UShort:
    {
      int tc = (T)b == z ? 1 : 0;
      dtUsed = (DataType)(dt - 2 * tc);
      return tc;
    }
    case DT_Int:
    {
      short s = (short)z;
      unsigned short us = (unsigned short)z;
      int tc = (T)b == z ? 3 : (T)s == z ? 2 : (T)us == z? 1 : 0;
      dtUsed = (DataType)(dt - tc);
      return tc;
    }
    case DT_UInt:
    {
      unsigned short us = (unsigned short)z;
      int tc = (T)b == z ? 2 : (T)us == z? 1 : 0;
      dtUsed = (DataType)(dt - 2 * tc);
      return tc;
    }
    case DT_Float:
    {
      short s = (short)z;
      int tc = (T)b == z ? 2 : (T)s == z? 1 : 0;
      dtUsed = tc == 0 ? dt : (tc == 1 ? DT_Short : DT_Byte);
      return tc;
    }
    case DT_Double:
    {
      short s = (short)z;
      int l = (int)z;
      float f = (float)z;
      int tc = (T)s == z ? 3 : (T)l == z? 2 : (T)f == z ? 1 : 0;
      dtUsed = tc == 0 ? dt : (DataType)(dt - 2 * tc + 1);
      return tc;
    }
    default:
    {
      dtUsed = dt;
      return 0;
    }
  }
}

// -------------------------------------------------------------------------- ;

inline
Lerc2::DataType Lerc2::GetDataTypeUsed(int tc) const
{
  DataType dt = m_headerInfo.dt;
  switch (dt)
  {
    case DT_Short:
    case DT_Int:     return (DataType)(dt - tc);
    case DT_UShort:
    case DT_UInt:    return (DataType)(dt - 2 * tc);
    case DT_Float:   return tc == 0 ? dt : (tc == 1 ? DT_Short : DT_Byte);
    case DT_Double:  return tc == 0 ? dt : (DataType)(dt - 2 * tc + 1);
    default:
      return dt;
  }
}

// -------------------------------------------------------------------------- ;

inline
bool Lerc2::WriteVariableDataType(Byte** ppByte, double z, DataType dtUsed)
{
  Byte* ptr = *ppByte;

  switch (dtUsed)
  {
    case DT_Char:    *((char*)ptr)           = (char)z;            ptr++;     break;
    case DT_Byte:    *((Byte*)ptr)           = (Byte)z;            ptr++;     break;
    case DT_Short: { short s = (short)z;
                     memcpy(ptr, &s, sizeof(short));
                     ptr += 2;
                     break; }
    case DT_UShort:{ unsigned short us = (unsigned short)z;
                     memcpy(ptr, &us, sizeof(unsigned short));
                     ptr += 2;
                     break; }
    case DT_Int:   { int i = (int)z;
                     memcpy(ptr, &i, sizeof(int));
                     ptr += 4;
                     break; }
    case DT_UInt:  { unsigned int n = (unsigned int)z;
                     memcpy(ptr, &n, sizeof(unsigned int));
                     ptr += 4;
                     break; }
    case DT_Float: { float f = (float)z;
                     memcpy(ptr, &f, sizeof(float));
                     ptr += 4;
                     break; }
    case DT_Double:  memcpy(ptr, &z, sizeof(double)); ptr += 8;  break;
    default:
      return false;
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

inline
bool Lerc2::ReadVariableDataType(const Byte** ppByte, size_t& nRemainingBytes, DataType dtUsed, double* pdfOutVal)
{
  const Byte* ptr = *ppByte;

  switch (dtUsed)
  {
    case DT_Char:
    {
      if( nRemainingBytes < 1 )
      {
        LERC_BRKPNT();
        return false;
      }
      char c = *((char*)ptr);
      *ppByte = ptr + 1;
      *pdfOutVal = c;
      nRemainingBytes -= 1;
      return true;
    }
    case DT_Byte:
    {
      if( nRemainingBytes < 1 )
      {
        LERC_BRKPNT();
        return false;
      }
      Byte b = *((Byte*)ptr);
      *ppByte = ptr + 1;
      *pdfOutVal = b;
      nRemainingBytes -= 1;
      return true;
    }
    case DT_Short:
    {
      if( nRemainingBytes < 2 )
      {
        LERC_BRKPNT();
        return false;
      }
      short s;
      memcpy(&s, ptr, sizeof(short));
      *ppByte = ptr + 2;
      *pdfOutVal = s;
      nRemainingBytes -= 2;
      return true;
    }
    case DT_UShort:
    {
      if( nRemainingBytes < 2 )
      {
        LERC_BRKPNT();
        return false;
      }
      unsigned short us;
      memcpy(&us, ptr, sizeof(unsigned short));
      *ppByte = ptr + 2;
      *pdfOutVal = us;
      nRemainingBytes -= 2;
      return true;
    }
    case DT_Int:
    {
      if( nRemainingBytes < 4 )
      {
        LERC_BRKPNT();
        return false;
      }
      int i;
      memcpy(&i, ptr, sizeof(int));
      *ppByte = ptr + 4;
      *pdfOutVal = i;
      nRemainingBytes -= 4;
      return true;
    }
    case DT_UInt:
    {
      if( nRemainingBytes < 4 )
      {
        LERC_BRKPNT();
        return false;
      }
      unsigned int n;
      memcpy(&n, ptr, sizeof(unsigned int));
      *ppByte = ptr + 4;
      *pdfOutVal = n;
      nRemainingBytes -= 4;
      return true;
    }
    case DT_Float:
    {
      if( nRemainingBytes < 4 )
      {
        LERC_BRKPNT();
        return false;
      }
      float f;
      memcpy(&f, ptr, sizeof(float));
      *ppByte = ptr + 4;
      *pdfOutVal = f;
      nRemainingBytes -= 4;
      return true;
    }
    case DT_Double:
    {
      if( nRemainingBytes < 8 )
      {
        LERC_BRKPNT();
        return false;
      }
      double d;
      memcpy(&d, ptr, sizeof(double));
      *ppByte = ptr + 8;
      *pdfOutVal = d;
      nRemainingBytes -= 8;
      return true;
    }
    default:
      *pdfOutVal = 0;
      return true;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
Lerc2::DataType Lerc2::GetDataType(T z) const
{
  const std::type_info& ti = typeid(z);

       if (ti == typeid(char))            return DT_Char;
  else if (ti == typeid(Byte))            return DT_Byte;
  else if (ti == typeid(short))           return DT_Short;
  else if (ti == typeid(unsigned short))  return DT_UShort;
  else if (ti == typeid(int))             return DT_Int;
  else if (ti == typeid(long)
      // cppcheck-suppress knownConditionTrueFalse
       && sizeof(long) == 4)              return DT_Int;
  else if (ti == typeid(unsigned int))    return DT_UInt;
  else if (ti == typeid(unsigned long)
      // cppcheck-suppress knownConditionTrueFalse
       && sizeof(unsigned long) == 4)     return DT_UInt;
  else if (ti == typeid(float))           return DT_Float;
  else if (ti == typeid(double))          return DT_Double;
  else
    return DT_Undefined;
}

// -------------------------------------------------------------------------- ;

inline
unsigned int Lerc2::GetMaxValToQuantize(Lerc2::DataType dt)
{
  switch (dt)
  {
    case DT_Char:
    case DT_Byte:    //return (1 <<  7) - 1;    // disabled: allow LUT mode for 8 bit segmented
    case DT_Short:
    case DT_UShort:  return (1 << 15) - 1;

    case DT_Int:
    case DT_UInt:
    case DT_Float:
    case DT_Double:  return (1 << 30) - 1;

    default:
      return 0;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ComputeHistoForHuffman(const T* data, std::vector<int>& histo) const
{
  if (!data)
    return false;

  histo.resize(256);
  memset(&histo[0], 0, histo.size() * sizeof(histo[0]));

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  T prevVal = 0;

  if (m_headerInfo.numValidPixel == width * height)    // all valid
  {
    for (int k = 0, i = 0; i < height; i++)
      for (int j = 0; j < width; j++, k++)
      {
        T val = data[k];
        T delta = val;

        if (j > 0)
          delta -= prevVal;    // use overflow
        else if (i > 0)
          delta -= data[k - width];
        else
          delta -= prevVal;

        prevVal = val;
        histo[offset + (int)delta]++;
      }
  }
  else    // not all valid
  {
    for (int k = 0, i = 0; i < height; i++)
      for (int j = 0; j < width; j++, k++)
        if (m_bitMask.IsValid(k))
        {
          T val = data[k];
          T delta = val;

          if (j > 0 && m_bitMask.IsValid(k - 1))
          {
            delta -= prevVal;    // use overflow
          }
          else if (i > 0 && m_bitMask.IsValid(k - width))
          {
            delta -= data[k - width];
          }
          else
            delta -= prevVal;

          prevVal = val;
          histo[offset + (int)delta]++;
        }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::EncodeHuffman(const T* data, Byte** ppByte, T& zMinA, T& zMaxA) const
{
  if (!data || !ppByte)
    return false;

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  zMinA = (T)(offset - 1);
  zMaxA = (T)(-offset);
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  T prevVal = 0;

  unsigned int* arr = (unsigned int*)(*ppByte);
  unsigned int* dstPtr = arr;
  int bitPos = 0;

  for (int k = 0, i = 0; i < height; i++)
  {
    for (int j = 0; j < width; j++, k++)
      if (m_bitMask.IsValid(k))
      {
        T val = data[k];
        T delta = val;

        if (val < zMinA)
            zMinA = val;
        if (val > zMaxA)
            zMaxA = val;

        if (j > 0 && m_bitMask.IsValid(k - 1))
        {
          delta -= prevVal;    // use overflow
        }
        else if (i > 0 && m_bitMask.IsValid(k - width))
        {
          delta -= data[k - width];
        }
        else
          delta -= prevVal;

        prevVal = val;

        // bit stuff the huffman code for this delta
        int len = m_huffmanCodes[offset + (int)delta].first;
        if (len <= 0)
          return false;

        unsigned int code = m_huffmanCodes[offset + (int)delta].second;

        if (32 - bitPos >= len)
        {
          if (bitPos == 0)
            Store(dstPtr, 0);

          Store(dstPtr, Load(dstPtr) | (code << (32 - bitPos - len)));
          bitPos += len;
          if (bitPos == 32)
          {
            bitPos = 0;
            dstPtr++;
          }
        }
        else
        {
          bitPos += len - 32;
          Store(dstPtr, Load(dstPtr) | (code >> bitPos));
          dstPtr ++;
          Store(dstPtr, code << (32 - bitPos));
        }
      }
  }

  size_t numUInts = dstPtr - arr + (bitPos > 0 ? 1 : 0) + 1;    // add one more as the decode LUT can read ahead
  *ppByte += numUInts * sizeof(unsigned int);
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::DecodeHuffman(const Byte** ppByte, size_t& nRemainingBytesInOut, T* data) const
{
  if (!data || !ppByte || !(*ppByte))
    return false;

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  T prevVal = 0;

  const unsigned int* arr = (const unsigned int*)(*ppByte);
  const unsigned int* srcPtr = arr;
  size_t nRemainingBytesTmp = nRemainingBytesInOut;
  int bitPos = 0;
  int numBitsLUT = 0;

  Huffman huffman;
  if (!huffman.SetCodes(m_huffmanCodes) || !huffman.BuildTreeFromCodes(numBitsLUT))
    return false;

  if (m_headerInfo.numValidPixel == width * height)    // all valid
  {
    for (int k = 0, i = 0; i < height; i++)
      for (int j = 0; j < width; j++, k++)
      {
        int val = 0;
        if (!huffman.DecodeOneValue(&srcPtr, nRemainingBytesTmp, bitPos, numBitsLUT, val))
          return false;

        T delta = (T)(val - offset);

        if (j > 0)
          delta += prevVal;    // use overflow
        else if (i > 0)
          delta += data[k - width];
        else
          delta += prevVal;

        data[k] = delta;
        prevVal = delta;
      }
  }
  else    // not all valid
  {
    for (int k = 0, i = 0; i < height; i++)
      for (int j = 0; j < width; j++, k++)
        if (m_bitMask.IsValid(k))
        {
          int val = 0;
          if (!huffman.DecodeOneValue(&srcPtr, nRemainingBytesTmp, bitPos, numBitsLUT, val))
            return false;

          T delta = (T)(val - offset);

          if (j > 0 && m_bitMask.IsValid(k - 1))
          {
            delta += prevVal;    // use overflow
          }
          else if (i > 0 && m_bitMask.IsValid(k - width))
          {
            delta += data[k - width];
          }
          else
            delta += prevVal;

          data[k] = delta;
          prevVal = delta;
        }
  }

  size_t numUInts = srcPtr - arr + (bitPos > 0 ? 1 : 0) + 1;    // add one more as the decode LUT can read ahead
  if( nRemainingBytesInOut < numUInts * sizeof(unsigned int))
  {
    LERC_BRKPNT();
    return false;
  }
  *ppByte += numUInts * sizeof(unsigned int);
  nRemainingBytesInOut -= numUInts * sizeof(unsigned int);
  return true;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
