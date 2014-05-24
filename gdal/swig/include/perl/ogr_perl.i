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

%include callback.i

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

#ifndef FROM_GDAL_I
%extend OGRDataSourceShadow {

  %rename (_ExecuteSQL) ExecuteSQL;

 }
#endif

%extend OGRGeometryShadow {

    %rename (AddPoint_3D) AddPoint;
    %rename (SetPoint_3D) SetPoint;
    %rename (GetPoint_3D) GetPoint;

}

%extend OGRFeatureShadow {

  %rename (_UnsetField) UnsetField;
  %rename (_SetField) SetField;
  %rename (_SetFrom) SetFrom;

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
            int d = OGR_G_GetCoordinateDimension(self);
            for (i = 0; i < OGR_G_GetPointCount(self); i++) {
                if (d == 0) {
                } else {
                    double x = OGR_G_GetX(self, i);
                    double y = OGR_G_GetY(self, i);
                    if (d == 2) {
                        OGR_G_SetPoint_2D(self, i, x+dx, y+dy);
                    } else {
                        double z = OGR_G_GetZ(self, i);
                        OGR_G_SetPoint(self, i, x+dx, y+dy, z+dz);
                    }
                }
            }
        }
    }
    
}

%rename (_ReleaseResultSet) ReleaseResultSet;
%rename (_GetLayerByIndex) GetLayerByIndex;
%rename (_GetLayerByName) GetLayerByName;
%rename (_CreateLayer) CreateLayer;
%rename (_DeleteLayer) DeleteLayer;
%rename (_CreateField) CreateField;
%rename (_DeleteField) DeleteField;
%rename (_GetFieldType) GetFieldType;
%rename (_SetGeometryDirectly) SetGeometryDirectly;
%rename (_ExportToWkb) ExportToWkb;
%rename (_GetDriver) GetDriver;
%rename (_TestCapability) TestCapability;

%perlcode %{
    use strict;
    use Carp;
    {
        package Geo::OGR;
    }
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
        *Name = *GetName;

        package Geo::OGR::DataSource;
        use Carp;
        use strict;
        use vars qw /@CAPABILITIES %CAPABILITIES %LAYERS %RESULT_SET/;
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
        sub Layer {
            my($self, $name) = @_;
            my $layer;
            if (defined $name) {
                $layer = _GetLayerByName($self, "$name");
                croak "$name is not a layer in this datasource" if (not $layer and not $name =~ /^\d+$/);
                $layer = _GetLayerByIndex($self, $name+0) unless $layer;
            } else {
                $layer = _GetLayerByIndex($self, 0);
            }
            croak "the data source does not appear to have a layer with name '$name'" unless $layer;
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
            croak "the data source does not appear to have a layer with index '$index'" unless $layer;
            $LAYERS{tied(%$layer)} = $self;
            return $layer;
        }
        sub GetLayerByName {
            my($self, $name) = @_;
            my $layer = _GetLayerByName($self, "$name");
            croak "the data source does not appear to have a layer with name $name" unless $layer;
            $LAYERS{tied(%$layer)} = $self;
            return $layer;
        }
        sub CreateLayer {
            my $self = shift;
            my %defaults = (Name => 'unnamed',
                            SRS => undef, 
                            GeometryType => 'Unknown', 
                            Options => [], 
                            Schema => undef,
                            Fields => undef);
            my %params;
            if (ref($_[0]) eq 'HASH') {
                %params = %{$_[0]};
            } else {
                ($params{Name}, $params{SRS}, $params{GeometryType}, $params{Options}, $params{Schema}) = @_;
            }
            for (keys %params) {
                carp "unknown parameter $_ in Geo::OGR::DataSource->CreateLayer" unless exists $defaults{$_};
            }
            for (keys %defaults) {
                $params{$_} = $defaults{$_} unless defined $params{$_};
            }
            $params{GeometryType} = $params{Schema}->{GeometryType} if 
                ($params{Schema} and exists $params{Schema}->{GeometryType});
            $params{GeometryType} = $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}} if 
                exists $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}};
            my $layer = _CreateLayer($self, $params{Name}, $params{SRS}, $params{GeometryType}, $params{Options});
            $LAYERS{tied(%$layer)} = $self;
            if ($params{Fields}) {
                $params{Schema} = {} unless $params{Schema};
                $params{Schema}{Fields} = $params{Fields};
            }
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
        use Carp;
        use Scalar::Util 'blessed';
        use vars qw /@CAPABILITIES %CAPABILITIES/;
        @CAPABILITIES = qw/RandomRead SequentialWrite RandomWrite 
                   FastSpatialFilter FastFeatureCount FastGetExtent 
                   CreateField DeleteField ReorderFields AlterFieldDefn
                   Transactions DeleteFeature FastSetNextByIndex
                   StringsAsUTF8 IgnoreFields/;
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
            if ($Geo::OGR::DataSource::RESULT_SET{$self}) {
                $Geo::OGR::DataSource::LAYERS{$self}->_ReleaseResultSet($self);
                delete $Geo::OGR::DataSource::RESULT_SET{$self}
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
        sub DataSource {
            my $self = shift;
            return $Geo::OGR::DataSource::LAYERS{$self};
        }
        sub HasField {
            my($self, $fn) = @_;
            eval {
                $fn = $self->GetLayerDefn->GetFieldIndex($fn) unless $fn =~ /^\d+$/;
                $self->GetLayerDefn->GetFieldDefn($fn);
            };
            return $@ eq '';
        }
        sub GetField {
            my($self, $fn) = @_;
            $fn = $self->GetLayerDefn->GetFieldIndex($fn) unless $fn =~ /^\d+$/;
            return $self->GetLayerDefn->GetFieldDefn($fn)->Schema;
        }
        sub CreateField {
            my $self = shift;
            my $fd = shift;
            if (blessed($fd) and $fd->isa('Geo::OGR::FieldDefn')) {
                my $n = $fd->Schema->{Name};
                croak "the layer already has a field with name '$n'" if $self->HasField($n);
                my $a = shift || 1;
                _CreateField($self, $fd, $a);
            } else {
                $fd = Geo::OGR::FieldDefn->create($fd, @_);
                my $n = $fd->Schema->{Name};
                croak "the layer already has a field with name '$n'" if $self->HasField($n);
                _CreateField($self, $fd); # approximation flag cannot be set using this method
            }
        }
        sub AlterField {
            my $self = shift;
            my $fn = shift;
            my $index = $fn;            
            $index = $self->GetLayerDefn->GetFieldIndex($fn) unless $fn =~ /^\d+$/;
            my $field = $self->GetLayerDefn->GetFieldDefn($index);
            my $definition = Geo::OGR::FieldDefn->create(@_);
            my $flags = 0;
            my %params = @_;
            $flags |= 1 if $params{Name};
            $flags |= 2 if $params{Type};
            $flags |= 4 if $params{Width};
            AlterFieldDefn($self, $index, $definition, $flags);
        }
        sub DeleteField {
            my($self, $fn) = @_;
            $fn = $self->GetLayerDefn->GetFieldIndex($fn) unless $fn =~ /\d+/;
            _DeleteField($self, $fn);
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
                    _CreateField($self, $fd, $schema{ApproxOK});
                }
            }
            return unless defined wantarray;
            return $self->GetLayerDefn->Schema;
        }
        sub Row {
            my $self = shift;
            my %row;
            my $update;
            if (@_ > 0 and ref($_[0])) { # undocumented hack: the first argument may be the schema
                $update = @_ > 1;
                %row = @_[1..$#$_];
            } else {
                $update = @_ > 0;
                %row = @_;
            }
            my $feature = defined $row{FID} ? $self->GetFeature($row{FID}) : $self->GetNextFeature;
            return unless $feature;
            my $ret;
            if (defined wantarray) {
                $ret = $feature->Row(@_);
            } else {
                $feature->Row(@_);
            }
            $self->SetFeature($feature) if $update;
            return unless defined wantarray;
            return $ret;
        }
        sub Tuple {
            my $self = shift;
            # undocumented hack: the first argument may be the schema
            my $schema = ref($_[0]) ? shift : $self->Schema;
            my $FID = shift;
            my $feature = defined $FID ? $self->GetFeature($FID) : $self->GetNextFeature;
            return unless $feature;
            my $set = @_ > 0;
            unshift @_, $feature->GetFID if $set;
            my @ret;
            if (defined wantarray) {
                @ret = $feature->Tuple($schema, @_);
            } else {
                $feature->Tuple($schema, @_);
            }
            $self->SetFeature($feature) if $set;
            return unless defined wantarray;
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
            my $feature = shift;
            croak "InsertFeature requires the feature data in an object or in a referenced hash or array" unless ref($feature);
            my $schema = shift;
            $schema = $self->Schema unless $schema;
            my $new = Geo::OGR::Feature->create($schema);
            if (ref($feature) eq 'HASH') {
                $new->Row($schema, %$feature);
            } elsif (ref($feature) eq 'ARRAY') {
                $new->Tuple($schema, @$feature);
            } elsif (blessed($feature) and $feature->isa('Geo::OGR::Feature')) {
                $new->Row($schema, $feature->Row);
            }
            $self->CreateFeature($new);
        }
        sub ForFeatures {
            my $self = shift;
            my $code = shift;
            my $in_place = shift;
            $self->ResetReading;
            while (my $f = $self->GetNextFeature) {
                $code->($f);
                $self->SetFeature($f) if $in_place;
            };
        }
        sub ForGeometries {
            my $self = shift;
            my $code = shift;
            my $in_place = shift;
            $self->ResetReading;
            while (my $f = $self->GetNextFeature) {
                my $g = $f->Geometry();
                $code->($g);
                if ($in_place) {
                    $f->Geometry($g);
                    $self->SetFeature($f);
                }
            }
        }
        sub GeometryType {
            my $self = shift;
            return $Geo::OGR::Geometry::TYPE_INT2STRING{GetGeomType($self)};
        }

        package Geo::OGR::FeatureDefn;
        use strict;
        use Encode;
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
                my $d;
                if (ref($fd) eq 'HASH') {
                    $d = Geo::OGR::FieldDefn->create(%$fd);
                } else {
                    $d = Geo::OGR::FieldDefn->create($fd->Schema);
                }
                AddFieldDefn($self, $d);
            }
            return $self;
        }
        *Name = *GetName;
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
                my $s = $self->GetFieldDefn($i)->Schema;
                $s->{Index} = $i;
                push @{$schema{Fields}}, $s;
            }
            return wantarray ? %schema : \%schema;
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
        sub GeometryIgnored {
            my $self = shift;
            SetGeometryIgnored($self, $_[0]) if @_;
            IsGeometryIgnored($self) if defined wantarray;
        }
        sub StyleIgnored {
            my $self = shift;
            SetStyleIgnored($self, $_[0]) if @_;
            IsStyleIgnored($self) if defined wantarray;
        }

        package Geo::OGR::Feature;
        use strict;
        use vars qw /%GEOMETRIES/;
        use Carp;
        use Encode;
        sub create {
            my $pkg = shift;
            $pkg->new(Geo::OGR::FeatureDefn->create(@_));
        }
        sub FETCH {
            my($self, $index) = @_;
            $self->GetField($index);
        }
        sub STORE {
            my $self = shift;
            $self->SetField(@_);
        }
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
            # undocumented hack: the first argument may be the schema
            my $schema = ref($_[0]) ? shift : $self->Schema;
            if (@_) { # update
                my %row = ref($_[0]) ? %{$_[0]} : @_;
                $self->SetFID($row{FID}) if defined $row{FID};
                $self->Geometry($schema, $row{Geometry}) if $row{Geometry};
                for my $fn (keys %row) {
                    next if $fn eq 'FID';
                    next if $fn eq 'Geometry';
                    my $index = GetFieldIndex($self, $fn);
                    next if $index < 0;
                    $self->SetField($index, $row{$fn});
                }
            }
            return unless defined wantarray;
            my %row = ();
            for my $field (@{$schema->{Fields}}) {
                my $n = $field->{Name};
                if (FieldIsList($self, $n)) {
                    $row{$n} = [$self->GetField($n)];
                } else {
                    $row{$n} = $self->GetField($n);
                }
            }
            $row{FID} = $self->GetFID;
            $row{Geometry} = $self->Geometry;
            return \%row;
        }
        sub Tuple {
            my $self = shift;
            # undocumented hack: the first argument may be the schema
            my $schema = ref($_[0]) ? shift : $self->Schema;
            my $FID = shift;
            if (defined $FID) {
                $self->SetFID($FID);
                my $geometry = shift;
                $self->Geometry($schema, $geometry) if $geometry;
                if (@_) {
                    for my $field (@{$schema->{Fields}}) {
                        my $v = shift;
                        my $n = $field->{Name};
                        $self->SetField($n, $v);
                    }
                }
            }
            return unless defined wantarray;
            my @ret = ($self->GetFID, $self->Geometry);
            my $i = 0;
            for my $field (@{$schema->{Fields}}) {
                if (FieldIsList($self, $i)) {
                    push @ret, [$self->GetField($i++)];
                } else {
                    push @ret, $self->GetField($i++);
                }
            }
            return @ret;
        }
        sub Index {
            my($self, $field) = @_;
            my $index;
            if ($field =~ /^\d+$/) {
                $index = $field;
            } else {
                $index = GetFieldIndex($self, "$field");
            }
            croak "the feature does not have a field with name '$field'" if $index < 0 or $index >= GetFieldCount($self);
            return $index;
        }
        sub GetFieldType {
            my($self, $field) = @_;
            $field = Index($self, $field);
            return $Geo::OGR::FieldDefn::TYPE_INT2STRING{_GetFieldType($self, $field)};
        }
        sub FieldIsList {
            my($self, $field) = @_;
            $field = Index($self, $field);
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
            $field = Index($self, $field);
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
                return wantarray ? @$ret : $ret;
            } 
            if ($type == $Geo::OGR::OFTRealList) {
                my $ret = GetFieldAsDoubleList($self, $field);
                return wantarray ? @$ret : $ret;
            }
            if ($type == $Geo::OGR::OFTStringList) {
                my $ret = GetFieldAsStringList($self, $field);
                return wantarray ? @$ret : $ret;
            }
            if ($type == $Geo::OGR::OFTBinary) {
                return GetFieldAsString($self, $field);
            }
            if ($type == $Geo::OGR::OFTDate) {
                my @ret = GetFieldAsDateTime($self, $field);
                # year, month, day, hour, minute, second, timezone
                return wantarray ? @ret[0..2] : [@ret[0..2]];
            }
            if ($type == $Geo::OGR::OFTTime) {
                my @ret = GetFieldAsDateTime($self, $field);
                return wantarray ? @ret[3..6] : [@ret[3..6]];
            }
            if ($type == $Geo::OGR::OFTDateTime) {
                return GetFieldAsDateTime($self, $field);
            }
            croak "GDAL does not have a field type whose constant is '$type'";
        }
        sub UnsetField {
            my($self, $field) = @_;
            $field = Index($self, $field);
            _UnsetField($self, $field);
        }
        sub SetField {
            my $self = shift;
            my $field = $_[0];
            $field = Index($self, $field);
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
                croak "GDAL does not have a field type of number '$type'";
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
            if (@_) {
                # undocumented hack: the first argument may be the schema
                my $schema = @_ == 2 ? shift : $self->Schema;
                my $geometry = shift;
                my $type = $schema->{GeometryType};
                if (ref($geometry) eq 'HASH') {
                    my $geom;
                    eval {
                        $geom = Geo::OGR::Geometry->create(%$geometry);
                    };
                    if ($@) {
                        $geometry->{GeometryType} = $type;
                        $geom = Geo::OGR::Geometry->create(%$geometry);
                    }
                    unless ($type eq 'Unknown' or !$geom->GeometryType) {
                        croak "an attempt to insert a geometry with type '",$geom->GeometryType,"' into a feature with geometry type '$type'" unless $type eq $geom->GeometryType;
                    }
                    $self->SetGeometryDirectly($geom);
                } else {
                    unless ($type eq 'Unknown') {
                        croak "an attempt to insert a geometry with type '",$geometry->GeometryType,"' into a feature with geometry type '$type'" unless $type eq $geometry->GeometryType;
                    }
                    $self->SetGeometry($geometry);
                }
            }
            return unless defined wantarray;
            my $geometry = $self->GetGeometryRef();
            $geometry->Clone() if $geometry;
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
        sub ReferenceGeometry {
            my $self = shift;
            SetGeometryDirectly($self, $_[0]) if @_;
            if (defined wantarray) {
                my $geometry = GetGeometry($self);
                return $geometry->Clone() if $geometry;
            }
        }
        sub SetFrom {
            my($self, $other) = @_;
            _SetFrom($self, $other), return if @_ <= 2;
            my $forgiving = $_[2];
            _SetFrom($self, $other, $forgiving), return if @_ <= 3;            
            my $map = $_[3];
            my @list;
            for my $i (1..GetFieldCount($self)) {
                push @list, ($map->{$i} || -1);
            }
            SetFromWithMap($self, $other, 1, \@list);
        }

        package Geo::OGR::FieldDefn;
        use strict;
        use vars qw /
            @FIELD_TYPES @JUSTIFY_TYPES
            %TYPE_STRING2INT %TYPE_INT2STRING
            %JUSTIFY_STRING2INT %JUSTIFY_INT2STRING
            /;
        use Carp;
        use Encode;
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
            croak "usage: Geo::OGR::FieldDefn->create(%params)" if ref($param{Name});
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
        sub Ignored {
            my $self = shift;
            SetIgnored($self, $_[0]) if @_;
            IsIgnored($self) if defined wantarray;
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
            my %schema = ( Name => $self->Name, 
                           Type  => $self->Type,
                           Justify  => $self->Justify,
                           Width  => $self->Width,
                           Precision => $self->Precision );
            return wantarray ? %schema : \%schema;
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
            my($type, $wkt, $wkb, $gml, $json, $srs, $points, $arc);
            if (@_ == 1) {
                $type = shift;
            } else {
                my %param = @_;
                $type = ($param{type} or $param{Type} or $param{GeometryType});
                $srs = ($param{srs} or $param{SRS});
                $wkt = ($param{wkt} or $param{WKT});
                $wkb = ($param{wkb} or $param{WKB});
                my $hex = ($param{hexewkb} or $param{HEXEWKB}); # PostGIS HEX EWKB
                substr($hex, 10, 8) = '' if $hex; # remove SRID
                $hex = ($param{hexwkb} or $param{HEXWKB}) unless $hex;
                if ($hex) {
                    $wkb = '';
                    for (my $i = 0; $i < length($hex); $i+=2) {
                        $wkb .= chr(hex(substr($hex,$i,2)));
                    }
                }
                $gml = ($param{gml} or $param{GML});
                $json = ($param{geojson} or $param{GeoJSON});
                $points = $param{Points};
                $arc = ($param{arc} or $param{Arc});
            }
            $type = $TYPE_STRING2INT{$type} if defined $type and exists $TYPE_STRING2INT{$type};
            my $self;
            if (defined $wkt) {
                $self = Geo::OGRc::CreateGeometryFromWkt($wkt, $srs);
            } elsif (defined $wkb) {
                $self = Geo::OGRc::CreateGeometryFromWkb($wkb, $srs);
            } elsif (defined $gml) {
                $self = Geo::OGRc::CreateGeometryFromGML($gml);
            } elsif (defined $json) {
                $self = Geo::OGRc::CreateGeometryFromJson($json);
            } elsif (defined $type) {
                croak "unknown GeometryType '$type' when creating a Geo::OGR::Geometry object" unless 
                    exists($TYPE_STRING2INT{$type}) or exists($TYPE_INT2STRING{$type});
                $self = Geo::OGRc::new_Geometry($type);
            } elsif (defined $arc) {
                $self = Geo::OGRc::ApproximateArcAngles(@$arc);
            } else {
                croak "missing a parameter when creating a Geo::OGR::Geometry object";
            }
            bless $self, $pkg if defined $self;
            $self->Points($points) if $points;
            return $self;
        }
        sub AsHEXWKB {
            my($self) = @_;
            my $wkb = _ExportToWkb($self, 1);
            my $hex = '';
            for (my $i = 0; $i < length($wkb); $i++) {
                my $x = sprintf("%x", ord(substr($wkb,$i,1)));
                $x = '0' . $x if length($x) == 1;
                $hex .= uc($x);
            }
            return $hex;
        }
        sub AsHEXEWKB {
            my($self, $srid) = @_;
            my $hex = AsHEXWKB($self);
            if ($srid) {
                my $s = sprintf("%x", $srid);
                $srid = '';
                do {
                    if (length($s) > 2) {
                        $srid .= substr($s,-2,2);
                        substr($s,-2,2) = '';
                    } elsif (length($s) > 1) {
                        $srid .= $s;
                        $s = '';
                    } else {
                        $srid .= '0'.$s;
                        $s = '';
                    }
                } until $s eq '';
            } else {
                $srid = '00000000';
            }
            while (length($srid) < 8) {
                $srid .= '00';
            }
            substr($hex, 10, 0) = uc($srid);
            return $hex;
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
                    croak("can't set points of a geometry of type '$t'");
                } elsif ($t eq 'Point') {
                    # support both "Point" as a list of one point and one point
                    if (ref($points->[0])) {
                        $flat ? 
                            AddPoint_2D($self, @{$points->[0]}[0..1]) : 
                            AddPoint_3D($self, @{$points->[0]}[0..2]);
                    } else {
                        $flat ? 
                            AddPoint_2D($self, @$points[0..1]) : 
                            AddPoint_3D($self, @$points[0..2]);
                    }
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
                            push @points, scalar GetPoint_2D($self, $i);
                        }
                    } else {
                        for my $i (0..$n-1) {
                            push @points, scalar GetPoint_3D($self, $i);
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
        sub ForceToMultiPoint {
            my $self = shift;
            $self = Geo::OGR::ForceToMultiPoint($self);
            for my $g (@_) {
                $self->AddGeometry($g);
            }
            return $self;
        }
        sub ForceToMultiLineString {
            my $self = shift;
            $self = Geo::OGR::ForceToMultiLineString($self);
            for my $g (@_) {
                $self->AddGeometry($g);
            }
            return $self;
        }
        sub ForceToMultiPolygon {
            my $self = shift;
            $self = Geo::OGR::ForceToMultiPolygon($self);
            for my $g (@_) {
                $self->AddGeometry($g);
            }
            return $self;
        }
        sub ForceToCollection {
            my $self = Geo::OGR::Geometry->create(GeometryType => 'GeometryCollection');
            for my $g (@_) {
                $self->AddGeometry($g);
            }
            return $self;
        }
        *Collect = *ForceToCollection;
        sub Dissolve {
            my $self = shift;
            my @c;
            my $n = $self->GetGeometryCount;
            if ($n > 0) {
                for my $i (0..$n-1) {
                    push @c, $self->GetGeometryRef($i)->Clone;
                }
            } else {
                push @c, $self;
            }
            return @c;
        }
        *AsText = *ExportToWkt;
        *AsBinary = *ExportToWkb;
        *AsGML = *ExportToGML;
        *AsKML = *ExportToKML;
        *AsJSON = *ExportToJson;
        *BuildPolygonFromEdges = *Geo::OGR::BuildPolygonFromEdges;
        *ForceToPolygon = *Geo::OGR::ForceToPolygon;
        
    }
    sub GeometryType {
        my($type_or_name) = @_;
        if (defined $type_or_name) {
            return $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name} if 
                exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name};
            return $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name} if 
                exists $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name};
            croak "unknown geometry type constant value or name '$type_or_name'";
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
        my($name) = @_;
        my $driver;
        $driver = _GetDriver($name) if $name =~ /^\d+$/; # is the name an index to driver list?
        $driver = GetDriverByName("$name") unless $driver;
        croak "OGR driver with name '$name' not found (maybe support for it was not built in?)" unless $driver;
        return $driver;
    }
    *Driver = *GetDriver;
%}
