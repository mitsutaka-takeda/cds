#include <iostream>
#include <thread>
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
                    boost::uintmax_t s_)
            : f(std::move(f_)),
              m(f, f.get_mode(), 0, s_)
            {}
        boost::interprocess::file_mapping f;
        boost::interprocess::mapped_region m;
    };

    struct result {
    private:
        file_reader fr;
        std::string s;
    public:
        using const_iterator = std::vector<std::match_results<const char*> >::const_iterator;
        result() = default;
        result(file_reader&& fr_,
               std::string s_)
            : fr(std::move(fr_)),
              s(std::move(s_)) {
        }

        const std::string& str() const noexcept {
            return s;
        }
    };

    template< class CharT, class Traits >
    std::basic_ostream<CharT, Traits>& newline(std::basic_ostream<CharT, Traits>& os) noexcept {
        // return os.put(os.widen('\n'));
        return os.put('\n');
    }
} // namespace

int main(int argc, char * argv[]) try {
    if(argc != 3){
        std::cout << "cds PATTERN PATH" << std::endl;
        return 1;
    }

    auto scheduler = stlab::default_scheduler();

    std::list<stlab::future<std::string>> tasks;
    using namespace std::literals;
    auto const pattern = std::regex(argv[1], std::regex::optimize|std::regex::nosubs);

    std::for_each(boost::filesystem::recursive_directory_iterator(boost::filesystem::path(argv[2])),
                  boost::filesystem::recursive_directory_iterator(),
                  [&](const auto& d){
                      const auto p = d.path();
                      if(!boost::filesystem::is_regular(p)){
                          return;
                      }

                      tasks.push_back(stlab::async(
                          scheduler,
                          [p = std::move(p), &pattern](){
                              file_reader 
                                  fr{boost::interprocess::file_mapping(p.string().c_str(), boost::interprocess::read_only),
                                      boost::filesystem::file_size(p)};
                              auto const begin = static_cast<char const*>(fr.m.get_address()),
                                  end = static_cast<char const*>(fr.m.get_address()) + fr.m.get_size();
                              if(!utf8::is_valid(begin, end)){
                                  return ""s;
                              }
                              else{
                                  std::string buf;
                                  buf.reserve(100);
                                  auto i = begin;
                                  auto line_number = 1u;
                                  while(i != end){
                                      const auto next = std::find(i, end, '\n');
                                      if(std::regex_match(i, next, pattern)) {
                                          // next can be either end or iterator pointing to '\n'.
                                          buf.append(std::to_string(line_number) + ":");
                                          buf.insert(buf.size(), i, next - i);
                                          buf.push_back('\n');
                                      }
                                      i = std::find_if(next, end, [](const auto ch) { return ch != '\n'; });
                                      ++line_number;
                                  }

                                  return buf;
                              }
                          }));
                  });

    std::ios_base::sync_with_stdio(false);
    std::ostreambuf_iterator<char> os(std::cout);
    while(!tasks.empty()){
        decltype(tasks.begin()->get_try()) r;
        auto ready = std::find_if(tasks.begin(), tasks.end(), [&r](auto& t) { return static_cast<bool>(r = t.get_try()); });
        if(ready == tasks.end()){
            continue;
        }
        std::copy(r->cbegin(), r->cend(), os);
        tasks.erase(ready);
    }
    
    return 0;
} catch(std::future_error& fe){
    std::cerr << fe.what() << std::endl;
} catch(std::exception& e){
   std::cerr << e.what() << std::endl;
}
