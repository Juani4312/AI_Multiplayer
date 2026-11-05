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

## Instalación (2026-11-05 11:09:50)
- Copiado `winmm.dll` (SHA-256 `DFF8E85FAE325BA1257DD3AFBB2A82C03E24F35DCFFE5E388FC2D30CACCEC671`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: AIMP_NET=join; AIMP_NET_PEER=100.126.161.126:47811
- Para deshacer: `-Action revert`.

## Resultado: red (2026-11-05 11:41:32)
- Log copiado a `.\work\windows\logs\aimp-red-20261105-114132.log` (450605 bytes, modificado 11/05/2026 11:41:21).
```text
[    0.001] AI_Multiplayer 0.1.0 (fase 1+2: proxy, hook por frame, sonda de estado), commit 8f288e9, compilado Nov  5 2026 14:10:11
[    0.001] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x73fb0000), link 0x6aecb874, pid 4116
[    0.001] config: 1 archivo(s) aimp.cfg leídos; aplicadas: AIMP_NET=join AIMP_NET_PEER=100.126.161.126:47811
[    0.002] plataforma: Windows 10.0.19045
[    0.002] proxy: winmm real = C:\Windows\system32\winmm.dll (0x73f80000)
[    0.002] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.002] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.003] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.003] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.003] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.003] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.005] NET: join hacia 100.126.161.126:47811, 20 paquetes/s
[    0.005] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.005] hook ModeManager::Run: instalado en 0x00d08460 (bytes robados [51 53 55 56 8b f1], trampolín 0x02b70000, detour 0x73fb28a0)
[   34.517] FASE 1: primer frame dentro de ModeManager::Run (this=0x08c62fb4)
[   34.519] Run tardó 32.82 s (¿carga?)
[   34.519]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   34.519] NET join: enviados 1, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   35.523]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   35.524] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   36.541]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.541] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   37.556]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.556] NET join: enviados 18, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   38.572]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.572] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   39.588]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   39.588] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   40.605]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   40.605] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   41.620]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   41.620] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   42.637]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   42.637] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   43.653]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   43.653] NET join: enviados 18, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   44.671]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   44.671] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   45.685]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   45.685] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   46.701]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   46.701] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   47.701]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   47.701] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   48.717]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   48.717] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   49.733]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   49.733] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   50.734]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   50.734] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   51.749]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   51.749] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   52.765]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   52.765] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   53.765]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   53.765] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   54.780]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   54.780] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   55.781]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.781] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   56.796]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.796] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   57.813]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   57.813] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   58.829]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.829] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   59.845]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.845] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   60.861]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.861] NET join: enviados 18, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   61.877]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.877] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   62.893]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.893] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   63.909]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.909] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   64.925]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.925] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   65.925]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.926] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   66.941]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   66.941] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   67.942]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.942] NET join: enviados 18, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   68.957]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.957] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   69.957]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.957] NET join: enviados 18, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   70.973]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.973] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   71.973]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.973] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   72.988]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.988] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   74.004]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.004] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   75.020]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   75.020] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   76.036]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   76.036] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   77.053]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   77.053] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   78.053]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   78.053] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   79.053]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   79.053] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   80.069]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   80.069] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   81.084]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   81.084] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   82.100]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   82.100] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   83.101]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   83.101] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   84.117]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   84.117] NET join: enviados 17, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   85.133]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   85.133] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   86.148]    red: NETZ 0x08c632e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x08c5eef8 proveedor 0x00000000 +2C=0 +2D=0
[   86.148] NET join: enviados 16, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
```
Últimas posiciones del jugador:
```text
[  380.703]    J0 pos=(6.617, 13.277, 34.610) yaw=-2.1945 rad (-125.7 grados) | se movió 0.000 desde el ST anterior
[  381.718]    J0 pos=(6.617, 13.277, 34.610) yaw=-2.1945 rad (-125.7 grados) | se movió 0.000 desde el ST anterior
[  382.734]    J0 pos=(6.617, 13.277, 34.610) yaw=-2.1945 rad (-125.7 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Paso 3: red con papito (2026-11-05)

Setup:
- Tailscale 1.104.5 instalado con winget en la laptop. La laptop es `papulaptop` (100.126.159.45) y papito es 100.126.161.126.
- `tailscale ping papito`: pong vía 192.168.1.6:41641 (directo por LAN) en 141 ms.
- La interfaz Tailscale está como red Private y el Wi-Fi como Public. No se tocó el firewall y Windows no preguntó por AI.exe.
- Hay una sola cuenta de Steam con el juego. La laptop corrió en **modo sin conexión de Steam** y papito online, con el juego en host.
- Mod de la laptop: `AIMP_NET=join`, `AIMP_NET_PEER=100.126.161.126:47811`.

Hechos (salen de `logs/aimp-red-20261105-114132.log`):
- `NET: join hacia 100.126.161.126:47811, 20 paquetes/s`.
- Hasta los ~94 s: `enviados ~17, recibidos 0`. El host todavía no estaba levantado.
- `[94.694] NET: primer estado del otro jugador: nivel 0, sin jugador`. papito estaba en el menú.
- `[306.072] EV nivel actual 0 -> 16 "Production\Tech_Hub"`. En el mismo segundo el remoto ya figura como `nivel 16 con jugador`.
- La posición remota cambia mientras se mueve el jugador de papito: (4.499, 13.279, 32.986) → (3.163, …, 33.198) → (2.431, …) → (-0.186, …, 33.331) → (2.277, …, 38.238) → (-0.146, …, 34.038). `hace` va de 0 a 67 ms.
- La posición local J0 entre MARCA 1 (334.3 s) y MARCA 2 (363.3 s) va de (7.59, 13.28, 33.34) a (6.62, 13.28, 34.61), con pasos de hasta 2.3 por segundo. Hay una MARCA 3 a los 367.5 s (un F8 de más).
- Totales en 276 resúmenes NET: **enviados 4518 (~16.4/s), recibidos 5141 (~18.6/s), perdidos 0, inválidos 0**.
- fps en partida: 60.0. Salida limpia: `ModeManager::Run devolvió 0` (20203 frames) y `DLL_PROCESS_DETACH`.

Observación (hipótesis, no verificada en el código):
- La laptop manda ~16-17 paquetes por segundo en lugar de 20, con el juego estable a 60 fps. Puede ser que el envío esté cuantizado a frames: 3 frames dan 49.8 ms, que es menos que 50 ms, así que saldría cada 4 frames (~15/s) con alguno intercalado. Del lado Linux llegan ~18/s. Lo puede revisar la sesión Linux en `mod/src/net.c`.

## COMMANDS.PAK en la laptop (dato para la sesión Linux)

| Archivo | SHA-256 en la laptop | ¿Igual que en papito? |
|---------|----------------------|-----------------------|
| `BSP_LV426_PT01\WORLD\COMMANDS.PAK` | `EB9FDBF197308941F6E43CDE7BD1712024011A16967A4819C71F2FB49076ED77` | **no** (papito `DDA19F70…A47F`) |
| `TECH_COMMS\WORLD\COMMANDS.PAK` | `42FD34D7FAF7EA9A22B961D7C6A0C12700FA90327CA1F796F445CC958ECBDD3A` | sí |

- Sin confirmar con el usuario: si el juego de la laptop se copió desde papito y si se verificó la integridad en Steam.
- En la carpeta del juego de la laptop no hay `dxgi.dll`, `d3d11.dll`, `xinput1_3.dll` ni un `winmm.dll` viejo. Las DLLs que hay son AMD_AGS, ATIAGS, CRASHREPORTER32, STEAM_API y el `winmm.dll` del mod instalado acá.

## Resumen final

- **Pasos 1, 2 y 3: OK en Windows 10 nativo.** El proxy, el hook, la sonda de estado y posición y la red UDP join hacia papito por Tailscale funcionan en los dos sentidos, con 0 paquetes perdidos.
- Hay un solo punto a revisar: la tasa de envío del lado Windows es ~16.4/s contra 20 configurados.

## COMMANDS.PAK: con integridad verificada (2026-11-05)

- Antes de la verificación, el usuario había instalado el juego en la laptop copiándolo desde papito por red local, después lo borró y lo reinstaló desde Steam. Después corrió "Verificar integridad" en Steam y salió todo correcto, sin archivos reemplazados.
- Hashes después de la verificación (no cambiaron respecto de los de arriba):
  - LV426 `EB9FDBF197308941F6E43CDE7BD1712024011A16967A4819C71F2FB49076ED77`
  - TECH_COMMS `42FD34D7FAF7EA9A22B961D7C6A0C12700FA90327CA1F796F445CC958ECBDD3A`
  - AI.exe `DDE66521…19A90`
- Conclusión (según la regla de las instrucciones):
  - **el `COMMANDS.PAK` de `BSP_LV426_PT01` vanilla es `EB9FDBF1…ED77`, y el de papito (`DDA19F70…A47F`) es el modificado**;
  - el de `TECH_COMMS` de papito ya era vanilla.
- Según el usuario, el mod de papito sirve para saltear la misión 9.

## Instalación (2026-11-05 12:50:15)
- Copiado `winmm.dll` (SHA-256 `82C584385083B21A190A5212CDCBE90A89DA04936B3BE5BF428DF648DD720CFA`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: AIMP_NET=join; AIMP_NET_PEER=100.126.161.126:47811; AIMP_GHOST=3
- Para deshacer: `-Action revert`.

## Resultado: avatar (2026-11-05 12:57:04)
- Log copiado a `.\work\windows\logs\aimp-avatar-20261105-125704.log` (326313 bytes, modificado 11/05/2026 12:56:36).
```text
[    0.001] AI_Multiplayer 0.2.0 (fases 1-3: proxy, hook por frame, sonda de estado, UDP del jugador), commit c2f86d1, compilado Nov  5 2026 17:45:03
[    0.002] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x73fa0000), link 0x6aeceacf, pid 1932
[    0.002] config: 1 archivo(s) aimp.cfg leídos; aplicadas: AIMP_NET=join AIMP_NET_PEER=100.126.161.126:47811 AIMP_GHOST=3
[    0.002] plataforma: Windows 10.0.19045
[    0.002] proxy: winmm real = C:\Windows\system32\winmm.dll (0x73f70000)
[    0.002] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.003] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.004] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.004] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.004] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.004] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.006] NET: join hacia 100.126.161.126:47811, 20 paquetes/s
[    0.006] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.006] hook ModeManager::Run: instalado en 0x01388460 (bytes robados [51 53 55 56 8b f1], trampolín 0x03890000, detour 0x73fa28a0)
[    0.006] hook spawn de autoridad (FUN_00797490): instalado en 0x01377490 (bytes robados [55 8b ec 53 56 57], trampolín 0x038a0000, detour 0x73fa8980)
[    0.006] hook SpawnCharacter (FUN_007d3b50): instalado en 0x013b3b50 (bytes robados [51 53 55 56 57], trampolín 0x038b0000, detour 0x73fa82f0)
[   34.594] FASE 1: primer frame dentro de ModeManager::Run (this=0x098e0fb4)
[   34.596] Run tardó 32.87 s (¿carga?)
[   34.596]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   34.596] NET join: enviados 1, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   35.610]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   35.610] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   36.610]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.610] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   37.625]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.625] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   38.642]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.642] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   39.643]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   39.643] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   40.657]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   40.657] NET join: enviados 21, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   41.673]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   41.673] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   42.690]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   42.690] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   43.707]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   43.707] NET join: enviados 21, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   44.722]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   44.722] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   45.722]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   45.722] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   46.738]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   46.738] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   47.754]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   47.754] NET join: enviados 21, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   48.754]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   48.754] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   49.770]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   49.770] NET join: enviados 20, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   49.869] NET: primer estado del otro jugador: nivel 0, sin jugador, pos=(0.00, 0.00, 0.00) yaw=0.000
[   50.786]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   50.786] NET join 100.126.161.126:47811: enviados 20, recibidos 19, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 19, hace 17 ms
[   51.786]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   51.786] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 39, hace 18 ms
[   52.801]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   52.801] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 59, hace 32 ms
[   53.817]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   53.817] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 80, hace 0 ms
[   54.833]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   54.833] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 100, hace 16 ms
[   55.850]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.850] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 120, hace 17 ms
[   56.866]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.866] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 141, hace 0 ms
[   57.882]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   57.882] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 161, hace 0 ms
[   58.898]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.898] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 181, hace 34 ms
[   59.915]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.915] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 202, hace 0 ms
[   60.930]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.930] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 222, hace 16 ms
[   61.947]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.947] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 242, hace 33 ms
[   62.947]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.947] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 262, hace 33 ms
[   63.962]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.962] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 283, hace 0 ms
[   64.978]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.978] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 17 ms
[   65.994]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.994] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 1033 ms
[   67.011]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.011] NET join 100.126.161.126:47811: enviados 21, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 2049 ms
[   68.027]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.027] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 3066 ms
[   69.044]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.044] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 4082 ms
[   70.059]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.059] NET join 100.126.161.126:47811: enviados 21, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 303, hace 5098 ms
[   71.060]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.060] NET join 100.126.161.126:47811: enviados 20, recibidos 1, perdidos 0, inválidos 0 | remoto: nivel 16 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 304, hace 434 ms
[   72.076]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.076] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 16 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 304, hace 1450 ms
[   73.091]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   73.092] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 16 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 304, hace 2466 ms
[   74.108]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.108] NET join 100.126.161.126:47811: enviados 21, recibidos 10, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(4.550, 13.618, 33.592) yaw=-3.142 seq 314, hace 17 ms
[   75.124]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   75.124] NET join 100.126.161.126:47811: enviados 20, recibidos 19, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(4.499, 13.279, 32.980) yaw=3.142 seq 333, hace 0 ms
[   82.928] Run tardó 7.34 s (¿carga?)
[   82.929] EV nivel actual 0 "Production\Frontend" -> 16 "Production\Tech_Hub"
[   82.929]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   82.929] NET join 100.126.161.126:47811: enviados 10, recibidos 155, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 488, hace 0 ms
[   86.532]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   86.532] NET join 100.126.161.126:47811: enviados 1, recibidos 73, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 561, hace 0 ms
[   86.784] EV jugador 0: 0x00000000 -> 0x2ba69aa0 (vtable rva 0xef5d80 = Character(ok); def 0x098e6160 tipo 0 flags 1c=1 1f=1)
[   86.785] NETZ +0x000 @ 0x098e12e0 (0x20 bytes)
[   86.785] NETZ +0x690 @ 0x098e1970 (0x10 bytes)
[   87.534]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 0000003f:00000001 (1 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   87.534] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 581, hace 33 ms
[   88.534]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   88.534] NET join 100.126.161.126:47811: enviados 18, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 601, hace 17 ms
[   89.550]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   89.550] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 621, hace 34 ms
[   90.567]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   90.567] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 642, hace 0 ms
[   91.582]    red: NETZ 0x098e12e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x098dcef8 proveedor 0x00000000 +2C=0 +2D=0
[   91.582] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.928, 13.279, 34.498) yaw=-2.228 seq 662, hace 16 ms
```
Últimas posiciones del jugador:
```text
[  235.777]    J0 pos=(5.924, 13.278, 33.491) yaw=-1.4482 rad (-83.0 grados) | se movió 0.000 desde el ST anterior
[  236.793]    J0 pos=(5.924, 13.278, 33.491) yaw=-1.4482 rad (-83.0 grados) | se movió 0.000 desde el ST anterior
[  237.793]    J0 pos=(5.924, 13.278, 33.491) yaw=-1.4482 rad (-83.0 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Paso 6: avatar del otro jugador por red (fase 4, 2026-11-05)

Setup:
- DLL `c2f86d1` (0.2.0), SHA-256 `82C58438…720CFA`, igual a `mod/dist/SHA256SUMS`.
- `aimp.cfg`: `AIMP_NET=join`, `AIMP_NET_PEER=100.126.161.126:47811`, `AIMP_GHOST=3`.
- Laptop con Steam sin conexión. papito en host con `AIMP_GHOST=3`. Las dos partidas en `Production\Tech_Hub` (nivel 16).

Hechos (salen de `logs/aimp-avatar-20261105-125704.log`):
- `[0.006] AIMP_GHOST=3 (fase 4, experimental: solo NPC existente): empieza 8000 ms después de que aparece el jugador`.
- `[82.929] EV nivel actual 0 -> 16 "Production\Tech_Hub"`.
- `[94.747] GHOST: NPC elegido para mover: NPCs[1] = 0x2ba6dd70 "CV5", vtable rva 0xef5d80, def 0x098e6330 tipo 6 flags 1c=0 1f=1, controlador 0x00000000 (activo 0), pos=(25.61, 20.64, 19.34), id 1c3460b37e03a5dc`.
- `[94.764] GHOST: primer teletransporte de 0x2ba6dd70 a (6.93, 13.28, 34.50; w 0.00) yaw -2.228 [red]`.
- Hay 141 líneas `GHOST: personaje en (…), objetivo (…) [red]`, con 640 teletransportes en total y 27 objetivos distintos. Ejemplos: 6.93 → 4.40 → 2.22 → 1.92 → 2.22 → 1.68.
- Distancia entre el civil y el objetivo: promedio 0.181 y máximo 4.341. El máximo es la única muestra por encima de 0.5 y está en 95.76 s, justo después del primer teletransporte.
- Con el remoto quieto, el civil se corre unos 0.1 a 0.35 del objetivo hasta el siguiente teletransporte. Por ejemplo, en 130 a 142 s está en (1.89, 13.28, 33.41) con objetivo (1.92, 13.28, 33.76).
- MARCA 1 a los 111.2 s, MARCA 2 a los 167.0 s y MARCA 3 a los 196.7 s, con J0 quieto en (5.92, 13.28, 33.49). El usuario apretó F8 viendo moverse al avatar del jugador de papito.
- **Ninguna `EXCEPCIÓN`.** Salida limpia: `ModeManager::Run devolvió 0` (11558 frames) y `DLL_PROCESS_DETACH`.
- Red: en 177 resúmenes, **enviados 3554 (~20.1/s), recibidos 3612 (~20.4/s), perdidos 0, inválidos 0**.
- fps en partida: 60.0 o 60.1 en casi todos los resúmenes, con un mínimo de 58.1.
- Según el usuario: "ya estoy metido en ambos y nos vemos mutuamente". El avatar se ve en las dos máquinas.

Observaciones:
- Con esta DLL la laptop manda ~20 paquetes por segundo. La tasa de ~16.4/s que se había visto con `8f288e9` no aparece.
- Hipótesis: el corrimiento del civil entre teletransportes viene de la física o la IA del NPC, que lo sigue moviendo. Con el remoto quieto, el mod parece teletransportarlo solo cuando el objetivo cambia (2 teletransportes por segundo).

## Instalación (2026-11-05 13:14:30)
- Copiado `winmm.dll` (SHA-256 `49D90E492730E7E61F37722DD37D1BFE20F8C127988693FD369DE7F993A3F2D5`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: AIMP_NET=join; AIMP_NET_PEER=100.126.161.126:47811; AIMP_GHOST=3; AIMP_SYNC_USE=1
- Para deshacer: `-Action revert`.

## Resultado: usar (2026-11-05 13:21:55)
- Log copiado a `.\work\windows\logs\aimp-usar-20261105-132155.log` (113978 bytes, modificado 11/05/2026 13:21:34).
```text
[    0.001] AI_Multiplayer 0.2.0 (fases 1-3: proxy, hook por frame, sonda de estado, UDP del jugador), commit 20cf297, compilado Nov  5 2026 18:12:37
[    0.001] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x73fa0000), link 0x6aecf145, pid 6880
[    0.001] config: 1 archivo(s) aimp.cfg leídos; aplicadas: AIMP_NET=join AIMP_NET_PEER=100.126.161.126:47811 AIMP_GHOST=3 AIMP_SYNC_USE=1
[    0.002] plataforma: Windows 10.0.19045
[    0.002] proxy: winmm real = C:\Windows\system32\winmm.dll (0x73f70000)
[    0.002] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.002] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.003] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.003] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.003] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.003] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.005] NET: join hacia 100.126.161.126:47811, 20 paquetes/s
[    0.005] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.005] hook ModeManager::Run: instalado en 0x01388460 (bytes robados [51 53 55 56 8b f1], trampolín 0x00ed0000, detour 0x73fa28a0)
[    0.005] hook spawn de autoridad (FUN_00797490): instalado en 0x01377490 (bytes robados [55 8b ec 53 56 57], trampolín 0x00fc0000, detour 0x73fa8ec0)
[    0.005] hook SpawnCharacter (FUN_007d3b50): instalado en 0x013b3b50 (bytes robados [51 53 55 56 57], trampolín 0x00fd0000, detour 0x73fa8830)
[    0.005] hook usar (FUN_00795d00): instalado en 0x01375d00 (bytes robados [55 8b ec 56 57], trampolín 0x03440000, detour 0x73faa330)
[    0.005] hook handler A del uso (FUN_00418a10): instalado en 0x00ff8a10 (bytes robados [55 8b ec 53 56 57], trampolín 0x03450000, detour 0x73faa210)
[    0.005] hook handler de sesión del uso (FUN_00418980): instalado en 0x00ff8980 (bytes robados [55 8b ec 56 57], trampolín 0x04af0000, detour 0x73faa1c0)
[   34.225] FASE 1: primer frame dentro de ModeManager::Run (this=0x092b5fb4)
[   34.227] Run tardó 32.85 s (¿carga?)
[   34.227]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   34.227] NET join: enviados 1, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   34.315] NET: primer estado del otro jugador: nivel 16, con jugador, pos=(0.26, 13.28, 34.24) yaw=1.702
[   35.230]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   35.230] NET join 100.126.161.126:47811: enviados 20, recibidos 19, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5286, hace 34 ms
[   36.247]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.247] NET join 100.126.161.126:47811: enviados 19, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5307, hace 0 ms
[   37.262]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.262] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5327, hace 0 ms
[   38.279]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.279] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5347, hace 17 ms
[   46.558] Run tardó 7.31 s (¿carga?)
[   46.558] EV nivel actual 0 "Production\Frontend" -> 16 "Production\Tech_Hub"
[   46.558]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   46.558] NET join 100.126.161.126:47811: enviados 21, recibidos 164, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5511, hace 0 ms
[   50.187]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   50.187] NET join 100.126.161.126:47811: enviados 1, recibidos 74, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5585, hace 0 ms
[   50.440] EV jugador 0: 0x00000000 -> 0x2b4457d0 (vtable rva 0xef5d80 = Character(ok); def 0x092bb160 tipo 0 flags 1c=1 1f=1)
[   50.441] NETZ +0x000 @ 0x092b62e0 (0x20 bytes)
[   50.441] NETZ +0x690 @ 0x092b6970 (0x10 bytes)
[   51.197]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (1 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   51.197] NET join 100.126.161.126:47811: enviados 19, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5606, hace 0 ms
[   52.205]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   52.205] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5626, hace 0 ms
[   53.222]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   53.222] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5646, hace 17 ms
[   54.237]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   54.237] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5666, hace 33 ms
[   55.254]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.254] NET join 100.126.161.126:47811: enviados 21, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5687, hace 0 ms
[   55.455] NETZ +0x000 @ 0x092b62e0 (0x20 bytes)
[   55.455] NETZ +0x690 @ 0x092b6970 (0x10 bytes)
[   56.270]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.270] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5707, hace 17 ms
[   57.286]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   57.286] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.263, 13.276, 34.242) yaw=1.702 seq 5727, hace 32 ms
[   58.303]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.303] NET join 100.126.161.126:47811: enviados 21, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.162, 13.276, 34.182) yaw=1.702 seq 5748, hace 0 ms
[   59.319]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.319] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.126, 13.276, 34.158) yaw=1.702 seq 5768, hace 0 ms
[   60.335]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.335] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.001, 13.276, 34.187) yaw=-2.910 seq 5788, hace 16 ms
[   61.351]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.351] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.918, 13.276, 34.492) yaw=-1.300 seq 5808, hace 33 ms
[   62.367]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.367] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.177, 13.245, 34.825) yaw=-1.573 seq 5829, hace 0 ms
[   62.367] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   63.383]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.383] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.262, 13.245, 34.798) yaw=1.565 seq 5849, hace 17 ms
[   63.383] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   64.399]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.399] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 5869, hace 34 ms
[   64.400] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   65.415]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.415] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.051, 13.243, 34.661) yaw=1.568 seq 5890, hace 0 ms
[   65.415] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   66.431]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   66.431] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.175, 13.271, 34.563) yaw=1.381 seq 5910, hace 16 ms
[   66.431] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   67.448]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.448] NET join 100.126.161.126:47811: enviados 21, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.305, 13.276, 34.219) yaw=1.108 seq 5931, hace 0 ms
[   67.448] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   68.464]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.464] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.277, 34.226) yaw=1.108 seq 5951, hace 0 ms
[   68.464] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   69.480]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.480] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.278, 34.226) yaw=1.108 seq 5971, hace 34 ms
[   69.480] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   70.496]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.496] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.279, 34.226) yaw=1.108 seq 5991, hace 33 ms
[   70.496] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   71.512]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.512] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.280, 34.226) yaw=1.108 seq 6012, hace 16 ms
[   71.512] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   72.528]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.528] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.281, 34.226) yaw=1.108 seq 6032, hace 17 ms
[   72.528] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   74.125] Run tardó 1.11 s (¿carga?)
[   74.125] EV jugador 0: 0x2b4457d0 -> 0x00000000 (vtable rva 0x0 = -; def 0x00000000 tipo 0 flags 1c=0 1f=0)
[   74.125]    red: NETZ 0x092b62e0 transporte 0x00000000 máscara 00000000:00000000 (1 cambios) | sesión 0x092b1ef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.125] NET join 100.126.161.126:47811: enviados 11, recibidos 32, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.281, 13.283, 34.226) yaw=1.108 seq 6064, hace 0 ms
[   74.125] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   74.125] NET: cerrado (enviados 558, recibidos 797, perdidos 0 en total)
[   74.329] DLL_PROCESS_DETACH (fin del proceso)
```
Últimas posiciones del jugador:
```text
[   70.496]    J0 pos=(-3.370, 13.236, 34.810) yaw=1.5708 rad (90.0 grados) | se movió 0.000 desde el ST anterior
[   71.512]    J0 pos=(-3.370, 13.236, 34.810) yaw=1.5708 rad (90.0 grados) | se movió 0.000 desde el ST anterior
[   72.528]    J0 pos=(-3.370, 13.236, 34.810) yaw=1.5708 rad (90.0 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Paso 7: "usar" compartido (fase 5, 2026-11-05)

Setup:
- DLL `20cf297`, SHA-256 `49D90E49…A3F2D5`, igual a `mod/dist/SHA256SUMS`.
- `aimp.cfg`: `AIMP_NET=join`, `AIMP_NET_PEER=100.126.161.126:47811`, `AIMP_GHOST=3`, `AIMP_SYNC_USE=1`.
- Laptop con Steam sin conexión y papito en host. Las dos partidas en `Production\Tech_Hub`.

Hechos (salen de `logs/aimp-usar-20261105-132155.log`):
- `[34.315] NET: primer estado del otro jugador: nivel 16, con jugador`.
- `[46.558] EV nivel 0 -> 16 "Production\Tech_Hub"`.
- `[58.269] GHOST: NPC elegido … "CV5"` y `[58.286] GHOST: primer teletransporte … [red]`.
- `[61.568] SYNC: aplicando uso remoto (evento 2): objetivo 3e3ec35dbebfb3d2, RPC A 0x26, personaje = jugador local, flag 007c5801`.
- `[61.585] SYNC handler A del uso 1: ctx 0x22072070 (vtable rva 0xef4eec, vf[1] rva 0x74d3f0, vf[2] rva 0x3806a0) | objetivo 3e3ec35dbebfb3d2, personaje 2ff722b2099d5b9b`.
- `[61.585] SYNC handler de sesión del uso 1: ctx 0x22072074 (vtable rva 0xef4ee4, vf[1] rva 0x393fb0, vf[2] rva 0x3b5a80) | objetivo 3e3ec35dbebfb3d2, personaje 2ff722b2099d5b9b`.
- `NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)`.
- **Después del uso remoto, el jugador local J0 salta y queda en la misma posición que el jugador remoto:**
  - `[61.351] J0 pos=(1.755, 13.278, 34.803) yaw=-1.4745`. Antes del uso, el remoto estaba en (-1.918, 13.276, 34.492).
  - `[62.367] J0 pos=(-2.251, 13.245, 34.849) yaw=-1.5728 | se movió 4.006`. El remoto está en (-2.177, 13.245, 34.825) yaw -1.573.
  - `[63.383] J0 pos=(-3.204, 13.245, 34.796) yaw=1.5646`. El remoto está en (-3.262, 13.245, 34.798) yaw 1.565.
  - `[64.399] J0 pos=(-3.370, 13.236, 34.810) yaw=1.5708`. El remoto está en **(-3.370, 13.236, 34.810) yaw 1.571**: coinciden en los 3 decimales.
  - Desde ahí J0 queda quieto hasta la salida.
- **La cámara local queda fija** desde 62.367 en `pos=(-2.522, 14.739, 34.725) q=(0.0559, -0.6703, 0.0508, 0.7382)`. Antes, en 61.351, estaba en (1.864, 14.845, 34.792) con el mismo q. El offset `cam - J0` pasa de (0.108, 1.567, -0.011) a (0.848, 1.503, -0.085).
- Del lado de la laptop no hubo ningún `SYNC uso local`, ni ninguna MARCA F8, ni ninguna `EXCEPCIÓN`.
- Salida: `[74.125] ModeManager::Run devolvió 0` (1657 frames) y `DLL_PROCESS_DETACH`, unos 12.5 s después del uso remoto. No hay eventos de error de AI.exe en el Visor de eventos.
- Red: 20 enviados y 20 recibidos por segundo, 0 perdidos. 60 fps.

Hipótesis (no verificadas):
- El uso de papito se aplicó con `personaje = jugador local`. Por eso el jugador de la laptop hizo la misma interacción sobre el mismo objetivo: el juego lo llevó al punto de anclaje de ese objeto (locker, terminal, etc.), que es donde estaba el jugador de papito, y la cámara quedó en el modo de esa interacción. Eso explicaría que J0 coincida con el remoto hasta el tercer decimal.
- Si es así, el uso sí tuvo efecto en el juego que lo recibe, pero lo hizo el jugador local y no el avatar. Para usos como abrir una puerta o una caja puede servir. Para lockers y terminales mueve al otro jugador.
- Falta: lo que vio el usuario en cada pantalla (qué usó en papito y qué pasó en la laptop), y la prueba en el otro sentido (uso desde la laptop).

## Instalación (2026-11-05 13:25:44)
- Copiado `winmm.dll` (SHA-256 `7EE4AB942A69E1BA1A45B1836FEA8C7E245CC3CE1EEA2943D73F7D790071CB1E`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: AIMP_NET=join; AIMP_NET_PEER=100.126.161.126:47811; AIMP_GHOST=3; AIMP_SYNC_USE=1
- Para deshacer: `-Action revert`.

## Resultado: usar (2026-11-05 13:30:59)
- Log copiado a `.\work\windows\logs\aimp-usar-20261105-133059.log` (404097 bytes, modificado 11/05/2026 13:30:49).
```text
[    0.001] AI_Multiplayer 0.2.0 (fases 1-3: proxy, hook por frame, sonda de estado, UDP del jugador), commit 1c1d558, compilado Nov  5 2026 18:24:35
[    0.001] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x73fa0000), link 0x6aecf413, pid 8940
[    0.001] config: 1 archivo(s) aimp.cfg leídos; aplicadas: AIMP_NET=join AIMP_NET_PEER=100.126.161.126:47811 AIMP_GHOST=3 AIMP_SYNC_USE=1
[    0.001] plataforma: Windows 10.0.19045
[    0.002] proxy: winmm real = C:\Windows\system32\winmm.dll (0x73f70000)
[    0.002] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.002] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.003] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.003] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.003] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.003] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.005] NET: join hacia 100.126.161.126:47811, 20 paquetes/s
[    0.005] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.005] hook ModeManager::Run: instalado en 0x01388460 (bytes robados [51 53 55 56 8b f1], trampolín 0x02ff0000, detour 0x73fa28a0)
[    0.005] hook spawn de autoridad (FUN_00797490): instalado en 0x01377490 (bytes robados [55 8b ec 53 56 57], trampolín 0x03340000, detour 0x73fa8f00)
[    0.005] hook SpawnCharacter (FUN_007d3b50): instalado en 0x013b3b50 (bytes robados [51 53 55 56 57], trampolín 0x03350000, detour 0x73fa8870)
[    0.005] hook usar (FUN_00795d00): instalado en 0x01375d00 (bytes robados [55 8b ec 56 57], trampolín 0x03360000, detour 0x73faa370)
[    0.005] hook handler A del uso (FUN_00418a10): instalado en 0x00ff8a10 (bytes robados [55 8b ec 53 56 57], trampolín 0x03370000, detour 0x73faa250)
[    0.005] hook handler de sesión del uso (FUN_00418980): instalado en 0x00ff8980 (bytes robados [55 8b ec 56 57], trampolín 0x036f0000, detour 0x73faa200)
[   34.136] FASE 1: primer frame dentro de ModeManager::Run (this=0x096d3fb4)
[   34.138] Run tardó 32.76 s (¿carga?)
[   34.138]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   34.138] NET join: enviados 1, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   34.226] NET: primer estado del otro jugador: nivel 0, sin jugador, pos=(0.00, 0.00, 0.00) yaw=0.000
[   35.142]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   35.142] NET join 100.126.161.126:47811: enviados 20, recibidos 18, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 18, hace 34 ms
[   36.158]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.158] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 38, hace 50 ms
[   37.174]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.174] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 59, hace 17 ms
[   38.190]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.190] NET join 100.126.161.126:47811: enviados 21, recibidos 1, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 60, hace 983 ms
[   39.231]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   39.231] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 60, hace 2024 ms
[   40.239]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   40.239] NET join 100.126.161.126:47811: enviados 21, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 60, hace 3032 ms
[   41.239]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   41.239] NET join 100.126.161.126:47811: enviados 20, recibidos 0, perdidos 0, inválidos 0 | remoto: nivel 0 sin jugador pos=(0.000, 0.000, 0.000) yaw=0.000 seq 60, hace 4032 ms
[   49.293] Run tardó 7.39 s (¿carga?)
[   49.293] EV nivel actual 0 "Production\Frontend" -> 16 "Production\Tech_Hub"
[   49.294]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   49.294] NET join 100.126.161.126:47811: enviados 14, recibidos 68, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(4.499, 13.279, 32.978) yaw=3.142 seq 128, hace 0 ms
[   52.947]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   52.947] NET join 100.126.161.126:47811: enviados 1, recibidos 73, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(2.019, 13.279, 34.941) yaw=-1.617 seq 201, hace 0 ms
[   53.200] EV jugador 0: 0x00000000 -> 0x2b8697d0 (vtable rva 0xef5d80 = Character(ok); def 0x096d9160 tipo 0 flags 1c=1 1f=1)
[   53.201] NETZ +0x000 @ 0x096d42e0 (0x20 bytes)
[   53.201] NETZ +0x690 @ 0x096d4970 (0x10 bytes)
[   53.953]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (1 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   53.953] NET join 100.126.161.126:47811: enviados 19, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(2.019, 13.279, 34.941) yaw=-1.617 seq 221, hace 25 ms
[   54.965]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   54.965] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.113, 13.279, 34.856) yaw=-1.605 seq 242, hace 0 ms
[   55.982]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.982] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.118, 13.279, 34.637) yaw=-1.438 seq 262, hace 17 ms
[   56.997]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.997] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 282, hace 33 ms
[   58.014]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.014] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 303, hace 0 ms
[   58.215] NETZ +0x000 @ 0x096d42e0 (0x20 bytes)
[   58.215] NETZ +0x690 @ 0x096d4970 (0x10 bytes)
[   59.030]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.030] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 323, hace 17 ms
[   60.046]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.046] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 343, hace 17 ms
[   61.063]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.063] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 363, hace 34 ms
[   62.079]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.079] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 384, hace 17 ms
[   63.095]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.095] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 404, hace 17 ms
[   64.111]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.111] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.174, 13.279, 34.644) yaw=-1.438 seq 424, hace 50 ms
[   65.127]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.127] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.223, 13.245, 34.754) yaw=-0.807 seq 445, hace 0 ms
[   66.143]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   66.143] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.335, 13.245, 34.816) yaw=1.567 seq 465, hace 17 ms
[   67.159]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.159] NET join 100.126.161.126:47811: enviados 21, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 486, hace 0 ms
[   68.175]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.175] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 506, hace 0 ms
[   69.192]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.192] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 526, hace 33 ms
[   70.209]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.209] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 546, hace 51 ms
[   71.224]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.224] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 567, hace 16 ms
[   72.240]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.240] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.326, 13.244, 34.720) yaw=1.568 seq 587, hace 17 ms
[   73.256]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   73.256] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.443, 13.252, 34.835) yaw=1.406 seq 607, hace 33 ms
[   74.273]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.273] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.443, 13.276, 34.835) yaw=1.406 seq 628, hace 0 ms
[   75.289]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   75.289] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.140, 13.277, 34.885) yaw=1.406 seq 648, hace 0 ms
[   76.305]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   76.305] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.140, 13.278, 34.885) yaw=1.406 seq 668, hace 33 ms
[   77.321]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   77.321] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.140, 13.279, 34.885) yaw=-0.129 seq 688, hace 50 ms
[   78.338]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   78.338] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.140, 13.280, 34.885) yaw=-1.610 seq 709, hace 17 ms
[   79.353]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   79.353] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.140, 13.280, 34.885) yaw=-1.610 seq 729, hace 33 ms
[   80.370]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   80.370] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.145, 13.280, 34.884) yaw=-1.609 seq 750, hace 0 ms
[   81.385]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   81.385] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.442, 13.245, 34.768) yaw=-0.129 seq 770, hace 16 ms
[   82.402]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   82.402] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.336, 13.244, 34.817) yaw=1.566 seq 790, hace 33 ms
[   83.402]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   83.402] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 810, hace 33 ms
[   84.417]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   84.417] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 831, hace 0 ms
[   85.433]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   85.433] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-3.370, 13.236, 34.810) yaw=1.571 seq 851, hace 17 ms
[   86.449]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   86.449] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.803, 13.244, 34.724) yaw=1.568 seq 871, hace 33 ms
[   87.466]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   87.466] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.443, 13.275, 34.835) yaw=1.416 seq 892, hace 0 ms
[   88.482]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
[   88.482] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-2.443, 13.276, 34.835) yaw=1.416 seq 912, hace 16 ms
[   89.498]    red: NETZ 0x096d42e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x096cfef8 proveedor 0x00000000 +2C=0 +2D=0
```
Últimas posiciones del jugador:
```text
[  282.336]    J0 pos=(0.716, 13.276, 39.928) yaw=-0.6969 rad (-39.9 grados) | se movió 0.000 desde el ST anterior
[  283.352]    J0 pos=(0.716, 13.276, 39.928) yaw=-0.6969 rad (-39.9 grados) | se movió 0.000 desde el ST anterior
[  284.369]    J0 pos=(0.716, 13.276, 39.928) yaw=-0.6969 rad (-39.9 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Paso 7b: filtro de usos (`AIMP_SYNC_USE_SKIP`, 2026-11-05)

Setup:
- DLL `1c1d558` (dist `d36b46b`), SHA-256 `7EE4AB94…71CB1E`, igual a `mod/dist/SHA256SUMS`.
- `aimp.cfg` igual que en el paso 7. El filtro por defecto es `[3e3ec35d,1e38b898]`, y el log lo confirma al arrancar.
- Laptop con Steam sin conexión y papito en host. Las dos partidas en Tech_Hub.

Hechos (salen de `logs/aimp-usar-20261105-133059.log`):
- **Un uso local filtrado:** `[114.560] SYNC uso local 1: … objetivo 1e38b8988722c94d (terminal de guardado), personaje 2ff722b2099d5b9b (jugador local), flag 013a5801`.
  - Le sigue `SYNC   no se manda: el objetivo empieza con un prefijo de AIMP_SYNC_USE_SKIP`.
  - El uso se ejecutó igual en la laptop: aparecen el handler A y el handler de sesión con el mismo objetivo.
  - Durante ese uso, la cámara local queda fija entre 116.9 y 121 s en (4.545, 14.917, 40.061). El remoto no cambia de posición: está en (4.502, 13.276, 38.286) a los 116.9 y a los 121 s.
- **Un uso remoto aplicado (no filtrado):** `[168.809] SYNC: aplicando uso remoto (evento 1): objetivo 044106b1eb7fc7c0, RPC A 0x26, personaje = jugador local, flag 007c5801`. Le siguen el handler A y el handler de sesión del uso 2, con el mismo objetivo.
  - Antes, en 168.725: J0 en (2.358, 13.277, 38.889) yaw -0.826, y el remoto en (-0.884, 13.276, 42.510) yaw -1.541.
  - Después, en 170.741: **J0 en (-1.562, 13.276, 42.670) yaw -1.5708 (-90.0° exacto)**, a ~5.5 m de donde estaba. El remoto está en (-1.277, 13.276, 42.242) yaw -1.571.
  - J0 y la cámara quedan fijos al menos entre 170.7 y 172.8 s. En 174.8 J0 se mueve de nuevo (`se movió 2.198`) y la cámara vuelve a girar.
  - Esta vez J0 no coincide exacto con el remoto: hay ~0.5 m de diferencia. En el locker del paso 7 coincidían los 3 decimales.
- `NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)`. Desde la laptop no se mandó ningún evento: el único uso local fue el de la terminal, que se filtró.
- MARCA 1 a los 128.9 s, MARCA 2 a los 138.8 s, MARCA 3 a los 167.3 s y MARCA 4 a los 176.8 s.
- Ninguna `EXCEPCIÓN`. Salida limpia: `ModeManager::Run devolvió 0` (14374 frames) y `DLL_PROCESS_DETACH`. 0 perdidos, 20/s, 60 fps.

Hipótesis (no verificadas):
- El filtro funciona: la terminal de guardado no se mandó.
- El objeto `044106b1…` (que no está en el filtro) también tiene un anclaje para el jugador. El uso remoto movió al jugador de la laptop hasta él (yaw -90° exacto) y lo trabó ~2-4 s, como una animación de interacción (panel, botón con animación o caja).
  - Si es así, el filtro por prefijo no alcanza para separar "estado del mundo" de "acción del jugador": todo objeto cuyo uso tenga animación arrastra al otro jugador.
  - Puede servir mirar si el uso tiene anclaje o animación, en lugar de filtrar por tipo.
- Falta: lo que vio el usuario en cada pantalla. No hay datos sobre qué es `044106b1…`, si se abrió o activó en las dos máquinas, ni si en algún momento se levantó un ítem.

## Instalación (2026-11-05 13:34:07)
- Copiado `winmm.dll` (SHA-256 `71F7F1DAAA29188BE822094F3E42A1E9C48923F39729EB5F5C2BFC88EA32C3AE`) a `c:\program files (x86)\steam\steamapps\common\Alien Isolation`.
- `aimp.cfg`: AIMP_NET=join; AIMP_NET_PEER=100.126.161.126:47811; AIMP_GHOST=3; AIMP_SYNC_USE=1
- Para deshacer: `-Action revert`.

## Resultado: usar (2026-11-05 13:38:21)
- Log copiado a `.\work\windows\logs\aimp-usar-20261105-133822.log` (192968 bytes, modificado 11/05/2026 13:38:11).
```text
[    0.002] AI_Multiplayer 0.2.0 (fases 1-3: proxy, hook por frame, sonda de estado, UDP del jugador), commit cba2b2e, compilado Nov  5 2026 18:31:19
[    0.002] DLL: C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\WINMM.dll (0x73fa0000), link 0x6aecf5a7, pid 13072
[    0.002] config: 1 archivo(s) aimp.cfg leídos; aplicadas: AIMP_NET=join AIMP_NET_PEER=100.126.161.126:47811 AIMP_GHOST=3 AIMP_SYNC_USE=1
[    0.002] plataforma: Windows 10.0.19045
[    0.003] proxy: winmm real = C:\Windows\system32\winmm.dll (0x73f70000)
[    0.003] proxy: 195 exports resueltos, 10 ausentes en la winmm real (2 de ellos son solo de Windows)
[    0.003] proxy: ausentes: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel
[    0.005] build: TimeDateStamp 0x54e3199d OK | SizeOfImage 0x01f81000 OK
[    0.005] build: RVA ovr_Initialize 0xba83b0 OK | RVA curl_easy_init 0xceab20 OK
[    0.005] build: prólogo de ModeManager::Run (RVA 0x3a8460) OK
[    0.005] build reconocida (AI.exe dde66521...19a90, buildid 4940658)
[    0.007] NET: join hacia 100.126.161.126:47811, 20 paquetes/s
[    0.008] sonda: muestra cada 250 ms, resumen cada 1000 ms, tecla de volcado 0x78, tecla de marca 0x77, primeros 3 frames al detalle
[    0.008] hook ModeManager::Run: instalado en 0x01388460 (bytes robados [51 53 55 56 8b f1], trampolín 0x00f80000, detour 0x73fa28a0)
[    0.008] hook spawn de autoridad (FUN_00797490): instalado en 0x01377490 (bytes robados [55 8b ec 53 56 57], trampolín 0x00f90000, detour 0x73fa8f00)
[    0.008] hook SpawnCharacter (FUN_007d3b50): instalado en 0x013b3b50 (bytes robados [51 53 55 56 57], trampolín 0x00fa0000, detour 0x73fa8870)
[    0.008] hook usar (FUN_00795d00): instalado en 0x01375d00 (bytes robados [55 8b ec 56 57], trampolín 0x00fb0000, detour 0x73faa3b0)
[    0.008] hook handler A del uso (FUN_00418a10): instalado en 0x00ff8a10 (bytes robados [55 8b ec 53 56 57], trampolín 0x00fc0000, detour 0x73faa290)
[    0.008] hook handler de sesión del uso (FUN_00418980): instalado en 0x00ff8980 (bytes robados [55 8b ec 56 57], trampolín 0x00fd0000, detour 0x73faa240)
[   34.341] FASE 1: primer frame dentro de ModeManager::Run (this=0x090bffb4)
[   34.343] Run tardó 32.85 s (¿carga?)
[   34.343]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   34.343] NET join: enviados 1, recibidos 0, inválidos 0 | sin datos del otro jugador (par: 100.126.161.126:47811
[   34.467] NET: primer estado del otro jugador: nivel 16, con jugador, pos=(0.88, 13.28, 35.61) yaw=-2.159
[   35.350]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   35.350] NET join 100.126.161.126:47811: enviados 20, recibidos 18, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 18, hace 17 ms
[   36.366]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   36.366] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 38, hace 34 ms
[   37.382]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   37.382] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 58, hace 50 ms
[   38.399]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   38.399] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 78, hace 67 ms
[   39.414]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   39.414] NET join 100.126.161.126:47811: enviados 19, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 99, hace 34 ms
[   40.431]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   40.431] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 120, hace 0 ms
[   41.446]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   41.446] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 140, hace 0 ms
[   42.463]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   42.463] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 160, hace 32 ms
[   43.479]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   43.479] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 180, hace 49 ms
[   51.398] Run tardó 7.37 s (¿carga?)
[   51.398] EV nivel actual 0 "Production\Frontend" -> 16 "Production\Tech_Hub"
[   51.398]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   51.398] NET join 100.126.161.126:47811: enviados 12, recibidos 158, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 338, hace 0 ms
[   55.043]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 00000000:00000000 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   55.043] NET join 100.126.161.126:47811: enviados 1, recibidos 74, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 412, hace 0 ms
[   55.307] EV jugador 0: 0x00000000 -> 0x2b2497d0 (vtable rva 0xef5d80 = Character(ok); def 0x090c5160 tipo 0 flags 1c=1 1f=1)
[   55.308] NETZ +0x000 @ 0x090c02e0 (0x20 bytes)
[   55.308] NETZ +0x690 @ 0x090c0970 (0x10 bytes)
[   56.057]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (1 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   56.057] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 432, hace 32 ms
[   57.073]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   57.073] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 453, hace 0 ms
[   58.089]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   58.089] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 473, hace 16 ms
[   59.105]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   59.105] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 493, hace 33 ms
[   60.121]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   60.121] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 514, hace 0 ms
[   60.322] NETZ +0x000 @ 0x090c02e0 (0x20 bytes)
[   60.322] NETZ +0x690 @ 0x090c0970 (0x10 bytes)
[   61.137]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   61.137] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 534, hace 17 ms
[   62.153]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   62.153] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.881, 13.279, 35.609) yaw=-2.159 seq 554, hace 33 ms
[   63.169]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   63.169] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.831, 13.279, 35.694) yaw=-0.529 seq 574, hace 50 ms
[   64.185]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   64.185] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.119, 13.279, 37.751) yaw=1.606 seq 594, hace 50 ms
[   65.202]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   65.202] NET join 100.126.161.126:47811: enviados 21, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.083, 13.279, 40.193) yaw=1.675 seq 615, hace 33 ms
[   66.218]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   66.218] NET join 100.126.161.126:47811: enviados 20, recibidos 19, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.002, 13.279, 41.777) yaw=2.369 seq 634, hace 83 ms
[   67.234]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   67.234] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.142, 13.279, 42.644) yaw=-1.301 seq 654, hace 100 ms
[   68.250]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   68.250] NET join 100.126.161.126:47811: enviados 21, recibidos 22, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.996, 13.279, 42.663) yaw=-1.662 seq 676, hace 16 ms
[   69.267]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   69.267] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.996, 13.279, 42.663) yaw=-1.662 seq 696, hace 0 ms
[   70.282]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   70.282] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.996, 13.279, 42.663) yaw=-1.662 seq 717, hace 0 ms
[   71.299]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   71.299] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.522, 13.378, 42.609) yaw=-1.571 seq 737, hace 16 ms
[   71.299] NET eventos: enviados 0, recibidos 1, repetidos descartados 2 (totales)
[   72.315]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   72.315] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-1.522, 13.276, 42.609) yaw=-1.571 seq 757, hace 0 ms
[   73.331]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   73.331] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.593, 13.276, 42.349) yaw=-1.571 seq 778, hace 0 ms
[   74.348]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   74.348] NET join 100.126.161.126:47811: enviados 21, recibidos 19, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 797, hace 50 ms
[   75.363]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   75.363] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 818, hace 0 ms
[   76.380]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   76.380] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 839, hace 0 ms
[   77.396]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   77.396] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 859, hace 17 ms
[   78.412]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   78.412] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 879, hace 17 ms
[   79.428]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   79.428] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 900, hace 0 ms
[   80.444]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   80.444] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 920, hace 17 ms
[   81.460]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   81.460] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 940, hace 33 ms
[   82.477]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   82.477] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 961, hace 0 ms
[   83.493]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   83.493] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(-0.019, 13.276, 41.838) yaw=-1.571 seq 981, hace 17 ms
[   84.509]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   84.509] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(0.686, 13.276, 41.680) yaw=1.796 seq 1001, hace 33 ms
[   85.525]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   85.525] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(2.365, 13.276, 39.936) yaw=1.604 seq 1022, hace 0 ms
[   86.541]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   86.541] NET join 100.126.161.126:47811: enviados 20, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(4.743, 13.276, 39.811) yaw=1.604 seq 1042, hace 16 ms
[   87.558]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   87.558] NET join 100.126.161.126:47811: enviados 21, recibidos 20, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.542, 13.276, 40.090) yaw=1.747 seq 1062, hace 34 ms
[   88.574]    red: NETZ 0x090c02e0 transporte 0x00000000 máscara 0000003f:00000001 (0 cambios) | sesión 0x090bbef8 proveedor 0x00000000 +2C=0 +2D=0
[   88.574] NET join 100.126.161.126:47811: enviados 20, recibidos 21, perdidos 0, inválidos 0 | remoto: nivel 16 con jugador pos=(6.556, 13.276, 40.089) yaw=1.747 seq 1083, hace 0 ms
```
Últimas posiciones del jugador:
```text
[  129.120]    J0 pos=(7.835, 13.276, 38.819) yaw=0.9782 rad (56.0 grados) | se movió 0.000 desde el ST anterior
[  130.136]    J0 pos=(7.835, 13.276, 38.819) yaw=0.9782 rad (56.0 grados) | se movió 0.000 desde el ST anterior
[  131.153]    J0 pos=(7.835, 13.276, 38.819) yaw=0.9782 rad (56.0 grados) | se movió 0.000 desde el ST anterior
```
- Sin eventos de error de AI.exe en el Visor de eventos (últimas 3 h).

## Paso 7c: el avatar hace el uso remoto (`AIMP_SYNC_USE_ACTOR=1`, corrida 12, 2026-11-05)

Setup:
- DLL `cba2b2e` (dist `6d19b71`), SHA-256 `71F7F1DA…32C3AE`, igual a `mod/dist/SHA256SUMS`.
- `aimp.cfg` igual que en el paso 7. `AIMP_SYNC_USE_ACTOR` queda en su valor por defecto, 1.
- Laptop con Steam sin conexión y papito en host. Las dos partidas en Tech_Hub.
- El primer `steam://rungameid` no abrió el juego: Steam se había reiniciado hacía un momento al pasar a offline, y el log seguía siendo el de la corrida anterior. Al relanzarlo arrancó bien.

Hechos (salen de `logs/aimp-usar-20261105-133822.log`):
- **Uso remoto hecho por el avatar:** `[70.832] SYNC: aplicando uso remoto (evento 1): objetivo 044106b1eb7fc7c0, RPC A 0x26, personaje = el avatar (1c3460b37e03a5dc), flag 007c5801`.
  - Le siguen el handler A y el handler de sesión del uso 1, con personaje `1c3460b37e03a5dc`, que es el NPC "CV5" elegido como avatar a los 63.1 s.
  - **El jugador local no se mueve:** J0 queda en (3.158, 13.278, 39.554) yaw -1.1227 desde 68.25 hasta 77.4 s, con la cámara fija en (3.273, 14.85, 39.498) y sin cambios de q. En el paso 7b, el mismo objeto había arrastrado a J0 unos 5.5 m.
  - El avatar va de (-1.00, 13.28, 42.66) a (-1.52, 13.37, 42.61) y después a (-0.02, 13.28, 41.84). Esas son las posiciones que manda el jugador remoto (yaw -1.571 desde 71.3), así que con este log no se puede separar lo que hizo el uso de lo que hizo el seguimiento por red.
- **Uso local mandado:** `[91.646] SYNC uso local 1: … objetivo 044106b1071fbb9e, personaje 2ff722b2099d5b9b (jugador local), flag 013a5801`, seguido de `SYNC   mandado al otro jugador (evento 1, nivel 16)`. Es otro objeto con el mismo prefijo `044106b1`.
  - En la laptop, J0 va de (9.110, 13.278, 38.775) a (10.551, 13.378, 38.695) **yaw 1.5708 (90° exacto)**: el jugador local se pone en el anclaje del objeto que usó.
  - El jugador remoto (papito) **no se mueve** mientras tanto: queda en (6.556, 13.276, 40.089) yaw 1.747 de 89.6 a 95.7 s. Con el actor avatar, el uso de la laptop no arrastró al jugador de papito.
- `NET eventos: enviados 1, recibidos 1, repetidos descartados 2 (totales)`.
- MARCA 1 a los 90.7 s y MARCA 2 a los 98.6 s.
- Ninguna `EXCEPCIÓN`. Salida limpia: `ModeManager::Run devolvió 0` (5183 frames) y `DLL_PROCESS_DETACH`. 0 perdidos, ~20/s.

Hipótesis (no verificadas):
- Con el avatar como actor, el uso remoto ya no mueve al jugador que lo recibe, en ninguno de los dos sentidos.
- Falta saber si el uso tuvo efecto en el mundo: si el botón o el transit se activó en la máquina que lo recibe. Eso no se ve en el log y no hay datos de lo que vio el usuario en cada pantalla.
- `044106b1` parece ser el prefijo de un tipo de objeto con anclaje (botón o panel). Hay dos instancias: `…eb7fc7c0`, que usó papito, y `…071fbb9e`, que usó la laptop.
