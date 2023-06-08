func f1() : int
  return 278;
endfunc

func g(a:bool)
  var b:int
  b = f1() or b > 4;
  b = (f2() or a) > 4;
endfunc
