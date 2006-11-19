# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package Geo::OGR;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
require Geo::OSR;
package Geo::OGRc;
bootstrap Geo::OGR;
package Geo::OGR;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package Geo::OGR;

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

package Geo::OGR;

*UseExceptions = *Geo::OGRc::UseExceptions;
*DontUseExceptions = *Geo::OGRc::DontUseExceptions;
*CreateGeometryFromWkb = *Geo::OGRc::CreateGeometryFromWkb;
*CreateGeometryFromWkt = *Geo::OGRc::CreateGeometryFromWkt;
*CreateGeometryFromGML = *Geo::OGRc::CreateGeometryFromGML;
*GetDriverCount = *Geo::OGRc::GetDriverCount;
*GetOpenDSCount = *Geo::OGRc::GetOpenDSCount;
*SetGenerate_DB2_V72_BYTE_ORDER = *Geo::OGRc::SetGenerate_DB2_V72_BYTE_ORDER;
*RegisterAll = *Geo::OGRc::RegisterAll;
*GetOpenDS = *Geo::OGRc::GetOpenDS;
*Open = *Geo::OGRc::Open;
*OpenShared = *Geo::OGRc::OpenShared;
*GetDriverByName = *Geo::OGRc::GetDriverByName;
*GetDriver = *Geo::OGRc::GetDriver;

############# Class : Geo::OGR::Driver ##############

package Geo::OGR::Driver;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
*swig_name_get = *Geo::OGRc::Driver_name_get;
*swig_name_set = *Geo::OGRc::Driver_name_set;
*CreateDataSource = *Geo::OGRc::Driver_CreateDataSource;
*CopyDataSource = *Geo::OGRc::Driver_CopyDataSource;
*Open = *Geo::OGRc::Driver_Open;
*DeleteDataSource = *Geo::OGRc::Driver_DeleteDataSource;
*TestCapability = *Geo::OGRc::Driver_TestCapability;
*GetName = *Geo::OGRc::Driver_GetName;
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


############# Class : Geo::OGR::DataSource ##############

package Geo::OGR::DataSource;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
*swig_name_get = *Geo::OGRc::DataSource_name_get;
*swig_name_set = *Geo::OGRc::DataSource_name_set;
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OGRc::delete_DataSource($self);
        delete $OWNER{$self};
    }
}

*GetRefCount = *Geo::OGRc::DataSource_GetRefCount;
*GetSummaryRefCount = *Geo::OGRc::DataSource_GetSummaryRefCount;
*GetLayerCount = *Geo::OGRc::DataSource_GetLayerCount;
*GetDriver = *Geo::OGRc::DataSource_GetDriver;
*GetName = *Geo::OGRc::DataSource_GetName;
*DeleteLayer = *Geo::OGRc::DataSource_DeleteLayer;
*CreateLayer = *Geo::OGRc::DataSource_CreateLayer;
*CopyLayer = *Geo::OGRc::DataSource_CopyLayer;
*GetLayerByIndex = *Geo::OGRc::DataSource_GetLayerByIndex;
*GetLayerByName = *Geo::OGRc::DataSource_GetLayerByName;
*TestCapability = *Geo::OGRc::DataSource_TestCapability;
*ExecuteSQL = *Geo::OGRc::DataSource_ExecuteSQL;
*ReleaseResultSet = *Geo::OGRc::DataSource_ReleaseResultSet;
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


############# Class : Geo::OGR::Layer ##############

package Geo::OGR::Layer;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
*GetRefCount = *Geo::OGRc::Layer_GetRefCount;
*SetSpatialFilter = *Geo::OGRc::Layer_SetSpatialFilter;
*SetSpatialFilterRect = *Geo::OGRc::Layer_SetSpatialFilterRect;
*GetSpatialFilter = *Geo::OGRc::Layer_GetSpatialFilter;
*SetAttributeFilter = *Geo::OGRc::Layer_SetAttributeFilter;
*ResetReading = *Geo::OGRc::Layer_ResetReading;
*GetName = *Geo::OGRc::Layer_GetName;
*GetFeature = *Geo::OGRc::Layer_GetFeature;
*GetNextFeature = *Geo::OGRc::Layer_GetNextFeature;
*SetNextByIndex = *Geo::OGRc::Layer_SetNextByIndex;
*SetFeature = *Geo::OGRc::Layer_SetFeature;
*CreateFeature = *Geo::OGRc::Layer_CreateFeature;
*DeleteFeature = *Geo::OGRc::Layer_DeleteFeature;
*SyncToDisk = *Geo::OGRc::Layer_SyncToDisk;
*GetLayerDefn = *Geo::OGRc::Layer_GetLayerDefn;
*GetFeatureCount = *Geo::OGRc::Layer_GetFeatureCount;
*GetExtent = *Geo::OGRc::Layer_GetExtent;
*TestCapability = *Geo::OGRc::Layer_TestCapability;
*CreateField = *Geo::OGRc::Layer_CreateField;
*StartTransaction = *Geo::OGRc::Layer_StartTransaction;
*CommitTransaction = *Geo::OGRc::Layer_CommitTransaction;
*RollbackTransaction = *Geo::OGRc::Layer_RollbackTransaction;
*GetSpatialRef = *Geo::OGRc::Layer_GetSpatialRef;
*GetFeatureRead = *Geo::OGRc::Layer_GetFeatureRead;
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


############# Class : Geo::OGR::Feature ##############

package Geo::OGR::Feature;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OGRc::delete_Feature($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = Geo::OGRc::new_Feature(@_);
    bless $self, $pkg if defined($self);
}

*GetDefnRef = *Geo::OGRc::Feature_GetDefnRef;
*SetGeometry = *Geo::OGRc::Feature_SetGeometry;
*SetGeometryDirectly = *Geo::OGRc::Feature_SetGeometryDirectly;
*GetGeometryRef = *Geo::OGRc::Feature_GetGeometryRef;
*Clone = *Geo::OGRc::Feature_Clone;
*Equal = *Geo::OGRc::Feature_Equal;
*GetFieldCount = *Geo::OGRc::Feature_GetFieldCount;
*GetFieldDefnRef = *Geo::OGRc::Feature_GetFieldDefnRef;
*GetField = *Geo::OGRc::Feature_GetField;
*GetFieldAsInteger = *Geo::OGRc::Feature_GetFieldAsInteger;
*GetFieldAsDouble = *Geo::OGRc::Feature_GetFieldAsDouble;
*IsFieldSet = *Geo::OGRc::Feature_IsFieldSet;
*GetFieldIndex = *Geo::OGRc::Feature_GetFieldIndex;
*GetFID = *Geo::OGRc::Feature_GetFID;
*SetFID = *Geo::OGRc::Feature_SetFID;
*DumpReadable = *Geo::OGRc::Feature_DumpReadable;
*UnsetField = *Geo::OGRc::Feature_UnsetField;
*SetField = *Geo::OGRc::Feature_SetField;
*SetFrom = *Geo::OGRc::Feature_SetFrom;
*GetStyleString = *Geo::OGRc::Feature_GetStyleString;
*SetStyleString = *Geo::OGRc::Feature_SetStyleString;
*GetFieldType = *Geo::OGRc::Feature_GetFieldType;
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


############# Class : Geo::OGR::FeatureDefn ##############

package Geo::OGR::FeatureDefn;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OGRc::delete_FeatureDefn($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = Geo::OGRc::new_FeatureDefn(@_);
    bless $self, $pkg if defined($self);
}

*GetName = *Geo::OGRc::FeatureDefn_GetName;
*GetFieldCount = *Geo::OGRc::FeatureDefn_GetFieldCount;
*GetFieldDefn = *Geo::OGRc::FeatureDefn_GetFieldDefn;
*GetFieldIndex = *Geo::OGRc::FeatureDefn_GetFieldIndex;
*AddFieldDefn = *Geo::OGRc::FeatureDefn_AddFieldDefn;
*GetGeomType = *Geo::OGRc::FeatureDefn_GetGeomType;
*SetGeomType = *Geo::OGRc::FeatureDefn_SetGeomType;
*GetReferenceCount = *Geo::OGRc::FeatureDefn_GetReferenceCount;
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


############# Class : Geo::OGR::FieldDefn ##############

package Geo::OGR::FieldDefn;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OGRc::delete_FieldDefn($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = Geo::OGRc::new_FieldDefn(@_);
    bless $self, $pkg if defined($self);
}

*GetName = *Geo::OGRc::FieldDefn_GetName;
*GetNameRef = *Geo::OGRc::FieldDefn_GetNameRef;
*SetName = *Geo::OGRc::FieldDefn_SetName;
*GetType = *Geo::OGRc::FieldDefn_GetType;
*SetType = *Geo::OGRc::FieldDefn_SetType;
*GetJustify = *Geo::OGRc::FieldDefn_GetJustify;
*SetJustify = *Geo::OGRc::FieldDefn_SetJustify;
*GetWidth = *Geo::OGRc::FieldDefn_GetWidth;
*SetWidth = *Geo::OGRc::FieldDefn_SetWidth;
*GetPrecision = *Geo::OGRc::FieldDefn_GetPrecision;
*SetPrecision = *Geo::OGRc::FieldDefn_SetPrecision;
*GetFieldTypeName = *Geo::OGRc::FieldDefn_GetFieldTypeName;
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


############# Class : Geo::OGR::Geometry ##############

package Geo::OGR::Geometry;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::OGR );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::OGRc::delete_Geometry($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = Geo::OGRc::new_Geometry(@_);
    bless $self, $pkg if defined($self);
}

*ExportToWkt = *Geo::OGRc::Geometry_ExportToWkt;
*ExportToWkb = *Geo::OGRc::Geometry_ExportToWkb;
*ExportToGML = *Geo::OGRc::Geometry_ExportToGML;
*AddPoint = *Geo::OGRc::Geometry_AddPoint;
*AddGeometryDirectly = *Geo::OGRc::Geometry_AddGeometryDirectly;
*AddGeometry = *Geo::OGRc::Geometry_AddGeometry;
*Clone = *Geo::OGRc::Geometry_Clone;
*GetGeometryType = *Geo::OGRc::Geometry_GetGeometryType;
*GetGeometryName = *Geo::OGRc::Geometry_GetGeometryName;
*GetArea = *Geo::OGRc::Geometry_GetArea;
*GetPointCount = *Geo::OGRc::Geometry_GetPointCount;
*GetX = *Geo::OGRc::Geometry_GetX;
*GetY = *Geo::OGRc::Geometry_GetY;
*GetZ = *Geo::OGRc::Geometry_GetZ;
*GetGeometryCount = *Geo::OGRc::Geometry_GetGeometryCount;
*SetPoint = *Geo::OGRc::Geometry_SetPoint;
*GetGeometryRef = *Geo::OGRc::Geometry_GetGeometryRef;
*GetBoundary = *Geo::OGRc::Geometry_GetBoundary;
*ConvexHull = *Geo::OGRc::Geometry_ConvexHull;
*Buffer = *Geo::OGRc::Geometry_Buffer;
*Intersection = *Geo::OGRc::Geometry_Intersection;
*Union = *Geo::OGRc::Geometry_Union;
*Difference = *Geo::OGRc::Geometry_Difference;
*SymmetricDifference = *Geo::OGRc::Geometry_SymmetricDifference;
*Distance = *Geo::OGRc::Geometry_Distance;
*Empty = *Geo::OGRc::Geometry_Empty;
*Intersect = *Geo::OGRc::Geometry_Intersect;
*Equal = *Geo::OGRc::Geometry_Equal;
*Disjoint = *Geo::OGRc::Geometry_Disjoint;
*Touches = *Geo::OGRc::Geometry_Touches;
*Crosses = *Geo::OGRc::Geometry_Crosses;
*Within = *Geo::OGRc::Geometry_Within;
*Contains = *Geo::OGRc::Geometry_Contains;
*Overlaps = *Geo::OGRc::Geometry_Overlaps;
*TransformTo = *Geo::OGRc::Geometry_TransformTo;
*Transform = *Geo::OGRc::Geometry_Transform;
*GetSpatialReference = *Geo::OGRc::Geometry_GetSpatialReference;
*AssignSpatialReference = *Geo::OGRc::Geometry_AssignSpatialReference;
*CloseRings = *Geo::OGRc::Geometry_CloseRings;
*FlattenTo2D = *Geo::OGRc::Geometry_FlattenTo2D;
*GetEnvelope = *Geo::OGRc::Geometry_GetEnvelope;
*Centroid = *Geo::OGRc::Geometry_Centroid;
*WkbSize = *Geo::OGRc::Geometry_WkbSize;
*GetCoordinateDimension = *Geo::OGRc::Geometry_GetCoordinateDimension;
*SetCoordinateDimension = *Geo::OGRc::Geometry_SetCoordinateDimension;
*GetDimension = *Geo::OGRc::Geometry_GetDimension;
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

package Geo::OGR;

*wkb25Bit = *Geo::OGRc::wkb25Bit;
*wkbUnknown = *Geo::OGRc::wkbUnknown;
*wkbPoint = *Geo::OGRc::wkbPoint;
*wkbLineString = *Geo::OGRc::wkbLineString;
*wkbPolygon = *Geo::OGRc::wkbPolygon;
*wkbMultiPoint = *Geo::OGRc::wkbMultiPoint;
*wkbMultiLineString = *Geo::OGRc::wkbMultiLineString;
*wkbMultiPolygon = *Geo::OGRc::wkbMultiPolygon;
*wkbGeometryCollection = *Geo::OGRc::wkbGeometryCollection;
*wkbNone = *Geo::OGRc::wkbNone;
*wkbLinearRing = *Geo::OGRc::wkbLinearRing;
*wkbPoint25D = *Geo::OGRc::wkbPoint25D;
*wkbLineString25D = *Geo::OGRc::wkbLineString25D;
*wkbPolygon25D = *Geo::OGRc::wkbPolygon25D;
*wkbMultiPoint25D = *Geo::OGRc::wkbMultiPoint25D;
*wkbMultiLineString25D = *Geo::OGRc::wkbMultiLineString25D;
*wkbMultiPolygon25D = *Geo::OGRc::wkbMultiPolygon25D;
*wkbGeometryCollection25D = *Geo::OGRc::wkbGeometryCollection25D;
*OFTInteger = *Geo::OGRc::OFTInteger;
*OFTIntegerList = *Geo::OGRc::OFTIntegerList;
*OFTReal = *Geo::OGRc::OFTReal;
*OFTRealList = *Geo::OGRc::OFTRealList;
*OFTString = *Geo::OGRc::OFTString;
*OFTStringList = *Geo::OGRc::OFTStringList;
*OFTWideString = *Geo::OGRc::OFTWideString;
*OFTWideStringList = *Geo::OGRc::OFTWideStringList;
*OFTBinary = *Geo::OGRc::OFTBinary;
*OFTDate = *Geo::OGRc::OFTDate;
*OFTTime = *Geo::OGRc::OFTTime;
*OFTDateTime = *Geo::OGRc::OFTDateTime;
*OJUndefined = *Geo::OGRc::OJUndefined;
*OJLeft = *Geo::OGRc::OJLeft;
*OJRight = *Geo::OGRc::OJRight;
*wkbXDR = *Geo::OGRc::wkbXDR;
*wkbNDR = *Geo::OGRc::wkbNDR;
*OLCRandomRead = *Geo::OGRc::OLCRandomRead;
*OLCSequentialWrite = *Geo::OGRc::OLCSequentialWrite;
*OLCRandomWrite = *Geo::OGRc::OLCRandomWrite;
*OLCFastSpatialFilter = *Geo::OGRc::OLCFastSpatialFilter;
*OLCFastFeatureCount = *Geo::OGRc::OLCFastFeatureCount;
*OLCFastGetExtent = *Geo::OGRc::OLCFastGetExtent;
*OLCCreateField = *Geo::OGRc::OLCCreateField;
*OLCTransactions = *Geo::OGRc::OLCTransactions;
*OLCDeleteFeature = *Geo::OGRc::OLCDeleteFeature;
*OLCFastSetNextByIndex = *Geo::OGRc::OLCFastSetNextByIndex;
*ODsCCreateLayer = *Geo::OGRc::ODsCCreateLayer;
*ODsCDeleteLayer = *Geo::OGRc::ODsCDeleteLayer;
*ODrCCreateDataSource = *Geo::OGRc::ODrCCreateDataSource;
*ODrCDeleteDataSource = *Geo::OGRc::ODrCDeleteDataSource;
1;
