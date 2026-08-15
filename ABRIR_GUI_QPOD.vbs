Set objShell = CreateObject("WScript.Shell")
objShell.CurrentDirectory = "f:\Documentos\Jota\jRobot\mini Q-Pod"
objShell.Run "cmd /c cd /d ""f:\Documentos\Jota\jRobot\mini Q-Pod"" && pip install pyserial >nul 2>&1 && python gui_qpod_config.py", 0
