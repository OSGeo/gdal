/*
 * $Id$
 */

/*
 * $Log$
 * Revision 1.4  2006/11/23 22:50:53  tamas
 * C# ExportToWkb support
 *
 * Revision 1.3  2006/09/07 10:26:31  tamas
 * Added default exception support
 *
 * Revision 1.2  2005/09/06 01:51:04  kruland
 * Removed GetDriverByName, GetDriver, Open, OpenShared because they are defined
 * in ogr now.
 *
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */
 
%include cpl_exceptions.i

%rename (GetFieldType) GetType;
%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%include typemaps_csharp.i


%typemap(cscode, noblock="1") OGRGeometryShadow {
  public int ExportToWkb( byte[] buffer, int byte_order ) {
      int retval;
      int size = WkbSize();
      if (buffer.Length < size)
        throw new ArgumentException("Buffer size is small (ExportToWkb)");
        
      IntPtr ptr = Marshal.AllocHGlobal(size * Marshal.SizeOf(buffer[0]));
      try {
          retval = ExportToWkb(size, ptr, byte_order);
          Marshal.Copy(ptr, buffer, 0, size);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      GC.KeepAlive(this);
      return retval;
  }
  public int ExportToWkb( byte[] buffer ) {
      return ExportToWkb( buffer, ogr.wkbXDR);
  }
}