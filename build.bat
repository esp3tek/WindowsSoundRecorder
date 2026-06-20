@echo off
setlocal
pushd "%~dp0"

where cl >nul 2>nul
if %errorlevel%==0 (
    rc /nologo /fo resources.res resources.rc
    cl /nologo /O2 /W3 sound_recorder.c audio_capture.c wav.c encoder.c audio_util.c mp3_encoder.c tags.c identify.c resources.res /link /SUBSYSTEM:WINDOWS ole32.lib user32.lib gdi32.lib shell32.lib mfplat.lib mf.lib mfreadwrite.lib mfuuid.lib winhttp.lib advapi32.lib /OUT:WindowsSoundRecorder.exe
    if exist sound_recorder.obj del sound_recorder.obj
    if exist audio_capture.obj del audio_capture.obj
    if exist wav.obj del wav.obj
    if exist encoder.obj del encoder.obj
    if exist audio_util.obj del audio_util.obj
    if exist mp3_encoder.obj del mp3_encoder.obj
    if exist tags.obj del tags.obj
    if exist identify.obj del identify.obj
    if exist resources.res del resources.res
    goto :done
)

where gcc >nul 2>nul
if %errorlevel%==0 (
    windres resources.rc -O coff -o resources.o
    gcc sound_recorder.c audio_capture.c wav.c encoder.c audio_util.c mp3_encoder.c tags.c identify.c resources.o -o WindowsSoundRecorder.exe -municode -mwindows -lole32 -luser32 -lgdi32 -lshell32 -luuid -lmfplat -lmf -lmfreadwrite -lmfuuid -lwinhttp -ladvapi32 -s -O2
    if exist resources.o del resources.o
    goto :done
)

echo No C compiler found. Install either MSVC (cl.exe) or MinGW-w64 (gcc.exe).
popd
exit /b 1

:done
echo Built WindowsSoundRecorder.exe
popd
endlocal
