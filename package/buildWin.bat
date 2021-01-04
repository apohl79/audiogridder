cd ..
del /F /Q build-win-10-x86_64
cmake -B build-win-10-x86_64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_ROOT=z:/ag-deps-win-x86_64
cmake --build build-win-10-x86_64 --config RelWithDebInfo --clean-first -j4

cd package
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderPlugin.iss
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderServer.iss

call archiveWin.bat
