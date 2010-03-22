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

  /*UseExceptions(); is set by GDAL module */
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
    %rename (SetPoint_3D) SetPoint;
    %rename (GetPoint_3D) GetPoint;

}

%extend OGRFeatureShadow {

  %rename (_UnsetField) UnsetField;
  %rename (_SetField) SetField;

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
%rename (_DeleteLayer) DeleteLayer;
%rename (_GetFieldType) GetFieldType;
%rename (_SetGeometryDirectly) SetGeometryDirectly;
%rename (_ExportToWkb) ExportToWkb;
%rename (_GetDriver) GetDriver;
%rename (_TestCapability) TestCapability;

%perlcode %{
    use strict;
    use Carp;
    {
	package Geo::OGR::Driver;
	use strict;
	use vars qw /@CAPABILITIES %CAPABILITIES/;
        @CAPABILITIES = qw/CreateDataSource DeleteDataSource/; 
	for my $s (@CAPABILITIES) {
	    my $cap = eval "\$Geo::OGR::ODrC$s";
	    $CAPABILITIES{$s} = $cap;
	}
	sub Capabilities {
	    return @CAPABILITIES if @_ == 0;
	    my $self = shift;
	    my @cap;
	    for my $cap (@CAPABILITIES) {
		push @cap, $cap if _TestCapability($self, $CAPABILITIES{$cap});
	    }
	    return @cap;
	}
        sub TestCapability {
	    my($self, $cap) = @_;
	    return _TestCapability($self, $CAPABILITIES{$cap});
	}
	*Create = *CreateDataSource;
	*Copy = *CopyDataSource;
	*OpenDataSource = *Open;
	*Delete = *DeleteDataSource;

	package Geo::OGR::DataSource;
	use Carp;
	use strict;
	use vars qw /@CAPABILITIES %CAPABILITIES %LAYERS/;
        @CAPABILITIES = qw/CreateLayer DeleteLayer/;
	for my $s (@CAPABILITIES) {
	    my $cap = eval "\$Geo::OGR::ODsC$s";
	    $CAPABILITIES{$s} = $cap;
	}
	sub Capabilities {
	    return @CAPABILITIES if @_ == 0;
	    my $self = shift;
	    my @cap;
	    for my $cap (@CAPABILITIES) {
		push @cap, $cap if _TestCapability($self, $CAPABILITIES{$cap});
	    }
	    return @cap;
	}
	sub TestCapability {
	    my($self, $cap) = @_;
	    return _TestCapability($self, $CAPABILITIES{$cap});
	}
	*GetDriver = *_GetDriver;
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
	sub Layer {
	    my($self, $name) = @_;
	    my $layer;
	    if (defined $name) {
		$layer = _GetLayerByName($self, "$name");
	    } else {
		$name = 0;
	    }
	    $layer = _GetLayerByIndex($self, $name) if !$layer and $name =~ /^\d+$/;
	    return unless $layer;
	    $LAYERS{tied(%$layer)} = $self;
	    return $layer;
	}
	sub Layers {
	    my $self = shift;
	    my @names;
	    for my $i (0..$self->GetLayerCount-1) {
		my $layer = _GetLayerByIndex($self, $i);
		push @names, $layer->GetName;
	    }
	    return @names;
	}
	sub GetLayerByIndex {
	    my($self, $index) = @_;
	    $index = 0 unless defined $index;
	    my $layer = _GetLayerByIndex($self, $index+0);
	    return unless $layer;
	    $LAYERS{tied(%$layer)} = $self;
	    return $layer;
	}
	sub GetLayerByName {
	    my($self, $name) = @_;
	    my $layer = _GetLayerByName($self, $name);
	    return unless $layer;
	    $LAYERS{tied(%$layer)} = $self;
	    return $layer;
	}
	sub CreateLayer {
	    my $self = shift;
	    my %defaults = (Name => 'unnamed',
			    SRS => undef, 
			    GeometryType => 'Unknown', 
			    Options => [], 
			    Schema => undef);
	    my %params;
	    if (ref($_[0]) eq 'HASH') {
		%params = %{$_[0]};
	    } else {
		($params{Name}, $params{SRS}, $params{GeometryType}, $params{Options}, $params{Schema}) = @_;
	    }
	    for (keys %params) {
		croak "unknown parameter: $_" unless exists $defaults{$_};
	    }
	    for (keys %defaults) {
		$params{$_} = $defaults{$_} unless defined $params{$_};
	    }
	    $params{GeometryType} = $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}} if 
		exists $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}};
	    my $layer = _CreateLayer($self, $params{Name}, $params{SRS}, $params{GeometryType}, $params{Options});
	    $LAYERS{tied(%$layer)} = $self;
	    $layer->Schema(%{$params{Schema}}) if $params{Schema};
	    return $layer;
	}
	sub DeleteLayer {
	    my $self = shift;
	    my $name;
	    if (@_ == 2) {
		my %param = @_;
		_DeleteLayer($self, $param{index}), return if exists $param{index};
		$name = $param{name};
	    } else {
		$name = shift;
	    }
	    my $index;
	    for my $i (0..$self->GetLayerCount-1) {
		my $layer = _GetLayerByIndex($self, $i);
		$index = $i, last if $layer->GetName eq $name;
	    }
	    $index = $name unless defined $index;
	    _DeleteLayer($self, $index) if defined $index;
	}

	package Geo::OGR::Layer;
	use strict;
	use vars qw /@CAPABILITIES %CAPABILITIES/;
        @CAPABILITIES = qw/RandomRead SequentialWrite RandomWrite 
		   FastSpatialFilter FastFeatureCount FastGetExtent 
		   CreateField Transactions DeleteFeature FastSetNextByIndex/;
	for my $s (@CAPABILITIES) {
	    my $cap = eval "\$Geo::OGR::OLC$s";
	    $CAPABILITIES{$s} = $cap;
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
		push @cap, $cap if _TestCapability($self, $CAPABILITIES{$cap});
	    }
	    return @cap;
	}
	sub TestCapability {
	    my($self, $cap) = @_;
	    return _TestCapability($self, $CAPABILITIES{$cap});
	}
	sub Schema {
	    my $self = shift;
	    if (@_) {
		my %schema = @_;
		# the Name and GeometryType cannot be set
		for my $fd (@{$schema{Fields}}) {
		    if (ref($fd) eq 'HASH') {
			$fd = Geo::OGR::FieldDefn->create(%$fd);
		    }
		    $schema{ApproxOK} = 1 unless defined $schema{ApproxOK};
		    CreateField($self, $fd, $schema{ApproxOK});
		}
	    }
	    return unless defined wantarray;
	    return $self->GetLayerDefn->Schema;
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
		my $n = $field->{Name};
		if ($f->FieldIsList($n)) {
		    $row{$n} = [$f->GetField($n)];
		} else {
		    $row{$n} = $f->GetField($n);
		}
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
		    my $n = $field->{Name};
		    defined $v ? $f->SetField($n, $v) : $f->UnsetField($n);
		}
		$changed = 1;
	    }
	    $self->SetFeature($f) if $changed;
	    return unless defined wantarray;
	    my @ret = ($f->GetFID, $f->GetGeometry);
	    my $i = 0;
	    for my $field (@{$s->{Fields}}) {
		if ($f->FieldIsList($i)) {
		    push @ret, [$f->GetField($i++)];
		} else {
		    push @ret, $f->GetField($i++);
		}
	    }
	    return @ret;
	}
	sub SpatialFilter {
	    my $self = shift;
	    $self->SetSpatialFilter($_[0]) if @_ == 1;
	    $self->SetSpatialFilterRect(@_) if @_ == 4;
	    return unless defined wantarray;
	    $self->GetSpatialFilter;
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
	use strict;
	sub create {
	    my $pkg = shift;
	    my %schema;
	    if (@_ == 1) {
	        %schema = %{$_[0]};
	    } else {
	        %schema = @_;
	    }
	    my $self = Geo::OGRc::new_FeatureDefn($schema{Name});
	    bless $self, $pkg;
	    $self->GeometryType($schema{GeometryType});
	    for my $fd (@{$schema{Fields}}) {
		if (ref($fd) eq 'HASH') {
		    $fd = Geo::OGR::FieldDefn->create(%$fd);
		}
		AddFieldDefn($self, $fd);
	    }
	    return $self;
	}
	sub Schema {
	    my $self = shift;
	    my %schema;
	    if (@_) {
		%schema = @_;
		# the Name cannot be set
		$self->GeomType($schema{GeometryType}) if exists $schema{GeometryType};
		for my $fd (@{$schema{Fields}}) {
		    if (ref($fd) eq 'HASH') {
			$fd = Geo::OGR::FieldDefn->create(%$fd);
		    }
		    AddFieldDefn($self, $fd);
		}
	    }
	    return unless defined wantarray;
	    $schema{Name} = $self->GetName();
	    $schema{GeometryType} = $self->GeomType();
	    $schema{Fields} = [];
	    for my $i (0..$self->GetFieldCount-1) {
		push @{$schema{Fields}}, $self->GetFieldDefn($i)->Schema;
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
	use Carp;
	sub FID {
	    my $self = shift;
	    $self->SetFID($_[0]) if @_;
	    return unless defined wantarray;
	    $self->GetFID;
	}
	sub StyleString {
	    my $self = shift;
	    $self->SetStyleString($_[0]) if @_;
	    return unless defined wantarray;
	    $self->GetStyleString;
	}
	sub Schema {
	    my $self = shift;
	    if (@_) {
		my %schema = @_;
		# the Name and GeometryType cannot be set
		for my $fd (@{$schema{Fields}}) {
		    if (ref($fd) eq 'HASH') {
			$fd = Geo::OGR::FieldDefn->create(%$fd);
		    }
		    $schema{ApproxOK} = 1 unless defined $schema{ApproxOK};
		    CreateField($self, $fd, $schema{ApproxOK});
		}
	    }
	    return unless defined wantarray;
	    return $self->GetDefnRef->Schema;
	}
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
		my $n = $field->{Name};
		if (FieldIsList($self, $n)) {
		    $row{$n} = [$self->GetField($n)];
		} else {
		    $row{$n} = $self->GetField($n);
		}
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
		    my $n = $field->{Name};
		    defined $v ? $self->SetField($n, $v) : $self->UnsetField($n);
		}
	    }
	    return unless defined wantarray;
	    my @ret = ($self->GetFID, $self->GetGeometry);
	    my $i = 0;
	    for my $field (@{$s->{Fields}}) {
		if (FieldIsList($self, $i)) {
		    push @ret, [$self->GetField($i++)];
		} else {
		    push @ret, $self->GetField($i++);
		}
	    }
	    return @ret;
	}
	sub GetFieldType {
	    my($self, $field) = @_;
	    return $Geo::OGR::FieldDefn::TYPE_INT2STRING{_GetFieldType($self, $field)};
	}
	sub FieldIsList {
	    my($self, $field) = @_;
	    my $count = GetFieldCount($self);
	    $field = GetFieldIndex($self, $field) unless $field =~ /^\d+$/;
	    croak("no such field: $_[1]") if $field < 0 or $field >= $count;
	    my $type = _GetFieldType($self, $field);
	    return 1 if ($type == $Geo::OGR::OFTIntegerList or
			 $type == $Geo::OGR::OFTRealList or
			 $type == $Geo::OGR::OFTStringList or
			 $type == $Geo::OGR::OFTDate or
			 $type == $Geo::OGR::OFTTime or
			 $type == $Geo::OGR::OFTDateTime);
	    return 0;
	}
	sub GetField {
	    my($self, $field) = @_;
	    my $count = GetFieldCount($self);
	    $field = GetFieldIndex($self, $field) unless $field =~ /^\d+$/;
	    croak("no such field: $_[1]") if $field < 0 or $field >= $count;
	    return undef unless IsFieldSet($self, $field);
	    my $type = _GetFieldType($self, $field);
	    if ($type == $Geo::OGR::OFTInteger) {
		return GetFieldAsInteger($self, $field);
	    }
	    if ($type == $Geo::OGR::OFTReal) {
		return GetFieldAsDouble($self, $field);
	    }
	    if ($type == $Geo::OGR::OFTString) {
		return GetFieldAsString($self, $field);
	    }
	    if ($type == $Geo::OGR::OFTIntegerList) {
		my $ret = GetFieldAsIntegerList($self, $field);
		return @$ret;
	    } 
	    if ($type == $Geo::OGR::OFTRealList) {
		my $ret = GetFieldAsDoubleList($self, $field);
		return @$ret;
	    }
	    if ($type == $Geo::OGR::OFTStringList) {
		my $ret = GetFieldAsStringList($self, $field);
		return @$ret;
	    }
	    if ($type == $Geo::OGR::OFTBinary) {
		return GetFieldAsString($self, $field);
	    }
	    if ($type == $Geo::OGR::OFTDate) {
		my @ret = GetFieldAsDateTime($self, $field);
		# year, month, day, hour, minute, second, timezone
		return @ret[0..2];
	    }
	    if ($type == $Geo::OGR::OFTTime) {
		my @ret = GetFieldAsDateTime($self, $field);
		return @ret[3..6];
	    }
	    if ($type == $Geo::OGR::OFTDateTime) {
		return GetFieldAsDateTime($self, $field);
	    }
	    carp "unknown/unsupported field type: $type";
	}
	sub UnsetField {
	    my($self, $field) = @_;
	    my $type = _GetFieldType($self, $field);
	    my $count = GetFieldCount($self);
	    $field = GetFieldIndex($self, $field) unless $field =~ /^\d+$/;
	    croak("no such field: $_[1]") if $field < 0 or $field >= $count;
	    _UnsetField($self, $field);
	}
	sub SetField {
	    my $self = shift;
	    my $field = $_[0];
	    my $count = GetFieldCount($self);
	    $field = GetFieldIndex($self, $field) unless $field =~ /^\d+$/;
	    croak("no such field: $_[0]") if $field < 0 or $field >= $count;
	    shift;
	    if (@_ == 0 or !defined($_[0])) {
		_UnsetField($self, $field);
		return;
	    }
	    my $list = ref($_[0]) ? $_[0] : [@_];
	    my $type = _GetFieldType($self, $field);
	    if ($type == $Geo::OGR::OFTInteger or 
		$type == $Geo::OGR::OFTReal or 
		$type == $Geo::OGR::OFTString or
		$type == $Geo::OGR::OFTBinary) 
	    {
		_SetField($self, $field, $_[0]);
	    } 
	    elsif ($type == $Geo::OGR::OFTIntegerList) {
		SetFieldIntegerList($self, $field, $list);
	    } 
	    elsif ($type == $Geo::OGR::OFTRealList) {
		SetFieldDoubleList($self, $field, $list);
	    } 
	    elsif ($type == $Geo::OGR::OFTStringList) {
		SetFieldStringList($self, $field, $list);
	    } 
	    elsif ($type == $Geo::OGR::OFTDate) {
		# year, month, day, hour, minute, second, timezone
		for my $i (0..6) {
		    $list->[$i] = 0 unless defined $list->[$i];
		}
		_SetField($self, $field, @$list[0..6]);
	    } 
	    elsif ($type == $Geo::OGR::OFTTime) {
		$list->[3] = 0 unless defined $list->[3];
		_SetField($self, $field, 0, 0, 0, @$list[0..3]);
	    } 
	    elsif ($type == $Geo::OGR::OFTDateTime) {
		$list->[6] = 0 unless defined $list->[6];
		_SetField($self, $field, @$list[0..6]);
	    } 
	    else {
		carp "unknown/unsupported field type: $type";
	    }
	}
	sub Field {
	    my $self = shift;
	    my $field = shift;
	    $self->SetField($field, @_) if @_;
	    $self->GetField($field);
	}
	sub Geometry {
	    my $self = shift;
	    SetGeometry($self, $_[0]) if @_;
            my $geometry = GetGeometryRef($self);
	    $geometry->Clone() if $geometry and defined wantarray;
	}
	sub SetGeometryDirectly {
	    _SetGeometryDirectly(@_);
	    $GEOMETRIES{tied(%{$_[1]})} = $_[0];
	}
	sub GetGeometry {
	    my $self = shift;
	    my $geom = GetGeometryRef($self);
	    $GEOMETRIES{tied(%$geom)} = $self if $geom;
	    return $geom;
	}

	package Geo::OGR::FieldDefn;
	use strict;
	use vars qw /
	    @FIELD_TYPES @JUSTIFY_TYPES
	    %TYPE_STRING2INT %TYPE_INT2STRING
	    %JUSTIFY_STRING2INT %JUSTIFY_INT2STRING
	    /;
	@FIELD_TYPES = qw/Integer IntegerList Real RealList String StringList 
			WideString WideStringList Binary Date Time DateTime/;
	@JUSTIFY_TYPES = qw/Undefined Left Right/;
	for my $string (@FIELD_TYPES) {
	    my $int = eval "\$Geo::OGR::OFT$string";
	    $TYPE_STRING2INT{$string} = $int;
	    $TYPE_INT2STRING{$int} = $string;
	}
	for my $string (@JUSTIFY_TYPES) {
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
	    $param{Type} = $TYPE_STRING2INT{$param{Type}} 
	    if defined $param{Type} and exists $TYPE_STRING2INT{$param{Type}};
	    $param{Justify} = $JUSTIFY_STRING2INT{$param{Justify}} 
	    if defined $param{Justify} and exists $JUSTIFY_STRING2INT{$param{Justify}};
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
	use strict;
	use Carp;
	use vars qw /
            @GEOMETRY_TYPES @BYTE_ORDER_TYPES
	    %TYPE_STRING2INT %TYPE_INT2STRING
	    %BYTE_ORDER_STRING2INT %BYTE_ORDER_INT2STRING
	    /;
        @GEOMETRY_TYPES = qw/Unknown 
			Point LineString Polygon 
			MultiPoint MultiLineString MultiPolygon GeometryCollection 
			None LinearRing
			Point25D LineString25D Polygon25D 
			MultiPoint25D MultiLineString25D MultiPolygon25D GeometryCollection25D/;
	for my $string (@GEOMETRY_TYPES) {
	    my $int = eval "\$Geo::OGR::wkb$string";
	    $TYPE_STRING2INT{$string} = $int;
	    $TYPE_INT2STRING{$int} = $string;
	}
	@BYTE_ORDER_TYPES = qw/XDR NDR/;
	for my $string (@BYTE_ORDER_TYPES) {
	    my $int = eval "\$Geo::OGR::wkb$string";
	    $BYTE_ORDER_STRING2INT{$string} = $int;
	    $BYTE_ORDER_INT2STRING{$int} = $string;
	}
	sub RELEASE_PARENTS {
	    my $self = shift;
	    delete $Geo::OGR::Feature::GEOMETRIES{$self};
	}
	sub create { # alternative constructor since swig created new cannot be overridden(?)
	    my $pkg = shift;
	    my($type, $wkt, $wkb, $gml, $json, $srs, $points);
	    if (@_ == 1) {
		$type = shift;
	    } else {
		my %param = @_;
		$type = ($param{type} or $param{Type} or $param{GeometryType});
		$srs = ($param{srs} or $param{SRS});
		$wkt = ($param{wkt} or $param{WKT});
		$wkb = ($param{wkb} or $param{WKB});
		my $hex = ($param{hexwkb} or $param{HEXWKB});
		if ($hex) {
		    $wkb = '';
		    for (my $i = 0; $i < length($hex); $i+=2) {
			$wkb .= chr(hex(substr($hex,$i,2)));
		    }
		}
		$gml = ($param{gml} or $param{GML});
		$json = ($param{geojson} or $param{GeoJSON});
		$points = $param{Points};
	    }
	    $type = $TYPE_STRING2INT{$type} if defined $type and exists $TYPE_STRING2INT{$type};
	    my $self;
	    if (defined $type) {
		croak "unknown GeometryType: $type" unless 
		    exists($TYPE_STRING2INT{$type}) or exists($TYPE_INT2STRING{$type});
		$self = Geo::OGRc::new_Geometry($type);
	    } elsif (defined $wkt) {
		$self = Geo::OGRc::CreateGeometryFromWkt($wkt, $srs);
	    } elsif (defined $wkb) {
		$self = Geo::OGRc::CreateGeometryFromWkb($wkb, $srs);
	    } elsif (defined $gml) {
		$self = Geo::OGRc::CreateGeometryFromGML($gml);
	    } elsif (defined $json) {
		$self = Geo::OGRc::CreateGeometryFromJson($json);
	    } else {
		croak "missing GeometryType, WKT, WKB, GML, or GeoJSON parameter in Geo::OGR::Geometry::create";
	    }
	    bless $self, $pkg if defined $self;
	    $self->Points($points) if $points;
	    return $self;
	}
	sub GeometryType {
	    my $self = shift;
	    return $TYPE_INT2STRING{$self->GetGeometryType};
	}
	sub CoordinateDimension {
	    my $self = shift;
	    SetCoordinateDimension($self, $_[0]) if @_;
	    GetCoordinateDimension($self) if defined wantarray;
	}
	sub AddPoint {
	    @_ == 4 ? AddPoint_3D(@_) : AddPoint_2D(@_);
	}
	sub SetPoint {
	    @_ == 5 ? SetPoint_3D(@_) : SetPoint_2D(@_);
	}
	sub GetPoint {
	    my($self, $i) = @_;
	    $i = 0 unless defined $i;
	    my $point = ($self->GetGeometryType & 0x80000000) == 0 ? GetPoint_2D($self, $i) : GetPoint_3D($self, $i);
	    return @$point;
	}
	sub Point {
	    my $self = shift;
	    my $i;
	    if (@_) {
		my $t = $self->GetGeometryType;
		if ($t == $Geo::OGR::wkbPoint) {
		    shift if @_ > 2;
		    $i = 0;
		} elsif ($t == $Geo::OGR::wkbPoint25D) {
		    shift if @_ > 3;
		    $i = 0;
		} else {
		    my $i = shift;
		}
		SetPoint($self, $i, @_);
	    }
	    return GetPoint($self, $i) if defined wantarray;
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
		    push @points, $flat ? GetPoint_2D($self) : GetPoint_3D($self);
		} else {
		    my $i;
		    if ($flat) {
			for my $i (0..$n-1) {
			    push @points, GetPoint_2D($self, $i);
			}
		    } else {
			for my $i (0..$n-1) {
			    push @points, GetPoint_3D($self, $i);
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
	*AsText = *ExportToWkt;
	*AsBinary = *ExportToWkb;
	*AsGML = *ExportToGML;
	*AsKML = *ExportToKML;
	*Equals = *Equal;
	*Intersects = *Intersect;
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
    sub Drivers {
	my @drivers;
	for my $i (0..GetDriverCount()-1) {
	    push @drivers, _GetDriver($i);
	}
	return @drivers;
    }
    sub GetDriver {
	my($name_or_number) = @_;
	return _GetDriver($name_or_number) if $name_or_number =~ /^\d/;
	return GetDriverByName("$name_or_number");
    }
    *Driver = *GetDriver;
%}
