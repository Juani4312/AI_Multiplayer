<#
  aimp_check.ps1 - chequeos, instalación y recolección de logs del mod AI_Multiplayer
  en Windows (laptop de pruebas). Compatible con PowerShell 5.1 (Windows 10).

  Uso, desde la raíz del repo:
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action check
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net none
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer 100.x.y.z:47811
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag primera
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action revert

  "-ExecutionPolicy Bypass" vale solo para ese proceso: no cambia la configuración del sistema.
  Todo lo que hace se agrega al final de work\windows\report.md. Los logs van a work\windows\logs\.
  En la carpeta del juego solo escribe winmm.dll y aimp.cfg (y "revert" los saca).
#>
param(
    [ValidateSet('check', 'install', 'collect', 'revert')] [string]$Action = 'check',
    [ValidateSet('none', 'join', 'host')] [string]$Net = 'none',
    [string]$Peer = '',
    [ValidateRange(0, 3)] [int]$Ghost = 0,
    [string[]]$Extra = @(),
    [string]$Tag = 'corrida'
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Report = Join-Path $PSScriptRoot 'report.md'
$LogsOut = Join-Path $PSScriptRoot 'logs'
$ExpectedSha = 'DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90'
$DistDll = Join-Path $Root 'mod\dist\winmm.dll'
$DistSums = Join-Path $Root 'mod\dist\SHA256SUMS'
$AimpDir = Join-Path $env:LOCALAPPDATA 'AI_Multiplayer'
$Mark = 'AI_Multiplayer'   # texto que está dentro de nuestra DLL

function Out-Report([string]$Text) {
    Add-Content -Path $Report -Value $Text -Encoding UTF8
    Write-Output $Text
}

# Programas nativos: con ErrorActionPreference=Stop, lo que escriben en stderr cortaría el script.
function Invoke-Native([string]$Exe, [string[]]$ArgList) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { return @(& $Exe @ArgList 2>&1 | ForEach-Object { "$_" }) }
    catch { return @("error: $_") }
    finally { $ErrorActionPreference = $old }
}

function Get-Tailscale {
    $c = Get-Command tailscale -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $p = 'C:\Program Files\Tailscale\tailscale.exe'
    if (Test-Path $p) { return $p }
    return $null
}

function Find-Game {
    $libs = @()
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $steam = $steam -replace '/', '\'
        $libs += $steam
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object {
                $libs += ($_.Matches[0].Groups[1].Value -replace '\\\\', '\')
            }
        }
    }
    foreach ($l in ($libs | Select-Object -Unique)) {
        $g = Join-Path $l 'steamapps\common\Alien Isolation'
        if (Test-Path (Join-Path $g 'AI.exe')) { return $g }
    }
    return $null
}

function Test-Ours([string]$Path) {
    return (Test-Path $Path) -and (Select-String -Path $Path -Pattern $Mark -SimpleMatch -Quiet)
}

function Do-Check {
    Out-Report ''
    Out-Report "## Chequeo del entorno ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    $os = Get-CimInstance Win32_OperatingSystem
    Out-Report "- Windows: $($os.Caption) $($os.Version) (build $($os.BuildNumber)), $($os.OSArchitecture)"
    $game = Find-Game
    if (-not $game) {
        Out-Report '- **Juego no encontrado** en las bibliotecas de Steam (¿está instalado?).'
        return
    }
    Out-Report "- Carpeta del juego: ``$game``"
    $sha = (Get-FileHash (Join-Path $game 'AI.exe') -Algorithm SHA256).Hash
    if ($sha -eq $ExpectedSha) { $s = 'OK (build esperada)' } else { $s = '**DISTINTO: otra build, el mod no va a hookear**' }
    Out-Report "- SHA-256 de AI.exe: ``$sha`` $s"
    $acf = Join-Path (Split-Path (Split-Path $game)) 'appmanifest_214490.acf'
    if (Test-Path $acf) {
        $m = Select-String -Path $acf -Pattern '"buildid"\s+"(\d+)"'
        if ($m) { Out-Report "- buildid de Steam: $($m.Matches[0].Groups[1].Value) (esperado 4940658)" }
    }
    $dlls = Get-ChildItem $game -Filter *.dll | ForEach-Object { "$($_.Name) ($($_.Length) B)" }
    Out-Report "- DLLs en la carpeta del juego: $($dlls -join ', ')"
    foreach ($f in 'winmm.dll', 'aimp.cfg') {
        $p = Join-Path $game $f
        if (Test-Path $p) { Out-Report "- Ya hay ``$f`` en la carpeta del juego$(if ($f -eq 'winmm.dll') { if (Test-Ours $p) { ' (es el mod)' } else { ' (**AJENO**)' } })" }
    }
    $kd = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs'
    $vals = $kd.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' } | ForEach-Object { "$($_.Name)=$($_.Value)" }
    $hit = $vals | Where-Object { $_ -match '(^|=)winmm(\.dll)?$' -or $_ -match '^winmm=' }
    Out-Report "- KnownDLLs: $($vals -join ', ')"
    if ($hit) { Out-Report '- **winmm ESTÁ en KnownDLLs: Windows va a ignorar la winmm.dll del juego (el proxy no carga).**' }
    else { Out-Report '- winmm NO está en KnownDLLs (bien: el proxy puede cargar).' }
    $ts = Get-Tailscale
    if ($ts) {
        $ver = (Invoke-Native $ts @('version') | Select-Object -First 1)
        $ip = (Invoke-Native $ts @('ip', '-4') | Select-Object -First 1)
        $host4 = (Invoke-Native $ts @('ip', '-4', 'papito') | Select-Object -First 1)
        Out-Report "- Tailscale $ver; IP de esta laptop: $ip; IP de 'papito' (la máquina Linux, host): $host4"
        Out-Report '```text'
        (Invoke-Native $ts @('status') | Select-Object -First 10) | ForEach-Object { Out-Report $_ }
        Out-Report '```'
    } else {
        Out-Report '- Tailscale: **no instalado** (hace falta para la prueba de red).'
    }
    $fw = Get-NetFirewallProfile | ForEach-Object { "$($_.Name)=$($_.Enabled)" }
    Out-Report "- Firewall de Windows (perfiles): $($fw -join ', ')"
    $prof = Get-NetConnectionProfile -ErrorAction SilentlyContinue | ForEach-Object { "$($_.InterfaceAlias): $($_.NetworkCategory)" }
    Out-Report "- Redes: $($prof -join '; ')"
}

function Do-Install {
    $game = Find-Game
    if (-not $game) { throw 'Juego no encontrado.' }
    $sha = (Get-FileHash (Join-Path $game 'AI.exe') -Algorithm SHA256).Hash
    if ($sha -ne $ExpectedSha) { throw "AI.exe no es la build esperada ($sha)." }
    if (-not (Test-Path $DistDll)) { throw "Falta $DistDll (hacer git pull)." }
    $expect = ((Get-Content $DistSums | Where-Object { $_ -match 'winmm\.dll' }) -split '\s+')[0].ToUpper()
    $have = (Get-FileHash $DistDll -Algorithm SHA256).Hash
    if ($have -ne $expect) { throw "El SHA-256 de mod\dist\winmm.dll ($have) no coincide con SHA256SUMS ($expect)." }
    $target = Join-Path $game 'winmm.dll'
    if ((Test-Path $target) -and -not (Test-Ours $target)) { throw 'Ya hay un winmm.dll ajeno en la carpeta del juego: no lo piso.' }
    Copy-Item $DistDll $target -Force
    $cfg = @('# aimp.cfg generado por work\windows\aimp_check.ps1')
    if ($Net -eq 'join') {
        if (-not $Peer) { throw 'Con -Net join hace falta -Peer ip:puerto (IP de Tailscale de la máquina Linux).' }
        $cfg += 'AIMP_NET=join'
        $cfg += "AIMP_NET_PEER=$Peer"
    } elseif ($Net -eq 'host') {
        $cfg += 'AIMP_NET=host'
    }
    if ($Ghost -gt 0) {
        # Fase 4 (experimental): 3 = un NPC existente hace de avatar del otro jugador.
        $cfg += "AIMP_GHOST=$Ghost"
    }
    foreach ($kv in $Extra) {
        # Claves adicionales, p. ej. -Extra AIMP_SYNC_USE=1 (fase 5). Solo las AIMP_*.
        if ($kv -notmatch '^AIMP_[A-Z0-9_]+=') { throw "-Extra espera AIMP_CLAVE=valor (vino '$kv')." }
        $cfg += $kv
    }
    Set-Content -Path (Join-Path $game 'aimp.cfg') -Value $cfg -Encoding ASCII
    Out-Report ''
    Out-Report "## Instalación ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    Out-Report "- Copiado ``winmm.dll`` (SHA-256 ``$have``) a ``$game``."
    Out-Report "- ``aimp.cfg``: $(($cfg | Select-Object -Skip 1) -join '; ')$(if ($cfg.Count -eq 1) { '(sin red)' })"
    Out-Report '- Para deshacer: `-Action revert`.'
}

function Do-Collect {
    Out-Report ''
    Out-Report "## Resultado: $Tag ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    $log = Join-Path $AimpDir 'aimp.log'
    if (-not (Test-Path $log)) {
        Out-Report "- **No hay ``$log``**: el mod no cargó. Causas posibles: winmm en KnownDLLs, la DLL no está al lado de AI.exe, o el juego no llegó a arrancar."
    } else {
        New-Item -ItemType Directory -Force -Path $LogsOut | Out-Null
        $dest = Join-Path $LogsOut ("aimp-{0}-{1}.log" -f $Tag, (Get-Date -Format 'yyyyMMdd-HHmmss'))
        Copy-Item $log $dest
        Out-Report "- Log copiado a ``$(Resolve-Path -Relative $dest)`` ($((Get-Item $log).Length) bytes, modificado $((Get-Item $log).LastWriteTime))."
        $keys = 'AI_Multiplayer ', 'DLL:', 'config:', 'plataforma:', 'proxy:', 'build', 'hook ', 'FASE 1', 'FATAL', 'NET', 'EV nivel actual', 'EV jugador 0', 'MARCA', 'Run tardó', 'INESPERADO', 'OJO', 'DLL_PROCESS_DETACH'
        $pat = ($keys | ForEach-Object { [regex]::Escape($_) }) -join '|'
        Out-Report '```text'
        Select-String -Path $log -Pattern $pat | Select-Object -First 120 | ForEach-Object { Out-Report $_.Line }
        Out-Report '```'
        $pos = Select-String -Path $log -Pattern 'J0 pos=' | Select-Object -Last 3
        if ($pos) {
            Out-Report 'Últimas posiciones del jugador:'
            Out-Report '```text'
            $pos | ForEach-Object { Out-Report $_.Line }
            Out-Report '```'
        }
    }
    $since = (Get-Date).AddHours(-3)
    $ev = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since } -ErrorAction SilentlyContinue |
        Where-Object { $_.Message -match 'AI\.exe|winmm' -and $_.LevelDisplayName -match 'Error|Crítico|Critical' } | Select-Object -First 5
    if ($ev) {
        Out-Report '- Eventos de error de las últimas 3 h que mencionan AI.exe o winmm (Visor de eventos):'
        Out-Report '```text'
        $ev | ForEach-Object { Out-Report ("{0} {1} id={2}: {3}" -f $_.TimeCreated, $_.ProviderName, $_.Id, (($_.Message -split "`n")[0..6] -join ' | ')) }
        Out-Report '```'
    } else {
        Out-Report '- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).'
    }
}

function Do-Revert {
    $game = Find-Game
    if (-not $game) { throw 'Juego no encontrado.' }
    $target = Join-Path $game 'winmm.dll'
    Out-Report ''
    Out-Report "## Reversión ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    if (Test-Path $target) {
        if (Test-Ours $target) { Remove-Item $target; Out-Report '- Borrado `winmm.dll` (era el mod).' }
        else { Out-Report '- **El winmm.dll de la carpeta del juego no es el mod: no lo toco.**' }
    }
    $cfg = Join-Path $game 'aimp.cfg'
    if (Test-Path $cfg) { Remove-Item $cfg; Out-Report '- Borrado `aimp.cfg`.' }
    Out-Report "- Los logs en ``$AimpDir`` quedan (se pueden borrar a mano)."
}

if (-not (Test-Path $Report)) { Set-Content -Path $Report -Value '# Reporte de pruebas en Windows (laptop)' -Encoding UTF8 }
switch ($Action) {
    'check' { Do-Check }
    'install' { Do-Install }
    'collect' { Do-Collect }
    'revert' { Do-Revert }
}
