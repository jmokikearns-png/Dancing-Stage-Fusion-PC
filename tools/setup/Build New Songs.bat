@echo off
rem Dancing Stage Fusion PC - Build New Songs
rem Builds the new songs from the simfile packs in "simfiles", with the song
rem tools and song list in "songtools", and puts the song pack in the game.
setlocal EnableExtensions
cd /d "%~dp0"
title Dancing Stage Fusion PC - Build New Songs
echo Dancing Stage Fusion PC - Build New Songs
echo Builds the new songs from your simfile packs in the "simfiles" folder.
echo.
set "CUE="
for %%F in (disc\*.cue) do call :pickcue "%%~fF"
if not defined CUE goto nodisc
set "PYTHONDONTWRITEBYTECODE=1"
set "OUT=%~dp0songtools\work\pack"
if exist "songtools\work" rmdir /s /q "songtools\work"
"songtools\python\python.exe" "songtools\extrasong\build_extrasong.py" "songtools\extrasong\songs.toml" "%CUE%" "%OUT%" --simfiles "simfiles"
if errorlevel 1 goto failed
set "DEST=mods\installed\dsf.extrasongs\1.0.0"
if exist "mods\installed\dsf.extrasongs" rmdir /s /q "mods\installed\dsf.extrasongs"
mkdir "%DEST%"
xcopy /e /i /q /y "%OUT%" "%DEST%" >nul
if errorlevel 1 goto failed
rmdir /s /q "songtools\work"
echo.
echo Done: the new songs are in the game. Start it with DancingStageFusionPC.exe.
echo The song pack holds your music and charts: for your own use only, do not share it.
goto end

:pickcue
if defined CUE goto :eof
rem (not a Deluxe image made from it)
set "NAME=%~n1"
if not "%NAME:Deluxe=%"=="%NAME%" goto :eof
set "CUE=%~1"
goto :eof

:nodisc
echo No disc image found. Put your Dancing Stage Fusion .bin and .cue in the "disc" folder.
goto end

:failed
echo.
echo Building the songs failed: see the message above.

:end
echo.
pause
