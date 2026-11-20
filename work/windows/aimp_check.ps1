<#
  aimp_check.ps1 - chequeos, instalación y recolección de logs del mod AI_Multiplayer
  en Windows (laptop de pruebas). Compatible con PowerShell 5.1 (Windows 10).

  Uso, desde la raíz del repo:
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action check [-Peer 100.x.y.z:47811]
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Preset host
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Preset join -Peer 100.x.y.z:47811
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action verify      (con el juego abierto)
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag primera
    powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action revert
  -Proxy xinput1_3 (con install): instala la variante del mod que entra por xinput1_3.dll en vez de winmm.dll, para
  máquinas donde Windows no carga la winmm de la carpeta del juego (verify lo sugiere). Se instala una sola variante.
  También: -Action install -Net none|host|join [-Peer ...] [-Ghost 3] [-Extra AIMP_X=1,AIMP_Y=2] (sin preset).

  -Preset usa la configuración recomendada de mod\dist\aimp-<preset>.cfg; -Extra agrega o reemplaza claves.
  "-ExecutionPolicy Bypass" vale solo para ese proceso: no cambia la configuración del sistema.
  Todo lo que hace se agrega a un informe: -Report <archivo>; por defecto work\windows\report.md en la laptop de
  pruebas (papulaptop) e instalacion-informe.md (en la raíz, ignorado por git) en cualquier otra máquina.
  En la carpeta del juego solo escribe winmm.dll (o xinput1_3.dll) y aimp.cfg (y "revert" los saca).
#>
param(
    [ValidateSet('check', 'install', 'verify', 'collect', 'revert')] [string]$Action = 'check',
    [ValidateSet('', 'host', 'join')] [string]$Preset = '',
    [ValidateSet('winmm', 'xinput1_3')] [string]$Proxy = 'winmm',
    [string]$Report = '',
    [ValidateSet('none', 'join', 'host')] [string]$Net = 'none',
    [string]$Peer = '',
    [ValidateRange(0, 3)] [int]$Ghost = 0,
    [string[]]$Extra = @(),
    [string]$Tag = 'corrida'
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $Report) {
    if ($env:COMPUTERNAME -eq 'PAPULAPTOP') { $Report = Join-Path $PSScriptRoot 'report.md' }
    else { $Report = Join-Path $Root 'instalacion-informe.md' }
}
$LogsOut = Join-Path $PSScriptRoot 'logs'
$ExpectedSha = 'DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90'
$Proxies = @('winmm', 'xinput1_3')   # las DLL por las que puede entrar el mod
$DistDll = Join-Path $Root "mod\dist\$Proxy.dll"
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
    foreach ($f in 'winmm.dll', 'xinput1_3.dll', 'aimp.cfg') {
        $p = Join-Path $game $f
        if (Test-Path $p) { Out-Report "- Ya hay ``$f`` en la carpeta del juego$(if ($f -like '*.dll') { if (Test-Ours $p) { ' (es el mod)' } else { ' (**AJENO**)' } })" }
    }
    $xs = Join-Path $env:WINDIR 'SysWOW64\xinput1_3.dll'
    Out-Report "- xinput1_3 del sistema (32 bits, la necesita la variante -Proxy xinput1_3): $(if (Test-Path $xs) { 'está' } else { '**no está** (instalar DirectX de _CommonRedist del juego)' })"
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
        Out-Report "- Tailscale $ver; IP de esta máquina: $ip"
        Out-Report '```text'
        (Invoke-Native $ts @('status') | Select-Object -First 10) | ForEach-Object { Out-Report $_ }
        Out-Report '```'
    } else {
        Out-Report '- Tailscale: **no instalado** (hace falta para jugar por internet sin abrir puertos).'
    }
    if ($Peer) {
        $peerHost = ($Peer -split ':')[0]
        $ok = Test-Connection -ComputerName $peerHost -Count 2 -Quiet -ErrorAction SilentlyContinue
        if ($ok) { Out-Report "- El host ($peerHost) responde al ping." }
        else { Out-Report "- **El host ($peerHost) no responde al ping**: revisar Tailscale (los dos conectados, la máquina compartida) o la IP." }
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
    $expect = ((Get-Content $DistSums | Where-Object { $_ -match "\s$([regex]::Escape($Proxy))\.dll$" }) -split '\s+')[0].ToUpper()
    $have = (Get-FileHash $DistDll -Algorithm SHA256).Hash
    if ($have -ne $expect) { throw "El SHA-256 de mod\dist\$Proxy.dll ($have) no coincide con SHA256SUMS ($expect)." }
    $target = Join-Path $game "$Proxy.dll"
    if ((Test-Path $target) -and -not (Test-Ours $target)) { throw "Ya hay un $Proxy.dll ajeno en la carpeta del juego (¿otro mod?): no lo piso." }
    if ($Proxy -eq 'xinput1_3' -and -not (Test-Path (Join-Path $env:WINDIR 'SysWOW64\xinput1_3.dll'))) {
        throw 'Falta la xinput1_3.dll del sistema (SysWOW64): instalar DirectX desde la carpeta _CommonRedist del juego.'
    }
    $removed = @()
    foreach ($px in $Proxies) {   # una sola variante del mod a la vez
        $other = Join-Path $game "$px.dll"
        if ($px -ne $Proxy -and (Test-Ours $other)) { Remove-Item $other; $removed += "$px.dll" }
    }
    Copy-Item $DistDll $target -Force
    $cfg = @('# aimp.cfg generado por work\windows\aimp_check.ps1')
    if ($Preset) {
        $presetFile = Join-Path $Root "mod\dist\aimp-$Preset.cfg"
        if (-not (Test-Path $presetFile)) { throw "Falta $presetFile (hacer git pull)." }
        if ($Preset -eq 'join' -and -not $Peer) { throw 'Con -Preset join hace falta -Peer ip:puerto (la IP de Tailscale del host, puerto 47811).' }
        foreach ($line in (Get-Content $presetFile)) {
            if ($line -match '^AIMP_NET_PEER=' -and $Peer) { $line = "AIMP_NET_PEER=$Peer" }
            $cfg += $line
        }
    } elseif ($Net -eq 'join') {
        if (-not $Peer) { throw 'Con -Net join hace falta -Peer ip:puerto (IP de Tailscale de la máquina Linux).' }
        $cfg += 'AIMP_NET=join'
        $cfg += "AIMP_NET_PEER=$Peer"
    } elseif ($Net -eq 'host') {
        $cfg += 'AIMP_NET=host'
    }
    if ($Ghost -gt 0) {
        # Fase 4 (experimental): 3 = un NPC existente hace de avatar del otro jugador.
        $cfg = @($cfg | Where-Object { $_ -notmatch '^AIMP_GHOST=' })
        $cfg += "AIMP_GHOST=$Ghost"
    }
    # Con -File, '-Extra A=1,B=1' llega como un solo string: se separa por comas.
    foreach ($kv in @($Extra | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })) {
        # Claves adicionales, p. ej. -Extra AIMP_SYNC_USE=1 (fase 5). Solo las AIMP_*.
        if ($kv -notmatch '^AIMP_[A-Z0-9_]+=') { throw "-Extra espera AIMP_CLAVE=valor (vino '$kv')." }
        # vale la primera aparición de cada clave: se saca la del preset antes de agregarla
        $key = ($kv -split '=')[0]
        $cfg = @($cfg | Where-Object { $_ -notmatch "^$key=" })
        $cfg += $kv
    }
    Set-Content -Path (Join-Path $game 'aimp.cfg') -Value $cfg -Encoding ASCII
    Out-Report ''
    Out-Report "## Instalación ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    Out-Report "- Copiado ``$Proxy.dll`` (SHA-256 ``$have``) a ``$game``."
    if ($removed) { Out-Report "- Borrada la otra variante del mod: $($removed -join ', ')." }
    Out-Report "- ``aimp.cfg``: $(($cfg | Select-Object -Skip 1 | Where-Object { $_ -notmatch '^#' }) -join '; ')$(if ($cfg.Count -eq 1) { '(sin red)' })"
    Out-Report '- Siguiente: abrir el juego (hasta el menú principal) y correr `-Action verify`.'
    Out-Report '- Para deshacer: `-Action revert`.'
}

function Do-Collect {
    Out-Report ''
    Out-Report "## Resultado: $Tag ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    $log = Join-Path $AimpDir 'aimp.log'
    if (-not (Test-Path $log)) {
        Out-Report "- **No hay ``$log``**: el mod no cargó. Con el juego abierto, `-Action verify` dice por qué."
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
        Where-Object { $_.Message -match 'AI\.exe|winmm|xinput1_3' -and $_.LevelDisplayName -match 'Error|Crítico|Critical' } | Select-Object -First 5
    if ($ev) {
        Out-Report '- Eventos de error de las últimas 3 h que mencionan AI.exe o winmm (Visor de eventos):'
        Out-Report '```text'
        $ev | ForEach-Object { Out-Report ("{0} {1} id={2}: {3}" -f $_.TimeCreated, $_.ProviderName, $_.Id, (($_.Message -split "`n")[0..6] -join ' | ')) }
        Out-Report '```'
    } else {
        Out-Report '- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).'
    }
}

# Con el juego abierto: ¿cargó el mod? Si no, por qué (Windows 11: Smart App Control / Integridad de código, o la
# winmm del sistema ya cargada). AI.exe es de 32 bits: sus módulos solo se ven desde un PowerShell de 32 bits.
function Do-Verify {
    Out-Report ''
    Out-Report "## Verificación ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    $os = Get-CimInstance Win32_OperatingSystem
    Out-Report "- Windows: $($os.Caption) $($os.Version) (build $($os.BuildNumber))"
    $p = Get-Process -Name AI -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) {
        Out-Report '- **El juego no está abierto.** Abrirlo desde Steam hasta el menú principal y repetir `-Action verify`.'
        return
    }
    $game = Split-Path $p.Path
    # La variante instalada: la DLL del mod que está en la carpeta (winmm si no hay ninguna).
    $px = ($Proxies | Where-Object { Test-Ours (Join-Path $game "$_.dll") } | Select-Object -First 1)
    if (-not $px) { $px = 'winmm' }
    $ours = Join-Path $game "$px.dll"
    Out-Report "- AI.exe abierto (pid $($p.Id)) en ``$game``; variante del mod: ``$px.dll``"
    if ([Environment]::Is64BitProcess) {
        $ps32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
        $loaded = @(Invoke-Native $ps32 @('-NoProfile', '-Command', "(Get-Process -Id $($p.Id)).Modules | Where-Object { `$_.ModuleName -eq '$px.dll' } | ForEach-Object { `$_.FileName }"))
    } else {
        $loaded = @($p.Modules | Where-Object { $_.ModuleName -eq "$px.dll" } | ForEach-Object { $_.FileName })
    }
    # Con el mod cargado hay dos módulos con ese nombre: el del juego (el mod) y el del sistema (la real).
    $loadedOurs = $loaded | Where-Object { $_ -ieq $ours } | Select-Object -First 1
    Out-Report "- $px.dll cargadas en el juego: $(if ($loaded) { ($loaded | ForEach-Object { "``$_``" }) -join ', ' } else { 'ninguna' })"
    $logs = @((Join-Path $AimpDir 'aimp.log'), (Join-Path $env:TEMP 'aimp.log')) | Where-Object { Test-Path $_ }
    $log = $logs | Where-Object { (Get-Item $_).LastWriteTime -ge $p.StartTime.AddSeconds(-5) } | Select-Object -First 1
    if ($loadedOurs) {
        Out-Report "- **El mod cargó** (Windows usa la $px.dll de la carpeta del juego)."
        if (-not $log) { Out-Report '- **Pero no hay log de esta corrida** en %LOCALAPPDATA%\AI_Multiplayer ni en %TEMP%: revisar permisos de esas carpetas.'; return }
        Out-Report "- Log: ``$log``"
        $pat = 'AI_Multiplayer |config:|plataforma:|build|FATAL|ERROR|NET |SESSION|SAVESYNC|UI: '
        Out-Report '```text'
        Get-Content $log -TotalCount 12 | ForEach-Object { Out-Report $_ }
        Out-Report '...'
        Select-String -Path $log -Pattern $pat | Select-Object -Last 12 | ForEach-Object { Out-Report $_.Line }
        Out-Report '```'
        if (Select-String -Path $log -Pattern 'build NO reconocida' -Quiet) { Out-Report '- **Build del juego no reconocida**: el mod queda inactivo. Verificar integridad de los archivos en Steam.' }
        if (Select-String -Path $log -Pattern 'recibidos [1-9]' -Quiet) { Out-Report '- **Red: hay datos del otro jugador.**' }
        elseif (Select-String -Path $log -Pattern 'NET ' -Quiet) { Out-Report '- Red: todavía sin datos del otro jugador (¿el otro tiene el juego abierto? ¿IP y Tailscale bien?).' }
        return
    }
    Out-Report "- **El mod NO cargó**: Windows usa otra $px.dll. Diagnóstico:"
    if (-not (Test-Path $ours)) { Out-Report "  - **No está** ``$ours``: copiar la DLL (o el antivirus la borró: ver Seguridad de Windows → Historial de protección)."; return }
    $kd = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs'
    $inKd = $kd.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' -and "$($_.Value)" -match "^$px(\.dll)?$" }
    if ($inKd) { Out-Report "  - $px está en KnownDLLs: Windows siempre usa la del sistema. El proxy por $px no puede andar en esta máquina." }
    else { Out-Report "  - $px no está en la lista de KnownDLLs." }
    $sac = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction SilentlyContinue).VerifiedAndReputablePolicyState
    $sacTxt = switch ("$sac") { '0' { 'apagado' } '1' { '**ACTIVADO**' } '2' { '**en evaluación**' } default { 'no figura (Windows 10 o sin el dato)' } }
    Out-Report "  - Smart App Control: $sacTxt"
    try {
        $ci = Get-WinEvent -LogName 'Microsoft-Windows-CodeIntegrity/Operational' -MaxEvents 300 -ErrorAction Stop |
            Where-Object { $_.TimeCreated -ge $p.StartTime.AddMinutes(-1) -and $_.Message -match 'winmm|xinput1_3|Alien Isolation' } | Select-Object -First 5
        if ($ci) {
            Out-Report '  - **Integridad de código bloqueó archivos del juego** (causa probable: falta de firma digital):'
            Out-Report '```text'
            $ci | ForEach-Object { Out-Report ("{0} id={1}: {2}" -f $_.TimeCreated, $_.Id, (($_.Message -split "`n")[0..2] -join ' | ')) }
            Out-Report '```'
        } else { Out-Report "  - Sin eventos de Integridad de código sobre $px desde que se abrió el juego." }
    } catch { Out-Report '  - No se pudo leer el registro de Integridad de código (correr PowerShell como administrador para verlo).' }
    if ($px -eq 'winmm') {
        Out-Report '  - Si no hubo bloqueo: la winmm del sistema probablemente se cargó antes que la del juego (pasa en Windows 11 nuevos). **Probar la variante xinput1_3**: cerrar el juego, `-Action install -Proxy xinput1_3` (con el mismo -Preset/-Peer) y repetir `-Action verify`. Ver INSTALAR_CON_CLAUDE.md.'
    } else {
        Out-Report '  - Si no hubo bloqueo: confirmar con Process Monitor (filtros Process Name is AI.exe y Path contains xinput1_3: ¿busca la DLL en la carpeta del juego?) y dejar la evidencia en el informe. Ver INSTALAR_CON_CLAUDE.md.'
    }
}

function Do-Revert {
    $game = Find-Game
    if (-not $game) { throw 'Juego no encontrado.' }
    Out-Report ''
    Out-Report "## Reversión ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
    foreach ($px in $Proxies) {
        $target = Join-Path $game "$px.dll"
        if (Test-Path $target) {
            if (Test-Ours $target) { Remove-Item $target; Out-Report "- Borrado ``$px.dll`` (era el mod)." }
            else { Out-Report "- El $px.dll de la carpeta del juego no es el mod: no lo toco." }
        }
    }
    $cfg = Join-Path $game 'aimp.cfg'
    if (Test-Path $cfg) { Remove-Item $cfg; Out-Report '- Borrado `aimp.cfg`.' }
    Out-Report "- Los logs en ``$AimpDir`` quedan (se pueden borrar a mano)."
}

if (-not (Test-Path $Report)) { Set-Content -Path $Report -Value '# Informe de instalación del mod AI_Multiplayer (aimp_check.ps1)' -Encoding UTF8 }
switch ($Action) {
    'check' { Do-Check }
    'install' { Do-Install }
    'verify' { Do-Verify }
    'collect' { Do-Collect }
    'revert' { Do-Revert }
}
