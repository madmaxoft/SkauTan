@echo off

rem Download the ffmpeg's LibAV for Windows, version 3.4.1:
if not exist c:\projects\lib\include\avformat\avformat.h (
	echo -------------------------
	echo Downloading precompiled LibAV libraries from ffmpeg...
	mkdir c:\projects\lib
	curl -o c:\projects\lib\libav.zip https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-3.4.1-win64-dev.zip
	if errorlevel 1 (
		echo Download unsuccessful
		exit /b 1
	)
	7z x c:\projects\lib\libav.zip -oc:\projects\lib
	if errorlevel 1 (
		echo Unzip unsuccessful
		exit /b 1
	)
	move c:\projects\lib\ffmpeg-3.4.1-win64-dev\include c:\projects\lib
	move c:\projects\lib\ffmpeg-3.4.1-win64-dev\lib c:\projects\lib
)

rem Download the precompiled TagLib:
if not exist c:\projects\lib\include\taglib\tag.h (
	echo -------------------------
	echo Downloading precompiled TagLib libraries...
	curl -o c:\projects\lib\taglib.7z http://xoft.cz/taglib-msvc2015-x64.7z
	if errorlevel 1 (
		echo Download unsuccessful
		exit /b 1
	)
	7z x c:\projects\lib\taglib.7z -oc:\projects\lib
	if errorlevel 1 (
		echo Unzip unsuccessful
		exit /b 1
	)
)
echo -------------------------
echo Libraries checked, compiling now...

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" amd64
set LIB=%LIB%;c:\projects\lib\lib
set INCLUDE=%INCLUDE%;c:\projects\lib\include

echo -------------------------
echo Running QMake...
qmake SkauTan.pro

echo -------------------------
echo Running nmake...
nmake
