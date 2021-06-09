<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>
<qgis styleCategories="AllStyleCategories" maxScale="0" version="3.18.0-Zürich" hasScaleBasedVisibilityFlag="0" minScale="1e+08">
  <flags>
    <Identifiable>1</Identifiable>
    <Removable>1</Removable>
    <Searchable>1</Searchable>
    <Private>0</Private>
  </flags>
  <temporal fetchMode="0" enabled="0" mode="0">
    <fixedRange>
      <start></start>
      <end></end>
    </fixedRange>
  </temporal>
  <customproperties>
    <property value="false" key="WMSBackgroundLayer"/>
    <property value="false" key="WMSPublishDataSourceUrl"/>
    <property value="0" key="embeddedWidgets/count"/>
    <property value="Value" key="identify/format"/>
  </customproperties>
  <pipe>
    <provider>
      <resampling zoomedInResamplingMethod="nearestNeighbour" maxOversampling="2" enabled="false" zoomedOutResamplingMethod="nearestNeighbour"/>
    </provider>
    <rasterrenderer type="singlebandpseudocolor" opacity="1" alphaBand="-1" band="1" classificationMax="100" classificationMin="0" nodataColor="">
      <rasterTransparency/>
      <minMaxOrigin>
        <limits>None</limits>
        <extent>WholeRaster</extent>
        <statAccuracy>Estimated</statAccuracy>
        <cumulativeCutLower>0.02</cumulativeCutLower>
        <cumulativeCutUpper>0.98</cumulativeCutUpper>
        <stdDevFactor>2</stdDevFactor>
      </minMaxOrigin>
      <rastershader>
        <colorrampshader classificationMode="1" minimumValue="0" maximumValue="100" clip="0" labelPrecision="4" colorRampType="INTERPOLATED">
          <colorramp type="gradient" name="[source]">
            <Option type="Map">
              <Option value="215,25,28,255" type="QString" name="color1"/>
              <Option value="43,131,186,255" type="QString" name="color2"/>
              <Option value="0" type="QString" name="discrete"/>
              <Option value="gradient" type="QString" name="rampType"/>
              <Option value="0.25;255,255,191,255:0.5;253,174,97,255:0.75;171,221,164,255" type="QString" name="stops"/>
            </Option>
            <prop k="color1" v="215,25,28,255"/>
            <prop k="color2" v="43,131,186,255"/>
            <prop k="discrete" v="0"/>
            <prop k="rampType" v="gradient"/>
            <prop k="stops" v="0.25;255,255,191,255:0.5;253,174,97,255:0.75;171,221,164,255"/>
          </colorramp>
          <item value="0" label="0.0000" color="#d7191c" alpha="255"/>
          <item value="25" label="25.0000" color="#ffffbf" alpha="255"/>
          <item value="50" label="50.0000" color="#fdae61" alpha="255"/>
          <item value="75" label="75.0000" color="#abdda4" alpha="255"/>
          <item value="100" label="100.0000" color="#2b83ba" alpha="255"/>
          <rampLegendSettings prefix="" direction="0" orientation="2" minimumLabel="" maximumLabel="" suffix="">
            <numericFormat id="basic">
              <Option type="Map">
                <Option value="" type="QChar" name="decimal_separator"/>
                <Option value="6" type="int" name="decimals"/>
                <Option value="0" type="int" name="rounding_type"/>
                <Option value="false" type="bool" name="show_plus"/>
                <Option value="true" type="bool" name="show_thousand_separator"/>
                <Option value="false" type="bool" name="show_trailing_zeros"/>
                <Option value="" type="QChar" name="thousand_separator"/>
              </Option>
            </numericFormat>
          </rampLegendSettings>
        </colorrampshader>
      </rastershader>
    </rasterrenderer>
    <brightnesscontrast brightness="0" gamma="1" contrast="0"/>
    <huesaturation colorizeRed="255" saturation="0" colorizeOn="0" colorizeStrength="100" colorizeGreen="128" colorizeBlue="128" grayscaleMode="0"/>
    <rasterresampler maxOversampling="2"/>
    <resamplingStage>resamplingFilter</resamplingStage>
  </pipe>
  <blendMode>0</blendMode>
</qgis>
