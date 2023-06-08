func f1(a: int) : int
  return a+1;
endfunc

func f2(b: int) : int
  return 2*b;
endfunc

func f3(c: int)
  write "hola";
endfunc


func main()
  var a,b,c : int
  var r : float
  var v : array[12] of int
  write a%b/3.5;
  write a%r + v;
  write 3 < d%a;
  c = a%b + b%a;
  c = v%a and a%v;
  f1 = f2;
  f1 = f3;
endfunc
