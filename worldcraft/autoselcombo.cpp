//====== Copyright � 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: Implements a combo box that autoselects items from the list as the
//			user types in the edit control. It has the additional feature of
//			being able to easily set the text color.
//
//=============================================================================

#include "stdafx.h"
#include "AutoSelCombo.h"


BEGIN_MESSAGE_MAP(CAutoSelComboBox, CComboBox)
	//{{AFX_MSG_MAP(CAutoSelComboBox)
	ON_WM_CTLCOLOR()
	ON_CONTROL_REFLECT_EX(CBN_EDITUPDATE, OnEditUpdate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CAutoSelComboBox::CAutoSelComboBox(void)
{
	m_szLastText[0] = '\0';
	m_dwTextColor = RGB(0, 0, 0);
	m_bNotifyParent = true;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches this object to the given dialog item.
//-----------------------------------------------------------------------------
void CAutoSelComboBox::SubclassDlgItem(UINT nID, CWnd *pParent)
{
	//
	// Disable parent notifications for CControlBar-derived classes. This is
	// necessary because these classes result in multiple message reflections
	// unless we return TRUE from our message handler.
	//
	if (pParent->IsKindOf(RUNTIME_CLASS(CControlBar)))
	{
		m_bNotifyParent = false;
	}
	else
	{
		m_bNotifyParent = true;
	}

	BaseClass::SubclassDlgItem(nID, pParent);
}


//-----------------------------------------------------------------------------
// Purpose: Automatically selects the first matching combo box item when the
//			edit control's text changes.
//-----------------------------------------------------------------------------
void CAutoSelComboBox::OnUpdateText(void)
{
	char szTypedText[MAX_PATH];

	GetWindowText(szTypedText, sizeof(szTypedText));

	//
	// Select the first match in the list if what they typed is different from
	// the current selection. This won't do anything if they are deleting characters
	// from the end of the text.
	//
	if (_strnicmp(szTypedText, m_szLastText, strlen(szTypedText)))
	{
		int curSel = GetCurSel();
		DWORD dwEditSel = GetEditSel();

		int nIndex = FindString(-1, szTypedText);

		if (curSel != nIndex)
		{
			SetCurSel(nIndex);
		}

		if (nIndex == CB_ERR)
		{
			SetWindowText(szTypedText);
			SetEditSel(HIWORD(dwEditSel), LOWORD(dwEditSel));
		}
		else
		{
			SetEditSel(strlen(szTypedText), -1);
		}

		if (curSel != nIndex)
		{
			GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), CBN_SELCHANGE), (LPARAM)m_hWnd);
		}
	}

	strcpy(m_szLastText, szTypedText);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CAutoSelComboBox::OnEditUpdate(void)
{
	OnUpdateText();

	//
	// Despite MSDN's lies, returning FALSE here allows the parent
	// window to hook the notification message as well, not TRUE.
	//
	return m_bNotifyParent ? FALSE : TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Resets the 'last typed text' buffer every time we gain/lose focus.
//-----------------------------------------------------------------------------
void CAutoSelComboBox::OnSetFocus(CWnd *pOldWnd)
{
	m_szLastText[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwColor - 
//-----------------------------------------------------------------------------
void CAutoSelComboBox::SetTextColor(COLORREF dwColor)
{
	m_dwTextColor = dwColor;
}


//-----------------------------------------------------------------------------
// Purpose: Called before painting to override default colors.
// Input  : pDC - DEvice context being painted into.
//			pWnd - Control asking for color.
//			nCtlColor - Type of control asking for color.
// Output : Returns the handle of a brush to use as the background color.
//-----------------------------------------------------------------------------
HBRUSH CAutoSelComboBox::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hBrush = CComboBox::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_EDIT)
	{
		pDC->SetTextColor(m_dwTextColor);
	}

	return(hBrush);
}
