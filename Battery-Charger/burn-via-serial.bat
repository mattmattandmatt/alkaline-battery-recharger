"C:\Program Files (x86)\WinAVR-20100110\bin\avrdude" -p m168 -P COM1 -c arduino -b 19200 -U flash:w:Release\Battery-Charger.hex:i -vvv
@rem pause
