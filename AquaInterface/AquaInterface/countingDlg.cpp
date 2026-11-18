// counting.cpp: 구현 파일
//

#include "pch.h"
#include "AquaInterface.h"
#include "afxdialogex.h"
#include "countingDlg.h"
#include "FileFunction.h"

#include <algorithm>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// Ccounting 대화 상자

IMPLEMENT_DYNAMIC(CcountingDlg, CDialogEx)

CcountingDlg::CcountingDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_COUNTING_DIALOG, pParent)
	, m_pProc(NULL)
	, m_nCurRow(-1)
	, m_pCountThread(NULL)
	, m_bStopCount(false)
	, m_bCounting(false)
	, m_nAllImages(0), m_nAllCount(0), m_nAllDead(0), m_nAllFail(0)
	, m_dAllMs(0.0)
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
	ON_WM_DESTROY()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILE, &CcountingDlg::OnLvnItemchangedListFile)
	ON_NOTIFY(LVN_DELETEITEM, IDC_LIST_FILE, &CcountingDlg::OnLvnDeleteitemListFile)
	ON_BN_CLICKED(IDC_BUTTON_START, &CcountingDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_FOLDERPATH, &CcountingDlg::OnBnClickedButtonFolderpath)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, &CcountingDlg::OnBnClickedButtonSave)
	ON_BN_CLICKED(IDC_BUTTON_ALLCOUNTING, &CcountingDlg::OnBnClickedButtonAllcounting)
	ON_MESSAGE(WM_COUNT_PROGRESS, &CcountingDlg::OnCountProgress)
	ON_MESSAGE(WM_COUNT_DONE, &CcountingDlg::OnCountDone)
END_MESSAGE_MAP()


// Ccounting 메시지 처리기

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

void CcountingDlg::OnDestroy()
{
	StopCountAll();                          // Caquamain::OnDestroy 에서 먼저 불렀으면 여기선 아무것도 안 함
	CDialogEx::OnDestroy();
}

void CcountingDlg::InitListControl(CListCtrl& list_control)
{
	list_control.ModifyStyle(NULL, LVS_REPORT | LVS_SHOWSELALWAYS, 0);
	list_control.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
}

void CcountingDlg::ClearAllAreas()
{
	CString empty_str = _T("");
	_area_list.clear();
	m_point_control.SetWindowText(empty_str);
	_selected_area_index = -1;
}

// ==================================================================== 파일 목록
// 행 데이터 = FileRowData (경로 + 결과). AddFileItem 에서 new, LVN_DELETEITEM 에서 delete

void CcountingDlg::OnClickedButtonOpenFile()
{
	CFileDialog file_dialog(TRUE, NULL, NULL, OFN_FILEMUSTEXIST,
		_T("Image (*.jpg;*.jpeg;*.png;*.bmp;*.tif)|*.jpg;*.jpeg;*.png;*.bmp;*.tif||"), this);
	if (file_dialog.DoModal() != IDOK)
		return;

	const CString path = file_dialog.GetPathName();
	std::cout << "[Counting] open: " << CT2A(path) << std::endl;
	RedrawWindow(NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);   // 파일 창이 덮었던 곳 먼저 다시 그림
	SelectRow(AddFileItem(path));                                 // 맨 아래 추가 -> 선택 -> ShowFile
}

void CcountingDlg::OnBnClickedButtonFolderpath()
{
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

	// 2) 목록 비우고 채우기 (DeleteAllItems -> LVN_DELETEITEM 에서 행 데이터 delete)
	_filelist_control.SetRedraw(FALSE);
	_filelist_control.DeleteAllItems();
	m_nCurRow = -1;
	_image_matrix.release();
	_result_matrix.release();
	m_SegResult.Clear();
	_filelist_control.SetItemCount((int)files.size());
	for (size_t i = 0; i < files.size(); ++i)
		AddFileItem(files[i]);
	_filelist_control.SetRedraw(TRUE);
	_filelist_control.Invalidate();

	// 3) 첫 장 선택 + 표출
	RedrawWindow(NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);
	SelectRow(0);
}

int CcountingDlg::AddFileItem(const CString& path)
{
	const int index = _filelist_control.InsertItem(_filelist_control.GetItemCount(), path);
	FileRowData* pRow = new FileRowData();
	pRow->path = path;
	pRow->pSeg = NULL;
	_filelist_control.SetItemData(index, (DWORD_PTR)pRow);
	return index;
}

void CcountingDlg::OnLvnDeleteitemListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* p = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	FileRowData* pRow = reinterpret_cast<FileRowData*>(_list_file_control.GetItemData(p->iItem));
	if (pRow != NULL)
	{
		if (pRow->pSeg != NULL)
		{
			delete pRow->pSeg;
			pRow->pSeg = NULL;
		}
		delete pRow;
		_list_file_control.SetItemData(p->iItem, 0);
	}
	*pResult = 0;
}

FileRowData* CcountingDlg::GetRow(int row) const
{
	if (row < 0 || row >= _filelist_control.GetItemCount())
		return NULL;
	return reinterpret_cast<FileRowData*>(_filelist_control.GetItemData(row));
}

void CcountingDlg::SetRowResult(int row, const SegResult& seg)
{
	FileRowData* pRow = GetRow(row);
	if (pRow == NULL)
		return;
	if (pRow->pSeg == NULL)
		pRow->pSeg = new SegResult(seg);
	else
		*pRow->pSeg = seg;                   // 다시 카운팅하면 덮어씀
	SetRowText(row, seg);
}

void CcountingDlg::SetRowText(int row, const SegResult& seg)
{
	CString s;
	s.Format(_T("%d"), seg.Count());
	_filelist_control.SetItemText(row, 1, s);
	s.Format(_T("%d"), seg.CountVerdict(Verdict::Dead));
	_filelist_control.SetItemText(row, 2, s);
	s.Format(_T("%.1f"), TotalMs(seg));
	_filelist_control.SetItemText(row, 3, s);
}

double CcountingDlg::TotalMs(const SegResult& seg) const
{
	return seg.preMs + seg.inferMs + seg.postMs + seg.cropMs + seg.anomalyMs;
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
		ShowFile(index);
}

bool CcountingDlg::IsImageFile(const CString& path) const
{
	CString ext = PathFindExtension(path);
	ext.MakeLower();
	return ext == _T(".jpg") || ext == _T(".jpeg") || ext == _T(".png") || ext == _T(".bmp") || ext == _T(".tif");
}

void CcountingDlg::OnLvnItemchangedListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* p = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	// 새로 "선택된" 경우만 (선택 해제·포커스 이동 등은 무시)
	if ((p->uChanged & LVIF_STATE) && (p->uNewState & LVIS_SELECTED) && !(p->uOldState & LVIS_SELECTED))
	{
		std::cout << "[List] select " << p->iItem << std::endl;
		if (!m_bCounting)                        // 전체 카운팅 중엔 무시
			ShowFile(p->iItem);
	}
	*pResult = 0;
}

// 클릭 : 이미지는 파일에서 읽고, 이미 카운팅한 사진이면 저장된 결과로 다시 그림 (추론 X)
void CcountingDlg::ShowFile(int index)
{
	FileRowData* pRow = GetRow(index);
	if (pRow == NULL)
		return;
	m_nCurRow = index;

	_image_matrix = ReadImageW(pRow->path.GetString());          // 한글 경로 안전 (imdecode)
	if (_image_matrix.empty())
	{
		std::cout << "[Counting] cannot read: " << CT2A(pRow->path) << std::endl;
		_result_matrix.release();
		m_SegResult.Clear();
		DrawViews();
		return;
	}

	if (pRow->pSeg != NULL && m_pProc != NULL)
	{
		m_SegResult = *pRow->pSeg;
		_image_matrix.copyTo(_result_matrix);
		SegDrawOptions opt;
		m_pProc->Draw(_result_matrix, m_SegResult, opt);
		std::cout << "[Counting] show " << index << " (saved result: count " << m_SegResult.Count()
			<< ", dead " << m_SegResult.CountVerdict(Verdict::Dead) << ")" << std::endl;
	}
	else
	{
		m_SegResult.Clear();
		_result_matrix.release();
		std::cout << "[Counting] show " << index << " (not counted yet)" << std::endl;
	}
	m_location_control.SetWindowText(pRow->path);
	DrawViews();
}

// ==================================================================== 한 장 카운팅
void CcountingDlg::OnBnClickedButtonStart()
{
	if (m_pProc == NULL)
	{
		AfxMessageBox(_T("처리기가 설정되지 않았습니다."));
		return;
	}
	if (_image_matrix.empty() || m_nCurRow < 0)
	{
		AfxMessageBox(_T("선택된 이미지가 없습니다."));
		return;
	}
	BeginWaitCursor();
	const bool success = m_pProc->Runprocess(_image_matrix, m_SegResult);
	EndWaitCursor();
	if (!success)
	{
		std::cout << "[Counting] Runprocess FAIL: " << m_SegResult.error << std::endl;
		return;
	}

	SegDrawOptions opt;
	_image_matrix.copyTo(_result_matrix);
	m_pProc->Draw(_result_matrix, m_SegResult, opt);
	SetRowResult(m_nCurRow, m_SegResult);                   // 행에 결과 저장 + Count / Dead / msec 표시

	std::cout << "[Counting] row " << m_nCurRow << "  count " << m_SegResult.Count()
		<< "  dead " << m_SegResult.CountVerdict(Verdict::Dead)
		<< "  total " << TotalMs(m_SegResult) << " ms" << std::endl;
	DrawViews();
}

// ==================================================================== 전체 카운팅
// 버튼 한 번 = 시작, 진행 중에 다시 누르면 = 중지
void CcountingDlg::OnBnClickedButtonAllcounting()
{
	if (m_bCounting)
	{
		m_bStopCount = true;
		GetDlgItem(IDC_BUTTON_ALLCOUNTING)->SetWindowText(_T("중지 중..."));
		std::cout << "[CountAll] stop requested" << std::endl;
		return;
	}
	if (m_pProc == NULL)
	{
		AfxMessageBox(_T("처리기가 설정되지 않았습니다."));
		return;
	}
	const int n = _filelist_control.GetItemCount();
	if (n <= 0)
	{
		AfxMessageBox(_T("파일 목록이 비어 있습니다."));
		return;
	}

	// 저장 여부 / 폴더
	const int ans = AfxMessageBox(_T("결과 이미지와 CSV를 저장할까요?\n(예: 폴더 선택, 아니요: 저장 안 함)"),
		MB_YESNOCANCEL | MB_ICONQUESTION);
	if (ans == IDCANCEL)
		return;
	m_strCountSaveDir.Empty();
	if (ans == IDYES)
	{
		const CString first = GetRow(0) ? GetRow(0)->path : CString();
		const CString start = m_strSaveDir.IsEmpty() ? first.Left(first.ReverseFind(_T('\\'))) : m_strSaveDir;
		CFolderPickerDialog dlg(start, 0, this);
		dlg.m_ofn.lpstrTitle = _T("결과 저장 폴더 선택");
		if (dlg.DoModal() != IDOK)
			return;
		m_strCountSaveDir = dlg.GetPathName();
		m_strSaveDir = m_strCountSaveDir;
	}

	// 경로 복사 (작업 스레드는 리스트를 직접 안 건드림) + 화면 초기화
	m_vCountPaths.clear();
	for (int i = 0; i < n; ++i)
	{
		FileRowData* pRow = GetRow(i);
		m_vCountPaths.push_back(pRow ? pRow->path : _filelist_control.GetItemText(i, 0));
		_filelist_control.SetItemText(i, 1, _T(""));
		_filelist_control.SetItemText(i, 2, _T(""));
		_filelist_control.SetItemText(i, 3, _T(""));
	}
	m_nAllImages = m_nAllCount = m_nAllDead = m_nAllFail = 0;
	m_dAllMs = 0.0;
	m_bStopCount = false;
	std::cout << "[CountAll] start " << n << " image(s), save "
		<< (m_strCountSaveDir.IsEmpty() ? std::string("OFF") : std::string(CT2A(m_strCountSaveDir))) << std::endl;

	SetBusy(true);
	m_pCountThread = AfxBeginThread(CountAllThread, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pCountThread == NULL)
	{
		std::cout << "[CountAll] thread create FAIL" << std::endl;
		SetBusy(false);
		return;
	}
	m_pCountThread->m_bAutoDelete = FALSE;                  // 끝나도 객체 유지 -> 우리가 delete
	m_pCountThread->ResumeThread();
}

// AfxBeginThread 진입점 (객체 없이 불리므로 static) -> 실제 처리는 RunCountAll
UINT CcountingDlg::CountAllThread(LPVOID pParam)
{
	return ((CcountingDlg*)pParam)->RunCountAll();
}

// 작업 스레드 : 한 장씩 읽기 -> RunProcess -> Draw -> (저장) -> UI 로 결과 전달
UINT CcountingDlg::RunCountAll()
{
	const int n = (int)m_vCountPaths.size();
	SegDrawOptions opt;
	for (int i = 0; i < n; ++i)
	{
		if (m_bStopCount)
			break;
		const CString& path = m_vCountPaths[i];
		CountRowResult* pRes = new CountRowResult();         // UI(OnCountProgress)에서 delete
		pRes->row = i;

		pRes->origin = ReadImageW(path.GetString());
		if (pRes->origin.empty())
		{
			std::cout << "[CountAll] " << i + 1 << "/" << n << " read FAIL: " << CT2A(path) << std::endl;
		}
		else if (m_pProc->Runprocess(pRes->origin, pRes->seg))
		{
			pRes->ok = true;
			pRes->origin.copyTo(pRes->result);
			m_pProc->Draw(pRes->result, pRes->seg, opt);

			if (!m_strCountSaveDir.IsEmpty())
			{
				const CString fileName = path.Mid(path.ReverseFind(_T('\\')) + 1);
				CString stem = fileName;
				const int dot = stem.ReverseFind(_T('.'));
				if (dot > 0)
					stem = stem.Left(dot);
				const CString imgPath = m_strCountSaveDir + _T("\\") + stem + _T("_result.jpg");
				if (!SaveImageW(imgPath, pRes->result))
					std::cout << "[CountAll] save image FAIL: " << CT2A(imgPath) << std::endl;
				if (!AppendResultCsv(m_strCountSaveDir + _T("\\result.csv"), fileName, pRes->seg))
					std::cout << "[CountAll] csv FAIL (엑셀에서 열려 있으면 닫기)" << std::endl;
			}
			std::cout << "[CountAll] " << i + 1 << "/" << n << " count " << pRes->seg.Count()
				<< " dead " << pRes->seg.CountVerdict(Verdict::Dead)
				<< " (" << TotalMs(pRes->seg) << " ms) " << CT2A(path) << std::endl;
		}
		else
		{
			std::cout << "[CountAll] " << i + 1 << "/" << n << " process FAIL: " << pRes->seg.error << std::endl;
		}

		if (!::PostMessage(m_hWnd, WM_COUNT_PROGRESS, (WPARAM)i, (LPARAM)pRes))
			delete pRes;                                      // 창이 이미 없으면 여기서 정리
	}
	::PostMessage(m_hWnd, WM_COUNT_DONE, m_bStopCount ? 1 : 0, 0);
	return 0;
}

// UI 스레드 : 한 장 결과 반영
LRESULT CcountingDlg::OnCountProgress(WPARAM wParam, LPARAM lParam)
{
	CountRowResult* pRes = (CountRowResult*)lParam;
	if (pRes == NULL)
		return 0;
	const int row = pRes->row;
	++m_nAllImages;
	if (pRes->ok)
	{
		SetRowResult(row, pRes->seg);                         // 행에 결과 저장 (클릭하면 바로 다시 그림)
		m_nAllCount += pRes->seg.Count();
		m_nAllDead += pRes->seg.CountVerdict(Verdict::Dead);
		m_dAllMs += TotalMs(pRes->seg);

		// 화면 : 방금 처리한 사진 (리스트 선택은 안 바꿈 -> ShowFile 이 다시 읽지 않게)
		_image_matrix = pRes->origin;
		_result_matrix = pRes->result;
		m_SegResult = pRes->seg;
		m_nCurRow = row;
	}
	else
	{
		_filelist_control.SetItemText(row, 1, _T("FAIL"));
		++m_nAllFail;
	}
	_filelist_control.EnsureVisible(row, FALSE);

	CString prog;
	prog.Format(_T("전체 카운팅 %d / %d"), row + 1, (int)m_vCountPaths.size());
	m_location_control.SetWindowText(prog);
	delete pRes;
	DrawViews();
	return 0;
}

// UI 스레드 : 끝 (완료 / 중지)
LRESULT CcountingDlg::OnCountDone(WPARAM wParam, LPARAM lParam)
{
	if (m_pCountThread != NULL)
	{
		::WaitForSingleObject(m_pCountThread->m_hThread, INFINITE);
		delete m_pCountThread;
		m_pCountThread = NULL;
	}
	SetBusy(false);

	const int nOk = m_nAllImages - m_nAllFail;
	CString msg;
	msg.Format(_T("%s\n\n이미지 %d장 (실패 %d)\n전체 %d마리, 폐사 %d마리\n평균 %.0f ms/장"),
		wParam ? _T("중지했습니다.") : _T("전체 카운팅 완료"),
		m_nAllImages, m_nAllFail, m_nAllCount, m_nAllDead, nOk > 0 ? m_dAllMs / nOk : 0.0);
	if (!m_strCountSaveDir.IsEmpty())
		msg += _T("\n저장: ") + m_strCountSaveDir;
	std::cout << "[CountAll] " << (wParam ? "stopped" : "done") << ": images " << m_nAllImages
		<< " fail " << m_nAllFail << " count " << m_nAllCount << " dead " << m_nAllDead << std::endl;
	AfxMessageBox(msg);
	return 0;
}

// 진행 중 : 다른 버튼 / 리스트 막기, 전체 카운팅 버튼은 "중지" 로
void CcountingDlg::SetBusy(bool busy)
{
	m_bCounting = busy;
	GetDlgItem(IDC_BUTTON_FILE)->EnableWindow(!busy);
	GetDlgItem(IDC_BUTTON_FOLDERPATH)->EnableWindow(!busy);
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(!busy);
	GetDlgItem(IDC_BUTTON_SAVE)->EnableWindow(!busy);
	GetDlgItem(IDC_BUTTON_ALLCOUNTING)->SetWindowText(busy ? _T("중지") : _T("전체 카운팅"));
	_filelist_control.EnableWindow(!busy);
}

// 진행 중이면 멈추고 스레드 정리 (Caquamain::OnDestroy 에서 Cprocesser 해제 전에 호출)
void CcountingDlg::StopCountAll()
{
	if (m_pCountThread == NULL)
		return;
	m_bStopCount = true;
	::WaitForSingleObject(m_pCountThread->m_hThread, INFINITE);   // 지금 사진까지 끝나길 기다림
	delete m_pCountThread;
	m_pCountThread = NULL;
	m_bCounting = false;

	// 아직 처리 안 된 결과 메시지 정리 (new 한 것 delete)
	MSG msg;
	while (::PeekMessage(&msg, m_hWnd, WM_COUNT_PROGRESS, WM_COUNT_DONE, PM_REMOVE))
	{
		if (msg.message == WM_COUNT_PROGRESS)
			delete (CountRowResult*)msg.lParam;
	}
	std::cout << "[CountAll] stopped and cleaned up" << std::endl;
}

// ==================================================================== 저장
void CcountingDlg::OnBnClickedButtonSave()
{
	FileRowData* pRow = GetRow(m_nCurRow);
	if (_result_matrix.empty() || pRow == NULL)
	{
		AfxMessageBox(_T("저장할 결과가 없습니다. 먼저 카운팅하세요."));
		return;
	}

	const CString& src = pRow->path;                                  // 원본 전체 경로
	const int slash = src.ReverseFind(_T('\\'));
	const CString fileName = src.Mid(slash + 1);

	// 1) 저장 폴더 선택 (처음엔 원본 폴더, 그 뒤엔 마지막 저장 폴더에서 시작)
	const CString start = m_strSaveDir.IsEmpty() ? src.Left(slash) : m_strSaveDir;
	CFolderPickerDialog dlg(start, 0, this);
	dlg.m_ofn.lpstrTitle = _T("결과 저장 폴더 선택");
	if (dlg.DoModal() != IDOK)
		return;
	m_strSaveDir = dlg.GetPathName();
	std::cout << "[Save] folder: " << CT2A(m_strSaveDir) << std::endl;

	// 2) 결과 이미지
	CString stem = fileName;
	const int dot = stem.ReverseFind(_T('.'));
	if (dot > 0)
		stem = stem.Left(dot);
	const CString imgPath = m_strSaveDir + _T("\\") + stem + _T("_result.jpg");
	if (!SaveImageW(imgPath, _result_matrix))
	{
		std::cout << "[Save] image FAIL: " << CT2A(imgPath) << std::endl;
		AfxMessageBox(_T("결과 이미지 저장 실패"));
		return;
	}
	std::cout << "[Save] image: " << CT2A(imgPath) << std::endl;

	// 3) CSV 한 줄 추가
	const CString csvPath = m_strSaveDir + _T("\\result.csv");
	if (!AppendResultCsv(csvPath, fileName, m_SegResult))
	{
		std::cout << "[Save] csv FAIL: " << CT2A(csvPath) << std::endl;
		AfxMessageBox(_T("result.csv 저장 실패 (엑셀에서 열려 있으면 닫고 다시 저장하세요)"));
		return;
	}
	std::cout << "[Save] csv: " << CT2A(csvPath) << std::endl;
	AfxMessageBox(_T("저장했습니다.\n") + imgPath);
}

bool CcountingDlg::SaveImageW(const CString& path, const cv::Mat& img) const
{
	std::vector<uchar> buf;
	const std::vector<int> prm = { cv::IMWRITE_JPEG_QUALITY, 95 };
	if (img.empty() || !cv::imencode(".jpg", img, buf, prm))
		return false;
	FILE* fp = NULL;
	if (_wfopen_s(&fp, CStringW(path).GetString(), L"wb") != 0 || fp == NULL)
		return false;
	const size_t n = fwrite(buf.data(), 1, buf.size(), fp);
	fclose(fp);
	return n == buf.size();
}

bool CcountingDlg::AppendResultCsv(const CString& csvPath, const CString& fileName, const SegResult& res) const
{
	const bool isNew = (::GetFileAttributes(csvPath) == INVALID_FILE_ATTRIBUTES);
	FILE* fp = NULL;
	if (_wfopen_s(&fp, CStringW(csvPath).GetString(), L"ab") != 0 || fp == NULL)
		return false;
	if (isNew)
	{
		const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };     // 엑셀에서 한글 안 깨지게 UTF-8 BOM
		fwrite(bom, 1, 3, fp);
		fputs("datetime,file,count,dead,partial,total_ms,gpu\r\n", fp);
	}

	int nPartial = 0;
	for (const SegObject& o : res.objects)
		if (o.cropClass == CropClass::Partial) ++nPartial;

	SYSTEMTIME st;
	::GetLocalTime(&st);
	char line[512];
	sprintf_s(line, "%04d-%02d-%02d %02d:%02d:%02d,\"%s\",%d,%d,%d,%.1f,%d\r\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
		(const char*)CT2A(fileName, CP_UTF8),
		res.Count(), res.CountVerdict(Verdict::Dead), nPartial,
		TotalMs(res), res.cuda ? 1 : 0);
	fputs(line, fp);
	fclose(fp);
	return true;
}

// ==================================================================== 화면
void CcountingDlg::OnPaint()
{
	CPaintDC dc(this);
	DrawViews();
}

void CcountingDlg::DrawViews()
{
	// 왼쪽 : 원본
	if (::IsWindow(_image_control.GetSafeHwnd()))
	{
		RECT rect;
		_image_control.GetClientRect(&rect);
		CDC* p_dc = _image_control.GetDC();
		if (!_image_matrix.empty())
			DisplayImage(_image_matrix, p_dc, rect);
		else
			p_dc->FillSolidRect(&rect, GetSysColor(COLOR_BTNFACE));
		_image_control.ReleaseDC(p_dc);
	}

	// 오른쪽 : 결과
	CWnd* p_resultwnd = GetDlgItem(IDC_STATIC_IMAGEVIEW);
	if (p_resultwnd != nullptr)
	{
		RECT rect;
		p_resultwnd->GetClientRect(&rect);
		CDC* p_resultdc = p_resultwnd->GetDC();
		if (!_result_matrix.empty())
			DisplayImage(_result_matrix, p_resultdc, rect);
		else
			p_resultdc->FillSolidRect(&rect, GetSysColor(COLOR_BTNFACE));
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
	else                                   bgr = image_matrix;

	// 2) 비율 유지 축소, 가로는 4의 배수 (DIB 행 정렬)
	const double ratio = (std::min)((double)cw / bgr.cols, (double)ch / bgr.rows);
	int w = (int)(bgr.cols * ratio);  w -= w % 4;
	int h = (int)(bgr.rows * ratio);
	if (w < 4 || h < 1)
		return;
	cv::Mat view;
	cv::resize(bgr, view, cv::Size(w, h), 0, 0, ratio < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);

	// 3) top-down DIB (biHeight 음수)
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = view.cols;
	bi.bmiHeader.biHeight = -view.rows;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	bi.bmiHeader.biCompression = BI_RGB;

	// 4) 가운데 정렬
	const int x = rect.left + (cw - w) / 2;
	const int y = rect.top + (ch - h) / 2;
	::SetStretchBltMode(p_dc->GetSafeHdc(), HALFTONE);          // 축소해도 윤곽선이 덜 끊김
	::SetBrushOrgEx(p_dc->GetSafeHdc(), 0, 0, NULL);
	::StretchDIBits(p_dc->GetSafeHdc(), x, y, w, h, 0, 0, view.cols, view.rows,
		view.data, &bi, DIB_RGB_COLORS, SRCCOPY);
}
