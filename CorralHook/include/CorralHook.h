#pragma once
#include <Windows.h>

// Initialize the hook - subclasses SHELLDLL_DefView for NM_CUSTOMDRAW icon hiding
bool InitializeCorralHook();

// Cleanup - restores original wndproc
void CleanupCorralHook();

// Check if active
bool IsCorralHookActive();
