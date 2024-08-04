#include "JsonXModelLoader.h"

#include "Csv/CsvStream.h"
#include "Game/T6/CommonT6.h"
#include "Game/T6/Json/JsonXModel.h"
#include "ObjLoading.h"
#include "Utils/QuatInt16.h"
#include "Utils/StringUtils.h"
#include "XModel/Gltf/GltfBinInput.h"
#include "XModel/Gltf/GltfLoader.h"
#include "XModel/Gltf/GltfTextInput.h"
#include "XModel/XModelCommon.h"

#pragma warning(push, 0)
#include <Eigen>
#pragma warning(pop)

#include <filesystem>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using namespace nlohmann;
using namespace T6;

namespace fs = std::filesystem;

namespace
{
    const char* HITLOC_NAMES[]{
        "none",           "helmet",          "head",           "neck",       "torso_upper", "torso_middle",    "torso_lower",    "right_arm_upper",
        "left_arm_upper", "right_arm_lower", "left_arm_lower", "right_hand", "left_hand",   "right_leg_upper", "left_leg_upper", "right_leg_lower",
        "left_leg_lower", "right_foot",      "left_foot",      "gun",        "shield",
    };
    static_assert(std::extent_v<decltype(HITLOC_NAMES)> == HITLOC_COUNT);

    class PartClassificationState final : public IZoneAssetLoaderState
    {
        static constexpr auto PART_CLASSIFICATION_FILE = "partclassification.csv";

    public:
        PartClassificationState()
            : m_loaded(false)
        {
        }

        bool Load(const IAssetLoadingManager& manager)
        {
            if (m_loaded)
                return true;

            if (ObjLoading::Configuration.Verbose)
                std::cout << "Loading part classification...\n";

            const auto file = manager.GetAssetLoadingContext()->m_raw_search_path->Open(PART_CLASSIFICATION_FILE);
            if (!file.IsOpen())
            {
                std::cerr << std::format("Could not load part classification: Failed to open {}\n", PART_CLASSIFICATION_FILE);
                return false;
            }

            CsvInputStream csvStream(*file.m_stream);
            std::vector<std::string> row;
            auto rowIndex = 0u;
            while (csvStream.NextRow(row))
            {
                if (!LoadRow(rowIndex++, row))
                    return false;
            }

            m_loaded = true;

            return false;
        }

        [[nodiscard]] unsigned GetPartClassificationForBoneName(const std::string& boneName) const
        {
            const auto entry = m_part_classifications.find(boneName);

            return entry != m_part_classifications.end() ? entry->second : HITLOC_NONE;
        }

    private:
        bool LoadRow(const unsigned rowIndex, std::vector<std::string>& row)
        {
            if (row.empty())
                return true;

            if (row.size() != 2)
            {
                std::cerr << "Could not load part classification: Invalid row\n";
                return false;
            }

            utils::MakeStringLowerCase(row[0]);
            utils::MakeStringLowerCase(row[1]);

            const auto foundHitLoc = std::ranges::find(HITLOC_NAMES, row[1]);
            if (foundHitLoc == std::end(HITLOC_NAMES))
            {
                std::cerr << std::format("Invalid hitloc name in row {}: {}\n", rowIndex + 1, row[1]);
                return false;
            }

            const auto hitLocNum = std::distance(std::begin(HITLOC_NAMES), foundHitLoc);

            m_part_classifications.emplace(row[0], hitLocNum);
            return true;
        }

        bool m_loaded;
        std::unordered_map<std::string, unsigned> m_part_classifications;
    };

    class JsonLoader
    {
    public:
        JsonLoader(std::istream& stream, MemoryManager& memory, IAssetLoadingManager& manager, std::set<XAssetInfoGeneric*>& dependencies)
            : m_stream(stream),
              m_memory(memory),
              m_script_strings(manager.GetAssetLoadingContext()->m_zone->m_script_strings),
              m_manager(manager),
              m_part_classification_state(*m_manager.GetAssetLoadingContext()->GetZoneAssetLoaderState<PartClassificationState>()),
              m_dependencies(dependencies)

        {
        }

        bool Load(XModel& xmodel) const
        {
            const auto jRoot = json::parse(m_stream);
            std::string type;
            unsigned version;

            jRoot.at("_type").get_to(type);
            jRoot.at("_version").get_to(version);

            if (type != "xmodel" || version != 1u)
            {
                std::cerr << std::format("Tried to load xmodel \"{}\" but did not find expected type material of version 1\n", xmodel.name);
                return false;
            }

            const auto jXModel = jRoot.get<JsonXModel>();
            return CreateXModelFromJson(jXModel, xmodel);
        }

    private:
        static void PrintError(const XModel& xmodel, const std::string& message)
        {
            std::cerr << std::format("Cannot load xmodel \"{}\": {}\n", xmodel.name, message);
        }

        static std::unique_ptr<XModelCommon> LoadModelByExtension(std::istream& stream, const std::string& extension)
        {
            if (extension == ".glb")
            {
                gltf::BinInput input;
                if (!input.ReadGltfData(stream))
                    return nullptr;

                const auto loader = gltf::Loader::CreateLoader(&input);
                return loader->Load();
            }

            if (extension == ".gltf")
            {
                gltf::TextInput input;
                if (!input.ReadGltfData(stream))
                    return nullptr;

                const auto loader = gltf::Loader::CreateLoader(&input);
                return loader->Load();
            }

            return nullptr;
        }

        static void ApplyBasePose(DObjAnimMat& baseMat, const XModelBone& bone)
        {
            baseMat.trans.x = bone.globalOffset[0];
            baseMat.trans.y = bone.globalOffset[1];
            baseMat.trans.z = bone.globalOffset[2];
            baseMat.quat.x = bone.globalRotation.x;
            baseMat.quat.y = bone.globalRotation.y;
            baseMat.quat.z = bone.globalRotation.z;
            baseMat.quat.w = bone.globalRotation.w;

            const auto quatNormSquared = Eigen::Quaternionf(baseMat.quat.w, baseMat.quat.x, baseMat.quat.y, baseMat.quat.z).squaredNorm();
            if (std::abs(quatNormSquared) < std::numeric_limits<float>::epsilon())
            {
                baseMat.quat.w = 1.0f;
                baseMat.transWeight = 2.0f;
            }
            else
            {
                baseMat.transWeight = 2.0f / quatNormSquared;
            }
        }

        static void CalculateBoneBounds(XBoneInfo& info, const unsigned boneIndex, const XModelCommon& common)
        {
            if (common.m_vertex_bone_weights.empty())
                return;

            info.bounds[0].x = 0.0f;
            info.bounds[0].y = 0.0f;
            info.bounds[0].z = 0.0f;
            info.bounds[1].x = 0.0f;
            info.bounds[1].y = 0.0f;
            info.bounds[1].z = 0.0f;
            info.offset.x = 0.0f;
            info.offset.y = 0.0f;
            info.offset.z = 0.0f;
            info.radiusSquared = 0.0f;

            const auto vertexCount = common.m_vertex_bone_weights.size();
            for (auto vertexIndex = 0u; vertexIndex < vertexCount; vertexIndex++)
            {
                const auto& vertex = common.m_vertices[vertexIndex];
                const auto& vertexWeights = common.m_vertex_bone_weights[vertexIndex];
                const auto* weights = &common.m_bone_weight_data.weights[vertexWeights.weightOffset];
                for (auto weightIndex = 0u; weightIndex < vertexWeights.weightCount; weightIndex++)
                {
                    const auto& weight = weights[weightIndex];
                    if (weight.boneIndex != boneIndex)
                        continue;

                    info.bounds[0].x = std::min(info.bounds[0].x, vertex.coordinates[0]);
                    info.bounds[0].y = std::min(info.bounds[0].y, vertex.coordinates[1]);
                    info.bounds[0].z = std::min(info.bounds[0].z, vertex.coordinates[2]);
                    info.bounds[1].x = std::max(info.bounds[1].x, vertex.coordinates[0]);
                    info.bounds[1].y = std::max(info.bounds[1].y, vertex.coordinates[1]);
                    info.bounds[1].z = std::max(info.bounds[1].z, vertex.coordinates[2]);
                }
            }

            const Eigen::Vector3f minEigen(info.bounds[0].x, info.bounds[0].y, info.bounds[0].z);
            const Eigen::Vector3f maxEigen(info.bounds[1].x, info.bounds[1].y, info.bounds[1].z);
            const Eigen::Vector3f boundsCenter = (minEigen + maxEigen) * 0.5f;
            info.offset.x = boundsCenter.x();
            info.offset.y = boundsCenter.y();
            info.offset.z = boundsCenter.z();
            info.radiusSquared = Eigen::Vector3f(maxEigen - boundsCenter).squaredNorm();
        }

        bool ApplyCommonBonesToXModel(const JsonXModelLod& jLod, XModel& xmodel, unsigned lodNumber, const XModelCommon& common) const
        {
            if (common.m_bones.empty())
                return true;

            m_part_classification_state.Load(m_manager);

            const auto boneCount = common.m_bones.size();
            constexpr auto maxBones = std::numeric_limits<decltype(XModel::numBones)>::max();
            if (boneCount > maxBones)
            {
                PrintError(xmodel, std::format("Model \"{}\" for lod {} contains too many bones ({} -> max={})", jLod.file, lodNumber, boneCount, maxBones));
                return false;
            }

            xmodel.numRootBones = 0u;
            xmodel.numBones = 0u;
            for (const auto& bone : common.m_bones)
            {
                if (!bone.parentIndex)
                {
                    // Make sure root bones are at the beginning
                    assert(xmodel.numRootBones == xmodel.numBones);
                    xmodel.numRootBones++;
                }

                xmodel.numBones++;
            }

            xmodel.boneNames = m_memory.Alloc<ScriptString>(xmodel.numBones);
            xmodel.partClassification = m_memory.Alloc<unsigned char>(xmodel.numBones);
            xmodel.baseMat = m_memory.Alloc<DObjAnimMat>(xmodel.numBones);
            xmodel.boneInfo = m_memory.Alloc<XBoneInfo>(xmodel.numBones);

            if (xmodel.numBones > xmodel.numRootBones)
            {
                xmodel.parentList = m_memory.Alloc<unsigned char>(xmodel.numBones - xmodel.numRootBones);
                xmodel.trans = m_memory.Alloc<vec4_t>(xmodel.numBones - xmodel.numRootBones);
                xmodel.quats = m_memory.Alloc<XModelQuat>(xmodel.numBones - xmodel.numRootBones);
            }
            else
            {
                xmodel.parentList = nullptr;
                xmodel.trans = nullptr;
                xmodel.quats = nullptr;
            }

            for (auto boneIndex = 0u; boneIndex < boneCount; boneIndex++)
            {
                const auto& bone = common.m_bones[boneIndex];
                xmodel.boneNames[boneIndex] = m_script_strings.AddOrGetScriptString(bone.name);
                xmodel.partClassification[boneIndex] = static_cast<unsigned char>(m_part_classification_state.GetPartClassificationForBoneName(bone.name));

                ApplyBasePose(xmodel.baseMat[boneIndex], bone);
                CalculateBoneBounds(xmodel.boneInfo[boneIndex], boneIndex, common);

                // Other boneInfo data is filled when calculating bone bounds
                xmodel.boneInfo[boneIndex].collmap = -1;

                if (xmodel.numRootBones < boneIndex)
                {
                    const auto nonRootIndex = boneIndex - xmodel.numRootBones;
                    xmodel.parentList[nonRootIndex] = static_cast<unsigned char>(bone.parentIndex.value_or(0u));

                    auto& trans = xmodel.trans[nonRootIndex];
                    trans.x = bone.localOffset[0];
                    trans.y = bone.localOffset[1];
                    trans.z = bone.localOffset[2];

                    auto& quats = xmodel.quats[nonRootIndex];
                    quats.v[0] = QuatInt16::ToInt16(bone.localRotation.x);
                    quats.v[1] = QuatInt16::ToInt16(bone.localRotation.y);
                    quats.v[2] = QuatInt16::ToInt16(bone.localRotation.z);
                    quats.v[3] = QuatInt16::ToInt16(bone.localRotation.w);
                }
            }

            return true;
        }

        bool LoadLod(const JsonXModelLod& jLod, XModel& xmodel, unsigned lodNumber) const
        {
            const auto file = m_manager.GetAssetLoadingContext()->m_raw_search_path->Open(jLod.file);
            if (!file.IsOpen())
            {
                PrintError(xmodel, std::format("Failed to open file for lod {}: \"{}\"", lodNumber, jLod.file));
                return false;
            }

            auto extension = fs::path(jLod.file).extension().string();
            utils::MakeStringLowerCase(extension);

            const auto common = LoadModelByExtension(*file.m_stream, extension);
            if (!common)
            {
                PrintError(xmodel, std::format("Failure while trying to load model for lod {}: \"{}\"", lodNumber, jLod.file));
                return false;
            }

            if (lodNumber == 0u)
            {
                if (!ApplyCommonBonesToXModel(jLod, xmodel, lodNumber, *common))
                    return false;
            }
            // TODO: Verify bones if not lod 0

            // TODO: Apply common XModel vertex data

            return true;
        }

        bool CreateXModelFromJson(const JsonXModel& jXModel, XModel& xmodel) const
        {
            auto lodNumber = 0u;
            for (const auto& jLod : jXModel.lods)
                LoadLod(jLod, xmodel, lodNumber++);

            if (jXModel.collLod && jXModel.collLod.value() >= 0)
            {
                if (static_cast<unsigned>(jXModel.collLod.value()) >= jXModel.lods.size())
                {
                    PrintError(xmodel, "Collision lod is not a valid lod");
                    return false;
                }
                xmodel.collLod = static_cast<int16_t>(jXModel.collLod.value());
            }
            else
                xmodel.collLod = -1;

            if (jXModel.physPreset)
            {
                auto* physPreset = m_manager.LoadDependency<AssetPhysPreset>(jXModel.physPreset.value());
                if (!physPreset)
                {
                    PrintError(xmodel, "Could not find phys preset");
                    return false;
                }
                m_dependencies.emplace(physPreset);
                xmodel.physPreset = physPreset->Asset();
            }
            else
            {
                xmodel.physPreset = nullptr;
            }

            if (jXModel.physConstraints)
            {
                auto* physConstraints = m_manager.LoadDependency<AssetPhysConstraints>(jXModel.physConstraints.value());
                if (!physConstraints)
                {
                    PrintError(xmodel, "Could not find phys constraints");
                    return false;
                }
                m_dependencies.emplace(physConstraints);
                xmodel.physConstraints = physConstraints->Asset();
            }
            else
            {
                xmodel.physConstraints = nullptr;
            }

            xmodel.flags = jXModel.flags;
            xmodel.lightingOriginOffset.x = jXModel.lightingOriginOffset.x;
            xmodel.lightingOriginOffset.y = jXModel.lightingOriginOffset.y;
            xmodel.lightingOriginOffset.z = jXModel.lightingOriginOffset.z;
            xmodel.lightingOriginRange = jXModel.lightingOriginRange;

            return true;
        }

        std::istream& m_stream;
        MemoryManager& m_memory;
        ZoneScriptStrings& m_script_strings;
        IAssetLoadingManager& m_manager;
        PartClassificationState& m_part_classification_state;
        std::set<XAssetInfoGeneric*>& m_dependencies;
    };
} // namespace

namespace T6
{
    bool LoadXModelAsJson(
        std::istream& stream, XModel& xmodel, MemoryManager* memory, IAssetLoadingManager* manager, std::vector<XAssetInfoGeneric*>& dependencies)
    {
        std::set<XAssetInfoGeneric*> dependenciesSet;
        const JsonLoader loader(stream, *memory, *manager, dependenciesSet);

        dependencies.assign(dependenciesSet.cbegin(), dependenciesSet.cend());

        return loader.Load(xmodel);
    }
} // namespace T6
