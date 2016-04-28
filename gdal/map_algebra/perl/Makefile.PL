use strict;
use warnings;
use ExtUtils::MakeMaker;

my $source_tree;
my $have_gnm;
my $config = '/home/ajolma/usr/bin/gdal-config';

my ($INC, $LIB) = get_gdal_inc_lib($config);
$INC = "-I.. $INC";
$LIB = " -L.. -lgma $LIB -lstdc++";

WriteMakefile( 
    NAME => 'gma',
    VERSION_FROM => 'gma.pm',
    OPTIMIZE => '-g',
    LIBS => $LIB,
    INC => $INC,
    OBJECT => 'gma_wrap.o'
);

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
