#include <Arduino.h>
//version final
#include <M5Cardputer.h>
#include <TOTP.h>
#include <WiFi.h>
#include "time.h"
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>

USBHIDKeyboard Keyboard;
Preferences prefs;

// --- CONFIGURACIÓN DE PANTALLA Y ENERGÍA ---
unsigned long lastActivity = 0;
int currentBrightness = 100; // Brillo normal (0 a 255)
const unsigned long TIMEOUT_SLEEP = 30000; // 30 segundos en milisegundos

struct Account {
    String name;
    String secret;
};

Account myAccounts[50]; 
int totalAccounts = 0;
int currentIndex = 0;
bool viewMode = false;
String lastToken = "";

// --- DECODIFICADOR BASE32 ---
int base32_decode(const char *encoded, uint8_t *result) {
    int buffer = 0, bitsLeft = 0, count = 0;
    for (const char *p = encoded; *p; p++) {
        uint8_t val;
        char c = toupper(*p);
        if (c == ' ' || c == '=') continue; 
        if (c >= 'A' && c <= 'Z') val = c - 'A';
        else if (c >= '2' && c <= '7') val = c - '2' + 26;
        else continue;
        
        buffer <<= 5; buffer |= val; bitsLeft += 5;
        if (bitsLeft >= 8) {
            result[count++] = buffer >> (bitsLeft - 8);
            bitsLeft -= 8;
        }
    }
    return count;
}

// --- GESTIÓN DE ENERGÍA ---
void esperarSuelte() {
    while (M5Cardputer.Keyboard.isPressed()) {
        M5Cardputer.update();
        delay(10);
    }
    delay(50);
}

void awakeScreen() {
    if (currentBrightness == 0) {
        currentBrightness = 100;
        M5Cardputer.Display.setBrightness(currentBrightness);
        esperarSuelte(); // Absorbe la pulsación para que no haga ninguna acción
    }
    lastActivity = millis();
}

void checkSleep() {
    if (currentBrightness > 0 && (millis() - lastActivity > TIMEOUT_SLEEP)) {
        currentBrightness = 0;
        M5Cardputer.Display.setBrightness(currentBrightness);
    }
}

// --- ENTRADA DE TEXTO MULTIUSO ---
String inputText(String title) {
    String txt = "";
    M5Cardputer.Display.fillScreen(BLACK);
    awakeScreen();
    
    while(true) {
        M5Cardputer.update();
        checkSleep();
        
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(10, 5);
        M5Cardputer.Display.setTextColor(YELLOW, BLACK);
        M5Cardputer.Display.println(title);
        
        M5Cardputer.Display.drawRect(5, 30, 230, 80, WHITE);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(10, 40);
        M5Cardputer.Display.setTextColor(WHITE, BLACK);
        
        if (txt.length() > 25) {
             M5Cardputer.Display.print(txt.substring(0, 25));
             M5Cardputer.Display.setCursor(10, 55);
             M5Cardputer.Display.print(txt.substring(25) + "_");
        } else {
             M5Cardputer.Display.print(txt + "_");
        }

        if (M5Cardputer.Keyboard.isPressed()) {
            if (currentBrightness == 0) { awakeScreen(); continue; }
            awakeScreen();
            
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            
            if (status.enter || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.BtnA.isPressed()) { 
                esperarSuelte();
                return txt;
            }
            
            if (status.del || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) || M5Cardputer.Keyboard.isKeyPressed(0x08)) { 
                if (txt.length() > 0) {
                    txt.remove(txt.length() - 1);
                    M5Cardputer.Display.fillRect(6, 31, 228, 78, BLACK); 
                }
                delay(150); 
            }
            
            for (auto c : status.word) {
                if (c >= 32 && c <= 126 && txt.length() < 60) {
                    txt += c;
                    delay(150);
                }
            }
        }
    }
}

// --- FUNCIÓN BASE PARA PEDIR UN PIN ---
String pedirPIN(String titulo) {
    String ingreso = "";
    M5Cardputer.Display.fillScreen(BLACK);
    awakeScreen();
    
    while (true) {
        M5Cardputer.update();
        checkSleep();
        
        M5Cardputer.Display.setCursor(10, 20);
        M5Cardputer.Display.setTextColor(WHITE, BLACK);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.println(titulo + "      "); 
        
        M5Cardputer.Display.fillRect(40, 60, 160, 40, DARKGREY);
        M5Cardputer.Display.setCursor(60, 70);
        M5Cardputer.Display.setTextSize(3);
        
        String mascara = "";
        for(int i=0; i<ingreso.length(); i++) mascara += "*";
        M5Cardputer.Display.print(mascara);

        if (M5Cardputer.Keyboard.isPressed()) {
            if (currentBrightness == 0) { awakeScreen(); continue; }
            awakeScreen();
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            
            if (status.del || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) || M5Cardputer.Keyboard.isKeyPressed(0x08)) {
                if(ingreso.length() > 0) {
                    ingreso.remove(ingreso.length() - 1);
                    M5Cardputer.Display.fillRect(40, 60, 160, 40, DARKGREY); // Limpiar zona
                }
                delay(200);
            }
            
            for (auto c : status.word) {
                if (isdigit(c) && ingreso.length() < 4) {
                    ingreso += c;
                    delay(200);
                }
            }
        }
        
        if (ingreso.length() == 4) {
            esperarSuelte();
            return ingreso;
        }
    }
}

// --- RUTINA PRINCIPAL DE SEGURIDAD (FÁBRICA VS NORMAL) ---
void rutinaPIN() {
    prefs.begin("totp_v6", false);
    String pinGuardado = prefs.getString("master_pin", "");
    prefs.end();

    bool isDefault = false;
    String pinActual = pinGuardado;

    // Si no hay PIN guardado, es el primer uso
    if (pinActual == "") {
        pinActual = "1234";
        isDefault = true;
    }

    // 1. Pedir el PIN actual para entrar
    while (true) {
        String intento = pedirPIN("INGRESE PIN:");
        if (intento == pinActual) {
            M5Cardputer.Display.fillScreen(GREEN);
            M5Cardputer.Display.setTextColor(BLACK);
            M5Cardputer.Display.setCursor(60, 50);
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.print("ACCESO OK");
            delay(800);
            break;
        } else {
            M5Cardputer.Display.fillScreen(BLACK);
            M5Cardputer.Display.setTextColor(RED);
            M5Cardputer.Display.setCursor(40, 60);
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.println("PIN ERROR");
            delay(1500);
        }
    }

    // 2. Si usamos el PIN por defecto "1234", forzar el cambio
    if (isDefault) {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setTextColor(CYAN);
        M5Cardputer.Display.setCursor(10, 40);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.println("CONFIGURACION");
        M5Cardputer.Display.println("DE FABRICA");
        delay(2000);
        
        while(true) {
            String nuevo = pedirPIN("CREAR PIN:");
            String conf = pedirPIN("CONFIRMAR:");
            
            if (nuevo == conf) {
                prefs.begin("totp_v6", false);
                prefs.putString("master_pin", nuevo);
                prefs.end();
                
                M5Cardputer.Display.fillScreen(GREEN);
                M5Cardputer.Display.setTextColor(BLACK);
                M5Cardputer.Display.setCursor(40, 50);
                M5Cardputer.Display.setTextSize(2);
                M5Cardputer.Display.print("PIN GUARDADO");
                delay(1500);
                break;
            } else {
                M5Cardputer.Display.fillScreen(BLACK);
                M5Cardputer.Display.setTextColor(RED);
                M5Cardputer.Display.setCursor(20, 60);
                M5Cardputer.Display.setTextSize(2);
                M5Cardputer.Display.println("NO COINCIDEN");
                delay(1500);
            }
        }
    }
}

// --- MEMORIA ---
void guardarTodo() {
    prefs.begin("totp_v6", false);
    prefs.putInt("count", totalAccounts);
    for (int i = 0; i < totalAccounts; i++) {
        prefs.putString(("n" + String(i)).c_str(), myAccounts[i].name);
        prefs.putString(("s" + String(i)).c_str(), myAccounts[i].secret);
    }
    prefs.end();
}

void cargarTodo() {
    prefs.begin("totp_v6", true);
    totalAccounts = prefs.getInt("count", 0);
    if (totalAccounts > 50) totalAccounts = 0;
    for (int i = 0; i < totalAccounts; i++) {
        myAccounts[i].name = prefs.getString(("n" + String(i)).c_str(), "Error");
        myAccounts[i].secret = prefs.getString(("s" + String(i)).c_str(), "");
    }
    prefs.end();
}

// --- SINCRONIZAR HORA EN UTC PURO ---
void sincronizarHora() {
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.setTextColor(GREEN);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.println("Wi-Fi Conectado!");
    
    M5Cardputer.Display.setCursor(10, 70);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.println("Sincronizando reloj NTP (UTC)...");
    
    configTime(0, 0, "pool.ntp.org"); 
    struct tm ti; 
    if(getLocalTime(&ti, 5000)) { 
        M5.Rtc.setDateTime(&ti); 
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
}

// --- GESTOR DE WIFI Y ESCANER ---
void conectarWiFi() {
    prefs.begin("totp_v6", false);
    String savedSSID = prefs.getString("wifi_ssid", "");
    String savedPass = prefs.getString("wifi_pass", "");
    WiFi.mode(WIFI_STA);
    
    if (savedSSID != "") {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setTextColor(CYAN);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(10, 20);
        M5Cardputer.Display.println("Conectando a:");
        M5Cardputer.Display.setTextColor(WHITE);
        M5Cardputer.Display.setCursor(10, 50);
        M5Cardputer.Display.println(savedSSID);
        
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        int tries = 0;
        M5Cardputer.Display.setCursor(10, 80);
        while (WiFi.status() != WL_CONNECTED && tries < 15) { 
            delay(500);
            M5Cardputer.Display.print(".");
            tries++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            sincronizarHora();
            prefs.end();
            return; 
        }
    }

    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextColor(YELLOW);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.println("Escaneando...");
    
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setTextColor(RED);
        M5Cardputer.Display.setCursor(10, 50);
        M5Cardputer.Display.println("No se encontraron redes");
        delay(2000);
        prefs.end();
        return;
    }

    int currentWifi = 0;
    bool exitWifiMenu = false;
    awakeScreen();

    while (!exitWifiMenu) {
        M5Cardputer.update();
        checkSleep();
        
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(10, 5);
        M5Cardputer.Display.setTextColor(CYAN);
        M5Cardputer.Display.println("SELECCIONA WI-FI");

        int startIdx = currentWifi > 3 ? currentWifi - 3 : 0;
        for (int i = 0; i < min(4, n - startIdx); i++) {
            int actualIdx = startIdx + i;
            if (actualIdx == currentWifi) {
                M5Cardputer.Display.fillRect(0, 35 + (i * 20), 240, 20, BLUE);
                M5Cardputer.Display.setTextColor(WHITE);
            } else {
                M5Cardputer.Display.setTextColor(LIGHTGREY);
            }
            M5Cardputer.Display.setCursor(10, 38 + (i * 20));
            M5Cardputer.Display.setTextSize(1);
            
            String ssidStr = WiFi.SSID(actualIdx);
            if (ssidStr.length() > 25) ssidStr = ssidStr.substring(0, 25);
            String candado = (WiFi.encryptionType(actualIdx) == WIFI_AUTH_OPEN) ? " " : " *";
            M5Cardputer.Display.print((actualIdx == currentWifi ? "> " : "  ") + ssidStr + candado);
        }

        M5Cardputer.Display.setTextColor(LIGHTGREY);
        M5Cardputer.Display.setCursor(5, 120);
        M5Cardputer.Display.print(";/.: Mover | OK: Elegir | DEL: Saltar");

        bool selectionMade = false;

        while (!selectionMade && !exitWifiMenu) {
            M5Cardputer.update();
            checkSleep();
            
            if (M5Cardputer.Keyboard.isPressed()) {
                if (currentBrightness == 0) { awakeScreen(); continue; }
                awakeScreen();
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                
                if (M5Cardputer.Keyboard.isKeyPressed(';')) { currentWifi--; if(currentWifi < 0) currentWifi = n-1; selectionMade = true; delay(150); }
                if (M5Cardputer.Keyboard.isKeyPressed('.')) { currentWifi++; if(currentWifi >= n) currentWifi = 0; selectionMade = true; delay(150); }
                
                if (status.del || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                    esperarSuelte();
                    exitWifiMenu = true;
                }
                
                if (status.enter || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                    esperarSuelte();
                    String selectedSSID = WiFi.SSID(currentWifi);
                    String pass = "";
                    
                    if (WiFi.encryptionType(currentWifi) != WIFI_AUTH_OPEN) {
                        pass = inputText("Clave para: " + selectedSSID.substring(0, 10));
                    }
                    
                    M5Cardputer.Display.fillScreen(BLACK);
                    M5Cardputer.Display.setTextColor(YELLOW);
                    M5Cardputer.Display.setCursor(10, 40);
                    M5Cardputer.Display.print("Conectando...");
                    
                    WiFi.begin(selectedSSID.c_str(), pass.c_str());
                    int tries = 0;
                    while (WiFi.status() != WL_CONNECTED && tries < 20) {
                        delay(500);
                        M5Cardputer.Display.print(".");
                        tries++;
                    }
                    
                    if (WiFi.status() == WL_CONNECTED) {
                        prefs.putString("wifi_ssid", selectedSSID);
                        prefs.putString("wifi_pass", pass);
                        sincronizarHora();
                        exitWifiMenu = true; 
                    } else {
                        M5Cardputer.Display.fillScreen(BLACK);
                        M5Cardputer.Display.setTextColor(RED);
                        M5Cardputer.Display.setTextSize(2);
                        M5Cardputer.Display.setCursor(10, 50);
                        M5Cardputer.Display.println("CLAVE INCORRECTA");
                        delay(2000);
                        selectionMade = true; 
                    }
                }
            }
        }
    }
    prefs.end();
}

void mostrarMenu() {
    viewMode = false;
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(10, 5);
    M5Cardputer.Display.setTextColor(CYAN);
    M5Cardputer.Display.println("MIS CUENTAS");

    if (totalAccounts == 0) {
        M5Cardputer.Display.setTextColor(DARKGREY);
        M5Cardputer.Display.setCursor(20, 50);
        M5Cardputer.Display.println("(Vacio) Pulsa N");
    } else {
        if (currentIndex >= totalAccounts) currentIndex = 0;
        if (currentIndex < 0) currentIndex = totalAccounts - 1;

        for (int i = 0; i < totalAccounts; i++) {
             if (i == currentIndex) {
                M5Cardputer.Display.fillRect(0, 35, 240, 30, BLUE);
                M5Cardputer.Display.setTextColor(WHITE);
                M5Cardputer.Display.setCursor(10, 42);
                M5Cardputer.Display.print("> " + myAccounts[i].name);
             }
        }
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(LIGHTGREY);
        M5Cardputer.Display.setCursor(200, 5);
        M5Cardputer.Display.print(String(currentIndex+1) + "/" + String(totalAccounts));
    }
    
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(LIGHTGREY);
    M5Cardputer.Display.setCursor(5, 120);
    M5Cardputer.Display.print("N: Nuevo | DEL: Borrar | OK: Ver");
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    Keyboard.begin();
    USB.begin();
    
    // Encender pantalla al inicio
    M5Cardputer.Display.setBrightness(currentBrightness);
    lastActivity = millis();
    
    rutinaPIN(); // 1. Pedir y/o crear PIN
    cargarTodo(); // 2. Cargar llaves
    conectarWiFi(); // 3. Buscar red
    
    mostrarMenu(); // 4. Iniciar App
}

void loop() {
    M5Cardputer.update();
    checkSleep(); // Vigilar el tiempo para apagar la pantalla

    if (M5Cardputer.Keyboard.isPressed()) {
        // Si la pantalla está apagada, la encendemos y saltamos este bucle
        if (currentBrightness == 0) {
            awakeScreen();
            return; 
        }
        awakeScreen();
        
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

        if (!viewMode && M5Cardputer.Keyboard.isKeyPressed('n')) {
            esperarSuelte(); 
            String n = inputText("Nombre (App/Mail):");
            if (n.length() > 0) {
                String s = inputText("Pegar Key Base32:");
                if (s.length() > 0) {
                    s.replace(" ", ""); 
                    myAccounts[totalAccounts] = {n, s};
                    totalAccounts++;
                    guardarTodo();
                    M5Cardputer.Display.fillScreen(GREEN);
                    delay(500);
                }
            }
            mostrarMenu();
        }

        if (!viewMode && totalAccounts > 0 && (status.del || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) || M5Cardputer.Keyboard.isKeyPressed(0x08))) {
            for (int i = currentIndex; i < totalAccounts - 1; i++) myAccounts[i] = myAccounts[i+1];
            totalAccounts--;
            if (currentIndex >= totalAccounts) currentIndex = 0;
            guardarTodo();
            mostrarMenu();
            esperarSuelte();
        }

        if (!viewMode && totalAccounts > 0) {
            if (M5Cardputer.Keyboard.isKeyPressed(';')) { currentIndex--; mostrarMenu(); delay(150); }
            if (M5Cardputer.Keyboard.isKeyPressed('.')) { currentIndex++; mostrarMenu(); delay(150); }
        }

        if (totalAccounts > 0 && (status.enter || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed(0x0D))) {
            viewMode = !viewMode;
            lastToken = "";
            if (!viewMode) mostrarMenu();
            else M5Cardputer.Display.fillScreen(BLACK);
            esperarSuelte(); 
        }

        if (viewMode && (status.space || M5Cardputer.Keyboard.isKeyPressed(' '))) {
             if (lastToken.length() == 6) {
                Keyboard.print(lastToken);
                M5Cardputer.Display.fillRect(0, 110, 240, 20, GREEN);
                delay(200);
            }
        }
    }

    if (viewMode && totalAccounts > 0) {
        auto dt = M5.Rtc.getDateTime();
        struct tm t;
        t.tm_year = dt.date.year - 1900; t.tm_mon = dt.date.month - 1; t.tm_mday = dt.date.date;
        t.tm_hour = dt.time.hours; t.tm_min = dt.time.minutes; t.tm_sec = dt.time.seconds;
        
        setenv("TZ", "UTC0", 1);
        tzset();
        time_t now = mktime(&t);

        uint8_t decodedKey[64];
        int keyLen = base32_decode(myAccounts[currentIndex].secret.c_str(), decodedKey);
        TOTP totp = TOTP(decodedKey, keyLen);
        String newToken = String(totp.getCode(now));
        while(newToken.length() < 6) newToken = "0" + newToken;

        if (newToken != lastToken) {
            lastToken = newToken;
            M5Cardputer.Display.fillScreen(BLACK);
            M5Cardputer.Display.setTextColor(YELLOW);
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setCursor(10, 10);
            M5Cardputer.Display.print(myAccounts[currentIndex].name);
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.setTextSize(6);
            M5Cardputer.Display.setCursor(15, 45);
            M5Cardputer.Display.print(lastToken);
        }
        
        int s = dt.time.seconds % 30;
        int w = map(s, 0, 30, 200, 0);
        M5Cardputer.Display.drawRect(20, 105, 200, 6, WHITE);
        M5Cardputer.Display.fillRect(21, 106, 198, 4, BLACK);
        M5Cardputer.Display.fillRect(21, 106, w, 4, CYAN);
        delay(50);
    }
}