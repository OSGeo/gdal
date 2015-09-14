use ExtUtils::MakeMaker;
use Config;

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
# ... or from the environment
$ARGV{'--gdal-config'} = $ENV{PERL_GDAL_CONFIG} if $ENV{PERL_GDAL_CONFIG};
$ARGV{'--no-version-check'} = 1 if $ENV{PERL_GDAL_NO_VERSION_CHECK};

# search and decide which GDAL (gdal-config) to build against if not given

my $config;
if ($ARGV{'--gdal-config'}) {
    if (-r $ARGV{'--gdal-config'}) {
        $config = $ARGV{'--gdal-config'};
    } else {
        print STDERR "The given gdal-config '$ARGV{'--gdal-config'}' does not exist or is unreadable.\n";
    }
} else {
    # scan known possible locations in the order of preference:
    my @configs;
    for ('../../apps/gdal-config',
         'c:/msys/1.0/local/bin/gdal-config',
         '/usr/local/bin/gdal-config',
         '/usr/bin/gdal-config') {
        push @configs, $_ if -r $_;
    }
    if (@configs) {
        $config = $configs[0];
        if (@configs) {
            print "Found gdal-config(s): '",join("', '", @configs),"'.\n";
        } else {
            print "Did not find any gdal-config(s)\n";
        }
        print "Will use '$config' (first from found or the given).\n";
    }
}
die "Can't find gdal-config. Please install GDAL development files or\n".
    "define the location of gdal-config using --gdal-config=XXX\n".
    "or with environment variable PERL_GDAL_CONFIG." unless $config;

# Obtain GDAL version, libs, and inc. We will assume gdal-config is not
# excutable if we're in a GDAL distro tree or on Windows.

my $gdal_version;
my $LIB = '';
my $INC = '';
if ($config eq '../../apps/gdal-config' or $config eq 'c:/msys/1.0/local/bin/gdal-config') { 
    if ($config eq '../../apps/gdal-config') {
        print "Building against GDAL in this distro tree\n";
        $LIB .= '-L../../.libs -L../.. ';
    } else {
        print "Building against GDAL in 'c:/msys/1.0/local/bin/'\n";
        $LIB .= '-Lc:/msys/1.0/local/lib ';
        $INC .= '-Ic:/msys/1.0/local/include ';
    }
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
} else {
    print "Building against GDAL defined by '$config'.\n";
    chomp($gdal_version = `$config --version`);
    chomp($LIB = `$config --libs`);
    chomp($INC = `$config --cflags`);
}

# Obtain our version.

my $pm_version;
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
    "$pm_version. These do not match. Building against different\n".
    "version may work but you need to remove this check first.\n".
    "You can pass by this warning with --no-version-check\n".
    "or by setting the environment variable PERL_GDAL_NO_VERSION_CHECK\n".
    "to a true value (for example 1).\n".
    "Thank you.\n".
    "=======================================================\n"
    if ($gdal_version ne $pm_version) and !$ARGV{'--no-version-check'};

%object = ( 'Geo::GDAL' => 'gdal_wrap.o',
            'Geo::GDAL::Const' => 'gdalconst_wrap.o',
            'Geo::OGR' => 'ogr_wrap.o',
            'Geo::OSR' => 'osr_wrap.o' );

if ($ARGV{'--debug'}) {
    print "LIB = $LIB\n";
    print "INC = $INC\n";
}

for my $module (keys %object) {
    my $add = $module;
    $add =~ s/:/_/g;
    WriteMakefile( NAME => $module,
                   VERSION_FROM => 'lib/Geo/GDAL.pm',
                   ABSTRACT_FROM => 'lib/Geo/GDAL.pm',
                   AUTHOR => 'Ari Jolma <https://trac.osgeo.org/gdal/wiki/GdalOgrInPerl>',
                   LICENSE => 'mit',
                   META_MERGE => {
                       'meta-spec' => { version => 2 },
                       resources => {
                           repository => {
                               type => 'svn',
                               url  => 'https://svn.osgeo.org/gdal/trunk/gdal/swig/perl',
                               web  => 'https://trac.osgeo.org/gdal/browser/trunk/gdal/swig/perl',
                           },
                       },
                   },
                   MAKEFILE => 'Makefile_'.$add,
                   LIBS => $LIB,
                   INC => $INC,
                   OPTIMIZE => "$ENV{CFLAGS} $ENV{CPPFLAGS}",
                   LD => "$Config{ld} $ENV{CFLAGS} $ENV{LDFLAGS}",
                   OBJECT => $object{$module},
                   PM => {'lib/Geo/GDAL.pm' => '$(INST_LIBDIR)/GDAL.pm',
                          'lib/Geo/OGR.pm' => '$(INST_LIBDIR)/OGR.pm',
                          'lib/Geo/OSR.pm' => '$(INST_LIBDIR)/OSR.pm',
                          'lib/Geo/GDAL/Const.pm' => '$(INST_LIBDIR)/GDAL/Const.pm'}
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
