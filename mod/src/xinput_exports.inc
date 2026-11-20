/* Exports del proxy xinput1_3.dll (variante del mod para cuando la winmm no carga).
 * Escrito a mano (son 11). Fuentes:
 *   - xinput1_3.dll de Proton 9 (syswow64 del prefix 214490, sha256 8d540d48...98970a7):
 *     objdump -p, ordinales 1-8 con nombre y 100-103 solo por ordinal;
 *   - nombres de libxinput1_3.a de mingw-w64 (los mismos 7 de la API);
 *   - AI.exe importa XINPUT1_3.dll solo por ordinal: 2, 3 y 4 (objdump -p, IAT 0xD9F4D8..0xD9F4E0).
 * El ordinal 1 (DllMain) no se reexporta: nadie lo importa y reenviarlo llamaría al DllMain de la real.
 *
 * X(nombre, ordinal): export por nombre.  O(ordinal): export solo por ordinal. */
X(XInputGetState, 2)
X(XInputSetState, 3)
X(XInputGetCapabilities, 4)
X(XInputEnable, 5)
X(XInputGetDSoundAudioDeviceGuids, 6)
X(XInputGetBatteryInformation, 7)
X(XInputGetKeystroke, 8)
O(100)
O(101)
O(102)
O(103)
