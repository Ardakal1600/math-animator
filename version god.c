/* ============================================================
   CALCULADORA DE HUELLA DE CARBONO PERSONAL - Version Fix Borrar Nombre
   ============================================================ */

#include "raylib.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef struct {
    int transporte;
    int tiempo_trayecto;
    int ducha;
    int riego;
    int lavado_auto;
    int electricidad;
    int ac;
    int gas;
} RespuestasUsuario;

typedef struct {
    float co2_transporte;
    float co2_agua;
    float co2_electricidad;
    float co2_ac;
    float co2_gas;
    float co2_total;
} ResultadoHuella;

ResultadoHuella calcular_huella(RespuestasUsuario r) {
    ResultadoHuella res;

    float factor_transporte = 0.0f;
    switch (r.transporte) {
        case 1: factor_transporte = 0.00f; break;
        case 2: factor_transporte = 0.05f; break;
        case 3: factor_transporte = 0.21f; break;
        case 4: factor_transporte = 0.11f; break;
    }

    float minutos = 0.0f;
    switch (r.tiempo_trayecto) {
        case 0: minutos = 0.0f;   break;
        case 1: minutos = 20.0f;  break;
        case 2: minutos = 60.0f;  break;
        case 3: minutos = 135.0f; if (r.transporte == 3) factor_transporte = 0.26f; break;
        case 4: minutos = 200.0f; if (r.transporte == 3) factor_transporte = 0.26f; break;
    }
    res.co2_transporte = minutos * factor_transporte;

    float co2_ducha = 0.0f;
    switch (r.ducha) {
        case 1: co2_ducha = 0.5f; break;
        case 2: co2_ducha = 1.0f; break;
        case 3: co2_ducha = 2.0f; break;
    }
    res.co2_agua = co2_ducha + (r.riego == 1 ? 1.0f : 0.0f) + (r.lavado_auto == 1 ? 1.2f : 0.0f);

    float mensual = 0.0f;
    switch (r.electricidad) {
        case 1: mensual = 30.0f;  break;
        case 2: mensual = 60.0f;  break;
        case 3: mensual = 100.0f; break;
    }
    res.co2_electricidad = mensual / 30.0f;

    switch (r.ac) {
        case 1: res.co2_ac = 0.0f; break;
        case 2: res.co2_ac = 2.1f; break;
        case 3: res.co2_ac = 6.5f; break;
        default: res.co2_ac = 0.0f;
    }

    switch (r.gas) {
        case 1: res.co2_gas = 1.2f; break;
        case 2: res.co2_gas = 0.0f; break;
        case 3: res.co2_gas = 0.2f; break;
        default: res.co2_gas = 0.0f;
    }

    res.co2_total = res.co2_transporte + res.co2_agua + res.co2_electricidad
                   + res.co2_ac + res.co2_gas;
    return res;
}

#define COLOR_FONDO        (Color){ 251, 250, 246, 255 }
#define COLOR_TARJETA      (Color){ 241, 239, 228, 255 }
#define COLOR_BORDE        (Color){ 217, 210, 154, 255 }
#define COLOR_ACENTO       (Color){ 133, 200, 162, 255 }
#define COLOR_ACENTO_TXT   (Color){ 15, 58, 40, 255 }
#define COLOR_MINT         (Color){ 187, 220, 181, 255 }
#define COLOR_ROSA         (Color){ 233, 193, 217, 255 }
#define COLOR_TEXTO        (Color){ 58, 58, 52, 255 }
#define COLOR_TEXTO_TENUE  (Color){ 95, 92, 78, 255 }

typedef enum { PANTALLA_INTRO, PANTALLA_BIENVENIDA, PANTALLA_FORMULARIO, PANTALLA_RESULTADO } Pantalla;

enum {
    PASO_TRANSPORTE = 0,
    PASO_TIEMPO     = 1,
    PASO_DUCHA      = 2,
    PASO_AGUA_EXTRA = 3,
    PASO_ELECTRICIDAD = 4,
    PASO_AC         = 5,
    PASO_GAS        = 6
};
#define TOTAL_PASOS 7

typedef enum { SIN_TRANSICION, FUNDIENDO_SALIDA, FUNDIENDO_ENTRADA } EstadoFade;

char nombre[32] = "";
int letrasNombre = 0;

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
void ActualizarNombreDesdeJS(const char *texto) {
    strncpy(nombre, texto, 31);
    nombre[31] = '\0';
    letrasNombre = (int)strlen(nombre);
}

EM_JS(void, OcultarTecladoJS, (), {
    var inputs = document.getElementsByTagName('input');
    for (var i = 0; i < inputs.length; i++) {
        inputs[i].blur();
        inputs[i].style.display = 'none';
        inputs[i].disabled = true;
    }
    if (document.activeElement) {
        document.activeElement.blur();
    }
});
#else
void OcultarTecladoJS() {}
#endif

Font fuente;

void texto(const char *msg, int x, int y, int tam, Color color) {
    DrawTextEx(fuente, msg, (Vector2){ (float)x, (float)y }, (float)tam, 1.0f, color);
}

void textoCentrado(const char *msg, int centroX, int y, int tam, Color color) {
    Vector2 medida = MeasureTextEx(fuente, msg, (float)tam, 1.0f);
    DrawTextEx(fuente, msg, (Vector2){ centroX - medida.x / 2.0f, (float)y }, (float)tam, 1.0f, color);
}

/* DETECCION INTELIGENTE DE ENTRADA EN MOVIL E IPHONE */
int FuePresionadoEnRec(Rectangle rect) {
    Vector2 pos = GetMousePosition();
    int interactuando = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP);

    if (GetTouchPointCount() > 0) {
        pos = GetTouchPosition(0);
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP)) {
            interactuando = 1;
        }
    }
    return CheckCollisionPointRec(pos, rect) && interactuando;
}

int botonConLetra(Rectangle rect, char letra, const char *label, int seleccionado) {
    Color fondo = seleccionado ? COLOR_ACENTO : (Color){255,255,255,255};
    Color colorTxt = seleccionado ? COLOR_ACENTO_TXT : COLOR_TEXTO;

    DrawRectangleRounded(rect, 0.3f, 8, fondo);
    if (!seleccionado) DrawRectangleRoundedLines(rect, 0.3f, 8, 2.0f, COLOR_BORDE);

    Rectangle cajaLetra = { rect.x + 10, rect.y + rect.height/2 - 14, 28, 28 };
    Color fondoLetra = seleccionado ? (Color){255,255,255,90} : COLOR_TARJETA;
    DrawRectangleRounded(cajaLetra, 0.3f, 6, fondoLetra);
    char textoLetra[2] = { letra, '\0' };
    Vector2 medidaLetra = MeasureTextEx(fuente, textoLetra, 15, 1.0f);
    DrawTextEx(fuente, textoLetra,
        (Vector2){ cajaLetra.x + 14 - medidaLetra.x/2, cajaLetra.y + 14 - medidaLetra.y/2 },
        15, 1.0f, colorTxt);

    texto(label, (int)rect.x + 50, (int)(rect.y + rect.height/2 - 9), 15, colorTxt);

    return FuePresionadoEnRec(rect);
}

int boton(Rectangle rect, const char *label, int seleccionado) {
    Color fondo = seleccionado ? COLOR_ACENTO : (Color){255,255,255,255};
    Color colorTxt = seleccionado ? COLOR_ACENTO_TXT : COLOR_TEXTO;

    DrawRectangleRounded(rect, 0.35f, 8, fondo);
    if (!seleccionado) DrawRectangleRoundedLines(rect, 0.35f, 8, 2.0f, COLOR_BORDE);
    texto(label, (int)rect.x + 14, (int)(rect.y + rect.height/2 - 9), 16, colorTxt);

    return FuePresionadoEnRec(rect);
}

void casilla(Rectangle rect, const char *label, int *marcado) {
    if (FuePresionadoEnRec(rect)) {
        *marcado = !(*marcado);
    }

    Color fondo = *marcado ? COLOR_ACENTO : (Color){255,255,255,255};
    DrawRectangleRounded(rect, 0.3f, 8, fondo);
    if (!*marcado) DrawRectangleRoundedLines(rect, 0.3f, 8, 2.0f, COLOR_BORDE);

    Rectangle caja = { rect.x + 12, rect.y + rect.height/2 - 9, 18, 18 };
    DrawRectangleRounded(caja, 0.3f, 6, (Color){255,255,255,255});
    DrawRectangleRoundedLines(caja, 0.3f, 6, 2.0f, COLOR_BORDE);
    if (*marcado) {
        DrawLineEx((Vector2){caja.x+3, caja.y+9}, (Vector2){caja.x+7, caja.y+14}, 2.5f, COLOR_ACENTO_TXT);
        DrawLineEx((Vector2){caja.x+7, caja.y+14}, (Vector2){caja.x+15, caja.y+4}, 2.5f, COLOR_ACENTO_TXT);
    }

    Color colorTxt = *marcado ? COLOR_ACENTO_TXT : COLOR_TEXTO;
    texto(label, (int)rect.x + 40, (int)(rect.y + rect.height/2 - 8), 14, colorTxt);
}

int siguientePaso(int paso, RespuestasUsuario r) {
    if (paso == PASO_TRANSPORTE) {
        return (r.transporte == 1) ? PASO_DUCHA : PASO_TIEMPO;
    }
    return paso + 1;
}

int pasoAnterior(int paso, RespuestasUsuario r) {
    if (paso == PASO_DUCHA) {
        return (r.transporte == 1) ? PASO_TRANSPORTE : PASO_TIEMPO;
    }
    return paso - 1;
}

int indiceVisible(int paso, RespuestasUsuario r) {
    if (paso <= PASO_TIEMPO) return paso;
    return (r.transporte == 1) ? paso - 1 : paso;
}

int totalPasosVisibles(RespuestasUsuario r) {
    return (r.transporte == 1) ? TOTAL_PASOS - 1 : TOTAL_PASOS;
}

int pasoValido(int paso, RespuestasUsuario r) {
    switch (paso) {
        case PASO_TRANSPORTE:    return r.transporte != 0;
        case PASO_TIEMPO:        return r.tiempo_trayecto != 0;
        case PASO_DUCHA:         return r.ducha != 0;
        case PASO_AGUA_EXTRA:    return 1;
        case PASO_ELECTRICIDAD:  return r.electricidad != 0;
        case PASO_AC:            return r.ac != 0;
        case PASO_GAS:           return r.gas != 0;
    }
    return 0;
}

#ifdef __EMSCRIPTEN__
EM_JS(void, EnviarAGoogleSheets,
    (const char *nombre, float total, float transp, float agua,
     float elec, float aire, float gas), {
    const datos = {
        nombre: UTF8ToString(nombre),
        total_co2: total,
        transporte: transp,
        agua: agua,
        electricidad: elec,
        aire: aire,
        gas: gas
    };

    fetch('https://script.google.com/macros/s/AKfycbzcitzkWIn7-512uh3FktTHb4rZwsDp0iyZLkIFwQ8GwIFx1jcomJFvqsspOmbfUz-1/exec', {
        method: 'POST',
        mode: 'no-cors',
        body: JSON.stringify(datos)
    }).catch(error => console.error('Error al enviar a Google Sheets:', error));
});
#else
void EnviarAGoogleSheets(const char *nombre, float total, float transp,
                          float agua, float elec, float aire, float gas) {
    printf("[Google Sheets omitido en escritorio] %s -> %.2f kg CO2\n", nombre, total);
}
#endif

int main(void) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);

    const int ANCHO = 420, ALTO = 800;
    InitWindow(ANCHO, ALTO, "Calculadora de Huella de Carbono");
    SetGesturesEnabled(GESTURE_TAP);
    SetTargetFPS(60);

    int totalCodepoints = 0;
    int *codepoints = LoadCodepoints(
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        "áéíóúñÁÉÍÓÚÑ¿¡üÜ", &totalCodepoints);
    fuente = LoadFontEx("recursos/Nunito-Regular.ttf", 64, codepoints, totalCodepoints);
    UnloadCodepoints(codepoints);

    Texture2D iconoBienvenida  = LoadTexture("recursos/trees.png");
    Texture2D iconoTransporte  = LoadTexture("recursos/transporte.png");
    Texture2D iconoAgua        = LoadTexture("recursos/agua.png");
    Texture2D iconoElectricidad= LoadTexture("recursos/electricidad.png");
    Texture2D iconoAire        = LoadTexture("recursos/aire.png");
    Texture2D iconoGas         = LoadTexture("recursos/gas.png");
    Texture2D iconoPlanta      = LoadTexture("recursos/plant.png");

    Pantalla pantallaActual = PANTALLA_INTRO;
    int paso = PASO_TRANSPORTE;

    RespuestasUsuario respuestas = {0,0,0,0,0,0,0,0};
    ResultadoHuella resultado = {0};

    EstadoFade fade = SIN_TRANSICION;
    float alphaFade = 0.0f;
    Pantalla pantallaDestino = PANTALLA_INTRO;
    const float VELOCIDAD_FADE = 3.5f;

    while (!WindowShouldClose()) {

        float dt = GetFrameTime();

        if (fade == FUNDIENDO_SALIDA) {
            alphaFade += dt * VELOCIDAD_FADE;
            if (alphaFade >= 1.0f) {
                alphaFade = 1.0f;
                pantallaActual = pantallaDestino;
                fade = FUNDIENDO_ENTRADA;
            }
        } else if (fade == FUNDIENDO_ENTRADA) {
            alphaFade -= dt * VELOCIDAD_FADE;
            if (alphaFade <= 0.0f) {
                alphaFade = 0.0f;
                fade = SIN_TRANSICION;
            }
        }
        int bloqueadoPorFade = (fade != SIN_TRANSICION);

        BeginDrawing();
        ClearBackground(COLOR_FONDO);

        /* ===================== INTRODUCCION ===================== */
        if (pantallaActual == PANTALLA_INTRO) {

            textoCentrado("Bienvenido a la Calculadora", ANCHO/2, 90, 19, COLOR_TEXTO);
            textoCentrado("de Huella de Carbono", ANCHO/2, 116, 19, COLOR_TEXTO);

            const char *parrafo[9] = {
                "La huella de carbono es la cantidad de",
                "gases de efecto invernadero que",
                "generamos con nuestras actividades",
                "diarias: como nos movemos, cuanta agua",
                "y electricidad usamos, y como cocinamos.",
                "",
                "Responde 7 preguntas rapidas sobre tu",
                "dia y descubre tu impacto ambiental,",
                "con recomendaciones para reducirlo."
            };
            int y = 190;
            for (int i = 0; i < 9; i++) {
                if (parrafo[i][0] != '\0') {
                    textoCentrado(parrafo[i], ANCHO/2, y, 13, COLOR_TEXTO_TENUE);
                }
                y += 20;
            }

            Rectangle btnContinuar = { 40, 430, ANCHO - 80, 48 };
            int clickContinuar = boton(btnContinuar, "Continuar", 1);
            if (!bloqueadoPorFade && clickContinuar) {
                fade = FUNDIENDO_SALIDA;
                alphaFade = 0.0f;
                pantallaDestino = PANTALLA_BIENVENIDA;
            }
        }

        /* ===================== BIENVENIDA ===================== */
        else if (pantallaActual == PANTALLA_BIENVENIDA) {

            float escalaIcono = 0.234f;
            DrawTextureEx(iconoBienvenida, (Vector2){ ANCHO/2 - 60, 40 }, 0, escalaIcono, WHITE);

            textoCentrado("Calculadora de Huella", ANCHO/2, 176, 19, COLOR_TEXTO);
            textoCentrado("de Carbono Personal", ANCHO/2, 202, 19, COLOR_TEXTO);
            textoCentrado("Responde 7 preguntas rapidas", ANCHO/2, 238, 13, COLOR_TEXTO_TENUE);
            textoCentrado("sobre tus actividades de hoy", ANCHO/2, 256, 13, COLOR_TEXTO_TENUE);

            texto("Tu nombre:", 40, 290, 13, COLOR_TEXTO_TENUE);
            Rectangle campoNombre = { 40, 310, ANCHO - 80, 44 };
            DrawRectangleRounded(campoNombre, 0.25f, 8, (Color){255,255,255,255});
            DrawRectangleRoundedLines(campoNombre, 0.25f, 8, 2.0f, COLOR_BORDE);

            if (letrasNombre == 0) {
                texto("Escribe tu nombre...", (int)campoNombre.x + 14, (int)campoNombre.y + 13, 14, COLOR_TEXTO_TENUE);
            } else {
                texto(nombre, (int)campoNombre.x + 14, (int)campoNombre.y + 13, 16, COLOR_TEXTO);

                /* Boton visual 'X' para limpiar el texto en moviles */
                Rectangle btnBorrar = { campoNombre.x + campoNombre.width - 36, campoNombre.y + 8, 28, 28 };
                DrawRectangleRounded(btnBorrar, 0.5f, 6, COLOR_TARJETA);
                textoCentrado("x", (int)(btnBorrar.x + 14), (int)(btnBorrar.y + 4), 16, COLOR_TEXTO_TENUE);

                if (!bloqueadoPorFade && FuePresionadoEnRec(btnBorrar)) {
                    letrasNombre = 0;
                    nombre[0] = '\0';
                }
            }

            if (!bloqueadoPorFade) {
                int tecla = GetCharPressed();
                while (tecla > 0) {
                    if (tecla >= 32 && tecla <= 255 && letrasNombre < 31) {
                        nombre[letrasNombre] = (char)tecla;
                        letrasNombre++;
                        nombre[letrasNombre] = '\0';
                    }
                    tecla = GetCharPressed();
                }
                /* Captura de borrar tanto por codigo de tecla como por la tecla física / virtual */
                if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && letrasNombre > 0) {
                    letrasNombre--;
                    nombre[letrasNombre] = '\0';
                }
            }

            Rectangle btnIniciar = { 40, 380, ANCHO - 80, 48 };
            int clickIniciar = boton(btnIniciar, "Iniciar test", 1);

            if (!bloqueadoPorFade && clickIniciar) {
                if (letrasNombre < 2) {
                    strcpy(nombre, "Invitado");
                    letrasNombre = 8;
                }
                
                OcultarTecladoJS();

                fade = FUNDIENDO_SALIDA;
                alphaFade = 0.0f;
                pantallaDestino = PANTALLA_FORMULARIO;
                paso = PASO_TRANSPORTE;
            }
        }

        /* ===================== FORMULARIO ===================== */
        else if (pantallaActual == PANTALLA_FORMULARIO) {

            int totalVisible = totalPasosVisibles(respuestas);
            int idxVisible = indiceVisible(paso, respuestas);
            float progreso = (idxVisible + 1) / (float)totalVisible;
            if (progreso > 1.0f) progreso = 1.0f;

            DrawRectangleRounded((Rectangle){ 30, 34, ANCHO - 60, 8 }, 0.5f, 4, COLOR_BORDE);
            DrawRectangleRounded((Rectangle){ 30, 34, (ANCHO - 60) * progreso, 8 }, 0.5f, 4, COLOR_ACENTO);

            char etiquetaProgreso[16];
            sprintf(etiquetaProgreso, "%d%%", (int)(progreso * 100));
            texto(etiquetaProgreso, ANCHO - 66, 48, 12, COLOR_TEXTO_TENUE);

            Texture2D iconoActual = iconoTransporte;
            const char *pregunta = "";

            switch (paso) {
                case PASO_TRANSPORTE:    iconoActual = iconoTransporte;   pregunta = "Como te transportaste hoy?"; break;
                case PASO_TIEMPO:        iconoActual = iconoTransporte;   pregunta = "Cuanto dura tu trayecto (ida y vuelta)?"; break;
                case PASO_DUCHA:         iconoActual = iconoAgua;         pregunta = "Como es tu ducha diaria?"; break;
                case PASO_AGUA_EXTRA:    iconoActual = iconoAgua;         pregunta = "Usos adicionales de agua"; break;
                case PASO_ELECTRICIDAD: iconoActual = iconoElectricidad; pregunta = "Cuanto pagas de luz al mes?"; break;
                case PASO_AC:            iconoActual = iconoAire;         pregunta = "Uso de aire acondicionado?"; break;
                case PASO_GAS:           iconoActual = iconoGas;          pregunta = "Que usas para cocinar?"; break;
            }

            DrawRectangleRounded((Rectangle){ 30, 74, ANCHO - 60, 92 }, 0.15f, 8, COLOR_TARJETA);
            DrawTextureEx(iconoActual, (Vector2){ 42, 86 }, 0, 0.11f, WHITE);
            char etiquetaPaso[32];
            sprintf(etiquetaPaso, "Pregunta %d de %d", idxVisible + 1, totalVisible);
            texto(etiquetaPaso, 108, 90, 12, COLOR_TEXTO_TENUE);
            texto(pregunta, 108, 112, 15, COLOR_TEXTO);

            const int ESPACIADO = 68;
            const int ALTO_OPCION = 54;

            if (paso == PASO_TRANSPORTE) {
                const char *ops[4] = {"A pie / Bicicleta", "Autobus / Metro", "Auto propio", "Motocicleta"};
                for (int i = 0; i < 4; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.transporte == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.transporte = i+1;
                }
            }
            else if (paso == PASO_TIEMPO) {
                const char *ops[4] = {"Menos de 30 min", "30 min a 1h30", "1h30 a 3h (tranque)", "Mas de 3h"};
                for (int i = 0; i < 4; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.tiempo_trayecto == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.tiempo_trayecto = i+1;
                }
            }
            else if (paso == PASO_DUCHA) {
                const char *ops[3] = {"Corta (menos de 5 min)", "Promedio (5 a 10 min)", "Larga (mas de 10 min)"};
                for (int i = 0; i < 3; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.ducha == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.ducha = i+1;
                }
            }
            else if (paso == PASO_AGUA_EXTRA) {
                Rectangle r1 = { 30, 186, ANCHO - 60, ALTO_OPCION };
                Rectangle r2 = { 30, 186 + ESPACIADO, ANCHO - 60, ALTO_OPCION };
                if (!bloqueadoPorFade) {
                    casilla(r1, "Riego jardin frecuente", &respuestas.riego);
                    casilla(r2, "Lavo mi auto con manguera", &respuestas.lavado_auto);
                }
            }
            else if (paso == PASO_ELECTRICIDAD) {
                const char *ops[3] = {"Menos de $30 al mes", "$30 a $70 al mes", "Mas de $70 al mes"};
                for (int i = 0; i < 3; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.electricidad == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.electricidad = i+1;
                }
            }
            else if (paso == PASO_AC) {
                const char *ops[3] = {"No uso (ventilacion natural)", "Moderado (1 a 4 horas)", "Intensivo (+5 horas)"};
                for (int i = 0; i < 3; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.ac == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.ac = i+1;
                }
            }
            else if (paso == PASO_GAS) {
                const char *ops[3] = {"Gas en tanque (Gas LP)", "Estufa electrica/induccion", "Casi no cocino en casa"};
                for (int i = 0; i < 3; i++) {
                    Rectangle r = { 30, 186 + i*ESPACIADO, ANCHO - 60, ALTO_OPCION };
                    int sel = (respuestas.gas == i+1);
                    if (!bloqueadoPorFade && botonConLetra(r, 'A'+i, ops[i], sel)) respuestas.gas = i+1;
                }
            }

            int hayRespuesta = pasoValido(paso, respuestas);

            Rectangle btnAtras = { 30, ALTO - 90, 120, 46 };
            Rectangle btnSiguiente = { ANCHO - 150, ALTO - 90, 120, 46 };

            if (paso != PASO_TRANSPORTE) {
                if (!bloqueadoPorFade && boton(btnAtras, "Atras", 0)) {
                    paso = pasoAnterior(paso, respuestas);
                }
            }

            if (hayRespuesta) {
                const char *labelBtn = (paso == PASO_GAS) ? "Resultado" : "Siguiente";
                if (!bloqueadoPorFade && boton(btnSiguiente, labelBtn, 1)) {
                    if (paso == PASO_GAS) {
                        resultado = calcular_huella(respuestas);
                        EnviarAGoogleSheets(
                            nombre,
                            resultado.co2_total,
                            resultado.co2_transporte,
                            resultado.co2_agua,
                            resultado.co2_electricidad,
                            resultado.co2_ac,
                            resultado.co2_gas
                        );
                        fade = FUNDIENDO_SALIDA;
                        alphaFade = 0.0f;
                        pantallaDestino = PANTALLA_RESULTADO;
                    } else {
                        paso = siguientePaso(paso, respuestas);
                    }
                }
            }
        }

        /* ===================== RESULTADO ===================== */
        else if (pantallaActual == PANTALLA_RESULTADO) {

            char saludo[80];
            sprintf(saludo, "%s, tu resultado es:", nombre);
            texto(saludo, 30, 60, 15, COLOR_TEXTO_TENUE);

            char textoTotal[32];
            sprintf(textoTotal, "%.1f kg CO2e/dia", resultado.co2_total);
            texto(textoTotal, 30, 84, 30, COLOR_TEXTO);

            const char *nivel; Color colorNivel;
            if (resultado.co2_total < 4.0f)      { nivel = "IMPACTO BAJO";      colorNivel = COLOR_MINT; }
            else if (resultado.co2_total <= 9.0f){ nivel = "IMPACTO MODERADO";  colorNivel = COLOR_BORDE; }
            else                                  { nivel = "IMPACTO ALTO";      colorNivel = COLOR_ROSA; }

            Vector2 medidaNivel = MeasureTextEx(fuente, nivel, 14, 1.0f);
            Rectangle badge = { 30, 130, medidaNivel.x + 28, 32 };
            DrawRectangleRounded(badge, 0.5f, 8, colorNivel);
            texto(nivel, 44, 138, 14, COLOR_TEXTO);

            DrawRectangleRounded((Rectangle){ 30, 180, ANCHO - 60, 150 }, 0.1f, 8, COLOR_TARJETA);
            char linea[48];
            sprintf(linea, "Transporte:    %.2f kg CO2", resultado.co2_transporte);
            texto(linea, 46, 196, 14, COLOR_TEXTO);
            sprintf(linea, "Agua:          %.2f kg CO2", resultado.co2_agua);
            texto(linea, 46, 224, 14, COLOR_TEXTO);
            sprintf(linea, "Electricidad:  %.2f kg CO2", resultado.co2_electricidad);
            texto(linea, 46, 252, 14, COLOR_TEXTO);
            sprintf(linea, "Aire Ac.:      %.2f kg CO2", resultado.co2_ac);
            texto(linea, 46, 280, 14, COLOR_TEXTO);
            sprintf(linea, "Gas cocina:    %.2f kg CO2", resultado.co2_gas);
            texto(linea, 46, 308, 14, COLOR_TEXTO);

            DrawRectangleRounded((Rectangle){ 30, 350, ANCHO - 60, 130 }, 0.1f, 8, COLOR_TARJETA);

            if (resultado.co2_total < 4.0f) {
                DrawTextureEx(iconoPlanta, (Vector2){ 44, 364 }, 0, 0.09f, WHITE);
                texto("Sigue asi! Tu huella esta entre", 108, 372, 13, COLOR_TEXTO);
                texto("las mas bajas posibles.", 108, 392, 13, COLOR_TEXTO);
            } else {
                float mayor = resultado.co2_transporte;
                Texture2D iconoTip = iconoTransporte;
                const char *tip1 = "Revisa la presion de tus llantas";
                const char *tip2 = "y evita frenar/acelerar de golpe.";

                if (resultado.co2_agua > mayor) {
                    mayor = resultado.co2_agua; iconoTip = iconoAgua;
                    tip1 = "Cerrar la llave mientras te enjabonas";
                    tip2 = "puede ahorrar hasta 40% del agua.";
                }
                if (resultado.co2_electricidad > mayor) {
                    mayor = resultado.co2_electricidad; iconoTip = iconoElectricidad;
                    tip1 = "Apaga luces/aparatos sin usar y";
                    tip2 = "cambia a focos LED si puedes.";
                }
                if (resultado.co2_ac > mayor) {
                    mayor = resultado.co2_ac; iconoTip = iconoAire;
                    tip1 = "Sube el termostato de 20 a 24:";
                    tip2 = "mismo confort, menos consumo.";
                }
                if (resultado.co2_gas > mayor) {
                    mayor = resultado.co2_gas; iconoTip = iconoGas;
                    tip1 = "Usar ollas con tapa y verificar fugas";
                    tip2 = "en el tanque reduce el consumo.";
                }

                DrawTextureEx(iconoTip, (Vector2){ 44, 364 }, 0, 0.09f, WHITE);
                texto(tip1, 108, 372, 13, COLOR_TEXTO);
                texto(tip2, 108, 392, 13, COLOR_TEXTO);
            }

            Rectangle btnReiniciar = { 30, ALTO - 90, ANCHO - 60, 48 };
            if (!bloqueadoPorFade && boton(btnReiniciar, "Volver a empezar", 1)) {
                respuestas = (RespuestasUsuario){0,0,0,0,0,0,0,0};
                letrasNombre = 0; nombre[0] = '\0';
                fade = FUNDIENDO_SALIDA;
                alphaFade = 0.0f;
                pantallaDestino = PANTALLA_INTRO;
            }
        }

        /* Capa de Transicion (Fade) */
        if (fade != SIN_TRANSICION) {
            Color overlay = COLOR_FONDO;
            overlay.a = (unsigned char)(alphaFade * 255.0f);
            DrawRectangle(0, 0, ANCHO, ALTO, overlay);
        }

        EndDrawing();
    }

    /* Descargar recursos */
    UnloadFont(fuente);
    UnloadTexture(iconoBienvenida);
    UnloadTexture(iconoTransporte);
    UnloadTexture(iconoAgua);
    UnloadTexture(iconoElectricidad);
    UnloadTexture(iconoAire);
    UnloadTexture(iconoGas);
    UnloadTexture(iconoPlanta);

    CloseWindow();
    return 0;
}