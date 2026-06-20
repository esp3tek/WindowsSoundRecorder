@echo off
setlocal
pushd "%~dp0"
where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /W3 test_tags.c tags.c identify.c wav.c audio_util.c /Fe:test_tags.exe /link winhttp.lib
    if exist test_tags.obj del test_tags.obj
    if exist tags.obj del tags.obj
    if exist identify.obj del identify.obj
    if exist wav.obj del wav.obj
    if exist audio_util.obj del audio_util.obj
    goto :run
)
where gcc >nul 2>nul
if %errorlevel%==0 (
    gcc test_tags.c tags.c identify.c wav.c audio_util.c -o test_tags.exe -lwinhttp
    goto :run
)
echo No C compiler found.
popd
exit /b 1
:run
.\test_tags.exe
popd
endlocal
