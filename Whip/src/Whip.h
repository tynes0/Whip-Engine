#pragma once
#ifndef _WHIP_
#define _WHIP_

// Core files
#include <Whip/Core/Core.h>
#include <Whip/Core/buffer.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/utility.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/Core/UUID.h>

// Whip logging system
#include <Whip/Core/Log.h>

// Whip application
#include <Whip/Core/Application.h>

// Math
#include <Whip/Math/math.h>

// Whip time stuff
#include <Whip/Core/Timestep.h>
#include <Whip/Core/timer_manager.h>

// input stuff
#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>

//project
#include <Whip/Project/project.h>

// scene
#include <Whip/Scene/components.h>
#include <Whip/Scene/entity.h>
#include <Whip/Scene/scriptable_entity.h>
#include <Whip/Scene/scene.h>

// -------- Whip render works ----------
#include <Whip/Render/Renderer.h>
#include <Whip/Render/Renderer2D.h>
#include <Whip/Render/RenderCommand.h>

#include <Whip/Render/Buffer.h>
#include <Whip/Render/framebuffer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/SubTexture2D.h>
#include <Whip/Render/VertexArray.h>

// Whip camera
#include <Whip/Render/orthographic_camera.h>
#include <Whip/Render/OrthographicCameraController.h>

// Animation
#include <Whip/Animation/animation2D.h>
#include <Whip/Animation/animation_manager.h>

// Audio
#include <Whip/Audio/audio_source.h>
#include <Whip/Audio/audio_engine.h>
// -------------------------------------

#include <Whip/Asset/asset.h>
#include <Whip/Asset/animation_importer.h>
#include <Whip/Asset/audio_importer.h>
#include <Whip/Asset/font_importer.h>
#include <Whip/Asset/scene_importer.h>
#include <Whip/Asset/texture_importer.h>
#include <Whip/Asset/asset_importer.h>
#include <Whip/Asset/editor_asset_manager.h>
#include <Whip/Asset/runtime_asset_manager.h>
#include <Whip/Asset/asset_manager.h>

// coco -> do we realy need this?
#include <coco/coco.h>
#endif // ! _WHIP_
