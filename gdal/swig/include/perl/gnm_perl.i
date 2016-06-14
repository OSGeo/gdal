/******************************************************************************
 *
 * Project:  GNM SWIG Interface declarations for Perl.
 *****************************************************************************/

%init %{

  /*UseExceptions(); is set by GDAL module */
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }

%}

%include callback.i
%include confess.i
%include cpl_exceptions.i

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (RegisterAll) OGRRegisterAll();

%import typemaps_perl.i

%import destroy.i

%rename (_CreateLayer) CreateLayer;
%rename (_DeleteLayer) DeleteLayer;
%rename (_Validate) Validate;

