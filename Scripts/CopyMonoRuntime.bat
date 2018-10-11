echo "Assuming framework libaries and headers have been copied from mac/linux"

call :copylibs x64 Win64
call :copylibs Win32 Win32

EXIT /B 0

:copylibs

set "SRC=..\..\..\..\..\mono\msvc\build\sgen\%1"
set "DEST=..\ThirdParty\mono\lib\%2"

mkdir "%DEST%
copy "%SRC%\lib\Debug\mono-2.0-sgen.lib" "%DEST%"
copy "%SRC%\bin\Debug\mono-2.0-sgen.dll" "%DEST%"
copy "%SRC%\bin\Debug\mono-2.0-sgen.pdb" "%DEST%"

EXIT /B 0