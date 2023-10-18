---
struct {
  enum : u8 {
    MEOW,
    MIX,
  } tag;

  variant <tag> {
    u16 MEOW;
    nt_str MIX;
  } var;
} @[2]

---
00                # `tag`
[1995:16]         # `var`

01                # `tag`
"hello there!\0"  # `var`
---
- tag: 0
  var: 1995
- tag: 1
  var: "hello there!"
