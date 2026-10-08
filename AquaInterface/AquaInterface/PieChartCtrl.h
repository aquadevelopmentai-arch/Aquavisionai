#pragma once
// ============================================================
//  CPieChartCtrl - GDI+ pie chart + legend in one control
//    left  : pie chart
//    right : legend rows  [ 12.3% ] name   (colored box with percent)
//    bottom: total count
//
//  Subclass a Picture Control / Static Text with DDX_Control.
//  Requires GdiplusStartup in CWinApp::InitInstance.
//  Call from the UI thread only.
// ============================================================
#include <vector>

class CPieChartCtrl : public CStatic
{
public:
    CPieChartCtrl();
    virtual ~CPieChartCtrl();                        // deletes all items
    CPieChartCtrl(const CPieChartCtrl&) = delete;
    CPieChartCtrl& operator=(const CPieChartCtrl&) = delete;

    // setup (once)
    // inPie = false : legend row only (not a slice, not counted in total)
    //                 e.g. "NG" row that is a subset of the tank rows
    // showSub = true: legend shows "count (sub)"  e.g. "1,234 (12)"
    int    AddItem(LPCTSTR name, COLORREF color, bool inPie = true, bool showSub = false);
    void   ClearItems();                             // delete all items
    void   SetTotalLabel(LPCTSTR label);             // default: "Total"
    void   SetShowPercent(bool percent);             // legend box: false = count (default), true = percent
    void   SetCountUnit(LPCTSTR unit);               // text after count, e.g. _T(" ea") (default: none)

    // data
    void   SetValue(int index, unsigned long long value);
    void   AddValue(int index, unsigned long long delta = 1);
    void   AddSub(int index, unsigned long long delta = 1);     // sub count shown in ( )
    unsigned long long GetSub(int index) const;
    void   ResetValues();
    unsigned long long GetValue(int index) const;
    unsigned long long GetTotal() const;             // sum of inPie items only

    // redraw only if values changed (call from a UI timer)
    void   Refresh();

protected:
    struct Item
    {
        CString            name;
        COLORREF           color;
        unsigned long long value;
        unsigned long long sub;      // e.g. NG count inside this tank
        bool               inPie;
        bool               showSub;
    };

    std::vector<Item*> m_items;                     // owned: new in AddItem, delete in ClearItems
    CString           m_totalLabel = _T("Total");
    CString           m_countUnit;
    bool              m_showPercent = false;
    bool              m_dirty = true;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};