/******************************************************************************
 *
 * Project:  OGR SWIG Interface declarations for Perl.
 * Purpose:  OGR declarations.
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

  UseExceptions();
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
  
%}

%include cpl_exceptions.i

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%import typemaps_perl.i

%import destroy.i

ALTERED_DESTROY(OGRDataSourceShadow, OGRc, delete_DataSource)
ALTERED_DESTROY(OGRFeatureShadow, OGRc, delete_Feature)
ALTERED_DESTROY(OGRFeatureDefnShadow, OGRc, delete_FeatureDefn)
ALTERED_DESTROY(OGRFieldDefnShadow, OGRc, delete_FieldDefn)
ALTERED_DESTROY(OGRGeometryShadow, OGRc, delete_Geometry)

%extend OGRGeometryShadow {

    %rename (AddPoint_3D) AddPoint;

}

%extend OGRFeatureShadow {

  const char* GetField(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetField(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return (const char *) OGR_F_GetFieldAsString(self, i);
    }
    return NULL;
  }

}

%extend OGRGeometryShadow {

    void Move(double dx, double dy, double dz = 0) {
	int n = OGR_G_GetGeometryCount(self);
	if (n > 0) {
	    int i;
	    for (i = 0; i < n; i++) {
		OGRGeometryShadow *g = (OGRGeometryShadow*)OGR_G_GetGeometryRef(self, i);
		OGRGeometryShadow_Move(g, dx, dy, dz);
	    }
	} else {
	    int i;
	    for (i = 0; i < OGR_G_GetPointCount(self); i++) {
		double x = OGR_G_GetX(self, i);
		double y = OGR_G_GetY(self, i);
		double z = OGR_G_GetZ(self, i);
		OGR_G_SetPoint(self, i, x+dx, y+dy, z+dz);
	    }
	}
    }
    
}

%rename (_GetLayerByIndex) GetLayerByIndex;
%rename (_GetLayerByName) GetLayerByName;
%rename (_CreateLayer) CreateLayer;
%rename (_GetFieldType) GetFieldType;
%rename (_ExportToWkb) ExportToWkb;
%rename (_GetDriver) GetDriver;

%perlcode %{
    use Carp;
    {
	package Geo::OGR::Driver;
	use vars qw /@CAPABILITIES/;
	for my $s (qw/CreateDataSource DeleteDataSource/) {
	    my $cap = eval "\$Geo::OGR::ODrC$s";
	    push @CAPABILITIES, $cap;
	}
	sub Capabilities {
	    return @CAPABILITIES if @_ == 0;
	    my $self = shift;
	    my @cap;
	    for my $cap (@CAPABILITIES) {
		push @cap, $cap if TestCapability($self, $cap);
	    }
	    return @cap;
	}
	package Geo::OGR::DataSource;
	use vars qw /@CAPABILITIES %LAYERS/;
	for my $s (qw/CreateLayer DeleteLayer/) {
	    my $cap = eval "\$Geo::OGR::ODsC$s";
	    push @CAPABILITIES, $cap;
	}
	sub Capabilities {
	    return @CAPABILITIES if @_ == 0;
	    my $self = shift;
	    my @cap;
	    for my $cap (@CAPABILITIES) {
		push @cap, $cap if TestCapability($self, $cap);
	    }
	    return @cap;
	}
	sub Open {
	    return Geo::OGR::Open(@_);
	}
	sub OpenShared {
	    return Geo::OGR::OpenShared(@_);
	}
	sub GetLayerByIndex {
	    my($self, $index) = @_;
	    $index = 0 unless defined $index;
	    my $layer = _GetLayerByIndex($self, $index);
	    $LAYERS{tied(%$layer)} = $self;
	    return $layer;
	}
	sub GetLayerByName {
	    my($self, $name) = @_;
	    my $layer = _GetLayerByName($self, $name);
	    $LAYERS{tied(%$layer)} = $self;
	    return $layer;
	}
	sub CreateLayer {
	    my @p = @_;
	    $p[3] = $Geo::OGR::Geometry::TYPE_STRING2INT{$p[3]} if 
		$p[3] and exists $Geo::OGR::Geometry::TYPE_STRING2INT{$p[3]};
	    my $layer = _CreateLayer(@p);
	    $LAYERS{tied(%$layer)} = $p[0];
	    return $layer;
	}
	package Geo::OGR::Layer;
	use vars qw /@CAPABILITIES/;
	for my $s (qw/RandomRead SequentialWrite RandomWrite 
		   FastSpatialFilter FastFeatureCount FastGetExtent 
		   CreateField Transactions DeleteFeature FastSetNextByIndex/) {
	    my $cap = eval "\$Geo::OGR::OLC$s";
	    push @CAPABILITIES, $cap;
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
	    delete $Geo::OGR::DataSource::LAYERS{$self};
	}
	sub Capabilities {
	    return @CAPABILITIES if @_ == 0;
	    my $self = shift;
	    my @cap;
	    for my $cap (@CAPABILITIES) {
		push @cap, $cap if TestCapability($self, $cap);
	    }
	    return @cap;
	}
	package Geo::OGR::FeatureDefn;
	sub GeomType {
	    my($self, $type) = @_;
	    if (defined $type) {
		$type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type} if 
		    $type and exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
		SetGeomType($self, $type);
	    } else {
		return GetGeomType($self);
	    }
	}
	package Geo::OGR::Feature;
	use vars qw /%GEOMETRIES/;
	sub GetFieldType {
	    my($self, $field) = @_;
	    return $Geo::OGR::Geometry::TYPE_INT2STRING{_GetFieldType($self, $field)};
	}
	sub GetGeometry {
	    my $self = shift;
	    my $geom =GetGeometryRef($self);
	    $GEOMETRIES{tied(%$geom)} = $self;
	    return $geom;
	}
	package Geo::OGR::FieldDefn;
	use vars qw /
	    %TYPE_STRING2INT %TYPE_INT2STRING
	    %JUSTIFY_STRING2INT %JUSTIFY_INT2STRING
	    /;
	for my $string (qw/Integer IntegerList Real RealList String StringList 
			WideString WideStringList Binary Date Time DateTime/) {
	    my $int = eval "\$Geo::OGR::OFT$string";
	    $TYPE_STRING2INT{$string} = $int;
	    $TYPE_INT2STRING{$int} = $string;
	}
	for my $string (qw/Undefined Left Right/) {
	    my $int = eval "\$Geo::OGR::OJ$string";
	    $JUSTIFY_STRING2INT{$string} = $int;
	    $JUSTIFY_INT2STRING{$int} = $string;
	}
	sub create {
	    my $pkg = shift;
	    my @p = @_;
	    $p[1] = $TYPE_STRING2INT{$p[1]} if $p[1] and exists $TYPE_STRING2INT{$p[1]};
	    my $self = Geo::OGRc::new_FieldDefn(@p);
	    bless $self, $pkg if defined($self);
	}
	sub Name {
	    my($self, $name) = @_;
	    defined $name ? SetName($self, $name) : GetName($self);
	}
	sub Type {
	    my($self, $type) = @_;
	    if (defined $type) {
		$type = $TYPE_STRING2INT{$type} if $type and exists $TYPE_STRING2INT{$type};
		SetType($self, $type);
	    } else {
		return $TYPE_INT2STRING{GetType($self)};
	    }
	}
	sub Justify {
	    my($self, $justify) = @_;
	    if (defined $justify) {
		$justify = $JUSTIFY_STRING2INT{$justify} if $justify and exists $JUSTIFY_STRING2INT{$justify};
		SetJustify($self, $justify);
	    } else {
		return $JUSTIFY_INT2STRING{GetJustify($self)};
	    }
	}
	sub Width {
	    my($self, $w) = @_;
	    defined $w ? SetWidth($self, $w) : GetWidth($self);
	}
	sub Precision {
	    my($self, $p) = @_;
	    defined $p ? SetPrecision($self, $p) : GetPrecision($self);
	}
	package Geo::OGR::Geometry;
	use Carp;
	use vars qw /
	    %TYPE_STRING2INT %TYPE_INT2STRING
	    %BYTE_ORDER_STRING2INT %BYTE_ORDER_INT2STRING
	    /;
	for my $string (qw/Unknown 
			Point LineString Polygon 
			MultiPoint MultiLineString MultiPolygon GeometryCollection 
			None LinearRing
			Point25D LineString25D Polygon25D 
			MultiPoint25D MultiLineString25D MultiPolygon25D GeometryCollection25D/) {
	    my $int = eval "\$Geo::OGR::wkb$string";
	    $TYPE_STRING2INT{$string} = $int;
	    $TYPE_INT2STRING{$int} = $string;
	}
	for my $string (qw/XDR NDR/) {
	    my $int = eval "\$Geo::OGR::wkb$string";
	    $BYTE_ORDER_STRING2INT{$string} = $int;
	    $BYTE_ORDER_INT2STRING{$int} = $string;
	}
	sub RELEASE_PARENTS {
	    my $self = shift;
	    delete $Geo::OGR::Feature::GEOMETRIES{$self};
	}
	sub create { # alternative constructor since swig created new can't be overridden(?)
	    my $pkg = shift;
	    my($type, $wkt, $wkb, $gml);
	    if (@_ == 1) {
		$type = shift;
	    } else {
		my %param = @_;
		$type = $param{type};
		$wkt = ($param{wkt} or $param{WKT});
		$wkb = ($param{wkb} or $param{WKB});
		$gml = ($param{gml} or $param{GML});
	    }
	    $type = $TYPE_STRING2INT{$type} if defined $type and exists $TYPE_STRING2INT{$type};
	    my $self;
	    if (defined $type) {
		$self = Geo::OGRc::new_Geometry($type);
	    } elsif (defined $wkt) {
		$self = Geo::OGRc::new_Geometry(undef, $wkt, undef, undef);
	    } elsif (defined $wkb) {
		$self = Geo::OGRc::new_Geometry(undef, undef, $wkb, undef);
	    } elsif (defined $gml) {
		$self = Geo::OGRc::new_Geometry(undef, undef, undef, $gml);
	    }
	    bless $self, $pkg if defined $self;
	}
	sub GeometryType {
	    my $self = shift;
	    return $TYPE_INT2STRING{$self->GetGeometryType};
	}
	sub AddPoint {
	    @_ == 4 ? AddPoint_3D(@_) : AddPoint_2D(@_);
	}
	sub ExportToWkb {
	    my($self, $bo) = @_;
	    $bo = $BYTE_ORDER_STRING2INT{$bo} if defined $bo and exists $BYTE_ORDER_STRING2INT{$bo};
	    return _ExportToWkb($self, $bo);
	}
    }
    sub GeometryType {
	my($type_or_name) = @_;
	if (defined $type_or_name) {
	    return $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name} if 
		exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name};
	    return $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name} if 
		exists $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name};
	    croak "unknown geometry type or name: $type_or_name";
	} else {
	    return keys %Geo::OGR::Geometry::TYPE_STRING2INT;
	}
    }
    sub RELEASE_PARENTS {
    }
    sub GeometryTypes {
	return keys %Geo::OGR::Geometry::TYPE_STRING2INT;
    }
    sub GetDriver {
	my($name_or_number) = @_;
	return _GetDriver($name_or_number) if $name_or_number =~ /^\d/;
	return GetDriverByName($name_or_number);
    }
%}
