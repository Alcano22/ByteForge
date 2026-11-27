#include <Engine/Core/FileSystem.h>
#include <Engine/Text/FontAtlas.h>
#include <Engine/Text/FontAtlasFile.h>

#include <exception>
#include <print>
#include <vector>

int main(const int argc, char* argv[])
{
    using namespace ByteForge;

    if (argc != 3)
    {
        std::println(stderr, "Usage: FontBaker <font.ttf> <output.bfatlas>");
        return 2;
    }

    try
    {
        const fs::path input = argv[1];
        const fs::path output = argv[2];

        const Result<std::vector<std::byte>> font = ReadBinaryFile(input);
        if (!font)
        {
            std::println(stderr, "FontBaker: cannot read '{}': {}", argv[1], font.error().c_str());
            return 1;
        }

        const FontAtlasOptions options;

        const Result<FontAtlasData> atlas = BuildFontAtlas(*font, options);
        if (!atlas)
        {
            std::println(stderr, "FontBaker: cannot build the atlas of '{}': {}", argv[1], atlas.error().c_str());
            return 1;
        }

        const std::vector<std::byte> file = SerializeFontAtlas(*atlas, MakeFontAtlasKey(*font, options));
        if (const Result<> written = WriteFileAtomic(output, file); !written)
        {
            std::println(stderr, "FontBaker: cannot write '{}': {}", argv[2], written.error().c_str());
            return 1;
        }

        std::println("FontBaker: {} -> {} ({} glyphs, {}x{})",
                     argv[1], argv[2], atlas->Glyphs.size(), atlas->Width, atlas->Height);
        return 0;
    } catch (const std::exception& e)
    {
        std::println(stderr, "FontBaker: {}", e.what());
        return 1;
    }
}
