use strict;
use warnings;
use File::Basename;
use ExtUtils::MakeMaker;
use Config;
use Cwd;

# pick up our parameters from @ARGV
my %ARGV;
for (@ARGV) {
    if (/^(.*?)\=(.*)/) {
        $ARGV{$1} = $2;
    } else {
        $ARGV{$_} = 1;
    }
    $_ = '' if /^--gdal-source-tree/;
    $_ = '' if /^--gdal-config/;
    $_ = '' if /^--no-version-check/;
    $_ = '' if /^--no-downloads/;
    print STDERR "Warning: unknown parameter: '$_'.\n" if $_;
}
# ... or from the environment
$ARGV{'--gdal-source-tree'} = $ENV{PERL_GDAL_SOURCE_TREE} if $ENV{PERL_GDAL_SOURCE_TREE};
$ARGV{'--gdal-config'} = $ENV{PERL_GDAL_CONFIG} if $ENV{PERL_GDAL_CONFIG};
$ARGV{'--no-version-check'} = 1 if $ENV{PERL_GDAL_NO_VERSION_CHECK};
$ARGV{'--no-downloads'} = 1 if $ENV{PERL_GDAL_NO_DOWNLOADS};

if ($ARGV{'--help'}) {
    print <<end;
'perl Makefile.pl' configures this package to many kinds of systems.

Usage: perl Makefile.pl [OPTION]

To assign environment variables, specify them as VAR=VALUE.

Control the GDAL that this module is built against:

  --gdal-source-tree=PATH  use PATH as the gdal source
    the same as environment variable PERL_GDAL_SOURCE_TREE=PATH

  --gdal-config=PATH  use PATH as the gdal-config
    the same as environment variable PERL_GDAL_CONFIG=PATH

  --no-version-check  allow building against GDAL with different version
    the same as setting environment variable PERL_GDAL_NO_VERSION_CHECK=1

  --no-downloads      disable fetching GDAL source code as a last resort
    the same as setting environment variable PERL_GDAL_NO_DOWNLOADS=1

More information is available at ExtUtils-MakeMaker documentation.

end
exit;
}

my $source_tree;
if ($ARGV{'--gdal-source-tree'}) {
    $source_tree = $ARGV{'--gdal-source-tree'};
    # hack to force downloading of the source
    if ($ARGV{'--gdal-source-tree'} eq 'download') {
        $source_tree = '';
    } else {
        die "Path '$source_tree' does not exist." unless -e $source_tree && -d $source_tree;
    }
}
elsif (-r '../../GDALmake.opt.in') {
    $source_tree = '../..';
    print "Building in source tree.\n" if $source_tree;
}

# Obtain the version of GDAL for this module distribution.

my $my_gdal_version;
if (open(my $fh, "lib/Geo/GDAL.pm")) {
    for (<$fh>) {
        ($my_gdal_version) = /(\d+\.\d+\.\d+)/ if /GDAL_VERSION/;
    }
    close $fh;
} else {
    die "GDAL Perl modules not found, perhaps you need to run make generate?";
}

# search and decide which GDAL (gdal-config) to build against if not given

my $versions_may_differ = $ARGV{'--no-version-check'};
my $downloads_are_ok = !$source_tree && !$ARGV{'--no-downloads'};
my $gdal_config;
if ($source_tree) {
    $gdal_config = "$source_tree/apps/gdal-config";
    die "There is no gdal-config in '$source_tree'.\n".
        "You have to first say \"cd $source_tree; make\"." unless -r $gdal_config;
}
elsif ($ARGV{'--gdal-config'}) {
    $gdal_config = $ARGV{'--gdal-config'};
}
my $config;
if ($gdal_config) {
    if (-r $gdal_config) {
        $config = $gdal_config;
    } else {
        die "The gdal-config '$gdal_config' does not exist or is unreadable.\n";
    }
} else {
    # scan known possible locations in the order of preference:
    my @configs;
    for ('c:/msys/1.0/local/bin/gdal-config',
         '/usr/local/bin/gdal-config',
         '/usr/bin/gdal-config') {
        push @configs, $_ if -r $_;
    }
    if (@configs) {
        $config = $configs[0];
        if (@configs) {
            print "Found gdal-config(s): '",join("', '", @configs),"'.\n";
            print "Will try '$config'.\n";
        } else {
            print STDERR "Did not find any gdal-config(s)\n";
        }
    }
}

if (!$config && $downloads_are_ok) {
    $source_tree = download_and_compile_gdal($my_gdal_version);
    $config = "$source_tree/apps/gdal-config";
}

my $uri = gdal_source_location($my_gdal_version);

my $gdal_version = get_gdal_version($config);
my $different_versions = $my_gdal_version ne $gdal_version;

my $msg = <<end;

The version of the GDAL development files you spefied is different
from the version these bindings were developed for (I have
$my_gdal_version and $config has $gdal_version). 
You can either

- get GDAL development files from $uri or from wherever you get
  development files for your system,

- specify another gdal-config,

- allow building against a different version of GDAL development files
  (with --no-version-check or by setting the environment variable
  PERL_GDAL_NO_VERSION_CHECK to a true value) - this is however not
  recommended, or

- let me download and build GDAL for you. It will happen automatically
  if you clear the command line and remove environment variables that
  may prevent it (see --help).

end

die $msg if ($gdal_config && $different_versions && !$versions_may_differ) ||
    (!$downloads_are_ok && $different_versions && !$versions_may_differ);

if ($different_versions && !$gdal_config && !$versions_may_differ && $downloads_are_ok) {
    $source_tree = download_and_compile_gdal($my_gdal_version, $gdal_version);
    $config = "$source_tree/apps/gdal-config";
}

# still in the game?

my $have_gnm = `grep "CONFIG_GNM_ENABLED" $config`;
my $have_ogr = `grep "CONFIG_OGR_ENABLED" $config`;
for ($have_gnm, $have_ogr) {
    chomp;
    $_ = /yes/ ? 1 : 0;
}

my ($INC, $LIB) = get_gdal_inc_lib($config);

my %object = ( 'Geo::GDAL' => 'gdal_wrap.o',
               'Geo::GDAL::Const' => 'gdalconst_wrap.o',
               'Geo::OSR' => 'osr_wrap.o' );

$object{'Geo::OGR'} = 'ogr_wrap.o' if $have_ogr;
$object{'Geo::GNM'} = 'gnm_wrap.o' if $have_gnm;

if ($ARGV{'--debug'}) {
    print "LIB = $LIB\n";
    print "INC = $INC\n";
}

for my $module (sort keys %object) {
    my $add = $module;
    $add =~ s/:/_/g;
    my $LD = $Config{ld};
    $LD .= ' '.$ENV{CFLAGS} if $ENV{CFLAGS};
    $LD .= ' '.$ENV{LDFLAGS} if $ENV{LDFLAGS};
    my $OPTIMIZE = '';
    $OPTIMIZE .= ' '.$ENV{CFLAGS} if $ENV{CFLAGS};
    $OPTIMIZE .= ' '.$ENV{CPPFLAGS} if $ENV{CFLAGS};
    
    my %PM = ( 'lib/Geo/GDAL.pm' => '$(INST_LIBDIR)/GDAL.pm',
               'lib/Geo/OSR.pm' => '$(INST_LIBDIR)/OSR.pm',
               'lib/Geo/GDAL/Const.pm' => '$(INST_LIBDIR)/GDAL/Const.pm' );
    
    $PM{'lib/Geo/OGR.pm'} = '$(INST_LIBDIR)/OGR.pm' if $have_ogr;
    $PM{'lib/Geo/GNM.pm'} = '$(INST_LIBDIR)/GNM.pm' if $have_gnm;

    WriteMakefile( NAME => $module,
                   VERSION_FROM => 'lib/Geo/GDAL.pm',
                   ABSTRACT_FROM => 'lib/Geo/GDAL.pm',
                   AUTHOR => 'Ari Jolma <ari.jolma at gmail.com>',
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
                   OPTIMIZE => $OPTIMIZE,
                   LD => $LD,
                   OBJECT => $object{$module},
                   PM => \%PM,
                   CONFIGURE_REQUIRES => {
                       'strict' => 0,
                       'warnings' => 0,
                       'File::Basename' => 0,
                       'ExtUtils::MakeMaker' => 0,
                       'Config' => 0,
                       'Cwd' => 0,
                       'File::Fetch' => 0,
                       'Capture::Tiny' => 0,
                   },
                   BUILD_REQUIRES => {
                       'strict' => 0,
                       'warnings' => 0,
                       'File::Basename' => 0,
                       'ExtUtils::MakeMaker' => 0,
                       'Config' => 0,
                       'File::Fetch' => 0,
                       'Capture::Tiny' => 0,
                       'Cwd' => 0 
                   },
                   PREREQ_PM => {
                       Carp => 0,
                       Encode => 0,
                       'Scalar::Util' => 0,
                       POSIX => 0 
                   },
                   TEST_REQUIRES => {
                       'Scalar::Util' => 0,
                       'Test::More' => 0,
                       'Encode' => 0,
                       POSIX => 0
                   }
        );
}

sub gdal_source_location {
    my $version = shift;
    return "http://download.osgeo.org/gdal/$version/gdal-$version.tar.gz";
}

sub download_and_compile_gdal {
    my ($version, $got_version) = @_;
    
    if (not defined $got_version) {
        print STDERR <<end;

I did not find GDAL development files. I am going to try to download
and compile GDAL for you. You can prevent this happening by a command
line argument --no-downloads or by setting the environment variable
PERL_GDAL_NO_DOWNLOADS to a true value, or by specifying a good
gdal-config with --gdal-config or the environment variable
PERL_GDAL_CONFIG. If the version of the GDAL development files is
different from what I have (it is $version) I will be back here.
end

    } else {
        print STDERR <<end;

The GDAL development files you gave or I found have a different
version ($got_version) than what these bindings were developed for
($version). I am going to try to download and compile GDAL for you.
You can prevent this happening by pointing me to good GDAL development
files, with a command line argument --no-downloads or by setting the
environment variable PERL_GDAL_NO_DOWNLOADS to a true value, or by
being adventurous by specifying --no-version-check or setting the
environment variable PERL_GDAL_NO_VERSION_CHECK.
end

}
    print STDERR <<end;

Note that this automatic download and compile is mostly meant for
automatic tests etc. It is recommended that you download and configure
GDAL by hand. This may take a very long time. I will capture the
output from the build and put it into gdal-build.log and
gdal-build.err. You can look into them while this goes on but the
output will be buffered.

end

{ 
    require File::Fetch;
    require Capture::Tiny;
    Capture::Tiny->import(':all');
    my $pwd = cwd();
    open(my $log, ">", "gdal-build.log") 
        or die "Can't open gdal-build.log: $!.";
    open(my $err, ">", "gdal-build.err") 
        or die "Can't open gdal-build.err: $!.";

    my $step = sub {
        my @cmd = @_;
        my $ret;
        print "@cmd\n";
        my $code = sub {
            if ($cmd[0] eq 'cd') {
                $ret = chdir($cmd[1]);
            } else {
                $ret = system(@cmd);
            }
        };
        my ($stdout, $stderr, $exit) = capture($code);
        print $err $stderr;
        print $log $stdout;
        return $cmd[0] eq 'cd' ? $ret : ($ret == 0);
    };

    my $gdal = "gdal-$version";
    my $uri = gdal_source_location($version);
    my $ok = 1;
    
    print "get GDAL version $version\n";
    my $code = sub {
        if ($version eq '2.0.0' or $version eq '2.0.1') {
            print "$version is buggy, get latest from 2.0 branch.\n";
            $ok = $step->("svn", "checkout", "https://svn.osgeo.org/gdal/branches/2.0/gdal", "gdal");
            $gdal = "gdal";
        } 
        elsif (-r "gdal-$version.tar.gz" and -s "gdal-$version.tar.gz") { # fetch leaves a zero sized file even when it fails
            print "Using existing \"$gdal.tar.gz\"\n";
            $ok = $step->("tar", "zxf", "$gdal.tar.gz");
        }
        else {
            print "Attempting to fetch '$uri'\n";
            my $ff = File::Fetch->new(uri => $uri);
            $ok = $ff->fetch() if $ff;
            if ($ok) {
                $ok = $step->("tar", "zxf", "$gdal.tar.gz");
            } else {
                print "No luck. Maybe this is the development version?\n";
                $ok = $step->("svn", "checkout", "https://svn.osgeo.org/gdal/trunk/gdal", "gdal");
                $gdal = "gdal";
            }
        }
    };
    my ($stdout, $stderr, $exit) = capture($code);
    print $err $stderr;
    print $log $stdout;
    if ($ok) {
        for my $cmd (["cd", $gdal],
                     ["./configure"],
                     [($^O =~ /bsd/i ? "gmake" : "make"), "-j4"],
                     ["cd", ".."]) 
        {
            $ok = $step->(@$cmd);
            last unless $ok;
        }
    }
    close $log;
    close $err;
    return "./$gdal" if $ok;
    chdir($pwd);
    open($err, "<", "gdal-build.err") 
        or die "Can't open gdal-build.err: $!.";
    my @err = <$err>;
    close $err;
    die "Downloading and compiling of GDAL failed.\n".
        "The error is probably explained by the error log.\n".
        "It is '$pwd/gdal-build.err'.\n".
        "@err\n";
}}

sub get_gdal_version {
    my $config = shift;
    my $version;
    if (-x $config) {
        chomp($version = `$config --version`);
    }
    else {
        if (open(my $fh, $config) || die "Can't open '$config': $!") {
            for (<$fh>) {
                ($version) = /(\d+\.\d+\.\d+)/ if /^CONFIG_VERSION/;
            }
            close $fh;
        }
        die "Can't find version from '$config'." unless $version;
    }
    return $version;
}

sub get_gdal_inc_lib {
    my $config = shift;
    my ($INC, $LIB) = ('', '');
    if ($source_tree) {
        $LIB = "-L$source_tree/.libs -L$source_tree -lgdal ";
        chomp($INC = `$config --cflags`);
        $INC .= " -I$source_tree/gnm " if $have_gnm;
    }
    elsif (-x $config) {
        chomp($INC = `$config --cflags`);
        chomp($LIB = `$config --libs`);
    }
    else {
        if (open(my $fh, $config) || die "Can't open '$config': $!") {
            for (<$fh>) {
                if (/^CONFIG_LIBS/) {
                    s/^CONFIG_LIBS="//;
                    s/"\s*$//;
                    if ($_ =~ /\.la$/) { 
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
    }
    return ($INC, $LIB);
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
