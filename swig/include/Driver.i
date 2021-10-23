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

%include constraints.i

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

%apply Pointer NONNULL { const char* newName, const char* oldName, GDALDatasetShadow* src };

%newobject Create;
#ifndef SWIGJAVA
%feature( "kwargs" ) Create;
#endif
  GDALDatasetShadow *Create(    const char *utf8_path,
                                int xsize,
                                int ysize,
                                int bands = 1,
                                GDALDataType eType=GDT_Byte,
                                char **options = 0 ) {

    GDALDatasetShadow* ds = (GDALDatasetShadow*) GDALCreate(    self,
                                                                utf8_path,
                                                                xsize,
                                                                ysize,
                                                                bands,
                                                                eType,
                                                                options );
    return ds;
  }

%newobject CreateMultiDimensional;
#ifndef SWIGJAVA
%feature( "kwargs" ) CreateMultiDimensional;
#endif
%apply (char **options ) { (char **root_group_options) };
  GDALDatasetShadow *CreateMultiDimensional(    const char *utf8_path,
                                char **root_group_options = 0,
                                char **options = 0 ) {

    GDALDatasetShadow* ds = (GDALDatasetShadow*) GDALCreateMultiDimensional(    self,
                                                                utf8_path,
                                                                root_group_options,
                                                                options );
    return ds;
  }
%clear (char **root_group_options);

%newobject CreateCopy;
#ifndef SWIGJAVA
#ifndef SWIGJAVA
%feature( "kwargs" ) CreateCopy;
#endif
#endif
  GDALDatasetShadow *CreateCopy(    const char *utf8_path,
                                    GDALDatasetShadow* src,
                                    int strict = 1,
                                    char **options = 0,
                                    GDALProgressFunc callback = NULL,
                                    void* callback_data=NULL) {

    GDALDatasetShadow *ds = (GDALDatasetShadow*) GDALCreateCopy(    self,
                                                                    utf8_path,
                                                                    src,
                                                                    strict,
                                                                    options,
                                                                    callback,
                                                                    callback_data );
    return ds;
  }

  CPLErr Delete( const char *utf8_path ) {
    return GDALDeleteDataset( self, utf8_path );
  }

  CPLErr Rename( const char *newName, const char *oldName ) {
    return GDALRenameDataset( self, newName, oldName );
  }

  CPLErr CopyFiles( const char *newName, const char *oldName ) {
    return GDALCopyDatasetFiles( self, newName, oldName );
  }

  int Register() {
    return GDALRegisterDriver( self );
  }

  void Deregister() {
    GDALDeregisterDriver( self );
  }
}
};

%clear const char *name, const char* newName, const char* oldName, GDALDatasetShadow* src;

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

