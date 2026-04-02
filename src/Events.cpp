#include "Events.h"

void Sink::UpdateRegisteredHotkeys() {

    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    if (controlMap && userEvents) {
        // Get the movement keys mapped by the player on the keyboard
        keyForward = controlMap->GetMappedKey(userEvents->forward, RE::INPUT_DEVICE::kKeyboard);
        keyBack    = controlMap->GetMappedKey(userEvents->back, RE::INPUT_DEVICE::kKeyboard);
        keyLeft    = controlMap->GetMappedKey(userEvents->strafeLeft, RE::INPUT_DEVICE::kKeyboard);
        keyRight   = controlMap->GetMappedKey(userEvents->strafeRight, RE::INPUT_DEVICE::kKeyboard);

        SKSE::log::info("Movement Keys - Forward: {}, Back: {}, Left: {}, Right: {}",
            Sink::keyForward, Sink::keyBack, Sink::keyLeft, Sink::keyRight);
    }
}

// Internal bitmask flags — never exposed to the graph variable directly
enum DirectionFlags : int {
    DIR_NONE    = 0,
    DIR_FORWARD = 1 << 0,  // 1
    DIR_BACK    = 1 << 1,  // 2
    DIR_LEFT    = 1 << 2,  // 4
    DIR_RIGHT   = 1 << 3,  // 8
};

// Maps all 16 bitmask combinations to the original int representation
static int MapToOriginalState(int flags)
{
    bool f = flags & DIR_FORWARD;
    bool b = flags & DIR_BACK;
    bool l = flags & DIR_LEFT;
    bool r = flags & DIR_RIGHT;

    // 3 keys simultaneously
    if (l && f && r && !b) return 11;  // Left+Forward+Right
    if (b && f && r && !l) return 12;  // Back+Forward+Right
    if (l && b && r && !f) return 13;  // Left+Back+Right
    if (b && f && l && !r) return 14;  // Back+Forward+Left

    // 2 opposing keys
    if (f && b && !l && !r) return 9;   // Forward+Back
    if (l && r && !f && !b) return 10;  // Left+Right

    // Clean diagonals (exactly 2 keys, non-opposing)
    if (f && r && !b && !l) return 2;  // Forward-Right
    if (b && r && !f && !l) return 4;  // Backward-Right
    if (b && l && !f && !r) return 6;  // Backward-Left
    if (f && l && !b && !r) return 8;  // Forward-Left

    // Cardinals — forward/back take priority over left/right when 3 keys held
    if (f && !b) return 1;  // Forward (covers F+L+R, F+R cancelled by L, etc.)
    if (b && !f) return 5;  // Backward
    if (r && !l) return 3;  // Right
    if (l && !r) return 7;  // Left

    // Fully cancelled (F+B, L+R, or all 4 held) → Idle
    return 0;
}

RE::BSEventNotifyControl Sink::InputListener::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    bool keyChanged = false;

    // FIX: Iterate through ALL chained events before acting.
    // Previously the return was inside this loop, causing early exit
    // after the first event and missing key-up events (stuck key bug).
    for (auto* event = *a_event; event; event = event->next) {

        RE::INPUT_DEVICE device = event->GetDevice();

        // --- MOVEMENT LOGIC (KEYBOARD AND CONTROLLER) ---

        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
            auto* thumbstick = event->AsThumbstickEvent();

            if (thumbstick && thumbstick->IsLeft()) {
                // Normalize values to avoid small analog stick fluctuations
                bool new_c_up    = thumbstick->yValue > 0.5f;
                bool new_c_down  = thumbstick->yValue < -0.5f;
                bool new_c_left  = thumbstick->xValue < -0.5f;
                bool new_c_right = thumbstick->xValue > 0.5f;

                if (c_up != new_c_up || c_down != new_c_down || c_left != new_c_left || c_right != new_c_right) {
                    c_up = new_c_up;
                    c_down = new_c_down;
                    c_left = new_c_left;
                    c_right = new_c_right;
                    keyChanged = true;
                }
            }
            else if (thumbstick && thumbstick->IsRight()) {
                bool new_rs_up    = thumbstick->yValue > 0.5f;
                bool new_rs_down  = thumbstick->yValue < -0.5f;
                bool new_rs_left  = thumbstick->xValue < -0.5f;
                bool new_rs_right = thumbstick->xValue > 0.5f;

                if (rs_up != new_rs_up || rs_down != new_rs_down || rs_left != new_rs_left || rs_right != new_rs_right) {
                    rs_up = new_rs_up; rs_down = new_rs_down; rs_left = new_rs_left; rs_right = new_rs_right;
                    keyChanged = true;
                }
            }
        }
        else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
            auto* button = event->AsButtonEvent();
            const uint32_t scanCode = button->GetIDCode();

            if (scanCode == keyForward) {
                if (button->IsDown() && !w_pressed) { w_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && w_pressed) { w_pressed = false; keyChanged = true; }
            }
            else if (scanCode == keyLeft) {
                if (button->IsDown() && !a_pressed) { a_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && a_pressed) { a_pressed = false; keyChanged = true; }
            }
            else if (scanCode == keyBack) {
                if (button->IsDown() && !s_pressed) { s_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && s_pressed) { s_pressed = false; keyChanged = true; }
            }
            else if (scanCode == keyRight) {
                if (button->IsDown() && !d_pressed) { d_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && d_pressed) { d_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x2A) { // Left Shift
                if (button->IsDown() && !ls_pressed) { ls_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && ls_pressed) { ls_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x10) { // Q
                if (button->IsDown() && !q_pressed) { q_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && q_pressed) { q_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x12) { // E
                if (button->IsDown() && !e_pressed) { e_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && e_pressed) { e_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x38) { // Left Alt
                if (button->IsDown() && !la_pressed) { la_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && la_pressed) { la_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x2C) { // Z
                if (button->IsDown() && !z_pressed) { z_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && z_pressed) { z_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x2D) { // X
                if (button->IsDown() && !x_pressed) { x_pressed = true;  keyChanged = true; }
                else if (button->IsUp() && x_pressed) { x_pressed = false; keyChanged = true; }
            }
        }
    }
    // FIX: UpdateDirectionalState called once after ALL events in the batch are
    // processed, not once per event. Prevents redundant graph variable writes.
    if (keyChanged) {
        UpdateDirectionalState();
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::InputListener::UpdateDirectionalState()
{
    // Prioritize keyboard input. If any WASD key is pressed, ignore the controller.
    bool FRONT     = w_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_up);
    bool BACK      = s_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_down);
    bool LEFT_DIR  = a_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_left);
    bool RIGHT_DIR = d_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_right);

    bool out_ls = ls_pressed || rs_left;
    bool out_la = la_pressed || rs_right;
    bool out_q  = q_pressed  || rs_down;
    bool out_e  = e_pressed  || rs_up;
    bool out_z  = z_pressed;
    bool out_x  = x_pressed;

    int previousDirectionalState = directionalState;

    // Build internal bitmask from current input state
    int flags = DIR_NONE;
    if (FRONT)     flags |= DIR_FORWARD;
    if (BACK)      flags |= DIR_BACK;
    if (LEFT_DIR)  flags |= DIR_LEFT;
    if (RIGHT_DIR) flags |= DIR_RIGHT;

    directionalState = MapToOriginalState(flags);

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->SetGraphVariableInt("DirecionalCycleMoveset", directionalState);
        player->SetGraphVariableBool("DMKLeftShift", out_ls);
        player->SetGraphVariableBool("DMKLeftAlt", out_la);
        player->SetGraphVariableBool("DMKQ", out_q);
        player->SetGraphVariableBool("DMKE", out_e);
        player->SetGraphVariableBool("DMKZ", out_z);
        player->SetGraphVariableBool("DMKX", out_x);
    }
}

RE::BSEventNotifyControl Sink::MenuWatcher::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
    if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME) {
        Sink::UpdateRegisteredHotkeys();
    }

    return RE::BSEventNotifyControl::kContinue;
}
