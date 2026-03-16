set Envs=ESP32_Without_LoRa LILYGO_T3_LoRa32_V1_6_1 LILYGO_T-Beam HELTEC_Wireless_Stick_Lite_V3 HELTEC_WiFi_LoRa_32_V3 HELTEC_WiFi_LoRa_32_V4 LILYGO_T-LoraPager
set VersionOLD=V1.0.22

echo %Version%
pause

for %%n in (%Envs%) do (
	C:\Users\DennisEisold\.platformio\penv\Scripts\platformio.exe run --environment %%n
	C:\Users\DennisEisold\.platformio\penv\Scripts\platformio.exe run --target buildfs --environment %%n
    mkdir "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n" 2>nul
    mkdir "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n\%VersionOLD%" 2>nul
    xcopy "updateServer\rMesh\%%n\image.webp" "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n" /Y /I
    xcopy ".pio\build\%%n\*.bin" "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n" /Y /I
    xcopy ".pio\build\%%n\*.bin" "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n\%VersionOLD%" /Y /I
    powershell -Command "Compress-Archive -Path '.pio\build\%%n\*.bin' -DestinationPath 'C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n\%%n.zip' -Force"
    xcopy "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n\%%n.zip" "C:\Users\DennisEisold\Nextcloud\rMesh\bin\%%n\%VersionOLD%" /Y /I
)

xcopy "changelog.txt" "C:\Users\DennisEisold\Nextcloud\rMesh\bin" /Y /I

pause
