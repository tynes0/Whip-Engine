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
#include <Whip/Core/WhipTemplateLibrary/Algorithms.h>
#include <Whip/Core/WhipTemplateLibrary/Allocator.h>
#include <Whip/Core/WhipTemplateLibrary/Any.h>
#include <Whip/Core/WhipTemplateLibrary/ArithmeticArray.h> // only c++20 and above
#include <Whip/Core/WhipTemplateLibrary/Array.h>
#include <Whip/Core/WhipTemplateLibrary/Bitset.h>
#include <Whip/Core/WhipTemplateLibrary/Concepts.h> // only whith c++20 concept support
#include <Whip/Core/WhipTemplateLibrary/OperatorWrapper.h>
#include <Whip/Core/WhipTemplateLibrary/Filesystem.h>
#include <Whip/Core/WhipTemplateLibrary/Hash.h>
#include <Whip/Core/WhipTemplateLibrary/Invoker.h>
#include <Whip/Core/WhipTemplateLibrary/Iterator.h>
#include <Whip/Core/WhipTemplateLibrary/MathDef.h>
#include <Whip/Core/WhipTemplateLibrary/Memory.h>
#include <Whip/Core/WhipTemplateLibrary/MemoryUtil.h>
#include <Whip/Core/WhipTemplateLibrary/Optional.h>
#include <Whip/Core/WhipTemplateLibrary/Pair.h>
#include <Whip/Core/WhipTemplateLibrary/Random.h> // xoshiro is only c++20 and above
#include <Whip/Core/WhipTemplateLibrary/Range.h>
#include <Whip/Core/WhipTemplateLibrary/ReferenceWrapper.h>
#include <Whip/Core/WhipTemplateLibrary/Span.h> // only c++20 and above
#include <Whip/Core/WhipTemplateLibrary/StringOperations.h>
#include <Whip/Core/WhipTemplateLibrary/Tag.h> 
#include <Whip/Core/WhipTemplateLibrary/Tuple.h> // only c++17 and above
#include <Whip/Core/WhipTemplateLibrary/TypeTraits.h> // only c++17 and above
#include <Whip/Core/WhipTemplateLibrary/Utility.h>
#include <Whip/Core/WhipTemplateLibrary/Variant.h> // only c++20 and above
#include <Whip/Core/WhipTemplateLibrary/Vector.h>

#endif // ! _WHIP_