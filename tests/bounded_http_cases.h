#pragma once
#include "bounded_http.h"
#include <cassert>

// Also invoked on first call and repeatedly under cleanup_delivery's allocator /
// free guard. No allocating fixtures, stdio, or result serialization here.
inline void bounded_http_number_cases() {
    struct Case { uint64_t n; const char *text; };
    const Case cases[]={{0,"0"},{1,"1"},{9,"9"},{10,"10"},{99,"99"},
        {100,"100"},{255,"255"},{UINT32_MAX,"4294967295"},
        {9007199254740991ULL,"9007199254740991"},
        {9007199254740992ULL,"9007199254740992"},
        {UINT64_MAX,"18446744073709551615"},{123456789,"123456789"}};
    auto check=[](uint64_t n, bounded_http::View expected, size_t remaining) {
        struct Fixture { char before[16]; bounded_http::Buffer b; char after[16]; } f;
        std::memset(f.before,'<',sizeof(f.before));
        std::memset(f.b.bytes,'.',sizeof(f.b.bytes));
        std::memset(f.after,'>',sizeof(f.after));
        const size_t start=sizeof(f.b.bytes)-remaining;
        f.b.size=start;
        f.b.number(n);
        const bool fits=expected.size()<=remaining;
        assert(f.b.valid==fits);
        assert(f.b.size==start+(fits?expected.size():0));
        for(size_t i=0;i<sizeof(f.b.bytes);++i) {
            const char want=fits&&i>=start&&i<start+expected.size()?expected[i-start]:'.';
            assert(f.b.bytes[i]==want); // prefix, partial-write and extra-NUL checks
        }
        if(!fits) {
            f.b.number(0); f.b.number(UINT64_MAX); f.b.add("x");
            assert(!f.b.valid&&f.b.size==start);
            for(char c:f.b.bytes) assert(c=='.');
        }
        for(char c:f.before) assert(c=='<');
        for(char c:f.after) assert(c=='>');
    };
    for(const auto &c:cases) {
        bounded_http::View text(c.text);
        check(c.n,text,4096); check(c.n,text,text.size());
        check(c.n,text,text.size()-1); check(c.n,text,0);
    }
    // Every representable power of ten and its immediate neighbors. Expected
    // digits are constructed from position, not by the formatter's division loop.
    uint64_t power=10;
    for(size_t exponent=1;exponent<=19;++exponent) {
        char below[20],at[20],above[20];
        for(size_t i=0;i<exponent;++i) below[i]='9';
        at[0]=above[0]='1';
        for(size_t i=1;i<=exponent;++i) at[i]=above[i]='0';
        above[exponent]='1';
        check(power-1,{below,exponent},32);
        check(power,{at,exponent+1},32);
        check(power+1,{above,exponent+1},32);
        if(exponent<19) power*=10;
    }
    bounded_http::Buffer b;
    b.add("prefix:"); b.number(UINT64_MAX); b.add("!");
    assert(b.valid&&bounded_http::View(b.bytes,b.size)=="prefix:18446744073709551615!");
}
