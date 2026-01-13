// ErrorObject.cpp: implementation of the CErrorObject class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "vidsend.h"
#include "ErrorObject.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
//**************************************************************************
CErrorObject::CErrorObject() {

}
//**************************************************************************
CErrorObject::~CErrorObject() {

}
//**************************************************************************
void CErrorObject::HandleError(CException *err,char* szRoutine, char* szModule ) {
	char msg[512];
	CRuntimeClass* prt = err->GetRuntimeClass();
	
	//  IN CASE OF FILE EXCEPTION
	if( strcmp( prt->m_lpszClassName, "CFileException" )  == 0) {		
		CFileException* e = (CFileException*) err;
		e->ReportError();
		/*sprintf(msg,"Module:  %s\nRoutine:  %s\nFilename:  %s\nCause:  %s",szModule,szRoutine,e->m_strFileName,e->m_cause );

		MessageBox(NULL, msg,"File Handling Error",MB_OK);*/
	}

	
	if( strcmp( prt->m_lpszClassName, "CDBException" )  == 0)	{		
		CDBException* e = (CDBException*) err;
		e->ReportError();
		
	}	

	if( strcmp( prt->m_lpszClassName, "CMemoryException" )  == 0)	{		
		CMemoryException* e = (CMemoryException*) err;
		e->ReportError();
		
	}	

	if( strcmp( prt->m_lpszClassName, "CArchiveException" )  == 0)	{		
		CArchiveException* e = (CArchiveException*) err;
		e->ReportError();
		
	}	

	if( strcmp( prt->m_lpszClassName, "CNotSupportedException" )  == 0)	{		
		CNotSupportedException* e = (CNotSupportedException*) err;
		e->ReportError();
		
		
	}
	
#ifdef _DEBUG
	sprintf(msg,"Module:  %s\nRoutine:  %s",szModule,szRoutine);
	AfxMessageBox(msg);
#endif


}

//**************************************************************************
void CErrorObject::HandleError(_com_error &e, char *szRoutine, char *szModule) {
	char msg[512];
	sprintf(msg,"Module:  %s\nRoutine:  %s\nMessage:  %s\nDescription:  %s",szModule,szRoutine,e.ErrorMessage(),e.Description() );

}
//**************************************************************************

