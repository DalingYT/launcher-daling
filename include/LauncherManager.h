#pragma once
#include <string>
#include <vector>

// Declaración de las funciones para que main.cpp las conozca
void ScanThemes();
void LoadTheme(const std::string& themeFolder);
void ScanAndLoadFonts(float fontSize = 16.0f);
void ScanPlugins();
void ExecuteLoadPlugin(const std::string& pluginName);
void RenderMainMenuBar();

extern std::vector<std::string> g_AvailableThemes;