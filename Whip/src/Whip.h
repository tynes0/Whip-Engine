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

// Whip timestep class
#include <Whip/Core/Timestep.h>

// input stuff
#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>

// -------- Whip render works ----------
#include <Whip/Render/Renderer.h>
#include <Whip/Render/Renderer2D.h>
#include <Whip/Render/RenderCommand.h>

// Whip shader - texture - buffer - vertexArray
#include <Whip/Render/Buffer.h>
#include <Whip/Render/Shader.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/SubTexture2D.h>
#include <Whip/Render/VertexArray.h>

// Whip camera
#include <Whip/Render/Camera.h>
#include <Whip/Render/OrthographicCameraController.h>
// -------------------------------------

// Templates And Containers
#include <Whip/Core/TemplatesAndContainers/MathDef.h>
#include <Whip/Core/TemplatesAndContainers/Vector.h>
#include <Whip/Core/TemplatesAndContainers/Array.h>
#include <Whip/Core/TemplatesAndContainers/Bitset.h>
#include <Whip/Core/TemplatesAndContainers/ArithmeticArray.h> // only c++20 and above
#include <Whip/Core/TemplatesAndContainers/Span.h> // only c++20 and above
#include <Whip/Core/TemplatesAndContainers/Any.h>
#include <Whip/Core/TemplatesAndContainers/Optional.h>
#include <Whip/Core/TemplatesAndContainers/Pair.h>
#include <Whip/Core/TemplatesAndContainers/Tuple.h> // only c++17 and above
#include <Whip/Core/TemplatesAndContainers/Variant.h> // only c++20 and above
#include <Whip/Core/TemplatesAndContainers/Hash.h>
#include <Whip/Core/TemplatesAndContainers/Algorithms.h>
#include <Whip/Core/TemplatesAndContainers/StringOperations.h>
#include <Whip/Core/TemplatesAndContainers/Filesystem.h>
#include <Whip/Core/TemplatesAndContainers/Range.h>
#include <Whip/Core/TemplatesAndContainers/Random.h> // xoshiro is only c++20 and above

#endif // ! _WHIP_