cmake_minimum_required(VERSION 3.20)
project(aimp C)

# Uso:
#   cmake -S mod -B mod/build -DCMAKE_TOOLCHAIN_FILE=$PWD/mod/cmake/toolchain-i686-w64-mingw32.cmake
#   (la ruta del toolchain tiene que ser absoluta: CMake la resuelve desde el directorio de build)
#   cmake --build mod/build
# Salida: mod/build/winmm.dll (el proxy) y los tests en mod/build/test/.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Hay que compilar para Windows i686 (usar cmake/toolchain-i686-w64-mingw32.cmake)")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Commit de git para el log, recalculado en cada build (ver cmake/githash.cmake).
add_custom_target(aimp_githash
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/githash.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/githash.cmake
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/githash.h
  COMMENT "commit de git -> githash.h")

# Opciones comunes. Sin CRT: el proxy solo importa kernel32.
set(AIMP_C_FLAGS
  -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter
  -ffreestanding -fno-stack-protector -fno-builtin
  -fno-asynchronous-unwind-tables -fno-unwind-tables
  -fno-tree-loop-distribute-patterns
  # MSVC solo garantiza 4 bytes de alineación del stack en las llamadas que nos hace el juego.
  -mincoming-stack-boundary=2
)

add_library(aimp_winmm SHARED
  src/dllmain.c
  src/cfg.c
  src/proxy.c
  src/game.c
  src/hook.c
  src/probe.c
  src/trace.c
  src/pins.c
  src/net.c
  src/ghost.c
  src/crash.c
  src/sync.c
  src/log.c
  src/mem.c
  src/fmt.c
  src/rt.c
  src/crt.c
  src/winmm.def
)
set_target_properties(aimp_winmm PROPERTIES OUTPUT_NAME winmm PREFIX "" SUFFIX ".dll")
target_compile_options(aimp_winmm PRIVATE ${AIMP_C_FLAGS})
add_dependencies(aimp_winmm aimp_githash)
target_include_directories(aimp_winmm PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_options(aimp_winmm PRIVATE
  -nostdlib -nostartfiles -static-libgcc -s
  -Wl,-e,_DllMainCRTStartup@12
  -Wl,--dynamicbase -Wl,--nxcompat
)
target_link_libraries(aimp_winmm PRIVATE kernel32 gcc)

# ---- Tests (se corren con wine; ver README) ----
# test_host: importa winmm.dll como AI.exe (por nombre) y la usa. Con el proxy al lado
# y WINEDLLOVERRIDES=winmm=n,b, carga el proxy, que tiene que reenviar a la real.
add_executable(test_host test/test_host.c)
target_link_libraries(test_host PRIVATE winmm)
set_target_properties(test_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_core: prueba hook.c sobre una función con el mismo prólogo que ModeManager::Run,
# y el formateador (fmt.c) contra el snprintf del CRT.
add_executable(test_core test/test_core.c test/test_shim.c src/hook.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_core PRIVATE src)
target_compile_options(test_core PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_core PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_probe: la sonda (probe.c + log.c) contra una imagen de AI.exe simulada.
add_executable(test_probe test/test_probe.c src/probe.c src/trace.c src/pins.c src/net.c src/ghost.c src/sync.c src/hook.c src/log.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_probe PRIVATE src)
target_compile_options(test_probe PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_net: host y cliente del protocolo de la fase 3 en el mismo proceso (UDP por 127.0.0.1).
add_executable(test_net test/test_net.c src/net.c src/log.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_net PRIVATE src)
target_link_libraries(test_net PRIVATE ws2_32)
target_compile_options(test_net PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_net PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_ghost: fase 4 (ghost.c) contra stubs con los prólogos y el `ret N` reales del spawn
# de autoridad y del teleport (prueba las convenciones de llamada).
add_executable(test_ghost test/test_ghost.c src/ghost.c src/crash.c src/hook.c src/log.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_ghost PRIVATE src)
target_compile_options(test_ghost PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_ghost PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_pins: fase 6 (pins.c) contra un stub con el prólogo y el `ret 0x18` reales de
# FUN_008be0d0 (disparar pin de salida de CATHODE), con entidades y tablas de links falsas.
add_executable(test_pins test/test_pins.c src/pins.c src/hook.c src/log.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_pins PRIVATE src)
target_compile_options(test_pins PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_pins PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# test_sync: fase 5 (sync.c) contra stubs con los prólogos reales del productor del "usar",
# sus dos handlers y el wrapper de encolado A; la red es real (127.0.0.1).
add_executable(test_sync test/test_sync.c src/sync.c src/ghost.c src/net.c src/hook.c src/log.c src/mem.c src/fmt.c src/rt.c)
target_include_directories(test_sync PRIVATE src)
target_compile_options(test_sync PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -mincoming-stack-boundary=2)
set_target_properties(test_sync PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)
