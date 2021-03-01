#ifndef WEBEVD_JSONFORMATTER_H
#define WEBEVD_JSONFORMATTER_H

#include <cmath>
#include <ostream>
#include <map>
#include <string>
#include <variant>

#include "TVector3.h"

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
                  std::is_enum_v<T> ||
                  std::is_same_v<T, std::string>);
    fStream << x;
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

  JSONFormatter& operator<<(const char* x)
  {
    fStream << x;
    return *this;
  }

  template<class T> JSONFormatter& operator<<(const std::vector<T>& v)
  {
    fStream << "[";
    for(const T& x: v){
      (*this) << x;
      if(&x != &v.back()) (*this) << ", ";
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
      (*this) << "  " << it.first << ": " << it.second;
      ++n;
      if(n != m.size()) (*this) << ",\n";
    }
    fStream << "\n}";
    return *this;
  }

  template<class T>
  JSONFormatter& operator<<(const std::map<std::string, T>& m)
  {
    fStream << "{\n";
    unsigned int n = 0;
    for(auto& it: m){
      (*this) << "  \"" << it.first << "\": " << it.second;
      ++n;
      if(n != m.size()) (*this) << ",\n";
    }
    fStream << "\n}";
    return *this;
  }

  template<class T>
  JSONFormatter& operator<<(const std::map<int, T>& m)
  {
    fStream << "{\n";
    unsigned int n = 0;
    for(auto& it: m){
      (*this) << "  \"" << it.first << "\": " << it.second;
      ++n;
      if(n != m.size()) (*this) << ",\n";
    }
    fStream << "\n}";
    return *this;
  }

  JSONFormatter& operator<<(const TVector3& v)
  {
    return *this << std::vector<double>{v.X(), v.Y(), v.Z()};
  }

protected:
  std::ostream& fStream;
};

// A model of the JSON dictionary type. A map from strings to any (specified)
// type.
template<class... Ts> class Dict: public std::map<std::string, std::variant<Ts...>>
{
public:
  // Convenience constructor, alternate keys and values
  template<class U, class... Us> Dict(const std::string& key, const U& val,
                                      const Us&... vals)
    : Dict(vals...)
  {
    if constexpr (std::is_same_v<U, std::string>){
      // TODO this is a real hack
      (*this)[key] = "\""+val+"\"";
    }
    else{
      (*this)[key] = val;
    }
  }

protected:
  Dict(){}
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
