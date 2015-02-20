#!/usr/bin/env python
import sys


if __name__ == '__main__':
    from sphinx import main, make_main

    if sys.argv[1:2] == ['-M']:
        sys.exit(make_main(sys.argv))
    else:
        sys.exit(main(sys.argv))
