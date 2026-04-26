@echo off
setlocal
set "REPO=%~dp0"
set "REPO=%REPO:~0,-1%"
set "AND=%REPO%\android"
cd /d "%AND%" || (
  echo android-verify: could not cd to android\
  exit /b 1
)

rem CI parity: scripts/android-verify.sh — needs Android SDK + JDK on PATH.
if not defined ANDROID_HOME if exist "%LOCALAPPDATA%\Android\Sdk\source.properties" (
  set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
)
if not defined ANDROID_SDK_ROOT if defined ANDROID_HOME set "ANDROID_SDK_ROOT=%ANDROID_HOME%"

if not exist "local.properties" if defined ANDROID_HOME (
  call :write_sdkdir_local_properties
  echo [android-verify] Created android\local.properties using SDK: %ANDROID_HOME%
  echo.
)

if not exist "local.properties" (
  echo.
  echo Android SDK location not found. Gradle needs one of:
  echo   - Set ANDROID_HOME or ANDROID_SDK_ROOT to your SDK ^(see Android Studio: Settings -^> SDK^), or
  echo   - Create android\local.properties with sdk.dir=... ^(see android\local.properties.example^)
  echo.
  echo Typical path: %LOCALAPPDATA%\Android\Sdk
  echo.
  exit /b 1
)

call gradlew.bat assembleDebug --no-daemon --stacktrace
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" exit /b %ERR%
echo.
echo OK: debug APK under android\app\build\outputs\apk\debug\
exit /b 0

:write_sdkdir_local_properties
set "SDKDIR=%ANDROID_HOME:\=/%"
>local.properties echo sdk.dir=%SDKDIR%
exit /b
