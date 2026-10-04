#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Scripting/API/ScriptBinding.h"

#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    enum class ScriptPurity : uint8_t { Impure, Pure };

    class BYTEFORGE_API ScriptAPI : NonCopyable
    {
    public:
        template<auto Fn, size_t N>
        void Register(std::string id, std::string displayName, const ScriptPurity purity,
                      const char* const (&parameterNames)[N])
        {
            Add(ScriptFunction{
                .Id          = std::move(id),
                .DisplayName = std::move(displayName),
                .Parameters  = Detail::DescribeParameters<Fn>(parameterNames),
                .Result      = Detail::DescribeResult<Fn>(),
                .Pure        = purity == ScriptPurity::Pure,
                .Invoke      = &Detail::Invoke<Fn>
            });
        }

        template<auto Fn>
        void Register(std::string id, std::string displayName, const ScriptPurity purity)
        {
            static_assert(Detail::FunctionTraits<decltype(Fn)>::Arity == 0,
                          "Name the parameters of this script function");

            Add(ScriptFunction{
                .Id          = std::move(id),
                .DisplayName = std::move(displayName),
                .Result      = Detail::DescribeResult<Fn>(),
                .Pure        = purity == ScriptPurity::Pure,
                .Invoke      = &Detail::Invoke<Fn>
            });
        }

        [[nodiscard]] std::span<const ScriptFunction> GetFunctions() const { return m_Functions; }
        [[nodiscard]] uint32_t FindIndex(std::string_view id) const;
        [[nodiscard]] const ScriptFunction* Find(std::string_view id) const;

        [[nodiscard]] const ScriptFunction& GetFunction(uint32_t index) const;

        [[nodiscard]] static const ScriptAPI& Get();

    private:
        ScriptAPI();

        void Add(ScriptFunction function);

    public:
        static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

    private:
        std::vector<ScriptFunction> m_Functions;
        std::map<std::string, uint32_t, std::less<>> m_Indices;
    };
}
