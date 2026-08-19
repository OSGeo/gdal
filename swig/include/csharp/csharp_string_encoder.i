/******************************************************************************
 *
 * Name:     csharp_string_encoder.i
 * Project:  GDAL CSharp Interface
 * Purpose:  String encoder and decoder for C# marshalling
 * Author:   Michael Bucari-Tovo, mbucari1@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2026, Michael Bucari-Tovo
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

/*
 * The default string encoderdecoder implementation used
 * by all GDAL modules. Should be imported only by OSR.
 */

%{
typedef struct {} DefaultStringEncoder;
%}
%typemap(csclassmodifiers) DefaultStringEncoder "public class";
%typemap(csinterfaces)     DefaultStringEncoder "IStringEncoder";
%typemap(csdisposing)      DefaultStringEncoder "";
%typemap(csdispose)        DefaultStringEncoder "";
%typemap(csimports)        DefaultStringEncoder %{

  using System;
  using System.Text;
  using System.Runtime.InteropServices;

  /* Interface for encoding/decoding string to/from Gdal unmanaged strings. */
  public interface IStringEncoder {
    /* Encode a string to a null-terminated array of bytes to be sent to Gdal unmanaged */
    byte[] ToNullTerminated(string str);
    /* Decode an unmanaged, null-terminated string from Gdal to a managed string */
    string FromNullTerminated(IntPtr pStr);
  }
%}

%typemap(csbody)           DefaultStringEncoder %{
  public virtual string FromNullTerminated(IntPtr pStr) {
#if NETCOREAPP1_1_OR_GREATER || NETSTANDARD2_1_OR_GREATER
    return Marshal.PtrToStringUTF8(pStr);
#else
    if (pStr == IntPtr.Zero) return null;
    unsafe {
      byte* pBytes = (byte*)pStr.ToPointer();
      int len = 0;
      checked {
        while (pBytes[len] != 0) len++;
      }
      return Encoding.UTF8.GetString(pBytes, len);
    }
#endif
  }
  public virtual byte[] ToNullTerminated(string str) {
    if (str == null) return null;
    int byteCount = Encoding.UTF8.GetByteCount(str);
    var bts = new byte[byteCount + 1];
    Encoding.UTF8.GetBytes(str, 0, str.Length, bts, 0);
    return bts;
  }
%}

%ignore DefaultStringEncoder::DefaultStringEncoder();
struct  DefaultStringEncoder{};
