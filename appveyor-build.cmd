@echo off

rem Download the ffmpeg's LibAV for Windows, version 3.4.1:
if not exist c:\projects\lib\include\avformat\avformat.h (
	echo Downloading precompiled LibAV libraries from ffmpeg...
	mkdir c:\projects\lib
	curl -o c:\projects\lib\libav.zip https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-3.4.1-win64-dev.zip
	if errorlevel 1 (
		echo Download unsuccessfull
		exit /b 1
	)
	7z x c:\projects\lib\libav.zip -oc:\projects\lib
	if errorlevel 1 (
		echo Unzip unsuccessfull
		exit /b 1
	)
	move c:\projects\lib\ffmpeg-3.4.1-win64-dev\include c:\projects\lib
	move c:\projects\lib\ffmpeg-3.4.1-win64-dev\lib c:\projects\lib
)

set LIB=%LIB%;c:\projects\lib\lib
set INCLUDE=%INCLUDE%;c:\projects\lib\include

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" amd64
qmake SkauTan.pro
nmake
