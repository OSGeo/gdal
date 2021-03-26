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

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <string>
#include <typeinfo>
#include "Defines.h"
#include "BitMask.h"
#include "BitStuffer2.h"
#include "Huffman.h"
#include "RLE.h"

NAMESPACE_LERC_START

/**   Lerc2 v1
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
 *    Lerc2 v2
 *
 *    -- add Huffman coding for better lossless compression of 8 bit data types Char, Byte
 *
 *    Lerc2 v3
 *
 *    -- add checksum for the entire byte blob, for more rigorous detection of compressed data corruption
 *    -- for the main bit stuffing routine, use an extra uint buffer for guaranteed memory alignment
 *    -- this also allows to drop the NumExtraBytesToAllocate functions
 *
 *    Lerc2 v4
 *
 *    -- allow array per pixel, nDim values per pixel. Such as RGB, complex number, or larger arrays per pixel
 *    -- extend Huffman coding for 8 bit data types from delta only to trying both delta and orig
 *    -- for integer data types, allow to drop bit planes containing only random noise
 *
 */

class Lerc2
{
public:
  Lerc2();
  Lerc2(int nDim, int nCols, int nRows, const Byte* pMaskBits = nullptr);    // valid / invalid bits as byte array
  virtual ~Lerc2()  {}

  bool SetEncoderToOldVersion(int version);    // call this to encode compatible to an old decoder

  bool Set(int nDim, int nCols, int nRows, const Byte* pMaskBits = nullptr);

  template<class T>
  unsigned int ComputeNumBytesNeededToWrite(const T* arr, double maxZError, bool encodeMask);

  //static unsigned int MinNumBytesNeededToReadHeader();

  /// dst buffer already allocated;  byte ptr is moved like a file pointer
  template<class T>
  bool Encode(const T* arr, Byte** ppByte);

  // data types supported by Lerc2
  enum DataType {DT_Char = 0, DT_Byte, DT_Short, DT_UShort, DT_Int, DT_UInt, DT_Float, DT_Double, DT_Undefined};

  struct HeaderInfo
  {
    int version;
    unsigned int checksum;
    int nRows,
        nCols,
        nDim,
        numValidPixel,
        microBlockSize,
        blobSize;

    DataType dt;

    double  maxZError,
            zMin,    // if nDim > 1, this is the overall range
            zMax;

    void RawInit()  { memset(this, 0, sizeof(struct HeaderInfo)); }

    bool TryHuffman() const  { return version > 1 && (dt == DT_Byte || dt == DT_Char) && maxZError == 0.5; }
  };

  static bool GetHeaderInfo(const Byte* pByte, size_t nBytesRemaining, struct HeaderInfo& headerInfo);

  /// dst buffer already allocated;  byte ptr is moved like a file pointer
  template<class T>
  bool Decode(const Byte** ppByte, size_t& nBytesRemaining, T* arr, Byte* pMaskBits = nullptr);    // if mask ptr is not 0, mask bits are returned (even if all valid or same as previous)

private:
  static const int kCurrVersion = 4;    // 2: added Huffman coding to 8 bit types DT_Char, DT_Byte;
                                        // 3: changed the bit stuffing to using a uint aligned buffer,
                                        //    added Fletcher32 checksum
                                        // 4: allow nDim values per pixel

  enum ImageEncodeMode { IEM_Tiling = 0, IEM_DeltaHuffman, IEM_Huffman };
  enum BlockEncodeMode { BEM_RawBinary = 0, BEM_BitStuffSimple, BEM_BitStuffLUT };

  int         m_microBlockSize,
              m_maxValToQuantize;
  BitMask     m_bitMask;
  HeaderInfo  m_headerInfo;
  BitStuffer2 m_bitStuffer2;
  bool        m_encodeMask,
              m_writeDataOneSweep;
  ImageEncodeMode  m_imageEncodeMode;

  std::vector<double> m_zMinVec, m_zMaxVec;
  std::vector<std::pair<unsigned short, unsigned int> > m_huffmanCodes;    // <= 256 codes, 1.5 kB

private:
  static std::string FileKey()  { return "Lerc2 "; }
  static bool IsLittleEndianSystem()  { int n = 1;  return (1 == *((Byte*)&n)) && (4 == sizeof(int)); }
  void Init();

  static unsigned int ComputeNumBytesHeaderToWrite(const struct HeaderInfo& hd);
  static bool WriteHeader(Byte** ppByte, const struct HeaderInfo& hd);
  static bool ReadHeader(const Byte** ppByte, size_t& nBytesRemaining, struct HeaderInfo& hd);

  bool WriteMask(Byte** ppByte) const;
  bool ReadMask(const Byte** ppByte, size_t& nBytesRemaining);

  bool DoChecksOnEncode(Byte* pBlobBegin, Byte* pBlobEnd) const;
  static unsigned int ComputeChecksumFletcher32(const Byte* pByte, int len);

  static void AddUIntToCounts(int* pCounts, unsigned int val, int nBits);
  static void AddIntToCounts(int* pCounts, int val, int nBits);

  template<class T>
  bool TryBitPlaneCompression(const T* data, double eps, double& newMaxZError) const;

  template<class T>
  bool WriteDataOneSweep(const T* data, Byte** ppByte) const;

  template<class T>
  bool ReadDataOneSweep(const Byte** ppByte, size_t& nBytesRemaining, T* data) const;

  template<class T>
  bool WriteTiles(const T* data, Byte** ppByte, int& numBytes, std::vector<double>& zMinVec, std::vector<double>& zMaxVec) const;

  template<class T>
  bool ReadTiles(const Byte** ppByte, size_t& nBytesRemaining, T* data) const;

  template<class T>
  bool GetValidDataAndStats(const T* data, int i0, int i1, int j0, int j1, int iDim,
    T* dataBuf, T& zMinA, T& zMaxA, int& numValidPixel, bool& tryLutA) const;

  static double ComputeMaxVal(double zMin, double zMax, double maxZError);

  template<class T>
  bool NeedToQuantize(int numValidPixel, T zMin, T zMax) const;

  template<class T>
  bool Quantize(const T* dataBuf, int num, T zMin, std::vector<unsigned int>& quantVec) const;

  template<class T>
  int NumBytesTile(int numValidPixel, T zMin, T zMax, bool tryLut, BlockEncodeMode& blockEncodeMode,
                   const std::vector<std::pair<unsigned int, unsigned int> >& sortedQuantVec) const;

  template<class T>
  bool WriteTile(const T* dataBuf, int num, Byte** ppByte, int& numBytesWritten, int j0, T zMin, T zMax,
    const std::vector<unsigned int>& quantVec, BlockEncodeMode blockEncodeMode,
    const std::vector<std::pair<unsigned int, unsigned int> >& sortedQuantVec) const;

  template<class T>
  bool ReadTile(const Byte** ppByte, size_t& nBytesRemaining, T* data, int i0, int i1, int j0, int j1, int iDim,
                std::vector<unsigned int>& bufferVec) const;

  template<class T>
  int TypeCode(T z, DataType& dtUsed) const;

  DataType GetDataTypeUsed(int typeCode) const;

  static DataType ValidateDataType(int dt);

  static bool WriteVariableDataType(Byte** ppByte, double z, DataType dtUsed);

  static double ReadVariableDataType(const Byte** ppByte, DataType dtUsed);

  template<class T> DataType GetDataType(T z) const;

  static unsigned int GetMaxValToQuantize(DataType dt);

  static unsigned int GetDataTypeSize(DataType dt);

  static void SortQuantArray(const std::vector<unsigned int>& quantVec,
    std::vector<std::pair<unsigned int, unsigned int> >& sortedQuantVec);

  template<class T>
  void ComputeHuffmanCodes(const T* data, int& numBytes, ImageEncodeMode& imageEncodeMode,
    std::vector<std::pair<unsigned short, unsigned int> >& codes) const;

  template<class T>
  void ComputeHistoForHuffman(const T* data, std::vector<int>& histo, std::vector<int>& deltaHisto) const;

  template<class T>
  bool EncodeHuffman(const T* data, Byte** ppByte) const;

  template<class T>
  bool DecodeHuffman(const Byte** ppByte, size_t& nBytesRemaining, T* data) const;

  template<class T>
  bool WriteMinMaxRanges(const T* data, Byte** ppByte) const;

  template<class T>
  bool ReadMinMaxRanges(const Byte** ppByte, size_t& nBytesRemaining, const T* data);

  bool CheckMinMaxRanges(bool& minMaxEqual) const;

  template<class T>
  bool FillConstImage(T* data) const;
};

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

template<class T>
unsigned int Lerc2::ComputeNumBytesNeededToWrite(const T* arr, double maxZError, bool encodeMask)
{
  if (!arr || !IsLittleEndianSystem())
    return 0;

  // header
  unsigned int nBytesHeaderMask = ComputeNumBytesHeaderToWrite(m_headerInfo);

  // valid / invalid mask
  int numValid = m_headerInfo.numValidPixel;
  int numTotal = m_headerInfo.nCols * m_headerInfo.nRows;

  bool needMask = numValid > 0 && numValid < numTotal;

  m_encodeMask = encodeMask;

  nBytesHeaderMask += 1 * sizeof(int);    // the mask encode numBytes

  if (needMask && encodeMask)
  {
    RLE rle;
    size_t n = rle.computeNumBytesRLE((const Byte*)m_bitMask.Bits(), m_bitMask.Size());
    nBytesHeaderMask += (unsigned int)n;
  }

  m_headerInfo.dt = GetDataType(arr[0]);

  if (m_headerInfo.dt == DT_Undefined)
    return 0;

  if (maxZError == 777)    // cheat code
    maxZError = -0.01;

  if (m_headerInfo.dt < DT_Float)    // integer types
  {
    // interpret a negative maxZError as bit plane epsilon; dflt = 0.01;
    if (maxZError < 0 && (!TryBitPlaneCompression(arr, -maxZError, maxZError)))
      maxZError = 0;

    maxZError = std::max(0.5, floor(maxZError));
  }
  else if (maxZError < 0)    // don't allow bit plane compression for float or double yet
    return 0;

  m_headerInfo.maxZError = maxZError;
  m_headerInfo.zMin = 0;
  m_headerInfo.zMax = 0;
  m_headerInfo.microBlockSize = m_microBlockSize;
  m_headerInfo.blobSize = nBytesHeaderMask;

  if (numValid == 0)
    return nBytesHeaderMask;

  m_maxValToQuantize = GetMaxValToQuantize(m_headerInfo.dt);

  Byte* ptr = nullptr;    // only emulate the writing and just count the bytes needed
  int nBytesTiling = 0;

  if (!WriteTiles(arr, &ptr, nBytesTiling, m_zMinVec, m_zMaxVec))    // also fills the min max ranges
    return 0;

  m_headerInfo.zMin = *std::min_element(m_zMinVec.begin(), m_zMinVec.end());
  m_headerInfo.zMax = *std::max_element(m_zMaxVec.begin(), m_zMaxVec.end());

  if (m_headerInfo.zMin == m_headerInfo.zMax)    // image is const
    return nBytesHeaderMask;

  int nDim = m_headerInfo.nDim;

  if (m_headerInfo.version >= 4)
  {
    // add the min max ranges behind the mask and before the main data;
    // so we do not write it if no valid pixel or all same value const
    m_headerInfo.blobSize += 2 * nDim * sizeof(T);

    bool minMaxEqual = false;
    if (!CheckMinMaxRanges(minMaxEqual))
      return 0;

    if (minMaxEqual)
      return m_headerInfo.blobSize;    // all nDim bands are const
  }

  // data
  m_imageEncodeMode = IEM_Tiling;
  int nBytesData = nBytesTiling;
  int nBytesHuffman = 0;

  if (m_headerInfo.TryHuffman())
  {
    ImageEncodeMode huffmanEncMode;
    ComputeHuffmanCodes(arr, nBytesHuffman, huffmanEncMode, m_huffmanCodes);    // save Huffman codes for later use

    if (!m_huffmanCodes.empty() && nBytesHuffman < nBytesTiling)
    {
      m_imageEncodeMode = huffmanEncMode;
      nBytesData = nBytesHuffman;
    }
    else
      m_huffmanCodes.resize(0);
  }

  m_writeDataOneSweep = false;
  int nBytesDataOneSweep = (int)(numValid * nDim * sizeof(T));

  {
    // try with double block size to reduce block header overhead, if
    if ( (nBytesTiling * 8 < numTotal * nDim * 2)    // resulting bit rate < x (2 bpp)
      && (nBytesTiling < 4 * nBytesDataOneSweep)     // bit stuffing is effective
      && (nBytesHuffman == 0 || nBytesTiling < 2 * nBytesHuffman) )    // not much worse than huffman (otherwise huffman wins anyway)
    {
      m_headerInfo.microBlockSize = m_microBlockSize * 2;

      std::vector<double> zMinVec, zMaxVec;
      int nBytes2 = 0;
      if (!WriteTiles(arr, &ptr, nBytes2, zMinVec, zMaxVec))    // no huffman in here anymore
        return 0;

      if (nBytes2 <= nBytesData)
      {
        nBytesData = nBytes2;
        m_imageEncodeMode = IEM_Tiling;
        m_huffmanCodes.resize(0);
      }
      else
      {
        m_headerInfo.microBlockSize = m_microBlockSize;    // reset to orig
      }
    }
  }

  if (m_headerInfo.TryHuffman())
    nBytesData += 1;    // flag for image encode mode

  if (nBytesDataOneSweep <= nBytesData)
  {
    m_writeDataOneSweep = true;    // fallback: write data binary uncompressed in one sweep
    m_headerInfo.blobSize += 1 + nBytesDataOneSweep;    // header, mask, min max ranges, flag, data one sweep
  }
  else
  {
    m_writeDataOneSweep = false;
    m_headerInfo.blobSize += 1 + nBytesData;    // header, mask, min max ranges, flag(s), data
  }

  return m_headerInfo.blobSize;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::Encode(const T* arr, Byte** ppByte)
{
  if (!arr || !ppByte || !IsLittleEndianSystem())
    return false;

  Byte* ptrBlob = *ppByte;    // keep a ptr to the start of the blob

  if (!WriteHeader(ppByte, m_headerInfo))
    return false;

  if (!WriteMask(ppByte))
    return false;

  if (m_headerInfo.numValidPixel == 0 || m_headerInfo.zMin == m_headerInfo.zMax)
  {
    return DoChecksOnEncode(ptrBlob, *ppByte);
  }

  if (m_headerInfo.version >= 4)
  {
    if (!WriteMinMaxRanges(arr, ppByte))
      return false;

    bool minMaxEqual = false;
    if (!CheckMinMaxRanges(minMaxEqual))
      return false;

    if (minMaxEqual)
      return DoChecksOnEncode(ptrBlob, *ppByte);
  }

  **ppByte = m_writeDataOneSweep ? 1 : 0;    // write flag
  (*ppByte)++;

  if (!m_writeDataOneSweep)
  {
    if (m_headerInfo.TryHuffman())
    {
      **ppByte = (Byte)m_imageEncodeMode;    // Huffman or tiling encode mode
      (*ppByte)++;

      if (!m_huffmanCodes.empty())   // Huffman, no tiling
      {
        if (m_imageEncodeMode != IEM_DeltaHuffman && m_imageEncodeMode != IEM_Huffman)
          return false;

        if (!EncodeHuffman(arr, ppByte))    // data bit stuffed
          return false;

        return DoChecksOnEncode(ptrBlob, *ppByte);
      }
    }

    int numBytes = 0;
    std::vector<double> zMinVec, zMaxVec;
    if (!WriteTiles(arr, ppByte, numBytes, zMinVec, zMaxVec))
      return false;
  }
  else
  {
    if (!WriteDataOneSweep(arr, ppByte))
      return false;
  }

  return DoChecksOnEncode(ptrBlob, *ppByte);
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::Decode(const Byte** ppByte, size_t& nBytesRemaining, T* arr, Byte* pMaskBits)
{
  if (!arr || !ppByte || !IsLittleEndianSystem())
    return false;

  const Byte* ptrBlob = *ppByte;    // keep a ptr to the start of the blob
  size_t nBytesRemaining00 = nBytesRemaining;

  if (!ReadHeader(ppByte, nBytesRemaining, m_headerInfo))
    return false;

  if (nBytesRemaining00 < (size_t)m_headerInfo.blobSize)
    return false;

  if (m_headerInfo.version >= 3)
  {
    int nBytes = (int)(FileKey().length() + sizeof(int) + sizeof(unsigned int));    // start right after the checksum entry
    if (m_headerInfo.blobSize < nBytes)
      return false;
    unsigned int checksum = ComputeChecksumFletcher32(ptrBlob + nBytes, m_headerInfo.blobSize - nBytes);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // For fuzzing, ignore checksum verification
    (void)checksum;
#else
    if (checksum != m_headerInfo.checksum)
      return false;
#endif
  }

  if (!ReadMask(ppByte, nBytesRemaining))
    return false;

  if (pMaskBits)    // return proper mask bits even if they were not stored
    memcpy(pMaskBits, m_bitMask.Bits(), m_bitMask.Size());

  memset(arr, 0, m_headerInfo.nCols * m_headerInfo.nRows * m_headerInfo.nDim * sizeof(T));

  if (m_headerInfo.numValidPixel == 0)
    return true;

  if (m_headerInfo.zMin == m_headerInfo.zMax)    // image is const
  {
    if (!FillConstImage(arr))
      return false;

    return true;
  }

  if (m_headerInfo.version >= 4)
  {
    if (!ReadMinMaxRanges(ppByte, nBytesRemaining, arr))
      return false;

    bool minMaxEqual = false;
    if (!CheckMinMaxRanges(minMaxEqual))
      return false;

    if (minMaxEqual)    // if all bands are const, fill outgoing and done
    {
      if (!FillConstImage(arr))
        return false;

      return true;    // done
    }
  }

  if (nBytesRemaining < 1)
    return false;

  Byte readDataOneSweep = **ppByte;    // read flag
  (*ppByte)++;
  nBytesRemaining--;

  if (!readDataOneSweep)
  {
    if (m_headerInfo.TryHuffman())
    {
      if (nBytesRemaining < 1)
        return false;

      Byte flag = **ppByte;    // read flag Huffman / Lerc2
      (*ppByte)++;
      nBytesRemaining--;

      if (flag > 2 || (m_headerInfo.version < 4 && flag > 1))
        return false;

      m_imageEncodeMode = (ImageEncodeMode)flag;

      if (m_imageEncodeMode == IEM_DeltaHuffman || m_imageEncodeMode == IEM_Huffman)
      {
        if (!DecodeHuffman(ppByte, nBytesRemaining, arr))
          return false;

        return true;    // done.
      }
    }

    if (!ReadTiles(ppByte, nBytesRemaining, arr))
      return false;
  }
  else
  {
    if (!ReadDataOneSweep(ppByte, nBytesRemaining, arr))
      return false;
  }

  return true;
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

inline
void Lerc2::AddUIntToCounts(int* pCounts, unsigned int val, int nBits)
{
  pCounts[0] += val & 1;
  for (int i = 1; i < nBits; i++)
    pCounts[i] += (val >>= 1) & 1;
}

// -------------------------------------------------------------------------- ;

inline
void Lerc2::AddIntToCounts(int* pCounts, int val, int nBits)
{
  pCounts[0] += val & 1;
  for (int i = 1; i < nBits; i++)
    pCounts[i] += (val >>= 1) & 1;
}

// -------------------------------------------------------------------------- ;

// for the theory and math, see
// https://pdfs.semanticscholar.org/d064/2e2ad1a4c3b445b0d795770f604a5d9e269c.pdf

template<class T>
bool Lerc2::TryBitPlaneCompression(const T* data, double eps, double& newMaxZError) const
{
  newMaxZError = 0;    // lossless is the obvious fallback

  if (!data || eps <= 0)
    return false;

  const HeaderInfo& hd = m_headerInfo;
  const int nDim = hd.nDim;
  const int maxShift = 8 * GetDataTypeSize(hd.dt);
  const int minCnt = 5000;

  if (hd.numValidPixel < minCnt)    // not enough data for good stats
    return false;

  std::vector<int> cntDiffVec(nDim * maxShift, 0);
  int cnt = 0;

  if (nDim == 1 && hd.numValidPixel == hd.nCols * hd.nRows)    // special but common case
  {
    if (hd.dt == DT_Byte || hd.dt == DT_UShort || hd.dt == DT_UInt)    // unsigned int
    {
      for (int i = 0; i < hd.nRows - 1; i++)
        for (int k = i * hd.nCols, j = 0; j < hd.nCols - 1; j++, k++)
        {
          unsigned int c = ((unsigned int)data[k]) ^ ((unsigned int)data[k + 1]);
          AddUIntToCounts(&cntDiffVec[0], c, maxShift);
          cnt++;
          c = ((unsigned int)data[k]) ^ ((unsigned int)data[k + hd.nCols]);
          AddUIntToCounts(&cntDiffVec[0], c, maxShift);
          cnt++;
        }
    }
    else if (hd.dt == DT_Char || hd.dt == DT_Short || hd.dt == DT_Int)    // signed int
    {
      for (int i = 0; i < hd.nRows - 1; i++)
        for (int k = i * hd.nCols, j = 0; j < hd.nCols - 1; j++, k++)
        {
          int c = ((int)data[k]) ^ ((int)data[k + 1]);
          AddIntToCounts(&cntDiffVec[0], c, maxShift);
          cnt++;
          c = ((int)data[k]) ^ ((int)data[k + hd.nCols]);
          AddIntToCounts(&cntDiffVec[0], c, maxShift);
          cnt++;
        }
    }
    else
      return false;    // unsupported data type
  }

  else    // general case:  nDim > 1 or not all pixel valid
  {
    if (hd.dt == DT_Byte || hd.dt == DT_UShort || hd.dt == DT_UInt)    // unsigned int
    {
      for (int k = 0, m0 = 0, i = 0; i < hd.nRows; i++)
        for (int j = 0; j < hd.nCols; j++, k++, m0 += nDim)
          if (m_bitMask.IsValid(k))
          {
            if (j < hd.nCols - 1 && m_bitMask.IsValid(k + 1))    // hori
            {
              for (int s0 = 0, iDim = 0; iDim < nDim; iDim++, s0 += maxShift)
              {
                unsigned int c = ((unsigned int)data[m0 + iDim]) ^ ((unsigned int)data[m0 + iDim + nDim]);
                AddUIntToCounts(&cntDiffVec[s0], c, maxShift);
              }
              cnt++;
            }
            if (i < hd.nRows - 1 && m_bitMask.IsValid(k + hd.nCols))    // vert
            {
              for (int s0 = 0, iDim = 0; iDim < nDim; iDim++, s0 += maxShift)
              {
                unsigned int c = ((unsigned int)data[m0 + iDim]) ^ ((unsigned int)data[m0 + iDim + nDim * hd.nCols]);
                AddUIntToCounts(&cntDiffVec[s0], c, maxShift);
              }
              cnt++;
            }
          }
    }
    else if (hd.dt == DT_Char || hd.dt == DT_Short || hd.dt == DT_Int)    // signed int
    {
      for (int k = 0, m0 = 0, i = 0; i < hd.nRows; i++)
        for (int j = 0; j < hd.nCols; j++, k++, m0 += nDim)
          if (m_bitMask.IsValid(k))
          {
            if (j < hd.nCols - 1 && m_bitMask.IsValid(k + 1))    // hori
            {
              for (int s0 = 0, iDim = 0; iDim < nDim; iDim++, s0 += maxShift)
              {
                int c = ((int)data[m0 + iDim]) ^ ((int)data[m0 + iDim + nDim]);
                AddIntToCounts(&cntDiffVec[s0], c, maxShift);
              }
              cnt++;
            }
            if (i < hd.nRows - 1 && m_bitMask.IsValid(k + hd.nCols))    // vert
            {
              for (int s0 = 0, iDim = 0; iDim < nDim; iDim++, s0 += maxShift)
              {
                int c = ((int)data[m0 + iDim]) ^ ((int)data[m0 + iDim + nDim * hd.nCols]);
                AddIntToCounts(&cntDiffVec[s0], c, maxShift);
              }
              cnt++;
            }
          }
    }
    else
      return false;    // unsupported data type
  }

  if (cnt < minCnt)    // not enough data for good stats
    return false;

  int nCutFound = 0, lastPlaneKept = 0;
  const bool printAll = false;

  for (int s = maxShift - 1; s >= 0; s--)
  {
    if (printAll) printf("bit plane %2d: ", s);
    bool bCrit = true;

    for (int iDim = 0; iDim < nDim; iDim++)
    {
      double x = cntDiffVec[iDim * maxShift + s];
      double n = cnt;
      double m = x / n;
      //double stdDev = sqrt(x * x / n - m * m) / n;

      //printf("  %.4f +- %.4f  ", (float)(2 * m), (float)(2 * stdDev));
      if (printAll) printf("  %.4f ", (float)(2 * m));

      if (fabs(1 - 2 * m) >= eps)
        bCrit = false;
    }
    if (printAll) printf("\n");

    if (bCrit && nCutFound < 2)
    {
      if (nCutFound == 0)
        lastPlaneKept = s;

      if (nCutFound == 1 && s < lastPlaneKept - 1)
      {
        lastPlaneKept = s;
        nCutFound = 0;
        if (printAll) printf(" reset ");
      }

      nCutFound++;
      if (printAll && nCutFound == 1) printf("\n");
    }
  }

  lastPlaneKept = std::max(0, lastPlaneKept);
  if (printAll) printf("%d \n", lastPlaneKept);

  newMaxZError = (1 << lastPlaneKept) >> 1;    // turn lastPlaneKept into new maxZError

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteDataOneSweep(const T* data, Byte** ppByte) const
{
  if (!data || !ppByte)
    return false;

  Byte* ptr = (*ppByte);
  const HeaderInfo& hd = m_headerInfo;
  int nDim = hd.nDim;
  int len = nDim * sizeof(T);

  for (int k = 0, m0 = 0, i = 0; i < hd.nRows; i++)
    for (int j = 0; j < hd.nCols; j++, k++, m0 += nDim)
      if (m_bitMask.IsValid(k))
      {
        memcpy(ptr, &data[m0], len);
        ptr += len;
      }

  (*ppByte) = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadDataOneSweep(const Byte** ppByte, size_t& nBytesRemaining, T* data) const
{
  if (!data || !ppByte || !(*ppByte))
    return false;

  const Byte* ptr = (*ppByte);
  const HeaderInfo& hd = m_headerInfo;
  int nDim = hd.nDim;
  int len = nDim * sizeof(T);

  size_t nValidPix = (size_t)m_bitMask.CountValidBits();

  if (nBytesRemaining < nValidPix * len)
    return false;

  for (int k = 0, m0 = 0, i = 0; i < hd.nRows; i++)
    for (int j = 0; j < hd.nCols; j++, k++, m0 += nDim)
      if (m_bitMask.IsValid(k))
      {
        memcpy(&data[m0], ptr, len);
        ptr += len;
      }

  (*ppByte) = ptr;
  nBytesRemaining -= nValidPix * len;

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteTiles(const T* data, Byte** ppByte, int& numBytes, std::vector<double>& zMinVec, std::vector<double>& zMaxVec) const
{
  if (!data || !ppByte)
    return false;

  numBytes = 0;
  int numBytesLerc = 0;

  std::vector<unsigned int> quantVec;
  std::vector<std::pair<unsigned int, unsigned int> > sortedQuantVec;

  const HeaderInfo& hd = m_headerInfo;
  int mbSize = hd.microBlockSize;
  int nDim = hd.nDim;

  std::vector<T> dataVec(mbSize * mbSize, 0);
  T* dataBuf = &dataVec[0];

  zMinVec.assign(nDim, DBL_MAX);
  zMaxVec.assign(nDim, -DBL_MAX);

  int numTilesVert = (hd.nRows + mbSize - 1) / mbSize;
  int numTilesHori = (hd.nCols + mbSize - 1) / mbSize;

  for (int iTile = 0; iTile < numTilesVert; iTile++)
  {
    int tileH = mbSize;
    int i0 = iTile * tileH;
    if (iTile == numTilesVert - 1)
      tileH = hd.nRows - i0;

    for (int jTile = 0; jTile < numTilesHori; jTile++)
    {
      int tileW = mbSize;
      int j0 = jTile * tileW;
      if (jTile == numTilesHori - 1)
        tileW = hd.nCols - j0;

      for (int iDim = 0; iDim < nDim; iDim++)
      {
        T zMin = 0, zMax = 0;
        int numValidPixel = 0;
        bool tryLut = false;

        if (!GetValidDataAndStats(data, i0, i0 + tileH, j0, j0 + tileW, iDim, dataBuf, zMin, zMax, numValidPixel, tryLut))
          return false;

        if (numValidPixel > 0)
        {
          zMinVec[iDim] = (std::min)(zMinVec[iDim], (double)zMin);
          zMaxVec[iDim] = (std::max)(zMaxVec[iDim], (double)zMax);
        }

        //tryLut = false;

        // if needed, quantize the data here once
        if ((*ppByte || tryLut) && NeedToQuantize(numValidPixel, zMin, zMax))
        {
          if (!Quantize(dataBuf, numValidPixel, zMin, quantVec))
            return false;

          if (tryLut)
            SortQuantArray(quantVec, sortedQuantVec);
        }

        BlockEncodeMode blockEncodeMode;
        int numBytesNeeded = NumBytesTile(numValidPixel, zMin, zMax, tryLut, blockEncodeMode, sortedQuantVec);
        numBytesLerc += numBytesNeeded;

        if (*ppByte)
        {
          int numBytesWritten = 0;

          if (!WriteTile(dataBuf, numValidPixel, ppByte, numBytesWritten, j0, zMin, zMax, quantVec, blockEncodeMode, sortedQuantVec))
            return false;

          if (numBytesWritten != numBytesNeeded)
            return false;
        }
      }
    }
  }

  numBytes += numBytesLerc;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadTiles(const Byte** ppByte, size_t& nBytesRemaining, T* data) const
{
  if (!data || !ppByte || !(*ppByte))
    return false;

  std::vector<unsigned int> bufferVec;

  const HeaderInfo& hd = m_headerInfo;
  int mbSize = hd.microBlockSize;
  int nDim = hd.nDim;

  if (mbSize > 32)  // fail gracefully in case of corrupted blob for old version <= 2 which had no checksum
    return false;

  if( mbSize <= 0 || hd.nRows < 0 || hd.nCols < 0 ||
      hd.nRows > std::numeric_limits<int>::max() - (mbSize - 1) ||
      hd.nCols > std::numeric_limits<int>::max() - (mbSize - 1) )
  {
    return false;
  }
  int numTilesVert = (hd.nRows + mbSize - 1) / mbSize;
  int numTilesHori = (hd.nCols + mbSize - 1) / mbSize;

  for (int iTile = 0; iTile < numTilesVert; iTile++)
  {
    int tileH = mbSize;
    int i0 = iTile * tileH;
    if (iTile == numTilesVert - 1)
      tileH = hd.nRows - i0;

    for (int jTile = 0; jTile < numTilesHori; jTile++)
    {
      int tileW = mbSize;
      int j0 = jTile * tileW;
      if (jTile == numTilesHori - 1)
        tileW = hd.nCols - j0;

      for (int iDim = 0; iDim < nDim; iDim++)
      {
        if (!ReadTile(ppByte, nBytesRemaining, data, i0, i0 + tileH, j0, j0 + tileW, iDim, bufferVec))
          return false;
      }
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::GetValidDataAndStats(const T* data, int i0, int i1, int j0, int j1, int iDim,
  T* dataBuf, T& zMin, T& zMax, int& numValidPixel, bool& tryLut) const
{
  const HeaderInfo& hd = m_headerInfo;

  if (!data || i0 < 0 || j0 < 0 || i1 > hd.nRows || j1 > hd.nCols || iDim < 0 || iDim > hd.nDim || !dataBuf)
    return false;

  zMin = 0;
  zMax = 0;
  tryLut = false;

  T prevVal = 0;
  int cnt = 0, cntSameVal = 0;
  int nDim = hd.nDim;

  if (hd.numValidPixel == hd.nCols * hd.nRows)    // all valid, no mask
  {
    for (int i = i0; i < i1; i++)
    {
      int k = i * hd.nCols + j0;
      int m = k * nDim + iDim;

      for (int j = j0; j < j1; j++, k++, m += nDim)
      {
        T val = data[m];
        dataBuf[cnt] = val;

        if (cnt > 0)
        {
          if (val < zMin)
            zMin = val;
          else if (val > zMax)
            zMax = val;

          if (val == prevVal)
            cntSameVal++;
        }
        else
          zMin = zMax = val;    // init

        prevVal = val;
        cnt++;
      }
    }
  }
  else    // not all valid, use mask
  {
    for (int i = i0; i < i1; i++)
    {
      int k = i * hd.nCols + j0;
      int m = k * nDim + iDim;

      for (int j = j0; j < j1; j++, k++, m += nDim)
        if (m_bitMask.IsValid(k))
        {
          T val = data[m];
          dataBuf[cnt] = val;

          if (cnt > 0)
          {
            if (val < zMin)
              zMin = val;
            else if (val > zMax)
              zMax = val;

            if (val == prevVal)
              cntSameVal++;
          }
          else
            zMin = zMax = val;    // init

          prevVal = val;
          cnt++;
        }
    }
  }

  if (cnt > 4)
    tryLut = (zMax > zMin + hd.maxZError) && (2 * cntSameVal > cnt);

  numValidPixel = cnt;
  return true;
}

// -------------------------------------------------------------------------- ;

inline double Lerc2::ComputeMaxVal(double zMin, double zMax, double maxZError)
{
  double fac = 1 / (2 * maxZError);    // must match the code in Decode(), don't touch it
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
bool Lerc2::Quantize(const T* dataBuf, int num, T zMin, std::vector<unsigned int>& quantVec) const
{
  quantVec.resize(num);

  if (m_headerInfo.dt < DT_Float && m_headerInfo.maxZError == 0.5)    // int lossless
  {
    for (int i = 0; i < num; i++)
      quantVec[i] = (unsigned int)(dataBuf[i] - zMin);    // ok, as char, short get promoted to int by C++ integral promotion rule
  }
  else    // float and/or lossy
  {
    double scale = 1 / (2 * m_headerInfo.maxZError);
    double zMinDbl = (double)zMin;

    for (int i = 0; i < num; i++)
      quantVec[i] = (unsigned int)(((double)dataBuf[i] - zMinDbl) * scale + 0.5);    // ok, consistent with ComputeMaxVal(...)
      //quantVec[i] = (unsigned int)((dataBuf[i] - zMin) * scale + 0.5);    // bad, not consistent with ComputeMaxVal(...)
  }

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
int Lerc2::NumBytesTile(int numValidPixel, T zMin, T zMax, bool tryLut, BlockEncodeMode& blockEncodeMode,
                         const std::vector<std::pair<unsigned int, unsigned int> >& sortedQuantVec) const
{
  blockEncodeMode = BEM_RawBinary;

  if (numValidPixel == 0 || (zMin == 0 && zMax == 0))
    return 1;

  double maxVal = 0, maxZError = m_headerInfo.maxZError;
  int nBytesRaw = (int)(1 + numValidPixel * sizeof(T));

  if ((maxZError == 0 && zMax > zMin)
    || (maxZError > 0 && (maxVal = ComputeMaxVal(zMin, zMax, maxZError)) > m_maxValToQuantize))
  {
    return nBytesRaw;
  }
  else
  {
    DataType dtUsed;
    TypeCode(zMin, dtUsed);
    int nBytes = 1 + GetDataTypeSize(dtUsed);

    unsigned int maxElem = (unsigned int)(maxVal + 0.5);
    if (maxElem > 0)
    {
      nBytes += (!tryLut) ? m_bitStuffer2.ComputeNumBytesNeededSimple(numValidPixel, maxElem)
                          : m_bitStuffer2.ComputeNumBytesNeededLut(sortedQuantVec, tryLut);
    }

    if (nBytes < nBytesRaw)
      blockEncodeMode = (!tryLut || maxElem == 0) ? BEM_BitStuffSimple : BEM_BitStuffLUT;
    else
      nBytes = nBytesRaw;

    return nBytes;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteTile(const T* dataBuf, int num, Byte** ppByte, int& numBytesWritten, int j0, T zMin, T zMax,
  const std::vector<unsigned int>& quantVec, BlockEncodeMode blockEncodeMode,
  const std::vector<std::pair<unsigned int, unsigned int> >& sortedQuantVec) const
{
  Byte* ptr = *ppByte;
  Byte comprFlag = ((j0 >> 3) & 15) << 2;    // use bits 2345 for integrity check

  if (num == 0 || (zMin == 0 && zMax == 0))    // special cases
  {
    *ptr++ = comprFlag | 2;    // set compression flag to 2 to mark tile as constant 0
    numBytesWritten = 1;
    *ppByte = ptr;
    return true;
  }

  if (blockEncodeMode == BEM_RawBinary)
  {
    *ptr++ = comprFlag | 0;    // write z's binary uncompressed

    memcpy(ptr, dataBuf, num * sizeof(T));
    ptr += num * sizeof(T);
  }
  else
  {
    double maxVal = (m_headerInfo.maxZError > 0) ? ComputeMaxVal(zMin, zMax, m_headerInfo.maxZError) : 0;

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
      if ((int)quantVec.size() != num)
        return false;

      if (blockEncodeMode == BEM_BitStuffSimple)
      {
        if (!m_bitStuffer2.EncodeSimple(&ptr, quantVec, m_headerInfo.version))
          return false;
      }
      else if (blockEncodeMode == BEM_BitStuffLUT)
      {
        if (!m_bitStuffer2.EncodeLut(&ptr, sortedQuantVec, m_headerInfo.version))
          return false;
      }
      else
        return false;
    }
  }

  numBytesWritten = (int)(ptr - *ppByte);
  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadTile(const Byte** ppByte, size_t& nBytesRemainingInOut, T* data, int i0, int i1, int j0, int j1, int iDim,
                     std::vector<unsigned int>& bufferVec) const
{
  const Byte* ptr = *ppByte;
  size_t nBytesRemaining = nBytesRemainingInOut;

  if (nBytesRemaining < 1)
    return false;

  Byte comprFlag = *ptr++;
  nBytesRemaining--;

  int bits67 = comprFlag >> 6;
  int testCode = (comprFlag >> 2) & 15;    // use bits 2345 for integrity check
  if (testCode != ((j0 >> 3) & 15))
    return false;

  const HeaderInfo& hd = m_headerInfo;
  int nCols = hd.nCols;
  int nDim = hd.nDim;

  comprFlag &= 3;

  if (comprFlag == 2)    // entire tile is constant 0 (all the valid pixels)
  {
    for (int i = i0; i < i1; i++)
    {
      int k = i * nCols + j0;
      int m = k * nDim + iDim;

      for (int j = j0; j < j1; j++, k++, m += nDim)
        if (m_bitMask.IsValid(k))
          data[m] = 0;
    }

    *ppByte = ptr;
    nBytesRemainingInOut = nBytesRemaining;
    return true;
  }

  else if (comprFlag == 0)    // read z's binary uncompressed
  {
    const T* srcPtr = (const T*)ptr;
    int cnt = 0;

    for (int i = i0; i < i1; i++)
    {
      int k = i * nCols + j0;
      int m = k * nDim + iDim;

      for (int j = j0; j < j1; j++, k++, m += nDim)
        if (m_bitMask.IsValid(k))
        {
          if (nBytesRemaining < sizeof(T))
            return false;

          data[m] = *srcPtr++;
          nBytesRemaining -= sizeof(T);

          cnt++;
        }
    }

    ptr += cnt * sizeof(T);
  }
  else
  {
    // read z's as int arr bit stuffed
    DataType dtUsed = GetDataTypeUsed(bits67);
    if( dtUsed == DT_Undefined )
      return false;
    size_t n = GetDataTypeSize(dtUsed);
    if (nBytesRemaining < n)
      return false;

    double offset = ReadVariableDataType(&ptr, dtUsed);
    nBytesRemaining -= n;

    if (comprFlag == 3)    // entire tile is constant zMin (all the valid pixels)
    {
      for (int i = i0; i < i1; i++)
      {
        int k = i * nCols + j0;
        int m = k * nDim + iDim;

        for (int j = j0; j < j1; j++, k++, m += nDim)
          if (m_bitMask.IsValid(k))
            data[m] = (T)offset;
      }
    }
    else
    {
      size_t maxElementCount = (i1 - i0) * (j1 - j0);
      if (!m_bitStuffer2.Decode(&ptr, nBytesRemaining, bufferVec, maxElementCount, hd.version))
        return false;

      double invScale = 2 * hd.maxZError;    // for int types this is int
      double zMax = (hd.version >= 4 && nDim > 1) ? m_zMaxVec[iDim] : hd.zMax;
      const unsigned int* srcPtr = bufferVec.data();

      if (bufferVec.size() == maxElementCount)    // all valid
      {
        for (int i = i0; i < i1; i++)
        {
          int k = i * nCols + j0;
          int m = k * nDim + iDim;

          for (int j = j0; j < j1; j++, k++, m += nDim)
          {
            double z = offset + *srcPtr++ * invScale;
            data[m] = (T)std::min(z, zMax);    // make sure we stay in the orig range
          }
        }
      }
      else    // not all valid
      {
#ifndef GDAL_COMPILATION
        if (hd.version > 2)
        {
          for (int i = i0; i < i1; i++)
          {
            int k = i * nCols + j0;
            int m = k * nDim + iDim;

            for (int j = j0; j < j1; j++, k++, m += nDim)
              if (m_bitMask.IsValid(k))
              {
                double z = offset + *srcPtr++ * invScale;
                data[m] = (T)std::min(z, zMax);    // make sure we stay in the orig range
              }
          }
        }
        else  // fail gracefully in case of corrupted blob for old version <= 2 which had no checksum
#endif
        {
          size_t bufferVecIdx = 0;

          for (int i = i0; i < i1; i++)
          {
            int k = i * nCols + j0;
            int m = k * nDim + iDim;

            for (int j = j0; j < j1; j++, k++, m += nDim)
              if (m_bitMask.IsValid(k))
              {
                if (bufferVecIdx == bufferVec.size())  // fail gracefully in case of corrupted blob for old version <= 2 which had no checksum
                  return false;

                double z = offset + bufferVec[bufferVecIdx] * invScale;
                bufferVecIdx++;
                data[m] = (T)std::min(z, zMax);    // make sure we stay in the orig range
              }
          }
        }
      }
    }
  }

  *ppByte = ptr;
  nBytesRemainingInOut = nBytesRemaining;
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
      signed char c = (signed char)z;
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
      int tc = (T)b == z ? 3 : (T)s == z ? 2 : (T)us == z ? 1 : 0;
      dtUsed = (DataType)(dt - tc);
      return tc;
    }
    case DT_UInt:
    {
      unsigned short us = (unsigned short)z;
      int tc = (T)b == z ? 2 : (T)us == z ? 1 : 0;
      dtUsed = (DataType)(dt - 2 * tc);
      return tc;
    }
    case DT_Float:
    {
      short s = (short)z;
      int tc = (T)b == z ? 2 : (T)s == z ? 1 : 0;
      dtUsed = tc == 0 ? dt : (tc == 1 ? DT_Short : DT_Byte);
      return tc;
    }
    case DT_Double:
    {
      short s = (short)z;
      int l = (int)z;
      float f = (float)z;
      int tc = (T)s == z ? 3 : (T)l == z ? 2 : (T)f == z ? 1 : 0;
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

inline Lerc2::DataType Lerc2::ValidateDataType(int dt)
{
  if( dt >= DT_Char && dt <= DT_Double )
    return static_cast<DataType>(dt);
  return DT_Undefined;
}

// -------------------------------------------------------------------------- ;

inline
Lerc2::DataType Lerc2::GetDataTypeUsed(int tc) const
{
  DataType dt = m_headerInfo.dt;
  switch (dt)
  {
    case DT_Short:
    case DT_Int:     return ValidateDataType(dt - tc);
    case DT_UShort:
    case DT_UInt:    return ValidateDataType(dt - 2 * tc);
    case DT_Float:   return tc == 0 ? dt : (tc == 1 ? DT_Short : DT_Byte);
    case DT_Double:  return tc == 0 ? dt : ValidateDataType(dt - 2 * tc + 1);
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
    case DT_Char:
    {
      *((signed char*)ptr) = (signed char)z;
      ptr++;
      break;
    }
    case DT_Byte:
    {
      *((Byte*)ptr) = (Byte)z;
      ptr++;
      break;
    }
    case DT_Short:
    {
      short s = (short)z;
      memcpy(ptr, &s, sizeof(short));
      ptr += 2;
      break;
    }
    case DT_UShort:
    {
      unsigned short us = (unsigned short)z;
      memcpy(ptr, &us, sizeof(unsigned short));
      ptr += 2;
      break;
    }
    case DT_Int:
    {
      int i = (int)z;
      memcpy(ptr, &i, sizeof(int));
      ptr += 4;
      break;
    }
    case DT_UInt:
    {
      unsigned int n = (unsigned int)z;
      memcpy(ptr, &n, sizeof(unsigned int));
      ptr += 4;
      break;
    }
    case DT_Float:
    {
      float f = (float)z;
      memcpy(ptr, &f, sizeof(float));
      ptr += 4;
      break;
    }
    case DT_Double:
    {
      memcpy(ptr, &z, sizeof(double));
      ptr += 8;
      break;
    }

    default:
      return false;
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

inline
double Lerc2::ReadVariableDataType(const Byte** ppByte, DataType dtUsed)
{
  const Byte* ptr = *ppByte;

  switch (dtUsed)
  {
    case DT_Char:
    {
      signed char c = *((signed char*)ptr);
      *ppByte = ptr + 1;
      return c;
    }
    case DT_Byte:
    {
      Byte b = *((Byte*)ptr);
      *ppByte = ptr + 1;
      return b;
    }
    case DT_Short:
    {
      short s;
      memcpy(&s, ptr, sizeof(short));
      *ppByte = ptr + 2;
      return s;
    }
    case DT_UShort:
    {
      unsigned short us;
      memcpy(&us, ptr, sizeof(unsigned short));
      *ppByte = ptr + 2;
      return us;
    }
    case DT_Int:
    {
      int i;
      memcpy(&i, ptr, sizeof(int));
      *ppByte = ptr + 4;
      return i;
    }
    case DT_UInt:
    {
      unsigned int n;
      memcpy(&n, ptr, sizeof(unsigned int));
      *ppByte = ptr + 4;
      return n;
    }
    case DT_Float:
    {
      float f;
      memcpy(&f, ptr, sizeof(float));
      *ppByte = ptr + 4;
      return f;
    }
    case DT_Double:
    {
      double d;
      memcpy(&d, ptr, sizeof(double));
      *ppByte = ptr + 8;
      return d;
    }
    default:
      return 0;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
Lerc2::DataType Lerc2::GetDataType(T z) const
{
  const std::type_info& ti = typeid(z);

       if (ti == typeid(signed char))     return DT_Char;
  else if (ti == typeid(Byte))            return DT_Byte;
  else if (ti == typeid(short))           return DT_Short;
  else if (ti == typeid(unsigned short))  return DT_UShort;
  else if (ti == typeid(int ) && sizeof(int ) == 4)   return DT_Int;
  else if (ti == typeid(long) && sizeof(long) == 4)   return DT_Int;
  else if (ti == typeid(unsigned int ) && sizeof(unsigned int ) == 4)   return DT_UInt;
  else if (ti == typeid(unsigned long) && sizeof(unsigned long) == 4)   return DT_UInt;
  else if (ti == typeid(float))           return DT_Float;
  else if (ti == typeid(double))          return DT_Double;
  else
    return DT_Undefined;
}

// -------------------------------------------------------------------------- ;

inline
unsigned int Lerc2::GetMaxValToQuantize(DataType dt)
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

inline
unsigned int Lerc2::GetDataTypeSize(DataType dt)
{
  switch (dt)
  {
    case DT_Char:
    case DT_Byte:   return 1;
    case DT_Short:
    case DT_UShort: return 2;
    case DT_Int:
    case DT_UInt:
    case DT_Float:  return 4;
    case DT_Double: return 8;

    default:
      return 0;
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
void Lerc2::ComputeHuffmanCodes(const T* data, int& numBytes, ImageEncodeMode& imageEncodeMode, std::vector<std::pair<unsigned short, unsigned int> >& codes) const
{
  std::vector<int> histo, deltaHisto;
  ComputeHistoForHuffman(data, histo, deltaHisto);

  int nBytes0 = 0, nBytes1 = 0;
  double avgBpp0 = 0, avgBpp1 = 0;
  Huffman huffman0, huffman1;

  if (m_headerInfo.version >= 4)
  {
    if (!huffman0.ComputeCodes(histo) || !huffman0.ComputeCompressedSize(histo, nBytes0, avgBpp0))
      nBytes0 = 0;
  }

  if (!huffman1.ComputeCodes(deltaHisto) || !huffman1.ComputeCompressedSize(deltaHisto, nBytes1, avgBpp1))
    nBytes1 = 0;

  if (nBytes0 > 0 && nBytes1 > 0)    // regular case, pick the better of the two
  {
    imageEncodeMode = (nBytes0 <= nBytes1) ? IEM_Huffman : IEM_DeltaHuffman;
    codes = (nBytes0 <= nBytes1) ? huffman0.GetCodes() : huffman1.GetCodes();
    numBytes = (std::min)(nBytes0, nBytes1);
  }
  else if (nBytes0 == 0 && nBytes1 == 0)    // rare case huffman cannot handle, fall back to tiling
  {
    imageEncodeMode = IEM_Tiling;
    codes.resize(0);
    numBytes = 0;
  }
  else    // rare also, pick the valid one, the other is 0
  {
    imageEncodeMode = (nBytes0 > nBytes1) ? IEM_Huffman : IEM_DeltaHuffman;
    codes = (nBytes0 > nBytes1) ? huffman0.GetCodes() : huffman1.GetCodes();
    numBytes = (std::max)(nBytes0, nBytes1);
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
void Lerc2::ComputeHistoForHuffman(const T* data, std::vector<int>& histo, std::vector<int>& deltaHisto) const
{
  histo.resize(256);
  deltaHisto.resize(256);

  memset(&histo[0], 0, histo.size() * sizeof(int));
  memset(&deltaHisto[0], 0, deltaHisto.size() * sizeof(int));

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  int nDim = m_headerInfo.nDim;

  if (m_headerInfo.numValidPixel == width * height)    // all valid
  {
    for (int iDim = 0; iDim < nDim; iDim++)
    {
      T prevVal = 0;
      for (int m = iDim, i = 0; i < height; i++)
        for (int j = 0; j < width; j++, m += nDim)
        {
          T val = data[m];
          T delta = val;

          if (j > 0)
            delta -= prevVal;    // use overflow
          else if (i > 0)
            delta -= data[m - width * nDim];
          else
            delta -= prevVal;

          prevVal = val;

          histo[offset + (int)val]++;
          deltaHisto[offset + (int)delta]++;
        }
    }
  }
  else    // not all valid
  {
    for (int iDim = 0; iDim < nDim; iDim++)
    {
      T prevVal = 0;
      for (int k = 0, m = iDim, i = 0; i < height; i++)
        for (int j = 0; j < width; j++, k++, m += nDim)
          if (m_bitMask.IsValid(k))
          {
            T val = data[m];
            T delta = val;

            if (j > 0 && m_bitMask.IsValid(k - 1))
            {
              delta -= prevVal;    // use overflow
            }
            else if (i > 0 && m_bitMask.IsValid(k - width))
            {
              delta -= data[m - width * nDim];
            }
            else
              delta -= prevVal;

            prevVal = val;

            histo[offset + (int)val]++;
            deltaHisto[offset + (int)delta]++;
          }
    }
  }
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::EncodeHuffman(const T* data, Byte** ppByte) const
{
  if (!data || !ppByte)
    return false;

  Huffman huffman;
  if (!huffman.SetCodes(m_huffmanCodes) || !huffman.WriteCodeTable(ppByte, m_headerInfo.version))    // header and code table
    return false;

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  int nDim = m_headerInfo.nDim;

  unsigned int* arr = (unsigned int*)(*ppByte);
  unsigned int* dstPtr = arr;
  int bitPos = 0;

  if (m_imageEncodeMode == IEM_DeltaHuffman)
  {
    for (int iDim = 0; iDim < nDim; iDim++)
    {
      T prevVal = 0;
      for (int k = 0, m = iDim, i = 0; i < height; i++)
        for (int j = 0; j < width; j++, k++, m += nDim)
          if (m_bitMask.IsValid(k))
          {
            T val = data[m];
            T delta = val;

            if (j > 0 && m_bitMask.IsValid(k - 1))
            {
              delta -= prevVal;    // use overflow
            }
            else if (i > 0 && m_bitMask.IsValid(k - width))
            {
              delta -= data[m - width * nDim];
            }
            else
              delta -= prevVal;

            prevVal = val;

            // bit stuff the huffman code for this delta
            int kBin = offset + (int)delta;
            int len = m_huffmanCodes[kBin].first;
            if (len <= 0)
              return false;

            unsigned int code = m_huffmanCodes[kBin].second;

            if (32 - bitPos >= len)
            {
              if (bitPos == 0)
                *dstPtr = 0;

              *dstPtr |= code << (32 - bitPos - len);
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
              *dstPtr++ |= code >> bitPos;
              *dstPtr = code << (32 - bitPos);
            }
          }
    }
  }

  else if (m_imageEncodeMode == IEM_Huffman)
  {
    for (int k = 0, m0 = 0, i = 0; i < height; i++)
      for (int j = 0; j < width; j++, k++, m0 += nDim)
        if (m_bitMask.IsValid(k))
          for (int m = 0; m < nDim; m++)
          {
            T val = data[m0 + m];

            // bit stuff the huffman code for this val
            int kBin = offset + (int)val;
            int len = m_huffmanCodes[kBin].first;
            if (len <= 0)
              return false;

            unsigned int code = m_huffmanCodes[kBin].second;

            if (32 - bitPos >= len)
            {
              if (bitPos == 0)
                *dstPtr = 0;

              *dstPtr |= code << (32 - bitPos - len);
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
              *dstPtr++ |= code >> bitPos;
              *dstPtr = code << (32 - bitPos);
            }
          }
  }

  else
    return false;

  size_t numUInts = dstPtr - arr + (bitPos > 0 ? 1 : 0) + 1;    // add one more as the decode LUT can read ahead
  *ppByte += numUInts * sizeof(unsigned int);
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::DecodeHuffman(const Byte** ppByte, size_t& nBytesRemainingInOut, T* data) const
{
  if (!data || !ppByte || !(*ppByte))
    return false;

  Huffman huffman;
  if (!huffman.ReadCodeTable(ppByte, nBytesRemainingInOut, m_headerInfo.version))    // header and code table
    return false;

  int numBitsLUT = 0;
  if (!huffman.BuildTreeFromCodes(numBitsLUT))
    return false;

  int offset = (m_headerInfo.dt == DT_Char) ? 128 : 0;
  int height = m_headerInfo.nRows;
  int width = m_headerInfo.nCols;
  int nDim = m_headerInfo.nDim;

  const unsigned int* arr = (const unsigned int*)(*ppByte);
  const unsigned int* srcPtr = arr;
  int bitPos = 0;
  size_t nBytesRemaining = nBytesRemainingInOut;

  if (m_headerInfo.numValidPixel == width * height)    // all valid
  {
    if (m_imageEncodeMode == IEM_DeltaHuffman)
    {
      for (int iDim = 0; iDim < nDim; iDim++)
      {
        T prevVal = 0;
        for (int m = iDim, i = 0; i < height; i++)
          for (int j = 0; j < width; j++, m += nDim)
          {
            int val = 0;
            if (nBytesRemaining >= 4 * sizeof(unsigned int))
            {
              if (!huffman.DecodeOneValue_NoOverrunCheck(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                return false;
            }
            else
            {
              if (!huffman.DecodeOneValue(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                return false;
            }

            T delta = (T)(val - offset);

            if (j > 0)
              delta += prevVal;    // use overflow
            else if (i > 0)
              delta += data[m - width * nDim];
            else
              delta += prevVal;

            data[m] = delta;
            prevVal = delta;
          }
      }
    }

    else if (m_imageEncodeMode == IEM_Huffman)
    {
      for (int k = 0, m0 = 0, i = 0; i < height; i++)
        for (int j = 0; j < width; j++, k++, m0 += nDim)
          for (int m = 0; m < nDim; m++)
          {
            int val = 0;
            if (nBytesRemaining >= 4 * sizeof(unsigned int))
            {
              if (!huffman.DecodeOneValue_NoOverrunCheck(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                return false;
            }
            else
            {
              if (!huffman.DecodeOneValue(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                return false;
            }

            data[m0 + m] = (T)(val - offset);
          }
    }

    else
      return false;
  }

  else    // not all valid
  {
    if (m_imageEncodeMode == IEM_DeltaHuffman)
    {
      for (int iDim = 0; iDim < nDim; iDim++)
      {
        T prevVal = 0;
        for (int k = 0, m = iDim, i = 0; i < height; i++)
          for (int j = 0; j < width; j++, k++, m += nDim)
            if (m_bitMask.IsValid(k))
            {
              int val = 0;
              if (nBytesRemaining >= 4 * sizeof(unsigned int))
              {
                if (!huffman.DecodeOneValue_NoOverrunCheck(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                  return false;
              }
              else
              {
                if (!huffman.DecodeOneValue(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                  return false;
              }

              T delta = (T)(val - offset);

              if (j > 0 && m_bitMask.IsValid(k - 1))
              {
                delta += prevVal;    // use overflow
              }
              else if (i > 0 && m_bitMask.IsValid(k - width))
              {
                delta += data[m - width * nDim];
              }
              else
                delta += prevVal;

              data[m] = delta;
              prevVal = delta;
            }
      }
    }

    else if (m_imageEncodeMode == IEM_Huffman)
    {
      for (int k = 0, m0 = 0, i = 0; i < height; i++)
        for (int j = 0; j < width; j++, k++, m0 += nDim)
          if (m_bitMask.IsValid(k))
            for (int m = 0; m < nDim; m++)
            {
              int val = 0;
              if (nBytesRemaining >= 4 * sizeof(unsigned int))
              {
                if (!huffman.DecodeOneValue_NoOverrunCheck(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                  return false;
              }
              else
              {
                if (!huffman.DecodeOneValue(&srcPtr, nBytesRemaining, bitPos, numBitsLUT, val))
                  return false;
              }

              data[m0 + m] = (T)(val - offset);
            }
    }

    else
      return false;
  }

  size_t numUInts = srcPtr - arr + (bitPos > 0 ? 1 : 0) + 1;    // add one more as the decode LUT can read ahead
  size_t len = numUInts * sizeof(unsigned int);

  if (nBytesRemainingInOut < len)
    return false;

  *ppByte += len;
  nBytesRemainingInOut -= len;
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::WriteMinMaxRanges(const T* /*data*/, Byte** ppByte) const
{
  if (!ppByte || !(*ppByte))
    return false;

  //printf("write min / max = %f  %f\n", m_zMinVec[0], m_zMaxVec[0]);

  int nDim = m_headerInfo.nDim;
  if (/* nDim < 2 || */ (int)m_zMinVec.size() != nDim || (int)m_zMaxVec.size() != nDim)
    return false;

  std::vector<T> zVec(nDim);
  size_t len = nDim * sizeof(T);

  for (int i = 0; i < nDim; i++)
    zVec[i] = (T)m_zMinVec[i];

  memcpy(*ppByte, &zVec[0], len);
  (*ppByte) += len;

  for (int i = 0; i < nDim; i++)
    zVec[i] = (T)m_zMaxVec[i];

  memcpy(*ppByte, &zVec[0], len);
  (*ppByte) += len;

  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::ReadMinMaxRanges(const Byte** ppByte, size_t& nBytesRemaining, const T* /*data*/)
{
  if (!ppByte || !(*ppByte))
    return false;

  int nDim = m_headerInfo.nDim;

  m_zMinVec.resize(nDim);
  m_zMaxVec.resize(nDim);

  std::vector<T> zVec(nDim);
  size_t len = nDim * sizeof(T);

  if (nBytesRemaining < len || !memcpy(&zVec[0], *ppByte, len))
    return false;

  (*ppByte) += len;
  nBytesRemaining -= len;

  for (int i = 0; i < nDim; i++)
    m_zMinVec[i] = zVec[i];

  if (nBytesRemaining < len || !memcpy(&zVec[0], *ppByte, len))
    return false;

  (*ppByte) += len;
  nBytesRemaining -= len;

  for (int i = 0; i < nDim; i++)
    m_zMaxVec[i] = zVec[i];

  //printf("read min / max = %f  %f\n", m_zMinVec[0], m_zMaxVec[0]);

  return true;
}

// -------------------------------------------------------------------------- ;

inline
bool Lerc2::CheckMinMaxRanges(bool& minMaxEqual) const
{
  int nDim = m_headerInfo.nDim;
  if ((int)m_zMinVec.size() != nDim || (int)m_zMaxVec.size() != nDim)
    return false;

  minMaxEqual = (0 == memcmp(&m_zMinVec[0], &m_zMaxVec[0], nDim * sizeof(m_zMinVec[0])));
  return true;
}

// -------------------------------------------------------------------------- ;

template<class T>
bool Lerc2::FillConstImage(T* data) const
{
  if (!data)
    return false;

  const HeaderInfo& hd = m_headerInfo;
  int nCols = hd.nCols;
  int nRows = hd.nRows;
  int nDim = hd.nDim;
  T z0 = (T)hd.zMin;

  if (nDim == 1)
  {
    for (int k = 0, i = 0; i < nRows; i++)
      for (int j = 0; j < nCols; j++, k++)
        if (m_bitMask.IsValid(k))
          data[k] = z0;
  }
  else
  {
    std::vector<T> zBufVec(nDim, z0);

    if (hd.zMin != hd.zMax)
    {
      if ((int)m_zMinVec.size() != nDim)
        return false;

      for (int m = 0; m < nDim; m++)
        zBufVec[m] = (T)m_zMinVec[m];
    }

    int len = nDim * sizeof(T);
    for (int k = 0, m = 0, i = 0; i < nRows; i++)
      for (int j = 0; j < nCols; j++, k++, m += nDim)
        if (m_bitMask.IsValid(k))
          memcpy(&data[m], &zBufVec[0], len);
  }

  return true;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
