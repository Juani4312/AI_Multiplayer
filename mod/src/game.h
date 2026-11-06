/* Direcciones y offsets de AI.exe usados por el mod.
 *
 * Build: AI.exe PE32 i386, SHA-256 dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90
 * (Steam buildid 4940658). Todas son RVAs (AI.exe tiene ASLR): en runtime, base + RVA.
 * Cada valor sale de un hallazgo de work/findings/ (evidencia de Ghidra/REA).
 * NINGUNO está verificado en runtime todavía: esta versión del mod es la que los verifica.
 */
#ifndef AIMP_GAME_H
#define AIMP_GAME_H
#include <stdint.h>

/* ---- Huella de la build (hallazgo 01; verificado contra game/AI.exe) ---- */
#define AIMP_EXPECT_TIMESTAMP     0x54E3199Du  /* COFF TimeDateStamp */
#define AIMP_EXPECT_SIZEOFIMAGE   0x01F81000u
#define AIMP_RVA_OVR_INITIALIZE   0x00BA83B0u  /* export */
#define AIMP_RVA_CURL_EASY_INIT   0x00CEAB20u  /* export */

/* ---- Loop principal (hallazgo 02) ----
 * ModeManager::Run: thiscall(ECX = [g_root+0x3C]), sin argumentos en el stack (RET
 * simple en 0x7A868B/0x7A8693), devuelve AL (1 = seguir). Un solo caller: 0x7DB240.
 * Prólogo: push ecx; push ebx; push ebp; push esi; mov esi,ecx; cmp [esi+0x4C],-1.
 * Los 6 primeros bytes no tienen direccionamiento relativo ni relocations, y nadie
 * salta a 0x7A8461..0x7A8466 (el loop interno vuelve a 0x7A8471). */
#define AIMP_RVA_MODEMGR_RUN      0x003A8460u
#define AIMP_MODEMGR_RUN_STEAL    6
static const uint8_t AIMP_MODEMGR_RUN_PROLOGUE[10] __attribute__((unused)) = {
    0x51, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x4C, 0xFF
};

#define AIMP_TEXT_RVA             0x00001000u  /* sección .text */
#define AIMP_TEXT_SIZE            0x00D9D13Bu

/* Fase 4 (hallazgo 12): spawn de autoridad y teletransporte de personajes. */
#define AIMP_RVA_SPAWN_CHAR       0x003D3B50u  /* SpawnCharacter: thiscall(interfaz, pedido*), ret 4 (hallazgo 05) */
#define AIMP_SPAWN_CHAR_STEAL     5            /* push ecx/ebx/ebp/esi/edi */
static const uint8_t AIMP_SPAWN_CHAR_PROLOGUE[9] __attribute__((unused)) = {
    0x51, 0x53, 0x55, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x18
};
/* Listas de observadores que recorren el spawn de autoridad y el handler del RPC 0x29.
 * Nadie las escribe en el binario (ni con dirección absoluta ni con base cercana): se
 * loguean en runtime para confirmar que están vacías. */
#define AIMP_RVA_OBS_AUTH_PRE     0x012F322Cu
#define AIMP_RVA_OBS_AUTH_POST    0x012F3230u
#define AIMP_RVA_OBS_RPC_PRE      0x012A6728u
#define AIMP_RVA_OBS_RPC_POST     0x012A672Cu
/* Fase 5 (hallazgo 12, parte 3): "usar" del jugador (RPC A 0x26, hallazgo 11). */
#define AIMP_RVA_USE_PRODUCER     0x00395D00u  /* FUN_00795d00: thiscall(this, payload*, flag), ret 8 */
#define AIMP_USE_PRODUCER_STEAL   5            /* push ebp; mov ebp,esp; push esi; push edi */
static const uint8_t AIMP_USE_PRODUCER_PROLOGUE[7] __attribute__((unused)) = { 0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B, 0xF9 };
#define AIMP_RVA_USE_HANDLER_A    0x00018A10u  /* FUN_00418a10: cdecl(ctx, payload), A 0x26 -> B 0x29 */
#define AIMP_USE_HANDLER_A_STEAL  6
static const uint8_t AIMP_USE_HANDLER_A_PROLOGUE[11] __attribute__((unused)) = {
    0x55, 0x8B, 0xEC, 0x53, 0x56, 0x57, 0xB8, 0x24, 0x00, 0x00, 0x00
};
#define AIMP_RVA_USE_HANDLER_S    0x00018980u  /* FUN_00418980: cdecl(ctx, payload), sesión 0x29: ctx->vf[1](copia) */
#define AIMP_USE_HANDLER_S_STEAL  5
static const uint8_t AIMP_USE_HANDLER_S_PROLOGUE[10] __attribute__((unused)) = {
    0x55, 0x8B, 0xEC, 0x56, 0x57, 0xB8, 0x24, 0x00, 0x00, 0x00
};
#define AIMP_RVA_USE_TYPE_A       0x0123652Cu  /* u16: id del RPC A de uso (0 en el archivo; lo asigna el registro) */
#define SREQ_KEY2                 0xF0  /* repite la clave (arg 1 de FUN_00787450) */
/* Bloques de 0x1D0 bytes por personaje en el CharacterManager (SpawnCharacter, hallazgos 05 y 10). */
#define CM_BLOCKS                 0x230 /* 0x22C relativo a la interfaz de spawn (CM+0x04), que es el this de SpawnCharacter */
#define CM_BLOCK_SIZE             0x1D0
#define CM_BLOCK_MAX              36    /* (0x4370 - 0x22C) / 0x1D0 */
#define BLOCK_AUTHORITY           0x4F  /* byte: 1 = autoritativo (la lógica corre acá) */
#define BLOCK_CHARACTER           0x160 /* Character* */
#define SREQ_POS                  0x50  /* float[3]: posición del spawn */
#define SREQ_YAW                  0x60  /* float */
#define AIMP_RVA_SPAWN_AUTH       0x00397490u  /* FUN_00797490: thiscall(this, pedido*, flag), ret 8 */
#define AIMP_SPAWN_AUTH_STEAL     6            /* push ebp; mov ebp,esp; push ebx; push esi; push edi */
static const uint8_t AIMP_SPAWN_AUTH_PROLOGUE[8] __attribute__((unused)) = {
    0x55, 0x8B, 0xEC, 0x53, 0x56, 0x57, 0x8B, 0xF9
};
#define AIMP_RVA_CHR_TELEPORT     0x003C6330u  /* FUN_007c6330: thiscall(pj, pos*, yaw, p3) -> SetTransform(.., p3, 0), ret 0xC */
static const uint8_t AIMP_CHR_TELEPORT_PROLOGUE[13] __attribute__((unused)) = {
    0x53, 0x8B, 0x5C, 0x24, 0x08, 0x55, 0x8B, 0x6C, 0x24, 0x14, 0x57, 0x8B, 0xF9
};
#define SPAWN_REQ_SIZE            0x130
#define SREQ_KEY                  0x00  /* clave del bloque por personaje (mapa FUN_007b1760) */
#define SREQ_ID                   0x28  /* 8 bytes; en el del jugador, el id que va en los RPCs */
#define SREQ_TYPE                 0x30  /* 0 jugador; 3/6 en NPCs (= def+0x04) */
#define SREQ_NAME                 0x64  /* char[0x40] */
#define SREQ_COMPOSITE            0xA4  /* char[0x40] (composite o "CUTSCENE") */

#define AIMP_RVA_G_ROOT           0x012F0C88u  /* Root* (dato en .data) */

/* Campos de g_root (Root::Init 0x7DA540, hallazgos 02, 04, 05 y 10) */
#define ROOT_NETZ                 0x04  /* NETZ (capa de red del engine) */
#define ROOT_CHARMGR              0x08  /* CharacterManager */
#define ROOT_MODEMGR              0x3C  /* ModeManager */
#define ROOT_GAMEFLOW             0x48
#define ROOT_QUEUE_A              0x50  /* cola de mensajes RPC "A" */
#define ROOT_SESSION              0x54  /* sesión de red (== *DAT_01be1bd4) */
#define ROOT_CAMMGR               0x58  /* gestor de la cámara del jugador */

/* ModeManager (hallazgo 02) */
#define MM_MODE                   0x40  /* modo actual */
#define MM_INDEX                  0x44  /* índice en el anillo (& 0xF) */
#define MM_STATE                  0x4C  /* estado; 5 = corriendo, -1 = salir */
#define MODE_LEVEL_INDEX          0x04  /* modo->m_4 = índice del nivel (GetCurrentLevelName 0x78BBE0) */
#define AIMP_RVA_VT_LEVEL_MODE    0x00EF43E8u  /* vtable del "modo de nivel" */

/* ---- Timer del frame (hallazgo 04; datos en .data) ---- */
#define AIMP_RVA_GAME_TIME        0x012410A8u  /* double, tiempo de juego acumulado */
#define AIMP_RVA_DT_SCALED        0x012410C0u  /* double, dt escalado (0 en pausa) */
#define AIMP_RVA_DT_REAL          0x012410D0u  /* double, dt real */
#define AIMP_RVA_FRAME_COUNTER    0x012410DCu  /* int, +1 por frame */

/* ---- Lista de niveles (hallazgo 09) ----
 * Doble indirección: FUN_00a2e800/0x00a2e810 = MOV EAX,[0x1BE1C4C]; MOV EAX,[EAX]; RET.
 * L = **(base + RVA). Nombres de 0x46 bytes; 160 entradas (0x2BC0 / 0x46). */
#define AIMP_RVA_LEVEL_LIST_PP    0x017E1C4Cu
#define LEVEL_NAME_STRIDE         0x46
#define LEVELS_MAX                160
#define LEVELS_COUNT              0x2BC0
#define LEVELS_CURRENT            0x2BC8  /* lo escribe ModeManager::Run en el estado 0 */

/* ---- Personajes (hallazgos 03 y 05) ---- */
#define CM_PLAYERS                0x80  /* Character** */
#define CM_NPLAYERS               0x84
#define CM_NPCS                   0xD0
#define CM_NNPCS                  0xD4
#define CM_NPENDING               0x174
#define AIMP_RVA_VT_CHARACTER     0x00EF5D80u  /* vtable final (clase derivada) */
#define CHR_ID                    0x54  /* id de 8 bytes (verificado en runtime: va en los RPCs, hallazgo 11) */
#define CHR_DEF                   0x64  /* definición: +0x04 tipo, +0x1C/+0x1F flags de jugador */
#define CHR_POS                   0x70  /* float x, y (vertical), z */
#define CHR_YAW                   0x174 /* float, radianes [-pi, pi] */
#define CHR_CTRL                  0x31C /* controlador de jugador (+0x48 = activo) */
#define DEF_TYPE                  0x04
#define DEF_FLAG_1C               0x1C
#define DEF_FLAG_1F               0x1F
#define CTRL_ACTIVE               0x48

/* ---- Cámara (hallazgo 04) ---- */
#define CAMMGR_ACTIVE             0x1DC /* cámara activa */
#define CAM_QUAT                  0x2C  /* float x4 */
#define CAM_POS                   0x3C  /* float x3 */

/* ---- Red del engine (hallazgo 10) ---- */
#define NETZ_MASK                 0x10  /* 2 dwords: canales sucios */
#define NETZ_TRANSPORT            0x69C /* NULL en retail (esperado) */
#define AIMP_RVA_NET_SESSION      0x017E1BD4u  /* DAT_01be1bd4 = sesión */
#define SESSION_PROVIDER          0x00  /* NULL en retail (esperado) */
#define SESSION_FLAG_2C           0x2C
#define SESSION_FLAG_2D           0x2D
#define QA_SINK                   0x00  /* sumidero; NULL en retail */
#define QA_HANDLERS               0x08
#define QA_DATA                   0x30
#define QA_SIZE                   0x34
/* La cola A también está en un global: DAT_01be1bc8, que solo escribe su
 * constructor FUN_00a2a340 (0xA2A3D7). Lo usa el wrapper de los productores. */
#define AIMP_RVA_QUEUE_A          0x017E1BC8u
/* Cola B = buffer de la sesión (FUN_00a2a760: +0x14 objeto buffer, +0x18 datos,
 * +0x1C tamaño; sin lock). FUN_00a2a5c0 solo lo entrega al proveedor de red; sin
 * proveedor lo descarta cada frame. */
#define SESSION_QDATA             0x18
#define SESSION_QSIZE             0x1C

/* ---- Catálogo de endpoints RPC (hallazgo 10, partes 5 y 7; experimento 3.5) ----
 * Listas estáticas que llenan inicializadores globales; RegisterAll asigna los ids en
 * el orden de la lista: cola A = FUN_00a2a2b0 (lista DAT_01be1bcc, tabla [colaA+0x08],
 * cantidad +0x0C); sesión = FUN_00a2a620 (lista DAT_01be1bd0, tabla [sesión+0x08]).
 * Endpoint: +0x00 next, +0x04 fn (handler: fn(ctx, payload) -> bytes), +0x08 ctx, +0x0C u16 tipo. */
#define AIMP_RVA_RPC_LIST_A       0x017E1BCCu
#define AIMP_RVA_RPC_LIST_S       0x017E1BD0u
#define EP_NEXT                   0x00
#define EP_FN                     0x04
#define EP_CTX                    0x08
#define EP_TYPE                   0x0C
#define QA_NHANDLERS              0x0C
#define SESSION_NHANDLERS         0x0C

/* ---- Productores de mensajes (hallazgo 10, parte 3; experimento 3.6) ----
 * Wrappers cdecl (tipo u16, datos, tamaño) -> encolar. Prólogo verificado en el
 * archivo, sin relocations en los 14 bytes:
 *   55 push ebp | 8B EC mov ebp,esp | 8B 45 10 mov eax,[ebp+10] | 8B 4D 0C mov ecx,[ebp+0C]
 *   8B 55 08 mov edx,[ebp+8] | 50 push eax | 51 push ecx   (sigue MOV ECX,[global] relocado)
 * A: FUN_00a2a290 (31 callers) -> FUN_00a2a160(cola A). B: FUN_00a2a810 (41) -> FUN_00a2a760(sesión). */
#define AIMP_RVA_ENQUEUE_A        0x0062A290u
#define AIMP_RVA_ENQUEUE_B        0x0062A810u
#define AIMP_ENQUEUE_STEAL        6
static const uint8_t AIMP_ENQUEUE_PROLOGUE[14] __attribute__((unused)) = {
    0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x10, 0x8B, 0x4D, 0x0C, 0x8B, 0x55, 0x08, 0x50, 0x51
};

/* ---- Scripts de CATHODE: disparo de pines de salida (hallazgo 15, fase 6) ----
 * FUN_008be0d0 = EntityInterface::vt[+0x394] (lo heredan 777 clases de entidad con RTTI y las
 * vtables concretas, p. ej. TriggerSimple 0x01261EB8). Lo llaman los wrappers por pin
 * (FUN_008bd920: this->vt[0x394](ctx, &pin, msg, 0.0, 0)), p. ej. on_triggered/on_finished.
 * thiscall(this = subobjeto EntityInterface, ctx*, pin*, msg, double demora, flag), ret 0x18.
 * Prólogo: 55 push ebp | 8B EC mov ebp,esp | 6A FF push -1 | 68 <imm32 relocado> push SEH. */
#define AIMP_RVA_FIRE_PIN         0x004BE0D0u
#define AIMP_FIRE_PIN_STEAL       5
static const uint8_t AIMP_FIRE_PIN_PROLOGUE[6] __attribute__((unused)) = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
/* EntityInterface: +0x04 MemoryRefPtr a la tabla de links {+8 n, +0xC entradas de 12 bytes:
 * [0] ShortGuid del pin, [8] primer link}; link: +0x08 destino, +0x0C siguiente.
 * +0x08: índice (24 bits) en la tabla global de entidades [0x0174DFB0] | flags (8 bits). */
#define ENT_LINKS                 0x04u
#define ENT_INDEX                 0x08u
#define AIMP_RVA_ENTITY_TABLE     0x0134DFB0u  /* DAT_0174dfb0: puntero a {+0 array, +4 capacidad (potencia de 2), +8 cantidad} */

#endif
