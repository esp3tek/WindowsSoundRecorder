@echo off
setlocal
pushd "%~dp0"
where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /W3 test_wav.c wav.c audio_util.c /Fe:test_wav.exe
    if exist test_wav.obj del test_wav.obj
    if exist wav.obj del wav.obj
    goto :run
)
where gcc >nul 2>nul
if %errorlevel%==0 (
    gcc test_wav.c wav.c audio_util.c -o test_wav.exe
    goto :run
)
echo No C compiler found.
popd
exit /b 1
:run
.\test_wav.exe
popd
endlocal
