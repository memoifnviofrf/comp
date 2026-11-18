@echo off
setlocal
cd /d "%~dp0"
rem Build and run the headless DSP tests (no GUI). Output: docs\TEST_REPORT_local.md
set "PATH=%PATH%;%ProgramFiles%\CMake\bin;%ProgramFiles%\Git\cmd"
where cmake >nul 2>nul
if errorlevel 1 goto nocmake
echo [1/3] Configuring (first run downloads JUCE)...
cmake -B build_tests -A x64 -DVC_BUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 goto fail
echo [2/3] Building tests (Release)...
cmake --build build_tests --config Release --target VocalCompressorTests --parallel
if errorlevel 1 goto fail
echo [3/3] Running tests (about a minute)...
"build_tests\Release\VocalCompressorTests.exe" > "docs\TEST_REPORT_local.md"
set "RC=%errorlevel%"
findstr /C:"PASS" /C:"FAIL" "docs\TEST_REPORT_local.md" | findstr /C:"/ FAIL"
echo.
if "%RC%"=="0" (echo [OK] All tests passed.) else (echo [NG] Some tests failed. See docs\TEST_REPORT_local.md)
echo Report: %CD%\docs\TEST_REPORT_local.md
pause
exit /b %RC%
:nocmake
echo CMake was not found. Run build.bat first (it offers to install it).
pause
exit /b 1
:fail
echo [ERROR] Build failed. Copy the error message above and send it to Claude.
pause
exit /b 1
