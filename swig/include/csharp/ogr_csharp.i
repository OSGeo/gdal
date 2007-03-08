/*
 * $Id$
 */
 
%include cpl_exceptions.i

%rename (GetFieldType) GetType;
%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%include typemaps_csharp.i


%typemap(cscode, noblock="1") OGRGeometryShadow {
  public int ExportToWkb( byte[] buffer, wkbByteOrder byte_order ) {
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
      return ExportToWkb( buffer, wkbByteOrder.wkbXDR);
  }
}