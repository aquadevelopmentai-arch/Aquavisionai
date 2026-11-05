// counting.cpp: 구현 파일
//

#include "pch.h"
#include "AquaInterface.h"
#include "afxdialogex.h"
#include "countingDlg.h"
#include "FileFunction.h"

// Ccounting 대화 상자

IMPLEMENT_DYNAMIC(CcountingDlg, CDialogEx)

CcountingDlg::CcountingDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_COUNTING_DIALOG, pParent)
	, m_pProc(NULL)
	, m_nCurRow(-1)
{

}

CcountingDlg::~CcountingDlg()
{
}

void CcountingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_FILE, _filelist_control);
	DDX_Control(pDX, IDC_EDIT_PATH, m_location_control);
	DDX_Control(pDX, IDC_STATIC_ORIIMAGEVIEW, _image_control);
	DDX_Control(pDX, IDC_STATIC_IMAGEVIEW, _result_control);
}



BEGIN_MESSAGE_MAP(CcountingDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_FILE, &CcountingDlg::OnClickedButtonOpenFile)
	ON_WM_PAINT()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILE, &CcountingDlg::OnLvnItemchangedListFile)
	ON_BN_CLICKED(IDC_BUTTON_START, &CcountingDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_FOLDERPATH, &CcountingDlg::OnBnClickedButtonFolderpath)
END_MESSAGE_MAP()


// Ccounting 메시지 처리기

void CcountingDlg::OnClickedButtonOpenFile()
{
	CFileDialog file_dialog(TRUE);
	if (file_dialog.DoModal() != IDOK)
		return;

	const std::string pathname = std::string(CT2CA(file_dialog.GetFolderPath()));
	const std::string filename = std::string(CT2CA(file_dialog.GetFileName()));
	const std::string fullname = pathname + "\\" + filename;

	int page = OpenImageFile(fullname, _image_matrix, _multi_image_matrix);
	if (page <= 0 || _image_matrix.empty())
	{
		std::cout << "[Counting] open failed: " << fullname << std::endl;
		AfxMessageBox(_T("이미지를 열 수 없습니다."));
		return;
	}
	std::cout << "[Counting] open: " << fullname << " pages=" << page << std::endl;
	
	CString page_string;
	page_string.Format(_T("%d"), page);
	//_page_control.SetWindowText(page_string);
	m_location_control.SetWindowText(CString(pathname.c_str()));

	const int index = _filelist_control.InsertItem(0, CString(fullname.c_str()));

	// 폴더 경로를 항목에 저장 (LVN_DELETEITEM에서 delete[])
	const size_t length = pathname.length() + 1;
	char* p_path = new char[length];
	strcpy_s(p_path, length, pathname.c_str());
	_filelist_control.SetItemData(index, (DWORD_PTR)p_path);

	_filelist_control.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);      // 기존 선택 해제
	_filelist_control.SetItemState(index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	_filelist_control.EnsureVisible(index, FALSE);
	_filelist_control.SetFocus();

	Invalidate(FALSE);
}

void CcountingDlg::OnLvnItemchangedListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* p = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	std::cout << "[List] changed item=" << p->iItem << " old=" << std::hex << p->uOldState
		<< " new=" << p->uNewState << std::dec << std::endl;
	// 새로 "선택된" 경우만 (선택 해제·포커스 이동 등은 무시)
	if ((p->uChanged & LVIF_STATE) && (p->uNewState & LVIS_SELECTED) && !(p->uOldState & LVIS_SELECTED))
		ShowFile(p->iItem);
	*pResult = 0;
}


void CcountingDlg::ShowFile(int index)
{
	if (index < 0 || index >= _filelist_control.GetItemCount())
		return;

	// 항목 데이터(폴더) + 항목 글자(파일명) = 전체 경로
	const char* p_folder = reinterpret_cast<const char*>(_filelist_control.GetItemData(index));
	if (p_folder == nullptr)
		return;
	const CString fullpath = _filelist_control.GetItemText(index, 0);
	//const CString fullpath = CString(p_folder) + _T("\\") + filename;

	// 원본
	cv::Mat oricvMat = ReadImageW(fullpath.GetString());              // 한글 경로 안전 (imdecode)
	m_nCurRow = index;
	if (oricvMat.empty())
	{
		std::cout << "[Counting] cannot read: " << CT2CA(fullpath) << std::endl;
		return;
	}
	oricvMat.copyTo(_image_matrix);
	// 이전 이미지는 자동 해제
	oricvMat.release();
	// 결과 (카운팅 때 results\파일명.jpg로 저장해 둔 경우)
	//const CString resultpath = CString(p_folder) + _T("\\results\\") + filename.Left(filename.ReverseFind(_T('.'))) + _T(".jpg");
	//_result_matrix = ReadImageW(resultpath);         // 없으면 빈 Mat → 오른쪽은 검은 화면
	_result_matrix.release();
	DrawViews();

	m_location_control.SetWindowText(fullpath.GetString());
	//std::cout << "[Counting] show " << index << ": " << CT2CA(fullpath)
	//	<< (_result_matrix.empty() ? " (no result yet)" : " (+result)") << std::endl;
	//_image_control.UpdateWindow();

	Invalidate(FALSE);                               // OnPaint에서 양쪽 다시 그림
	
}

BOOL CcountingDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	InitListControl(_filelist_control);
	_filelist_control.InsertColumn(0, _T("파일명"), LVCFMT_LEFT, 600);
	_filelist_control.InsertColumn(1, _T("Total Count"), LVCFMT_LEFT, 100);
	_filelist_control.InsertColumn(2, _T("Dead"), LVCFMT_LEFT, 80);
	_filelist_control.InsertColumn(3, _T("Total msec"), LVCFMT_LEFT, 100);

	
	return TRUE;  // return TRUE unless you set the focus to a control
}


void CcountingDlg::InitListControl(CListCtrl& list_control)
{
	list_control.ModifyStyle(NULL, LVS_REPORT | LVS_SHOWSELALWAYS, 0);
	list_control.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	// list_c
}
void CcountingDlg::ClearAllAreas()
{
	CString empty_str = _T("");
	_area_list.clear();
	m_point_control.SetWindowText(empty_str);
	_selected_area_index = -1;
}

void CcountingDlg::OnPaint()
{
	CPaintDC dc(this);                     
	DrawViews();
}

void CcountingDlg::DrawViews()
{
	if (!_image_matrix.empty() && ::IsWindow(_image_control.GetSafeHwnd()))
	{
		RECT rect;
		_image_control.GetClientRect(&rect);
		CDC* p_dc = _image_control.GetDC();
		DisplayImage(_image_matrix, p_dc, rect);
		_image_control.ReleaseDC(p_dc);       // ← GetDC 한 컨트롤에서 ReleaseDC
	}


	CWnd* p_resultwnd = GetDlgItem(IDC_STATIC_IMAGEVIEW);
	if (p_resultwnd != nullptr)
	{
		RECT rect;
		p_resultwnd->GetClientRect(&rect);
		CDC* p_resultdc = p_resultwnd->GetDC();
		if (!_result_matrix.empty())
		{
			DisplayImage(_result_matrix, p_resultdc, rect);
		}
		else
		{
			p_resultdc->FillSolidRect(&rect, GetSysColor(COLOR_BTNFACE));
		}
		p_resultwnd->ReleaseDC(p_resultdc);
	}
	
}

void CcountingDlg::DisplayImage(const cv::Mat& image_matrix, CDC* p_dc, RECT rect)
{
	const int cw = rect.right - rect.left;
	const int ch = rect.bottom - rect.top;
	p_dc->FillSolidRect(&rect, RGB(0, 0, 0));
	if (image_matrix.empty() || cw < 4 || ch < 4)
		return;

	// 1) 3채널 BGR로 (원본은 건드리지 않음)
	cv::Mat bgr;
	if (image_matrix.channels() == 1)      cv::cvtColor(image_matrix, bgr, cv::COLOR_GRAY2BGR);
	else if (image_matrix.channels() == 4) cv::cvtColor(image_matrix, bgr, cv::COLOR_BGRA2BGR);
	else                                   bgr = image_matrix;          // 아래 resize가 새 메모리에 씀

	// 2) 비율 유지 축소, 가로는 4의 배수 (DIB 행 정렬)
	const double ratio = (std::min)((double)cw / bgr.cols, (double)ch / bgr.rows);
	int w = (int)(bgr.cols * ratio);  w -= w % 4;
	int h = (int)(bgr.rows * ratio);
	if (w < 4 || h < 1)
		return;
	cv::Mat view;
	cv::resize(bgr, view, cv::Size(w, h), 0, 0, ratio < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);

	// 3) top-down DIB (biHeight 음수) -> flip 불필요, CImage 불필요
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = view.cols;
	bi.bmiHeader.biHeight = -view.rows;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	bi.bmiHeader.biCompression = BI_RGB;

	// 4) 가운데 정렬로 바로 그림
	const int x = rect.left + (cw - w) / 2;
	const int y = rect.top + (ch - h) / 2;
	::SetStretchBltMode(p_dc->GetSafeHdc(), COLORONCOLOR);
	::StretchDIBits(p_dc->GetSafeHdc(), x, y, w, h, 0, 0, view.cols, view.rows,
		view.data, &bi, DIB_RGB_COLORS, SRCCOPY);
}





void CcountingDlg::OnBnClickedButtonStart()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	if (m_pProc == NULL)
	{
		AfxMessageBox(_T("처리기가 설정되지 않았습니다."));
		return;
	}
	if (_image_matrix.empty())
	{
		AfxMessageBox(_T("선택된 이미지가 없습니다."));
		return;
	}
	BeginWaitCursor();
	const bool success = m_pProc->RunYoloseg(_image_matrix, m_SegResult);
	EndWaitCursor();
	if (!success)
	{
		std::cout << "[Counting] RunYoloseg FAIL: " << m_SegResult.error << std::endl;
		return;
	}
	SegDrawOptions opt;
	_image_matrix.copyTo(_result_matrix);
	m_pProc->Draw(_result_matrix, m_SegResult, opt);

	std::cout << "[Counting] count " << m_SegResult.Count()
		<< "  pre " << m_SegResult.preMs << "  infer " << m_SegResult.inferMs << "  post " << m_SegResult.postMs << " ms" << std::endl;
	CString count_str;
	CString msec_str;
	count_str.Format(_T("%d"), m_SegResult.Count());
	msec_str.Format(_T("%.3fms"), m_SegResult.preMs + m_SegResult.inferMs + m_SegResult.postMs);
	const int index = _filelist_control.SetItemText(m_nCurRow, 1, count_str);
	const int msec_index = _filelist_control.SetItemText(m_nCurRow, 3, msec_str);
	// 3) 우측 표출
	DrawViews();
}

void CcountingDlg::OnBnClickedButtonFolderpath()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	CFolderPickerDialog dlg(NULL, 0, this);
	if (dlg.DoModal() != IDOK)
		return;
	const CString folder = dlg.GetPathName();
	m_location_control.SetWindowText(folder);
	std::cout << "[List] open folder : " << CT2A(folder) << std::endl;

	// 1) 이미지 경로 수집 (하위 폴더 제외)
	std::vector<CString> files;
	CFileFind finder;
	BOOL bWork = finder.FindFile(folder + _T("\\*.*"));
	while (bWork)
	{
		bWork = finder.FindNextFile();
		if (finder.IsDots() || finder.IsDirectory())
			continue;
		const CString path = finder.GetFilePath();
		if (IsImageFile(path))
			files.push_back(path);
	}
	finder.Close();
	std::sort(files.begin(), files.end(),
		[](const CString& a, const CString& b) { return a.CompareNoCase(b) < 0; });
	std::cout << "[List] " << files.size() << " image(s)" << std::endl;
	if (files.empty())
	{
		AfxMessageBox(_T("이미지 파일이 없습니다."));
		return;
	}

	// 2) 목록 비우고 채우기 (DeleteAllItems → LVN_DELETEITEM에서 delete[])
	_filelist_control.SetRedraw(FALSE);
	_filelist_control.DeleteAllItems();
	m_nCurRow = -1;
	_image_matrix.release();
	_result_matrix.release();
	_filelist_control.SetItemCount((int)files.size());
	for (size_t i = 0; i < files.size(); ++i)
		AddFileItem(files[i]);
	_filelist_control.SetRedraw(TRUE);
	_filelist_control.Invalidate();

	// 3) 첫 장 선택 + 표출
	SelectRow(0);
}

int CcountingDlg::AddFileItem(const CString& path)
{
	const int index = _filelist_control.InsertItem(_filelist_control.GetItemCount(), path);
	const std::string fullname = std::string(CT2A(path));
	const size_t length = fullname.length() + 1;
	char* p_path = new char[length];
	strcpy_s(p_path, length, fullname.c_str());
	_filelist_control.SetItemData(index, (DWORD_PTR)p_path);    // LVN_DELETEITEM에서 delete[]
	return index;
}

void CcountingDlg::SelectRow(int index)
{
	if (index < 0 || index >= _filelist_control.GetItemCount())
		return;
	_filelist_control.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	_filelist_control.SetItemState(index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	_filelist_control.SetSelectionMark(index);
	_filelist_control.EnsureVisible(index, FALSE);
	_filelist_control.SetFocus();
	if (m_nCurRow != index)
	{
		ShowFile(index);
	}
}

bool CcountingDlg::IsImageFile(const CString& path) const
{
	CString ext = PathFindExtension(path);
	ext.MakeLower();
	return ext == _T(".jpg") || ext == _T(".jpeg") || ext == _T(".png") || ext == _T(".bmp") || ext == _T(".tif");
}
