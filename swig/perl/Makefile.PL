use ExtUtils::MakeMaker;

$ext = $^O eq 'MSWin32' ? '.dll' : ''; # works with MinGW but probably not with ActivePerl

$libs = $ENV{'PREFIX'} ? "-L$ENV{'PREFIX'}/lib " : '';

$libs .= "-LC:/msys/1.0/local/lib -lgdal$ext";

$libs .= ' -lstdc++' if $^O eq 'cygwin';

$inc = '-IC:/msys/1.0/local/include';

$obj = 'gdal_wrap.o gdalconst_wrap.o ogr_wrap.o osr_wrap.o';

WriteMakefile( NAME => 'Geo::GDAL',
	       VERSION_FROM => 'lib/Geo/GDAL.pm',
	       LIBS => $libs,
	       INC => $inc,
	       OBJECT => $obj,
	       );
