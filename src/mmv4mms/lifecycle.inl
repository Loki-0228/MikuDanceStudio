void Log(void*,uint32_t level,const char* message) {
    const std::string text=message?message:"";
    if(level>=2){if(text==lastError)return;lastError=text;}
    runtime_log::Write("MMV4MMS: %s",text.c_str());
}
void Redraw(void*) {
    if(host&&host->MainWindow())PostMessageW(host->MainWindow(),WM_APP+17,0,0);
}
void ReleasePlugin() {
    rendererColor.Reset();
    if(plugin&&api.runtime.destroy)api.runtime.destroy(plugin);
    plugin=nullptr;api={};menuApi={};
    if(library)FreeLibrary(library);
    library=nullptr;
}
void Notify(unsigned event,const wchar_t* path=nullptr,uint64_t object=0) {
    if(plugin&&api.runtime.notify(plugin,event,path,object)!=MMV_OK)
        Log(nullptr,2,api.runtime.status(plugin));
}
