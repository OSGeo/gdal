/*
 * $Id$
 */

/*
 * $Log$
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */

%rename (GetFieldType) GetType;
%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%inline %{
OGRDriverShadow* OGR_GetDriverByName( char const *name ) {
  return (OGRDriverShadow*) OGRGetDriverByName( name );
}
  
OGRDriverShadow* OGR_GetDriver(int driver_number) {
  return (OGRDriverShadow*) OGRGetDriver(driver_number);
  

}
%}

%feature( "kwargs" ) OGR_Open;
%newobject Open;
%inline %{
  OGRDataSourceShadow *Open( const char * filename, int update=0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(filename,update, NULL);
    return ds;
  }
 
%}

%feature( "kwargs" ) OGR_OpenShared;
%newobject OpenShared;
%inline %{
  OGRDataSourceShadow *OpenShared( const char * filename, int update=0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(filename,update, NULL);
    return ds;
  }
 
%}

%include typemaps_csharp.i
