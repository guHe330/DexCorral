#pragma once
#include <Windows.h>

HRESULT RegisterShellExtension(const wchar_t* dllPath);
HRESULT UnregisterShellExtension();
