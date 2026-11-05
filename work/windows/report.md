# Reporte de pruebas en Windows (laptop)

## Chequeo del entorno (2026-11-05 10:57:01)
- Windows: Microsoft Windows 10 Home 10.0.19045 (build 19045), 64-bit
- Carpeta del juego: `c:\program files (x86)\steam\steamapps\common\Alien Isolation`
- SHA-256 de AI.exe: `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90` OK (build esperada)
- buildid de Steam: 4940658 (esperado 4940658)
- DLLs en la carpeta del juego: AMD_AGS.DLL (256000 B), ATIAGS.DLL (217088 B), CRASHREPORTER32.DLL (34816 B), STEAM_API.DLL (106920 B)
- KnownDLLs: _wow64cpu=wow64cpu.dll, _wowarmhw=wowarmhw.dll, _xtajit=xtajit.dll, advapi32=advapi32.dll, clbcatq=clbcatq.dll, combase=combase.dll, COMDLG32=COMDLG32.dll, coml2=coml2.dll, DifxApi=difxapi.dll, gdi32=gdi32.dll, gdiplus=gdiplus.dll, IMAGEHLP=IMAGEHLP.dll, IMM32=IMM32.dll, kernel32=kernel32.dll, MSCTF=MSCTF.dll, MSVCRT=MSVCRT.dll, NORMALIZ=NORMALIZ.dll, NSI=NSI.dll, ole32=ole32.dll, OLEAUT32=OLEAUT32.dll, rpcrt4=rpcrt4.dll, sechost=sechost.dll, Setupapi=Setupapi.dll, SHCORE=SHCORE.dll, SHELL32=SHELL32.dll, SHLWAPI=SHLWAPI.dll, user32=user32.dll, WLDAP32=WLDAP32.dll, wow64=wow64.dll, wow64win=wow64win.dll, WS2_32=WS2_32.dll
- winmm NO está en KnownDLLs (bien: el proxy puede cargar).
- Tailscale: **no instalado** (hace falta para la prueba de red).
- Firewall de Windows (perfiles): Domain=True, Private=True, Public=True
- Redes: Wi-Fi: Public

## Instalación (2026-11-05 10:58:20)
- Copiado `winmm.dll` (SHA-256 `DFF8E85FAE325BA1257DD3AFBB2A82C03E24F35DCFFE5E388FC2D30CACCEC671`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: (sin red)
- Para deshacer: `-Action revert`.

## Resultado: partida (2026-11-05 11:03:43)
- Log copiado a `.\work\windows\logs\aimp-partida-20261105-110343.log` (266672 bytes, modificado 11/05/2026 11:03:32).
```text
[    0.001] AI_Multiplayer 0.1.0 (fase 1+2: proxy, hook por frame, sonda de estado), commit 8f288e9, compilado Nov  5 2026 14:10:11
[    0.001] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x74ed0000), link 0x6aecb874, pid 6044
[    0.001] config: 1 archivo(s) aimp.cfg leídos; aplicadas: (ninguna)
[    0.001] plataforma: Windows 10.0.19045
[    0.002] proxy: winmm real = C:\Windows\system32\winmm.dll (0x74be0000)
[    0.002] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.002] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.003] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.003] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.003] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.003] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.003] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.003] hook ModeManager::Run: instalado en 0x00d08460 (bytes robados [51 53 55 56 8b f1], trampolín 0x00900000, detour 0x74ed28a0)
[   35.125] FASE 1: primer frame dentro de ModeManager::Run (this=0x08cc3fb4)
[   35.127] Run tardó 32.27 s (¿carga?)
[   35.127]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.136]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.142]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.153]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   39.159]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   40.171]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   41.186]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   42.193]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   43.214]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   44.226]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   45.242]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   46.251]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   47.264]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   48.279]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   49.281]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   50.282]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   51.288]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   52.296]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   53.304]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   54.319]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.339]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.352]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   57.367]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.367]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.371]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.385]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.400]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.417]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.431]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.433]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.449]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   66.481]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.489]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.521]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.531]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.545]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.560]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.562]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   73.577]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.579]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   75.593]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   76.610]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   77.642]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   78.643]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   79.658]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   80.659]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   81.682]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   82.683]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   83.688]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   84.703]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   86.438] Run tardó 1.22 s (¿carga?)
[   86.438]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   87.444]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   88.459]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   89.475]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   90.492]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   91.507]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   92.524]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   93.524]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   94.539]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   95.556]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   96.571]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   97.588]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   98.604]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[   99.620]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  100.636]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  101.653]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  102.668]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  103.685]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  104.701]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  105.717]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  106.733]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  107.734]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  108.749]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  109.765]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  118.378] Run tardó 8.35 s (¿carga?)
[  118.378] EV nivel actual 0 "Production\Frontend" -> 16 "Production\Tech_Hub"
[  118.378]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  121.989]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  122.509] EV jugador 0: 0x00000000 -> 0x2ae597d0 (vtable rva 0xef5d80 = Character(ok); def 0x08cc98a0 tipo 0 flags 1c=1 1f=1)
[  122.510] NETZ +0x000 @ 0x08cc42e0 (0x20 bytes)
[  122.510] NETZ +0x690 @ 0x08cc4970 (0x10 bytes)
[  122.992]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (2 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  123.992]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  125.008]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  126.024]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  127.040]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  127.524] NETZ +0x000 @ 0x08cc42e0 (0x20 bytes)
[  127.524] NETZ +0x690 @ 0x08cc4970 (0x10 bytes)
[  128.056]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  129.072]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  130.089]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  131.105]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  132.121]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  133.137]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  134.137]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  135.153]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  136.170]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  137.186]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  138.201]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  139.218]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  140.234]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  141.250]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  142.266]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
[  143.282]    red: NETZ 0x08cc42e0 transporte 0x00000000 máscara 0000001f:00000001 (0 cambios) | sesión 0x08cbfef8 proveedor 0x00000000 +2C=0 +2D=0
```
Últimas posiciones del jugador:
```text
[  249.946]    J0 pos=(30.687, 14.753, -19.874) yaw=0.0875 rad (5.0 grados) | se movió 0.000 desde el ST anterior
[  250.962]    J0 pos=(30.687, 14.753, -19.874) yaw=0.0875 rad (5.0 grados) | se movió 0.000 desde el ST anterior
[  251.978]    J0 pos=(30.687, 14.753, -19.874) yaw=0.0875 rad (5.0 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Resumen de la sesión Windows (2026-11-05)

Hechos (salen del log `logs/aimp-partida-20261105-110343.log`):
- **Pasos 1 y 2: OK en Windows 10 Home 19045 nativo.** Hubo una sola corrida, que cubrió menú y partida, así que se recolectó con `-Tag partida` y no hay corrida `primera` separada.
- Build esperada (SHA-256 OK, buildid 4940658). winmm **no** está en KnownDLLs. No había `winmm.dll` ajeno en la carpeta del juego. La instalación en `Program Files (x86)` **no** pidió administrador.
- `proxy: winmm real = C:\Windows\system32\winmm.dll`, con **195 exports resueltos y 10 ausentes**: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel. El juego no pidió ninguno: no hubo cartel ni error de punto de entrada.
- `build reconocida`, hook `ModeManager::Run` instalado y `FASE 1: primer frame` a los 35.1 s en `Production\Frontend`.
- Durante la corrida el usuario cambió la configuración de video sin problemas. Después cargó `Production\Tech_Hub` (nivel 16), con una carga de 8.35 s.
- J0 detectado como `Character(ok)`. Hubo **41 resúmenes ST con movimiento** (hasta ~2.4 por segundo) y la posición cambió de (33.84, 15.73, -44.42) a (30.69, 14.75, -19.87).
- `MARCA 1 (frame 11458, contador del juego 3157) J0 pos=(30.69, 14.75, -19.87)`.
- fps en los resúmenes: promedio 58.1, máximo 61.3, mínimo 0.8 (el mínimo cae durante la carga).
- Salida limpia: `ModeManager::Run devolvió 0` (12109 frames) y `DLL_PROCESS_DETACH`. No hay eventos de error de AI.exe en el Visor de eventos.
- Detalle: el log se escribe en UTF-8, pero PowerShell 5.1 lo lee como ANSI (sale `mÃ³dulos`). Las líneas que copia `collect` al reporte se ven bien.

Pendiente:
- **Paso 3 (red): no se hizo.** Tailscale no está instalado en la laptop y hace falta que el juego Linux esté abierto en modo host.
- `git pull` falló desde esta sesión porque no había credenciales de GitHub (`could not read Username`). Las pruebas se hicieron con el commit local `31fe898` (la DLL dice `commit 8f288e9`, que es el que la compiló).
