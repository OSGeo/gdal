
#pragma once

// You can include this file (if you work in C++) but you don't have to. 
// If you call this api from another language (Python, C#), you see integers. 
// This header file tells you what these integers mean. 
// These enum's may grow in the future. More values can be added. 

#include "Defines.h"

NAMESPACE_LERC_START
  enum class ErrCode : int
  {
    Ok = 0,
    Failed,
    WrongParam,
    BufferTooSmall,
    NaN
  };

  enum class DataType : int
  {
    dt_char = 0,
    dt_uchar,
    dt_short,
    dt_ushort,
    dt_int,
    dt_uint,
    dt_float,
    dt_double
  };

  enum class InfoArrOrder : int
  {
    version = 0,
    dataType,
    nDim,
    nCols,
    nRows,
    nBands,
    nValidPixels,
    blobSize
  };

  enum class DataRangeArrOrder : int
  {
    zMin = 0,
    zMax,
    maxZErrUsed
  };

NAMESPACE_LERC_END
