//==========================================================================
// File : ToolTipDialog.h
//
// Definition of class CToolTipDialog
//
// Author : H. Devos
//
// Created : 14/10/98
//
//                                         (C) n.v. dZine
//==========================================================================

#ifndef _TOOLTIPDIALOG_H_IS_INCLUDED
#define _TOOLTIPDIALOG_H_IS_INCLUDED

#ifndef __AFXWIN_H__
#include <AfxWin.h>
#endif

#ifndef __AFXCMN_H__
#include <AfxCmn.h>
#endif

/***************************************************************************
* Class CToolTipDialog
*
* Derived from : CDialog
*
* Description : A generic dialog that displays tooltips
*				Derive your class from CToolTipDialog instead of CDialog
*
*/
class CToolTipDialog : public CDialog
{
	DECLARE_DYNAMIC(CToolTipDialog)
	DECLARE_MESSAGE_MAP()
public:
	CToolTipDialog();
	CToolTipDialog(LPCTSTR lpszTemplateName, CWnd* pParentWnd = NULL);
	CToolTipDialog(UINT nIDTemplate, CWnd* pParentWnd = NULL);
	virtual ~CToolTipDialog();
	static void EnableToolTips(BOOL bEnable);
protected:
	afx_msg void OnDestroy();
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	CToolTipCtrl m_wndToolTip;
	static BOOL c_bShowToolTips;
private:
	void ListBoxTextExpand(MSG* pMsg);
};

#else
#error file ToolTipDialog.h included more than once.
#endif