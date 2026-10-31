#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <mmsystem.h>
#include <string>
#include <cmath>
#include <cstdio>
#include <commdlg.h>
#ifdef ISLAND_SELF_TEST
#include <cassert>
#endif
#include "timer.h"
#include "storage.h"

constexpr wchar_t CLASS_NAME[]=L"QuietIsland.Native.1";
constexpr UINT TRAY=WM_APP+1, VISIBLE_TICK=1, DEADLINE_TICK=2;
constexpr int VOL=101, MINUTES=102, SECONDS=103, START=104, RESET=105, HIDE=106;
constexpr int SHOTS=201, NOTES=202, TOOLS=203, COPY_SHOT=204, SAVE_SHOT=205, DELETE_SHOT=206, NOTE_EDIT=207, NOTE_DONE=208, NOTE_NEW=209, NOTE_LIST=210, NOTE_COPY=211, NOTE_DELETE=212, THUMB=220;
constexpr UINT CLIP_RETRY=3, NOTE_SAVE=4;
enum class Page { Shots, Notes, Tools };Page page=Page::Shots;
bool expanded=false,loadingNote=false,noteDirty=false;uint64_t editingNote=0;
int selectedShot=0,clipboardRetries=0;DWORD clipboardSequence=0;
Store store;std::wstring notice;
std::vector<std::unique_ptr<Bitmap>> thumbnails;std::unique_ptr<Bitmap> selectedPreview;
HWND shotsButton,notesButton,toolsButton,copyShotButton,saveShotButton,deleteShotButton,noteEdit,noteDone,noteNew,noteList,noteCopy,noteDelete,thumbButtons[5];
HFONT iconFont;
bool commitNote();void loadPreview();void updatePanels();void rebuildNotes();void clipboardChanged();
HWND win, slider, minutesBox, secondsBox, startButton, resetButton, hideButton;
HWND previousForeground;
HMONITOR targetMonitor=nullptr;
bool positioning=false;
HINSTANCE instance;
HFONT normalFont, smallFont, largeFont, headingFont;
HBRUSH backgroundBrush, fieldBrush;
COLORREF bg, fg, dim, field, line, accent;
bool dark=false, audioReady=false, muted=false, shown=false, menuOpen=false;
#ifdef ISLAND_SELF_TEST
int forcedTestTheme=-1;
#endif
#ifdef ISLAND_PREVIEW
constexpr ULONGLONG idleTimeout=120000; // Allow UI inspection tools to attach.
#else
constexpr ULONGLONG idleTimeout=10000;
#endif
float volume=0;
UINT dpi=96, taskbarMessage;
ULONGLONG lastActivity=0, lastAudio=0;
POINT lastPointer={LONG_MIN,LONG_MIN};
Countdown countdown;
NOTIFYICONDATAW tray{};
int px(int x){return MulDiv(x,dpi,96);}
int compactX(int x){return MulDiv(x,380,440);}
int compactY(int y){return y>=54?y-12:y;}
RECT box(int x,int y,int w,int h){return {px(compactX(x)),px(compactY(y)),px(compactX(x+w)),px(compactY(y)+h)};}
void repaint(){if(shown)InvalidateRect(win,nullptr,FALSE);}
void activity(){lastActivity=GetTickCount64();}
int designHeight(){return page==Page::Shots?440:page==Page::Notes?454:338;}
int panelHeight(){return !expanded?42:designHeight()-12;}
int panelWidth(){return expanded?380:200;}
void toast(const std::wstring& text){notice=text;activity();repaint();}

// Acquire the current default output each time, so unplugging headphones is safe.
IAudioEndpointVolume* endpoint(){
    IMMDeviceEnumerator* enumerator=nullptr; IMMDevice* device=nullptr; IAudioEndpointVolume* result=nullptr;
    if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)&enumerator))){
        if(SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device))){
            device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,(void**)&result);device->Release();
        }enumerator->Release();
    }return result;
}
void readVolume(){
    auto* ep=endpoint(); float value=volume; BOOL mute=FALSE;
    bool ok=ep && SUCCEEDED(ep->GetMasterVolumeLevelScalar(&value)) && SUCCEEDED(ep->GetMute(&mute));
    if(ep)ep->Release();
    bool changed=(ok!=audioReady || value!=volume || bool(mute)!=muted);
    audioReady=ok; volume=value; muted=bool(mute);
    EnableWindow(slider,ok);SendMessageW(slider,TBM_SETPOS,TRUE,lround(value*100));
    if(changed)repaint();
}
void setVolume(int percent){
    auto* ep=endpoint();if(ep){ep->SetMasterVolumeLevelScalar(std::clamp(percent,0,100)/100.f,nullptr);ep->Release();}
    activity();readVolume();
}
void theme(){
    DWORD light=1, bytes=sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);
    dark=!light;
#ifdef ISLAND_SELF_TEST
    if(forcedTestTheme>=0)dark=forcedTestTheme;
#endif
    bg=dark?RGB(32,32,32):RGB(249,249,249);fg=dark?RGB(245,245,245):RGB(25,25,25);
    dim=dark?RGB(169,169,169):RGB(100,100,100);field=dark?RGB(45,45,45):RGB(239,239,239);
    line=dark?RGB(65,65,65):RGB(220,220,220);accent=dark?RGB(96,205,255):RGB(0,103,192);
    if(backgroundBrush)DeleteObject(backgroundBrush);if(fieldBrush)DeleteObject(fieldBrush);
    backgroundBrush=CreateSolidBrush(bg);fieldBrush=CreateSolidBrush(field);
    BOOL b=dark;DwmSetWindowAttribute(win,20,&b,sizeof(b));
    if(slider)SetWindowTheme(slider,L"",L"");
    if(noteList)SetWindowTheme(noteList,L"",L"");
    if(win)RedrawWindow(win,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
HFONT font(int size,int weight=FW_NORMAL){return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
void layout(){
    for(HFONT f:{normalFont,smallFont,largeFont,headingFont,iconFont})if(f)DeleteObject(f);
    normalFont=font(14);smallFont=font(12);largeFont=font(28,FW_SEMIBOLD);headingFont=font(15,FW_SEMIBOLD);
    iconFont=CreateFontW(-px(17),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe MDL2 Assets");
    struct Item{HWND h;int x,y,w,hgt;HFONT f;};
    for(auto a:{Item{slider,20,96,400,30,normalFont},Item{minutesBox,30,193,90,36,largeFont},Item{secondsBox,154,193,70,36,largeFont},Item{startButton,24,260,240,36,normalFont},Item{resetButton,276,260,140,36,normalFont},
        Item{copyShotButton,20,340,174,36,normalFont},Item{saveShotButton,202,340,166,36,normalFont},Item{deleteShotButton,376,340,44,36,normalFont},
        Item{noteEdit,20,100,400,94,normalFont},Item{noteDone,20,208,140,34,normalFont},Item{noteNew,170,208,126,34,normalFont},
        Item{noteList,20,256,400,112,normalFont},Item{noteCopy,20,380,180,34,normalFont},Item{noteDelete,210,380,210,34,normalFont}}){
        MoveWindow(a.h,px(compactX(a.x)),px(compactY(a.y)),px(compactX(a.x+a.w)-compactX(a.x)),px(a.hgt),TRUE);SendMessageW(a.h,WM_SETFONT,(WPARAM)a.f,TRUE);
    }
    int left=(panelWidth()-180)/2;
    for(auto a:{Item{shotsButton,left,5,48,32,normalFont},Item{notesButton,left+52,5,40,32,normalFont},Item{toolsButton,left+96,5,40,32,normalFont},Item{hideButton,left+140,5,40,32,normalFont}}){MoveWindow(a.h,px(a.x),px(a.y),px(a.w),px(a.hgt),TRUE);SendMessageW(a.h,WM_SETFONT,(WPARAM)a.f,TRUE);}
    for(int i=0;i<5;i++){MoveWindow(thumbButtons[i],px(compactX(20+i*82)),px(88),px(compactX(72)),px(54),TRUE);SendMessageW(thumbButtons[i],WM_SETFONT,(WPARAM)smallFont,TRUE);}
    SendMessageW(noteList,LB_SETITEMHEIGHT,0,px(48));
    // A rounded region works on Windows 10 as well as Windows 11.
    SetWindowRgn(win,CreateRoundRectRgn(0,0,px(panelWidth())+1,px(panelHeight())+1,px(38),px(38)),TRUE);
    updatePanels();
}
RECT anchoredRect(RECT work,int width,int height,int gap){
    int x=work.left+std::max(0,(int(work.right-work.left)-width)/2);
    int y=work.top+std::min(gap,std::max(0,int(work.bottom-work.top)-height));
    return {x,y,x+width,y+height};
}
void position(){
    if(positioning)return;positioning=true;
    MONITORINFO mi{};mi.cbSize=sizeof(mi);
    if(!GetMonitorInfoW(targetMonitor,&mi)){targetMonitor=MonitorFromWindow(win,MONITOR_DEFAULTTONEAREST);if(!GetMonitorInfoW(targetMonitor,&mi)){positioning=false;return;}}
    // Moving across screens can synchronously deliver WM_DPICHANGED. Recenter
    // once with the new DPI, without recursively moving back to another screen.
    for(int pass=0;pass<2;++pass){UINT before=dpi;RECT r=anchoredRect(mi.rcWork,px(panelWidth()),px(panelHeight()),px(10));SetWindowPos(win,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);if(before==dpi)break;}
    positioning=false;
}
void hide(){if(!commitNote())return;bool restore=GetForegroundWindow()==win;shown=false;KillTimer(win,VISIBLE_TICK);ShowWindow(win,SW_HIDE);thumbnails.clear();selectedPreview.reset();if(restore&&IsWindow(previousForeground))SetForegroundWindow(previousForeground);}
void show(){HWND current=GetForegroundWindow();if(current!=win)previousForeground=current;POINT cursor;if(GetCursorPos(&cursor))targetMonitor=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);else targetMonitor=MonitorFromWindow(current,MONITOR_DEFAULTTONEAREST);if(!shown)expanded=false;shown=true;theme();layout();position();loadPreview();if(expanded&&page==Page::Tools)readVolume();activity();GetCursorPos(&lastPointer);ShowWindow(win,SW_SHOW);SetWindowPos(win,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);SetForegroundWindow(win);SetTimer(win,VISIBLE_TICK,250,nullptr);repaint();}
void addTray(){
    tray={};tray.cbSize=sizeof(tray);tray.hWnd=win;tray.uID=1;tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=TRAY;
    tray.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));if(!tray.hIcon)tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(tray.szTip,L"Island · Ctrl + PgUp — открыть");Shell_NotifyIconW(NIM_ADD,&tray);
}
void syncTimer(){
    bool active=countdown.state==Countdown::Running || countdown.state==Countdown::Paused;
    EnableWindow(minutesBox,!active);EnableWindow(secondsBox,!active);
    SetWindowTextW(startButton,countdown.state==Countdown::Running?L"Пауза":countdown.state==Countdown::Paused?L"Продолжить":L"Запустить");
    KillTimer(win,DEADLINE_TICK);
    if(countdown.state==Countdown::Running)SetTimer(win,DEADLINE_TICK,(UINT)std::max<uint64_t>(1,countdown.remaining(GetTickCount64())),nullptr);
    repaint();
}
void complete(){
    if(countdown.update(GetTickCount64())){
        PlaySoundW(MAKEINTRESOURCEW(2),instance,SND_RESOURCE|SND_ASYNC|SND_NODEFAULT);
        syncTimer(); // Never reveal the island automatically.
    }
}
bool parseTime(HWND h,unsigned max,unsigned& result){
    wchar_t value[16];GetWindowTextW(h,value,16);result=0;if(!value[0])return true;
    for(auto* p=value;*p;++p){if(*p<L'0'||*p>L'9')return false;result=result*10+(*p-L'0');if(result>max)return false;}return true;
}
void toggleTimer(){
    complete();
    if(countdown.state==Countdown::Running)countdown.pause(GetTickCount64());
    else if(countdown.state==Countdown::Paused)countdown.resume(GetTickCount64());
    else{
        unsigned m,s;
        if(!parseTime(minutesBox,999,m)||!parseTime(secondsBox,59,s)||(!m&&!s)){
            MessageBoxW(win,L"Введите от 0 до 999 минут и от 0 до 59 секунд. Время должно быть больше нуля.",L"Время таймера",MB_OK|MB_ICONINFORMATION);activity();return;
        }
        PlaySoundW(nullptr,nullptr,0);countdown.start(GetTickCount64(),(m*60ULL+s)*1000);
    }syncTimer();activity();
}
void updatePanels(){
    auto visible=[&](HWND h,bool v){if(h)ShowWindow(h,v?SW_SHOW:SW_HIDE);};
    for(HWND h:{slider,minutesBox,secondsBox,startButton,resetButton})visible(h,expanded&&page==Page::Tools);
    for(HWND h:{copyShotButton,saveShotButton,deleteShotButton}){visible(h,expanded&&page==Page::Shots);EnableWindow(h,!store.shots.empty());}
    for(int i=0;i<5;i++)visible(thumbButtons[i],expanded&&page==Page::Shots&&i<int(store.shots.size()));
    for(HWND h:{noteEdit,noteDone,noteNew,noteList,noteCopy,noteDelete})visible(h,expanded&&page==Page::Notes);
    EnableWindow(noteEdit,store.ready&&store.notesReadable);EnableWindow(noteDelete,editingNote!=0);
    InvalidateRect(shotsButton,nullptr,FALSE);InvalidateRect(notesButton,nullptr,FALSE);InvalidateRect(toolsButton,nullptr,FALSE);InvalidateRect(hideButton,nullptr,FALSE);
}
void loadPreview(){
    selectedPreview.reset();thumbnails.clear();
    selectedShot=store.shots.empty()?0:std::clamp(selectedShot,0,int(store.shots.size())-1);
    if(!shown||!expanded||page!=Page::Shots)return;
    for(auto&s:store.shots)thumbnails.push_back(reducedImage(s.path,144,108));
    if(!store.shots.empty())selectedPreview=reducedImage(store.shots[selectedShot].path,800,280);
    for(HWND h:thumbButtons)InvalidateRect(h,nullptr,FALSE);
}
std::wstring editText(HWND h){int length=GetWindowTextLengthW(h);std::wstring s(length+1,L'\0');GetWindowTextW(h,s.data(),length+1);s.resize(length);return s;}
void rebuildNotes(){
    SendMessageW(noteList,WM_SETREDRAW,FALSE,0);SendMessageW(noteList,LB_RESETCONTENT,0,0);
    for(size_t i=0;i<store.notes.size();i++){auto&note=store.notes[i];LRESULT row=SendMessageW(noteList,LB_ADDSTRING,0,(LPARAM)note.text.c_str());if(row!=LB_ERR&&row!=LB_ERRSPACE){SendMessageW(noteList,LB_SETITEMDATA,row,i);if(note.id==editingNote)SendMessageW(noteList,LB_SETCURSEL,row,0);}}
    SendMessageW(noteList,WM_SETREDRAW,TRUE,0);InvalidateRect(noteList,nullptr,TRUE);
}
bool commitNote(){
    KillTimer(win,NOTE_SAVE);if(!noteDirty)return true;
    std::wstring text=editText(noteEdit);auto backup=store.notes;uint64_t oldId=editingNote;
    auto found=std::find_if(store.notes.begin(),store.notes.end(),[](const Note&n){return n.id==editingNote;});
    if(found!=store.notes.end())found->text=text;
    else if(text.find_first_not_of(L" \r\n\t")!=std::wstring::npos){editingNote=newId();store.notes.insert(store.notes.begin(),{editingNote,text});}
    else {noteDirty=false;return true;}
    if(!store.saveNotes()){store.notes=std::move(backup);editingNote=oldId;toast(L"Не удалось сохранить заметку. Текст остаётся в поле.");return false;}
    noteDirty=false;notice=L"Сохранено на этом компьютере";rebuildNotes();updatePanels();repaint();return true;
}
void newNote(){if(!commitNote())return;editingNote=0;loadingNote=true;SetWindowTextW(noteEdit,L"");loadingNote=false;SendMessageW(noteList,LB_SETCURSEL,-1,0);notice.clear();SetFocus(noteEdit);updatePanels();repaint();}
void selectNote(){
    int row=int(SendMessageW(noteList,LB_GETCURSEL,0,0));if(row==LB_ERR)return;size_t i=SendMessageW(noteList,LB_GETITEMDATA,row,0);if(i>=store.notes.size())return;uint64_t id=store.notes[i].id;
    if(!commitNote())return;auto it=std::find_if(store.notes.begin(),store.notes.end(),[&](const Note&n){return n.id==id;});if(it==store.notes.end())return;
    editingNote=id;loadingNote=true;SetWindowTextW(noteEdit,it->text.c_str());loadingNote=false;notice=L"Изменения сохраняются автоматически";updatePanels();repaint();
}
void deleteNote(){
    if(!editingNote)return;KillTimer(win,NOTE_SAVE);auto backup=store.notes;store.notes.erase(std::remove_if(store.notes.begin(),store.notes.end(),[](const Note&n){return n.id==editingNote;}),store.notes.end());
    if(!store.saveNotes()){store.notes=std::move(backup);toast(L"Не удалось удалить заметку");return;}
    noteDirty=false;editingNote=0;loadingNote=true;SetWindowTextW(noteEdit,L"");loadingNote=false;rebuildNotes();updatePanels();toast(L"Заметка удалена");
}
void selectPage(Page selected){if(!commitNote())return;page=selected;expanded=true;notice.clear();layout();position();loadPreview();if(page==Page::Tools)readVolume();SetFocus(win);activity();repaint();}
void clipboardChanged(){
    KillTimer(win,CLIP_RETRY);DWORD seq=GetClipboardSequenceNumber();
    if(seq==clipboardSequence||GetClipboardOwner()==win){clipboardSequence=seq;clipboardRetries=0;return;}
    if(!IsClipboardFormatAvailable(CF_BITMAP)){clipboardSequence=seq;clipboardRetries=0;return;}
    if(!OpenClipboard(win)){if(++clipboardRetries<=5)SetTimer(win,CLIP_RETRY,100,nullptr);return;}
    HBITMAP source=(HBITMAP)GetClipboardData(CF_BITMAP);HBITMAP copy=source?(HBITMAP)CopyImage(source,IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION):nullptr;
    clipboardSequence=GetClipboardSequenceNumber();CloseClipboard();clipboardRetries=0;
    ImagePixels pixels;bool ok=bitmapPixels(copy,pixels);if(copy)DeleteObject(copy);if(!ok){notice=L"Изображение недоступно или больше 32 млн пикселей";repaint();return;}
    int result=store.addImage(pixels);
    if(result>0){selectedShot=0;loadPreview();updatePanels();notice=L"Снимок добавлен из буфера Windows";repaint();}
    else if(result<0){notice=L"Не удалось сохранить снимок на диск";repaint();}
}
bool putClipboard(UINT format,HGLOBAL memory){
    if(!OpenClipboard(win)){GlobalFree(memory);toast(L"Буфер занят другой программой. Повторите.");return false;}
    bool ok=EmptyClipboard()&&SetClipboardData(format,memory);CloseClipboard();clipboardSequence=GetClipboardSequenceNumber();
    if(!ok){GlobalFree(memory);toast(L"Не удалось скопировать");}return ok;
}
void copyShot(){
    if(store.shots.empty())return;Bitmap source(store.shots[selectedShot].path.c_str());HBITMAP bitmap=nullptr;
    if(source.GetHBITMAP(Gdiplus::Color(255,255,255),&bitmap)!=Gdiplus::Ok){toast(L"Не удалось прочитать снимок");return;}
    ImagePixels pixels;bool ok=bitmapPixels(bitmap,pixels);DeleteObject(bitmap);if(!ok)return;
    size_t size=sizeof(BITMAPINFOHEADER)+pixels.bytes.size();HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory){toast(L"Недостаточно памяти");return;}
    auto* data=(BYTE*)GlobalLock(memory);if(!data){GlobalFree(memory);return;}BITMAPINFOHEADER bi{};bi.biSize=sizeof(bi);bi.biWidth=pixels.width;bi.biHeight=-pixels.height;bi.biPlanes=1;bi.biBitCount=32;bi.biCompression=BI_RGB;bi.biSizeImage=DWORD(pixels.bytes.size());memcpy(data,&bi,sizeof(bi));memcpy(data+sizeof(bi),pixels.bytes.data(),pixels.bytes.size());GlobalUnlock(memory);
    if(putClipboard(CF_DIB,memory))toast(L"Скопировано. Ctrl + PgDn, затем Ctrl + V");
}
void copyNote(){
    std::wstring text=editText(noteEdit);if(text.empty()){toast(L"Выберите заметку или введите текст");return;}
    size_t bytes=(text.size()+1)*sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory)return;void*p=GlobalLock(memory);if(!p){GlobalFree(memory);return;}memcpy(p,text.c_str(),bytes);GlobalUnlock(memory);if(putClipboard(CF_UNICODETEXT,memory))toast(L"Текст скопирован");
}
void exportShot(){
    if(store.shots.empty())return;wchar_t path[32768]=L"Скриншот.png";OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=win;ofn.lpstrFilter=L"Изображение PNG\0*.png\0\0";ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrDefExt=L"png";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    menuOpen=true;bool chosen=GetSaveFileNameW(&ofn);menuOpen=false;activity();
    if(chosen){std::error_code ec;if(fs::equivalent(store.shots[selectedShot].path,fs::path(path),ec)){toast(L"Этот снимок уже сохранён здесь");return;}toast(CopyFileW(store.shots[selectedShot].path.c_str(),path,FALSE)?L"PNG сохранён":L"Не удалось сохранить PNG");}
}
void drawText(HDC dc,const std::wstring& s,RECT r,HFONT f,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE){SelectObject(dc,f);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),-1,&r,flags);}
void rounded(HDC dc,RECT r,COLORREF fill,int radius){auto b=CreateSolidBrush(fill);auto oldB=SelectObject(dc,b);auto oldP=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,px(radius),px(radius));SelectObject(dc,oldP);SelectObject(dc,oldB);DeleteObject(b);}
void drawImage(HDC dc,Bitmap* bitmap,RECT r){if(!bitmap)return;double ratio=std::min(double(r.right-r.left)/bitmap->GetWidth(),double(r.bottom-r.top)/bitmap->GetHeight());int w=int(bitmap->GetWidth()*ratio),h=int(bitmap->GetHeight()*ratio);Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(bitmap,r.left+(r.right-r.left-w)/2,r.top+(r.bottom-r.top-h)/2,w,h);}
void separator(HDC dc,int y){auto p=CreatePen(PS_SOLID,1,line);auto old=SelectObject(dc,p);MoveToEx(dc,px(compactX(20)),px(compactY(y)),nullptr);LineTo(dc,px(compactX(420)),px(compactY(y)));SelectObject(dc,old);DeleteObject(p);}
void drawSurface(HDC dc,RECT client){
    FillRect(dc,&client,backgroundBrush);
    if(!expanded)return;
    separator(dc,54);
    if(page==Page::Shots){
        drawText(dc,L"Скриншоты",box(20,64,240,28),headingFont,fg);
        drawText(dc,std::to_wstring(store.shots.size())+L" / 5",box(310,64,110,28),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        rounded(dc,box(20,166,400,140),field,12);
        if(store.shots.empty()){
            drawText(dc,L"Скопируйте изображение",box(38,192,364,26),headingFont,fg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            drawText(dc,L"или сделайте снимок через Win + Shift + S",box(28,224,384,44),smallFont,dim,DT_CENTER|DT_WORDBREAK);
        }else{
            drawImage(dc,selectedPreview.get(),box(26,172,388,128));
            drawText(dc,L"Выберите снимок и скопируйте для вставки",box(20,310,400,26),smallFont,dim);
        }
        drawText(dc,notice,box(20,378,400,28),smallFont,dim,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(page==Page::Notes){
        drawText(dc,editingNote?L"Заметка":L"Быстрая заметка",box(20,64,270,28),headingFont,fg);
        drawText(dc,std::to_wstring(editText(noteEdit).size())+L" / 500",box(310,64,110,28),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        drawText(dc,L"Ctrl + Enter",box(302,208,118,34),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        if(store.notes.empty())drawText(dc,L"Сохранённые заметки появятся здесь",box(26,270,388,70),smallFont,dim,DT_CENTER|DT_WORDBREAK);
    }else{
        drawText(dc,L"Громкость",box(24,65,190,25),normalFont,fg);
        std::wstring vol=audioReady?std::to_wstring(lround(volume*100))+L"%":L"Нет устройства";if(muted&&audioReady)vol=L"Звук выключен · "+vol;
        drawText(dc,vol,box(190,65,226,25),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        separator(dc,140);drawText(dc,L"Таймер",box(24,151,150,28),headingFont,fg);
        const wchar_t* status=countdown.state==Countdown::Running?L"Идёт отсчёт":countdown.state==Countdown::Paused?L"На паузе":countdown.state==Countdown::Finished?L"Время вышло":L"Готов к запуску";
        drawText(dc,status,box(220,151,196,28),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        rounded(dc,box(24,187,104,46),field,10);rounded(dc,box(148,187,82,46),field,10);drawText(dc,L":",box(128,187,20,46),largeFont,dim,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        drawText(dc,L"минуты",box(24,233,104,20),smallFont,dim);drawText(dc,L"секунды",box(148,233,82,20),smallFont,dim);
        if(countdown.state!=Countdown::Idle){auto sec=(countdown.remaining(GetTickCount64())+999)/1000;wchar_t text[32];swprintf_s(text,L"%02llu:%02llu",sec/60,sec%60);drawText(dc,text,box(238,187,178,46),largeFont,fg,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
    }
    separator(dc,designHeight()-30);
    std::wstring footer=page==Page::Notes?(noteDirty?L"Сохранение…":notice.empty()?L"Автосохранение · только на этом ПК":notice):L"Автоскрытие · 10 с";
    drawText(dc,footer,box(20,designHeight()-28,290,24),smallFont,dim,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    drawText(dc,L"Ctrl + PgDn",box(324,designHeight()-28,96,24),smallFont,dim,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
}
void paint(){
    PAINTSTRUCT ps;HDC target=BeginPaint(win,&ps);RECT client;GetClientRect(win,&client);
    HDC dc=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);auto old=SelectObject(dc,bitmap);
    drawSurface(dc,client);
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(win,&ps);
}
void trayMenu(){
    menuOpen=true;activity();HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Открыть   Ctrl + PgUp");AppendMenuW(menu,MF_STRING,2,L"Скрыть   Ctrl + PgDn");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,3,L"Выход");
    POINT p;GetCursorPos(&p);SetForegroundWindow(win);int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,win,nullptr);DestroyMenu(menu);PostMessageW(win,WM_NULL,0,0);menuOpen=false;
    if(cmd==1)show();else if(cmd==2)hide();else if(cmd==3&&commitNote())DestroyWindow(win);activity();
}
LRESULT CALLBACK timerEditProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
    if(msg==WM_SETFOCUS)PostMessageW(h,EM_SETSEL,0,-1);
    if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,timerEditProc,id);
    return DefSubclassProc(h,msg,wp,lp);
}
LRESULT CALLBACK proc(HWND h,UINT message,WPARAM wp,LPARAM lp){
    if(message==taskbarMessage&&taskbarMessage){addTray();return 0;}
    switch(message){
    case WM_CREATE:{
        win=h;dpi=GetDpiForWindow(h);theme();
        auto child=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,0,0,h,(HMENU)(INT_PTR)id,instance,nullptr);};
        slider=child(TRACKBAR_CLASSW,L"Громкость",TBS_HORZ|TBS_NOTICKS,VOL);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(slider,TBM_SETPAGESIZE,0,5);
        minutesBox=child(L"EDIT",L"5",ES_NUMBER|ES_CENTER,MINUTES);secondsBox=child(L"EDIT",L"00",ES_NUMBER|ES_CENTER,SECONDS);
        SetWindowSubclass(minutesBox,timerEditProc,1,0);SetWindowSubclass(secondsBox,timerEditProc,2,0);
        SendMessageW(minutesBox,EM_SETLIMITTEXT,3,0);SendMessageW(secondsBox,EM_SETLIMITTEXT,2,0);
        startButton=child(L"BUTTON",L"Запустить",BS_OWNERDRAW,START);resetButton=child(L"BUTTON",L"Сброс",BS_OWNERDRAW,RESET);hideButton=child(L"BUTTON",L"−",BS_OWNERDRAW,HIDE);
        shotsButton=child(L"BUTTON",L"Скриншоты",BS_OWNERDRAW,SHOTS);notesButton=child(L"BUTTON",L"Заметки",BS_OWNERDRAW,NOTES);toolsButton=child(L"BUTTON",L"Громкость и таймер",BS_OWNERDRAW,TOOLS);
        SetWindowTextW(hideButton,L"Свернуть или раскрыть");
        copyShotButton=child(L"BUTTON",L"Копировать",BS_OWNERDRAW,COPY_SHOT);saveShotButton=child(L"BUTTON",L"Сохранить PNG",BS_OWNERDRAW,SAVE_SHOT);deleteShotButton=child(L"BUTTON",L"Удалить снимок",BS_OWNERDRAW,DELETE_SHOT);
        for(int i=0;i<5;i++){std::wstring name=L"Снимок "+std::to_wstring(i+1);thumbButtons[i]=child(L"BUTTON",name.c_str(),BS_OWNERDRAW,THUMB+i);}
        noteEdit=child(L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,NOTE_EDIT);SendMessageW(noteEdit,EM_SETLIMITTEXT,500,0);SendMessageW(noteEdit,EM_SETCUEBANNER,TRUE,(LPARAM)L"Что нужно запомнить?");
        noteDone=child(L"BUTTON",L"Готово",BS_OWNERDRAW,NOTE_DONE);noteNew=child(L"BUTTON",L"+ Новая",BS_OWNERDRAW,NOTE_NEW);
        noteList=child(L"LISTBOX",L"Заметки",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,NOTE_LIST);
        noteCopy=child(L"BUTTON",L"Копировать текст",BS_OWNERDRAW,NOTE_COPY);noteDelete=child(L"BUTTON",L"Удалить заметку",BS_OWNERDRAW,NOTE_DELETE);
        HWND tips=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,0,0,0,0,h,nullptr,instance,nullptr);
        for(auto item:{std::pair<HWND,const wchar_t*>{shotsButton,L"Последние 5 изображений"},{notesButton,L"Быстрые заметки"},{toolsButton,L"Громкость и таймер"},{hideButton,L"Свернуть / раскрыть панель"},{deleteShotButton,L"Удалить снимок из истории"}}){TOOLINFOW ti{};ti.cbSize=sizeof(ti);ti.uFlags=TTF_IDISHWND|TTF_SUBCLASS;ti.hwnd=h;ti.uId=(UINT_PTR)item.first;ti.lpszText=const_cast<wchar_t*>(item.second);SendMessageW(tips,TTM_ADDTOOLW,0,(LPARAM)&ti);}
        layout();position();theme();rebuildNotes();addTray();return 0;
    }
    case WM_PAINT:paint();return 0;
    case WM_PRINTCLIENT:{RECT client;GetClientRect(h,&client);drawSurface((HDC)wp,client);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_NOTIFY:{auto n=(NMHDR*)lp;if(n->idFrom==VOL&&n->code==NM_CUSTOMDRAW){auto d=(NMCUSTOMDRAW*)lp;
        if(d->dwDrawStage==CDDS_PREPAINT){RECT r;GetClientRect(slider,&r);FillRect(d->hdc,&r,backgroundBrush);return CDRF_NOTIFYITEMDRAW;}
        if(d->dwDrawStage==CDDS_ITEMPREPAINT){
            if(d->dwItemSpec==TBCD_CHANNEL){RECT r=d->rc;int y=(r.top+r.bottom)/2;r.top=y-px(2);r.bottom=y+px(2);rounded(d->hdc,r,line,4);RECT fill=r;fill.right=r.left+MulDiv(r.right-r.left,(int)SendMessageW(slider,TBM_GETPOS,0,0),100);if(fill.right>fill.left)rounded(d->hdc,fill,accent,4);return CDRF_SKIPDEFAULT;}
            if(d->dwItemSpec==TBCD_THUMB){Gdiplus::Graphics g(d->hdc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::SolidBrush outer(Gdiplus::Color(255,GetRValue(bg),GetGValue(bg),GetBValue(bg))),inner(Gdiplus::Color(255,GetRValue(accent),GetGValue(accent),GetBValue(accent)));int x=(d->rc.left+d->rc.right)/2,y=(d->rc.top+d->rc.bottom)/2;g.FillEllipse(&outer,x-px(9),y-px(9),px(18),px(18));g.FillEllipse(&inner,x-px(6),y-px(6),px(12),px(12));return CDRF_SKIPDEFAULT;}
        }
    }return 0;}
    case WM_MEASUREITEM:{auto m=(MEASUREITEMSTRUCT*)lp;m->itemHeight=px(48);return TRUE;}
    case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)lp;RECT r=d->rcItem;
        if(d->CtlID==NOTE_LIST){FillRect(d->hDC,&r,backgroundBrush);if(d->itemID<store.notes.size()){bool selected=d->itemState&ODS_SELECTED;if(selected)rounded(d->hDC,r,field,7);RECT t=r;InflateRect(&t,-px(9),0);std::wstring text=store.notes[d->itemID].text;std::replace(text.begin(),text.end(),L'\r',L' ');std::replace(text.begin(),text.end(),L'\n',L' ');drawText(d->hDC,text,t,normalFont,fg,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);}return TRUE;}
        if(d->CtlID>=THUMB&&d->CtlID<THUMB+5){int i=d->CtlID-THUMB;FillRect(d->hDC,&r,backgroundBrush);rounded(d->hDC,r,i==selectedShot?accent:line,8);InflateRect(&r,-px(2),-px(2));rounded(d->hDC,r,field,6);InflateRect(&r,-px(3),-px(3));if(i<int(thumbnails.size()))drawImage(d->hDC,thumbnails[i].get(),r);if(d->itemState&ODS_FOCUS)DrawFocusRect(d->hDC,&r);return TRUE;}
        bool primary=d->CtlID==START||d->CtlID==COPY_SHOT||d->CtlID==NOTE_DONE;
        bool nav=d->CtlID==SHOTS||d->CtlID==NOTES||d->CtlID==TOOLS||d->CtlID==HIDE;
        bool active=expanded&&((d->CtlID==SHOTS&&page==Page::Shots)||(d->CtlID==NOTES&&page==Page::Notes)||(d->CtlID==TOOLS&&page==Page::Tools));
        FillRect(d->hDC,&r,backgroundBrush);COLORREF fill=primary?accent:nav&&!active?bg:field;if(d->itemState&ODS_SELECTED)fill=dim;rounded(d->hDC,r,fill,9);
        wchar_t s[64];GetWindowTextW(d->hwndItem,s,64);HFONT f=normalFont;
        if(nav||d->CtlID==DELETE_SHOT){f=iconFont;wcscpy_s(s,d->CtlID==SHOTS?L"\uEB9F":d->CtlID==NOTES?L"\uE70B":d->CtlID==TOOLS?L"\uE713":d->CtlID==DELETE_SHOT?L"\uE74D":expanded?L"\uE70E":L"\uE70D");}
        COLORREF color=(d->itemState&ODS_DISABLED)?dim:primary?(dark?RGB(0,40,58):RGB(255,255,255)):active?accent:fg;
        drawText(d->hDC,s,r,f,color,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(d->CtlID==SHOTS&&!store.shots.empty()){RECT badge=r;badge.left=badge.right-px(15);badge.top+=px(3);badge.bottom=badge.top+px(14);drawText(d->hDC,std::to_wstring(store.shots.size()),badge,smallFont,dim,DT_CENTER|DT_SINGLELINE);}
        if(d->itemState&ODS_FOCUS){InflateRect(&r,-px(4),-px(4));DrawFocusRect(d->hDC,&r);}return TRUE;}
    case WM_CTLCOLOREDIT:SetTextColor((HDC)wp,fg);SetBkColor((HDC)wp,field);return (LRESULT)fieldBrush;
    case WM_CTLCOLORLISTBOX:SetTextColor((HDC)wp,fg);SetBkColor((HDC)wp,bg);return (LRESULT)backgroundBrush;
    case WM_CTLCOLORSTATIC:if((HWND)lp==minutesBox||(HWND)lp==secondsBox||(HWND)lp==noteEdit){SetTextColor((HDC)wp,dim);SetBkColor((HDC)wp,field);return (LRESULT)fieldBrush;}SetBkColor((HDC)wp,bg);return (LRESULT)backgroundBrush;
    case WM_HSCROLL:if((HWND)lp==slider){setVolume((int)SendMessageW(slider,TBM_GETPOS,0,0));InvalidateRect(slider,nullptr,TRUE);}return 0;
    case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&p);if(expanded&&page==Page::Tools&&p.y>=px(compactY(60))&&p.y<=px(compactY(134)))setVolume(lround(volume*100)+(GET_WHEEL_DELTA_WPARAM(wp)>0?2:-2));activity();return 0;}
    case WM_COMMAND:{activity();int id=LOWORD(wp);
        if(id==NOTE_EDIT){if(HIWORD(wp)==EN_CHANGE&&!loadingNote){noteDirty=true;KillTimer(h,NOTE_SAVE);SetTimer(h,NOTE_SAVE,600,nullptr);repaint();}return 0;}
        if(id==NOTE_LIST){if(HIWORD(wp)==LBN_SELCHANGE)selectNote();return 0;}
        if(id>=THUMB&&id<THUMB+5){selectedShot=id-THUMB;loadPreview();repaint();return 0;}
        switch(id){case START:toggleTimer();break;case RESET:countdown.reset();PlaySoundW(nullptr,nullptr,0);syncTimer();break;
        case HIDE:if(commitNote()){expanded=!expanded;layout();position();loadPreview();SetFocus(win);repaint();}break;
        case SHOTS:selectPage(Page::Shots);break;case NOTES:selectPage(Page::Notes);break;case TOOLS:selectPage(Page::Tools);break;
        case COPY_SHOT:copyShot();break;case SAVE_SHOT:exportShot();break;case DELETE_SHOT:if(!store.shots.empty()){if(store.removeShot(selectedShot)){loadPreview();updatePanels();toast(L"Снимок удалён из истории");}else toast(L"Не удалось удалить снимок");}break;
        case NOTE_DONE:if(commitNote()){SetFocus(win);toast(L"Сохранено на этом компьютере");}break;case NOTE_NEW:newNote();break;case NOTE_COPY:copyNote();break;case NOTE_DELETE:deleteNote();break;}return 0;}
    case WM_HOTKEY:if(wp==1)show();else if(wp==2)hide();return 0;
    case WM_CLIPBOARDUPDATE:clipboardRetries=0;clipboardChanged();return 0;
    case WM_TIMER:if(wp==NOTE_SAVE){commitNote();}else if(wp==CLIP_RETRY)clipboardChanged();else if(wp==DEADLINE_TICK){complete();syncTimer();}else if(wp==VISIBLE_TICK){complete();auto now=GetTickCount64();POINT p=lastPointer;bool pointerAvailable=GetCursorPos(&p);RECT r;GetWindowRect(h,&r);if(pointerAvailable&&(p.x!=lastPointer.x||p.y!=lastPointer.y)&&PtInRect(&r,p))activity();lastPointer=p;bool writing=expanded&&page==Page::Notes&&GetFocus()==noteEdit;if(!menuOpen&&!writing&&now-lastActivity>=idleTimeout){hide();return 0;}if(now-lastAudio>=1000){lastAudio=now;if(expanded&&page==Page::Tools){readVolume();if(countdown.state==Countdown::Running)repaint();}}}return 0;
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:theme();return 0;
    case WM_DISPLAYCHANGE:position();return 0;
    case WM_DPICHANGED:{dpi=HIWORD(wp);layout();if(!positioning){auto suggested=(RECT*)lp;if(suggested){positioning=true;SetWindowPos(h,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);positioning=false;}position();}return 0;}
    case WM_POWERBROADCAST:if(wp==PBT_APMRESUMEAUTOMATIC){complete();syncTimer();}return TRUE;
    case TRAY:if(lp==WM_LBUTTONUP)show();else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)trayMenu();return 0;
    case WM_CONTEXTMENU:if((HWND)wp==h)trayMenu();else return DefWindowProcW(h,message,wp,lp);return 0;
    case WM_CLOSE:hide();return 0;
    case WM_QUERYENDSESSION:commitNote();return TRUE;
    case WM_DESTROY:RemoveClipboardFormatListener(h);for(UINT id:{VISIBLE_TICK,DEADLINE_TICK,CLIP_RETRY,NOTE_SAVE})KillTimer(h,id);UnregisterHotKey(h,1);UnregisterHotKey(h,2);Shell_NotifyIconW(NIM_DELETE,&tray);PlaySoundW(nullptr,nullptr,0);PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,message,wp,lp);
}
#ifdef ISLAND_SELF_TEST
#include "integration_test.h"
#endif
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR args,int){
#ifdef ISLAND_SELF_TEST
    const wchar_t* mutexName=L"Local\\QuietIsland.SelfTest";
#else
    const wchar_t* mutexName=L"Local\\QuietIsland.Native.1";
#endif
    HANDLE mutex=CreateMutexW(nullptr,FALSE,mutexName);if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
    instance=inst;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput graphicsInput;ULONG_PTR graphicsToken=0;if(Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr)!=Gdiplus::Ok){CoUninitialize();CloseHandle(mutex);return 1;}
    store.init();if(!store.ready)notice=L"Папка данных недоступна";else if(!store.notesReadable)notice=L"Файл заметок повреждён. Оригинал сохранён.";
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);taskbarMessage=RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.lpszClassName=CLASS_NAME;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
    // Preview exposes a taskbar window for inspection; normal launches stay in the tray.
    bool preview=wcsstr(args,L"--preview")!=nullptr;
#ifdef ISLAND_PREVIEW
    preview=true;
#endif
    win=CreateWindowExW((preview?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW)|WS_EX_TOPMOST|WS_EX_CONTROLPARENT,CLASS_NAME,L"Island",WS_POPUP|WS_CLIPCHILDREN,0,0,460,338,nullptr,nullptr,inst,nullptr);
    if(!win){Gdiplus::GdiplusShutdown(graphicsToken);CoUninitialize();CloseHandle(mutex);return 1;}
    clipboardSequence=GetClipboardSequenceNumber();
#ifndef ISLAND_SELF_TEST
    if(!AddClipboardFormatListener(win))notice=L"Не удалось подключиться к буферу Windows";
    bool openKey=RegisterHotKey(win,1,MOD_CONTROL|MOD_NOREPEAT,VK_PRIOR),closeKey=RegisterHotKey(win,2,MOD_CONTROL|MOD_NOREPEAT,VK_NEXT);
    if(!openKey||!closeKey){
        MessageBoxW(nullptr,L"Не удалось зарегистрировать Ctrl + PgUp / Ctrl + PgDn. Сочетание занято другой программой. Island можно открыть через значок в трее.",L"Island",MB_OK|MB_ICONWARNING);
    }
#endif
    if(preview)show();
#ifdef ISLAND_SELF_TEST
    runIntegrationTests();
#endif
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
        if(shown&&(msg.hwnd==win||IsChild(win,msg.hwnd))){
            if((msg.message>=WM_KEYFIRST&&msg.message<=WM_KEYLAST)||(msg.message>=WM_LBUTTONDOWN&&msg.message<=WM_MOUSELAST&&msg.message!=WM_MOUSEMOVE))activity();
            if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){hide();continue;}
            if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&msg.hwnd==noteEdit&&(GetKeyState(VK_CONTROL)&0x8000)){if(commitNote())SetFocus(win);continue;}
            if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&(msg.hwnd==minutesBox||msg.hwnd==secondsBox)){toggleTimer();continue;}
            if(!(msg.hwnd==noteEdit&&(msg.message==WM_CHAR||msg.message==WM_KEYDOWN)&&msg.wParam==VK_RETURN)&&IsDialogMessageW(win,&msg))continue;
        }
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    thumbnails.clear();selectedPreview.reset();for(HFONT f:{normalFont,smallFont,largeFont,headingFont,iconFont})if(f)DeleteObject(f);DeleteObject(backgroundBrush);DeleteObject(fieldBrush);Gdiplus::GdiplusShutdown(graphicsToken);CoUninitialize();CloseHandle(mutex);return 0;
}
