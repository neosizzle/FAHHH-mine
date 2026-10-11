mkdir /tmp/test
mkdir /tmp/test2
echo "asdf" > /tmp/test/bad 
echo "asdfg" > /tmp/test/bad2
printf 'd%.0s' {1..80} > /tmp/test/bad3
cp /lib/x86_64-linux-gnu/libseccomp.so.2  /tmp/test/bad4

# Create a simple C program
cat > /tmp/test/simple.c <<'EOF'
int main(void) {
    return 0;
}
EOF

# Compile a 32-bit executable
gcc -m32 /tmp/test/simple.c -o /tmp/test/bad4

# Compile a 64-bit executable
gcc -m64 /tmp/test/simple.c -o /tmp/test/good-gcc