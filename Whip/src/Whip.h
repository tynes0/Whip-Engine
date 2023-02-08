#pragma once
#ifndef _WHIP_
#define _WHIP_

// Whip application
#include <Whip/Core/Application.h>

// Whip logging system
#include <Whip/Core/Log.h>

// Layer stuff and imGui layer
#include <Whip/ImGui/ImGuiLayer.h>

// Whip timestep class
#include <Whip/Core/Timestep.h>

// input stuff
#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>

// -------- Whip render works ----------
#include <Whip/Render/Renderer.h>
#include <Whip/Render/RenderCommand.h>

// Whip shader - texture - buffer - vertexArray
#include <Whip/Render/Buffer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/VertexArray.h>

// Whip camera
#include <Whip/Render/Camera.h>
#include <Whip/Render/OrthographicCameraController.h>
// -------------------------------------

// random class 
#include <Whip/Core/Random.h>

// ----- Giris Noktasi -----
#include <Whip/Core/EntryPoint.h>
// -------------------------

#endif // ! _WHIP_