<xsl:transform version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns:od="urn:schemas-microsoft-com:officedata">

 <xsl:template name="string-replace-all">
    <xsl:param name="text" />
    <xsl:param name="replace" />
    <xsl:param name="by" />
    <xsl:choose>
      <xsl:when test="contains($text, $replace)">
        <xsl:value-of select="substring-before($text,$replace)" />
        <xsl:value-of select="$by" />
        <xsl:value-of select="substring-after($text,$replace)" />
<!--        <xsl:call-template name="string-replace-all">
          <xsl:with-param name="text"
          select="substring-after($text,$replace)" />
          <xsl:with-param name="replace" select="$replace" />
          <xsl:with-param name="by" select="$by" />
        </xsl:call-template>-->
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$text" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- yay xslt 1.0 -->
  <xsl:template name="unnorsk">
    <xsl:param name="text" />
    <xsl:variable name="aa">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$text" />
      <xsl:with-param name="replace" select="'å'" />
      <xsl:with-param name="by" select="'aa'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="Aa">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$aa" />
      <xsl:with-param name="replace" select="'Å'" />
      <xsl:with-param name="by" select="'Aa'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="ae">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$Aa" />
      <xsl:with-param name="replace" select="'æ'" />
      <xsl:with-param name="by" select="'ae'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="Ae">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$ae" />
      <xsl:with-param name="replace" select="'Æ'" />
      <xsl:with-param name="by" select="'Ae'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="oe">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$Ae" />
      <xsl:with-param name="replace" select="'ø'" />
      <xsl:with-param name="by" select="'oe'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="Oe">
    <xsl:call-template name="string-replace-all">
      <xsl:with-param name="text" select="$oe" />
      <xsl:with-param name="replace" select="'Ø'" />
      <xsl:with-param name="by" select="'Oe'" />
    </xsl:call-template>
    </xsl:variable>
    <xsl:value-of select="$Oe" />
  </xsl:template>
</xsl:transform>
