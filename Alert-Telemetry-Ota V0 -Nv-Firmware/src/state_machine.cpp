#include <Arduino.h>
#include "state_machine.h"
#include "led.h"
#include "power_manager.h"

// ============================================================================
//  ÉTAT INTERNE
// ============================================================================
static DeviceState currentState = STATE_OFF;
static uint32_t stateEnteredAt = 0;
static StateChangeCallback stateChangeCb = nullptr;

// ============================================================================
//  ACTIONS À CHAQUE TRANSITION
// ============================================================================
// Appelée à l'entrée d'un nouvel état pour faire le setup propre à cet état.
static void onEnterState(DeviceState newState)
{
    switch (newState)
    {
    case STATE_OFF:
        setLED(LED_STARTUP);
        break;

    case STATE_STANDBY:
        setLED(LED_MQTT_OK); // vert sombre = prêt, en veille
        Serial.println("[FSM] >>> Entering STANDBY - waiting for SOS");
        power_enterstandby();
        break;

    case STATE_ACTION:
        setLED(LED_ERROR); // rouge = alerte active
        Serial.println("[FSM] >>> Entering ACTION - SOS triggered, alerts enabled");
        power_enteraction();
        break;
    }
}

// ============================================================================
//  TRANSITION CENTRALE
// ============================================================================
// Toute transition d'état passe OBLIGATOIREMENT par cette fonction.
// Elle log, met à jour le timestamp, et appelle onEnterState() pour les actions.
static void transition(DeviceState newState)
{
    if (newState == currentState)
        return; // no-op si déjà dans cet état

    Serial.printf("[FSM] Transition: %s -> %s\n",
                  stateMachine_stateName(currentState),
                  stateMachine_stateName(newState));

    currentState = newState;
    stateEnteredAt = millis();

    onEnterState(newState);
    // Notifier l'extérieur (main.cpp) du changement d'état    
    if (stateChangeCb != nullptr)
    {
        stateChangeCb(newState);
    }
}

// ============================================================================
//  INIT
// ============================================================================
void stateMachine_init()
{
    Serial.println("[FSM] Init - starting in STATE_OFF");
    currentState = STATE_OFF;
    stateEnteredAt = millis();
}

// ============================================================================
//  DISPATCH D'ÉVÉNEMENTS
// ============================================================================
// Les transitions ne sont autorisées que selon l'état courant.
// Un événement reçu dans un état où il n'a pas de sens est IGNORÉ (et loggé).
void stateMachine_dispatch(DeviceEvent evt)
{
    Serial.printf("[FSM] Event received: %d in state %s\n",
                  evt, stateMachine_stateName(currentState));

    switch (currentState)
    {

    // ── État OFF : on n'accepte que EVT_BOOT_OK ─────────────────────────
    case STATE_OFF:
        if (evt == EVT_BOOT_OK)
        {
            transition(STATE_STANDBY);
        }
        else
        {
            Serial.println("[FSM] Event ignored in OFF state");
        }
        break;

    // ── État STANDBY : on n'accepte que EVT_SOS_TRIGGERED ───────────────
    case STATE_STANDBY:
        if (evt == EVT_SOS_TRIGGERED)
        {
            transition(STATE_ACTION);
        }
        else
        {
            Serial.println("[FSM] Event ignored in STANDBY state");
        }
        break;

    // ── État ACTION : on n'accepte que EVT_USER_RESET ───────────────────
    case STATE_ACTION:
        if (evt == EVT_USER_RESET)
        {
            transition(STATE_STANDBY);
        }
        else
        {
            Serial.println("[FSM] Event ignored in ACTION state");
        }
        break;
    }
}

DeviceState stateMachine_getState()
{
    return currentState;
}

uint32_t stateMachine_getStateUptime()
{
    return millis() - stateEnteredAt;
}

const char *stateMachine_stateName(DeviceState s)
{
    switch (s)
    {
    case STATE_OFF:
        return "OFF";
    case STATE_STANDBY:
        return "STANDBY";
    case STATE_ACTION:
        return "ACTION";
    default:
        return "UNKNOWN";
    }
}
void stateMachine_onStateChange(StateChangeCallback cb) {
    stateChangeCb = cb;
}