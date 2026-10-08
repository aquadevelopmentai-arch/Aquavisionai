// AquaInterfaceDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "AquaInterface.h"
#include "AquaInterfaceDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ============================================================
//  표시 규칙
//    [VS 기본]  : VS 마법사가 만든 코드 (지우면 링크 에러)
//    [추가]     : 카메라 / LED 연동으로 새로 넣은 코드
// ============================================================


// ============================================================
//  [VS 기본] CAboutDlg : 응용 프로그램 정보 대화 상자
// ============================================================
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// ============================================================
//  [VS 기본] 생성자
// ============================================================
CAquaInterfaceDlg::CAquaInterfaceDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AQUAINTERFACE_DIALOG, pParent)
{
}

// [추가] 소멸자 (OnDestroy 에서 이미 해제했으면 아무것도 안 함)
CAquaInterfaceDlg::~CAquaInterfaceDlg()
{
	// 보통은 OnDestroy 에서 이미 해제됨. 혹시 남아 있으면 여기서 정리.
	if (m_pcam)
	{
		m_pcam->Close();
		delete m_pcam;
		m_pcam = nullptr;
	}

	if (m_pPie) 
	{ 
	   delete m_pPie; 
	   m_pPie = nullptr; 
	}
}

// [VS 기본] DoDataExchange
void CAquaInterfaceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_REDSYMBOL, m_ledRed);       
	DDX_Control(pDX, IDC_STATIC_GREENSYMBOL, m_ledGreen);     
}

// [VS 기본] 메시지 맵
BEGIN_MESSAGE_MAP(CAquaInterfaceDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_DESTROY()                                                         
	ON_WM_CTLCOLOR()
	ON_WM_TIMER()                                                           
	ON_MESSAGE(WM_USER_RESULT, &CAquaInterfaceDlg::OnResult)               
	ON_BN_CLICKED(IDC_BUTTON_START, &CAquaInterfaceDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_END, &CAquaInterfaceDlg::OnBnClickedEnd)             
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, &CAquaInterfaceDlg::OnLvnItemchangedList1)
	ON_BN_CLICKED(IDC_BUTTON_SETTINGFILE, &CAquaInterfaceDlg::OnBnClickedButtonSettingfile)
END_MESSAGE_MAP()


// ============================================================
//  [VS 기본] 초기화
// ============================================================
BOOL CAquaInterfaceDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 추가 (VS 기본 생성 코드)
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}


	// [추가] ---- 상태 LED ----
	m_ledRed.SetColor(RGB(255, 60, 60));
	m_ledGreen.SetColor(RGB(40, 255, 40));
	m_ledRed.SetSize(40);
	m_ledGreen.SetSize(40);
	SetRunState(false);         // 시작은 정지 상태 (빨강)

	// [추가] ---- 카메라 객체 ----
	if (m_pcam == nullptr)
	{
		m_pcam = new CBaslerLineScan();
	}
	
	m_pPie = new CPieChartCtrl();
	m_pPie->SubclassDlgItem(IDC_PIE_STATS, this);     // 리소스 컨트롤에 연결

	CRect rcPic;
	GetDlgItem(IDC_PIC_VIEW)->GetWindowRect(&rcPic);   // 왼쪽 위 사진 칸
	ScreenToClient(&rcPic);

	m_pPie->SetWindowPos(nullptr,
		rcPic.left,          // 사진 칸과 왼쪽 맞춤
		rcPic.bottom + 50,   // 사진 칸 아래 15픽셀
		400, 200,            // 폭, 높이
		SWP_NOZORDER | SWP_NOACTIVATE);

	m_pieA = m_pPie->AddItem(_T("A수조"), RGB(40, 180, 70), true, true);   // "1,234 (12)"
	m_pieB = m_pPie->AddItem(_T("B수조"), RGB(30, 170, 200), true, true);
	m_pieC = m_pPie->AddItem(_T("C수조"), RGB(240, 180, 40), true, true);
	m_pieNg = m_pPie->AddItem(_T("불량"), RGB(220, 50, 50), false, false);  // 범례만 (합계에 안 들어감)
	m_pieUnk = m_pPie->AddItem(_T("미판정"), RGB(140, 140, 140), true, false);
	m_pPie->SetTotalLabel(_T("합계"));
	m_pPie->Invalidate(FALSE);

	ResetStatus();

	return TRUE;
}

HBRUSH CAquaInterfaceDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (pWnd->GetDlgCtrlID() == IDC_ST_FRAMES && m_pBrIdle && m_pBrOk && m_pBrLost)
	{
		COLORREF bk;
		CBrush* pBr;
		switch (m_frameState)
		{
		case FrameState_Lost: bk = RGB(220, 50, 50); pBr = m_pBrLost; break;
		case FrameState_Ok:   bk = RGB(40, 170, 70); pBr = m_pBrOk;   break;
		default:              bk = RGB(200, 200, 200); pBr = m_pBrIdle; break;
		}

		pDC->SetBkColor(bk);                                   // 글자 바로 뒤
		pDC->SetTextColor(m_frameState == FrameState_Idle ? RGB(60, 60, 60)
			: RGB(255, 255, 255));
		return static_cast<HBRUSH>(pBr->GetSafeHandle());     // 칸 전체
	}
	return hbr;
}

// [VS 기본]
void CAquaInterfaceDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// [VS 기본] 최소화 상태에서 아이콘 그리기


// ============================================================
//  [추가] 검측 상태 : LED + 버튼 활성/비활성
// ============================================================
void CAquaInterfaceDlg::SetRunState(bool running)
{
	m_ledGreen.SetActive(running);
	m_ledRed.SetActive(!running);
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(!running);
	GetDlgItem(IDC_END)->EnableWindow(running);
}


// ============================================================
//  [추가] 시작 / 종료 / 창 닫기
// ============================================================

void CAquaInterfaceDlg::ShowCamError(LPCTSTR title)
{
	CString msg = title;
	if (m_pcam)
	{
		CString detail(m_pcam->GetLastError().c_str());
		if (!detail.IsEmpty())
			msg += _T("\n\n") + detail;
	}
	AfxMessageBox(msg, MB_OK | MB_ICONERROR);
}


void CAquaInterfaceDlg::OnBnClickedButtonStart()
{
	if (m_pcam == nullptr || m_pcam->IsGrabbing())
		return;

	// 연결 (이미 열려 있으면 바로 true)
	if (!m_pcam->Open())
	{
		ShowCamError(_T("카메라를 열 수 없습니다.\n연결 상태와 IP 설정을 확인하세요."));
		SetRunState(false);                    
		return;
	}
	if (m_pPie)
	{
		m_pPie->ResetValues();
	}
	// 카메라 설정 (Stop 상태에서만)
	LineScanConfig cfg;
	cfg.height = 128;
	cfg.lineRateHz = 10000.0;
	cfg.exposureUs = 50.0;
	cfg.useEncoder = false;
	cfg.maxBuffers = 100;
	if (!m_pcam->Configure(cfg))
	{
		AfxMessageBox(CString(m_pcam->GetLastError().c_str()));
		return;
	}

	// 저장 설정
	SaveConfig sc;
	sc.mode = SaveMode_Requested;          
	sc.format = SaveFormat_Bmp; 
	sc.maxQueue = 30;
	strcpy_s(sc.folder, "C:\\AquaImages");
	m_pcam->SetSaveConfig(sc);

	// 취득 시작
	m_nOk = m_nNg = 0;
	if (!m_pcam->Start(&CAquaInterfaceDlg::OnChunk, this))
	{
		ShowCamError(_T("카메라 취득을 시작할 수 없습니다."));
		SetRunState(false);                    
		return;
	}
	ResetStatus();
	SetTimer(TIMER_STATUS, 500, nullptr);      // 0.5초마다 상태 갱신
	SetRunState(true);                         // 초록 ON
}

void CAquaInterfaceDlg::ResetStatus()
{
	m_lastTick = ::GetTickCount64();
	m_lastFrames = 0;

	m_pBrIdle = new CBrush(RGB(200, 200, 200));
	m_pBrOk = new CBrush(RGB(40, 170, 70));
	m_pBrLost = new CBrush(RGB(220, 50, 50));

	SetDlgItemText(IDC_ST_FRAMES, _T("0 (0)"));

	m_frameState = FrameState_Idle;                    // 회색
	GetDlgItem(IDC_ST_FRAMES)->Invalidate();
}


CString CAquaInterfaceDlg::FormatCountSub(unsigned long long count, unsigned long long sub)
{
	return FormatNum(count) + _T(" (") + FormatNum(sub) + _T(")");
}

CString CAquaInterfaceDlg::FormatNum(unsigned long long v)          // 12345 -> "12,345"
{
	CString s;
	s.Format(_T("%llu"), v);
	for (int i = s.GetLength() - 3; i > 0; i -= 3)
		s.Insert(i, _T(','));
	return s;
}

// OnTimer 에서 주기적으로 호출
void CAquaInterfaceDlg::UpdateStatus()
{
	if (m_pcam == nullptr)
		return;

	const std::uint64_t frames = m_pcam->GetFrameCount();
	const std::uint64_t lost = m_pcam->GetLostCount();
	const ULONGLONG     now = ::GetTickCount64();
	const ULONGLONG     dt = now - m_lastTick;

	// FPS = 이번 구간에 받은 프레임 / 경과 시간
	double fps = 0.0;
	if (dt > 0)
		fps = static_cast<double>(frames - m_lastFrames) * 1000.0 / static_cast<double>(dt);
	m_lastTick = now;
	m_lastFrames = frames;

	CString s;

	// 프레임 (누락)
	s = FormatNum(frames) + _T(" (") + FormatNum(lost) + _T(")");
	SetDlgItemText(IDC_ST_FRAMES, s);

	// 색 상태 : 누락 있으면 빨강, 없으면 초록
	FrameState st = (lost > 0) ? FrameState_Lost : FrameState_Ok;
	if (st != m_frameState)
	{
		m_frameState = st;
		GetDlgItem(IDC_ST_FRAMES)->Invalidate();       // 색 바뀔 때만 다시 그림
	}
}

void CAquaInterfaceDlg::OnBnClickedEnd()
{
	KillTimer(TIMER_STATUS);
	if (m_pcam)
	{
		m_pcam->Stop();                        // 남은 저장까지 끝내고 반환
		UpdateStatus();
		m_frameState = FrameState_Idle;
		GetDlgItem(IDC_ST_FRAMES)->Invalidate();
	}

	SetRunState(false);                        // 빨강 ON
}

void CAquaInterfaceDlg::OnDestroy()
{
	KillTimer(TIMER_STATUS);
	if (m_pcam)
	{
		m_pcam->Close();                       // 창이 없어지기 전에 스레드 정리
		delete m_pcam;                         // 카메라 해제 -> PylonTerminate
		m_pcam = nullptr;
	}

	if (m_pPie)
	{
		m_pPie->UnsubclassWindow();     // 창 연결 해제 (창 자체는 대화 상자가 없앰)
		delete m_pPie;                  // 내부 Item 들은 소멸자에서 delete
		m_pPie = nullptr;
	}

	if (m_pBrIdle) 
	{ 
		delete m_pBrIdle; m_pBrIdle = nullptr; 
	}
	if (m_pBrOk) 
	{ 
		delete m_pBrOk;   m_pBrOk = nullptr; 
	}
	if (m_pBrLost) 
	{ 
		delete m_pBrLost; m_pBrLost = nullptr; 
	}

	CDialogEx::OnDestroy();
}


// ============================================================
//  [추가] 처리 스레드에서 호출 (UI 스레드 아님!)
// ============================================================
bool CAquaInterfaceDlg::OnChunk(const FrameChunk& c, void* user)
{
	CAquaInterfaceDlg* dlg = static_cast<CAquaInterfaceDlg*>(user);

	// TODO: 새우 검출 + 길이 측정 + 이상탐지 (모델 붙이면 여기서 계산)
	//   c.data, c.width, c.height, c.stride, c.pixelFormat
	double weightG = 0.0;     // 길이로 추정한 무게 (g)
	bool   isAnomaly = false;   // 이상탐지 결과

	// ---- 1) 무게 구간으로 수조 결정 ----
	WPARAM wp = RESULT_UNKNOWN;
	if (weightG <= 0.0)  wp = RESULT_UNKNOWN;   // 측정 실패
	else if (weightG < 20.0) wp = RESULT_TANK_C;    // 예시 기준 (나중에 ini 값으로)
	else if (weightG < 23.0) wp = RESULT_TANK_B;
	else                      wp = RESULT_TANK_A;

	// ---- 2) 불량이면 플래그 ----
	if (isAnomaly)
		wp |= RESULT_FLAG_NG;

	// ---- 3) UI 로 전달 (PostMessage 만, SendMessage 금지 -> Stop 시 데드락) ----
	dlg->PostMessage(WM_USER_RESULT, wp, (LPARAM)c.frameIndex);

	return isAnomaly;           // true -> 이미지 저장 (SaveMode_Requested)
}

// ============================================================
//  [추가] UI 스레드 : 결과 카운트 / 상태 표시
// ============================================================
LRESULT CAquaInterfaceDlg::OnResult(WPARAM wp, LPARAM /*lp*/)
{
	if (wp == 0) ++m_nOk;
	else         ++m_nNg;

	if (m_pPie)
	{
		const int  tank = static_cast<int>(wp & RESULT_TANK_MASK);
		const bool ng = (wp & RESULT_FLAG_NG) != 0;

		int idx = m_pieUnk;
		if (tank == RESULT_TANK_A) idx = m_pieA;
		else if (tank == RESULT_TANK_B) idx = m_pieB;
		else if (tank == RESULT_TANK_C) idx = m_pieC;

		m_pPie->AddValue(idx);                  // 수조 마리 수 (합계에 포함)
		if (ng)
		{
			m_pPie->AddSub(idx);                // 그 수조 안의 불량 수 -> ( ) 안에 표시
			m_pPie->AddValue(m_pieNg);          // 불량 전체 수 (합계에는 안 들어감)
		}
	}


	return 0;
}

void CAquaInterfaceDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_STATUS && m_pcam)
	{
		CString s;
		s.Format(_T("수신 %llu  누락 %llu  대기 %zu | OK %llu  NG %llu | 저장 %llu  skip %llu"),
			m_pcam->GetFrameCount(), m_pcam->GetLostCount(), m_pcam->GetQueueDepth(),
			m_nOk, m_nNg,
			m_pcam->GetSavedCount(), m_pcam->GetSaveDropCount());
		SetWindowText(s);                      // 일단 창 제목에 표시
	}
	if (m_pPie) m_pPie->Refresh();

	CDialogEx::OnTimer(nIDEvent);
}
void CAquaInterfaceDlg::OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	*pResult = 0;
}

void CAquaInterfaceDlg::OnBnClickedButtonSettingfile()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
}
