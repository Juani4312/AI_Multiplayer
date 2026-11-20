# AIMovieFix 1.1 beta (emoose) — la DLL del Cutscene Remaster

Fuente: página del Cutscene Remaster v4 (08.10.2025; cutscenes de 1080p/30 a 4K/60), captura del usuario 2026-11-20:
> 1. Navigate to your game video directory e.g. …\Alien Isolation\DATA\UI\MOVIES
> 2. Copy the files from the 7z file into your video folder above.
> 3. Copy the DLL from the AIMovieFix.7z into your main game directory (where the main .exe file is)

- **Videos (pasos 1–2): ya estaban instalados** (15 `.USM` del 2026-10-05 en `DATA/UI/MOVIES`, p. ej. `CUTSCENE_00.USM`).
  Su comprimido no está en esta carpeta y los originales que pisaron no tienen backup: no tienen toggle. Para volver a los
  originales: Steam → Verificar integridad (ojo: también deshace los otros mods y las texturas).
- **DLL (paso 3): `toggle.sh`** copia `xinput1_3.dll` (sha256 `71a8b86d…`) a la raíz del juego; al desactivar la borra (el
  juego original no la trae).
- La misma DLL está también como `d3d11.dll` en la raíz desde antes (MovieFix es un proxy "universal"). En Proton es
  la que la carga (`aimoviefix.log`): `xinput1_3.dll` de la carpeta solo se cargaría con el override `xinput1_3=n,b`.
  El toggle no toca `d3d11.dll`.

**Interfiere con el coop:** solo con la variante `xinput1_3.dll` del mod coop (mismo nombre). El instalador del coop
(`runtime_setup.sh --proxy xinput1_3 --apartar-ajena`) la aparta y la devuelve; con la variante `winmm.dll` conviven.
