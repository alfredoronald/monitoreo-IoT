param(
    [string]$Compiler = 'g++',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../.pio/desktop-tests')
)

$ErrorActionPreference = 'Stop'
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
$firmwareDirectory = Split-Path -Parent $PSScriptRoot
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$originalPath = $env:PATH
$summary = @('PRUEBAS DE ESCRITORIO: datos sinteticos; no validacion fisica.',
             "Compilador: $compilerPath", 'Opciones: -std=c++11 -Wall -Wextra -Werror -pedantic')
$failedSuites = 0
try {
    $env:PATH = (Split-Path -Parent $compilerPath) + ';' + $env:PATH
    foreach ($test in (Get-ChildItem -LiteralPath (Join-Path $firmwareDirectory 'tests') -Filter '*_test.cpp' | Sort-Object Name)) {
        $executable = Join-Path $outputPath ($test.BaseName + '.exe')
        $buildLog = Join-Path $outputPath ($test.BaseName + '-build.txt')
        $testLog = Join-Path $outputPath ($test.BaseName + '.txt')
        & $compilerPath -std=c++11 -Wall -Wextra -Werror -pedantic $test.FullName -o $executable 2>&1 |
            Set-Content -Encoding utf8 -LiteralPath $buildLog
        $compileExit = $LASTEXITCODE
        if ($compileExit -ne 0) {
            $failedSuites++
            $summary += "$($test.Name): COMPILACION FALLIDA; exit=$compileExit"
            Get-Content -LiteralPath $buildLog
            continue
        }
        & $executable 2>&1 | Set-Content -Encoding utf8 -LiteralPath $testLog
        $testExit = $LASTEXITCODE
        if ($testExit -ne 0) { $failedSuites++ }
        $lastLine = Get-Content -Encoding utf8 -LiteralPath $testLog | Select-Object -Last 1
        $summary += "$($test.Name): compilacion=0; ejecucion=$testExit; $lastLine"
    }
} finally {
    $env:PATH = $originalPath
}
$summary += "Suites fallidas: $failedSuites"
$summary | Set-Content -Encoding utf8 -LiteralPath (Join-Path $outputPath 'summary.txt')
$summary | Write-Output
if ($failedSuites -ne 0) { exit 1 }
