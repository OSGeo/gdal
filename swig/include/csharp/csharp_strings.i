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

/*******************************************************************************
 * Marshaler for NULL terminated UTF-8 encoded strings                         *
 * Creates a callback function which is used from the native runtime to create *
 * .NET strings. The managed Utf8StringHelper registers a callback function    *
 * with the native runtime which is used by the native runtime to create .NET  *
 * strings. When called by the native runtime, the .NET function allocates     *
 * unmanaged memory and decodes the UTF-8 string into a length-prefixed UTF-16 *
 * string into that buffer. A pointer to that buffer is returned. When the     *
 * callback returns to managed .NET, a managed string is created from the      *
 * unmanaged buffer using LengthPrefixedUtf16UnmanagedToString, which frees    *
 * the unmanaged memory that was allocated by the callback function.           *
 ******************************************************************************/

%insert(runtime) %{
/* Callback for returning strings to C# without leaking memory */
typedef char * (SWIGSTDCALL* CSharpUtf8StringHelperCallback)(const char *);
static CSharpUtf8StringHelperCallback SWIG_csharp_string_callback = NULL;
%}

%pragma(csharp) imclasscode=%{
  public class Utf8StringHelper {

    public delegate System.IntPtr Utf8StringDelegate(System.IntPtr message);

   /*
    * IMPORTANT:
    * Every call to SWIG_csharp_string_callback MUST be followed by exactly one call
    * to LengthPrefixedUtf16UnmanagedToString on the returned pointer.
    * Failure to do so will result in memory leaks or double frees.
    */
    static Utf8StringDelegate stringDelegate = new Utf8StringDelegate($modulePINVOKE.Utf8UnmanagedToLengthPrefixedUtf16Unmanaged);

    [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="RegisterUtf8StringCallback_$module")]
    public static extern void RegisterUtf8StringCallback_$module(Utf8StringDelegate stringDelegate);

    static Utf8StringHelper() {
      RegisterUtf8StringCallback_$module(stringDelegate);
    }
  }

  //Instantiate the helper class so that the static constructor executes
  public static readonly Utf8StringHelper utf8StringHelper = new Utf8StringHelper();

  public unsafe static IntPtr Utf8UnmanagedToLengthPrefixedUtf16Unmanaged(IntPtr pUtf8Bts)
  {
    if (pUtf8Bts == IntPtr.Zero)
      return IntPtr.Zero;

    byte* pStringUtf8 = (byte*)pUtf8Bts;
    int len = 0;
    while (pStringUtf8[len] != 0) len = checked(len + 1);

    var charCount = System.Text.Encoding.UTF8.GetCharCount(pStringUtf8, len);
    int size = checked(sizeof(int) + charCount * sizeof(char));
    var bstrDest = System.Runtime.InteropServices.Marshal.AllocHGlobal(size);
    *(int*)bstrDest = charCount;

    System.Text.Encoding.UTF8.GetChars(pStringUtf8, len, (char*)IntPtr.Add(bstrDest, sizeof(int)), charCount);
    return bstrDest;
  }

  public unsafe static string LengthPrefixedUtf16UnmanagedToString(IntPtr pBstr)
  {
    if (pBstr == IntPtr.Zero)
      return null;

    try
    {
      int charCount = *(int*)pBstr;
      if (charCount < 0)
        return null;
      int charStart = sizeof(int) / sizeof(char);
      return new string((char*)pBstr, charStart, charCount);
    }
    finally
    {
      System.Runtime.InteropServices.Marshal.FreeHGlobal(pBstr);
    }
  }

  public unsafe static IntPtr StringToUtf8Unmanaged(string str)
  {
    if (str == null)
      return IntPtr.Zero;

    int byteCount = System.Text.Encoding.UTF8.GetByteCount(str);
    IntPtr unmanagedString = System.Runtime.InteropServices.Marshal.AllocHGlobal(byteCount + 1);

    byte* ptr = (byte*)unmanagedString.ToPointer();
    fixed (char *pStr = str)
    {
      System.Text.Encoding.UTF8.GetBytes(pStr, str.Length, ptr, byteCount);
      // null-terminate
      ptr[byteCount] = 0;
    }

    return unmanagedString;
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

/******************************************************************************
 * Apply typemaps for all SWIG string types and for const char *utf8_string   *
 ******************************************************************************/

%typemap(cstype) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) "string"
%typemap(imtype) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) "IntPtr"

%typemap(in) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{
  $1 = ($1_ltype)$input;
%}

%typemap(out) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{
  $result = SWIG_csharp_string_callback((const char *)$1);
%}

%typemap(csin,
  pre="    IntPtr temp$csinput = $modulePINVOKE.StringToUtf8Unmanaged($csinput);",
  post="    System.Runtime.InteropServices.Marshal.FreeHGlobal(temp$csinput);",
  cshin="$csinput"
  ) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string)
  "temp$csinput"

%typemap(csout, excode=SWIGEXCODE) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string)
{
  /* %typemap(csout) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) */
  IntPtr cPtr = $imcall;
  string ret = $modulePINVOKE.LengthPrefixedUtf16UnmanagedToString(cPtr);
  $excode
  return ret;
}

/*
 * Typemap for UTF-8 char* string properties.
 */

%typemap(csvarin, excode=SWIGEXCODE2) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{
  /* %typemap(csvarin) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) */
  set {
    IntPtr temp$csinput = $modulePINVOKE.StringToUtf8Unmanaged($csinput);
    try {
      $imcall;$excode
    }
    finally {
      System.Runtime.InteropServices.Marshal.FreeHGlobal(temp$csinput);
    }
  }
%}

%typemap(csvarout, excode=SWIGEXCODE2) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) %{
  get {
    /* %typemap(csvarout) (char *), (char *&), (char[ANY]), (char[]), (const char *utf8_string) */
    IntPtr cPtr = $imcall;
    string ret = $modulePINVOKE.LengthPrefixedUtf16UnmanagedToString(cPtr);
    $excode
    return ret;
  }
%}

/*
 * Typemaps for  (retStringAndCPLFree*)
 */

%typemap(out) (retStringAndCPLFree*) %{
  /* %typemap(out) (retStringAndCPLFree*) */
  if($1)
  {
    $result = SWIG_csharp_string_callback((const char *)$1);
    CPLFree($1);
  }
  else
  {
    $result = NULL;
  }
%}

/*
 * Typemap for char **argout. Used for 'out string' parameters.
 */

%typemap(imtype) (char** argout), (char **username), (char **usrname), (char **type) "ref IntPtr"
%typemap(cstype) (char** argout), (char **username), (char **usrname), (char **type) "out string"
%typemap(in) (char **argout), (char **username), (char **usrname), (char **type) %{ $1 = ($1_ltype)$input; %}
%typemap(csin,
  pre="    IntPtr temp$csinput = IntPtr.Zero;",
  post="    $csinput = $modulePINVOKE.LengthPrefixedUtf16UnmanagedToString(temp$csinput);",
  cshin="out $csinput"
  ) (char** argout), (char **username), (char **usrname), (char **type)
  "ref temp$csinput"

%typemap(argout) (char **argout)
{
  /* %typemap(argout) (char **argout) */
  char* temp_string;
  temp_string = SWIG_csharp_string_callback(*$1);
  if (*$1)
    CPLFree(*$1);
  *$1 = temp_string;
}
%typemap(argout) (char **username), (char **usrname), (char **type)
{
  /* %typemap(argout) (char **username), (char **usrname), (char **type) */
  *$1 = SWIG_csharp_string_callback(*$1);
}

/*
 * Typemap for char **ignorechange. Used for 'ref string' parameters where
 * caller doesn't want to see changes.
 */

%typemap(imtype) (char **ignorechange) "ref IntPtr"
%typemap(cstype) (char **ignorechange) "ref string"
%typemap(csin,
  pre="    IntPtr temp$csinput = $modulePINVOKE.StringToUtf8Unmanaged($csinput);",
  post="    System.Runtime.InteropServices.Marshal.FreeHGlobal(temp$csinput);",
  cshin="ref $csinput"
  ) (char** ignorechange)
  "ref temp$csinput"

%typemap(in, noblock="1") (char **ignorechange)
{
  /* %typemap(in) (char **ignorechange) */
  $*1_type savearg = *(($1_type)$input);
  $1 = ($1_ltype)$input;
}
%typemap(argout, noblock="1") (char **ignorechange)
{
  /* %typemap(argout) (char **ignorechange) */
  if ((*$1 - savearg) > 0)
     memmove(savearg, *$1, strlen(*$1)+1);
  *$1 = savearg;
}

/******************************************************************************
 * Marshaler for NULL terminated lists of NULL terminated UTF-8 strings.     *
 *****************************************************************************/

%pragma(csharp) imclasscode=%{
  public class StringListMarshal : IDisposable {
    public readonly IntPtr[] _ar;
    private int _isDisposed;
    public StringListMarshal(string[] ar) {
      if (ar == null)
        return;
      _ar = new IntPtr[ar.Length+1];
      for (int cx = 0; cx < ar.Length; cx++) {
        _ar[cx] = $modulePINVOKE.StringToUtf8Unmanaged(ar[cx]);
      }
      _ar[ar.Length] = IntPtr.Zero;
    }
    ~StringListMarshal() => Dispose();
    public virtual void Dispose() {
      if (System.Threading.Interlocked.CompareExchange(ref _isDisposed, 1, 0) == 0 && _ar != null) {
        for (int cx = 0; cx < _ar.Length - 1; cx++) {
            System.Runtime.InteropServices.Marshal.FreeHGlobal(_ar[cx]);
        }
      }
      GC.SuppressFinalize(this);
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
%typemap(csin,
  pre="    using (var temp$csinput = new $modulePINVOKE.StringListMarshal($csinput)) { ",
  terminator="    }",
  cshin="$csinput"
  ) char **options, char **dict, char **dictAndCSLDestroy, char **CSL
  "temp$csinput._ar"

/*
 * C# code to marshal NULL terminated lists of NULL terminated UTF-8 strings.
 */

%define CS_MARSHAL_STRING_LIST()
  IntPtr cPtr = $imcall;
  IntPtr objPtr;
  int count = 0;
  if (cPtr != IntPtr.Zero) {
    while (System.Runtime.InteropServices.Marshal.ReadIntPtr(cPtr, count*IntPtr.Size) != IntPtr.Zero)
      ++count;
  }
  string[] ret = new string[count];
  if (count > 0) {
    for(int cx = 0; cx < count; cx++) {
      objPtr = System.Runtime.InteropServices.Marshal.ReadIntPtr(cPtr, cx * System.Runtime.InteropServices.Marshal.SizeOf(typeof(IntPtr)));
      ret[cx]= (objPtr == IntPtr.Zero) ? null : $module.Utf8BytesToString(objPtr);
    }
  }
%enddef

/*
 * Marshal char**options, char **dict to string[] and don't free the unmanaged
 * string list.
 */

%typemap(csout, excode=SWIGEXCODE) char**options, char **dict
{
  /* %typemap(csout) char**options, char **dict */
  CS_MARSHAL_STRING_LIST()
  $excode
  return ret;
}

/*
 * Marshal char** CSL, char **dictAndCSLDestroy to string[] and then free the
 * unmanaged string list.
 */

%typemap(csout, excode=SWIGEXCODE) char** CSL, char **dictAndCSLDestroy {
  /* %typemap(csout) char** CSL, char **dictAndCSLDestroy */
  CS_MARSHAL_STRING_LIST()
  if (cPtr != IntPtr.Zero)
    $modulePINVOKE.StringListDestroy(cPtr);
  $excode
  return ret;
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
