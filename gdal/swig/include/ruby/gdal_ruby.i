/*
 * $Id$
 *
 * ruby specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.2  2005/09/26 08:18:55  cfis
 * Copied over code from the Python version of gdal_ruby.i.  Will have to port the code to Ruby.
 *
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accommodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */


%init %{
  /* gdal_ruby.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%include "cpl_exceptions.i";


%header %{

static CPLXMLNode *RubyArrayToXMLTree(VALUE rubyArray)
{
    int      nChildCount = 0, iChild, nType;
    CPLXMLNode *psThisNode;
    CPLXMLNode *psChild;
    char       *pszText = NULL;

    nChildCount = RARRAY_LEN(rubyArray) - 2;
    if( nChildCount < 0 )
    {
		 rb_raise(rb_eRuntimeError, "Error in input XMLTree, child count is less than zero.");
    }

	 VALUE item1 = rb_ary_entry(rubyArray, 0);
	 nType = NUM2INT(item1);

	 VALUE item2 = rb_ary_entry(rubyArray, 1);
	 pszText = StringValuePtr(item2);

    psThisNode = CPLCreateXMLNode( NULL, (CPLXMLNodeType) nType, pszText );

    for( iChild = 0; iChild < nChildCount; iChild++ )
    {
        psChild = RubyArrayToXMLTree( rb_ary_entry(rubyArray,iChild+2) );
        CPLAddXMLChild( psThisNode, psChild );
    }

    return psThisNode;
}

static VALUE XMLTreeToRubyArray( CPLXMLNode *psTree )
{
    int      nChildCount = 0, iChild;
    CPLXMLNode *psChild;

    for( psChild = psTree->psChild; 
         psChild != NULL; 
         psChild = psChild->psNext )
        nChildCount++;

    VALUE rubyArray = rb_ary_new2(nChildCount+2);

	 rb_ary_store(rubyArray, 0, INT2NUM((int) psTree->eType));
	 rb_ary_store(rubyArray, 1, rb_str_new2(psTree->pszValue));

    for( psChild = psTree->psChild, iChild = 2; 
         psChild != NULL; 
         psChild = psChild->psNext, iChild++ )
    {
        rb_ary_store(rubyArray, iChild, XMLTreeToRubyArray(psChild));
    }

    return rubyArray; 
}
%}


/*%extend GDAL_GCP {
%pythoncode {
  def __str__(self):
    str = '%s (%.2fP,%.2fL) -> (%.7fE,%.7fN,%.2f) %s '\
          % (self.Id, self.GCPPixel, self.GCPLine,
             self.GCPX, self.GCPY, self.GCPZ, self.Info )
    return str
    def serialize(self,with_Z=0):
        base = [CXT_Element,'GCP']
        base.append([CXT_Attribute,'Id',[CXT_Text,self.Id]])
        pixval = '%0.15E' % self.GCPPixel       
        lineval = '%0.15E' % self.GCPLine
        xval = '%0.15E' % self.GCPX
        yval = '%0.15E' % self.GCPY
        zval = '%0.15E' % self.GCPZ
        base.append([CXT_Attribute,'Pixel',[CXT_Text,pixval]])
        base.append([CXT_Attribute,'Line',[CXT_Text,lineval]])
        base.append([CXT_Attribute,'X',[CXT_Text,xval]])
        base.append([CXT_Attribute,'Y',[CXT_Text,yval]])
        if with_Z:
            base.append([CXT_Attribute,'Z',[CXT_Text,zval]])        
        return base
} 
}

%extend GDALRasterBandShadow {
%pythoncode {
  def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_obj=None):
      import gdalnumeric

      return gdalnumeric.BandReadAsArray( self, xoff, yoff,
                                          win_xsize, win_ysize,
                                          buf_xsize, buf_ysize, buf_obj )
    
  def WriteArray(self, array, xoff=0, yoff=0):
      import gdalnumeric

      return gdalnumeric.BandWriteArray( self, array, xoff, yoff )

}
}

%extend GDALMajorObjectShadow {
%pythoncode {
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
}
}
*/

%import typemaps_ruby.i

