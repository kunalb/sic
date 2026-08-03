int check(int a, int b, int c, int d) {
if (a == b) {
return 1;
}
if ((a == b) && (c < d)) {
return 2;
}
if (!c) {
return 3;
}
while (a != b) {
(++a);
}
do {
(++a);
} while (a == b);
for (
int i = 0; (i == c); (++i)) {
(++b);
}
int t = ((a == b) ? 1 : 0);
int e = (a == b);
if (a) {
return 4;
}
return t;
}
