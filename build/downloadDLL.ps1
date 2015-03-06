# Use Bypass so script does not need to be digitally signed
Set-ExecutionPolicy Bypass -Force

if ($Env:Platform -eq "Win32") {
    Write-host "Using Win32: Don't need to download dll. Exiting script."
    exit
}

# dot source so we can access the $countryCodes hash table/dict variable 
. ./build/countryCodes.ps1


$apiUrl = 'https://ci.appveyor.com/api'
$headers = @{
  "Authorization" = "Bearer $Env:secureToken"
  "ContentType" = "application/json"
}

$buildVersion = $Env:APPVEYOR_BUILD_VERSION
$accountName = "PTC-ALM"
$projectSlug = "TortoiseSI"

# determine root directory
$root = Resolve-Path .
$downloadLocation = "$root\bin\Release\bin\"

# get project based on current build version
$project = Invoke-RestMethod -Method Get -Uri "$apiURL/projects/$accountName/$projectSlug/build/$buildVersion" -Headers $headers

# get job id for Win32 job for current build
# Note this presumes that Win32 was the first job to run
$jobId = $project.build.jobs[0].jobId

# get artifacts from Win32 job
$artifacts = Invoke-RestMethod -Method Get -Uri "$apiUrl/buildJobs/$jobId/artifacts" -Headers $headers

# specify artifact file name and copy location
$artifactFileName1 = "TortoiseSI32.dll"
$localArtifactPath1 = "$downloadLocation\$artifactFileName1"

$artifactFileName2 = "TortoiseSIStub32.dll"
$localArtifactPath2 = "$downloadLocation\$artifactFileName2"

# if download location doesn't exist, create it
if (!(Test-Path -Path $downloadLocation)) {
    Write-host "creating $downloadLocation"
    New-Item -ItemType directory -Path $downloadLocation
}

# download TortoiseSI32.dll to download location
Invoke-RestMethod -Method Get -Uri "$apiUrl/buildjobs/$jobId/artifacts/$artifactFileName1" `
     -OutFile $localArtifactPath1 -Headers $headers

Write-host "Copied $artifactFileName1 to $downloadLocation"

# download TortoiseSIStub32.dll to download location
Invoke-RestMethod -Method Get -Uri "$apiUrl/buildjobs/$jobId/artifacts/$artifactFileName2" `
     -OutFile $localArtifactPath2 -Headers $headers

Write-host "Copied $artifactFileName2 to $downloadLocation"

foreach ($code in $countryCodes.GetEnumerator()) {
    $langDLLArtifact = "TortoiseShell$($code.Key).dll"
    $localLangDLLArtifactPath = "$downloadLocation\$langDLLArtifact"

    Invoke-RestMethod -Method Get -Uri "$apiUrl/buildjobs/$jobId/artifacts/$langDLLArtifact" `
     -OutFile $localLangDLLArtifactPath -Headers $headers


}
# download TortoiseSI32.dll to download location
Invoke-RestMethod -Method Get -Uri "$apiUrl/buildjobs/$jobId/artifacts/$artifactFileName1" `
     -OutFile $localArtifactPath1 -Headers $headers