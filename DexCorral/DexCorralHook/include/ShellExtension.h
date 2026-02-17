#pragma once
#include <Windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <docobj.h>  // IOleCommandTarget

class DexCorralShellExt : public IShellExtInit, public IContextMenu,
                          public IOleCommandTarget, public IShellIconOverlayIdentifier {
public:
    DexCorralShellExt();
    ~DexCorralShellExt();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IShellExtInit
    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE pidlFolder,
        IDataObject* pdtobj, HKEY hkeyProgID) override;

    // IContextMenu
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU hmenu, UINT indexMenu,
        UINT idCmdFirst, UINT idCmdLast, UINT uFlags) override;
    HRESULT STDMETHODCALLTYPE InvokeCommand(CMINVOKECOMMANDINFO* pici) override;
    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR idCmd, UINT uType,
        UINT* pReserved, CHAR* pszName, UINT cchMax) override;

    // IOleCommandTarget (for ShellServiceObjectDelayLoad)
    HRESULT STDMETHODCALLTYPE QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds,
        OLECMD prgCmds[], OLECMDTEXT* pCmdText) override;
    HRESULT STDMETHODCALLTYPE Exec(const GUID* pguidCmdGroup, DWORD nCmdID,
        DWORD nCmdexecopt, VARIANT* pvaIn, VARIANT* pvaOut) override;

    // IShellIconOverlayIdentifier (ensures DLL loads on Explorer startup)
    HRESULT STDMETHODCALLTYPE IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib) override;
    HRESULT STDMETHODCALLTYPE GetOverlayInfo(PWSTR pwszIconFile, int cchMax,
        int* pIndex, DWORD* pdwFlags) override;
    HRESULT STDMETHODCALLTYPE GetPriority(int* pPriority) override;

private:
    LONG m_refCount = 1;
};
