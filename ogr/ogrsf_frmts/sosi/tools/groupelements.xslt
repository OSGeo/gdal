<xsl:transform version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns:od="urn:schemas-microsoft-com:officedata">

<xsl:include href="unnorsk.xslt" />

<xsl:variable name="and"><![CDATA[&]]></xsl:variable>
<xsl:key name="groups" match="//gruppe_element" use="@first"/>

<xsl:template match="/dataroot">

  <xsl:for-each select="Gruppeelement_sammensetning">
   <xsl:variable name="datatyp"><xsl:value-of select="datatypenavn" /></xsl:variable>
   <xsl:variable name="dtnsafe"><xsl:call-template name="unnorsk"><xsl:with-param name="text" select="gruppe_element"/></xsl:call-template></xsl:variable>
   <xsl:variable name="maxrf">
     <xsl:for-each select="//Gruppeelement_sammensetning[datatypenavn=$datatyp]/rekkefølge">
       <xsl:sort data-type="number" order="descending"/>
       <xsl:if test="position()=1"><xsl:value-of select="."/></xsl:if>
     </xsl:for-each>
   </xsl:variable>
   <xsl:if test="./datatypenavn">
     <xsl:if test="rekkefølge=1">
OGRSOSIDataType <xsl:value-of select="$dtnsafe" />Type = OGRSOSIDataType(<xsl:value-of select="$maxrf" />);
     </xsl:if>
   </xsl:if>
  </xsl:for-each>

  <xsl:for-each select="Gruppeelement_sammensetning">
   <xsl:variable name="elemtyp"><xsl:value-of select="basis_element" /></xsl:variable>
   <xsl:variable name="dtnsafe"><xsl:call-template name="unnorsk"><xsl:with-param name="text" select="gruppe_element"/></xsl:call-template></xsl:variable>
   <xsl:variable name="oft">
    <xsl:for-each select="//Elementdefinisjoner[elementnavn=$elemtyp]">
      <xsl:choose>
        <xsl:when test="verditype='T'">OFTString</xsl:when>
        <xsl:when test="verditype='H'">OFTInteger</xsl:when>
        <xsl:when test="verditype='D'">OFTReal</xsl:when>
        <xsl:when test="verditype='DATO'">OFTDate</xsl:when>
        <xsl:when test="verditype='DATOTID'">OFTDateTime</xsl:when>
        <xsl:when test="verditype='BOOLSK'">OFTString</xsl:when>
        <xsl:otherwise>OFTString</xsl:otherwise> <!-- DEBUG: Nested complex type -->
      </xsl:choose>
    </xsl:for-each>
   </xsl:variable>

   <xsl:if test="./datatypenavn">
<xsl:value-of select="$dtnsafe" />Type.setElement(<xsl:value-of select="rekkefølge -1" />, "<xsl:value-of select="egenskapsnavn" />", <xsl:value-of select="$oft" />);
   </xsl:if>
  </xsl:for-each>

  <xsl:for-each select="Gruppeelement_sammensetning">
   <xsl:variable name="dtnsafe"><xsl:call-template name="unnorsk"><xsl:with-param name="text" select="gruppe_element"/></xsl:call-template></xsl:variable>
   <xsl:if test="./datatypenavn">
     <xsl:if test="rekkefølge=1">
addType(<xsl:value-of select="$and" disable-output-escaping="yes"/>oTypes, "<xsl:value-of select="gruppe_element" />", <xsl:value-of select="$and" disable-output-escaping="yes"/><xsl:value-of select="$dtnsafe"/>Type);
     </xsl:if>
   </xsl:if>
  </xsl:for-each>

</xsl:template>

</xsl:transform>

