%include cpl_exceptions.i
%import typemaps_perl.i

%rename (_TransformPoints) TransformPoints;
%rename (_GetUTMZone) GetUTMZone;

%perlcode %{

package Geo::OSR;

use vars qw /%PROJECTIONS %PARAMETERS %LINEAR_UNITS %ANGULAR_UNITS %DATUMS/;

for (keys %Geo::OSR::) {
    if (/^SRS_PT_(\w+)/) {
        $p = eval '$Geo::OSR::'.$_;
        $PROJECTIONS{$p} = 1;
    }
    elsif (/^SRS_PP_(\w+)/) {
        $p = eval '$Geo::OSR::'.$_;
        $PARAMETERS{$p} = 1;
    }
    elsif (/^SRS_UL_(\w+)/) {
        $p = eval '$Geo::OSR::'.$_;
        $LINEAR_UNITS{$p} = 1;
    }
    elsif (/^SRS_UA_(\w+)/) {
        $p = eval '$Geo::OSR::'.$_;
        $ANGULAR_UNITS{$p} = 1;
    }
    elsif (/^SRS_DN_(\w+)/) {
        $p = eval '$Geo::OSR::'.$_;
        $DATUMS{$p} = 1;
    }
}

sub Projections {
    return sort keys %PROJECTIONS;
}

sub Parameters {
    return sort keys %PARAMETERS;
}

sub LinearUnits {
    return sort keys %LINEAR_UNITS;
}

sub AngularUnits {
    return sort keys %ANGULAR_UNITS;
}

sub Datums {
    return sort keys %DATUMS;
}

sub RELEASE_PARENTS {
}


package Geo::OSR::SpatialReference;
use strict;
use Carp;

sub create {
    my $pkg = shift;
    my %param = @_;
    my $self = Geo::OSRc::new_SpatialReference();
    if ($param{WKT}) {
        ImportFromWkt($self, $param{WKT});
    } elsif ($param{Text}) {
        ImportFromWkt($self, $param{Text});
    } elsif ($param{Proj4}) {
        ImportFromProj4($self, $param{Proj4});
    } elsif ($param{ESRI}) {
        ImportFromESRI($self, @{$param{ESRI}});
    } elsif ($param{EPSG}) {
        ImportFromEPSG($self, $param{EPSG});
    } elsif ($param{EPSGA}) {
        ImportFromEPSGA($self, $param{EPSGA});
    } elsif ($param{PCI}) {
        ImportFromPCI($self, @{$param{PCI}});
    } elsif ($param{USGS}) {
        ImportFromUSGS($self, @{$param{USGS}});
    } elsif ($param{XML}) {
        ImportFromXML($self, $param{XML});
    } elsif ($param{GML}) {
        ImportFromGML($self, $param{GML});
    } elsif ($param{URL}) {
        ImportFromUrl($self, $param{URL});
    } elsif ($param{ERMapper}) {
        ImportFromERM($self, @{$param{ERMapper}} );
    } elsif ($param{ERM}) {
        ImportFromERM($self, @{$param{ERM}} );
    } elsif ($param{MICoordSys}) {
        ImportFromMICoordSys($self, $param{MICoordSys} );
    } elsif ($param{MapInfoCS}) {
        ImportFromMICoordSys($self, $param{MapInfoCS} );
    } else {
        croak "unrecognized import format '@_' for Geo::OSR::SpatialReference";
    }
    bless $self, $pkg if defined $self;
}

sub Export {
    my $self = shift;
    my $format;
    $format = pop if @_ == 1;
    my %params = @_;
    $format = $params{to} unless $format;
    $format = $params{format} unless $format;
    $format = $params{as} unless $format;
    if ($format eq 'WKT' or $format eq 'Text') {
        return ExportToWkt($self);
    } elsif ($format eq 'PrettyWKT') {
        my $simplify = exists $params{simplify} ? $params{simplify} : 0;
        return ExportToPrettyWkt($self, $simplify);
    } elsif ($format eq 'Proj4') {
        return ExportToProj4($self);
    } elsif ($format eq 'PCI') {            
        return ExportToPCI($self);
    } elsif ($format eq 'USGS') {
        return ExportToUSGS($self);
    } elsif ($format eq 'GML' or $format eq 'XML') {
        my $dialect = exists $params{dialect} ? $params{dialect} : '';
        return ExportToXML($self, $dialect);
    } elsif ($format eq 'MICoordSys' or $format eq 'MapInfoCS') {
        return ExportToMICoordSys();
    } else {
        croak "unrecognized export format '$format/@_' for Geo::OSR::SpatialReference.";
    }
}
*AsText = *ExportToWkt;
*As = *Export;

sub Set {
    my($self, %params) = @_;
    if (exists $params{Authority} and exists $params{TargetKey} and exists $params{Node} and exists $params{Code}) {
        SetAuthority($self, $params{TargetKey}, $params{Authority}, $params{Code});
    } elsif (exists $params{Node} and exists $params{Value}) {
        SetAttrValue($self, $params{Node}, $params{Value});
    } elsif (exists $params{AngularUnits} and exists $params{Value}) {
        SetAngularUnits($self, $params{AngularUnits}, $params{Value});
    } elsif (exists $params{LinearUnits} and exists $params{Node} and exists $params{Value}) {
        SetTargetLinearUnits($self, $params{Node}, $params{LinearUnits}, $params{Value});
    } elsif (exists $params{LinearUnits} and exists $params{Value}) {
        SetLinearUnitsAndUpdateParameters($self, $params{LinearUnits}, $params{Value});
    } elsif ($params{Parameter} and exists $params{Value}) {
        croak "Unknown projection parameter '$params{Parameter}'." unless exists $Geo::OSR::PARAMETERS{$params{Parameter}};
        $params{Normalized} ?
            SetNormProjParm($self, $params{Parameter}, $params{Value}) :
            SetProjParm($self, $params{Parameter}, $params{Value});
    } elsif ($params{Name}) {
        SetWellKnownGeogCS($self, $params{Name});
    } elsif ($params{GuessFrom}) {
        SetFromUserInput($self, $params{GuessFrom});
    } elsif ($params{LOCAL_CS}) {
        SetLocalCS($self, $params{LOCAL_CS});
    } elsif ($params{GeocentricCS}) {
        SetGeocCS($self, $params{GeocentricCS});
    } elsif ($params{VerticalCS} and $params{Datum}) {
        my $type = $params{VertDatumType} || 2005;
        SetVertCS($self, $params{VerticalCS}, $params{Datum}, $type);
    } elsif ($params{CoordinateSystem}) {
        my @parameters = ();
        @parameters = @{$params{Parameters}} if ref($params{Parameters});
        if ($params{CoordinateSystem} eq 'State Plane' and exists $params{Zone}) {
            my $NAD83 = exists $params{NAD83} ? $params{NAD83} : 1;
            my $name = exists $params{UnitName} ? $params{UnitName} : undef;
            my $c = exists $params{UnitConversionFactor} ? $params{UnitConversionFactor} : 0.0;
            SetStatePlane($self, $params{Zone}, $NAD83, $name, $c);
        } elsif ($params{CoordinateSystem} eq 'UTM' and exists $params{Zone} and exists $params{North}) {
            my $north = exists $params{North} ? $params{North} : 1;
            SetUTM($self, $params{Zone}, $north);
        } elsif ($params{CoordinateSystem} eq 'WGS') {
            SetTOWGS84($self, @parameters);
        } elsif ($params{CoordinateSystem} and $params{Datum} and $params{Spheroid}) {
            SetGeogCS($self, $params{CoordinateSystem}, $params{Datum}, $params{Spheroid}, @parameters);
        } elsif ($params{CoordinateSystem} and $params{HorizontalCS} and $params{VerticalCS}) {
            SetCompoundCS($self, $params{CoordinateSystem}, $params{HorizontalCS}, $params{VerticalCS});
        } else {
            SetProjCS($self, $params{CoordinateSystem});
        }
    } elsif ($params{Projection}) {
        croak "Unknown projection '$params{Projection}'." unless exists $Geo::OSR::PROJECTIONS{$params{Projection}};
        my @parameters = ();
        @parameters = @{$params{Parameters}} if ref($params{Parameters});
        if ($params{Projection} eq 'Albers_Conic_Equal_Area') {
            SetACEA($self, @parameters);
        } elsif ($params{Projection} eq 'Azimuthal_Equidistant') {
            SetAE($self, @parameters);
        } elsif ($params{Projection} eq 'Bonne') {
            SetBonne($self, @parameters);
        } elsif ($params{Projection} eq 'Cylindrical_Equal_Area') {
            SetCEA($self, @parameters);
        } elsif ($params{Projection} eq 'Cassini_Soldner') {
            SetCS($self, @parameters);
        } elsif ($params{Projection} eq 'Equidistant_Conic') {
            SetEC($self, @parameters);
            # Eckert_I, Eckert_II, Eckert_III, Eckert_V ?
        } elsif ($params{Projection} eq 'Eckert_IV') {
            SetEckertIV($self, @parameters);
        } elsif ($params{Projection} eq 'Eckert_VI') {
            SetEckertVI($self, @parameters);
        } elsif ($params{Projection} eq 'Equirectangular') {
            @parameters == 4 ?
                SetEquirectangular($self, @parameters) :
                SetEquirectangular2($self, @parameters);
        } elsif ($params{Projection} eq 'Gauss_Schreiber_Transverse_Mercator') {
            SetGaussSchreiberTMercator($self, @parameters);
        } elsif ($params{Projection} eq 'Gall_Stereographic') {
            SetGS($self, @parameters);
        } elsif ($params{Projection} eq 'Goode_Homolosine') {
            SetGH($self, @parameters);
        } elsif ($params{Projection} eq 'Interrupted_Goode_Homolosine') {
            SetIGH($self);
        } elsif ($params{Projection} eq 'Geostationary_Satellite') {
            SetGEOS($self, @parameters);
        } elsif ($params{Projection} eq 'Gnomonic') {
            SetGnomonic($self, @parameters);
        } elsif ($params{Projection} eq 'Hotine_Oblique_Mercator') {
            # Hotine_Oblique_Mercator_Azimuth_Center ?
            SetHOM($self, @parameters);
        } elsif ($params{Projection} eq 'Hotine_Oblique_Mercator_Two_Point_Natural_Origin') {
            SetHOM2PNO($self, @parameters);
        } elsif ($params{Projection} eq 'Krovak') {
            SetKrovak($self, @parameters);
        } elsif ($params{Projection} eq 'Lambert_Azimuthal_Equal_Area') {
            SetLAEA($self, @parameters);
        } elsif ($params{Projection} eq 'Lambert_Conformal_Conic_2SP') {
            SetLCC($self, @parameters);
        } elsif ($params{Projection} eq 'Lambert_Conformal_Conic_1SP') {
            SetLCC1SP($self, @parameters);
        } elsif ($params{Projection} eq 'Lambert_Conformal_Conic_2SP_Belgium') {
            SetLCCB($self, @parameters);
        } elsif ($params{Projection} eq 'miller_cylindrical') {
            SetMC($self, @parameters);
        } elsif ($params{Projection} =~ /^Mercator/) {
            # Mercator_1SP, Mercator_2SP, Mercator_Auxiliary_Sphere ?
            # variant is in Variant (or Name)
            SetMercator($self, @parameters);
        } elsif ($params{Projection} eq 'Mollweide') {
            SetMollweide($self, @parameters);
        } elsif ($params{Projection} eq 'New_Zealand_Map_Grid') {
            SetNZMG($self, @parameters);
        } elsif ($params{Projection} eq 'Oblique_Stereographic') {
            SetOS($self, @parameters);
        } elsif ($params{Projection} eq 'Orthographic') {
            SetOrthographic($self, @parameters);
        } elsif ($params{Projection} eq 'Polyconic') {
            SetPolyconic($self, @parameters);
        } elsif ($params{Projection} eq 'Polar_Stereographic') {
            SetPS($self, @parameters);
        } elsif ($params{Projection} eq 'Robinson') {
            SetRobinson($self, @parameters);
        } elsif ($params{Projection} eq 'Sinusoidal') {
            SetSinusoidal($self, @parameters);
        } elsif ($params{Projection} eq 'Stereographic') {
            SetStereographic($self, @parameters);
        } elsif ($params{Projection} eq 'Swiss_Oblique_Cylindrical') {
            SetSOC($self, @parameters);
        } elsif ($params{Projection} eq 'Transverse_Mercator_South_Orientated') {
            SetTMSO($self, @parameters);
        } elsif ($params{Projection} =~ /^Transverse_Mercator/) {
            my($variant) = $params{Projection} =~ /^Transverse_Mercator_(\w+)/;
            $variant = $params{Variant} unless $variant;
            $variant = $params{Name} unless $variant;
            $variant ?
                SetTMVariant($self, $variant, @parameters) :
                SetTM($self, @parameters);
        } elsif ($params{Projection} eq 'Tunisia_Mining_Grid') {
            SetTMG($self, @parameters);
        } elsif ($params{Projection} eq 'VanDerGrinten') {
            SetVDG($self, @parameters);
        } else {
            # Aitoff, Craster_Parabolic, International_Map_of_the_World_Polyconic, Laborde_Oblique_Mercator
            # Loximuthal, Miller_Cylindrical, Quadrilateralized_Spherical_Cube, Quartic_Authalic, Two_Point_Equidistant
            # Wagner_I, Wagner_II, Wagner_III, Wagner_IV, Wagner_V, Wagner_VI, Wagner_VII
            # Winkel_I, Winkel_II, Winkel_Tripel
            # ?
            SetProjection($self, $params{Projection});
        }
    } else {
        croak "Not enough information for a spatial reference object.";
    }
}

sub GetUTMZone {
    my $self = shift;
    my $zone = _GetUTMZone($self);
    if (wantarray) {            
        my $north = 1;
        if ($zone < 0) {
            $zone *= -1;
            $north = 0;
        }
        return ($zone, $north);
    } else {
        return $zone;
    }
}


package Geo::OSR::CoordinateTransformation;
use strict;

sub TransformPoints {
    my($self, $points) = @_;
    _TransformPoints($self, $points), return unless ref($points->[0]->[0]);
    for my $p (@$points) {
        TransformPoints($self, $p);
    }
}
%}
