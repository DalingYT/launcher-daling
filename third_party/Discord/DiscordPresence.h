// =========================================================
//  DiscordPresence.h
//  Wrapper simple sobre discord-rpc para mostrar el estado
//  del launcher en el perfil de Discord (Rich Presence).
// =========================================================
#pragma once

#include "discord_rpc.h"
#include <string>
#include <ctime>

class DiscordPresence {
public:
    static constexpr const char* APP_ID = "1521355590096388158";

    void Iniciar() {
        DiscordEventHandlers handlers = {};
        Discord_Initialize(APP_ID, &handlers, 1, nullptr);

        horaInicio = (int64_t)std::time(nullptr);
        inicializado = true;

        MostrarEnMenu();
    }

    void Actualizar() {
        if (!inicializado) return;
        Discord_RunCallbacks();
    }

    void MostrarEnMenu() {
        if (!inicializado) return;

        // Guardamos el texto en una variable miembro (no temporal),
        // para que siga viva mientras Discord usa el puntero.
        textoEstado = "En el menu principal";

        DiscordRichPresence presence = {};
        presence.state = textoEstado.c_str();
        presence.details = "Launcher";
        presence.startTimestamp = horaInicio;
        presence.largeImageKey = "launcher_icon";
        presence.largeImageText = "Launcher - Daling";

        Discord_UpdatePresence(&presence);
    }

    void MostrarJugando(const std::string& nombreJuego) {
        if (!inicializado) return;

        // IMPORTANTE: armamos el texto completo PRIMERO, guardado en una
        // variable miembro, y DESPUES le pasamos el .c_str() a presence.state.
        // Asi el string vive en memoria estable, no en un temporal de la linea.
        textoEstado = "Jugando: " + nombreJuego;

        DiscordRichPresence presence = {};
        presence.state = textoEstado.c_str();
        presence.details = "Launcher";
        presence.startTimestamp = horaInicio;
        presence.largeImageKey = "launcher_icon";
        presence.largeImageText = "Launcher - Daling";

        Discord_UpdatePresence(&presence);
    }

    void MostrarEscuchandoMusica(const std::string& nombreCancion) {
        if (!inicializado) return;

        textoEstado = "Escuchando: " + nombreCancion;

        DiscordRichPresence presence = {};
        presence.state = textoEstado.c_str();
        presence.details = "Launcher";
        presence.startTimestamp = horaInicio;
        presence.largeImageKey = "launcher_icon";
        presence.largeImageText = "Launcher - Daling";

        Discord_UpdatePresence(&presence);
    }

    void Apagar() {
        if (!inicializado) return;
        Discord_ClearPresence();
        Discord_Shutdown();
        inicializado = false;
    }

    void MostrarJugandoDetallado(const std::string& nombreJuego,
        const std::string& detalle,
        const std::string& estado)
    {
        if (!inicializado) return;

        // Guardamos los textos para que sigan vivos mientras Discord usa los punteros.
        textoEstado = estado.empty() ? ("Jugando: " + nombreJuego) : estado;
        textoDetalle = detalle.empty() ? nombreJuego : detalle;

        DiscordRichPresence presence = {};
        presence.state = textoEstado.c_str();
        presence.details = textoDetalle.c_str();
        presence.startTimestamp = horaInicio;
        presence.largeImageKey = "launcher_icon";
        presence.largeImageText = "Launcher - Daling";

        Discord_UpdatePresence(&presence);
    }

private:
    bool inicializado = false;
    int64_t horaInicio = 0;
    std::string textoEstado; // vive mientras dure el objeto, no se destruye al final de cada funcion
    std::string textoDetalle;
};