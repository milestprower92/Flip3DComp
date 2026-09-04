// ============================================================================
// Flip3DAccessible.h — COM IAccessible for Flip3D (uDWM CFlip3DAccessible port)
// ============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <oleacc.h>
#include <oaidl.h>

class Flip3DCompApp;

class Flip3DAccessible final : public IAccessible
{
public:
    static HRESULT Create(Flip3DCompApp* app, IAccessible** ppAccessible);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDispatch (uDWM: E_NOTIMPL stubs)
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override;
    STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override;
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                               LCID lcid, DISPID* rgDispId) override;
    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                        DISPPARAMS* pDispParams, VARIANT* pVarResult,
                        EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

    // IAccessible
    STDMETHODIMP get_accParent(IDispatch** ppdispParent) override;
    STDMETHODIMP get_accChildCount(long* pChildCount) override;
    STDMETHODIMP get_accChild(VARIANT varChild, IDispatch** ppdispChild) override;
    STDMETHODIMP get_accName(VARIANT varChild, BSTR* pszName) override;
    STDMETHODIMP get_accValue(VARIANT varChild, BSTR* pszValue) override;
    STDMETHODIMP get_accDescription(VARIANT varChild, BSTR* pszDescription) override;
    STDMETHODIMP get_accRole(VARIANT varChild, VARIANT* pvarRole) override;
    STDMETHODIMP get_accState(VARIANT varChild, VARIANT* pvarState) override;
    STDMETHODIMP get_accHelp(VARIANT varChild, BSTR* pszHelp) override;
    STDMETHODIMP get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild,
                                  long* pidTopic) override;
    STDMETHODIMP get_accKeyboardShortcut(VARIANT varChild,
                                         BSTR* pszKeyboardShortcut) override;
    STDMETHODIMP get_accFocus(VARIANT* pvarFocusChild) override;
    STDMETHODIMP get_accSelection(VARIANT* pvarSelectedChildren) override;
    STDMETHODIMP get_accDefaultAction(VARIANT varChild,
                                      BSTR* pszDefaultAction) override;
    STDMETHODIMP accSelect(long flagsSelect, VARIANT varChild) override;
    STDMETHODIMP accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                             long* pcyHeight, VARIANT varChild) override;
    STDMETHODIMP accNavigate(long navDir, VARIANT varStart,
                             VARIANT* pvarEndUpAt) override;
    STDMETHODIMP accHitTest(long xLeft, long yTop,
                            VARIANT* pvarChildAtPoint) override;
    STDMETHODIMP accDoDefaultAction(VARIANT varChild) override;
    STDMETHODIMP put_accName(VARIANT varChild, BSTR szName) override;
    STDMETHODIMP put_accValue(VARIANT varChild, BSTR szValue) override;

private:
    explicit Flip3DAccessible(Flip3DCompApp* app);

    int  GetChildrenCount() const;
    void SetChildIndex(int index, VARIANT* pvarPlace) const;

    ULONG           m_ref = 1;
    Flip3DCompApp*  m_app = nullptr;
};
