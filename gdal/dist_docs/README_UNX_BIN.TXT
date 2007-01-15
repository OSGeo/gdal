
		GDAL/OGR Binary Release
		=======================

This is a release of GDAL/OGR libraries and related utilities.  Full
information on GDAL and OGR can be found at:

  http://www.remotesensing.org/gdal                (GDAL)
  http://gdal.velocet.ca/projects/opengis          (OGR)

Details on the utility programs in this package can be found specifically at:

  http://www.remotesensing.org/gdal/gdal_utilities.html
  http://gdal.velocet.ca/projets/opengis/ogrhtml/ogr_utilities.html


Installing GDAL/OGR
-------------------

The install_unx.sh script can be used to install GDAL into an existing
file system tree such as /usr/local or /usr.  It assumes that the 
$PREFIX/bin directory will already be in the user path, and that 
$PREFIX/lib is already in the LD_LIBRARY_PATH or configured for ldconfig.
If ldconfig is used it may be necessary to rerun ldconfig as root. 

To install:

 ./install_unx.sh /usr/local

Note that html documentation and python files are not installed.  Only the
shared library, binaries and shared data files are installed into
$PREFIX/lib, $PREFIX/bin and $PREFIX/share/gdal respectively.

After installation the "gdalinfo --version" command can be used to determine
if the installation was successful.  Then use "gdal_translate --version"
to verify that the shared libraries are being found properly.  If not
add $PREFIX/lib to the LD_LIBRARY_PATH or rerun ldconfig as appropriate
to local usage and the platform in question. 


Using GDAL/OGR Without Installing
---------------------------------

To utilize GDAL/OGR the following steps need to be followed:

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

