#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
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
  \param     type PCRaster data type.
  \return    GDAL data type, GDT_Uknown if conversion is not possible.
*/
GDALDataType PCRasterType2GDALType(CSF_CR type)
{
  GDALDataType dataType = GDT_Unknown;

  switch(type) {
    case CR_UINT1: {
      dataType = GDT_Byte;
      break;
    }
    case CR_INT4: {
      dataType = GDT_Int32;
      break;
    }
    case CR_REAL4: {
      dataType = GDT_Float32;
      break;
    }
    case CR_REAL8: {
      dataType = GDT_Float64;
      break;
    }
    default: {
      break;
    }
  }

  return dataType;
}



CSF_VS string2PCRasterValueScale(const std::string& string)
{
  CSF_VS valueScale = VS_UNDEFINED;

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

  return valueScale;
}



std::string PCRasterValueScale2String(CSF_VS valueScale)
{
  std::string result = "VS_UNDEFINED";

  switch(valueScale) {
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
  \exception .
  \warning   .
  \sa        .
  \todo      Convert assertion to exception.

  GDAL byte is regarded as PCRaster boolean, integral as nominal and float
  as scalar. This function will never return VS_LDD, VS_ORDINAL or
  VS_DIRECTION.
*/
CSF_VS GDALType2PCRasterValueScale(GDALDataType type)
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
      assert(false);
      break;
    }
  }

  return valueScale;
}



//!
/*!
  \param     .
  \return    .
  \exception .
  \warning   The string must contain a valid version 2.0 cell representation.
  \sa        .
*/
/*
CSF_CR string2PCRasterCellRepresentation(const std::string& string)
{
  CSF_CR cellRepresentation = CR_UNDEFINED;

  if(string == "UINT1") {
    cellRepresentation = CR_UINT1;
  }
  else if(string == "INT4") {
    cellRepresentation = CR_INT4;
  }
  else if(string == "REAL4") {
    cellRepresentation = CR_REAL4;
  }
  else {
    assert(false);
  }

  return cellRepresentation;
}
*/



//!
/*!
  \param     .
  \return    .
  \exception .
  \warning   .
  \sa        .

  If exact is false, conversion to CSF2.0 types will take place. This is
  usefull for in file cell representations. If exact is true, and exact match
  is made. This is usefull for in app cell representations.

  If exact is false, this function alwasy returns one of CR_UINT1, CR_INT4
  or CR_REAL4.
*/
CSF_CR GDALType2PCRasterCellRepresentation(GDALDataType type, bool exact)
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



//!
/*!
  \param     .
  \return    Buffer or 0 if buffer cannot be created.
  \exception .
  \warning   Use deleteBuffer(void*) to delete the buffer again.
  \sa        .
*/
void* createBuffer(size_t size, CSF_CR type)
{
  void* buffer = 0;

  switch(type) {
    case CR_UINT1: {
      buffer = new UINT1[size];
      break;
    }
    case CR_UINT2: {
      buffer = new UINT2[size];
      break;
    }
    case CR_UINT4: {
      buffer = new UINT4[size];
      break;
    }
    case CR_INT2: {
      buffer = new INT2[size];
      break;
    }
    case CR_INT4: {
      buffer = new INT4[size];
      break;
    }
    case CR_REAL4: {
      buffer = new REAL4[size];
      break;
    }
    case CR_REAL8: {
      buffer = new REAL8[size];
      break;
    }
    default: {
      assert(false);
      break;
    }
  }

  return buffer;
}



void deleteBuffer(void* buffer, CSF_CR type)
{
  if(buffer) {

    switch(type) {
      case CR_UINT1: {
        delete static_cast<UINT1*>(buffer);
        break;
      }
      case CR_UINT2: {
        delete static_cast<UINT2*>(buffer);
        break;
      }
      case CR_UINT4: {
        delete static_cast<UINT4*>(buffer);
        break;
      }
      case CR_INT2: {
        delete static_cast<INT2*>(buffer);
        break;
      }
      case CR_INT4: {
        delete static_cast<INT4*>(buffer);
        break;
      }
      case CR_REAL4: {
        delete static_cast<REAL4*>(buffer);
        break;
      }
      case CR_REAL8: {
        delete static_cast<REAL8*>(buffer);
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
  }
}



bool isContinuous(CSF_VS valueScale)
{
  return valueScale == VS_SCALAR || valueScale == VS_DIRECTION;
}



//! Determines a missing value to use for data of \a type.
/*!
  \param     type Cell representation of the data.
  \return    Missing value.
  \exception .
  \warning   \a type must be CR_UINT1, CR_INT4 or CR_REAL4.
  \sa        .
*/
double missingValue(CSF_CR type)
{
  double missingValue = 0.0;

  switch(type) {
    case CR_UINT1: {
      missingValue = static_cast<double>(MV_UINT1);
      break;
    }
    case CR_INT4: {
      missingValue = static_cast<double>(MV_INT4);
      break;
    }
    case CR_REAL4: {
      missingValue = 1e-30;
      break;
    }
    default: {
      assert(false);
      break;
    }
  }

  return missingValue;
}



//! Updates \a cellRepresentation to a currently supported value.
/*!
  \param     valueScale In file value scale of the data.
  \param     cellRepresentation Cell representation of the data.
  \return    Cell representation.
  \exception .
  \warning   .
  \sa        .

  Some (older) applications write PCRaster rasters using a cell representation
  which we currently don't want to write anymore. This function can be called
  to convert these cell representations to a value we currently use.
*/
CSF_CR updateCellRepresentation(CSF_VS valueScale, CSF_CR type)
{
  CSF_CR result = type;

  /*
  if(valueScale == VS_NOMINAL || valueScale == VS_ORDINAL) {
    if(type == CR_UINT1) {
      result = CR_INT4;
    }
  }
  */
  /*
  else if(valueScale == VS_SCALAR) {
    if(type == CR_REAL8) {
      result = CR_REAL4;
    }
  }
  */

  return result;
}



//! Opens the raster in \a filename using mode \a mode.
/*!
  \param     filename Filename of raster to open.
  \return    Pointer to CSF MAP structure.
  \exception .
  \warning   .
  \sa        .
*/
MAP* open(std::string const& filename, MOPEN_PERM mode)
{
  MAP* map = Mopen(filename.c_str(), mode);
  if(map && MgetVersion(map) > 1) {
    // When needed, update in-app cell representation from older / not
    // supported cell representations to one of the currently supported ones.
    // This means that UINT1 is silently updated to INT4 for nominal and
    // ordinal data.
    int result = RuseAs(map, updateCellRepresentation(
           RgetValueScale(map), RgetCellRepr(map)));
    assert(result == 0);
  }

  return map;
}



void alterFromStdMV(void* buffer, size_t size, CSF_CR cellRepresentation,
         double missingValue)
{
  switch(cellRepresentation) {
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
    default: {
      assert(false);
      break;
    }
  }
}



void alterToStdMV(void* buffer, size_t size, CSF_CR cellRepresentation,
         double missingValue)
{
  switch(cellRepresentation) {
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
    default: {
      assert(false);
      break;
    }
  }
}



CSF_VS fitValueScale(CSF_VS valueScale, CSF_CR cellRepresentation)
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
