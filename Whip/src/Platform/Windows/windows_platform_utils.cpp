#include "whippch.h"
#include <Whip/Utils/platform_utils.h>


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
	bool shell_execute_open(const std::filesystem::path& target, const wchar_t* parameters, const std::filesystem::path& working_directory)
	{
		const std::wstring target_string = target.wstring();
		const std::wstring directory_string = working_directory.empty() ? std::wstring() : working_directory.wstring();
		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			target_string.c_str(),
			parameters,
			directory_string.empty() ? nullptr : directory_string.c_str(),
			SW_SHOWNORMAL);

		return reinterpret_cast<INT_PTR>(result) > 32;
	}
}

std::string file_dialogs::open_file(const char* filter, const char* root)
{
	OPENFILENAMEA ofn;
	CHAR sz_file[260] = { 0 };
	CHAR current_dir[256] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)application::get().get_window().get_native_window());
	ofn.lpstrFile = sz_file;
	ofn.nMaxFile = sizeof(sz_file);
	if (std::strcmp(root, "") == 0 && GetCurrentDirectoryA(256, current_dir))
		ofn.lpstrInitialDir = current_dir;
	else
		ofn.lpstrInitialDir = root;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn) == TRUE)
		return ofn.lpstrFile;
	return std::string();
}

std::string file_dialogs::save_file(const char* filter, const char* root)
{
	OPENFILENAMEA ofn;
	CHAR sz_file[260] = { 0 };
	CHAR current_dir[256] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)application::get().get_window().get_native_window());
	ofn.lpstrFile = sz_file;
	ofn.nMaxFile = sizeof(sz_file);
	if (std::strcmp(root, "") == 0 && GetCurrentDirectoryA(256, current_dir))
		ofn.lpstrInitialDir = current_dir;
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

std::string file_dialogs::open_folder()
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

std::string file_dialogs::open_folder_under_a_spesific_directory(const std::filesystem::path& root)
{
	LPITEMIDLIST rootID;
	HRESULT hr = SHParseDisplayName(root.c_str(), NULL, &rootID, 0, NULL);
	if (FAILED(hr))
	{
		WHP_CORE_ERROR("[Application] Failed to parse root path.");
		return std::string{};
	}

	BROWSEINFOA bi = { 0 };
	bi.lpszTitle = "Select Folder";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.pidlRoot = rootID;

	LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
	CHAR path[MAX_PATH];

	if (pidl != 0)
	{
		SHGetPathFromIDListA(pidl, path);
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Free(rootID);
			imalloc->Release();
		}

		return std::string(path);
	}

	IMalloc* imalloc = 0;
	if (SUCCEEDED(SHGetMalloc(&imalloc)))
	{
		imalloc->Free(rootID);
		imalloc->Release();
	}

	return std::string();
}

float time::get_time()
{
	return (float)glfwGetTime();
}

bool utils::restart_program()
{
	char programPath[MAX_PATH];
	if (GetModuleFileNameA(NULL, programPath, MAX_PATH) == 0)
	{
		WHP_CORE_ERROR("[Application] restart_program failed. Windows error (GetModuleFileNameA): {0}", GetLastError());
		return false;
	}

	std::string command_line = GetCommandLineA();
	std::vector<char> command_line_buffer(command_line.begin(), command_line.end());
	command_line_buffer.push_back('\0');

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessA(programPath, command_line_buffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		WHP_CORE_ERROR("[Application] restart_program failed. Windows error (CreateProcessA): {0}", GetLastError());
		return false;
	}

	WHP_CORE_INFO("[Application] Program restart requested.");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}

bool utils::open_external_path(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	std::error_code error;
	const bool is_directory = std::filesystem::is_directory(path, error);
	const std::filesystem::path working_directory = is_directory ? path : path.parent_path();
	return shell_execute_open(path, nullptr, working_directory);
}

bool utils::open_external_path_with(const std::filesystem::path& executable, const std::filesystem::path& path)
{
	if (executable.empty() || path.empty())
		return false;

	const std::wstring parameters = L"\"" + path.wstring() + L"\"";
	return shell_execute_open(executable, parameters.c_str(), path.parent_path());
}

std::string utils::wstring_to_utf8(const std::wstring& wstr)
{
	if (wstr.empty()) 
		return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string str_to(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str_to[0], size_needed, NULL, NULL);
	return str_to;
}

_WHIP_END
