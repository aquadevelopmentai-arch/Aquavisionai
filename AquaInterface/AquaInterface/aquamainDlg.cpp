// aquamain.cpp: 구현 파일
//

#include "pch.h"
#include "AquaInterface.h"
#include "afxdialogex.h"
#include "aquamainDlg.h"


// Caquamain 대화 상자

IMPLEMENT_DYNAMIC(Caquamain, CDialogEx)

Caquamain::Caquamain(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_AQUAMAIN, pParent)
{
	m_pAquaDlg = NULL;
	m_pCountingDlg = NULL;
	m_pProcess = NULL;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

Caquamain::~Caquamain()
{
}

void Caquamain::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_FUNCTION, m_aqua_tab_control);
}


BEGIN_MESSAGE_MAP(Caquamain, CDialogEx)
	//ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_FUNCTION, &Caquamain::OnTcnSelchangeTab1)
	ON_NOTIFY(TCN_SELCHANGING, IDC_TAB_FUNCTION, &Caquamain::OnTcnSelchangingTab)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_FUNCTION, &Caquamain::OnTcnSelchangeTab)
END_MESSAGE_MAP()

BOOL Caquamain::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  여기에 추가 초기화 작업을 추가합니다.
	m_aqua_tab_control.InsertItem(0, L"분류기");
	m_aqua_tab_control.InsertItem(1, L"카운팅");

	m_aqua_tab_control.SetCurSel(0);

	CRect rect_tab;
	m_aqua_tab_control.GetWindowRect(&rect_tab);

	// dlg 1
	m_pAquaDlg = new CAquaInterfaceDlg();
	m_pAquaDlg->Create(IDD_AQUAINTERFACE_DIALOG, &m_aqua_tab_control);
	m_pAquaDlg->MoveWindow(0, 25, rect_tab.Width(), rect_tab.Height());
	m_pAquaDlg->ShowWindow(SW_SHOW);

	// dlg 2
	m_pCountingDlg = new CcountingDlg();
	m_pCountingDlg->Create(IDD_COUNTING_DIALOG, &m_aqua_tab_control);
	m_pCountingDlg->MoveWindow(0, 25, rect_tab.Width(), rect_tab.Height());
	m_pCountingDlg->ShowWindow(SW_HIDE);

	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.
/*	if (m_pProcess == NULL)
	{
		m_pProcess = new Cprocesser();
		if (m_pProcess->InitYoloseg() && m_pProcess->InitAnomaly() && m_pProcess->initCrrop())
		{
			std::cout << "[Process] Yoloseg, Anomaly, and Crop initialized successfully" << std::endl;
		}
		else
		{
			AfxMessageBox(_T("처리기 초기화 실패"));
			delete m_pProcess;
			m_pProcess = NULL;
		}
	}
	m_pCountingDlg->SetProcesser(m_pProcess);\
*/
	return TRUE;  // return TRUE unless you set the focus to a control
	// 예외: OCX 속성 페이지는 FALSE를 반환해야 합니다.
}

// Caquamain 메시지 처리기

void Caquamain::OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	*pResult = 0;
	int cur_sel = m_aqua_tab_control.GetCurSel();
	if (cur_sel == 0)
	{
		m_pAquaDlg->ShowWindow(SW_SHOW);
		m_pCountingDlg->ShowWindow(SW_HIDE);
	}
	else if (cur_sel == 1)
	{
		m_pAquaDlg->ShowWindow(SW_HIDE);
		m_pCountingDlg->ShowWindow(SW_SHOW);
	}
}

void Caquamain::OnCancel()
{
	if (m_pCountingDlg != NULL && m_pCountingDlg->IsCounting())
	{
		const int ans = AfxMessageBox(_T("전체 카운팅이 진행 중입니다.\n중지하고 종료할까요?"),
			MB_YESNO | MB_ICONWARNING);
		if (ans != IDYES)
		{
			std::cout << "[Main] close canceled (counting in progress)" << std::endl;
			return;                                   // 닫지 않음
		}
		std::cout << "[Main] stop counting and close" << std::endl;
		m_pCountingDlg->StopCountAll();               // 지금 사진까지 끝내고 정지
	}
	CDialogEx::OnCancel();                            // -> DestroyWindow -> 해제 순서대로 정리
}

void Caquamain::OnOK()
{
	// Enter로 프로그램이 꺼지지 않게 막기
}

BOOL Caquamain::DestroyWindow()
{

	// 1) 전체 카운팅 스레드부터 정지 (Cprocesser를 지우기 전에)
	if (m_pCountingDlg != NULL)
	{
		m_pCountingDlg->StopCountAll();
		m_pCountingDlg->SetProcesser(NULL);
	}

	// 2) Cprocesser 해제 (모델 -> Env)
	if (m_pProcess != NULL)
	{
		//m_pProcess->Release();
		delete m_pProcess;
		m_pProcess = NULL;
	}

	// 3) 탭 다이얼로그 정리 : 창을 닫고(DestroyWindow -> OnDestroy 자동 호출) 객체 delete
	if (m_pAquaDlg != NULL)
	{
		if (::IsWindow(m_pAquaDlg->GetSafeHwnd()))
		{
			m_pAquaDlg->DestroyWindow();
		}
		delete m_pAquaDlg;
		m_pAquaDlg = NULL;
	}
	if (m_pCountingDlg != NULL)
	{
		m_pCountingDlg->DestroyWindow();
		delete m_pCountingDlg;
		m_pCountingDlg = NULL;
	}
	//m_pAquaDlg->OnDestroy();
	//m_pCountingDlg->OnDestroy();

	//delete m_pAquaDlg;
	//m_pAquaDlg = NULL;
	ReleaseProcesser();

	return CDialogEx::DestroyWindow();
}

// 탭 바뀌기 전 : 전체 카운팅 중이면 막기
void Caquamain::OnTcnSelchangingTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = FALSE;                                     // FALSE = 전환 허용
	if (m_pCountingDlg != NULL && m_pCountingDlg->IsCounting())
	{
		std::cout << "[Main] tab change blocked (counting in progress)" << std::endl;
		AfxMessageBox(_T("전체 카운팅이 진행 중입니다.\n[중지] 후 탭을 바꾸세요."));
		*pResult = TRUE;                                  // TRUE = 전환 취소
	}
}

// 탭 바뀐 후 : 카운팅 탭이면 초기화, 분류기 탭이면 해제
void Caquamain::OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	const int sel = m_aqua_tab_control.GetCurSel();
	if (sel == 0)                                 // 분류기
	{
		m_pCountingDlg->ShowWindow(SW_HIDE);
		ReleaseProcesser();                       // GPU 메모리 해제
		m_pAquaDlg->ShowWindow(SW_SHOW);
	}
	else if (sel == 1)                            // 카운팅
	{
		m_pAquaDlg->ShowWindow(SW_HIDE);
		m_pCountingDlg->ShowWindow(SW_SHOW);
		m_pCountingDlg->UpdateWindow();           // 탭 화면 먼저 보여주고
		EnsureProcesser();                        // 모델 로딩 (모래시계 몇 초)
	}
	*pResult = 0;
}

bool Caquamain::EnsureProcesser()
{
	if (m_pProcess != NULL)
		return true;

	std::cout << "[Main] processer init" << std::endl;
	BeginWaitCursor();
	m_pProcess = new Cprocesser();
	std::string segpath, adpath;
	const bool ok = m_pProcess->InitFromSetting("Setting.ini", segpath, adpath);
	SegParams sp;
	sp.useCuda = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "usecuda", true);
	sp.deviceId = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "deviceid", 0);
	sp.confTh = m_pProcess->ReadIniFloat("Setting.ini", "SEGMENTATION", "confthreashold", 0.25f);
	sp.iouTh = m_pProcess->ReadIniFloat("Setting.ini", "SEGMENTATION", "iouthreashold", 0.5f);
	sp.maxDet = m_pProcess->ReadIniInt("Setting.ini", "SEGMENTATION", "maxdetection", 1000);
	sp.maskTh = m_pProcess->ReadIniFloat("Setting.ini", "SEGMENTATION", "maskthreashold", 0.5f);
	sp.useMask = m_pProcess->ReadIniInt("Setting.ini", "SEGMENTATION", "usemask", true);
	sp.warmup = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "segwarmup", 1);
	sp.minPolyArea = m_pProcess->ReadIniInt("Setting.ini", "SEGMENTATION", "minpolyarea", 100);
	sp.inputSize = m_pProcess->ReadIniInt("Setting.ini", "SEGMENTATION", "inputsize", 0);
	if (!m_pProcess->InitYoloseg(segpath, sp))                       // 필수
	{
		EndWaitCursor();
		std::cout << "[Main] Yoloseg init FAIL" << std::endl;
		AfxMessageBox(_T("세그멘테이션 모델 초기화 실패\n(models 폴더와 CUDA 설치를 확인하세요)"));
		delete m_pProcess;
		m_pProcess = NULL;
		return false;
	}
	AnomalyParams ap;
	ap.maxBatch = m_pProcess->ReadIniInt("Setting.ini", "ANOMALY", "adbatch", 4);
	ap.threshold = m_pProcess->ReadIniFloat("Setting.ini", "ANOMALY", "adthreashold", 0.20f);
	ap.warmup = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "Warmup", 1);
	ap.useCuda = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "usecuda", true);
	ap.deviceId = m_pProcess->ReadIniInt("Setting.ini", "DEEPLEARNINGINFO", "deviceid", 0);
	const bool anomalyOk = m_pProcess->InitAnomaly(adpath, ap) && m_pProcess->initCrrop();   // 실패해도 카운팅은 가능
	EndWaitCursor();
	if (!anomalyOk)
	{
		std::cout << "[Main] Anomaly init FAIL -> count only" << std::endl;
		AfxMessageBox(_T("이상탐지 모델 초기화 실패\n카운팅만 동작합니다 (폐사 판정 없음)."));
	}
	m_pCountingDlg->SetProcesser(m_pProcess);
	std::cout << "[Main] processer ready" << std::endl;
	return true;
}

void Caquamain::ReleaseProcesser()
{
	if (m_pProcess == NULL)
		return;
	if (m_pCountingDlg != NULL)
	{
		m_pCountingDlg->StopCountAll();                   // 혹시 돌고 있으면 정지 (보통은 위에서 막힘)
		m_pCountingDlg->SetProcesser(NULL);               // 카운팅 탭이 지워진 객체를 안 쓰게
	}
	delete m_pProcess;                                    // ~Cprocesser : 모델 -> Env 순서로 해제
	m_pProcess = NULL;
	std::cout << "[Main] processer released (GPU memory freed)" << std::endl;
}