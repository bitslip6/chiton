// Included by the production-emitter fixture. Independent all-pairs oracle:
// no production dispatch/location helper, no early return after a matching pair.
static Result aggressive_oracle(const uint16_t *src,unsigned i,const uint8_t *members,const Config &c) {
    const int center=src[i],x=i%width,y=i/width;
    if(!(members[i]&1)||!center||center<c.evidence_min||center>c.evidence_max)return {Flag::Unclassified,false};
    bool usable=false,gap=false,mixed=false;
    for(int r=1;r<=6;++r)for(int direction=0;direction<4;++direction){
        const int dx=direction==1?0:1,dy=direction==0?0:direction==3?-1:1;
        const int ax=x-r*dx,ay=y-r*dy,bx=x+r*dx,by=y+r*dy;
        if(ax<0||ax>=640||bx<0||bx>=640||ay<0||ay>=480||by<0||by>=480)continue;
        if(!(members[ay*640+ax]&1)||!(members[by*640+bx]&1))continue;
        const int a=src[ay*640+ax],b=src[by*640+bx];
        if(!a||!b||a<c.evidence_min||a>c.evidence_max||b<c.evidence_min||b>c.evidence_max)continue;
        usable=true;const int lo=std::min(a,b),hi=std::max(a,b);
        if(hi-lo>c.boundary_gap){gap=true;mixed|=center-lo>c.center_margin&&hi-center>c.center_margin;}
    }
    return {mixed?Flag::MixedCandidate:gap?Flag::BoundaryRisk:usable?Flag::Retained:Flag::Unclassified,gap};
}
static void aggressive_kernel_tests() {
    Config c=cfg;c.algorithm=Algorithm::AggressiveBoundaryBands;c.center_support=50;
    assert(valid(c));for(auto &v:domain)v=domain_member|domain_interior;
    guarded=true; // Includes first aggressive call, with no lazy setup.
    fill(1500);assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)if(dx||dy)for(int half=0;half<=5;++half){
        for(unsigned i=0;i<pixels;++i){const int t=(int(i%width)-320)*dx+(int(i/width)-240)*dy;
            frame[i]=t < -half?1000:t>half?2000:1500;}
        assert(classify_sample(frame,at(),domain,c).flag==Flag::MixedCandidate);
        Config old=c;old.algorithm=Algorithm::Exact;
        if(half>=2)assert(classify_sample(frame,at(),domain,old).flag!=Flag::MixedCandidate);
    }
    // A broad third surface is not erased at its stable interior; narrow ones are.
    fill(1500);for(unsigned i=0;i<pixels;++i)if(i%width<300)frame[i]=1000;else if(i%width>340)frame[i]=2000;
    assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    // Smooth ramps are intentionally suspect even with coherent local support.
    for(unsigned i=0;i<pixels;++i)frame[i]=1000+(i%width)*4;
    Config ramp=c;ramp.boundary_gap=20;ramp.center_margin=5;
    assert(classify_sample(frame,at(),domain,ramp).flag==Flag::MixedCandidate);
    // Any direction qualifies: largest gap alone would miss this center.
    fill(1500);frame[at()-1]=4000;frame[at()+1]=6000;frame[at()-width]=1000;frame[at()+width]=2000;
    assert(classify_sample(frame,at(),domain,c).flag==Flag::MixedCandidate);
    frame[at()]=500;assert(classify_sample(frame,at(),domain,c).flag!=Flag::MixedCandidate);
    frame[at()]=6200;c.evidence_max=65535;assert(classify_sample(frame,at(),domain,c).flag!=Flag::MixedCandidate);
    // Radius-two interior bit must NEVER authorize radius-six unchecked reads.
    fill(1500);frame[at()-6]=1000;frame[at()+6]=2000;domain[at()+6]=0;
    assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    domain[at()-6]=0;domain[at()+6]=domain_member;assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    domain[at()-6]=domain_member;assert(classify_sample(frame,at(),domain,c).flag==Flag::MixedCandidate);
    // Threshold endpoints are strict; ignored fields cannot secretly rescue.
    c.evidence_min=0;c.evidence_max=65535;c.boundary_gap=1000;c.center_margin=0;
    assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    c.boundary_gap=999;c.center_margin=500;assert(classify_sample(frame,at(),domain,c).flag==Flag::BoundaryRisk);
    c.center_margin=499;assert(classify_sample(frame,at(),domain,c).flag==Flag::MixedCandidate);
    c.anchor_tolerance=65535;c.ramp_residual=65535;c.surface_support=255;c.center_support=0;
    assert(classify_sample(frame,at(),domain,c).flag==Flag::MixedCandidate);
    fill(0);assert(classify_sample(frame,at(),domain,c).flag==Flag::Unclassified);
    fill(65535);assert(classify_sample(frame,at(),domain,c).flag==Flag::Retained);
    // Zero cannot be an anchor even when the configured interval includes zero.
    fill(1500);frame[at()-6]=0;frame[at()+6]=65535;
    assert(classify_sample(frame,at(),domain,c).flag!=Flag::MixedCandidate);
    uint32_t random=827;auto next=[&](){random=random*1664525u+1013904223u;return random;};
    static uint8_t forward[pixels];
    for(unsigned scene=0;scene<16;++scene){
        for(unsigned i=0;i<pixels;++i){frame[i]=uint16_t(next()>>8);domain[i]=(next()%5)?3:0;}
        c.evidence_min=scene%3?0:uint16_t(next());c.evidence_max=scene%3?65535:uint16_t(next());
        c.boundary_gap=scene%2?uint16_t(next()):0;c.center_margin=scene%2?0:uint16_t(next());
        for(unsigned i=0;i<pixels;i+=13){const auto ref=aggressive_oracle(frame,i,domain,c);
            const auto actual=classify_sample(frame,i,domain,c),checked=classify_sample_checked(frame,i,domain,c);
            assert(actual.flag==ref.flag&&actual.gated==ref.gated&&!actual.early_retained);
            assert(actual.flag==checked.flag&&actual.gated==checked.gated);forward[i]=uint8_t(actual.flag);}
        // Reverse traversal gives identical flags: no cascading mask feedback.
        for(int i=int((pixels-1)/13*13);i>=0;i-=13)assert(uint8_t(classify_sample(frame,i,domain,c).flag)==forward[i]);
        for(unsigned x=0;x<width;++x)for(unsigned i:{x,(height-1)*width+x})
            assert(classify_sample(frame,i,domain,c).flag==aggressive_oracle(frame,i,domain,c).flag);
        for(unsigned y=0;y<height;++y)for(unsigned i:{y*width,y*width+width-1})
            assert(classify_sample(frame,i,domain,c).flag==aggressive_oracle(frame,i,domain,c).flag);
    }
    guarded=false;assert(allocation_events==0&&source_copies==0);
}
