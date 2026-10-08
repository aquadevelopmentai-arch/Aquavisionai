#include "pch.h"
#include "LedCtrl.h"

BEGIN_MESSAGE_MAP(CLedCtrl, CStatic)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

static BYTE Clamp255(int v)
{
    return static_cast<BYTE>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

void CLedCtrl::SetColor(COLORREF color)
{
    m_color = color;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CLedCtrl::SetActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CLedCtrl::SetSize(int size)
{
    SetWindowPos(nullptr, 0, 0, size, size, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    Invalidate(FALSE);
}

BOOL CLedCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CLedCtrl::OnPaint()
{
    using namespace Gdiplus;

    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return;

    // ---- 더블 버퍼 ----
    Bitmap   buf(rc.Width(), rc.Height(), PixelFormat32bppARGB);
    if (buf.GetLastStatus() != Ok)
    {
        // GDI+ 가 초기화되지 않음 (App::InitInstance 의 GdiplusStartup 누락)
        // -> 일반 GDI 로 원만 그려서 최소한 보이게 함
        TRACE(_T("CLedCtrl: GDI+ not initialized. Call GdiplusStartup in InitInstance.\n"));
        dc.FillSolidRect(rc, ::GetSysColor(COLOR_BTNFACE));
        const COLORREF c = m_active ? m_color
            : RGB(GetRValue(m_color) * 3 / 10, GetGValue(m_color) * 3 / 10, GetBValue(m_color) * 3 / 10);
        CBrush br(c);
        CBrush* old = dc.SelectObject(&br);
        dc.Ellipse(rc);
        dc.SelectObject(old);
        return;
    }
    Graphics g(&buf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    const float OFF_LEVEL = 0.30f;   // OFF brightness (0..1)
    const int   CENTER_LIFT = 70;      // center highlight (+ toward white)
    const int   EDGE_PCT = 80;      // edge brightness in % of base (higher = brighter)
    const float BEZEL_INSET = 0.08f;   // bezel thickness ratio (smaller = bigger lens)

    const COLORREF bk = ::GetSysColor(COLOR_BTNFACE);
    g.Clear(Color(255, GetRValue(bk), GetGValue(bk), GetBValue(bk)));

    const float size = static_cast<float>((rc.Width() < rc.Height() ? rc.Width() : rc.Height()) - 2);
    const float x = (rc.Width() - size) / 2.0f;
    const float y = (rc.Height() - size) / 2.0f;

    // ---- 색 계산 : 꺼짐이면 30% 밝기 ----
    const float f = m_active ? 1.0f : OFF_LEVEL;
    const int   r = static_cast<int>(GetRValue(m_color) * f);
    const int   gr = static_cast<int>(GetGValue(m_color) * f);
    const int   b = static_cast<int>(GetBValue(m_color) * f);

    
	const int lift = m_active ? CENTER_LIFT : CENTER_LIFT / 3;

    const Color light(255, Clamp255(r + lift), Clamp255(gr + lift), Clamp255(b + lift));  // 중심 (밝게)
    const Color dark(255, Clamp255(r * EDGE_PCT / 100), Clamp255(gr * EDGE_PCT / 100), Clamp255(b * EDGE_PCT / 100));  // 테두리 (어둡게)

    // ---- 1) 금속 테두리 ----
    RectF outer(x, y, size, size);
    LinearGradientBrush bezel(outer, Color(255, 235, 235, 235), Color(255, 95, 95, 95), LinearGradientModeVertical);
    g.FillEllipse(&bezel, outer);
    Pen outline(Color(255, 70, 70, 70), 1.0f);
    g.DrawEllipse(&outline, outer);

    // ---- 2) 렌즈 (원형 그라데이션) ----
    const float inset = size * 0.12f;
    RectF lens(x + inset, y + inset, size - 2 * inset, size - 2 * inset);

    GraphicsPath path;
    path.AddEllipse(lens);
    PathGradientBrush lensBrush(&path);
    lensBrush.SetCenterPoint(PointF(lens.X + lens.Width * 0.42f, lens.Y + lens.Height * 0.38f));
    lensBrush.SetCenterColor(light);
    Color surround[] = { dark };
    int   count = 1;
    lensBrush.SetSurroundColors(surround, &count);
    g.FillPath(&lensBrush, &path);

    // ---- 3) 광택 (위쪽 반사광) ----
    RectF gloss(lens.X + lens.Width * 0.20f, lens.Y + lens.Height * 0.06f,
        lens.Width * 0.60f, lens.Height * 0.45f);
    LinearGradientBrush glossBrush(gloss,
        Color(m_active ? 180 : 70, 255, 255, 255),
        Color(0, 255, 255, 255),
        LinearGradientModeVertical);
    g.FillEllipse(&glossBrush, gloss);

    // ---- 화면에 출력 ----
    Graphics screen(dc.GetSafeHdc());
    screen.DrawImage(&buf, 0, 0);
}