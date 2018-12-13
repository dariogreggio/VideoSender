#ifndef __ARRAYSORT_H
#define __ARRAYSORT_H

#include <stdlib.h>

namespace arraysort
{
	template <class ARRAY,class ELEM>
	class c_arraysort
	{
	public:
		//	Generic
		typedef int (_cdecl *GENERICCOMPAREFUNC)(const void *,const void*);
		typedef int (_cdecl *TCOMPAREFUNC)(const ELEM*,const ELEM*);

		c_arraysort(
			ARRAY& array,unsigned count,
			TCOMPAREFUNC CompareFunc
		)
		{
			sort_array(array,count,CompareFunc);
		}

		c_arraysort(
			ARRAY& array,
			TCOMPAREFUNC CompareFunc
		)
		{
			sort_array(array,array.GetSize(),CompareFunc);
		}

	protected:

		//	Main order function
		inline sort_array(ARRAY& array,unsigned count,TCOMPAREFUNC CompareFunc)
		{
			qsort(array.GetData(),count,sizeof(ELEM),(GENERICCOMPAREFUNC)CompareFunc);
		}
	};

	//	Ascending order (ELEM must implement operator <)
	template <class ELEM>
	static inline int _cdecl sort_asc(const ELEM* elem1,const ELEM* elem2)
	{	return *elem1 < *elem2 ? -1 : 1;	}

	template <class ELEM>
	static inline int _cdecl sort_ascNocase(const ELEM* elem1,const ELEM* elem2)
	{	return elem1->CompareNoCase(*elem2);	}

	//	Descending order (ELEM must implement operator >)
	template <class ELEM>
	static inline int _cdecl sort_desc(const ELEM* elem1,const ELEM* elem2)
	{	return *elem1 > *elem2 ? -1 : 1;	}

	template <class ELEM>
	static inline int _cdecl sort_descNocase(const ELEM* elem1,const ELEM* elem2)
	{	return -elem1->CompareNoCase(*elem2);	}
}

using namespace arraysort;

#endif	//	 __ARRAYSORT_H