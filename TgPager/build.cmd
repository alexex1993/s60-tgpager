@echo off
set EPOCROOT=\Symbian\8.1a\S60_2nd_FP3\
set PATH=C:\Perl\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\gcc\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\tools;%PATH%
set PRJ=C:\Users\alexnew\Downloads\reps\s60-tgpager\TgPager
cd /d "%PRJ%\group"
cmd /c bldmake bldfiles || goto :eof
cmd /c abld.bat build thumb urel || goto :eof
cd /d "%PRJ%\sis"
C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\tools\makesis.exe tgpager.pkg || goto :eof
copy /y TgPager.sis C:\Users\alexnew\Downloads\reps\s60-tgpager\ >nul
echo === SIS ready: C:\Users\alexnew\Downloads\reps\s60-tgpager\TgPager.sis ===