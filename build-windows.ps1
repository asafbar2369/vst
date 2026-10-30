param(
    [string]$JucePath = ""
)

$ErrorActionPreference = "Stop"
$configureArgs = @("-S", ".", "-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64")

if ($JucePath -ne "") {
    $resolvedJucePath = (Resolve-Path $JucePath).Path
    if (-not (Test-Path (Join-Path $resolvedJucePath "CMakeLists.txt"))) {
        throw "JucePath must point to the JUCE source root containing CMakeLists.txt"
    }
    $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$resolvedJucePath"
}

cmake @configureArgs
cmake --build build --config Release --target RMX16_Reconstruction_VST3

$plugin = Get-ChildItem -Path "build" -Filter "*.vst3" -Recurse -Directory | Select-Object -First 1
if ($null -eq $plugin) {
    throw "Build completed but no .vst3 bundle was found under build/"
}
Write-Host "Built VST3 bundle: $($plugin.FullName)"
