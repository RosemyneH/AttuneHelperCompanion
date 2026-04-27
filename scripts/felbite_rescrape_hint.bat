@echo off
REM Windows: prints import_web_catalogs command (stderr) and writes build\tbc_zip_audit.json. Exit 1 if no TBC-matching URLs; see felbite_rescrape_hint.sh for Git Bash.
cd /d "%~dp0\.."
python scripts\audit_tbc_zip_urls.py --emit-rescrape-bash --no-stdout-json -o "build\tbc_zip_audit.json"
exit /b %ERRORLEVEL%
