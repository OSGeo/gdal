use ExtUtils::MakeMaker;

$ExtUtils::MakeMaker::Verbose = 1;

$libdir='';

$libdir = "-L$ENV{'PREFIX'}/lib" if ($ENV{'PREFIX'});

WriteMakefile( NAME => 'gdal',
               FIRST_MAKEFILE => 'Makefile_gdal',
	       LD => "g++",
               MAKEFILE => 'Makefile_gdal',
               LIBS => "$libdir -lgdal",
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'gdal_wrap.o'
             );

WriteMakefile( NAME => 'gdalconst',
               FIRST_MAKEFILE => 'Makefile_gdalconst',
               MAKEFILE => 'Makefile_gdalconst',
               LIBS => "$libdir -lgdal",
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'gdalconst_wrap.o'
             );

WriteMakefile( NAME => 'ogr',
               FIRST_MAKEFILE => 'Makefile_ogr',
               MAKEFILE => 'Makefile_ogr',
	       LD => "g++",
               LIBS => "$libdir -lgdal",
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'ogr_wrap.o'
             );

WriteMakefile( NAME => 'osr',
               FIRST_MAKEFILE => 'Makefile_osr',
               MAKEFILE => 'Makefile_osr',
	       LD => "g++",
               LIBS => "$libdir -lgdal",
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'osr_wrap.o'
             );

