
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

#ifndef CNTZIMAGE_H
#define CNTZIMAGE_H

// Define on big endian machines
// #define BIG_ENDIAN

// ---- includes ------------------------------------------------------------ ;

#include "TImage.hpp"
#include "BitStuffer.h"

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

// ---- related classes ----------------------------------------------------- ;

//class String;

// -------------------------------------------------------------------------- ;

/**     count / z image
 *
 *      count can also be a weight, therefore float;
 *      z can be elevation or intensity;
 */

class CntZImage : public TImage< CntZ >
{
public:
  CntZImage();
  const std::string getTypeString() const override { return "CntZImage "; }

  bool resizeFill0(int width, int height);
  bool hasValidPixel() const;
  void normalize();

  /// binary file IO with optional compression
  /// (maxZError = 0  means no lossy compression for Z; the Cnt part is compressed lossless or not at all)
  /// read succeeds only if maxZError on file <= maxZError requested (!)

  unsigned int computeNumBytesNeededToWrite(double maxZError, bool onlyZPart = false)
    { return computeNumBytesNeededToWrite(maxZError, onlyZPart, m_infoFromComputeNumBytes); }

  static unsigned int numExtraBytesToAllocate()    { return BitStuffer::numExtraBytesToAllocate(); }
  static unsigned int computeNumBytesNeededToWriteVoidImage();

  /// these 2 do not allocate memory. Byte ptr is moved like a file pointer.
  bool write(Byte** ppByte,
             double maxZError = 0,
             bool useInfoFromPrevComputeNumBytes = false,
             bool onlyZPart = false) const;

  bool read(Byte** ppByte,
            size_t& nRemainingBytes,
             double maxZError,
             bool onlyHeader = false,
             bool onlyZPart = false);

  template<class T> bool ConvertToMemBlock(T* arr, T noDataValue) const
  {
    if (!arr)
      return false;

    const CntZ* srcPtr = getData();
    T* dstPtr = arr;

    for (int i = 0; i < height_*width_; i++)
    {
        if (srcPtr->cnt > 0)
            *dstPtr++ = (T)srcPtr->z;
        else
            *dstPtr++ = noDataValue;
        srcPtr++;
    }

    return true;
  }

protected:

  struct InfoFromComputeNumBytes
  {
    double maxZError;
    bool cntsNoInt;
    int numTilesVertCnt;
    int numTilesHoriCnt;
    int numBytesCnt;
    float maxCntInImg;
    int numTilesVertZ;
    int numTilesHoriZ;
    int numBytesZ;
    float maxZInImg;
  };

  unsigned int computeNumBytesNeededToWrite(double maxZError, bool onlyZPart,
    InfoFromComputeNumBytes& info) const;

  bool findTiling(bool zPart, double maxZError, bool cntsNoInt,
    int& numTilesVert, int& numTilesHori, int& numBytesOpt, float& maxValInImg) const;

  bool writeTiles(bool zPart, double maxZError, bool cntsNoInt,
    int numTilesVert, int numTilesHori, Byte* bArr, int& numBytes, float& maxValInImg) const;

  bool readTiles( bool zPart, double maxZErrorInFile,
    int numTilesVert, int numTilesHori, float maxValInImg, Byte* bArr, size_t nRemainingBytes);

  bool cntsNoInt() const;
  bool computeCntStats(int i0, int i1, int j0, int j1, float& cntMin, float& cntMax) const;
  bool computeZStats(  int i0, int i1, int j0, int j1, float& zMin, float& zMax, int& numValidPixel) const;

  int numBytesCntTile(int numPixel, float cntMin, float cntMax, bool cntsNoInt) const;
  int numBytesZTile(int numValidPixel, float zMin, float zMax, double maxZError) const;

  bool writeCntTile(Byte** ppByte, int& numBytes, int i0, int i1, int j0, int j1,
    float cntMin, float cntMax, bool cntsNoInt) const;

  bool writeZTile(Byte** ppByte, int& numBytes, int i0, int i1, int j0, int j1,
    int numValidPixel, float zMin, float zMax, double maxZError) const;

  bool readCntTile(Byte** ppByte, size_t& nRemainingBytes, int i0, int i1, int j0, int j1);
  bool readZTile(  Byte** ppByte, size_t& nRemainingBytes, int i0, int i1, int j0, int j1, double maxZErrorInFile, float maxZInImg);

  static int numBytesFlt(float z);    // returns 1, 2, or 4
  // These are not portable on architectures that enforce alignment
  static bool writeFlt(Byte** ppByte, float z, int numBytes);
  static bool readFlt( Byte** ppByte, size_t& nRemainingBytes, float& z, int numBytes);

  // Portable versions of the above, endian independent if BIG_ENDIAN is defined when needed

  // Writes a floating point value as 1 or 2 byte LSB int or 4 byte LSB float
  // If numBytes is 0, it figures how many bytes to use
  // returns the number of bytes used
  static int writeVal(Byte** ppByte, float z, int numBytes = 0);
  // Reads from an LSB int for 1, 2 bytes, or LSB float for 4
  // Not safe when alliased, cannot be used to read in place
  static void readVal(Byte** ppByte, float& z, int numBytes = 4);

protected:

  InfoFromComputeNumBytes m_infoFromComputeNumBytes;

  std::vector<unsigned int> m_tmpDataVec;    // used in read fcts
};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
