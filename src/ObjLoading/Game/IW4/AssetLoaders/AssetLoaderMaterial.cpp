#include "AssetLoaderMaterial.h"

#include <cstring>
#include <cmath>
#include <iostream>
#include <sstream>

#include "AssetLoaderTechniqueSet.h"
#include "ObjLoading.h"
#include "AssetLoading/AbstractGdtEntryReader.h"
#include "Game/IW4/CommonIW4.h"
#include "Game/IW4/IW4.h"
#include "Game/IW4/MaterialConstantsIW4.h"
#include "Game/IW4/ObjConstantsIW4.h"
#include "Game/IW4/TechsetConstantsIW4.h"
#include "Math/Vector.h"
#include "Pool/GlobalAssetPool.h"
#include "StateMap/StateMapFromTechniqueExtractor.h"
#include "StateMap/StateMapHandler.h"
#include "Techset/TechniqueFileReader.h"

using namespace IW4;

namespace IW4
{
    class SkipMaterialException final : public std::exception
    {
    };

    class MaterialGdtLoader : AbstractGdtEntryReader
    {
    public:
        MaterialGdtLoader(const GdtEntry& entry, MemoryManager* memory, ISearchPath* searchPath, IAssetLoadingManager* manager)
            : AbstractGdtEntryReader(entry),
              m_memory(memory),
              m_search_path(searchPath),
              m_manager(manager),
              m_state_map_cache(manager->GetAssetLoadingContext()->GetZoneAssetLoaderState<techset::TechniqueStateMapCache>()),
              m_material(nullptr),
              m_base_state_bits{}
        {
        }

        bool Load()
        {
            m_material = m_memory->Create<Material>();
            memset(m_material, 0, sizeof(Material));

            m_material->info.name = m_memory->Dup(m_entry.m_name.c_str());
            material_template();

            FinalizeMaterial();
            return true;
        }

        _NODISCARD Material* GetMaterial() const
        {
            return m_material;
        }

        _NODISCARD std::vector<XAssetInfoGeneric*> GetDependencies()
        {
            return std::move(m_dependencies);
        }

    private:
        void material_template()
        {
            const auto materialType = ReadStringProperty("materialType");

            if (materialType == GDT_MATERIAL_TYPE_MODEL_PHONG
                || materialType == GDT_MATERIAL_TYPE_WORLD_PHONG
                || materialType == GDT_MATERIAL_TYPE_IMPACT_MARK)
            {
                mtl_phong_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_MODEL_AMBIENT)
            {
                mtl_ambient_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_2D)
            {
                mtl_2d_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_MODEL_UNLIT
                || materialType == GDT_MATERIAL_TYPE_WORLD_UNLIT)
            {
                mtl_unlit_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_UNLIT)
            {
                mtl_unlit_deprecated_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_EFFECT)
            {
                mtl_effect_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_DISTORTION)
            {
                mtl_distortion_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_SPARK_FOUNTAIN)
            {
                mtl_sparkfountain_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_SPARK_CLOUD)
            {
                mtl_sparkcloud_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_PARTICLE_CLOUD)
            {
                mtl_particlecloud_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_TOOLS)
            {
                mtl_tools_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_SKY)
            {
                mtl_sky_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_WATER)
            {
                mtl_water_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_OBJECTIVE)
            {
                mtl_objective_template();
            }
            else if (materialType == GDT_MATERIAL_TYPE_CUSTOM)
            {
                custom_template();
            }
            else
            {
                std::ostringstream ss;
                ss << "Unknown material type: \"" << materialType << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void mtl_phong_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_ambient_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_2d_template()
        {
            commonsetup_template();

            SetTechniqueSet("2d");

            const auto colorMapName = ReadStringProperty("colorMap");
            const auto tileColor = ReadEnumProperty<TileMode_e>("tileColor", GdtTileModeNames, std::extent_v<decltype(GdtTileModeNames)>);
            const auto filterColor = ReadEnumProperty<GdtFilter_e>("filterColor", GdtSamplerFilterNames, std::extent_v<decltype(GdtSamplerFilterNames)>);

            if (!colorMapName.empty())
                AddMapTexture("colorMap", tileColor, filterColor, TS_2D, colorMapName);
            else
                throw GdtReadingException("ColorMap may not be blank in 2d materials");
        }

        void mtl_unlit_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_unlit_deprecated_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_effect_template()
        {
            // TODO
            throw SkipMaterialException();

            commonsetup_template();
            unitlitcommon_template();
        }

        void mtl_distortion_template()
        {
            commonsetup_template();

            const auto uvAnim = ReadBoolProperty("uvAnim");
            if (uvAnim)
                SetTechniqueSet("distortion_scale_ua_zfeather");
            else
                SetTechniqueSet("distortion_scale_zfeather");

            const auto colorMapName = ReadStringProperty("colorMap");
            const auto tileColor = ReadEnumProperty<TileMode_e>("tileColor", GdtTileModeNames, std::extent_v<decltype(GdtTileModeNames)>);
            const auto filterColor = ReadEnumProperty<GdtFilter_e>("filterColor", GdtSamplerFilterNames, std::extent_v<decltype(GdtSamplerFilterNames)>);

            if (!colorMapName.empty())
                AddMapTexture("colorMap", tileColor, filterColor, TS_COLOR_MAP, colorMapName);
            else
                throw GdtReadingException("ColorMap may not be blank in tools materials");

            const auto distortionScaleX = ReadFloatProperty("distortionScaleX");
            const auto distortionScaleY = ReadFloatProperty("distortionScaleY");
            AddConstant("distortionScale", Vector4f(distortionScaleX, distortionScaleY, 0, 0));

            if (uvAnim)
            {
                const auto uvScrollX = ReadFloatProperty("uvScrollX");
                const auto uvScrollY = ReadFloatProperty("uvScrollY");
                const auto uvScrollRotate = ReadFloatProperty("uvScrollRotate");
                AddConstant("uvAnimParms", Vector4f(uvScrollX, uvScrollY, uvScrollRotate, 0));
            }
        }

        void mtl_sparkfountain_template()
        {
            particlecloud_common_template("_sparkf");
        }

        void mtl_sparkcloud_template()
        {
            particlecloud_common_template("_spark");
        }

        void mtl_particlecloud_template()
        {
            particlecloud_common_template("");
        }

        void particlecloud_common_template(const std::string& particleCloudSuffix)
        {
            refblend_template();
            sort_template();
            clamp_template();

            SetTextureAtlas(1, 1);
            // tessSize(0)

            // hasEditorMaterial
            // allocLightmap

            statebits_template();

            std::string outdoorSuffix;
            const auto outdoorOnly = ReadBoolProperty("outdoorOnly");
            if (outdoorOnly)
                outdoorSuffix = "_outdoor";

            std::string blendFuncSuffix;
            const auto blendFunc = ReadStringProperty("blendFunc");
            if (blendFunc == GDT_BLEND_FUNC_ADD)
                blendFuncSuffix = "_add";
            else if(blendFunc == GDT_BLEND_FUNC_SCREEN_ADD)
                blendFuncSuffix = "_screen";

            std::string spotSuffix;
            const auto useSpotLight = ReadBoolProperty("useSpotLight");
            if (useSpotLight)
                spotSuffix = "_spot_sm";

            if (outdoorOnly && useSpotLight)
                throw GdtReadingException("Outdoor and spot aren't supported on particle cloud materials");

            std::ostringstream ss;
            ss << "particle_cloud" << particleCloudSuffix << outdoorSuffix << blendFuncSuffix << spotSuffix;
            SetTechniqueSet(ss.str());

            const auto colorMapName = ReadStringProperty("colorMap");
            const auto tileColor = ReadEnumProperty<TileMode_e>("tileColor", GdtTileModeNames, std::extent_v<decltype(GdtTileModeNames)>);
            const auto filterColor = ReadEnumProperty<GdtFilter_e>("filterColor", GdtSamplerFilterNames, std::extent_v<decltype(GdtSamplerFilterNames)>);

            if (!colorMapName.empty())
                AddMapTexture("colorMap", tileColor, filterColor, TS_COLOR_MAP, colorMapName);
            else
                throw GdtReadingException("ColorMap may not be blank in particle cloud materials");

            std::cout << "Using particlecloud for \"" << m_material->info.name << "\"\n";
        }

        void mtl_tools_template()
        {
            commonsetup_template();

            SetTechniqueSet("tools");

            AddMapTexture("normalMap", TileMode_e::NO_TILE, GdtFilter_e::NOMIP_NEAREST, TS_NORMAL_MAP, "$identitynormalmap");

            const auto colorMapName = ReadStringProperty("colorMap");
            const auto tileColor = ReadEnumProperty<TileMode_e>("tileColor", GdtTileModeNames, std::extent_v<decltype(GdtTileModeNames)>);
            const auto filterColor = ReadEnumProperty<GdtFilter_e>("filterColor", GdtSamplerFilterNames, std::extent_v<decltype(GdtSamplerFilterNames)>);

            if (!colorMapName.empty())
                AddMapTexture("colorMap", tileColor, filterColor, TS_COLOR_MAP, colorMapName);
            else
                throw GdtReadingException("ColorMap may not be blank in tools materials");

            const auto colorTint = ReadVec4Property("colorTint", {1.0f, 1.0f, 1.0f, 1.0f});
            AddConstant("colorTint", colorTint);
        }

        void mtl_sky_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_water_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_objective_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void custom_template()
        {
            const auto customTemplate = ReadStringProperty("customTemplate");

            if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_CUSTOM)
            {
                mtl_custom_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_PHONG_FLAG)
            {
                mtl_phong_flag_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_GRAIN_OVERLAY)
            {
                mtl_grain_overlay_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_EFFECT_EYE_OFFSET)
            {
                mtl_effect_eyeoffset_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_REFLEXSIGHT)
            {
                mtl_reflexsight_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_SHADOWCLEAR)
            {
                mtl_shadowclear_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_SHADOWOVERLAY)
            {
                mtl_shadowoverlay_template();
            }
            else if (customTemplate == GDT_CUSTOM_MATERIAL_TYPE_SPLATTER)
            {
                mtl_splatter_template();
            }
            else
            {
                std::ostringstream ss;
                ss << "Unknown custom template: \"" << customTemplate << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void mtl_custom_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_phong_flag_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_grain_overlay_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_effect_eyeoffset_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_reflexsight_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_shadowclear_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_shadowoverlay_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void mtl_splatter_template()
        {
            // TODO
            throw SkipMaterialException();
        }

        void unitlitcommon_template()
        {
            const auto outdoorOnly = ReadBoolProperty("outdoorOnly");
            const auto blendFunc = ReadStringProperty("blendFunc");

            std::string distFalloffSuffix;
            const auto distFalloff = ReadBoolProperty("distFalloff");
            if (distFalloff)
            {
                const auto hdrPortal = ReadBoolProperty("hdrPortal");
                if (!hdrPortal)
                    throw GdtReadingException("Cannot have distance falloff active without hdrPortal.");

                if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
                    throw GdtReadingException("Distance falloff does not currently support Multiply.");

                if (outdoorOnly)
                    throw GdtReadingException("Distance falloff does not currently support outdoor-only types.");

                distFalloffSuffix = "_falloff";
            }

            std::string godFalloffSuffix;
            const auto falloff = ReadBoolProperty("falloff");
            if (falloff)
            {
                if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
                    throw GdtReadingException("Falloff does not currently support Multiply.");

                if (outdoorOnly)
                    throw GdtReadingException("Falloff does not currently support outdoor-only types.");

                godFalloffSuffix = "_falloff";
            }

            std::string noFogSuffix;
            const auto noFog = ReadBoolProperty("noFog");
            if (noFog)
                noFogSuffix = "_nofog";

            std::string spotSuffix;
            const auto useSpotLight = ReadBoolProperty("useSpotLight");
            if (useSpotLight)
                spotSuffix = "_spot";

            std::string eyeOffsetSuffix;
            const auto eyeOffsetDepth = ReadFloatProperty("eyeOffsetDepth");
            if (std::fpclassify(eyeOffsetDepth) != FP_ZERO)
                eyeOffsetSuffix = "_eyeoffset";

            const auto materialType = ReadStringProperty("materialType");
            const auto zFeather = ReadBoolProperty("zFeather");

            if (materialType == GDT_MATERIAL_TYPE_EFFECT && zFeather)
            {
                if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
                    throw GdtReadingException("zFeather does not support multiply.");

                std::string addSuffix;
                if (blendFunc == GDT_BLEND_FUNC_ADD || blendFunc == GDT_BLEND_FUNC_SCREEN_ADD)
                    addSuffix = "_add";

                if (outdoorOnly)
                {
                    std::ostringstream ss;
                    ss << "effect_zfeather_outdoor" << addSuffix << noFogSuffix << spotSuffix << eyeOffsetSuffix;
                    SetTechniqueSet(ss.str());
                }
                else
                {
                    std::ostringstream ss;
                    ss << "effect_zfeather" << distFalloffSuffix << godFalloffSuffix << addSuffix << noFogSuffix << spotSuffix << eyeOffsetSuffix;
                    SetTechniqueSet(ss.str());
                }
            }
            else
            {
                std::string baseTechName = materialType == GDT_MATERIAL_TYPE_EFFECT ? "effect" : "unlit";

                if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
                {
                    std::ostringstream ss;
                    ss << baseTechName << "_multiply" << noFogSuffix << spotSuffix << eyeOffsetSuffix;
                    SetTechniqueSet(ss.str());
                }
                else
                {
                    std::string addSuffix;
                    if (blendFunc == GDT_BLEND_FUNC_ADD || blendFunc == GDT_BLEND_FUNC_SCREEN_ADD)
                        addSuffix = "_add";

                    std::ostringstream ss;
                    ss << baseTechName << distFalloffSuffix << godFalloffSuffix << addSuffix << noFogSuffix << spotSuffix << eyeOffsetSuffix;
                    SetTechniqueSet(ss.str());
                }
            }

            const auto colorMapName = ReadStringProperty("colorMap");
            const auto tileColor = ReadEnumProperty<TileMode_e>("tileColor", GdtTileModeNames, std::extent_v<decltype(GdtTileModeNames)>);
            const auto filterColor = ReadEnumProperty<GdtFilter_e>("filterColor", GdtSamplerFilterNames, std::extent_v<decltype(GdtSamplerFilterNames)>);

            if (!colorMapName.empty())
                AddMapTexture("colorMap", tileColor, filterColor, TS_COLOR_MAP, colorMapName);
            else
                throw GdtReadingException("ColorMap may not be blank in effect/unlit materials");

            if (falloff || distFalloff)
            {
                // TODO
            }

            if (zFeather)
            {
                const auto zFeatherDepth = ReadFloatProperty("zFeatherDepth");
                if (std::fpclassify(zFeatherDepth) == FP_ZERO)
                    throw GdtReadingException("zFeatherDepth may not be zero");
                AddConstant("featherParms", Vector4f(1.0f / zFeatherDepth, zFeatherDepth, 0, 0));
            }

            if (std::fpclassify(eyeOffsetDepth) != FP_ZERO)
                AddConstant("eyeOffsetParms", Vector4f(eyeOffsetDepth, 0, 0, 0));

            const auto colorTint = ReadVec4Property("colorTint", {1.0f, 1.0f, 1.0f, 1.0f});
            AddConstant("colorTint", colorTint);
        }

        void commonsetup_template()
        {
            refblend_template();
            sort_template();
            clamp_template();

            // tessSize

            textureAtlas_template();

            // hasEditorMaterial

            // allocLightmap

            statebits_template();
        }

        void refblend_template()
        {
            const auto blendFunc = ReadStringProperty("blendFunc");
        }

        void sort_template()
        {
            const auto sort = ReadStringProperty("sort");
            const auto materialType = ReadStringProperty("materialType");
            const auto polygonOffset = ReadStringProperty("polygonOffset");
            const auto blendFunc = ReadStringProperty("blendFunc");

            std::string sortKey;
            if (sort.empty() || sort == GDT_SORTKEY_DEFAULT)
            {
                if (materialType == GDT_MATERIAL_TYPE_DISTORTION)
                    sortKey = GDT_SORTKEY_DISTORTION;
                else if (polygonOffset == "Static Decal")
                    sortKey = GDT_SORTKEY_DECAL_STATIC;
                else if (polygonOffset == "Weapon Impact")
                    sortKey = GDT_SORTKEY_DECAL_WEAPON_IMPACT;
                else if (materialType == GDT_MATERIAL_TYPE_EFFECT)
                    sortKey = GDT_SORTKEY_EFFECT_AUTO_SORT;
                else if (materialType == GDT_MATERIAL_TYPE_OBJECTIVE
                    || blendFunc == "Blend" || blendFunc == "Add" || blendFunc == "Screen Add")
                    sortKey = GDT_SORTKEY_BLEND_ADDITIVE;
                    // else if (blendFunc == "Multiply") // TODO
                    //     sortKey = GDT_SORTKEY_MULTIPLICATIVE;
                else if (materialType == GDT_MATERIAL_TYPE_SKY)
                    sortKey = GDT_SORTKEY_SKY;
                else if (materialType == GDT_MATERIAL_TYPE_MODEL_AMBIENT)
                    sortKey = GDT_SORTKEY_OPAQUE_AMBIENT;
                else
                    sortKey = GDT_SORTKEY_OPAQUE;
            }
            else
                sortKey = sort;

            bool foundSortKey = false;
            for (auto sortKeyIndex = 0u; sortKeyIndex < SORTKEY_MAX; sortKeyIndex++)
            {
                if (SortKeyNames[sortKeyIndex] && sortKey == SortKeyNames[sortKeyIndex])
                {
                    SetSort(static_cast<unsigned char>(sortKeyIndex));
                    foundSortKey = true;
                    break;
                }
            }

            if (!foundSortKey)
            {
                char* endPtr;
                const auto sortKeyNum = strtoul(sortKey.c_str(), &endPtr, 10);

                if (endPtr != &sortKey[sortKey.size()])
                {
                    std::ostringstream ss;
                    ss << "Invalid sort value: \"" << sortKey << "\"";
                    throw GdtReadingException(ss.str());
                }

                SetSort(static_cast<unsigned char>(sortKeyNum));
            }
        }

        void clamp_template()
        {
        }

        void textureAtlas_template()
        {
            const auto rowCount = ReadIntegerProperty("textureAtlasRowCount", 1);
            const auto columnCount = ReadIntegerProperty("textureAtlasColumnCount", 1);

            SetTextureAtlas(static_cast<unsigned char>(rowCount), static_cast<unsigned char>(columnCount));
        }

        void statebits_template()
        {
            alphatest_template();
            blendfunc_template();
            colorwrite_template();
            cullface_template();
            depthtest_template();
            depthwrite_template();
            gammawrite_template();
            polygonoffset_template();
            stencil_template();
        }

        void alphatest_template()
        {
            const auto alphaTest = ReadStringProperty("alphaTest");

            if (alphaTest == GDT_ALPHA_TEST_ALWAYS)
                SetAlphaTest(AlphaTest_e::ALWAYS);
            else if (alphaTest == GDT_ALPHA_TEST_GE128)
                SetAlphaTest(AlphaTest_e::GE128);
            else if (alphaTest == GDT_ALPHA_TEST_GT0) // TODO: This is not available for IW3
                SetAlphaTest(AlphaTest_e::GT0);
            else
            {
                std::ostringstream ss;
                ss << "Invalid alphatest value: \"" << alphaTest << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void blendfunc_template()
        {
            const auto blendFunc = ReadStringProperty("blendFunc");

            if (blendFunc == GDT_BLEND_FUNC_ADD)
            {
                SetBlendFunc(BlendOp_e::ADD, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ONE);
                SetSeparateAlphaBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
            }
            else if (blendFunc == GDT_BLEND_FUNC_BLEND)
            {
                SetBlendFunc(BlendOp_e::ADD, CustomBlendFunc_e::SRC_ALPHA, CustomBlendFunc_e::INV_SRC_ALPHA);
                SetSeparateAlphaBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
            }
            else if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
            {
                SetBlendFunc(BlendOp_e::ADD, CustomBlendFunc_e::ZERO, CustomBlendFunc_e::SRC_COLOR);
                SetSeparateAlphaBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
            }
            else if (blendFunc == GDT_BLEND_FUNC_REPLACE)
            {
                SetBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
                SetSeparateAlphaBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
            }
            else if (blendFunc == GDT_BLEND_FUNC_SCREEN_ADD)
            {
                SetBlendFunc(BlendOp_e::ADD, CustomBlendFunc_e::INV_DST_COLOR, CustomBlendFunc_e::ONE);
                SetSeparateAlphaBlendFunc(BlendOp_e::DISABLE, CustomBlendFunc_e::ONE, CustomBlendFunc_e::ZERO);
            }
            else if (blendFunc == GDT_BLEND_FUNC_CUSTOM)
            {
                const auto customBlendOpRgb = ReadEnumProperty<BlendOp_e>("customBlendOpRgb", GdtBlendOpNames, std::extent_v<decltype(GdtBlendOpNames)>);
                const auto srcCustomBlendFunc = ReadEnumProperty<CustomBlendFunc_e>("srcCustomBlendFunc", GdtCustomBlendFuncNames, std::extent_v<decltype(GdtCustomBlendFuncNames)>);
                const auto destCustomBlendFunc = ReadEnumProperty<CustomBlendFunc_e>("destCustomBlendFunc", GdtCustomBlendFuncNames, std::extent_v<decltype(GdtCustomBlendFuncNames)>);
                const auto customBlendOpAlpha = ReadEnumProperty<BlendOp_e>("customBlendOpAlpha", GdtBlendOpNames, std::extent_v<decltype(GdtBlendOpNames)>);
                const auto srcCustomBlendFuncAlpha = ReadEnumProperty<CustomBlendFunc_e>("srcCustomBlendFuncAlpha", GdtCustomBlendFuncNames, std::extent_v<decltype(GdtCustomBlendFuncNames)>);
                const auto destCustomBlendFuncAlpha = ReadEnumProperty<CustomBlendFunc_e>("destCustomBlendFuncAlpha", GdtCustomBlendFuncNames, std::extent_v<decltype(GdtCustomBlendFuncNames)>);

                SetBlendFunc(customBlendOpRgb, srcCustomBlendFunc, destCustomBlendFunc);
                SetSeparateAlphaBlendFunc(customBlendOpAlpha, srcCustomBlendFuncAlpha, destCustomBlendFuncAlpha);
            }
            else
            {
                std::ostringstream ss;
                ss << "Invalid blendfunc value: \"" << blendFunc << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void colorwrite_template()
        {
            const auto colorWriteRed = ReadEnumProperty<StateBitsEnabledStatus_e>("colorWriteRed", GdtStateBitsEnabledStatusNames, std::extent_v<decltype(GdtStateBitsEnabledStatusNames)>);
            const auto colorWriteGreen = ReadEnumProperty<StateBitsEnabledStatus_e>("colorWriteGreen", GdtStateBitsEnabledStatusNames, std::extent_v<decltype(GdtStateBitsEnabledStatusNames)>);
            const auto colorWriteBlue = ReadEnumProperty<StateBitsEnabledStatus_e>("colorWriteBlue", GdtStateBitsEnabledStatusNames, std::extent_v<decltype(GdtStateBitsEnabledStatusNames)>);
            const auto colorWriteAlpha = ReadEnumProperty<StateBitsEnabledStatus_e>("colorWriteAlpha", GdtStateBitsEnabledStatusNames, std::extent_v<decltype(GdtStateBitsEnabledStatusNames)>);

            SetColorWrite(colorWriteRed, colorWriteGreen, colorWriteBlue, colorWriteAlpha);
        }

        void cullface_template()
        {
            const auto cullFace = ReadEnumProperty<CullFace_e>("cullFace", GdtCullFaceNames, std::extent_v<decltype(GdtCullFaceNames)>);

            SetCullFace(cullFace);
        }

        void depthtest_template()
        {
            const auto depthTest = ReadEnumProperty<DepthTest_e>("depthTest", GdtDepthTestNames, std::extent_v<decltype(GdtDepthTestNames)>);

            SetDepthTest(depthTest);
        }

        void depthwrite_template()
        {
            const auto depthWrite = ReadEnumProperty<StateBitsEnabledStatus_e>("depthWrite", GdtStateBitsOnOffStatusNames, std::extent_v<decltype(GdtStateBitsOnOffStatusNames)>);
            const auto blendFunc = ReadStringProperty("blendFunc");

            if (depthWrite == StateBitsEnabledStatus_e::ENABLED)
                SetDepthWrite(true);
            else if (depthWrite == StateBitsEnabledStatus_e::DISABLED)
                SetDepthWrite(false);
            else if (blendFunc == GDT_BLEND_FUNC_ADD)
                SetDepthWrite(false);
            else if (blendFunc == GDT_BLEND_FUNC_BLEND)
                SetDepthWrite(false);
            else if (blendFunc == GDT_BLEND_FUNC_MULTIPLY)
                SetDepthWrite(false);
            else if (blendFunc == GDT_BLEND_FUNC_REPLACE)
                SetDepthWrite(true);
            else if (blendFunc == GDT_BLEND_FUNC_SCREEN_ADD)
                SetDepthWrite(false);
            else if (blendFunc == GDT_BLEND_FUNC_CUSTOM)
                SetDepthWrite(false);
            else
            {
                std::ostringstream ss;
                ss << "Invalid depthWrite blendFunc value: \"" << blendFunc << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void gammawrite_template()
        {
            const auto gammaWrite = ReadEnumProperty<StateBitsEnabledStatus_e>("gammaWrite", GdtStateBitsOnOffStatusNames, std::extent_v<decltype(GdtStateBitsOnOffStatusNames)>);

            if (gammaWrite == StateBitsEnabledStatus_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Invalid gammaWrite blendFunc value: \"\"";
                throw GdtReadingException(ss.str());
            }

            SetGammaWrite(gammaWrite == StateBitsEnabledStatus_e::ENABLED);
        }

        void polygonoffset_template()
        {
            const auto polygonOffset = ReadEnumProperty<PolygonOffset_e>("polygonOffset", GdtPolygonOffsetNames, std::extent_v<decltype(GdtPolygonOffsetNames)>);

            SetPolygonOffset(polygonOffset);
        }

        void stencil_template()
        {
            const auto stencilMode = ReadEnumProperty<StencilMode_e>("stencil", GdtStencilModeNames, std::extent_v<decltype(GdtStencilModeNames)>);

            if (stencilMode == StencilMode_e::DISABLED)
            {
                DisableStencil(StencilIndex::FRONT);
                DisableStencil(StencilIndex::BACK);
            }
            else
            {
                if (stencilMode == StencilMode_e::TWO_SIDED)
                {
                    const auto stencilBackFunc = ReadEnumProperty<StencilFunc_e>("stencilFunc2", GdtStencilFuncNames, std::extent_v<decltype(GdtStencilFuncNames)>);
                    const auto stencilBackOpFail = ReadEnumProperty<StencilOp_e>("stencilOpFail2", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);
                    const auto stencilBackOpZFail = ReadEnumProperty<StencilOp_e>("stencilOpZFail2", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);
                    const auto stencilBackOpPass = ReadEnumProperty<StencilOp_e>("stencilOpPass2", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);

                    EnableStencil(StencilIndex::BACK, stencilBackFunc, stencilBackOpFail, stencilBackOpZFail, stencilBackOpPass);
                }

                const auto stencilFrontFunc = ReadEnumProperty<StencilFunc_e>("stencilFunc1", GdtStencilFuncNames, std::extent_v<decltype(GdtStencilFuncNames)>);
                const auto stencilFrontOpFail = ReadEnumProperty<StencilOp_e>("stencilOpFail1", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);
                const auto stencilFrontOpZFail = ReadEnumProperty<StencilOp_e>("stencilOpZFail1", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);
                const auto stencilFrontOpPass = ReadEnumProperty<StencilOp_e>("stencilOpPass1", GdtStencilOpNames, std::extent_v<decltype(GdtStencilOpNames)>);

                EnableStencil(StencilIndex::FRONT, stencilFrontFunc, stencilFrontOpFail, stencilFrontOpZFail, stencilFrontOpPass);
            }
        }

        void SetTechniqueSet(const std::string& techsetName)
        {
            auto* techset = reinterpret_cast<XAssetInfo<MaterialTechniqueSet>*>(m_manager->LoadDependency(ASSET_TYPE_TECHNIQUE_SET, techsetName));

            if (techset == nullptr)
            {
                std::ostringstream ss;
                ss << "Could not load techset: \"" << techsetName << "\"";
                throw GdtReadingException(ss.str());
            }

            m_dependencies.push_back(techset);
            m_material->techniqueSet = techset->Asset();

            auto* loadingContext = m_manager->GetAssetLoadingContext();
            auto* searchPath = loadingContext->m_raw_search_path;
            auto* definitionCache = loadingContext->GetZoneAssetLoaderState<techset::TechsetDefinitionCache>();

            const auto* techsetDefinition = AssetLoaderTechniqueSet::LoadTechsetDefinition(techsetName, searchPath, definitionCache);
            if (techsetDefinition == nullptr)
            {
                std::ostringstream ss;
                ss << "Could not find techset definition for: \"" << techsetName << "\"";
                throw GdtReadingException(ss.str());
            }

            SetTechniqueSetStateBits(techsetDefinition);
            SetTechniqueSetCameraRegion(techsetDefinition);
        }

        void SetTechniqueSetStateBits(const techset::TechsetDefinition* techsetDefinition)
        {
            for (auto i = 0; i < TECHNIQUE_COUNT; i++)
            {
                std::string techniqueName;
                if (techsetDefinition->GetTechniqueByIndex(i, techniqueName))
                {
                    const auto stateBitsForTechnique = GetStateBitsForTechnique(techniqueName);
                    const auto foundStateBits = std::find_if(m_state_bits.begin(), m_state_bits.end(),
                                                             [stateBitsForTechnique](const GfxStateBits& s1)
                                                             {
                                                                 return s1.loadBits[0] == stateBitsForTechnique.loadBits[0] && s1.loadBits[1] == stateBitsForTechnique.loadBits[1];
                                                             });

                    if (foundStateBits != m_state_bits.end())
                    {
                        m_material->stateBitsEntry[i] = static_cast<unsigned char>(foundStateBits - m_state_bits.begin());
                    }
                    else
                    {
                        m_material->stateBitsEntry[i] = static_cast<unsigned char>(m_state_bits.size());
                        m_state_bits.push_back(stateBitsForTechnique);
                    }
                }
                else
                {
                    m_material->stateBitsEntry[i] = std::numeric_limits<unsigned char>::max();
                }
            }
        }

        GfxStateBits GetStateBitsForTechnique(const std::string& techniqueName)
        {
            const auto* stateMap = AssetLoaderTechniqueSet::GetStateMapForTechnique(techniqueName, m_search_path, m_state_map_cache);
            if (!stateMap)
                return m_base_state_bits;

            const auto preCalculatedStateBits = m_state_bits_per_state_map.find(stateMap);
            if (preCalculatedStateBits != m_state_bits_per_state_map.end())
                return preCalculatedStateBits->second;

            const auto stateBits = CalculateStateBitsWithStateMap(stateMap);
            m_state_bits_per_state_map.emplace(stateMap, stateBits);

            return stateBits;
        }

        GfxStateBits CalculateStateBitsWithStateMap(const state_map::StateMapDefinition* stateMap) const
        {
            const state_map::StateMapHandler stateMapHandler(stateMapLayout, *stateMap);

            GfxStateBits outBits{};
            stateMapHandler.ApplyStateMap(m_base_state_bits.loadBits, outBits.loadBits);

            return outBits;
        }

        void SetTechniqueSetCameraRegion(const techset::TechsetDefinition* techsetDefinition) const
        {
            std::string tempName;
            if (techsetDefinition->GetTechniqueByIndex(TECHNIQUE_LIT, tempName))
            {
                if (m_material->info.sortKey >= SORTKEY_TRANS_START)
                    m_material->cameraRegion = CAMERA_REGION_LIT_TRANS;
                else
                    m_material->cameraRegion = CAMERA_REGION_LIT_OPAQUE;
            }
            else if (techsetDefinition->GetTechniqueByIndex(TECHNIQUE_EMISSIVE, tempName))
            {
                m_material->cameraRegion = CAMERA_REGION_EMISSIVE;
            }
            else
            {
                m_material->cameraRegion = CAMERA_REGION_NONE;
            }
        }

        void AddMapTexture(const std::string& typeName, const TileMode_e tileMode, GdtFilter_e filterMode, const TextureSemantic semantic, const std::string& textureName)
        {
            MaterialTextureDef textureDef{};
            textureDef.nameHash = Common::R_HashString(typeName.c_str());
            textureDef.nameStart = typeName[0];
            textureDef.nameEnd = typeName[typeName.size() - 1];
            textureDef.samplerState = 0;
            textureDef.semantic = static_cast<unsigned char>(semantic);

            switch (tileMode)
            {
            case TileMode_e::TILE_BOTH:
                textureDef.samplerState |= SAMPLER_CLAMP_U | SAMPLER_CLAMP_V | SAMPLER_CLAMP_W;
                break;
            case TileMode_e::TILE_HORIZONTAL:
                textureDef.samplerState |= SAMPLER_CLAMP_V;
                break;
            case TileMode_e::TILE_VERTICAL:
                textureDef.samplerState |= SAMPLER_CLAMP_U;
                break;
            case TileMode_e::UNKNOWN:
            case TileMode_e::NO_TILE:
                break;
            default:
                assert(false);
                break;
            }

            switch (filterMode)
            {
            case GdtFilter_e::MIP_2X_BILINEAR:
                textureDef.samplerState |= SAMPLER_FILTER_ANISO2X | SAMPLER_MIPMAP_NEAREST;
                break;
            case GdtFilter_e::MIP_2X_TRILINEAR:
                textureDef.samplerState |= SAMPLER_FILTER_ANISO2X | SAMPLER_MIPMAP_LINEAR;
                break;
            case GdtFilter_e::MIP_4X_BILINEAR:
                textureDef.samplerState |= SAMPLER_FILTER_ANISO4X | SAMPLER_MIPMAP_NEAREST;
                break;
            case GdtFilter_e::MIP_4X_TRILINEAR:
                textureDef.samplerState |= SAMPLER_FILTER_ANISO4X | SAMPLER_MIPMAP_LINEAR;
                break;
            case GdtFilter_e::NOMIP_NEAREST:
                textureDef.samplerState |= SAMPLER_FILTER_NEAREST | SAMPLER_MIPMAP_DISABLED;
                break;
            case GdtFilter_e::NOMIP_BILINEAR:
                textureDef.samplerState |= SAMPLER_FILTER_LINEAR | SAMPLER_MIPMAP_DISABLED;
                break;
            default:
                assert(false);
                break;
            }

            auto* image = reinterpret_cast<XAssetInfo<GfxImage>*>(m_manager->LoadDependency(ASSET_TYPE_IMAGE, textureName));

            if (image == nullptr)
            {
                std::ostringstream ss;
                ss << "Could not load image: \"" << textureName << "\"";
                throw GdtReadingException(ss.str());
            }

            m_dependencies.push_back(image);
            textureDef.u.image = image->Asset();

            m_textures.push_back(textureDef);
        }

        void AddConstant(const std::string& constantName, Vector4f literalData)
        {
            MaterialConstantDef constantDef{};
            constantDef.literal[0] = literalData(0);
            constantDef.literal[1] = literalData(1);
            constantDef.literal[2] = literalData(2);
            constantDef.literal[3] = literalData(3);
            strncpy(constantDef.name, constantName.c_str(), std::extent_v<decltype(MaterialConstantDef::name)>);
            constantDef.nameHash = Common::R_HashString(constantName.c_str());

            m_constants.push_back(constantDef);
        }

        void SetSort(const unsigned char sort) const
        {
            m_material->info.sortKey = sort;
        }

        void SetTextureAtlas(const unsigned char rowCount, const unsigned char columnCount) const
        {
            m_material->info.textureAtlasRowCount = rowCount;
            m_material->info.textureAtlasColumnCount = columnCount;
        }

        void SetAlphaTest(const AlphaTest_e alphaTest)
        {
            switch (alphaTest)
            {
            case AlphaTest_e::ALWAYS:
                m_base_state_bits.loadBits[0] |= GFXS0_ATEST_DISABLE;
                break;

            case AlphaTest_e::GT0:
                m_base_state_bits.loadBits[0] |= GFXS0_ATEST_GT_0;
                break;

            case AlphaTest_e::LT128:
                m_base_state_bits.loadBits[0] |= GFXS0_ATEST_LT_128;
                break;

            case AlphaTest_e::GE128:
                m_base_state_bits.loadBits[0] |= GFXS0_ATEST_GE_128;
                break;

            case AlphaTest_e::UNKNOWN:
            default:
                std::ostringstream ss;
                ss << "Unknown alphatest value: \"" << static_cast<int>(alphaTest) << "\"";
                throw GdtReadingException(ss.str());
            }
        }

        void SetBlendFunc(BlendOp_e blendOp, CustomBlendFunc_e srcFunc, CustomBlendFunc_e destFunc)
        {
            if (blendOp == BlendOp_e::UNKNOWN || srcFunc == CustomBlendFunc_e::UNKNOWN || destFunc == CustomBlendFunc_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Unknown SeparateAlphaBlendFunc values: \"\"";
                throw GdtReadingException(ss.str());
            }

            m_base_state_bits.loadBits[0] &= ~GFXS0_BLENDOP_RGB_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(blendOp) - 1) << GFXS0_BLENDOP_RGB_SHIFT) & GFXS0_BLENDOP_RGB_MASK;

            m_base_state_bits.loadBits[0] &= ~GFXS0_SRCBLEND_RGB_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(srcFunc) - 1) << GFXS0_SRCBLEND_RGB_SHIFT) & GFXS0_SRCBLEND_RGB_MASK;

            m_base_state_bits.loadBits[0] &= ~GFXS0_DSTBLEND_RGB_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(destFunc) - 1) << GFXS0_DSTBLEND_RGB_SHIFT) & GFXS0_DSTBLEND_RGB_MASK;
        }

        void SetSeparateAlphaBlendFunc(BlendOp_e blendOp, CustomBlendFunc_e srcFunc, CustomBlendFunc_e destFunc)
        {
            if (blendOp == BlendOp_e::UNKNOWN || srcFunc == CustomBlendFunc_e::UNKNOWN || destFunc == CustomBlendFunc_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Unknown SeparateAlphaBlendFunc values: \"\"";
                throw GdtReadingException(ss.str());
            }

            m_base_state_bits.loadBits[0] &= ~GFXS0_BLENDOP_ALPHA_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(blendOp) - 1) << GFXS0_BLENDOP_ALPHA_SHIFT) & GFXS0_BLENDOP_ALPHA_MASK;

            m_base_state_bits.loadBits[0] &= ~GFXS0_SRCBLEND_ALPHA_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(srcFunc) - 1) << GFXS0_SRCBLEND_ALPHA_SHIFT) & GFXS0_SRCBLEND_ALPHA_MASK;

            m_base_state_bits.loadBits[0] &= ~GFXS0_DSTBLEND_ALPHA_MASK;
            m_base_state_bits.loadBits[0] |= ((static_cast<unsigned>(destFunc) - 1) << GFXS0_DSTBLEND_ALPHA_SHIFT) & GFXS0_DSTBLEND_ALPHA_MASK;
        }

        void SetColorWrite(const StateBitsEnabledStatus_e colorWriteRed, const StateBitsEnabledStatus_e colorWriteGreen, const StateBitsEnabledStatus_e colorWriteBlue,
                           const StateBitsEnabledStatus_e colorWriteAlpha)
        {
            if (colorWriteRed == StateBitsEnabledStatus_e::UNKNOWN || colorWriteGreen == StateBitsEnabledStatus_e::UNKNOWN
                || colorWriteBlue == StateBitsEnabledStatus_e::UNKNOWN || colorWriteAlpha == StateBitsEnabledStatus_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Unknown ColorWrite values: \"\"";
                throw GdtReadingException(ss.str());
            }

            if (colorWriteRed != colorWriteGreen || colorWriteRed != colorWriteBlue)
            {
                std::ostringstream ss;
                ss << "Invalid ColorWrite values: values for rgb must match";
                throw GdtReadingException(ss.str());
            }

            m_base_state_bits.loadBits[0] &= ~GFXS0_COLORWRITE_MASK;
            if (colorWriteRed == StateBitsEnabledStatus_e::ENABLED)
                m_base_state_bits.loadBits[0] |= GFXS0_COLORWRITE_RGB;
            if (colorWriteAlpha == StateBitsEnabledStatus_e::ENABLED)
                m_base_state_bits.loadBits[0] |= GFXS0_COLORWRITE_ALPHA;
        }

        void SetCullFace(const CullFace_e cullFace)
        {
            if (cullFace == CullFace_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Unknown cullFace values: \"\"";
                throw GdtReadingException(ss.str());
            }

            m_base_state_bits.loadBits[0] &= ~GFXS0_CULL_MASK;

            if (cullFace == CullFace_e::FRONT)
            {
                m_base_state_bits.loadBits[0] |= GFXS0_CULL_FRONT;
            }
            else if (cullFace == CullFace_e::BACK)
            {
                m_base_state_bits.loadBits[0] |= GFXS0_CULL_BACK;
            }
            else
            {
                assert(cullFace == CullFace_e::NONE);
                m_base_state_bits.loadBits[0] |= GFXS0_CULL_NONE;
            }
        }

        void SetDepthTest(const DepthTest_e depthTest)
        {
            m_base_state_bits.loadBits[1] &= GFXS1_DEPTHTEST_MASK;

            switch (depthTest)
            {
            case DepthTest_e::LESS_EQUAL:
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHTEST_LESSEQUAL;
                break;

            case DepthTest_e::LESS:
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHTEST_LESS;
                break;

            case DepthTest_e::EQUAL:
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHTEST_EQUAL;
                break;

            case DepthTest_e::ALWAYS:
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHTEST_ALWAYS;
                break;

            case DepthTest_e::DISABLE:
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHTEST_DISABLE;
                break;

            case DepthTest_e::UNKNOWN:
            default:
                std::ostringstream ss;
                ss << "Unknown depthTest values: \"\"";
                throw GdtReadingException(ss.str());
            }
        }

        void SetDepthWrite(const bool depthWrite)
        {
            m_base_state_bits.loadBits[1] &= ~GFXS1_DEPTHWRITE;

            if (depthWrite)
                m_base_state_bits.loadBits[1] |= GFXS1_DEPTHWRITE;
        }

        void SetGammaWrite(const bool gammaWrite)
        {
            m_base_state_bits.loadBits[0] &= ~GFXS0_GAMMAWRITE;

            if (gammaWrite)
                m_base_state_bits.loadBits[0] |= GFXS0_GAMMAWRITE;
        }

        void SetPolygonOffset(const PolygonOffset_e polygonOffset)
        {
            if (polygonOffset == PolygonOffset_e::UNKNOWN)
            {
                std::ostringstream ss;
                ss << "Unknown polygonOffset values: \"\"";
                throw GdtReadingException(ss.str());
            }

            m_base_state_bits.loadBits[1] &= ~GFXS1_POLYGON_OFFSET_MASK;
            m_base_state_bits.loadBits[1] |= ((static_cast<unsigned>(polygonOffset) - 1) << GFXS1_POLYGON_OFFSET_SHIFT) & GFXS1_POLYGON_OFFSET_MASK;
        }

        static void GetStencilMasksForIndex(const StencilIndex stencil, unsigned& enabledMask, unsigned& funcShift, unsigned& funcMask, unsigned& opFailShift, unsigned& opFailMask,
                                            unsigned& opZFailShift, unsigned& opZFailMask, unsigned& opPassShift, unsigned& opPassMask)
        {
            if (stencil == StencilIndex::FRONT)
            {
                enabledMask = GFXS1_STENCIL_FRONT_ENABLE;
                funcShift = GFXS1_STENCIL_FRONT_FUNC_SHIFT;
                funcMask = GFXS1_STENCIL_FRONT_FUNC_MASK;
                opFailShift = GFXS1_STENCIL_FRONT_FAIL_SHIFT;
                opFailMask = GFXS1_STENCIL_FRONT_FAIL_MASK;
                opZFailShift = GFXS1_STENCIL_FRONT_ZFAIL_SHIFT;
                opZFailMask = GFXS1_STENCIL_FRONT_ZFAIL_MASK;
                opPassShift = GFXS1_STENCIL_FRONT_PASS_SHIFT;
                opPassMask = GFXS1_STENCIL_FRONT_PASS_MASK;
            }
            else
            {
                assert(stencil == StencilIndex::BACK);

                enabledMask = GFXS1_STENCIL_BACK_ENABLE;
                funcShift = GFXS1_STENCIL_BACK_FUNC_SHIFT;
                funcMask = GFXS1_STENCIL_BACK_FUNC_MASK;
                opFailShift = GFXS1_STENCIL_BACK_FAIL_SHIFT;
                opFailMask = GFXS1_STENCIL_BACK_FAIL_MASK;
                opZFailShift = GFXS1_STENCIL_BACK_ZFAIL_SHIFT;
                opZFailMask = GFXS1_STENCIL_BACK_ZFAIL_MASK;
                opPassShift = GFXS1_STENCIL_BACK_PASS_SHIFT;
                opPassMask = GFXS1_STENCIL_BACK_PASS_MASK;
            }
        }

        void DisableStencil(const StencilIndex stencil)
        {
            unsigned enabledMask, funcShift, funcMask, opFailShift, opFailMask, opZFailShift, opZFailMask, opPassShift, opPassMask;
            GetStencilMasksForIndex(stencil, enabledMask, funcShift, funcMask, opFailShift, opFailMask, opZFailShift, opZFailMask, opPassShift, opPassMask);

            m_base_state_bits.loadBits[1] &= ~(enabledMask | funcMask | opFailMask | opZFailMask | opPassMask);
        }

        void EnableStencil(const StencilIndex stencil, StencilFunc_e stencilFunc, StencilOp_e stencilOpFail, StencilOp_e stencilOpZFail, StencilOp_e stencilOpPass)
        {
            unsigned enabledMask, funcShift, funcMask, opFailShift, opFailMask, opZFailShift, opZFailMask, opPassShift, opPassMask;
            GetStencilMasksForIndex(stencil, enabledMask, funcShift, funcMask, opFailShift, opFailMask, opZFailShift, opZFailMask, opPassShift, opPassMask);

            m_base_state_bits.loadBits[1] |= enabledMask;

            m_base_state_bits.loadBits[1] &= ~funcMask;
            m_base_state_bits.loadBits[1] |= ((static_cast<unsigned>(stencilFunc) - 1) << funcShift) & funcMask;

            m_base_state_bits.loadBits[1] &= ~opFailMask;
            m_base_state_bits.loadBits[1] |= ((static_cast<unsigned>(stencilOpFail) - 1) << opFailShift) & opFailMask;

            m_base_state_bits.loadBits[1] &= ~opZFailMask;
            m_base_state_bits.loadBits[1] |= ((static_cast<unsigned>(stencilOpZFail) - 1) << opZFailShift) & opZFailMask;

            m_base_state_bits.loadBits[1] &= ~opPassMask;
            m_base_state_bits.loadBits[1] |= ((static_cast<unsigned>(stencilOpPass) - 1) << opPassShift) & opPassMask;
        }

        void FinalizeMaterial() const
        {
            if (!m_textures.empty())
            {
                m_material->textureTable = static_cast<MaterialTextureDef*>(m_memory->Alloc(sizeof(MaterialTextureDef) * m_textures.size()));
                m_material->textureCount = static_cast<unsigned char>(m_textures.size());
                memcpy(m_material->textureTable, m_textures.data(), sizeof(MaterialTextureDef) * m_textures.size());
            }
            else
            {
                m_material->textureTable = nullptr;
                m_material->textureCount = 0u;
            }

            if (!m_constants.empty())
            {
                m_material->constantTable = static_cast<MaterialConstantDef*>(m_memory->Alloc(sizeof(MaterialConstantDef) * m_constants.size()));
                m_material->constantCount = static_cast<unsigned char>(m_constants.size());
                memcpy(m_material->constantTable, m_constants.data(), sizeof(MaterialConstantDef) * m_constants.size());
            }
            else
            {
                m_material->constantTable = nullptr;
                m_material->constantCount = 0u;
            }

            if (!m_state_bits.empty())
            {
                m_material->stateBitsTable = static_cast<GfxStateBits*>(m_memory->Alloc(sizeof(GfxStateBits) * m_state_bits.size()));
                m_material->stateBitsCount = static_cast<unsigned char>(m_state_bits.size());
                memcpy(m_material->stateBitsTable, m_state_bits.data(), sizeof(GfxStateBits) * m_state_bits.size());
            }
            else
            {
                m_material->stateBitsTable = nullptr;
                m_material->stateBitsCount = 0u;
            }
        }

        static size_t GetIndexForString(const std::string& propertyName, const std::string& value, const char** validValuesArray, const size_t validValuesArraySize)
        {
            for (auto i = 0u; i < validValuesArraySize; i++)
            {
                if (validValuesArray[i] == value)
                    return i;
            }

            std::ostringstream ss;
            ss << "Unknown " << propertyName << " value: \"" << value << "\"";
            throw GdtReadingException(ss.str());
        }

        template <typename T>
        T ReadEnumProperty(const std::string& propertyName, const char** validValuesArray, const size_t validValuesArraySize) const
        {
            return static_cast<T>(GetIndexForString(propertyName, ReadStringProperty(propertyName), validValuesArray, validValuesArraySize));
        }

        MemoryManager* m_memory;
        ISearchPath* m_search_path;
        IAssetLoadingManager* m_manager;
        techset::TechniqueStateMapCache* m_state_map_cache;
        std::unordered_map<const state_map::StateMapDefinition*, GfxStateBits> m_state_bits_per_state_map;
        std::vector<XAssetInfoGeneric*> m_dependencies;

        Material* m_material;
        GfxStateBits m_base_state_bits;
        std::vector<GfxStateBits> m_state_bits;
        std::vector<MaterialTextureDef> m_textures;
        std::vector<MaterialConstantDef> m_constants;
    };
}

void* AssetLoaderMaterial::CreateEmptyAsset(const std::string& assetName, MemoryManager* memory)
{
    auto* material = memory->Create<Material>();
    memset(material, 0, sizeof(Material));
    material->info.name = memory->Dup(assetName.c_str());
    return material;
}

bool AssetLoaderMaterial::CanLoadFromGdt() const
{
    return true;
}

bool AssetLoaderMaterial::LoadFromGdt(const std::string& assetName, IGdtQueryable* gdtQueryable, MemoryManager* memory, IAssetLoadingManager* manager, Zone* zone) const
{
    const auto* entry = gdtQueryable->GetGdtEntryByGdfAndName(ObjConstants::GDF_FILENAME_MATERIAL, assetName);
    if (!entry)
        return false;

    MaterialGdtLoader loader(*entry, memory, manager->GetAssetLoadingContext()->m_raw_search_path, manager);

    try
    {
        if (loader.Load())
            manager->AddAsset(ASSET_TYPE_MATERIAL, assetName, loader.GetMaterial(), loader.GetDependencies(), std::vector<scr_string_t>());
    }
    catch (const SkipMaterialException&)
    {
        return false;
    }
    catch (const GdtReadingException& e)
    {
        std::cerr << "Error while trying to load material from gdt: " << e.what() << " @ GdtEntry \"" << entry->m_name << "\"\n";
        return false;
    }

    return true;
}
