# -*- compile-command: "nmake"; -*- emacs でのコンパイルモードの設定
TARGET_OS_VERSION=_WIN32_WINNT_WIN7 # Windows7
CCOPT_DEBUG_FLAGS= #/MDd /D_CRTDBG_MAP_ALLOC=1 /D_DEBUG=1

CC=cl.exe
CCOPT=/nologo /EHsc /c /W4 /O2 /Zi $(CCOPT_DEBUG_FLAGS) /DUNICODE=1 /D_UNICODE=1 /DWINVER=$(TARGET_OS_VERSION) /D_WIN32_WINNT=$(TARGET_OS_VERSION)
LINK="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.14.26428\bin\Hostx64\x64\link.exe"
#LINK=link.exe
LINKOPT=/nologo /WX /DEBUG /OPT:REF /OPT:ICF /MAP /LARGEADDRESSAWARE /MANIFEST "/MANIFESTDEPENDENCY:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*' "


sample_OBJS=entryPoint.obj renderThread.obj D2DUIComponent.obj
sample_LIBS=kernel32.lib User32.lib Ole32.lib comsuppw.lib Dxgi.lib D2d1.lib D3D10_1.lib Avrt.lib Dwmapi.lib 

ALL_TARGET=sample.exe

all: $(ALL_TARGET)

sample.exe: $(sample_OBJS)
	$(LINK) $(LINKOPT) /OUT:$@ $(sample_OBJS) $(sample_LIBS) 

entryPoint.obj: entryPoint.cpp whVERIFY.hxx whDllFunction.hxx
	$(CC) $(CCOPT) entryPoint.cpp

renderThread.obj: renderThread.cpp 
	$(CC) $(CCOPT) renderThread.cpp

D2DUIComponent.obj: D2DUIComponent.cpp
	$(CC) $(CCOPT) D2DUIComponent.cpp

clean-eamcsbackup:
	@-FOR /F "usebackq" %I IN (`dir *~ /b 2^>nul`) DO @IF EXIST "%~I" del "%~I"

clean: clean-eamcsbackup
	@FOR %I in ( $(sample_OBJS) $(ALL_TARGET) ) DO @IF EXIST "%~I" del "%~I"

distclean: clean
	@-FOR /F "usebackq" %I IN (`dir *.pdb *.map /B 2^>nul`) DO @IF EXIST "%~I" del "%~I"
