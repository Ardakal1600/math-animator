#include "raylib.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// ---------------------------------------------------------------------------
// Estructuras de Datos
// ---------------------------------------------------------------------------
typedef enum {
    PANTALLA_INTRO = 0,
    PANTALLA_BIENVENIDA,
    PANTALLA_FORMULARIO,
    PANTALLA_RESULTADO
} Pantalla;

typedef enum {
    PASO_TRANSPORTE = 0,
    PASO_TIEMPO,
    PASO_ELECTRICIDAD,
    PASO_CARNE,
    PASO_GAS,
    PASO_RECICLAJE,
    PASO_VUELOS,
    TOTAL_PASOS
} PasoFormulario;

typedef struct {
    int transporte;   // 1: A pie/Bici, 2: Bus/Metro, 3: Auto
    int tiempo;       // 1: <30min, 2: 30-60min, 3: >60min
    int electricidad; // 1: <$30, 2: $30-$60, 3: >$60
    int carne;        // 1: Nunca, 2: 1-3 días, 3: Casi diario
    int gas;          // 1: <1 balón, 2: 1 balón, 3: >1 balón
    int reciclaje;    // 1: Siempre, 2: A veces, 3: Nunca
    int vuelos;       // 1: Ninguno, 2: 1-2, 3: 3+
} RespuestasUsuario;

typedef struct {
    float co2_transporte;
    float co2_electricidad;
    float co2_carne;
    float co2_gas;
    float co2_reciclaje;
    float co2_vuelos;
    float co2_total_diario;
} ResultadoHuella;

// ---------------------------------------------------------------------------
// Variables Globales
// ---------------------------------------------------------------------------
static Pantalla pantallaActual = PANTALLA_INTRO;
static PasoFormulario pasoActual = PASO_TRANSPORTE;
static RespuestasUsuario respuestas = {0};
static ResultadoHuella resultado = {0};

static char nombre[32] = "";
static int letrasNombre = 0;
static Font fuentePersonalizada;

// ---------------------------------------------------------------------------
// Puente JS / Emscripten (Llama a Google Sheets y recibe el Nombre)
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
void ActualizarNombreDesdeJS(const char *texto) {
    if (texto != NULL) {
        strncpy(nombre, texto, 31);
        nombre[31] = '\0';
        letrasNombre = (int)strlen(nombre);
    }
}

EM_JS(void, EnviarAGoogleSheets, (const char* pNombre, float totalCO2), {
    const url = 'https://script.google.com/macros/s/AKfycbz_REEMPLAZA_CON_TU_URL_DE_SCRIPT/exec';
    const payload = {
        nombre: UTF8ToString(pNombre),
        total_co2: totalCO2
    };
    fetch(url, {
        method: 'POST',
        mode: 'no-cors',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    }).catch(err => console.error("Error al enviar a Google Sheets:", err));
});
#endif

// ---------------------------------------------------------------------------
// Lógica de Cálculos
// ---------------------------------------------------------------------------
void CalcularHuella(void) {
    float t = 0.0f;
    if (respuestas.transporte == 2) {
        if (respuestas.tiempo == 1) t = 1.2f;
        else if (respuestas.tiempo == 2) t = 2.5f;
        else if (respuestas.tiempo == 3) t = 4.0f;
    } else if (respuestas.transporte == 3) {
        if (respuestas.tiempo == 1) t = 3.0f;
        else if (respuestas.tiempo == 2) t = 6.0f;
        else if (respuestas.tiempo == 3) t = 10.0f;
    }
    resultado.co2_transporte = t;

    float e = 0.0f;
    if (respuestas.electricidad == 1) e = 1.0f;
    else if (respuestas.electricidad == 2) e = 2.0f;
    else if (respuestas.electricidad == 3) e = 3.5f;
    resultado.co2_electricidad = e;

    float c = 0.0f;
    if (respuestas.carne == 1) c = 0.5f;
    else if (respuestas.carne == 2) c = 1.8f;
    else if (respuestas.carne == 3) c = 3.5f;
    resultado.co2_carne = c;

    float g = 0.0f;
    if (respuestas.gas == 1) g = 0.3f;
    else if (respuestas.gas == 2) g = 0.8f;
    else if (respuestas.gas == 3) g = 1.5f;
    resultado.co2_gas = g;

    float r = 0.0f;
    if (respuestas.reciclaje == 1) r = -0.3f;
    else if (respuestas.reciclaje == 2) r = 0.0f;
    else if (respuestas.reciclaje == 3) r = 0.5f;
    resultado.co2_reciclaje = r;

    float v = 0.0f;
    if (respuestas.vuelos == 1) v = 0.0f;
    else if (respuestas.vuelos == 2) v = 1.0f;
    else if (respuestas.vuelos == 3) v = 2.5f;
    resultado.co2_vuelos = v;

    resultado.co2_total_diario = resultado.co2_transporte + resultado.co2_electricidad +
                                 resultado.co2_carne + resultado.co2_gas +
                                 resultado.co2_reciclaje + resultado.co2_vuelos;

#ifdef __EMSCRIPTEN__
    EnviarAGoogleSheets(nombre, resultado.co2_total_diario);
#endif
}

void SiguientePaso(void) {
    if (pasoActual == PASO_TRANSPORTE && respuestas.transporte == 1) {
        pasoActual = PASO_ELECTRICIDAD;
    } else {
        pasoActual++;
    }

    if (pasoActual >= TOTAL_PASOS) {
        CalcularHuella();
        pantallaActual = PANTALLA_RESULTADO;
    }
}

void PasoAnterior(void) {
    if (pasoActual == PASO_ELECTRICIDAD && respuestas.transporte == 1) {
        pasoActual = PASO_TRANSPORTE;
    } else if (pasoActual > PASO_TRANSPORTE) {
        pasoActual--;
    } else {
        pantallaActual = PANTALLA_BIENVENIDA;
    }
}

// ---------------------------------------------------------------------------
// Renderizado y Bucle Principal
// ---------------------------------------------------------------------------
int main(void) {
    const int ANCHO = 420;
    const int ALTO = 800;

    InitWindow(ANCHO, ALTO, "Calculadora de Huella de Carbono");
    SetTargetFPS(60);

    // Carga de codepoints para acentos y caracteres UTF-8 en español
    int totalCodepoints = 0;
    int *codepoints = LoadCodepoints(
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        "áéíóúñÁÉÍÓÚÑ¿¡üÜ", &totalCodepoints);
    
    // Intentar cargar fuente personalizada si existe, o usar fuente por defecto
    fuentePersonalizada = GetFontDefault();

    Color colorFondoTarj = (Color){252, 251, 248, 255};
    Color colorBoton = (Color){116, 196, 148, 255};
    Color colorTexto = (Color){40, 40, 40, 255};

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){232, 226, 196, 255}); // Fondo Kaki

        // Tarjeta Blanca Central
        DrawRectangleRounded((Rectangle){15, 15, 390, 770}, 0.04f, 8, colorFondoTarj);

        switch (pantallaActual) {
            case PANTALLA_INTRO:
                DrawText("Calculadora de\nHuella de Carbono", 40, 100, 28, colorTexto);
                DrawText("Descubre tu impacto ambiental diario.", 40, 200, 16, DARKGRAY);

                if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){40, 680, 340, 50}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    pantallaActual = PANTALLA_BIENVENIDA;
                }
                DrawRectangleRounded((Rectangle){40, 680, 340, 50}, 0.3f, 4, colorBoton);
                DrawText("Empezar", 170, 695, 20, WHITE);
                break;

            case PANTALLA_BIENVENIDA:
                DrawText("¿Cuál es tu nombre?", 40, 100, 24, colorTexto);
                
                // Caja de texto
                DrawRectangleRounded((Rectangle){40, 310, 340, 44}, 0.2f, 4, LIGHTGRAY);
                DrawRectangleRoundedLines((Rectangle){40, 310, 340, 44}, 0.2f, 4, 2, GRAY);
                
                if (letrasNombre > 0) {
                    DrawText(nombre, 50, 322, 20, colorTexto);
                } else {
                    DrawText("Escribe tu nombre...", 50, 322, 18, GRAY);
                }

                if (letrasNombre > 0) {
                    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){40, 680, 340, 50}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        pasoActual = PASO_TRANSPORTE;
                        pantallaActual = PANTALLA_FORMULARIO;
                    }
                    DrawRectangleRounded((Rectangle){40, 680, 340, 50}, 0.3f, 4, colorBoton);
                    DrawText("Continuar", 160, 695, 20, WHITE);
                }
                break;

            case PANTALLA_FORMULARIO:
                // Título e indicador de paso
                DrawText(TextFormat("Pregunta %d de %d", pasoActual + 1, TOTAL_PASOS), 40, 40, 16, GRAY);

                switch (pasoActual) {
                    case PASO_TRANSPORTE:
                        DrawText("¿Cómo te desplazas principalmente?", 40, 80, 20, colorTexto);
                        if (DrawText("1. A pie / Bicicleta", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.transporte = 1; SiguientePaso(); }
                        if (DrawText("2. Transporte Público (Bus/Metro)", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.transporte = 2; SiguientePaso(); }
                        if (DrawText("3. Vehículo Particular / Moto", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.transporte = 3; SiguientePaso(); }
                        break;

                    case PASO_TIEMPO:
                        DrawText("¿Cuánto tiempo dura tu trayecto diario?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Menos de 30 minutos", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.tiempo = 1; SiguientePaso(); }
                        if (DrawText("2. Entre 30 y 60 minutos", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.tiempo = 2; SiguientePaso(); }
                        if (DrawText("3. Más de 60 minutos", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.tiempo = 3; SiguientePaso(); }
                        break;

                    case PASO_ELECTRICIDAD:
                        DrawText("¿Cuánto pagan de electricidad al mes?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Menos de $30", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.electricidad = 1; SiguientePaso(); }
                        if (DrawText("2. Entre $30 y $60", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.electricidad = 2; SiguientePaso(); }
                        if (DrawText("3. Más de $60", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.electricidad = 3; SiguientePaso(); }
                        break;

                    case PASO_CARNE:
                        DrawText("¿Con qué frecuencia consumes carne?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Rara vez / Vegetariano", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.carne = 1; SiguientePaso(); }
                        if (DrawText("2. 1 a 3 veces por semana", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.carne = 2; SiguientePaso(); }
                        if (DrawText("3. Casi todos los días", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.carne = 3; SiguientePaso(); }
                        break;

                    case PASO_GAS:
                        DrawText("¿Uso de tanques de gas al mes?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Menos de 1 tanque", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.gas = 1; SiguientePaso(); }
                        if (DrawText("2. 1 tanque completo", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.gas = 2; SiguientePaso(); }
                        if (DrawText("3. Más de 1 tanque", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.gas = 3; SiguientePaso(); }
                        break;

                    case PASO_RECICLAJE:
                        DrawText("¿Sueles separar o me reciclar tus desechos?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Siempre", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.reciclaje = 1; SiguientePaso(); }
                        if (DrawText("2. Ocasionalmente", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.reciclaje = 2; SiguientePaso(); }
                        if (DrawText("3. Casi nunca", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.reciclaje = 3; SiguientePaso(); }
                        break;

                    case PASO_VUELOS:
                        DrawText("¿Cuántos viajes en avión realizas al año?", 40, 80, 20, colorTexto);
                        if (DrawText("1. Ninguno", 50, 160, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.vuelos = 1; SiguientePaso(); }
                        if (DrawText("2. 1 a 2 vuelos", 50, 220, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.vuelos = 2; SiguientePaso(); }
                        if (DrawText("3. 3 o más vuelos", 50, 280, 18, colorTexto) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { respuestas.vuelos = 3; SiguientePaso(); }
                        break;

                    default:
                        break;
                }

                // Botón para retroceder
                if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){40, 700, 100, 40}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    PasoAnterior();
                }
                DrawRectangleRounded((Rectangle){40, 700, 100, 40}, 0.3f, 4, LIGHTGRAY);
                DrawText("Atrás", 68, 712, 16, DARKGRAY);
                break;

            case PANTALLA_RESULTADO: {
                char saludo[80];
                snprintf(saludo, sizeof(saludo), "%s, tu resultado es:", nombre);

                DrawText("Resultado Estimado", 40, 60, 24, colorTexto);
                DrawText(saludo, 40, 100, 18, DARKGRAY);

                DrawText(TextFormat("%.2f kg CO2 / día", resultado.co2_total_diario), 40, 150, 32, colorBoton);

                DrawText("Desglose aproximado:", 40, 220, 18, colorTexto);
                DrawText(TextFormat("• Transporte: %.1f kg", resultado.co2_transporte), 50, 260, 16, DARKGRAY);
                DrawText(TextFormat("• Electricidad: %.1f kg", resultado.co2_electricidad), 50, 290, 16, DARKGRAY);
                DrawText(TextFormat("• Alimentación: %.1f kg", resultado.co2_carne), 50, 320, 16, DARKGRAY);
                DrawText(TextFormat("• Gas Domiciliario: %.1f kg", resultado.co2_gas), 50, 350, 16, DARKGRAY);
                DrawText(TextFormat("• Impacto Reciclaje: %.1f kg", resultado.co2_reciclaje), 50, 380, 16, DARKGRAY);
                DrawText(TextFormat("• Vuelos: %.1f kg", resultado.co2_vuelos), 50, 410, 16, DARKGRAY);

                if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){40, 680, 340, 50}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    memset(&respuestas, 0, sizeof(respuestas));
                    memset(&resultado, 0, sizeof(resultado));
                    pantallaActual = PANTALLA_INTRO;
                }
                DrawRectangleRounded((Rectangle){40, 680, 340, 50}, 0.3f, 4, colorBoton);
                DrawText("Reiniciar Test", 145, 695, 20, WHITE);
                break;
            }
        }

        EndDrawing();
    }

    UnloadCodepoints(codepoints);
    CloseWindow();
    return 0;
}