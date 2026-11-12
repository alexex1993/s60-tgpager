@echo off
set EPOCROOT=\Symbian\8.1a\S60_2nd_FP3\
set PATH=C:\Perl\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\gcc\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\tools;%PATH%
set PRJ=C:\Users\alexnew\Downloads\n70_test\HelloN70
cd /d "%PRJ%\group"
cmd /c bldmake bldfiles || goto :eof
cmd /c abld.bat build thumb urel || goto :eof
cd /d "%PRJ%\sis"
C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\tools\makesis.exe hellon70.pkg || goto :eof
copy /y HelloN70.sis C:\Users\alexnew\Downloads\n70_test\ >nul
echo === SIS ready: n70_test\HelloN70.sis ===
