#include "whippch.h"
#include <Whip/Utils/platform_utils.h>


#include <shlobj.h>
#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <Whip/Core/Application.h>

_WHIP_START

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

void utils::restart_program()
{
	char programPath[MAX_PATH];
	GetModuleFileNameA(NULL, programPath, MAX_PATH);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if(!CreateProcessA(programPath, GetCommandLineA(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		WHP_CORE_ERROR("[Application] restart_program failed. Windows error (CreateProcessA): {0}", GetLastError());
	else
	{
		WHP_CORE_INFO("[Application] Program restarted!");
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		ExitProcess(0);
	}
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
