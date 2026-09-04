// ============================================================================
// Flip3DAccessible.cpp — COM IAccessible (uDWM CFlip3DAccessible port)
// ============================================================================
#include "Flip3DAccessible.h"
#include "Flip3DComp.h"

#include <algorithm>
#include <cstddef>

namespace {

constexpr int kAccStringContainer     = 3000;  // uDWM resource 0xBB8
constexpr int kAccStringDefaultAction = 3001;  // uDWM resource 0xBB9

// Fallbacks when string resources are absent (standalone exe).
constexpr wchar_t kFallbackContainerName[]     = L"Flip 3D";
constexpr wchar_t kFallbackDefaultActionName[] = L"Switch";

HRESULT LoadAccString(int stringId, BSTR* pbstrTarget)
{
    if (!pbstrTarget)
        return E_POINTER;

    *pbstrTarget = nullptr;

    wchar_t buffer[260] = {};
    if (LoadStringW(GetModuleHandleW(nullptr),
                    (UINT)stringId, buffer, (int)std::size(buffer)))
    {
        *pbstrTarget = SysAllocString(buffer);
        return *pbstrTarget ? S_OK : E_OUTOFMEMORY;
    }

    const wchar_t* fallback = (stringId == kAccStringDefaultAction)
        ? kFallbackDefaultActionName : kFallbackContainerName;
    *pbstrTarget = SysAllocString(fallback);
    return *pbstrTarget ? S_OK : E_OUTOFMEMORY;
}

} // namespace

// ============================================================================
Flip3DAccessible::Flip3DAccessible(Flip3DCompApp* app)
    : m_app(app)
{
}

HRESULT Flip3DAccessible::Create(Flip3DCompApp* app, IAccessible** ppAccessible)
{
    if (!app || !ppAccessible)
        return E_POINTER;

    *ppAccessible = new (std::nothrow) Flip3DAccessible(app);
    if (!*ppAccessible)
        return E_OUTOFMEMORY;

    return S_OK;
}

STDMETHODIMP Flip3DAccessible::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj)
        return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IDispatch || riid == IID_IAccessible)
    {
        *ppvObj = static_cast<IAccessible*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) Flip3DAccessible::AddRef()
{
    return (ULONG)InterlockedIncrement((LONG*)&m_ref);
}

STDMETHODIMP_(ULONG) Flip3DAccessible::Release()
{
    const ULONG ref = (ULONG)InterlockedDecrement((LONG*)&m_ref);
    if (ref == 0)
        delete this;
    return ref;
}

STDMETHODIMP Flip3DAccessible::GetTypeInfoCount(UINT* /*pctinfo*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::GetTypeInfo(UINT /*iTInfo*/, LCID /*lcid*/,
                                             ITypeInfo** /*ppTInfo*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::GetIDsOfNames(REFIID /*riid*/, LPOLESTR* /*rgszNames*/,
                                             UINT /*cNames*/, LCID /*lcid*/,
                                             DISPID* /*rgDispId*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::Invoke(DISPID /*dispIdMember*/, REFIID /*riid*/,
                                      LCID /*lcid*/, WORD /*wFlags*/,
                                      DISPPARAMS* /*pDispParams*/,
                                      VARIANT* /*pVarResult*/,
                                      EXCEPINFO* /*pExcepInfo*/, UINT* /*puArgErr*/)
{
    return E_NOTIMPL;
}

int Flip3DAccessible::GetChildrenCount() const
{
    return m_app ? m_app->AccessibleChildCount() : 0;
}

void Flip3DAccessible::SetChildIndex(int index, VARIANT* pvarPlace) const
{
    VariantInit(pvarPlace);
    pvarPlace->vt   = VT_I4;
    pvarPlace->lVal = index;
}

STDMETHODIMP Flip3DAccessible::get_accParent(IDispatch** ppdispParent)
{
    if (!ppdispParent)
        return E_POINTER;

    *ppdispParent = nullptr;
    // uDWM passes object id 0 for the desktop root accessible.
    return AccessibleObjectFromWindow(
        GetDesktopWindow(), 0, IID_IDispatch, (void**)ppdispParent);
}

STDMETHODIMP Flip3DAccessible::get_accChildCount(long* pChildCount)
{
    if (!pChildCount)
        return E_POINTER;

    *pChildCount = GetChildrenCount();
    return S_OK;
}

STDMETHODIMP Flip3DAccessible::get_accChild(VARIANT varChild,
                                            IDispatch** ppdispChild)
{
    if (varChild.vt == VT_EMPTY)
        return E_INVALIDARG;

    if (!ppdispChild)
        return E_POINTER;

    *ppdispChild = nullptr;
    return S_FALSE;
}

STDMETHODIMP Flip3DAccessible::get_accName(VARIANT varChild, BSTR* pszName)
{
    if (!pszName)
        return E_POINTER;

    if (varChild.vt != VT_I4)
        return E_INVALIDARG;

    if (varChild.lVal == CHILDID_SELF)
        return LoadAccString(kAccStringContainer, pszName);

    const int index = varChild.lVal - 1;
    if (index < 0 || index >= GetChildrenCount())
        return E_INVALIDARG;

    if (!m_app)
        return DISP_E_MEMBERNOTFOUND;

    return m_app->AccessibleWindowName(index, pszName);
}

STDMETHODIMP Flip3DAccessible::get_accValue(VARIANT /*varChild*/, BSTR* /*pszValue*/)
{
    return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP Flip3DAccessible::get_accDescription(VARIANT /*varChild*/,
                                                    BSTR* pszDescription)
{
    if (!pszDescription)
        return E_POINTER;

    *pszDescription = nullptr;
    return S_FALSE;
}

STDMETHODIMP Flip3DAccessible::get_accRole(VARIANT varChild, VARIANT* pvarRole)
{
    if (!pvarRole)
        return E_POINTER;

    if (varChild.vt != VT_I4)
        return E_INVALIDARG;

    VariantInit(pvarRole);
    pvarRole->vt   = VT_I4;
    pvarRole->lVal = (varChild.lVal == CHILDID_SELF)
        ? ROLE_SYSTEM_CLIENT
        : ROLE_SYSTEM_WINDOW;
    return S_OK;
}

STDMETHODIMP Flip3DAccessible::get_accState(VARIANT varChild, VARIANT* pvarState)
{
    if (!pvarState)
        return E_POINTER;

    if (varChild.vt != VT_I4)
        return E_INVALIDARG;

    DWORD state = STATE_SYSTEM_FOCUSABLE;

    if (varChild.lVal > 0)
    {
        const int index = varChild.lVal - 1;
        if (index < 0)
            return E_INVALIDARG;

        state = STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_SELECTABLE;

        if (GetChildrenCount() > 0 && index == 0)
            state |= STATE_SYSTEM_SELECTED | STATE_SYSTEM_FOCUSED;
    }

    VariantInit(pvarState);
    pvarState->vt   = VT_I4;
    pvarState->lVal = (LONG)state;
    return S_OK;
}

STDMETHODIMP Flip3DAccessible::get_accHelp(VARIANT /*varChild*/, BSTR* /*pszHelp*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::get_accHelpTopic(BSTR* pszHelpFile, VARIANT /*varChild*/,
                                                long* pidTopic)
{
    if (!pszHelpFile || !pidTopic)
        return E_POINTER;

    *pszHelpFile = nullptr;
    *pidTopic    = 0;
    return S_FALSE;
}

STDMETHODIMP Flip3DAccessible::get_accKeyboardShortcut(VARIANT /*varChild*/,
                                                       BSTR* /*pszKeyboardShortcut*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::get_accFocus(VARIANT* pvarFocusChild)
{
    if (!pvarFocusChild)
        return E_POINTER;

    if (GetChildrenCount() <= 0)
        return DISP_E_MEMBERNOTFOUND;

    SetChildIndex(1, pvarFocusChild);
    return S_OK;
}

STDMETHODIMP Flip3DAccessible::get_accSelection(VARIANT* pvarSelectedChildren)
{
    if (!pvarSelectedChildren)
        return E_POINTER;

    VariantInit(pvarSelectedChildren);
    pvarSelectedChildren->vt = VT_EMPTY;
    return S_OK;
}

STDMETHODIMP Flip3DAccessible::get_accDefaultAction(VARIANT /*varChild*/,
                                                    BSTR* pszDefaultAction)
{
    if (!pszDefaultAction)
        return E_POINTER;

    return LoadAccString(kAccStringDefaultAction, pszDefaultAction);
}

STDMETHODIMP Flip3DAccessible::accSelect(long flagsSelect, VARIANT varChild)
{
    if (varChild.vt != VT_I4 || !(flagsSelect & SELFLAG_TAKEFOCUS))
        return E_INVALIDARG;

    if (varChild.lVal == CHILDID_SELF || !m_app)
        return E_INVALIDARG;

    const int index = varChild.lVal - 1;
    if (index < 0 || index >= GetChildrenCount())
        return E_INVALIDARG;

    return m_app->AccessibleRotateToIndex(index);
}

STDMETHODIMP Flip3DAccessible::accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                                           long* pcyHeight, VARIANT varChild)
{
    if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight || varChild.vt != VT_I4)
        return E_INVALIDARG;

    if (varChild.lVal == CHILDID_SELF)
    {
        if (!m_app || !m_app->WindowHandle())
            return E_INVALIDARG;

        RECT rc = {};
        if (!GetWindowRect(m_app->WindowHandle(), &rc))
            return E_INVALIDARG;

        *pxLeft   = rc.left;
        *pyTop    = rc.top;
        *pcxWidth  = std::max(0L, rc.right - rc.left);
        *pcyHeight = std::max(0L, rc.bottom - rc.top);
        return S_OK;
    }

    const int index = varChild.lVal - 1;
    if (index < 0 || index >= GetChildrenCount() || !m_app)
        return E_INVALIDARG;

    if (!m_app->AccessibleCardScreenRect(index, pxLeft, pyTop, pcxWidth, pcyHeight))
        return E_INVALIDARG;

    return S_OK;
}

STDMETHODIMP Flip3DAccessible::accNavigate(long navDir, VARIANT varStart,
                                           VARIANT* pvarEndUpAt)
{
    if (!pvarEndUpAt)
        return E_POINTER;

    if (varStart.vt != VT_I4)
        return E_INVALIDARG;

    const int childCount = GetChildrenCount();
    HRESULT hr           = DISP_E_MEMBERNOTFOUND;

    if (varStart.lVal == CHILDID_SELF)
    {
        if (navDir == NAVDIR_FIRSTCHILD)
        {
            SetChildIndex(childCount > 0 ? 1 : 0, pvarEndUpAt);
            hr = S_OK;
        }
        else if (navDir == NAVDIR_LASTCHILD)
        {
            SetChildIndex(childCount, pvarEndUpAt);
            hr = S_OK;
        }
        else
        {
            hr = E_INVALIDARG;
        }
    }
    else if (varStart.lVal > 0 && varStart.lVal <= childCount)
    {
        int current  = varStart.lVal;
        int newIndex = current;

        switch (navDir)
        {
        case NAVDIR_NEXT:
        case NAVDIR_PREVIOUS:
        case NAVDIR_RIGHT:
        case NAVDIR_UP:
            if (current < childCount)
                newIndex = current + 1;
            break;

        case NAVDIR_LEFT:
        case NAVDIR_DOWN:
            if (current > 1)
                newIndex = current - 1;
            break;

        default:
            return E_INVALIDARG;
        }

        SetChildIndex(newIndex, pvarEndUpAt);
        hr = S_OK;
    }

    return hr;
}

STDMETHODIMP Flip3DAccessible::accHitTest(long xLeft, long yTop,
                                          VARIANT* pvarChildAtPoint)
{
    if (!pvarChildAtPoint || !m_app)
        return E_POINTER;

    const int index = m_app->AccessibleHitTest(xLeft, yTop);
    if (index >= 0)
    {
        SetChildIndex(index + 1, pvarChildAtPoint);
        return S_OK;
    }

    POINT pt = { (int)xLeft, (int)yTop };
    if (m_app->AccessiblePointInView(pt))
    {
        SetChildIndex(CHILDID_SELF, pvarChildAtPoint);
        return S_OK;
    }

    return S_FALSE;
}

STDMETHODIMP Flip3DAccessible::accDoDefaultAction(VARIANT varChild)
{
    if (varChild.vt != VT_I4 || varChild.lVal == CHILDID_SELF || !m_app)
        return E_INVALIDARG;

    const int index = varChild.lVal - 1;
    if (index < 0 || index >= GetChildrenCount())
        return E_INVALIDARG;

    return m_app->AccessibleSelectIndex(index);
}

STDMETHODIMP Flip3DAccessible::put_accName(VARIANT /*varChild*/, BSTR /*szName*/)
{
    return E_NOTIMPL;
}

STDMETHODIMP Flip3DAccessible::put_accValue(VARIANT /*varChild*/, BSTR /*szValue*/)
{
    return E_NOTIMPL;
}
