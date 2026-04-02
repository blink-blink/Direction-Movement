#include "Events.h"

void Sink::UpdateRegisteredHotkeys() {
    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    if (controlMap && userEvents) {
        // Get the movement keys mapped by the player on the keyboard
        keyForward = controlMap->GetMappedKey(userEvents->forward, RE::INPUT_DEVICE::kKeyboard);
        keyBack = controlMap->GetMappedKey(userEvents->back, RE::INPUT_DEVICE::kKeyboard);
        keyLeft = controlMap->GetMappedKey(userEvents->strafeLeft, RE::INPUT_DEVICE::kKeyboard);
        keyRight = controlMap->GetMappedKey(userEvents->strafeRight, RE::INPUT_DEVICE::kKeyboard);

        // Optional: Log the keys to confirm we captured the correct scan codes
        SKSE::log::info("Movement Keys - Forward: {}, Back: {}, Left: {}, Right: {}",
            Sink::keyForward, Sink::keyBack, Sink::keyLeft, Sink::keyRight);
    }

}

RE::BSEventNotifyControl Sink::InputListener::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    bool keyChanged = false;

    for (auto* event = *a_event; event; event = event->next) {
        RE::INPUT_DEVICE device = event->GetDevice();

        // --- MOVEMENT LOGIC (KEYBOARD AND CONTROLLER) ---
        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
            auto* thumbstick = event->AsThumbstickEvent();
            if (thumbstick && thumbstick->IsLeft()) {
                // Normalize values to avoid small analog stick fluctuations
                bool new_c_up = thumbstick->yValue > 0.5f;
                bool new_c_down = thumbstick->yValue < -0.5f;
                bool new_c_left = thumbstick->xValue < -0.5f;
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
                bool new_rs_up = thumbstick->yValue > 0.5f;
                bool new_rs_down = thumbstick->yValue < -0.5f;
                bool new_rs_left = thumbstick->xValue < -0.5f;
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

            // Strict state-machine logic for each key
            if (scanCode == keyForward) {
                // Only change to 'pressed' if the key is DOWN and our current state is 'released'.
                if (button->IsDown() && !w_pressed) {
                    w_pressed = true;
                    keyChanged = true;
                }
                // Only change to 'released' if the key is UP and our current state is 'pressed'.
                else if (button->IsUp() && w_pressed) {
                    w_pressed = false;
                    keyChanged = true;
                }
            }
            else if (scanCode == keyLeft) {
                if (button->IsDown() && !a_pressed) {
                    a_pressed = true;
                    keyChanged = true;
                }
                else if (button->IsUp() && a_pressed) {
                    a_pressed = false;
                    keyChanged = true;
                }
            }
            else if (scanCode == keyBack) {
                if (button->IsDown() && !s_pressed) {
                    s_pressed = true;
                    keyChanged = true;
                }
                else if (button->IsUp() && s_pressed) {
                    s_pressed = false;
                    keyChanged = true;
                }
            }
            else if (scanCode == keyRight) {
                if (button->IsDown() && !d_pressed) {
                    d_pressed = true;
                    keyChanged = true;
                }
                else if (button->IsUp() && d_pressed) {
                    d_pressed = false;
                    keyChanged = true;
                }
            }
            else if (scanCode == 0x2A) { // Left Shift
                if (button->IsDown() && !ls_pressed) { ls_pressed = true; keyChanged = true; }
                else if (button->IsUp() && ls_pressed) { ls_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x10) { // Q
                if (button->IsDown() && !q_pressed) { q_pressed = true; keyChanged = true; }
                else if (button->IsUp() && q_pressed) { q_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x12) { // E
                if (button->IsDown() && !e_pressed) { e_pressed = true; keyChanged = true; }
                else if (button->IsUp() && e_pressed) { e_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x38) { // Left Alt
                if (button->IsDown() && !la_pressed) { la_pressed = true; keyChanged = true; }
                else if (button->IsUp() && la_pressed) { la_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x2C) { // Z
                if (button->IsDown() && !z_pressed) { z_pressed = true; keyChanged = true; }
                else if (button->IsUp() && z_pressed) { z_pressed = false; keyChanged = true; }
            }
            else if (scanCode == 0x2D) { // X
                if (button->IsDown() && !x_pressed) { x_pressed = true; keyChanged = true; }
                else if (button->IsUp() && x_pressed) { x_pressed = false; keyChanged = true; }
            }


        }
        int previousDirectionalState = directionalState;
        // Only recalculate direction if one of our movement keys actually changed state.
        if (keyChanged) {
            UpdateDirectionalState();
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}

void Sink::InputListener::UpdateDirectionalState()
{
    //static int DirecionalCycleMoveset = 0;
    int previousDirectionalState = directionalState;

    // Prioritize keyboard input. If any WASD key is pressed, ignore the controller.
    // Otherwise, use the controller state.
    bool FRONT = w_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_up);
    bool BACK = s_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_down);
    bool LEFT_DIR = a_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_left);
    bool RIGHT_DIR = d_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_right);

    bool out_ls = ls_pressed || rs_left; 
    bool out_la = la_pressed || rs_right; 
    bool out_q = q_pressed || rs_down; 
    bool out_e = e_pressed || rs_up; 
    bool out_z = z_pressed;  
    bool out_x = x_pressed;  

    // Decision logic remains the same but now uses the combined variables
    // 1st Priority: 3 keys pressed simultaneously
    if (LEFT_DIR && FRONT && RIGHT_DIR) {
        directionalState = 11;
    }
    else if (BACK && FRONT && RIGHT_DIR) {
        directionalState = 12;
    }
    else if (LEFT_DIR && BACK && RIGHT_DIR) {
        directionalState = 13;
    }
    else if (BACK && FRONT && LEFT_DIR) {
        directionalState = 14; 
    }
    // 2nd Priority: 2 OPPOSITE keys pressed simultaneously
    else if (FRONT && BACK) {
        directionalState = 9;
    }
    else if (LEFT_DIR && RIGHT_DIR) {
        directionalState = 10;
    }

    else if (FRONT && LEFT_DIR) {
        directionalState = 8;  // front left
    }
    else if (FRONT && RIGHT_DIR) {
        directionalState = 2;  // front right
    }
    else if (BACK && LEFT_DIR) {
        directionalState = 6;  // back left
    }
    else if (BACK && RIGHT_DIR) {
        directionalState = 4;  // back right
    }
    else if (FRONT) {
        directionalState = 1;  // front
    }
    else if (LEFT_DIR) {
        directionalState = 7;  // left
    }
    else if (BACK) {
        directionalState = 5;  // back
    }
    else if (RIGHT_DIR) {
        directionalState = 3;  // right
    }
    else {
        directionalState = 0;  // Idle
    }

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
