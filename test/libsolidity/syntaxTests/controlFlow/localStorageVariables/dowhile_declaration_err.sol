contract C {
    struct S { bool f; }
    S s;
    function f() internal view {
        S storage c;
        do {
            break;
            c = s;
        } while(false);
        c;
    }
    function g() internal view {
        S storage c;
        do {
            if (s.f) {
                continue;
                c = s;
            }
            else {
            }
        } while(false);
        c;
    }
    function h() internal view {
        S storage c;
        do {
            if (s.f) {
                break;
            }
            else {
                c = s;
            }
        } while(false);
        c;
    }
    function i() internal view {
        S storage c;
        do {
            if (s.f) {
                continue;
            }
            else {
                c = s;
            }
        } while(false);
        c;
    }
    function j() internal view {
        S storage c;
        do {
            continue;
            c = s;
        } while(false);
        c;
    }
}
// ----
// TypeError: (184-185): This variable is of storage pointer type and can be accessed without prior assignment, which would lead to undefined behaviour.
//  Warning: (145-150): Unreachable code.
// Warning: (168-173): Unreachable code.
// TypeError: (411-412): This variable is of storage pointer type and can be accessed without prior assignment, which would lead to undefined behaviour.
// Warning: (325-330): Unreachable code.
// TypeError: (635-636): This variable is of storage pointer type and can be accessed without prior assignment, which would lead to undefined behaviour.
// TypeError: (862-863): This variable is of storage pointer type and can be accessed without prior assignment, which would lead to undefined behaviour.
// TypeError: (1011-1012): This variable is of storage pointer type and can be accessed without prior assignment, which would lead to undefined behaviour.
//  Warning: (972-977): Unreachable code.
