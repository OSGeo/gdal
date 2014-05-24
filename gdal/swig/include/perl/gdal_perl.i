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

%rename (_GetDataTypeSize) GetDataTypeSize;
%rename (_DataTypeIsComplex) DataTypeIsComplex;

%rename (_GetDriver) GetDriver;

%rename (_Open) Open;
%newobject _Open;

%rename (_OpenShared) OpenShared;
%newobject _OpenShared;

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
    use strict;
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

    our $VERSION = '2.0000';
    our $GDAL_VERSION = '2.0.0';
    use vars qw/
        %TYPE_STRING2INT %TYPE_INT2STRING
        %ACCESS_STRING2INT %ACCESS_INT2STRING
        %RESAMPLING_STRING2INT %RESAMPLING_INT2STRING
        %NODE_TYPE_STRING2INT %NODE_TYPE_INT2STRING
        /;
    for my $string (qw/Unknown Byte UInt16 Int16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64/) {
        my $int = eval "\$Geo::GDAL::Constc::GDT_$string";
        $TYPE_STRING2INT{$string} = $int;
        $TYPE_INT2STRING{$int} = $string;
    }
    for my $string (qw/ReadOnly Update/) {
        my $int = eval "\$Geo::GDAL::Constc::GA_$string";
        $ACCESS_STRING2INT{$string} = $int;
        $ACCESS_INT2STRING{$int} = $string;
    }
    for my $string (qw/NearestNeighbour Bilinear Cubic CubicSpline/) {
        my $int = eval "\$Geo::GDAL::Constc::GRA_$string";
        $RESAMPLING_STRING2INT{$string} = $int;
        $RESAMPLING_INT2STRING{$int} = $string;
    }
    {
        my $int = 0;
        for my $string (qw/Element Text Attribute Comment Literal/) {
            $NODE_TYPE_STRING2INT{$string} = $int;
            $NODE_TYPE_INT2STRING{$int} = $string;
            $int++;
        }
    }
    sub RELEASE_PARENTS {
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
        $t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
        return _GetDataTypeSize($t);
    }
    sub DataTypeValueRange {
            my $t = shift;
        # these values are from gdalrasterband.cpp
        return (0,255) if $t =~ /^Byte$/;
        return (0,65535) if $t =~ /^UInt16$/;
        return (-32768,32767) if $t =~ /Int16$/; # also CInt
        return (0,4294967295) if $t =~ /^UInt32$/;
        return (-2147483648,2147483647) if $t =~ /Int32$/; # also CInt
        return (-4294967295.0,4294967295.0) if $t =~ /Float32$/; # also CFloat
        return (-4294967295.0,4294967295.0) if $t =~ /Float64$/; # also CFloat
        croak "GDAL does not support data type '$t'";
    }
    sub DataTypeIsComplex {
        my $t = shift;
        $t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
        return _DataTypeIsComplex($t);
    }
    sub PackCharacter {
        my $t = shift;
        $t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
        my $is_big_endian = unpack("h*", pack("s", 1)) =~ /01/; # from Programming Perl
        return 'C' if $t =~ /^Byte$/;
        return ($is_big_endian ? 'n': 'v') if $t =~ /^UInt16$/;
        return 's' if $t =~ /^Int16$/;
        return ($is_big_endian ? 'N' : 'V') if $t =~ /^UInt32$/;
        return 'l' if $t =~ /^Int32$/;
        return 'f' if $t =~ /^Float32$/;
        return 'd' if $t =~ /^Float64$/;
        croak "data type '$t' is not known in Geo::GDAL::PackCharacter";
    }
    sub Drivers {
        my @drivers;
        for my $i (0..GetDriverCount()-1) {
            push @drivers, _GetDriver($i);
        }
        return @drivers;
    }
    sub GetDriver {
        my $driver = shift;
        return _GetDriver($driver) if $driver =~ /^\d/;
        return GetDriverByName($driver);
    }
    *Driver = *GetDriver;
    sub Open {
        my @p = @_;
        $p[1] = $ACCESS_STRING2INT{$p[1]} if $p[1] and exists $ACCESS_STRING2INT{$p[1]};
        return _Open(@p);
    }
    sub OpenShared {
        my @p = @_;
        $p[1] = $ACCESS_STRING2INT{$p[1]} if $p[1] and exists $ACCESS_STRING2INT{$p[1]};
        return _OpenShared(@p);
    }
    sub ComputeMedianCutPCT {
            $_[6] = 1 if $_[5] and not defined $_[6];
        _ComputeMedianCutPCT(@_);
    }
    sub DitherRGB2PCT {
            $_[6] = 1 if $_[5] and not defined $_[6];
        _DitherRGB2PCT(@_);
    }
    sub ComputeProximity {
            $_[4] = 1 if $_[3] and not defined $_[4];
        _ComputeProximity(@_);
    }
    sub RasterizeLayer {
            $_[8] = 1 if $_[7] and not defined $_[8];
        _RasterizeLayer(@_);
    }
    sub Polygonize {
        my @params = @_;
        $params[6] = 1 if $params[5] and not defined $params[6];
        $params[3] = $params[2]->GetLayerDefn->GetFieldIndex($params[3]) unless $params[3] =~ /^\d/;
        _Polygonize(@params);
    }
    sub SieveFilter {
            $_[7] = 1 if $_[6] and not defined $_[7];
        _SieveFilter(@_);
    }
    sub RegenerateOverviews {
            $_[4] = 1 if $_[3] and not defined $_[4];
        _RegenerateOverviews(@_);
    }
    sub RegenerateOverview {
            $_[4] = 1 if $_[3] and not defined $_[4];
        _RegenerateOverview(@_);
    }
    sub ReprojectImage {
            $_[8] = 1 if $_[7] and not defined $_[8];
        $_[4] = $RESAMPLING_STRING2INT{$_[4]} if $_[4] and exists $RESAMPLING_STRING2INT{$_[4]};
        return _ReprojectImage(@_);
    }
    sub AutoCreateWarpedVRT {
        $_[3] = $RESAMPLING_STRING2INT{$_[3]} if $_[3] and exists $RESAMPLING_STRING2INT{$_[3]};
        return _AutoCreateWarpedVRT(@_);
    }

    package Geo::GDAL::MajorObject;
    use vars qw/@DOMAINS/;
    use strict;
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
    use vars qw/@CAPABILITIES @DOMAINS/;
    use strict;
    @CAPABILITIES = qw/Create CreateCopy VirtualIO/;
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
        return split /\s+/, $h->{DMD_CREATIONDATATYPES};
    }
    sub CreateDataset {
        my @p = @_;
        $p[5] = $Geo::GDAL::TYPE_STRING2INT{$p[5]} if $p[5] and exists $Geo::GDAL::TYPE_STRING2INT{$p[5]};
        return _Create(@p);
    }
    *Create = *CreateDataset;
    *Copy = *CreateCopy;

    package Geo::GDAL::Dataset;
    use strict;
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
        $p[1] = $Geo::GDAL::TYPE_STRING2INT{$p[1]} if $p[1] and exists $Geo::GDAL::TYPE_STRING2INT{$p[1]};
        return _AddBand(@p);
    }
    sub Projection {
        my($self, $proj) = @_;
        SetProjection($self, $proj) if defined $proj;
        GetProjection($self) if defined wantarray;
    }
    sub GeoTransform {
        my $self = shift;
        SetGeoTransform($self, \@_) if @_ > 0;
        return unless defined wantarray;
        my $t = GetGeoTransform($self);
        return @$t;
    }
    sub GCPs {
        my $self = shift;
        if (@_ > 0) {
            my $proj = pop @_;
            SetGCPs($self, \@_, $proj);
        }
        return unless defined wantarray;
        my $proj = GetGCPProjection($self);
        my $GCPs = GetGCPs($self);
        return (@$GCPs, $proj);
    }

    package Geo::GDAL::Band;
    use strict;
    use POSIX;
    use Carp;
    use Scalar::Util 'blessed';
    use vars qw/
        @COLOR_INTERPRETATIONS
        %COLOR_INTERPRETATION_STRING2INT %COLOR_INTERPRETATION_INT2STRING @DOMAINS
        /;
    @COLOR_INTERPRETATIONS = qw/Undefined GrayIndex PaletteIndex RedBand GreenBand BlueBand AlphaBand 
                    HueBand SaturationBand LightnessBand CyanBand MagentaBand YellowBand BlackBand/;
    for my $string (@COLOR_INTERPRETATIONS) {
        my $int = eval "\$Geo::GDAL::Constc::GCI_$string";
        $COLOR_INTERPRETATION_STRING2INT{$string} = $int;
        $COLOR_INTERPRETATION_INT2STRING{$int} = $string;
    }
    @DOMAINS = qw/IMAGE_STRUCTURE RESAMPLING/;
    sub Domains {
        return @DOMAINS;
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
        GetUnitType($self);
    }
    sub ScaleAndOffset {
        my $self = shift;
        SetScale($self, $_[0]) if @_ > 0 and defined $_[0];
        SetOffset($self, $_[1]) if @_ > 1 and defined $_[1];
        (GetScale($self), GetOffset($self));
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
        my $w = $xsize * Geo::GDAL::GetDataTypeSize($self->{DataType})/8;
        for my $i (0..$ysize-1) {
            my $scanline = pack($pc."[$xsize]", @{$data->[$i]});
            $self->WriteRaster( $xoff, $yoff+$i, $xsize, 1, $scanline );
        }
    }
    sub ColorInterpretation {
        my($self, $ci) = @_;
        if ($ci) {
            $ci = $COLOR_INTERPRETATION_STRING2INT{$ci} if exists $COLOR_INTERPRETATION_STRING2INT{$ci};
            SetRasterColorInterpretation($self, $ci);
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
        $Geo::GDAL::RasterAttributeTable::BANDS{$r} = $self;
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
                        callback => undef,
                        callback_data => undef);
        my %params;
        if (!defined($_[0]) or (blessed($_[0]) and $_[0]->isa('Geo::OGR::DataSource'))) {
            ($params{DataSource}, $params{LayerConstructor},
             $params{ContourInterval}, $params{ContourBase},
             $params{FixedLevels}, $params{NoDataValue}, 
             $params{IDField}, $params{ElevField},
             $params{callback}, $params{callback_data}) = @_;
        } else {
            %params = @_;
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
        $params{callback_data} = 1 if $params{callback} and not defined $params{callback_data};
        ContourGenerate($self, $params{ContourInterval}, $params{ContourBase}, $params{FixedLevels},
                        $params{NoDataValue}, $layer, $params{IDField}, $params{ElevField},
                        $params{callback}, $params{callback_data});
        return $layer;
    }
    sub FillNodata {
      croak 'usage: Geo::GDAL::Band->FillNodata($mask)' unless blessed($_[1]) and $_[1]->isa('Geo::GDAL::Band');
      $_[2] = 10 unless defined $_[2];
      $_[3] = 0 unless defined $_[3];
      $_[4] = undef unless defined $_[4];
      $_[5] = undef unless defined $_[5];
      $_[6] = undef unless defined $_[6];
      Geo::GDAL::FillNodata(@_);
    }
    *GetBandNumber = *GetBand;

    package Geo::GDAL::ColorTable;
    use strict;
    use vars qw/
        %PALETTE_INTERPRETATION_STRING2INT %PALETTE_INTERPRETATION_INT2STRING
        /;
    for my $string (qw/Gray RGB CMYK HLS/) {
        my $int = eval "\$Geo::GDAL::Constc::GPI_$string";
        $PALETTE_INTERPRETATION_STRING2INT{$string} = $int;
        $PALETTE_INTERPRETATION_INT2STRING{$int} = $string;
    }
    sub create {
        my($pkg, $pi) = @_;
        $pi = $PALETTE_INTERPRETATION_STRING2INT{$pi} if defined $pi and exists $PALETTE_INTERPRETATION_STRING2INT{$pi};
        my $self = Geo::GDALc::new_ColorTable($pi);
        bless $self, $pkg if defined($self);
    }
    sub GetPaletteInterpretation {
        my $self = shift;
        return $PALETTE_INTERPRETATION_INT2STRING{GetPaletteInterpretation($self)};
    }
    sub SetColorEntry {
        @_ == 3 ? _SetColorEntry(@_) : _SetColorEntry(@_[0..1], [@_[2..5]]);
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
    use vars qw/ %BANDS
        %FIELD_TYPE_STRING2INT %FIELD_TYPE_INT2STRING
        %FIELD_USAGE_STRING2INT %FIELD_USAGE_INT2STRING
        /;
    for my $string (qw/Integer Real String/) {
        my $int = eval "\$Geo::GDAL::Constc::GFT_$string";
        $FIELD_TYPE_STRING2INT{$string} = $int;
        $FIELD_TYPE_INT2STRING{$int} = $string;
    }
    for my $string (qw/Generic PixelCount Name Min Max MinMax 
                    Red Green Blue Alpha RedMin 
                    GreenMin BlueMin AlphaMin RedMax GreenMax BlueMax AlphaMax 
                    MaxCount/) {
        my $int = eval "\$Geo::GDAL::Constc::GFU_$string";
        $FIELD_USAGE_STRING2INT{$string} = $int;
        $FIELD_USAGE_INT2STRING{$int} = $string;
    }
    sub FieldTypes {
        return keys %FIELD_TYPE_STRING2INT;
    }
    sub FieldUsages {
        return keys %FIELD_USAGE_STRING2INT;
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
        _CreateColumn($self, $name, $FIELD_TYPE_STRING2INT{$type}, $FIELD_USAGE_STRING2INT{$usage});
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
                           GDALProgressFunc callback = NULL,
                           void* callback_data = NULL) {
        return GDALContourGenerate( self, dfContourInterval, dfContourBase,
                                    nFixedLevelCount, padfFixedLevels,
                                    bUseNoData, dfNoDataValue, 
                                    hLayer, iIDField, iElevField,
                                    callback,
                                    callback_data );
    }
    %clear (int nFixedLevelCount, double *padfFixedLevels);
    %clear (int bUseNoData, double dfNoDataValue);
}
