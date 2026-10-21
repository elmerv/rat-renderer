#include "VulkanCommon.h"
#include "LightEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

static const char *typeNames[] = {"Directional", "Point", "Spot"};

void drawLightEditor(std::vector<Light> &lights)
{
        ImGui::Begin("Lights");

        for (size_t i = 0; i < lights.size(); ++i)
        {
                Light &l = lights[i];
                ImGui::PushID(static_cast<int>(i));   // every light has a "Color" sli

                char header[32];
                std::snprintf(header, sizeof(header), "Light %zu (%s)", i, typeNames[static_cast<int>(l.type)]);

                if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
                {
                        int type = static_cast<int>(l.type);
                        if (ImGui::Combo("Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
                            l.type = static_cast<LightType>(type);

                        ImGui::ColorEdit3("Color", glm::value_ptr(l.color));
                        ImGui::DragFloat("Intensity", &l.intensity, 0.01f, 0.0f, 20.0f);

                        if (l.type != LightType::Directional)
                                ImGui::DragFloat3("Position", glm::value_ptr(l.position), 0.05f);

                        if (l.type != LightType::Point)
                        {
                                ImGui::DragFloat3("Direction", glm::value_ptr(l.direction), 0.01f, -1.0f, 1.0f);
                                if (glm::length(l.direction) < 1e-4f)   // normalize(0N
                                        l.direction = {0.0f, 0.0f, -1.0f};
                        }

                        if (l.type == LightType::Spot)
                        {
                                // stored as cosines for the shader; edited as degrees
                                float inner = glm::degrees(std::acos(l.innerCone));
                                float outer = glm::degrees(std::acos(l.outerCone));
                                bool changed = ImGui::SliderFloat("Inner angle", &inner, 1.0f, 89.0f, "%.1f deg");
                                changed |= ImGui::SliderFloat("Outer angle", &outer, 1.0f, 89.0f, "%.1f deg");
                                if (changed)
                                {
                                        outer = std::max(outer, inner + 0.5f);   // smoothstep breaks if the edges are equal
                                        l.innerCone = std::cos(glm::radians(inner));
                                        l.outerCone = std::cos(glm::radians(outer));
                                }
                        }

                        if (ImGui::Button("Remove"))
                        {
                                lights.erase(lights.begin() + i);
                                ImGui::PopID();
                                break;   // the vector changed; skip the rest this frame
                        }
                }
                ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::BeginDisabled(lights.size() >= MAX_LIGHTS);
        if (ImGui::Button("Add light"))
        {
                lights.push_back(Light{
                    .position  = {0.0f, 0.0f, 1.5f},
                    .innerCone = std::cos(glm::radians(20.0f)),
                    .color     = {1.0f, 1.0f, 1.0f},
                    .intensity = 1.0f,
                    .direction = {0.0f, 0.0f, -1.0f},
                    .outerCone = std::cos(glm::radians(25.0f)),
                    .type      = LightType::Point,
                });
        }
        ImGui::EndDisabled();
        ImGui::Text("%zu / %d lights", lights.size(), MAX_LIGHTS);

        ImGui::End();
}