/******************************************************************************
 * $Id$
 *
 * Name:     ogr.i
 * Project:  GDAL Python Interface
 * Purpose:  OGR Core SWIG Interface declarations.
 * Author:   Howard Butler, hobu@iastate.edu
 *

 *
 * $Log$
 * Revision 1.2  2005/02/16 20:02:57  hobu
 * added constants
 *
 * Revision 1.1  2005/02/16 19:47:03  hobu
 * skeleton OGR interface
 *
 *
 *
*/

%module ogr

%init %{
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
%}

  
%pythoncode %{

wkb25Bit = -2147483648 # 0x80000000
wkbUnknown = 0
wkbPoint = 1  
wkbLineString = 2
wkbPolygon = 3
wkbMultiPoint = 4
wkbMultiLineString = 5
wkbMultiPolygon = 6
wkbGeometryCollection = 7
wkbNone = 100
wkbLinearRing = 101
wkbPoint25D =              wkbPoint              + wkb25Bit
wkbLineString25D =         wkbLineString         + wkb25Bit
wkbPolygon25D =            wkbPolygon            + wkb25Bit
wkbMultiPoint25D =         wkbMultiPoint         + wkb25Bit
wkbMultiLineString25D =    wkbMultiLineString    + wkb25Bit
wkbMultiPolygon25D =       wkbMultiPolygon       + wkb25Bit
wkbGeometryCollection25D = wkbGeometryCollection + wkb25Bit

# OGRFieldType

OFTInteger = 0
OFTIntegerList= 1
OFTReal = 2
OFTRealList = 3
OFTString = 4
OFTStringList = 5
OFTWideString = 6
OFTWideStringList = 7
OFTBinary = 8

# OGRJustification

OJUndefined = 0
OJLeft = 1
OJRight = 2

wkbXDR = 0
wkbNDR = 1

###############################################################################
# Constants for testing Capabilities

# Layer
OLCRandomRead          = "RandomRead"
OLCSequentialWrite     = "SequentialWrite"
OLCRandomWrite         = "RandomWrite"
OLCFastSpatialFilter   = "FastSpatialFilter"
OLCFastFeatureCount    = "FastFeatureCount"
OLCFastGetExtent       = "FastGetExtent"
OLCCreateField         = "CreateField"
OLCTransactions        = "Transactions"
OLCDeleteFeature       = "DeleteFeature"
OLCFastSetNextByIndex  = "FastSetNextByIndex"

# DataSource
ODsCCreateLayer        = "CreateLayer"
ODsCDeleteLayer        = "DeleteLayer"

# Driver
ODrCCreateDataSource   = "CreateDataSource"
ODrCDeleteDataSource   = "DeleteDataSource"

%}

%{
#include <iostream>
using namespace std;

#include "ogr_api.h"
#include "ogr_core.h"

%}

%import gdal_typemaps.i

