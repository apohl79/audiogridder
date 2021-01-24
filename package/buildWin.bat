cd ..
del /F /S /Q build-win-10-x86_64
cmake -B build-win-10-x86_64 -A x64 -DFFMPEG_ROOT=z:/ag-deps-win-x86_64
cmake --build build-win-10-x86_64 --config RelWithDebInfo -j6

cd package
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderPlugin.iss
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderServer.iss

call archiveWin.bat
