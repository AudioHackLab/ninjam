#ifdef _WIN32



#include <windows.h>
#define CURSES_INSTANCE ___ERRROR_____
#include "curses.h"

#include <ctype.h>
#include <conio.h>
#include <stdio.h>

#include "jesusonic.h"

#define WIN32_CONSOLE_KBQUEUE
//#define WIN32_CONSOLE_RUNUI

#ifdef WIN32_CONSOLE_RUNUI
  #ifndef WIN32_CONSOLE_KBQUEUE
    #error WIN32_CONSOLE_RUNUI requires WIN32_CONSOLE_KBQUEUE
  #endif
#endif

static void m_InvalidateArea(jesusonicInstance *inst, int sx, int sy, int ex, int ey)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  RECT r;
  r.left=sx*ctx->m_font_w;
  r.top=sy*ctx->m_font_h;
  r.right=ex*ctx->m_font_w;
  r.bottom=ey*ctx->m_font_h;
  if (ctx->m_hwnd) InvalidateRect(ctx->m_hwnd,&r,FALSE);
}

void __addnstr(jesusonicInstance *inst, char *str,int n)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  int sx=ctx->m_cursor_x;
  int sy=ctx->m_cursor_y;
  if (!ctx->m_framebuffer || ctx->m_cursor_y < 0 || ctx->m_cursor_y >= ctx->lines) return;
  char *p=(char *)ctx->m_framebuffer + 2*(ctx->m_cursor_x + ctx->m_cursor_y*ctx->cols);
  while (n-- && *str)
  {
    p[0]=*str++;
    p[1]=ctx->m_cur_attr;
    p+=2;
	if (++ctx->m_cursor_x >= ctx->cols) 
	{ 
		ctx->m_cursor_y++; 
		ctx->m_cursor_x=0; 
		if (ctx->m_cursor_y >= ctx->lines) { ctx->m_cursor_y=ctx->lines-1; ctx->m_cursor_x=ctx->cols-1; break; }
	}
  }
  m_InvalidateArea(inst,sx,sy,sy < ctx->m_cursor_y ? ctx->cols : ctx->m_cursor_x+1,ctx->m_cursor_y+1);
}

void __clrtoeol(jesusonicInstance *inst)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  int n = ctx->cols - ctx->m_cursor_x;
  if (!ctx->m_framebuffer || ctx->m_cursor_y < 0 || ctx->m_cursor_y >= ctx->lines || n < 1) return;
  char *p=(char *)ctx->m_framebuffer + 2*(ctx->m_cursor_x + ctx->m_cursor_y*ctx->cols);
  int sx=ctx->m_cursor_x;
  while (n--)
  {
    p[0]=0;
    p[1]=ctx->m_cur_erase_attr;
    p+=2;
  }
  m_InvalidateArea(inst,sx,ctx->m_cursor_y,ctx->cols,ctx->m_cursor_y+1);
}

void __curses_erase(jesusonicInstance *inst)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  ctx->m_cur_attr=0;
  ctx->m_cur_erase_attr=0;
  if (ctx->m_framebuffer) memset(ctx->m_framebuffer,0,ctx->cols*ctx->lines*2);
  ctx->m_cursor_x=0;
  ctx->m_cursor_y=0;
  m_InvalidateArea(inst,0,0,ctx->cols,ctx->lines);
}

void __move(jesusonicInstance *inst, int x, int y, int noupdest)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  m_InvalidateArea(inst,ctx->m_cursor_x,ctx->m_cursor_y,ctx->m_cursor_x+1,ctx->m_cursor_y+1);
  ctx->m_cursor_x=y;
  ctx->m_cursor_y=x;
  if (!noupdest) m_InvalidateArea(inst,ctx->m_cursor_x,ctx->m_cursor_y,ctx->m_cursor_x+1,ctx->m_cursor_y+1);
}


void __init_pair(win32CursesCtx *ctx, int pair, int fcolor, int bcolor)
{
  if (pair < 0 || pair >= COLOR_PAIRS) return;

  pair=COLOR_PAIR(pair);

  ctx->colortab[pair][0]=fcolor;
  ctx->colortab[pair][1]=bcolor;

  if (fcolor & 0xff) fcolor|=0xff;
  if (fcolor & 0xff00) fcolor|=0xff00;
  if (fcolor & 0xff0000) fcolor|=0xff0000;
  ctx->colortab[pair|A_BOLD][0]=fcolor;
  ctx->colortab[pair|A_BOLD][1]=bcolor;

}

static int xlateKey(int msg, int wParam)
{
   if (msg == WM_KEYDOWN)
	  {
		  switch (wParam)
		  {
			    case VK_HOME: return KEY_HOME;
			    case VK_UP: return KEY_UP;
			    case VK_PRIOR: return KEY_PPAGE;
			    case VK_LEFT: return KEY_LEFT;
			    case VK_RIGHT: return KEY_RIGHT;
			    case VK_END: return KEY_END;
			    case VK_DOWN: return KEY_DOWN;
			    case VK_NEXT: return KEY_NPAGE;
			    case VK_INSERT: return KEY_IC;
			    case VK_DELETE: return KEY_DC;
			    case VK_F1: return KEY_F1;
			    case VK_F2: return KEY_F2;
			    case VK_F3: return KEY_F3;
			    case VK_F4: return KEY_F4;
			    case VK_F5: return KEY_F5;
			    case VK_F6: return KEY_F6;
			    case VK_F7: return KEY_F7;
			    case VK_F8: return KEY_F8;
			    case VK_F9: return KEY_F9;
			    case VK_F10: return KEY_F10;
			    case VK_F11: return KEY_F11;
			    case VK_F12: return KEY_F12;
          case VK_CONTROL: break;
          case VK_RETURN:
          case VK_BACK: 
          case VK_TAB:
          case VK_ESCAPE:
            return wParam;
          default:
            if(GetAsyncKeyState(VK_CONTROL)&0x8000)
        {
          if (wParam>='a' && wParam<='z') 
          {
            wParam += 1-'a';
            return wParam;
          }
          if (wParam>='A' && wParam<='Z') 
          {
            wParam += 1-'A';
            return wParam;
          }
        }
		  }
	  }
    else if (msg == WM_CHAR)
    {
      if(wParam>=32) return wParam;
    }
    return ERR;
}


static void m_reinit_framebuffer(win32CursesCtx *ctx)
{
    RECT r;

    GetClientRect(ctx->m_hwnd,&r);

    ctx->lines=r.bottom / ctx->m_font_h;
    ctx->cols=r.right / ctx->m_font_w;
    ctx->m_cursor_x=0;
    ctx->m_cursor_y=0;
    ctx->m_framebuffer=(unsigned char *)realloc(ctx->m_framebuffer,2*ctx->lines*ctx->cols);
    if (ctx->m_framebuffer) memset(ctx->m_framebuffer,0,2*ctx->lines*ctx->cols);
}

static LRESULT CALLBACK m_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
  jesusonicInstance *cti=0;
  win32CursesCtx *ctx=0;
  if (uMsg == WM_CREATE)
  {
    CREATESTRUCT *cs=(CREATESTRUCT*)lParam;
    cti = (jesusonicInstance *)cs->lpCreateParams;
    SetWindowLong(hwnd,GWL_USERDATA, (long)cti);
  }
  else cti = (jesusonicInstance *)GetWindowLong(hwnd,GWL_USERDATA);

  if (cti) ctx=&cti->cursesCtx;

  if (ctx)switch (uMsg)
  {
	case WM_DESTROY:
		if (ctx->mOurFont) DeleteObject(ctx->mOurFont);
		if (ctx->mOurFont_ul) DeleteObject(ctx->mOurFont_ul);
		free(ctx->m_framebuffer);
		ctx->m_framebuffer=0;
		ctx->mOurFont=0;
		ctx->mOurFont_ul=0;
		ctx->m_hwnd=0;
	return 0;
#ifdef WIN32_CONSOLE_KBQUEUE
  case WM_CHAR: case WM_KEYDOWN: 

    {
      int a=xlateKey(uMsg,wParam);
      if (a != ERR && ctx->m_kbq)
      {
        ctx->m_kbq->Add(&a,sizeof(int));
      }
    }
  break;
#endif
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			m_reinit_framebuffer(ctx);
			ctx->m_need_redraw=1;
		}
	return 0;
  case WM_LBUTTONDOWN:
    SetFocus(hwnd);
  return 0;
  case WM_GETDLGCODE:
    if (GetParent(hwnd))
    {
      return DLGC_WANTALLKEYS;
    }
  return 0;
  case WM_TIMER:
    if (wParam == 1)
    {
      if (!ctx->m_intimer)
      {
        ctx->m_intimer=1;
        cti->ui_run(1);
        ctx->m_intimer=0;
      }
    }
  return 0;
    case WM_CREATE:
      ctx->m_hwnd=hwnd;
#ifdef WIN32_CONSOLE_RUNUI
      SetTimer(hwnd,1,33,NULL);
#endif
    return 0;
    case WM_ERASEBKGND:
    return 0;
    case WM_PAINT:
      {
        RECT r;
        if (GetUpdateRect(hwnd,&r,FALSE))
        {
          PAINTSTRUCT ps;
          HDC hdc=BeginPaint(hwnd,&ps);
          if (hdc)
          {
            HGDIOBJ oldf=SelectObject(hdc,ctx->mOurFont);
            int y,ypos;
			      int lattr=-1;
            SetTextAlign(hdc,TA_TOP|TA_LEFT);
            char *ptr=(char*)ctx->m_framebuffer;
            RECT updr=r;

			      r.left /= ctx->m_font_w;
			      r.top /= ctx->m_font_h;
			      r.bottom += ctx->m_font_h-1;
			      r.bottom /= ctx->m_font_h;
			      r.right += ctx->m_font_w-1;
			      r.right /= ctx->m_font_w;
    
			      ypos = r.top * ctx->m_font_h;
			      ptr += 2*(r.top * ctx->cols);

			      if (r.top < 0) r.top=0;
			      if (r.bottom > ctx->lines) r.bottom=ctx->lines;
			      if (r.left < 0) r.left=0;
			      if (r.right > ctx->cols) r.right=ctx->cols;


            if (ctx->m_framebuffer) for (y = r.top; y < r.bottom; y ++, ypos+=ctx->m_font_h)
            {
              int x,xpos;
				      xpos = r.left * ctx->m_font_w;

				      char *p = ptr + r.left*2;
				      ptr += ctx->cols*2;

              for (x = r.left; x < r.right; x ++, xpos+=ctx->m_font_w)
              {
                char c=p[0];
                char attr=p[1];

				        if (attr != lattr)
				        {
						      SetTextColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][0]);
						      SetBkColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][1]);
					        lattr=attr;
				        }
						    int rf=0;
						    if (y == ctx->m_cursor_y && x == ctx->m_cursor_x)
						    {
							    rf=1;
							    SelectObject(hdc,ctx->mOurFont_ul);
						    }
				        TextOut(hdc,xpos,ypos,isprint(c) && !isspace(c) ? p : " ",1);
						    if (rf)
						    {
 							    SelectObject(hdc,ctx->mOurFont);
						    }
                p+=2;
              }
            }
            int rm=ctx->cols * ctx->m_font_w;
            int bm=ctx->lines * ctx->m_font_h;
            if (updr.right >= rm)
            {
                SelectObject(hdc,GetStockObject(BLACK_BRUSH));
                SelectObject(hdc,GetStockObject(BLACK_PEN));
                Rectangle(hdc,max(rm,updr.left),max(updr.top,0),updr.right,updr.bottom);
            }
            if (updr.bottom >= bm)
            {
                if (updr.right < rm)
                {
                  SelectObject(hdc,GetStockObject(BLACK_BRUSH));
                  SelectObject(hdc,GetStockObject(BLACK_PEN));
                }
                Rectangle(hdc,max(0,updr.left),max(updr.top,bm),updr.right,updr.bottom);
            }
            SelectObject(hdc,oldf);
            EndPaint(hwnd,&ps);
          }
        }
      }
    return 0;
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

static int m_wndregcnt;


void deinit_win32_window(jesusonicInstance *inst)
{
  __endwin(inst);
}

HWND init_win32_window(jesusonicInstance *inst)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  if (ctx->m_hwnd)
  {
    deinit_win32_window(inst);
  }
  delete ctx->m_kbq;
  ctx->m_kbq=new WDL_Queue;

  free(ctx->m_framebuffer);
  ctx->m_framebuffer=0;

	static WNDCLASS wc={0,};	

	wc.style = 0;
	wc.lpfnWndProc = m_WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	
  if (!wc.hIcon)
  {
    wc.hIcon = LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(101));//if we have a linked in icon, use it
  }
  if (!wc.hIcon) wc.hIcon = (HICON) LoadImage(NULL,"jesusonic.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.lpszClassName = "Jesusonic";

  if (!m_wndregcnt++)
  {
	  if (!RegisterClass(&wc))
    {
      MessageBox(NULL,"Error registering window class!","Jesusonic Error",MB_OK);
    }
  }

	HWND h=CreateWindowEx(0,wc.lpszClassName, "Jesusonic",WS_CAPTION|WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_SIZEBOX|WS_SYSMENU,
					CW_USEDEFAULT,CW_USEDEFAULT,640,480,
					NULL, NULL,wc.hInstance,(void *)inst);

  if (!h)
  {
    MessageBox(NULL,"Error creating window","Jesusonic Error",MB_OK);
  }
  else
  {
      ctx->mOurFont = CreateFont(16,
                            0, // width
                            0, // escapement
                            0, // orientation
                            FW_NORMAL, // normal
                            FALSE, //italic
                            FALSE, //undelrine
                            FALSE, //strikeout
                            ANSI_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY,
                            FF_MODERN,
                            "Courier New");
      ctx->mOurFont_ul = CreateFont(16,
                            0, // width
                            0, // escapement
                            0, // orientation
                            FW_NORMAL, // normal
                            FALSE, //italic
                            TRUE, //undelrine
                            FALSE, //strikeout
                            ANSI_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY,
                            FF_MODERN,
                            "Courier New");

      {
        HDC hdc=GetDC(h);
        HGDIOBJ oldf=SelectObject(hdc,ctx->mOurFont);
        TEXTMETRIC tm;
        GetTextMetrics(hdc,&tm);
        ctx->m_font_h=tm.tmHeight;
        ctx->m_font_w=tm.tmAveCharWidth;
        SelectObject(hdc,oldf);
        ReleaseDC(h,hdc);
      }      
  }

  return h;
}




void __initscr(jesusonicInstance *inst, int flags)
{
  win32CursesCtx *ctx=&inst->cursesCtx;
  __init_pair(ctx,0,RGB(192,192,192),RGB(0,0,0));


  ctx->m_kbq=0; 
	ctx->m_framebuffer=0;

  ctx->cols=80;
  ctx->lines=25;

  if (flags&1) return;

  HWND h=init_win32_window(inst);
  SetWindowPos(h,NULL,0,0,ctx->m_font_w * 80+32, ctx->m_font_h * 30+32,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
  m_reinit_framebuffer(ctx);
  ShowWindow(h,SW_SHOW);
}

void __endwin(jesusonicInstance *inst)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  if (ctx->m_hwnd) DestroyWindow(ctx->m_hwnd);
  ctx->m_hwnd=0;
  free(ctx->m_framebuffer);
  ctx->m_framebuffer=0;
  delete ctx->m_kbq;
  ctx->m_kbq=0;
  if (m_wndregcnt > 0)
  {
    if (!--m_wndregcnt)
    {
      UnregisterClass("Jesusonic",GetModuleHandle(NULL));
    }
  }
}


int curses_getch(jesusonicInstance *inst)
{
  win32CursesCtx *ctx=&inst->cursesCtx;

  if (!ctx->m_hwnd) return ERR;

#ifndef WIN32_CONSOLE_KBQUEUE
  // if we're suppose to run the message pump ourselves (optional!)
  MSG msg;
  while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
  {
    TranslateMessage(&msg);
    int a=xlateKey(msg.message,msg.wParam);
    if (a != ERR) return a;

    DispatchMessage(&msg);

  }
#else
  if (ctx->m_kbq && ctx->m_kbq->Available()>=(int)sizeof(int))
  {
    int a=*(int *) ctx->m_kbq->Get();
    ctx->m_kbq->Advance(sizeof(int));
    ctx->m_kbq->Compact();
    return a;
  }
#endif
  
  if (ctx->m_need_redraw)
  {
    ctx->m_need_redraw=0;
    InvalidateRect(ctx->m_hwnd,NULL,FALSE);
    return 'L'-'A'+1;
  }


  return ERR;
}


#endif

