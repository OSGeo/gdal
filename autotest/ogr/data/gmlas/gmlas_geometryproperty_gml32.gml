<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     gml:id="aFeatureCollection"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ gmlas_geometryproperty_gml32.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://fake_gml32">
  <ogr:featureMember>
    <ogr:test gml:id="poly.0">
      <ogr:geometryProperty> <gml:Point gml:id="poly.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::4326"> <gml:pos>49 2</gml:pos> </gml:Point> </ogr:geometryProperty>
      <ogr:geometryPropertyEmpty xsi:nil="true"/>
      <ogr:pointProperty><gml:Point gml:id="poly.geom.Point"><gml:pos srsName="http://www.opengis.net/def/crs/EPSG/0/4326">50 3</gml:pos></gml:Point></ogr:pointProperty>
      <ogr:lineStringProperty><gml:LineString gml:id="poly.geom.LineString" srsName="EPSG:4326"><gml:pos>2 49</gml:pos></gml:LineString></ogr:lineStringProperty>
      <ogr:polygonProperty><gml:Polygon gml:id="poly.geom.Polygon"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></ogr:polygonProperty>
      <ogr:multiPointProperty><gml:MultiPoint gml:id="poly.geom.MultiPoint"><gml:pointMember><gml:Point gml:id="poly.geom.MultiPoint.Point.1"><gml:pos>1.0 1.0</gml:pos></gml:Point></gml:pointMember><gml:pointMember><gml:Point gml:id="poly.geom.MultiPoint.Point.2"><gml:pos>2.0 2.0</gml:pos></gml:Point></gml:pointMember></gml:MultiPoint></ogr:multiPointProperty>
      <ogr:multiLineStringProperty><gml:MultiLineString gml:id="poly.geom.MultiLineString"><gml:lineStringMember><gml:LineString gml:id="poly.geom.MultiLineString.LineString"><gml:pos>1.0 1.0</gml:pos></gml:LineString></gml:lineStringMember></gml:MultiLineString></ogr:multiLineStringProperty>
      <ogr:multiPolygonProperty><gml:MultiPolygon gml:id="poly.geom.MultiPolygon"><gml:polygonMember><gml:Polygon gml:id="poly.geom.MultiPolygon.Polygon"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:polygonMember></gml:MultiPolygon></ogr:multiPolygonProperty>
      <ogr:multiGeometryProperty><gml:MultiGeometry gml:id="poly.geom.MultiGeometry"><gml:geometryMember><gml:Polygon gml:id="poly.geom.MultiGeometry.Polygon"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:geometryMember></gml:MultiGeometry></ogr:multiGeometryProperty>
      <ogr:multiCurveProperty><gml:MultiCurve gml:id="poly.geom.MultiCurve"><gml:curveMember><gml:LineString gml:id="poly.geom.MultiCurve.LineString"><gml:pos>1.0 1.0</gml:pos></gml:LineString></gml:curveMember></gml:MultiCurve></ogr:multiCurveProperty>
      <ogr:multiSurfaceProperty><gml:MultiSurface gml:id="poly.geom.MultiSurface"><gml:surfaceMember><gml:Polygon gml:id="poly.geom.MultiSurface.Polygon"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface></ogr:multiSurfaceProperty>
      <ogr:curveProperty><gml:LineString gml:id="poly.geom.Curve"><gml:pos>1.0 1.0</gml:pos></gml:LineString></ogr:curveProperty>
      <ogr:surfaceProperty><gml:Polygon gml:id="poly.geom.Surface"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></ogr:surfaceProperty>
      <ogr:pointPropertyRepeated><gml:Point gml:id="poly.geom.pointPropertyRepeated.1"><gml:pos>0 1</gml:pos></gml:Point></ogr:pointPropertyRepeated>
      <ogr:pointPropertyRepeated><gml:Point gml:id="poly.geom.pointPropertyRepeated.2"><gml:pos>1 2</gml:pos></gml:Point></ogr:pointPropertyRepeated>
      <ogr:pointPropertyRepeated><gml:Point gml:id="poly.geom.pointPropertyRepeated.3"><gml:pos>3 4</gml:pos></gml:Point></ogr:pointPropertyRepeated>
      <ogr:myCustomPointProperty><gml:Point gml:id="poly.geom.myCustomPointProperty"><gml:pos>5 6</gml:pos></gml:Point></ogr:myCustomPointProperty>
    </ogr:test>
  </ogr:featureMember>
  <ogr:featureMember>
    <ogr:test gml:id="poly.1">
      <ogr:geometryProperty><gml:Point gml:id="poly.1.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::32631"><gml:pos>500000 0</gml:pos></gml:Point></ogr:geometryProperty>
    </ogr:test>
  </ogr:featureMember>
  <ogr:featureMember>
    <ogr:test gml:id="poly.2">
      <ogr:geometryProperty><gml:Point gml:id="poly.2.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::i_dont_exist"><gml:pos>500000 0</gml:pos></gml:Point></ogr:geometryProperty>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>
