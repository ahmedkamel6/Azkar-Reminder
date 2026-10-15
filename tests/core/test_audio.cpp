#include "platform/IAudioEngine.h"
#include "core/Settings.h"
int main() {
    dhikr::Settings::Instance().Modify([](dhikr::AppSettings& settings) { settings.soundEnabled = true; });
    auto audio = dhikr::IAudioEngine::Create();
    audio->PlayReminder();
    audio->Stop();
    audio->Shutdown();
    audio->PlayReminder();
    audio->Stop();
}
