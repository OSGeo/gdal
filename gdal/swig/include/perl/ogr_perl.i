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
%include confess.i
%include cpl_exceptions.i

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%import typemaps_perl.i

%import destroy.i

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

/* wrapped layer methods: */
%rename (_TestCapability) TestCapability;
%rename (_ReleaseResultSet) ReleaseResultSet;
%rename (_CreateLayer) CreateLayer;
%rename (_DeleteLayer) DeleteLayer;
%rename (_CreateField) CreateField;
%rename (_DeleteField) DeleteField;
%rename (_Validate) Validate;
%rename (_GetFeature) GetFeature;

/* wrapped feature methods: */
%rename (_AlterFieldDefn) AlterFieldDefn;
%rename (_SetGeometry) SetGeometry;

/* wrapped geometry methods: */
%rename (_ExportToWkb) ExportToWkb;

%perlcode %{

package Geo::OGR;
our $VERSION = '2.0101'; # this needs to be the same as that in gdal_perl.i

sub Driver {
    return 'Geo::GDAL::Driver' unless @_;
    bless Geo::GDAL::Driver(@_), 'Geo::OGR::Driver';
}
*GetDriver = *Driver;

sub GetDriverNames {
    my @names;
    for my $i (0..Geo::GDAL::GetDriverCount()-1) {
        my $driver = Geo::GDAL::GetDriver($i);
        push @names, $driver->Name if $driver->TestCapability('VECTOR');
    }
    return @names;
}
*DriverNames = *GetDriverNames;

sub Drivers {
    my @drivers;
    for my $i (0..GetDriverCount()-1) {
        my $driver = Geo::GDAL::GetDriver($i);
        push @drivers, $driver if $driver->TestCapability('VECTOR');
    }
    return @drivers;
}

sub Open {
    my @p = @_; # name, update
    my @flags = qw/VECTOR/;
    push @flags, qw/UPDATE/ if $p[1];
    my $dataset = Geo::GDAL::OpenEx($p[0], \@flags);
    Geo::GDAL::error("Failed to open $p[0]. Is it a vector dataset?") unless $dataset;
    return $dataset;
}

sub OpenShared {
    my @p = @_; # name, update
    my @flags = qw/VECTOR SHARED/;
    push @flags, qw/UPDATE/ if $p[1];
    my $dataset = Geo::GDAL::OpenEx($p[0], \@flags);
    Geo::GDAL::error("Failed to open $p[0]. Is it a vector dataset?") unless $dataset;
    return $dataset;
}

package Geo::OGR::Driver;
our @ISA = qw/Geo::GDAL::Driver/;

sub Create {
    my ($self, $name, $options) = @_; # name, options
    $options //= {};
    $self->SUPER::Create(Name => $name, Width => 0, Height => 0, Bands => 0, Type => 'Byte', Options => $options);
}

sub Copy {
    my ($self, @p) = @_; # src, name, options
    my $strict = 1; # the default in bindings
    $strict = 0 if $p[2] && $p[2]->{STRICT} eq 'NO';
    $self->SUPER::Copy($p[1], $p[0], $strict, @{$p[2..4]}); # path, src, strict, options, cb, cb_data
}

sub Open {
    my $self = shift;
    my @p = @_; # name, update
    my @flags = qw/VECTOR/;
    push @flags, qw/UPDATE/ if $p[1];
    my $dataset = Geo::GDAL::OpenEx($p[0], \@flags, [$self->Name()]);
    Geo::GDAL::error("Failed to open $p[0]. Is it a vector dataset?") unless $dataset;
    return $dataset;
}


package Geo::OGR::DataSource;

*Open = *Geo::OGR::Open;
*OpenShared = *Geo::OGR::OpenShared;


package Geo::OGR::Layer;
use strict;
use warnings;
use Carp;
use Scalar::Util 'blessed';
use vars qw /@CAPABILITIES %CAPABILITIES %DEFNS %FEATURES/;
for (keys %Geo::OGR::) {
    push(@CAPABILITIES, $1), next if /^OLC(\w+)/;
}
for my $s (@CAPABILITIES) {
    my $cap = eval "\$Geo::OGR::OLC$s";
    $CAPABILITIES{$s} = $cap;
}

sub DESTROY {
    my $self = shift;
    unless ($self->isa('SCALAR')) {
        return unless $self->isa('HASH');
        $self = tied(%{$self});
        return unless defined $self;
    }
    if ($Geo::GDAL::Dataset::RESULT_SET{$self}) {
        $Geo::GDAL::Dataset::LAYERS{$self}->_ReleaseResultSet($self);
        delete $Geo::GDAL::Dataset::RESULT_SET{$self}
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENTS();
}

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::GDAL::Dataset::LAYERS{$self};
}

sub Dataset {
    my $self = shift;
    return $Geo::GDAL::Dataset::LAYERS{tied(%$self)};
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
    return $Geo::GDAL::Dataset::LAYERS{tied(%$self)};
}
*DataSource = *GetDataSource;

sub GetDefn {
    my $self = shift;
    my $defn = $self->GetLayerDefn;
    $DEFNS{tied(%$defn)} = $self;
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
    for my $k (keys %defaults) {
        $params{$k} //= $defaults{$k};
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
        } elsif (exists $Geo::OGR::Geometry::TYPE_STRING2INT{$params{Type}}) {
            my $fd = Geo::OGR::GeomFieldDefn->new(%params);
            CreateGeomField($self, $fd, $a);
        } else {
            Geo::GDAL::error("Invalid field type: $params{Type}.")
        }
    }
}

sub AlterFieldDefn {
    my $self = shift;
    my $field = shift;
    my $index = $self->GetLayerDefn->GetFieldIndex($field);
    if (blessed($_[0]) and $_[0]->isa('Geo::OGR::FieldDefn')) {
        _AlterFieldDefn($self, $index, @_);
    } else {
        my $params = @_ % 2 == 0 ? {@_} : shift;
        my $definition = Geo::OGR::FieldDefn->new($params);
        my $flags = 0;
        $flags |= 1 if exists $params->{Name};
        $flags |= 2 if exists $params->{Type};
        $flags |= 4 if exists $params->{Width} or exists $params->{Precision};
        $flags |= 8 if exists $params->{Nullable};
        $flags |= 16 if exists $params->{Default};
        _AlterFieldDefn($self, $index, $definition, $flags);
    }
}

sub DeleteField {
    my($self, $field) = @_;
    my $index = $self->GetLayerDefn->GetFieldIndex($field);
    _DeleteField($self, $index);
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
    Geo::GDAL::error("Usage: \$feature->InsertFeature(reference to a hash or array).") unless ref($feature);
    my $new = Geo::OGR::Feature->new($self->GetDefn);
    if (ref($feature) eq 'HASH') {
        $new->Row(%$feature);
    } elsif (ref($feature) eq 'ARRAY') {
        $new->Tuple(@$feature);
    } elsif (blessed($feature) and $feature->isa('Geo::OGR::Feature')) {
        $new->Row($feature->Row);
    }
    $self->CreateFeature($new);
    return unless defined wantarray;
    $FEATURES{tied(%$new)} = $self;
    return $new;
}

sub GetFeature {
    my ($self, $fid) = @_;
    $fid //= 0;
    my $f = $self->_GetFeature($fid);
    $FEATURES{tied(%$f)} = $self;
    return $f;
}

sub ForFeatures {
    my $self = shift;
    my $code = shift;
    my $in_place = shift;
    $self->ResetReading;
    while (my $f = $self->GetNextFeature) {
        $FEATURES{tied(%$f)} = $self;
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
    Geo::GDAL::error(2, $name, 'Field');
}

sub GeometryType {
    my $self = shift;
    my $field = shift;
    $field //= 0;
    my $fd = $self->GetDefn->GetGeomFieldDefn($field);
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

sub Feature {
    my $self = shift;
    return $Geo::OGR::Feature::DEFNS{tied(%$self)};
}
%}

%feature("shadow") OGRFeatureDefnShadow(const char* name_null_ok=NULL)
%{
use strict;
use warnings;
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
    Geo::GDAL::error("The 'Fields' argument must be an array reference.") if $fields and ref($fields) ne 'ARRAY';
    $schema{Name} //= '';
    my $self = Geo::OGRc::new_FeatureDefn($schema{Name});
    bless $self, $pkg;
    my $gt = $schema{GeometryType};
    if ($gt) {
        $self->GeometryType($gt);
    } elsif ($fields) {
        $self->DeleteGeomFieldDefn(0);
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
            Geo::GDAL::error("Do not mix GeometryType and geometry fields in Fields.") if $gt;
            AddGeomFieldDefn($self, $d);
        } else {
            Geo::GDAL::error("Item in field list does not define a field.");
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
    Geo::GDAL::error("Read-only definition.") if $Geo::OGR::Feature::DEFNS{tied(%$self)} || $Geo::OGR::Layer::DEFNS{tied(%$self)};
    my %params;
    if (@_ == 0) {
    } elsif (ref($_[0]) eq 'HASH') {
        %params = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %params = @_;
    }
    $params{Type} //= '';
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
    Geo::GDAL::error("Read-only definition.") if $Geo::OGR::Feature::DEFNS{tied(%$self)} || $Geo::OGR::Layer::DEFNS{tied(%$self)};
    for my $i (0..$self->GetFieldCount-1) {
        Geo::GDAL::error("Non-geometry fields cannot be deleted.") if $self->GetFieldDefn($i)->Name eq $name;
    }
    for my $i (0..$self->GetGeomFieldCount-1) {
        $self->DeleteGeomFieldDefn($i) if $self->GetGeomFieldDefn($i)->Name eq $name;
    }
    Geo::GDAL::error(2, $name, 'Field');
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
    Geo::GDAL::error(2, $name, 'Field');
}

sub GeomType {
    my ($self, $type) = @_;
    Geo::GDAL::error("Read-only definition.") if $Geo::OGR::Feature::DEFNS{tied(%$self)} || $Geo::OGR::Layer::DEFNS{tied(%$self)};
    if (defined $type) {
        $type = Geo::GDAL::string2int($type, \%Geo::OGR::Geometry::TYPE_STRING2INT);
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

%feature("shadow") OGRFeatureShadow( OGRFeatureDefnShadow *feature_def )
%{
use Carp;
sub new {
    my $pkg = shift;
    my $arg = blessed($_[0]);
    my $defn;
    if ($arg && $arg eq 'Geo::OGR::FeatureDefn') {
        $defn = $_[0];
    } else {
        $defn = Geo::OGR::FeatureDefn->new(@_);
    }
    my $self = Geo::OGRc::new_Feature($defn);
    bless $self, $pkg if defined($self);
}
%}

%perlcode %{

sub RELEASE_PARENTS {
    my $self = shift;
    delete $Geo::OGR::Layer::FEATURES{$self};
}

sub Layer {
    my $self = shift;
    return $Geo::OGR::Layer::FEATURES{tied(%$self)};
}

sub FETCH {
    my($self, $index) = @_;
    my $i;
    eval {$i = $self->GetFieldIndex($index)};
    return $self->GetField($i) unless $@;
    Geo::GDAL::error("'$index' is not a non-spatial field and it is not safe to retrieve geometries from a feature this way.");
}

sub STORE {
    my $self = shift;
    my $index = shift;
    my $i;
    eval {$i = $self->GetFieldIndex($index)};
    unless ($@) {
      $self->SetField($i, @_);
    } else {
      $i = $self->GetGeomFieldIndex($index);
      $self->Geometry($i, @_);
    }
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
    Geo::GDAL::error("Schema of a feature cannot be set directly.") if @_;
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
            Geo::GDAL::error('Usage: $feature->Row(%FeatureData).');
        }
        $self->SetFID($row{FID}) if defined $row{FID};
        #$self->Geometry($schema, $row{Geometry}) if $row{Geometry};
        for my $name (keys %row) {
            next if $name eq 'FID';
            if ($name eq 'Geometry') {
                $self->Geometry(0, $row{$name});
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
                    $self->Geometry($i, $row{$name});
                    $f = 1;
                    last;
                }
            }
            next if $f;
            carp "Unknown field: '$name'.";
        }
    }
    return unless defined wantarray;
    my %row = ();
    for my $i (0..$nf-1) {
        my $name = $self->GetFieldDefnRef($i)->Name;
        $row{$name} = $self->GetField($i);
    }
    for my $i (0..$ngf-1) {
        my $name = $self->GetGeomFieldDefnRef($i)->Name || 'Geometry';
        $row{$name} = $self->GetGeometry($i);
    }
    $row{FID} = $self->GetFID;
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
            Geo::GDAL::error("Too many or too few attribute values for a feature (need $n).");
        }
        my $index = 0; # index to non-geometry and geometry fields
        for my $i (0..$nf-1) {
            $self->SetField($i, $values->[$i]);
        }
        for my $i (0..$ngf-1) {
            $self->Geometry($i, $values->[$nf+$i]);
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
    $DEFNS{tied(%$defn)} = $self;
    return $defn;
}

*GetGeomFieldDefn = *GetGeomFieldDefnRef;

*GetFieldNames = *Geo::OGR::Layer::GetFieldNames;
*GetFieldDefn = *Geo::OGR::Layer::GetFieldDefn;

sub GetField {
    my($self, $field) = @_;
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
        return GetFieldAsBinary($self, $field);
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
        my @ret = GetFieldAsDateTime($self, $field);
        return wantarray ? @ret : [@ret];
    }
    Geo::GDAL::error("Perl bindings do not support field type '$Geo::OGR::FieldDefn::TYPE_INT2STRING{$type}'.");
}

sub UnsetField {
    my($self, $field) = @_;
    _UnsetField($self, $field);
}

sub SetField {
    my $self = shift;
    my $field = shift;
    my $arg = $_[0];
    if (@_ == 0 or !defined($arg)) {
        _UnsetField($self, $field);
        return;
    }
    $arg = [@_] if @_ > 1;
    my $type = $self->GetFieldType($field);
    if (ref($arg)) {
        if ($type == $Geo::OGR::OFTIntegerList) {
            SetFieldIntegerList($self, $field, $arg);
        }
        elsif ($type == $Geo::OGR::OFTInteger64List) {
            SetFieldInteger64List($self, $field, $arg);
        }
        elsif ($type == $Geo::OGR::OFTRealList) {
            SetFieldDoubleList($self, $field, $arg);
        }
        elsif ($type == $Geo::OGR::OFTStringList) {
            SetFieldStringList($self, $field, $arg);
        }
        elsif ($type == $Geo::OGR::OFTDate) {
            _SetField($self, $field, @$arg[0..2], 0, 0, 0, 0);
        }
        elsif ($type == $Geo::OGR::OFTTime) {
            $arg->[3] //= 0;
            _SetField($self, $field, 0, 0, 0, @$arg[0..3]);
        }
        elsif ($type == $Geo::OGR::OFTDateTime) {
            $arg->[6] //= 0;
            _SetField($self, $field, @$arg[0..6]);
        }
        else {
            _SetField($self, $field, @$arg);
        }
    } else {
        if ($type == $Geo::OGR::OFTBinary) {
            #$arg = unpack('H*', $arg); # remove when SetFieldBinary is available
            $self->SetFieldBinary($field, $arg);
        } else {
            _SetField($self, $field, $arg);
        }
    }
}

sub Field {
    my $self = shift;
    my $field = shift;
    $self->SetField($field, @_) if @_;
    $self->GetField($field) if defined wantarray;
}

sub Geometry {
    my $self = shift;
    my $field = ((@_ > 0 and ref($_[0]) eq '') or (@_ > 2 and @_ % 2 == 1)) ? shift : 0;
    my $geometry;
    if (@_ and @_ % 2 == 0) {
        %$geometry = @_;
    } else {
        $geometry = shift;
    }
    if ($geometry) {
        my $type = $self->GetDefn->GetGeomFieldDefn($field)->Type;
        if (blessed($geometry) and $geometry->isa('Geo::OGR::Geometry')) {
            my $gtype = $geometry->GeometryType;
            Geo::GDAL::error("The type of the inserted geometry ('$gtype') is not the same as the type of the field ('$type').")
                if $type ne 'Unknown' and $type ne $gtype;
            eval {
                $self->SetGeomFieldDirectly($field, $geometry->Clone);
            };
            confess Geo::GDAL->last_error if $@;
        } elsif (ref($geometry) eq 'HASH') {
            $geometry->{GeometryType} //= $type;
            eval {
                $geometry = Geo::OGR::Geometry->new($geometry);
            };
            my $gtype = $geometry->GeometryType;
            Geo::GDAL::error("The type of the inserted geometry ('$gtype') is not the same as the type of the field ('$type').")
                if $type ne 'Unknown' and $type ne $gtype;
            eval {
                $self->SetGeomFieldDirectly($field, $geometry);
            };
            confess Geo::GDAL->last_error if $@;
        } else {
            Geo::GDAL::error("Usage: \$feature->Geometry([field],[geometry])");
        }
    }
    return unless defined wantarray;
    $geometry = $self->GetGeomFieldRef($field);
    return unless $geometry;
    $GEOMETRIES{tied(%$geometry)} = $self;
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
    my $params = {Name => 'unnamed', Type => 'String'};
    if (@_ == 0) {
    } elsif (@_ == 1 and not ref $_[0]) {
        $params->{Name} = shift;
    } elsif (@_ == 2 and not $Geo::OGR::FieldDefn::SCHEMA_KEYS{$_[0]}) {
        $params->{Name} = shift;
        $params->{Type} = shift;
    } else {
        my $tmp = @_ % 2 == 0 ? {@_} : shift;
        for my $key (keys %$tmp) {
            if ($Geo::OGR::FieldDefn::SCHEMA_KEYS{$key}) {
                $params->{$key} = $tmp->{$key};
            } else {
                carp "Unknown parameter: '$key'." if $key ne 'Index';
            }
        }
    }
    $params->{Type} = Geo::GDAL::string2int($params->{Type}, \%Geo::OGR::FieldDefn::TYPE_STRING2INT);
    my $self = Geo::OGRc::new_FieldDefn($params->{Name}, $params->{Type});
    bless $self, $pkg;
    delete $params->{Name};
    delete $params->{Type};
    $self->Schema($params);
    return $self;
}
%}

%perlcode %{
sub Schema {
    my $self = shift;
    if (@_) {
        my $params = @_ % 2 == 0 ? {@_} : shift;
        for my $key (keys %SCHEMA_KEYS) {
            next unless exists $params->{$key};
            eval "\$self->$key(\$params->{$key})";
            confess(Geo::GDAL->last_error()) if $@;
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
        Geo::GDAL::error(1, $type, \%TYPE_STRING2INT) unless exists $TYPE_STRING2INT{$type};
        $type = $TYPE_STRING2INT{$type};
        SetType($self, $type);
    }
    return $TYPE_INT2STRING{GetType($self)} if defined wantarray;
}

sub SubType {
    my($self, $sub_type) = @_;
    if (defined $sub_type) {
        Geo::GDAL::error(1, $sub_type, \%SUB_TYPE_STRING2INT) unless exists $SUB_TYPE_STRING2INT{$sub_type};
        $sub_type = $SUB_TYPE_STRING2INT{$sub_type};
        SetSubType($self, $sub_type);
    }
    return $SUB_TYPE_INT2STRING{GetSubType($self)} if defined wantarray;
}

sub Justify {
    my($self, $justify) = @_;
    if (defined $justify) {
        Geo::GDAL::error(1, $justify, \%JUSTIFY_STRING2INT) unless exists $JUSTIFY_STRING2INT{$justify};
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
    my $params = {Name => 'geom', Type => 'Unknown'};
    if (@_ == 0) {
    } elsif (@_ == 1) {
        $params->{Name} = shift;
    } elsif (@_ == 2 and not $Geo::OGR::GeomFieldDefn::SCHEMA_KEYS{$_[0]}) {
        $params->{Name} = shift;
        $params->{Type} = shift;
    } else {
        my $tmp = @_ % 2 == 0 ? {@_} : shift;
        for my $key (keys %$tmp) {
            if ($Geo::OGR::GeomFieldDefn::SCHEMA_KEYS{$key}) {
                $params->{$key} = $tmp->{$key};
            } else {
                carp "Unknown parameter: '$key'." if $key ne 'Index' && $key ne 'GeometryType';
            }
        }
        $params->{Type} //= $tmp->{GeometryType};
    }
    $params->{Type} = Geo::GDAL::string2int($params->{Type}, \%Geo::OGR::Geometry::TYPE_STRING2INT);
    my $self = Geo::OGRc::new_GeomFieldDefn($params->{Name}, $params->{Type});
    bless $self, $pkg;
    delete $params->{Name};
    delete $params->{Type};
    $self->Schema($params);
    return $self;
}
%}

%perlcode %{
sub Schema {
    my $self = shift;
    if (@_) {
        my $params = @_ % 2 == 0 ? {@_} : shift;
        for my $key (keys %SCHEMA_KEYS) {
            next unless exists $params->{$key};
            eval "\$self->$key(\$params->{$key})";
            confess Geo::GDAL->last_error() if $@;
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
        $type = Geo::GDAL::string2int($type, \%Geo::OGR::Geometry::TYPE_STRING2INT);
        SetType($self, $type);
    }
    $Geo::OGR::Geometry::TYPE_INT2STRING{GetType($self)} if defined wantarray;
}
*GeometryType = *Type;

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
    if ($string =~ /25D/) {
        my $s = $string;
        $s =~ s/25D/Z/;
        $TYPE_STRING2INT{$s} = $int;
    }
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

sub Feature {
    my $self = shift;
    return $Geo::OGR::Feature::GEOMETRIES{tied(%$self)};
}
%}

%feature("shadow") OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int wkb = 0, char *wkb_buf = 0, char *gml = 0 )
%{
use Carp;
sub new {
    my $pkg = shift;
    my %param;
    if (@_ == 1 and ref($_[0]) eq 'HASH') {
        %param = %{$_[0]};
    } elsif (@_ % 2 == 0) {
        %param = @_;
    } else {
        ($param{GeometryType}) = @_;
    }
    my $type = $param{GeometryType} // $param{Type} // $param{type};
    my $srs = $param{SRS} // $param{srs};
    my $wkt = $param{WKT} // $param{wkt};
    my $wkb = $param{WKB} // $param{wkb};
    my $hex = $param{HEXEWKB} // $param{HEX_EWKB} // $param{hexewkb} // $param{hex_ewkb};
    my $srid;
    if ($hex) {
        # EWKB contains SRID
        $srid = substr($hex, 10, 8);
        substr($hex, 10, 8) = '';
    } else {
        $hex = $param{HEXWKB} // $param{HEX_WKB} // $param{hexwkb} // $param{hex_wkb};
    }
    if ($hex) {
        $wkb = '';
        for (my $i = 0; $i < length($hex); $i+=2) {
            $wkb .= chr(hex(substr($hex,$i,2)));
        }
    }
    my $gml = $param{GML} // $param{gml};
    my $json = $param{GeoJSON} // $param{geojson} // $param{JSON} // $param{json};
    my $points = $param{Points} // $param{points};
    my $arc = $param{Arc} // $param{arc};
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
        $type = Geo::GDAL::string2int($type, \%Geo::OGR::Geometry::TYPE_STRING2INT);
        $self = Geo::OGRc::new_Geometry($type); # flattens the type
        $self->Set3D(1) if Geo::OGR::GT_HasZ($type);
        $self->SetMeasured(1) if Geo::OGR::GT_HasM($type);
    } elsif (defined $arc) {
        $self = Geo::OGRc::ApproximateArcAngles(@$arc);
    } else {
        Geo::GDAL::error(1, undef, map {$_=>1} qw/GeometryType WKT WKB HEXEWKB HEXWKB GML GeoJSON Arc/);
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
            $p{$p} //= $default{$p};
        } else {
            carp "Unknown parameter: '$p'.";
        }
    }
    for my $p (keys %default) {
        $p{$p} //= $default{$p};
    }
    Geo::GDAL::error("Usage: Center => [x,y,z].") unless ref($p{Center}) eq 'ARRAY';
    for my $i (0..2) {
        $p{Center}->[$i] //= 0;
    }
    return Geo::OGR::ApproximateArcAngles($p{Center}->[0], $p{Center}->[1], $p{Center}->[2], $p{PrimaryRadius}, $p{SecondaryAxis}, $p{Rotation}, $p{StartAngle}, $p{EndAngle}, $p{MaxAngleStepSizeDegrees});
}

sub As {
    my $self = shift;
    my $p = Geo::GDAL::named_parameters(\@_, Format => undef, ByteOrder => 'XDR', SRID => undef, Options => undef, AltitudeMode => undef);
    my $f = $p->{format};
    if ($f =~ /text/i) {
        return $self->AsText;
    } elsif ($f =~ /wkt/i) {
        if ($f =~ /iso/i) {
            return $self->ExportToIsoWkt;
        } else {
            return $self->AsText;
        }
    } elsif ($f =~ /binary/i) {
        return $self->ExportToWkb($p->{byteorder});
    } elsif ($f =~ /wkb/i) {
        if ($f =~ /iso/i) {
            $p->{byteorder} = Geo::GDAL::string2int($p->{byteorder}, \%Geo::OGR::Geometry::BYTE_ORDER_STRING2INT);
            return $self->ExportToIsoWkb($p->{byteorder});
        } elsif ($f =~ /ewkb/i) {
            return $self->AsHEXEWKB($p->{srid});
        } elsif ($f =~ /hex/i) {
            return $self->AsHEXWKB;
        } else {
            return $self->ExportToWkb($p->{byteorder});
        }
    } elsif ($f =~ /gml/i) {
        return $self->ExportToGML($p->{options});
    } elsif ($f =~ /kml/i) {
        return $self->ExportToKML($p->{altitudemode});
    } elsif ($f =~ /json/i) {
        return $self->AsJSON;
    } else {
        Geo::GDAL::error(1, $f, map {$_=>1} qw/Text WKT ISO_WKT ISO_WKB HEX_WKB HEX_EWKB Binary GML KML JSON/);
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

sub Extent {
    my $self = shift;
    return Geo::GDAL::Extent->new($self->GetEnvelope);
}

sub AddPoint {
    my $self = shift;
    my $t = $self->GetGeometryType;
    my $has_z = Geo::OGR::GT_HasZ($t);
    my $has_m = Geo::OGR::GT_HasM($t);
    if (!$has_z && !$has_m) {
        $self->AddPoint_2D(@_[0..1]);
    } elsif ($has_z && !$has_m) {
        $self->AddPoint_3D(@_[0..2]);
    } elsif (!$has_z && $has_m) {
        $self->AddPointM(@_[0..2]);
    } else {
        $self->AddPointZM(@_[0..3]);
    }
}

sub SetPoint {
    my $self = shift;
    my $t = $self->GetGeometryType;
    my $has_z = Geo::OGR::GT_HasZ($t);
    my $has_m = Geo::OGR::GT_HasM($t);
    if (!$has_z && !$has_m) {
        $self->SetPoint_2D(@_[0..2]);
    } elsif ($has_z && !$has_m) {
        $self->SetPoint_3D(@_[0..3]);
    } elsif (!$has_z && $has_m) {
        $self->SetPointM(@_[0..3]);
    } else {
        $self->SetPointZM(@_[0..4]);
    }
}

sub GetPoint {
    my($self, $i) = @_;
    $i //= 0;
    my $t = $self->GetGeometryType;
    my $has_z = Geo::OGR::GT_HasZ($t);
    my $has_m = Geo::OGR::GT_HasM($t);
    my $point;
    if (!$has_z && !$has_m) {
        $point = $self->GetPoint_2D($i);
    } elsif ($has_z && !$has_m) {
        $point = $self->GetPoint_3D($i);
    } elsif (!$has_z && $has_m) {
        $point = $self->GetPointZM($i);
        @$point = ($point->[0], $point->[1], $point->[3]);
    } else {
        $point = $self->GetPointZM($i);
    }
    return wantarray ? @$point : $point;
}

sub Point {
    my $self = shift;
    my $i;
    if (@_) {
        my $t = $self->GetGeometryType;
        my $i;
        if (Geo::OGR::GT_Flatten($t) == $Geo::OGR::wkbPoint) {
            my $has_z = Geo::OGR::GT_HasZ($t);
            my $has_m = Geo::OGR::GT_HasM($t);
            if (!$has_z && !$has_m) {
                shift if @_ > 2;
                $i = 0;
            } elsif ($has_z || $has_m) {
                shift if @_ > 3;
                $i = 0;
            } else {
                shift if @_ > 4;
                $i = 0;
            }
        }
        $i = shift unless defined $i;
        $self->SetPoint($i, @_);
    }
    return unless defined wantarray;
    my $point = $self->GetPoint;
    return wantarray ? @$point : $point;
}

sub Points {
    my $self = shift;
    my $t = $self->GetGeometryType;
    my $has_z = Geo::OGR::GT_HasZ($t);
    my $has_m = Geo::OGR::GT_HasM($t);
    my $postfix = '';
    $postfix .= 'Z' if Geo::OGR::GT_HasZ($t);
    $postfix .= 'M' if Geo::OGR::GT_HasM($t);
    $t = $TYPE_INT2STRING{Geo::OGR::GT_Flatten($t)};
    my $points = shift;
    if ($points) {
        Empty($self);
        if ($t eq 'Unknown' or $t eq 'None' or $t eq 'GeometryCollection') {
            Geo::GDAL::error("Can't set points of a geometry of type '$t'.");
        } elsif ($t eq 'Point') {
            # support both "Point" as a list of one point and one point
            if (ref($points->[0])) {
                $self->AddPoint(@{$points->[0]});
            } else {
                $self->AddPoint(@$points);
            }
        } elsif ($t eq 'LineString' or $t eq 'LinearRing' or $t eq 'CircularString') {
            for my $p (@$points) {
                $self->AddPoint(@$p);
            }
        } elsif ($t eq 'Polygon') {
            for my $r (@$points) {
                my $ring = Geo::OGR::Geometry->new('LinearRing');
                $ring->Set3D(1) if $has_z;
                $ring->SetMeasured(1) if $has_m;
                $ring->Points($r);
                $self->AddGeometryDirectly($ring);
            }
        } elsif ($t eq 'MultiPoint') {
            for my $p (@$points) {
                my $point = Geo::OGR::Geometry->new('Point'.$postfix);
                $point->Points($p);
                $self->AddGeometryDirectly($point);
            }
        } elsif ($t eq 'MultiLineString') {
            for my $l (@$points) {
                my $linestring = Geo::OGR::Geometry->new('Point'.$postfix);
                $linestring->Points($l);
                $self->AddGeometryDirectly($linestring);
            }
        } elsif ($t eq 'MultiPolygon') {
            for my $p (@$points) {
                my $polygon = Geo::OGR::Geometry->new('Point'.$postfix);
                $polygon->Points($p);
                $self->AddGeometryDirectly($polygon);
            }
        }
    }
    return unless defined wantarray;
    $self->_GetPoints();
}

sub _GetPoints {
    my($self) = @_;
    my @points;
    my $n = $self->GetGeometryCount;
    if ($n) {
        for my $i (0..$n-1) {
            push @points, $self->GetGeometryRef($i)->_GetPoints();
        }
    } else {
        $n = $self->GetPointCount;
        if ($n == 1) {
            push @points, $self->GetPoint;
        } else {
            for my $i (0..$n-1) {
                push @points, scalar $self->GetPoint($i);
            }
        }
    }
    return \@points;
}

sub ExportToWkb {
    my($self, $bo) = @_;
    $bo = Geo::GDAL::string2int($bo, \%BYTE_ORDER_STRING2INT);
    return _ExportToWkb($self, $bo);
}

sub ForceTo {
    my $self = shift;
    my $type = shift;
    $type = Geo::GDAL::string2int($type, \%TYPE_STRING2INT);
    eval {
        $self = Geo::OGR::ForceTo($self, $type, @_);
    };
    confess Geo::GDAL->last_error if $@;
    return $self;
}

sub ForceToLineString {
    my $self = shift;
    return Geo::OGR::ForceToLineString($self);
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
    my($type) = @_;
    if (defined $type) {
        return Geo::GDAL::string2int($type, \%Geo::OGR::Geometry::TYPE_STRING2INT, \%Geo::OGR::Geometry::TYPE_INT2STRING);
    } else {
        return @Geo::OGR::Geometry::GEOMETRY_TYPES;
    }
}

sub GeometryTypeModify {
    my($type, $modifier) = @_;
    Geo::GDAL::error(1, $type, \%Geo::OGR::Geometry::TYPE_STRING2INT) unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_Flatten($type)} if $modifier =~ /flat/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_SetZ($type)} if $modifier =~ /z/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetCollection($type)} if $modifier =~ /collection/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetCurve($type)} if $modifier =~ /curve/i;
    return $Geo::OGR::Geometry::TYPE_INT2STRING{GT_GetLinear($type)} if $modifier =~ /linear/i;
    Geo::GDAL::error(1, $modifier, {Flatten => 1, SetZ => 1, GetCollection => 1, GetCurve => 1, GetLinear => 1});
}

sub GeometryTypeTest {
    my($type, $test, $type2) = @_;
    Geo::GDAL::error(1, $type, \%Geo::OGR::Geometry::TYPE_STRING2INT) unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    $type = $Geo::OGR::Geometry::TYPE_STRING2INT{$type};
    if (defined $type2) {
        Geo::GDAL::error(1, $type2, \%Geo::OGR::Geometry::TYPE_STRING2INT) unless exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type2};
        $type2 = $Geo::OGR::Geometry::TYPE_STRING2INT{$type2};
    } else {
        Geo::GDAL::error("Usage: GeometryTypeTest(type1, 'is_subclass_of', type2).") if $test =~ /subclass/i;
    }
    return GT_HasZ($type) if $test =~ /z/i;
    return GT_IsSubClassOf($type, $type2) if $test =~ /subclass/i;
    return GT_IsCurve($type) if $test =~ /curve/i;
    return GT_IsSurface($type) if $test =~ /surface/i;
    return GT_IsNonLinear($type) if $test =~ /linear/i;
    Geo::GDAL::error(1, $test, {HasZ => 1, IsSubClassOf => 1, IsCurve => 1, IsSurface => 1, IsNonLinear => 1});
}

sub RELEASE_PARENTS {
}

*ByteOrders = *Geo::OGR::Geometry::ByteOrders;
*GeometryTypes = *Geo::OGR::Geometry::GeometryTypes;

%}
