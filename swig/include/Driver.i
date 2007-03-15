/******************************************************************************
 * $Id$
 *
 * Name:     Driver.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 *****************************************************************************/

/*************************************************************************
*
*  Define the extensions for Driver (nee GDALDriverShadow)
*
*************************************************************************/


%rename (Driver) GDALDriverShadow;

class GDALDriverShadow : public GDALMajorObjectShadow {
private:
  ~GDALDriverShadow();
  GDALDriverShadow();
public:
%extend {

%immutable;
  char const *ShortName;
  char const *LongName;
  char const *HelpTopic;
%mutable;

%newobject Create;
%feature( "kwargs" ) Create;
  GDALDatasetShadow *Create( const char *name, int xsize, int ysize, int bands =1,
                       GDALDataType eType=GDT_Byte, char **options = 0 ) {
    GDALDatasetShadow* ds = (GDALDatasetShadow*) GDALCreate( self, name, xsize, ysize, bands, eType, options );
    return ds;
  }

%newobject CreateCopy;
%feature( "kwargs" ) CreateCopy;
  GDALDatasetShadow *CreateCopy( const char *name, GDALDatasetShadow* src, int strict =1, char **options = 0 ) {
    GDALDatasetShadow *ds = (GDALDatasetShadow*) GDALCreateCopy(self, name, src, strict, options, 0, 0 );
    return ds;
  }

  int Delete( const char *name ) {
    return GDALDeleteDataset( self, name );
  }

  int Register() {
    return GDALRegisterDriver( self );
  }

  void Deregister() {
    return GDALDeregisterDriver( self );
  }

}
};

%{
char const *GDALDriverShadow_ShortName_get( GDALDriverShadow *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriverShadow_LongName_get( GDALDriverShadow *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriverShadow_HelpTopic_get( GDALDriverShadow *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}

