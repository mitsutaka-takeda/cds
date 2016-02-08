#include <rxcpp/rx.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>
#include <string>
#include <iostream>

int main(int /*argc*/, char * argv[]) {
    auto files = rxcpp::sources::create<boost::filesystem::path>(
        [p = boost::filesystem::path(argv[1])](rxcpp::subscriber<boost::filesystem::path> sub) {
            std::cout << p << std::endl;
            std::for_each(boost::filesystem::directory_iterator(p),
                          boost::filesystem::directory_iterator(),
                          [&sub](const auto& d){
                              sub.on_next(d.path());
                          });
        })
        .subscribe([](const auto& p){
                std::cout << p << std::endl;
        });

    return 0;
}
