#ifndef WEBEVD_JSONFORMATTER_H
#define WEBEVD_JSONFORMATTER_H

#include <cmath>
#include <ostream>
#include <map>
#include <string>

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
    *this << "["
          << v.X() << ", "
          << v.Y() << ", "
          << v.Z() << "]";
    return *this;
  }

protected:
  std::ostream& fStream;
};

} // namespace

#endif
