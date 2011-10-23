use ExtUtils::MakeMaker;

# pick up our parameters from @ARGV
my %ARGV;
for (@ARGV) {
    if (/^(.*?)\=(.*)/) {
	$ARGV{$1} = $2;
    } else {
	$ARGV{$_} = 1;
    }
    $_ = '' if /^--gdal-config/;
    $_ = '' if /^--no-version-check/;
}

# search and decide which GDAL (gdal-config) to build against

# scan known possible locations in the order of preference:
my @configs;
for ('../../apps/gdal-config',
     'c:/msys/1.0/local/bin/gdal-config',
     '/usr/local/bin/gdal-config',
     '/usr/bin/gdal-config') {
    push @configs, $_ if -r $_;
}
print "Found @configs\n";

my $config;
if ($ARGV{'--gdal-config'}) {
    die "'$ARGV{'--gdal-config'}' does not exist or is unreadable." unless -r $ARGV{'--gdal-config'};
    $config = $ARGV{'--gdal-config'};
} else {
    $config = shift @configs;
}

die "Can't find gdal-config. Please install GDAL development files or\n".
    "define the location of gdal-config using --gdal-config=XXX.\n"
    unless $config;

print "Using $config.\n";

# check that we're part of GDAL distro
# or that installed GDAL version is the same as that in lib/Geo/GDAL.pm 

my $LIB = '';
my $INC = '';
if ($config eq '../../apps/gdal-config') { 
    print "Building against GDAL in this distro tree\n";
    $LIB .= '-L../../.libs -L../.. ';
} elsif ($config eq 'c:/msys/1.0/local/bin/gdal-config') {
    print "Building against GDAL in c:/msys/1.0/local/bin/\n";
    $LIB .= '-Lc:/msys/1.0/local/lib ';
    $INC .= '-Ic:/msys/1.0/local/include ';
} else {
    print "Building against GDAL defined in $config\n";
}

my $gdal_version;
my $pm_version;
my $fh;
if (open($fh, $config)) {
    for (<$fh>) {
	($gdal_version) = /(\d+\.\d+\.\d+)/ if /^CONFIG_VERSION/;
	if (/^CONFIG_LIBS/) {
	    s/^CONFIG_LIBS="//;
	    s/"\s*$//;
	    if ($_ =~ /\.la$/) { 
		# parse a libtool library file
		$LIB .= parse_libtool_library_file_for_l($_);
	    } else {
		$LIB .= $_;
	    }
	    $LIB .= ' ';
	}
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
    close $fh;
}
if (open($fh, "lib/Geo/GDAL.pm")) {
    for (<$fh>) {
	($pm_version) = /(\d+\.\d+\.\d+)/ if /GDAL_VERSION/;
    }
    close $fh;
} else {
    die "GDAL Perl modules not found, perhaps you need to run make generate?";
}

die "=======================================================\n".
    "PLEASE NOTE!\n".
    "The GDAL that you try to build against has version\n".
    "$gdal_version and this module was released from version\n".
    "$pm_version. These do not match. Building against newer\n".
    "version may work but you need to remove this check first.\n".
    "You can pass by this warning with --no-version-check.\n".
    "Thank you.\n".
    "=======================================================\n"
    if ($gdal_version ne $pm_version) and !$ARGV{'--no-version-check'};

%object = ( 'Geo::GDAL' => 'gdal_wrap.o',
	    'Geo::GDAL::Const' => 'gdalconst_wrap.o',
	    'Geo::OGR' => 'ogr_wrap.o',
	    'Geo::OSR' => 'osr_wrap.o' );

#print "LIB = $LIB\n";
#print "INC = $INC\n";

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

sub parse_libtool_library_file_for_l {
    my $fn = shift;
    my $fh;
    my $l = '';
    if (open($fh, $fn)) {
	while (<$fh>) {
	    if (/^dlname=(.*)/) {
		$l = $1;
		$l =~ s/^'//;
		$l =~ s/^lib/\-l/;
		$l =~ s/\..*$//;
		last;
	    }
	}
	close $fh;
    }
    return $l;
}
