<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     gml:id="aFeatureCollection"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ multiple_geometry_fields_srs_detection.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml/3.2">
  <ogr:featureMember>
    <ogr:test gml:id="test.0">
      <ogr:geom2><gml:Point srsName="EPSG:32631" gml:id="test.geom2.0"><gml:pos>1 2</gml:pos></gml:Point></ogr:geom2>
      <ogr:geom_mixed_srs><gml:Point srsName="EPSG:32631" gml:id="test.geom_mixed_srs.0"><gml:pos>1 2</gml:pos></gml:Point></ogr:geom_mixed_srs>
    </ogr:test>
  </ogr:featureMember>
  <ogr:featureMember>
    <ogr:test gml:id="test.1">
      <ogr:geom2><gml:Point srsName="EPSG:32631" gml:id="test.geom2.1"><gml:pos>1 2</gml:pos></gml:Point></ogr:geom2>
      <ogr:geom3><gml:Point srsName="EPSG:32632" gml:id="test.geom3.1"><gml:pos>3 4</gml:pos></gml:Point></ogr:geom3>
      <ogr:geom_mixed_srs><gml:Point srsName="EPSG:32633" gml:id="test.geom_mixed_srs.1"><gml:pos>1 2</gml:pos></gml:Point></ogr:geom_mixed_srs>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>
