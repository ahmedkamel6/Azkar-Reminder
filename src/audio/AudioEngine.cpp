#include "AudioEngine.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include "../core/Application.h"
#include "../resource.h"
#include <cmath>
#include <filesystem>
#include <shlwapi.h>

namespace dhikr {

AudioEngine& AudioEngine::Instance() {
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_WARN("AudioEngine: CoInitializeEx failed");
        return false;
    }

    // Initialize XAudio2
    hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        LOG_WARN("AudioEngine: XAudio2Create failed");
        return false;
    }

    hr = m_pXAudio2->CreateMasteringVoice(&m_pMasterVoice);
    if (FAILED(hr)) {
        LOG_WARN("AudioEngine: CreateMasteringVoice failed");
        if (m_pXAudio2) { m_pXAudio2->Release(); m_pXAudio2 = nullptr; }
        return false;
    }

    GenerateDefaultWav();
    
    if (!m_audioData.empty() && m_wfx.wFormatTag != 0) {
        hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &m_wfx);
        if (FAILED(hr)) {
            LOG_WARN("AudioEngine: CreateSourceVoice failed");
            if (m_pMasterVoice) { m_pMasterVoice->DestroyVoice(); m_pMasterVoice = nullptr; }
            if (m_pXAudio2) { m_pXAudio2->Release(); m_pXAudio2 = nullptr; }
            return false;
        }
    } else {
        LOG_WARN("AudioEngine: Audio data empty, CreateSourceVoice skipped");
        // We'll fall back to MessageBeep when playing
    }

    m_initialized = true;
    LOG_INFO("AudioEngine initialized via XAudio2");
    return true;
}

void AudioEngine::Shutdown() {
    if (m_pSourceVoice) {
        m_pSourceVoice->Stop();
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = nullptr;
    }
    if (m_pMasterVoice) {
        m_pMasterVoice->DestroyVoice();
        m_pMasterVoice = nullptr;
    }
    if (m_pXAudio2) {
        m_pXAudio2->Release();
        m_pXAudio2 = nullptr;
    }
    m_initialized = false;
    // CoUninitialize is handled globally by Application
}

void AudioEngine::Stop() {
    if (m_initialized && m_pSourceVoice) {
        m_pSourceVoice->Stop();
        m_pSourceVoice->FlushSourceBuffers();
    }
}

void AudioEngine::ReloadSound() {
    if (!m_initialized || !m_pXAudio2) return;
    if (m_pSourceVoice) {
        m_pSourceVoice->Stop();
        m_pSourceVoice->FlushSourceBuffers();
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = nullptr;
    }
    GenerateDefaultWav();
    if (!m_audioData.empty() && m_wfx.wFormatTag != 0) {
        m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &m_wfx);
    }
}

// ── Load and decode MP3/audio into memory ────────────────────────────────────
void AudioEngine::GenerateDefaultWav() {
    m_audioData.clear();
    memset(&m_wfx, 0, sizeof(m_wfx));

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return;

    IMFSourceReader* pReader = nullptr;
    
    auto preset = Settings::Instance().GetCopy().soundPreset;

    // Procedural synthesis for built-in acoustic presets (Zero disk dependency)
    if (preset == SoundPreset::WaterDrop || preset == SoundPreset::AmbientEcho || preset == SoundPreset::WoodTap) {
        int sampleRate = 44100;
        m_wfx.wFormatTag = WAVE_FORMAT_PCM;
        m_wfx.nChannels = 1;
        m_wfx.nSamplesPerSec = sampleRate;
        m_wfx.wBitsPerSample = 16;
        m_wfx.nBlockAlign = 2;
        m_wfx.nAvgBytesPerSec = sampleRate * 2;
        m_wfx.cbSize = 0;

        float durationSec = (preset == SoundPreset::AmbientEcho) ? 1.35f : ((preset == SoundPreset::WaterDrop) ? 0.28f : 0.12f);
        int totalSamples = (int)(sampleRate * durationSec);
        m_audioData.resize(totalSamples * sizeof(short));
        short* pSamples = reinterpret_cast<short*>(m_audioData.data());

        double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;
        for (int i = 0; i < totalSamples; ++i) {
            float t = (float)i / (float)sampleRate;
            float sampleVal = 0.0f;

            if (preset == SoundPreset::WaterDrop) {
                // Pristine water droplet: frequency drops gracefully from 1250Hz to 620Hz
                float freq = 620.0f + 630.0f * expf(-18.0f * t);
                phase1 += 2.0 * 3.1415926535 * freq / sampleRate;
                float env = (t < 0.008f) ? (t / 0.008f) : expf(-14.0f * (t - 0.008f));
                sampleVal = 0.85f * env * sinf((float)phase1);
            } else if (preset == SoundPreset::AmbientEcho) {
                // Warm ambient harmonic chord: C5 (523Hz), E5 (659Hz), G5 (784Hz)
                phase1 += 2.0 * 3.1415926535 * 523.25 / sampleRate;
                phase2 += 2.0 * 3.1415926535 * 659.25 / sampleRate;
                phase3 += 2.0 * 3.1415926535 * 783.99 / sampleRate;
                float env = (t < 0.045f) ? (t / 0.045f) : expf(-2.8f * (t - 0.045f));
                sampleVal = 0.35f * env * (sinf((float)phase1) + 0.7f * sinf((float)phase2) + 0.5f * sinf((float)phase3));
            } else if (preset == SoundPreset::WoodTap) {
                // Gentle wood tap: 460Hz dampened impulse
                phase1 += 2.0 * 3.1415926535 * 460.0 / sampleRate;
                float env = (t < 0.003f) ? (t / 0.003f) : expf(-38.0f * (t - 0.003f));
                sampleVal = 0.75f * env * sinf((float)phase1);
            }

            sampleVal = std::clamp(sampleVal, -1.0f, 1.0f);
            pSamples[i] = (short)(sampleVal * 32767.0f);
        }
        LOG_INFO("AudioEngine: Synthesized preset " + std::to_string((int)preset) + " successfully");
        return;
    }

    // 1. Try custom user sound path if configured and preset is Custom
    std::wstring customPath = Settings::Instance().GetCopy().customSoundPath;
    if (preset == SoundPreset::Custom && !customPath.empty() && std::filesystem::exists(customPath)) {
        hr = MFCreateSourceReaderFromURL(customPath.c_str(), nullptr, &pReader);
        if (SUCCEEDED(hr) && pReader) {
            LOG_INFO("AudioEngine: Loading custom notification sound from disk");
        }
    }

    // 2. Default Chime: load audio/diode_matrix.mp3 relative to executable
    if (!pReader) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring exeDir = path;
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
        } else {
            exeDir = L"";
        }

        const std::wstring audioCandidates[] = {
            exeDir + L"audio\\diode_matrix.mp3",
            exeDir + L"audio\\notification.mp3",
            exeDir + L"assets\\audio\\diode_matrix.mp3",
            exeDir + L"assets\\audio\\notification.mp3",
            exeDir + L"diode_matrix.mp3",
            exeDir + L"notification.mp3"
        };

        for (const auto& candidate : audioCandidates) {
            if (std::filesystem::exists(candidate)) {
                hr = MFCreateSourceReaderFromURL(candidate.c_str(), nullptr, &pReader);
                if (SUCCEEDED(hr) && pReader) {
                    char pathBuf[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0, candidate.c_str(), -1, pathBuf, sizeof(pathBuf), nullptr, nullptr);
                    LOG_INFO(std::string("AudioEngine: Loading notification sound from disk: ") + pathBuf);
                    break;
                }
            }
        }
    }

    if (!pReader) {
        LOG_WARN("AudioEngine: No audio source could be loaded");
        MFShutdown();
        return;
    }

    IMFMediaType* pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    if (SUCCEEDED(hr)) {
        hr = pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        hr = pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
        pPartialType->Release();
    }

    if (SUCCEEDED(hr)) {
        IMFMediaType* pUncompressedAudioType = nullptr;
        hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedAudioType);
        if (SUCCEEDED(hr)) {
            // Get the wave format
            UINT32 cbFormat = 0;
            WAVEFORMATEX* pWavFormat = nullptr;
            MFCreateWaveFormatExFromMFMediaType(pUncompressedAudioType, &pWavFormat, &cbFormat);
            if (pWavFormat) {
                m_wfx = *pWavFormat;
                CoTaskMemFree(pWavFormat);
            }

            // Read all samples
            while (true) {
                DWORD flags = 0;
                IMFSample* pSample = nullptr;
                hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
                if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                    if (pSample) pSample->Release();
                    break;
                }
                if (pSample) {
                    IMFMediaBuffer* pBuffer = nullptr;
                    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                    if (SUCCEEDED(hr)) {
                        BYTE* pAudioData = nullptr;
                        DWORD cbCurrentLength = 0;
                        hr = pBuffer->Lock(&pAudioData, nullptr, &cbCurrentLength);
                        if (SUCCEEDED(hr)) {
                            m_audioData.insert(m_audioData.end(), pAudioData, pAudioData + cbCurrentLength);
                            pBuffer->Unlock();
                        }
                        pBuffer->Release();
                    }
                    pSample->Release();
                }
            }
            pUncompressedAudioType->Release();
        }
    }
    
    pReader->Release();
    MFShutdown();
    
    LOG_INFO("AudioEngine: Audio decoded successfully, bytes: " + std::to_string(m_audioData.size()));
}

void AudioEngine::PlayReminder(float volumeOverride) {
    if (!m_initialized || !m_pSourceVoice || m_audioData.empty()) {
        MessageBeep(MB_ICONASTERISK); // Fallback
        return;
    }

    auto s = Settings::Instance().GetCopy();
    if (!s.soundEnabled && volumeOverride < 0.0f) return;

    // Ensure playback isn't stacking out of control
    m_pSourceVoice->Stop();
    m_pSourceVoice->FlushSourceBuffers();

    XAUDIO2_BUFFER buffer = {0};
    buffer.AudioBytes = (UINT32)m_audioData.size();
    buffer.pAudioData = m_audioData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    HRESULT hr = m_pSourceVoice->SubmitSourceBuffer(&buffer);
    if (SUCCEEDED(hr)) {
        float vol = (volumeOverride >= 0.0f) ? std::clamp(volumeOverride, 0.0f, 1.0f) : (s.soundVolume / 100.0f);
        float amp = vol * vol;
        m_pSourceVoice->SetVolume(amp);
        m_pSourceVoice->Start(0);
        LOG_INFO("AudioEngine: Playing notification sound at volume " + std::to_string(vol));
    } else {
        LOG_WARN("AudioEngine: SubmitSourceBuffer failed");
        MessageBeep(MB_ICONASTERISK);
    }
}

} // namespace dhikr
