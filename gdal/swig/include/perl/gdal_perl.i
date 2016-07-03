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
use Geo::OGR;
use Geo::OSR;
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

our $VERSION = '2.0101';
our $GDAL_VERSION = '2.1.1';

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
L<http://ajolma.net/Geo-GDAL/snapshot/>

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

use Scalar::Util 'blessed';
use vars qw/
    @EXPORT_OK %EXPORT_TAGS
    @DATA_TYPES @OPEN_FLAGS @RESAMPLING_TYPES @RIO_RESAMPLING_TYPES @NODE_TYPES
    %TYPE_STRING2INT %TYPE_INT2STRING
    %OF_STRING2INT
    %RESAMPLING_STRING2INT %RESAMPLING_INT2STRING
    %RIO_RESAMPLING_STRING2INT %RIO_RESAMPLING_INT2STRING
    %NODE_TYPE_STRING2INT %NODE_TYPE_INT2STRING
    @error %stdout_redirection
    /;
@EXPORT_OK = qw/Driver Open BuildVRT/;
%EXPORT_TAGS = (all => [qw(Driver Open BuildVRT)]);
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
    $TYPE_STRING2INT{$string} = $int;
    $TYPE_INT2STRING{$int} = $string;
}
for my $string (@OPEN_FLAGS) {
    my $int = eval "\$Geo::GDAL::Const::OF_$string";
    $OF_STRING2INT{$string} = $int;
}
for my $string (@RESAMPLING_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GRA_$string";
    $RESAMPLING_STRING2INT{$string} = $int;
    $RESAMPLING_INT2STRING{$int} = $string;
}
for my $string (@RIO_RESAMPLING_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GRIORA_$string";
    $RIO_RESAMPLING_STRING2INT{$string} = $int;
    $RIO_RESAMPLING_INT2STRING{$int} = $string;
}
for my $string (@NODE_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::CXT_$string";
    $NODE_TYPE_STRING2INT{$string} = $int;
    $NODE_TYPE_INT2STRING{$int} = $string;
}

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
        confess($error);
    }
    my @stack = @error;
    chomp(@stack);
    @error = ();
    return wantarray ? @stack : join("\n", @stack);
}

sub last_error {
    my $error = $error[$#error] // '';
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

sub string2int {
    my ($string, $string2int_hash, $int2string_hash, $default) = @_;
    $string = $default if defined $default && !defined $string;
    return unless defined $string;
    return $string if $int2string_hash && exists $int2string_hash->{$string};
    error(1, $string, $string2int_hash) unless exists $string2int_hash->{$string};
    $string2int_hash->{$string};
}

sub RELEASE_PARENTS {
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
    return $NODE_TYPE_INT2STRING{$type} if $type =~ /^\d/;
    return $NODE_TYPE_STRING2INT{$type};
}

sub NodeData {
    my $node = shift;
    return (Geo::GDAL::NodeType($node->[0]), $node->[1]);
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
    return _GetDataTypeSize(string2int(shift, \%TYPE_STRING2INT, \%TYPE_INT2STRING));
}

sub DataTypeValueRange {
    my $t = shift;
    Geo::GDAL::error(1, $t, \%TYPE_STRING2INT) unless exists $TYPE_STRING2INT{$t};
    # these values are from gdalrasterband.cpp
    return (0,255) if $t =~ /Byte/;
    return (0,65535) if $t =~/UInt16/;
    return (-32768,32767) if $t =~/Int16/;
    return (0,4294967295) if $t =~/UInt32/;
    return (-2147483648,2147483647) if $t =~/Int32/;
    return (-4294967295.0,4294967295.0) if $t =~/Float32/;
    return (-4294967295.0,4294967295.0) if $t =~/Float64/;
}

sub DataTypeIsComplex {
    return _DataTypeIsComplex(string2int(shift, \%TYPE_STRING2INT));
}

sub PackCharacter {
    my $t = shift;
    $t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
    Geo::GDAL::error(1, $t, \%TYPE_STRING2INT) unless exists $TYPE_STRING2INT{$t};
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
    return GetDriver(@_);
}

sub AccessTypes {
    return qw/ReadOnly Update/;
}

sub Open {
    my $p = Geo::GDAL::named_parameters(\@_, Name => '.', Access => 'ReadOnly', Type => 'Any', Options => {}, Files => []);
    my @flags;
    my %o = (READONLY => 1, UPDATE => 1);
    Geo::GDAL::error(1, $p->{access}, \%o) unless $o{uc($p->{access})};
    push @flags, uc($p->{access});
    %o = (RASTER => 1, VECTOR => 1, ANY => 1);
    Geo::GDAL::error(1, $p->{type}, \%o) unless $o{uc($p->{type})};
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
    Geo::GDAL::error(1, $p[1], {ReadOnly => 1, Update => 1}) unless ($p[1] eq 'ReadOnly' or $p[1] eq 'Update');
    push @flags, qw/READONLY/ if $p[1] eq 'ReadOnly';
    push @flags, qw/UPDATE/ if $p[1] eq 'Update';
    my $dataset = OpenEx($p[0], \@flags);
    error("Failed to open $p[0]. Is it a raster dataset?") unless $dataset;
    return $dataset;
}

sub OpenEx {
    my $p = Geo::GDAL::named_parameters(\@_, Name => '.', Flags => [], Drivers => [], Options => {}, Files => []);
    unless ($p) {
        my $name = shift // '';
        my @flags = @_;
        $p = {name => $name, flags => \@flags, drivers => [], options => {}, files => []};
    }
    if ($p->{flags}) {
        my $f = 0;
        for my $flag (@{$p->{flags}}) {
            Geo::GDAL::error(1, $flag, \%OF_STRING2INT) unless exists $OF_STRING2INT{$flag};
            $f |= $Geo::GDAL::OF_STRING2INT{$flag};
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
    $p[4] = string2int($p[4], \%RESAMPLING_STRING2INT);
    return _ReprojectImage(@p);
}

sub AutoCreateWarpedVRT {
    my @p = @_;
    for my $i (1..2) {
        if (defined $p[$i] and ref($p[$i])) {
            $p[$i] = $p[$i]->ExportToWkt;
        }
    }
    $p[3] = string2int($p[3], \%RESAMPLING_STRING2INT, undef, 'NearestNeighbour');
    return _AutoCreateWarpedVRT(@p);
}

sub make_processing_options {
    my ($o) = @_;
    if (ref $o eq 'HASH') {
        for my $key (keys %$o) {
            unless ($key =~ /^-/) {
                $o->{'-'.$key} = $o->{$key};
                delete $o->{$key};
            }
        }
        $o = [%$o];
    }
    return $o;
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
    return $h->{DMD_EXTENSION};
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
        $h = Geo::GDAL::ParseXMLString($h);
        my($type, $value) = Geo::GDAL::NodeData($h);
        if ($value eq 'CreationOptionList') {
            for my $o (Geo::GDAL::Children($h)) {
                my %option;
                for my $a (Geo::GDAL::Children($o)) {
                    my(undef, $key) = Geo::GDAL::NodeData($a);
                    my(undef, $value) = Geo::GDAL::NodeData(Geo::GDAL::Child($a, 0));
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
        Geo::GDAL::VSIStdoutSetRedirection($ref);
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
            Geo::GDAL::VSIStdoutUnsetRedirection();
            $object->close;
        }
    }
    confess(Geo::GDAL->last_error) if $@;
    confess("Failed. Use Geo::OGR::Driver for vector drivers.") unless $ds;
    return $ds;
}

sub Create {
    my $self = shift;
    my $p = Geo::GDAL::named_parameters(\@_, Name => 'unnamed', Width => 256, Height => 256, Bands => 1, Type => 'Byte', Options => {});
    my $type = Geo::GDAL::string2int($p->{type}, \%Geo::GDAL::TYPE_STRING2INT);
    return $self->stdout_redirection_wrapper(
        $p->{name},
        $self->can('_Create'),
        $p->{width}, $p->{height}, $p->{bands}, $type, $p->{options}
    );
}
*CreateDataset = *Create;

sub Copy {
    my $self = shift;
    my $p = Geo::GDAL::named_parameters(\@_, Name => 'unnamed', Src => undef, Strict => 1, Options => {}, Progress => undef, ProgressData => undef);
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
    my $dataset = Geo::GDAL::OpenEx($p[0], \@flags, [$self->Name()]);
    Geo::GDAL::error("Failed to open $p[0]. Is it a raster dataset?") unless $dataset;
    return $dataset;
}




package Geo::GDAL::Dataset;
use strict;
use warnings;
use POSIX qw/floor ceil/;
use Scalar::Util 'blessed';
use Carp;
use Exporter 'import';

use vars qw/@EXPORT @DOMAINS @CAPABILITIES %CAPABILITIES %BANDS %LAYERS %RESULT_SET/;
@EXPORT = qw/BuildVRT/;
@DOMAINS = qw/IMAGE_STRUCTURE SUBDATASETS GEOLOCATION/;

sub RELEASE_PARENTS {
    my $self = shift;
    delete $BANDS{$self};
}

sub Dataset {
    my $self = shift;
    return $BANDS{tied(%$self)};
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
    Geo::GDAL::error(2, $index, 'Band') unless $band;
    $BANDS{tied(%{$band})} = $self;
    return $band;
}
*Band = *GetRasterBand;

sub AddBand {
    my ($self, $type, $options) = @_;
    $type //= 'Byte';
    $type = Geo::GDAL::string2int($type, \%Geo::GDAL::TYPE_STRING2INT);
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
    $LAYERS{tied(%$layer)} = $self;
    $RESULT_SET{tied(%$layer)} = 1;
    return $layer;
}

sub ReleaseResultSet {
    # a no-op, _ReleaseResultSet is called from Layer::DESTROY
}

sub GetLayer {
    my($self, $name) = @_;
    my $layer = defined $name ? GetLayerByName($self, "$name") : GetLayerByIndex($self, 0);
    $name //= '';
    Geo::GDAL::error(2, $name, 'Layer') unless $layer;
    $LAYERS{tied(%$layer)} = $self;
    return $layer;
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
    my $p = Geo::GDAL::named_parameters(\@_,
                                        Name => 'unnamed',
                                        SRS => undef,
                                        GeometryType => 'Unknown',
                                        Options => {},
                                        Schema => undef,
                                        Fields => undef,
                                        ApproxOK => 1);
    Geo::GDAL::error("The 'Fields' argument must be an array reference.") if $p->{fields} && ref($p->{fields}) ne 'ARRAY';
    if (defined $p->{schema}) {
        my $s = $p->{schema};
        $p->{geometrytype} = $s->{GeometryType} if exists $s->{GeometryType};
        $p->{fields} = $s->{Fields} if exists $s->{Fields};
        $p->{name} = $s->{Name} if exists $s->{Name};
    }
    $p->{fields} = [] unless ref($p->{fields}) eq 'ARRAY';
    # if fields contains spatial fields, then do not create default one
    for my $f (@{$p->{fields}}) {
        if ($f->{GeometryType} or exists $Geo::OGR::Geometry::TYPE_STRING2INT{$f->{Type}}) {
            $p->{geometrytype} = 'None';
            last;
        }
    }
    my $gt = Geo::GDAL::string2int($p->{geometrytype}, \%Geo::OGR::Geometry::TYPE_STRING2INT);
    my $layer = _CreateLayer($self, $p->{name}, $p->{srs}, $gt, $p->{options});
    $LAYERS{tied(%$layer)} = $self;
    for my $f (@{$p->{fields}}) {
        $layer->CreateField($f);
    }
    return $layer;
}

sub DeleteLayer {
    my ($self, $name) = @_;
    my $index;
    for my $i (0..$self->GetLayerCount-1) {
        my $layer = GetLayerByIndex($self, $i);
        $index = $i, last if $layer->GetName eq $name;
    }
    Geo::GDAL::error(2, $name, 'Layer') unless defined $index;
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
    confess(Geo::GDAL->last_error) if $@;
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
    return $self->GeoTransform->Extent($self->Size);
}

sub Tile { # $xoff, $yoff, $xsize, $ysize, assuming strict north up
    my ($self, $e) = @_;
    my ($w, $h) = $self->Size;
    #print "sz $w $h\n";
    my $gt = $self->GeoTransform;
    #print "gt @$gt\n";
    confess "GeoTransform is not \"north up\"." unless $gt->NorthUp;
    my $x = $gt->Extent($w, $h);
    my $xoff = floor(($e->[0] - $gt->[0])/$gt->[1]);
    $xoff = 0 if $xoff < 0;
    my $yoff = floor(($gt->[3] - $e->[3])/(-$gt->[5]));
    $yoff = 0 if $yoff < 0;
    my $xsize = ceil(($e->[2] - $gt->[0])/$gt->[1]) - $xoff;
    $xsize = $w - $xoff if $xsize > $w - $xoff;
    my $ysize = ceil(($gt->[3] - $e->[1])/(-$gt->[5])) - $yoff;
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
    my $p = Geo::GDAL::named_parameters(\@_,
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
    $p->{resamplealg} = Geo::GDAL::string2int($p->{resamplealg}, \%Geo::GDAL::RIO_RESAMPLING_STRING2INT);
    $p->{buftype} = Geo::GDAL::string2int($p->{buftype}, \%Geo::GDAL::TYPE_STRING2INT, \%Geo::GDAL::TYPE_INT2STRING);
    $self->_ReadRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bandlist},$p->{bufpixelspace},$p->{buflinespace},$p->{bufbandspace},$p->{resamplealg},$p->{progress},$p->{progressdata});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->Band->DataType;
    my $p = Geo::GDAL::named_parameters(\@_,
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
    $p->{buftype} = Geo::GDAL::string2int($p->{buftype}, \%Geo::GDAL::TYPE_STRING2INT, \%Geo::GDAL::TYPE_INT2STRING);
    $self->_WriteRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{buf},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bandlist},$p->{bufpixelspace},$p->{buflinespace},$p->{bufbandspace});
}

sub BuildOverviews {
    my $self = shift;
    my @p = @_;
    $p[0] = uc($p[0]) if $p[0];
    eval {
        $self->_BuildOverviews(@p);
    };
    confess(Geo::GDAL->last_error) if $@;
}

sub stdout_redirection_wrapper {
    my ($self, $name, $sub, @params) = @_;
    my $object = 0;
    if ($name && blessed $name) {
        $object = $name;
        my $ref = $object->can('write');
        Geo::GDAL::VSIStdoutSetRedirection($ref);
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
            Geo::GDAL::VSIStdoutUnsetRedirection();
            $object->close;
        }
    }
    confess(Geo::GDAL->last_error) if $@;
    return $ds;
}

sub DEMProcessing {
    my ($self, $dest, $Processing, $ColorFilename, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALDEMProcessingOptions->new(Geo::GDAL::make_processing_options($options));
    return $self->stdout_redirection_wrapper(
        $dest,
        \&Geo::GDAL::wrapper_GDALDEMProcessing,
        $Processing, $ColorFilename, $options, $progress, $progress_data
    );
}

sub Nearblack {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALNearblackOptions->new(Geo::GDAL::make_processing_options($options));
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
                $options = Geo::GDAL::GDALTranslateOptions->new(Geo::GDAL::make_processing_options($options));
                $ds = Geo::GDAL::wrapper_GDALTranslate($dest, $self, $options, $progress, $progress_data);
            } else {
                $options = Geo::GDAL::GDALVectorTranslateOptions->new(Geo::GDAL::make_processing_options($options));
                Geo::GDAL::wrapper_GDALVectorTranslateDestDS($dest, $self, $options, $progress, $progress_data);
                $ds = Geo::GDAL::wrapper_GDALVectorTranslateDestName($dest, $self, $options, $progress, $progress_data);
            }
            return $ds;
        }
    );
}

sub Warped {
    my $self = shift;
    my $p = Geo::GDAL::named_parameters(\@_, SrcSRS => undef, DstSRS => undef, ResampleAlg => 'NearestNeighbour', MaxError => 0);
    for my $srs (qw/srcsrs dstsrs/) {
        $p->{$srs} = $p->{$srs}->ExportToWkt if $p->{$srs} && blessed $p->{$srs};
    }
    $p->{resamplealg} = Geo::GDAL::string2int($p->{resamplealg}, \%Geo::GDAL::RESAMPLING_STRING2INT);
    my $warped = Geo::GDAL::_AutoCreateWarpedVRT($self, $p->{srcsrs}, $p->{dstsrs}, $p->{resamplealg}, $p->{maxerror});
    $BANDS{tied(%{$warped})} = $self if $warped; # self must live as long as warped
    return $warped;
}

sub Warp {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    # can be run as object method (one dataset) and as package sub (a list of datasets)
    $options = Geo::GDAL::GDALWarpAppOptions->new(Geo::GDAL::make_processing_options($options));
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
    $o = Geo::GDAL::GDALInfoOptions->new(Geo::GDAL::make_processing_options($o));
    return Geo::GDAL::GDALInfo($self, $o);
}

sub Grid {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALGridOptions->new(Geo::GDAL::make_processing_options($options));
    return $self->stdout_redirection_wrapper(
        $dest,
        \&Geo::GDAL::wrapper_GDALGrid,
        $options, $progress, $progress_data
    );
}

sub Rasterize {
    my ($self, $dest, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALRasterizeOptions->new(Geo::GDAL::make_processing_options($options));
    my $b = blessed($dest);
    if ($b && $b eq 'Geo::GDAL::Dataset') {
        Geo::GDAL::wrapper_GDALRasterizeDestDS($dest, $self, $options, $progress, $progress_data);
    } else {
        return $self->stdout_redirection_wrapper(
            $dest,
            \&Geo::GDAL::wrapper_GDALRasterizeDestName,
            $options, $progress, $progress_data
        );
    }
}

sub BuildVRT {
    my ($dest, $sources, $options, $progress, $progress_data) = @_;
    $options = Geo::GDAL::GDALBuildVRTOptions->new(Geo::GDAL::make_processing_options($options));
    Geo::GDAL::error("Usage: Geo::GDAL::DataSet::BuildVRT(\$vrt_file_name, \\\@sources)")
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
    my $p = Geo::GDAL::named_parameters(\@_,
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
    my $p = Geo::GDAL::named_parameters(\@_,
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

use vars qw/ %RATS
    @COLOR_INTERPRETATIONS
    %COLOR_INTERPRETATION_STRING2INT %COLOR_INTERPRETATION_INT2STRING @DOMAINS
    %MASK_FLAGS
    /;
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@COLOR_INTERPRETATIONS, $1), next if /^GCI_(\w+)/;
}
for my $string (@COLOR_INTERPRETATIONS) {
    my $int = eval "\$Geo::GDAL::Constc::GCI_$string";
    $COLOR_INTERPRETATION_STRING2INT{$string} = $int;
    $COLOR_INTERPRETATION_INT2STRING{$int} = $string;
}
@DOMAINS = qw/IMAGE_STRUCTURE RESAMPLING/;
%MASK_FLAGS = (AllValid => 1, PerDataset => 2, Alpha => 4, NoData => 8);

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
    $self->RELEASE_PARENTS();
}

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::GDAL::Dataset::BANDS{$self};
}

sub Dataset {
    my $self = shift;
    return $Geo::GDAL::Dataset::BANDS{tied(%{$self})};
}

sub Size {
    my $self = shift;
    return ($self->{XSize}, $self->{YSize});
}

sub DataType {
    my $self = shift;
    return $Geo::GDAL::TYPE_INT2STRING{$self->{DataType}};
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
    my $t = $self->{DataType};
    $alg = Geo::GDAL::string2int($alg, \%Geo::GDAL::RIO_RESAMPLING_STRING2INT);
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
        $ci = Geo::GDAL::string2int($ci, \%COLOR_INTERPRETATION_STRING2INT);
        SetRasterColorInterpretation($self, $ci);
    }
    return unless defined wantarray;
    $COLOR_INTERPRETATION_INT2STRING{GetRasterColorInterpretation($self)};
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
    $RATS{tied(%$r)} = $self if $r;
    return $r;
}
*RasterAttributeTable = *AttributeTable;

sub GetHistogram {
    my $self = shift;
    my $p = Geo::GDAL::named_parameters(\@_,
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
    my $p = Geo::GDAL::named_parameters(\@_,
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
    my $p = Geo::GDAL::named_parameters(\@_,
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
    $p->{resamplealg} = Geo::GDAL::string2int($p->{resamplealg}, \%Geo::GDAL::RIO_RESAMPLING_STRING2INT);
    $p->{buftype} = Geo::GDAL::string2int($p->{buftype}, \%Geo::GDAL::TYPE_STRING2INT, \%Geo::GDAL::TYPE_INT2STRING);
    $self->_ReadRaster($p->{xoff},$p->{yoff},$p->{xsize},$p->{ysize},$p->{bufxsize},$p->{bufysize},$p->{buftype},$p->{bufpixelspace},$p->{buflinespace},$p->{resamplealg},$p->{progress},$p->{progressdata});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->DataType;
    my $p = Geo::GDAL::named_parameters(\@_,
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
    $p->{buftype} = Geo::GDAL::string2int($p->{buftype}, \%Geo::GDAL::TYPE_STRING2INT, \%Geo::GDAL::TYPE_INT2STRING);
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
    my $self = shift; # should use named parameters for read raster and band
    # add Piddle sub to dataset too to make N x M x n piddles
    my ($w, $h) = $self->Size;
    my $data = $self->ReadRaster;
    my $pdl = PDL->new;
    my %map = (
        Byte => 0,
        UInt16 => 2,
        Int16 => 1,
        UInt32 => -1,
        Int32 => 3,
        Float32 => 5,
        Float64 => 6,
        CInt16 => -1,
        CInt32 => -1,
        CFloat32 => -1,
        CFloat64 => -1
        );
    my $datatype = $map{$self->DataType};
    croak "there is no direct mapping between the band datatype and PDL" if $datatype < 0;
    $pdl->set_datatype($datatype);
    $pdl->setdims([1,$w,$h]);
    my $dref = $pdl->get_dataref();
    $$dref = $data;
    $pdl->upd_data;
    return $pdl;
}

sub GetMaskBand {
    my $self = shift;
    my $band = _GetMaskBand($self);
    $Geo::GDAL::Dataset::BANDS{tied(%{$band})} = $self;
    return $band;
}

sub GetOverview {
    my ($self, $index) = @_;
    my $band = _GetOverview($self, $index);
    $Geo::GDAL::Dataset::BANDS{tied(%{$band})} = $self;
    return $band;
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
    my $p = Geo::GDAL::named_parameters(\@_, Mask => undef, OutLayer => undef, PixValField => 'val', Options => undef, Progress => undef, ProgressData => undef);
    my $dt = $self->DataType;
    my %leInt32 = (Byte => 1, Int16 => 1, Int32 => 1, UInt16 => 1);
    my $leInt32 = $leInt32{$dt};
    $dt = $dt =~ /Float/ ? 'Real' : 'Integer';
    $p->{outlayer} //= Geo::OGR::Driver('Memory')->Create()->
        CreateLayer(Name => 'polygonized',
                    Fields => [{Name => 'val', Type => $dt},
                               {Name => 'geom', Type => 'Polygon'}]);
    $p->{pixvalfield} = $p->{outlayer}->GetLayerDefn->GetFieldIndex($p->{pixvalfield});
    $p->{options}{'8CONNECTED'} = $p->{options}{Connectedness} if $p->{options}{Connectedness};
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
    my $p = Geo::GDAL::named_parameters(\@_, Mask => undef, Dest => undef, Threshold => 10, Options => undef, Progress => undef, ProgressData => undef);
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
    my $p = Geo::GDAL::named_parameters(\@_, Distance => undef, Options => undef, Progress => undef, ProgressData => undef);
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

use vars qw/%PALETTE_INTERPRETATION_STRING2INT %PALETTE_INTERPRETATION_INT2STRING/;
for (keys %Geo::GDAL::Const::) {
    if (/^GPI_(\w+)/) {
        my $int = eval "\$Geo::GDAL::Const::GPI_$1";
        $PALETTE_INTERPRETATION_STRING2INT{$1} = $int;
        $PALETTE_INTERPRETATION_INT2STRING{$int} = $1;
    }
}
%}

%feature("shadow") GDALColorTableShadow(GDALPaletteInterp palette = GPI_RGB)
%{
use Carp;
sub new {
    my($pkg, $pi) = @_;
    $pi //= 'RGB';
    $pi = Geo::GDAL::string2int($pi, \%PALETTE_INTERPRETATION_STRING2INT);
    my $self = Geo::GDALc::new_ColorTable($pi);
    bless $self, $pkg if defined($self);
}
%}

%perlcode %{
sub GetPaletteInterpretation {
    my $self = shift;
    return $PALETTE_INTERPRETATION_INT2STRING{GetPaletteInterpretation($self)};
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
    confess(Geo::GDAL->last_error) if $@;
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

use vars qw/
    @FIELD_TYPES @FIELD_USAGES
    %FIELD_TYPE_STRING2INT %FIELD_TYPE_INT2STRING
    %FIELD_USAGE_STRING2INT %FIELD_USAGE_INT2STRING
    /;
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@FIELD_TYPES, $1), next if /^GFT_(\w+)/;
    push(@FIELD_USAGES, $1), next if /^GFU_(\w+)/;
}
for my $string (@FIELD_TYPES) {
    my $int = eval "\$Geo::GDAL::Constc::GFT_$string";
    $FIELD_TYPE_STRING2INT{$string} = $int;
    $FIELD_TYPE_INT2STRING{$int} = $string;
}
for my $string (@FIELD_USAGES) {
    my $int = eval "\$Geo::GDAL::Constc::GFU_$string";
    $FIELD_USAGE_STRING2INT{$string} = $int;
    $FIELD_USAGE_INT2STRING{$int} = $string;
}

sub FieldTypes {
    return @FIELD_TYPES;
}

sub FieldUsages {
    return @FIELD_USAGES;
}

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::GDAL::Band::RATS{$self};
}

sub Band {
    my $self = shift;
    return $Geo::GDAL::Band::RATS{tied(%$self)};
}

sub GetUsageOfCol {
    my($self, $col) = @_;
    $FIELD_USAGE_INT2STRING{_GetUsageOfCol($self, $col)};
}

sub GetColOfUsage {
    my($self, $usage) = @_;
    _GetColOfUsage($self, $FIELD_USAGE_STRING2INT{$usage});
}

sub GetTypeOfCol {
    my($self, $col) = @_;
    $FIELD_TYPE_INT2STRING{_GetTypeOfCol($self, $col)};
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
    $type = Geo::GDAL::string2int($type, \%FIELD_TYPE_STRING2INT);
    $usage = Geo::GDAL::string2int($usage, \%FIELD_USAGE_STRING2INT);
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



package Geo::GDAL::VSIF;
use strict;
use warnings;
use Carp;
require Exporter;
our @ISA = qw(Exporter);

our @EXPORT_OK   = qw(Open Close Write Read Seek Tell Truncate MkDir ReadDir ReadDirRecursive Rename RmDir Stat Unlink);
our %EXPORT_TAGS = (all => \@EXPORT_OK);

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
    my ($self, $data) = @_;
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
        Geo::GDAL::error("Cannot remove directory \"$dirname\"$r.");
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

sub new {
    my $class = shift;
    my $self;
    if (@_ == 0) {
        $self = [0,1,0,0,0,1];
    } elsif (@_ == 1) {
        $self = $_[0];
    } else {
        my @a = @_;
        $self = \@a;
    }
    bless $self, $class;
    return $self;
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
    Geo::GDAL::error('Usage: Geo::GDAL::GeoTransform::FromGCPs(\@gcps, $approx_ok)') unless @$gcps;
    my $self = Geo::GDAL::GCPsToGeoTransform($gcps, $approx_ok);
    bless $self, 'Geo::GDAL::GetTransform';
    return $self;
}

sub Apply {
    my ($self, $columns, $rows) = @_;
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
    return new(@inv) if defined wantarray;
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

sub new {
    my $class = shift;
    my $self;
    if (@_ == 0) {
        $self = [0,0,0,0];
    } elsif (ref $_[0]) {
        @$self = @{$_[0]};
    } else {
        @$self = @_;
    }
    bless $self, $class;
    return $self;
}

sub Size {
    my $self = shift;
    return ($self->[2] - $self->[0], $self->[3] - $self->[1]);
}

sub Overlaps {
    my ($self, $e) = @_;
    return $self->[0] < $e->[2] && $self->[2] > $e->[0] && $self->[1] < $e->[3] && $self->[3] > $e->[1];
}

sub Overlap {
    my ($self, $e) = @_;
    return undef unless $self->Overlaps($e);
    my $ret = Geo::GDAL::Extent->new($self);
    $ret->[0] = $e->[0] if $self->[0] < $e->[0];
    $ret->[1] = $e->[1] if $self->[1] < $e->[1];
    $ret->[2] = $e->[2] if $self->[2] > $e->[2];
    $ret->[3] = $e->[3] if $self->[3] > $e->[3];
    return $ret;
}

sub ExpandToInclude {
    my ($self, $e) = @_;
    $self->[0] = $e->[0] if $e->[0] < $self->[0];
    $self->[1] = $e->[1] if $e->[1] < $self->[1];
    $self->[2] = $e->[2] if $e->[2] > $self->[2];
    $self->[3] = $e->[3] if $e->[3] > $self->[3];
}

package Geo::GDAL::XML;

use strict;
use warnings;
use Carp;

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
    my $self = Geo::GDAL::ParseXMLString($xml);
    bless $self, $class;
    $self->traverse(sub {my $node = shift; bless $node, $class});
    return $self;
}

sub traverse {
    my ($self, $sub) = @_;
    my $type = $self->[0];
    my $data = $self->[1];
    $type = Geo::GDAL::NodeType($type);
    $sub->($self, $type, $data);
    for my $child (@{$self}[2..$#$self]) {
        traverse($child, $sub);
    }
}

sub serialize {
    my $self = shift;
    return Geo::GDAL::SerializeXMLTree($self);
}

%}

%{
typedef void OGRLayerShadow;
%}
%extend GDALRasterBandShadow {
    %apply (int nList, double* pList) {(int nFixedLevelCount, double *padfFixedLevels)};
    %apply (int defined, double value) {(int bUseNoData, double dfNoDataValue)};
    CPLErr ContourGenerate(double dfContourInterval, double dfContourBase,
                           int nFixedLevelCount, double *padfFixedLevels,
                           int bUseNoData, double dfNoDataValue,
                           OGRLayerShadow *hLayer, int iIDField, int iElevField,
                           GDALProgressFunc progress = NULL,
                           void* progress_data = NULL) {
        return GDALContourGenerate( self, dfContourInterval, dfContourBase,
                                    nFixedLevelCount, padfFixedLevels,
                                    bUseNoData, dfNoDataValue,
                                    hLayer, iIDField, iElevField,
                                    progress,
                                    progress_data );
    }
    %clear (int nFixedLevelCount, double *padfFixedLevels);
    %clear (int bUseNoData, double dfNoDataValue);
}
