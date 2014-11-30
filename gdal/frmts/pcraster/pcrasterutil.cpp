/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support functions.
 * Author:   Kor de Jong, k.dejong at geog.uu.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, Kor de Jong
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_ALGORITHM
#include <algorithm>
#define INCLUDED_ALGORITHM
#endif

#ifndef INCLUDED_FLOAT
#include <float.h>
#define INCLUDED_FLOAT
#endif

#ifndef INCLUDED_PCRTYPES
#include "pcrtypes.h"
#define INCLUDED_PCRTYPES
#endif

#ifndef INCLUDED_PCRASTERUTIL
#include "pcrasterutil.h"
#define INCLUDED_PCRASTERUTIL
#endif



//! Converts PCRaster data type to GDAL data type.
/*!
  \param     cellRepresentation Cell representation.
  \return    GDAL data type, GDT_Uknown if conversion is not possible.
*/
GDALDataType cellRepresentation2GDALType(
         CSF_CR cellRepresentation)
{
  GDALDataType type = GDT_Unknown;

  switch(cellRepresentation) {
    // CSF version 2. ----------------------------------------------------------
    case CR_UINT1: {
      type = GDT_Byte;
      break;
    }
    case CR_INT4: {
      type = GDT_Int32;
      break;
    }
    case CR_REAL4: {
      type = GDT_Float32;
      break;
    }
    case CR_REAL8: {
      type = GDT_Float64;
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      type = GDT_Byte;
      break;
    }
    case CR_INT2: {
      type = GDT_Int16;
      break;
    }
    case CR_UINT2: {
      type = GDT_UInt16;
      break;
    }
    case CR_UINT4: {
      type = GDT_UInt32;
      break;
    }
    default: {
      break;
    }
  }

  return type;
}



CSF_VS string2ValueScale(
         std::string const& string)
{
  CSF_VS valueScale = VS_UNDEFINED;

  // CSF version 2. ------------------------------------------------------------
  if(string == "VS_BOOLEAN") {
    valueScale = VS_BOOLEAN;
  }
  else if(string == "VS_NOMINAL") {
    valueScale = VS_NOMINAL;
  }
  else if(string == "VS_ORDINAL") {
    valueScale = VS_ORDINAL;
  }
  else if(string == "VS_SCALAR") {
    valueScale = VS_SCALAR;
  }
  else if(string == "VS_DIRECTION") {
    valueScale = VS_DIRECTION;
  }
  else if(string == "VS_LDD") {
    valueScale = VS_LDD;
  }
  // CSF version1. -------------------------------------------------------------
  else if(string == "VS_CLASSIFIED") {
    valueScale = VS_CLASSIFIED;
  }
  else if(string == "VS_CONTINUOUS") {
    valueScale = VS_CONTINUOUS;
  }
  else if(string == "VS_NOTDETERMINED") {
    valueScale = VS_NOTDETERMINED;
  }

  return valueScale;
}



std::string valueScale2String(
         CSF_VS valueScale)
{
  std::string result = "VS_UNDEFINED";

  switch(valueScale) {
    // CSF version 2. ----------------------------------------------------------
    case VS_BOOLEAN: {
      result = "VS_BOOLEAN";
      break;
    }
    case VS_NOMINAL: {
      result = "VS_NOMINAL";
      break;
    }
    case VS_ORDINAL: {
      result = "VS_ORDINAL";
      break;
    }
    case VS_SCALAR: {
      result = "VS_SCALAR";
      break;
    }
    case VS_DIRECTION: {
      result = "VS_DIRECTION";
      break;
    }
    case VS_LDD: {
      result = "VS_LDD";
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case VS_CLASSIFIED: {
      result = "VS_CLASSIFIED";
      break;
    }
    case VS_CONTINUOUS: {
      result = "VS_CONTINUOUS";
      break;
    }
    case VS_NOTDETERMINED: {
      result = "VS_NOTDETERMINED";
      break;
    }
    default: {
      break;
    }
  }

  return result;
}



std::string cellRepresentation2String(
         CSF_CR cellRepresentation)
{
  std::string result = "CR_UNDEFINED";

  switch(cellRepresentation) {

    // CSF version 2. ----------------------------------------------------------
    case CR_UINT1: {
      result = "CR_UINT1";
      break;
    }
    case CR_INT4: {
      result = "CR_INT4";
      break;
    }
    case CR_REAL4: {
      result = "CR_REAL4";
      break;
    }
    case CR_REAL8: {
      result = "CR_REAL8";
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      result = "CR_INT1";
      break;
    }
    case CR_INT2: {
      result = "CR_INT2";
      break;
    }
    case CR_UINT2: {
      result = "CR_UINT2";
      break;
    }
    case CR_UINT4: {
      result = "CR_UINT4";
      break;
    }
    default: {
      break;
    }
  }

  return result;
}



//! Converts GDAL data type to PCRaster value scale.
/*!
  \param     type GDAL data type.
  \return    Value scale.
  \warning   \a type must be one of the standard numerical types and not
             complex.

  GDAL byte is regarded as PCRaster boolean, integral as nominal and float
  as scalar. This function will never return VS_LDD, VS_ORDINAL or
  VS_DIRECTION.
*/
CSF_VS GDALType2ValueScale(
         GDALDataType type)
{
  CSF_VS valueScale = VS_UNDEFINED;

  switch(type) {
    case GDT_Byte: {
      // A foreign dataset is unlikely to support our LDD's.
      valueScale = VS_BOOLEAN;
      break;
    }
    case GDT_UInt16:
    case GDT_UInt32:
    case GDT_Int16:
    case GDT_Int32: {
      valueScale = VS_NOMINAL;
      break;
    }
    case GDT_Float32: {
      // A foreign dataset is unlikely to support our directional.
      valueScale = VS_SCALAR;
      break;
    }
    case GDT_Float64: {
      // A foreign dataset is unlikely to support our directional.
      valueScale = VS_SCALAR;
      break;
    }
    default: {
      CPLAssert(false);
      break;
    }
  }

  return valueScale;
}



//! Converts a GDAL type to a PCRaster cell representation.
/*!
  \param     type GDAL type.
  \param     exact Whether an exact match or a CSF2.0 supported cell
                   representation should be returned.
  \return    Cell representation.
  \warning   \a type must be one of the standard numerical types and not
             complex.

  If exact is false, conversion to CSF2.0 types will take place. This is
  useful for in file cell representations. If exact is true, and exact match
  is made. This is useful for in app cell representations.

  If exact is false, this function always returns one of CR_UINT1, CR_INT4
  or CR_REAL4.
*/
CSF_CR GDALType2CellRepresentation(
         GDALDataType type,
         bool exact)
{
  CSF_CR cellRepresentation = CR_UNDEFINED;

  switch(type) {
    case GDT_Byte: {
      cellRepresentation = CR_UINT1;
      break;
    }
    case GDT_UInt16: {
      cellRepresentation = exact ? CR_UINT2: CR_UINT1;
      break;
    }
    case GDT_UInt32: {
      cellRepresentation = exact ? CR_UINT4: CR_UINT1;
      break;
    }
    case GDT_Int16: {
      cellRepresentation = exact ? CR_INT2: CR_INT4;
      break;
    }
    case GDT_Int32: {
      cellRepresentation = CR_INT4;
      break;
    }
    case GDT_Float32: {
      cellRepresentation = CR_REAL4;
      break;
    }
    case GDT_Float64: {
      cellRepresentation = exact ? CR_REAL8: CR_REAL4;
      break;
    }
    default: {
      break;
    }
  }

  return cellRepresentation;
}



//! Determines a missing value to use for data of \a cellRepresentation.
/*!
  \param     cellRepresentation Cell representation of the data.
  \return    Missing value.
  \exception .
  \sa        .
*/
double missingValue(
         CSF_CR cellRepresentation)
{
  // It turns out that the missing values set here should be equal to the ones
  // used in gdal's code to do data type conversion. Otherwise missing values
  // in the source raster will be lost in the destination raster. It seems that
  // when assigning new missing values gdal uses its own nodata values instead
  // of the value set in the dataset.

  double missingValue = 0.0;

  switch(cellRepresentation) {
    // CSF version 2. ----------------------------------------------------------
    case CR_UINT1: {
      // missingValue = static_cast<double>(MV_UINT1);
      missingValue = UINT1(255);
      break;
    }
    case CR_INT4: {
      // missingValue = static_cast<double>(MV_INT4);
      missingValue = INT4(-2147483647);
      break;
    }
    case CR_REAL4: {
      // using <limits> breaks on gcc 2.95
      // CPLAssert(std::numeric_limits<REAL4>::is_iec559);
      // missingValue = -std::numeric_limits<REAL4>::max();
      missingValue = -FLT_MAX;
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      missingValue = static_cast<double>(MV_INT1);
      break;
    }
    case CR_INT2: {
      missingValue = static_cast<double>(MV_INT2);
      break;
    }
    case CR_UINT2: {
      missingValue = static_cast<double>(MV_UINT2);
      break;
    }
    case CR_UINT4: {
      missingValue = static_cast<double>(MV_UINT4);
      break;
    }
    default: {
      CPLAssert(false);
      break;
    }
  }

  return missingValue;
}



//! Opens the raster in \a filename using mode \a mode.
/*!
  \param     filename Filename of raster to open.
  \return    Pointer to CSF MAP structure.
  \exception .
  \warning   .
  \sa        .
*/
MAP* mapOpen(
         std::string const& filename,
         MOPEN_PERM mode)
{
  MAP* map = Mopen(filename.c_str(), mode);

  return map;
}



void alterFromStdMV(
         void* buffer,
         size_t size,
         CSF_CR cellRepresentation,
         double missingValue)
{
  switch(cellRepresentation) {
    // CSF version 2. ----------------------------------------------------------
    case(CR_UINT1): {
      std::for_each(static_cast<UINT1*>(buffer),
         static_cast<UINT1*>(buffer) + size,
         pcr::AlterFromStdMV<UINT1>(static_cast<UINT1>(missingValue)));
      break;
    }
    case(CR_INT4): {
      std::for_each(static_cast<INT4*>(buffer),
         static_cast<INT4*>(buffer) + size,
         pcr::AlterFromStdMV<INT4>(static_cast<INT4>(missingValue)));
      break;
    }
    case(CR_REAL4): {
      std::for_each(static_cast<REAL4*>(buffer),
         static_cast<REAL4*>(buffer) + size,
         pcr::AlterFromStdMV<REAL4>(static_cast<REAL4>(missingValue)));
      break;
    }
    case(CR_REAL8): {
      std::for_each(static_cast<REAL8*>(buffer),
         static_cast<REAL8*>(buffer) + size,
         pcr::AlterFromStdMV<REAL8>(static_cast<REAL8>(missingValue)));
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      std::for_each(static_cast<INT1*>(buffer),
         static_cast<INT1*>(buffer) + size,
         pcr::AlterFromStdMV<INT1>(static_cast<INT1>(missingValue)));
      break;
    }
    case CR_INT2: {
      std::for_each(static_cast<INT2*>(buffer),
         static_cast<INT2*>(buffer) + size,
         pcr::AlterFromStdMV<INT2>(static_cast<INT2>(missingValue)));
      break;
    }
    case CR_UINT2: {
      std::for_each(static_cast<UINT2*>(buffer),
         static_cast<UINT2*>(buffer) + size,
         pcr::AlterFromStdMV<UINT2>(static_cast<UINT2>(missingValue)));
      break;
    }
    case CR_UINT4: {
      std::for_each(static_cast<UINT4*>(buffer),
         static_cast<UINT4*>(buffer) + size,
         pcr::AlterFromStdMV<UINT4>(static_cast<UINT4>(missingValue)));
      break;
    }
    default: {
      CPLAssert(false);
      break;
    }
  }
}



void alterToStdMV(
         void* buffer,
         size_t size,
         CSF_CR cellRepresentation,
         double missingValue)
{
  switch(cellRepresentation) {
    // CSF version 2. ----------------------------------------------------------
    case(CR_UINT1): {
      std::for_each(static_cast<UINT1*>(buffer),
         static_cast<UINT1*>(buffer) + size,
         pcr::AlterToStdMV<UINT1>(static_cast<UINT1>(missingValue)));
      break;
    }
    case(CR_INT4): {
      std::for_each(static_cast<INT4*>(buffer),
         static_cast<INT4*>(buffer) + size,
         pcr::AlterToStdMV<INT4>(static_cast<INT4>(missingValue)));
      break;
    }
    case(CR_REAL4): {
      std::for_each(static_cast<REAL4*>(buffer),
         static_cast<REAL4*>(buffer) + size,
         pcr::AlterToStdMV<REAL4>(static_cast<REAL4>(missingValue)));
      break;
    }
    case(CR_REAL8): {
      std::for_each(static_cast<REAL8*>(buffer),
         static_cast<REAL8*>(buffer) + size,
         pcr::AlterToStdMV<REAL8>(static_cast<REAL8>(missingValue)));
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      std::for_each(static_cast<INT1*>(buffer),
         static_cast<INT1*>(buffer) + size,
         pcr::AlterToStdMV<INT1>(static_cast<INT1>(missingValue)));
      break;
    }
    case CR_INT2: {
      std::for_each(static_cast<INT2*>(buffer),
         static_cast<INT2*>(buffer) + size,
         pcr::AlterToStdMV<INT2>(static_cast<INT2>(missingValue)));
      break;
    }
    case CR_UINT2: {
      std::for_each(static_cast<UINT2*>(buffer),
         static_cast<UINT2*>(buffer) + size,
         pcr::AlterToStdMV<UINT2>(static_cast<UINT2>(missingValue)));
      break;
    }
    case CR_UINT4: {
      std::for_each(static_cast<UINT4*>(buffer),
         static_cast<UINT4*>(buffer) + size,
         pcr::AlterToStdMV<UINT4>(static_cast<UINT4>(missingValue)));
      break;
    }
    default: {
      CPLAssert(false);
      break;
    }
  }
}



CSF_VS fitValueScale(
         CSF_VS valueScale,
         CSF_CR cellRepresentation)
{
  CSF_VS result = valueScale;

  switch(cellRepresentation) {
    case CR_UINT1: {
      switch(valueScale) {
        case VS_LDD: {
          result = VS_LDD;
          break;
        }
        default: {
          result = VS_BOOLEAN;
          break;
        }
      }
      break;
    }
    case CR_INT4: {
      switch(valueScale) {
        case VS_BOOLEAN: {
          result = VS_NOMINAL;
          break;
        }
        case VS_SCALAR: {
          result = VS_ORDINAL;
          break;
        }
        case VS_DIRECTION: {
          result = VS_ORDINAL;
          break;
        }
        case VS_LDD: {
          result = VS_NOMINAL;
          break;
        }
        default: {
          result = valueScale;
          break;
        }
      }
      break;
    }
    case CR_REAL4: {
      switch(valueScale) {
        case VS_DIRECTION: {
          result = VS_DIRECTION;
          break;
        }
        default: {
          result = VS_SCALAR;
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  return result;
}



void castValuesToBooleanRange(
         void* buffer,
         size_t size,
         CSF_CR cellRepresentation)
{
  switch(cellRepresentation) {
    // CSF version 2. ----------------------------------------------------------
    case(CR_UINT1): {
      std::for_each(static_cast<UINT1*>(buffer),
         static_cast<UINT1*>(buffer) + size,
         CastToBooleanRange<UINT1>());
      break;
    }
    case(CR_INT4): {
      std::for_each(static_cast<INT4*>(buffer),
         static_cast<INT4*>(buffer) + size,
         CastToBooleanRange<INT4>());
      break;
    }
    case(CR_REAL4): {
      std::for_each(static_cast<REAL4*>(buffer),
         static_cast<REAL4*>(buffer) + size,
         CastToBooleanRange<REAL4>());
      break;
    }
    case(CR_REAL8): {
      std::for_each(static_cast<REAL8*>(buffer),
         static_cast<REAL8*>(buffer) + size,
         CastToBooleanRange<REAL8>());
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      std::for_each(static_cast<INT1*>(buffer),
         static_cast<INT1*>(buffer) + size,
         CastToBooleanRange<INT1>());
      break;
    }
    case CR_INT2: {
      std::for_each(static_cast<INT2*>(buffer),
         static_cast<INT2*>(buffer) + size,
         CastToBooleanRange<INT2>());
      break;
    }
    case CR_UINT2: {
      std::for_each(static_cast<UINT2*>(buffer),
         static_cast<UINT2*>(buffer) + size,
         CastToBooleanRange<UINT2>());
      break;
    }
    case CR_UINT4: {
      std::for_each(static_cast<UINT4*>(buffer),
         static_cast<UINT4*>(buffer) + size,
         CastToBooleanRange<UINT4>());
      break;
    }
    default: {
      CPLAssert(false);
      break;
    }
  }
}

