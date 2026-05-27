#include <Arduino.h>
#include <TFT_eSPI.h>

// Define the TFT display object
TFT_eSPI tft;

// Define the GameState structure for testing
struct GameState {
    char name[16] = "ALEX";
    uint8_t life = 5;
    uint16_t teamColor = TFT_BLACK;
    bool dirtyName = true;
    bool dirtyLife = true;
    bool dirtyColor = true;
    bool receivedHUD = false; // remains false until first valid HUD update
};

// Function to initialize the TFT display
void renderInit() {
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
}

// Function to render the game state
void render(GameState &st) {
    // If team color changed, redraw full background
    if (st.dirtyColor) {
        tft.fillScreen(st.teamColor);
        st.dirtyColor = false;
        // force name and life redraw on color change
        st.dirtyName = true;
        st.dirtyLife = true;
    }

    // Render the name
    if (st.dirtyName) {
        tft.setTextColor(TFT_WHITE, st.teamColor);
        tft.setTextFont(4); // Font 4 is alphanumeric. Font 6 only has digits and apm!
        int w = tft.textWidth(st.name);
        tft.drawString(st.name, (240 - w) / 2, 38);
        st.dirtyName = false;
    }

    // Render the life
    if (st.dirtyLife) {
        tft.fillRect(0, 90, 240, 30, st.teamColor);
        for (int i = 0; i < 5; i++) {
            uint16_t col = (i < st.life) ? TFT_RED : TFT_DARKGREY;
            tft.fillCircle(30 + i * 40, 105, 10, col);
        }
        st.dirtyLife = false;
    }
}

// Initialize the game state
GameState state;

// Non-blocking line reader for UART
static char s_line[64];
static size_t s_len = 0;
static bool s_lineReady = false;

void linkPoll() {
    while (Serial1.available() && !s_lineReady) {
        int c = Serial1.read();
        if (c == '\r') continue;
        if (c == '\n') {
            s_line[s_len] = '\0';
            s_lineReady = (s_len > 0);
            s_len = 0;
            return;
        }
        if (s_len < sizeof(s_line) - 1) {
            s_line[s_len++] = (char)c;
        } else {
            s_len = 0;  // overflow -> drop this line
        }
    }
}

void parseLine(const char *line, GameState &st) {
    if (!line || !*line || line[0] == '#') return;

    // Accept lines with optional HUD: prefix so debug output can be mixed.
    // Formats accepted after optional prefix:
    // NAME:<text>
    // COLOR:<hexcolor>   e.g. COLOR:#FF0000 or COLOR:FF0000
    // HP:<number>

    // work on a local copy we can modify
    char buf[64];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // strip optional HUD: prefix
    const char *p = buf;
    if (strncmp(p, "HUD:", 4) == 0) p += 4;

    const char *colon = strchr(p, ':');
    if (!colon) return;

    size_t keyLen = colon - p;
    char key[16];
    if (keyLen >= sizeof(key)) return;
    memcpy(key, p, keyLen);
    key[keyLen] = '\0';

    const char *value = colon + 1;

    if (strcasecmp(key, "NAME") == 0) {
        if (strncmp(st.name, value, sizeof(st.name)) != 0) {
            strncpy(st.name, value, sizeof(st.name) - 1);
            st.name[sizeof(st.name) - 1] = '\0';
            st.dirtyName = true;
            st.receivedHUD = true;
        }
    } else if (strcasecmp(key, "COLOR") == 0) {
        // parse hex color (RRGGBB) to 16-bit 565
        const char *s = value;
        // allow optional leading '#'
        if (*s == '#') s++;
        unsigned int r=0,g=0,b=0;
        if (sscanf(s, "%2x%2x%2x", &r, &g, &b) == 3) {
            uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            if (st.teamColor != c) {
                st.teamColor = c;
                st.dirtyColor = true;
                st.receivedHUD = true;
            }
        }
    } else if (strcasecmp(key, "HP") == 0) {
        int v = atoi(value);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        if (st.life != (uint8_t)v) {
            st.life = (uint8_t)v;
            st.dirtyLife = true;
            st.receivedHUD = true;
        }
    } else if (strcasecmp(key, "HIT") == 0) {
        // legacy: decrement
        if (st.life > 0) {
            st.life--;
            st.dirtyLife = true;
            st.receivedHUD = true;
        }
    }
}

void setup() {
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}
    Serial.println("Starting TFT Test...");
#endif

    // Setup UART1 for incoming commands (RX on GPIO 21, TX not used)
    Serial1.begin(115200, SERIAL_8N1, 21, -1);
    Serial1.setRxBufferSize(512);

    // Configure the backlight pin
    pinMode(33, OUTPUT);
    digitalWrite(33, HIGH);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.println("Backlight enabled");
#endif

    // Initialize the TFT display
    renderInit();
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.println("TFT initialized");
#endif

    // Render the initial state
    render(state);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.println("TFT Test Complete!");
#endif
}

void loop() {
    linkPoll();
    if (s_lineReady) {
#if ARDUINO_USB_CDC_ON_BOOT
        Serial.print("Received: ");
        Serial.println(s_line);
#endif
        parseLine(s_line, state);
        s_lineReady = false;
    }

    // Update the display if anything changed
    render(state);
}