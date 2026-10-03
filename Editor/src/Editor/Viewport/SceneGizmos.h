#pragma once

namespace ByteForge
{
    class EditorCamera2D;
    class Entity;
    class LineRenderer;

    namespace SceneGizmos
    {
        void DrawGrid(LineRenderer& lines, const EditorCamera2D& camera);

        void DrawSelection(LineRenderer& lines, Entity entity);
    }
}
