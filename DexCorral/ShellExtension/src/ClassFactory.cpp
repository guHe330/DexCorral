#include "ClassFactory.h"
#include "ShellExtension.h"
#include "Guids.h"
#include <new>

HRESULT STDMETHODCALLTYPE DexCorralClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DexCorralClassFactory::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE DexCorralClassFactory::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE DexCorralClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    if (!ppv) return E_POINTER;

    auto* ext = new (std::nothrow) DexCorralShellExt();
    if (!ext) return E_OUTOFMEMORY;

    HRESULT hr = ext->QueryInterface(riid, ppv);
    ext->Release();  // QI added a ref if successful
    return hr;
}

HRESULT STDMETHODCALLTYPE DexCorralClassFactory::LockServer(BOOL fLock) {
    // We never allow unloading while Explorer is running
    return S_OK;
}
