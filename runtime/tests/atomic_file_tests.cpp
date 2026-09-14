#include "platform/atomic_file.h"
#include <chrono>
#include <iostream>
#include <thread>
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(root);
    const auto path=root/"Config.toml";
    const auto read=[&] { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); };
    bool ok=mkw::platform::AtomicWriteText(path,"old")&&read()=="old";
    ok &= mkw::platform::AtomicWriteText(path,"new")&&read()=="new";
    // Fail to open staging without ever truncating the destination.
    fs::create_directory(root/"Config.toml.pending");
    ok &= !mkw::platform::AtomicWriteText(path,"broken")&&read()=="new";
    fs::remove(root/"Config.toml.pending");
    std::thread a([&]{ for(int i=0;i<30;++i) mkw::platform::AtomicWriteText(path,std::string(4096,'a')); });
    std::thread b([&]{ for(int i=0;i<30;++i) mkw::platform::AtomicWriteText(path,std::string(8192,'b')); });
    a.join();b.join();
    const auto result=read();
    ok &= result==std::string(4096,'a')||result==std::string(8192,'b');
    ok &= !fs::exists(root/"Config.toml.pending");
    fs::remove_all(root);
    if(!ok) std::cerr<<"Atomic file preservation failed\n";
    return ok?0:1;
}
