#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
#endif

#ifndef INCLUDED_PCRASTERUTIL
#include "pcrasterutil.h"
#define INCLUDED_PCRASTERUTIL
#endif



//! Converts PCRaster data type to GDAL data type.
/*!
  \param     PCRType PCRaster data type.
  \return    GDAL data type, GDT_Uknown if conversion is not possible.
*/
GDALDataType PCRType2GDALType(CSF_CR PCRType)
{
  GDALDataType dataType = GDT_Unknown;

  switch(PCRType) {
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
    default: {
      assert(false);
      break;
    }
  }

  return dataType;
}



CSF_VS string2PCRValueScale(const std::string& string)
{
  CSF_VS valueScale = VS_UNDEFINED;

  if(string == "BOOLEAN") {
    valueScale = VS_BOOLEAN;
  }
  else if(string == "NOMINAL") {
    valueScale = VS_NOMINAL;
  }
  else if(string == "ORDINAL") {
    valueScale = VS_ORDINAL;
  }
  else if(string == "SCALAR") {
    valueScale = VS_SCALAR;
  }
  else if(string == "DIRECTIONAL") {
    valueScale = VS_DIRECTION;
  }
  else if(string == "LDD") {
    valueScale = VS_LDD;
  }
  else {
    assert(false);
  }

  return valueScale;
}



CSF_VS GDALType2PCRValueScale(GDALDataType type)
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
CSF_CR string2PCRCellRepresentation(const std::string& string)
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
*/
CSF_CR GDALType2PCRCellRepresentation(GDALDataType type, bool exact)
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
      // CSF_CR bla = exact ? CR_INT2: CR_INT4;
      // std::cout << "------> " << CR_INT2 << '\t' << bla << std::endl;
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
    default: {
      assert(false);
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
    case CR_INT4: {
      buffer = new INT4[size];
      break;
    }
    case CR_REAL4: {
      buffer = new REAL4[size];
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
      case CR_INT4: {
        delete static_cast<INT4*>(buffer);
        break;
      }
      case CR_REAL4: {
        delete static_cast<REAL4*>(buffer);
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



CSF_CR updateCellRepresentation(CSF_VS valueScale, CSF_CR cellRepresentation)
{
  CSF_CR result = cellRepresentation;

  if(valueScale == VS_NOMINAL || valueScale == VS_ORDINAL) {
    if(cellRepresentation == CR_UINT1) {
      result = CR_INT4;
    }
  }

/*
  switch(cellRepresentation) {
    case CR_INT2: {
      result = CR_INT4;
      break;
    }
    case CR_UINT1:
    case CR_INT4:
    case CR_REAL4: {
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
*/

  return result;
}



MAP* open(std::string const& filename, MOPEN_PERM mode)
{
  MAP* map = Mopen(filename.c_str(), mode);
  if(map && MgetVersion(map) > 1) {
    // When needed, update in-app cell representation from older / not
    // supported cell representations to one of the currently supported ones.
    // This means that UINT1 is silently upgraded to INT4 for nominal and
    // ordinal data.
    int result = RuseAs(map, updateCellRepresentation(
           RgetValueScale(map), RgetCellRepr(map)));
    assert(result == 0);
  }

  return map;
}
