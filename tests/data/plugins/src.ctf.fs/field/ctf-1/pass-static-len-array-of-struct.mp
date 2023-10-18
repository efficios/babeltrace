---
struct {
  struct {
    u8 a;
    nt_str b;
  } x[3];
} @[2]

---
01              # `a`
"salut\0"       # `b`

02              # `a`
"patente\0"     # `b`

03              # `a`
"Quebec\0"      # `b`

04              # `a`
"chez nous\0"   # `b`

05              # `a`
"aidez-moi\0"   # `b`

06              # `a`
"rasseye\0"     # `b`

---
- x:
  - a: 1
    b: "salut"
  - a: 2
    b: "patente"
  - a: 3
    b: "Quebec"
- x:
  - a: 4
    b: "chez nous"
  - a: 5
    b: "aidez-moi"
  - a: 6
    b: "rasseye"
