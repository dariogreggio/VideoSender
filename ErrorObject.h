// ErrorObject.h: interface for the CErrorObject class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ERROROBJECT_H__C7BDCC79_AD99_43E2_843A_3944D6B57122__INCLUDED_)
#define AFX_ERROROBJECT_H__C7BDCC79_AD99_43E2_843A_3944D6B57122__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <comdef.h>
class CErrorObject  
{
public:
	void HandleError(_com_error& e,char* szRoutine,char* szModule);
	void HandleError(CException* err, char* szRoutine, char* szModule);
	CErrorObject();
	virtual ~CErrorObject();

};

#endif // !defined(AFX_ERROROBJECT_H__C7BDCC79_AD99_43E2_843A_3944D6B57122__INCLUDED_)
