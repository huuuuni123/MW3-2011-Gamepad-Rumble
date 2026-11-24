@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist iw5sp.exe (
  echo iw5sp.exe 가 있는 게임 폴더에서 실행하세요. / Run this from the game folder.
  pause
  exit /b 1
)
echo.
echo  MW3 2011 Gamepad + 진동 완벽 호환 모드를 제거합니다.
echo  Removes this mod. 한글패치와 패드 모드는 그대로 둡니다. / Korean patch and MW3GamepadFix are kept.
echo.
choice /c YN /m "계속할까요? Continue"
if errorlevel 2 exit /b 0

del /q MW3KoreanBridge.asi MW3Rumble.asi MW3Rumble.ini MW3KoreanBridge.log MW3Rumble.log MW3-Gamepad-Rumble-README.txt 2>nul
del /q licenses\MW3-2011-Gamepad-Rumble.txt 2>nul

if exist MW3GamepadFix.asi (
  echo  winmm.dll 은 패드 모드가 사용하므로 남겨 둡니다. / winmm.dll kept for MW3GamepadFix.
) else (
  del /q winmm.dll licenses\Ultimate-ASI-Loader.txt 2>nul
  echo  winmm.dll 도 제거했습니다. / winmm.dll removed.
)
rmdir licenses 2>nul

echo.
echo  제거 완료. 다시 설치하려면 zip 파일을 다시 복사하세요. / Done.
pause
(goto) 2>nul & del "%~f0"
