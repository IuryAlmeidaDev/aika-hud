@echo off
setlocal
echo ========================================================
echo           AikaHUD - Compilador Win32 em C Puro
echo ========================================================

set TCC_BIN=C:\Users\iuryc\tcc\tcc\tcc.exe

if exist "%TCC_BIN%" (
    echo [OK] Compilando com Tiny C Compiler
    "%TCC_BIN%" src\main.c src\combat.c src\ui.c -o AikaHUD.exe -lgdi32 -luser32 -lkernel32 -mwindows
) else (
    echo [INFO] Compilando com GCC
    gcc -O3 src\main.c src\combat.c src\ui.c -o AikaHUD.exe -lgdi32 -luser32 -lkernel32 -mwindows
)

if %ERRORLEVEL% equ 0 (
    echo [OK] Embutindo manifesto de Administrador
    python tools_inject_manifest.py
    echo [OK] Embutindo icone oficial
    python tools_inject_icon.py

    echo.
    echo [SUCESSO] AikaHUD.exe compilado com exito!
    copy /Y AikaHUD.exe "C:\Users\iuryc\Desktop\Aika_Combat_Macro.exe" >nul
    copy /Y AikaHUD.exe "C:\Users\iuryc\Desktop\AikaHUD.exe" >nul
    if exist config.ini (
        copy /Y config.ini "C:\Users\iuryc\Desktop\config.ini" >nul
    )
    echo [OK] Copiado para Area de Trabalho!
    dir AikaHUD.exe
) else (
    echo.
    echo [ERRO] Falha na compilacao.
)

endlocal
