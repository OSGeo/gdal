
COMTest1ps.dll: dlldata.obj COMTest1_p.obj COMTest1_i.obj
	link /dll /out:COMTest1ps.dll /def:COMTest1ps.def /entry:DllMain dlldata.obj COMTest1_p.obj COMTest1_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del COMTest1ps.dll
	@del COMTest1ps.lib
	@del COMTest1ps.exp
	@del dlldata.obj
	@del COMTest1_p.obj
	@del COMTest1_i.obj
