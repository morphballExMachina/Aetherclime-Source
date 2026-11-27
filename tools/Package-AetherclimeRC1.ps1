<#
.SYNOPSIS
  Assemble the Aetherclime 0.5.0 RC1 two-option FOMOD using a locally compiled Windows x64 DLL.
.DESCRIPTION
  Source preview archives do NOT contain Aetherclime.dll. This command refuses to build
  a release archive without a real PE32+ x64 DLL carrying the validated Alpha-50 milestone.
  Does not touch the active MO2 mod, .git, saves, or user configurations.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Package-AetherclimeRC1.ps1
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Package-AetherclimeRC1.ps1 -BuiltDllPath 'C:\path\Aetherclime.dll'
#>
[CmdletBinding()]
param(
    [string]$BuiltDllPath,
    [string]$OutputDirectory,
    [string]$VcpkgInstalledRoot
)
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuiltDllPath)) {
    $BuiltDllPath = Join-Path $root 'build-skse-static-02\Aetherclime.dll'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root 'release-output'
}
if ([string]::IsNullOrWhiteSpace($VcpkgInstalledRoot)) {
    $VcpkgInstalledRoot = Join-Path (Split-Path -Path $BuiltDllPath -Parent) 'vcpkg_installed'
}

# Exact license hashes and versions verified from two Windows x64-windows-static-md
# Alpha-50/RC1 build trees on 2026-11-25. Fail closed on unreviewed changes.
$ThirdPartyPackages = @(
    [pscustomobject]@{ Name='commonlibsse-ng'; Version='3.7.0'; Hash='F8F983D0D1E2BB361F117487465F21F5F4047ACCA3610F7ABA3482D524C83CE4' },
    [pscustomobject]@{ Name='nlohmann-json'; Version='3.12.0'; Hash='46A65CFFD1EA955132D95A8DD921640714A8D6B537D2E4E482D31145AE95B603' },
    [pscustomobject]@{ Name='spdlog'; Version='1.17.0'; Hash='D678C20826E1392578508E1C78649A1E3C31BDA05EF4388D967DBD654461B140' },
    [pscustomobject]@{ Name='fmt'; Version='12.2.0'; Hash='A65FCC5A1095FC33CC37E4FFF69366E49D7E92F613558A2B95ED1B70E33758B4' },
    [pscustomobject]@{ Name='rapidcsv'; Version='9.07'; Hash='8E60B029B7A16C4087AF1E8DC695E4412F36F46819AA2FE1EF445E7776E40594' },
    [pscustomobject]@{ Name='xbyak'; Version='7.28'; Hash='7E411D5C5D38501A504843F58AFB6824439C828872B35D2FF1E892E73FD1A30D' }
)
function Get-VerifiedThirdPartyLicenses([string]$installedRoot) {
    $triplet = 'x64-windows-static-md'
    $share = Join-Path $installedRoot "$triplet\share"
    $statusPath = Join-Path $installedRoot 'vcpkg\status'
    if (-not (Test-Path -LiteralPath $statusPath -PathType Leaf)) {
        throw "Missing vcpkg status manifest: $statusPath. Supply -VcpkgInstalledRoot if your DLL is outside its build directory."
    }
    $statusText = Get-Content -LiteralPath $statusPath -Raw -Encoding UTF8
    $results = @()
    foreach ($package in $ThirdPartyPackages) {
        $pattern = '(?mi)^Package:\s*' + [regex]::Escape($package.Name) + '\s*\r?\nVersion:\s*' + [regex]::Escape($package.Version) + '(?:\s|$)'
        if (-not [regex]::IsMatch($statusText, $pattern)) {
            throw "Installed package version mismatch: expected $($package.Name) $($package.Version) in $statusPath"
        }
        $source = Join-Path $share "$($package.Name)\copyright"
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Missing dependency license text: $source"
        }
        $foundHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($foundHash -ne $package.Hash) {
            throw "Dependency license hash differs from audited RC1: $($package.Name). Expected $($package.Hash); got $foundHash. Review before packaging."
        }
        $results += [pscustomobject]@{ Name=$package.Name; Version=$package.Version; CopyrightPath=$source; Hash=$foundHash }
    }
    return $results
}
function Assert-JsonConfig([string]$path) {
    $config = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    if ([string]$config.schemaVersion -ne '1') { throw 'Expected config.schemaVersion=1' }
    if ($config.hud.mode -ne 'off' -or [double]$config.hud.cooldownSeconds -ne 90.0 -or
        [double]$config.hud.heartbeatSeconds -ne 0.0) { throw 'Source JSON HUD release defaults are incorrect.' }
    if (-not $config.interior.enabled -or -not $config.interior.estimateUnclassified -or
        -not $config.settings.geographicFallbackEnabled) { throw 'Core climate fallback defaults are disabled.' }
    foreach ($name in @('apiConsumerProbeEnabled','experimentalLiveMonitor','experimentalMovementProbe','eventClimateEnabled',
                         'experimentalBoundaryProbe','experimentalWeatherCorrelationProbe')) {
        if ($config.settings.$name) { throw "Release default must disable settings.$name" }
    }
}
try {
    if (Get-Process -Name SkyrimSE, SkyrimVR -ErrorAction SilentlyContinue) {
        throw 'Close Skyrim before packaging to avoid selecting a DLL from an active development session.'
    }
    if (-not (Test-Path -LiteralPath $BuiltDllPath -PathType Leaf)) { throw "Missing compiled DLL: $BuiltDllPath" }
    $dllBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $BuiltDllPath).Path)
    if ($dllBytes.Length -lt 4096 -or $dllBytes[0] -ne 0x4D -or $dllBytes[1] -ne 0x5A) { throw 'Not a Windows PE DLL (MZ signature missing).' }
    $peOffset = [BitConverter]::ToInt32($dllBytes,0x3C)
    if ($peOffset -lt 64 -or $peOffset + 26 -ge $dllBytes.Length) { throw 'Invalid PE header offset.' }
    if ($dllBytes[$peOffset] -ne 0x50 -or $dllBytes[$peOffset+1] -ne 0x45 -or
        $dllBytes[$peOffset+2] -ne 0 -or $dllBytes[$peOffset+3] -ne 0) { throw 'Invalid PE header signature.' }
    if ([BitConverter]::ToUInt16($dllBytes,$peOffset+4) -ne 0x8664) { throw 'DLL must target Windows x64 (AMD64).' }
    if ([BitConverter]::ToUInt16($dllBytes,$peOffset+24) -ne 0x20B) { throw 'DLL must be PE32+ x64.' }
    $text = [Text.Encoding]::ASCII.GetString($dllBytes)
    if (-not $text.Contains('AETHERCLIME_BUILD_ALPHA50_RELEASE_READINESS')) {
        throw 'Wrong Aetherclime milestone marker: expected validated Alpha-50 engine.'
    }
    $verifiedLicenses = @(Get-VerifiedThirdPartyLicenses $VcpkgInstalledRoot)
    $template = Join-Path $root 'Data\SKSE\Plugins\Aetherclime\config.json'
    if (-not (Test-Path -LiteralPath $template -PathType Leaf)) { throw "Missing default config: $template" }
    Assert-JsonConfig $template
    $xmlRoot = Join-Path $root 'packaging\rc1\fomod'
    $xml = New-Object System.Xml.XmlDocument
    $xml.Load((Join-Path $xmlRoot 'ModuleConfig.xml'))
    if ($xml.config.installSteps.installStep.optionalFileGroups.group.type -ne 'SelectExactlyOne' -or
        $xml.config.installSteps.installStep.optionalFileGroups.group.plugins.plugin.Count -ne 2) {
        throw 'FOMOD must contain exactly one selection group with exactly two options.'
    }
    $staging = Join-Path ([System.IO.Path]::GetTempPath()) ('AetherclimeRC1-' + [guid]::NewGuid().ToString('N'))
    try {
        [void][System.IO.Directory]::CreateDirectory($staging)
        [void][System.IO.Directory]::CreateDirectory((Join-Path $staging 'fomod'))
        Copy-Item -LiteralPath (Join-Path $xmlRoot 'ModuleConfig.xml') -Destination (Join-Path $staging 'fomod\ModuleConfig.xml')
        Copy-Item -LiteralPath (Join-Path $xmlRoot 'info.xml') -Destination (Join-Path $staging 'fomod\info.xml')
        $core = Join-Path $staging 'Core\SKSE\Plugins'
        [void][System.IO.Directory]::CreateDirectory($core)
        Copy-Item -LiteralPath $BuiltDllPath -Destination (Join-Path $core 'Aetherclime.dll')
        $baseText = [System.IO.File]::ReadAllText($template)
        $variants = @{'FrameworkOnly'='off'; 'MinimalHUD'='minimal'}
        foreach ($variant in $variants.Keys) {
            $directory = Join-Path $staging "Options\$variant\SKSE\Plugins\Aetherclime"
            [void][System.IO.Directory]::CreateDirectory($directory)
            $pattern = '(?m)("mode"\s*:\s*)"off"'
            $matches = [regex]::Matches($baseText,$pattern)
            if ($matches.Count -ne 1) { throw "Expected exactly one HUD mode setting; found $($matches.Count)." }
            $contents = [regex]::Replace($baseText,$pattern,('$1"' + $variants[$variant] + '"'))
            $configPath = Join-Path $directory 'config.json'
            [System.IO.File]::WriteAllText($configPath,$contents,(New-Object System.Text.UTF8Encoding($false)))
            $parsed = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json -ErrorAction Stop
            if ($parsed.hud.mode -ne $variants[$variant] -or [double]$parsed.hud.cooldownSeconds -ne 90 -or
                [double]$parsed.hud.heartbeatSeconds -ne 0 -or $parsed.settings.apiConsumerProbeEnabled) {
                throw "Invalid settings in FOMOD preset $variant"
            }
        }
        Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $staging 'LICENSE.txt')
        $docs = Join-Path $staging 'Documentation'
        [void][System.IO.Directory]::CreateDirectory($docs)
        # Preserve verbatim license/copyright texts from the actual vcpkg build tree.
        # This directory is part of the ZIP even though the FOMOD installs only the DLL/JSON.
        $licenseDirectory = Join-Path $docs 'ThirdPartyLicenses'
        [void][System.IO.Directory]::CreateDirectory($licenseDirectory)
        foreach ($entry in $verifiedLicenses) {
            Copy-Item -LiteralPath $entry.CopyrightPath -Destination (Join-Path $licenseDirectory "$($entry.Name).txt")
        }
        Copy-Item -LiteralPath (Join-Path $root 'docs\release\THIRD_PARTY_NOTICES.md') -Destination (Join-Path $docs 'THIRD_PARTY_NOTICES.md')
        Copy-Item -LiteralPath (Join-Path $root 'docs\release\THIRD_PARTY_NOTICES.md') -Destination (Join-Path $staging 'THIRD_PARTY_NOTICES.md')
        foreach ($document in @('CONFIGURATION.md','INSTALL_COMPATIBILITY.md','API_QUICK_START.md','API_REFERENCE.md',
                                 'CLIMATE_MODEL_PLAIN_ENGLISH.md','GEOGRAPHY_AND_ELEVATION.md','CALIBRATION_EXAMPLES.md','NEXUS_PAGE_RC1.md','NEXUS_TAGS_AND_DISCLOSURE.md','FOMOD_INSTALLATION_RC1.md','RC1_RELEASE_GATES.md',
                                 'TROUBLESHOOTING.md','BUG_REPORTING.md','RELEASE_PERMISSIONS.md')) {
            $path = Join-Path $root "docs\release\$document"
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing documentation: $path" }
            Copy-Item -LiteralPath $path -Destination (Join-Path $docs $document)
        }
        $readme = @'
Aetherclime 0.5.0 Release Candidate 1

This is the FOMOD runtime installer; install the ZIP in MO2/Vortex, NOT by extracting it directly into Data.
Framework Only (recommended) installs a silent HUD-off config; Minimal HUD is optional.
Change hud.mode later in SKSE\Plugins\Aetherclime\config.json.
WARNING: Reinstalling/rerunning the FOMOD can overwrite customized config.json. Back it up first.
No ESP/ESL/ESM, no API probe DLL, and no development build files are included.
See Documentation/INSTALL_COMPATIBILITY.md for requirements and troubleshooting.
'@
        [System.IO.File]::WriteAllText((Join-Path $staging 'README-FIRST.txt'),$readme)
        [void][System.IO.Directory]::CreateDirectory($OutputDirectory)
        $output = Join-Path $OutputDirectory 'Aetherclime-0.5.0-RC1-FOMOD.zip'
        if (Test-Path -LiteralPath $output) { throw "Output already exists: $output (move it before rebuilding; refusing silent overwrite)." }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory($staging,$output,[System.IO.Compression.CompressionLevel]::Optimal,$false)
        $zip = [System.IO.Compression.ZipFile]::OpenRead($output)
        try {
            $entries = @($zip.Entries | ForEach-Object { $_.FullName -replace '\\','/' })
            foreach ($required in @('fomod/info.xml','fomod/ModuleConfig.xml','Core/SKSE/Plugins/Aetherclime.dll',
                  'Options/FrameworkOnly/SKSE/Plugins/Aetherclime/config.json',
                  'Options/MinimalHUD/SKSE/Plugins/Aetherclime/config.json',
                  'THIRD_PARTY_NOTICES.md','Documentation/THIRD_PARTY_NOTICES.md',
                  'Documentation/ThirdPartyLicenses/commonlibsse-ng.txt',
                  'Documentation/ThirdPartyLicenses/nlohmann-json.txt',
                  'Documentation/ThirdPartyLicenses/spdlog.txt',
                  'Documentation/ThirdPartyLicenses/fmt.txt',
                  'Documentation/ThirdPartyLicenses/rapidcsv.txt',
                  'Documentation/ThirdPartyLicenses/xbyak.txt')) {
                if ($entries -notcontains $required) { throw "Missing ZIP member: $required" }
            }
            if ($entries | Where-Object { $_ -match '(?i)AetherclimeApiProbe\.dll$|\.(esm|esp|esl)$' }) { throw 'Unexpected game plugin or developer probe in runtime package.' }
        } finally { $zip.Dispose() }
        $digest = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
        [System.IO.File]::WriteAllText("$output.sha256","$digest  $(Split-Path $output -Leaf)`r`n")
        Write-Host '[OK] Included six exact third-party license notices from audited vcpkg dependencies.' -ForegroundColor Green
        Write-Host "[OK] FOMOD runtime archive: $output" -ForegroundColor Green
        Write-Host "[OK] SHA256: $digest" -ForegroundColor Green
        Write-Host '[NEXT] Install each FOMOD choice in a TEST MO2 mod and check its SKSE/Plugins payload before uploading to Nexus.' -ForegroundColor Cyan
    } finally {
        if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    }
} catch {
    Write-Host "[FAIL] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
