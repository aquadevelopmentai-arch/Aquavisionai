#include "pch.h"
#include "PieChartCtrl.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

BEGIN_MESSAGE_MAP(CPieChartCtrl, CStatic)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// ---- helpers ------------------------------------------------
static Gdiplus::Color ToColor(COLORREF c, BYTE a = 255)
{
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// 23739 -> "23,739"
static CStringW FormatCount(unsigned long long v)
{
    CStringW s;
    s.Format(L"%llu", v);
    for (int i = s.GetLength() - 3; i > 0; i -= 3)
        s.Insert(i, L',');
    return s;
}

// ---- construction -------------------------------------------
CPieChartCtrl::CPieChartCtrl()
{
}

CPieChartCtrl::~CPieChartCtrl()
{
    ClearItems();
}

// ---- data ---------------------------------------------------
int CPieChartCtrl::AddItem(LPCTSTR name, COLORREF color, bool inPie, bool showSub)
{
    Item* it = new Item;
    it->name = name;
    it->color = color;
    it->value = 0;
    it->sub = 0;
    it->inPie = inPie;
    it->showSub = showSub;
    m_items.push_back(it);          // stores the pointer only
    m_dirty = true;
    return static_cast<int>(m_items.size()) - 1;
}

void CPieChartCtrl::ClearItems()
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        delete m_items[i];
        m_items[i] = nullptr;
    }
    m_items.clear();
    m_dirty = true;
}

void CPieChartCtrl::SetTotalLabel(LPCTSTR label)
{
    m_totalLabel = label;
    m_dirty = true;
}

void CPieChartCtrl::SetShowPercent(bool percent)
{
    m_showPercent = percent;
    m_dirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CPieChartCtrl::SetCountUnit(LPCTSTR unit)
{
    m_countUnit = unit;
    m_dirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CPieChartCtrl::SetValue(int index, unsigned long long value)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return;
    if (m_items[index]->value != value)
    {
        m_items[index]->value = value;
        m_dirty = true;
    }
}

void CPieChartCtrl::AddValue(int index, unsigned long long delta)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return;
    m_items[index]->value += delta;
    m_dirty = true;
}

void CPieChartCtrl::AddSub(int index, unsigned long long delta)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return;
    m_items[index]->sub += delta;
    m_dirty = true;
}

unsigned long long CPieChartCtrl::GetSub(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return 0;
    return m_items[index]->sub;
}

void CPieChartCtrl::ResetValues()
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        m_items[i]->value = 0;
        m_items[i]->sub = 0;
    }
    m_dirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

unsigned long long CPieChartCtrl::GetValue(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return 0;
    return m_items[index]->value;
}

unsigned long long CPieChartCtrl::GetTotal() const
{
    unsigned long long t = 0;
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i]->inPie)
            t += m_items[i]->value;
    return t;
}

void CPieChartCtrl::Refresh()
{
    if (m_dirty && GetSafeHwnd())
        Invalidate(FALSE);
}

// ---- drawing ------------------------------------------------
BOOL CPieChartCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CPieChartCtrl::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return;

    Gdiplus::Bitmap buf(rc.Width(), rc.Height(), PixelFormat32bppARGB);
    if (buf.GetLastStatus() != Gdiplus::Ok) return;      // GDI+ not started

    Gdiplus::Graphics g(&buf);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    const COLORREF bk = ::GetSysColor(COLOR_BTNFACE);
    g.Clear(ToColor(bk));

    const float W = static_cast<float>(rc.Width());
    const float H = static_cast<float>(rc.Height());
    const float pad = 6.0f;

    // ---- layout ----
    const float totalH = H * 0.14f;                         // bottom total row
    const float bodyH = H - totalH - pad;
    float pieSize = bodyH - 2 * pad;
    if (pieSize > W * 0.45f) pieSize = W * 0.45f;
    const Gdiplus::RectF pieRect(pad, pad + (bodyH - pieSize) / 2.0f, pieSize, pieSize);

    const float legendX = pieRect.X + pieRect.Width + pad * 3;
    const float legendW = W - legendX - pad;

    const unsigned long long total = GetTotal();
    const int n = static_cast<int>(m_items.size());

    // ---- pie ----
    if (total == 0 || n == 0)
    {
        Gdiplus::SolidBrush empty(Gdiplus::Color(255, 200, 200, 200));
        g.FillEllipse(&empty, pieRect);
    }
    else
    {
        float start = -90.0f;                               // start at 12 o'clock
        for (int i = 0; i < n; ++i)
        {
            const Item* it = m_items[i];
            if (it == nullptr || !it->inPie || it->value == 0) continue;
            const float sweep = 360.0f * static_cast<float>(it->value) / static_cast<float>(total);
            Gdiplus::SolidBrush br(ToColor(it->color));
            g.FillPie(&br, pieRect, start, sweep);
            start += sweep;
        }
    }
    Gdiplus::Pen rim(Gdiplus::Color(255, 90, 90, 90), 1.0f);
    g.DrawEllipse(&rim, pieRect);

    // ---- legend ----
    Gdiplus::FontFamily family(L"Malgun Gothic");
    if (n > 0 && legendW > 40.0f)
    {
        float rowH = bodyH / static_cast<float>(n);
        if (rowH > 30.0f) rowH = 30.0f;
        const float boxW = rowH * 4.0f;     // wide enough for "123,456 (1,234)"
        const float fontPx = rowH * 0.48f;

        Gdiplus::Font font(&family, fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fontBold(&family, fontPx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::StringFormat center;
        center.SetAlignment(Gdiplus::StringAlignmentCenter);
        center.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::StringFormat left;
        left.SetAlignment(Gdiplus::StringAlignmentNear);
        left.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::SolidBrush black(Gdiplus::Color(255, 30, 30, 30));
        Gdiplus::SolidBrush rowBg(Gdiplus::Color(255, 225, 225, 225));
        Gdiplus::Pen        rowPen(Gdiplus::Color(255, 170, 170, 170), 1.0f);

        const float top = pad + (bodyH - rowH * n) / 2.0f;
        for (int i = 0; i < n; ++i)
        {
            const Item* it = m_items[i];
            if (it == nullptr) continue;
            const float y = top + rowH * i;
            const Gdiplus::RectF row(legendX, y + 1, legendW, rowH - 2);
            const Gdiplus::RectF box(legendX, y + 1, boxW, rowH - 2);
            const Gdiplus::RectF txt(legendX + boxW + 6, y + 1, legendW - boxW - 6, rowH - 2);

            g.FillRectangle(&rowBg, row);
            g.DrawRectangle(&rowPen, row);

            Gdiplus::SolidBrush boxBr(ToColor(it->color));
            g.FillRectangle(&boxBr, box);

            CStringW label;
            if (m_showPercent)
            {
                const double p = total ? 100.0 * static_cast<double>(it->value) / static_cast<double>(total) : 0.0;
                label.Format(L"%.1f%%", p);
            }
            else
            {
                label = FormatCount(it->value) + CStringW(m_countUnit);   // e.g. "1,234"
                if (it->showSub)
                    label += L" (" + FormatCount(it->sub) + L")";          // e.g. "1,234 (12)"
            }

            // dark text on light colors, white text on dark colors
            const COLORREF c = it->color;
            const int lum = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
            g.DrawString(label, -1, &fontBold, box, &center, lum > 150 ? &black : &white);

            g.DrawString(static_cast<LPCWSTR>(it->name), -1, &font, txt, &left, &black);
        }
    }

    // ---- total ----
    {
        const Gdiplus::RectF totalRect(pad, H - totalH, W - 2 * pad, totalH - 2);
        Gdiplus::Font font(&family, totalH * 0.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::StringFormat left;
        left.SetAlignment(Gdiplus::StringAlignmentNear);
        left.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush black(Gdiplus::Color(255, 30, 30, 30));

        CStringW s;
        s.Format(L"%s : %s", static_cast<LPCWSTR>(CStringW(m_totalLabel)), static_cast<LPCWSTR>(FormatCount(total)));
        g.DrawString(s, -1, &font, totalRect, &left, &black);
    }

    Gdiplus::Graphics screen(dc.GetSafeHdc());
    screen.DrawImage(&buf, 0, 0);
    m_dirty = false;
}