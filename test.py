def int2long(line):
  return line.replace("int","long")

if __name__ == "__main__":

  targets = ["static struct demangle_component *d_identifier (struct d_info *, int);", "d_identifier (struct d_info *di, int len)", "if (len >= (int) ANONYMOUS_NAMESPACE_PREFIX_LEN + 2"]

  with open("libiberty/cp-demangle.c", "r") as file:
    lines = file.readlines()
  for i in range(0, len(lines)):
    for t in targets:   
      if t in lines[i]:
        print("line number: %d" % (i+1))
        lines[i] = int2long(lines[i])
  
  with open("libiberty/cp-demangle.c", "w+") as file:
    file.writelines(lines)
