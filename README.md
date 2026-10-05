# Launcher C++

Launcher C++ es un launcher hecho con Dear ImGui y DirectX 11 que puedes personalizar agregando juegos, plugins, temas y fuentes directamente desde sus carpetas, sin tocar el código. Además incluye un reproductor de música con playlist y Discord Rich Presence.

![](data:image/png;base64,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)

---

## ✨ Características

- Juegos como archivos `.dll`: solo copia el archivo y aparece en el menú.
- Plugins `.dll` que se activan desde la barra de menú.
- Temas personalizados con archivos `.json`.
- Fuentes personalizadas (`.ttf` y `.otf`).
- Reproductor de música con playlist, repetición y modo aleatorio.
- Discord Rich Presence.
- Registro de errores en `/ErrorLog`.

---

## 📦 Instalación

1. Descarga el Launcher desde [el repositorio](https://github.com/DalingYT/launcher-daling).
2. Extrae los archivos en la carpeta que quieras.
3. Ejecuta `Launcher.exe`.

Al abrirse por primera vez, el Launcher crea solo las carpetas que necesita:


| Carpeta       | Para qué sirve           |
| ------------- | ------------------------ |
| `/Games`      | Juegos (`.dll`)          |
| `/Plugins`    | Plugins (`.dll`)         |
| `/Theme`      | Temas                    |
| `/Fonts`      | Fuentes (`.ttf`, `.otf`) |
| `/Sounds`     | Música del reproductor   |
| `/ErrorLog`   | Registros de errores     |
| `/DataFolder` | Datos del Launcher       |


---

## 🎮 Cómo agregar juegos

Si es un juego (archivo `.dll`), solo ponlo en la carpeta `/Games`, que está al lado de `Launcher.exe`. Al abrir el Launcher aparecerá en la lista.

Para quien quiera crear juegos: cada `.dll` debe exportar estas 4 funciones obligatorias, o el Launcher lo rechazará y guardará el motivo en `/ErrorLog`:

- `Juego_SetImGuiContext`
- `Juego_Nombre`
- `Juego_Iniciar`
- `Juego_Dibujar`

Opcionalmente, puede exportar funciones para Discord Rich Presence (actividad, multijugador y progreso).

## 🧩 Cómo agregar plugins

Pon el archivo `.dll` en `/Plugins`. Luego, en la barra superior ve a **Plugins → Escanear Carpeta** y haz clic en el plugin para activarlo. El `.dll` debe exportar una función llamada `PluginInit`.

## 🎨 Cómo agregar temas

Cada tema es una carpeta dentro de `/Theme` con estos archivos:

```
Theme/
└── MiTema/
    ├── theme.json     (obligatorio, los colores)
    └── detail.json    (opcional, nombre, autor, etc.)
```

**theme.json**: todos los colores son opcionales y van en formato hexadecimal (`#RRGGBB`, o `#RRGGBBAA` si quieres transparencia):

```json
{
  "background": "#18181B",
  "button": "#27272A",
  "button_hover": "#3F3F46",
  "button_active": "#52525B",
  "text": "#FFFFFF",
  "bars": "#6366F1"
}
```

**detail.json**: la información que se muestra en el menú Tema:

```json
{
  "name": "Mi Tema",
  "author": "Tu nombre",
  "version": "1.0",
  "description": "Una descripción corta."
}
```

Para usarlo, abre el menú **Tema → Escanear Temas** y selecciónalo. El Launcher incluye por defecto el tema **Daling Dark**.

## 🔤 Cómo agregar fuentes

Copia tus archivos `.ttf` o `.otf` a la carpeta `/Fonts`. Después puedes elegirlas desde el menú **Fuente**.

## 🎵 Música

Pon tus canciones en la carpeta `/Sounds` y se agregan a la playlist del reproductor.

---

## ❗ El Launcher se cuelga

Revisa los logs en la carpeta `/ErrorLog`; ahí estará la causa del error.

1. Si fue por un juego, retíralo de `/Games` e intenta de nuevo.
2. Si fue por un plugin, retíralo de `/Plugins` e intenta de nuevo.
3. Si fue por un tema, retíralo de `/Theme` e intenta de nuevo.
4. Si nada de esto funciona, reinstala el Launcher completo desde [aquí](https://github.com/DalingYT/launcher-daling).

---

## ⚠️ Advertencia

Los juegos y plugins son archivos `.dll`, y eso significa que ejecutan código directamente en tu computadora. Instala solo los de fuentes que conozcas y en las que confíes, porque uno malicioso podría infectar tu PC.

---

## 🙏 Créditos

Launcher C++ no existiría sin estas herramientas y librerías:

- [Dear ImGui](https://github.com/ocornut/imgui), de Omar Cornut: la interfaz gráfica (licencia MIT).
- [nlohmann/json](https://github.com/nlohmann/json), de Niels Lohmann: lectura de los archivos `.json` de temas (licencia MIT).
- **DirectX 11** y la **API de Windows (Win32)**, de Microsoft: ventana y renderizado.
- [discord-rpc](https://github.com/discord/discord-rpc), de Discord: Discord Rich Presence, para mostrar en tu perfil lo que estás haciendo en el Launcher (licencia MIT).
- [RapidJSON](https://github.com/Tencent/rapidjson), de Tencent: librería JSON que usa discord-rpc (licencia MIT).
- [miniaudio](https://github.com/mackron/miniaudio), de David Reid: reproducción de audio del reproductor de música (dominio público o MIT-0, a elección).

### 🎵 Música

"The Angel, the Demon" de **Cacola** ([@noize_princess](https://x.com/noize_princess) en X): [escúchala en Spotify](https://open.spotify.com/intl-es/album/54vSNQ8AtD0CvERwtQ3bYv?si=NmzIKnuPRm-wNy7Gz7g4_w).

---

Hecho por: Daling 💙