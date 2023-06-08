func f(v: array[10] of int)
  var c: array[10] of int
  var i: int

  c = v;

  write "en f. c: ";
  i = 0;
  while i < 10 do
    write c[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';
endfunc


func g(v: array[10] of int)
  var d: array[10] of int
  var i: int
  i = 0;
  while i < 10 do
    d[i] = -1;
    i = i+1;
  endwhile

  v = d;

endfunc


func main()
  var a, b: array[10] of int
  var i, j: int
  i = 0;
  while i < 10 do
    a[i] = i;
    b[i] = 0;
    i = i+1;
  endwhile

  b = a;

  write "despres de b=a. b: ";
  i = 0;
  while i < 10 do
    write b[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';
  write "despres de b=a. a: ";
  i = 0;
  while i < 10 do
    write a[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';

  f(a);

  write "despres de f(a). a: ";
  i = 0;
  while i < 10 do
    write a[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';
  
  g(a);

  write "despres de g(a). a: ";
  i = 0;
  while i < 10 do
    write a[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';  
endfunc
