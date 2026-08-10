# test.ks - sample Kaleidoscope source for lexer testing

def foo(x y)
  x + y * 2.5

extern sin(x)

def fib(n)
  if n < 2 then
    n
  else
    fib(n - 1) + fib(n - 2)

fib(10)
