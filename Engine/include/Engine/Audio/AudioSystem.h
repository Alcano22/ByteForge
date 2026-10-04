#pragma once

namespace ByteForge
{
    class Scene;
    struct AudioSourceComponent;

    class AudioSystem
    {
    public:
        static void Update(Scene& scene);

        static void Stop(Scene& scene);

    private:
        static void Reload(AudioSourceComponent& source);
        static void Apply(const AudioSourceComponent& source);
    };
}
