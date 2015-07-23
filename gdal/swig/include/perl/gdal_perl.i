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

%rename (_GetDriver) GetDriver;

%rename (_Open) Open;
%newobject _Open;

%rename (_OpenShared) OpenShared;
%newobject _OpenShared;

%rename (_BuildOverviews) BuildOverviews;

%rename (_ReadRaster) ReadRaster;
%rename (_WriteRaster) WriteRaster;

%rename (_GetMaskFlags) GetMaskFlags;
%rename (_CreateMaskBand) CreateMaskBand;

/* those that need callback_data check: */

%rename (_ComputeMedianCutPCT) ComputeMedianCutPCT;
%rename (_DitherRGB2PCT) DitherRGB2PCT;
%rename (_ReprojectImage) ReprojectImage;
%rename (_ComputeProximity) ComputeProximity;
%rename (_RasterizeLayer) RasterizeLayer;
%rename (_Polygonize) Polygonize;
%rename (_SieveFilter) SieveFilter;
%rename (_RegenerateOverviews) RegenerateOverviews;
%rename (_RegenerateOverview) RegenerateOverview;

%rename (_AutoCreateWarpedVRT) AutoCreateWarpedVRT;
%newobject _AutoCreateWarpedVRT;

%rename (_Create) Create;
%newobject _Create;

%rename (_GetRasterBand) GetRasterBand;
%rename (_AddBand) AddBand;

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
use strict;
use warnings;
use Carp;
use Encode;
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

our $VERSION = '2.0100';
our $GDAL_VERSION = '2.1.0';
use vars qw/
    @DATA_TYPES @ACCESS_TYPES @RESAMPLING_TYPES @RIO_RESAMPLING_TYPES @NODE_TYPES
    %TYPE_STRING2INT %TYPE_INT2STRING
    %ACCESS_STRING2INT %ACCESS_INT2STRING
    %RESAMPLING_STRING2INT %RESAMPLING_INT2STRING
    %RIO_RESAMPLING_STRING2INT %RIO_RESAMPLING_INT2STRING
    %NODE_TYPE_STRING2INT %NODE_TYPE_INT2STRING
    /;
for (keys %Geo::GDAL::Const::) {
    next if /TypeCount/;
    push(@DATA_TYPES, $1), next if /^GDT_(\w+)/;
    push(@ACCESS_TYPES, $1), next if /^GA_(\w+)/;
    push(@RESAMPLING_TYPES, $1), next if /^GRA_(\w+)/;
    push(@RIO_RESAMPLING_TYPES, $1), next if /^GRIORA_(\w+)/;
    push(@NODE_TYPES, $1), next if /^CXT_(\w+)/;
}
for my $string (@DATA_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GDT_$string";
    $TYPE_STRING2INT{$string} = $int;
    $TYPE_INT2STRING{$int} = $string;
}
for my $string (@ACCESS_TYPES) {
    my $int = eval "\$Geo::GDAL::Const::GA_$string";
    $ACCESS_STRING2INT{$string} = $int;
    $ACCESS_INT2STRING{$int} = $string;
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

sub RELEASE_PARENTS {
}

sub DataTypes {
    return @DATA_TYPES;
}

sub AccessTypes {
    return @ACCESS_TYPES;
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
    my $t = shift;
    my $t2 = $t;
    $t2 = $TYPE_STRING2INT{$t} if exists $TYPE_STRING2INT{$t};
    confess "Unknown data type: '$t'." unless exists $TYPE_INT2STRING{$t2};
    return _GetDataTypeSize($t2);
}

sub DataTypeValueRange {
    my $t = shift;
    confess "Unknown data type: '$t'." unless exists $TYPE_STRING2INT{$t};
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
    my $t = shift;
    my $t2 = $t;
    $t2 = $TYPE_STRING2INT{$t} if exists $TYPE_STRING2INT{$t};
    confess "Unknown data type: '$t'." unless exists $TYPE_INT2STRING{$t2};
    return _DataTypeIsComplex($t2);
}

sub PackCharacter {
    my $t = shift;
    $t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
    confess "Unknown data type: '$t'." unless exists $TYPE_STRING2INT{$t};
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
        my $driver = _GetDriver($i);
        my $md = $driver->GetMetadata;
        next unless $md->{DCAP_RASTER} and $md->{DCAP_RASTER} eq 'YES';
        push @names, _GetDriver($i)->Name;
    }
    return @names;
}

sub Drivers {
    my @drivers;
    for my $i (0..GetDriverCount()-1) {
        my $driver = _GetDriver($i);
        my $md = $driver->GetMetadata;
        next unless $md->{DCAP_RASTER} and $md->{DCAP_RASTER} eq 'YES';
        push @drivers, _GetDriver($i);
    }
    return @drivers;
}

sub GetDriver {
    my($name) = @_;
    $name = 0 unless defined $name;
    my $driver;
    $driver = _GetDriver($name) if $name =~ /^\d+$/; # is the name an index to driver list?
    $driver = GetDriverByName("$name") unless $driver;
    if ($driver) {
        my $md = $driver->GetMetadata;
        confess "Driver exists but does not have raster capabilities." 
            unless $md->{DCAP_RASTER} and $md->{DCAP_RASTER} eq 'YES';
        return $driver;
    }
    confess "Driver not found: '$name'. Maybe support for it was not built in?";
}
*Driver = *GetDriver;

sub Open {
    my @p = @_;
    if (defined $p[1]) {
        confess "Unknown access type: '$p[1]'." unless exists $Geo::GDAL::ACCESS_STRING2INT{$p[1]};
        $p[1] = $Geo::GDAL::ACCESS_STRING2INT{$p[1]};
    }
    return _Open(@p);
}

sub OpenShared {
    my @p = @_;
    if (defined $p[1]) {
        confess "Unknown access type: '$p[1]'." unless exists $Geo::GDAL::ACCESS_STRING2INT{$p[1]};
        $p[1] = $Geo::GDAL::ACCESS_STRING2INT{$p[1]};
    }
    return _OpenShared(@p);
}

sub ComputeMedianCutPCT {
    my @p = @_;
    $p[6] = 1 if $p[5] and not defined $p[6];
    _ComputeMedianCutPCT(@p);
}

sub DitherRGB2PCT {
    my @p = @_;
    $p[6] = 1 if $p[5] and not defined $p[6];
    _DitherRGB2PCT(@p);
}

sub ComputeProximity {
    my @p = @_;
    $p[4] = 1 if $p[3] and not defined $p[4];
    _ComputeProximity(@p);
}

sub RasterizeLayer {
    my @p = @_;
    $p[8] = 1 if $p[7] and not defined $p[8];
    _RasterizeLayer(@p);
}

sub Polygonize {
    my @params = @_;
    $params[6] = 1 if $params[5] and not defined $params[6];
    $params[3] = $params[2]->GetLayerDefn->GetFieldIndex($params[3]) unless $params[3] =~ /^\d/;
    _Polygonize(@params);
}

sub SieveFilter {
    my @p = @_;
    $p[7] = 1 if $p[6] and not defined $p[7];
    _SieveFilter(@p);
}

sub RegenerateOverviews {
    my @p = @_;
    $p[2] = uc($p[2]) if $p[2]; # see overview.cpp:2030
    $p[4] = 1 if $p[3] and not defined $p[4];
    _RegenerateOverviews(@p);
}

sub RegenerateOverview {
    my @p = @_;
    $p[2] = uc($p[2]) if $p[2]; # see overview.cpp:2030
    $p[4] = 1 if $p[3] and not defined $p[4];
    _RegenerateOverview(@p);
}

sub ReprojectImage {
    my @p = @_;
    $p[8] = 1 if $p[7] and not defined $p[8];
    if (defined $p[4]) {
        confess "Unknown data type: '$p[4]'." unless exists $Geo::GDAL::RESAMPLING_STRING2INT{$p[4]};
        $p[4] = $Geo::GDAL::RESAMPLING_STRING2INT{$p[4]};
    }
    return _ReprojectImage(@p);
}

sub AutoCreateWarpedVRT {
    my @p = @_;
    for my $i (1..2) {
        if (defined $p[$i] and ref($p[$i])) {
            $p[$i] = $p[$i]->ExportToWkt;
        }
    }
    if (defined $p[3]) {
        confess "Unknown data type: '$p[3]'." unless exists $Geo::GDAL::RESAMPLING_STRING2INT{$p[3]};
        $p[3] = $Geo::GDAL::RESAMPLING_STRING2INT{$p[3]};
    }
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
    my $self = shift;
    my $metadata;
    $metadata = shift if ref $_[0];
    my $domain = shift;
    $domain = '' unless defined $domain;
    SetMetadata($self, $metadata, $domain) if defined $metadata;
    GetMetadata($self, $domain) if defined wantarray;
}


package Geo::GDAL::Driver;
use strict;
use warnings;
use Carp;
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

sub Create {
    my $self = shift;
    my %defaults = ( Name => 'unnamed',
                     Width => 256,
                     Height => 256,
                     Bands => 1,
                     Type => 'Byte',
                     Options => {} );
    my %params;
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %params = %{$_[0]};
    } elsif (exists $defaults{$_[0]} and @_ % 2 == 0) {
        %params = @_;
    } else {
        ($params{Name}, $params{Width}, $params{Height}, $params{Bands}, $params{Type}, $params{Options}) = @_;
    }
    for my $k (keys %params) {
        carp "Create: unrecognized named parameter '$k'." unless exists $defaults{$k};
    }
    for my $k (keys %defaults) {
        $params{$k} = $defaults{$k} unless defined $params{$k};
    }
    my $type;
    confess "Unknown data type: '$params{Type}'." unless exists $Geo::GDAL::TYPE_STRING2INT{$params{Type}};
    $type = $Geo::GDAL::TYPE_STRING2INT{$params{Type}};
    return $self->_Create($params{Name}, $params{Width}, $params{Height}, $params{Bands}, $type, $params{Options});
}
*CreateDataset = *Create;
*Copy = *CreateCopy;




package Geo::GDAL::Dataset;
use strict;
use warnings;
use Carp;
use vars qw/%BANDS @DOMAINS/;
@DOMAINS = qw/IMAGE_STRUCTURE SUBDATASETS GEOLOCATION/;

sub Domains {
    return @DOMAINS;
}
*GetDriver = *_GetDriver;

sub Open {
    return Geo::GDAL::Open(@_);
}

sub OpenShared {
    return Geo::GDAL::OpenShared(@_);
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
    my($self, $index) = @_;
    $index = 1 unless defined $index;
    my $band = _GetRasterBand($self, $index);
    $BANDS{tied(%{$band})} = $self;
    return $band;
}
*Band = *GetRasterBand;

sub AddBand {
    my @p = @_;
    if (defined $p[1]) {
        confess "Unknown data type: '$p[1]'." unless exists $Geo::GDAL::TYPE_STRING2INT{$p[1]};
        $p[1] = $Geo::GDAL::TYPE_STRING2INT{$p[1]};
    }
    return _AddBand(@p);
}

sub Projection {
    my($self, $proj) = @_;
    SetProjection($self, $proj) if defined $proj;
    GetProjection($self) if defined wantarray;
}

sub SpatialReference {
    my($self, $sr) = @_;
    SetProjection($self, $sr->As('WKT')) if defined $sr;
    return Geo::OSR::SpatialReference->new(GetProjection($self)) if defined wantarray;
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
    confess $@ if $@;
    return unless defined wantarray;
    my $t = GetGeoTransform($self);
    if (wantarray) {
        return @$t;
    } else {
        return Geo::GDAL::GeoTransform->new($t);
    }
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

sub ReadRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->Band->DataType;
    my %d = (
        XOFF => 0,
        YOFF => 0,
        XSIZE => $width,
        YSIZE => $height,
        BUFXSIZE => undef,
        BUFYSIZE => undef,
        BUFTYPE => $type,
        BANDLIST => [1],
        BUFPIXELSPACE => 0,
        BUFLINESPACE => 0,
        BUFBANDSPACE => 0,
        RESAMPLEALG => 'NearestNeighbour',
        PROGRESS => undef,
        PROGRESSDATA => undef
        );
    my %p;
    my $t;
    if (defined $_[0]) {
        $t = uc($_[0]); 
        $t =~ s/_//g;
    }
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %p = %{$_[0]};
    } elsif (@_ % 2 == 0 and (defined $t and exists $d{$t})) {
        %p = @_;
    } else {
        ($p{xoff},$p{yoff},$p{xsize},$p{ysize},$p{buf_xsize},$p{buf_ysize},$p{buf_type},$p{band_list},$p{buf_pixel_space},$p{buf_line_space},$p{buf_band_space},$p{resample_alg},$p{progress},$p{progress_data}) = @_;
    }
    for (keys %p) {
        my $u = uc($_);
        $u =~ s/_//g;
        carp "Unknown named parameter '$_'." unless exists $d{$u};
        $p{$u} = $p{$_};
    }
    for (keys %d) {
        $p{$_} = $d{$_} unless defined $p{$_};
    }
    confess "Unknown resampling algorithm: '$p{RESAMPLEALG}'." 
        unless exists $Geo::GDAL::RIO_RESAMPLING_STRING2INT{$p{RESAMPLEALG}};
    $p{RESAMPLEALG} = $Geo::GDAL::RIO_RESAMPLING_STRING2INT{$p{RESAMPLEALG}};
    unless ($Geo::GDAL::TYPE_INT2STRING{$p{BUFTYPE}}) {
        confess "Unknown data type: '$p{BUFTYPE}'." 
            unless exists $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
        $p{BUFTYPE} = $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
    }
    $self->_ReadRaster($p{XOFF},$p{YOFF},$p{XSIZE},$p{YSIZE},$p{BUFXSIZE},$p{BUFYSIZE},$p{BUFTYPE},$p{BANDLIST},$p{BUFPIXELSPACE},$p{BUFLINESPACE},$p{BUFBANDSPACE},$p{RESAMPLEALG},$p{PROGRESS},$p{PROGRESSDATA});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->Band->DataType;
    my %d = (
        XOFF => 0,
        YOFF => 0,
        XSIZE => $width,
        YSIZE => $height,
        BUF => undef,
        BUFXSIZE => undef,
        BUFYSIZE => undef,
        BUFTYPE => $type,
        BANDLIST => [1],
        BUFPIXELSPACE => 0,
        BUFLINESPACE => 0,
        BUFBANDSPACE => 0
        );
    my %p;
    my $t;
    if (defined $_[0]) {
        $t = uc($_[0]); 
        $t =~ s/_//g;
    }
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %p = %{$_[0]};
    } elsif (@_ % 2 == 0 and (defined $t and exists $d{$t})) {
        %p = @_;
    } else {
        ($p{xoff},$p{yoff},$p{xsize},$p{ysize},$p{buf},$p{buf_xsize},$p{buf_ysize},$p{buf_type},$p{band_list},$p{buf_pixel_space},$p{buf_line_space},$p{buf_band_space}) = @_;
    }
    for (keys %p) {
        my $u = uc($_);
        $u =~ s/_//g;
        carp "Unknown named parameter '$_'." unless exists $d{$u};
        $p{$u} = $p{$_};
    }
    for (keys %d) {
        $p{$_} = $d{$_} unless defined $p{$_};
    }
    unless ($Geo::GDAL::TYPE_INT2STRING{$p{BUFTYPE}}) {
        confess "Unknown data type: '$p{BUFTYPE}'." 
            unless exists $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
        $p{BUFTYPE} = $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
    }
    $self->_WriteRaster($p{XOFF},$p{YOFF},$p{XSIZE},$p{YSIZE},$p{BUF},$p{BUFXSIZE},$p{BUFYSIZE},$p{BUFTYPE},$p{BANDLIST},$p{BUFPIXELSPACE},$p{BUFLINESPACE},$p{BUFBANDSPACE});
}

sub BuildOverviews {
    my $self = shift;
    my @p = @_;
    $p[0] = uc($p[0]) if $p[0];
    eval {
        $self->_BuildOverviews(@p);
    };
    confess $@ if $@;
}




package Geo::GDAL::Band;
use strict;
use warnings;
use POSIX;
use Carp;
use Scalar::Util 'blessed';
use vars qw/
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
    my $self;
    if ($_[0]->isa('SCALAR')) {
        $self = $_[0];
    } else {
        return unless $_[0]->isa('HASH');
        $self = tied(%{$_[0]});
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
        $unit = '' unless defined $unit;
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
    my($self, $xoff, $yoff, $xsize, $ysize) = @_;
    $xoff = 0 unless defined $xoff;
    $yoff = 0 unless defined $yoff;
    $xsize = $self->{XSize} - $xoff unless defined $xsize;
    $ysize = $self->{YSize} - $yoff unless defined $ysize;
    my $buf = $self->ReadRaster($xoff, $yoff, $xsize, $ysize);
    my $pc = Geo::GDAL::PackCharacter($self->{DataType});
    my $w = $xsize * Geo::GDAL::GetDataTypeSize($self->{DataType})/8;
    my $offset = 0;
    my @data;
    for (0..$ysize-1) {
        my $sub = substr($buf, $offset, $w);
        my @d = unpack($pc."[$xsize]", $sub);
        push @data, \@d;
        $offset += $w;
    }
    return \@data;
}

sub WriteTile {
    my($self, $data, $xoff, $yoff) = @_;
    $xoff = 0 unless defined $xoff;
    $yoff = 0 unless defined $yoff;
    my $xsize = @{$data->[0]};
    $xsize = $self->{XSize} - $xoff if $xsize > $self->{XSize} - $xoff;
    my $ysize = @{$data};
    $ysize = $self->{YSize} - $yoff if $ysize > $self->{YSize} - $yoff;
    my $pc = Geo::GDAL::PackCharacter($self->{DataType});
    for my $i (0..$ysize-1) {
        my $scanline = pack($pc."[$xsize]", @{$data->[$i]});
        $self->WriteRaster( $xoff, $yoff+$i, $xsize, 1, $scanline );
    }
}

sub ColorInterpretation {
    my($self, $ci) = @_;
    if (defined $ci) {
        my $ci2 = $ci;
        $ci2 = $COLOR_INTERPRETATION_STRING2INT{$ci} if exists $COLOR_INTERPRETATION_STRING2INT{$ci};
        confess "Unknown color interpretation: '$ci'." unless exists $COLOR_INTERPRETATION_INT2STRING{$ci2};
        SetRasterColorInterpretation($self, $ci2);
        return $ci;
    } else {
        return $COLOR_INTERPRETATION_INT2STRING{GetRasterColorInterpretation($self)};
    }
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
    $Geo::GDAL::RasterAttributeTable::BANDS{$r} = $self if $r;
    return $r;
}

sub GetHistogram {
    my $self = shift;
    my %defaults = (Min => -0.5,
                    Max => 255.5,
                    Buckets => 256,
                    IncludeOutOfRange => 0,
                    ApproxOK => 0,
                    Progress => undef,
                    ProgressData => undef);
    my %params = @_;
    for (keys %params) {
        carp "unknown parameter $_ in Geo::GDAL::Band::GetHistogram" unless exists $defaults{$_};
    }
    for (keys %defaults) {
        $params{$_} = $defaults{$_} unless defined $params{$_};
    }
    $params{ProgressData} = 1 if $params{Progress} and not defined $params{ProgressData};
    _GetHistogram($self, $params{Min}, $params{Max}, $params{Buckets},
                  $params{IncludeOutOfRange}, $params{ApproxOK},
                  $params{Progress}, $params{ProgressData});
}

sub Contours {
    my $self = shift;
    my %defaults = (DataSource => undef,
                    LayerConstructor => {Name => 'contours'},
                    ContourInterval => 100,
                    ContourBase => 0,
                    FixedLevels => [],
                    NoDataValue => undef,
                    IDField => -1,
                    ElevField => -1,
                    Progress => undef,
                    ProgressData => undef);
    my %params;
    if (!defined($_[0]) or (blessed($_[0]) and $_[0]->isa('Geo::OGR::DataSource'))) {
        ($params{DataSource}, $params{LayerConstructor},
         $params{ContourInterval}, $params{ContourBase},
         $params{FixedLevels}, $params{NoDataValue},
         $params{IDField}, $params{ElevField},
         $params{Progress}, $params{ProgressData}) = @_;
    } else {
        %params = @_;
        if (exists $params{progress}) {
            $params{Progress} = $params{progress};
            delete $params{progress};
        }
        if (exists $params{progress_data}) {
            $params{ProgressData} = $params{progress_data};
            delete $params{progress_data};
        }
    }
    for (keys %params) {
        carp "unknown parameter $_ in Geo::GDAL::Band::Contours" unless exists $defaults{$_};
    }
    for (keys %defaults) {
        $params{$_} = $defaults{$_} unless defined $params{$_};
    }
    $params{DataSource} = Geo::OGR::GetDriver('Memory')->CreateDataSource('ds')
        unless defined $params{DataSource};
    $params{LayerConstructor}->{Schema} = {} unless $params{LayerConstructor}->{Schema};
    $params{LayerConstructor}->{Schema}{Fields} = [] unless $params{LayerConstructor}->{Schema}{Fields};
    my %fields;
    unless ($params{IDField} =~ /^[+-]?\d+$/ or $fields{$params{IDField}}) {
        push @{$params{LayerConstructor}->{Schema}{Fields}}, {Name => $params{IDField}, Type => 'Integer'};
    }
    unless ($params{ElevField} =~ /^[+-]?\d+$/ or $fields{$params{ElevField}}) {
        my $type = $self->DataType() =~ /Float/ ? 'Real' : 'Integer';
        push @{$params{LayerConstructor}->{Schema}{Fields}}, {Name => $params{ElevField}, Type => $type};
    }
    my $layer = $params{DataSource}->CreateLayer($params{LayerConstructor});
    my $schema = $layer->GetLayerDefn;
    for ('IDField', 'ElevField') {
        $params{$_} = $schema->GetFieldIndex($params{$_}) unless $params{$_} =~ /^[+-]?\d+$/;
    }
    $params{ProgressData} = 1 if $params{Progress} and not defined $params{ProgressData};
    ContourGenerate($self, $params{ContourInterval}, $params{ContourBase}, $params{FixedLevels},
                    $params{NoDataValue}, $layer, $params{IDField}, $params{ElevField},
                    $params{Progress}, $params{ProgressData});
    return $layer;
}

sub FillNodata {
    my $self = shift;
    my $mask = shift;
    $mask = $self->GetMaskBand unless $mask;
    my @p = @_;
    $p[0] = 10 unless defined $p[0];
    $p[1] = 0 unless defined $p[1];
    $p[2] = undef unless defined $p[2];
    $p[3] = undef unless defined $p[3];
    $p[4] = undef unless defined $p[1];
    Geo::GDAL::FillNodata($self, $mask, @p);
}
*GetBandNumber = *GetBand;

sub ReadRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->DataType;
    my %d = (
        XOFF => 0,
        YOFF => 0,
        XSIZE => $width,
        YSIZE => $height,
        BUFXSIZE => undef,
        BUFYSIZE => undef,
        BUFTYPE => $type,
        BUFPIXELSPACE => 0,
        BUFLINESPACE => 0,
        RESAMPLEALG => 'NearestNeighbour',
        PROGRESS => undef,
        PROGRESSDATA => undef
        );
    my %p;
    my $t;
    if (defined $_[0]) {
        $t = uc($_[0]); 
        $t =~ s/_//g;
    }
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %p = %{$_[0]};
    } elsif (@_ % 2 == 0 and (defined $t and exists $d{$t})) {
        %p = @_;
    } else {
        ($p{xoff},$p{yoff},$p{xsize},$p{ysize},$p{buf_xsize},$p{buf_ysize},$p{buf_type},$p{buf_pixel_space},$p{buf_line_space},$p{resample_alg},$p{progress},$p{progress_data}) = @_;
    }
    for (keys %p) {
        my $u = uc($_);
        $u =~ s/_//g;
        carp "Unknown named parameter '$_'." unless exists $d{$u};
        $p{$u} = $p{$_};
    }
    for (keys %d) {
        $p{$_} = $d{$_} unless defined $p{$_};
    }
    confess "Unknown resampling algorithm: '$p{RESAMPLEALG}'." 
        unless exists $Geo::GDAL::RIO_RESAMPLING_STRING2INT{$p{RESAMPLEALG}};
    $p{RESAMPLEALG} = $Geo::GDAL::RIO_RESAMPLING_STRING2INT{$p{RESAMPLEALG}};
    unless ($Geo::GDAL::TYPE_INT2STRING{$p{BUFTYPE}}) {
        confess "Unknown data type: '$p{BUFTYPE}'." 
            unless exists $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
        $p{BUFTYPE} = $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
    }
    $self->_ReadRaster($p{XOFF},$p{YOFF},$p{XSIZE},$p{YSIZE},$p{BUFXSIZE},$p{BUFYSIZE},$p{BUFTYPE},$p{BUFPIXELSPACE},$p{BUFLINESPACE},$p{RESAMPLEALG},$p{PROGRESS},$p{PROGRESSDATA});
}

sub WriteRaster {
    my $self = shift;
    my ($width, $height) = $self->Size;
    my ($type) = $self->DataType;
    my %d = (
        XOFF => 0,
        YOFF => 0,
        XSIZE => $width,
        YSIZE => $height,
        BUF => undef,
        BUFXSIZE => undef,
        BUFYSIZE => undef,
        BUFTYPE => $type,
        BUFPIXELSPACE => 0,
        BUFLINESPACE => 0
        );
    my %p;
    my $t;
    if (defined $_[0]) {
        $t = uc($_[0]); 
        $t =~ s/_//g;
    }
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %p = %{$_[0]};
    } elsif (@_ % 2 == 0 and (defined $t and exists $d{$t})) {
        %p = @_;
    } else {
        ($p{xoff},$p{yoff},$p{xsize},$p{ysize},$p{buf},$p{buf_xsize},$p{buf_ysize},$p{buf_type},$p{buf_pixel_space},$p{buf_line_space}) = @_;
    }
    for (keys %p) {
        my $u = uc($_);
        $u =~ s/_//g;
        carp "Unknown named parameter '$_'." unless exists $d{$u};
        $p{$u} = $p{$_};
    }
    for (keys %d) {
        $p{$_} = $d{$_} unless defined $p{$_};
    }
    unless ($Geo::GDAL::TYPE_INT2STRING{$p{BUFTYPE}}) {
        confess "Unknown data type: '$p{BUFTYPE}'." 
            unless exists $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
        $p{BUFTYPE} = $Geo::GDAL::TYPE_STRING2INT{$p{BUFTYPE}};
    }
    $self->_WriteRaster($p{XOFF},$p{YOFF},$p{XSIZE},$p{YSIZE},$p{BUF},$p{BUFXSIZE},$p{BUFYSIZE},$p{BUFTYPE},$p{BUFPIXELSPACE},$p{BUFLINESPACE});
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

# GetMaskBand should be redefined and the result should be put into 
# %Geo::GDAL::Dataset::BANDS

# GetOverview should be redefined and the result should be put into 
# %Geo::GDAL::Dataset::BANDS

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




package Geo::GDAL::ColorTable;
use strict;
use warnings;
use Carp;
use vars qw/
    %PALETTE_INTERPRETATION_STRING2INT %PALETTE_INTERPRETATION_INT2STRING
    /;
for my $string (qw/Gray RGB CMYK HLS/) {
    my $int = eval "\$Geo::GDAL::Constc::GPI_$string";
    $PALETTE_INTERPRETATION_STRING2INT{$string} = $int;
    $PALETTE_INTERPRETATION_INT2STRING{$int} = $string;
}
%}

%feature("shadow") GDALColorTableShadow(GDALPaletteInterp palette = GPI_RGB)
%{
use Carp;
sub new {
    my($pkg, $pi) = @_;
    $pi = $PALETTE_INTERPRETATION_STRING2INT{$pi} if defined $pi and exists $PALETTE_INTERPRETATION_STRING2INT{$pi};
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
    confess $@ if $@;
}

sub ColorEntry {
    my $self = shift;
    my $index = shift;
    SetColorEntry($self, $index, @_) if @_ > 0;
    GetColorEntry($self, $index) if defined wantarray;
}

sub ColorTable {
    my $self = shift;
    my @table;
    if (@_) {
        my $index = 0;
        for my $color (@_) {
            push @table, [ColorEntry($self, $index, @$color)];
            $index++;
        }
    } else {
        for (my $index = 0; $index < GetCount($self); $index++) {
            push @table, [ColorEntry($self, $index)];
        }
    }
    return @table;
}
*ColorEntries = *ColorTable;




package Geo::GDAL::RasterAttributeTable;
use strict;
use warnings;
use Carp;
use vars qw/ %BANDS
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
    delete $BANDS{$self};
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
    confess "Unknown RAT column type: '$type'." unless exists $FIELD_TYPE_STRING2INT{$type};
    confess "Unknown RAT column usage: '$usage'." unless exists $FIELD_USAGE_STRING2INT{$usage};
    for my $color (qw/Red Green Blue Alpha/) {
        carp "RAT column type will be 'Integer' for usage '$color'." if $usage eq $color and $type ne 'Integer';
    }
    $type = $FIELD_TYPE_STRING2INT{$type};
    $usage = $FIELD_USAGE_STRING2INT{$usage};
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
    eval {
        Geo::GDAL::VSIFCloseL($self);
    };
    if ($@) {
        confess "Cannot close file: $@.";
    }
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
    eval {
        Geo::GDAL::VSIFTruncateL($self, $new_size);
    };
    if ($@) {
        confess "Cannot truncate file: $@.";
    }
}

sub MkDir {
    my ($path) = @_;
    my $mode = 0; # unused in CPL
    eval {
        Geo::GDAL::Mkdir($path, $mode);
    };
    if ($@) {
        confess "Cannot make directory \"$path\": $@.";
    }
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
    eval {
        Geo::GDAL::Rename($old, $new);
    };
    if ($@) {
        confess "Cannot rename file \"$old\": $@.";
    }
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
        confess "Cannot remove directory \"$dirname\"$r: $@.";
    }
}
*Rmdir = *RmDir;

sub Stat {
    my ($path) = @_;
    eval {
        Geo::GDAL::Stat($path);
    };
    if ($@) {
        confess "Cannot stat file \"$path\": $@.";
    }
}

sub Unlink {
    my ($filename) = @_;
    eval {
        Geo::GDAL::Unlink($filename);
    };
    if ($@) {
        confess "Cannot unlink file \"$filename\": $@.";
    }
}




package Geo::GDAL::GeoTransform;
use strict;
use warnings;
use Carp;

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

sub FromGCPs {
    my @GCPs;
    my $ApproxOK = 1;
    if (ref($_[0]) eq 'ARRAY') {
        @GCPs = @{$_[0]};
        $ApproxOK = $_[1] if defined $_[1];
    } else {
        @GCPs = @_;
        $ApproxOK = pop @GCPs if !ref($GCPs[$#GCPs]);
    }
    my $self = Geo::GDAL::GCPsToGeoTransform(\@GCPs, $ApproxOK);
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
    unless (defined wantarray) {
        @$self = @inv;
    } else {
        return new(@inv);
    }
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
