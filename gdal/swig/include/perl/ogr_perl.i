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
      if (!OGR_F_IsFieldSet(self, id))
	  return NULL;
      return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetField(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else {
            if (!OGR_F_IsFieldSet(self, i))
		return NULL;
	    return (const char *) OGR_F_GetFieldAsString(self, i);
	}
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
%rename (_SetGeometryDirectly) SetGeometryDirectly;
%rename (_ExportToWkb) ExportToWkb;
%rename (_GetDriver) GetDriver;

%perlcode %{
    use strict;
    use Carp;
    {
	package Geo::OGR::Driver;
	use strict;
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
	use strict;
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
	sub new {
	    my $pkg = shift;
	    return Geo::OGR::Open(@_);
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
	use strict;
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
	sub Schema {
	    my $self = shift;
	    my %schema;
	    if (@_) {
		%schema = @_;
		# the Name and GeometryType cannot be set
		for my $fd (@{$schema{Fields}}) {
		    if (ref($fd) eq 'HASH') {
			$fd = Geo::OGR::FieldDefn->create(%$fd);
		    }
		    CreateField($self, $fd, $schema{ApproxOK});
		}
	    }
	    return unless defined wantarray;
	    my $defn = $self->GetLayerDefn->Schema;
	    $schema{Name} = $self->GetName();
	    return \%schema;
	}
	sub Row {
	    my $self = shift;
	    my %row = @_;
	    my $f = defined $row{FID} ? $self->GetFeature($row{FID}) : $self->GetNextFeature;
	    my $d = $f->GetDefnRef;
	    my $changed = 0;
	    if (defined $row{Geometry}) {
		if (ref($row{Geometry}) eq 'HASH') {
		    my %geom = %{$row{Geometry}};
		    $geom{GeometryType} = $d->GeometryType unless $geom{GeometryType};
		    $f->SetGeometryDirectly(Geo::OGR::Geometry->create(%geom));
		} else {
		    $f->SetGeometryDirectly($row{Geometry});
		}
		$changed = 1;
	    }
	    for my $fn (keys %row) {
		next if $fn eq 'FID';
		next if $fn eq 'Geometry';
		if (defined $row{$fn}) {
		    $f->SetField($fn, $row{$fn});
		} else {
		    $f->UnsetField($fn);
		}
		$changed = 1;
	    }
	    $self->SetFeature($f) if $changed;
	    return unless defined wantarray;
	    %row = ();
	    my $s = $d->Schema;
	    for my $field (@{$s->{Fields}}) {
		my $n = $field->Name;
		$row{$n} = $f->GetField($n);
	    }
	    $row{FID} = $f->GetFID;
	    $row{Geometry} = $f->GetGeometry;
	    return \%row;
	}
	sub Tuple {
	    my $self = shift;
	    my $FID = shift;
	    my $Geometry = shift;
	    my $f = defined $FID ? $self->GetFeature($FID) : $self->GetNextFeature;
	    my $d = $f->GetDefnRef;
	    my $changed = 0;
	    if (defined $Geometry) {
		if (ref($Geometry) eq 'HASH') {
		    my %geom = %$Geometry;
		    $geom{GeometryType} = $d->GeometryType unless $geom{GeometryType};
		    $f->SetGeometryDirectly(Geo::OGR::Geometry->create(%geom));
		} else {
		    $f->SetGeometryDirectly($Geometry);
		}
		$changed = 1;
	    }
	    my $s = $d->Schema;
	    if (@_) {
		for my $field (@{$s->{Fields}}) {
		    my $v = shift;
		    my $n = $field->Name;
		    defined $v ? $f->SetField($n, $v) : $f->UnsetField($n);
		}
		$changed = 1;
	    }
	    $self->SetFeature($f) if $changed;
	    return unless defined wantarray;
	    my @ret = ($f->GetFID, $f->GetGeometry);
	    my $i = 0;
	    for my $field (@{$s->{Fields}}) {
		push @ret, $f->GetField($i++);
	    }
	    return @ret;
	}
	sub InsertFeature {
	    my $self = shift;
	    my $f = shift;
	    if (ref($f) eq 'HASH') {
		my %row = %$f;
		$f = Geo::OGR::Feature->new($self->GetLayerDefn);
		$f->Row(%row);
	    } elsif (ref($f) eq 'ARRAY') {
		my @tuple = @$f;
		$f = Geo::OGR::Feature->new($self->GetLayerDefn);
		$f->Tuple(@tuple);
	    }
	    $self->CreateFeature($f);
	}
	package Geo::OGR::FeatureDefn;
	sub Schema {
	    my $self = shift;
	    my %schema;
	    if (@_) {
		%schema = @_;
		# the Name cannot be set
		$self->GeomType($schema{GeometryType}) if exists $schema{GeometryType};
		for my $fd (@{$schema{Fields}}) {
		    AddFieldDefn($self, $fd);
		}
	    }
	    return unless defined wantarray;
	    $schema{Name} = $self->GetName();
	    $schema{GeometryType} = $self->GeomType();
	    $schema{Fields} = [];
	    for my $i (0..$self->GetFieldCount-1) {
		push @{$schema{Fields}}, $self->GetFieldDefn($i);
	    }
	    return \%schema;
	}
	sub GeomType {
	    my($self, $type) = @_;
	    if ($type) {
		$type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type} if 
		    $type and exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
		SetGeomType($self, $type);
	    }
	    return $Geo::OGR::Geometry::TYPE_INT2STRING{GetGeomType($self)} if defined wantarray;
	}
	*GeometryType = *GeomType;
	package Geo::OGR::Feature;
	use strict;
	use vars qw /%GEOMETRIES/;
	sub Row {
	    my $self = shift;
	    my %row = @_;
	    $self->SetFID($row{FID}) if defined $row{FID};
	    if (defined $row{Geometry}) {
		if (ref($row{Geometry}) eq 'HASH') {
		    my %geom = %{$row{Geometry}};
		    $geom{GeometryType} = $self->GetDefnRef->GeometryType unless $geom{GeometryType};
		    $self->SetGeometryDirectly(Geo::OGR::Geometry->create(%geom));
		} else {
		    $self->SetGeometryDirectly($row{Geometry});
		}
	    }
	    for my $fn (keys %row) {
		next if $fn eq 'FID';
		next if $fn eq 'Geometry';
		if (defined $row{$fn}) {
		    $self->SetField($fn, $row{$fn});
		} else {
		    $self->UnsetField($fn);
		}
	    }
	    return unless defined wantarray;
	    %row = ();
	    my $s = $self->GetDefnRef->Schema;
	    for my $field (@{$s->{Fields}}) {
		my $n = $field->Name;
		$row{$n} = $self->GetField($n);
	    }
	    $row{FID} = $self->GetFID;
	    $row{Geometry} = $self->GetGeometry;
	    return \%row;
	}
	sub Tuple {
	    my $self = shift;
	    my $FID = shift;
	    my $Geometry = shift;
	    $self->SetFID($FID) if defined $FID;
	    if (defined $Geometry) {
		if (ref($Geometry) eq 'HASH') {
		    my %geom = %$Geometry;
		    $geom{GeometryType} = $self->GetDefnRef->GeometryType unless $geom{GeometryType};
		    $self->SetGeometryDirectly(Geo::OGR::Geometry->create(%geom));
		} else {
		    $self->SetGeometryDirectly($Geometry);
		}
	    }
	    my $s = $self->GetDefnRef->Schema;
	    if (@_) {
		for my $field (@{$s->{Fields}}) {
		    my $v = shift;
		    my $n = $field->Name;
		    defined $v ? $self->SetField($n, $v) : $self->UnsetField($n);
		}
	    }
	    return unless defined wantarray;
	    my @ret = ($self->GetFID, $self->GetGeometry);
	    my $i = 0;
	    for my $field (@{$s->{Fields}}) {
		push @ret, $self->GetField($i++);
	    }
	    return @ret;
	}
	sub GetFieldType {
	    my($self, $field) = @_;
	    return $Geo::OGR::Geometry::TYPE_INT2STRING{_GetFieldType($self, $field)};
	}
	sub Geometry {
	    my $self = shift;
	    SetGeometry($self, $_[0]) if @_;
	    GetGeometryRef($self)->Clone() if defined wantarray;
	}
	sub SetGeometryDirectly {
	    _SetGeometryDirectly(@_);
	    $GEOMETRIES{tied(%{$_[1]})} = $_[0];
	}
	sub GetGeometry {
	    my $self = shift;
	    my $geom = GetGeometryRef($self);
	    $GEOMETRIES{tied(%$geom)} = $self;
	    return $geom;
	}
	package Geo::OGR::FieldDefn;
	use strict;
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
	    my %param = ( Name => 'unnamed', Type => 'String' );
	    if (@_ == 0) {
	    } elsif (@_ == 1) {
		$param{Name} = shift;
	    } else {
		my %known = map {$_ => 1} qw/Name Type Justify Width Precision/;
		unless ($known{$_[0]}) {
		    $param{Name} = shift;
		    $param{Type} = shift;
		} else {
		    my %p = @_;
		    for my $k (keys %known) {
			$param{$k} = $p{$k} if exists $p{$k};
		    }
		}
	    }
	    $param{Type} = $TYPE_STRING2INT{$param{Type}} if defined $param{Type} and exists $TYPE_STRING2INT{$param{Type}};
	    my $self = Geo::OGRc::new_FieldDefn($param{Name}, $param{Type});
	    if (defined($self)) {
		bless $self, $pkg;
		$self->Justify($param{Justify}) if exists $param{Justify};
		$self->Width($param{Width}) if exists $param{Width};
		$self->Precision($param{Precision}) if exists $param{Precision};
	    }
	    return $self;
	}
	sub Name {
	    my $self = shift;
	    SetName($self, $_[0]) if @_;
	    GetName($self) if defined wantarray;
	}
	sub Type {
	    my($self, $type) = @_;
	    if (defined $type) {
		$type = $TYPE_STRING2INT{$type} if $type and exists $TYPE_STRING2INT{$type};
		SetType($self, $type);
	    }
	    return $TYPE_INT2STRING{GetType($self)} if defined wantarray;
	}
	sub Justify {
	    my($self, $justify) = @_;
	    if (defined $justify) {
		$justify = $JUSTIFY_STRING2INT{$justify} if $justify and exists $JUSTIFY_STRING2INT{$justify};
		SetJustify($self, $justify);
	    }
	    return $JUSTIFY_INT2STRING{GetJustify($self)} if defined wantarray;
	}
	sub Width {
	    my $self = shift;
	    SetWidth($self, $_[0]) if @_;
	    GetWidth($self) if defined wantarray;
	}
	sub Precision {
	    my $self = shift;
	    SetPrecision($self, $_[0]) if @_;
	    GetPrecision($self) if defined wantarray;
	}
	sub Schema {
	    my $self = shift;
	    if (@_) {
		my %param = @_;
 		$self->Name($param{Name}) if exists $param{Name};
		$self->Type($param{Type}) if exists $param{Type};
		$self->Justify($param{Justify}) if exists $param{Justify};
		$self->Width($param{Width}) if exists $param{Width};
		$self->Precision($param{Precision}) if exists $param{Precision};
	    }
	    return unless defined wantarray;
	    return { Name => $self->Name, 
		     Type  => $self->Type,
		     Justify  => $self->Justify,
		     Width  => $self->Width,
		     Precision => $self->Precision };
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
	    my($type, $wkt, $wkb, $gml, $points);
	    if (@_ == 1) {
		$type = shift;
	    } else {
		my %param = @_;
		$type = ($param{type} or $param{Type} or $param{GeometryType});
		$wkt = ($param{wkt} or $param{WKT});
		$wkb = ($param{wkb} or $param{WKB});
		$gml = ($param{gml} or $param{GML});
		$points = $param{Points};
	    }
	    $type = $TYPE_STRING2INT{$type} if defined $type and exists $TYPE_STRING2INT{$type};
	    my $self;
	    if (defined $type) {
		croak "unknown GeometryType: $type" unless 
		    exists($TYPE_STRING2INT{$type}) or exists($TYPE_INT2STRING{$type});
		$self = Geo::OGRc::new_Geometry($type);
	    } elsif (defined $wkt) {
		$self = Geo::OGRc::new_Geometry(undef, $wkt, undef, undef);
	    } elsif (defined $wkb) {
		$self = Geo::OGRc::new_Geometry(undef, undef, $wkb, undef);
	    } elsif (defined $gml) {
		$self = Geo::OGRc::new_Geometry(undef, undef, undef, $gml);
	    }
	    bless $self, $pkg if defined $self;
	    $self->Points($points) if $points;
	    return $self;
	}
	sub GeometryType {
	    my $self = shift;
	    return $TYPE_INT2STRING{$self->GetGeometryType};
	}
	sub AddPoint {
	    @_ == 4 ? AddPoint_3D(@_) : AddPoint_2D(@_);
	}
	sub Points {
	    my $self = shift;
	    my $t = $self->GetGeometryType;
	    my $flat = ($t & 0x80000000) == 0;
	    $t = $TYPE_INT2STRING{$t & ~0x80000000};
	    my $points = shift;
	    if ($points) {
		Empty($self);
		if ($t eq 'Unknown' or $t eq 'None' or $t eq 'GeometryCollection') {
		    croak("Can't set points of a geometry of type: $t");
		} elsif ($t eq 'Point') {
		    $flat ? AddPoint_2D($self, @$points[0..1]) : AddPoint_3D($self, @$points[0..2]);
		} elsif ($t eq 'LineString' or $t eq 'LinearRing') {
		    if ($flat) {
			for my $p (@$points) {
			    AddPoint_2D($self, @$p[0..1]);
			}
		    } else{
			for my $p (@$points) {
			    AddPoint_3D($self, @$p[0..2]);
			}
		    }
		} elsif ($t eq 'Polygon') {
		    for my $r (@$points) {
			my $ring = Geo::OGR::Geometry->create('LinearRing');
			$ring->SetCoordinateDimension(3) unless $flat;
			$ring->Points($r);
			$self->AddGeometryDirectly($ring);
		    }
		} elsif ($t eq 'MultiPoint') {
		    for my $p (@$points) {
			my $point = Geo::OGR::Geometry->create($flat ? 'Point' : 'Point25D');
			$point->Points($p);
			$self->AddGeometryDirectly($point);
		    }
		} elsif ($t eq 'MultiLineString') {
		    for my $l (@$points) {
			my $linestring = Geo::OGR::Geometry->create($flat ? 'LineString' : 'LineString25D');
			$linestring->Points($l);
			$self->AddGeometryDirectly($linestring);
		    }
		} elsif ($t eq 'MultiPolygon') {
		    for my $p (@$points) {
			my $polygon = Geo::OGR::Geometry->create($flat ? 'Polygon' : 'Polygon25D');
			$polygon->Points($p);
			$self->AddGeometryDirectly($polygon);
		    }
		}
	    }
	    return unless defined wantarray;
	    $self->_GetPoints($flat);
	}
	sub _GetPoints {
	    my($self, $flat) = @_;
	    my @points;
	    my $n = $self->GetGeometryCount;
	    if ($n) {
		for my $i (0..$n-1) {
		    push @points, $self->GetGeometryRef($i)->_GetPoints($flat);
		}
	    } else {
		$n = $self->GetPointCount;
		if ($n == 1) {
		    push @points, $flat ? [$self->GetX, $self->GetY] : [$self->GetX, $self->GetY, $self->GetZ];
		} else {
		    my $i;
		    if ($flat) {
			for my $i (0..$n-1) {
			    push @points, [$self->GetX($i), $self->GetY($i)];
			}
		    } else {
			for my $i (0..$n-1) {
			    push @points, [$self->GetX($i), $self->GetY($i), $self->GetZ($i)];
			}
		    }
		}
	    }
	    return \@points;
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
