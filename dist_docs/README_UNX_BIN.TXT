		GDAL/OGR Binary Release
		=======================


This is a release of GDAL/OGR libraries and related utilities.  To utilize 
the following steps need to be followed:

 1) Define GDAL_HOME to point to this directory.
 2) Ensure that $GDAL_HOME/lib is in the LD_LIBRARY_PATH.
 3) Ensure $GDAL_HOME/bin is in the PATH. 

The following commands in a .cshrc file (for csh or tcsh) should suffice:

  setenv GDAL_HOME ...insert path to this distribution directory...
  set path = ($path $GDAL_HOME/bin)
  if ( $?LD_LIBRARY_PATH == 0 ) then
    setenv LD_LIBRARY_PATH $GDAL_HOME/lib
  else
    setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$GDAL_HOME/lib
  endif

The following commands should work for bash, sh, ksh and zsh:

  GDAL_HOME=...insert path to this distribution directory...
  export GDAL_HOME
  PATH=$PATH:$GDAL_HOME/bin
  if test "${LD_LIBRARY_PATH:-EMPTY}" == "EMPTY" ; then
    LD_LIBRARY_PATH=$GDAL_HOME/lib
    export LD_LIBRARY_PATH
  else
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GDAL_HOME/lib
  fi




