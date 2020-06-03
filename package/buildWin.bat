call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"

msbuild ..\Server\Builds\VisualStudio2019\AudioGridderServer.sln /nologo /p:Configuration=Release /p:Platform=x64 /t:Clean
msbuild ..\Server\Builds\VisualStudio2019\AudioGridderServer.sln /nologo /p:Configuration=Release /p:Platform=x64
msbuild ..\Plugin\Fx\Builds\VisualStudio2019\AudioGridder.sln /nologo /p:Configuration=Release /p:Platform=x64 /t:Clean
msbuild ..\Plugin\Fx\Builds\VisualStudio2019\AudioGridder.sln /nologo /p:Configuration=Release /p:Platform=x64
msbuild ..\Plugin\Inst\Builds\VisualStudio2019\AudioGridderInst.sln /nologo /p:Configuration=Release /p:Platform=x64 /t:Clean
msbuild ..\Plugin\Inst\Builds\VisualStudio2019\AudioGridderInst.sln /nologo /p:Configuration=Release /p:Platform=x64

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridder.iss

pause
