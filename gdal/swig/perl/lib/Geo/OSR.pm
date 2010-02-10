# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package Geo::OSR;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
package Geo::OSRc;
bootstrap Geo::OSR;
package Geo::OSR;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package Geo::OSR;

sub TIEHASH {
    my ($classname,$obj) = @_;
    return bless $obj, $classname;
}

sub CLEAR { }

sub FIRSTKEY { }

sub NEXTKEY { }

sub FETCH {
    my ($self,$field) = @_;
    my $member_func = "swig_${field}_get";
    $self->$member_func();
}

sub STORE {
    my ($self,$field,$newval) = @_;
    my $member_func = "swig_${field}_set";
    $self->$member_func($newval);
}

sub this {
    my $ptr = shift;
    return tied(%$ptr);
}


# ------- FUNCTION WRAPPERS --------

package Geo::OSR;

*UseExceptions = *Geo::OSRc::UseExceptions;
*DontUseExceptions = *Geo::OSRc::DontUseExceptions;
*GetWellKnownGeogCSAsWKT = *Geo::OSRc::GetWellKnownGeogCSAsWKT;
*GetUserInputAsWKT = *Geo::OSRc::GetUserInputAsWKT;
*GetProjectionMethods = *Geo::OSRc::GetProjectionMethods;
*GetProjectionMethodParameterList = *Geo::OSRc::GetProjectionMethodParameterList;
*GetProjectionMethodParamInfo = *Geo::OSRc::GetProjectionMethodParamInfo;

############# Class : Geo::OSR::SpatialReference ##############

package Geo::OSR::SpatialReference;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OSR );
%OWNER = ();
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = Geo::OSRc::new_SpatialReference(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OSRc::delete_SpatialReference($self);
        delete $OWNER{$self};
    }
}

*__str__ = *Geo::OSRc::SpatialReference___str__;
*IsSame = *Geo::OSRc::SpatialReference_IsSame;
*IsSameGeogCS = *Geo::OSRc::SpatialReference_IsSameGeogCS;
*IsGeographic = *Geo::OSRc::SpatialReference_IsGeographic;
*IsProjected = *Geo::OSRc::SpatialReference_IsProjected;
*IsLocal = *Geo::OSRc::SpatialReference_IsLocal;
*EPSGTreatsAsLatLong = *Geo::OSRc::SpatialReference_EPSGTreatsAsLatLong;
*SetAuthority = *Geo::OSRc::SpatialReference_SetAuthority;
*GetAttrValue = *Geo::OSRc::SpatialReference_GetAttrValue;
*SetAttrValue = *Geo::OSRc::SpatialReference_SetAttrValue;
*SetAngularUnits = *Geo::OSRc::SpatialReference_SetAngularUnits;
*GetAngularUnits = *Geo::OSRc::SpatialReference_GetAngularUnits;
*SetLinearUnits = *Geo::OSRc::SpatialReference_SetLinearUnits;
*SetLinearUnitsAndUpdateParameters = *Geo::OSRc::SpatialReference_SetLinearUnitsAndUpdateParameters;
*GetLinearUnits = *Geo::OSRc::SpatialReference_GetLinearUnits;
*GetLinearUnitsName = *Geo::OSRc::SpatialReference_GetLinearUnitsName;
*GetAuthorityCode = *Geo::OSRc::SpatialReference_GetAuthorityCode;
*GetAuthorityName = *Geo::OSRc::SpatialReference_GetAuthorityName;
*SetUTM = *Geo::OSRc::SpatialReference_SetUTM;
*SetStatePlane = *Geo::OSRc::SpatialReference_SetStatePlane;
*AutoIdentifyEPSG = *Geo::OSRc::SpatialReference_AutoIdentifyEPSG;
*SetProjection = *Geo::OSRc::SpatialReference_SetProjection;
*SetProjParm = *Geo::OSRc::SpatialReference_SetProjParm;
*GetProjParm = *Geo::OSRc::SpatialReference_GetProjParm;
*SetNormProjParm = *Geo::OSRc::SpatialReference_SetNormProjParm;
*GetNormProjParm = *Geo::OSRc::SpatialReference_GetNormProjParm;
*SetACEA = *Geo::OSRc::SpatialReference_SetACEA;
*SetAE = *Geo::OSRc::SpatialReference_SetAE;
*SetBonne = *Geo::OSRc::SpatialReference_SetBonne;
*SetCEA = *Geo::OSRc::SpatialReference_SetCEA;
*SetCS = *Geo::OSRc::SpatialReference_SetCS;
*SetEC = *Geo::OSRc::SpatialReference_SetEC;
*SetEckertIV = *Geo::OSRc::SpatialReference_SetEckertIV;
*SetEckertVI = *Geo::OSRc::SpatialReference_SetEckertVI;
*SetEquirectangular = *Geo::OSRc::SpatialReference_SetEquirectangular;
*SetEquirectangular2 = *Geo::OSRc::SpatialReference_SetEquirectangular2;
*SetGaussSchreiberTMercator = *Geo::OSRc::SpatialReference_SetGaussSchreiberTMercator;
*SetGS = *Geo::OSRc::SpatialReference_SetGS;
*SetGH = *Geo::OSRc::SpatialReference_SetGH;
*SetGEOS = *Geo::OSRc::SpatialReference_SetGEOS;
*SetGnomonic = *Geo::OSRc::SpatialReference_SetGnomonic;
*SetHOM = *Geo::OSRc::SpatialReference_SetHOM;
*SetHOM2PNO = *Geo::OSRc::SpatialReference_SetHOM2PNO;
*SetKrovak = *Geo::OSRc::SpatialReference_SetKrovak;
*SetLAEA = *Geo::OSRc::SpatialReference_SetLAEA;
*SetLCC = *Geo::OSRc::SpatialReference_SetLCC;
*SetLCC1SP = *Geo::OSRc::SpatialReference_SetLCC1SP;
*SetLCCB = *Geo::OSRc::SpatialReference_SetLCCB;
*SetMC = *Geo::OSRc::SpatialReference_SetMC;
*SetMercator = *Geo::OSRc::SpatialReference_SetMercator;
*SetMollweide = *Geo::OSRc::SpatialReference_SetMollweide;
*SetNZMG = *Geo::OSRc::SpatialReference_SetNZMG;
*SetOS = *Geo::OSRc::SpatialReference_SetOS;
*SetOrthographic = *Geo::OSRc::SpatialReference_SetOrthographic;
*SetPolyconic = *Geo::OSRc::SpatialReference_SetPolyconic;
*SetPS = *Geo::OSRc::SpatialReference_SetPS;
*SetRobinson = *Geo::OSRc::SpatialReference_SetRobinson;
*SetSinusoidal = *Geo::OSRc::SpatialReference_SetSinusoidal;
*SetStereographic = *Geo::OSRc::SpatialReference_SetStereographic;
*SetSOC = *Geo::OSRc::SpatialReference_SetSOC;
*SetTM = *Geo::OSRc::SpatialReference_SetTM;
*SetTMVariant = *Geo::OSRc::SpatialReference_SetTMVariant;
*SetTMG = *Geo::OSRc::SpatialReference_SetTMG;
*SetTMSO = *Geo::OSRc::SpatialReference_SetTMSO;
*SetVDG = *Geo::OSRc::SpatialReference_SetVDG;
*SetWellKnownGeogCS = *Geo::OSRc::SpatialReference_SetWellKnownGeogCS;
*SetFromUserInput = *Geo::OSRc::SpatialReference_SetFromUserInput;
*CopyGeogCSFrom = *Geo::OSRc::SpatialReference_CopyGeogCSFrom;
*SetTOWGS84 = *Geo::OSRc::SpatialReference_SetTOWGS84;
*GetTOWGS84 = *Geo::OSRc::SpatialReference_GetTOWGS84;
*SetLocalCS = *Geo::OSRc::SpatialReference_SetLocalCS;
*SetGeogCS = *Geo::OSRc::SpatialReference_SetGeogCS;
*SetProjCS = *Geo::OSRc::SpatialReference_SetProjCS;
*ImportFromWkt = *Geo::OSRc::SpatialReference_ImportFromWkt;
*ImportFromProj4 = *Geo::OSRc::SpatialReference_ImportFromProj4;
*ImportFromUrl = *Geo::OSRc::SpatialReference_ImportFromUrl;
*ImportFromESRI = *Geo::OSRc::SpatialReference_ImportFromESRI;
*ImportFromEPSG = *Geo::OSRc::SpatialReference_ImportFromEPSG;
*ImportFromEPSGA = *Geo::OSRc::SpatialReference_ImportFromEPSGA;
*ImportFromPCI = *Geo::OSRc::SpatialReference_ImportFromPCI;
*ImportFromUSGS = *Geo::OSRc::SpatialReference_ImportFromUSGS;
*ImportFromXML = *Geo::OSRc::SpatialReference_ImportFromXML;
*ImportFromMICoordSys = *Geo::OSRc::SpatialReference_ImportFromMICoordSys;
*ExportToWkt = *Geo::OSRc::SpatialReference_ExportToWkt;
*ExportToPrettyWkt = *Geo::OSRc::SpatialReference_ExportToPrettyWkt;
*ExportToProj4 = *Geo::OSRc::SpatialReference_ExportToProj4;
*ExportToPCI = *Geo::OSRc::SpatialReference_ExportToPCI;
*ExportToUSGS = *Geo::OSRc::SpatialReference_ExportToUSGS;
*ExportToXML = *Geo::OSRc::SpatialReference_ExportToXML;
*ExportToMICoordSys = *Geo::OSRc::SpatialReference_ExportToMICoordSys;
*CloneGeogCS = *Geo::OSRc::SpatialReference_CloneGeogCS;
*Clone = *Geo::OSRc::SpatialReference_Clone;
*Validate = *Geo::OSRc::SpatialReference_Validate;
*StripCTParms = *Geo::OSRc::SpatialReference_StripCTParms;
*FixupOrdering = *Geo::OSRc::SpatialReference_FixupOrdering;
*Fixup = *Geo::OSRc::SpatialReference_Fixup;
*MorphToESRI = *Geo::OSRc::SpatialReference_MorphToESRI;
*MorphFromESRI = *Geo::OSRc::SpatialReference_MorphFromESRI;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::OSR::CoordinateTransformation ##############

package Geo::OSR::CoordinateTransformation;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OSR );
%OWNER = ();
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = Geo::OSRc::new_CoordinateTransformation(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OSRc::delete_CoordinateTransformation($self);
        delete $OWNER{$self};
    }
}

*TransformPoint = *Geo::OSRc::CoordinateTransformation_TransformPoint;
*TransformPoints = *Geo::OSRc::CoordinateTransformation_TransformPoints;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


# ------- VARIABLE STUBS --------

package Geo::OSR;

*SRS_WKT_WGS84 = *Geo::OSRc::SRS_WKT_WGS84;
*SRS_PT_ALBERS_CONIC_EQUAL_AREA = *Geo::OSRc::SRS_PT_ALBERS_CONIC_EQUAL_AREA;
*SRS_PT_AZIMUTHAL_EQUIDISTANT = *Geo::OSRc::SRS_PT_AZIMUTHAL_EQUIDISTANT;
*SRS_PT_CASSINI_SOLDNER = *Geo::OSRc::SRS_PT_CASSINI_SOLDNER;
*SRS_PT_CYLINDRICAL_EQUAL_AREA = *Geo::OSRc::SRS_PT_CYLINDRICAL_EQUAL_AREA;
*SRS_PT_BONNE = *Geo::OSRc::SRS_PT_BONNE;
*SRS_PT_ECKERT_I = *Geo::OSRc::SRS_PT_ECKERT_I;
*SRS_PT_ECKERT_II = *Geo::OSRc::SRS_PT_ECKERT_II;
*SRS_PT_ECKERT_III = *Geo::OSRc::SRS_PT_ECKERT_III;
*SRS_PT_ECKERT_IV = *Geo::OSRc::SRS_PT_ECKERT_IV;
*SRS_PT_ECKERT_V = *Geo::OSRc::SRS_PT_ECKERT_V;
*SRS_PT_ECKERT_VI = *Geo::OSRc::SRS_PT_ECKERT_VI;
*SRS_PT_EQUIDISTANT_CONIC = *Geo::OSRc::SRS_PT_EQUIDISTANT_CONIC;
*SRS_PT_EQUIRECTANGULAR = *Geo::OSRc::SRS_PT_EQUIRECTANGULAR;
*SRS_PT_GALL_STEREOGRAPHIC = *Geo::OSRc::SRS_PT_GALL_STEREOGRAPHIC;
*SRS_PT_GAUSSSCHREIBERTMERCATOR = *Geo::OSRc::SRS_PT_GAUSSSCHREIBERTMERCATOR;
*SRS_PT_GEOSTATIONARY_SATELLITE = *Geo::OSRc::SRS_PT_GEOSTATIONARY_SATELLITE;
*SRS_PT_GOODE_HOMOLOSINE = *Geo::OSRc::SRS_PT_GOODE_HOMOLOSINE;
*SRS_PT_GNOMONIC = *Geo::OSRc::SRS_PT_GNOMONIC;
*SRS_PT_HOTINE_OBLIQUE_MERCATOR = *Geo::OSRc::SRS_PT_HOTINE_OBLIQUE_MERCATOR;
*SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = *Geo::OSRc::SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN;
*SRS_PT_LABORDE_OBLIQUE_MERCATOR = *Geo::OSRc::SRS_PT_LABORDE_OBLIQUE_MERCATOR;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = *Geo::OSRc::SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = *Geo::OSRc::SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = *Geo::OSRc::SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM;
*SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = *Geo::OSRc::SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA;
*SRS_PT_MERCATOR_1SP = *Geo::OSRc::SRS_PT_MERCATOR_1SP;
*SRS_PT_MERCATOR_2SP = *Geo::OSRc::SRS_PT_MERCATOR_2SP;
*SRS_PT_MILLER_CYLINDRICAL = *Geo::OSRc::SRS_PT_MILLER_CYLINDRICAL;
*SRS_PT_MOLLWEIDE = *Geo::OSRc::SRS_PT_MOLLWEIDE;
*SRS_PT_NEW_ZEALAND_MAP_GRID = *Geo::OSRc::SRS_PT_NEW_ZEALAND_MAP_GRID;
*SRS_PT_OBLIQUE_STEREOGRAPHIC = *Geo::OSRc::SRS_PT_OBLIQUE_STEREOGRAPHIC;
*SRS_PT_ORTHOGRAPHIC = *Geo::OSRc::SRS_PT_ORTHOGRAPHIC;
*SRS_PT_POLAR_STEREOGRAPHIC = *Geo::OSRc::SRS_PT_POLAR_STEREOGRAPHIC;
*SRS_PT_POLYCONIC = *Geo::OSRc::SRS_PT_POLYCONIC;
*SRS_PT_ROBINSON = *Geo::OSRc::SRS_PT_ROBINSON;
*SRS_PT_SINUSOIDAL = *Geo::OSRc::SRS_PT_SINUSOIDAL;
*SRS_PT_STEREOGRAPHIC = *Geo::OSRc::SRS_PT_STEREOGRAPHIC;
*SRS_PT_SWISS_OBLIQUE_CYLINDRICAL = *Geo::OSRc::SRS_PT_SWISS_OBLIQUE_CYLINDRICAL;
*SRS_PT_TRANSVERSE_MERCATOR = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR;
*SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED;
*SRS_PT_TRANSVERSE_MERCATOR_MI_21 = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_MI_21;
*SRS_PT_TRANSVERSE_MERCATOR_MI_22 = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_MI_22;
*SRS_PT_TRANSVERSE_MERCATOR_MI_23 = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_MI_23;
*SRS_PT_TRANSVERSE_MERCATOR_MI_24 = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_MI_24;
*SRS_PT_TRANSVERSE_MERCATOR_MI_25 = *Geo::OSRc::SRS_PT_TRANSVERSE_MERCATOR_MI_25;
*SRS_PT_TUNISIA_MINING_GRID = *Geo::OSRc::SRS_PT_TUNISIA_MINING_GRID;
*SRS_PT_TWO_POINT_EQUIDISTANT = *Geo::OSRc::SRS_PT_TWO_POINT_EQUIDISTANT;
*SRS_PT_VANDERGRINTEN = *Geo::OSRc::SRS_PT_VANDERGRINTEN;
*SRS_PT_KROVAK = *Geo::OSRc::SRS_PT_KROVAK;
*SRS_PT_IMW_POLYCONIC = *Geo::OSRc::SRS_PT_IMW_POLYCONIC;
*SRS_PT_WAGNER_I = *Geo::OSRc::SRS_PT_WAGNER_I;
*SRS_PT_WAGNER_II = *Geo::OSRc::SRS_PT_WAGNER_II;
*SRS_PT_WAGNER_III = *Geo::OSRc::SRS_PT_WAGNER_III;
*SRS_PT_WAGNER_IV = *Geo::OSRc::SRS_PT_WAGNER_IV;
*SRS_PT_WAGNER_V = *Geo::OSRc::SRS_PT_WAGNER_V;
*SRS_PT_WAGNER_VI = *Geo::OSRc::SRS_PT_WAGNER_VI;
*SRS_PT_WAGNER_VII = *Geo::OSRc::SRS_PT_WAGNER_VII;
*SRS_PP_CENTRAL_MERIDIAN = *Geo::OSRc::SRS_PP_CENTRAL_MERIDIAN;
*SRS_PP_SCALE_FACTOR = *Geo::OSRc::SRS_PP_SCALE_FACTOR;
*SRS_PP_STANDARD_PARALLEL_1 = *Geo::OSRc::SRS_PP_STANDARD_PARALLEL_1;
*SRS_PP_STANDARD_PARALLEL_2 = *Geo::OSRc::SRS_PP_STANDARD_PARALLEL_2;
*SRS_PP_PSEUDO_STD_PARALLEL_1 = *Geo::OSRc::SRS_PP_PSEUDO_STD_PARALLEL_1;
*SRS_PP_LONGITUDE_OF_CENTER = *Geo::OSRc::SRS_PP_LONGITUDE_OF_CENTER;
*SRS_PP_LATITUDE_OF_CENTER = *Geo::OSRc::SRS_PP_LATITUDE_OF_CENTER;
*SRS_PP_LONGITUDE_OF_ORIGIN = *Geo::OSRc::SRS_PP_LONGITUDE_OF_ORIGIN;
*SRS_PP_LATITUDE_OF_ORIGIN = *Geo::OSRc::SRS_PP_LATITUDE_OF_ORIGIN;
*SRS_PP_FALSE_EASTING = *Geo::OSRc::SRS_PP_FALSE_EASTING;
*SRS_PP_FALSE_NORTHING = *Geo::OSRc::SRS_PP_FALSE_NORTHING;
*SRS_PP_AZIMUTH = *Geo::OSRc::SRS_PP_AZIMUTH;
*SRS_PP_LONGITUDE_OF_POINT_1 = *Geo::OSRc::SRS_PP_LONGITUDE_OF_POINT_1;
*SRS_PP_LATITUDE_OF_POINT_1 = *Geo::OSRc::SRS_PP_LATITUDE_OF_POINT_1;
*SRS_PP_LONGITUDE_OF_POINT_2 = *Geo::OSRc::SRS_PP_LONGITUDE_OF_POINT_2;
*SRS_PP_LATITUDE_OF_POINT_2 = *Geo::OSRc::SRS_PP_LATITUDE_OF_POINT_2;
*SRS_PP_LONGITUDE_OF_POINT_3 = *Geo::OSRc::SRS_PP_LONGITUDE_OF_POINT_3;
*SRS_PP_LATITUDE_OF_POINT_3 = *Geo::OSRc::SRS_PP_LATITUDE_OF_POINT_3;
*SRS_PP_RECTIFIED_GRID_ANGLE = *Geo::OSRc::SRS_PP_RECTIFIED_GRID_ANGLE;
*SRS_PP_LANDSAT_NUMBER = *Geo::OSRc::SRS_PP_LANDSAT_NUMBER;
*SRS_PP_PATH_NUMBER = *Geo::OSRc::SRS_PP_PATH_NUMBER;
*SRS_PP_PERSPECTIVE_POINT_HEIGHT = *Geo::OSRc::SRS_PP_PERSPECTIVE_POINT_HEIGHT;
*SRS_PP_SATELLITE_HEIGHT = *Geo::OSRc::SRS_PP_SATELLITE_HEIGHT;
*SRS_PP_FIPSZONE = *Geo::OSRc::SRS_PP_FIPSZONE;
*SRS_PP_ZONE = *Geo::OSRc::SRS_PP_ZONE;
*SRS_PP_LATITUDE_OF_1ST_POINT = *Geo::OSRc::SRS_PP_LATITUDE_OF_1ST_POINT;
*SRS_PP_LONGITUDE_OF_1ST_POINT = *Geo::OSRc::SRS_PP_LONGITUDE_OF_1ST_POINT;
*SRS_PP_LATITUDE_OF_2ND_POINT = *Geo::OSRc::SRS_PP_LATITUDE_OF_2ND_POINT;
*SRS_PP_LONGITUDE_OF_2ND_POINT = *Geo::OSRc::SRS_PP_LONGITUDE_OF_2ND_POINT;
*SRS_UL_METER = *Geo::OSRc::SRS_UL_METER;
*SRS_UL_FOOT = *Geo::OSRc::SRS_UL_FOOT;
*SRS_UL_FOOT_CONV = *Geo::OSRc::SRS_UL_FOOT_CONV;
*SRS_UL_US_FOOT = *Geo::OSRc::SRS_UL_US_FOOT;
*SRS_UL_US_FOOT_CONV = *Geo::OSRc::SRS_UL_US_FOOT_CONV;
*SRS_UL_NAUTICAL_MILE = *Geo::OSRc::SRS_UL_NAUTICAL_MILE;
*SRS_UL_NAUTICAL_MILE_CONV = *Geo::OSRc::SRS_UL_NAUTICAL_MILE_CONV;
*SRS_UL_LINK = *Geo::OSRc::SRS_UL_LINK;
*SRS_UL_LINK_CONV = *Geo::OSRc::SRS_UL_LINK_CONV;
*SRS_UL_CHAIN = *Geo::OSRc::SRS_UL_CHAIN;
*SRS_UL_CHAIN_CONV = *Geo::OSRc::SRS_UL_CHAIN_CONV;
*SRS_UL_ROD = *Geo::OSRc::SRS_UL_ROD;
*SRS_UL_ROD_CONV = *Geo::OSRc::SRS_UL_ROD_CONV;
*SRS_UA_DEGREE = *Geo::OSRc::SRS_UA_DEGREE;
*SRS_UA_DEGREE_CONV = *Geo::OSRc::SRS_UA_DEGREE_CONV;
*SRS_UA_RADIAN = *Geo::OSRc::SRS_UA_RADIAN;
*SRS_PM_GREENWICH = *Geo::OSRc::SRS_PM_GREENWICH;
*SRS_DN_NAD27 = *Geo::OSRc::SRS_DN_NAD27;
*SRS_DN_NAD83 = *Geo::OSRc::SRS_DN_NAD83;
*SRS_DN_WGS72 = *Geo::OSRc::SRS_DN_WGS72;
*SRS_DN_WGS84 = *Geo::OSRc::SRS_DN_WGS84;
*SRS_WGS84_SEMIMAJOR = *Geo::OSRc::SRS_WGS84_SEMIMAJOR;
*SRS_WGS84_INVFLATTENING = *Geo::OSRc::SRS_WGS84_INVFLATTENING;

    sub RELEASE_PARENTS {
    }
    package Geo::OSR::SpatialReference;
    use strict;
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
	    ImportFromESRI($self, $param{ESRI});
	} elsif ($param{EPSG}) {
	    ImportFromEPSG($self, $param{EPSG});
	} elsif ($param{PCI}) {
	    ImportFromPCI($self, $param{PCI});
	} elsif ($param{USGS}) {
	    ImportFromUSGS($self, $param{USGS});
	} elsif ($param{XML}) {
	    ImportFromXML($self, $param{XML});
	}
	bless $self, $pkg if defined $self;
    }
    *AsText = *ExportToWkt;
1;
