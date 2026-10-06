#include "pipeline_kernels.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main() {
    using namespace pipeline_kernels;
    // Exhaustive /16 semantics, both inversion policies, SIMD-unaligned starts.
    std::vector<uint16_t> source(65536+32);
    for(size_t i=0;i<65536;++i) source[i+1]=static_cast<uint16_t>(i);
    for(bool invert:{false,true}) {
        std::vector<uint8_t> actual(65536+34,0xA5), expected=actual;
        grey16_reference(source.data()+1,expected.data()+3,65536,invert);
        if(invert) grey16<true>(source.data()+1,actual.data()+3,65536);
        else grey16<false>(source.data()+1,actual.data()+3,65536);
        assert(actual==expected);
    }
    for(size_t offset=0;offset<8;++offset) for(size_t length=0;length<33;++length) {
        std::vector<uint8_t> actual(64,0xA5), expected=actual;
        grey16_reference(source.data()+offset,expected.data()+offset,length,true);
        grey16<true>(source.data()+offset,actual.data()+offset,length);
        assert(actual==expected);
    }
    // Float values are deliberately within the existing defined uint16 cast
    // domain. Negative, NaN, infinite and out-of-range policies are not changed.
    constexpr size_t count=2048;
    std::vector<uint16_t> src(count+16), history(count+16);
    std::vector<float> gain(count+16), offset(count+16);
    for(size_t i=0;i<src.size();++i) {
        src[i]=static_cast<uint16_t>(i*4051+17);
        history[i]=static_cast<uint16_t>(i*97+65535);
        gain[i]=.125f+static_cast<float>(i%27)/100.f;
        offset[i]=(i%2 && src[i]>=32) ? -.499f : std::nextafter(static_cast<float>(i%19),0.f);
    }
    for(size_t start=0;start<4;++start) for(size_t length:{size_t(0),size_t(1),size_t(3),size_t(4),size_t(5),size_t(19),count}) {
        for(bool smooth:{false,true}) {
            auto actual=history, expected=history;
            for(int frame=0;frame<12;++frame) {
                for(size_t i=start;i<start+length;++i) {
                    // Original scalar expression: target contraction differences
                    // are a FAILED acceptance gate, not silently new semantics.
                    const float corrected=src[i]*gain[i]+offset[i];
                    expected[i]=smooth ? expected[i]*.7f+corrected*.3f : corrected;
                }
                if(smooth) calibrate<true>(src.data()+start,actual.data()+start,gain.data()+start,offset.data()+start,length);
                else calibrate<false>(src.data()+start,actual.data()+start,gain.data()+start,offset.data()+start,length);
                assert(actual==expected);
            }
        }
        auto actual=history, expected=history;
        for(int frame=0;frame<12;++frame) {
            for(size_t i=start;i<start+length;++i) expected[i]=expected[i]*.8f+src[i]*.2f;
            average<true>(src.data()+start,actual.data()+start,length);
            assert(actual==expected);
        }
    }
}
