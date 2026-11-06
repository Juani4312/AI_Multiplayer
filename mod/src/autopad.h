#ifndef AIMP_AUTOPAD_H
#define AIMP_AUTOPAD_H
#include "aimp.h"

/* Joystick virtual para pruebas sin nadie al teclado (AIMP_AUTOPAD).
 * Reemplaza en la IAT de AI.exe los slots de XINPUT1_3 ordinal 2 (XInputGetState) y 4
 * (XInputGetCapabilities): el mando 0 aparece conectado y, encima de lo que haya de verdad, se
 * aplican los pasos del guion activos en ese momento.
 *
 * Guion: pasos separados por ',' (no ';': runtime_setup.sh usa ';' entre variables):
 *     [P]inicio-fin:acción+acción...
 * - tiempos en ms; sin prefijo, desde que se instaló el mod; con 'P', desde que apareció el
 *   jugador 0 (se reinicia en cada carga de nivel).
 * - acciones: A B X Y START BACK LB RB LS RS UP DOWN LEFT RIGHT, LT RT (gatillo a fondo),
 *   LX=n LY=n RX=n RY=n (stick, -32768..32767), MARK (una MARCA en el log al empezar el paso),
 *   GO=x/z (caminar hasta (x, z) girando con RX y avanzando con LY; los GO se encadenan: cada uno
 *   espera a que termine el anterior; si no se acerca en 4 s se saltea. El fin del paso es el tope).
 * Ejemplo: AIMP_AUTOPAD=9000-9200:A,13000-13200:A,P3000-3100:MARK,P3000-8000:LY=32000
 * AIMP_AUTOPAD_FILE=ruta: archivo con el mismo formato (también vale un paso por línea). Se relee
 * cada 250 ms si cambió; reemplaza el guion y sus pasos sin 'P' cuentan desde la lectura. Sirve para
 * manejar el juego desde afuera ("0-200:A" = apretar A ahora). */
int autopad_install(void);
/* Por frame, desde el thread principal. */
void autopad_frame(int64_t now, int64_t freq, int have_player, const float *pos, float yaw);

/* Para tests: parsear un guion y consultar el estado en un instante. */
int autopad_parse(const char *script);
void autopad_state_at(int64_t ms_since_start, int64_t ms_since_player, uint16_t *buttons, int16_t thumbs[4],
                      uint8_t trig[2]);
/* Para tests: instalar sobre una IAT arbitraria (módulo cargado con import de XINPUT1_3). */
int autopad_install_on(uintptr_t module);

#endif
