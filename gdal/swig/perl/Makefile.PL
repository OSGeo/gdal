use strict;
use warnings;
use File::Basename;
use ExtUtils::MakeMaker;
use Config;
use Cwd;

my $source_tree = '../..';
unless (-r "$source_tree/GDALmake.opt.in") {
    die "../../GDALmake.opt.in was not found. Is this the GDAL source tree?";
}

unless (-r "lib/Geo/GDAL.pm") {
    die "GDAL Perl modules not found, perhaps you need to run make generate?";
}

my $config = "$source_tree/apps/gdal-config";
unless (-r $config) {
    die "There is no gdal-config in '$source_tree'.\n".
        "You have to first say \"cd $source_tree; make\".";
}

# logic to set INSTALLSITEMAN3DIR if INSTALL_BASE and INST_MAN are set
# INSTALL_BASE is used in the standard case when this script is called
# as a part of overall build with --with-perl
# INST_MAN may be set by --mandir=DIR option to configure
if ($ARGV[0] =~ /INSTALL_BASE=(.*)/) {
    my $INSTALL_BASE = $1;
    my $opt = "$source_tree/GDALmake.opt";
    open(my $fh, "<", $opt) or die "Can't open < $opt: $!";
    while (<$fh>) {
        if (/(\w+)\s+=\s+(.*)/) {
            my $key = $1;
            my $val = $2;
            $val =~ s/\s+$//;
            if ($key eq 'prefix') {
                die 'prefix in GDALmake.opt is set different to \$INSTALL_BASE' unless $INSTALL_BASE eq $val;
            } elsif ($key eq 'INST_MAN') {
                $val =~ s/\$\{prefix\}/$INSTALL_BASE/;
                push @ARGV, "INSTALLSITEMAN1DIR=$val/man1";
                push @ARGV, "INSTALLSITEMAN3DIR=$val/man3";
            }
        }
    }
    close $fh;
}

my ($INC, $LIB, $objects) = get_config($config);

for my $module (sort keys %$objects) {
    my $add = $module;
    $add =~ s/:/_/g;
    my $LD = $Config{ld};
    $LD .= ' '.$ENV{CFLAGS} if $ENV{CFLAGS};
    $LD .= ' '.$ENV{LDFLAGS} if $ENV{LDFLAGS};
    my $OPTIMIZE = '';
    $OPTIMIZE .= ' '.$ENV{CFLAGS} if $ENV{CFLAGS};
    $OPTIMIZE .= ' '.$ENV{CPPFLAGS} if $ENV{CFLAGS};
    
    my %PM = ( 'lib/Geo/GDAL.pm' => '$(INST_LIBDIR)/GDAL.pm',
               'lib/Geo/OGR.pm' => '$(INST_LIBDIR)/OGR.pm',
               'lib/Geo/OSR.pm' => '$(INST_LIBDIR)/OSR.pm',
               'lib/Geo/GDAL/Const.pm' => '$(INST_LIBDIR)/GDAL/Const.pm' );
    
    $PM{'lib/Geo/GNM.pm'} = '$(INST_LIBDIR)/GNM.pm' if $objects->{'Geo::GNM'};

    WriteMakefile( NAME => $module,
                   VERSION_FROM => 'lib/Geo/GDAL.pm',
                   ABSTRACT => 'Perl extension for the GDAL library for geospatial data',
                   AUTHOR => 'Ari Jolma <ari.jolma at gmail.com>',
                   MAKEFILE => 'Makefile_'.$add,
                   LIBS => $LIB,
                   INC => $INC,
                   OPTIMIZE => $OPTIMIZE,
                   LD => $LD,
                   OBJECT => $objects->{$module},
                   PM => \%PM );
}

sub get_config {
    my $config = shift;
    my $INC = "-I$source_tree ";
    my $LIB = "-L$source_tree/.libs -L$source_tree -lgdal ";
    my $objects = { 
        'Geo::GDAL' => 'gdal_wrap.o',
        'Geo::OGR' => 'ogr_wrap.o',
        'Geo::GDAL::Const' => 'gdalconst_wrap.o',
        'Geo::OSR' => 'osr_wrap.o' };
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
            if (/CONFIG_GNM_ENABLED/ and /yes/) {
                $objects->{'Geo::GNM'} = 'gnm_wrap.o';
                $INC .= " -I$source_tree/gnm ";
            }
        }
        close $fh;
    }
    return ($INC, $LIB, $objects);
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
