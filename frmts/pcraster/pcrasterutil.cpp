#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
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
GDALDataType cellRepresentation2GDALType(CSF_CR cellRepresentation)
{
  GDALDataType type = GDT_Unknown;

  switch(cellRepresentation) {
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
    default: {
      break;
    }
  }

  return type;
}



CSF_VS string2ValueScale(const std::string& string)
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



std::string valueScale2String(CSF_VS valueScale)
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



std::string cellRepresentation2String(CSF_CR cellRepresentation)
{
  std::string result = "CR_UNDEFINED";

  switch(cellRepresentation) {
    case CR_UINT1: {
      result = "CR_UINT1";
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
    case CR_INT1: {
     result = "CR_INT1";
     break;
   }
    case CR_INT2: {
     result = "CR_INT2";
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
CSF_VS GDALType2ValueScale(GDALDataType type)
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



//! Converts a GDAL type to a PCRaster cell representation.
/*!
  \param     type GDAL type.
  \param     exact Whether an exact match or a CSF2.0 supported cell
                   representation should be returned.
  \return    Cell representation.
  \warning   \a type must be one of the standard numerical types and not
             complex.

  If exact is false, conversion to CSF2.0 types will take place. This is
  usefull for in file cell representations. If exact is true, and exact match
  is made. This is usefull for in app cell representations.

  If exact is false, this function always returns one of CR_UINT1, CR_INT4
  or CR_REAL4.
*/
CSF_CR GDALType2CellRepresentation(GDALDataType type, bool exact)
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
  \warning   \a cellRepresentation must be CR_UINT1, CR_INT4 or CR_REAL4.
  \sa        .
*/
double missingValue(CSF_CR cellRepresentation)
{
  double missingValue = 0.0;

  switch(cellRepresentation) {
    case CR_UINT1: {
      missingValue = static_cast<double>(MV_UINT1);
      break;
    }
    case CR_INT4: {
      missingValue = static_cast<double>(MV_INT4);
      break;
    }
    case CR_REAL4: {
      // using <limits> breaks on gcc 2.95
      // assert(std::numeric_limits<REAL4>::is_iec559);
      // missingValue = -std::numeric_limits<REAL4>::max();
      missingValue = -FLT_MAX;
      break;
    }
    default: {
      assert(false);
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
MAP* open(std::string const& filename, MOPEN_PERM mode)
{
  MAP* map = Mopen(filename.c_str(), mode);

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
