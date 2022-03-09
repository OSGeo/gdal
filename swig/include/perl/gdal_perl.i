/******************************************************************************
 *
 * Project:  GDAL SWIG Interface declarations for Perl.
 * Purpose:  GDAL declarations.
 * Author:   Ari Jolma and Kevin Ruland
 *
 ******************************************************************************
 * Copyright (c) 2007, Ari Jolma and Kevin Ruland
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

%include init.i

%init %{
  /* gdal_perl.i %init code */
  UseExceptions();
  if ( GDALGetDriverCount() == 0 ) {
      GDALAllRegister();
  }
%}

%include callback.i
%include confess.i
%include cpl_exceptions.i

%include band.i

%rename (GetMetadata) GetMetadata_Dict;
%ignore GetMetadata_List;

%import typemaps_perl.i

%import destroy.i

ALTERED_DESTROY(GDALColorTableShadow, GDALc, delete_ColorTable)
ALTERED_DESTROY(GDALConstShadow, GDALc, delete_Const)
ALTERED_DESTROY(GDALDatasetShadow, GDALc, delete_Dataset)
ALTERED_DESTROY(GDALDriverShadow, GDALc, delete_Driver)
ALTERED_DESTROY(GDAL_GCP, GDALc, delete_GCP)
ALTERED_DESTROY(GDALMajorObjectShadow, GDALc, delete_MajorObject)
ALTERED_DESTROY(GDALRasterAttributeTableShadow, GDALc, delete_RasterAttributeTable)

/* remove unwanted name duplication */

%rename (X) GCPX;
%rename (Y) GCPY;
%rename (Z) GCPZ;
%rename (Column) GCPPixel;
%rename (Row) GCPLine;

/* Make room for Perl interface */

%rename (_FindFile) FindFile;

%rename (_Open) Open;
%newobject _Open;

%rename (_OpenShared) OpenShared;
%newobject _OpenShared;

%rename (_OpenEx) OpenEx;
%newobject _OpenEx;

%rename (_BuildOverviews) BuildOverviews;

%rename (_ReadRaster) ReadRaster;
%rename (_WriteRaster) WriteRaster;

%rename (_CreateLayer) CreateLayer;
%rename (_DeleteLayer) DeleteLayer;

%rename (_GetMaskFlags) GetMaskFlags;
%rename (_CreateMaskBand) CreateMaskBand;

%rename (_ReprojectImage) ReprojectImage;
%rename (_Polygonize) Polygonize;
%rename (_RegenerateOverviews) RegenerateOverviews;
%rename (_RegenerateOverview) RegenerateOverview;

%rename (_AutoCreateWarpedVRT) AutoCreateWarpedVRT;
%newobject _AutoCreateWarpedVRT;

%rename (_Create) Create;
%newobject _Create;

%rename (_CreateCopy) CreateCopy;
%newobject _CreateCopy;

%rename (_GetRasterBand) GetRasterBand;
%rename (_AddBand) AddBand;

%rename (_GetMaskBand) GetMaskBand;
%rename (_GetOverview) GetOverview;

%rename (_GetPaletteInterpretation) GetPaletteInterpretation;
%rename (_GetHistogram) GetHistogram;

%rename (_SetColorEntry) SetColorEntry;

%rename (_GetUsageOfCol) GetUsageOfCol;
%rename (_GetColOfUsage) GetColOfUsage;
%rename (_GetTypeOfCol) GetTypeOfCol;
%rename (_CreateColumn) CreateColumn;

%rename (Stat) VSIStatL;

%perlcode %{

package Geo::GDAL;
require 5.10.0; # we use //=
use strict;
use warnings;
use Carp;
use Encode;
use Exporter 'import';
use Geo::GDAL::Const;

# $VERSION is the Perl module (CPAN) version number, which must be
# an increasing floating point number.  $GDAL_VERSION is the
# version number of the GDAL that this module is a part of. It is
# used in build time to check the version of GDAL against which we
# build.
# For GDAL 2.0 or above, GDAL X.Y.Z should then
# VERSION = X + Y / 100.0 + Z / 10000.0
# Note also the $VERSION in ogr_perl.i (required by pause.perl.org)
# Note that the 1/100000 digits may be used to create more than one
# CPAN release from one GDAL release.

our $VERSION = '3.0300';
our $GDAL_VERSION = '3.3.0';

=pod

=head1 NAME

Geo::GDAL - Perl extension for the GDAL library for geospatial data

=head1 SYNOPSIS

  use Geo::GDAL;

  my $raster_file = shift @ARGV;

  my $raster_dataset = Geo::GDAL::Open($file);

  my $raster_data = $dataset->GetRasterBand(1)->ReadTile;

  my $vector_datasource = Geo::OGR::Open('./');

  my $vector_layer = $datasource->Layer('borders'); # e.g. a shapefile borders.shp in current directory

  $vector_layer->ResetReading();
  while (my $feature = $vector_layer->GetNextFeature()) {
      my $geometry = $feature->GetGeometry();
      my $value = $feature->GetField($field);
  }

=head1 DESCRIPTION

This Perl module lets you to manage (read, analyse, write) geospatial
data stored in several formats.

=head2 EXPORT

None by default.

=head1 SEE ALSO

The GDAL home page is L<http://gdal.org/>

The documentation of this module is written in Doxygen format. See
L<http://arijolma.org/Geo-GDAL/snapshot/>

=head1 AUTHOR

Ari Jolma

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005- by Ari Jolma and GDAL bindings developers.

This library is free software; you can redistribute it and/or modify
it under the terms of MIT License

L<https://opensource.org/licenses/MIT>

=head1 REPOSITORY

L<https://trac.osgeo.org/gdal>

=cut

unless ($ENV{GDAL_PERL_BINDINGS_OK}) {
    my $msg = "NOTE: GDAL Perl Bindings are deprecated and will be removed in version 3.5.\n";
    $msg .= "NOTE: Please use Geo::GDAL::FFI instead.\n";
    $msg .= "NOTE: To remove this message define environment variable GDAL_PERL_BINDINGS_OK.\n";
    warn $msg;
}

use Scalar::Util 'blessed';
use vars qw/
    @EXPORT_OK %EXPORT_TAGS
    @DATA_TYPES @OPEN_FLAGS @RESAMPLING_TYPES @RIO_RESAMPLING_TYPES @NODE_TYPES
    %S2I %I2S @error %stdout_redirection
    /;
BEGIN {
@EXPORT_OK = qw(
    Driver Open BuildVRT
    ParseXMLString NodeData Child Children NodeData ParseXMLString SerializeXMLTree
    error last_error named_parameters keep unkeep parent note unnote make_processing_options
    VSIStdoutSetRedirection VSIStdoutUnsetRedirection
    i2s s2i s_exists);
%EXPORT_TAGS = (
    all => [qw(Driver Open BuildVRT)],
    XML => [qw(ParseXMLString NodeData Child Children NodeData ParseXMLString SerializeXMLTree)],
    INTERNAL => [qw(error last_error named_parameters keep unkeep parent note unnote make_processing_options
                    VSIStdoutSetRedirection VSIStdoutUnsetRedirection i2s s2i s_exists)]
    );
}
*BuildVRT = *Geo::GDAL::Dataset::BuildVRT;
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@DATA_TYPES, $1), next if /^GDT_(\w+)/;
    push(@OPEN_FLAGS, $1), next if /^OF_(\w+)/;
    push(@RESAMPLING_TYPES, $1), next if /^GRA_(\w+)/;
    push(@RIO_RESAMPLING_TYPES, $1), next if /^GRIORA_(\w+)/;
    push(@NODE_TYPES, $1), next if /^CXT_(\w+)/;
}
for my $string (@DATA_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GDT_$string";
    $S2I{data_type}{$string} = $int;
    $I2S{data_type}{$int} = $string;
}
for my $string (@OPEN_FLAGS) {
    my $int = eval "\$Geo::GDAL::Const::OF_$string";
    $S2I{open_flag}{$string} = $int;
}
for my $string (@RESAMPLING_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GRA_$string";
    $S2I{resampling}{$string} = $int;
    $I2S{resampling}{$int} = $string;
}
for my $string (@RIO_RESAMPLING_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GRIORA_$string";
    $S2I{rio_resampling}{$string} = $int;
    $I2S{rio_resampling}{$int} = $string;
}
for my $string (@NODE_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::CXT_$string";
    $S2I{node_type}{$string} = $int;
    $I2S{node_type}{$int} = $string;
}

our $HAVE_PDL;
eval 'require PDL';
$HAVE_PDL = 1 unless $@;

sub error {
    if (@_) {
        my $error;
        if (@_ == 3) {
            my ($ecode, $offender, $ex) = @_;
            if ($ecode == 1) {
                my @k = sort keys %$ex;
                $error = "Unknown value: '$offender'. " if defined $offender;
                $error .= "Expected one of ".join(', ', @k).".";
            } elsif ($ecode == 2) {
                $error = "$ex not found: '$offender'.";
            } else {
                die("error in error: $ecode, $offender, $ex");
            }
        } else {
            $error = shift;
        }
        push @error, $error;
        $error = join("\n", reverse @error);
        confess($error."\n");
    }
    my @stack = @error;
    chomp(@stack);
    @error = ();
    return wantarray ? @stack : join("\n", reverse @stack);
}

sub last_error {
    my $error;
    # all errors should be in @error
    if (@error) {
        $error = $error[$#error];
    } elsif ($@) {
        # swig exceptions are not in @error
        $error = $@;
        $error =~ s/ at .*//;
    } else {
        $error = 'unknown error';
    }
    chomp($error);
    return $error;
}

sub errstr {
    my @stack = @error;
    chomp(@stack);
    @error = ();
    return join("\n", @stack);
}

# usage: named_parameters(\@_, key value list of default parameters);
# returns parameters in a hash with low-case-without-_ keys
sub named_parameters {
    my $parameters = shift;
    my %defaults = @_;
    my %c;
    for my $k (keys %defaults) {
        my $c = lc($k); $c =~ s/_//g;
        $c{$c} = $k;
    }
    my %named;
    my @p = ref($parameters->[0]) eq 'HASH' ? %{$parameters->[0]} : @$parameters;
    if (@p) {
        my $c = lc($p[0] // ''); $c =~ s/_//g;
        if (@p % 2 == 0 && defined $c && exists $c{$c}) {
            for (my $i = 0; $i < @p; $i+=2) {
                my $c = lc($p[$i]); $c =~ s/_//g;
                error(1, $p[$i], \%defaults) unless defined $c{$c} && exists $defaults{$c{$c}};
                $named{$c} = $p[$i+1];
            }
        } else {
            for (my $i = 0; $i < @p; $i++) {
                my $c = lc($_[$i*2]); $c =~ s/_//g;
                my $t = ref($defaults{$c{$c}});
                if (!blessed($p[$i]) and (ref($p[$i]) ne $t)) {
                    $t = $t eq '' ? 'SCALAR' : "a reference to $t";
                    error("parameter '$p[$i]' is not $t as it should for parameter $c{$c}.");
                }
                $named{$c} = $p[$i]; # $p[$i] could be a sub ...
            }
        }
    }
    for my $k (keys %defaults) {
        my $c = lc($k); $c =~ s/_//g;
        $named{$c} //= $defaults{$k};
    }
    return \%named;
}

sub i2s {
    my ($class, $int) = @_;
    return $I2S{$class}{$int} if exists $I2S{$class}{$int};
    return $int;
}

sub s2i {
    my ($class, $string, $backwards, $default) = @_;
    $string = $default if defined $default && !defined $string;
    return unless defined $string;
    return $string if $backwards && exists $I2S{$class}{$string};
    error(1, $string, $S2I{$class}) unless exists $S2I{$class}{$string};
    $S2I{$class}{$string};
}

sub s_exists {
    my ($class, $string) = @_;
    return exists $S2I{$class}{$string};
}

sub make_processing_options {
    my ($o) = @_;
    my @options;
    my $processor = sub {
        my $val = shift;
        if (ref $val eq 'ARRAY') {
            return @$val;
        } elsif (not ref $val) {
            if ($val =~ /\s/ && $val =~ /^[+\-0-9.,% ]+$/) {
                return split /\s+/, $val;
            }
            return $val;
        } else {
            error("'$val' is not a valid processing option.");
        }
    };
    if (ref $o eq 'HASH') {
        for my $key (keys %$o) {
            my $val = $o->{$key};
            # without hyphen is deprecated
            $key = '-'.$key unless $key =~ /^-/;
            push @options, $key;
            push @options, $processor->($val);
        }
    } elsif (ref $o eq 'ARRAY') {
        for my $item (@$o) {
            push @options, $processor->($item);
        }
    }
    $o = \@options;
    return $o;
}

sub RELEASE_PARENT {
}

sub FindFile {
    if (@_ == 1) {
        _FindFile('', @_);
    } else {
        _FindFile(@_);
    }
}

sub DataTypes {
    return @DATA_TYPES;
}

sub OpenFlags {
    return @DATA_TYPES;
}

sub ResamplingTypes {
    return @RESAMPLING_TYPES;
}

sub RIOResamplingTypes {
    return @RIO_RESAMPLING_TYPES;
}

sub NodeTypes {
    return @NODE_TYPES;
}

sub NodeType {
    my $type = shift;
    return i2s(node_type => $type) if $type =~ /^\d/;
    return s2i(node_type => $type);
}

sub NodeData {
    my $node = shift;
    return (NodeType($node->[0]), $node->[1]);
}

sub Children {
    my $node = shift;
    return @$node[2..$#$node];
}

sub Child {
    my($node, $child) = @_;
    return $node->[2+$child];
}

sub GetDataTypeSize {
    return _GetDataTypeSize(s2i(data_type => shift, 1));
}

sub DataTypeValueRange {
    my $t = shift;
    s2i(data_type => $t);
    # these values are from gdalrasterband.cpp
    return (0,255) if $t =~ /Byte/;
    return (0,65535) if $t =~/UInt16/;
    return (-32768,32767) if $t =~/Int16/;
    return (0,4294967295) if $t =~/UInt32/;
    return (-2147483648,2147483647) if $t =~/Int32/;
    return (-4294967295.0,4294967295.0) if $t =~/Float32/;
    return (-4294967295.0,4294967295.0) if $t =~/Float64/;
    return (-4294967295.0,4294967295.0) if $t =~/Float32/;
    return (-9223372036854775808,9223372036854775807) if $t =~/Int64/;
    return (0,18446744073709551615) if $t =~/UInt64/;
}

sub DataTypeIsComplex {
    return _DataTypeIsComplex(s2i(data_type => shift));
}

sub PackCharacter {
    my $t = shift;
    $t = i2s(data_type => $t);
    s2i(data_type => $t); # test
    my $is_big_endian = unpack("h*", pack("s", 1)) =~ /01/; # from Programming Perl
    return 'C' if $t =~ /^Byte$/;
    return ($is_big_endian ? 'n': 'v') if $t =~ /^UInt16$/;
    return 's' if $t =~ /^Int16$/;
    return ($is_big_endian ? 'N' : 'V') if $t =~ /^UInt32$/;
    return 'l' if $t =~ /^Int32$/;
    return 'f' if $t =~ /^Float32$/;
    return 'd' if $t =~ /^Float64$/;
}

sub GetDriverNames {
    my @names;
    for my $i (0..GetDriverCount()-1) {
        my $driver = GetDriver($i);
        push @names, $driver->Name if $driver->TestCapability('RASTER');
    }
    return @names;
}
*DriverNames = *GetDriverNames;

sub Drivers {
    my @drivers;
    for my $i (0..GetDriverCount()-1) {
        my $driver = GetDriver($i);
        push @drivers, $driver if $driver->TestCapability('RASTER');
    }
    return @drivers;
}

sub Driver {
    return 'Geo::GDAL::Driver' unless @_;
    my $name = shift;
    my $driver = GetDriver($name);
    error("Driver \"$name\" not found. Is it built in? Check with Geo::GDAL::Drivers or Geo::OGR::Drivers.")
        unless $driver;
    return $driver;
}

sub AccessTypes {
    return qw/ReadOnly Update/;
}

sub Open {
    my $p = named_parameters(\@_, Name => '.', Access => 'ReadOnly', Type => 'Any', Options => {}, Files => []);
    my @flags;
    my %o = (READONLY => 1, UPDATE => 1);
    error(1, $p->{access}, \%o) unless $o{uc($p->{access})};
    push @flags, uc($p->{access});
    %o = (RASTER => 1, VECTOR => 1, ANY => 1);
    error(1, $p->{type}, \%o) unless $o{uc($p->{type})};
    push @flags, uc($p->{type}) unless uc($p->{type}) eq 'ANY';
    my $dataset = OpenEx(Name => $p->{name}, Flags => \@flags, Options => $p->{options}, Files => $p->{files});
    unless ($dataset) {
        my $t = "Failed to open $p->{name}.";
        $t .= " Is it a ".lc($p->{type})." dataset?" unless uc($p->{type}) eq 'ANY';
        error($t);
    }
    return $dataset;
}

sub OpenShared {
    my @p = @_; # name, update
    my @flags = qw/RASTER SHARED/;
    $p[1] //= 'ReadOnly';
    error(1, $p[1], {ReadOnly => 1, Update => 1}) unless ($p[1] eq 'ReadOnly' or $p[1] eq 'Update');
    push @flags, qw/READONLY/ if $p[1] eq 'ReadOnly';
    push @flags, qw/UPDATE/ if $p[1] eq 'Update';
    my $dataset = OpenEx($p[0], \@flags);
    error("Failed to open $p[0]. Is it a raster dataset?") unless $dataset;
    return $dataset;
}

sub OpenEx {
    my $p = named_parameters(\@_, Name => '.', Flags => [], Drivers => [], Options => {}, Files => []);
    unless ($p) {
        my $name = shift // '';
        my @flags = @_;
        $p = {name => $name, flags => \@flags, drivers => [], options => {}, files => []};
    }
    if ($p->{flags}) {
        my $f = 0;
        for my $flag (@{$p->{flags}}) {
            $f |= s2i(open_flag => $flag);
        }
        $p->{flags} = $f;
    }
    return _OpenEx($p->{name}, $p->{flags}, $p->{drivers}, $p->{options}, $p->{files});
}

sub Polygonize {
    my @params = @_;
    $params[3] = $params[2]->GetLayerDefn->GetFieldIndex($params[3]) unless $params[3] =~ /^\d/;
    _Polygonize(@params);
}

sub RegenerateOverviews {
    my @p = @_;
    $p[2] = uc($p[2]) if $p[2]; # see overview.cpp:2030
    _RegenerateOverviews(@p);
}

sub RegenerateOverview {
    my @p = @_;
    $p[2] = uc($p[2]) if $p[2]; # see overview.cpp:2030
    _RegenerateOverview(@p);
}

sub ReprojectImage {
    my @p = @_;
    $p[4] = s2i(resampling => $p[4]);
    return _ReprojectImage(@p);
}

sub AutoCreateWarpedVRT {
    my @p = @_;
    for my $i (1..2) {
        if (defined $p[$i] and ref($p[$i])) {
            $p[$i] = $p[$i]->ExportToWkt;
        }
    }
    $p[3] = s2i(resampling => $p[3], undef, 'NearestNeighbour');
    return _AutoCreateWarpedVRT(@p);
}




package Geo::GDAL::MajorObject;
use strict;
use warnings;
use vars qw/@DOMAINS/;

sub Domains {
    return @DOMAINS;
}

sub Description {
    my($self, $desc) = @_;
    SetDescription($self, $desc) if defined $desc;
    GetDescription($self) if defined wantarray;
}

sub Metadata {
    my $self = shift,
    my $metadata = ref $_[0] ? shift : undef;
    my $domain = shift // '';
    SetMetadata($self, $metadata, $domain) if defined $metadata;
    GetMetadata($self, $domain) if defined wantarray;
}




package Geo::GDAL::Driver;
use strict;
use warnings;
use Carp;
use Scalar::Util 'blessed';

Geo::GDAL->import(qw(:XML :INTERNAL));

use vars qw/@CAPABILITIES @DOMAINS/;
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@CAPABILITIES, $1), next if /^DCAP_(\w+)/;
}

sub Domains {
    return @DOMAINS;
}

sub Name {
    my $self = shift;
    return $self->{ShortName};
}

sub Capabilities {
    my $self = shift;
    return @CAPABILITIES unless $self;
    my $h = $self->GetMetadata;
    my @cap;
    for my $cap (@CAPABILITIES) {
        my $test = $h->{'DCAP_'.uc($cap)};
        push @cap, $cap if defined($test) and $test eq 'YES';
    }
    return @cap;
}

sub TestCapability {
    my($self, $cap) = @_;
    my $h = $self->GetMetadata->{'DCAP_'.uc($cap)};
    return (defined($h) and $h eq 'YES') ? 1 : undef;
}

sub Extension {
    my $self = shift;
    my $h = $self->GetMetadata;
    if (wantarray) {
        my $e = $h->{DMD_EXTENSIONS};
        my @e = split / /, $e;
        @e = split /\//, $e if $e =~ /\//; # ILWIS returns mpr/mpl
        for my $i (0..$#e) {
            $e[$i] =~ s/^\.//; # CALS returns extensions with a dot prefix
        }
        return @e;
    } else {
        my $e = $h->{DMD_EXTENSION};
        return '' if $e =~ /\//; # ILWIS returns mpr/mpl
        $e =~ s/^\.//;
        return $e;
    }
}

sub MIMEType {
    my $self = shift;
    my $h = $self->GetMetadata;
    return $h->{DMD_MIMETYPE};
}

sub CreationOptionList {
    my $self = shift;
    my @options;
    my $h = $self->GetMetadata->{DMD_CREATIONOPTIONLIST};
    if ($h) {
        $h = ParseXMLString($h);
        my($type, $value) = NodeData($h);
        if ($value eq 'CreationOptionList') {
            for my $o (Children($h)) {
                my %option;
                for my $a (Children($o)) {
                    my(undef, $key) = NodeData($a);
                    my(undef, $value) = NodeData(Child($a, 0));
                    if ($key eq 'Value') {
                        push @{$option{$key}}, $value;
                    } else {
                        $option{$key} = $value;
                    }
                }
                push @options, \%option;
            }
        }
    }
    return @options;
}

sub CreationDataTypes {
    my $self = shift;
    my $h = $self->GetMetadata;
    return split /\s+/, $h->{DMD_CREATIONDATATYPES} if $h->{DMD_CREATIONDATATYPES};
}

sub stdout_redirection_wrapper {
    my ($self, $name, $sub, @params) = @_;
    my $object = 0;
    if ($name && blessed $name) {
        $object = $name;
        my $ref = $object->can('write');
        VSIStdoutSetRedirection($ref);
        $name = '/vsistdout/';
    }
    my $ds;
    eval {
        $ds = $sub->($self, $name, @params);
    };
    if ($object) {
        if ($ds) {
            $Geo::GDAL::stdout_redirection{tied(%$ds)} = $object;
        } else {
            VSIStdoutUnsetRedirection();
            $object->close;
        }
    }
    confess(last_error()) if $@;
    confess("Failed. Use Geo::OGR::Driver for vector drivers.") unless $ds;
    return $ds;
}

sub Create {
    my $self = shift;
    my $p = named_parameters(\@_, Name => 'unnamed', Width => 256, Height => 256, Bands => 1, Type => 'Byte', Options => {});
    my $type = s2i(data_type => $p->{type});
    return $self->stdout_redirection_wrapper(
        $p->{name},
        $self->can('_Create'),
        $p->{width}, $p->{height}, $p->{bands}, $type, $p->{options}
    );
}
*CreateDataset = *Create;

sub Copy {
    my $self = shift;
    my $p = named_parameters(\@_, Name => 'unnamed', Src => undef, Strict => 1, Options => {}, Progress => undef, ProgressData => undef);
    return $self->stdout_redirection_wrapper(
        $p->{name},
        $self->can('_CreateCopy'),
        $p->{src}, $p->{strict}, $p->{options}, $p->{progress}, $p->{progressdata});
}
*CreateCopy = *Copy;

sub Open {
    my $self = shift;
    my @p = @_; # name, update
    my @flags = qw/RASTER/;
    push @flags, qw/READONLY/ if $p[1] eq 'ReadOnly';
    push @flags, qw/UPDATE/ if $p[1] eq 'Update';
    my $dataset = OpenEx($p[0], \@flags, [$self->Name()]);
    error("Failed to open $p[0]. Is it a raster dataset?") unless $dataset;
    return $dataset;
}




package Geo::GDAL::Dataset;
use strict;
use warnings;
use POSIX qw/floor ceil/;
use Scalar::Util 'blessed';
use Carp;
use Exporter 'import';

Geo::GDAL->import(qw(:INTERNAL));

use vars qw/@EXPORT @DOMAINS @CAPABILITIES %CAPABILITIES/;

@EXPORT = qw/BuildVRT/;
@DOMAINS = qw/IMAGE_STRUCTURE SUBDATASETS GEOLOCATION/;

sub RELEASE_PARENT {
    my $self = shift;
    unkeep($self);
}

*Driver = *GetDriver;

sub Dataset {
    my $self = shift;
    parent($self);
}

sub Domains {
    return @DOMAINS;
}

*Open = *Geo::GDAL::Open;
*OpenShared = *Geo::GDAL::OpenShared;

sub TestCapability {
    return _TestCapability(@_);
}

sub Size {
    my $self = shift;
    return ($self->{RasterXSize}, $self->{RasterYSize});
}

sub Bands {
    my $self = shift;
    my @bands;
    for my $i (1..$self->{RasterCount}) {
        push @bands, GetRasterBand($self, $i);
    }
    return @bands;
}

sub GetRasterBand {
    my ($self, $index) = @_;
    $index //= 1;
    my $band = _GetRasterBand($self, $index);
    error(2, $index, 'Band') unless $band;
    keep($band, $self);
}
*Band = *GetRasterBand;

sub AddBand {
    my ($self, $type, $options) = @_;
    $type //= 'Byte';
    $type = s2i(data_type => $type);
    $self->_AddBand($type, $options);
    return unless defined wantarray;
    return $self->GetRasterBand($self->{RasterCount});
}

sub CreateMaskBand {
    return _CreateMaskBand(@_);
}

sub ExecuteSQL {
    my $self = shift;
    my $layer = $self->_ExecuteSQL(@_);
    note($layer, "is result set");
    keep($layer, $self);
}

sub ReleaseResultSet {
    # a no-op, _ReleaseResultSet is called from Layer::DESTROY
}

sub GetLayer {
    my($self, $name) = @_;
    my $layer = defined $name ? GetLayerByName($self, "$name") : GetLayerByIndex($self, 0);
    $name //= '';
    error(2, $name, 'Layer') unless $layer;
    keep($layer, $self);
}
*Layer = *GetLayer;

sub GetLayerNames {
    my $self = shift;
    my @names;
    for my $i (0..$self->GetLayerCount-1) {
        my $layer = GetLayerByIndex($self, $i);
        push @names, $layer->GetName;
    }
    return @names;
}
*Layers = *GetLayerNames;

sub CreateLayer {
    my $self = shift;
    my $p = named_parameters(\@_,
                             Name => 'unnamed',
                             SRS => undef,
                             GeometryType => 'Unknown',
                             Options => {},
                             Schema => undef,
                             Fields => undef,
                             ApproxOK => 1);
    error("The 'Fields' argument must be an array reference.") if $p->{fields} && ref($p->{fields}) ne 'ARRAY';
    if (defined $p->{schema}) {
        my $s = $p->{schema};
        $p->{geometrytype} = $s->{GeometryType} if exists $s->{GeometryType};
        $p->{fields} = $s->{Fields} if exists $s->{Fields};
        $p->{name} = $s->{Name} if exists $s->{Name};
    }
    $p->{fields} = [] unless ref($p->{fields}) eq 'ARRAY';
    # if fields contains spatial fields, then do not create default one
    for my $f (@{$p->{fields}}) {
        error("Field definitions must be hash references.") unless ref $f eq 'HASH';
        if ($f->{GeometryType} || ($f->{Type} && s_exists(geometry_type => $f->{Type}))) {
            $p->{geometrytype} = 'None';
            last;
        }
    }
    my $gt = s2i(geometry_type => $p->{geometrytype});
    my $layer = _CreateLayer($self, $p->{name}, $p->{srs}, $gt, $p->{options});
    for my $f (@{$p->{fields}}) {
        $layer->CreateField($f);
    }
    keep($layer, $self);
}

sub DeleteLayer {
    my ($self, $name) = @_;
    my $index;
    for my $i (0..$self->GetLayerCount-1) {
        my $layer = GetLayerByIndex($self, $i);
        $index = $i, last if $layer->GetName eq $name;
    }
    error(2, $name, 'Layer') unless defined $index;
    _DeleteLayer($self, $index);
}

sub Projection {
    my($self, $proj) = @_;
    SetProjection($self, $proj) if defined $proj;
    GetProjection($self) if defined wantarray;
}

sub SpatialReference {
    my($self, $sr) = @_;
    SetProjection($self, $sr->As('WKT')) if defined $sr;
    if (defined wantarray) {
        my $p = GetProjection($self);
        return unless $p;
        return Geo::OSR::SpatialReference->new(WKT => $p);
    }
}

sub GeoTransform {
    my $self = shift;
    eval {
        if (@_ == 1) {
            SetGeoTransform($self, $_[0]);
        } elsif (@_ > 1) {
            SetGeoTransform($self, \@_);
        }
    };
    confess(last_error()) if $@;
    return unless defined wantarray;
    my $t = GetGeoTransform($self);
    if (wantarray) {
        return @$t;
    } else {
        return Geo::GDAL::GeoTransform->new($t);
    }
}

sub Extent {
    my $self = shift;
    my $t = $self->GeoTransform;
    my $extent = $t->Extent($self->Size);
    if (@_) {
        my ($xoff, $yoff, $w, $h) = @_;
        my ($x, $y) = $t->Apply([$xoff, $xoff+$w, $xoff+$w, $xoff], [$yoff, $yoff, $yoff+$h, $yoff+$h]);
        my $xmin = shift @$x;
        my $xmax = $xmin;
        for my $x (@$x) {
            $xmin = $x if $x < $xmin;
            $xmax = $x if $x > $xmax;
        }
        my $ymin = shift @$y;
        my $ymax = $ymin;
        for my $y (@$y) {
            $ymin = $y if $y < $ymin;
            $ymax = $y if $y > $ymax;
        }
        $extent = Geo::GDAL::Extent->new($xmin, $ymin, $xmax, $ymax);
    }
    return $extent;
}

sub Tile { # $xoff, $yoff, $xsize, $ysize, assuming strict north up
    my ($self, $e) = @_;
    my ($w, $h) = $self->Size;
    my $t = $self->GeoTransform;
    confess "GeoTransform is not \"north up\"." unless $t->NorthUp;
    my $xoff = floor(($e->[0] - $t->[0])/$t->[1]);
    $xoff = 0 if $xoff < 0;
    my $yoff = floor(($e->[1] - $t->[3])/$t->[5]);
    $yoff = 0 if $yoff < 0;
    my $xsize = ceil(($e->[2] - $t->[0])/$t->[1]) - $xoff;
    $xsize = $w - $xoff if $xsize > $w - $xoff;
    my $ysize = ceil(($e->[3] - $t->[3])/$t->[5]) - $yoff;
    $ysize = $h - $yoff if $ysize > $h - $yoff;
    return ($xoff, $yoff, $xsize, $ysize);
}

sub GCPs {
    my $self = shift;
    if (@_ > 0) {
        my $proj = pop @_;
        $proj = $proj->Export('WKT') if $proj and ref($proj);
        SetGCPs($self, \@_, $proj);
    }
    return unless defined wantarray;
    my $proj = Geo::OSR::SpatialReference->new(GetGCPProjection($self));
    my $GCPs = GetGCPs($self);
    return (@$GCPs, $proj);
}

sub ReadTile {
    my ($self, $xoff, $yoff, $xsize, $ysize, $w_tile, $h_tile, $alg) = @_;
    my @data;
    for my $i (0..$self->Bands-1) {
        $data[$i] = $self->Band($i+1)->ReadTile($xoff, $yoff, $xsize, $ysize, $w_tile, $h_tile, $alg);
    }
    return \@data;
}

sub WriteTile {
    my ($self, $data, $xoff, $yoff) = @_;
    $xoff //= 0;
    $yoff //= 0;
    for my $i (0..$self->Bands-1) {
        $self->Band($i+1)->WriteTile($data->[$i], $xoff, $yoff);
    }
}

sub ReadRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->Band->DataType;
    my $p = named_parameters(\@_,
                             XOff => 0,
                             YOff => 0,
                             XSize => $width,
                             YSize => $height,
                             BufXSize => undef,
                             BufYSize => undef,
                             BufType => $type,
                             BandList => [1],
                             BufPixelSpace => 0,
                             BufLineSpace => 0,
                             BufBandSpace => 0,
                             ResampleAlg => 'NearestNeighbour',
                             Progress => undef,
                             ProgressData => undef
        );
    $p->{resamplealg} = s2i(rio_resampling => $p->{resamplealg});
    $p->{buftype} = s2i(data_type => $p->{buftype}, 1);
    $self->_ReadRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bandlist},$p->{bufpixelspace},$p->{buflinespace},$p->{bufbandspace},$p->{resamplealg},$p->{progress},$p->{progressdata});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->Band->DataType;
    my $p = named_parameters(\@_,
                             XOff => 0,
                             YOff => 0,
                             XSize => $width,
                             YSize => $height,
                             Buf => undef,
                             BufXSize => undef,
                             BufYSize => undef,
                             BufType => $type,
                             BandList => [1],
                             BufPixelSpace => 0,
                             BufLineSpace => 0,
                             BufBandSpace => 0
        );
    $p->{buftype} = s2i(data_type => $p->{buftype}, 1);
    $self->_WriteRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{buf},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bandlist},$p->{bufpixelspace},$p->{buflinespace},$p->{bufbandspace});
}

sub BuildOverviews {
    my $self = shift;
    my @p = @_;
    $p[0] = uc($p[0]) if $p[0];
    eval {
        $self->_BuildOverviews(@p);
    };
    confess(last_error()) if $@;
}

sub stdout_redirection_wrapper {
    my ($self, $name, $sub, @params) = @_;
    my $object = 0;
    if ($name && blessed $name) {
        $object = $name;
        my $ref = $object->can('write');
        VSIStdoutSetRedirection($ref);
        $name = '/vsistdout/';
    }
    my $ds;
    eval {
        $ds = $sub->($name, $self, @params); # self and name opposite to what is in Geo::GDAL::Driver!
    };
    if ($object) {
        if ($ds) {
            $Geo::GDAL::stdout_redirection{tied(%$ds)} = $object;
        } else {
            VSIStdoutUnsetRedirection();
            $object->close;
        }
    }
    confess(last_error()) if $@;
    return $ds;
}

sub DEMProcessing {
    my ($self, $dest, $Processing, $ColorFilename, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALDEMProcessingOptions->new(make_processing_options($options));
    return $self->stdout_redirection_wrapper(
        $dest,
        \&Geo::GDAL::wrapper_GDALDEMProcessing,
        $Processing, $ColorFilename, $options, $progress, $progress_data
    );
}

sub Nearblack {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALNearblackOptions->new(make_processing_options($options));
    my $b = blessed($dest);
    if ($b && $b eq 'Geo::GDAL::Dataset') {
        Geo::GDAL::wrapper_GDALNearblackDestDS($dest, $self, $options, $progress, $progress_data);
    } else {
        return $self->stdout_redirection_wrapper(
            $dest,
            \&Geo::GDAL::wrapper_GDALNearblackDestName,
            $options, $progress, $progress_data
        );
    }
}

sub Translate {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    return $self->stdout_redirection_wrapper(
        $dest,
        sub {
            my ($dest, $self) = @_;
            my $ds;
            if ($self->_GetRasterBand(1)) {
                $options = Geo::GDAL::GDALTranslateOptions->new(make_processing_options($options));
                $ds = Geo::GDAL::wrapper_GDALTranslate($dest, $self, $options, $progress, $progress_data);
            } else {
                $options = Geo::GDAL::GDALVectorTranslateOptions->new(make_processing_options($options));
                Geo::GDAL::wrapper_GDALVectorTranslateDestDS($dest, $self, $options, $progress, $progress_data);
                $ds = Geo::GDAL::wrapper_GDALVectorTranslateDestName($dest, $self, $options, $progress, $progress_data);
            }
            return $ds;
        }
    );
}

sub Warped {
    my $self = shift;
    my $p = named_parameters(\@_, SrcSRS => undef, DstSRS => undef, ResampleAlg => 'NearestNeighbour', MaxError => 0);
    for my $srs (qw/srcsrs dstsrs/) {
        $p->{$srs} = $p->{$srs}->ExportToWkt if $p->{$srs} && blessed $p->{$srs};
    }
    $p->{resamplealg} = s2i(resampling => $p->{resamplealg});
    my $warped = Geo::GDAL::_AutoCreateWarpedVRT($self, $p->{srcsrs}, $p->{dstsrs}, $p->{resamplealg}, $p->{maxerror});
    keep($warped, $self) if $warped; # self must live as long as warped
}

sub Warp {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    # can be run as object method (one dataset) and as package sub (a list of datasets)
    $options = Geo::GDAL::GDALWarpAppOptions->new(make_processing_options($options));
    my $b = blessed($dest);
    $self = [$self] unless ref $self eq 'ARRAY';
    if ($b && $b eq 'Geo::GDAL::Dataset') {
        Geo::GDAL::wrapper_GDALWarpDestDS($dest, $self, $options, $progress, $progress_data);
    } else {
        return stdout_redirection_wrapper(
            $self,
            $dest,
            \&Geo::GDAL::wrapper_GDALWarpDestName,
            $options, $progress, $progress_data
        );
    }
}

sub Info {
    my ($self, $o) = @_;
    $o = Geo::GDAL::GDALInfoOptions->new(make_processing_options($o));
    return GDALInfo($self, $o);
}

sub Grid {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALGridOptions->new(make_processing_options($options));
    return $self->stdout_redirection_wrapper(
        $dest,
        \&Geo::GDAL::wrapper_GDALGrid,
        $options, $progress, $progress_data
    );
}

sub Rasterize {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALRasterizeOptions->new(make_processing_options($options));
    my $b = blessed($dest);
    if ($b && $b eq 'Geo::GDAL::Dataset') {
        Geo::GDAL::wrapper_GDALRasterizeDestDS($dest, $self, $options, $progress, $progress_data);
    } else {
        # TODO: options need to force a new raster be made, otherwise segfault
        return $self->stdout_redirection_wrapper(
            $dest,
            \&Geo::GDAL::wrapper_GDALRasterizeDestName,
            $options, $progress, $progress_data
        );
    }
}

sub BuildVRT {
    my ($dest, $sources, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALBuildVRTOptions->new(make_processing_options($options));
    error("Usage: Geo::GDAL::DataSet::BuildVRT(\$vrt_file_name, \\\@sources)")
        unless ref $sources eq 'ARRAY' && defined $sources->[0];
    unless (blessed($dest)) {
        if (blessed($sources->[0])) {
            return Geo::GDAL::wrapper_GDALBuildVRT_objects($dest, $sources, $options, $progress, $progress_data);
        } else {
            return Geo::GDAL::wrapper_GDALBuildVRT_names($dest, $sources, $options, $progress, $progress_data);
        }
    } else {
        if (blessed($sources->[0])) {
            return stdout_redirection_wrapper(
                $sources, $dest,
                \&Geo::GDAL::wrapper_GDALBuildVRT_objects,
                $options, $progress, $progress_data);
        } else {
            return stdout_redirection_wrapper(
                $sources, $dest,
                \&Geo::GDAL::wrapper_GDALBuildVRT_names,
                $options, $progress, $progress_data);
        }
    }
}

sub ComputeColorTable {
    my $self = shift;
    my $p = named_parameters(\@_,
                             Red => undef,
                             Green => undef,
                             Blue => undef,
                             NumColors => 256,
                             Progress => undef,
                             ProgressData => undef,
                             Method => 'MedianCut');
    for my $b ($self->Bands) {
        for my $cion ($b->ColorInterpretation) {
            if ($cion eq 'RedBand') { $p->{red} //= $b; last; }
            if ($cion eq 'GreenBand') { $p->{green} //= $b; last; }
            if ($cion eq 'BlueBand') { $p->{blue} //= $b; last; }
        }
    }
    my $ct = Geo::GDAL::ColorTable->new;
    Geo::GDAL::ComputeMedianCutPCT($p->{red},
                                   $p->{green},
                                   $p->{blue},
                                   $p->{numcolors},
                                   $ct, $p->{progress},
                                   $p->{progressdata});
    return $ct;
}

sub Dither {
    my $self = shift;
    my $p = named_parameters(\@_,
                             Red => undef,
                             Green => undef,
                             Blue => undef,
                             Dest => undef,
                             ColorTable => undef,
                             Progress => undef,
                             ProgressData => undef);
    for my $b ($self->Bands) {
        for my $cion ($b->ColorInterpretation) {
            if ($cion eq 'RedBand') { $p->{red} //= $b; last; }
            if ($cion eq 'GreenBand') { $p->{green} //= $b; last; }
            if ($cion eq 'BlueBand') { $p->{blue} //= $b; last; }
        }
    }
    my ($w, $h) = $self->Size;
    $p->{dest} //= Geo::GDAL::Driver('MEM')->Create(Name => 'dithered',
                                                    Width => $w,
                                                    Height => $h,
                                                    Type => 'Byte')->Band;
    $p->{colortable}
        //= $p->{dest}->ColorTable
            // $self->ComputeColorTable(Red => $p->{red},
                                        Green => $p->{green},
                                        Blue => $p->{blue},
                                        Progress => $p->{progress},
                                        ProgressData => $p->{progressdata});
    Geo::GDAL::DitherRGB2PCT($p->{red},
                             $p->{green},
                             $p->{blue},
                             $p->{dest},
                             $p->{colortable},
                             $p->{progress},
                             $p->{progressdata});
    $p->{dest}->ColorTable($p->{colortable});
    return $p->{dest};
}




package Geo::GDAL::Band;
use strict;
use warnings;
use POSIX;
use Carp;
use Scalar::Util 'blessed';

Geo::GDAL->import(qw(:INTERNAL));

use vars qw/
    @COLOR_INTERPRETATIONS @DOMAINS
    %MASK_FLAGS %DATATYPE2PDL %PDL2DATATYPE
    /;

for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@COLOR_INTERPRETATIONS, $1), next if /^GCI_(\w+)/;
}
for my $string (@COLOR_INTERPRETATIONS) {
    my $int = eval "\$Geo::GDAL::Constc::GCI_$string";
    $Geo::GDAL::S2I{color_interpretation}{$string} = $int;
    $Geo::GDAL::I2S{color_interpretation}{$int} = $string;
}
@DOMAINS = qw/IMAGE_STRUCTURE RESAMPLING/;
%MASK_FLAGS = (AllValid => 1, PerDataset => 2, Alpha => 4, NoData => 8);
if ($Geo::GDAL::HAVE_PDL) {
    require PDL;
    require PDL::Types;
    %DATATYPE2PDL = (
        $Geo::GDAL::Const::GDT_Byte => $PDL::Types::PDL_B,
        $Geo::GDAL::Const::GDT_Int16 => $PDL::Types::PDL_S,
        $Geo::GDAL::Const::GDT_UInt16 => $PDL::Types::PDL_US,
        $Geo::GDAL::Const::GDT_Int32 => $PDL::Types::PDL_L,
        $Geo::GDAL::Const::GDT_UInt32 => -1,
        #$PDL_IND,
        #$PDL_LL,
        $Geo::GDAL::Const::GDT_Float32 => $PDL::Types::PDL_F,
        $Geo::GDAL::Const::GDT_Float64 => $PDL::Types::PDL_D,
        $Geo::GDAL::Const::GDT_CInt16 => -1,
        $Geo::GDAL::Const::GDT_CInt32 => -1,
        $Geo::GDAL::Const::GDT_CFloat32 => -1,
        $Geo::GDAL::Const::GDT_CFloat64 => -1
        );
    %PDL2DATATYPE = (
        $PDL::Types::PDL_B => $Geo::GDAL::Const::GDT_Byte,
        $PDL::Types::PDL_S => $Geo::GDAL::Const::GDT_Int16,
        $PDL::Types::PDL_US => $Geo::GDAL::Const::GDT_UInt16,
        $PDL::Types::PDL_L  => $Geo::GDAL::Const::GDT_Int32,
        $PDL::Types::PDL_IND  => -1,
        $PDL::Types::PDL_LL  => -1,
        $PDL::Types::PDL_F  => $Geo::GDAL::Const::GDT_Float32,
        $PDL::Types::PDL_D => $Geo::GDAL::Const::GDT_Float64
        );
}

sub Domains {
    return @DOMAINS;
}

sub ColorInterpretations {
    return @COLOR_INTERPRETATIONS;
}

sub MaskFlags {
    my @f = sort {$MASK_FLAGS{$a} <=> $MASK_FLAGS{$b}} keys %MASK_FLAGS;
    return @f;
}

sub DESTROY {
    my $self = shift;
    unless ($self->isa('SCALAR')) {
        return unless $self->isa('HASH');
        $self = tied(%{$self});
        return unless defined $self;
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENT;
}

sub RELEASE_PARENT {
    my $self = shift;
    unkeep($self);
}

sub Dataset {
    my $self = shift;
    parent($self);
}

sub Size {
    my $self = shift;
    return ($self->{XSize}, $self->{YSize});
}
*BlockSize = *GetBlockSize;

sub DataType {
    my $self = shift;
    return i2s(data_type => $self->{DataType});
}

sub PackCharacter {
    my $self = shift;
    return Geo::GDAL::PackCharacter($self->DataType);
}

sub NoDataValue {
    my $self = shift;
    if (@_ > 0) {
        if (defined $_[0]) {
            SetNoDataValue($self, $_[0]);
        } else {
            SetNoDataValue($self, POSIX::FLT_MAX); # hopefully an "out of range" value
        }
    }
    GetNoDataValue($self);
}

sub Unit {
    my $self = shift;
    if (@_ > 0) {
        my $unit = shift;
        $unit //= '';
        SetUnitType($self, $unit);
    }
    return unless defined wantarray;
    GetUnitType($self);
}

sub ScaleAndOffset {
    my $self = shift;
    SetScale($self, $_[0]) if @_ > 0 and defined $_[0];
    SetOffset($self, $_[1]) if @_ > 1 and defined $_[1];
    return unless defined wantarray;
    my $scale = GetScale($self);
    my $offset = GetOffset($self);
    return ($scale, $offset);
}

sub ReadTile {
    my($self, $xoff, $yoff, $xsize, $ysize, $w_tile, $h_tile, $alg) = @_;
    $xoff //= 0;
    $yoff //= 0;
    $xsize //= $self->{XSize} - $xoff;
    $ysize //= $self->{YSize} - $yoff;
    $w_tile //= $xsize;
    $h_tile //= $ysize;
    $alg //= 'NearestNeighbour';
    $alg = s2i(rio_resampling => $alg);
    my $t = $self->{DataType};
    my $buf = $self->_ReadRaster($xoff, $yoff, $xsize, $ysize, $w_tile, $h_tile, $t, 0, 0, $alg);
    my $pc = Geo::GDAL::PackCharacter($t);
    my $w = $w_tile * Geo::GDAL::GetDataTypeSize($t)/8;
    my $offset = 0;
    my @data;
    for my $y (0..$h_tile-1) {
        my @d = unpack($pc."[$w_tile]", substr($buf, $offset, $w));
        push @data, \@d;
        $offset += $w;
    }
    return \@data;
}

sub WriteTile {
    my($self, $data, $xoff, $yoff) = @_;
    $xoff //= 0;
    $yoff //= 0;
    error('The data must be in a two-dimensional array') unless ref $data eq 'ARRAY' && ref $data->[0] eq 'ARRAY';
    my $xsize = @{$data->[0]};
    if ($xsize > $self->{XSize} - $xoff) {
        warn "Buffer XSize too large ($xsize) for this raster band (width = $self->{XSize}, offset = $xoff).";
        $xsize = $self->{XSize} - $xoff;
    }
    my $ysize = @{$data};
    if ($ysize > $self->{YSize} - $yoff) {
        $ysize = $self->{YSize} - $yoff;
        warn "Buffer YSize too large ($ysize) for this raster band (height = $self->{YSize}, offset = $yoff).";
    }
    my $pc = Geo::GDAL::PackCharacter($self->{DataType});
    for my $i (0..$ysize-1) {
        my $scanline = pack($pc."[$xsize]", @{$data->[$i]});
        $self->WriteRaster( $xoff, $yoff+$i, $xsize, 1, $scanline );
    }
}

sub ColorInterpretation {
    my($self, $ci) = @_;
    if (defined $ci) {
        $ci = s2i(color_interpretation => $ci);
        SetRasterColorInterpretation($self, $ci);
    }
    return unless defined wantarray;
    i2s(color_interpretation => GetRasterColorInterpretation($self));
}

sub ColorTable {
    my $self = shift;
    SetRasterColorTable($self, $_[0]) if @_ and defined $_[0];
    return unless defined wantarray;
    GetRasterColorTable($self);
}

sub CategoryNames {
    my $self = shift;
    SetRasterCategoryNames($self, \@_) if @_;
    return unless defined wantarray;
    my $n = GetRasterCategoryNames($self);
    return @$n;
}

sub AttributeTable {
    my $self = shift;
    SetDefaultRAT($self, $_[0]) if @_ and defined $_[0];
    return unless defined wantarray;
    my $r = GetDefaultRAT($self);
    keep($r, $self) if $r;
}
*RasterAttributeTable = *AttributeTable;

sub GetHistogram {
    my $self = shift;
    my $p = named_parameters(\@_,
                             Min => -0.5,
                             Max => 255.5,
                             Buckets => 256,
                             IncludeOutOfRange => 0,
                             ApproxOK => 0,
                             Progress => undef,
                             ProgressData => undef);
    $p->{progressdata} = 1 if $p->{progress} and not defined $p->{progressdata};
    _GetHistogram($self, $p->{min}, $p->{max}, $p->{buckets},
                  $p->{includeoutofrange}, $p->{approxok},
                  $p->{progress}, $p->{progressdata});
}

sub Contours {
    my $self = shift;
    my $p = named_parameters(\@_,
                             DataSource => undef,
                             LayerConstructor => {Name => 'contours'},
                             ContourInterval => 100,
                             ContourBase => 0,
                             FixedLevels => [],
                             NoDataValue => undef,
                             IDField => -1,
                             ElevField => -1,
                             Progress => undef,
                             ProgressData => undef);
    $p->{datasource} //= Geo::OGR::GetDriver('Memory')->CreateDataSource('ds');
    $p->{layerconstructor}->{Schema} //= {};
    $p->{layerconstructor}->{Schema}{Fields} //= [];
    my %fields;
    unless ($p->{idfield} =~ /^[+-]?\d+$/ or $fields{$p->{idfield}}) {
        push @{$p->{layerconstructor}->{Schema}{Fields}}, {Name => $p->{idfield}, Type => 'Integer'};
    }
    unless ($p->{elevfield} =~ /^[+-]?\d+$/ or $fields{$p->{elevfield}}) {
        my $type = $self->DataType() =~ /Float/ ? 'Real' : 'Integer';
        push @{$p->{layerconstructor}->{Schema}{Fields}}, {Name => $p->{elevfield}, Type => $type};
    }
    my $layer = $p->{datasource}->CreateLayer($p->{layerconstructor});
    my $schema = $layer->GetLayerDefn;
    for ('idfield', 'elevfield') {
        $p->{$_} = $schema->GetFieldIndex($p->{$_}) unless $p->{$_} =~ /^[+-]?\d+$/;
    }
    $p->{progressdata} = 1 if $p->{progress} and not defined $p->{progressdata};
    ContourGenerate($self, $p->{contourinterval}, $p->{contourbase}, $p->{fixedlevels},
                    $p->{nodatavalue}, $layer, $p->{idfield}, $p->{elevfield},
                    $p->{progress}, $p->{progressdata});
    return $layer;
}

sub FillNodata {
    my $self = shift;
    my $mask = shift;
    $mask = $self->GetMaskBand unless $mask;
    my @p = @_;
    $p[0] //= 10;
    $p[1] //= 0;
    Geo::GDAL::FillNodata($self, $mask, @p);
}
*FillNoData = *FillNodata;
*GetBandNumber = *GetBand;

sub ReadRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->DataType;
    my $p = named_parameters(\@_,
                             XOff => 0,
                             YOff => 0,
                             XSize => $width,
                             YSize => $height,
                             BufXSize => undef,
                             BufYSize => undef,
                             BufType => $type,
                             BufPixelSpace => 0,
                             BufLineSpace => 0,
                             ResampleAlg => 'NearestNeighbour',
                             Progress => undef,
                             ProgressData => undef
        );
    $p->{resamplealg} = s2i(rio_resampling => $p->{resamplealg});
    $p->{buftype} = s2i(data_type => $p->{buftype}, 1);
    $self->_ReadRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bufpixelspace},$p->{buflinespace},$p->{resamplealg},$p->{progress},$p->{progressdata});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->DataType;
    my $p = named_parameters(\@_,
                             XOff => 0,
                             YOff => 0,
                             XSize => $width,
                             YSize => $height,
                             Buf => undef,
                             BufXSize => undef,
                             BufYSize => undef,
                             BufType => $type,
                             BufPixelSpace => 0,
                             BufLineSpace => 0
        );
    confess "Usage: \$band->WriteRaster( Buf => \$data, ... )" unless defined $p->{buf};
    $p->{buftype} = s2i(data_type => $p->{buftype}, 1);
    $self->_WriteRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{buf},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bufpixelspace},$p->{buflinespace});
}

sub GetMaskFlags {
    my $self = shift;
    my $f = $self->_GetMaskFlags;
    my @f;
    for my $flag (keys %MASK_FLAGS) {
        push @f, $flag if $f & $MASK_FLAGS{$flag};
    }
    return wantarray ? @f : $f;
}

sub CreateMaskBand {
    my $self = shift;
    my $f = 0;
    if (@_ and $_[0] =~ /^\d$/) {
        $f = shift;
    } else {
        for my $flag (@_) {
            carp "Unknown mask flag: '$flag'." unless $MASK_FLAGS{$flag};
            $f |= $MASK_FLAGS{$flag};
        }
    }
    $self->_CreateMaskBand($f);
}

sub Piddle {
    # TODO: add Piddle sub to dataset too to make Width x Height x Bands piddles
    error("PDL is not available.") unless $Geo::GDAL::HAVE_PDL;
    my $self = shift;
    my $t = $self->{DataType};
    unless (defined wantarray) {
        my $pdl = shift;
        error("The datatype of the Piddle and the band do not match.")
          unless $PDL2DATATYPE{$pdl->get_datatype} == $t;
        my ($xoff, $yoff, $xsize, $ysize) = @_;
        $xoff //= 0;
        $yoff //= 0;
        my $data = $pdl->get_dataref();
        my ($xdim, $ydim) = $pdl->dims();
        if ($xdim > $self->{XSize} - $xoff) {
            warn "Piddle XSize too large ($xdim) for this raster band (width = $self->{XSize}, offset = $xoff).";
            $xdim = $self->{XSize} - $xoff;
        }
        if ($ydim > $self->{YSize} - $yoff) {
            $ydim = $self->{YSize} - $yoff;
            warn "Piddle YSize too large ($ydim) for this raster band (height = $self->{YSize}, offset = $yoff).";
        }
        $xsize //= $xdim;
        $ysize //= $ydim;
        $self->_WriteRaster($xoff, $yoff, $xsize, $ysize, $data, $xdim, $ydim, $t, 0, 0);
        return;
    }
    my ($xoff, $yoff, $xsize, $ysize, $xdim, $ydim, $alg) = @_;
    $xoff //= 0;
    $yoff //= 0;
    $xsize //= $self->{XSize} - $xoff;
    $ysize //= $self->{YSize} - $yoff;
    $xdim //= $xsize;
    $ydim //= $ysize;
    $alg //= 'NearestNeighbour';
    $alg = s2i(rio_resampling => $alg);
    my $buf = $self->_ReadRaster($xoff, $yoff, $xsize, $ysize, $xdim, $ydim, $t, 0, 0, $alg);
    my $pdl = PDL->new;
    my $datatype = $DATATYPE2PDL{$t};
    error("The band datatype is not supported by PDL.") if $datatype < 0;
    $pdl->set_datatype($datatype);
    $pdl->setdims([$xdim, $ydim]);
    my $data = $pdl->get_dataref();
    $$data = $buf;
    $pdl->upd_data;
    # FIXME: we want approximate equality since no data value can be very large floating point value
    my $bad = GetNoDataValue($self);
    return $pdl->setbadif($pdl == $bad) if defined $bad;
    return $pdl;
}

sub GetMaskBand {
    my $self = shift;
    my $band = _GetMaskBand($self);
    keep($band, $self);
}

sub GetOverview {
    my ($self, $index) = @_;
    my $band = _GetOverview($self, $index);
    keep($band, $self);
}

sub RegenerateOverview {
    my $self = shift;
    #Geo::GDAL::Band overview, scalar resampling, subref callback, scalar callback_data
    my @p = @_;
    Geo::GDAL::RegenerateOverview($self, @p);
}

sub RegenerateOverviews {
    my $self = shift;
    #arrayref overviews, scalar resampling, subref callback, scalar callback_data
    my @p = @_;
    Geo::GDAL::RegenerateOverviews($self, @p);
}

sub Polygonize {
    my $self = shift;
    my $p = named_parameters(\@_, Mask => undef, OutLayer => undef, PixValField => 'val', Options => undef, Progress => undef, ProgressData => undef);
    my %known_options = (Connectedness => 1, ForceIntPixel => 1, DATASET_FOR_GEOREF => 1, '8CONNECTED' => 1);
    for my $option (keys %{$p->{options}}) {
        error(1, $option, \%known_options) unless exists $known_options{$option};
    }

    my $dt = $self->DataType;
    my %leInt32 = (Byte => 1, Int16 => 1, Int32 => 1, UInt16 => 1);
    my $leInt32 = $leInt32{$dt};
    $dt = $dt =~ /Float/ ? 'Real' : 'Integer';
    $p->{outlayer} //= Geo::OGR::Driver('Memory')->Create()->
        CreateLayer(Name => 'polygonized',
                    Fields => [{Name => 'val', Type => $dt},
                               {Name => 'geom', Type => 'Polygon'}]);
    $p->{pixvalfield} = $p->{outlayer}->GetLayerDefn->GetFieldIndex($p->{pixvalfield});
    $p->{options}{'8CONNECTED'} = 1 if $p->{options}{Connectedness} && $p->{options}{Connectedness} == 8;
    if ($leInt32 || $p->{options}{ForceIntPixel}) {
        Geo::GDAL::_Polygonize($self, $p->{mask}, $p->{outlayer}, $p->{pixvalfield}, $p->{options}, $p->{progress}, $p->{progressdata});
    } else {
        Geo::GDAL::FPolygonize($self, $p->{mask}, $p->{outlayer}, $p->{pixvalfield}, $p->{options}, $p->{progress}, $p->{progressdata});
    }
    set the srs of the outlayer if it was created here
    return $p->{outlayer};
}

sub Sieve {
    my $self = shift;
    my $p = named_parameters(\@_, Mask => undef, Dest => undef, Threshold => 10, Options => undef, Progress => undef, ProgressData => undef);
    unless ($p->{dest}) {
        my ($w, $h) = $self->Size;
        $p->{dest} = Geo::GDAL::Driver('MEM')->Create(Name => 'sieved', Width => $w, Height => $h, Type => $self->DataType)->Band;
    }
    my $c = 8;
    if ($p->{options}{Connectedness}) {
        $c = $p->{options}{Connectedness};
        delete $p->{options}{Connectedness};
    }
    Geo::GDAL::SieveFilter($self, $p->{mask}, $p->{dest}, $p->{threshold}, $c, $p->{options}, $p->{progress}, $p->{progressdata});
    return $p->{dest};
}

sub Distance {
    my $self = shift;
    my $p = named_parameters(\@_, Distance => undef, Options => undef, Progress => undef, ProgressData => undef);
    for my $key (keys %{$p->{options}}) {
        $p->{options}{uc($key)} = $p->{options}{$key};
    }
    $p->{options}{TYPE} //= $p->{options}{DATATYPE} //= 'Float32';
    unless ($p->{distance}) {
        my ($w, $h) = $self->Size;
        $p->{distance} = Geo::GDAL::Driver('MEM')->Create(Name => 'distance', Width => $w, Height => $h, Type => $p->{options}{TYPE})->Band;
    }
    Geo::GDAL::ComputeProximity($self, $p->{distance}, $p->{options}, $p->{progress}, $p->{progressdata});
    return $p->{distance};
}




package Geo::GDAL::ColorTable;
use strict;
use warnings;
use Carp;

Geo::GDAL->import(qw(:INTERNAL));

for (keys %Geo::GDAL::Const::) {
    if (/^GPI_(\w+)/) {
        my $int = eval "\$Geo::GDAL::Const::GPI_$1";
        $Geo::GDAL::S2I{palette_interpretation}{$1} = $int;
        $Geo::GDAL::I2S{palette_interpretation}{$int} = $1;
    }
}
%}

%feature("shadow") GDALColorTableShadow(GDALPaletteInterp palette = GPI_RGB)
%{
use Carp;
sub new {
    my($pkg, $pi) = @_;
    $pi //= 'RGB';
    $pi = s2i(palette_interpretation => $pi);
    my $self = Geo::GDALc::new_ColorTable($pi);
    bless $self, $pkg if defined($self);
}
%}

%perlcode %{
sub GetPaletteInterpretation {
    my $self = shift;
    return i2s(palette_interpretation => GetPaletteInterpretation($self));
}

sub SetColorEntry {
    my $self = shift;
    my $index = shift;
    my $color;
    if (ref($_[0]) eq 'ARRAY') {
        $color = shift;
    } else {
        $color = [@_];
    }
    eval {
        $self->_SetColorEntry($index, $color);
    };
    confess(last_error()) if $@;
}

sub ColorEntry {
    my $self = shift;
    my $index = shift // 0;
    SetColorEntry($self, $index, @_) if @_;
    return unless defined wantarray;
    return wantarray ? GetColorEntry($self, $index) : [GetColorEntry($self, $index)];
}
*Color = *ColorEntry;

sub ColorTable {
    my $self = shift;
    if (@_) {
        my $index = 0;
        for my $color (@_) {
            ColorEntry($self, $index, $color);
            $index++;
        }
    }
    return unless defined wantarray;
    my @table;
    for (my $index = 0; $index < GetCount($self); $index++) {
        push @table, [ColorEntry($self, $index)];
    }
    return @table;
}
*ColorEntries = *ColorTable;
*Colors = *ColorTable;




package Geo::GDAL::RasterAttributeTable;
use strict;
use warnings;
use Carp;

Geo::GDAL->import(qw(:INTERNAL));

use vars qw(@FIELD_TYPES @FIELD_USAGES);
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@FIELD_TYPES, $1), next if /^GFT_(\w+)/;
    push(@FIELD_USAGES, $1), next if /^GFU_(\w+)/;
}
for my $string (@FIELD_TYPES) {
    my $int = eval "\$Geo::GDAL::Constc::GFT_$string";
    $Geo::GDAL::S2I{rat_field_type}{$string} = $int;
    $Geo::GDAL::I2S{rat_field_type}{$int} = $string;
}
for my $string (@FIELD_USAGES) {
    my $int = eval "\$Geo::GDAL::Constc::GFU_$string";
    $Geo::GDAL::S2I{rat_field_usage}{$string} = $int;
    $Geo::GDAL::I2S{rat_field_usage}{$int} = $string;
}

sub FieldTypes {
    return @FIELD_TYPES;
}

sub FieldUsages {
    return @FIELD_USAGES;
}

sub RELEASE_PARENT {
    my $self = shift;
    unkeep($self);
}

sub Band {
    my $self = shift;
    parent($self);
}

sub GetUsageOfCol {
    my($self, $col) = @_;
    i2s(rat_field_usage => _GetUsageOfCol($self, $col));
}

sub GetColOfUsage {
    my($self, $usage) = @_;
    _GetColOfUsage($self, s2i(rat_field_usage => $usage));
}

sub GetTypeOfCol {
    my($self, $col) = @_;
    i2s(rat_field_type => _GetTypeOfCol($self, $col));
}

sub Columns {
    my $self = shift;
    my %columns;
    if (@_) { # create columns
        %columns = @_;
        for my $name (keys %columns) {
            $self->CreateColumn($name, $columns{$name}{Type}, $columns{$name}{Usage});
        }
    }
    %columns = ();
    for my $c (0..$self->GetColumnCount-1) {
        my $name = $self->GetNameOfCol($c);
        $columns{$name}{Type} = $self->GetTypeOfCol($c);
        $columns{$name}{Usage} = $self->GetUsageOfCol($c);
    }
    return %columns;
}

sub CreateColumn {
    my($self, $name, $type, $usage) = @_;
    for my $color (qw/Red Green Blue Alpha/) {
        carp "RAT column type will be 'Integer' for usage '$color'." if $usage eq $color and $type ne 'Integer';
    }
    $type = s2i(rat_field_type => $type);
    $usage = s2i(rat_field_usage => $usage);
    _CreateColumn($self, $name, $type, $usage);
}

sub Value {
    my($self, $row, $column) = @_;
    SetValueAsString($self, $row, $column, $_[3]) if defined $_[3];
    return unless defined wantarray;
    GetValueAsString($self, $row, $column);
}

sub LinearBinning {
    my $self = shift;
    SetLinearBinning($self, @_) if @_ > 0;
    return unless defined wantarray;
    my @a = GetLinearBinning($self);
    return $a[0] ? ($a[1], $a[2]) : ();
}




package Geo::GDAL::GCP;

*swig_Pixel_get = *Geo::GDALc::GCP_Column_get;
*swig_Pixel_set = *Geo::GDALc::GCP_Column_set;
*swig_Line_get = *Geo::GDALc::GCP_Row_get;
*swig_Line_set = *Geo::GDALc::GCP_Row_set;

Geo::GDAL->import(qw(:INTERNAL));

package Geo::GDAL::VSIF;
use strict;
use warnings;
use Carp;
require Exporter;
our @ISA = qw(Exporter);

our @EXPORT_OK   = qw(Open Close Write Read Seek Tell Truncate MkDir ReadDir ReadDirRecursive Rename RmDir Stat Unlink);
our %EXPORT_TAGS = (all => \@EXPORT_OK);

Geo::GDAL->import(qw(:INTERNAL));

sub Open {
    my ($path, $mode) = @_;
    my $self = Geo::GDAL::VSIFOpenL($path, $mode);
    bless $self, 'Geo::GDAL::VSIF';
}

sub Write {
    my ($self, $data) = @_;
    Geo::GDAL::VSIFWriteL($data, $self);
}

sub Close {
    my ($self) = @_;
    Geo::GDAL::VSIFCloseL($self);
}

sub Read {
    my ($self, $count) = @_;
    Geo::GDAL::VSIFReadL($count, $self);
}

sub Seek {
    my ($self, $offset, $whence) = @_;
    Geo::GDAL::VSIFSeekL($self, $offset, $whence);
}

sub Tell {
    my ($self) = @_;
    Geo::GDAL::VSIFTellL($self);
}

sub Flush {
    my ($self) = @_;
    Geo::GDAL::VSIFFlushL($self);
}

sub Truncate {
    my ($self, $new_size) = @_;
    Geo::GDAL::VSIFTruncateL($self, $new_size);
}

sub MkDir {
    my ($path) = @_;
    # mode unused in CPL
    Geo::GDAL::Mkdir($path, 0);
}
*Mkdir = *MkDir;

sub ReadDir {
    my ($path) = @_;
    Geo::GDAL::ReadDir($path);
}

sub ReadDirRecursive {
    my ($path) = @_;
    Geo::GDAL::ReadDirRecursive($path);
}

sub Rename {
    my ($old, $new) = @_;
    Geo::GDAL::Rename($old, $new);
}

sub RmDir {
    my ($dirname, $recursive) = @_;
    eval {
        if (!$recursive) {
            Geo::GDAL::Rmdir($dirname);
        } else {
            for my $f (ReadDir($dirname)) {
                next if $f eq '..' or $f eq '.';
                my @s = Stat($dirname.'/'.$f);
                if ($s[0] eq 'f') {
                    Unlink($dirname.'/'.$f);
                } elsif ($s[0] eq 'd') {
                    Rmdir($dirname.'/'.$f, 1);
                    Rmdir($dirname.'/'.$f);
                }
            }
            RmDir($dirname);
        }
    };
    if ($@) {
        my $r = $recursive ? ' recursively' : '';
        error("Cannot remove directory \"$dirname\"$r.");
    }
}
*Rmdir = *RmDir;

sub Stat {
    my ($path) = @_;
    Geo::GDAL::Stat($path);
}

sub Unlink {
    my ($filename) = @_;
    Geo::GDAL::Unlink($filename);
}




package Geo::GDAL::GeoTransform;
use strict;
use warnings;
use Carp;
use Scalar::Util 'blessed';

Geo::GDAL->import(qw(:INTERNAL));

sub new {
    my $class = shift;
    my $self;
    if (@_ == 0) {
        $self = [0,1,0,0,0,1];
    } elsif (ref $_[0]) {
        @$self = @{$_[0]};
    } elsif ($_[0] =~ /^[a-zA-Z]/i) {
        my $p = named_parameters(\@_, GCPs => undef, ApproxOK => 1, Extent => undef, CellSize => 1);
        if ($p->{gcps}) {
            $self = Geo::GDAL::GCPsToGeoTransform($p->{gcps}, $p->{approxok});
        } elsif ($p->{extent}) {
            $self = Geo::GDAL::GeoTransform->new($p->{extent}[0], $p->{cellsize}, 0, $p->{extent}[2], 0, -$p->{cellsize});
        } else {
            error("Missing GCPs or Extent");
        }
    } else {
        my @a = @_;
        $self = \@a;
    }
    bless $self, $class;
}

sub NorthUp {
    my $self = shift;
    return $self->[2] == 0 && $self->[4] == 0;
}

sub FromGCPs {
    my $gcps;
    my $p = shift;
    if (ref $p eq 'ARRAY') {
        $gcps = $p;
    } else {
        $gcps = [];
        while ($p && blessed $p) {
            push @$gcps, $p;
            $p = shift;
        }
    }
    my $approx_ok = shift // 1;
    error('Usage: Geo::GDAL::GeoTransform::FromGCPs(\@gcps, $approx_ok)') unless @$gcps;
    my $self = Geo::GDAL::GCPsToGeoTransform($gcps, $approx_ok);
    bless $self, 'Geo::GDAL::GetTransform';
    return $self;
}

sub Apply {
    my ($self, $columns, $rows) = @_;
    return Geo::GDAL::ApplyGeoTransform($self, $columns, $rows) unless ref($columns) eq 'ARRAY';
    my (@x, @y);
    for my $i (0..$#$columns) {
        ($x[$i], $y[$i]) =
            Geo::GDAL::ApplyGeoTransform($self, $columns->[$i], $rows->[$i]);
    }
    return (\@x, \@y);
}

sub Inv {
    my $self = shift;
    my @inv = Geo::GDAL::InvGeoTransform($self);
    return Geo::GDAL::GeoTransform->new(@inv) if defined wantarray;
    @$self = @inv;
}

sub Extent {
    my ($self, $w, $h) = @_;
    my $e = Geo::GDAL::Extent->new($self->[0], $self->[3], $self->[0], $self->[3]);
    for my $x ($self->[0] + $self->[1]*$w, $self->[0] + $self->[2]*$h, $self->[0] + $self->[1]*$w + $self->[2]*$h) {
        $e->[0] = $x if $x < $e->[0];
        $e->[2] = $x if $x > $e->[2];
    }
    for my $y ($self->[3] + $self->[4]*$w, $self->[3] + $self->[5]*$h, $self->[3] + $self->[4]*$w + $self->[5]*$h) {
        $e->[1] = $y if $y < $e->[1];
        $e->[3] = $y if $y > $e->[3];
    }
    return $e;
}

package Geo::GDAL::Extent; # array 0=xmin|left, 1=ymin|bottom, 2=xmax|right, 3=ymax|top

use strict;
use warnings;
use Carp;
use Scalar::Util 'blessed';

Geo::GDAL->import(qw(:INTERNAL));

sub new {
    my $class = shift;
    my $self;
    if (@_ == 0) {
        $self = [0,0,-1,0];
    } elsif (ref $_[0]) {
        @$self = @{$_[0]};
    } else {
        @$self = @_;
    }
    bless $self, $class;
    return $self;
}

sub IsEmpty {
    my $self = shift;
    return $self->[2] < $self->[0];
}

sub Size {
    my $self = shift;
    return (0,0) if $self->IsEmpty;
    return ($self->[2] - $self->[0], $self->[3] - $self->[1]);
}

sub Overlaps {
    my ($self, $e) = @_;
    return $self->[0] < $e->[2] && $self->[2] > $e->[0] && $self->[1] < $e->[3] && $self->[3] > $e->[1];
}

sub Overlap {
    my ($self, $e) = @_;
    return Geo::GDAL::Extent->new() unless $self->Overlaps($e);
    my $ret = Geo::GDAL::Extent->new($self);
    $ret->[0] = $e->[0] if $self->[0] < $e->[0];
    $ret->[1] = $e->[1] if $self->[1] < $e->[1];
    $ret->[2] = $e->[2] if $self->[2] > $e->[2];
    $ret->[3] = $e->[3] if $self->[3] > $e->[3];
    return $ret;
}

sub ExpandToInclude {
    my ($self, $e) = @_;
    return if $e->IsEmpty;
    if ($self->IsEmpty) {
        @$self = @$e;
    } else {
        $self->[0] = $e->[0] if $e->[0] < $self->[0];
        $self->[1] = $e->[1] if $e->[1] < $self->[1];
        $self->[2] = $e->[2] if $e->[2] > $self->[2];
        $self->[3] = $e->[3] if $e->[3] > $self->[3];
    }
}

package Geo::GDAL::XML;

use strict;
use warnings;
use Carp;

Geo::GDAL->import(qw(:INTERNAL));

# XML related subs in Geo::GDAL

#Geo::GDAL::Child
#Geo::GDAL::Children
#Geo::GDAL::NodeData
#Geo::GDAL::NodeType
#Geo::GDAL::NodeTypes
#Geo::GDAL::ParseXMLString
#Geo::GDAL::SerializeXMLTree

sub new {
    my $class = shift;
    my $xml = shift // '';
    my $self = ParseXMLString($xml);
    bless $self, $class;
    $self->traverse(sub {my $node = shift; bless $node, $class});
    return $self;
}

sub traverse {
    my ($self, $sub) = @_;
    my $type = $self->[0];
    my $data = $self->[1];
    $type = NodeType($type);
    $sub->($self, $type, $data);
    for my $child (@{$self}[2..$#$self]) {
        traverse($child, $sub);
    }
}

sub serialize {
    my $self = shift;
    return SerializeXMLTree($self);
}

%}
