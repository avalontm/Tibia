[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build')]
    [string] $Action = 'Build',

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string] $Configuration = 'Release',

    [string] $BuildDirectory = 'build-msvc'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
}
$repoRootPrefix = $repoRoot.TrimEnd('\') + '\'
if ($buildDirectory -ne $repoRoot -and
    -not $buildDirectory.StartsWith($repoRootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory debe estar dentro del repositorio.'
}
$vswhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
)

$vswhere = $vswhereCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vswhere) {
    throw 'No encontré vswhere.exe. Instala Visual Studio o Visual Studio Build Tools.'
}

# Require the actual x86/x64 C++ compiler component. This skips VS instances
# that only have managed build tools installed.
$vsInstallPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsInstallPath) {
    throw 'No encontré una instancia de Visual Studio con el compilador MSVC x86/x64.'
}

$vsInstallPath = $vsInstallPath.Trim()
$vcvars = Join-Path $vsInstallPath 'VC\Auxiliary\Build\vcvarsall.bat'
$cmake = Get-Command 'cmake' -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "La instalación detectada no contiene vcvarsall.bat: $vsInstallPath"
}
if (-not $cmake) {
    throw 'No encontré CMake en PATH. Instala CMake o habilita el componente CMake de Visual Studio.'
}

Write-Host "Visual Studio: $vsInstallPath"
Write-Host "CMake: $($cmake.Source)"
Write-Host "Configuración: $Configuration"

$configureArguments = @(
    '-S', $repoRoot,
    '-B', $buildDirectory,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    "-DCMAKE_GENERATOR_INSTANCE=$vsInstallPath"
)

function Invoke-CleanEnvironmentProcess {
    param([Parameter(Mandatory)][string] $Executable, [Parameter(Mandatory)][string[]] $Arguments)

    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.UseShellExecute = $false
    # Rebuild the child environment from PowerShell's normalized view. The
    # desktop parent exposes both PATH and Path, which breaks MSBuild's
    # case-insensitive .NET Framework environment dictionary when it starts cl.exe.
    $start.EnvironmentVariables.Clear()
    Get-ChildItem Env: | ForEach-Object {
        # StringDictionary is case-insensitive, so Path/PATH collapse to one key.
        $start.EnvironmentVariables[[string]$_.Name] = [string]$_.Value
    }
    # Windows PowerShell 5.1 runs on .NET Framework, whose ProcessStartInfo has
    # no ArgumentList property. Quote the known CMake arguments for CreateProcess.
    $start.Arguments = (($Arguments | ForEach-Object {
        '"' + ([string]$_).Replace('"', '\"') + '"'
    }) -join ' ')

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) { throw "No pude iniciar: $Executable" }
    $process.WaitForExit()
    return $process.ExitCode
}

$exitCode = Invoke-CleanEnvironmentProcess -Executable $cmake.Source -Arguments $configureArguments
if ($exitCode -ne 0) { exit $exitCode }

if ($Action -eq 'Build') {
    $exitCode = Invoke-CleanEnvironmentProcess -Executable $cmake.Source -Arguments @(
        '--build', $buildDirectory, '--config', $Configuration
    )
    exit $exitCode
}
