
// AquaInterface.cpp: 애플리케이션에 대한 클래스 동작을 정의합니다.
//

#include "pch.h"
#include "framework.h"
#include "AquaInterface.h"
#include "AquaInterfaceDlg.h"

#include <gdiplus.h>                          
#pragma comment(lib, "gdiplus.lib") 

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAquaInterfaceApp

BEGIN_MESSAGE_MAP(CAquaInterfaceApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CAquaInterfaceApp 생성

CAquaInterfaceApp::CAquaInterfaceApp()
{
	// 다시 시작 관리자 지원
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 여기에 생성 코드를 추가합니다.
	// InitInstance에 모든 중요한 초기화 작업을 배치합니다.
}


// 유일한 CAquaInterfaceApp 개체입니다.

CAquaInterfaceApp theApp;


// CAquaInterfaceApp 초기화

BOOL CAquaInterfaceApp::InitInstance()
{
	// [추가] ---- GDI+ 시작 (대화 상자보다 먼저) ----
	Gdiplus::GdiplusStartupInput gdiplusInput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusInput, nullptr);
	// --------------------------------------------

	// Windows XP에서는 InitCommonControlsEx()를 필요로 합니다.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// 대화 상자에 셸 트리 뷰 또는 셸 목록 뷰 컨트롤이 포함되어 있는 경우 셸 관리자를 만듭니다.
	CShellManager* pShellManager = new CShellManager;

	// MFC 컨트롤의 테마를 사용하기 위해 "Windows 원형" 비주얼 관리자 활성화
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("로컬 애플리케이션 마법사에서 생성된 애플리케이션"));

	CAquaInterfaceDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "경고: 대화 상자를 만들지 못했으므로 애플리케이션이 예기치 않게 종료됩니다.\n");
		TRACE(traceAppMsg, 0, "경고: 대화 상자에서 MFC 컨트롤을 사용하는 경우 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS를 수행할 수 없습니다.\n");
	}

	// 위에서 만든 셸 관리자를 삭제합니다.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
		pShellManager = nullptr;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 대화 상자가 닫혔으므로 FALSE 반환 -> 이후 ExitInstance 가 호출됨
	return FALSE;
}

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
// [추가] ---- GDI+ 해제 (대화 상자가 모두 사라진 뒤 호출됨) ----
int CAquaInterfaceApp::ExitInstance()
{
	if (m_gdiplusToken)
	{
		Gdiplus::GdiplusShutdown(m_gdiplusToken);
		m_gdiplusToken = 0;
	}
	return CWinApp::ExitInstance();
}
