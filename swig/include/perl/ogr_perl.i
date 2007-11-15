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

%import destroy.i

ALTERED_DESTROY(OGRDataSourceShadow, OGRc, delete_DataSource)
ALTERED_DESTROY(OGRFeatureShadow, OGRc, delete_Feature)
ALTERED_DESTROY(OGRFeatureDefnShadow, OGRc, delete_FeatureDefn)
ALTERED_DESTROY(OGRFieldDefnShadow, OGRc, delete_FieldDefn)
ALTERED_DESTROY(OGRGeometryShadow, OGRc, delete_Geometry)

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
	use Carp;
	use vars qw /%TYPE_STRING2INT %TYPE_INT2STRING/;
	for my $string ('Unknown', 'Point', 'LineString', 'Polygon',
			'MultiPoint', 'MultiLineString', 'MultiPolygon',
			'GeometryCollection', 'None', 'LinearRing',
			'Point25D', 'LineString25D', 'Polygon25D',
			'MultiPoint25D', 'MultiLineString25D', 'MultiPolygon25D',
			'GeometryCollection25D') {
	    my $int = eval "\$Geo::OGR::wkb$string";
	    $TYPE_STRING2INT{$string} = $int;
	    $TYPE_INT2STRING{$int} = $string;
	}
	sub create { # alternative constructor since swig created new can't be overridden(?)
	    my $pkg = shift;
	    my($type, $wkt, $wkb, $gml);
	    if (@_ == 1) {
		$type = shift;
	    } else {
		my %param = @_;
		$wkt = $param{type};
		$wkt = ($param{wkt} or $param{WKT});
		$wkb = ($param{wkb} or $param{WKB});
		$gml = ($param{gml} or $param{GML});
	    }
	    $type = $TYPE_STRING2INT{$type} if exists $TYPE_STRING2INT{$type};
	    my $self = Geo::OGRc::new_Geometry($type, $wkt, $wkb, $gml);
	    bless $self, $pkg if defined $self;
	}
	sub GeometryType {
	    my $self = shift;
	    return $TYPE_INT2STRING{$self->GetGeometryType};
	}
	sub AddPoint {
	    @_ == 4 ? AddPoint_3D(@_) : AddPoint_2D(@_);
	}
    }
    sub GeometryType {
	my($type_or_name) = @_;
	if (defined $type_or_name) {
	    return $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name} 
	    if exists $Geo::OGR::Geometry::TYPE_STRING2INT{$type_or_name};
	    return $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name} 
	    if exists $Geo::OGR::Geometry::TYPE_INT2STRING{$type_or_name};
	    croak "unknown geometry type or name: $type_or_name";
	} else {
	    return keys %Geo::OGR::Geometry::TYPE_STRING2INT;
	}
    }
%}
