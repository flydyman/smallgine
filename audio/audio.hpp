#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <glm/glm.hpp>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <iostream>

namespace smallgine {

// Minimal OpenAL audio system. Generates PCM tones procedurally (no asset file).
class AudioSystem
{
private:
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    ALuint buffer = 0;
    ALuint source = 0;
    ALuint wavBuffer = 0;   // positioned looping emitter (spatial audio)
    ALuint wavSource = 0;

    static void logError(const char* where)
    {
        ALenum err = alGetError();
        if (err != AL_NO_ERROR)
        {
            std::cout << "Audio error (" << where << "): 0x" << std::hex << err << std::dec << std::endl;
        }
    }

public:
    bool init()
    {
        device = alcOpenDevice(nullptr); // default output device
        if (!device)
        {
            std::cout << "Audio: no output device" << std::endl;
            return false;
        }

        context = alcCreateContext(device, nullptr);
        if (!context || !alcMakeContextCurrent(context))
        {
            std::cout << "Audio: failed to create context" << std::endl;
            return false;
        }

        alGenBuffers(1, &buffer);
        alGenSources(1, &source);
        logError("init");

        const ALCchar* name = alcGetString(device, ALC_DEFAULT_DEVICE_SPECIFIER);
        std::cout << "Audio initialized: " << (name ? name : "unknown") << std::endl;
        return true;
    }

    // Play a sine tone. Replaces whatever the single source was playing.
    void playTone(float freq = 440.0f, float seconds = 0.3f)
    {
        if (!context) return;

        const int rate = 44100;
        int samples = (int)(rate * seconds);
        std::vector<short> pcm(samples);
        const float twoPi = 6.28318530718f;
        for (int i = 0; i < samples; ++i)
        {
            pcm[i] = (short)(sinf(twoPi * freq * i / rate) * 3000.0f);
        }

        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0); // detach before refilling
        alBufferData(buffer, AL_FORMAT_MONO16, pcm.data(),
                     (ALsizei)(samples * sizeof(short)), rate);
        alSourcei(source, AL_BUFFER, buffer);
        alSourcePlay(source);
        logError("playTone");
    }

    // Parse a canonical 16-bit PCM WAV into a positioned looping source.
    // Returns false on failure. Requires an initialized context.
    bool loadEmitter(const char* path, const glm::vec3& pos)
    {
        if (!context) return false;
        std::ifstream f(path, std::ios::binary);
        if (!f) { std::cout << "WAV load failed: " << path << std::endl; return false; }
        std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4))
        {
            std::cout << "WAV bad header: " << path << std::endl; return false;
        }

        uint16_t channels = 0, bits = 0;
        uint32_t rate = 0;
        const unsigned char* pcm = nullptr;
        uint32_t pcmLen = 0;
        size_t p = 12;
        while (p + 8 <= b.size())
        {
            uint32_t sz; std::memcpy(&sz, &b[p + 4], 4);
            if (!std::memcmp(&b[p], "fmt ", 4))
            {
                std::memcpy(&channels, &b[p + 10], 2);
                std::memcpy(&rate, &b[p + 12], 4);
                std::memcpy(&bits, &b[p + 22], 2);
            }
            else if (!std::memcmp(&b[p], "data", 4))
            {
                pcm = &b[p + 8];
                pcmLen = (p + 8 + sz <= b.size()) ? sz : (uint32_t)(b.size() - p - 8);
            }
            p += 8 + sz + (sz & 1);
        }
        if (!pcm || bits != 16)
        {
            std::cout << "WAV unsupported (need 16-bit PCM): " << path << std::endl; return false;
        }

        ALenum fmt = (channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
        alGenBuffers(1, &wavBuffer);
        alBufferData(wavBuffer, fmt, pcm, (ALsizei)pcmLen, (ALsizei)rate);
        alGenSources(1, &wavSource);
        alSourcei(wavSource, AL_BUFFER, wavBuffer);
        alSourcei(wavSource, AL_LOOPING, AL_TRUE);
        alSourcef(wavSource, AL_REFERENCE_DISTANCE, 1.5f);
        alSource3f(wavSource, AL_POSITION, pos.x, pos.y, pos.z);
        alSourcePlay(wavSource);
        logError("loadEmitter");
        std::cout << "WAV loaded: " << path << " (" << pcmLen << " bytes, "
                  << channels << "ch " << rate << "Hz), looping at emitter" << std::endl;
        return true;
    }

    void setEmitterPos(const glm::vec3& p)
    {
        if (wavSource) alSource3f(wavSource, AL_POSITION, p.x, p.y, p.z);
    }

    // Listener follows the camera each frame (spatial panning).
    void setListener(const glm::vec3& pos, const glm::vec3& fwd, const glm::vec3& up)
    {
        alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        float ori[6] = { fwd.x, fwd.y, fwd.z, up.x, up.y, up.z };
        alListenerfv(AL_ORIENTATION, ori);
    }

    void cleanup()
    {
        if (wavSource) alDeleteSources(1, &wavSource);
        if (wavBuffer) alDeleteBuffers(1, &wavBuffer);
        if (source) alDeleteSources(1, &source);
        if (buffer) alDeleteBuffers(1, &buffer);
        alcMakeContextCurrent(nullptr);
        if (context) alcDestroyContext(context);
        if (device) alcCloseDevice(device);
        std::cout << "Audio destroyed" << std::endl;
    }
};

} // namespace smallgine
