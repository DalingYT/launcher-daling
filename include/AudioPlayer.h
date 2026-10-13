// =========================================================
//  AudioPlayer.h
//  Reproductor de musica con playlist, usando miniaudio.
//
//  Soporta: MP3, WAV, FLAC (miniaudio) y OGG (con stb_vorbis.c)
//  Funciones: reproducir, pausar, siguiente, anterior, loop infinito,
//             orden aleatorio (shuffle), posicion/duracion en segundos.
// =========================================================
#pragma once

#pragma once

#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "../third_party/stb/stb_vorbis.c"
#undef L
#undef C
#undef R

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/MiniAudio/miniaudio.h"

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <random>
#include <ctime>
#include <cctype>
#include <chrono>

namespace fs = std::filesystem;

class AudioPlayer {
public:
    static bool EsFormatoSoportado(const std::string& extension) {
        return extension == ".mp3" ||
               extension == ".wav" ||
               extension == ".ogg" ||
               extension == ".flac";
    }

    bool Iniciar() {
        if (motorListo) return true;

        ma_result resultado = ma_engine_init(nullptr, &motor);
        if (resultado != MA_SUCCESS) return false;

        motorListo = true;
        return true;
    }

    void CargarPlaylist(const std::string& carpetaSounds) {
        playlist.clear();
        ordenAleatorio.clear();
        indiceActual = -1;
        posicionEnOrdenAleatorio = 0;

        if (!fs::exists(carpetaSounds)) {
            fs::create_directories(carpetaSounds);
            return;
        }

        for (const auto& entry : fs::directory_iterator(carpetaSounds)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();

            std::transform(
                ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                }
            );

            if (EsFormatoSoportado(ext)) {
                CancionInfo info;
                info.ruta = entry.path().string();
                info.nombreMostrado = entry.path().stem().string();
                playlist.push_back(info);
            }
        }

        ordenAleatorio.resize(playlist.size());

        for (size_t i = 0; i < playlist.size(); ++i) {
            ordenAleatorio[i] = static_cast<int>(i);
        }
    }

    bool ReproducirIndice(int indice) {
        if (!motorListo || playlist.empty()) return false;
        if (indice < 0 || indice >= static_cast<int>(playlist.size()))
            return false;

        DetenerYLiberarSonidoActual();

        ma_result resultado = ma_sound_init_from_file(
            &motor,
            playlist[indice].ruta.c_str(),
            MA_SOUND_FLAG_STREAM,
            nullptr,
            nullptr,
            &sonidoActual
        );

        if (resultado != MA_SUCCESS) return false;

        sonidoCargado = true;
        indiceActual = indice;
        fadeSalidaIniciado = false;
        cambioPendiente = false;
        duracionRespaldo = 0.0f;

        ma_sound_set_looping(
            &sonidoActual,
            soloUnaCancionEnLoop ? MA_TRUE : MA_FALSE
        );

        // Fade-in: silencio -> volumen normal.
        ma_sound_set_fade_in_milliseconds(
            &sonidoActual,
            0.0f,
            1.0f,
            static_cast<ma_uint64>(duracionFade * 1000.0f)
        );

        ma_sound_start(&sonidoActual);

        // Respaldo para archivos OGG sin duración reportada.
        float duracion = 0.0f;

        if (ma_sound_get_length_in_seconds(
                &sonidoActual, &duracion) != MA_SUCCESS ||
            duracion <= 0.0f) {

            int error = 0;

            stb_vorbis* vorbis = stb_vorbis_open_filename(
                playlist[indice].ruta.c_str(),
                &error,
                nullptr
            );

            if (vorbis) {
                stb_vorbis_info info = stb_vorbis_get_info(vorbis);
                unsigned int muestras =
                    stb_vorbis_stream_length_in_samples(vorbis);

                if (info.sample_rate > 0) {
                    duracionRespaldo =
                        static_cast<float>(muestras) /
                        static_cast<float>(info.sample_rate);
                }

                stb_vorbis_close(vorbis);
            }
        }

        return true;
    }

    void ReproducirDesdeElInicio() {
        if (playlist.empty()) return;

        if (modoAleatorio) {
            BarajearOrden();
            posicionEnOrdenAleatorio = 0;
            ReproducirIndice(ordenAleatorio[0]);
        }
        else {
            ReproducirIndice(0);
        }
    }

    // Llamar una vez por frame.
    void Actualizar() {
        if (!sonidoCargado || playlist.empty()) return;

        // Esperar a que termine el fade-out de un cambio manual
        // o automático antes de sustituir el sonido.
        if (cambioPendiente) {
            auto ahora = std::chrono::steady_clock::now();

            float transcurrido = std::chrono::duration<float>(
                ahora - inicioCambio
            ).count();

            if (transcurrido >= duracionFade) {
                int destino = indicePendiente;
                cambioPendiente = false;
                ReproducirIndice(destino);
            }

            return;
        }

        if (soloUnaCancionEnLoop) return;

        float duracion = DuracionEnSegundos();
        float posicion = SegundosTranscurridos();

        // Iniciar fade-out antes de que termine la canción.
        if (!fadeSalidaIniciado &&
            duracion > 0.0f &&
            posicion >= duracion - duracionFade) {

            int siguiente = ObtenerSiguienteIndice();

            if (siguiente >= 0) {
                ProgramarCambio(siguiente);
                return;
            }

            // No hay siguiente canción: terminar suavemente.
            fadeSalidaIniciado = true;

            ma_sound_set_fade_in_milliseconds(
                &sonidoActual,
                1.0f,
                0.0f,
                static_cast<ma_uint64>(duracionFade * 1000.0f)
            );
        }

        if (ma_sound_at_end(&sonidoActual)) {
            Siguiente();
        }
    }

    void Siguiente() {
        if (playlist.empty()) return;

        int siguiente = ObtenerSiguienteIndice();

        if (siguiente >= 0) {
            ProgramarCambio(siguiente);
        }
    }

    void Anterior() {
        if (playlist.empty()) return;

        int anterior = ObtenerAnteriorIndice();

        if (anterior >= 0) {
            ProgramarCambio(anterior);
        }
    }

    void Pausar() {
        if (sonidoCargado) {
            ma_sound_stop(&sonidoActual);
        }
    }

    void Reanudar() {
        if (sonidoCargado) {
            ma_sound_start(&sonidoActual);
        }
    }

    bool EstaSonando() const {
        return sonidoCargado &&
               ma_sound_is_playing(&sonidoActual);
    }

    void IrASegundo(float segundos) {
        if (sonidoCargado) {
            ma_sound_seek_to_second(&sonidoActual, segundos);
            fadeSalidaIniciado = false;
            cambioPendiente = false;
        }
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

        return duracion > 0.0f ? duracion : duracionRespaldo;
    }

    std::string NombreCancionActual() const {
        if (indiceActual < 0 ||
            indiceActual >= static_cast<int>(playlist.size())) {
            return "";
        }

        return playlist[indiceActual].nombreMostrado;
    }

    int CantidadCanciones() const {
        return static_cast<int>(playlist.size());
    }

    bool HayPlaylist() const {
        return !playlist.empty();
    }

    // Configuración de modos.

    void SetLoopInfinitoPlaylist(bool activo) {
        loopInfinitoPlaylist = activo;
    }

    bool GetLoopInfinitoPlaylist() const {
        return loopInfinitoPlaylist;
    }

    void SetSoloUnaCancionEnLoop(bool activo) {
        soloUnaCancionEnLoop = activo;

        if (sonidoCargado) {
            ma_sound_set_looping(
                &sonidoActual,
                activo ? MA_TRUE : MA_FALSE
            );
        }
    }

    bool GetSoloUnaCancionEnLoop() const {
        return soloUnaCancionEnLoop;
    }

    void SetModoAleatorio(bool activo) {
        modoAleatorio = activo;

        if (activo) BarajearOrden();
    }

    bool GetModoAleatorio() const {
        return modoAleatorio;
    }

    // Duración configurable de los fades, en segundos.
    void SetDuracionFade(float segundos) {
        if (segundos < 0.0f) segundos = 0.0f;
        duracionFade = segundos;
    }

    float GetDuracionFade() const {
        return duracionFade;
    }

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

    bool loopInfinitoPlaylist = true;
    bool soloUnaCancionEnLoop = false;
    bool modoAleatorio = false;

    std::vector<int> ordenAleatorio;
    int posicionEnOrdenAleatorio = 0;

    float duracionRespaldo = 0.0f;

    // Transiciones.
    float duracionFade = 1.0f;
    bool fadeSalidaIniciado = false;
    bool cambioPendiente = false;
    int indicePendiente = -1;

    std::chrono::steady_clock::time_point inicioCambio;

    void ProgramarCambio(int destino) {
        if (!sonidoCargado || destino < 0 ||
            destino >= static_cast<int>(playlist.size())) {
            return;
        }

        if (duracionFade <= 0.0f) {
            ReproducirIndice(destino);
            return;
        }

        indicePendiente = destino;
        cambioPendiente = true;
        fadeSalidaIniciado = true;
        inicioCambio = std::chrono::steady_clock::now();

        ma_sound_set_fade_in_milliseconds(
            &sonidoActual,
            1.0f,
            0.0f,
            static_cast<ma_uint64>(duracionFade * 1000.0f)
        );
    }

    int ObtenerSiguienteIndice() {
        if (playlist.empty()) return -1;

        if (modoAleatorio) {
            int siguientePosicion = posicionEnOrdenAleatorio + 1;

            if (siguientePosicion >=
                static_cast<int>(ordenAleatorio.size())) {

                if (!loopInfinitoPlaylist) return -1;

                BarajearOrden();
                siguientePosicion = 0;
            }

            posicionEnOrdenAleatorio = siguientePosicion;
            return ordenAleatorio[posicionEnOrdenAleatorio];
        }

        int siguiente = indiceActual + 1;

        if (siguiente >= static_cast<int>(playlist.size())) {
            if (!loopInfinitoPlaylist) return -1;
            siguiente = 0;
        }

        return siguiente;
    }

    int ObtenerAnteriorIndice() {
        if (playlist.empty()) return -1;

        if (modoAleatorio) {
            int anteriorPosicion = posicionEnOrdenAleatorio - 1;

            if (anteriorPosicion < 0) {
                if (!loopInfinitoPlaylist) return -1;

                anteriorPosicion =
                    static_cast<int>(ordenAleatorio.size()) - 1;
            }

            posicionEnOrdenAleatorio = anteriorPosicion;
            return ordenAleatorio[posicionEnOrdenAleatorio];
        }

        int anterior = indiceActual - 1;

        if (anterior < 0) {
            if (!loopInfinitoPlaylist) anterior = 0;
            else anterior = static_cast<int>(playlist.size()) - 1;
        }

        return anterior;
    }

    void DetenerYLiberarSonidoActual() {
        if (sonidoCargado) {
            ma_sound_stop(&sonidoActual);
            ma_sound_uninit(&sonidoActual);
            sonidoCargado = false;
        }

        cambioPendiente = false;
        indicePendiente = -1;
        fadeSalidaIniciado = false;
    }

    void BarajearOrden() {
        ordenAleatorio.resize(playlist.size());

        for (size_t i = 0; i < playlist.size(); ++i) {
            ordenAleatorio[i] = static_cast<int>(i);
        }

        std::mt19937 rng(
            static_cast<unsigned>(std::time(nullptr))
        );

        std::shuffle(
            ordenAleatorio.begin(),
            ordenAleatorio.end(),
            rng
        );
    }
};