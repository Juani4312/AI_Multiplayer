# Escribe OUT (githash.h) con el commit actual. Lo corre CMakeLists.txt en cada build, no
# solo al configurar: antes el log de la DLL mostraba un commit viejo.
# Agrega "+cambios" si hay cambios sin commitear en mod/src (el resto del repo no importa).
# Solo reescribe OUT si cambió, así dllmain.c se recompila únicamente cuando cambia el commit.
execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY "${SRC}"
                OUTPUT_VARIABLE hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT hash)
  set(hash "desconocido")
else()
  execute_process(COMMAND git status --porcelain -- src WORKING_DIRECTORY "${SRC}"
                  OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(dirty)
    string(APPEND hash "+cambios")
  endif()
endif()
set(content "#define AIMP_GIT_HASH \"${hash}\"\n")
set(old "")
if(EXISTS "${OUT}")
  file(READ "${OUT}" old)
endif()
if(NOT "${old}" STREQUAL "${content}")
  file(WRITE "${OUT}" "${content}")
endif()
