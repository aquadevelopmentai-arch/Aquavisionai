#pragma once
// ============================================================
//  CZoomImageView - Picture Control with zoom / pan (UI thread only)
//
//    mouse wheel      : zoom in / out around the cursor (fit ~ x16)
//    left drag        : pan
//    double click     : fit to window
//    linked view      : left (original) and right (result) move together
//
//  * the image is NOT copied / owned : SetImage() stores the pointer only.
//    After changing the Mat content (same object), call Refresh().
//    Before destroying the Mat, call SetImage(NULL).
//  * only the visible part is resized -> fast even for 4096x3072 images
//  * double buffered (no flicker), paints itself -> the parent dialog does NOT draw it
//
//  usage (dialog)
//    header : CZoomImageView _image_control;   CZoomImageView _result_control;
//    DDX_Control(pDX, IDC_STATIC_ORIIMAGEVIEW, _image_control);
//    OnInitDialog : _image_control.SetLinkedView(&_result_control);
//                   _result_control.SetLinkedView(&_image_control);
//    show        : _image_control.SetImage(&_image_matrix);  _result_control.SetImage(&_result_matrix);
// ============================================================
#include <opencv2/core.hpp>

class CZoomImageView : public CStatic
{
public:
	CZoomImageView();
	virtual ~CZoomImageView();
	CZoomImageView(const CZoomImageView&) = delete;
	CZoomImageView& operator=(const CZoomImageView&) = delete;

	void SetImage(const cv::Mat* pImg);              // NULL -> empty. image size changed -> fit
	void Refresh();                                  // same Mat, new content -> repaint
	void FitToWindow();
	void SetLinkedView(CZoomImageView* pOther);      // zoom / pan follow each other (same image size only)
	double GetZoom() const { return m_zoom; }

protected:
	virtual void PreSubclassWindow();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	DECLARE_MESSAGE_MAP()

private:
	bool   HasImage() const;
	double FitZoom() const;                          // whole image inside the control
	void   ClampOffset();                            // keep the image on screen, center when smaller
	void   ZoomAt(double newZoom, CPoint ptClient);  // keep the image point under the cursor fixed
	void   SyncLinked();                             // push zoom / offset to the linked view
	void   ApplyView(double zoom, double offX, double offY);
	void   Render(CDC* pDC, const CRect& rc);

	const cv::Mat*  m_pImg;          // NOT owned
	cv::Size        m_lastSize;      // to detect a new image size -> fit
	double          m_zoom;          // screen px per image px
	double          m_offX, m_offY;  // image coord at the view's top-left
	bool            m_bFit;          // true while showing "fit" (window resize keeps fit)
	bool            m_bDrag;
	CPoint          m_dragStart;
	double          m_dragOffX, m_dragOffY;
	CZoomImageView* m_pLinked;       // NOT owned
	bool            m_bSyncing;      // guard against ping-pong between linked views
	cv::Mat         m_buf;           // render buffer (BGRA), reused
};
