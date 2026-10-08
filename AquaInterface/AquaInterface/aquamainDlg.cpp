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
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_FUNCTION, &Caquamain::OnTcnSelchangeTab1)
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

BOOL Caquamain::DestroyWindow()
{
	m_pAquaDlg->OnDestroy();
	//m_pCountingDlg->DestroyWindow();

	delete m_pAquaDlg;
	m_pAquaDlg = NULL;

	return CDialogEx::DestroyWindow();
}