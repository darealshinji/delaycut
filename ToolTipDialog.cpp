//==========================================================================
// File : ToolTipDialog.cpp
//
// Implementation of class CToolTipDialog
//
// Author : H. Devos
//
// Created : 14/10/98
//
//                                         (C) n.v. dZine
//==========================================================================

#include <stdafx.h>
//include <AfxWin.h>
//include <AfxCmn.h>
#include "ToolTipDialog.h"

IMPLEMENT_DYNAMIC(CToolTipDialog, CDialog)

BEGIN_MESSAGE_MAP(CToolTipDialog, CDialog)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

//--------------------------------------------------------------------------
// Static variables
//--------------------------------------------------------------------------
BOOL CToolTipDialog::c_bShowToolTips = TRUE;

//--------------------------------------------------------------------------
// Constructors and Destructors
//--------------------------------------------------------------------------

/***************************************************************************
* CToolTipDialog::CToolTipDialog()
*
* Constructor
*
*/
CToolTipDialog::CToolTipDialog()
{
}

CToolTipDialog::CToolTipDialog(LPCTSTR lpszTemplateName, CWnd* pParentWnd) :
	CDialog(lpszTemplateName, pParentWnd)
{
}

CToolTipDialog::CToolTipDialog(UINT nIDTemplate, CWnd* pParentWnd) :
	CDialog(nIDTemplate, pParentWnd)
{
}

/***************************************************************************
* CToolTipDialog::~CToolTipDialog()
*
* Destructor
*
*/
CToolTipDialog::~CToolTipDialog()
{
}



//--------------------------------------------------------------------------
// Operations
//--------------------------------------------------------------------------


/***************************************************************************
* CToolTipDialog::OnInitDialog()
*
* Description : Performs initialization:
*				Reads the resource strings to use as tooltips and
*				creates the tooltip control
*
* Return value : see CDialog::OnInitDialog()
*
*/

BOOL CToolTipDialog::OnInitDialog()
{
	BOOL bResult = CDialog::OnInitDialog();
	m_wndToolTip.Create(this);
	m_wndToolTip.Activate(c_bShowToolTips);
	CWnd *pWndChild = GetWindow(GW_CHILD);
	CString strToolTip;
	while (pWndChild)
	{
		int nID = pWndChild->GetDlgCtrlID();
		if (strToolTip.LoadString(nID))
		{
			m_wndToolTip.AddTool(pWndChild, strToolTip);
		}
		pWndChild = pWndChild->GetWindow(GW_HWNDNEXT);
	}
	return bResult;
}


/***************************************************************************
* CToolTipDialog::OnDestroy()
*
* Description : Cleanup: destroys the tooltip control
*
*/

void CToolTipDialog::OnDestroy()
{
	m_wndToolTip.DestroyWindow();
}


/***************************************************************************
* CToolTipDialog::PreTranslateMessage()
*
* Description : Override of CWnd::PreTranslateMessage
*				Passes mose messages to the tooltip control
*				Call this version of the function from your override
*
* Return value : see CWnd::PreTranslateMessage
*
* Parameters :
*   - pMsg : The current message as received in ProcessMessageFilter
*
*/

BOOL CToolTipDialog::PreTranslateMessage(MSG *pMsg)
{
	// As pointed out by Richard Collins, the original code did not work
	// for controls that have child windows, like combo boxes.
	// My original idea to solve the problem was adding every
	// child of the control using m_wndToolTip.AddTool(), but this
	// didn't work as I expected. This piece of code however does
	// the job.
	// It is quite simple: If the window isn't a direct child, take
	// its parent and continue taking the parent until you reach the dialog.
	// This is a piece of code that gets called over and over again, so
	// it has to be optimized a little bit more than some other code.
	// This is why I use window handles instead of CWnd pointers.
	// Using CWnd pointers would cause the window map being searched
	// all the time and creating temporary CWnd objects.
	if (c_bShowToolTips &&
		pMsg->message >= WM_MOUSEFIRST &&
		pMsg->message <= WM_MOUSELAST)
	{
		MSG msg;
		::CopyMemory(&msg, pMsg, sizeof(MSG));
		HWND hWndParent = ::GetParent(msg.hwnd);
		while (hWndParent && hWndParent != m_hWnd)
		{
			msg.hwnd = hWndParent;
			hWndParent = ::GetParent(hWndParent);
		}
		if (msg.hwnd)
		{
			// Code added by Murali Soundararajan
			// Let's See if we need to expand the list
			ListBoxTextExpand(&msg); // use original 

			m_wndToolTip.RelayEvent(&msg);
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}


/***************************************************************************
* CToolTipDialog::EnableToolTips()
*
* Description : Enables/disables tooltips in this application
*
* Parameters :
*   - bEnable : TRUE : enable
*				FALSE : disable
*
*/
void CToolTipDialog::EnableToolTips(BOOL bEnable)
{
	c_bShowToolTips = bEnable;
}



/***************************************************************************
* CToolTipDialog::ListBoxTextExpand()
*
* Description : Added by Murali Soundararajan.
*				This function does the magic for displaying the line
*				beneath the mouse pointer from a listbox in the tooltip.
*
* Parameters :
*   - pMsg : A MSG pointer like passed to PreTranslateMessage.
*
*/
void CToolTipDialog::ListBoxTextExpand(MSG* pMsg)
{
	CString strClassName;
	const int nMaxCount = 20;
	LPTSTR lpszClassName = strClassName.GetBuffer(nMaxCount);
	GetClassName(pMsg->hwnd,lpszClassName,nMaxCount);
	strClassName.ReleaseBuffer();
	// This code could be made univesal...It dosn't have to be limited to ListBox controls...
	if (strClassName==_T("ListBox")) // Not made upper just in case some very eccentric programmer uses LISTBOX as his class name...
	{
		CListBox* plstBox = (CListBox*)CListBox::FromHandle(pMsg->hwnd);
		BOOL bEnableToolTipExpand = FALSE;

		// ToolTip text will only be "expanded" if StringID for this resource is set to "ToolTipExpand"
		UINT nID = plstBox->GetDlgCtrlID();
		if (nID)
		{
			CString strID;
			if (strID.LoadString(nID)) // loads from resource chain...
			{
				strID.MakeUpper();
				bEnableToolTipExpand = strID == _T("TOOLTIPEXPAND");
			}
		}

		if (bEnableToolTipExpand)
		{
			BOOL bOutside;
			POINT pt(pMsg->pt);
			plstBox->ScreenToClient(&pt); // ItemFromPoint needs coords converted with 0,0 as upper left most of listbox
			int nItem = plstBox->ItemFromPoint(pt, bOutside);
			CString strExpandText;
			if (!bOutside && nItem >= 0)
				plstBox->GetText( nItem, strExpandText ) ;
			m_wndToolTip.UpdateTipText(strExpandText, CWnd::FromHandle(pMsg->hwnd));
			if (bOutside)
				m_wndToolTip.Invalidate();
		}
	}
}

