#include <rxcpp/rx.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <iostream>
#include <regex>

#include "utf8.h"

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
        begin() {
            return std::cregex_iterator(static_cast<const char*>(m.get_address()),
                                         static_cast<const char*>(m.get_address()) + m.get_size(),
                                         r);
        }

        std::cregex_iterator end() {
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

    auto files = rxcpp::sources::create<boost::filesystem::path>(
        [p = boost::filesystem::path(argv[2])](rxcpp::subscriber<boost::filesystem::path> sub) {
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
        .map([pattern = argv[1]](const auto& p) {
                return std::make_shared<file_reader>(
                    boost::interprocess::file_mapping(p.string().c_str(), boost::interprocess::read_only),
                    boost::filesystem::file_size(p),
                    std::regex(pattern)
                    );
            })
        // .flat_map(
        //     [](const auto& r){
        //         // return observable<line and line number>
        //     },
        //     [](const auto& r, const auto& valid_line_and_line_number) {
        //     }
        //     )
        .filter([](const auto& r){
                // I need to check if the file r contains valid utf-8 characters!
                // I think I have to split the file to list of lines.
                // A line is represented by string_view, a pair of pointer.
                // Then match the regex with the line.
                // @todo Don't forget to remove after debugging!
                // @todo Don't forget to remove after debugging!
                return utf8::is_valid(static_cast<char const*>(r->m.get_address()), static_cast<char const*>(r->m.get_address()) + r->m.get_size()) && r->begin() != r->end();
            })
        .subscribe([](const auto& p){
                try {
                    std::cout << p->f.get_name() << ":" << p->m.get_size() << newline;

                    for(auto&& m : *p){
                        std::cout << m[0] << newline;
                    }
                } catch(...){
                    std::cout << "something bad happend!" << std::endl;
                }
        });

    std::cout << "last" << std::endl;
    
    return 0;
}
