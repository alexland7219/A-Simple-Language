func f(r: float, v: array[10] of int) : float
  return 2;
endfunc

func g(i: int, v: array[10] of float)
  v[i+1] = 4;
endfunc

func h() : int
  return 3.4;
endfunc

func main()
  var a, b: array[10] of int
  var c: array[20] of int
  var r: array[10] of float
  var u : int
  var v : float
  a = b;
  a = r;
  r = a;
  u = f(v, a) + f(3, c);
  v = 2 * f(v, r);
  u = g(v, b);
  v = u;
  a = c;
endfunc
