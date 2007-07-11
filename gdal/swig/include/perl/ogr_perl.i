/*
 * $Id$
 *
 * perl specific code for ogr bindings.
 */

/*
 * $Log$
 * Revision 1.6  2006/11/19 20:07:35  ajolma
 * instead of renaming, create GetField as a copy of GetFieldAsString
 *
 * Revision 1.5  2006/11/19 17:42:24  ajolma
 * There is no sense in having typed versions of GetField in Perl, renamed GetFieldAsString to GetField
 *
 * Revision 1.4  2005/09/21 19:04:12  kruland
 * Need to %include cpl_exceptions.i
 *
 * Revision 1.3  2005/09/21 18:00:05  kruland
 * Turn on UseExceptions in ogr init code.
 *
 * Revision 1.2  2005/09/13 17:36:28  kruland
 * Whoops!  import typemaps_perl.i.
 *
 * Revision 1.1  2005/09/13 16:08:45  kruland
 * Added perl specific modifications for gdal and ogr.
 *
 *
 */

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

%extend OGRGeometryShadow {

    %rename (AddPoint_3D) AddPoint;

}

%extend OGRFeatureShadow {

  const char* GetField(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetField(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return (const char *) OGR_F_GetFieldAsString(self, i);
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

%perlcode %{
    use Carp;
    {
	package Geo::OGR::Geometry;
	sub AddPoint {
	    @_ == 4 ? AddPoint_3D(@_) : AddPoint_2D(@_);
	}
    }
    sub GeometryType {
	my($type_or_name) = @_;
	my @types = ('Unknown', 'Point', 'LineString', 'Polygon',
		     'MultiPoint', 'MultiLineString', 'MultiPolygon',
		     'GeometryCollection', 'None', 'LinearRing',
		     'Point25D', 'LineString25D', 'Polygon25D',
		     'MultiPoint25D', 'MultiLineString25D', 'MultiPolygon25D',
		     'GeometryCollection25D');
	if (defined $type_or_name) {
	    if ($type_or_name =~ /^[+-]?\d/) {
		for (@types) {
		    if (eval "\$type_or_name == \$Geo::OGR::wkb$_") {return $_}
		}
		croak "unknown geometry type value: $type_or_name";
	    } else {
		for (@types) {
		    if ($type_or_name eq $_) {return eval "\$Geo::OGR::wkb$_"}
		}
		croak "unknown geometry type name: $type_or_name";
	    }
	} else {
	    return @types;
	}
    }
%}
