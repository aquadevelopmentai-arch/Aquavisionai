#pragma once
//#error LEDCTRL_HEADER_READ
// ============================================================

//    SetColor(RGB(...))  : 
//    SetActive(true)     : 
//    SetActive(false)    : 
//
//  
// ============================================================

class CLedCtrl : public CStatic
{
public:
    void SetColor(COLORREF color);
    void SetActive(bool active);
    bool IsActive() const { return m_active; }
    void SetSize(int size);          

protected:
    COLORREF m_color  = RGB(0, 200, 0);
    bool     m_active = false;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
