
func one(): float
  return +1;
endfunc

func sort(v: array[20] of float)
  var i,j,jmin: int
  var aux: float
  i = 0;
  while i < 20-1 do
    jmin = i;
    j = i+1;
    while j < 20 do
      if v[j] < v[jmin] then
        jmin = j;
      endif
      j = j + 1;
    endwhile
    if jmin != i then
      aux = v[i];
      v[i] = v[jmin];
      v[jmin] = aux;
    endif
    i = i + 1;
  endwhile
endfunc

func evenPositivesAndSort(v: array[20] of float)
  var i: int
  i = 0;
  while i < 20 do
    if v[i] > 0 then
      v[i] = one();
    endif
    i = i + 1;
  endwhile
  sort(v);
endfunc

func main()
  var af: array[20] of float
  var i: int
  i = 0;
  while i < 20 do
    read af[i];
    i = i + 1;
  endwhile
  evenPositivesAndSort(af);
  i = 0;
  while i < 20 do
    if af[i] != one() then
      write af[i]; write ' ';
      i = i + 1;
    else
      write '\n';
      return;
    endif
  endwhile
  write '\n';
endfunc
