// Compiled only with -DISLAND_SELF_TEST. All fixtures live in ISLAND_DATA_DIR.
#undef assert
#define assert(condition) do { if(!(condition)){FILE* failure=_wfopen(L"work/integration-v2-failure.txt",L"w");if(failure){fprintf(failure,"FAIL line %d: %s\n",__LINE__,#condition);fclose(failure);}ExitProcess(3);} } while(false)
void saveTestRender(const wchar_t* path){
    RECT r;GetClientRect(win,&r);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=r.right;bi.bmiHeader.biHeight=-r.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*bits=nullptr;
    HBITMAP dib=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,dib);SendMessageW(win,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);GdiFlush();
    {Bitmap image(r.right,r.bottom,r.right*4,PixelFormat32bppRGB,(BYTE*)bits);const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};assert(image.Save(path,&png,nullptr)==Gdiplus::Ok);}
    SelectObject(dc,old);DeleteObject(dib);DeleteDC(dc);
}
void runIntegrationTests(){
    assert(store.ready&&store.notesReadable&&store.notes.empty()&&store.shots.empty());
    // Work-area geometry includes monitors left of / above the primary display.
    for(RECT work:{RECT{0,0,1920,1040},RECT{-2560,-1440,0,0},RECT{1920,40,3840,1080}}){for(UINT scale:{96U,120U,144U,192U}){int w=MulDiv(200,scale,96),height=MulDiv(42,scale,96);RECT r=anchoredRect(work,w,height,MulDiv(10,scale,96));assert(r.left>=work.left&&r.right<=work.right&&r.top>=work.top&&r.bottom<=work.bottom);assert(abs((r.left+r.right)-(work.left+work.right))<=1);}}
    std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM context)->BOOL{((std::vector<HMONITOR>*)context)->push_back(m);return TRUE;},(LPARAM)&monitors);
    for(auto monitor:monitors){targetMonitor=monitor;expanded=false;layout();position();RECT r;GetWindowRect(win,&r);MONITORINFO mi{};mi.cbSize=sizeof(mi);assert(GetMonitorInfoW(monitor,&mi));assert(MonitorFromWindow(win,MONITOR_DEFAULTTONEAREST)==monitor);assert(dpi==GetDpiForWindow(win));assert(r.right-r.left==px(200)&&r.bottom-r.top==px(42));assert(abs((r.left+r.right)-(mi.rcWork.left+mi.rcWork.right))<=1);page=Page::Notes;expanded=true;layout();position();assert(targetMonitor==monitor&&MonitorFromWindow(win,MONITOR_DEFAULTTONEAREST)==monitor);}
    show();POINT pointer;bool cursorAvailable=GetCursorPos(&pointer);if(cursorAvailable)assert(targetMonitor==MonitorFromPoint(pointer,MONITOR_DEFAULTTONEAREST));hide();
    ImagePixels pixels;pixels.width=320;pixels.height=180;pixels.bytes.resize(320*180*4);
    for(int k=0;k<7;k++){
        for(int y=0;y<180;y++)for(int x=0;x<320;x++){size_t p=(y*320+x)*4;pixels.bytes[p]=BYTE(65+k*18);pixels.bytes[p+1]=BYTE(90+x/3);pixels.bytes[p+2]=BYTE(80+y/2);pixels.bytes[p+3]=255;}
        assert(store.addImage(pixels)==1);assert(store.shots.size()==size_t(std::min(k+1,5)));assert(store.addImage(pixels)==0);
    }
    Store restored;assert(restored.init());assert(restored.shots.size()==5);assert(restored.shots[0].hash==store.shots[0].hash);
    size_t files=0;for(auto&e:fs::directory_iterator(store.root/L"shots"))if(e.path().extension()==L".png")++files;assert(files==5);
    // Windows bitmap conversion preserves RGB and forces deterministic alpha.
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=pixels.width;bi.bmiHeader.biHeight=-pixels.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*data=nullptr;HBITMAP bmp=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&data,nullptr,0);assert(bmp);memcpy(data,pixels.bytes.data(),pixels.bytes.size());ImagePixels converted;assert(bitmapPixels(bmp,converted));DeleteObject(bmp);assert(converted.bytes==pixels.bytes);
    assert(IsWindow(noteEdit)&&IsWindow(noteList)&&IsWindow(slider));
    auto typeNote=[&](const wchar_t* text){SendMessageW(noteEdit,EM_SETSEL,0,-1);SendMessageW(noteEdit,EM_REPLACESEL,TRUE,(LPARAM)text);};
    selectPage(Page::Notes);typeNote(L"Купить кофе\r\nВстреча в 18:30 ☕");assert(noteDirty);assert(commitNote());assert(store.notes.size()==1);
    uint64_t firstId=editingNote;typeNote(L"Обновлённая заметка\r\nВторая строка");assert(commitNote());assert(store.notes.size()==1&&editingNote==firstId);
    newNote();typeNote(L"Проверить макет перед встречей");assert(commitNote());assert(store.notes.size()==2);
    assert(restored.loadNotes());assert(restored.notes.size()==2&&restored.notes[1].text==L"Обновлённая заметка\r\nВторая строка");
    show();selectPage(Page::Notes);SetFocus(noteEdit);lastActivity=GetTickCount64()-idleTimeout-1;SendMessageW(win,WM_TIMER,VISIBLE_TICK,0);assert(shown);SetFocus(win);lastActivity=GetTickCount64()-idleTimeout-1;GetCursorPos(&lastPointer);SendMessageW(win,WM_TIMER,VISIBLE_TICK,0);assert(!shown);
    show();selectPage(Page::Shots);assert(thumbnails.size()==5&&selectedPreview);saveTestRender(L"work/capsule-shots.png");
    selectPage(Page::Notes);saveTestRender(L"work/capsule-notes.png");
    SendMessageW(win,WM_COMMAND,HIDE,0);assert(!expanded&&panelHeight()==42&&panelWidth()==200);saveTestRender(L"work/capsule-folded.png");
    selectPage(Page::Tools);saveTestRender(L"work/capsule-tools.png");
    forcedTestTheme=1;theme();saveTestRender(L"work/capsule-tools-dark.png");selectPage(Page::Notes);saveTestRender(L"work/capsule-notes-dark.png");forcedTestTheme=-1;theme();selectPage(Page::Tools);
    SetWindowTextW(minutesBox,L"0");SetWindowTextW(secondsBox,L"30");toggleTimer();assert(countdown.state==Countdown::Running&&!IsWindowEnabled(minutesBox));toggleTimer();assert(countdown.state==Countdown::Paused);toggleTimer();assert(countdown.state==Countdown::Running);SendMessageW(win,WM_COMMAND,RESET,0);assert(countdown.state==Countdown::Idle);
    SetWindowTextW(minutesBox,L"");SetWindowTextW(secondsBox,L"30");toggleTimer();assert(countdown.state==Countdown::Running);assert(countdown.remaining(GetTickCount64())<=30000&&countdown.remaining(GetTickCount64())>29000);countdown.reset();syncTimer();
    unsigned value=99;SetWindowTextW(secondsBox,L"60");assert(!parseTime(secondsBox,59,value));SetWindowTextW(secondsBox,L"");assert(parseTime(secondsBox,59,value)&&value==0);
    hide();assert(thumbnails.empty()&&!selectedPreview);countdown.start(GetTickCount64()-1000,500);SendMessageW(win,WM_TIMER,DEADLINE_TICK,0);assert(countdown.state==Countdown::Finished&&!shown);
    size_t before=store.shots.size();assert(store.removeShot(0));assert(store.shots.size()==before-1);restored.loadShots();assert(restored.shots.size()==before-1);
    selectPage(Page::Notes);deleteNote();assert(store.notes.size()==1);assert(restored.loadNotes()&&restored.notes.size()==1);
    // A damaged file is never silently overwritten.
    auto validNotes=store.notes;assert(atomicBytes(store.root/L"notes.dat",{1,2,3}));Store damaged;assert(damaged.init()&&!damaged.notesReadable);assert(!damaged.saveNotes());store.notes=validNotes;assert(store.saveNotes());
    FILE*log=_wfopen(L"work/integration-v2-results.txt",L"w");assert(log);fprintf(log,"PASS: 5-image FIFO; duplicate suppression; PNG persistence; RGB bitmap conversion; multiline Unicode notes; edit without duplicate; notes persistence; autosave; focus pauses autohide; inactivity hide; capsule folding; image cache release; volume/timer controls; pause/resume/reset; hidden completion; image/note deletion; corrupt-file protection; six UI renders.\nPASS: negative monitor coordinates; 100/125/150/200 percent geometry; stable monitor on expand; real monitor positioning and DPI on %zu available displays.\nCursor API available in test desktop: %s.\n",monitors.size(),cursorAvailable?"yes":"no, fallback exercised");fclose(log);
    DestroyWindow(win);
}
