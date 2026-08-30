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
  f10(x) + f11(x) + f12(x) + 14.0

def f14(x)
  f3(x) + f9(x) + 15.0

def f15(x)
  f1(x) + 15.0

def f16(x)
  f4(x) + f6(x) + f11(x) + f14(x) + 16.0

def f17(x)
  f0(x) + f3(x) + 18.0

def f18(x)
  f1(x) + f7(x) + f12(x) + f16(x) + 18.0

def f19(x)
  f5(x) + f12(x) + 19.0

def f20(x)
  f7(x) + f17(x) + 20.0

def f21(x)
  f15(x) + 21.0

def f22(x)
  f2(x) + f7(x) + 22.0

def f23(x)
  f5(x) + f9(x) + f22(x) + 23.0

def f24(x)
  f8(x) + f9(x) + f10(x) + f21(x) + 24.0

def f25(x)
  f5(x) + f12(x) + f13(x) + f19(x) + f20(x) + 25.0

def f26(x)
  f9(x) + f20(x) + 26.0

def f27(x)
  f10(x) + f19(x) + 27.0

def f28(x)
  f11(x) + f16(x) + f26(x) + 29.0

def f29(x)
  f14(x) + f25(x) + 29.0

def f30(x)
  f14(x) + f26(x) + 30.0

def f31(x)
  f16(x) + f20(x) + f28(x) + 32.0

def f32(x)
  f12(x) + f17(x) + 32.0

def f33(x)
  f23(x) + f28(x) + f32(x) + 33.0

def f34(x)
  f25(x) + 34.0

def f35(x)
  f18(x) + f24(x) + f26(x) + f29(x) + f31(x) + f33(x) + 36.0

def f36(x)
  f23(x) + f24(x) + f34(x) + 36.0

def f37(x)
  f17(x) + f26(x) + f29(x) + f30(x) + f36(x) + 37.0

def f38(x)
  f20(x) + f24(x) + f34(x) + 38.0

def f39(x)
  f32(x) + f38(x) + 39.0

def f40(x)
  f28(x) + f30(x) + f38(x) + 40.0

def f41(x)
  f32(x) + f34(x) + 41.0

def f42(x)
  f25(x) + f29(x) + f35(x) + 42.0

def f43(x)
  f24(x) + f29(x) + f31(x) + f32(x) + f36(x) + 43.0

def f44(x)
  f37(x) + f38(x) + 44.0

def f45(x)
  f38(x) + f44(x) + 45.0

def f46(x)
  f31(x) + f32(x) + f36(x) + f44(x) + 46.0

def f47(x)
  f34(x) + 47.0

def f48(x)
  f46(x) + 48.0

def f49(x)
  f34(x) + f38(x) + 49.0

def f50(x)
  f45(x) + 50.0

def f51(x)
  f38(x) + f47(x) + 51.0

def f52(x)
  x + 52.0

def f53(x)
  f34(x) + f35(x) + f41(x) + f47(x) + f51(x) + 53.0

def f54(x)
  f37(x) + f42(x) + f46(x) + 54.0

def f55(x)
  f53(x) + 55.0

def f56(x)
  f55(x) + 56.0

def f57(x)
  f40(x) + f52(x) + f54(x) + 57.0

def f58(x)
  f44(x) + f52(x) + f54(x) + 58.0

def f59(x)
  f40(x) + f45(x) + f49(x) + f51(x) + 59.0

def f60(x)
  f41(x) + f45(x) + f50(x) + f57(x) + 60.0

def f61(x)
  f44(x) + f47(x) + f49(x) + f55(x) + f57(x) + f59(x) + f60(x) + 61.0

def f62(x)
  f45(x) + f46(x) + f51(x) + f58(x) + f61(x) + 62.0

def f63(x)
  f48(x) + f54(x) + f55(x) + f57(x) + 63.0

def f64(x)
  f47(x) + f59(x) + 64.0

def f65(x)
  f46(x) + f48(x) + f49(x) + f56(x) + 65.0

def f66(x)
  f50(x) + f61(x) + 66.0

def f67(x)
  f50(x) + f54(x) + f56(x) + f65(x) + f66(x) + 67.0

def f68(x)
  f57(x) + f63(x) + f66(x) + 68.0

def f69(x)
  f50(x) + f53(x) + f61(x) + f63(x) + f64(x) + 69.0

def f70(x)
  f52(x) + f53(x) + f59(x) + 70.0

def f71(x)
  f51(x) + f59(x) + 71.0

def f72(x)
  f58(x) + f62(x) + f66(x) + f68(x) + 72.0

def f73(x)
  f64(x) + f65(x) + 73.0

def f74(x)
  f64(x) + f65(x) + 74.0

def f75(x)
  f55(x) + 75.0

def f76(x)
  f61(x) + f62(x) + f64(x) + f68(x) + f71(x) + 76.0

def f77(x)
  x + 77.0

def f78(x)
  f63(x) + f68(x) + f75(x) + 78.0

def f79(x)
  f62(x) + f63(x) + f68(x) + f73(x) + f77(x) + 79.0

def f80(x)
  f61(x) + f62(x) + f68(x) + f71(x) + f75(x) + f78(x) + 80.0

def f81(x)
  f62(x) + f68(x) + f70(x) + f80(x) + 82.0

def f82(x)
  f69(x) + f78(x) + 82.0

def f83(x)
  f63(x) + f65(x) + f77(x) + 83.0

def f84(x)
  f71(x) + f76(x) + f83(x) + 84.0

def f85(x)
  f71(x) + f77(x) + f80(x) + f81(x) + 85.0

def f86(x)
  f77(x) + 87.0

def f87(x)
  f70(x) + f75(x) + f80(x) + 87.0

def f88(x)
  f70(x) + f72(x) + f74(x) + f80(x) + f87(x) + 88.0

def f89(x)
  f81(x) + f82(x) + 89.0

def f90(x)
  f75(x) + f88(x) + f89(x) + 90.0

def f91(x)
  f85(x) + f86(x) + 91.0

def f92(x)
  f77(x) + f84(x) + 92.0

def f93(x)
  f80(x) + f86(x) + 93.0

def f94(x)
  f83(x) + f89(x) + 95.0

def f95(x)
  f80(x) + f81(x) + 95.0

def f96(x)
  f87(x) + 96.0

def f97(x)
  f80(x) + f82(x) + f84(x) + f85(x) + f94(x) + f95(x) + 97.0

def f98(x)
  x + 98.0

def f99(x)
  f86(x) + 99.0
