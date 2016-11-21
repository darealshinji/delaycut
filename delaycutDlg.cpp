/*
delaycut: cuts and corrects delay in ac3 and dts files
Copyright (C) 2004 by jsoto

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// delaycutDlg.cpp : implementation file
//
// 2007-09-11 E-AC3 support added by www.madshi.net
//

#include "stdafx.h"
#include "ToolTipDialog.h"
#include "delayac3.h"
#include "delaycut.h"
#include "delaycutDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int getwavinfo ( FILE *in, FILEINFO *myinfo);
extern int getac3info ( FILE *in, FILEINFO *myinfo);
extern int geteac3info( FILE *in, FILEINFO *myinfo);
extern int getdtsinfo ( FILE *in, FILEINFO *myinfo);
extern int getmpainfo ( FILE *in, FILEINFO *myinfo);
extern int gettargetinfo ( FILEINFO *fileinfo);
extern CDelaycutApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDelaycutDlg dialog

CDelaycutDlg::CDelaycutDlg(CWnd* pParent /*=NULL*/)
	: CToolTipDialog(CDelaycutDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDelaycutDlg)
	m_bCheckcut = FALSE;
	m_bCheckdelay = FALSE;
	m_csCutend = _T("0");
	m_csCutstart = _T("0");
	m_csInputfile = _T("");
	m_csOutputfile = _T("");
	m_csInfo = _T("");
	m_iCrc = 0;
	m_csDelayend = _T("0");
	m_csDelaystart = _T("0");
	m_csProgresstitle = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDelaycutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDelaycutDlg)
	DDX_Control(pDX, IDC_PROGRESSBAR, m_CProgressbar);
	DDX_Check(pDX, IDC_CHECKCUT, m_bCheckcut);
	DDX_Check(pDX, IDC_CHECKDELAY, m_bCheckdelay);
	DDX_Text(pDX, IDC_EDITCUTE, m_csCutend);
	DDX_Text(pDX, IDC_EDITCUTS, m_csCutstart);
	DDX_Text(pDX, IDC_EDITINPUT, m_csInputfile);
	DDX_Text(pDX, IDC_EDITOUTPUT, m_csOutputfile);
	DDX_Text(pDX, IDC_INFO, m_csInfo);
	DDX_Radio(pDX, IDC_RADIOCRC1, m_iCrc);
	DDX_Text(pDX, IDC_EDITDELAYE, m_csDelayend);
	DDX_Text(pDX, IDC_EDITDELAYS, m_csDelaystart);
	DDX_Text(pDX, IDC_PROGRESSTITLE, m_csProgresstitle);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDelaycutDlg, CDialog)
	//{{AFX_MSG_MAP(CDelaycutDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTONINPUT, OnButtoninput)
	ON_BN_CLICKED(IDC_BUTTONOUTPUT, OnButtonoutput)
	ON_BN_CLICKED(IDC_CHECKDELAY, OnCheckdelay)
	ON_BN_CLICKED(IDC_CHECKCUT, OnCheckcut)
	ON_EN_CHANGE(IDC_EDITDELAYE, OnChangeEditdelaye)
	ON_EN_CHANGE(IDC_EDITCUTE, OnChangeEditcute)
	ON_EN_CHANGE(IDC_EDITCUTS, OnChangeEditcuts)
	ON_EN_CHANGE(IDC_EDITDELAYS, OnChangeEditdelays)
	ON_BN_CLICKED(IDC_BUTTONABORT, OnButtonabort)
	ON_EN_CHANGE(IDC_EDITINPUT, OnChangeEditinput)
	//}}AFX_MSG_MAP
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDelaycutDlg message handlers

BOOL CDelaycutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	CEdit *pCEdit;
	CWnd  *pWnd;
	

	pCEdit = (CEdit*) GetDlgItem(IDC_EDITDELAYS);
	pCEdit->SetLimitText(8);
	pCEdit = (CEdit*) GetDlgItem(IDC_EDITDELAYE);
	pCEdit->SetLimitText(8);
	pCEdit = (CEdit*) GetDlgItem(IDC_EDITCUTS);
	pCEdit->SetLimitText(8);
	pCEdit = (CEdit*) GetDlgItem(IDC_EDITCUTE);
	pCEdit->SetLimitText(8);

	m_csFileInfo=m_csTarget=m_csLog="";

	pWnd = GetDlgItem(IDC_BUTTONABORT);
	pWnd->EnableWindow(FALSE);

	OnCheckdelay();

	c_bShowToolTips = TRUE;
	m_wndToolTip.Create(this);
	m_wndToolTip.Activate(c_bShowToolTips);

	m_wndToolTip.AddTool(GetDlgItem(IDOK), "Use this to generate the file");
	m_wndToolTip.AddTool(GetDlgItem(IDCANCEL), "Use this for...quitting" );
	m_wndToolTip.AddTool(GetDlgItem(IDC_RADIOCRC1), "Ignore CRC errors" );
	m_wndToolTip.AddTool(GetDlgItem(IDC_RADIOCRC2), "Replace bad CRCs with calculated CRC" );
	m_wndToolTip.AddTool(GetDlgItem(IDC_RADIOCRC3), "Replace an errored frame with a silenced one" );
	m_wndToolTip.AddTool(GetDlgItem(IDC_RADIOCRC4), "Skip CRC errored frames" );

	if ( __argc == 2 ) 
//			PostMessage(WM_COMMAND, ID_FILE_OPEN, NULL);
	{
		m_csInputfile.Format(_T("%s"),(__argv[1]));
		GetInputInfo();
	}	

	UpdateData (FALSE);
	CalculateTarget();

	DragAcceptFiles(TRUE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDelaycutDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDelaycutDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDelaycutDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CDelaycutDlg::OnButtoninput() 
{
	// TODO: Add your control notification handler code here
	CString	csFiltro,csTitulo, csAux;
//	FILEINFO *myinfo;

//	myinfo=&theApp.m_myinfo;

	// String del filtro de ficheros y del titulo
	UpdateData (TRUE);

	// szFilter[] = "Chart Files (*.xlc)|*.xlc|Worksheet Files (*.xls)|*.xls|Data Files (*.xlc;*.xls)|*.xlc; *.xls|All Files (*.*)|*.*||";
 

//	csFiltro =  "All supported files (*.mpa;*.mp2;*.mp3;*.ac3;*.dts)|*.mpa;*.mp2;*.mp3;*.ac3;*.dts|";
//	csFiltro += "mpa files (*.mp2)|*.mp2|";
//	csFiltro += "mpa files (*.mp3)|*.mp3|";
	csFiltro =  "All supported files (*.wav;*.mpa;*.ac3;*.dts)|*.wav;*.mpa;*.ac3;*.dts|";
	csFiltro += "wave files (*.wav)|*.wav|";
	csFiltro += "mpeg1 files (*.mpa)|*.mpa|";
	csFiltro += "dolby files (*.ac3)|*.ac3|";
	csFiltro += "dts files (*.dts)|*.dts|";
	csFiltro += "All Files (*.*)|*.*|";
	csFiltro += "|";

	csTitulo = "Select input file";
	// OpenFile Dlg construction 
	CFileDialog dlgAbrir(TRUE,"","",OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,(const char*)csFiltro);

	dlgAbrir.m_ofn.lpstrTitle = (const char*)csTitulo;

	if (dlgAbrir.DoModal() == IDOK)
	{
		m_csInputfile = dlgAbrir.GetPathName(); 
		GetInputInfo();
	}	
	else
	{
		m_csInfo= m_csFileInfo="";
	}
	UpdateData (FALSE);
	CalculateTarget();

}


void CDelaycutDlg::GetInputInfo() 
{
	CString csAux;
	FILEINFO *myinfo;


	myinfo=&theApp.m_myinfo;

	m_csExt= m_csInputfile.Right(m_csInputfile.GetLength() - m_csInputfile.ReverseFind('.')-1);
	m_csExt.MakeLower();
	m_csOutputfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_fixed." + m_csExt;

	m_Fin=fopen(m_csInputfile,"rb");

	if (m_Fin==NULL)
	{
		m_csInfo= m_csFileInfo=m_csOutputfile="";
		UpdateData(FALSE);
		SetScroll();
		return;
	}
	theApp.Initinfo();
	if (m_csExt=="wav")
		getwavinfo ( m_Fin, myinfo);
	if (m_csExt=="ac3")
		getac3info ( m_Fin, myinfo);
	if (m_csExt=="eac3" || m_csExt=="ddp" || m_csExt=="ec3" || m_csExt=="dd+")
		geteac3info ( m_Fin, myinfo);
	if (m_csExt=="dts")
		getdtsinfo ( m_Fin, myinfo);
	if (m_csExt=="mpa" || m_csExt=="mp2" || m_csExt=="mp3" )
		getmpainfo ( m_Fin, myinfo);
	fclose (m_Fin);

	
	m_csFileInfo ="====== INPUT FILE INFO ========================\r\n";
	csAux.Format(_T("File is          \t%s\r\n"),    myinfo->csType);  m_csFileInfo+=csAux;
	if (myinfo->csType != "unknown" && myinfo->csType != "wav")
	{
		csAux.Format(_T("Bitrate  (kbit/s) \t%d\r\n"),    myinfo->iBitrate);  m_csFileInfo+=csAux;
		csAux.Format(_T("Act rate (kbit/s) \t%5.3lf\r\n"), myinfo->dactrate);  m_csFileInfo+=csAux;
		csAux.Format(_T("File size (bytes)\t%I64d\r\n"), myinfo->i64Filesize);  m_csFileInfo+=csAux;
		csAux.Format(_T("Channels mode    \t%s\r\n"),    myinfo->csMode);  m_csFileInfo+=csAux;
		csAux.Format(_T("Sampling Frec    \t%d\r\n"),    myinfo->fsample);  m_csFileInfo+=csAux;
		csAux.Format(_T("Low Frec Effects \t%s\r\n"),    myinfo->csLFE);  m_csFileInfo+=csAux;
		csAux.Format(_T("Duration         \t%s\r\n"),    myinfo->csOriginalDuration);  m_csFileInfo+=csAux;
		csAux.Format(_T("Frame length (ms)\t%8.6lf\r\n"),myinfo->dFrameduration);  m_csFileInfo+=csAux;
		csAux.Format(_T("Frames/second    \t%8.6lf\r\n"),myinfo->dFramesPerSecond);  m_csFileInfo+=csAux;
		csAux.Format(_T("Num of frames    \t%I64d\r\n"), myinfo->i64TotalFrames);  m_csFileInfo+=csAux;
		csAux.Format(_T("Bytes per Frame  \t%9.4lf\r\n"),    myinfo->dBytesperframe);  m_csFileInfo+=csAux;
		csAux.Format(_T("Size %% Framesize \t%I64d\r\n"), myinfo->i64rest);  m_csFileInfo+=csAux;
		if (myinfo->cpf) {csAux.Format(_T("CRC present: \tYES\r\n"));  m_csFileInfo+=csAux;}
		else             {csAux.Format(_T("CRC present: \tNO\r\n"));  m_csFileInfo+=csAux;}
	}
	if (myinfo->csType == "wav")
	{
		csAux.Format(_T("Bitrate  (kbit/s) \t%d\r\n"),    myinfo->iBitrate);  m_csFileInfo+=csAux;
		csAux.Format(_T("Act rate (kbit/s) \t%5.3lf\r\n"), myinfo->dactrate);  m_csFileInfo+=csAux;
		csAux.Format(_T("Byte rate (kbit/s) \t%d\r\n"), myinfo->iByterate);  m_csFileInfo+=csAux;
		csAux.Format(_T("File size (bytes)\t%I64d\r\n"), myinfo->i64Filesize);  m_csFileInfo+=csAux;
		csAux.Format(_T("Channels mode    \t%s\r\n"),    myinfo->csMode);  m_csFileInfo+=csAux;
		csAux.Format(_T("Sampling Frec    \t%d\r\n"),    myinfo->fsample);  m_csFileInfo+=csAux;
		csAux.Format(_T("Bits of Prec.    \t%d\r\n"),    myinfo->iBits);  m_csFileInfo+=csAux;
		csAux.Format(_T("Duration         \t%s\r\n"),    myinfo->csOriginalDuration);  m_csFileInfo+=csAux;
		csAux.Format(_T("Sample length (ms)\t%8.6lf\r\n"),myinfo->dFrameduration);  m_csFileInfo+=csAux;
		csAux.Format(_T("Num of samples   \t%I64d\r\n"),  myinfo->i64TotalFrames);  m_csFileInfo+=csAux;
		csAux.Format(_T("Bytes per Sample  \t%1.0lf\r\n"), myinfo->dBytesperframe);  m_csFileInfo+=csAux;
		csAux.Format(_T("Size %% Samplesize \t%I64d\r\n"), myinfo->i64rest);  m_csFileInfo+=csAux;
	}

	m_csFileInfo+="=============================================\r\n";
	m_csInfo= m_csFileInfo;
	SetScroll();

	m_iCrc=2;

}

void CDelaycutDlg::OnButtonoutput() 
{
	// TODO: Add your control notification handler code here
		CString	csFiltro,csTitulo;

	// String del filtro de ficheros y del titulo
	UpdateData (TRUE);
	if (m_csExt=="ac3")
		csFiltro =  "ac3 files (*.ac3)|*.ac3||";
	if (m_csExt=="eac3" || m_csExt=="ddp" || m_csExt=="ec3" || m_csExt=="dd+")
		csFiltro =  "eac3 files (*.eac3;*.ddp;*.ec3;*.dd+)|*.eac3;*.ddp;*.ec3;*.dd+||";
	if (m_csExt=="dts")
		csFiltro =  "dts files (*.dts)|*.dts||";

	csTitulo = "Select output file";
	// OpenFile Dlg construction 
	CFileDialog dlgAbrir(FALSE,m_csExt,"",OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,(const char*)csFiltro);

	dlgAbrir.m_ofn.lpstrTitle = (const char*)csTitulo;

	if (dlgAbrir.DoModal() == IDOK)
	{
		m_csOutputfile = dlgAbrir.GetPathName(); 
		UpdateData (FALSE);
	}	

	CalculateTarget();
}

void CDelaycutDlg::OnCheckdelay() 
{
	// TODO: Add your control notification handler code here
	CWnd* pWnd, pWnd1;

	UpdateData(TRUE);

	if (m_bCheckdelay)
	{
		m_csDelayend=m_csDelaystart;

		pWnd = GetDlgItem(IDC_EDITDELAYE);
		pWnd->EnableWindow(FALSE);

	}
	else
	{
		pWnd = GetDlgItem(IDC_EDITDELAYE);
		pWnd->EnableWindow(TRUE);
	}
	UpdateData(FALSE);

	CalculateTarget();
	
}

void CDelaycutDlg::OnCheckcut() 
{
	// TODO: Add your control notification handler code here

		CalculateTarget();

}

void CDelaycutDlg::OnChangeEditdelaye() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	CString csAux;

	UpdateData(TRUE);
	csAux=m_csDelayend.SpanIncluding("-0123456789");
	m_csDelayend=csAux;

	if (m_csDelayend.Find('-')!= -1)
	{
		while (m_csDelayend.Find('-')!= m_csDelayend.ReverseFind('-'))
				m_csDelayend.Delete(m_csDelayend.ReverseFind('-'), 1);
		csAux=m_csDelayend.Right(m_csDelayend.GetLength()-m_csDelayend.Find('-'));
		m_csDelayend=csAux;
	}
	if (m_csDelayend.IsEmpty()) m_csDelayend="0";
	UpdateData(FALSE);

	CalculateTarget();

}

void CDelaycutDlg::OnChangeEditcute() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
		CString csAux;
	int	iCutstart,iCutend;

	UpdateData(TRUE);

	if (m_csCutend.IsEmpty()) m_csCutend="0";
	sscanf(m_csCutend,"%ld",&iCutend);
	sscanf(m_csCutstart,"%ld",&iCutstart);

	if (iCutend <iCutstart)
		m_csCutstart= m_csCutend;
	UpdateData(FALSE);
	
	CalculateTarget();

}

void CDelaycutDlg::OnChangeEditcuts() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here

	CString csAux;
	int	iCutstart,iCutend;

	UpdateData(TRUE);

	if (m_csCutstart.IsEmpty()) m_csCutstart="0";
	sscanf(m_csCutend,"%ld",&iCutend);
	sscanf(m_csCutstart,"%ld",&iCutstart);

	if (iCutend <iCutstart)
		m_csCutend= m_csCutstart;
	UpdateData(FALSE);

	CalculateTarget();
	
}

void CDelaycutDlg::OnChangeEditdelays() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here

	CString csAux;

	UpdateData(TRUE);
	csAux=m_csDelaystart.SpanIncluding("-0123456789");
	m_csDelaystart=csAux;

	if (m_csDelaystart.Find('-')!= -1)
	{
		while (m_csDelaystart.Find('-')!= m_csDelaystart.ReverseFind('-'))
				m_csDelaystart.Delete(m_csDelaystart.ReverseFind('-'), 1);
		csAux=m_csDelaystart.Right(m_csDelaystart.GetLength()-m_csDelaystart.Find('-'));
		m_csDelaystart=csAux;
	}
	if (m_csDelaystart.IsEmpty()) m_csDelaystart="0";

	if (m_bCheckdelay)
	{
		m_csDelayend=m_csDelaystart;
	}
	UpdateData(FALSE);

	CalculateTarget();

}


void CDelaycutDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CDelaycutDlg::OnOK() 
{
	// TODO: Add extra validation here

	CWnd* pWnd;
	int iRet;


	UpdateData(TRUE);

	if (m_csInputfile.IsEmpty())
	{
		MessageBox( "Fill input file path!.", "Error",  MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	if (m_csOutputfile.IsEmpty())
	{
		MessageBox( "Fill input file path!.", "Error",  MB_OK | MB_ICONEXCLAMATION );
		return;
	}

	if (m_bCheckcut)
	{
		sscanf (m_csCutend,"%lf",&theApp.m_dCutend);
		sscanf (m_csCutstart,"%lf",&theApp.m_dCutstart);

		// Cut points: translation to secs
		theApp.m_dCutend/=1000.0; 
		theApp.m_dCutstart/=1000.0;

	}
	else
		theApp.m_dCutend=theApp.m_dCutstart=0.0;
	
	sscanf (m_csDelayend,"%lf",&theApp.m_dDelayend);
	sscanf (m_csDelaystart,"%lf",&theApp.m_dDelaystart);
	theApp.m_csInputfile=m_csInputfile;
	theApp.m_csOutputfile=m_csOutputfile;
	theApp.m_csLogfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_log.txt";
	theApp.m_iCrc=m_iCrc;
	theApp.m_csExt=m_csExt;


	pWnd = GetDlgItem(IDOK);
	pWnd->EnableWindow(FALSE);
	pWnd = GetDlgItem(IDCANCEL);
	pWnd->EnableWindow(FALSE);

	pWnd = GetDlgItem(IDC_BUTTONABORT);
	pWnd->EnableWindow(TRUE);

	theApp.m_bAbort=false;
	theApp.m_csLog="";

	theApp.LogInputInfo();
	theApp.LogTargetInfo();
	iRet= theApp.ExeDelaycut(this);

	if (theApp.m_bAbort==true)
		MessageBox( "Aborted!.", "Info",  MB_OK | MB_ICONINFORMATION );
	else if (iRet==0)
		MessageBox( "Finished OK!.", "Info",  MB_OK | MB_ICONINFORMATION );
	else 
		MessageBox( "Finished with error!.", "Info",  MB_OK | MB_ICONINFORMATION );


	m_csProgresstitle="";
	m_CProgressbar.SetPos(0);
	
	pWnd = GetDlgItem(IDOK);
	pWnd->EnableWindow(TRUE);
	pWnd = GetDlgItem(IDCANCEL);
	pWnd->EnableWindow(TRUE);
	pWnd = GetDlgItem(IDC_BUTTONABORT);
	pWnd->EnableWindow(FALSE);
	UpdateData(FALSE);
	SetScroll();

	return;
	CDialog::OnOK();

}

void CDelaycutDlg::OnButtonabort() 
{
	// TODO: Add your control notification handler code here
	theApp.m_bAbort=true;
}

void CDelaycutDlg::UpdateProgress(int nPerc)
{
	static CString csInfo_old="";
	CWnd * pWnd;

	if (nPerc <0 ) nPerc=0;
	if (nPerc >100 ) nPerc=100;
	m_CProgressbar.SetPos(nPerc);
	m_csProgresstitle.Format(_T("Processing... %d %%"),nPerc);
	m_csInfo=m_csFileInfo+m_csTarget+theApp.m_csLog;
	pWnd=GetDlgItem(IDC_PROGRESSTITLE);
	pWnd->SetWindowText(m_csProgresstitle);
	if (csInfo_old!=m_csInfo)
	{
		csInfo_old=m_csInfo;
		UpdateData(FALSE);
		SetScroll();
	}
}

void CDelaycutDlg::CalculateTarget()
{

	FILEINFO *myinfo;
	CString csAux;
	CWnd* pWnd;

	myinfo=&theApp.m_myinfo;

	UpdateData(TRUE);
	if (m_csInputfile.IsEmpty())
		m_csInfo=m_csFileInfo=m_csTarget=theApp.m_csLog=m_csExt="";
	
	if (  m_csExt=="ac3" || m_csExt=="eac3" || m_csExt=="ddp" || m_csExt=="ec3" || m_csExt=="dd+" ||
	    ((m_csExt=="mpa" || m_csExt=="mp2") &&
		 (myinfo->layer==2 && myinfo->cpf!=0)  ) )
	{
		pWnd = GetDlgItem(IDC_RADIOCRC1);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_RADIOCRC2);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_RADIOCRC3);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_RADIOCRC4);
		pWnd->EnableWindow(TRUE);
	}
	else
	{
		pWnd = GetDlgItem(IDC_RADIOCRC1);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_RADIOCRC2);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_RADIOCRC3);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_RADIOCRC4);
		pWnd->EnableWindow(FALSE);
		m_iCrc=0;
	}

	if (m_csFileInfo.IsEmpty())
	{
		m_csTarget="";
		m_csInfo=m_csFileInfo+m_csTarget;
		pWnd = GetDlgItem(IDOK);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_BUTTONOUTPUT);
		pWnd->EnableWindow(FALSE);
		UpdateData(FALSE);
		return;
	}
	else
	{
		pWnd = GetDlgItem(IDOK);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_BUTTONOUTPUT);
		pWnd->EnableWindow(TRUE);
	}



	if (m_bCheckcut)
	{
		sscanf (m_csCutend,"%lf",&theApp.m_dCutend);
		sscanf (m_csCutstart,"%lf",&theApp.m_dCutstart);
		// Cut points: translation to secs
		theApp.m_dCutend/=1000.0; 
		theApp.m_dCutstart/=1000.0;
	}
	else
		theApp.m_dCutend=theApp.m_dCutstart=0.0;
	
	sscanf (m_csDelayend,"%lf",&theApp.m_dDelayend);
	sscanf (m_csDelaystart,"%lf",&theApp.m_dDelaystart);

// Refresh other values
	theApp.m_csInputfile=m_csInputfile;
	theApp.m_csOutputfile=m_csOutputfile;
	theApp.m_iCrc=m_iCrc;
	theApp.m_csExt=m_csExt;

	gettargetinfo ( myinfo ) ;

	m_csTarget ="====== TARGET FILE INFO ======================\r\n";
	if (myinfo->csType!="unknown" && myinfo->csType!="wav")
	{
		csAux.Format(_T("Start Frame   \t%I64d\r\n"), myinfo->i64StartFrame);  m_csTarget+=csAux;
		csAux.Format(_T("End Frame     \t%I64d\r\n"), myinfo->i64EndFrame);  m_csTarget+=csAux;
		csAux.Format(_T("Num of Frames \t%I64d\r\n"), myinfo->i64EndFrame-myinfo->i64StartFrame+1);  m_csTarget+=csAux;
		csAux.Format(_T("Duration      \t%s\r\n"), myinfo->csTimeLengthEst);  m_csTarget+=csAux;
		csAux.Format(_T("NotFixedDelay \t%5.4lf\r\n"), myinfo->dNotFixedDelay);  m_csTarget+=csAux;
	}
	if (myinfo->csType=="wav")
	{
		csAux.Format(_T("Start Sample   \t%I64d\r\n"), myinfo->i64StartFrame);  m_csTarget+=csAux;
		csAux.Format(_T("End Sample     \t%I64d\r\n"), myinfo->i64EndFrame);  m_csTarget+=csAux;
		csAux.Format(_T("Num of Samples \t%I64d\r\n"), myinfo->i64EndFrame-myinfo->i64StartFrame+1);  m_csTarget+=csAux;
		csAux.Format(_T("Duration       \t%s\r\n"), myinfo->csTimeLengthEst);  m_csTarget+=csAux;
		csAux.Format(_T("NotFixedDelay  \t%5.4lf\r\n"), myinfo->dNotFixedDelay);  m_csTarget+=csAux;
	}

	m_csTarget+="=============================================\r\n";
	m_csInfo=m_csFileInfo+m_csTarget;
	UpdateData(FALSE);
	SetScroll();

}

void CDelaycutDlg::OnChangeEditinput() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);	

	GetInputInfo();
	UpdateData(FALSE);	

	CalculateTarget();
	UpdateData(FALSE);


}

void CDelaycutDlg::SetScroll() 
{
	CEdit * pEdit;
	int i,nLines;

	for (i=0,nLines=0; i< m_csInfo.GetLength() ;i++)
		if (m_csInfo[i]=='\n' ) nLines++;


	pEdit=(CEdit *)GetDlgItem(IDC_INFO);

	pEdit->LineScroll(nLines,0);
}

void CDelaycutDlg::OnDropFiles( HDROP hDropInfo)
{
	UINT nFiles;
	CWinApp* pApp;
	TCHAR szFileName[_MAX_PATH];

	SetActiveWindow();      // activate us first !
	POINT ThePoint;

	DragQueryPoint(hDropInfo,&ThePoint);

	nFiles = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);
	pApp = AfxGetApp();
	ASSERT(pApp != NULL);
	if(nFiles> 1)
	{
		MessageBox( "Only one file at a time!.", "Error",  MB_OK | MB_ICONEXCLAMATION );
	}
	else if(nFiles== 1)
	{
		::DragQueryFile(hDropInfo, 0, szFileName, _MAX_PATH);

		m_csInputfile.Format(_T("%s"),szFileName);
		GetInputInfo();
		UpdateData (FALSE);
		CalculateTarget();
	}

	::DragFinish(hDropInfo);
}


