
SFps.dll: dlldata.obj SF_p.obj SF_i.obj
	link /dll /out:SFps.dll /def:SFps.def /entry:DllMain dlldata.obj SF_p.obj SF_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del SFps.dll
	@del SFps.lib
	@del SFps.exp
	@del dlldata.obj
	@del SF_p.obj
	@del SF_i.obj
