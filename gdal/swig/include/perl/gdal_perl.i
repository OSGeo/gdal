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

%include cpl_exceptions.i

%rename (GetMetadata) GetMetadata_Dict;
%ignore GetMetadata_List;

%import typemaps_perl.i

%import destroy.i

/*ALTERED_DESTROY(GDALRasterBandShadow, GDALc, delete_Band) !does not work! */
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

%rename (_ReprojectImage) ReprojectImage;
%newobject _ReprojectImage;

%rename (_AutoCreateWarpedVRT) AutoCreateWarpedVRT;
%newobject _AutoCreateWarpedVRT;

%rename (_Create) Create;
%newobject _Create;

%rename (_GetRasterBand) GetRasterBand;
%rename (_AddBand) AddBand;

%rename (_GetPaletteInterpretation) GetPaletteInterpretation;

%rename (_SetColorEntry) SetColorEntry;

%perlcode %{
    use Carp;
    use Geo::GDAL::Const;
    use Geo::OGR;
    use Geo::OSR;
    our $VERSION = '0.21';
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
    sub GetDataTypeSize {
	my $t = shift;
	$t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
	return _GetDataTypeSize($t);
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
	croak "unsupported data type: $t";
    }
    sub GetDriver {
	my $driver = shift;
	return _GetDriver($driver) if $driver =~ /^\d/;
	return GetDriverByName($driver);
    }
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
    sub ReprojectImage {
	my @p = @_;
	$p[4] = $RESAMPLING_STRING2INT{$p[4]} if $p[4] and exists $RESAMPLING_STRING2INT{$p[4]};
	return _ReprojectImage(@p);
    }
    sub AutoCreateWarpedVRT {
	my @p = @_;
	$p[3] = $RESAMPLING_STRING2INT{$p[3]} if $p[3] and exists $RESAMPLING_STRING2INT{$p[3]};
	return _AutoCreateWarpedVRT(@p);
    }
    package Geo::GDAL::MajorObject;
    sub Description {
	my($self, $desc) = @_;
	defined $desc ? SetDescription($self, $desc) : GetDescription($self);
    }
    sub Metadata {
	my $self = shift;
	my $metadata = shift if ref $_[0];
	my $domain = shift;
	$domain = '' unless defined $domain;
	defined $metadata ? SetMetadata($self, $metadata, $domain) : GetMetadata($self, $domain);
    }
    package Geo::GDAL::Driver;
    sub Create {
	my @p = @_;
	$p[5] = $Geo::GDAL::TYPE_STRING2INT{$p[5]} if $p[5] and exists $Geo::GDAL::TYPE_STRING2INT{$p[5]};
	return _Create(@p);
    }
    package Geo::GDAL::Dataset;
    use vars qw/%BANDS/;
    sub Open {
	return Geo::GDAL::Open(@_);
    }
    sub OpenShared {
	return Geo::GDAL::OpenShared(@_);
    }
    sub GetRasterBand {
	my($self, $index) = @_;
	my $band = _GetRasterBand($self, $index);
	$BANDS{tied(%{$band})} = $self;
	return $band;
    }
    sub AddBand {
	my @p = @_;
	$p[1] = $Geo::GDAL::TYPE_STRING2INT{$p[1]} if $p[1] and exists $Geo::GDAL::TYPE_STRING2INT{$p[1]};
	return _AddBand(@p);
    }
    package Geo::GDAL::Band;
    use vars qw/
	%COLOR_INTERPRETATION_STRING2INT %COLOR_INTERPRETATION_INT2STRING
	/;
    for my $string (qw/Undefined GrayIndex PaletteIndex RedBand GreenBand BlueBand AlphaBand 
		    HueBand SaturationBand LightnessBand CyanBand MagentaBand YellowBand BlackBand/) {
	my $int = eval "\$Geo::GDAL::Constc::GCI_$string";
	$COLOR_INTERPRETATION_STRING2INT{$string} = $int;
	$COLOR_INTERPRETATION_INT2STRING{$int} = $string;
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
	my($self, $ct) = @_;
	$ct ? SetRasterColorTable($self, $ct) : GetRasterColorTable($self);
    }
    package Geo::GDAL::ColorTable;
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
	my($self) = @_;
	return $PALETTE_INTERPRETATION_INT2STRING{GetPaletteInterpretation($self)};
    }
    sub SetColorEntry {
	@_ == 3 ? _SetColorEntry(@_) : _SetColorEntry(@_[0..1], @{$_[2]});
    }
 %}
