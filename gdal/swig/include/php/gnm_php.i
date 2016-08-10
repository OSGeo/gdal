/*
 * $Id$
 *
 * php specific code for gnm bindings.
 */

%init %{
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
%}

%include typemaps_php.i
