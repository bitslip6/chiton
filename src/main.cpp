/******************************************************************************
 *
 * @file            main.cpp
 *
 * @brief           Main Chiton application front-end.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 SWYM, LLC and BitSlip6. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <iostream>
#include <string>
#include <unistd.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
#include "app.hpp"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>

#include "system.h"

#define PORT 2222

DemoConf DCONF;
uint16_t *raw_data;
unsigned char *mem2 = NULL;
unsigned long len = 0;

typedef int (*IntCallbackString)(std::vector<std::string>, int);

template <typename String, typename ContainerOut = std::vector<String>>
ContainerOut split_words_space(const bool allowEmpty, const String &str)
{
    return fplus::split_by(fplus::is_whitespace<String>, allowEmpty, str);
}

/*
 parse an HTTP request into a vector of url parts split on / and .
*/
std::vector<std::string> parse_url(char *request) {
    std::string s(request);
    auto lines = fplus::split_lines(false, s);
    auto words = split_words_space(false, lines[0]);
    auto url_parts = fplus::split_words(false, words[1]);
    return url_parts;
}

/**
 * floating point hsv calc for reference
 * hsv to rgb with floats...
 */ 
void HsvToRgb_f(u16 in, u16 max, float rotation, RGB8 *out) {

    //in = min(in, max);
    
    float      hh, p, q, t, ff;
    int        i;

    hh = ((float)in/(float)max)*360.0f;
    hh += rotation;
    while (hh >= 360.0f) {
        hh -= 360.0f;
    }
    hh /= 60.0f;
    i = (int)hh;
    ff = hh - i;
    p = 1.0f * (1.0f - 1.0f);
    q = 1.0f * (1.0f - (1.0f * ff));
    t = 1.0f * (1.0f - (1.0f * (1.0f - ff)));

    switch(i) {
    case 0:
        out->r = 255;
        out->g = t * 255;
        out->b = p * 255;
        break;
    case 1:
        out->r = q * 255;
        out->g = 255;
        out->b = p * 255;
        break;
    case 2:
        out->r = p * 255;
        out->g = 255;
        out->b = t * 255;
        break;

    case 3:
        out->r = p * 255;
        out->g = q * 255;
        out->b = 255;
        break;
    case 4:
        out->r = t * 255;
        out->g = p * 255;
        out->b = 255;
        break;
    case 5:
    default:
        out->r = 255;
        out->g = p * 255;
        out->b = q * 255;
        break;
    }
}

void depth_to_grayscale(uint16_t *value, RGB8 *ptr) {
    uint16_t v = *value >> 4;
    ptr->r = (u8) v;
    ptr->g = ptr->r;
    ptr->b = ptr->r;
}

void depth_to_hsv(uint16_t *value, RGB8 *ptr, bool init) {
    
    uint16_t v = *value;
    if (v == 0) {
        ptr->r=0;
        ptr->g=0;
        ptr->b=0;
    } else {
        HsvToRgb_f(v, 3500, 0.0f, ptr);
    }
    return;

    static RGB8* cache = new RGB8[10000];
    if (init) {
        for (int i=0; i<3500; i++) {
            HsvToRgb_f(i, 3500, GOPTS->color_rotation, &cache[i]);
        }
        return;
    }
    if (value != NULL && *value <= GOPTS->max_cam_value) {
        printf("d:%d ", *value);
        ptr->r = cache[*value].r;
        ptr->g = cache[*value].g;
        ptr->b = cache[*value].b;
    }
}

void write_jpeg(JSAMPLE *image_buffer, unsigned char *mem, unsigned long *mem_size)
{
    //unsigned char *mem = NULL;
    //unsigned long mem_size = 0;

    struct jpeg_compress_struct cinfo;
    /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
    struct jpeg_error_mgr jerr;
    /* More stuff */
    JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
    int row_stride;          /* physical row width in image buffer */

    /* Step 1: allocate and initialize JPEG compression object */

    /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    /* Step 2: specify data destination (eg, a file) */
    /* Note: steps 2 and 3 can be done in either order. */

    /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
    jpeg_mem_dest(&cinfo, &mem2, mem_size);

    /* Step 3: set parameters for compression */

    /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
    cinfo.image_width = 640; /* image width and height, in pixels */
    cinfo.image_height = 480;
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
    /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
    jpeg_set_defaults(&cinfo);
    /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
    jpeg_set_quality(&cinfo, 100, TRUE /* limit to baseline-JPEG values */);

    /* Step 4: Start compressor */

    /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */

    /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
    row_stride = cinfo.image_width * 3; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height)
    {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
        row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /* Step 6: Finish compression */

    jpeg_finish_compress(&cinfo);
    /* After finish_compress, we can close the output file. */

    /* Step 7: release JPEG compression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_compress(&cinfo);

    /* And we're done! */
}

uint16_t *get_data_u16(std::string filename, int *out_sz)
{
    std::ifstream myfile(filename, std::ios::in | std::ios::binary | std::ios::ate);
    size_t sz = myfile.tellg();
    uint16_t *data1 = new uint16_t[sz/2];
    if (sz >= 1 && myfile.is_open())
    {
        myfile.seekg(0, std::ios::beg);
        myfile.read((char *)data1, sz);
        myfile.close();
        *out_sz = sz;
    } else {
        *out_sz = 0;
    }
    return data1;
}

uint8_t *get_data_u8(std::string filename, int *out_sz)
{
    std::ifstream myfile(filename, std::ios::in | std::ios::binary | std::ios::ate);
    size_t sz = myfile.tellg();
    uint8_t *data1 = new uint8_t[sz];
    if (sz >= 1 && myfile.is_open())
    {
        myfile.seekg(0, std::ios::beg);
        myfile.read((char *)data1, sz);
        myfile.close();
        *out_sz = sz;
    } else {
        *out_sz = 0;
    }
    return data1;
}



bool send_headers(std::string raw, int socket) {
    int wrote = write(socket, raw.c_str(), raw.length());
    if (su_cmp(wrote, raw.length()) != 0) {
        printf("unable to send headers\n");
        return false;
    }

    return true;
}



bool send_data(int socket, std::string content_type, unsigned char *data, unsigned int sz) {
    std::string raw = "HTTP/1.1 200 OK\nContent-Type: "+content_type+"\nContent-Length: "
        +std::to_string(sz) + "\n\n";
    send_headers(raw, socket);

    int bufsz = 1500;
    int towrite = len;
    while (towrite > 0) {
        int wrote = write(socket, data+(len-towrite), bufsz);
        if (wrote < 0) {
            printf("unable to send data [%d] [%d] [%s]\n", wrote, errno, strerror(errno));
            break;
        }
        bufsz = min(towrite, bufsz);
        towrite -= wrote;
    }
    return (towrite == 0);
}

int camera_fn(std::vector<std::string> params, int socket)
{
    std::cout << "call camera with: " << params[0] << std::endl;
    if (params[0].compare(std::string("enable")) == 0) {
        GOPTS->enable = true;
        return 200;
    }
    if (params[0].compare(std::string("disable")) == 0) {
        GOPTS->enable = false;
        return 200;
    }
    if (params[0].compare(std::string("frame")) == 0) {
        send_data(socket, std::string("image/jpeg"), mem2, len);
        return 0;
    }
    return 404;
}

int value_fn(std::vector<std::string> params, int socket)
{
    if (params.size() < 4) {
        std::cout << "invalid request, get/value requires 2 additional parameters" << std::endl;
        return 500;
    }
    int x = std::stoi(params[2]);
    int y = std::stoi(params[3]);
    if (x>640||x<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 600" << std::endl;
        return 500;
    }
    if (y>480||y<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 480" << std::endl;
        return 500;
    }


    std::string resp = "{\"value\":"+std::to_string(raw_data[(y*640)+x])+"}";
    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        +std::to_string(resp.length()) + "\n\n" + resp;
    send_headers(raw, socket);
    return 200;
}

int setting_fn(std::vector<std::string> params, int socket)
{
    std::cout << "call setting with: " << params[0] << std::endl;
    return 200;
}

int main2()
{
    int server_fd, new_socket;
    // long bytes_read;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    std::string ok = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nOK!";
    std::string fail = "HTTP/1.1 500 OK\nContent-Type: text/plain\nContent-Length: 12\n\nFAIL";
    std::string unknown = "HTTP/1.1 404 OK\nContent-Type: text/plain\nContent-Length: 12\n\nFAIL";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    memset(address.sin_zero, '\0', sizeof address.sin_zero);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("In bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("In listen");
        exit(EXIT_FAILURE);
    }

    std::map<std::string, IntCallbackString> fnMap;
    std::map<int, std::string> respMap;
    std::map<std::string, std::string> contentMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["setting"] = setting_fn;
    contentMap["ico"] = "image/jpeg";
    contentMap["png"] = "image/png";
    contentMap["jpg"] = "image/jpeg";
    contentMap["jpeg"] = "image/jpeg";
    contentMap["js"] = "text/javascript";
    contentMap["json"] = "application/json";
    contentMap["css"] = "text/css";
    contentMap["bin"] = "application/octet-stream";
    contentMap["csv"] = "text/csv";
    contentMap["gz"] = "application/gzip";
    contentMap["html"] = "text/html";

    
    respMap[500] = fail;
    respMap[200] = ok;
    respMap[404] = unknown;

    while (GOPTS->running)
    {
        // printf("\n+++++++ Waiting for new connection ++++++++\n\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("In accept");
            exit(EXIT_FAILURE);
        }

        int sz;
        int response_code = 500;
        char buffer[30000] = {0};
        read(new_socket, buffer, 30000);
        auto url_parts = parse_url(buffer);

        // no valid url path, serve the homepage...
        if (url_parts.size() < 2) {
            url_parts = parse_url("GET /app.html");
            std::cout << "serve homepage: " << url_parts[0] << url_parts[1] << std::endl;
        }
        // find the function to execute
        auto search = fnMap.find(url_parts[1]);
        if (search != fnMap.end()) {
            response_code = fnMap[url_parts[1]](url_parts, new_socket);
        }
        // no function, try and serve content as a file
        else {
            std::cout << "serve file func" << std::endl;
            response_code = 404;
            // find the content type from the file extension
            auto type = contentMap.find(url_parts[1]);
            if (type != contentMap.end()) {
                std::cout << "serve type" << type->second << std::endl;
                uint8_t *data = get_data_u8(url_parts[0] + "." + url_parts[1], &sz);
                // we have a valid content type and read file bytes
                if (sz >= 1) {
                    send_data(new_socket, type->second, data, sz);
                    delete[] data;
                    response_code = 0;
                }
            }

            if (response_code > 0) {
                auto search2 = respMap.find(response_code);
                if (search2 != respMap.end()) {
                    unsigned int wrote = write(new_socket, respMap[response_code].c_str(), respMap[response_code].length());
                    if (wrote != respMap[response_code].length()) {
                        printf("unable to write full response\n");
                    }
                }
            } else {
                std::cout << "we done in here!" << std::endl;
            }
        }
        shutdown(new_socket, 0);
        close(new_socket);
    }

    printf("server exited\n");
    return 0;
}

unsigned char* convert_to_rgb(uint16_t *data, int len) {
    unsigned char *data2 = new unsigned char[len*3];
    int i=0, offset=0;;
    RGB8 c;

    printf("c to rgb\n");
    while(i++<len) {
       depth_to_hsv(&data[i], &c, false);
       data2[offset++] = c.r;
       data2[offset++] = c.g;
       data2[offset++] = c.b;
    }

    printf("return rgb ptr\n");
    return data2;
}


int main()
{
    //RGB8 tmp;
    //depth_to_hsv(0, &tmp, true);

    SystemInit(320, 240);
    GOPTS->max_cam_value = 3500;
    GOPTS->min_cam_value = 0;

    std::string out_file = "test1.jpeg";
    int raw_len = 0;
    printf("loading raw file\n");
    raw_data = get_data_u16("test.raw", &raw_len);

    raw_len /= 2;
    printf("convert to rgb: %d\n", raw_len);
    JSAMPLE *data = (JSAMPLE *)convert_to_rgb(raw_data, raw_len);

    printf("jpeg data: \n");
    write_jpeg(data, mem2, &len);
    for (int i=0;i<20;i++) { printf(" %x", mem2[i]); } printf("\n");

    std::ofstream os;
    os.open("test2.jpeg", std::ios::out | std::ios::binary);
    os.write((const char*)mem2, len);
    os.close();


    std::cout << "read file, compressed bytes: " << len << std::endl;
    //printf("mem at %x addr: %x len [%d]\n", mem2, &mem2, len);
    //delete[] raw_data;
    //exit(0);

    app::init();

    std::list<std::string> things = {"same old", "same old"};
    if (fplus::all_the_same(things))
    {
        std::cout << "All things being equal." << std::endl;
    }
    else
    {
        std::cout << "Nothing will ever be the same." << std::endl;
    }
    main2();

/*
    while (GOPTS->running)
    {
        app::refresh();
        sleep(51000);
    }
    */
    printf("main loop exit\n");
}
