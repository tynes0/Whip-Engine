#include "WhipPch.h"
#include <Whip/Utils/PlatformUtils.h>


#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <Whip/Core/Application.h>

#include <vector>

_WHIP_START

namespace
{
	bool ShellExecuteOpen(const std::filesystem::path& target, const wchar_t* parameters, const std::filesystem::path& workingDirectory)
	{
		const std::wstring targetString = target.wstring();
		const std::wstring directoryString = workingDirectory.empty() ? std::wstring() : workingDirectory.wstring();
		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			targetString.c_str(),
			parameters,
			directoryString.empty() ? nullptr : directoryString.c_str(),
			SW_SHOWNORMAL);

		return reinterpret_cast<INT_PTR>(result) > 32;
	}
}

std::string FileDialogs::OpenFile(const char* filter, const char* root)
{
	OPENFILENAMEA ofn;
	CHAR filePath[260] = { 0 };
	CHAR currentDirectory[256] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = sizeof(filePath);
	if (std::strcmp(root, "") == 0 && GetCurrentDirectoryA(256, currentDirectory))
		ofn.lpstrInitialDir = currentDirectory;
	else
		ofn.lpstrInitialDir = root;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn) == TRUE)
		return ofn.lpstrFile;
	return std::string();
}

std::string FileDialogs::SaveFile(const char* filter, const char* root)
{
	OPENFILENAMEA ofn;
	CHAR filePath[260] = { 0 };
	CHAR currentDirectory[256] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = sizeof(filePath);
	if (std::strcmp(root, "") == 0 && GetCurrentDirectoryA(256, currentDirectory))
		ofn.lpstrInitialDir = currentDirectory;
	else
		ofn.lpstrInitialDir = root;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = std::strchr(filter, '\0') + 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileNameA(&ofn) == TRUE)
		return ofn.lpstrFile;
	return std::string();
}

std::string FileDialogs::OpenFolder()
{
	BROWSEINFOA bi = { 0 };
	bi.lpszTitle = "Select Folder";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
	CHAR path[MAX_PATH];
	if (pidl != 0)
	{
		SHGetPathFromIDListA(pidl, path);
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Release();
		}
		return std::string(path);
	}
	return std::string();
}

std::string FileDialogs::OpenFolderUnderASpesificDirectory(const std::filesystem::path& root)
{
	LPITEMIDLIST rootId;
	HRESULT hr = SHParseDisplayName(root.c_str(), NULL, &rootId, 0, NULL);
	if (FAILED(hr))
	{
		WHP_CORE_ERROR("[Application] Failed to parse root path.");
		return std::string{};
	}

	BROWSEINFOA bi = { 0 };
	bi.lpszTitle = "Select Folder";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.pidlRoot = rootId;

	LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
	CHAR path[MAX_PATH];

	if (pidl != 0)
	{
		SHGetPathFromIDListA(pidl, path);
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Free(rootId);
			imalloc->Release();
		}

		return std::string(path);
	}

	IMalloc* imalloc = 0;
	if (SUCCEEDED(SHGetMalloc(&imalloc)))
	{
		imalloc->Free(rootId);
		imalloc->Release();
	}

	return std::string();
}

float Time::GetTime()
{
	return (float)glfwGetTime();
}

bool Utils::RestartProgram()
{
	char programPath[MAX_PATH];
	if (GetModuleFileNameA(NULL, programPath, MAX_PATH) == 0)
	{
		WHP_CORE_ERROR("[Application] RestartProgram failed. Windows error (GetModuleFileNameA): {0}", GetLastError());
		return false;
	}

	std::string commandLine = GetCommandLineA();
	std::vector<char> commandLineBuffer(commandLine.begin(), commandLine.end());
	commandLineBuffer.push_back('\0');

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessA(programPath, commandLineBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		WHP_CORE_ERROR("[Application] RestartProgram failed. Windows error (CreateProcessA): {0}", GetLastError());
		return false;
	}

	WHP_CORE_INFO("[Application] Program restart requested.");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}

bool Utils::OpenExternalPath(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	std::error_code error;
	const bool isDirectory = std::filesystem::is_directory(path, error);
	const std::filesystem::path workingDirectory = isDirectory ? path : path.parent_path();
	return ShellExecuteOpen(path, nullptr, workingDirectory);
}

bool Utils::OpenExternalPathWith(const std::filesystem::path& executable, const std::filesystem::path& path)
{
	if (executable.empty() || path.empty())
		return false;

	const std::wstring parameters = L"\"" + path.wstring() + L"\"";
	return ShellExecuteOpen(executable, parameters.c_str(), path.parent_path());
}

std::string Utils::WStringToUtf8(const std::wstring& wideString)
{
	if (wideString.empty()) 
		return std::string();
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wideString[0], (int)wideString.size(), NULL, 0, NULL, NULL);
	std::string utf8String(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wideString[0], (int)wideString.size(), &utf8String[0], sizeNeeded, NULL, NULL);
	return utf8String;
}

_WHIP_END
