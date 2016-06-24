<xsl:transform version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns:od="urn:schemas-microsoft-com:officedata">

<xsl:variable name="and"><![CDATA[&]]></xsl:variable>

<xsl:template match="/dataroot">
  <xsl:for-each select="Elementdefinisjoner">
    <xsl:if test="verditype!='*'">
    <xsl:if test="verditype!='REF'">
      addSimpleType(<xsl:value-of select="$and" disable-output-escaping="yes"/>oTypes, "<xsl:value-of select="elementnavn"/>", "<xsl:value-of select="logisk_navn"/>", <xsl:choose>
        <xsl:when test="verditype='T'">OFTString</xsl:when>
        <xsl:when test="verditype='H'">OFTInteger</xsl:when>
        <xsl:when test="verditype='D'">OFTReal</xsl:when>
        <xsl:when test="verditype='DATO'">OFTDate</xsl:when>
        <xsl:when test="verditype='DATOTID'">OFTDateTime</xsl:when>
        <xsl:when test="verditype='BOOLSK'">OFTString</xsl:when><!--TODO-->
      </xsl:choose>);
    </xsl:if>
    </xsl:if>
  </xsl:for-each>
</xsl:template>

</xsl:transform>

