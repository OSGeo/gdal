/******************************************************************************
 *
 * Name:     Driver.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
 *
 * SPDX-License-Identifier: MIT
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

%newobject CreateVector;
#ifndef SWIGJAVA
%feature( "kwargs" ) CreateVector;
#endif
  GDALDatasetShadow *CreateVector(const char *utf8_path, char **options = 0) {
    GDALDatasetShadow* ds = (GDALDatasetShadow*) GDALCreate(self, utf8_path, 0, 0, 0, GDT_Unknown, options);
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

%apply ( const char *utf8_path ) { ( const char *newName ) };
%apply ( const char *utf8_path ) { ( const char *oldName ) };
  CPLErr Rename( const char *newName, const char *oldName ) {
    return GDALRenameDataset( self, newName, oldName );
  }

  CPLErr CopyFiles( const char *newName, const char *oldName ) {
    return GDALCopyDatasetFiles( self, newName, oldName );
  }
%clear ( const char *newName );
%clear ( const char *oldName );

  bool HasOpenOption( const char *openOptionName ) {
    return GDALDriverHasOpenOption( self, openOptionName );
  }

#ifdef SWIGPYTHON
  bool TestCapability(const char* cap) {
    // TODO: should this also check DCAP entries in driver metadata?
    return (OGR_Dr_TestCapability(self, cap) > 0);
  }
#endif

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

