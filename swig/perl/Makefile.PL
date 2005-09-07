use ExtUtils::MakeMaker;

WriteMakefile( NAME => 'gdal',
               FIRST_MAKEFILE => 'Makefile_gdal',
               LIBS => '-lgdal',
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'gdal_wrap.o'
             );

WriteMakefile( NAME => 'gdalconst',
               FIRST_MAKEFILE => 'Makefile_gdalconst',
               LIBS => '-lgdal',
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'gdalconst_wrap.o'
             );

WriteMakefile( NAME => 'ogr',
               FIRST_MAKEFILE => 'Makefile_ogr',
               LIBS => '-lgdal',
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'ogr_wrap.o'
             );

WriteMakefile( NAME => 'osr',
               FIRST_MAKEFILE => 'Makefile_osr',
               LIBS => '-lgdal',
               INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
               OBJECT => 'osr_wrap.o'
             );

