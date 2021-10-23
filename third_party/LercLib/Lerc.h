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

#pragma once

#include <cstring>
#include <vector>
#include "Lerc_types.h"
#include "BitMask.h"
#include "Lerc2.h"

NAMESPACE_LERC_START

#ifdef HAVE_LERC1_DECODE
  class CntZImage;
#endif

  class Lerc
  {
  public:
    Lerc() {}
    ~Lerc() {}

    // data types supported by Lerc
    enum DataType { DT_Char, DT_Byte, DT_Short, DT_UShort, DT_Int, DT_UInt, DT_Float, DT_Double, DT_Undefined };

    // all functions are provided in 2 flavors
    // - using void pointers to the image data, can be called on a Lerc lib or dll
    // - data templated, can be called if compiled together


    // Encode

    // if more than 1 band, the outgoing Lerc blob has the single band Lerc blobs concatenated; 
    // or, if you have multiple values per pixel and stored as [RGB, RGB, ... ], then set nDim accordingly (e.g., 3)

    // computes the number of bytes needed to allocate the buffer, accurate to the byte;
    // does not encode the image data, but uses statistics and formulas to compute the buffer size needed;
    // this function is optional, you can also use a buffer large enough to call Encode() directly, 
    // or, if encoding a batch of same width / height tiles, call this function once, double the buffer size, and
    // then just call Encode() on all tiles;

    static ErrCode ComputeCompressedSize(
      const void* pData,               // raw image data, row by row, band by band
      int version,                     // 2 = v2.2, 3 = v2.3, 4 = v2.4
      DataType dt,                     // data type, char to double
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      const BitMask* pBitMask,         // 0 if all pixels are valid
      double maxZErr,                  // max coding error per pixel, defines the precision
      unsigned int& numBytesNeeded);   // size of outgoing Lerc blob

    // encodes or compresses the image data into the buffer

    static ErrCode Encode(
      const void* pData,               // raw image data, row by row, band by band
      int version,                     // 2 = v2.2, 3 = v2.3, 4 = v2.4
      DataType dt,                     // data type, char to double
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      const BitMask* pBitMask,         // 0 if all pixels are valid
      double maxZErr,                  // max coding error per pixel, defines the precision
      Byte* pBuffer,                   // buffer to write to, function fails if buffer too small
      unsigned int numBytesBuffer,     // buffer size
      unsigned int& numBytesWritten);  // num bytes written to buffer


    // Decode

    struct LercInfo
    {
      int version,        // Lerc version number (0 for old Lerc1, 1 to 4 for Lerc 2.1 to 2.4)
        nDim,             // number of values per pixel
        nCols,            // number of columns
        nRows,            // number of rows
        numValidPixel,    // number of valid pixels
        nBands,           // number of bands
        blobSize;         // total blob size in bytes
      DataType dt;        // data type (float only for old Lerc1)
      double zMin,        // min pixel value, over all data values
        zMax,             // max pixel value, over all data values
        maxZError;        // maxZError used for encoding

      void RawInit()  { memset(this, 0, sizeof(struct LercInfo)); }
    };

    // again, this function is optional;
    // call it on a Lerc blob to get the above struct returned, from this the data arrays
    // can be constructed before calling Decode();
    // same as above, for a batch of Lerc blobs of the same kind, you can call this function on 
    // the first blob, get the info, and on the other Lerc blobs just call Decode();
    // this function is very fast on (newer) Lerc2 blobs as it only reads the blob headers;

    // Remark on param numBytesBlob. Usually it is known, either the file size of the blob written to disk, 
    // or the size of the blob transmitted. It should be accurate for 2 reasons:
    // _ function finds out how many single band Lerc blobs are concatenated
    // _ function checks for truncated file or blob
    // It is OK to pass numBytesBlob too large as long as there is no other (valid) Lerc blob following next.
    // If in doubt, check the code in Lerc::GetLercInfo(...) for the exact logic. 

    static ErrCode GetLercInfo(const Byte* pLercBlob,       // Lerc blob to decode
                               unsigned int numBytesBlob,   // size of Lerc blob in bytes
                               struct LercInfo& lercInfo);

    // setup outgoing arrays accordingly, then call Decode()

    static ErrCode Decode(
      const Byte* pLercBlob,           // Lerc blob to decode
      unsigned int numBytesBlob,       // size of Lerc blob in bytes
      BitMask* pBitMask,               // gets filled if not 0, even if all valid
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      DataType dt,                     // data type of outgoing array
      void* pData);                    // outgoing data bands


    static ErrCode ConvertToDouble(
      const void* pDataIn,             // pixel data of image tile of data type dt (< double)
      DataType dt,                     // data type of input data
      size_t nDataValues,              // total number of data values (nDim * nCols * nRows * nBands)
      double* pDataOut);               // pixel data converted to double


    // same as functions above, but data templated instead of using void pointers

    template<class T> static ErrCode ComputeCompressedSizeTempl(
      const T* pData,                  // raw image data, row by row, band by 
      int version,                     // 2 = v2.2, 3 = v2.3, 4 = v2.4
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      const BitMask* pBitMask,         // 0 means all pixels are valid
      double maxZErr,                  // max coding error per pixel, defines the precision
      unsigned int& numBytes);         // size of outgoing Lerc blob

    template<class T> static ErrCode EncodeTempl(
      const T* pData,                  // raw image data, row by row, band by band
      int version,                     // 2 = v2.2, 3 = v2.3, 4 = v2.4
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      const BitMask* pBitMask,         // 0 means all pixels are valid
      double maxZErr,                  // max coding error per pixel, defines the precision
      Byte* pBuffer,                   // buffer to write to, function will fail if buffer too small
      unsigned int numBytesBuffer,     // buffer size
      unsigned int& numBytesWritten);  // num bytes written to buffer

    template<class T> static ErrCode DecodeTempl(
      T* pData,                        // outgoing data bands
      const Byte* pLercBlob,           // Lerc blob to decode
      unsigned int numBytesBlob,       // size of Lerc blob in bytes
      int nDim,                        // number of values per pixel
      int nCols,                       // number of cols
      int nRows,                       // number of rows
      int nBands,                      // number of bands
      BitMask* pBitMask);              // gets filled if not 0, even if all valid

  private:
#ifdef HAVE_LERC1_DECODE
    template<class T> static bool Convert(const CntZImage& zImg, T* arr, BitMask* pBitMask);
#endif
    template<class T> static ErrCode ConvertToDoubleTempl(const T* pDataIn, size_t nDataValues, double* pDataOut);

    template<class T> static ErrCode CheckForNaN(const T* arr, int nDim, int nCols, int nRows, const BitMask* pBitMask);
  };
NAMESPACE_LERC_END
