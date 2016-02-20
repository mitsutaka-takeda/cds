#include <iostream>
#include <thread>
#include <rxcpp/rx.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <regex>

#include "utf8.h"
#include "rx-threadpool.hpp"

namespace  {
    struct file_reader {
        file_reader(boost::interprocess::file_mapping&& f_,
                    boost::uintmax_t s_,
                    std::regex r_)
            : f(std::move(f_)),
              m(f, f.get_mode(), 0, s_),
              r(std::move(r_))
            {}
        std::cregex_iterator
        begin() const {
            return std::cregex_iterator(static_cast<const char*>(m.get_address()),
                                         static_cast<const char*>(m.get_address()) + m.get_size(),
                                         r);
        }

        std::cregex_iterator end() const {
            return std::cregex_iterator();
        }

        boost::interprocess::file_mapping f;
        boost::interprocess::mapped_region m;
        std::regex r;
    };

    template< class CharT, class Traits >
    std::basic_ostream<CharT, Traits>& newline(std::basic_ostream<CharT, Traits>& os) {
        return os.put(os.widen('\n'));
    }
} // namespace

int main(int argc, char * argv[]) {
    if(argc != 3){
        std::cout << "cds PATTERN PATH" << std::endl;
        return 1;
    }

    auto matches = rxcpp::sources::create<boost::filesystem::path>(
        [p = boost::filesystem::path(argv[2])](rxcpp::subscriber<boost::filesystem::path> sub) {
            std::cout << "observable thread: " << std::this_thread::get_id() << newline;
            try{
                std::for_each(boost::filesystem::recursive_directory_iterator(p),
                              boost::filesystem::recursive_directory_iterator(),
                              [&sub](const auto& d){
                                  sub.on_next(d.path());
                              });
            }
            catch(...) {
                std::clog << "Something bad happend." << std::endl;
                sub.on_error(std::current_exception());
                return;
            }
            sub.on_completed();
        })
        .filter([](const auto& p){
                return boost::filesystem::is_regular(p);
            })
        .map([pattern = argv[1]](auto&& p) {
                return std::make_shared<file_reader>(
                    boost::interprocess::file_mapping(p.string().c_str(), boost::interprocess::read_only),
                    boost::filesystem::file_size(p),
                    std::regex(pattern)
                    );
            })
        .filter([](const auto& r){
                // I need to check if the file r contains valid utf-8 characters!
                // I think I have to split the file to list of lines.
                // A line is represented by string_view, a pair of pointer.
                // Then match the regex with the line.
                return utf8::is_valid(static_cast<char const*>(r->m.get_address()), static_cast<char const*>(r->m.get_address()) + r->m.get_size()) && r->begin() != r->end();
            });

    std::promise<void> pr;
    auto f = pr.get_future();
    matches
        .observe_on(rxcpp::operators::observe_on_thread_pool())
        .subscribe([](const auto& p){
                try {
                    std::cout << p->f.get_name() << ":" << p->m.get_size() << newline;
                    for(auto const& m : *p){
                        std::cout << m[0] << newline;
                    }
                } catch(...){
                    std::cerr << "something bad happend!" << std::endl;
                }
            },
            [&pr](){
                pr.set_value();
            }
            );

    f.wait();
    std::cout << "last" << std::endl;
    
    return 0;
}
