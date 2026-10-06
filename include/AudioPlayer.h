// =========================================================
//  AudioPlayer.h
//  Reproductor de musica con playlist, usando miniaudio.
//
//  Soporta: MP3, WAV, OGG, FLAC (lo que miniaudio soporta de fabrica)
//  Funciones: reproducir, pausar, siguiente, anterior, loop infinito,
//             orden aleatorio (shuffle), posicion/duracion en segundos.
// =========================================================
#pragma once

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/MiniAudio/miniaudio.h"

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <random>
#include <ctime>

namespace fs = std::filesystem;

class AudioPlayer {
public:
    // Extensiones que vamos a aceptar como "musica" al escanear Sounds/.
    // miniaudio soporta mas formatos, pero limitamos a estos por pedido.
    static bool EsFormatoSoportado(const std::string& extension) {
        return extension == ".mp3" || extension == ".wav" ||
            extension == ".ogg" || extension == ".flac";
    }

    // Inicializa el motor de audio. Se llama UNA VEZ, al abrir el launcher.
    bool Iniciar() {
        ma_result resultado = ma_engine_init(nullptr, &motor);
        if (resultado != MA_SUCCESS) return false;
        motorListo = true;
        return true;
    }

    // Escanea la carpeta Sounds/ buscando archivos de audio soportados,
    // y construye la playlist. NO empieza a reproducir todavia.
    void CargarPlaylist(const std::string& carpetaSounds) {
        playlist.clear();

        if (!fs::exists(carpetaSounds)) {
            fs::create_directories(carpetaSounds);
            return;
        }

        for (const auto& entry : fs::directory_iterator(carpetaSounds)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            // Pasamos la extension a minusculas para comparar sin distinguir mayusculas
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return std::tolower(c); });

            if (EsFormatoSoportado(ext)) {
                CancionInfo info;
                info.ruta = entry.path().string();
                info.nombreMostrado = entry.path().stem().string(); // nombre sin extension
                playlist.push_back(info);
            }
        }

        // Armamos tambien un orden "aleatorio" base, para el modo shuffle.
        ordenAleatorio.resize(playlist.size());
        for (size_t i = 0; i < playlist.size(); ++i) ordenAleatorio[i] = (int)i;
    }

    // Empieza a reproducir la cancion en el indice dado de la playlist (orden normal).
    bool ReproducirIndice(int indice) {
        if (!motorListo || playlist.empty()) return false;
        if (indice < 0 || indice >= (int)playlist.size()) return false;

        DetenerYLiberarSonidoActual();

        ma_result resultado = ma_sound_init_from_file(
            &motor, playlist[indice].ruta.c_str(),
            MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sonidoActual
        );
        if (resultado != MA_SUCCESS) return false;

        sonidoCargado = true;
        indiceActual = indice;
        ma_sound_set_looping(&sonidoActual, soloUnaCancionEnLoop ? MA_TRUE : MA_FALSE);
        ma_sound_start(&sonidoActual);
        return true;
    }

    // Empieza la playlist desde el principio (o desde un orden aleatorio si shuffle esta activo).
    void ReproducirDesdeElInicio() {
        if (playlist.empty()) return;

        if (modoAleatorio) {
            BarajearOrden();
            ReproducirIndice(ordenAleatorio[0]);
            posicionEnOrdenAleatorio = 0;
        }
        else {
            ReproducirIndice(0);
        }
    }

    // -----------------------------------------------------------------
    //  Llamar UNA VEZ POR FRAME. Revisa si la cancion actual termino,
    //  y si "escuchar infinitamente" (modo playlist en loop) esta activo,
    //  pasa automaticamente a la siguiente.
    // -----------------------------------------------------------------
    void Actualizar() {
        if (!sonidoCargado || playlist.empty()) return;

        bool yaTermino = ma_sound_at_end(&sonidoActual);
        if (yaTermino && !soloUnaCancionEnLoop) {
            Siguiente(); // avanza solita a la siguiente cancion
        }
    }

    // Pasa a la siguiente cancion. Si "loopInfinito" esta activo y llegamos
    // al final de la playlist, regresamos a la cancion 0 (loop de toda la lista).
    void Siguiente() {
        if (playlist.empty()) return;

        if (modoAleatorio) {
            posicionEnOrdenAleatorio++;
            if (posicionEnOrdenAleatorio >= (int)ordenAleatorio.size()) {
                if (!loopInfinitoPlaylist) return; // se acabo la playlist y no hay loop
                BarajearOrden();
                posicionEnOrdenAleatorio = 0;
            }
            ReproducirIndice(ordenAleatorio[posicionEnOrdenAleatorio]);
        }
        else {
            int siguiente = indiceActual + 1;
            if (siguiente >= (int)playlist.size()) {
                if (!loopInfinitoPlaylist) return;
                siguiente = 0;
            }
            ReproducirIndice(siguiente);
        }
    }

    // Regresa a la cancion anterior. Si estamos al inicio y hay loop, va a la ultima.
    void Anterior() {
        if (playlist.empty()) return;

        if (modoAleatorio) {
            posicionEnOrdenAleatorio--;
            if (posicionEnOrdenAleatorio < 0) {
                if (!loopInfinitoPlaylist) { posicionEnOrdenAleatorio = 0; return; }
                posicionEnOrdenAleatorio = (int)ordenAleatorio.size() - 1;
            }
            ReproducirIndice(ordenAleatorio[posicionEnOrdenAleatorio]);
        }
        else {
            int anterior = indiceActual - 1;
            if (anterior < 0) {
                if (!loopInfinitoPlaylist) { anterior = 0; }
                else { anterior = (int)playlist.size() - 1; }
            }
            ReproducirIndice(anterior);
        }
    }

    void Pausar() {
        if (sonidoCargado) ma_sound_stop(&sonidoActual);
    }

    void Reanudar() {
        if (sonidoCargado) ma_sound_start(&sonidoActual);
    }

    bool EstaSonando() const {
        return sonidoCargado && ma_sound_is_playing(&sonidoActual);
    }

    // Mueve la posicion de reproduccion a un segundo especifico (para una barra de progreso).
    void IrASegundo(float segundos) {
        if (sonidoCargado) ma_sound_seek_to_second(&sonidoActual, segundos);
    }

    float SegundosTranscurridos() const {
        if (!sonidoCargado) return 0.0f;
        float cursor = 0.0f;
        ma_sound_get_cursor_in_seconds(&sonidoActual, &cursor);
        return cursor;
    }

    float DuracionEnSegundos() const {
        if (!sonidoCargado) return 0.0f;
        float duracion = 0.0f;
        ma_sound_get_length_in_seconds(&sonidoActual, &duracion);
        return duracion;
    }

    std::string NombreCancionActual() const {
        if (indiceActual < 0 || indiceActual >= (int)playlist.size()) return "";
        return playlist[indiceActual].nombreMostrado;
    }

    int CantidadCanciones() const { return (int)playlist.size(); }
    bool HayPlaylist() const { return !playlist.empty(); }

    // -------- Configuracion de modos --------

    // "Escuchar infinitamente": cuando se acaba la playlist completa, vuelve a empezar.
    void SetLoopInfinitoPlaylist(bool activo) { loopInfinitoPlaylist = activo; }
    bool GetLoopInfinitoPlaylist() const { return loopInfinitoPlaylist; }

    // Repetir SOLO la cancion actual en loop (no avanza a la siguiente nunca).
    void SetSoloUnaCancionEnLoop(bool activo) {
        soloUnaCancionEnLoop = activo;
        if (sonidoCargado) ma_sound_set_looping(&sonidoActual, activo ? MA_TRUE : MA_FALSE);
    }
    bool GetSoloUnaCancionEnLoop() const { return soloUnaCancionEnLoop; }

    // Orden aleatorio (shuffle) en vez de seguir el orden en que se encontraron los archivos.
    void SetModoAleatorio(bool activo) {
        modoAleatorio = activo;
        if (activo) BarajearOrden();
    }
    bool GetModoAleatorio() const { return modoAleatorio; }

    void Liberar() {
        DetenerYLiberarSonidoActual();
        if (motorListo) {
            ma_engine_uninit(&motor);
            motorListo = false;
        }
    }

private:
    struct CancionInfo {
        std::string ruta;
        std::string nombreMostrado;
    };

    ma_engine motor{};
    bool motorListo = false;

    ma_sound sonidoActual{};
    bool sonidoCargado = false;

    std::vector<CancionInfo> playlist;
    int indiceActual = -1;

    bool loopInfinitoPlaylist = true;   // "escuchar infinitamente" = activo por defecto
    bool soloUnaCancionEnLoop = false;
    bool modoAleatorio = false;

    std::vector<int> ordenAleatorio;
    int posicionEnOrdenAleatorio = 0;

    void DetenerYLiberarSonidoActual() {
        if (sonidoCargado) {
            ma_sound_stop(&sonidoActual);
            ma_sound_uninit(&sonidoActual);
            sonidoCargado = false;
        }
    }

    void BarajearOrden() {
        ordenAleatorio.resize(playlist.size());
        for (size_t i = 0; i < playlist.size(); ++i) ordenAleatorio[i] = (int)i;

        std::mt19937 rng((unsigned)std::time(nullptr));
        std::shuffle(ordenAleatorio.begin(), ordenAleatorio.end(), rng);
    }
};