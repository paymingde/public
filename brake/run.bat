@echo off
setlocal

gcc *.c -o main.exe

if %errorlevel% equ 0 (
    echo [INFO] compile ok
    main.exe
) else (
    echo [ERROR] compile fail, errno: %errorlevel%
    exit /b %errorlevel%
)

endlocal
