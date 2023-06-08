
func read_chars(a: array[10] of char) : int
  var i: int
  i = 0;
  while i < 10 do
    read a[i];
    if a[i] != '.' then
      i = i + 1;
    else
      return i;
    endif
  endwhile
  return 10;
endfunc

func find_vowels(n: int, vc: array[10] of char, vb: array[10] of bool)
  var c: char
  while n > 0 do
    c = vc[n-1];
    vb[n-1] = c=='a' or c=='e' or c=='i' or c=='o' or c=='u';
    n = n-1;
  endwhile
endfunc

func write_consonants(n: int, vc: array[10] of char, vb: array[10] of bool) : float
  var k: float
  var i: int
  i = 0;
  k = 0;
  while i != n do
    if vb[i] then
      k = k + 1;
    else
      write vc[i];
    endif
    i = i + 1;
  endwhile
  write '\n';
  return 100*k/n;
endfunc

func main()
  var a: array[10] of char
  var b: array[10] of bool
  var n: int
  var p: float
  n = read_chars(a);
  find_vowels(n,a,b);
  p = write_consonants(n,a,b);
  write p; write "\n";
endfunc
