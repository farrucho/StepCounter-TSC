# StepCounter-TSC

Compilar e dar Upload: 
./compile.sh operate_highmode.ino 

Extrair dados para ficheiro:
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200 >> "filename.txt"

Ver Real Time detection:
python dashboard.py