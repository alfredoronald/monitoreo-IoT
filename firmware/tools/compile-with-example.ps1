param(
    [string]$PlatformIO = 'platformio',
    [string[]]$Environments = @('esp32dev', 'esp32dev-mq135-demo'),
    [string]$AdditionalSourceFlags = ''
)

# SOLO COMPILACION. Utiliza la plantilla existente, no crea config.h, no hace
# upload y separa los binarios de ejemplo de los builds normales.
$ErrorActionPreference = 'Stop'
$firmwareDirectory = Split-Path -Parent $PSScriptRoot
if (Test-Path -LiteralPath (Join-Path $firmwareDirectory 'include/config.h')) {
    throw 'Existe config.h privado. Usar la compilacion normal; este helper solo prueba sin secretos.'
}
$template = (Join-Path $firmwareDirectory 'include/config.example.h').Replace('\', '/')
$originalSourceFlags = $env:PLATFORMIO_BUILD_SRC_FLAGS
$originalBuildDirectory = $env:PLATFORMIO_BUILD_DIR
try {
    $env:PLATFORMIO_BUILD_SRC_FLAGS = '-include "' + $template + '" ' + $AdditionalSourceFlags
    $env:PLATFORMIO_BUILD_DIR = Join-Path $firmwareDirectory '.pio/example-build'
    $arguments = @('run', '-d', $firmwareDirectory)
    foreach ($environment in $Environments) {
        if ($environment -notin @('esp32dev', 'esp32dev-mq135-demo')) {
            throw "Entorno no permitido por este helper: $environment"
        }
        $arguments += @('-e', $environment)
    }
    Write-Output 'SOLO COMPILACION CON config.example.h: no valida conexiones ni hardware. NO CARGAR estos binarios.'
    & $PlatformIO @arguments
    $buildExit = $LASTEXITCODE
} finally {
    $env:PLATFORMIO_BUILD_SRC_FLAGS = $originalSourceFlags
    $env:PLATFORMIO_BUILD_DIR = $originalBuildDirectory
}
exit $buildExit
