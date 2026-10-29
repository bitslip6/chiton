// SDK-free diagnostic: the production TU is included to inject private run fixtures
// without adding instrumentation/setters to the live API. No camera/app linkage.
#include "../src/graphics/calibration.cpp"
#include "json.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <vector>

GlobalOpts options{};
GlobalOpts *GOPTS = &options;
constexpr unsigned pixels = 640 * 480;
constexpr unsigned components = pixels * 3; // Actual production allocation, NOT full-density XYZD.
static std::vector<s16> cloud(components + 16, 12345);
static std::vector<u16> slam_storage(SLAM_POINTS * 101);
static unsigned cloud_size = 0;
namespace data {
    s16 *get_point_cloud() { return cloud.data(); }
    u16 *get_slam() { return slam_storage.data(); }
    void set_point_cloud_size(int n) { assert(n >= 0); cloud_size = n; }
}
namespace utils { void log_fn(int, const char *, const char *, int, const char *, ...) {} }

static void cleanup() {
    using namespace calibration;
    // Every allocation made by make_calibration_data; keep leak checking enabled.
    free(pixel_dist); pixel_dist = nullptr;
    free(pixel_gain); pixel_gain = nullptr;
    free(pixel_offset); pixel_offset = nullptr;
    free(angle_sin_vert_cos); angle_sin_vert_cos = nullptr;
    free(vert_sin); vert_sin = nullptr;
    free(vert_cos); vert_cos = nullptr;
    free(angle_cos_vert_sin); angle_cos_vert_sin = nullptr;
    free(pixel_angle); pixel_angle = nullptr;
    free(pixel_vert_radians); pixel_vert_radians = nullptr;
    free(pixel_angle_bucket); pixel_angle_bucket = nullptr;
    free(angle_bucket); angle_bucket = nullptr;
    free(pixel_line_bucket); pixel_line_bucket = nullptr;
    free(src_offsets); src_offsets = nullptr;
    free(runlines); runlines = nullptr;
    free(calibration::calibration); calibration::calibration = nullptr;
}

static void geometry(int cx, int cy, int inner, int outer) {
    assert(cx >= 0 && cx < 640 && cy >= 0 && cy < 480);
    assert(inner >= 0 && outer >= inner && outer <= 800);
    options.center_x = cx; options.center_y = cy;
    options.inner_dist = inner; options.outer_dist = outer;
    calibration::make_calibration_data();
}

static std::vector<unsigned> indices() {
    std::vector<unsigned> output;
    std::set<unsigned> seen;
    unsigned previous_end = 0;
    for (unsigned j = 0; j < calibration::get_num_runlines(); ++j) {
        const auto &r = calibration::get_run_lines()[j];
        assert(r.srcstart <= r.runlen && r.runlen <= pixels);
        assert(r.srcstart >= previous_end);
        previous_end = r.runlen;
        for (unsigned i = r.srcstart; i < (r.runlen & ~3u); i += 4) {
            assert(i < pixels && seen.insert(i).second);
            assert(output.size() < components / 4);
            output.push_back(i);
        }
    }
    return output;
}

static void emit(std::vector<u16> &input, const std::vector<unsigned> &order) {
    // Preflight every float->int16 conversion; never execute UB in normal tests.
    for (auto i : order) {
        const float d = input[i];
        assert(d <= 32767);
        for (auto factor : {calibration::get_x_factor()[i], calibration::get_y_factor()[i], calibration::get_z_factor()[i]}) {
            assert(std::isfinite(factor) && std::isfinite(d * factor));
            assert(std::trunc(d * factor) >= -32768 && std::trunc(d * factor) <= 32767);
        }
    }
    std::fill(cloud.begin(), cloud.end(), 12345);
    cloud_size = 0;
    std::vector<uint32_t> source_indices(order.size()+8,0xfedcba98);
    pointcloud_capture::Recorder recorder{source_indices.data(),order.size(),0,true};
    pointcloud_capture::recorder=&recorder;
    calibration::vector_point_cloud(input.data());
    pointcloud_capture::recorder=nullptr;
    assert(recorder.valid&&recorder.count==order.size());
    for(size_t k=0;k<order.size();++k)assert(source_indices[k]==order[k]);
    for(size_t k=order.size();k<source_indices.size();++k)assert(source_indices[k]==0xfedcba98);
    assert(cloud_size == order.size() * 4 && cloud_size <= components);
    for (unsigned i = cloud_size; i < cloud.size(); ++i) assert(cloud[i] == 12345);
    for (unsigned k = 0; k < order.size(); ++k) {
        unsigned i = order[k];
        assert(cloud[k*4+0] == static_cast<s16>(input[i] * calibration::get_x_factor()[i]));
        assert(cloud[k*4+1] == static_cast<s16>(input[i] * calibration::get_y_factor()[i]));
        assert(cloud[k*4+2] == static_cast<s16>(input[i] * calibration::get_z_factor()[i]));
        assert(cloud[k*4+3] == input[i]);
        const double radius = std::hypot(std::hypot(cloud[k*4], cloud[k*4+1]), cloud[k*4+2]);
        assert(std::abs(radius-input[i]) < std::sqrt(3.) + .02);
    }
}

static int segment(unsigned r) {
    if (r >= 261 && r <= 285) return 8;
    if (r >= 240 && r <= 261) return 7;
    if (r >= 223 && r <= 240) return 6;
    if (r >= 204 && r <= 223) return 5;
    if (r >= 188 && r <= 204) return 4;
    if (r >= 173 && r <= 188) return 3;
    if (r >= 161 && r <= 173) return 2;
    if (r >= 151 && r <= 161) return 1;
    if (r >= 137 && r <= 151) return 0;
    return -1;
}

static void check_mapping() {
    auto order = indices();
    if (order.empty()) throw std::runtime_error("geometry selects no emitted records; cannot analyze an empty source selection");
    unsigned unsupported = 0, admitted = 0, admitted_in_runs = 0;
    std::vector<bool> in_runs(pixels, false);
    for (unsigned j=0; j<calibration::get_num_runlines(); ++j)
        for (unsigned i=calibration::runlines[j].srcstart; i<calibration::runlines[j].runlen; ++i) in_runs[i]=true;
    for (unsigned i=0; i<pixels; ++i) {
        const int x=i%640, y=i/640;
        const int r=core::calc_distance(options.center_x,options.center_y,x,y);
        const float a=-gfx::pixel_to_radians({options.center_x,options.center_y},{x,y});
        const bool intended=!(a>-.795f && a<.795f) && r>=options.inner_dist && r<=options.outer_dist;
        if (in_runs[i]) assert(intended);
        admitted += intended;
        admitted_in_runs += in_runs[i];
    }
    for (auto i : order) {
        const int x = i % 640, y = i / 640;
        const int r = core::calc_distance(options.center_x, options.center_y, x, y);
        const float a = -gfx::pixel_to_radians({options.center_x, options.center_y}, {x, y});
        assert(!(a > -.795f && a < .795f));
        assert(r >= options.inner_dist && r <= options.outer_dist);
        const float fx = calibration::get_x_factor()[i], fy = calibration::get_y_factor()[i], fz = calibration::get_z_factor()[i];
        assert(std::isfinite(fx) && std::isfinite(fy) && std::isfinite(fz));
        assert(std::abs(std::hypot(std::hypot(fx, fy), fz)-1) < 1e-6);
        if (r < 137 || r > 285) {
            ++unsupported;
            assert(fy == 0 && calibration::get_vert_angle()[i] == 0);
        }
    }
    std::vector<u16> input(pixels);
    for (u16 depth : {0, 1, 800, 6000, 32767}) {
        std::fill(input.begin(), input.end(), depth);
        emit(input, order);
    }
    // Ramp and sharp edge preserve production source selection/order.
    for (unsigned i=0; i<pixels; ++i) input[i] = i%640 < 320 ? 800 : 1200 + i%3000;
    emit(input, order);
    // Independent declared plane: n=(0,0,-1), n dot XYZ = 1000.
    // Generate ray/plane intersections on safe forward directions, not a flat-depth wall.
    std::fill(input.begin(), input.end(), 0);
    unsigned plane_count = 0;
    for (auto i : order) if (-calibration::get_z_factor()[i] > .3f) {
        input[i] = static_cast<u16>(1000.0 / -calibration::get_z_factor()[i]);
        ++plane_count;
    }
    emit(input, order);
    for (unsigned k=0; k<order.size(); ++k) if (input[order[k]]) assert(std::abs(cloud[k*4+2]+1000) <= 2);
    // Real EMA on constant frames: show warm-up, no cloud accumulation.
    std::vector<u16> averaged(pixels, 0);
    std::fill(input.begin(), input.end(), 800);
    options.smoothing = true;
    for (int f=0; f<8; ++f) {
        calibration::average_frame(input.data(), averaged.data());
        assert(averaged[order[0]] > 0 && averaged[order[0]] < 800);
        emit(averaged, order);
    }
    options.smoothing = false;
    calibration::average_frame(input.data(), averaged.data());
    assert(averaged == input);
    emit(averaged, order);
    std::cout << "mapping " << options.center_x << ',' << options.center_y << ' ' << options.inner_dist << ".." << options.outer_dist
              << " records=" << order.size() << " unsupported=" << unsupported << " plane=" << plane_count
              << " admitted=" << admitted << " run_covered=" << admitted_in_runs
              << " omitted_by_run_builder=" << admitted-admitted_in_runs << '\n';
}

static void lookup_boundaries() {
    geometry(320,240,130,295);
    // Exact-radius asymmetric rays and both sides of each knot; reference is a
    // declared piecewise table, not an assertion about the physical optical model.
    const int knots[] = {137,151,161,173,188,204,223,240,261,285};
    const double degrees[] = {44.8,38.964,31.995,26.565,20.621,14.175,7.633,0,-7.633,-15};
    for (int knot : knots) for (int delta : {-1,0,1}) {
        const int radius=knot+delta;
        const unsigned i=240*640+320+radius;
        double expected=0;
        if (radius>=137 && radius<=285) {
            int seg=0;
            while (seg<8 && radius>knots[seg+1]) ++seg;
            expected=degrees[seg]+(degrees[seg+1]-degrees[seg])*(radius-knots[seg])/(knots[seg+1]-knots[seg]);
        }
        assert(calibration::get_pixel_dist()[i]==radius);
        assert(std::abs(calibration::get_vert_angle()[i]-expected*.0174533)<1e-6);
    }
    // Branch selection uses truncated integer radius; interpolation uses float
    // radius, so just beyond 285 still extrapolates, but radius 286 collapses.
    const unsigned slightly_beyond=241*640+605;
    assert(calibration::get_pixel_dist()[slightly_beyond]==285);
    assert(calibration::get_vert_angle()[slightly_beyond] < -15*.0174533f);
    assert(calibration::get_vert_angle()[240*640+606]==0);
}

static void asymmetric_orientation() {
    geometry(320,240,137,285);
    // Independently declared source directions in the existing API basis. These
    // expectations do not call production angle/factor helpers: image-right maps
    // to negative API X; image-down maps to negative API Z. Elevation is positive
    // at radius 200 and negative at radius 260 under the current lookup table.
    struct Expected { unsigned x, y; double horizontal_x, horizontal_z, elevation; };
    const Expected cases[] = {
        {480,120,-.8, .6, 15.7865},
        {200,400, .6,-.8, 15.7865},
        {440,400,-.6,-.8, 15.7865},
        {476,448,-.6,-.8,-7.26952380952381},
    };
    auto lines=calibration::get_run_lines();
    for (unsigned j=0;j<calibration::get_num_runlines();++j) lines[j].srcstart=lines[j].runlen=pixels;
    std::vector<unsigned> expected_order;
    std::vector<std::array<double,3>> expected_xyz;
    for (unsigned k=0;k<4;++k) {
        const auto &c=cases[k];
        const unsigned i=c.y*640+c.x;
        const double elevation=c.elevation*.0174533;
        const std::array<double,3> factor={c.horizontal_x*std::cos(elevation),std::sin(elevation),c.horizontal_z*std::cos(elevation)};
        assert(std::abs(calibration::get_x_factor()[i]-factor[0])<1e-6);
        assert(std::abs(calibration::get_y_factor()[i]-factor[1])<1e-6);
        assert(std::abs(calibration::get_z_factor()[i]-factor[2])<1e-6);
        expected_xyz.push_back({800*factor[0],800*factor[1],800*factor[2]});
        lines[k].srcstart=i; lines[k].runlen=i+4;
        expected_order.push_back(i);
    }
    assert(indices()==expected_order);
    std::vector<u16> input(pixels,800);
    emit(input,expected_order);
    for (unsigned k=0;k<4;++k) for (unsigned axis=0;axis<3;++axis) {
        // Quantization tolerance permits truncation, but neither an axis swap nor
        // a sign inversion; each independently expected component is nonzero.
        assert(std::abs(cloud[k*4+axis]-expected_xyz[k][axis])<1.001);
        assert((cloud[k*4+axis]>0)==(expected_xyz[k][axis]>0));
    }
}

static void sampling_edges() {
    // Synthetic runs test the active scalar emitter (including offset zero/final pixel).
    auto lines = calibration::get_run_lines();
    auto count = calibration::get_num_runlines();
    for (unsigned j=0; j<count; ++j) lines[j].srcstart = lines[j].runlen = pixels;
    const unsigned begins[] = {0, 9, 17, 639, pixels-1};
    const unsigned ends[] = {8, 13, 19, 645, pixels};
    for (unsigned j=0; j<5; ++j) { lines[j].srcstart=begins[j]; lines[j].runlen=ends[j]; }
    const std::vector<unsigned> expected = {0, 4, 9, 639, 643, pixels-1};
    assert(indices() == expected);
    for (auto i : expected) {
        calibration::get_x_factor()[i] = .3f;
        calibration::get_y_factor()[i] = -.4f;
        calibration::get_z_factor()[i] = std::sqrt(.75f);
    }
    std::vector<u16> input(pixels);
    input[pixels-1] = 800; // Isolated final source sample survives when run is explicit.
    emit(input, expected);
    assert(cloud[cloud_size-1] == 800 && cloud[0] == 0);
}

static void sidecar(const char *path) {
    // Refuse overwrite; this is test-only synthetic evidence, not live pixel metadata.
    const std::string plot_path=std::string(path)+".svg";
    const std::string summary_path=std::string(path)+".json";
    for (const std::string &p : {std::string(path),plot_path,summary_path}) {
        std::ifstream exists(p); if (exists.good()) throw std::runtime_error("sidecar output already exists");
    }
    std::ofstream out(path); if (!out) throw std::runtime_error("cannot open sidecar");
    auto order = indices();
    std::vector<u16> input(pixels, 800);
    emit(input, order);
    out << "# SYNTHETIC constant-800 production mapping; NOT a correspondence to the supplied OBJ\n";
    out << "# geometry " << options.center_x << ',' << options.center_y << ',' << options.inner_dist << ',' << options.outer_dist << "\n";
    out << "record,offset,pixel_x,pixel_y,radius_integer,radius_float,lookup_segment,factor_x,factor_y,factor_z,factor_norm,elevation_radians,input_depth,float_x,float_y,float_z,X,Y,Z,D\n";
    out << std::setprecision(9);
    for (unsigned k=0; k<order.size(); ++k) {
        auto i=order[k]; int x=i%640, y=i/640;
        auto r=calibration::get_pixel_dist()[i];
        float fx=calibration::get_x_factor()[i], fy=calibration::get_y_factor()[i], fz=calibration::get_z_factor()[i];
        out << k << ',' << i << ',' << x << ',' << y << ',' << r << ','
            << core::calc_distance_float(options.center_x,options.center_y,x,y) << ',' << segment(r) << ','
            << fx << ',' << fy << ',' << fz << ',' << std::hypot(std::hypot(fx,fy),fz) << ','
            << calibration::get_vert_angle()[i] << ",800," << 800*fx << ',' << 800*fy << ',' << 800*fz;
        for (int c=0;c<4;++c) out << ',' << cloud[k*4+c];
        out << '\n';
    }
    if (!out) throw std::runtime_error("sidecar write failure");
    std::ofstream plot(plot_path);
    plot << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1320\" height=\"600\" viewBox=\"0 0 1320 600\">"
         << "<rect width=\"1320\" height=\"600\" fill=\"#101820\"/>"
         << "<g fill=\"white\" font-family=\"sans-serif\" font-size=\"14\">"
         << "<text x=\"15\" y=\"25\">SYNTHETIC constant-800 production mapping; NOT a source correspondence to the supplied OBJ.</text>"
         << "<text x=\"15\" y=\"50\">All " << order.size() << " emitted samples. Blue: radius below 137; green: 137..285; red: above 285.</text>"
         << "<text x=\"15\" y=\"75\">Source pixels (image Y down), geometry " << options.center_x << ',' << options.center_y << ',' << options.inner_dist << ',' << options.outer_dist << "</text>"
         << "<text x=\"690\" y=\"75\">Float image radius (horizontal) / model elevation -20..50 degrees (vertical)</text></g>";
    unsigned below=0, above=0, exact_horizon=0, quantized=0, zero_y=0;
    for (unsigned k=0;k<order.size();++k) {
        const auto i=order[k]; const auto r=calibration::get_pixel_dist()[i];
        const char *color=r<137 ? "#59acff" : r>285 ? "#ff654f" : "#64c993";
        below+=r<137; above+=r>285;
        const float angle=calibration::get_vert_angle()[i];
        const bool yz=cloud[k*4+1]==0;
        zero_y+=yz;
        exact_horizon+=yz && r>=137 && r<=285 && angle==0;
        quantized+=yz && r>=137 && r<=285 && angle!=0;
        const double rf=core::calc_distance_float(options.center_x,options.center_y,i%640,i/640);
        const double px=690+(rf-options.inner_dist)/std::max(1,int(options.outer_dist-options.inner_dist)+1)*600;
        const double py=110+(50-angle/.0174533)/70*450;
        plot << "<circle cx=\"" << 15+i%640 << "\" cy=\"" << 100+i/640 << "\" r=\".65\" fill=\"" << color << "\"/>"
             << "<circle cx=\"" << px << "\" cy=\"" << py << "\" r=\".65\" fill=\"" << color << "\"/>";
    }
    plot << "</svg>\n";
    nlohmann::json summary={
        {"source","synthetic_constant_800_not_supplied_capture"}, {"record_count",order.size()},
        {"center_x",options.center_x},{"center_y",options.center_y},{"inner_dist",options.inner_dist},{"outer_dist",options.outer_dist},
        {"below_lookup_domain",below},{"above_lookup_domain",above},{"zero_y_count",zero_y},
        {"zero_y_supported_exact_horizon",exact_horizon},{"zero_y_supported_quantization",quantized}};
    std::ofstream report(summary_path); report << summary.dump(2) << '\n';
    if (!plot || !report) throw std::runtime_error("sidecar plot/report write failure");
}

static void diagnostic_exclusions() {
    using namespace pointcloud_capture;
    geometry(320,240,137,285);
    calibration::num_runlines=2;
    calibration::runlines[0]={0,0,12,0,0};calibration::runlines[1]={640,0,652,0,0};
    for(unsigned i=0;i<pixels;++i){calibration::pixel_gain[i]=1;calibration::pixel_offset[i]=0;}
    std::vector<u16> source(pixels,1000),processed(pixels),reference(pixels);
    std::vector<uint8_t> mask(pixels);
    calibration::calibrate_frame(source.data(),reference.data());
    source[0]=source[1]=0;calibration::pixel_offset[4]=-2000;
    source[8]=65535;calibration::pixel_gain[8]=2;
    source[644]=10;calibration::pixel_offset[644]=-10;
    calibration::pixel_gain[2]=std::numeric_limits<float>::quiet_NaN();
    calibration::pixel_gain[pixels-1]=std::numeric_limits<float>::quiet_NaN(); // outside runs: ignored
    calibration::diagnostic_calibrate_frame(source.data(),processed.data(),mask.data(),true);
    assert(mask[0]==ZeroSensor&&mask[1]==ZeroSensor&&mask[2]==CalibrationNonfinite);
    assert(mask[4]==CalibrationRange&&mask[8]==CalibrationRange&&mask[644]==ZeroProcessed);
    assert(mask[pixels-1]==OutsideRunlines&&processed[pixels-1]==0);
    for(unsigned i=0;i<pixels;++i)if(mask[i]==Valid)assert(processed[i]==reference[i]);
    uint32_t recorded[7];std::fill(recorded,recorded+7,0xfedcba98);
    Recorder trace{recorded,6,0,true,mask.data()};recorder=&trace;
    for(unsigned i:{0u,4u,8u,640u,644u,648u}){
        calibration::angle_sin_vert_cos[i]=1;calibration::vert_sin[i]=0;calibration::angle_cos_vert_sin[i]=0;
    }
    calibration::vector_point_cloud(processed.data());recorder=nullptr;
    assert(trace.valid&&trace.count==2&&cloud_size==8&&recorded[0]==640&&recorded[1]==648&&recorded[6]==0xfedcba98);
    assert(cloud[0]==1000&&cloud[3]==1000&&cloud[4]==1000&&cloud[7]==1000);
    calibration::vector_point_cloud(processed.data());assert(cloud_size==24); // live emitter still unfiltered
    std::fill(source.begin(),source.end(),0);
    calibration::diagnostic_calibrate_frame(source.data(),processed.data(),mask.data(),true);
    recorder=&trace;calibration::vector_point_cloud(processed.data());recorder=nullptr;
    assert(trace.valid&&trace.count==0&&cloud_size==0); // all missing still completes
}

int main(int argc, char **argv) {
    try {
        const char *output = nullptr;
        int cx=314, cy=174, inner=130, outer=295;
        for (int a=1; a<argc; ++a) {
            std::string arg=argv[a];
            if (arg=="--sidecar" && a+1<argc) output=argv[++a];
            else if (arg=="--config" && a+1<argc) {
                std::ifstream f(argv[++a]);
                f.seekg(0,std::ios::end);
                if (!f || f.tellg()>4096) throw std::runtime_error("geometry config missing or exceeds 4096 bytes");
                f.seekg(0,std::ios::beg);
                nlohmann::json cfg; f >> cfg;
                if (!cfg.is_object() || cfg.size()!=4) throw std::runtime_error("use a sanitized four-field geometry config");
                for (auto key : {"center_x", "center_y", "inner_dist", "outer_dist"})
                    if (!cfg.at(key).is_number_integer()) throw std::runtime_error("geometry fields must be integers");
                for (auto key : {"center_x", "center_y", "inner_dist", "outer_dist"})
                    if (cfg.at(key)<0 || cfg.at(key)>800) throw std::runtime_error("out-of-range geometry field");
                cx=cfg.at("center_x"); cy=cfg.at("center_y"); inner=cfg.at("inner_dist"); outer=cfg.at("outer_dist");
                if (cx<0 || cx>=640 || cy<0 || cy>=480 || inner<0 || outer<inner || outer>800)
                    throw std::runtime_error("out-of-range geometry");
            } else throw std::runtime_error("usage: pointcloud_geometry_test [--config sanitized.json] [--sidecar new.csv]");
        }
        asymmetric_orientation();
        lookup_boundaries();
        // Wide model-domain mismatch, supported-only, narrow boundary annuli, shifted center.
        geometry(314,174,130,295); check_mapping();
        geometry(320,240,137,285); check_mapping();
        for (int knot : {137,151,161,173,188,204,223,240,261,285}) {
            geometry(320,240,knot-1,knot+1); check_mapping();
        }
        geometry(639,479,0,800); check_mapping();
        sampling_edges();
        diagnostic_exclusions();
        geometry(cx,cy,inner,outer); check_mapping();
        if (output) sidecar(output);
        // Do not execute known out-of-range float->signed casts: normal tests stay defined.
        std::cout << "depth 32768 and 65535: unrepresentable positive signed D; producer float-to-int16 conversion is outside defined range, not valid geometry\n";
        cleanup();
        std::cout << "pointcloud geometry: PASS (production scalar mapping/emitter, no SDK)\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n'; cleanup(); return 2;
    }
}
