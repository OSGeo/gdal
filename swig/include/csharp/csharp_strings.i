/******************************************************************************
 *
 * Name:     csharp_strings.i
 * Project:  GDAL CSharp Interface
 * Purpose:  Typemaps for C# marshalling to/from UTF-8 strings and string lists
 * Authors:  Tamas Szekeres, szekerest@gmail.com
 *           Michael Bucari-Tovo, mbucari1@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres; 2026, Michael Bucari-Tovo
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

/******************************************************************************
 * Marshaller for NULL terminated UTF-8 encoded strings                       *
 * The managed Utf8StringHelper registers a callback function with the native *
 * runtime which is used by the native runtime to create .NET strings. When   *
 * called by the native runtime, the .NET function decodes the unmanaged      *
 * memory into a managed string and returns a new pinned GCHandle for that    *
 * string. A pointer to that GCHandle is returned. When the callback returns  *
 * to managed .NET, the managed string is retrieved from the GCHandle and the *
 * GCHandle is freed by calling StringFromPinnedGCHandle.                     *
 *                                                                            *
 * IMPORTANT:                                                                 *
 * Every call to SWIG_csharp_string_callback MUST be followed by exactly      *
 * one call to StringFromPinnedGCHandle() on the returned pointer.            *
 * Failure to do so will result in memory leaks or double frees.              *
 *****************************************************************************/

%insert(runtime) %{
/* Callback for returning strings to C# without leaking memory */
typedef char * (SWIGSTDCALL* CSharpUtf8StringHelperCallback)(const char *);
static CSharpUtf8StringHelperCallback SWIG_csharp_string_callback = NULL;
%}

/*
 * Define a dummy type so that we can extend it to add custom types
 * inside the module namespace using the csimports typemap
 */
%{
typedef struct {} CsharpDummyObject;
%}
%typemap(csclassmodifiers) CsharpDummyObject "internal class";
%typemap(csimports) CsharpDummyObject %{
  using System;
  using System.Text;
  using System.Runtime.InteropServices;

  /* Interface for encoding/decoding string to/from Gdal unmanaged strings. */
  public interface I$moduleStringEncoder {
    /* Encode a string to a null-terminated array of bytes to be sent to Gdal unmanaged */
    byte[] ToNullTerminated(string str);
    /* Decode an unmanaged, null-terminated string from Gdal to a managed string */
    string FromNullTerminated(IntPtr pStr);
  }
  
  public class Default$moduleStringEncoder : I$moduleStringEncoder {
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
  }
%}
struct CsharpDummyObject{};

%pragma(csharp) modulecode=%{
  internal static readonly I$moduleStringEncoder s_DefaultStringEncoder = new Default$moduleStringEncoder();
  internal static I$moduleStringEncoder s_StringEncoder;
  public static I$moduleStringEncoder StringEncoder {
    get {
      if (s_StringEncoder == null)
        s_StringEncoder = s_DefaultStringEncoder;
      return s_StringEncoder;
    }
    set {
      s_StringEncoder = value;
    }
  }
%}

%pragma(csharp) imclasscode=%{
  public class Utf8StringHelper {

    public delegate System.IntPtr Utf8StringDelegate(System.IntPtr message);

    static Utf8StringDelegate stringDelegate = new Utf8StringDelegate(DecodeStringToPinnedGCHandle);

    [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="RegisterUtf8StringCallback_$module")]
    public static extern void RegisterUtf8StringCallback_$module(Utf8StringDelegate stringDelegate);

    static Utf8StringHelper() {
      RegisterUtf8StringCallback_$module(stringDelegate);
    }

    public static IntPtr DecodeStringToPinnedGCHandle(IntPtr pUtf8Bts) {
      string value = $module.StringEncoder?.FromNullTerminated(pUtf8Bts);
      if (value == null)
        return IntPtr.Zero;
      var handle = System.Runtime.InteropServices.GCHandle.Alloc(value,
                     System.Runtime.InteropServices.GCHandleType.Pinned);
      return System.Runtime.InteropServices.GCHandle.ToIntPtr(handle);
    }
  }

  /* Instantiate the helper class so that the static constructor executes */
  public static readonly Utf8StringHelper utf8StringHelper = new Utf8StringHelper();

  internal static string StringFromPinnedGCHandle(IntPtr pinnedHandle){
    if (pinnedHandle == IntPtr.Zero)
      return null;
    var handle = System.Runtime.InteropServices.GCHandle.FromIntPtr(pinnedHandle);
    string value = handle.Target as string;
    handle.Free();
    return value;
  }
%}

%insert(runtime) %{
#ifdef __cplusplus
extern "C"
#endif
SWIGEXPORT void SWIGSTDCALL RegisterUtf8StringCallback_$module(CSharpUtf8StringHelperCallback callback) {
  SWIG_csharp_string_callback = callback;
}
%}

/*****************************************************************************
 * Apply typemaps for all SWIG string types and for const char *utf8_string  *
 ****************************************************************************/

%typemap(cstype) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) "string"
%typemap(imtype, out="IntPtr") (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) "byte[]"
%typemap(in) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{ $1 = ($1_ltype)$input; %}
%typemap(out) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{
 /*
  * %typemap(out) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string)
  * GCHandle is released by %typemap(csout) or %typemap(csvarout) of same
  */
  $result = SWIG_csharp_string_callback((const char *)$1);
%}

%typemap(csin) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string)
  "$module.StringEncoder?.ToNullTerminated($csinput)"
  
%define CS_RETURN_UTF8_STRING
  IntPtr cPtr = $imcall;
  string ret = $modulePINVOKE.StringFromPinnedGCHandle(cPtr);
  $excode
  return ret;
%enddef

%typemap(csout, excode=SWIGEXCODE) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) {
  CS_RETURN_UTF8_STRING
}

/*
 * Typemap for UTF-8 char* string properties.
 */

%typemap(csvarout, noblock=1, excode=SWIGEXCODE2) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) {
  get {
    CS_RETURN_UTF8_STRING
  }
}

/*
 * Typemaps for  (retStringAndCPLFree*)
 */

%typemap(out) (retStringAndCPLFree*) %{
 /*
  * %typemap(out) (retStringAndCPLFree*)
  * GCHandle is released by %typemap(csout) (char *)
  */
  if($1) {
    $result = SWIG_csharp_string_callback((const char *)$1);
    CPLFree($1);
  } else {
    $result = NULL;
  }
%}

/*
 * Typemap for char **argout. Used for 'out string' parameters.
 */

%typemap(imtype) (char** argout), (char **username), (char **usrname), (char **type) "ref IntPtr"
%typemap(cstype) (char** argout), (char **username), (char **usrname), (char **type) "out string"
%typemap(in) (char **argout), (char **username), (char **usrname), (char **type) %{ $1 = ($1_ltype)$input; %}
%typemap(csin, cshin="out $csinput",
  pre="    IntPtr temp$csinput = IntPtr.Zero;",
  post="    $csinput = $modulePINVOKE.StringFromPinnedGCHandle(temp$csinput);")
  (char** argout), (char **username), (char **usrname), (char **type)
  "ref temp$csinput"

%typemap(argout) (char **argout) {
 /*
  * %typemap(argout) (char **argout)
  * GCHandle is released by %typemap(csin)
  */
  char* temp_string;
  temp_string = SWIG_csharp_string_callback(*$1);
  if (*$1)
    CPLFree(*$1);
  *$1 = temp_string;
}
%typemap(argout) (char **username), (char **usrname), (char **type) {
 /*
  * %typemap(argout) (char **username), (char **usrname), (char **type)
  * GCHandle is released by %typemap(csin)
  */
  *$1 = SWIG_csharp_string_callback(*$1);
}

/*
 * Typemap for char **ignorechange. Used for 'ref string' parameters where
 * caller doesn't want to see changes.
 */

%typemap(imtype) (char **ignorechange) "byte[]"
%typemap(cstype) (char **ignorechange) "ref string"
%typemap(in)     (char **ignorechange) %{ $1 = ($1_ltype)&$input; %}
%typemap(csin)   (char **ignorechange) "$module.StringEncoder?.ToNullTerminated($csinput)"

/******************************************************************************
 * Marshaller for NULL terminated lists of NULL terminated UTF-8 strings.     *
 *****************************************************************************/

%pragma(csharp) imclasscode=%{
  public class StringListMarshal : IDisposable {
    public readonly IntPtr[] _ar;
    private readonly System.Runtime.InteropServices.GCHandle[] _handles;
    private int _isDisposed;
    public StringListMarshal(string[] ar) {
      if (ar == null)
        return;
      _handles = new System.Runtime.InteropServices.GCHandle[ar.Length];
      _ar = new IntPtr[ar.Length + 1];
      for (int cx = 0; cx < ar.Length; cx++) {
        byte[] bts = $module.StringEncoder?.ToNullTerminated(ar[cx]);
        _handles[cx] = System.Runtime.InteropServices.GCHandle.Alloc(bts, System.Runtime.InteropServices.GCHandleType.Pinned);
        _ar[cx] = _handles[cx].AddrOfPinnedObject();
      }
      _ar[ar.Length] = IntPtr.Zero;
    }
    ~StringListMarshal() => Dispose();
    public virtual void Dispose() {
      if (System.Threading.Interlocked.CompareExchange(ref _isDisposed, 1, 0) == 0 && _handles != null) {
         for (int cx = 0; cx < _handles.Length; cx++) {
           _handles[cx].Free();
           _ar[cx] = IntPtr.Zero;
        }
      }
      GC.SuppressFinalize(this);
    }
    public static string[] DecodeStringArray(IntPtr pList) {
      int count = 0;
      if (pList != IntPtr.Zero) checked {
        while (System.Runtime.InteropServices.Marshal.ReadIntPtr(pList, count * IntPtr.Size) != IntPtr.Zero)
          count++;
      }
      string[] ret = new string[count];
      for(int cx = 0; cx < count; cx++) {
        IntPtr objPtr = System.Runtime.InteropServices.Marshal.ReadIntPtr(pList, cx * IntPtr.Size);
        ret[cx]= $module.StringEncoder?.FromNullTerminated(objPtr);
      }
      return ret;
    }
  }
%}

/*
 * Typemap for char** options, char **dict, char **dictAndCSLDestroy, char **CSL
 */

%typemap(imtype, out="IntPtr") char **options, char **dict, char **dictAndCSLDestroy, char **CSL "IntPtr[]"
%typemap(cstype) char **options, char **dict, char **dictAndCSLDestroy, char **CSL "string[]"
%typemap(in) char **options, char **dict, char **dictAndCSLDestroy, char **CSL %{ $1 = ($1_ltype)$input; %}
%typemap(out) char **options, char **dict, char **dictAndCSLDestroy, char **CSL %{ $result = $1; %}
%typemap(csin, cshin="$csinput",
  pre="    using (var temp$csinput = new $modulePINVOKE.StringListMarshal($csinput)) { ",
  terminator="    }")
  char **options, char **dict, char **dictAndCSLDestroy, char **CSL
  "temp$csinput._ar"

/*
 * Marshal char**options, char **dict to string[] and don't free the unmanaged
 * string list.
 */

%typemap(csout, excode=SWIGEXCODE) char**options, char **dict
{
  /* %typemap(csout) char**options, char **dict */
  IntPtr cPtr = $imcall;
  string[] ret = $modulePINVOKE.StringListMarshal.DecodeStringArray(cPtr);
  $excode
  return ret;
}

/*
 * Marshal char** CSL, char **dictAndCSLDestroy to string[] and then free the
 * unmanaged string list.
 */

%typemap(csout, excode=SWIGEXCODE) char** CSL, char **dictAndCSLDestroy {
  /* %typemap(csout) char** CSL, char **dictAndCSLDestroy */
  IntPtr cPtr = $imcall;
  try {
    string[] ret = $modulePINVOKE.StringListMarshal.DecodeStringArray(cPtr);
    $excode
    return ret;
  }
  finally {
    if (cPtr != IntPtr.Zero)
      $modulePINVOKE.StringListDestroy(cPtr);
  }
}

/*
 * Expose the CSLDestroy function to free string lists
 */

%typemap(imtype) void *string_list_ptr "IntPtr"
%typemap(cstype) void *string_list_ptr "IntPtr"
%typemap(csin) void *string_list_ptr "$csinput"
%inline %{
  #include "cpl_string.h"
  void StringListDestroy(void *string_list_ptr) {
    CSLDestroy((char**)string_list_ptr);
  }
%}

/******************************************************************************
 * SWIG helper for C# exceptions with UTF-8 string parameters
 *
 * This helper class supersedes SWIG's built-in SWIGExceptionHelper.
 * SWIGExceptionHelper is still present and registers its own callbacks. Then
 * ExceptionHelperUtf8 registers its own callbacks, overwriting the callbacks
 * previously registered by SWIGExceptionHelper. Do this instead of defining
 * SWIG_CSHARP_NO_EXCEPTION_HELPER so that we can still make use of the C#
 * exception infrastructure built into SWIG.
 *****************************************************************************/

%pragma(csharp) imclasscode=%{
  public class ExceptionHelperUtf8 {
    public delegate void ExceptionDelegateUtf8(IntPtr pMessage);
    public delegate void ExceptionArgumentDelegateUtf8(IntPtr pMessage, IntPtr pParamName);
	
    [DllImport("$dllimport", EntryPoint = "SWIGRegisterExceptionCallbacks_$module")]
    public static extern void RegisterExceptionCallbacksUtf8(
        ExceptionDelegateUtf8 applicationDelegate,
        ExceptionDelegateUtf8 arithmeticDelegate,
        ExceptionDelegateUtf8 divideByZeroDelegate,
        ExceptionDelegateUtf8 indexOutOfRangeDelegate,
        ExceptionDelegateUtf8 invalidCastDelegate,
        ExceptionDelegateUtf8 invalidOperationDelegate,
        ExceptionDelegateUtf8 ioDelegate,
        ExceptionDelegateUtf8 nullReferenceDelegate,
        ExceptionDelegateUtf8 outOfMemoryDelegate,
        ExceptionDelegateUtf8 overflowDelegate,
        ExceptionDelegateUtf8 systemExceptionDelegate);

    [DllImport("$dllimport", EntryPoint = "SWIGRegisterExceptionArgumentCallbacks_$module")]
    public static extern void RegisterExceptionCallbacksArgumentUtf8(
        ExceptionArgumentDelegateUtf8 argumentDelegate,
        ExceptionArgumentDelegateUtf8 argumentNullDelegate,
        ExceptionArgumentDelegateUtf8 argumentOutOfRangeDelegate);

    static ExceptionHelperUtf8() {
      RegisterExceptionCallbacksUtf8(
        new ExceptionDelegateUtf8(SetPendingApplicationException),
        new ExceptionDelegateUtf8(SetPendingArithmeticException),
        new ExceptionDelegateUtf8(SetPendingDivideByZeroException),
        new ExceptionDelegateUtf8(SetPendingIndexOutOfRangeException),
        new ExceptionDelegateUtf8(SetPendingInvalidCastException),
        new ExceptionDelegateUtf8(SetPendingInvalidOperationException),
        new ExceptionDelegateUtf8(SetPendingIOException),
        new ExceptionDelegateUtf8(SetPendingNullReferenceException),
        new ExceptionDelegateUtf8(SetPendingOutOfMemoryException),
        new ExceptionDelegateUtf8(SetPendingOverflowException),
        new ExceptionDelegateUtf8(SetPendingSystemException));
		
      RegisterExceptionCallbacksArgumentUtf8(
        new ExceptionArgumentDelegateUtf8(SetPendingArgumentException),
        new ExceptionArgumentDelegateUtf8(SetPendingArgumentNullException),
        new ExceptionArgumentDelegateUtf8(SetPendingArgumentOutOfRangeException));
    }

    static void SetPendingApplicationException(IntPtr pMessage)
      => SWIGPendingException.Set(new ApplicationException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingArithmeticException(IntPtr pMessage)
      => SWIGPendingException.Set(new ArithmeticException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingDivideByZeroException(IntPtr pMessage)
      => SWIGPendingException.Set(new DivideByZeroException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingIndexOutOfRangeException(IntPtr pMessage)
      => SWIGPendingException.Set(new IndexOutOfRangeException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingInvalidCastException(IntPtr pMessage)
      => SWIGPendingException.Set(new InvalidCastException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingInvalidOperationException(IntPtr pMessage)
      => SWIGPendingException.Set(new InvalidOperationException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingIOException(IntPtr pMessage)
      => SWIGPendingException.Set(new System.IO.IOException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingNullReferenceException(IntPtr pMessage)
      => SWIGPendingException.Set(new NullReferenceException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingOutOfMemoryException(IntPtr pMessage)
      => SWIGPendingException.Set(new OutOfMemoryException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingOverflowException(IntPtr pMessage)
      => SWIGPendingException.Set(new OverflowException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingSystemException(IntPtr pMessage)
      => SWIGPendingException.Set(new SystemException(Utf8UnmanagedToString(pMessage), SWIGPendingException.Retrieve()));
    static void SetPendingArgumentException(IntPtr pMessage, IntPtr pParamName)
      => SWIGPendingException.Set(new ArgumentException(Utf8UnmanagedToString(pMessage), Utf8UnmanagedToString(pParamName), SWIGPendingException.Retrieve()));
    static void SetPendingArgumentNullException(IntPtr pMessage, IntPtr pParamName) {
      Exception e = SWIGPendingException.Retrieve();
	  string message = e != null ? " Inner Exception: " + e.Message : Utf8UnmanagedToString(pMessage);
      SWIGPendingException.Set(new ArgumentNullException(Utf8UnmanagedToString(pParamName), message));
    }
    static void SetPendingArgumentOutOfRangeException(IntPtr pMessage, IntPtr pParamName) {
      Exception e = SWIGPendingException.Retrieve();
	  string message = e != null ? " Inner Exception: " + e.Message : Utf8UnmanagedToString(pMessage);
      SWIGPendingException.Set(new ArgumentOutOfRangeException(Utf8UnmanagedToString(pParamName), message));
    }
    static string Utf8UnmanagedToString(IntPtr pUtf8Bts)
	  => $module.StringEncoder?.FromNullTerminated(pUtf8Bts);
  }
  //Instantiate the helper class so that the static constructor executes
  static readonly ExceptionHelperUtf8 exceptionHelperUtf8 = new ExceptionHelperUtf8();
%}
