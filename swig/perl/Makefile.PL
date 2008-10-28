use ExtUtils::MakeMaker;

$LIB = '';
$INC = '';
# check that we're part of GDAL distro
# or that installed GDAL version is the same as that in lib/Geo/GDAL.pm 
if (-f "../../GDALmake.opt") {
    print "Building in GDAL distro tree\n";
    $LIB .= '-L../../.libs -L../..';
    $INC .= '-I../../gcore -I../../alg -I../../ogr -I../../port ';
    $fh = "../../apps/gdal-config";
} elsif (-f "c:/msys/1.0/local/bin/gdal-config") {
    print "found gdal-config in c:/msys/1.0/local/bin/\n";
    $LIB .= '-Lc:/msys/1.0/local/lib';
    $INC .= '-Ic:/msys/1.0/local/include ';
    $fh = "c:/msys/1.0/local/bin/gdal-config";
} elsif (-f "/usr/local/bin/gdal-config") {
    print "found gdal-config in /usr/local/bin/\n";
    $fh = "/usr/local/bin/gdal-config";
} elsif (-f "/usr/bin/gdal-config") {
    print "found gdal-config in /usr/bin/\n";
    $fh = "/usr/bin/gdal-config";
} else {
    die "can't find gdal-config";
}
open $fh, $fh or die "$fh not found!";
$LIB .= " -lgdal ";
if ($fh) {
    @config = <$fh>;
    close $fh;
    for (@config) {
	($gdal_version) = /(\d+\.\d+\.\d+)/ if /^CONFIG_VERSION/;
	if (/^CONFIG_DEP_LIBS/) {
	    s/^CONFIG_DEP_LIBS="//;
	    s/"\s*$//;
	    $LIB .= $_;
	}
	if (/^CONFIG_CFLAGS/) {
	    s/^CONFIG_CFLAGS="//;
	    s/"\s*$//;
	    $INC .= $_;
	}
    }
    open $fh, "lib/Geo/GDAL.pm" or die "lib/Geo/GDAL.pm not found, perhaps you need to run make generate?";
    @config = <$fh>;
    close $fh;
    for (@config) {
	($pm_version) = /(\d+\.\d+\.\d+)/ if /GDAL_VERSION/;
    }
    die "GDAL version ($gdal_version) and module version ($pm_version) do not match" 
	unless $gdal_version eq $pm_version;
}

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
		   LIBS => $LIB,
		   INC => $INC,
		   OBJECT => $object{$module}
		   );
}
