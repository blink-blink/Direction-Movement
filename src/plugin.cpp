#include "logger.h"
#include "Events.h"
#include "Settings.h"
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Sink::MenuWatcher::GetSingleton()->Register();
        Sink::UpdateRegisteredHotkeys();
        OARConverterUI::Register();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        if (auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton()) {
            inputDeviceManager->AddEventSink(Sink::InputListener::GetSingleton());
            SKSE::log::info("Listener de input registrado com sucesso!");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
