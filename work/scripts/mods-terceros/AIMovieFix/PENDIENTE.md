# AIMovieFix 1.1 beta (emoose) — sin README; ya está instalado

El zip solo trae `xinput1_3.dll` (sha256 `71a8b86d…`). Es la **misma DLL** que la `d3d11.dll` que ya está en la carpeta
del juego: MovieFix es un proxy "universal" (exporta los nombres de xinput1_3, d3d11, dxgi, winmm, dinput8…) y se puede
instalar con cualquiera de esos nombres. En Proton arranca por `d3d11.dll` (`aimoviefix.log`); la copia como
`xinput1_3.dll` no se carga porque `AI.exe` no tiene override de xinput1_3 en el prefix.

**Interfiere con el desarrollo del coop: solo con la variante `xinput1_3.dll` del mod coop** (mismo nombre de archivo).
El instalador del coop (`work/scripts/runtime_setup.sh --proxy xinput1_3 --apartar-ajena`) la aparta y la devuelve al
desinstalar. Con la variante `winmm.dll` (la normal) convive: se usó así en las corridas 1–147.

Pendiente: decidir si hace falta un `toggle.sh` (instalar = copiar `xinput1_3.dll` a la raíz del juego). Hoy no conviene,
porque esa ruta la maneja también el instalador del coop.
