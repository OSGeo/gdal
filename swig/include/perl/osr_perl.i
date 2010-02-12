%include cpl_exceptions.i
%import typemaps_perl.i

%rename (_TransformPoints) TransformPoints;

%perlcode %{
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
