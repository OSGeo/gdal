# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package osr;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
package osrc;
bootstrap osr;
package osr;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package osr;

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

package osr;

*GetWellKnownGeogCSAsWKT = *osrc::GetWellKnownGeogCSAsWKT;
*GetProjectionMethods = *osrc::GetProjectionMethods;
*GetProjectionMethodParameterList = *osrc::GetProjectionMethodParameterList;
*GetProjectionMethodParamInfo = *osrc::GetProjectionMethodParamInfo;

############# Class : osr::SpatialReference ##############

package osr::SpatialReference;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( osr );
%OWNER = ();
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = osrc::new_SpatialReference(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        osrc::delete_SpatialReference($self);
        delete $OWNER{$self};
    }
}

*__str__ = *osrc::SpatialReference___str__;
*IsSame = *osrc::SpatialReference_IsSame;
*IsSameGeogCS = *osrc::SpatialReference_IsSameGeogCS;
*IsGeographic = *osrc::SpatialReference_IsGeographic;
*IsProjected = *osrc::SpatialReference_IsProjected;
*GetAttrValue = *osrc::SpatialReference_GetAttrValue;
*SetAttrValue = *osrc::SpatialReference_SetAttrValue;
*SetAngularUnits = *osrc::SpatialReference_SetAngularUnits;
*GetAngularUnits = *osrc::SpatialReference_GetAngularUnits;
*SetLinearUnits = *osrc::SpatialReference_SetLinearUnits;
*GetLinearUnits = *osrc::SpatialReference_GetLinearUnits;
*GetLinearUnitsName = *osrc::SpatialReference_GetLinearUnitsName;
*GetAuthorityCode = *osrc::SpatialReference_GetAuthorityCode;
*GetAuthorityName = *osrc::SpatialReference_GetAuthorityName;
*SetUTM = *osrc::SpatialReference_SetUTM;
*SetStatePlane = *osrc::SpatialReference_SetStatePlane;
*AutoIdentifyEPSG = *osrc::SpatialReference_AutoIdentifyEPSG;
*SetProjection = *osrc::SpatialReference_SetProjection;
*SetProjParm = *osrc::SpatialReference_SetProjParm;
*GetProjParm = *osrc::SpatialReference_GetProjParm;
*SetNormProjParm = *osrc::SpatialReference_SetNormProjParm;
*GetNormProjParm = *osrc::SpatialReference_GetNormProjParm;
*SetACEA = *osrc::SpatialReference_SetACEA;
*SetAE = *osrc::SpatialReference_SetAE;
*SetCS = *osrc::SpatialReference_SetCS;
*SetBonne = *osrc::SpatialReference_SetBonne;
*SetEC = *osrc::SpatialReference_SetEC;
*SetEckertIV = *osrc::SpatialReference_SetEckertIV;
*SetEckertVI = *osrc::SpatialReference_SetEckertVI;
*SetEquirectangular = *osrc::SpatialReference_SetEquirectangular;
*SetGS = *osrc::SpatialReference_SetGS;
*SetWellKnownGeogCS = *osrc::SpatialReference_SetWellKnownGeogCS;
*SetFromUserInput = *osrc::SpatialReference_SetFromUserInput;
*CopyGeogCSFrom = *osrc::SpatialReference_CopyGeogCSFrom;
*SetTOWGS84 = *osrc::SpatialReference_SetTOWGS84;
*GetTOWGS84 = *osrc::SpatialReference_GetTOWGS84;
*SetGeogCS = *osrc::SpatialReference_SetGeogCS;
*SetProjCS = *osrc::SpatialReference_SetProjCS;
*ImportFromWkt = *osrc::SpatialReference_ImportFromWkt;
*ImportFromProj4 = *osrc::SpatialReference_ImportFromProj4;
*ImportFromESRI = *osrc::SpatialReference_ImportFromESRI;
*ImportFromEPSG = *osrc::SpatialReference_ImportFromEPSG;
*ImportFromPCI = *osrc::SpatialReference_ImportFromPCI;
*ImportFromUSGS = *osrc::SpatialReference_ImportFromUSGS;
*ImportFromXML = *osrc::SpatialReference_ImportFromXML;
*ExportToWkt = *osrc::SpatialReference_ExportToWkt;
*ExportToPrettyWkt = *osrc::SpatialReference_ExportToPrettyWkt;
*ExportToProj4 = *osrc::SpatialReference_ExportToProj4;
*ExportToPCI = *osrc::SpatialReference_ExportToPCI;
*ExportToUSGS = *osrc::SpatialReference_ExportToUSGS;
*ExportToXML = *osrc::SpatialReference_ExportToXML;
*CloneGeogCS = *osrc::SpatialReference_CloneGeogCS;
*Validate = *osrc::SpatialReference_Validate;
*StripCTParms = *osrc::SpatialReference_StripCTParms;
*FixupOrdering = *osrc::SpatialReference_FixupOrdering;
*Fixup = *osrc::SpatialReference_Fixup;
*MorphToESRI = *osrc::SpatialReference_MorphToESRI;
*MorphFromESRI = *osrc::SpatialReference_MorphFromESRI;
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


############# Class : osr::CoordinateTransformation ##############

package osr::CoordinateTransformation;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( osr );
%OWNER = ();
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = osrc::new_CoordinateTransformation(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        osrc::delete_CoordinateTransformation($self);
        delete $OWNER{$self};
    }
}

*TransformPoint = *osrc::CoordinateTransformation_TransformPoint;
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

package osr;

*SRS_PT_ALBERS_CONIC_EQUAL_AREA = *osrc::SRS_PT_ALBERS_CONIC_EQUAL_AREA;
*SRS_PT_AZIMUTHAL_EQUIDISTANT = *osrc::SRS_PT_AZIMUTHAL_EQUIDISTANT;
*SRS_PT_CASSINI_SOLDNER = *osrc::SRS_PT_CASSINI_SOLDNER;
*SRS_PT_CYLINDRICAL_EQUAL_AREA = *osrc::SRS_PT_CYLINDRICAL_EQUAL_AREA;
*SRS_PT_ECKERT_IV = *osrc::SRS_PT_ECKERT_IV;
*SRS_PT_ECKERT_VI = *osrc::SRS_PT_ECKERT_VI;
*SRS_PT_EQUIDISTANT_CONIC = *osrc::SRS_PT_EQUIDISTANT_CONIC;
*SRS_PT_EQUIRECTANGULAR = *osrc::SRS_PT_EQUIRECTANGULAR;
*SRS_PT_GALL_STEREOGRAPHIC = *osrc::SRS_PT_GALL_STEREOGRAPHIC;
*SRS_PT_GNOMONIC = *osrc::SRS_PT_GNOMONIC;
*SRS_PT_GOODE_HOMOLOSINE = *osrc::SRS_PT_GOODE_HOMOLOSINE;
*SRS_PT_HOTINE_OBLIQUE_MERCATOR = *osrc::SRS_PT_HOTINE_OBLIQUE_MERCATOR;
*SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = *osrc::SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN;
*SRS_PT_LABORDE_OBLIQUE_MERCATOR = *osrc::SRS_PT_LABORDE_OBLIQUE_MERCATOR;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = *osrc::SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = *osrc::SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP;
*SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = *osrc::SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM;
*SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = *osrc::SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA;
*SRS_PT_MERCATOR_1SP = *osrc::SRS_PT_MERCATOR_1SP;
*SRS_PT_MERCATOR_2SP = *osrc::SRS_PT_MERCATOR_2SP;
*SRS_PT_MILLER_CYLINDRICAL = *osrc::SRS_PT_MILLER_CYLINDRICAL;
*SRS_PT_MOLLWEIDE = *osrc::SRS_PT_MOLLWEIDE;
*SRS_PT_NEW_ZEALAND_MAP_GRID = *osrc::SRS_PT_NEW_ZEALAND_MAP_GRID;
*SRS_PT_OBLIQUE_STEREOGRAPHIC = *osrc::SRS_PT_OBLIQUE_STEREOGRAPHIC;
*SRS_PT_ORTHOGRAPHIC = *osrc::SRS_PT_ORTHOGRAPHIC;
*SRS_PT_POLAR_STEREOGRAPHIC = *osrc::SRS_PT_POLAR_STEREOGRAPHIC;
*SRS_PT_POLYCONIC = *osrc::SRS_PT_POLYCONIC;
*SRS_PT_ROBINSON = *osrc::SRS_PT_ROBINSON;
*SRS_PT_SINUSOIDAL = *osrc::SRS_PT_SINUSOIDAL;
*SRS_PT_STEREOGRAPHIC = *osrc::SRS_PT_STEREOGRAPHIC;
*SRS_PT_SWISS_OBLIQUE_CYLINDRICAL = *osrc::SRS_PT_SWISS_OBLIQUE_CYLINDRICAL;
*SRS_PT_TRANSVERSE_MERCATOR = *osrc::SRS_PT_TRANSVERSE_MERCATOR;
*SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = *osrc::SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED;
*SRS_PT_TRANSVERSE_MERCATOR_MI_22 = *osrc::SRS_PT_TRANSVERSE_MERCATOR_MI_22;
*SRS_PT_TRANSVERSE_MERCATOR_MI_23 = *osrc::SRS_PT_TRANSVERSE_MERCATOR_MI_23;
*SRS_PT_TRANSVERSE_MERCATOR_MI_24 = *osrc::SRS_PT_TRANSVERSE_MERCATOR_MI_24;
*SRS_PT_TRANSVERSE_MERCATOR_MI_25 = *osrc::SRS_PT_TRANSVERSE_MERCATOR_MI_25;
*SRS_PT_TUNISIA_MINING_GRID = *osrc::SRS_PT_TUNISIA_MINING_GRID;
*SRS_PT_VANDERGRINTEN = *osrc::SRS_PT_VANDERGRINTEN;
*SRS_PT_KROVAK = *osrc::SRS_PT_KROVAK;
*SRS_PP_CENTRAL_MERIDIAN = *osrc::SRS_PP_CENTRAL_MERIDIAN;
*SRS_PP_SCALE_FACTOR = *osrc::SRS_PP_SCALE_FACTOR;
*SRS_PP_STANDARD_PARALLEL_1 = *osrc::SRS_PP_STANDARD_PARALLEL_1;
*SRS_PP_STANDARD_PARALLEL_2 = *osrc::SRS_PP_STANDARD_PARALLEL_2;
*SRS_PP_PSEUDO_STD_PARALLEL_1 = *osrc::SRS_PP_PSEUDO_STD_PARALLEL_1;
*SRS_PP_LONGITUDE_OF_CENTER = *osrc::SRS_PP_LONGITUDE_OF_CENTER;
*SRS_PP_LATITUDE_OF_CENTER = *osrc::SRS_PP_LATITUDE_OF_CENTER;
*SRS_PP_LONGITUDE_OF_ORIGIN = *osrc::SRS_PP_LONGITUDE_OF_ORIGIN;
*SRS_PP_LATITUDE_OF_ORIGIN = *osrc::SRS_PP_LATITUDE_OF_ORIGIN;
*SRS_PP_FALSE_EASTING = *osrc::SRS_PP_FALSE_EASTING;
*SRS_PP_FALSE_NORTHING = *osrc::SRS_PP_FALSE_NORTHING;
*SRS_PP_AZIMUTH = *osrc::SRS_PP_AZIMUTH;
*SRS_PP_LONGITUDE_OF_POINT_1 = *osrc::SRS_PP_LONGITUDE_OF_POINT_1;
*SRS_PP_LATITUDE_OF_POINT_1 = *osrc::SRS_PP_LATITUDE_OF_POINT_1;
*SRS_PP_LONGITUDE_OF_POINT_2 = *osrc::SRS_PP_LONGITUDE_OF_POINT_2;
*SRS_PP_LATITUDE_OF_POINT_2 = *osrc::SRS_PP_LATITUDE_OF_POINT_2;
*SRS_PP_LONGITUDE_OF_POINT_3 = *osrc::SRS_PP_LONGITUDE_OF_POINT_3;
*SRS_PP_LATITUDE_OF_POINT_3 = *osrc::SRS_PP_LATITUDE_OF_POINT_3;
*SRS_PP_RECTIFIED_GRID_ANGLE = *osrc::SRS_PP_RECTIFIED_GRID_ANGLE;
*SRS_PP_LANDSAT_NUMBER = *osrc::SRS_PP_LANDSAT_NUMBER;
*SRS_PP_PATH_NUMBER = *osrc::SRS_PP_PATH_NUMBER;
*SRS_PP_PERSPECTIVE_POINT_HEIGHT = *osrc::SRS_PP_PERSPECTIVE_POINT_HEIGHT;
*SRS_PP_FIPSZONE = *osrc::SRS_PP_FIPSZONE;
*SRS_PP_ZONE = *osrc::SRS_PP_ZONE;
*SRS_UL_METER = *osrc::SRS_UL_METER;
*SRS_UL_FOOT = *osrc::SRS_UL_FOOT;
*SRS_UL_FOOT_CONV = *osrc::SRS_UL_FOOT_CONV;
*SRS_UL_US_FOOT = *osrc::SRS_UL_US_FOOT;
*SRS_UL_US_FOOT_CONV = *osrc::SRS_UL_US_FOOT_CONV;
*SRS_UL_NAUTICAL_MILE = *osrc::SRS_UL_NAUTICAL_MILE;
*SRS_UL_NAUTICAL_MILE_CONV = *osrc::SRS_UL_NAUTICAL_MILE_CONV;
*SRS_UL_LINK = *osrc::SRS_UL_LINK;
*SRS_UL_LINK_CONV = *osrc::SRS_UL_LINK_CONV;
*SRS_UL_CHAIN = *osrc::SRS_UL_CHAIN;
*SRS_UL_CHAIN_CONV = *osrc::SRS_UL_CHAIN_CONV;
*SRS_UL_ROD = *osrc::SRS_UL_ROD;
*SRS_UL_ROD_CONV = *osrc::SRS_UL_ROD_CONV;
*SRS_DN_NAD27 = *osrc::SRS_DN_NAD27;
*SRS_DN_NAD83 = *osrc::SRS_DN_NAD83;
*SRS_DN_WGS72 = *osrc::SRS_DN_WGS72;
*SRS_DN_WGS84 = *osrc::SRS_DN_WGS84;
*SRS_WGS84_SEMIMAJOR = *osrc::SRS_WGS84_SEMIMAJOR;
*SRS_WGS84_INVFLATTENING = *osrc::SRS_WGS84_INVFLATTENING;
1;
