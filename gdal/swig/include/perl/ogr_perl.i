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

/* wrapped data source methods: */
%rename (_GetDriver) GetDriver;
%rename (_TestCapability) TestCapability;

/* wrapped layer methods: */
%rename (_ReleaseResultSet) ReleaseResultSet;
%rename (_CreateLayer) CreateLayer;
%rename (_DeleteLayer) DeleteLayer;
%rename (_CreateField) CreateField;
%rename (_DeleteField) DeleteField;
%rename (_Validate) Validate;

/* wrapped feature methods: */
%rename (_AlterFieldDefn) AlterFieldDefn;
%rename (_SetGeometry) SetGeometry;

/* wrapped geometry methods: */
%rename (_ExportToWkb) ExportToWkb;

%perlcode %{

package Geo::OGR::Driver;
use strict;
use warnings;
use Carp;
use vars qw /@CAPABILITIES %CAPABILITIES/;
for (keys %Geo::OGR::) {
    push(@CAPABILITIES, $1), next if /^ODrC(\w+)/;
}
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
    confess "No such capability defined for class Driver: '$cap'." unless defined $CAPABILITIES{$cap};
    return _TestCapability($self, $CAPABILITIES{$cap});
}

*Create = *CreateDataSource;
*Copy = *CopyDataSource;
*OpenDataSource = *Open;
*Delete = *DeleteDataSource;
*Name = *GetName;




package Geo::OGR::DataSource;
use strict;
use warnings;
use Carp;
use vars qw /@CAPABILITIES %CAPABILITIES %LAYERS %RESULT_SET/;
for (keys %Geo::OGR::) {
    push(@CAPABILITIES, $1), next if /^ODsC(\w+)/;
}
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
    confess "No such capability defined for class DataSource: '$cap'." unless defined $CAPABILITIES{$cap};
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

sub GetLayer {
    my($self, $name) = @_;
    my $layer = defined $name ? GetLayerByName($self, "$name") : GetLayerByIndex($self, 0);
    $name = '' unless defined $name;
    confess "No such layer: '$name'." unless $layer;
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
    my %defaults = ( Name => 'unnamed',
                     SRS => undef,
                     Options => {},
                     GeometryType => 'Unknown',
                     Schema => undef,
                     Fields => undef,
                     ApproxOK => 1);
    my %params;
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %params = %{$_[0]};
    } elsif (@_ % 2 == 0 and (defined $_[0] and exists $defaults{$_[0]})) {
        %params = @_;
    } else {
        ($params{Name}, $params{SRS}, $params{GeometryType}, $params{Options}, $params{Schema}) = @_;
    }
    for (keys %params) {
        carp "CreateLayer: unknown named parameter '$_'." unless exists $defaults{$_};
    }
    if (exists $params{Schema}) {
        my $s = $params{Schema};
        $params{GeometryType} = $s->{GeometryType} if exists $s->{GeometryType};
        $params{Fields} = $s->{Fields} if exists $s->{Fields};
        $params{Name} = $s->{Name} if exists $s->{Name};
    }
    $defaults{GeometryType} = 'None' if $params{Fields};
    for (keys %defaults) {
        $params{$_} = $defaults{$_} unless defined $params{$_};
    }
    confess "Unknown geometry type: '$params{GeometryType}'."
        unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}};
    my $gt = $Geo::OGR::Geometry::TYPE_STRING2INT{$params{GeometryType}};
    my $layer = _CreateLayer($self, $params{Name}, $params{SRS}, $gt, $params{Options});
    $LAYERS{tied(%$layer)} = $self;
    my $f = $params{Fields};
    if ($f) {
        confess "Named parameter 'Fields' must be a reference to an array." unless ref($f) eq 'ARRAY';
        for my $field (@$f) {
            $layer->CreateField($field);
        }
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
    confess "No such layer: '$name'." unless defined $index;
    _DeleteLayer($self, $index);
}




package Geo::OGR::Layer;
use strict;
use warnings;
use Carp;
use Scalar::Util 'blessed';
use vars qw /@CAPABILITIES %CAPABILITIES  %DEFNS/;
for (keys %Geo::OGR::) {
    push(@CAPABILITIES, $1), next if /^OLC(\w+)/;
}
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

sub GetDataSource {
    my $self = shift;
    return $Geo::OGR::DataSource::LAYERS{$self};
}
*DataSource = *GetDataSource;

sub GetDefn {
    my $self = shift;
    my $defn = $self->GetLayerDefn;
    $DEFNS{$defn} = $self;
    return $defn;
}

sub CreateField {
    my $self = shift;
    my %defaults = ( ApproxOK => 1,
                     Type => '' );
    my %params;
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %params = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %params = @_;
    } else {
        ($params{Defn}) = @_;
    }
    for (keys %defaults) {
        $params{$_} = $defaults{$_} unless defined $params{$_};
    }
    if (blessed($params{Defn}) and $params{Defn}->isa('Geo::OGR::FieldDefn')) {
        $self->_CreateField($params{Defn}, $params{ApproxOK});
    } elsif (blessed($_[0]) and $params{Defn}->isa('Geo::OGR::GeomFieldDefn')) {
        $self->CreateGeomField($params{Defn}, $params{ApproxOK});
    } else {
        my $a = $params{ApproxOK};
        delete $params{ApproxOK};
        if (exists $params{GeometryType}) {
            $params{Type} = $params{GeometryType};
            delete $params{GeometryType};
        }
        if (exists $Geo::OGR::FieldDefn::TYPE_STRING2INT{$params{Type}}) {
            my $fd = Geo::OGR::FieldDefn->new(%params);
            _CreateField($self, $fd, $a);
        } else {
            my $fd = Geo::OGR::GeomFieldDefn->new(%params);
            CreateGeomField($self, $fd, $a);
        }
    }
}

sub AlterFieldDefn {
    my $self = shift;
    my $field = shift;
    my $index;
    eval {
        $index = $self->GetFieldIndex($field);
    };
    confess "Only non-spatial fields can be altered.\n$@" if $@;
    if (blessed($_[0]) and $_[0]->isa('Geo::OGR::FieldDefn')) {
        _AlterFieldDefn($self, $index, @_);
    } elsif (@_ % 2 == 0) {
        my %params = @_;
        my $definition = Geo::OGR::FieldDefn->new(%params);
        my $flags = 0;
        $flags |= 1 if exists $params{Name};
        $flags |= 2 if exists $params{Type};
        $flags |= 4 if exists $params{Width} or exists $params{Precision};
        $flags |= 8 if exists $params{Nullable};
        $flags |= 16 if exists $params{Default};
        _AlterFieldDefn($self, $index, $definition, $flags);
    } else {
        croak "Usage: AlterFieldDefn(\$Name, \%NamedParameters)";
    }
}

sub DeleteField {
    my($self, $fn) = @_;
    my $d = $self->GetDefn;
    my $index = $d->GetFieldIndex($fn);
    $index = $fn if $index < 0;
    eval {
        _DeleteField($self, $index);
    };
    confess "Field not found: '$fn'. Only non-spatial fields can be deleted." if $@;
}

sub GetSchema {
    my $self = shift;
    carp "Schema of a layer should not be set directly." if @_;
    if (@_ and @_ % 2 == 0) {
        my %schema = @_;
        if ($schema{Fields}) {
            for my $field (@{$schema{Fields}}) {
                $self->CreateField($field);
            }
        }
    }
    return $self->GetDefn->Schema;
}
*Schema = *GetSchema;

sub Row {
    my $self = shift;
    my $update = @_ > 0;
    my %row = @_;
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
    my $FID = shift;
    my $feature = defined $FID ? $self->GetFeature($FID) : $self->GetNextFeature;
    return unless $feature;
    my $set = @_ > 0;
    unshift @_, $feature->GetFID if $set;
    my @ret;
    if (defined wantarray) {
        @ret = $feature->Tuple(@_);
    } else {
        $feature->Tuple(@_);
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
    confess "Usage: \$feature->InsertFeature(reference to a hash or array)." unless ref($feature);
    my $new = Geo::OGR::Feature->new($self->GetDefn);
    if (ref($feature) eq 'HASH') {
        $new->Row(%$feature);
    } elsif (ref($feature) eq 'ARRAY') {
        $new->Tuple(@$feature);
    } elsif (blessed($feature) and $feature->isa('Geo::OGR::Feature')) {
        $new->Row($feature->Row);
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

sub GetFieldNames {
    my $self = shift;
    my $d = $self->GetDefn;
    my @ret;
    for (my $i = 0; $i < $d->GetFieldCount; $i++) {
        push @ret, $d->GetFieldDefn($i)->Name();
    }
    for (my $i = 0; $i < $d->GetGeomFieldCount; $i++) {
        push @ret, $d->GetGeomFieldDefn($i)->Name();
    }
    return @ret;
}

sub GetFieldDefn {
    my ($self, $name) = @_;
    my $d = $self->GetDefn;
    for (my $i = 0; $i < $d->GetFieldCount; $i++) {
        my $fd = $d->GetFieldDefn($i);
        return $fd if $fd->Name eq $name;
    }
    for (my $i = 0; $i < $d->GetGeomFieldCount; $i++) {
        my $fd = $d->GetGeomFieldDefn($i);
        return $fd if $fd->Name eq $name;
    }
    confess "No such field: '$name'.";
}

sub GeometryType {
    my $self = shift;
    my $d = $self->GetDefn;
    my $fd = $d->GetGeomFieldDefn(0);
    return $fd->Type if $fd;
}

sub SpatialReference {
    my($self, $field, $sr) = @_;
    my $d = $self->GetDefn;
    my $i;
    if (not defined $field or (blessed($field) and $field->isa('Geo::OSR::SpatialReference'))) {
        $i = 0;
    } else {
        $i = $d->GetGeomFieldIndex($field);
    }
    my $d2 = $d->GetGeomFieldDefn($i);
    $d2->SpatialReference($sr) if defined $sr;
    return $d2->SpatialReference() if defined wantarray;
}




package Geo::OGR::FeatureDefn;
use strict;
use warnings;
use Encode;
use Carp;
use Scalar::Util 'blessed';

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::OGR::Feature::DEFNS{$self};
    delete $Geo::OGR::Layer::DEFNS{$self};
}
%}

%feature("shadow") OGRFeatureDefnShadow(const char* name_null_ok=NULL)
%{
use Carp;
use Scalar::Util 'blessed';
sub new {
    my $pkg = shift;
    my %schema;
    if (@_ == 1 and ref($_[0]) eq 'HASH') {
        %schema = %{$_[0]};
    } elsif (@_ and @_ % 2 == 0) {
        %schema = @_;
    }
    my $fields = $schema{Fields};
    confess "The Fields argument is not a reference to an array." if $fields and ref($fields) ne 'ARRAY';
    $schema{Name} = '' unless exists $schema{Name};
    my $self = Geo::OGRc::new_FeatureDefn($schema{Name});
    bless $self, $pkg;
    my $gt = $schema{GeometryType};
    if ($fields) {
        $self->DeleteGeomFieldDefn(0); # either default behavior or argument specified
    } else {
        $self->GeometryType($schema{GeometryType}) if exists $schema{GeometryType};
    }
    $self->StyleIgnored($schema{StyleIgnored}) if exists $schema{StyleIgnored};
    for my $fd (@{$fields}) {
        my $d = $fd;
        if (ref($fd) eq 'HASH') {
            if ($fd->{GeometryType} or exists $Geo::OGR::Geometry::TYPE_STRING2INT{$fd->{Type}}) {
                $d = Geo::OGR::GeomFieldDefn->new(%$fd);
            } else {
                $d = Geo::OGR::FieldDefn->new(%$fd);
            }
        }
        if (blessed($d) and $d->isa('Geo::OGR::FieldDefn')) {
            AddFieldDefn($self, $d);
        } elsif (blessed($d) and $d->isa('Geo::OGR::GeomFieldDefn')) {
            AddGeomFieldDefn($self, $d);
        } else {
            confess "Item in field list does not define a field.";
        }
    }
    return $self;
}
%}

%perlcode %{
*Name = *GetName;

sub GetSchema {
    my $self = shift;
    carp "Schema of a feature definition should not be set directly." if @_;
    if (@_ and @_ % 2 == 0) {
        my %schema = @_;
        if ($schema{Fields}) {
            for my $field (@{$schema{Fields}}) {
                $self->AddField($field);
            }
        }
    }
    my %schema;
    $schema{Name} = $self->Name();
    $schema{StyleIgnored} = $self->StyleIgnored();
    $schema{Fields} = [];
    for my $i (0..$self->GetFieldCount-1) {
        my $s = $self->GetFieldDefn($i)->Schema;
        push @{$schema{Fields}}, $s;
    }
    for my $i (0..$self->GetGeomFieldCount-1) {
        my $s = $self->GetGeomFieldDefn($i)->Schema;
        push @{$schema{Fields}}, $s;
    }
    return wantarray ? %schema : \%schema;
}
*Schema = *GetSchema;

sub AddField {
    my $self = shift;
    confess "Read-only definition." if $Geo::OGR::Feature::DEFNS{$self} or $Geo::OGR::Layer::DEFNS{$self};
    my %params;
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %params = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %params = @_;
    }
    $params{Type} = '' unless defined $params{Type};
    if (exists $Geo::OGR::FieldDefn::TYPE_STRING2INT{$params{Type}}) {
        my $fd = Geo::OGR::FieldDefn->new(%params);
        $self->AddFieldDefn($fd);
    } else {
        my $fd = Geo::OGR::GeomFieldDefn->new(%params);
        $self->AddGeomFieldDefn($fd);
    }
}

sub DeleteField {
    my ($self, $name) = @_;
    confess "Read-only definition." if $Geo::OGR::Feature::DEFNS{$self} or $Geo::OGR::Layer::DEFNS{$self};
    for my $i (0..$self->GetFieldCount-1) {
        confess "Non-geometry fields cannot be deleted." if $self->GetFieldDefn($i)->Name eq $name;
    }
    for my $i (0..$self->GetGeomFieldCount-1) {
        $self->DeleteGeomFieldDefn($i) if $self->GetGeomFieldDefn($i)->Name eq $name;
    }
    confess "No such field: '$name'.";
}

sub GetFieldNames {
    my $self = shift;
    my @names = ();
    for my $i (0..$self->GetFieldCount-1) {
        push @names, $self->GetFieldDefn($i)->Name;
    }
    for my $i (0..$self->GetGeomFieldCount-1) {
        push @names, $self->GetGeomFieldDefn($i)->Name;
    }
    return @names;
}

sub GetFieldDefn {
    my ($self, $name) = @_;
    for my $i (0..$self->GetFieldCount-1) {
        my $fd = $self->GetFieldDefn($i);
        return $fd if $fd->Name eq $name;
    }
    for my $i (0..$self->GetGeomFieldCount-1) {
        my $fd = $self->GetGeomFieldDefn($i);
        return $fd if $fd->Name eq $name;
    }
    confess "No such field: '$name'.";
}

sub GeomType {
    my ($self, $type) = @_;
    confess "Read-only definition." if $Geo::OGR::Feature::DEFNS{$self} or $Geo::OGR::Layer::DEFNS{$self};
    if (defined $type) {
        confess "Unknown geometry data type: '$type'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
        $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
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
use warnings;
use vars qw /%GEOMETRIES %DEFNS/;
use Carp;
use Encode;
use Scalar::Util 'blessed';
%}

%feature("shadow") OGRFeatureShadow()
%{
use Carp;
sub new {
    my $pkg = shift;
    if (blessed($_[0]) and $_[0]->isa('Geo::OGR::FeatureDefn')) {
        return $pkg->new($_[0]);
    } else {
        return $pkg->new(Geo::OGR::FeatureDefn->new(@_));
    }
}
%}

%perlcode %{
sub FETCH {
    my($self, $index) = @_;
    my $i;
    eval {$i = $self->_GetFieldIndex($index)};
    $self->GetField($i) unless $@;
    $i = $self->_GetGeomFieldIndex($index);
    $self->GetGeometry($i);
}

sub STORE {
    my $self = shift;
    my $index = shift;
    my $i;
    eval {$i = $self->_GetFieldIndex($index)};
    $self->SetField($i, @_) unless $@;
    $i = $self->_GetGeomFieldIndex($index);
    $self->SetGeometry($i, @_);
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

sub Validate {
    my $self = shift;
    my $flags = 0;
    for my $flag (@_) {
        my $f = eval '$Geo::OGR::'.uc($flag);
        $flags |= $f;
    }
    _Validate($self, $flags);
}

sub GetSchema {
    my $self = shift;
    confess "Schema of a feature cannot be set directly." if @_;
    return $self->GetDefnRef->Schema;
}
*Schema = *GetSchema;

sub Row {
    my $self = shift;
    my $nf = $self->GetFieldCount;
    my $ngf = $self->GetGeomFieldCount;
    if (@_) { # update
        my %row;
        if (@_ == 1 and ref($_[0]) eq 'HASH') {
            %row = %{$_[0]};
        } elsif (@_ and @_ % 2 == 0) {
            %row = @_;
        } else {
            confess 'Usage: $feature->Row(%FeatureData).';
        }
        $self->SetFID($row{FID}) if defined $row{FID};
        #$self->Geometry($schema, $row{Geometry}) if $row{Geometry};
        for my $name (keys %row) {
            next if $name eq 'FID';
            if ($name eq 'Geometry') {
                $self->SetGeometry(0, $row{$name});
                next;
            }
            my $f = 0;
            for my $i (0..$nf-1) {
                if ($self->GetFieldDefnRef($i)->Name eq $name) {
                    $self->SetField($i, $row{$name});
                    $f = 1;
                    last;
                }
            }
            next if $f;
            for my $i (0..$ngf-1) {
                if ($self->GetGeomFieldDefnRef($i)->Name eq $name) {
                    $self->SetGeometry($i, $row{$name});
                    $f = 1;
                    last;
                }
            }
            next if $f;
            carp "Feature->Row: Unknown field: '$name'.";
        }
    }
    return unless defined wantarray;
    my %row = ();
    for my $i (0..$nf-1) {
        my $name = $self->GetFieldDefnRef($i)->Name;
        $row{$name} = $self->GetField($i);
    }
    for my $i (0..$ngf-1) {
        my $name = $self->GetGeomFieldDefnRef($i)->Name;
        $name = 'Geometry' if $name eq '';
        $row{$name} = $self->GetGeometry($i);
    }
    $row{FID} = $self->GetFID;
    #$row{Geometry} = $self->Geometry;
    return \%row;
}

sub Tuple {
    my $self = shift;
    my $nf = $self->GetFieldCount;
    my $ngf = $self->GetGeomFieldCount;
    if (@_) {
        my $FID;
        $FID = shift if @_ == $nf + $ngf + 1;
        $self->SetFID($FID) if defined $FID;
        my $values = \@_;
        if (@$values != $nf + $ngf) {
            my $n = $nf + $ngf;
            confess "Too many or too few attribute values for a feature (need $n).";
        }
        my $index = 0; # index to non-geometry and geometry fields
        for my $i (0..$nf-1) {
            $self->SetField($i, $values->[$i]);
        }
        for my $i (0..$ngf-1) {
            $self->SetGeometry($i, $values->[$nf+$i]);
        }
    }
    return unless defined wantarray;
    my @ret = ($self->GetFID);
    for my $i (0..$nf-1) {
        my $v = $self->GetField($i);
        push @ret, $v;
    }
    for my $i (0..$ngf-1) {
        my $v = $self->GetGeometry($i);
        push @ret, $v;
    }
    return @ret;
}

sub GetDefn {
    my $self = shift;
    my $defn = $self->GetDefnRef;
    $DEFNS{$defn} = $self;
    return $defn;
}

*GetFieldNames = *Geo::OGR::Layer::GetFieldNames;
*GetFieldDefn = *Geo::OGR::Layer::GetFieldDefn;

sub _GetFieldIndex {
    my($self, $field) = @_;
    if ($field =~ /^\d+$/) {
        return $field if $field >= 0 and $field < $self->GetFieldCount;
    } else {
        for my $i (0..$self->GetFieldCount-1) {
            return $i if $self->GetFieldDefnRef($i)->Name eq $field;
        }
    }
    confess "No such field: '$field'.";
}

sub GetField {
    my($self, $field) = @_;
    $field = $self->_GetFieldIndex($field);
    return unless IsFieldSet($self, $field);
    my $type = GetFieldType($self, $field);
    if ($type == $Geo::OGR::OFTInteger) {
        return GetFieldAsInteger($self, $field);
    }
    if ($type == $Geo::OGR::OFTInteger64) {
        return GetFieldAsInteger64($self, $field);
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
    if ($type == $Geo::OGR::OFTInteger64List) {
        my $ret = GetFieldAsInteger64List($self, $field);
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
    confess "Perl bindings do not support field type '$Geo::OGR::FieldDefn::TYPE_INT2STRING{$type}'.";
}

sub UnsetField {
    my($self, $field) = @_;
    $field = $self->_GetFieldIndex($field);
    _UnsetField($self, $field);
}

sub SetField {
    my $self = shift;
    my $field = shift;
    $field = $self->_GetFieldIndex($field);
    if (@_ == 0 or !defined($_[0])) {
        _UnsetField($self, $field);
        return;
    }
    my $list = ref($_[0]) ? $_[0] : [@_];
    my $type = GetFieldType($self, $field);
    if ($type == $Geo::OGR::OFTInteger or
        $type == $Geo::OGR::OFTInteger64 or
        $type == $Geo::OGR::OFTReal or
        $type == $Geo::OGR::OFTString or
        $type == $Geo::OGR::OFTBinary)
    {
        _SetField($self, $field, $_[0]);
    }
    elsif ($type == $Geo::OGR::OFTIntegerList) {
        SetFieldIntegerList($self, $field, $list);
    }
    elsif ($type == $Geo::OGR::OFTInteger64List) {
        SetFieldInteger64List($self, $field, $list);
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
        confess "Perl bindings do not support field type '$Geo::OGR::FieldDefn::TYPE_INT2STRING{$type}'.";
    }
}

sub Field {
    my $self = shift;
    my $field = shift;
    $self->SetField($field, @_) if @_;
    $self->GetField($field);
}

sub _GetGeomFieldIndex {
    my($self, $field) = @_;
    if (not defined $field) {
        return 0 if $self->GetGeomFieldCount > 0;
    } if ($field =~ /^\d+$/) {
        return $field if $field >= 0 and $field < $self->GetGeomFieldCount;
    } else {
        for my $i (0..$self->GetGeomFieldCount-1) {
            return $i if $self->GetGeomFieldDefn($i)->Name eq $field;
        }
        return 0 if $self->GetGeomFieldCount > 0 and $field eq 'Geometry';
    }
    confess "No such field: '$field'.";
}

sub Geometry {
    my $self = shift;
    my $field = (ref($_[0]) eq '' or (@_ > 2 and @_ % 2 == 1)) ? shift : undef;
    $field = $self->_GetGeomFieldIndex($field);
    if (@_) {
        my $type = $self->GetDefn->GetGeomFieldDefn($field)->Type;
        my $geometry;
        if (@_ == 1) {
            $geometry = $_[0];
        } elsif (@_ and @_ % 2 == 0) {
            %$geometry = @_;
        }
        if (blessed($geometry) and $geometry->isa('Geo::OGR::Geometry')) {
            confess "The type of the inserted geometry ('".$geometry->GeometryType."') is not the field type ('$type')."
                if $type ne 'Unknown' and $type ne $geometry->GeometryType;
            eval {
                $self->SetGeomFieldDirectly($field, $geometry);
            };
            confess "$@" if $@;
            $GEOMETRIES{tied(%{$geometry})} = $self;
        } elsif (ref($geometry) eq 'HASH') {
            $geometry->{GeometryType} = $type unless exists $geometry->{GeometryType};
            eval {
                $geometry = Geo::OGR::Geometry->new($geometry);
            };
            confess "The type of the inserted geometry ('".$geometry->GeometryType."') is not the field type ('$type')."
                if $type ne 'Unknown' and $type ne $geometry->GeometryType;
            eval {
                $self->SetGeomFieldDirectly($field, $geometry);
            };
            confess "$@" if $@;
        } else {
            confess "'@_' does not define a geometry.";
        }
    }
    return unless defined wantarray;
    my $geometry = $self->GetGeomFieldRef($field);
    return unless $geometry;
    $GEOMETRIES{tied(%{$geometry})} = $self;
    return $geometry;
}
*GetGeometry = *Geometry;
*SetGeometry = *Geometry;

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
use warnings;
use vars qw /
    %SCHEMA_KEYS
    @TYPES @SUB_TYPES @JUSTIFY_VALUES
    %TYPE_STRING2INT %TYPE_INT2STRING
    %SUB_TYPE_STRING2INT %SUB_TYPE_INT2STRING
    %JUSTIFY_STRING2INT %JUSTIFY_INT2STRING
    /;
use Carp;
use Encode;
%SCHEMA_KEYS = map {$_ => 1} qw/Name Type SubType Justify Width Precision Nullable Default Ignored/;
for (keys %Geo::OGR::) {
    push(@TYPES, $1), next if /^OFT(\w+)/;
    push(@SUB_TYPES, $1), next if /^OFST(\w+)/;
    push(@JUSTIFY_VALUES, $1), next if /^OJ(\w+)/;
}
for my $string (@TYPES) {
    my $int = eval "\$Geo::OGR::OFT$string";
    $TYPE_STRING2INT{$string} = $int;
    $TYPE_INT2STRING{$int} = $string;
}
for my $string (@SUB_TYPES) {
    my $int = eval "\$Geo::OGR::OFST$string";
    $SUB_TYPE_STRING2INT{$string} = $int;
    $SUB_TYPE_INT2STRING{$int} = $string;
}
for my $string (@JUSTIFY_VALUES) {
    my $int = eval "\$Geo::OGR::OJ$string";
    $JUSTIFY_STRING2INT{$string} = $int;
    $JUSTIFY_INT2STRING{$int} = $string;
}

sub Types {
    return @TYPES;
}

sub SubTypes {
    return @SUB_TYPES;
}

sub JustifyValues {
    return @JUSTIFY_VALUES;
}
%}

%feature("shadow") OGRFieldDefnShadow( const char* name_null_ok="unnamed", OGRFieldType field_type=OFTString)
%{
use Carp;
sub new {
    my $pkg = shift;
    my ($name, $type) = ('unnamed', 'String');
    my %args;
    if (@_ == 0) {
    } elsif (@_ == 1) {
        $name = shift;
    } elsif (@_ == 2 and not $SCHEMA_KEYS{$_[0]}) {
        $name = shift;
        $type = shift;
    } else {
        my %named = @_;
        for my $key (keys %named) {
            if ($SCHEMA_KEYS{$key}) {
                $args{$key} = $named{$key};
            } else {
                carp "Unrecognized argument: '$key'." if $key ne 'Index';
            }
        }
        $name = $args{Name} if exists $args{Name};
        delete $args{Name};
        $type = $args{Type} if exists $args{Type};
        delete $args{Type};
    }
    confess "Unknown field type: '$type'." unless exists $TYPE_STRING2INT{$type};
    $type = $TYPE_STRING2INT{$type};
    my $self = Geo::OGRc::new_FieldDefn($name, $type);
    if (defined($self)) {
        bless $self, $pkg;
        $self->Schema(%args);
    }
    return $self;
}
%}

%perlcode %{
sub Schema {
    my $self = shift;
    if (@_) {
        my %args = @_;
        for my $key (keys %SCHEMA_KEYS) {
            eval '$self->'.$key.'($args{'.$key.'}) if exists $args{'.$key.'}';
            croak $@ if $@;
        }
    }
    return unless defined wantarray;
    my %schema = ();
    for my $key (keys %SCHEMA_KEYS) {
        $schema{$key} = eval '$self->'.$key;
    }
    return wantarray ? %schema : \%schema;
}
*GetSchema = *Schema;
*SetSchema = *Schema;

sub Name {
    my $self = shift;
    SetName($self, $_[0]) if @_;
    GetName($self) if defined wantarray;
}

sub Type {
    my($self, $type) = @_;
    if (defined $type) {
        confess "Unknown field type: '$type'." unless exists $TYPE_STRING2INT{$type};
        $type = $TYPE_STRING2INT{$type};
        SetType($self, $type);
    }
    return $TYPE_INT2STRING{GetType($self)} if defined wantarray;
}

sub SubType {
    my($self, $sub_type) = @_;
    if (defined $sub_type) {
        confess "Unknown field sub type: '$sub_type'." unless exists $SUB_TYPE_STRING2INT{$sub_type};
        $sub_type = $SUB_TYPE_STRING2INT{$sub_type};
        SetSubType($self, $sub_type);
    }
    return $SUB_TYPE_INT2STRING{GetSubType($self)} if defined wantarray;
}

sub Justify {
    my($self, $justify) = @_;
    if (defined $justify) {
        confess "Unknown justify value: '$justify'." unless exists $JUSTIFY_STRING2INT{$justify};
        $justify = $JUSTIFY_STRING2INT{$justify} if exists $JUSTIFY_STRING2INT{$justify};
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

sub Nullable {
    my $self = shift;
    SetNullable($self, $_[0]) if @_;
    IsNullable($self) if defined wantarray;
}

sub Default {
    my $self = shift;
    SetDefault($self, $_[0]) if @_;
    GetDefault($self) if defined wantarray;
}

sub Ignored {
    my $self = shift;
    SetIgnored($self, $_[0]) if @_;
    IsIgnored($self) if defined wantarray;
}




package Geo::OGR::GeomFieldDefn;
use strict;
use warnings;
use vars qw / %SCHEMA_KEYS /;
use Carp;
use Scalar::Util 'blessed';
%SCHEMA_KEYS = map {$_ => 1} qw/Name Type SpatialReference Nullable Ignored/;
%}

%feature("shadow") OGRGeomFieldDefnShadow( const char* name_null_ok="", OGRwkbGeometryType field_type = wkbUnknown)
%{
use Carp;
sub new {
    my $pkg = shift;
    my ($name, $type) = ('geom', 'Unknown');
    my %args;
    if (@_ == 0) {
    } elsif (@_ == 1) {
        $name = shift;
    } elsif (@_ == 2 and not $SCHEMA_KEYS{$_[0]}) {
        $name = shift;
        $type = shift;
    } else {
        my %named = @_;
        for my $key (keys %named) {
            if ($SCHEMA_KEYS{$key}) {
                $args{$key} = $named{$key};
            } else {
                carp "Unrecognized argument: '$key'." if $key ne 'Index';
            }
        }
        $name = $args{Name} if exists $args{Name};
        delete $args{Name};
        $type = $args{Type} if $args{Type};
        delete $args{Type};
        $type = $args{GeometryType} if $args{GeometryType};
        delete $args{GeometryType};
    }
    confess "Unknown geometry type: '$type'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    my $self = Geo::OGRc::new_GeomFieldDefn($name, $type);
    if (defined($self)) {
        bless $self, $pkg;
        $self->Schema(%args);
    }
    return $self;
}
%}

%perlcode %{
sub Schema {
    my $self = shift;
    if (@_) {
        my %args = @_;
        for my $key (keys %SCHEMA_KEYS) {
            eval '$self->'.$key.'($args{'.$key.'}) if exists $args{'.$key.'}';
            croak $@ if $@;
        }
    }
    return unless defined wantarray;
    my %schema = ();
    for my $key (keys %SCHEMA_KEYS) {
        $schema{$key} = eval '$self->'.$key;
    }
    return wantarray ? %schema : \%schema;
}
*GetSchema = *Schema;
*SetSchema = *Schema;

sub Name {
    my $self = shift;
    SetName($self, $_[0]) if @_;
    GetName($self) if defined wantarray;
}

sub Type {
    my($self, $type) = @_;
    if (defined $type) {
        confess "Unknown geometry type: '$type'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
        $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
        SetType($self, $type);
    }
    $Geo::OGR::Geometry::TYPE_INT2STRING{GetType($self)} if defined wantarray;
}

sub Types {
  return @Geo::OGR::Geometry::GEOMETRY_TYPES;
}

sub SpatialReference {
    my $self = shift;
    SetSpatialRef($self, $_[0]) if @_;
    GetSpatialRef($self) if defined wantarray;
}

sub Nullable {
    my $self = shift;
    SetNullable($self, $_[0]) if @_;
    IsNullable($self) if defined wantarray;
}

sub Ignored {
    my $self = shift;
    SetIgnored($self, $_[0]) if @_;
    IsIgnored($self) if defined wantarray;
}




package Geo::OGR::Geometry;
use strict;
use warnings;
use Carp;
use vars qw /
    @BYTE_ORDER_TYPES @GEOMETRY_TYPES
    %BYTE_ORDER_STRING2INT %BYTE_ORDER_INT2STRING
    %TYPE_STRING2INT %TYPE_INT2STRING
    /;
@BYTE_ORDER_TYPES = qw/XDR NDR/;
for my $string (@BYTE_ORDER_TYPES) {
    my $int = eval "\$Geo::OGR::wkb$string";
    $BYTE_ORDER_STRING2INT{$string} = $int;
    $BYTE_ORDER_INT2STRING{$int} = $string;
}
for (keys %Geo::OGR::) {
    next if /^wkb25/;
    next if /^wkb.DR/;
    push(@GEOMETRY_TYPES, $1), next if /^wkb(\w+)/;
}
for my $string (@GEOMETRY_TYPES) {
    my $int = eval "\$Geo::OGR::wkb$string";
    $TYPE_STRING2INT{$string} = $int;
    $TYPE_INT2STRING{$int} = $string;
}

sub ByteOrders {
    return @BYTE_ORDER_TYPES;
}

sub GeometryTypes {
    return @GEOMETRY_TYPES;
}

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::OGR::Feature::GEOMETRIES{$self};
}
%}

%feature("shadow") OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int wkb = 0, char *wkb_buf = 0, char *gml = 0 )
%{
use Carp;
sub new {
    my $pkg = shift;
    my ($type, $wkt, $wkb, $gml, $json, $srs, $points, $arc);
    my %param;
    if (@_ == 1 and ref($_[0]) eq 'HASH') {
        %param = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %param = @_;
    } else {
        ($param{GeometryType}) = @_;
    }
    $type = ($param{type} or $param{Type} or $param{GeometryType});
    $srs = ($param{srs} or $param{SRS});
    $wkt = ($param{wkt} or $param{WKT});
    $wkb = ($param{wkb} or $param{WKB});
    my $hex = ($param{hexewkb} or $param{HEXEWKB}); # PostGIS HEX EWKB
    my $srid;
    if ($hex) {
        # get and remove SRID
        $srid = substr($hex, 10, 8);
        substr($hex, 10, 8) = '';
    } else {
        $hex = ($param{hexwkb} or $param{HEXWKB});
    }
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
        confess "Unknown geometry type: '$type'." unless exists $TYPE_STRING2INT{$type};
        $type = $TYPE_STRING2INT{$type};
        $self = Geo::OGRc::new_Geometry($type); # flattens the type
        SetCoordinateDimension($self, 3) if Geo::OGR::GT_HasZ($type);
    } elsif (defined $arc) {
        $self = Geo::OGRc::ApproximateArcAngles(@$arc);
    } else {
        confess "Missing a parameter when creating a Geo::OGR::Geometry object.";
    }
    bless $self, $pkg if defined $self;
    $self->Points($points) if $points;
    return $self;
}
%}

%perlcode %{
sub ApproximateArcAngles {
    my %p = @_;
    my %default = ( Center => [0,0,0],
                    PrimaryRadius => 1,
                    SecondaryAxis => 1,
                    Rotation => 0,
                    StartAngle => 0,
                    EndAngle => 360,
                    MaxAngleStepSizeDegrees => 4 
        );
    for my $p (keys %p) {
        if (exists $default{$p}) {
            $p{$p} = $default{$p} unless defined $p{$p};
        } else {
            carp "Unknown named parameter: '$p'.";
        }
    }
    for my $p (keys %default) {
        $p{$p} = $default{$p} unless exists $p{$p};
    }
    confess "Usage: Center => [x,y,z]." unless ref($p{Center}) eq 'ARRAY';
    for my $i (0..2) {
        $p{Center}->[$i] = 0 unless defined $p{Center}->[$i];
    }
    return Geo::OGR::ApproximateArcAngles($p{Center}->[0], $p{Center}->[1], $p{Center}->[2], $p{PrimaryRadius}, $p{SecondaryAxis}, $p{Rotation}, $p{StartAngle}, $p{EndAngle}, $p{MaxAngleStepSizeDegrees});
}

sub As {
    my $self = shift;
    my %param;
    if (@_ == 1 and ref($_[0]) eq 'HASH') {
        %param = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %param = @_;
    } else {
        ($param{Format}, $param{x}) = @_;
    }
    $param{ByteOrder} = 'XDR' unless $param{ByteOrder};
    my $f = $param{Format};
    my $x = $param{x};
    $x = $param{ByteOrder} unless defined $x;
    if ($f =~ /text/i) {
        return $self->AsText;
    } elsif ($f =~ /wkt/i) {
        if ($f =~ /iso/i) {
            return $self->ExportToIsoWkt;
        } else {
            return $self->AsText;
        }
    } elsif ($f =~ /binary/i) {
        return $self->AsBinary($x);        
    } elsif ($f =~ /wkb/i) {
        if ($f =~ /iso/i) {
            return $self->ExportToIsoWkb;
        } elsif ($f =~ /hexe/i) {
            $param{srid} = 'XDR' unless $param{srid};
            $x = $param{srid} unless defined $x;
            return $self->AsHEXEWKB($x);
        } elsif ($f =~ /hex/i) {
            return $self->AsHEXWKB;
        } else {
            return $self->AsBinary($x);
        }
    } elsif ($f =~ /gml/i) {
        return $self->AsGML;
    } elsif ($f =~ /kml/i) {
        return $self->AsKML;
    } elsif ($f =~ /json/i) {
        return $self->AsJSON;
    } else {
        confess "Unsupported format: $f.";
    }
}

sub AsHEXWKB {
    my ($self) = @_;
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
    my ($self, $srid) = @_;
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
    my $flat = not Geo::OGR::GT_HasZ($t);
    $t = Geo::OGR::GT_Flatten($t);
    $t = $TYPE_INT2STRING{$t};
    my $points = shift;
    if ($points) {
        Empty($self);
        if ($t eq 'Unknown' or $t eq 'None' or $t eq 'GeometryCollection') {
            confess "Can't set points of a geometry of type '$t'.";
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
                my $ring = Geo::OGR::Geometry->new('LinearRing');
                $ring->SetCoordinateDimension(3) unless $flat;
                $ring->Points($r);
                $self->AddGeometryDirectly($ring);
            }
        } elsif ($t eq 'MultiPoint') {
            for my $p (@$points) {
                my $point = Geo::OGR::Geometry->new($flat ? 'Point' : 'Point25D');
                $point->Points($p);
                $self->AddGeometryDirectly($point);
            }
        } elsif ($t eq 'MultiLineString') {
            for my $l (@$points) {
                my $linestring = Geo::OGR::Geometry->new($flat ? 'LineString' : 'LineString25D');
                $linestring->Points($l);
                $self->AddGeometryDirectly($linestring);
            }
        } elsif ($t eq 'MultiPolygon') {
            for my $p (@$points) {
                my $polygon = Geo::OGR::Geometry->new($flat ? 'Polygon' : 'Polygon25D');
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
    if (defined $bo) {
        confess "Unknown byte order: '$bo'." unless exists $BYTE_ORDER_STRING2INT{$bo};
        $bo = $BYTE_ORDER_STRING2INT{$bo};
    }
    return _ExportToWkb($self, $bo);
}

sub ForceTo {
    my $self = shift;
    my $type = shift;
    confess "Unknown geometry type: '$type'." unless exists $TYPE_STRING2INT{$type};
    $type = $TYPE_STRING2INT{$type};
    eval {
        $self = Geo::OGR::ForceTo($self, $type, @_);
    };
    confess $@ if $@;
    return $self;
}

sub ForceToLineString {
    my $self = shift;
    $self = Geo::OGR::ForceToLineString($self);
    return $self;
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
    my $self = Geo::OGR::Geometry->new(GeometryType => 'GeometryCollection');
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


package Geo::OGR;
use strict;
use warnings;
use Carp;

sub GeometryType {
    my($type_or_name) = @_;
    if (defined $type_or_name) {
        return $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name} if
            exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name};
        return $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name} if
            exists $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name};
        confess "Unknown geometry type: '$type_or_name'.";
    } else {
        return @Geo::OGR::Geometry::GEOMETRY_TYPES;
    }
}

sub GeometryTypeModify {
    my($type, $modifier) = @_;
    confess "Unknown geometry type: '$type'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_Flatten($type)} if $modifier =~ /flat/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_SetZ($type)} if $modifier =~ /z/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetCollection($type)} if $modifier =~ /collection/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetCurve($type)} if $modifier =~ /curve/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetLinear($type)} if $modifier =~ /linear/i;
    confess "Unknown geometry type modifier: '$modifier'.";
}

sub GeometryTypeTest {
    my($type, $test, $type2) = @_;
    confess "Unknown geometry type: '$type'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    if (defined $type2) {
        confess "Unknown geometry type: '$type2'." unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type2};
        $type2 = $Geo::OGR::Geometry::TYPE_STRING2INT{$type2};
    } else {
        confess "Usage: GeometryTypeTest(type1, 'is_subclass_of', type2)." if $test =~ /subclass/i;
    }
    return GT_HasZ($type) if $test =~ /z/i;
    return GT_IsSubClassOf($type, $type2) if $test =~ /subclass/i;
    return GT_IsCurve($type) if $test =~ /curve/i;
    return GT_IsSurface($type) if $test =~ /surface/i;
    return GT_IsNonLinear($type) if $test =~ /linear/i;
    confess "Unknown geometry type test: '$test'.";
}

sub RELEASE_PARENTS {
}

*ByteOrders = *Geo::OGR::Geometry::ByteOrders;
*GeometryTypes = *Geo::OGR::Geometry::GeometryTypes;

sub GetDriverNames {
    my @names;
    for my $i (0..GetDriverCount()-1) {
        push @names, _GetDriver($i)->Name;
    }
    return @names;
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
    $name = 0 unless defined $name;
    my $driver;
    $driver = _GetDriver($name) if $name =~ /^\d+$/; # is the name an index to driver list?
    $driver = GetDriverByName("$name") unless $driver;
    return $driver if $driver;
    confess "Driver not found: '$name'. Maybe support for it was not built in?";
}
*Driver = *GetDriver;
%}
