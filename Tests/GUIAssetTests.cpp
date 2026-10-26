// Lightweight, self-contained source-asset checks without requiring JUCE or
// libpng. Runtime JUCE rendering still requires full plugin compilation.
#include <algorithm>
#include <array>
#include <iterator>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<unsigned char> read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);
    if (!f) throw std::runtime_error("Missing GUI asset: "+path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
unsigned read32(const std::vector<unsigned char>& v, std::size_t i) {
    return (static_cast<unsigned>(v.at(i))<<24)
        |(static_cast<unsigned>(v.at(i+1))<<16)
        |(static_cast<unsigned>(v.at(i+2))<<8)
        |static_cast<unsigned>(v.at(i+3));
}
void png(const std::string& path,unsigned w,unsigned h) {
    const auto v=read(path);
    constexpr std::array<unsigned char,8> sig{{137,80,78,71,13,10,26,10}};
    if (v.size()<33 || !std::equal(sig.begin(),sig.end(),v.begin())
        || read32(v,16)!=w || read32(v,20)!=h)
        throw std::runtime_error("Wrong PNG dimensions/header: "+path);
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::runtime_error("Expected source asset directory");
        const std::string dir=argv[1];
        png(dir+"/faceplate_master.png",1672,941);
        for (const auto name:{"input","meter","mode","output"})
            png(dir+"/knob_"+name+".png",206,206);
        png(dir+"/preview.png",1254,706);
        const auto jpg=read(dir+"/panel.jpg");
        if (jpg.size()<300000 || jpg[0]!=0xff || jpg[1]!=0xd8
            || jpg[jpg.size()-2]!=0xff || jpg.back()!=0xd9)
            throw std::runtime_error("Background photo asset invalid");
        std::cout<<"GUI asset dimensions / signatures OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what()<<'\n';
        return 1;
    }
}
