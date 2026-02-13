#include "ShellExtension.h"
#include "Guids.h"
#include "App.h"
#include <strsafe.h>

// Menu command IDs
enum {
    IDM_NEW_CORRAL = 0,
    IDM_NEW_VIRTUAL_CORRAL,
    IDM_COUNT
};

DexCorralShellExt::DexCorralShellExt() = default;
DexCorralShellExt::~DexCorralShellExt() = default;

HRESULT STDMETHODCALLTYPE DexCorralShellExt::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown)) {
        *ppv = static_cast<IShellExtInit*>(this);
        AddRef();
        return S_OK;
    }
    if (IsEqualIID(riid, IID_IShellExtInit)) {
        *ppv = static_cast<IShellExtInit*>(this);
        AddRef();
        return S_OK;
    }
    if (IsEqualIID(riid, IID_IContextMenu)) {
        *ppv = static_cast<IContextMenu*>(this);
        AddRef();
        return S_OK;
    }
    if (IsEqualIID(riid, IID_IOleCommandTarget)) {
        *ppv = static_cast<IOleCommandTarget*>(this);
        AddRef();
        return S_OK;
    }
    if (IsEqualIID(riid, IID_IShellIconOverlayIdentifier)) {
        *ppv = static_cast<IShellIconOverlayIdentifier*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DexCorralShellExt::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE DexCorralShellExt::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::Initialize(PCIDLIST_ABSOLUTE pidlFolder,
    IDataObject* pdtobj, HKEY hkeyProgID) {
    // Called when user right-clicks the desktop background
    // We don't need the folder info — we always add our menu items
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::QueryContextMenu(HMENU hmenu, UINT indexMenu,
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags) {
    // Don't add items if Explorer is asking for default verb only
    if (uFlags & CMF_DEFAULTONLY)
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

    // Add "New Corral" submenu
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING,
        idCmdFirst + IDM_NEW_CORRAL, L"New DexCorral");
    InsertMenuW(hmenu, indexMenu++, MF_BYPOSITION | MF_STRING,
        idCmdFirst + IDM_NEW_VIRTUAL_CORRAL, L"New Virtual DexCorral");

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, IDM_COUNT);
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
    // Check if the command is by index (not by verb string)
    if (HIWORD(pici->lpVerb) != 0)
        return E_INVALIDARG;

    POINT pt;
    GetCursorPos(&pt);

    App* app = App::GetInstance();
    if (!app) return E_FAIL;

    switch (LOWORD(pici->lpVerb)) {
    case IDM_NEW_CORRAL:
        app->CreateCorralAt(pt);
        return S_OK;
    case IDM_NEW_VIRTUAL_CORRAL:
        app->CreateVirtualCorralAt(pt);
        return S_OK;
    default:
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::GetCommandString(UINT_PTR idCmd, UINT uType,
    UINT* pReserved, CHAR* pszName, UINT cchMax) {
    if (uType != GCS_HELPTEXTW)
        return E_NOTIMPL;

    switch (idCmd) {
    case IDM_NEW_CORRAL:
        StringCchCopyW((LPWSTR)pszName, cchMax, L"Create a new DexCorral on the desktop");
        return S_OK;
    case IDM_NEW_VIRTUAL_CORRAL:
        StringCchCopyW((LPWSTR)pszName, cchMax, L"Create a new virtual DexCorral linked to a folder");
        return S_OK;
    default:
        return E_INVALIDARG;
    }
}

// IOleCommandTarget — required for ShellServiceObjectDelayLoad
// Explorer creates our COM object on startup via CoCreateInstance,
// then calls Exec. We don't need to do anything here since the DLL's
// DLL_PROCESS_ATTACH already started the hook and worker thread.

HRESULT STDMETHODCALLTYPE DexCorralShellExt::QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds,
    OLECMD prgCmds[], OLECMDTEXT* pCmdText) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::Exec(const GUID* pguidCmdGroup, DWORD nCmdID,
    DWORD nCmdexecopt, VARIANT* pvaIn, VARIANT* pvaOut) {
    // Explorer calls this on startup — everything is already initialized in DLL_PROCESS_ATTACH
    return S_OK;
}

// IShellIconOverlayIdentifier — dummy implementation
// Explorer loads ALL registered icon overlay handlers on startup.
// We register one so our DLL gets loaded into Explorer automatically.
// All methods return S_FALSE / E_FAIL so we never actually show an overlay.

HRESULT STDMETHODCALLTYPE DexCorralShellExt::IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib) {
    return S_FALSE;  // Never overlay any file
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::GetOverlayInfo(PWSTR pwszIconFile, int cchMax,
    int* pIndex, DWORD* pdwFlags) {
    return E_FAIL;  // No overlay icon
}

HRESULT STDMETHODCALLTYPE DexCorralShellExt::GetPriority(int* pPriority) {
    if (pPriority) *pPriority = 100;  // Low priority (doesn't matter since IsMemberOf always returns S_FALSE)
    return S_OK;
}
