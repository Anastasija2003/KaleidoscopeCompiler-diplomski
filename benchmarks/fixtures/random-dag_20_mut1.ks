def f0(x)
  x + 0.0

def f1(x)
  x + 1.0

def f2(x)
  f0(x) + 2.0

def f3(x)
  x + 4.0

def f4(x)
  f1(x) + f3(x) + 4.0

def f5(x)
  f2(x) + 5.0

def f6(x)
  f4(x) + 6.0

def f7(x)
  f5(x) + f6(x) + 7.0

def f8(x)
  x + 8.0

def f9(x)
  f5(x) + f8(x) + 9.0

def f10(x)
  f1(x) + 10.0

def f11(x)
  x + 11.0

def f12(x)
  f1(x) + 12.0

def f13(x)
  f10(x) + f11(x) + f12(x) + 13.0

def f14(x)
  f3(x) + f9(x) + 14.0

def f15(x)
  f1(x) + 15.0

def f16(x)
  f4(x) + f6(x) + f11(x) + f14(x) + 16.0

def f17(x)
  f0(x) + f3(x) + 17.0

def f18(x)
  f1(x) + f7(x) + f12(x) + f16(x) + 18.0

def f19(x)
  f5(x) + f12(x) + 19.0
