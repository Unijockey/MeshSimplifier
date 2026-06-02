@echo off
setlocal
set "ROOT=%~dp0"
set "PYTHONHOME=%ROOT%python"
set "PYTHONPATH=%ROOT%python\Lib;%ROOT%python\Lib\site-packages;%ROOT%app"
set "PATH=%ROOT%python;%ROOT%python\DLLs;%ROOT%python\Lib\site-packages\PyQt5\Qt5\bin;%ROOT%python\Lib\site-packages\vtk.libs;%ROOT%python\Lib\site-packages\numpy.libs;%PATH%"
set "QT_PLUGIN_PATH=%ROOT%python\Lib\site-packages\PyQt5\Qt5\plugins"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%ROOT%python\Lib\site-packages\PyQt5\Qt5\plugins\platforms"
cd /d "%ROOT%app"
start "" "%ROOT%python\pythonw.exe" "mesh_simplifier.py"
