/******************************************************************************
 * $Id$
 *
 * Name:     gdal.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.11  2005/02/16 16:55:49  kruland
 * Added typedef for CPLErr to prevent wrapping of the enum.
 * Moved the AllRegister method definition.
 *
 * Revision 1.10  2005/02/15 22:31:52  kruland
 * Moved CPL wrapping to cpl.i
 *
 * Revision 1.9  2005/02/15 22:02:16  kruland
 * Previous revision introduced a problem.  Put the #defines CPL_* back in
 * until the cpl specific code is cleaned up.
 *
 * Revision 1.8  2005/02/15 21:40:00  kruland
 * Stripped out all the extras by no longer including gdal.h or gdal_priv.h.
 * Added CreateShared() method.
 *
 * Revision 1.7  2005/02/15 16:51:20  kruland
 * Removed use of <vector> and <algorith> stdlib includes.  Added typedefs for
 * the fixed size array types which are needed for new mapping mechanism.
 *
 * Revision 1.6  2005/02/15 06:25:42  kruland
 * Moved the Band definition to Band.i.
 *
 * Revision 1.5  2005/02/15 06:01:15  kruland
 * Moved the Dataset definition to Dataset.i.
 *
 * Revision 1.4  2005/02/14 23:50:16  hobu
 * Added log info
 *
*/

%module gdal

//
// We register all the drivers upon module initialization
//
%init %{
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%pythoncode %{
  from gdalconst import *
%}


%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"

typedef double double_6[6];
typedef double double_2[2];

%}

%import gdal_typemaps.i

typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;

%include "cpl.i"

%include "Driver.i"

%include "Dataset.i"

%include "Band.i"

%include "ColorTable.i"

//************************************************************************
//
// Define the global methods
//
//************************************************************************
%rename (AllRegister) GDALAllRegister;
void GDALAllRegister();

//************************************************************************
//
// Define the factory functions for Drivers and Datasets
//
//************************************************************************

%inline %{
GDALDriver* GetDriverByName( char const *name ) {
  return (GDALDriver*) GDALGetDriverByName( name );
}
%}

%newobject Open;
%inline %{
GDALDataset* Open( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  return (GDALDataset*) GDALOpen( name, eAccess );
}
%}

%newobject OpenShared;
%inline %{
GDALDataset* OpenShared( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  return (GDALDataset*) GDALOpenShared( name, eAccess );
}
%}

