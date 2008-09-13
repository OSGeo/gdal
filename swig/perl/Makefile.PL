use ExtUtils::MakeMaker;

$LIB = '';
$INC = '';
# check that we're part of GDAL distro
# or that installed GDAL version is the same as that in lib/Geo/GDAL.pm 
if (-d "../../.libs") {
    print "Building in GDAL distro tree\n";
    $LIB = '-L../../.libs';
    $INC = '-I../../gcore -I../../alg -I../../ogr -I../../port';
} elsif (-f "c:/msys/1.0/local/bin/gdal-config") {
    print "found gdal-config in c:/msys/1.0/local/bin/\n";
    $LIB = '-Lc:/msys/1.0/local/lib';
    $INC = '-Ic:/msys/1.0/local/include';
    open $fh, "c:/msys/1.0/local/bin/gdal-config";
} elsif (-f "/usr/local/bin/gdal-config") {
    print "found gdal-config in /usr/local/bin/\n";
    open $fh, "/usr/local/bin/gdal-config";
} else {
    die "can't find gdal-config";
}
if ($fh) {
    @config = <$fh>;
    close $fh;
    for (@config) {
	($gdal_version) = /(\d+\.\d+\.\d+)/ if /^CONFIG_VERSION/;
    }
    open $fh, "lib/Geo/GDAL.pm";
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
		   LIBS => "$LIB -lgdal",
		   INC => $INC,
		   OBJECT => $object{$module}
		   );
}
