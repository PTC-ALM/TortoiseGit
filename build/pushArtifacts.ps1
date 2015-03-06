# Use Bypass so script does not need to be digitally signed
Set-ExecutionPolicy Bypass -Force

$version = $Env:APPVEYOR_BUILD_VERSION

# dot source so we can access the $countryCodes hash table/dict variable 
. ./build/countryCodes.ps1

# If building Win32 version, upload TortoiseSI32.dll
if($Env:Platform -eq "Win32")
{
	$root = Resolve-Path .

    Write-host "Uploading TortoiseSI32.dll, TortoiseSIStub32.dll and 32 bit installers"	

	Push-AppveyorArtifact "$root\bin\Release\bin\TortoiseSI32.dll"
	Push-AppveyorArtifact "$root\bin\Release\bin\TortoiseSIStub32.dll"
	
	foreach ($code in $countryCodes.GetEnumerator()) {
        # upload language DLLs
		Push-AppveyorArtifact "$root\bin\Release\bin\TortoiseShell$($code.Key).dll"
        # upload 32-bit language installers
        Push-AppveyorArtifact "$root\bin\setup\x86\TortoiseGit-LanguagePack-$($code.Value).msi" -FileName "TortoiseGit-LanguagePack-$($code.Value)-x86.msi"
	}
	
	Push-AppveyorArtifact "$root\bin\setup\x86\TortoiseSI.msi" -FileName "TortoiseSI-$($version)-x86.msi"	
	
}
# if building x64 version, upload installer
elseif($Env:Platform -eq "x64")
{
	$root = Resolve-Path .

    Write-host "Uploading TortoiseSI 64 bit installers"

	Push-AppveyorArtifact "$root\bin\setup\x64\TortoiseSI.msi" -FileName "TortoiseSI-$($version)-x64.msi"
    
    foreach ($code in $countryCodes.GetEnumerator()) {
        # upload 32-bit language installers
        Push-AppveyorArtifact "$root\bin\setup\x64\TortoiseGit-LanguagePack-$($code.Value).msi" -FileName "TortoiseGit-LanguagePack-$($code.Value)-x64.msi"
	}
    
}
else
{
	$platform = $Env:Platform
	Write-host "No artifacts defined for platform $platform"
}