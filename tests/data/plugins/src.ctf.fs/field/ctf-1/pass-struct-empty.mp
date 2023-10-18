---
struct {
  u8 a;

  struct {
  } b;

  u8 c;
}
---
01  # `a`
02  # `c`

---
a: 1
b: {}
c: 2
