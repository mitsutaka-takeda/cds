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
#include "stlab/future.hpp"

namespace  {
    struct file_reader {
        file_reader() = default;
        file_reader(boost::interprocess::file_mapping&& f_,
                    boost::uintmax_t s_,
                    std::shared_ptr<const std::regex> r_
            )
            : f(std::move(f_)),
              m(f, f.get_mode(), 0, s_),
              r(std::move(r_))
            {}
        std::cregex_iterator
        begin() const {
            return std::cregex_iterator(static_cast<const char*>(m.get_address()),
                                        static_cast<const char*>(m.get_address()) + m.get_size(),
                                        *r);
        }

        std::cregex_iterator end() const {
            return std::cregex_iterator();
        }

        boost::interprocess::file_mapping f;
        boost::interprocess::mapped_region m;
        std::shared_ptr<const std::regex> r;
    };

    struct result {
    private:
        file_reader fr;
        std::vector<std::match_results<const char*> > r;
    public:
        using const_iterator = std::vector<std::match_results<const char*> >::const_iterator;
        result() = default;
        result(file_reader&& fr_,
               std::vector<std::match_results<const char*> >&& r_)
            : fr(std::move(fr_)),
              r(std::move(r_)) {
        }

        const_iterator 
        begin() const {
            return r.cbegin();
        }

        const_iterator
        end() const {
            return r.cend();
        }
    };

    template< class CharT, class Traits >
    std::basic_ostream<CharT, Traits>& newline(std::basic_ostream<CharT, Traits>& os) {
        return os.put(os.widen('\n'));
    }
} // namespace

int main(int argc, char * argv[]) try {
    if(argc != 3){
        std::cout << "cds PATTERN PATH" << std::endl;
        return 1;
    }

    auto scheduler = stlab::default_scheduler();

    std::list<stlab::future<result>> tasks;

    auto pattern = std::make_shared<std::regex const>(argv[1]);

    std::for_each(boost::filesystem::recursive_directory_iterator(boost::filesystem::path(argv[2])),
                  boost::filesystem::recursive_directory_iterator(),
                  [&](const auto& d){
                      const auto p = d.path();
                      if(!boost::filesystem::is_regular(p)){
                          return;
                      }

                      tasks.push_back(stlab::async(
                          scheduler,
                          [p = std::move(p), pattern](){
                              file_reader 
                                  fr{boost::interprocess::file_mapping(p.string().c_str(), boost::interprocess::read_only),
                                      boost::filesystem::file_size(p),
                                      pattern};
                              auto const begin = static_cast<char const*>(fr.m.get_address()),
                                  end = static_cast<char const*>(fr.m.get_address()) + fr.m.get_size();
                              if(!utf8::is_valid(begin, end)){
                                  return result{};
                              }
                              else{
                                  auto const crbegin = fr.begin(), crend = fr.end();
                                  return result{std::move(fr), std::vector<std::match_results<const char*> >(crbegin, crend)};
                              }
                          }));
                  });

    while(!tasks.empty()){
        decltype(tasks.begin()->get_try()) r;
        auto ready = std::find_if(tasks.begin(), tasks.end(), [&r](auto& t) { return static_cast<bool>(r = t.get_try()); });
        if(ready == tasks.end()){
            continue;
        }
        for(const auto& m: *r){
            std::cout << m[0] << newline;
        }
        tasks.erase(ready);
    }
    std::cout << "last" << std::endl;
    
    return 0;
} catch(std::future_error& fe){
    std::cerr << fe.what() << std::endl;
}
