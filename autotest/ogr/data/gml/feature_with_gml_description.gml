<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     gml:id="aFeatureCollection"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ feature_with_gml_description.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml/3.2">
  <gml:description>toplevel description</gml:description>
  <gml:identifier codeSpace="ignored">toplevel identifier</gml:identifier>
  <gml:name>toplevel name</gml:name>
  <ogr:featureMember>
    <ogr:foo gml:id="foo.0">
      <gml:description>gml_description</gml:description>
      <gml:identifier codeSpace="ignored">gml_identifier</gml:identifier>
      <gml:name>gml_name</gml:name>
      <ogr:bar>1</ogr:bar>
      <!-- would be ignored currently -->
      <!-- <ogr:description>ogr_description</ogr:description> -->
    </ogr:foo>
  </ogr:featureMember>
</ogr:FeatureCollection>
