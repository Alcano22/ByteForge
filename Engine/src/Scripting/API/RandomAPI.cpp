#include "Scripting/API/RandomAPI.h"

#include "Engine/Core/Random.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

namespace ByteForge
{
    namespace
    {
        Random& RequireRandom(const ScriptCallContext& context)
        {
            if (context.ActiveScene == nullptr)
                throw ScriptError("No scene is running");

            return context.ActiveScene->GetRandom();
        }

        f32 RandomFloat(const ScriptCallContext& context) { return RequireRandom(context).NextFloat(); }

        f32 RandomFloatInRange(const ScriptCallContext& context, const f32 min, const f32 max)
        {
            if (!std::isfinite(min) || !std::isfinite(max))
                throw ScriptError("Random.FloatInRange: Min and Max must be finite, got {} and {}", min, max);

            if (max < min)
                throw ScriptError("Random.FloatInRange: Min ({}) is greater than Max ({})", min, max);

            return RequireRandom(context).Range(min, max);
        }

        i32 RandomIntInRange(const ScriptCallContext& context, const i32 min, const i32 maxExclusive)
        {
            if (maxExclusive < min)
                throw ScriptError("Random.IntInRange: Min ({}) is greater than Max Exclusive ({})", min, maxExclusive);

            return RequireRandom(context).Range(min, maxExclusive);
        }

        bool RandomChance(const ScriptCallContext& context, const f32 probability)
        {
            if (std::isnan(probability))
                throw ScriptError("Random.Chance: Probability is not a number");

            return RequireRandom(context).Chance(probability);
        }

        glm::vec2 RandomDirection2D(const ScriptCallContext& context)
        {
            const f32 angle = RequireRandom(context).NextFloat() * glm::two_pi<f32>();
            return { std::cos(angle), std::sin(angle) };
        }

        glm::vec2 RandomPointInCircle(const ScriptCallContext& context, const f32 radius)
        {
            if (!std::isfinite(radius) || radius < 0.0f)
                throw ScriptError("Random.PointInCircle: Radius must be a finite number of at least 0, got {}", radius);

            Random& random = RequireRandom(context);
            const f32 angle = random.NextFloat() * glm::two_pi<f32>();
            const f32 distance = radius * std::sqrt(random.NextFloat());

            return { std::cos(angle) * distance, std::sin(angle) * distance };
        }

        void RandomSetSeed(const ScriptCallContext& context, const i32 seed)
        {
            RequireRandom(context).SetSeed(static_cast<u32>(seed));
        }
    }

    void RegisterRandomAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&RandomFloat>("Random.Float", "Random Float", Impure);
        api.Register<&RandomFloatInRange>("Random.FloatInRange", "Random Float In Range", Impure, { "Min", "Max" });
        api.Register<&RandomIntInRange>("Random.IntInRange", "Random Int In Range", Impure, { "Min", "Max Exclusive" });
        api.Register<&RandomChance>("Random.Chance", "Random Chance", Impure, { "Probability" });
        api.Register<&RandomDirection2D>("Random.Direction2D", "Random Direction (Vector2)", Impure);
        api.Register<&RandomPointInCircle>("Random.PointInCircle", "Random Point In Circle", Impure, { "Radius" });
        api.Register<&RandomSetSeed>("Random.SetSeed", "Set Random Seed", Impure, { "Seed" });
    }
}
