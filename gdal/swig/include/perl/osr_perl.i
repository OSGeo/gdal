%include cpl_exceptions.i
%import typemaps_perl.i

%rename (_TransformPoints) TransformPoints;
%rename (_GetUTMZone) GetUTMZone;

%perlcode %{
    sub RELEASE_PARENTS {
    }
    package Geo::OSR::SpatialReference;
    use strict;
    use Carp;
    use vars qw /@PROJECTIONS %PROJECTIONS @PARAMETERS %PARAMETERS/;
    @PROJECTIONS = qw/
ALBERS_CONIC_EQUAL_AREA
AZIMUTHAL_EQUIDISTANT
CASSINI_SOLDNER
CYLINDRICAL_EQUAL_AREA
BONNE
ECKERT_I
ECKERT_II
ECKERT_III
ECKERT_IV
ECKERT_V
ECKERT_VI
EQUIDISTANT_CONIC
EQUIRECTANGULAR
GALL_STEREOGRAPHIC
GAUSSSCHREIBERTMERCATOR
GEOSTATIONARY_SATELLITE
GOODE_HOMOLOSINE
IGH
GNOMONIC
HOTINE_OBLIQUE_MERCATOR
HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN
LABORDE_OBLIQUE_MERCATOR
LAMBERT_CONFORMAL_CONIC_1SP
LAMBERT_CONFORMAL_CONIC_2SP
LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM
LAMBERT_AZIMUTHAL_EQUAL_AREA
MERCATOR_1SP
MERCATOR_2SP
MILLER_CYLINDRICAL
MOLLWEIDE
NEW_ZEALAND_MAP_GRID
OBLIQUE_STEREOGRAPHIC
ORTHOGRAPHIC
POLAR_STEREOGRAPHIC
POLYCONIC
ROBINSON
SINUSOIDAL
STEREOGRAPHIC
SWISS_OBLIQUE_CYLINDRICAL
TRANSVERSE_MERCATOR
TRANSVERSE_MERCATOR_SOUTH_ORIENTED
TRANSVERSE_MERCATOR_MI_21
TRANSVERSE_MERCATOR_MI_22
TRANSVERSE_MERCATOR_MI_23
TRANSVERSE_MERCATOR_MI_24
TRANSVERSE_MERCATOR_MI_25
TUNISIA_MINING_GRID
TWO_POINT_EQUIDISTANT
VANDERGRINTEN
KROVAK
IMW_POLYCONIC
WAGNER_I
WAGNER_II
WAGNER_III
WAGNER_IV
WAGNER_V
WAGNER_VI
WAGNER_VII
/;
    for my $s (@PROJECTIONS) {
	my $p = eval "\$Geo::OGR::SRS_PT_$s";
	$PROJECTIONS{$s} = $p;
    }
    @PARAMETERS = qw/
CENTRAL_MERIDIAN
SCALE_FACTOR
STANDARD_PARALLEL_1
STANDARD_PARALLEL_2
PSEUDO_STD_PARALLEL_1
LONGITUDE_OF_CENTER
LATITUDE_OF_CENTER
LONGITUDE_OF_ORIGIN
LATITUDE_OF_ORIGIN
FALSE_EASTING
FALSE_NORTHING
AZIMUTH
LONGITUDE_OF_POINT_1
LATITUDE_OF_POINT_1
LONGITUDE_OF_POINT_2
LATITUDE_OF_POINT_2
LONGITUDE_OF_POINT_3
LATITUDE_OF_POINT_3
RECTIFIED_GRID_ANGLE
LANDSAT_NUMBER
PATH_NUMBER
PERSPECTIVE_POINT_HEIGHT
SATELLITE_HEIGHT
FIPSZONE
ZONE
LATITUDE_OF_1ST_POINT
LONGITUDE_OF_1ST_POINT
LATITUDE_OF_2ND_POINT
LONGITUDE_OF_2ND_POINT
/;
    for my $s (@PARAMETERS) {
	my $p = eval "\$Geo::OGR::SRS_PP_$s";
	$PARAMETERS{$s} = $p;
    }
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
	if (exists $params{Authority} and exists $params{Node} and exists $params{Code}) {
	    SetAuthority($self, $params{TargetKey}, $params{Authority}, $params{Code});
	} elsif (exists $params{Node} and exists $params{Value}) {
	    SetAttrValue($self, $params{Node}, $params{Value});
	} elsif (exists $params{AngularUnits} and exists $params{Value}) {
	    SetAngularUnits($self, $params{AngularUnits}, $params{Value});
	} elsif (exists $params{LinearUnits} and exists $params{Node} and exists $params{Value}) {
	    SetTargetLinearUnits($self, $params{Node}, $params{LinearUnits}, $params{Value});
	} elsif (exists $params{LinearUnits} and exists $params{Value}) {
	    SetLinearUnitsAndUpdateParameters($self, $params{LinearUnits}, $params{Value});
	} elsif ($params{CoordinateSystem} eq 'UTM' and exists $params{Zone} and exists $params{North}) {
	    my $north = exists $params{North} ? $params{North} : 1;
	    SetUTM($self, $params{Zone}, $north);
	} elsif ($params{CoordinateSystem} eq 'State Plane' and exists $params{Zone}) {
	    my $NAD83 = exists $params{NAD83} ? $params{NAD83} : 1;
	    my $name = exists $params{UnitName} ? $params{UnitName} : undef;
	    my $c = exists $params{UnitConversionFactor} ? $params{UnitConversionFactor} : 0.0;
	    SetStatePlane($self, $params{Zone}, $NAD83, $name, $c);
	} elsif ($params{Parameter} and exists $params{Value}) {
	    croak "unknown parameter '$params{Parameter}' in Geo::OSR::SpatialReference->Set" unless exists $PARAMETERS{$params{Parameter}};
	    $params{Normalized} ?
		SetNormProjParm($self, $params{Parameter}, $params{Value}) :
		SetProjParm($self, $params{Parameter}, $params{Value});
	} elsif ($params{Projection}) {
	    croak "unknown projection '$params{Projection}' in Geo::OSR::SpatialReference->Set" unless exists $PROJECTIONS{$params{Projection}};
	    if (not $params{Parameters}) {
		SetProjection($self, $PROJECTIONS{$params{Projection}});
	    } elsif ($params{Projection} eq 'ALBERS_CONIC_EQUAL_AREA' and $params{Parameters}) {
		SetACEA($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'AZIMUTHAL_EQUIDISTANT' and $params{Parameters}) {
		SetAE($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'BONNE' and $params{Parameters}) {
		SetBonne($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'CYLINDRICAL_EQUAL_AREA' and $params{Parameters}) {
		SetCEA($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'CASSINI_SOLDNER' and $params{Parameters}) {
		SetCS($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'EQUIDISTANT_CONIC' and $params{Parameters}) {
		SetEC($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'ECKERT_IV' and $params{Parameters}) {
		SetEckertIV($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'ECKERT_VI' and $params{Parameters}) {
		SetEckertVI($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'EQUIRECTANGULAR' and $params{Parameters}) {
		@{$params{Parameters}} == 4 ?
		    SetEquirectangular($self, @{$params{Parameters}}) :
		    SetEquirectangular2($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'GAUSSSCHREIBERTMERCATOR' and $params{Parameters}) {
		SetGaussSchreiberTMercator($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'GALL_STEREOGRAPHIC' and $params{Parameters}) {
		SetGS($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'GOODE_HOMOLOSINE' and $params{Parameters}) {
		SetGH($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'IGH') {
		SetIGH($self);
	    } elsif ($params{Projection} eq 'GEOSTATIONARY_SATELLITE' and $params{Parameters}) {
		SetGEOS($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'GNOMONIC' and $params{Parameters}) {
		SetGnomonic($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'HOTINE_OBLIQUE_MERCATOR' and $params{Parameters}) {
		SetHOM($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN' and $params{Parameters}) {
		SetHOM2PNO($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'KROVAK' and $params{Parameters}) {
		SetKrovak($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'LAMBERT_AZIMUTHAL_EQUAL_AREA' and $params{Parameters}) {
		SetLAEA($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'LAMBERT_CONFORMAL_CONIC_2SP' and $params{Parameters}) {
		SetLCC($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'LAMBERT_CONFORMAL_CONIC_1SP' and $params{Parameters}) {
		SetLCC1SP($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM' and $params{Parameters}) {
		SetLCCB($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'MILLER_CYLINDRICAL' and $params{Parameters}) {
		SetMC($self, @{$params{Parameters}});
	    } elsif ($params{Projection} =~ /^MERCATOR/ and $params{Parameters}) {
		SetMercator($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'MOLLWEIDE' and $params{Parameters}) {
		SetMollweide($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'NEW_ZEALAND_MAP_GRID' and $params{Parameters}) {
		SetNZMG($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'OBLIQUE_STEREOGRAPHIC' and $params{Parameters}) {
		SetOS($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'ORTHOGRAPHIC' and $params{Parameters}) {
		SetOrthographic($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'POLYCONIC' and $params{Parameters}) {
		SetPolyconic($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'POLAR_STEREOGRAPHIC' and $params{Parameters}) {
		SetPS($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'ROBINSON' and $params{Parameters}) {
		SetRobinson($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'SINUSOIDAL' and $params{Parameters}) {
		SetSinusoidal($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'STEREOGRAPHIC' and $params{Parameters}) {
		SetStereographic($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'SWISS_OBLIQUE_CYLINDRICAL' and $params{Parameters}) {
		SetSOC($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'TRANSVERSE_MERCATOR_SOUTH_ORIENTED' and $params{Parameters}) {
		SetTMSO($self, @{$params{Parameters}});
	    } elsif ($params{Projection} =~ /^TRANSVERSE_MERCATOR/ and $params{Parameters}) {
		my($variant) = $params{Projection} =~ /^TRANSVERSE_MERCATOR_(\w+)/;
		$variant = $params{Name} unless $variant;
		$variant ?
		    SetTMVariant($self, $variant, @{$params{Parameters}}) :
		    SetTM($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'TUNISIA_MINING_GRID' and $params{Parameters}) {
		SetTMG($self, @{$params{Parameters}});
	    } elsif ($params{Projection} eq 'VANDERGRINTEN' and $params{Parameters}) {
		SetVDG($self, @{$params{Parameters}});
	    } elsif ($params{Name}) {
		SetWellKnownGeogCS($self, $params{Name});
	    } elsif ($params{GuessFrom}) {
		SetFromUserInput($self, $params{GuessFrom});
	    } elsif ($params{CoordinateSystem} eq 'WGS' and $params{Parameters}) {
		SetTOWGS84($self, @{$params{Parameters}});
	    } elsif ($params{LOCAL_CS}) {
		SetLocalCS($self, $params{LOCAL_CS});
	    } elsif ($params{CoordinateSystem} and $params{Datum} and $params{Spheroid} and $params{Parameters}) {
		SetGeogCS($self, $params{CoordinateSystem}, $params{Datum}, $params{Spheroid}, @{$params{Parameters}});
	    } elsif ($params{CoordinateSystem}) {
		SetProjCS($self, $params{CoordinateSystem});
	    } elsif ($params{GeocentricCS}) {
		SetGeocCS($self, $params{GeocentricCS});
	    } elsif ($params{VerticalCS} and $params{Datum}) {
		my $type = $params{VertDatumType} || 2005;
		SetVertCS($self, $params{VerticalCS}, $params{Datum}, $type);
	    } elsif ($params{CoordinateSystem} and $params{HorizontalCS} and $params{VerticalCS}) {
		SetCompoundCS($self, $params{CoordinateSystem}, $params{HorizontalCS}, $params{VerticalCS});
	    } else {
		croak "not enough information to set anything in a spatial reference object in Geo::OSR::SpatialReference->Set";
	    }
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
