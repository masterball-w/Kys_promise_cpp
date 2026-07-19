@echo off
cd /d "%~dp0"
python file_masker.py
if %errorlevel% neq 0 (
    echo Python execution failed. Please ensure Python is installed and in your PATH.
    pause
)
