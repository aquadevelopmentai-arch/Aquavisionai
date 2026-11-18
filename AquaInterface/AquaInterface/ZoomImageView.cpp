#include "pch.h"
#include "ZoomImageView.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

BEGIN_MESSAGE_MAP(CZoomImageView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

CZoomImageView::CZoomImageView()
{
	m_pImg = NULL;
	m_lastSize = cv::Size(0, 0);
	m_zoom = 1.0;
	m_offX = m_offY = 0.0;
	m_bFit = true;
	m_bDrag = false;
	m_dragOffX = m_dragOffY = 0.0;
	m_pLinked = NULL;
	m_bSyncing = false;
}

CZoomImageView::~CZoomImageView()
{
	m_pImg = NULL;
	m_pLinked = NULL;
}

// a static control gets mouse messages only with SS_NOTIFY
void CZoomImageView::PreSubclassWindow()
{
	ModifyStyle(0, SS_NOTIFY);
	CStatic::PreSubclassWindow();
}

// ==================================================================== public
void CZoomImageView::SetImage(const cv::Mat* pImg)
{
	m_pImg = pImg;
	const cv::Size sz = HasImage() ? m_pImg->size() : cv::Size(0, 0);
	if (sz != m_lastSize)
	{
		m_lastSize = sz;
		FitToWindow();                                      // new size -> fit (also repaints)
		return;
	}
	ClampOffset();
	Invalidate(FALSE);
}

void CZoomImageView::Refresh()
{
	if (HasImage() && m_pImg->size() != m_lastSize)
	{
		SetImage(m_pImg);
		return;
	}
	Invalidate(FALSE);
}

void CZoomImageView::FitToWindow()
{
	m_zoom = FitZoom();
	m_bFit = true;
	ClampOffset();                                          // centers the image
	Invalidate(FALSE);
	SyncLinked();
}

void CZoomImageView::SetLinkedView(CZoomImageView* pOther)
{
	m_pLinked = pOther;
}

// ==================================================================== helpers
bool CZoomImageView::HasImage() const
{
	return m_pImg != NULL && !m_pImg->empty();
}

double CZoomImageView::FitZoom() const
{
	if (!HasImage() || !::IsWindow(m_hWnd))
		return 1.0;
	CRect rc;
	GetClientRect(&rc);
	if (rc.Width() < 2 || rc.Height() < 2)
		return 1.0;
	return (std::min)((double)rc.Width() / m_pImg->cols, (double)rc.Height() / m_pImg->rows);
}

void CZoomImageView::ClampOffset()
{
	if (!HasImage() || !::IsWindow(m_hWnd))
		return;
	CRect rc;
	GetClientRect(&rc);
	const double visW = rc.Width() / m_zoom, visH = rc.Height() / m_zoom;
	const double W = m_pImg->cols, H = m_pImg->rows;
	// smaller than the view -> center, bigger -> keep inside
	m_offX = (visW >= W) ? (W - visW) / 2.0 : (std::max)(0.0, (std::min)(m_offX, W - visW));
	m_offY = (visH >= H) ? (H - visH) / 2.0 : (std::max)(0.0, (std::min)(m_offY, H - visH));
}

void CZoomImageView::ZoomAt(double newZoom, CPoint pt)
{
	const double fit = FitZoom();
	newZoom = (std::max)(fit, (std::min)(newZoom, 16.0));    // fit ~ x16
	const double ix = m_offX + pt.x / m_zoom;                // image point under the cursor
	const double iy = m_offY + pt.y / m_zoom;
	m_zoom = newZoom;
	m_offX = ix - pt.x / m_zoom;
	m_offY = iy - pt.y / m_zoom;
	m_bFit = (std::abs(m_zoom - fit) < 1e-6);
	ClampOffset();
	Invalidate(FALSE);
	SyncLinked();
}

void CZoomImageView::SyncLinked()
{
	if (m_pLinked != NULL && !m_bSyncing && ::IsWindow(m_pLinked->m_hWnd))
		m_pLinked->ApplyView(m_zoom, m_offX, m_offY);
}

void CZoomImageView::ApplyView(double zoom, double offX, double offY)
{
	if (!HasImage() || m_pImg->size() != m_lastSize)
		return;
	if (m_pLinked != NULL && m_pLinked->HasImage() && m_pLinked->m_pImg->size() != m_pImg->size())
		return;                                              // different image size -> do not follow
	m_bSyncing = true;
	m_zoom = zoom;
	m_offX = offX;
	m_offY = offY;
	m_bFit = (std::abs(m_zoom - FitZoom()) < 1e-6);
	ClampOffset();
	Invalidate(FALSE);
	m_bSyncing = false;
}

// ==================================================================== mouse
BOOL CZoomImageView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (!HasImage())
		return TRUE;
	ScreenToClient(&pt);
	const double factor = (zDelta > 0) ? 1.25 : 0.8;
	ZoomAt(m_zoom * factor, pt);
	return TRUE;
}

void CZoomImageView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (HasImage())
	{
		m_bDrag = true;
		m_dragStart = point;
		m_dragOffX = m_offX;
		m_dragOffY = m_offY;
		SetCapture();
		SetFocus();                                          // wheel goes here
	}
	CStatic::OnLButtonDown(nFlags, point);
}

void CZoomImageView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDrag && HasImage())
	{
		m_offX = m_dragOffX - (point.x - m_dragStart.x) / m_zoom;
		m_offY = m_dragOffY - (point.y - m_dragStart.y) / m_zoom;
		ClampOffset();
		Invalidate(FALSE);
		SyncLinked();
	}
	CStatic::OnMouseMove(nFlags, point);
}

void CZoomImageView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDrag)
	{
		m_bDrag = false;
		ReleaseCapture();
	}
	CStatic::OnLButtonUp(nFlags, point);
}

void CZoomImageView::OnCaptureChanged(CWnd* pWnd)
{
	m_bDrag = false;
	CStatic::OnCaptureChanged(pWnd);
}

void CZoomImageView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	FitToWindow();
	CStatic::OnLButtonDblClk(nFlags, point);
}

void CZoomImageView::OnSize(UINT nType, int cx, int cy)
{
	CStatic::OnSize(nType, cx, cy);
	if (m_bFit)
		m_zoom = FitZoom();
	ClampOffset();
	Invalidate(FALSE);
}

// ==================================================================== paint
BOOL CZoomImageView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;                                             // everything is painted in OnPaint
}

void CZoomImageView::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	if (rc.Width() < 1 || rc.Height() < 1)
		return;

	// double buffer
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap* pOld = mem.SelectObject(&bmp);

	Render(&mem, rc);

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}

void CZoomImageView::Render(CDC* pDC, const CRect& rc)
{
	pDC->FillSolidRect(&rc, ::GetSysColor(COLOR_BTNFACE));
	if (!HasImage())
		return;

	const cv::Mat& img = *m_pImg;
	const double visW = rc.Width() / m_zoom, visH = rc.Height() / m_zoom;

	// visible part of the image (image coords)
	const int x0 = (std::max)(0, (int)std::floor(m_offX));
	const int y0 = (std::max)(0, (int)std::floor(m_offY));
	const int x1 = (std::min)(img.cols, (int)std::ceil(m_offX + visW));
	const int y1 = (std::min)(img.rows, (int)std::ceil(m_offY + visH));
	if (x1 <= x0 || y1 <= y0)
		return;
	const cv::Rect roi(x0, y0, x1 - x0, y1 - y0);

	// where it goes on screen
	const int dx = (int)std::lround((x0 - m_offX) * m_zoom);
	const int dy = (int)std::lround((y0 - m_offY) * m_zoom);
	const int dw = (std::max)(1, (int)std::lround(roi.width * m_zoom));
	const int dh = (std::max)(1, (int)std::lround(roi.height * m_zoom));

	// resize only the visible part. zoom in -> nearest (pixels visible), zoom out -> area
	cv::Mat part;
	cv::resize(img(roi), part, cv::Size(dw, dh), 0, 0, m_zoom >= 1.0 ? cv::INTER_NEAREST : cv::INTER_AREA);
	if (part.channels() == 1)      cv::cvtColor(part, m_buf, cv::COLOR_GRAY2BGRA);
	else if (part.channels() == 3) cv::cvtColor(part, m_buf, cv::COLOR_BGR2BGRA);
	else                           part.copyTo(m_buf);     // already 4 channels

	// 32 bit top-down DIB : no row padding problem
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = m_buf.cols;
	bi.bmiHeader.biHeight = -m_buf.rows;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	::SetDIBitsToDevice(pDC->GetSafeHdc(), dx, dy, m_buf.cols, m_buf.rows, 0, 0, 0, m_buf.rows,
	                    m_buf.data, &bi, DIB_RGB_COLORS);

	// zoom % (bottom-left)
	CString z;
	z.Format(_T(" %.0f%% "), m_zoom * 100.0);
	pDC->SetBkColor(RGB(0, 0, 0));
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(OPAQUE);
	pDC->TextOut(4, rc.Height() - 20, z);
}
