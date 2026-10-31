#pragma once
#include <filesystem>
#include <vector>
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <gdiplus.h>
#include <shlobj.h>
namespace fs=std::filesystem;
using Gdiplus::Bitmap;
struct Note { uint64_t id; std::wstring text; };
struct Shot { fs::path path; uint64_t hash; };
struct ImagePixels { int width=0,height=0; std::vector<BYTE> bytes; };
inline uint64_t newId(){FILETIME ft;GetSystemTimeAsFileTime(&ft);static uint64_t last=0;uint64_t now=(uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;return last=std::max(now,last+1);}
inline bool bitmapPixels(HBITMAP bitmap,ImagePixels& image){
    BITMAP info{};if(!bitmap||!GetObjectW(bitmap,sizeof(info),&info)||info.bmWidth<=0||info.bmHeight<=0||uint64_t(info.bmWidth)*info.bmHeight>32000000)return false;
    image.width=info.bmWidth;image.height=info.bmHeight;image.bytes.resize(size_t(image.width)*image.height*4);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=image.width;bi.bmiHeader.biHeight=-image.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);bool ok=GetDIBits(dc,bitmap,0,image.height,image.bytes.data(),&bi,DIB_RGB_COLORS)==image.height;DeleteDC(dc);
    if(ok)for(size_t i=3;i<image.bytes.size();i+=4)image.bytes[i]=255;return ok;
}
inline bool atomicBytes(const fs::path& path,const std::vector<BYTE>& bytes){
    fs::path temp=path;temp+=L".tmp";HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(f);CloseHandle(f);
    if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!ok)DeleteFileW(temp.c_str());return ok;
}
inline std::unique_ptr<Bitmap> reducedImage(const fs::path& path,int maxW,int maxH){
    Bitmap source(path.c_str());if(source.GetLastStatus()!=Gdiplus::Ok||!source.GetWidth()||!source.GetHeight())return {};
    double scale=std::min({1.,double(maxW)/source.GetWidth(),double(maxH)/source.GetHeight()});int w=std::max(1,int(source.GetWidth()*scale)),h=std::max(1,int(source.GetHeight()*scale));
    auto result=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);Gdiplus::Graphics g(result.get());g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(&source,0,0,w,h);return result;
}
class Store {
public:
    fs::path root;std::vector<Shot> shots;std::vector<Note> notes;bool ready=false,notesReadable=true;
    bool init(){
        wchar_t overridePath[32768];if(GetEnvironmentVariableW(L"ISLAND_DATA_DIR",overridePath,32768))root=overridePath;
        else {PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return false;root=fs::path(local)/L"Island";CoTaskMemFree(local);}
        std::error_code ec;fs::create_directories(root/L"shots",ec);ready=!ec;if(ready){loadShots();notesReadable=loadNotes();}return ready;
    }
    void loadShots(){
        shots.clear();std::error_code ec;
        for(fs::directory_iterator it(root/L"shots",ec),end;!ec&&it!=end;it.increment(ec)){
            auto name=it->path().filename().wstring();
            if(name.size()!=46||name.substr(0,5)!=L"shot-"||name[25]!=L'-'||name.substr(42)!=L".png")continue;
            if(!std::all_of(name.begin()+5,name.begin()+25,[](wchar_t c){return c>=L'0'&&c<=L'9';})||!std::all_of(name.begin()+26,name.begin()+42,[](wchar_t c){return (c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');}))continue;
            uint64_t hash=wcstoull(name.substr(26,16).c_str(),nullptr,16);shots.push_back({it->path(),hash});
        }
        std::sort(shots.begin(),shots.end(),[](const Shot&a,const Shot&b){return a.path.filename()>b.path.filename();});
        while(shots.size()>5){DeleteFileW(shots.back().path.c_str());shots.pop_back();}
    }
    // 1 = stored, 0 = duplicate, -1 = storage error.
    int addImage(const ImagePixels& pixels){
        if(!ready||pixels.width<=0||pixels.height<=0||pixels.bytes.size()!=size_t(pixels.width)*pixels.height*4)return -1;
        uint64_t hash=1469598103934665603ULL;for(BYTE b:pixels.bytes){hash^=b;hash*=1099511628211ULL;}hash^=uint64_t(pixels.width)<<32|UINT(pixels.height);
        for(auto&s:shots)if(s.hash==hash)return 0;
        wchar_t name[80];swprintf_s(name,L"shot-%020llu-%016llx.png",newId(),hash);fs::path dest=root/L"shots"/name,temp=dest;temp+=L".tmp";
        static const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        Bitmap image(pixels.width,pixels.height,pixels.width*4,PixelFormat32bppRGB,const_cast<BYTE*>(pixels.bytes.data()));
        if(image.Save(temp.c_str(),&png,nullptr)!=Gdiplus::Ok)return -1;
        if(!MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());return -1;}
        shots.insert(shots.begin(),{dest,hash});while(shots.size()>5){DeleteFileW(shots.back().path.c_str());shots.pop_back();}return 1;
    }
    bool removeShot(size_t i){if(i>=shots.size()||!DeleteFileW(shots[i].path.c_str()))return false;shots.erase(shots.begin()+i);return true;}
    bool loadNotes(){
        notes.clear();fs::path path=root/L"notes.dat";std::error_code ec;if(!fs::exists(path,ec))return !ec;
        auto size=fs::file_size(path,ec);if(ec||size>16*1024*1024)return false;
        std::ifstream f(path,std::ios::binary);std::vector<BYTE> bytes((std::istreambuf_iterator<char>(f)),{});if(!f.good()&&!f.eof())return false;
        size_t pos=0;auto read=[&](void* out,size_t n){if(n>bytes.size()-pos)return false;memcpy(out,bytes.data()+pos,n);pos+=n;return true;};
        uint32_t magic=0,count=0;if(!read(&magic,4)||magic!=0x324E5149||!read(&count,4)||count>10000)return false;
        std::vector<Note> parsed;
        for(uint32_t i=0;i<count;++i){uint64_t id;uint32_t length;if(!read(&id,8)||!read(&length,4)||length>500)return false;std::wstring text(length,L'\0');if(!read(text.data(),size_t(length)*sizeof(wchar_t)))return false;parsed.push_back({id,std::move(text)});}
        if(pos!=bytes.size())return false;notes=std::move(parsed);return true;
    }
    bool saveNotes(){
        if(!ready||!notesReadable||notes.size()>10000)return false;
        std::vector<BYTE> bytes;auto append=[&](const void* p,size_t n){const auto*b=static_cast<const BYTE*>(p);bytes.insert(bytes.end(),b,b+n);};
        uint32_t magic=0x324E5149,count=UINT(notes.size());append(&magic,4);append(&count,4);
        for(auto&n:notes){uint32_t length=UINT(n.text.size());if(length>500)return false;append(&n.id,8);append(&length,4);append(n.text.data(),length*sizeof(wchar_t));}return atomicBytes(root/L"notes.dat",bytes);
    }
};
