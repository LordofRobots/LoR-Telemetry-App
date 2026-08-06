$ErrorActionPreference = 'Stop'

$ProjectRoot = $PSScriptRoot

$AndroidSdk = $env:ANDROID_SDK_ROOT
if ([string]::IsNullOrWhiteSpace($AndroidSdk)) {
    $AndroidSdk = $env:ANDROID_HOME
}
if ([string]::IsNullOrWhiteSpace($AndroidSdk)) {
    $AndroidSdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
}
if (-not (Test-Path -LiteralPath $AndroidSdk -PathType Container)) {
    throw 'Android SDK not found. Set ANDROID_SDK_ROOT or ANDROID_HOME.'
}

$BuildToolsRoot = Join-Path $AndroidSdk 'build-tools'
$BuildTools = Get-ChildItem -LiteralPath $BuildToolsRoot -Directory |
    Sort-Object { try { [version]$_.Name } catch { [version]'0.0' } } -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if ([string]::IsNullOrWhiteSpace($BuildTools)) {
    throw "No Android build tools found under $BuildToolsRoot"
}

$PlatformRoot = Join-Path $AndroidSdk 'platforms'
$AndroidJar = Get-ChildItem -LiteralPath $PlatformRoot -Directory |
    Where-Object { $_.Name -match '^android-\d+$' } |
    Sort-Object { [int]($_.Name -replace '^android-', '') } -Descending |
    ForEach-Object { Join-Path $_.FullName 'android.jar' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($AndroidJar)) {
    throw "No Android platform android.jar found under $PlatformRoot"
}

$JavaHome = $env:JAVA_HOME
if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    $AndroidStudioJbr = Join-Path $env:ProgramFiles 'Android\Android Studio\jbr'
    if (Test-Path -LiteralPath $AndroidStudioJbr -PathType Container) {
        $JavaHome = $AndroidStudioJbr
    }
}
if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    throw 'Java not found. Set JAVA_HOME or install Android Studio with its bundled JBR.'
}
$JavaBin = Join-Path $JavaHome 'bin'

$BuildDir = Join-Path $ProjectRoot 'build'
$OutputDir = Join-Path $ProjectRoot 'output'
$ClassesDir = Join-Path $BuildDir 'classes'
$DexDir = Join-Path $BuildDir 'dex'
$GeneratedDir = Join-Path $BuildDir 'generated'
$CompiledDir = Join-Path $BuildDir 'compiled-resources'
$UnsignedApk = Join-Path $BuildDir 'dashboard-unsigned.apk'
$AlignedApk = Join-Path $BuildDir 'dashboard-aligned.apk'
$FinalApk = Join-Path $OutputDir 'LoR-Telemetry-debug.apk'
$KeyStore = Join-Path $ProjectRoot 'debug.keystore'

if (Test-Path -LiteralPath $BuildDir) {
    $resolvedBuild = (Resolve-Path -LiteralPath $BuildDir).Path
    $resolvedRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
    if (-not $resolvedBuild.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean build directory outside project: $resolvedBuild"
    }
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

New-Item -ItemType Directory -Path $ClassesDir, $DexDir, $GeneratedDir, $CompiledDir, $OutputDir -Force | Out-Null

$Aapt2 = Join-Path $BuildTools 'aapt2.exe'
$Aapt = Join-Path $BuildTools 'aapt.exe'
$D8 = Join-Path $BuildTools 'd8.bat'
$ZipAlign = Join-Path $BuildTools 'zipalign.exe'
$ApkSigner = Join-Path $BuildTools 'apksigner.bat'
$Javac = Join-Path $JavaBin 'javac.exe'
$KeyTool = Join-Path $JavaBin 'keytool.exe'

foreach ($RequiredTool in @($Aapt2, $Aapt, $D8, $ZipAlign, $ApkSigner, $Javac, $KeyTool, $AndroidJar)) {
    if (-not (Test-Path -LiteralPath $RequiredTool -PathType Leaf)) {
        throw "Required build tool not found: $RequiredTool"
    }
}

$ResourceDir = Join-Path $ProjectRoot 'res'
& $Aapt2 compile --dir $ResourceDir -o $CompiledDir
if ($LASTEXITCODE -ne 0) { throw "aapt2 compile failed: $LASTEXITCODE" }

$LinkArgs = @(
    'link', '-o', $UnsignedApk,
    '-I', $AndroidJar,
    '--manifest', (Join-Path $ProjectRoot 'AndroidManifest.xml'),
    '--java', $GeneratedDir,
    '--auto-add-overlay',
    '--min-sdk-version', '31',
    '--target-sdk-version', '36',
    '--version-code', '2',
    '--version-name', '2.0'
)
Get-ChildItem -LiteralPath $CompiledDir -Filter '*.flat' -File | ForEach-Object {
    $LinkArgs += @('-R', $_.FullName)
}
& $Aapt2 @LinkArgs
if ($LASTEXITCODE -ne 0) { throw "aapt2 link failed: $LASTEXITCODE" }

$JavaFiles = @(
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'src') -Recurse -Filter '*.java' -File
    Get-ChildItem -LiteralPath $GeneratedDir -Recurse -Filter '*.java' -File -ErrorAction SilentlyContinue
) | ForEach-Object { $_.FullName }

& $Javac -source 8 -target 8 -Xlint:deprecation `
    -bootclasspath $AndroidJar `
    -d $ClassesDir `
    $JavaFiles
if ($LASTEXITCODE -ne 0) { throw "javac failed: $LASTEXITCODE" }

$ClassFiles = Get-ChildItem -LiteralPath $ClassesDir -Recurse -Filter '*.class' -File |
    ForEach-Object { $_.FullName }
& $D8 --lib $AndroidJar --min-api 31 --output $DexDir $ClassFiles
if ($LASTEXITCODE -ne 0) { throw "d8 failed: $LASTEXITCODE" }

Push-Location $DexDir
try {
    & $Aapt add $UnsignedApk 'classes.dex'
    if ($LASTEXITCODE -ne 0) { throw "aapt add failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}

& $ZipAlign -f 4 $UnsignedApk $AlignedApk
if ($LASTEXITCODE -ne 0) { throw "zipalign failed: $LASTEXITCODE" }

if (-not (Test-Path -LiteralPath $KeyStore)) {
    & $KeyTool -genkeypair -keystore $KeyStore -storepass android -keypass android `
        -alias androiddebugkey -dname 'CN=LoR Debug,O=Lord of Robots,C=CA' `
        -keyalg RSA -keysize 2048 -validity 10000 -noprompt
    if ($LASTEXITCODE -ne 0) { throw "keytool failed: $LASTEXITCODE" }
}

& $ApkSigner sign --ks $KeyStore --ks-pass pass:android --key-pass pass:android `
    --out $FinalApk $AlignedApk
if ($LASTEXITCODE -ne 0) { throw "apksigner failed: $LASTEXITCODE" }

& $ApkSigner verify --verbose $FinalApk
if ($LASTEXITCODE -ne 0) { throw "APK verification failed: $LASTEXITCODE" }

Get-Item -LiteralPath $FinalApk | Select-Object FullName, Length, LastWriteTime
