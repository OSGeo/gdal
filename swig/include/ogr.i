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
 * Revision 1.1  2005/02/16 19:47:03  hobu
 * skeleton OGR interface
 *
 *
 *
*/

%module ogr

%pythoncode %{

%}

%{
#include <iostream>
using namespace std;

#include "ogr_srs_api.h"


%}

%import gdal_typemaps.i

