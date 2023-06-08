func f(i : bool, b:array[10] of int) : int
  var a : int
  write (a + b);
  a = a > 1;
  a = f(a < 1, b[3+i]);
  f = g;
  f = f(c, i[3]);
  return 77;
endfunc

func main()
endfunc
