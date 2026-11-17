#ifndef AIMP_MEMSRV_H
#define AIMP_MEMSRV_H
#include "aimp.h"

/* Lector de memoria por archivo (AIMP_MEMSRV_FILE; ver memsrv.c). Solo lee. */
void memsrv_install(void);
/* Por frame, en el thread principal. */
void memsrv_frame(int64_t now, int64_t freq);

#endif
