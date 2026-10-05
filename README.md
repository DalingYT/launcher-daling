# Launcher C++

Launcher C++ es un launcher hecho con Dear ImGui y DirectX 11 que puedes personalizar agregando juegos, plugins, temas y fuentes directamente desde sus carpetas, sin tocar el código. Además incluye un reproductor de música con playlist y Discord Rich Presence.

 <!-- Agrega aquí una captura de pantalla del Launcher: ![Launcher C++](ruta/a/captura.png) --> 

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


 <!-- Si el Launcher necesita Windows 10/11 o algún runtime de C++ (por ejemplo Visual C++ Redistributable), agrégalo aquí como "Requisitos". --> 

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

 <!-- Aquí puedes agregar los formatos de audio compatibles (mp3, wav, etc.). --> 

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

Hecho por: Daling 💙