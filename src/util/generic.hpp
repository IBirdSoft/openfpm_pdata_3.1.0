#ifndef OPENFPM_PDATA_UTILS_HPP
#define OPENFPM_PDATA_UTILS_HPP

#include <iostream>

static bool amIMaster(Vcluster<>& v_cl) { return v_cl.getProcessUnitID() == 0; }

static void printMe(Vcluster<>& v_cl) {
  std::cout << "VCL #" << v_cl.getProcessUnitID() << " ";
}

template <typename T>
bool isIn(const openfpm::vector<T>& v, const T& x) {
  for (auto i = 0; i < v.size(); i++) {
    if (v.get(i) == x) {
      return true;
    }
  }

  return false;
}

static void print(const std::string& msg) { std::cout << msg; }

template <class T>
void printLn(const T& stuff) {
  std::cout << stuff << std::endl;
}

template <class T>
void _printVar(const std::string& varName, const T& var, Vcluster<>& v_cl) {
  printMe(v_cl);
  print(varName + " ~> ");
  printLn(var);
}

// use preprocessor # (stringify)
#define printVar(name) _printVar(#name, (name), create_vcluster())

#endif  // OPENFPM_PDATA_UTILS_HPP
