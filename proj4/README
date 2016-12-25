   -------------------- P R O J . 4 --------------------

This is Release 4.4 of cartographic projection software.

PLEASE read the following information as well as READMEs in the src
and nad directories.

For more information on PROJ.4 maintenance please see the web page at:

  http://www.remotesensing.org/proj
                or
  http://proj.maptools.org/

   ---------------------------------------------------

Installation:
-------------

FSF's configuration procedure is used to ease installation of the
PROJ.4 system.

The default destination path prefix for installed files is /usr/local.
Results from the installation script will be placed into subdirectories
bin, include, lib, man/man1 and man/man3.  If this default path prefix
is proper, then execute:

	./configure

If another path prefix is required, then execute:

	./configure --prefix=/my/path

In either case, the directory of the prefix path must exist and be
writable by the installer.

After executing configure, execute:

	make
        make install

The install target will create, if necessary, all required sub-directories.

Windows Build
-------------

PROJ.4 can be built with Microsoft Visual C/C++ using the makefile.vc
in the PROJ directory.  First edit the PROJ\nmake.opt and modify
the INSTDIR value at the top to point to the directory where
the PROJ tree shall be installed. If you want to install into
C:\PROJ, it can remain unchanged.
Then use the makefile.vc to build the software:

eg. 
C:\> cd proj
C:\PROJ> nmake /f makefile.vc
C:\PROJ> nmake /f makefile.vc install-all

Note that you have to have the VC++ environment variables, and path
setup properly.  This may involve running the VCVARS32.BAT script out
of the Visual C++ tree.  

The makefile.vc builds proj.exe, proj.dll and proj.lib. 

It should also be possible to build using the Unix instructions
and Cygwin32, but this hasn't been tested recently. 


   ---------------------------------------------------

Distribution files and format.
------------------------------

Sources are distributed in one or more files.  The principle elements
of the system are in a compress tar file named `PROJ.4.x.tar.gz' where
"x" will indicate level.sub-level of the release.  For U.S. users
interested in NADCON datum shifting procedures, additional files
containing conversion matricies are distributed with the name
`PROJ.4.x.y.tar' where y is an uppercase letter starting with "A."
These supplementary files will contain compressed files and thus
the tar file is not compressed.

Interim reports on Rel. 4 proj are available in PostScript form as
*.ps.gz .  New and old users are strongly recommended to carefully read
these manuals.  They are supplements and NOT a replacement for the full
manual OF 90-284 (which new users should also obtain).

   ---------------------------------------------------

Principle new aspects of system:
--------------------------------

ANSI X3.159-1989 C code.  Site must have ANSI C compiler and header files.

Several method of determining radius from specified ellipsoid.

Use of initialization files through +init=file:key.  Default projection
specifications also may be defined in an ASCII file.

+inv option REMOVED and -I may be used in its place.  Use of invproj
alias of proj still functions as per Rel.3.

+ellps=list and +proj=list REMOVED.  Use respective -le and -lp.

+units= to specify cartesian coordinate system units.  To get list
use -lu.

-v added to dump final cartographic parameters employed.

Addition of computing scale factors and angular distortion added through
-S option.  Valuable for designing new projection parameter details.

-V option which verbosely lists projected point characteristics.

Programmers may use projection library with calls to pj_init,
pj_fwd, pj_inv and pj_transform.

Program nad2nad for conversion of data to and from NAD27 and NAD83
datums.

Program cs2cs for converting between coordinate systems, with optional
datum translation.

-------------------------------------------------------------

Things currently left undone:
----------------------------

proj_def.dat NOT fully in place.  Needs additional settings for
many of the projections.  Probably will not be completed until main
manual rewritten.
