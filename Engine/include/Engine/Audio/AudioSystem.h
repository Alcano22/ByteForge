#pragma once

namespace ByteForge
{
    class Scene;
    struct AudioSourceComponent;

    class AudioSystem
    {
    public:
        static void Start(Scene& scene);
        static void Stop(Scene& scene);

        static void Update(Scene& scene);

    private:
        static void Reload(AudioSourceComponent& source);
        static void Apply(const AudioSourceComponent& source);
    };
}
