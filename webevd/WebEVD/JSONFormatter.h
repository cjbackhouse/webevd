#ifndef WEBEVD_JSONFORMATTER_H
#define WEBEVD_JSONFORMATTER_H

#include <cmath>
#include <ostream>
#include <map>
#include <string>
#include <variant>

namespace evd
{

// ----------------------------------------------------------------------------
class JSONFormatter
{
public:
  JSONFormatter(std::ostream& os) : fStream(os) {}

  template<class T> JSONFormatter& operator<<(const T& x)
  {
    static_assert(std::is_arithmetic_v<T> ||
                  std::is_enum_v<T>);
    fStream << x;
    return *this;
  }

  JSONFormatter& operator<<(const std::string& s)
  {
    fStream << "\"" << Escape(s) << "\"";
    return *this;
  }

  JSONFormatter& operator<<(double x)
  {
    // "NaN" and "Infinity" are the javascript syntax, but JSON doesn't include
    // these concepts at all. Our options are to send a string, a null, or a
    // number that is unrepresentable and will become Infinity again. I don't
    // know any way to play the same trick with a NaN.
    if(isnan(x)) fStream << "1e999";
    else if(isinf(x)) fStream << "1e999";
    else fStream << x;
    return *this;
  }

  JSONFormatter& operator<<(float x){return *this << double(x);}

  JSONFormatter& operator<<(int x)
  {
    fStream << x;
    return *this;
  }

  // No-one should be trying to write un-formatted strings
  JSONFormatter& operator<<(const char* x) = delete;

  // Reify all pointers
  template<class T> JSONFormatter& operator<<(const T* x)
  {
    return *this << *x;
  }

  template<class T> JSONFormatter& operator<<(const std::vector<T>& v)
  {
    fStream << "[";
    for(const T& x: v){
      *this << x;
      if(&x != &v.back()) fStream << ", ";
    }
    fStream << "]";
    return *this;
  }

  template<class T, class U>
  JSONFormatter& operator<<(const std::map<T, U>& m)
  {
    fStream << "{\n";
    unsigned int n = 0;
    for(auto& it: m){
      fStream << "  ";
      if constexpr(std::is_same_v<T, int>){
        // JSON dicts can't be indexed by integer
        *this << std::to_string(it.first);
      }
      else{
        *this << it.first;
      }
      fStream << ": ";
      *this << it.second;
      ++n;
      if(n != m.size()) fStream << ",\n";
    }
    fStream << "\n}";
    return *this;
  }

protected:
  std::string Escape(std::string s)
  {
    // These are dumb O(N^2) loops, but there are no sensible functions for
    // this in the STL, so writing an efficient version would be a big fiddly
    // loop.

    // escape backslashes to double backslashes
    while(s.find("\\") != std::string::npos){
      s.replace(s.find("\\"), 1, "\\\\");
    }

    // escape quote marks
    while(s.find("\"") != std::string::npos) {
      s.replace(s.find("\""), 1, "\\\"");
    }

    return s;
  }

  std::ostream& fStream;
};

// A model of the JSON dictionary type. A map from strings to any (specified)
// type.
template<class... Ts> class Dict: public std::map<std::string, std::variant<Ts...>>
{
public:
  Dict(){}

  // Convenience constructor, alternate keys and values
  template<class U, class... Us> Dict(const std::string& key, const U& val,
                                      const Us&... vals)
    : Dict(vals...)
  {
    (*this)[key] = val;
  }
};

template<class... Ts> JSONFormatter& operator<<(JSONFormatter& json, const std::variant<Ts...>& x)
{
  // Convert the variant to whatever type it actually has and pass it to json
  std::visit([&](auto&& arg){json << arg;}, x);
  return json;
}


template<class... Ts> JSONFormatter& operator<<(JSONFormatter& json, const Dict<Ts...>& dict)
{
  // Print a Dict as if it was the map it inherits from
  return json << std::map<std::string, std::variant<Ts...>>(dict);
}

} // namespace

#endif
