/*  Copyright (C) 2010-2012  kaosu (qiupf2000@gmail.com)
 *  This file is part of the Interactive Text Hooker.

 *  Interactive Text Hooker is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ITH\IHF_SYS.h>
#include <ITH\ntdll.h>
#include <ITH\string.h>

#include <ITH\TextThread.h>

MK_BASIC_TYPE(BYTE)
MK_BASIC_TYPE(ThreadParameter)

static DWORD MIN_DETECT=0x20;
static DWORD MIN_REDETECT=0x80;
//#define MIN_DETECT		0x20
//#define MIN_REDETECT	0x80
#ifndef CURRENT_SELECT
#define CURRENT_SELECT				0x1000
#endif
#ifndef REPEAT_NUMBER_DECIDED
#define REPEAT_NUMBER_DECIDED	0x2000
#endif

extern SettingManager* setman;
extern HWND hMainWnd;
void CALLBACK NewLineBuff(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	KillTimer(hwnd,idEvent);
	TextThread *id=(TextThread*)idEvent;
			
	if (id->Status()&CURRENT_SELECT)
	{
		//texts->SetLine();
		id->CopyLastToClipboard();
	}
	id->SetNewLineFlag();
}
void CALLBACK NewLineConsole(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	KillTimer(hwnd,idEvent);
	TextThread *id=(TextThread*)idEvent;
	if (id->Status()&USING_UNICODE)
		id->AddText((BYTE*)L"\r\n",4,true,true);
	if (id->Status()&CURRENT_SELECT)
	{
		//texts->SetLine();
	}
}
DWORD GetHookName(LPWSTR str, DWORD pid, DWORD hook_addr,DWORD max);
void ReplaceSentence(BYTE* text, int len)
{
	__asm int 3
}
TextThread::TextThread(DWORD id, DWORD hook, DWORD retn, DWORD spl, WORD num) : thread_number(num)
{
	tp.pid=id;
	tp.hook=hook;
	tp.retn=retn;
	tp.spl=spl;
	head=new RepeatCountNode;
	head->count=head->repeat=0;
	link_number=-1;	
	repeat_detect_limit=0x80;
	filter = 0;
	output = 0;
}
TextThread::~TextThread()
{
	//KillTimer(hMainWnd,timer);
	RepeatCountNode *t=head,*tt;
	while (t)
	{
		tt=t;
		t=tt->next;
		delete tt;
	}
	head=0;
	if (comment) {delete comment;comment=0;}
	if (thread_string) {delete thread_string;thread_string=0;}
}
void TextThread::Reset()
{
	//timer=0;
	last_sentence=0;
	if (comment) {delete comment;comment=0;}
	MyVector::Reset();
}
void TextThread::RemoveSingleRepeatAuto(BYTE* con,int &len)
{
	WORD* text=(WORD*)con;
	if (len<=2)
	{
		if (repeat_single)
		{
			if (repeat_single_count<repeat_single&&
				last==*text) {
					len=0;
					repeat_single_count++;
			}
			else 
			{
				last=*text;
				repeat_single_count=0;
			}
		}
		if (status&REPEAT_NUMBER_DECIDED)
		{
			if (++repeat_detect_count>MIN_REDETECT)
			{
				repeat_detect_count=0;
				status^=REPEAT_NUMBER_DECIDED;
				last=0;
				RepeatCountNode *t=head,*tt;
				while (t)
				{
					tt=t;
					t=tt->next;
					delete tt;
				}
				head=new RepeatCountNode;
			}
		}
		else
		{
			repeat_detect_count++;
			if (last==*text) repeat_single_current++;
			else 
			{
				if (last==0) 
				{
					last=*text;
					return;
				}
				if (repeat_single_current==0)
				{
					status|=REPEAT_NUMBER_DECIDED;
					repeat_single=0;
					return;
				}
				last=*text;
				RepeatCountNode* it=head;
				if (repeat_detect_count>MIN_DETECT)
				{
					while (it=it->next)
					{
						if (it->count>head->count)
						{
							head->count=it->count;
							head->repeat=it->repeat;
						}
					}
					repeat_single=head->repeat;
					repeat_single_current=0;
					repeat_detect_count=0;
					status|=REPEAT_NUMBER_DECIDED;
					DWORD repeat_sc=repeat_single*4;
					if (repeat_sc>MIN_DETECT)
					{
						MIN_DETECT<<=1;
						MIN_REDETECT<<=1;
					}
				}
				else
				{
					bool flag=true;
					while (it)
					{
						if (it->repeat==repeat_single_current)
						{
							it->count++;
							flag=false;
							break;
						}
						it=it->next;
					}
					if (flag)
					{
						RepeatCountNode *n=new RepeatCountNode;
						n->count=1;
						n->repeat=repeat_single_current;
						n->next=head->next;
						head->next=n;
					}
					repeat_single_current=0;
				} //Decide repeat_single
			} //Check Repeat
		} //repeat_single decided?
	} //len
	else
	{
		status|=REPEAT_NUMBER_DECIDED;
		repeat_single=0;
	}
}
void TextThread::RemoveSingleRepeatForce(BYTE* con,int &len)
{
	WORD* text=(WORD*)con;
	if (repeat_single_count<setman->GetValue(SETTING_REPEAT_COUNT)&&last==*text) 
	{	
		len=0;		
		repeat_single_count++;
	}
	else 
	{
		last=*text;
		repeat_single_count=0;
	}
}
void TextThread::RemoveCyclicRepeat(BYTE* &con, int &len)
{
	DWORD currnet_time=GetTickCount();
	if (status&REPEAT_SUPPRESS)
	{
		if (currnet_time-last_time<setman->GetValue(SETTING_SPLIT_TIME)&&
			memcmp(storage+last_sentence+repeat_index,con,len)==0)
		{
			repeat_index+=len;
			if (repeat_index>=sentence_length) repeat_index-=sentence_length;
			len=0;
		}
		else
		{
			repeat_index=0;
			status&=~REPEAT_SUPPRESS;
		}
	}
	else if (status&REPEAT_DETECT)
	{
		if (memcmp(storage+last_sentence+repeat_index,con,len)==0)
		{
			int half_length=repeat_index+len;
			if (memcmp(storage+last_sentence,storage+last_sentence+half_length,repeat_index)==0)
			{
				len=0;
				sentence_length=half_length;
				status&=~REPEAT_DETECT;
				status|=REPEAT_SUPPRESS;
				
				if (status&CURRENT_SELECT)
				{
					ReplaceSentence(storage+last_sentence+half_length,repeat_index);
				}
				ClearMemory(last_sentence+half_length,repeat_index);
				used-=repeat_index;
				repeat_index=0;
			}
			else repeat_index+=len;
		}		
		else
		{
			repeat_index=0;
			status&=~REPEAT_DETECT;
		}
	}
	else
	{
		if (sentence_length==0) return;
		else if (len<=(int)sentence_length)
		{
			if (memcmp(storage+last_sentence,con,len)==0)
			{
				status|=REPEAT_DETECT;
				repeat_index=len;
				if (repeat_index==sentence_length)
				{
					repeat_index=0;
					len=0;
				}
			}
			else if (sentence_length>repeat_detect_limit)
			{
				if (len>2)
				{
					DWORD u=used;
					while (memcmp(storage+u-len,con,len)==0) u-=len;
					ClearMemory(u,used-u);
					used=u;
					repeat_index=0;
					if (status&CURRENT_SELECT)
					{
						ReplaceSentence(storage+last_sentence,used-u);
					}
					status|=REPEAT_SUPPRESS;
					len=0;
				}
				else if (len<=2)
				{
					WORD tmp=*(WORD*)(storage+last_sentence);
					DWORD index,last_index,tmp_len;
					index=used-len;
					if (index<last_sentence) index=last_sentence;
					//Locate position of current input.
_again:
					*(WORD*)(storage+last_sentence)=*(WORD*)con;
					while (*(WORD*)(storage+index)!=*(WORD*)con) index--;
					*(WORD*)(storage+last_sentence)=tmp;
					if (index>last_sentence)
					{
						tmp_len=used-index;
						if (tmp_len<=2)
						{
							repeat_detect_limit+=0x40;
							last_time=currnet_time;
							return;
						}
						if  (index-last_sentence>=tmp_len&&
							memcmp(storage+index-tmp_len,storage+index,tmp_len)==0)
						{
							repeat_detect_limit=0x80;
							sentence_length=tmp_len;
							index-=tmp_len;
							while (memcmp(storage+index-sentence_length,storage+index,sentence_length)==0)
								index-=sentence_length;
							repeat_index=2;
							len=0;
							last_index=index;
							if (status&USING_UNICODE)
							{							
								while (storage[index]==storage[index+sentence_length]) index-=2;
								index+=2;
								while (1)
								{
									tmp=*(WORD*)(storage+index);
									if (tmp>=0x3000&&tmp<0x3020) index+=2;
									else break;
								}								
							}
							else
							{
								DWORD last_char_len;
								while (storage[index]==storage[index+sentence_length]) 
								{
									last_char_len=LeadByteTable[storage[index]];
									index-=last_char_len;
								}
								index+=last_char_len;
								while (storage[index]==0x81)
								{
									if ((storage[index+1]>>4)==4) index+=2;
									else break;
								}
							}
							repeat_index+=last_index-index;
							status|=REPEAT_SUPPRESS;
							last_sentence=index;
							
							index+=sentence_length;
							if (status&CURRENT_SELECT) 
							{
								ReplaceSentence(storage+index,used-index);
							}
							
							ClearMemory(index,used-index);
							//memset(storage+index,0,used-index);
							used=index;
						}
						else 
						{
							index--;
							goto _again;
						}
					}
					else repeat_detect_limit+=0x40;
				}
			}
		}
	}
	last_time=currnet_time;
}
void TextThread::ResetRepeatStatus()
{
	last=0;
	repeat_single=0;
	repeat_single_current=0;
	repeat_single_count=0;
	repeat_detect_count=0;
	RepeatCountNode *t=head->next,*tt;
	while (t)
	{
		tt=t;
		t=tt->next;
		delete tt;
	}
	//head=new RepeatCountNode;
	head->count=head->repeat=0;
	status&=~REPEAT_NUMBER_DECIDED;
}
void TextThread::AddLineBreak()
{
	if (sentence_length == 0) return;
	if (status&BUFF_NEWLINE)
	{
		prev_sentence=last_sentence;
		sentence_length=0;
		if (status&USING_UNICODE) AddToStore((BYTE*)L"\r\n\r\n",8);
		else AddToStore((BYTE*)"\r\n\r\n",4);
		if (output) output(this,0,8,TRUE,app_data);
		last_sentence=used;
		status&=~BUFF_NEWLINE;
	}
}
void TextThread::AddText(BYTE* con,int len, bool new_line,bool console)
{
	if (con == 0 || len <=0) return;
	if (!console)
	{
		if (len<=0) return;
		if (!new_line) 
		{
			if (setman->GetValue(SETTING_REPEAT_COUNT)) 
			{
				status|=REPEAT_NUMBER_DECIDED;
				RemoveSingleRepeatForce(con,len);
			}
			else RemoveSingleRepeatAuto(con,len);
		}
		if (len<=0) return;
		if(setman->GetValue(SETTING_CYCLIC_REMOVE)) 
		{
			//if (status&REPEAT_NUMBER_DECIDED)
				RemoveCyclicRepeat(con,len);
		}
	}
	if (len<=0) return;

	if (filter) len = filter(this,con,len, new_line,app_data);
	if (len <= 0) return;

	if (sentence_length == 0)
	{
		if (status & USING_UNICODE)
		{
			if (*(WORD*)con == 0x3000)
			{
				con += 2;
				len -= 2;
			}
		}
		else
		{
			if (*(WORD*)con == 0x4081)
			{
				con += 2;
				len -= 2;
			}
		}
	}
	if (len <= 0) return;

	if (status&BUFF_NEWLINE) AddLineBreak();

	if (new_line)
	{
		prev_sentence=last_sentence;
		last_sentence=used+4;
		if (status&USING_UNICODE) last_sentence+=4;
		sentence_length=0;
	}
	else
	{
		SetNewLineTimer();
		if (link)
		{
			BYTE* send=con;
			int l=len;
			if (status&USING_UNICODE) //Although unlikely, a thread and its link may have different encoding.
			{
				if ((link->Status()&USING_UNICODE)==0)
				{
					send=new BYTE[l];
					l=WC_MB((LPWSTR)con,(char*)send);
				}
				link->AddTextDirect(send,l);
			}
			else
			{
				if (link->Status()&USING_UNICODE)
				{
					send=new BYTE[len*2+2];
					l=MB_WC((char*)con,(LPWSTR)send)<<1;
				}
				link->AddTextDirect(send,l);
			}
			link->SetNewLineTimer();
			if (send!=con) delete send;
		}
		sentence_length+=len;
	}
	
	if (output) len = output(this,con,len, new_line,app_data);
	if (AddToStore(con,len))
	{
		//sentence_length += len;
		/*ResetRepeatStatus();
		last_sentence=0;
		prev_sentence=0;
		sentence_length=len;
		repeat_index=0;
		status&=~REPEAT_DETECT|REPEAT_SUPPRESS;		*/
	}

}
void TextThread::AddTextDirect(BYTE* con, int len) //Add to store directly, penetrating repetition filters.
{
	if (status&BUFF_NEWLINE) AddLineBreak();
	//SetNewLineTimer();
	if (link)
	{
		BYTE* send=con;
		int l=len;
		if (status&USING_UNICODE)
		{
			if ((link->Status()&USING_UNICODE)==0)
			{
				send=new BYTE[l];
				l=WC_MB((LPWSTR)con,(char*)send);
			}
			link->AddText(send,l);
		}
		else
		{
			if (link->Status()&USING_UNICODE)
			{
				send=new BYTE[len*2+2];
				l=MB_WC((char*)con,(LPWSTR)send)<<1;
			}
			link->AddText(send,l);
		}
		link->SetNewLineTimer();
		if (send!=con) delete send;
	}
	sentence_length+=len;
	if (output) len = output(this,con,len,false,app_data);
	AddToStore(con,len);
}
DWORD TextThread::GetEntryString(LPWSTR str, DWORD max)
{
	DWORD len = 0;
	if (str && max > 0x40)
	{
		max--;
		if (thread_string)
		{
			len = wcslen(thread_string);
			len = len < max ? len : max;
			memcpy(str, thread_string, len << 1);
			str[len] = 0;
		}
		else
		{
			len = swprintf(str,L"%.4X:%.4d:0x%08X:0x%08X:0x%08X:",
				thread_number,tp.pid,tp.hook,tp.retn,tp.spl); 

			len += GetHookName(str + len, tp.pid, tp.hook, max - len);
			thread_string=new WCHAR[len + 1];
			memcpy(thread_string, str, len << 1);
			thread_string[len] = 0;
		}
		if (comment)
		{
			str += len;
			max--;
			DWORD cl = wcslen(comment);
			if (len + cl >= max) cl = max - len;
			*str++=L'-';
			memcpy(str, comment, cl << 1);
			str[cl] = 0;
			len += cl;
		}
	}
	return len;
}
void TextThread::CopyLastSentence(LPWSTR str)
{
	int i,j,l;
	if (status&USING_UNICODE)
	{
		if (used>8)
		{
			j=used>0xF0?(used-0xF0):0;
			for (i=used-0xA;i>=j;i-=2)
			{
				if (*(DWORD*)(storage+i)==0xA000D) break;
			}
			if (i>=j)
			{
				l=used-i;
				if (i>j) l-=4;			
				j=4;
			}
			else
			{
				i+=2;
				l=used-i;
				j=0;
			}
			memcpy(str,storage+i+j,l);
			str[l>>1]=0;
		}
		else 
		{
			memcpy(str,storage,used);
			str[used>>1]=0;
		}
	}
	else
	{
		if (used>4)
		{
			j=used>0x80?(used-0x80):0;
			for (i=used-5;i>=j;i--)
			{
				if (*(DWORD*)(storage+i)==0xA0D0A0D) break;
			}
			if (i>=j)
			{
				l=used-i;
				if (i>j) l-=4;
				j=4;
			}
			else
			{
				i++;
				l=used-i;
				j=0;
			}
			char* buff=new char[(l|0xF)+1];
			memcpy(buff,storage+i+j,l);
			buff[l]=0;		
			str[MB_WC(buff,str)]=0;
			delete buff;
		}
		else 
		{
			storage[used]=0;
			str[MB_WC((char*)storage,str)]=0;
		}
	}
}

static char clipboard_buffer[0x400];
void CopyToClipboard(void* str,bool unicode, int len)
{
	if (setman->GetValue(SETTING_CLIPFLAG))
	if (str && len > 0)
	{
		int size=(len*2|0xF)+1;
		if (len>=0x3FE) return;
		memcpy(clipboard_buffer,str,len);
		*(WORD*)(clipboard_buffer+len)=0;
		HGLOBAL hCopy;
		LPWSTR copy;
		if (OpenClipboard(0))
		{
			if (hCopy=GlobalAlloc(GMEM_MOVEABLE,size))
			{
				if (copy=(LPWSTR)GlobalLock(hCopy))
				{
					if (unicode)
					{
						memcpy(copy,clipboard_buffer,len+2);
					}
					else
						copy[MB_WC(clipboard_buffer,copy)]=0;					
					GlobalUnlock(hCopy);
					EmptyClipboard();
					SetClipboardData(CF_UNICODETEXT,hCopy);
				}
			}
			CloseClipboard();
		}
	}
}
void TextThread::CopyLastToClipboard()
{
	CopyToClipboard(storage+last_sentence,(status&USING_UNICODE)>0,used-last_sentence);
}

void TextThread::ResetEditText()
{
	//__asm int 3;
	WCHAR str[0x20];
	swprintf(str,L"%.8X",_ReturnAddress());
}
void TextThread::ExportTextToFile(LPWSTR filename)
{
	HANDLE hFile=IthCreateFile(filename,FILE_WRITE_DATA,0,FILE_OPEN_IF);
	if (hFile==INVALID_HANDLE_VALUE) return;
	EnterCriticalSection(&cs_store);
	IO_STATUS_BLOCK ios;
	LPVOID buffer=storage;
	DWORD len=used;
	BYTE bom[4]={0xFF,0xFE,0,0};
	LARGE_INTEGER offset={2,0};
	if ((status&USING_UNICODE)==0)
	{
		len=MB_WC_count((char*)storage,used);
		buffer=new WCHAR[len+1];
		MB_WC((char*)storage,(wchar_t*)buffer);
		len<<=1;
	}
	NtWriteFile(hFile,0,0,0,&ios,bom,2,0,0);
	NtWriteFile(hFile,0,0,0,&ios,buffer,len,&offset,0);
	NtFlushBuffersFile(hFile,&ios);
	if (buffer!=storage) delete buffer;
	NtClose(hFile);
	LeaveCriticalSection(&cs_store);
}
void TextThread::SetComment(LPWSTR str)
{
	if (comment) delete comment;
	comment=new WCHAR[wcslen(str)+1];
	wcscpy(comment,str);
}
void TextThread::SetNewLineFlag()
{
	status|=BUFF_NEWLINE;
}
bool TextThread::CheckCycle(TextThread* start)
{
	if (link==start||this==start) return true;
	if (link==0) return false;
	return link->CheckCycle(start);
}
void TextThread::SetNewLineTimer()
{
	if (thread_number==0)
		timer=SetTimer(hMainWnd,(UINT_PTR)this,setman->GetValue(SETTING_SPLIT_TIME),NewLineConsole);
	else
		timer=SetTimer(hMainWnd,(UINT_PTR)this,setman->GetValue(SETTING_SPLIT_TIME),NewLineBuff);
}
DWORD TextThread::GetThreadString( LPWSTR str, DWORD max )
{
	WCHAR buffer[0x200],c;
	DWORD len = 0;
	if (max)
	{
		if (thread_string == 0) GetEntryString(buffer, 0x200); //This will allocate thread_string.
		LPWSTR p1,end;
		for (end = thread_string; *end; end++);
		c = thread_string[0];
		thread_string[0] = L':';
		for (p1 = end; *p1 != L':'; p1--);
		thread_string[0] = c;
		if (p1 == thread_string) return 0;
		p1++;
		len = end - p1;
		if (len >= max) len = max;
		memcpy(str, p1, len << 1);
		str[len] = 0;
	}

	return len;
}
void TextThread::UnLinkAll()
{
	if (link) link->UnLinkAll();
	link = 0;
	link_number = -1;
}