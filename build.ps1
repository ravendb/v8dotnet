param(
    $Target="",
    [switch]$WinX64,
    [switch]$WinX86,
    [switch]$LinuxX64,
    [switch]$MacOs,
    [switch]$Osx,
    [switch]$Rpi,
    [switch]$LinuxArm64,
    [switch]$DontRebuildStudio,
    [switch]$DontBuildStudio,
    [switch]$JustStudio,
    [switch]$JustNuget,
    [switch]$Debug,
    [switch]$NoBundling,
    [switch]$DryRunVersionBump = $false,
    [switch]$DryRunSign = $false,
    [switch]$Help)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

. '.\scripts\env.ps1'
. '.\scripts\checkLastExitCode.ps1'
. '.\scripts\checkPrerequisites.ps1'
. '.\scripts\restore.ps1'
. '.\scripts\clean.ps1'
. '.\scripts\archive.ps1'
. '.\scripts\package.ps1'
. '.\scripts\buildProjects.ps1'
. '.\scripts\getScriptDirectory.ps1'
. '.\scripts\copyAssets.ps1'
. '.\scripts\validateAssembly.ps1'
. '.\scripts\validateRuntimeConfig.ps1'
#. '.\scripts\version.ps1'
. '.\scripts\updateSourceWithBuildInfo.ps1'
. '.\scripts\nuget.ps1'
. '.\scripts\target.ps1'
. '.\scripts\help.ps1'
. '.\scripts\sign.ps1'
#. '.\scripts\docker.ps1'
#. '.\scripts\schemaInfo.ps1'
. '.\scripts\runtime.ps1'
. '.\scripts\Get-DestinationFilePath.ps1'
. '.\scripts\Copy-FileHash.ps1'
. '.\scripts\exec.ps1'

if ($Help) {
    Help
}

if ($Osx) {
    $MacOs = $true
}

CheckPrerequisites

if ($Debug.IsPresent) {
    $BUILD_TYPE = "Debug"
} else {
    $BUILD_TYPE = "Release"
}

$PROJECT_DIR = Get-ScriptDirectory
$RELEASE_DIR = [io.path]::combine($PROJECT_DIR, "artifacts")
$OUT_DIR = [io.path]::combine($PROJECT_DIR, "artifacts")

$V8DOTNET_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Source", "V8.Net")
$V8DOTNET_PROJ_PATH = [io.path]::combine($V8DOTNET_SRC_DIR, "V8.Net-Standard.csproj")
$V8DOTNET_OUT_DIR = [io.path]::combine($V8DOTNET_SRC_DIR, "bin", $BUILD_TYPE)

$V8NET_PROXY_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Source", "V8.NET-Proxy")
#$V8NET_PROXY_PROJ_PATH = [io.path]::combine($V8NET_PROXY_SRC_DIR, "V8.Net-Proxy-x64-Standard.csproj")
$V8NET_PROXY_OUT_DIR = [io.path]::combine($V8NET_PROXY_SRC_DIR, "bin", $BUILD_TYPE)


$TEST_ASPDOTNET_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "ASPNetTest")
$TEST_ASPDOTNET_PROJ_PATH = [io.path]::combine($TEST_ASPDOTNET_SRC_DIR, "ASPNetTest.csproj")
$TEST_ASPDOTNET_OUT_DIR = [io.path]::combine($TEST_ASPDOTNET_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLE_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "V8.NET-Console")
$TEST_V8NET_CONSOLE_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLE_SRC_DIR, "V8.Net-Console.csproj")
$TEST_V8NET_CONSOLE_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLE_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLE_HANDLE_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "V8.NET-Console-HandleTests")
$TEST_V8NET_CONSOLE_HANDLE_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLE_HANDLE_SRC_DIR, "V8.NET-Console-HandleTests.csproj")
$TEST_V8NET_CONSOLE_HANDLE_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLE_HANDLE_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLE_NET_CORE_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "V8.NET-Console-NetCore")
$TEST_V8NET_CONSOLE_NET_CORE_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLE_NET_CORE_SRC_DIR, "V8.NET-Console-NetCore.csproj")
$TEST_V8NET_CONSOLE_NET_CORE_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLE_NET_CORE_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLE_CORE_NUGET_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "V8.NET-Console-NetCore-NuGet")
$TEST_V8NET_CONSOLE_CORE_NUGET_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLE_CORE_NUGET_SRC_DIR, "V8.NET-Console-NetCore-NuGet.csproj")
$TEST_V8NET_CONSOLE_CORE_NUGET_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLE_CORE_NUGET_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLEOBJECT_EXAMPLES_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "V8.NET-Console-ObjectExamples")
$TEST_V8NET_CONSOLEOBJECT_EXAMPLES_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLEOBJECT_EXAMPLES_SRC_DIR, "V8.NET-Console-ObjectExamples.csproj")
$TEST_V8NET_CONSOLEOBJECT_EXAMPLES_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLEOBJECT_EXAMPLES_SRC_DIR, "bin", $BUILD_TYPE)

$TEST_V8NET_CONSOLE_SERVICE_SRC_DIR = [io.path]::combine($PROJECT_DIR, "Tests", "WCFServiceTest")
$TEST_V8NET_CONSOLE_SERVICE_PROJ_PATH = [io.path]::combine($TEST_V8NET_CONSOLE_SERVICE_SRC_DIR, "WCFServiceTest.csproj")
$TEST_V8NET_CONSOLE_SERVICE_OUT_DIR = [io.path]::combine($TEST_V8NET_CONSOLE_SERVICE_SRC_DIR, "bin", $BUILD_TYPE)


if ([string]::IsNullOrEmpty($Target) -eq $false) {
    $Target = $Target.Split(",")
} else {
    $Target = $null

    if ($WinX64) {
        $Target = @( "win-x64" );
    }

    if ($WinX86) {
        $Target = @( "win-x86" );
    }

    if ($LinuxX64) {
        $Target = @( "linux-x64" );
    } 

    if ($MacOs) {
        $Target = @( "macos" );
    }

    if ($Rpi) {
        $Target = @( "rpi" );
    }

    if ($LinuxArm64) {
        $Target = @( "linux-arm64" );
    }
}

$targets = GetBuildTargets $Target

if ($targets.Count -eq 0) {
    write-host "No targets specified."
    exit 0;
} else {
    Write-Host -ForegroundColor Magenta "Build targets: $($targets.Name)"
}

New-Item -Path $RELEASE_DIR -Type Directory -Force
CleanFiles $RELEASE_DIR

#$embeddedDir = Join-Path -Path $OUT_DIR -ChildPath "V8Net.Embedded"
#if (Test-Path $embeddedDir) {
#    CleanDir $embeddedDir 
#}

CleanSrcDirs $TYPINGS_GENERATOR_SRC_DIR, $RVN_SRC_DIR, $DRTOOL_SRC_DIR, $SERVER_SRC_DIR, $CLIENT_SRC_DIR, $V8DOTNET_SRC_DIR, $TESTDRIVER_SRC_DIR

#LayoutDockerPrerequisites $PROJECT_DIR $RELEASE_DIR

#$versionObj = SetVersionInfo
#$version = $versionObj.Version
#$versionSuffix = $versionObj.VersionSuffix
#$buildNumber = $versionObj.BuildNumber
#$buildType = $versionObj.BuildType.ToLower()
#Write-Host -ForegroundColor Green "Building $version"

#SetSchemaInfoInTeamCity $PROJECT_DIR

#ValidateClientDependencies $CLIENT_SRC_DIR $V8DOTNET_SRC_DIR
#UpdateSourceWithBuildInfo $PROJECT_DIR $buildNumber $version

InitGlobals $Debug $NoBundling
#DownloadDependencies

if ($JustStudio -eq $False) {
    BuildV8NetProxy $V8NET_PROXY_SRC_DIR $BUILD_TYPE
    BuildV8DotNet $V8DOTNET_PROJ_PATH $BUILD_TYPE

    #BuildV8NetTest $TEST_ASPDOTNET_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_V8NET_CONSOLE_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_V8NET_CONSOLE_HANDLE_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_V8NET_CONSOLE_NET_CORE_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_V8NET_CONSOLE_NET_CORE_NUGET_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_V8NET_CONSOLE_OBJECT_EXAMPLES_PROJ_PATH $BUILD_TYPE
    #BuildV8NetTest $TEST_WCF_SERVICE_PROJ_PATH $BUILD_TYPE

    #CreateNugetPackage $CLIENT_SRC_DIR $RELEASE_DIR $versionSuffix
}

#$IsPosix = $IsWindows -eq $False
#if (($JustStudio -eq $False) -and ($IsPosix -eq $False)) {
#    $studioZipPath = [io.path]::combine($STUDIO_OUT_DIR, "Raven.Studio.zip")
#    BuildEmbeddedNuget $PROJECT_DIR $OUT_DIR $SERVER_SRC_DIR $studioZipPath
#    $embeddedDir = [io.path]::combine($OUT_DIR, "V8Net.Embedded")
#    if ($target.Name -eq "windows-x64") {
#        Validate-AssemblyVersion $(Join-Path -Path $embeddedDir -ChildPath "lib/netstandard2.0/Raven.Embedded.dll" ) $versionObj
#    }
#
#    $nupkgs = Join-Path $embeddedDir -ChildPath "*.nupkg"
#    Move-Item -Path $nupkgs -Destination $OUT_DIR
#    Remove-Item -Recurse $embeddedDir
#} else {
#    write-host "Skip building V8Net Embedded."
#}

Foreach ($target in $targets) {
    $specOutDir = [io.path]::combine($OUT_DIR, $target.Name)
    CleanDir $specOutDir

    $specOutDirs = @{
        "Main" = $specOutDir;
        "V8Net" = $V8DOTNET_OUT_DIR;
        "V8NetProxy" = $V8NET_PROXY_OUT_DIR;
    }

    #ValidateRuntimeConfig $target $specOutDirs.Server

    if ($target.Name -eq "windows-x64") {
        #Validate-ExecutableVersion $(Join-Path -Path $specOutDirs.V8Net -ChildPath "Raven.Server.exe" ) $versionObj
        #Validate-AssemblyVersion $(Join-Path -Path $specOutDirs.Client -ChildPath "netstandard2.0/Raven.Client.dll" ) $versionObj
    }

    $packOpts = @{
        "Target" = $target;
        "DryRunSign" = $DryRunSign;
        "VersionInfo" = $versionObj;
        "OutDirs" = $specOutDirs;
    }
    
    #CreatePackage $PROJECT_DIR $RELEASE_DIR $packOpts
}

write-host "Done creating packages."


