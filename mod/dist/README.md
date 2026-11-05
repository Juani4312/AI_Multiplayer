# mod/dist

`winmm.dll` compilada para probar en máquinas sin el toolchain (por ejemplo, la laptop Windows). Es el
mod, no un archivo del juego. SHA-256 en `SHA256SUMS`.

- Commit de origen: el que la agregó o actualizó (`git log -- mod/dist/winmm.dll`).
- Se regenera con `cmake --build mod/build` y copiando `mod/build/winmm.dll` acá (y `sha256sum winmm.dll > SHA256SUMS`).
- Uso en Windows: `work/windows/instructions.md`.
