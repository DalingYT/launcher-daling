#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <windows.h> // Necesario para la API de Windows (LoadLibrary / GetProcAddress)
#include "../third_party/imgui/imgui.h"
#include "../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Firma de la función que deben tener las DLLs para inicializarse
typedef void (*PluginInitFunc)();

// --- ESTRUCTURAS DE DATOS ---
struct ThemeDetails {
    std::string name = "Por defecto";
    std::string author = "Desconocido";
    std::string version = "1.0";
    std::string description = "";
};

struct LoadedFont {
    std::string name;
    ImFont* fontPtr;
};

struct LoadedPluginInfo {
    std::string fileName;
    HMODULE handle;
};

// --- VARIABLES GLOBALES DE CONTROL ---
static ThemeDetails g_CurrentTheme;
static std::vector<LoadedFont> g_AvailableFonts;
static ImFont* g_SelectedFont = nullptr;
static std::vector<std::string> g_DetectedPlugins;
static std::vector<LoadedPluginInfo> g_ActivePlugins;

std::vector<std::string> g_AvailableThemes;
static std::string g_SelectedTheme;

// --- ESCANEAR SUBCARPETAS DE TEMAS ---
void ScanThemes() {
    g_AvailableThemes.clear();

    if (!fs::exists("Theme") || !fs::is_directory("Theme"))
        return;

    for (const auto& entry : fs::directory_iterator("Theme")) {
        if (entry.is_directory() &&
            fs::exists(entry.path() / "theme.json")) {

            g_AvailableThemes.push_back(
                entry.path().filename().string()
            );
        }
    }
}

// --- HELPER PARA CONVERTIR HEX A IMVEC4 ---
ImVec4 HexToImVec4(const std::string& hexStr) {
    if (hexStr.empty() || hexStr[0] != '#')
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    unsigned int r = 255, g = 255, b = 255, a = 255;

    if (hexStr.length() == 7) {
        sscanf_s(hexStr.c_str(), "#%02x%02x%02x", &r, &g, &b);
    }
    else if (hexStr.length() == 9) {
        sscanf_s(hexStr.c_str(), "#%02x%02x%02x%02x", &r, &g, &b, &a);
    }

    return ImVec4(
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        a / 255.0f
    );
}

// ==========================================
// 1. CARGADOR DE TEMAS
// ==========================================
void LoadTheme(const std::string& themeFolder = "") {

    // Crear Daling Dark si no existe
    fs::path defaultTheme = fs::path("Theme") / "DalingDark";

    if (!fs::exists(defaultTheme)) {
        fs::create_directories(defaultTheme);

        std::ofstream detail(defaultTheme / "detail.json");
        detail << R"({
"name": "Daling Dark",
"author": "Daling",
"version": "1.0",
"description": "El tema oscuro oficial de Daling."
})";

        std::ofstream theme(defaultTheme / "theme.json");
        theme << R"({
"background": "#18181B",
"button": "#27272A",
"button_hover": "#3F3F46",
"button_active": "#52525B",
"text": "#FFFFFF",
"bars": "#6366F1"
})";
    }

    std::string folder = themeFolder;

    if (folder.empty()) {
        folder = g_SelectedTheme;
    }

    if (folder.empty()) {
        ScanThemes();

        if (g_AvailableThemes.empty())
            return;

        folder = g_AvailableThemes[0];
    }

    fs::path themeDir = fs::path("Theme") / folder;

    // Leer detail.json
    std::string detailPath = (themeDir / "detail.json").string();

    if (fs::exists(detailPath)) {
        try {
            std::ifstream file(detailPath);
            json j;
            file >> j;

            if (j.contains("name"))
                g_CurrentTheme.name = j["name"];

            if (j.contains("author"))
                g_CurrentTheme.author = j["author"];

            if (j.contains("version"))
                g_CurrentTheme.version = j["version"];

            if (j.contains("description"))
                g_CurrentTheme.description = j["description"];

        } catch (...) {}
    }

    // Leer theme.json
    std::string themePath = (themeDir / "theme.json").string();

    if (fs::exists(themePath)) {
        try {
            std::ifstream file(themePath);
            json j;
            file >> j;

            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            if (j.contains("background")) {
                ImVec4 bg = HexToImVec4(j["background"]);

                colors[ImGuiCol_WindowBg] = bg;
                colors[ImGuiCol_ChildBg] = bg;
                colors[ImGuiCol_PopupBg] = bg;
            }

            if (j.contains("button"))
                colors[ImGuiCol_Button] =
                    HexToImVec4(j["button"]);

            if (j.contains("button_hover"))
                colors[ImGuiCol_ButtonHovered] =
                    HexToImVec4(j["button_hover"]);

            if (j.contains("button_active"))
                colors[ImGuiCol_ButtonActive] =
                    HexToImVec4(j["button_active"]);

            if (j.contains("text"))
                colors[ImGuiCol_Text] =
                    HexToImVec4(j["text"]);

            if (j.contains("bars")) {
                ImVec4 barColor =
                    HexToImVec4(j["bars"]);

                colors[ImGuiCol_ScrollbarGrab] =
                    barColor;

                colors[ImGuiCol_ScrollbarGrabHovered] =
                    ImVec4(
                        barColor.x * 1.1f,
                        barColor.y * 1.1f,
                        barColor.z * 1.1f,
                        barColor.w
                    );

                colors[ImGuiCol_PlotHistogram] =
                    barColor;
            }

        } catch (...) {}
    }

    g_SelectedTheme = folder;
}

// ==========================================
// 2. CARGADOR DE FUENTES
// ==========================================
void ScanAndLoadFonts(float fontSize = 16.0f) {
    ImGuiIO& io = ImGui::GetIO();
    g_AvailableFonts.clear();

    std::string fontsDir = "Fonts";

    if (fs::exists(fontsDir) && fs::is_directory(fontsDir)) {
        for (const auto& entry : fs::directory_iterator(fontsDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();

                if (ext == ".ttf" || ext == ".otf") {
                    std::string fontPath =
                        entry.path().string();

                    std::string fontName =
                        entry.path().filename().string();

                    ImFont* font =
                        io.Fonts->AddFontFromFileTTF(
                            fontPath.c_str(),
                            fontSize
                        );

                    if (font) {
                        g_AvailableFonts.push_back({
                            fontName,
                            font
                        });
                    }
                }
            }
        }
    }

    if (!g_AvailableFonts.empty()) {
        g_SelectedFont =
            g_AvailableFonts[0].fontPtr;
    }
}

// ==========================================
// 3. ESCÁNER Y CARGADOR REAL DE PLUGINS (.DLL)
// ==========================================
void ScanPlugins() {
    g_DetectedPlugins.clear();

    std::string pluginsDir = "Plugins";

    if (fs::exists(pluginsDir) &&
        fs::is_directory(pluginsDir)) {

        for (const auto& entry :
             fs::directory_iterator(pluginsDir)) {

            if (entry.is_regular_file() &&
                entry.path().extension() == ".dll") {

                g_DetectedPlugins.push_back(
                    entry.path().filename().string()
                );
            }
        }
    }
}

// Función que ejecuta la carga real de la DLL en memoria
void ExecuteLoadPlugin(const std::string& pluginName) {

    // 1. Verificar si ya está cargado
    for (const auto& active : g_ActivePlugins) {
        if (active.fileName == pluginName) {
            std::cout << "[PLUGIN] "
                      << pluginName
                      << " ya esta activo."
                      << std::endl;
            return;
        }
    }

    // 2. Cargar el módulo DLL en el espacio de memoria
    std::string fullPath =
        "Plugins/" + pluginName;

    HMODULE hModule =
        LoadLibraryA(fullPath.c_str());

    if (!hModule) {
        std::cout << "[ERROR] No se pudo cargar la DLL: "
                  << pluginName
                  << " (Error Code: "
                  << GetLastError()
                  << ")"
                  << std::endl;
        return;
    }

    // 3. Buscar la función 'PluginInit' dentro de la DLL
    PluginInitFunc initFunc =
        (PluginInitFunc)GetProcAddress(
            hModule,
            "PluginInit"
        );

    if (initFunc) {
        // Ejecutar el punto de entrada del plugin
        initFunc();

        g_ActivePlugins.push_back({
            pluginName,
            hModule
        });

        std::cout << "[SUCCESS] Plugin "
                  << pluginName
                  << " cargado e inicializado correctamente."
                  << std::endl;

    } else {
        std::cout << "[WARNING] Se cargo la DLL "
                  << pluginName
                  << " pero no se encontro "
                  << "la funcion 'PluginInit'."
                  << std::endl;

        FreeLibrary(hModule);
    }
}

// ==========================================
// 4. BARRA DE MENÚ SUPERIOR
// ==========================================
void RenderMainMenuBar() {

    if (ImGui::BeginMainMenuBar()) {

        // --- MENÚ FUENTE ---
        if (ImGui::BeginMenu("Fuente")) {

            if (g_AvailableFonts.empty()) {
                ImGui::TextDisabled(
                    "No hay fuentes en /Fonts"
                );

            } else {

                for (const auto& fontItem :
                     g_AvailableFonts) {

                    bool isSelected =
                        (g_SelectedFont ==
                         fontItem.fontPtr);

                    if (ImGui::MenuItem(
                        fontItem.name.c_str(),
                        nullptr,
                        isSelected)) {

                        g_SelectedFont =
                            fontItem.fontPtr;
                    }
                }
            }

            ImGui::EndMenu();
        }

        // --- MENÚ TEMA ---
        if (ImGui::BeginMenu("Tema")) {

            ImGui::TextDisabled(
                "Tema: %s",
                g_CurrentTheme.name.c_str()
            );

            ImGui::TextDisabled(
                "Autor: %s (v%s)",
                g_CurrentTheme.author.c_str(),
                g_CurrentTheme.version.c_str()
            );

            if (!g_CurrentTheme.description.empty()) {
                ImGui::TextWrapped(
                    "%s",
                    g_CurrentTheme.description.c_str()
                );
            }

            ImGui::Separator();

            // Mostrar temas detectados
            for (const auto& theme :
                 g_AvailableThemes) {

                bool isSelected =
                    (g_SelectedTheme == theme);

                if (ImGui::MenuItem(
                    theme.c_str(),
                    nullptr,
                    isSelected)) {

                    LoadTheme(theme);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(
                "Escanear Temas")) {

                ScanThemes();
            }

            if (ImGui::MenuItem(
                "Recargar Tema")) {

                LoadTheme();
            }

            ImGui::EndMenu();
        }

        // --- MENÚ PLUGINS ---
        if (ImGui::BeginMenu("Plugins")) {

            if (ImGui::MenuItem(
                "Escanear Carpeta")) {

                ScanPlugins();
            }

            ImGui::Separator();

            if (g_DetectedPlugins.empty()) {

                ImGui::TextDisabled(
                    "No se detectaron .dll en /Plugins"
                );

            } else {

                for (const auto& pluginName :
                     g_DetectedPlugins) {

                    // Comprobar si esta DLL ya fue cargada
                    bool isLoaded = false;

                    for (const auto& active :
                         g_ActivePlugins) {

                        if (active.fileName ==
                            pluginName) {

                            isLoaded = true;
                            break;
                        }
                    }

                    // Se muestra con un check si está activa
                    if (ImGui::MenuItem(
                        pluginName.c_str(),
                        nullptr,
                        isLoaded)) {

                        ExecuteLoadPlugin(
                            pluginName
                        );
                    }
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}