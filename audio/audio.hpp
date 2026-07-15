#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <vector>
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

    void cleanup()
    {
        if (source) alDeleteSources(1, &source);
        if (buffer) alDeleteBuffers(1, &buffer);
        alcMakeContextCurrent(nullptr);
        if (context) alcDestroyContext(context);
        if (device) alcCloseDevice(device);
        std::cout << "Audio destroyed" << std::endl;
    }
};

} // namespace smallgine
