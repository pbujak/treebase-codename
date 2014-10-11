// TAbstractThreadLocal.cpp: implementation of the TAbstractThreadLocal class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TAbstractThreadLocal.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TAbstractThreadLocal           *TAbstractThreadLocal::S_pFirst      = NULL;
long                            TAbstractThreadLocal::S_nSizeTotal  = 0;
TAbstractThreadLocal::TlsIndex  TAbstractThreadLocal::S_tlsIndex;

//**********************************************************
TAbstractThreadLocal::TlsIndex::~TlsIndex()
{
    if(m_nIndex != -1)
        TlsFree(m_nIndex);
}

//**********************************************************
TAbstractThreadLocal::TlsIndex::operator long()
{
    if(m_nIndex == -1)
        m_nIndex = TlsAlloc();
    
    return m_nIndex;
}

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
    if(pvData == NULL)
    {
        pvData = malloc(S_nSizeTotal);
        if(!pvData)
            return NULL;

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
