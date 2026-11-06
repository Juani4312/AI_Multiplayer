#ifndef AIMP_PINS_H
#define AIMP_PINS_H
#include "aimp.h"
#include "net.h"

/* Fase 6 (hallazgos 15 y 16): pines de salida de CATHODE.
 * Hook en FUN_008be0d0 (EntityInterface::vt[+0x394], "disparar pin").
 * - AIMP_TRACE_PINS=1: traza. Por cada (entidad, pin) loguea las primeras apariciones con el
 *   ShortGuid del pin, el id, el tipo, el pid, los links y quién lo disparó. Solo mira.
 *   AIMP_TRACE_PINS_N = apariciones detalladas por (entidad, pin) (2); AIMP_TRACE_PINS_MAX = tope (20000).
 * - AIMP_PINS_FILE=ruta: comandos FIRE / INDEX (ver pins.c).
 * - AIMP_SYNC_PINS=1 (con red): el host manda los disparos de las reglas AIMP_SYNC_PINS_LIST
 *   (tipo:pin, por defecto los de zona); el cliente bloquea los suyos y aplica los del host. */
int pins_install(AimpNet *net);
int pins_enabled(void);
int pins_sync_enabled(void);
/* Desde el thread principal, cada AIMP_TRACE_MS: los (entidad, pin) más frecuentes del período. */
void pins_report_period(void);
void pins_report_totals(const char *why);
/* Por frame (thread principal): cambio de nivel, archivo de comandos, eventos por mandar y por aplicar. */
void pins_frame(int64_t now, uint32_t level);
/* Evento AIMP_EVT_PIN recibido (lo despacha sync_frame). */
void pins_remote_event(const AimpEvt *e);
/* Reinyección: vuelve a disparar `pin` sobre la entidad con ese pid (o id). mflags != 0 cambia la
 * clase del mensaje (bits 0x0F000000, p. ej. entró/salió). 1 si disparó. */
int pins_fire(uint32_t id, uint32_t pin, uint32_t mflags);
/* Índice pid -> entidad, recorriendo el árbol de composites desde las raíces conocidas. Devuelve
 * cuántas entidades indexó. */
uint32_t pins_index_build(const char *why);

/* Para tests: instalar el hook sobre una función con el mismo prólogo. */
int pins_install_at(void *target, AimpNet *net);
/* Para tests: cambiar de rol (1 host, 2 cliente) sin reinstalar. */
void pins_test_role(int role);

#endif
