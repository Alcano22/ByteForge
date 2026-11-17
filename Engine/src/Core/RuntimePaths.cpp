#include "Engine/Core/RuntimePaths.h"
#include "Engine/Core/Platform.h"

namespace ByteForge::RuntimePaths
{
    namespace
    {
        Layout s_Layout = Layout::Flat;
    }

    Layout GetLayout() { return s_Layout; }
    void SetLayout(const Layout layout) { s_Layout = layout; }

    fs::path GetRoot()
    {
        const fs::path& executableDirectory = Platform::GetExecutableDirectory();
        return s_Layout == Layout::Install ? executableDirectory.parent_path() : executableDirectory;
    }

    fs::path GetDataDirectory()
    {
        return s_Layout == Layout::Install ? GetRoot() / "share" : GetRoot();
    }
}
