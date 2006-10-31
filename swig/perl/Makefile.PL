use ExtUtils::MakeMaker;

$ExtUtils::MakeMaker::Verbose = 0;

$libdir='';

$libdir = "-L$ENV{'PREFIX'}/lib" if ($ENV{'PREFIX'});

for my $lib ('gdal','gdalconst','ogr','osr') {
    WriteMakefile( NAME => $lib,
		   FIRST_MAKEFILE => 'Makefile_'.$lib,
		   LD => "g++",
		   MAKEFILE => 'Makefile_'.$lib,
		   LIBS => "$libdir -L../../.libs -lgdal",
		   #CCFLAGS => "-fpic", # needed for SELinux, but using overrides default behavior
		   INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
		   OBJECT => $lib.'_wrap.o'
		   );
}
