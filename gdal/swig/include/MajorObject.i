
%rename (MajorObject) GDALMajorObjectShadow;

class GDALMajorObjectShadow {
private:
  GDALMajorObjectShadow();
  ~GDALMajorObjectShadow();

public:
%extend {
/*
 * GetDescription
 */
  const char *GetDescription() {
    return GDALGetDescription( self );
  }

/*
 * SetDescription
 */
  void SetDescription( const char *pszNewDesc ) {
    GDALSetDescription( self, pszNewDesc );
  }

/*
 * GetMetadata methods
 */
%apply (char **dict) { char ** };
  char ** GetMetadata_Dict( const char * pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

%apply (char **options) {char **};
  char **GetMetadata_List( const char *pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

/*
 * SetMetadata methods
 */
%apply (char **dict) { char ** papszMetadata };
  CPLErr SetMetadata( char ** papszMetadata, const char * pszDomain = "" ) {
    return GDALSetMetadata( self, papszMetadata, pszDomain );
  }
%clear char **papszMetadata;

  CPLErr SetMetadata( char * pszMetadataString , const char *pszDomain = "" ) {
    char *tmpList[2];
    tmpList[0] = pszMetadataString;
    tmpList[1] = 0;
    return GDALSetMetadata( self, tmpList, pszDomain );
  }

/*
 * GetMetadataItem
 */

/*
 * SetMetadataItem
 */

} /* %extend */
};
