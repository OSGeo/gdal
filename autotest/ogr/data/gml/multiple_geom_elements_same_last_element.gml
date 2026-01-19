<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     gml:id="aFeatureCollection"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ test.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml/3.2">
  <ogr:featureMember>
    <ogr:test gml:id="test.0">
      <ogr:firstGeom><gml:Point><gml:posList>1 2</gml:posList></gml:Point></ogr:firstGeom>
      <ogr:secondGeom><gml:Point><gml:posList>3 4</gml:posList></gml:Point></ogr:secondGeom>
    </ogr:test>
  </ogr:featureMember>
  <ogr:featureMember>
    <ogr:test gml:id="test.1">
      <ogr:otherGeom><gml:Point><gml:posList>5 6</gml:posList></gml:Point></ogr:otherGeom>
      <ogr:secondGeom><gml:Point><gml:posList>7 8</gml:posList></gml:Point></ogr:secondGeom>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>
