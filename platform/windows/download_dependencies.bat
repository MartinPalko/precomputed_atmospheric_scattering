@echo off
REM     https://cmake.org/files/v3.7/cmake-3.7.2-win64-x64.zip -> cmake
REM     http://sourceforge.net/projects/gnuwin32/files//sed/4.2.1/sed-4.2.1-bin.zip/download -> sed
REM     http://sourceforge.net/projects/gnuwin32/files//sed/4.2.1/sed-4.2.1-dep.zip/download -> sed
REM     http://files.transmissionzero.co.uk/software/development/GLUT/freeglut-MSVC.zip -> freeglut
REM     https://sourceforge.net/projects/glew/files/glew/2.0.0/glew-2.0.0-win32.zip/download -> glew

set EXT=%~dp0external

echo Installing dependencies into %EXT%
if not exist %EXT% md %EXT%

REM ------------------ freeglut

set SRC=http://files.transmissionzero.co.uk/software/development/GLUT/freeglut-MSVC.zip
set ZIP=%EXT%\freeglut-MSVC.zip
set DST=%EXT%\.

if not exist %EXT%\freeglut (
echo Downloading freeglut from %SRC%...
powershell -Command "Start-BitsTransfer '%SRC%' '%ZIP%'"
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP%', '%DST%'); }"
del %ZIP%
) else echo freeglut detected. skipping.

REM ------------------ glew

set SRC=https://sourceforge.net/projects/glew/files/glew/2.0.0/glew-2.0.0-win32.zip/download
set ZIP=%EXT%\glew-2.0.0-win32.zip
set DST=%EXT%\.

if not exist %EXT%\glew (
echo Downloading glew from %SRC%...
powershell -Command "Start-BitsTransfer '%SRC%' '%ZIP%'"
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP%', '%DST%'); }"
rename %EXT%\glew-2.0.0 glew
del %ZIP%
) else echo glew detected. skipping.

REM ------------------ sed

set SRC=http://sourceforge.net/projects/gnuwin32/files//sed/4.2.1/sed-4.2.1-bin.zip/download
set ZIP=%EXT%\sed-4.2.1-bin.zip
set DST=%EXT%\sed

if not exist %EXT%\sed\bin\sed.exe (
echo Downloading sed from %SRC%...
powershell -Command "Start-BitsTransfer '%SRC%' '%ZIP%'"
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP%', '%DST%'); }"
del %ZIP%
) else echo sed.exe detected. skipping.

REM ------------------ sed-dep

set SRC=http://sourceforge.net/projects/gnuwin32/files//sed/4.2.1/sed-4.2.1-dep.zip/download
set ZIP=%EXT%\sed-4.2.1-dep.zip
set DST=%EXT%\sed

if not exist %EXT%\sed\bin\libiconv2.dll (
echo Downloading sed-dep from %SRC%...
powershell -Command "Start-BitsTransfer '%SRC%' '%ZIP%'"
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP%', '%DST%'); }"
del %ZIP%
) else echo sed-dep detected. skipping.

REM ------------------ cmake

set SRC=https://cmake.org/files/v3.7/cmake-3.7.2-win64-x64.zip
set ZIP=%EXT%\cmake-3.7.2-win64-x64.zip
set DST=%EXT%\.

if not exist %EXT%\cmake (
echo Downloading cmake from %SRC%...
powershell -Command "Start-BitsTransfer '%SRC%' '%ZIP%'"
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP%', '%DST%'); }"
rename %EXT%\cmake-3.7.2-win64-x64 cmake
del %ZIP%
) else echo cmake detected. skipping.

REM ------------------ libtiff

REM set SRC_LIB=https://sourceforge.net/projects/gnuwin32/files/tiff/3.8.2-1/tiff-3.8.2-lib.zip/download
REM set SRC_BIN=https://sourceforge.net/projects/gnuwin32/files/tiff/3.8.2-1/tiff-3.8.2-bin.zip/download
REM set ZIP_LIB=%EXT%\tiff-3.8.2-1-lib.zip
REM set ZIP_BIN=%EXT%\tiff-3.8.2-1-bin.zip
REM set DST=%EXT%\libtiff

REM if not exist %EXT%\libtiff (
REM echo Downloading libtiff lib from %SRC_LIB%...
REM powershell -Command "Start-BitsTransfer '%SRC_LIB%' '%ZIP_LIB%'"
REM powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP_LIB%', '%DST%'); }"
REM del %ZIP_LIB%
REM echo Downloading libtiff bin from %SRC_BIN%...
REM powershell -Command "Start-BitsTransfer '%SRC_BIN%' '%ZIP_BIN%'"
REM powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%ZIP_BIN%', '%DST%'); }"
REM del %ZIP_BIN%
REM ) else echo libtiff detected. skipping.


:end
if NOT '%1' == 'NOPAUSE' pause

