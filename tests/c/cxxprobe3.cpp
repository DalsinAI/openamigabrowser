#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
int main()
{
    std::printf("CXXPROBE3_START\n");
    std::printf("round(2.5)=%.1f trunc(-1.7)=%.1f\n", std::round(2.5), std::trunc(-1.7));
    std::ostringstream os; os << "iostream " << 42 << ' ' << 3.25;
    std::cout << os.str() << std::endl;
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    std::printf("cwd=%s ec=%d\n", cwd.string().c_str(), ec.value());
    std::printf("exists(DH1:Probe)=%d\n", (int)std::filesystem::exists("DH1:Probe", ec));
    std::printf("to_string=%s\n", std::to_string(123456).c_str());
    std::printf("CXXPROBE3_OK\n");
}
