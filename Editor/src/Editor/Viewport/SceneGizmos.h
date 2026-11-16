#pragma once

namespace ByteForge
{
    class EditorCamera2D;
    class Entity;
    class LineRenderer;
    class Scene;

    enum class ColliderVisibility : u8
    {
        All,
        Selected,
        Off
    };

    namespace SceneGizmos
    {
        void DrawGrid(LineRenderer& lines, const EditorCamera2D& camera);

        void DrawColliders(LineRenderer& lines, Scene& scene, const EditorCamera2D& camera,
                           Entity selected, ColliderVisibility visibility);

        void DrawSelection(LineRenderer& lines, Entity entity);
    }
}
