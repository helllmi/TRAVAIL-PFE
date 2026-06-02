#include <Arduino.h>
#include "sos_button.h"
#include "config.h"
#include "led.h"

// État debounced du bouton (LOW = appuyé)
static bool buttonState = HIGH;   // état stable après debounce
static bool lastReading = HIGH;   // dernière lecture brute
static uint32_t lastBounceMs = 0; // moment du dernier changement brut

// Compteur de clics dans la fenêtre actuelle
static uint8_t clickCount = 0;
static uint32_t firstClickMs = 0; // timestamp du 1er clic d'une séquence
static uint32_t lastClickMs = 0;  // timestamp du dernier clic
static uint32_t pressStartMs = 0; // début du clic en cours

// Drapeau "triple clic détecté"
static bool tripleClickFlag = false;

// Données du dernier triple clic (pour le JSON)
static uint8_t lastPressCount = 0;
static uint32_t lastPressDuration = 0;

void sosButton_init()
{
    pinMode(PIN_SOS_BUTTON, INPUT_PULLUP);
    Serial.printf("[BTN] Init triple-click detector on GPIO %d\n", PIN_SOS_BUTTON);
    Serial.printf("[BTN] Window=%dms, Debounce=%dms, ClickMax=%dms\n",
                  SOS_TRIPLE_WINDOW_MS, SOS_DEBOUNCE_MS, SOS_CLICK_MAX_MS);
}

// ============================================================================
//  TICK — à appeler depuis loop()
// ============================================================================
void sosButton_tick()
{
    uint32_t now = millis();
    bool reading = digitalRead(PIN_SOS_BUTTON);

    // ── 1) ANTI-REBOND : on attend SOS_DEBOUNCE_MS de stabilité ─────────────
    if (reading != lastReading)
    {
        lastBounceMs = now; // un changement vient d'arriver, on reset le timer
        lastReading = reading;
        return; // on attend la suite, pas encore stable
    }

    // Si on n'a pas encore atteint la stabilité, on attend
    if ((now - lastBounceMs) < SOS_DEBOUNCE_MS)
    {
        return;
    }

    // Si l'état stable n'a pas changé, rien à faire
    if (reading == buttonState)
    {
        return;
    }

    // ── 2) UN CHANGEMENT STABLE A EU LIEU ───────────────────────────────────
    buttonState = reading;

    // ── 3) FRONT DESCENDANT (HIGH→LOW) : appui détecté ──────────────────────
    if (buttonState == LOW)
    {
        pressStartMs = now;

        // Si c'est le tout premier clic, ou si la fenêtre est expirée → nouveau cycle
        if (clickCount == 0 || (now - firstClickMs) > SOS_TRIPLE_WINDOW_MS)
        {
            clickCount = 1;
            firstClickMs = now;
            Serial.println("[BTN] Click 1/3 (start window)");
        }
        else
        {
            clickCount++;
            Serial.printf("[BTN] Click %d/3\n", clickCount);
        }

        lastClickMs = now;
    }

    // ── 4) FRONT MONTANT (LOW→HIGH) : relâchement détecté ────────────────────
    else
    {
        uint32_t pressDuration = now - pressStartMs;

        // Si l'appui a duré trop longtemps, c'est un long press → on ignore
        if (pressDuration > SOS_CLICK_MAX_MS)
        {
            Serial.printf("[BTN] Long press ignored (%dms)\n", pressDuration);
            clickCount = 0; // on reset le compteur
            return;
        }

        // ── 5) TRIPLE CLIC COMPLET ? ────────────────────────────────────────
        if (clickCount >= 3 && (now - firstClickMs) <= SOS_TRIPLE_WINDOW_MS)
        {
            Serial.println("[BTN] *** TRIPLE CLICK DETECTED ***");

            // Capture des stats pour le JSON
            lastPressCount = clickCount;
            lastPressDuration = now - firstClickMs;
            tripleClickFlag = true;

            // Feedback visuel : 3 flashs rouges rapides
            // (en attendant J3 où on déclenchera l'état ACTION)
            blinkLED(LED_ERROR, 3, 100);

            // Reset pour le prochain triple clic
            clickCount = 0;
            firstClickMs = 0;
        }
    }
}

// ============================================================================
//  API DE LECTURE
// ============================================================================
bool sosButton_wasTripleClicked()
{
    if (tripleClickFlag)
    {
        tripleClickFlag = false; // auto-reset (latch consumable)
        return true;
    }
    return false;
}

bool sosButton_isPressed()
{
    return buttonState == LOW;
}

uint8_t sosButton_getPressCount()
{
    return lastPressCount;
}

uint32_t sosButton_getPressDurationMs()
{
    return lastPressDuration;
}