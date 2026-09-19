#pragma once

#if defined(_WIN32)
#   if defined(BYTEFORGE_BUILD_DLL)
#       define BYTEFORGE_API __declspec(dllexport)
#   else
#       define BYTEFORGE_API __declspec(dllimport)
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   if defined(BYTEFORGE_BUILD_DLL)
#       define BYTEFORGE_API __attribute__((visibility("default")))
#   else
#       define BYTEFORGE_API
#   endif
#else
#   error "ByteForge only supports Windows and GCC/Clang-based platforms currently"
#endif

#include <memory>

namespace ByteForge
{
    template<typename T, typename D = std::default_delete<T>>
    using Scope = std::unique_ptr<T, D>;

    template<typename T, typename... Args>
    constexpr Scope<T> MakeScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T, typename D>
    constexpr Scope<T, D> WrapScope(T* handle, D deleter)
    {
        return Scope<T, D>(handle, deleter);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename... Args>
    constexpr Ref<T> MakeRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    using WeakRef = std::weak_ptr<T>;
}
