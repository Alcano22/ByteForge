#pragma once

#include "Editor/PopupManager.h"

#include <Engine/Assets/Pak.h>

#include <algorithm>
#include <atomic>
#include <expected>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>

namespace ByteForge
{
    class EditorContext;

    struct ExportResult
    {
        fs::path Directory;
        usize FileCount = 0;
        PakStats Archive;
        usize ShaderStages = 0;
        std::string DotNetVersion;
    };

    struct ExportPlan
    {
        fs::path EditorDirectory;
        fs::path AssetsDirectory;
        fs::path ProjectFile;
        std::optional<fs::path> GameScripts;
        fs::path Output;
        std::string ExecutableName;
    };

    class ExportProgress
    {
    public:
        static constexpr usize StepCount = 5;

        struct State
        {
            std::string Step;
            f32 Fraction = 0.0f;
        };

        void BeginStep(std::string name)
        {
            std::scoped_lock lock(m_Mutex);
            ++m_StepIndex;
            m_Step = std::move(name);
            m_Work = 0;
            m_Done = 0;
        }

        void AddWork(const usize units)
        {
            std::scoped_lock lock(m_Mutex);
            m_Work += units;
        }

        void Advance(const usize units = 1)
        {
            std::scoped_lock lock(m_Mutex);
            m_Done = std::min(m_Done + units, m_Work);
        }

        [[nodiscard]] State Get() const
        {
            std::scoped_lock lock(m_Mutex);

            const f32 inStep = m_Work > 0 ? static_cast<f32>(m_Done) / static_cast<f32>(m_Work) : 0.0f;
            const f32 finished = m_StepIndex > 0 ? static_cast<f32>(m_StepIndex - 1) : 0.0f;
            return {
                .Step     = m_Step,
                .Fraction = std::clamp((finished + inStep) / static_cast<f32>(StepCount), 0.0f, 1.0f)
            };
        }

        void Cancel() { m_Cancelled = true; }
        [[nodiscard]] bool IsCancelled() const { return m_Cancelled; }

    private:
        mutable std::mutex m_Mutex;
        std::string m_Step;
        usize m_StepIndex = 0;
        usize m_Work = 0;
        usize m_Done = 0;
        std::atomic<bool> m_Cancelled = false;
    };

    class GameExporter
    {
    public:
        [[nodiscard]] static Result<ExportPlan> Prepare(const fs::path& projectRoot, const fs::path& outputRoot);

        [[nodiscard]] static Result<ExportResult> Run(const ExportPlan& plan, ExportProgress& progress);
    };

    class ExportTask
    {
    public:
        ExportTask() = default;
        ~ExportTask();

        ExportTask(const ExportTask&) = delete;
        ExportTask& operator=(const ExportTask&) = delete;

        bool Start(ExportPlan plan, PopupManager& popups);

        void Update();

        [[nodiscard]] bool IsRunning() const { return m_Future.valid(); }

    private:
        Ref<ExportProgress> m_Progress;
        std::future<Result<ExportResult>> m_Future;
    };

    void ExportGameWithDialog(EditorContext& context);
}
