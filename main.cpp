#include <iostream>
#include <thread>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <regex>

#include "utf8.h"
#include "stlab/future.hpp"

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
                              boost::interprocess::file_mapping fm(p.string().c_str(), boost::interprocess::read_only);
                              boost::interprocess::mapped_region m(fm, fm.get_mode(), 0, boost::filesystem::file_size(p));

                              auto const begin = static_cast<char const*>(m.get_address()),
                                  end = static_cast<char const*>(m.get_address()) + m.get_size();
                              if(!utf8::is_valid(begin, end)){
                                  return ""s;
                              }
                              else{
                                  auto const file_name = p.string();
                                  std::string buf{file_name};
                                  buf.reserve(100);
                                  auto i = begin;
                                  auto line_number = 1u;
                                  while(i != end){
                                      char const * const next = std::find(i, end, '\n');
                                      if(std::regex_match(i, next, pattern)) {
                                          buf.push_back('\n');
                                          // next can be either end or iterator pointing to '\n'.
                                          buf.append(std::to_string(line_number) + ":");
                                          buf.insert(buf.size(), i, boost::numeric_cast<std::string::size_type>(next - i));
                                      }
                                      i = std::find_if(next, end, [](const auto ch) { return ch != '\n'; });
                                      line_number += (i - next);
                                  }

                                  if(buf == file_name){
                                      return ""s;
                                  } else {
                                      buf.append("\n\n");
                                      return buf;
                                  }
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
