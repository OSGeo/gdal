/*
 * $Id$
 *
 * ruby specific code for ogr bindings.
 */

/* Include default Ruby typemaps */
%include typemaps_ruby.i

/* Include exception handling code */
%include cpl_exceptions.i

/* Setup a few renames */
%rename(get_driver_count) OGRGetDriverCount;
%rename(get_open_dscount) OGRGetOpenDSCount;
%rename(register_all) OGRRegisterAll;


%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }

  /* Setup exception handling */
  UseExceptions();
%}

