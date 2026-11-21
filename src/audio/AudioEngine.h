#pragma once
#include <windows.h>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace dhikr {

class AudioEngine : public IXAudio2EngineCallback {
public:
    static AudioEngine& Instance();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Play the notification reminder sound (optionally overriding volume 0.0 - 1.0)
    void PlayReminder(float volumeOverride = -1.0f);

    // Stop any playing sound
    void Stop();

    // Reload sound from custom path or default resource
    void ReloadSound();

    // IXAudio2EngineCallback
    STDMETHOD_(void, OnProcessingPassStart)() override {}
    STDMETHOD_(void, OnProcessingPassEnd)() override {}
    STDMETHOD_(void, OnCriticalError)(HRESULT Error) override {
        (void)Error;
        m_deviceLost.store(true);
    }

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

private:
    AudioEngine() = default;
    ~AudioEngine() { Shutdown(); }

    bool InitializeLocked();
    void ShutdownLocked();
    void GenerateDefaultWavLocked();
    const WAVEFORMATEX* WfxPtr() const {
        return m_wfxBuffer.size() >= sizeof(WAVEFORMATEX)
            ? reinterpret_cast<const WAVEFORMATEX*>(m_wfxBuffer.data())
            : nullptr;
    }

    mutable std::mutex      m_audioMutex;
    HINSTANCE               m_hInstance   = nullptr;
    bool                    m_initialized = false;
    std::atomic<bool>       m_deviceLost  { false };

    IXAudio2*               m_pXAudio2     = nullptr;
    IXAudio2MasteringVoice* m_pMasterVoice = nullptr;
    IXAudio2SourceVoice*    m_pSourceVoice = nullptr;

    std::vector<BYTE>       m_audioData;
    std::vector<uint8_t>    m_wfxBuffer;
};

} // namespace dhikr
