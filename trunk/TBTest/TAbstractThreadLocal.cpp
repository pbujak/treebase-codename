// TAbstractThreadLocal.cpp: implementation of the TAbstractThreadLocal class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TAbstractThreadLocal.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TAbstractThreadLocal *TAbstractThreadLocal::S_pFirst      = NULL;
long                  TAbstractThreadLocal::S_nSizeTotal  = 0;
long                  TAbstractThreadLocal::S_tlsIndex    = -1;

//**********************************************************
TAbstractThreadLocal::TAbstractThreadLocal(long a_nSize)
{
    // Force data alignment
    a_nSize = (a_nSize + 3) & (~3);
    //----------------------------------------
    m_nOffset     = S_nSizeTotal;
    S_nSizeTotal += a_nSize;
    m_pNext       = S_pFirst;
    S_pFirst      = this;
    if(S_tlsIndex==-1)
        S_tlsIndex = TlsAlloc();
}

//**********************************************************
TAbstractThreadLocal::~TAbstractThreadLocal()
{
}

//**********************************************************
void TAbstractThreadLocal::ClearAllData()
{
    void *pvData = TlsGetValue(S_tlsIndex);
    if(pvData)
    {
        TAbstractThreadLocal *pItem = S_pFirst;
        while(pItem)
        {
            pItem->ClearData(pvData);
            pItem = pItem->m_pNext;
        }
        free(pvData);
        TlsSetValue(S_tlsIndex, NULL);
    }
}

//**********************************************************
void* TAbstractThreadLocal::GetData()
{
    void *pvData = TlsGetValue(S_tlsIndex);
    if(pvData==NULL)
    {
        pvData = malloc(S_nSizeTotal);
        if(!pvData)
            AfxThrowMemoryException();
        memset(pvData, 0, S_nSizeTotal);
        TlsSetValue(S_tlsIndex, pvData);
        
        TAbstractThreadLocal *pItem = S_pFirst;
        while(pItem)
        {
            pItem->InitData(pvData);
            pItem = pItem->m_pNext;
        }
    }
    return pvData;
}
