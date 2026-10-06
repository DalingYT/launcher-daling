// =========================================================
//  Launcher CPP - Daling
//  GUI: Dear ImGui + Win32 + DirectX 11
//  Incluye: sistema de plugins (.dll), reproductor de musica
//  con playlist, y Discord Rich Presence.
// =========================================================

#include "../include/resource.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_impl_win32.h"
#include "../third_party/imgui/imgui_impl_dx11.h"
#include "../include/GameAPI.h"
#include "../include/AudioPlayer.h"
#include "../third_party/Discord/DiscordPresence.h"
#include "../include/LauncherManager.h"
#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>   // ShellExecute, solo para abrir el link de YouTube

#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

// -------------------- Sistema de pantallas (state machine) --------------------
// MENU = estamos viendo la lista de juegos.
// JUGANDO = un juego (DLL) esta activo y dibujando su propia UI.
// REPRODUCTOR = pantalla dedicada del reproductor de musica.
enum class Pantalla {
    MENU,
    JUGANDO,
    REPRODUCTOR
};

// =========================================================
//  Carpetas que el launcher necesita para funcionar.
//  Si no existen, se crean automaticamente al arrancar.
//  Para agregar una carpeta nueva en el futuro, solo
//  añadela a esta lista — nada mas que cambiar.
// =========================================================
namespace CarpetasLauncher {
    const std::vector<std::string> REQUERIDAS = {
        "Games",
        "ErrorLog",
        "Fonts",
        "Theme",
        "Plugins",
        "Sounds",
        "DataFolder"
    };

    void VerificarYCrear() {
        for (const auto& carpeta : REQUERIDAS) {
            if (!fs::exists(carpeta)) {
                fs::create_directories(carpeta);
            }
        }
    }
}

static Pantalla g_pantallaActual = Pantalla::MENU;
static AudioPlayer g_audio; // el reproductor de musica, vive durante todo el programa
static DiscordPresence g_discord; // Discord Rich Presence, vive durante todo el programa

// Convierte segundos (float) a formato "m:ss" para mostrar en la UI.
std::string FormatearTiempo(float segundos) {
    if (segundos < 0) segundos = 0;
    int totalSeg = (int)segundos;
    int minutos = totalSeg / 60;
    int segs = totalSeg % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d:%02d", minutos, segs);
    return std::string(buffer);
}

// Creditos a Cacola por el album The Angel, the Demon confirmado 100% via Discord creeme por favor es 100% real no fake
static const char* NOMBRE_ARCHIVO_CON_CREDITO = "TheAngelTheDemon";
static const char* CREDITO_MUSICA =
"Musica: \"The Angel, the Demon\" by: Cacola (@noize_princess on X) "
"(https://open.spotify.com/intl-es/album/54vSNQ8AtD0CvERwtQ3bYv?si=NmzIKnuPRm-wNy7Gz7g4_w)";

// -------------------- Variables globales de DirectX --------------------
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// -------------------- Forward declarations --------------------
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// -------------------- Estructura de un juego cargado desde DLL --------------------
struct JuegoPlugin {
    std::string   nombreArchivo;   // ej: "AdivinaNumero.dll" (para logs/errores)
    std::string   nombreMostrado;  // el texto que devuelve Juego_Nombre()
    HMODULE       handleDll = nullptr;

    // Punteros a las 4 funciones OBLIGATORIAS que la DLL exporta (el "contrato")
    FnJuegoSetImGuiContext setContext = nullptr;
    FnJuegoNombre          nombre = nullptr;
    FnJuegoIniciar         iniciar = nullptr;
    FnJuegoDibujar         dibujar = nullptr;

    // Punteros a las funciones OPCIONALES (Discord Rich Presence).
    // Pueden quedar en nullptr si la DLL no las exporta; eso es normal.
    FnJuegoActividad     actividad = nullptr;
    FnJuegoMultijugador  multijugador = nullptr;
    FnJuegoProgreso      progreso = nullptr;
};

static std::vector<JuegoPlugin> g_juegos;
static int g_indiceJuegoActivo = -1; // que juego (dentro de g_juegos) esta corriendo ahora

// Crea un log de error con fecha/hora (igual que tu version en Python/bat)
void GuardarErrorLog(const std::string& carpetaErrorLog, const std::string& dll, const std::string& razon) {
    if (!fs::exists(carpetaErrorLog))
        fs::create_directories(carpetaErrorLog);

    std::time_t t = std::time(nullptr);
    std::tm tmResult;
    localtime_s(&tmResult, &t);

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tmResult);

    std::string logPath = carpetaErrorLog + "\\error_" + buffer + ".txt";
    std::ofstream log(logPath);
    if (log.is_open()) {
        log << "ERROR: No se pudo cargar la DLL del juego\n";
        log << "Archivo: " << dll << "\n";
        log << "Fecha y hora: " << buffer << "\n";
        log << "Razon: " << razon << "\n";
        log.close();
    }
}

// =====================================================================
//  TRANSPARENCIA: que hace y que NO hace esta funcion
// =====================================================================
std::vector<JuegoPlugin> EscanearYCargarJuegos(const std::string& carpetaGames, const std::string& carpetaErrorLog) {
    std::vector<JuegoPlugin> juegos;

    if (!fs::exists(carpetaGames)) {
        fs::create_directories(carpetaGames);
        return juegos;
    }

    for (const auto& entry : fs::directory_iterator(carpetaGames)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension().string() != ".dll") continue;

        std::string archivo = entry.path().filename().string();
        std::string rutaCompleta = entry.path().string();

        // Convertimos a wide string porque LoadLibraryW espera wchar_t*
        std::wstring rutaW(rutaCompleta.begin(), rutaCompleta.end());

        HMODULE handle = ::LoadLibraryW(rutaW.c_str());
        if (!handle) {
            GuardarErrorLog(carpetaErrorLog, archivo, "LoadLibrary fallo (la DLL puede estar corrupta o le faltan dependencias).");
            continue;
        }

        JuegoPlugin plugin;
        plugin.nombreArchivo = archivo;
        plugin.handleDll = handle;

        // Buscamos cada funcion OBLIGATORIA del contrato por su nombre exacto.
        plugin.setContext = (FnJuegoSetImGuiContext)::GetProcAddress(handle, NOMBRE_FN_JUEGO_SET_IMGUI_CONTEXT);
        plugin.nombre = (FnJuegoNombre)::GetProcAddress(handle, NOMBRE_FN_JUEGO_NOMBRE);
        plugin.iniciar = (FnJuegoIniciar)::GetProcAddress(handle, NOMBRE_FN_JUEGO_INICIAR);
        plugin.dibujar = (FnJuegoDibujar)::GetProcAddress(handle, NOMBRE_FN_JUEGO_DIBUJAR);

        // Si le falta CUALQUIERA de las 4 obligatorias, la DLL no cumple el contrato.
        if (!plugin.setContext || !plugin.nombre || !plugin.iniciar || !plugin.dibujar) {
            GuardarErrorLog(carpetaErrorLog, archivo,
                "La DLL no exporta las 4 funciones requeridas (Juego_SetImGuiContext, Juego_Nombre, Juego_Iniciar, Juego_Dibujar).");
            ::FreeLibrary(handle);
            continue;
        }

        // Buscamos las funciones OPCIONALES (Discord Rich Presence).
        plugin.actividad = (FnJuegoActividad)::GetProcAddress(handle, NOMBRE_FN_JUEGO_ACTIVIDAD);
        plugin.multijugador = (FnJuegoMultijugador)::GetProcAddress(handle, NOMBRE_FN_JUEGO_MULTIJUGADOR);
        plugin.progreso = (FnJuegoProgreso)::GetProcAddress(handle, NOMBRE_FN_JUEGO_PROGRESO);

        plugin.nombreMostrado = plugin.nombre(); // llamamos Juego_Nombre() una vez y guardamos el resultado
        juegos.push_back(plugin);
    }

    return juegos;
}

// -----------------------------------------------------------------
//  Arma el Rich Presence para el juego activo, usando las funciones
//  opcionales si la DLL las exporta (Actividad, Multijugador, Progreso).
// -----------------------------------------------------------------
void ActualizarPresenceDelJuego(const JuegoPlugin& juego) {
    std::string detalle = juego.nombreMostrado;
    if (juego.actividad) {
        const char* actividadTexto = juego.actividad();
        if (actividadTexto && actividadTexto[0] != '\0') {
            detalle = actividadTexto;
        }
    }

    std::string estadoExtra;
    if (juego.multijugador) {
        int num = 0, max = 0;
        if (juego.multijugador(&num, &max)) {
            estadoExtra = std::to_string(num) + "/" + std::to_string(max) + " jugadores";
        }
    }
    if (estadoExtra.empty() && juego.progreso) {
        int actual = 0, total = 0;
        if (juego.progreso(&actual, &total)) {
            estadoExtra = (total > 0)
                ? ("Progreso: " + std::to_string(actual) + "/" + std::to_string(total))
                : ("Progreso: " + std::to_string(actual));
        }
    }

    g_discord.MostrarJugandoDetallado(juego.nombreMostrado, detalle, estadoExtra);
}

// -------------------- main --------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

    // =========================================================
    // 1. VERIFICACIÓN Y CREACIÓN DE LAS CARPETAS NECESARIAS
    // =========================================================
    CarpetasLauncher::VerificarYCrear();

    // Rutas base (equivalente a CARPETA = os.getcwd() en Python)
    std::string carpetaBase = fs::current_path().string();
    std::string carpetaGames = carpetaBase + "\\Games";
    std::string carpetaErrorLog = carpetaBase + "\\ErrorLog";
    std::string carpetaSounds = carpetaBase + "\\Sounds";
    fs::create_directories(carpetaErrorLog);
    fs::create_directories(carpetaGames);
    fs::create_directories(carpetaSounds);

    std::vector<JuegoPlugin> juegosCargados = EscanearYCargarJuegos(carpetaGames, carpetaErrorLog);
    g_juegos = juegosCargados;

    // Carga el icono desde tus recursos
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1));

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        hIcon,          // <--- Asigna el icono aquí (Grande)
        nullptr,
        nullptr,
        nullptr,
        L"LauncherCPPClass",
        hIcon           // <--- Asigna el mismo icono aquí (Pequeño/Barra de tareas)
    };

    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"Launcher - Daling",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // tamaño fijo, como tu Python (resizable=False)
        100, 100,
        500, 500,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // -------------------- Reproductor de musica (playlist) --------------------
    g_audio.Iniciar();
    g_audio.CargarPlaylist(carpetaSounds);
    g_audio.SetLoopInfinitoPlaylist(true); // "escuchar infinitamente" activado por defecto
    if (g_audio.HayPlaylist()) {
        g_audio.ReproducirDesdeElInicio();
    }

    // -------------------- Discord Rich Presence --------------------
    g_discord.Iniciar(); // arranca mostrando "En el menu principal"

    // -------------------- Setup ImGui --------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no guardar layout en disco, no lo necesitamos

    ImGui::StyleColorsDark();

    // Paleta base (se sobreescribirá si theme.json existe)
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.118f, 0.118f, 0.184f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.157f, 0.173f, 0.204f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.27f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.32f, 0.38f, 1.0f);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // ==========================================================
    // INICIALIZACIÓN DE LAUNCHER MANAGER
    // ==========================================================
    ScanThemes();

    if (!g_AvailableThemes.empty()) {
        LoadTheme(g_AvailableThemes[0]);
    }

    ScanAndLoadFonts(16.0f);
    ScanPlugins();
    // ==========================================================

    // -------------------- Compartir el contexto de ImGui con cada DLL --------------------
    {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        ImGuiMemAllocFunc allocFn;
        ImGuiMemFreeFunc freeFn;
        void* userData;
        ImGui::GetAllocatorFunctions(&allocFn, &freeFn, &userData);

        for (auto& j : g_juegos) {
            j.setContext(ctx, allocFn, freeFn, userData);
        }
    }

    // Mensaje de error a mostrar en un popup, si algo falla al abrir un juego
    std::string ultimoError;
    bool mostrarPopupError = false;

    // Acumulador de tiempo para actualizar el Rich Presence del juego
    auto ultimaActualizacionPresence = std::chrono::steady_clock::now();

    // -------------------- Loop principal --------------------
    bool running = true;
    while (running) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        // Avanza la playlist automaticamente si la cancion actual termino.
        g_audio.Actualizar();

        // Procesa mensajes internos de Discord.
        g_discord.Actualizar();

        // Si estamos jugando, refrescamos el Rich Presence cada ~3 segundos
        if (g_pantallaActual == Pantalla::JUGANDO && g_indiceJuegoActivo >= 0) {
            auto ahora = std::chrono::steady_clock::now();
            double segundosTranscurridos = std::chrono::duration<double>(ahora - ultimaActualizacionPresence).count();
            if (segundosTranscurridos > 3.0) {
                ActualizarPresenceDelJuego(g_juegos[g_indiceJuegoActivo]);
                ultimaActualizacionPresence = ahora;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ===================================================================
        //  BARRA DE MENÚ SUPERIOR (Fuente | Tema | Plugins)
        // ===================================================================
        RenderMainMenuBar();

        // -------------------- Ventana principal (ocupa toda la app) --------------------
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        // Agregamos ImGuiWindowFlags_MenuBar para que la ventana se adapte correctamente
        ImGui::Begin("LauncherMain", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_MenuBar);

        // ===================================================================
        //  PANTALLA: MENU PRINCIPAL
        // ===================================================================
        if (g_pantallaActual == Pantalla::MENU) {

            ImGui::TextColored(ImVec4(1, 1, 1, 1), "Launcher");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("ScrollGames", ImVec2(0, -60), true);

            if (g_juegos.empty()) {
                ImGui::TextWrapped("No hay juegos (.dll) en la carpeta Games.\nCompila uno con la plantilla y ponlo ahi.");
            }

            for (size_t i = 0; i < g_juegos.size(); ++i) {
                if (ImGui::Button(g_juegos[i].nombreMostrado.c_str(), ImVec2(-1, 40))) {
                    g_indiceJuegoActivo = (int)i;
                    g_juegos[i].iniciar();           // Juego_Iniciar() - una sola vez al entrar
                    g_pantallaActual = Pantalla::JUGANDO;

                    // Actualizamos el Rich Presence de inmediato al entrar al juego.
                    ActualizarPresenceDelJuego(g_juegos[i]);
                    ultimaActualizacionPresence = std::chrono::steady_clock::now();
                }
                ImGui::Spacing();
            }

            ImGui::EndChild();

            ImGui::Spacing();

            // ---- Botones lado a lado: Reproductor + YouTube ----
            float anchoMitad = (ImGui::GetContentRegionAvail().x - 8) / 2.0f;

            if (g_audio.HayPlaylist()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.52f, 0.92f, 1.0f));
                if (ImGui::Button("Reproductor", ImVec2(anchoMitad, 40))) {
                    g_pantallaActual = Pantalla::REPRODUCTOR;
                    g_discord.MostrarEscuchandoMusica(g_audio.NombreCancionActual());
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("YouTube", ImVec2(g_audio.HayPlaylist() ? anchoMitad : -1, 40))) {
                ShellExecuteA(nullptr, "open", "https://www.youtube.com/@DalingYT", nullptr, nullptr, SW_SHOWNORMAL);
            }
            ImGui::PopStyleColor(2);
        }

        // ===================================================================
        //  PANTALLA: JUGANDO (el juego activo dibuja su propia UI aqui)
        // ===================================================================
        else if (g_pantallaActual == Pantalla::JUGANDO && g_indiceJuegoActivo >= 0) {
            bool pidioVolver = g_juegos[g_indiceJuegoActivo].dibujar(); // Juego_Dibujar()
            if (pidioVolver) {
                g_pantallaActual = Pantalla::MENU;
                g_indiceJuegoActivo = -1;
                g_discord.MostrarEnMenu();
            }
        }

        // ===================================================================
        //  PANTALLA: REPRODUCTOR DE MUSICA
        // ===================================================================
        else if (g_pantallaActual == Pantalla::REPRODUCTOR) {

            ImGui::TextColored(ImVec4(1, 1, 1, 1), "Reproductor de Musica");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            // ---- Nombre de la cancion actual ----
            std::string nombre = g_audio.NombreCancionActual();
            ImGui::TextWrapped("%s", nombre.empty() ? "(sin cancion)" : nombre.c_str());
            ImGui::Spacing();

            // ---- Barra de progreso + tiempo actual / duracion ----
            float actual = g_audio.SegundosTranscurridos();
            float duracion = g_audio.DuracionEnSegundos();
            float progreso = (duracion > 0.0f) ? (actual / duracion) : 0.0f;

            ImGui::ProgressBar(progreso, ImVec2(-1, 18), "");
            ImGui::Text("%s / %s", FormatearTiempo(actual).c_str(), FormatearTiempo(duracion).c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            // ---- Botones: Anterior / Pausa-Reanudar / Siguiente ----
            float anchoBoton = (ImGui::GetContentRegionAvail().x - 16) / 3.0f;

            if (ImGui::Button("<< Anterior", ImVec2(anchoBoton, 40))) {
                g_audio.Anterior();
                g_discord.MostrarEscuchandoMusica(g_audio.NombreCancionActual());
            }
            ImGui::SameLine();

            bool sonando = g_audio.EstaSonando();
            if (ImGui::Button(sonando ? "Pausar" : "Reanudar", ImVec2(anchoBoton, 40))) {
                if (sonando) g_audio.Pausar();
                else g_audio.Reanudar();
            }
            ImGui::SameLine();

            if (ImGui::Button("Siguiente >>", ImVec2(anchoBoton, 40))) {
                g_audio.Siguiente();
                g_discord.MostrarEscuchandoMusica(g_audio.NombreCancionActual());
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ---- Switches de modo: loop de playlist, repetir 1 cancion, aleatorio ----
            bool loopPlaylist = g_audio.GetLoopInfinitoPlaylist();
            if (ImGui::Checkbox("Escuchar infinitamente (repetir playlist)", &loopPlaylist)) {
                g_audio.SetLoopInfinitoPlaylist(loopPlaylist);
            }

            bool repetirUna = g_audio.GetSoloUnaCancionEnLoop();
            if (ImGui::Checkbox("Repetir solo esta cancion", &repetirUna)) {
                g_audio.SetSoloUnaCancionEnLoop(repetirUna);
            }

            bool aleatorio = g_audio.GetModoAleatorio();
            if (ImGui::Checkbox("Orden aleatorio (shuffle)", &aleatorio)) {
                g_audio.SetModoAleatorio(aleatorio);
            }

            ImGui::Spacing();
            ImGui::Text("Canciones en la playlist: %d", g_audio.CantidadCanciones());

            ImGui::Spacing();
            ImGui::Spacing();

            // ---- Credito de la musica (solo visible si esta sonando ESA cancion) ----
            if (g_audio.NombreCancionActual() == NOMBRE_ARCHIVO_CON_CREDITO) {
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextWrapped("%s", CREDITO_MUSICA);
                ImGui::Spacing();
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Volver al menu", ImVec2(150, 35))) {
                g_pantallaActual = Pantalla::MENU;
                g_discord.MostrarEnMenu();
            }
        }

        ImGui::End();

        // -------------------- Popup de error --------------------
        if (mostrarPopupError) {
            ImGui::OpenPopup("Error");
            mostrarPopupError = false;
        }
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", ultimoError.c_str());
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // -------------------- Render --------------------
        ImGui::Render();
        const float clear_color[4] = { 0.118f, 0.118f, 0.184f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // VSync on
    }

    // -------------------- Cleanup --------------------
    g_discord.Apagar();
    g_audio.Liberar();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for (auto& j : g_juegos) {
        if (j.handleDll) ::FreeLibrary(j.handleDll);
    }

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// =========================================================
//  Funciones de soporte DirectX 11 (boilerplate estandar de ImGui)
// =========================================================

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // bloquea menu de Alt
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}