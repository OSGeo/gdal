<?xml version="1.0" encoding="UTF-8"?>
<app:FeatureCollection xmlns:app="http://www.safe.com/gml2" xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.safe.com/gml2 031d12_1_0.xsd">

<!-- Demonstrates the behaviour in GDAL ticket #2349.
     A crash can produced on GDAL 1.5.1 using the ogrinfo sample application
	 with the following command line:

	 $ ogrinfo -ro ticket_2349_test_1.gml MyPolyline -where 'height > 300'

-->

<gml:featureMember><app:MyPolyline>
	<gml:name>My Polyline Rectangle</gml:name>
	<app:height>101</app:height>
	<app:extentLineString>
		<gml:lineStringProperty><gml:LineString>
			<gml:coordinates>-45,+60 +45,+60 +45,-60, -45,-60</gml:coordinates>
		</gml:LineString></gml:lineStringProperty>
	</app:extentLineString>
</app:MyPolyline></gml:featureMember>

<gml:featureMember><app:MyPoint>
	<gml:name>My Point</gml:name>
	<app:height>201</app:height>
		<gml:pointProperty>
			<gml:Point>
				<gml:coordinates>-40,+55</gml:coordinates>
			</gml:Point>
		</gml:pointProperty>
</app:MyPoint></gml:featureMember>

</app:FeatureCollection>
