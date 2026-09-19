#include "Platform/Vulkan/VulkanShaderReflection.h"
#include "Platform/Vulkan/VulkanHelpers.h"

#include <spirv-reflect/spirv_reflect.h>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    namespace
    {
        VkShaderStageFlagBits ToVkShaderStage(const ShaderStage stage)
        {
            switch (stage)
            {
                case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
                case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            throw std::runtime_error("Unknown ShaderStage");
        }

        const char* FormatToString(const VkFormat format)
        {
            switch (format)
            {
                case VK_FORMAT_R32_SFLOAT:          return "Float";
                case VK_FORMAT_R32G32_SFLOAT:       return "Float2";
                case VK_FORMAT_R32G32B32_SFLOAT:    return "Float3";
                case VK_FORMAT_R32G32B32A32_SFLOAT: return "Float4";
                case VK_FORMAT_R32_SINT:            return "Int";
                case VK_FORMAT_R32G32_SINT:         return "Int2";
                case VK_FORMAT_R32G32B32_SINT:      return "Int3";
                case VK_FORMAT_R32G32B32A32_SINT:   return "Int4";
                case VK_FORMAT_R8_UINT:             return "Bool";
                default:                            return "an unsupported format";
            }
        }

        void CheckReflect(const SpvReflectResult result, const char* what)
        {
            if (result != SPV_REFLECT_RESULT_SUCCESS)
            {
                throw std::runtime_error(std::format("SPIRV-Reflect failed to {} (SpvReflectResult {})",
                                                     what, static_cast<int>(result)));
            }
        }

        template<typename T, typename Fn>
        std::vector<T*> Enumerate(Fn&& enumerate, const char* what)
        {
            uint32_t count = 0;
            CheckReflect(enumerate(&count, nullptr), what);

            std::vector<T*> items(count);
            CheckReflect(enumerate(&count, items.data()), what);
            return items;
        }
    }

    ShaderReflection ShaderReflection::Reflect(const std::span<const uint32_t> spirv, const ShaderStage stage)
    {
        const spv_reflect::ShaderModule module(spirv.size_bytes(), spirv.data());
        CheckReflect(module.GetResult(), "parse SPIR-V");

        const VkShaderStageFlags vkStage = ToVkShaderStage(stage);
        ShaderReflection result;

        const auto bindings = Enumerate<SpvReflectDescriptorBinding>(
            [&](uint32_t* count, SpvReflectDescriptorBinding** out)
            {
                return module.EnumerateDescriptorBindings(count, out);
            }, "enumerate descriptor bindings");

        for (const SpvReflectDescriptorBinding* binding : bindings)
        {
            if (binding->count != 1)
                throw std::runtime_error("Descriptor arrays are not supported yet");

            ReflectedDescriptorBinding reflected{
                .Name    = binding->name != nullptr ? binding->name : "",
                .Set     = binding->set,
                .Binding = binding->binding,
                .Type    = static_cast<VkDescriptorType>(binding->descriptor_type),
                .Stages  = vkStage,
                .Size    = binding->block.size
            };

            for (uint32_t i = 0; i < binding->block.member_count; ++i)
            {
                const SpvReflectBlockVariable& member = binding->block.members[i];
                reflected.Members.push_back({
                    .Name   = member.name != nullptr ? member.name : "",
                    .Offset = member.offset,
                    .Size   = member.size
                });
            }

            result.Bindings.push_back(std::move(reflected));
        }

        const auto pushConstantBlocks = Enumerate<SpvReflectBlockVariable>(
            [&](uint32_t* count, SpvReflectBlockVariable** out)
            {
                return module.EnumeratePushConstantBlocks(count, out);
            }, "enumerate push constant blocks");

        if (pushConstantBlocks.size() > 1)
            throw std::runtime_error("A shader stage may declare at most one push constant block");

        if (!pushConstantBlocks.empty())
        {
            const SpvReflectBlockVariable* block = pushConstantBlocks.front();
            if (block->offset != 0)
                throw std::runtime_error("Push constant blocks must start at offset 0");

            result.PushConstants = { .Stages = vkStage, .Size = block->size };
        }

        if (stage == ShaderStage::Vertex)
        {
            const auto inputs = Enumerate<SpvReflectInterfaceVariable>(
                [&](uint32_t* count, SpvReflectInterfaceVariable** out)
                {
                    return module.EnumerateInputVariables(count, out);
                }, "enumerate input variables");

            for (const SpvReflectInterfaceVariable* input : inputs)
            {
                if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;

                std::string name = input->name != nullptr ? input->name : "";
                if (name.starts_with("in.var."))
                    name.erase(0, 7);

                result.VertexInputs.push_back({
                    .Name     = std::move(name),
                    .Location = input->location,
                    .Format   = static_cast<VkFormat>(input->format)
                });
            }

            std::ranges::sort(result.VertexInputs, {}, &ReflectedVertexInput::Location);
        }

        return result;
    }

    ShaderReflection ShaderReflection::Merge(const ShaderReflection& vertex, const ShaderReflection& fragment)
    {
        ShaderReflection merged;
        merged.Bindings = vertex.Bindings;
        merged.VertexInputs = vertex.VertexInputs;

        for (const ReflectedDescriptorBinding& binding : fragment.Bindings)
        {
            const auto existing = std::ranges::find_if(merged.Bindings, [&](const auto& other)
            {
                return other.Set == binding.Set && other.Binding == binding.Binding;
            });

            if (existing == merged.Bindings.end())
            {
                merged.Bindings.push_back(binding);
                continue;
            }

            if (existing->Type != binding.Type || existing->Size != binding.Size)
            {
                throw std::runtime_error(std::format("Descriptor (set {}, binding {}) is declared differently "
                                                     "in the vertex and fragment shader",
                                                     binding.Set, binding.Binding));
            }

            existing->Stages |= binding.Stages;
        }

        merged.PushConstants.Stages = vertex.PushConstants.Stages | fragment.PushConstants.Stages;
        merged.PushConstants.Size = std::max(vertex.PushConstants.Size, fragment.PushConstants.Size);

        return merged;
    }

    void ValidateVertexLayout(const ShaderReflection& reflection, const BufferLayout& layout)
    {
        const auto& elements = layout.GetElements();

        for (const ReflectedVertexInput& input : reflection.VertexInputs)
        {
            if (input.Location >= elements.size())
            {
                throw std::runtime_error(std::format("Vertex shader input '{}' (location {}) has no element "
                                                     "in the vertex layout ({} elements)",
                                                     input.Name, input.Location, elements.size()));
            }

            const BufferElement& element = elements[input.Location];
            const VkFormat provided = ShaderDataTypeToVkFormat(element.Type);

            if (input.Format != provided)
            {
                throw std::runtime_error(std::format("Vertex shader input '{}' (location {}) expects {}, "
                                                     "but layout element '{}' provides {}",
                                                     input.Name, input.Location, FormatToString(input.Format),
                                                     element.Name, FormatToString(provided)));
            }
        }
    }
}
