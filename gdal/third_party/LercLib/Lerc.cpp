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

#include "Defines.h"
#include "Lerc.h"
#include "Lerc2.h"
#include <typeinfo>
#include <limits>

#ifdef HAVE_LERC1_DECODE
#include "Lerc1Decode/CntZImage.h"
#endif

using namespace std;
USING_NAMESPACE_LERC

// -------------------------------------------------------------------------- ;

ErrCode Lerc::ComputeCompressedSize(const void* pData, int version, DataType dt, int nDim, int nCols, int nRows, int nBands,
  const BitMask* pBitMask, double maxZErr, unsigned int& numBytesNeeded)
{
  switch (dt)
  {
  case DT_Char:    return ComputeCompressedSizeTempl((const signed char*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_Byte:    return ComputeCompressedSizeTempl((const Byte*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_Short:   return ComputeCompressedSizeTempl((const short*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_UShort:  return ComputeCompressedSizeTempl((const unsigned short*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_Int:     return ComputeCompressedSizeTempl((const int*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_UInt:    return ComputeCompressedSizeTempl((const unsigned int*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_Float:   return ComputeCompressedSizeTempl((const float*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);
  case DT_Double:  return ComputeCompressedSizeTempl((const double*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, numBytesNeeded);

  default:
    return ErrCode::WrongParam;
  }
}

// -------------------------------------------------------------------------- ;

ErrCode Lerc::Encode(const void* pData, int version, DataType dt, int nDim, int nCols, int nRows, int nBands,
  const BitMask* pBitMask, double maxZErr, Byte* pBuffer, unsigned int numBytesBuffer, unsigned int& numBytesWritten)
{
  switch (dt)
  {
  case DT_Char:    return EncodeTempl((const signed char*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_Byte:    return EncodeTempl((const Byte*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_Short:   return EncodeTempl((const short*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_UShort:  return EncodeTempl((const unsigned short*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_Int:     return EncodeTempl((const int*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_UInt:    return EncodeTempl((const unsigned int*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_Float:   return EncodeTempl((const float*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);
  case DT_Double:  return EncodeTempl((const double*)pData, version, nDim, nCols, nRows, nBands, pBitMask, maxZErr, pBuffer, numBytesBuffer, numBytesWritten);

  default:
    return ErrCode::WrongParam;
  }
}

// -------------------------------------------------------------------------- ;

ErrCode Lerc::GetLercInfo(const Byte* pLercBlob, unsigned int numBytesBlob, struct LercInfo& lercInfo)
{
  lercInfo.RawInit();

  // first try Lerc2
  struct Lerc2::HeaderInfo lerc2Info;
  if (Lerc2::GetHeaderInfo(pLercBlob, numBytesBlob, lerc2Info))
  {
    lercInfo.version = lerc2Info.version;
    lercInfo.nDim = lerc2Info.nDim;
    lercInfo.nCols = lerc2Info.nCols;
    lercInfo.nRows = lerc2Info.nRows;
    lercInfo.numValidPixel = lerc2Info.numValidPixel;
    lercInfo.nBands = 1;
    lercInfo.blobSize = lerc2Info.blobSize;
    lercInfo.dt = (DataType)lerc2Info.dt;
    lercInfo.zMin = lerc2Info.zMin;
    lercInfo.zMax = lerc2Info.zMax;
    lercInfo.maxZError = lerc2Info.maxZError;

    if (lercInfo.blobSize > (int)numBytesBlob)    // truncated blob, we won't be able to read this band
      return ErrCode::BufferTooSmall;

    struct Lerc2::HeaderInfo hdInfo;
    while (Lerc2::GetHeaderInfo(pLercBlob + lercInfo.blobSize, numBytesBlob - lercInfo.blobSize, hdInfo))
    {
      if (hdInfo.nDim != lercInfo.nDim
       || hdInfo.nCols != lercInfo.nCols
       || hdInfo.nRows != lercInfo.nRows
       || hdInfo.numValidPixel != lercInfo.numValidPixel
       || (int)hdInfo.dt != (int)lercInfo.dt)
       //|| hdInfo.maxZError != lercInfo.maxZError)  // with the new bitplane compression, maxZError can vary between bands
      {
        return ErrCode::Failed;
      }

      if (lercInfo.blobSize > std::numeric_limits<int>::max() - hdInfo.blobSize)
        return ErrCode::Failed;

      lercInfo.blobSize += hdInfo.blobSize;

      if (lercInfo.blobSize > (int)numBytesBlob)    // truncated blob, we won't be able to read this band
        return ErrCode::BufferTooSmall;

      lercInfo.nBands++;
      lercInfo.zMin = min(lercInfo.zMin, hdInfo.zMin);
      lercInfo.zMax = max(lercInfo.zMax, hdInfo.zMax);
      lercInfo.maxZError = max(lercInfo.maxZError, hdInfo.maxZError);  // with the new bitplane compression, maxZError can vary between bands
    }

    return ErrCode::Ok;
  }


#ifdef HAVE_LERC1_DECODE
  // only if not Lerc2, try legacy Lerc1
  unsigned int numBytesHeaderBand0 = CntZImage::computeNumBytesNeededToReadHeader(false);
  unsigned int numBytesHeaderBand1 = CntZImage::computeNumBytesNeededToReadHeader(true);
  Byte* pByte = const_cast<Byte*>(pLercBlob);

  lercInfo.zMin =  FLT_MAX;
  lercInfo.zMax = -FLT_MAX;

  CntZImage cntZImg;
  if (numBytesHeaderBand0 <= numBytesBlob && cntZImg.read(&pByte, 1e12, true))    // read just the header
  {
    size_t nBytesRead = pByte - pLercBlob;
    size_t nBytesNeeded = 10 + 4 * sizeof(int) + 1 * sizeof(double);

    if (nBytesRead < nBytesNeeded)
      return ErrCode::Failed;

    Byte* ptr = const_cast<Byte*>(pLercBlob);
    ptr += 10 + 2 * sizeof(int);

    int height(0), width(0);
    memcpy(&height, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&width,  ptr, sizeof(int));  ptr += sizeof(int);
    double maxZErrorInFile(0);
    memcpy(&maxZErrorInFile, ptr, sizeof(double));

    if (height > 20000 || width > 20000)    // guard against bogus numbers; size limitation for old Lerc1
      return ErrCode::Failed;

    lercInfo.nDim = 1;
    lercInfo.nCols = width;
    lercInfo.nRows = height;
    lercInfo.dt = Lerc::DT_Float;
    lercInfo.maxZError = maxZErrorInFile;

    Byte* pByte = const_cast<Byte*>(pLercBlob);
    bool onlyZPart = false;

    while (lercInfo.blobSize + numBytesHeaderBand1 < numBytesBlob)    // means there could be another band
    {
      if (!cntZImg.read(&pByte, 1e12, false, onlyZPart))
        return (lercInfo.nBands > 0) ? ErrCode::Ok : ErrCode::Failed;    // no other band, we are done

      onlyZPart = true;

      lercInfo.nBands++;
      lercInfo.blobSize = (int)(pByte - pLercBlob);

      // now that we have decoded it, we can go the extra mile and collect some extra info
      int numValidPixels = 0;
      float zMin =  FLT_MAX;
      float zMax = -FLT_MAX;

      for (int i = 0; i < height; i++)
      {
        for (int j = 0; j < width; j++)
          if (cntZImg(i, j).cnt > 0)
          {
            numValidPixels++;
            float z = cntZImg(i, j).z;
            zMax = max(zMax, z);
            zMin = min(zMin, z);
          }
      }

      lercInfo.numValidPixel = numValidPixels;
      lercInfo.zMin = std::min(lercInfo.zMin, (double)zMin);
      lercInfo.zMax = std::max(lercInfo.zMax, (double)zMax);
    }

    return ErrCode::Ok;
  }
#endif

  return ErrCode::Failed;
}

// -------------------------------------------------------------------------- ;

ErrCode Lerc::Decode(const Byte* pLercBlob, unsigned int numBytesBlob, BitMask* pBitMask,
  int nDim, int nCols, int nRows, int nBands, DataType dt, void* pData)
{
  switch (dt)
  {
  case DT_Char:    return DecodeTempl((signed char*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_Byte:    return DecodeTempl((Byte*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_Short:   return DecodeTempl((short*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_UShort:  return DecodeTempl((unsigned short*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_Int:     return DecodeTempl((int*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_UInt:    return DecodeTempl((unsigned int*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_Float:   return DecodeTempl((float*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);
  case DT_Double:  return DecodeTempl((double*)pData, pLercBlob, numBytesBlob, nDim, nCols, nRows, nBands, pBitMask);

  default:
    return ErrCode::WrongParam;
  }
}

// -------------------------------------------------------------------------- ;

ErrCode Lerc::ConvertToDouble(const void* pDataIn, DataType dt, size_t nDataValues, double* pDataOut)
{
  switch (dt)
  {
  case DT_Char:    return ConvertToDoubleTempl((const signed char*)pDataIn, nDataValues, pDataOut);
  case DT_Byte:    return ConvertToDoubleTempl((const Byte*)pDataIn, nDataValues, pDataOut);
  case DT_Short:   return ConvertToDoubleTempl((const short*)pDataIn, nDataValues, pDataOut);
  case DT_UShort:  return ConvertToDoubleTempl((const unsigned short*)pDataIn, nDataValues, pDataOut);
  case DT_Int:     return ConvertToDoubleTempl((const int*)pDataIn, nDataValues, pDataOut);
  case DT_UInt:    return ConvertToDoubleTempl((const unsigned int*)pDataIn, nDataValues, pDataOut);
  case DT_Float:   return ConvertToDoubleTempl((const float*)pDataIn, nDataValues, pDataOut);
  //case DT_Double:  no convert double to double

  default:
    return ErrCode::WrongParam;
  }
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

template<class T>
ErrCode Lerc::ComputeCompressedSizeTempl(const T* pData, int version, int nDim, int nCols, int nRows, int nBands,
  const BitMask* pBitMask, double maxZErr, unsigned int& numBytesNeeded)
{
  numBytesNeeded = 0;

  if (!pData || nDim <= 0 || nCols <= 0 || nRows <= 0 || nBands <= 0 || maxZErr < 0)
    return ErrCode::WrongParam;

  if (pBitMask && (pBitMask->GetHeight() != nRows || pBitMask->GetWidth() != nCols))
    return ErrCode::WrongParam;

  Lerc2 lerc2;
  if (version >= 0 && !lerc2.SetEncoderToOldVersion(version))
    return ErrCode::WrongParam;

  bool rv = pBitMask ? lerc2.Set(nDim, nCols, nRows, pBitMask->Bits()) : lerc2.Set(nDim, nCols, nRows);
  if (!rv)
    return ErrCode::Failed;

  // loop over the bands
  for (int iBand = 0; iBand < nBands; iBand++)
  {
    bool encMsk = (iBand == 0);    // store bit mask with first band only
    const T* arr = pData + nDim * nCols * nRows * iBand;

    ErrCode errCode = CheckForNaN(arr, nDim, nCols, nRows, pBitMask);
    if (errCode != ErrCode::Ok)
      return errCode;

    unsigned int nBytes = lerc2.ComputeNumBytesNeededToWrite(arr, maxZErr, encMsk);
    if (nBytes <= 0)
      return ErrCode::Failed;

    numBytesNeeded += nBytes;
  }

  return ErrCode::Ok;
}

// -------------------------------------------------------------------------- ;

template<class T>
ErrCode Lerc::EncodeTempl(const T* pData, int version, int nDim, int nCols, int nRows, int nBands,
  const BitMask* pBitMask, double maxZErr, Byte* pBuffer, unsigned int numBytesBuffer, unsigned int& numBytesWritten)
{
  numBytesWritten = 0;

  if (!pData || nDim <= 0 || nCols <= 0 || nRows <= 0 || nBands <= 0 || maxZErr < 0 || !pBuffer || !numBytesBuffer)
    return ErrCode::WrongParam;

  if (pBitMask && (pBitMask->GetHeight() != nRows || pBitMask->GetWidth() != nCols))
    return ErrCode::WrongParam;

  Lerc2 lerc2;
  if (version >= 0 && !lerc2.SetEncoderToOldVersion(version))
    return ErrCode::WrongParam;

  bool rv = pBitMask ? lerc2.Set(nDim, nCols, nRows, pBitMask->Bits()) : lerc2.Set(nDim, nCols, nRows);
  if (!rv)
    return ErrCode::Failed;

  Byte* pByte = pBuffer;

  // loop over the bands, encode into array of single band Lerc blobs
  for (int iBand = 0; iBand < nBands; iBand++)
  {
    bool encMsk = (iBand == 0);    // store bit mask with first band only
    const T* arr = pData + nDim * nCols * nRows * iBand;

    ErrCode errCode = CheckForNaN(arr, nDim, nCols, nRows, pBitMask);
    if (errCode != ErrCode::Ok)
      return errCode;

    unsigned int nBytes = lerc2.ComputeNumBytesNeededToWrite(arr, maxZErr, encMsk);
    if (nBytes == 0)
      return ErrCode::Failed;

    unsigned int nBytesAlloc = nBytes;

    if ((size_t)(pByte - pBuffer) + nBytesAlloc > numBytesBuffer)    // check we have enough space left
      return ErrCode::BufferTooSmall;

    if (!lerc2.Encode(arr, &pByte))
      return ErrCode::Failed;
  }

  numBytesWritten = (unsigned int)(pByte - pBuffer);
  return ErrCode::Ok;
}

// -------------------------------------------------------------------------- ;

template<class T>
ErrCode Lerc::DecodeTempl(T* pData, const Byte* pLercBlob, unsigned int numBytesBlob,
  int nDim, int nCols, int nRows, int nBands, BitMask* pBitMask)
{
  if (!pData || nDim <= 0 || nCols <= 0 || nRows <= 0 || nBands <= 0 || !pLercBlob || !numBytesBlob)
    return ErrCode::WrongParam;

  if (pBitMask && (pBitMask->GetHeight() != nRows || pBitMask->GetWidth() != nCols))
    return ErrCode::WrongParam;

  const Byte* pByte = pLercBlob;
#ifdef HAVE_LERC1_DECODE
  Byte* pByte1 = const_cast<Byte*>(pLercBlob);
#endif
  Lerc2::HeaderInfo hdInfo;

  if (Lerc2::GetHeaderInfo(pByte, numBytesBlob, hdInfo) && hdInfo.version >= 1)    // is Lerc2
  {
    size_t nBytesRemaining = numBytesBlob;
    Lerc2 lerc2;

    for (int iBand = 0; iBand < nBands; iBand++)
    {
      if (((size_t)(pByte - pLercBlob) < numBytesBlob) && Lerc2::GetHeaderInfo(pByte, nBytesRemaining, hdInfo))
      {
        if (hdInfo.nDim != nDim || hdInfo.nCols != nCols || hdInfo.nRows != nRows)
          return ErrCode::Failed;

        if ((pByte - pLercBlob) + (size_t)hdInfo.blobSize > numBytesBlob)
          return ErrCode::BufferTooSmall;

        T* arr = pData + nDim * nCols * nRows * iBand;

        if (!lerc2.Decode(&pByte, nBytesRemaining, arr, (pBitMask && iBand == 0) ? pBitMask->Bits() : nullptr))
          return ErrCode::Failed;
      }
    }
  }

  else    // might be old Lerc1
  {
#ifdef HAVE_LERC1_DECODE
    unsigned int numBytesHeaderBand0 = CntZImage::computeNumBytesNeededToReadHeader(false);
    unsigned int numBytesHeaderBand1 = CntZImage::computeNumBytesNeededToReadHeader(true);
    CntZImage zImg;

    for (int iBand = 0; iBand < nBands; iBand++)
    {
      unsigned int numBytesHeader = iBand == 0 ? numBytesHeaderBand0 : numBytesHeaderBand1;
      if ((size_t)(pByte - pLercBlob) + numBytesHeader > numBytesBlob)
        return ErrCode::BufferTooSmall;

      bool onlyZPart = iBand > 0;
      if (!zImg.read(&pByte1, 1e12, false, onlyZPart))
        return ErrCode::Failed;

      if (zImg.getWidth() != nCols || zImg.getHeight() != nRows)
        return ErrCode::Failed;

      T* arr = pData + nCols * nRows * iBand;

      if (!Convert(zImg, arr, pBitMask))
        return ErrCode::Failed;
    }
#else
    return ErrCode::Failed;
#endif
  }

  return ErrCode::Ok;
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

#ifdef HAVE_LERC1_DECODE
template<class T>
bool Lerc::Convert(const CntZImage& zImg, T* arr, BitMask* pBitMask)
{
  if (!arr || !zImg.getSize())
    return false;

  const bool fltPnt = (typeid(*arr) == typeid(double)) || (typeid(*arr) == typeid(float));

  int h = zImg.getHeight();
  int w = zImg.getWidth();

  if (pBitMask && (pBitMask->GetHeight() != h || pBitMask->GetWidth() != w))
    return false;

  if (pBitMask)
    pBitMask->SetAllValid();

  const CntZ* srcPtr = zImg.getData();
  T* dstPtr = arr;
  int num = w * h;
  for (int k = 0; k < num; k++)
  {
    if (srcPtr->cnt > 0)
      *dstPtr = fltPnt ? (T)srcPtr->z : (T)floor(srcPtr->z + 0.5);
    else if (pBitMask)
      pBitMask->SetInvalid(k);

    srcPtr++;
    dstPtr++;
  }

  return true;
}
#endif

// -------------------------------------------------------------------------- ;

template<class T>
ErrCode Lerc::ConvertToDoubleTempl(const T* pDataIn, size_t nDataValues, double* pDataOut)
{
  if (!pDataIn || !nDataValues || !pDataOut)
    return ErrCode::WrongParam;

  for (size_t k = 0; k < nDataValues; k++)
    pDataOut[k] = pDataIn[k];

  return ErrCode::Ok;
}

// -------------------------------------------------------------------------- ;

template<class T> ErrCode Lerc::CheckForNaN(const T* arr, int nDim, int nCols, int nRows, const BitMask* pBitMask)
{
  if (!arr || nDim <= 0 || nCols <= 0 || nRows <= 0)
    return ErrCode::WrongParam;

#ifdef CHECK_FOR_NAN

  if (typeid(T) == typeid(double) || typeid(T) == typeid(float))
  {
    bool foundNaN = false;

    for (int k = 0, i = 0; i < nRows; i++)
    {
      const T* rowArr = &(arr[i * nCols * nDim]);

      if (!pBitMask)    // all valid
      {
        for (int n = 0, j = 0; j < nCols; j++, n += nDim)
          for (int m = 0; m < nDim; m++)
            if (std::isnan((double)rowArr[n + m]))
              foundNaN = true;
      }
      else    // not all valid
      {
        for (int n = 0, j = 0; j < nCols; j++, k++, n += nDim)
          if (pBitMask->IsValid(k))
          {
            for (int m = 0; m < nDim; m++)
              if (std::isnan((double)rowArr[n + m]))
                foundNaN = true;
          }
      }

      if (foundNaN)
        return ErrCode::NaN;
    }
  }

#endif

  return ErrCode::Ok;
}

// -------------------------------------------------------------------------- ;

