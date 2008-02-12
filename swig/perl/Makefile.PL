use ExtUtils::MakeMaker;

$libs = '';

$libdir = $ENV{'PREFIX'} ? "-L$ENV{'PREFIX'}/lib" : '';
$libdir .= ' -L/msys/1.0/local/lib'; # for MinGW, should do no harm in other systems

$ext = $^O eq 'MSWin32' ? '.dll' : ''; # works with MinGW but probably not with ActivePerl

$libs .= " -lgdal$ext";
$libs .= ' -lstdc++' if $^O eq 'cygwin';

# for old-style, use gdal, gdalconst,ogr, and osr as keys

%object = ( 'Geo::GDAL' => 'gdal_wrap.o',
	    'Geo::GDAL::Const' => 'gdalconst_wrap.o',
	    'Geo::OGR' => 'ogr_wrap.o',
	    'Geo::OSR' => 'osr_wrap.o' );

for my $module (keys %object) {
    my $add = $module;
    $add =~ s/:/_/g;
    WriteMakefile( NAME => $module,
		   VERSION_FROM => 'lib/Geo/GDAL.pm',
		   MAKEFILE => 'Makefile_'.$add,
		   LIBS => "$libdir -L../../.libs -L../.. $libs",
		   INC => '-I../../gcore -I../../alg -I../../ogr -I../../port',
		   OBJECT => $object{$module}
		   );
}
