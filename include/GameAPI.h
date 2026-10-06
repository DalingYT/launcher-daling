// =========================================================
//  GameAPI.h
//  EL CONTRATO entre el Launcher y cada DLL de juego.
// =========================================================
#pragma once

#include "imgui/imgui.h"

// -----------------------------------------------------------------
//  FUNCIONES OBLIGATORIAS (sin estas 4, la DLL no se carga - ver
//  EscanearYCargarJuegos en main.cpp)
// -----------------------------------------------------------------

typedef const char* (*FnJuegoNombre)();
typedef void (*FnJuegoIniciar)();
typedef bool (*FnJuegoDibujar)();
typedef void (*FnJuegoSetImGuiContext)(
    ImGuiContext* ctx,
    ImGuiMemAllocFunc allocFn,
    ImGuiMemFreeFunc freeFn,
    void* userData
    );

// -----------------------------------------------------------------
//  FUNCIONES OPCIONALES (para Discord Rich Presence)
//
//  Una DLL de juego puede exportar CUALQUIERA de estas 3, todas,
//  o ninguna. El launcher revisa con GetProcAddress si existen;
//  si no existen, simplemente no las usa (no es un error).
// -----------------------------------------------------------------

// Que esta haciendo el jugador ahora mismo dentro del juego.
// Ejemplo: "Adivinando un numero", "Calculando una expresion"
typedef const char* (*FnJuegoActividad)();

// Si el juego tiene multijugador activo. Llena numJugadores/maxJugadores
// y devuelve true si aplica; devuelve false si no hay multijugador ahora.
typedef bool (*FnJuegoMultijugador)(int* numJugadores, int* maxJugadores);

// Si el juego tiene progreso medible (ronda actual, escena actual, etc.)
// Llena actual/total y devuelve true si aplica. Si total es 0, significa
// "sin limite conocido" (solo se muestra el numero de actual).
typedef bool (*FnJuegoProgreso)(int* actual, int* total);

// -----------------------------------------------------------------
//  Nombres EXACTOS que el Launcher busca dentro de cada .dll
// -----------------------------------------------------------------
#define NOMBRE_FN_JUEGO_NOMBRE            "Juego_Nombre"
#define NOMBRE_FN_JUEGO_INICIAR           "Juego_Iniciar"
#define NOMBRE_FN_JUEGO_DIBUJAR           "Juego_Dibujar"
#define NOMBRE_FN_JUEGO_SET_IMGUI_CONTEXT "Juego_SetImGuiContext"

// Opcionales (Discord Rich Presence)
#define NOMBRE_FN_JUEGO_ACTIVIDAD     "Juego_Actividad"
#define NOMBRE_FN_JUEGO_MULTIJUGADOR  "Juego_Multijugador"
#define NOMBRE_FN_JUEGO_PROGRESO      "Juego_Progreso"