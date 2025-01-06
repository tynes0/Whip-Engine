#pragma once
#ifndef _WHIP_
#define _WHIP_

// Core file
#include <Whip/Core/Core.h>

// Whip logging system
#include <Whip/Core/Log.h>

// Whip application
#include <Whip/Core/Application.h>

// Layer stuff and imGui layer
#include <Whip/ImGui/ImGuiLayer.h>

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
// -------------------------------------

#include <coco/coco.h>
#endif // ! _WHIP_
