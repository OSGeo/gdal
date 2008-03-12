use ExtUtils::MakeMaker;

$ext = $^O eq 'MSWin32' ? '.dll' : ''; # works with MinGW but probably not with ActivePerl

$libs = $ENV{'PREFIX'} ? "-L$ENV{'PREFIX'}/lib " : '';

$libs .= "-L../../.libs -L../..";
$libs .= " -LC:/msys/1.0/local/lib -lgdal$ext";

$libs .= ' -lstdc++' if $^O eq 'cygwin';

$inc = '-I../../gcore -I../../alg -I../../ogr -I../../port';
$inc .= ' -IC:/msys/1.0/local/include';

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
		   LIBS => $libs,
		   INC => $inc,
		   OBJECT => $object{$module}
		   );
}
