<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     gml:id="aFeatureCollection"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ datetime.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml/3.2">
  <ogr:featureMember>
    <ogr:test gml:id="test.0">
      <ogr:time>23:59:60</ogr:time>
      <ogr:date>9999-12-31</ogr:date>
      <ogr:datetime>9999-12-31T23:59:60.999</ogr:datetime>
      <ogr:dtInTimePosition><gml:timePosition>9999-12-31T23:59:60.999</gml:timePosition></ogr:dtInTimePosition>
    </ogr:test>
  </ogr:featureMember>
  <ogr:featureMember>
    <ogr:test gml:id="test.1">
      <ogr:time>23:59:60.999</ogr:time>
      <ogr:date>9999-12-31</ogr:date>
      <ogr:datetime>9999-12-31T23:59:60Z</ogr:datetime>
      <ogr:dtInTimePosition><gml:timePosition>9999-12-31T23:59:60+12:30</gml:timePosition></ogr:dtInTimePosition>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>
