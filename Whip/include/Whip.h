#pragma once
#ifndef _WHIP_
#define _WHIP_

// Core files
#include <Whip/Core/Core.h>
#include <Whip/Helper/Buffer.h>
#include <Whip/Core/Memory.h>
#include <Whip/Utils/Utility.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip/Core/UUID.h>

// Whip logging system
#include <Whip/Core/Log.h>

// Whip Application
#include <Whip/Core/Application.h>
#include <Whip/Core/AsyncJobSystem.h>

// Math
#include <Whip/Math/Math.h>

// Whip time stuff
#include <Whip/Core/Timestep.h>
#include <Whip/Helper/TimerManager.h>

// Input stuff
#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>

//project
#include <Whip/Project/Project.h>
#include <Whip/Project/PlayerConfig.h>

// scene
#include <Whip/Scene/Components.h>
#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Scene.h>

// -------- Whip render works ----------
#include <Whip/Render/Renderer.h>
#include <Whip/Render/Renderer2D.h>
#include <Whip/Render/RenderCommand.h>

#include <Whip/Render/Buffer.h>
#include <Whip/Render/Framebuffer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/SubTexture2D.h>
#include <Whip/Render/VertexArray.h>

// Whip camera
#include <Whip/Render/OrthographicCamera.h>
#include <Whip/Render/OrthographicCameraController.h>

// Animation
#include <Whip/Animation/Animation2D.h>
#include <Whip/Animation/AnimationManager.h>
#include <Whip/Animation/AnimationController.h>

// Audio
#include <Whip/Audio/AudioSource.h>
#include <Whip/Audio/AudioEngine.h>
// -------------------------------------

#include <Whip/Asset/Asset.h>
#include <Whip/Asset/AnimationImporter.h>
#include <Whip/Asset/AudioImporter.h>
#include <Whip/Asset/FontImporter.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Asset/TextureImporter.h>
#include <Whip/Asset/TextureSlicer.h>
#include <Whip/Asset/AssetImporter.h>
#include <Whip/Asset/EditorAssetManager.h>
#include <Whip/Asset/RuntimeAssetManager.h>
#include <Whip/Asset/AssetManager.h>

#include <Whip/Texture/TextureManager.h>

// coco -> do we realy need this?
#include <coco/coco.h>
#endif // ! _WHIP_
