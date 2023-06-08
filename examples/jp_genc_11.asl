func main()
  var a,b,c : int
  a = 13;
  b = 4;
  c = a%b;
  write c; write '.'; write '\n';
  c = (-a)%b;
  write c; write '.'; write '\n';
  c = a%(-b);
  write c; write '.'; write '\n';
  c = (a+3)%b;
  write c; write '.'; write '\n';
  c = (-a-3)%(-b);
  write c; write '.'; write '\n';
endfunc
