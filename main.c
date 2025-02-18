#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include <curl/curl.h>

// #define print_progress 0

char server_adress[50] = {0};

struct
{
    unsigned long file_size;
    unsigned char *bitmap;
} file_info = {
    .file_size = 0,
};

struct
{
    unsigned long width;
    long double target_x;

    unsigned long height;
    long double target_y;

    long double range;

    unsigned long max_iterations;

    unsigned long fragment_count;

    unsigned long current_fragment;

    unsigned long total_fragments_recived;

    unsigned long y_start;
    unsigned long y_end;
} render_info = {0};

CURL *handle;
CURLcode res;

struct MemoryStruct
{
    char *memory;
    size_t size;
};

struct MemoryStruct chunk;

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
    {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void parse_render_info()
{
    long *long_buffer_1 = malloc(8);
    long *long_buffer_2 = malloc(8);
    printf("Init buffers\n");

    memcpy(long_buffer_1, chunk.memory, 8);
    render_info.width = *long_buffer_1;
    printf("Chunk contents: \n");
    for (int i = 0; i < 8; i++)
    {
        printf("I %d: %u\n", i, chunk.memory[i]);
    }
    printf("\n");
    printf("Width: %lu\n", render_info.width);

    memcpy(long_buffer_1, chunk.memory + 8, 8);
    render_info.height = *long_buffer_1;
    printf("Height: %lu\n", render_info.width);

    memcpy(long_buffer_1, chunk.memory + 16, 8);
    memcpy(long_buffer_2, chunk.memory + 24, 8);
    render_info.target_x = (long double)*long_buffer_1 / *long_buffer_2;
    printf("Target X: %.10Lf\n", render_info.target_x);

    memcpy(long_buffer_1, chunk.memory + 32, 8);
    memcpy(long_buffer_2, chunk.memory + 40, 8);
    render_info.target_y = (long double)*long_buffer_1 / *long_buffer_2;
    printf("Target Y: %.10Lf\n", render_info.target_y);

    memcpy(long_buffer_1, chunk.memory + 48, 8);
    memcpy(long_buffer_2, chunk.memory + 56, 8);
    render_info.range = (long double)*long_buffer_1 / *long_buffer_2;
    printf("Range: %.10Lf\n", render_info.range);

    memcpy(long_buffer_1, chunk.memory + 64, 8);
    render_info.max_iterations = *long_buffer_1;
    printf("Max Iterations: %lu\n", render_info.max_iterations);

    memcpy(long_buffer_1, chunk.memory + 72, 8);
    render_info.fragment_count = *long_buffer_1;
    printf("Fragment Count: %lu\n", render_info.fragment_count);

    free(long_buffer_1);
    free(long_buffer_2);
}

int send_fragment()
{
    char dest_buffer[50] = {0};
    snprintf(dest_buffer, 50, "%s%s", server_adress, "/fragment");
    curl_easy_setopt(handle, CURLOPT_URL, dest_buffer);

    curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
    // curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
    // curl_easy_setopt(handle, CURLOPT_READDATA, &upload);

    /* size of the POST data */
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, file_info.file_size);

    /* binary data */
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, file_info.bitmap);

    printf("Sending fragment...\n");
    res = curl_easy_perform(handle);

    /* check for errors */
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        exit(1);
    }
    else
    {
        printf("Fragment Sent\n");
        printf("--------------------------------\n");
        return 1;
    }
}

void init()
{
    char buffer[50] = {0};
    snprintf(buffer, 50, "%s%s", server_adress, "/init");

    curl_easy_setopt(handle, CURLOPT_URL, buffer);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);

    printf("Awaiting initial data...\n");
    res = curl_easy_perform(handle);

    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }

    printf("Data recieved\n");

    parse_render_info();

    file_info.file_size = ((double)render_info.width / render_info.fragment_count) * render_info.height + 8;
    printf("File size: %lu\n", file_info.file_size);
}

int next_fragment()
{
    char buffer[50] = {0};
    snprintf(buffer, 50, "%s%s", server_adress, "/fragment");
    curl_easy_setopt(handle, CURLOPT_URL, buffer);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);

    printf("Awaiting next fragment...\n");
    res = curl_easy_perform(handle);

    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }

    unsigned long *long_buffer = malloc(8);
    memcpy(long_buffer, chunk.memory + (80 + render_info.total_fragments_recived * 8), 8);

    if (*long_buffer == 0xffffffffffffffff)
    {
        free(long_buffer);
        return 1;
    }

    render_info.current_fragment = *long_buffer;

    free(long_buffer);

    render_info.total_fragments_recived++;

    printf("Fragment recieved: %lu\n", render_info.current_fragment);

    render_info.y_start = render_info.height * ((double)render_info.current_fragment / render_info.fragment_count);
    render_info.y_end = render_info.height * (((double)render_info.current_fragment + 1) / render_info.fragment_count);

    printf("Y start: %lu\n", render_info.y_start);
    printf("Y end: %lu\n", render_info.y_end);

    return 0;
}

void write_ulong_to_bitmap(long write_pos, unsigned long input)
{
    for (unsigned char i = 0; i < 8; i++)
    {
        file_info.bitmap[write_pos + i * 8] = (input >> (i * 8) & 0xff);
    }

    // TODO Using this loop, make the loop count a parameter to this function, then it can handle any amount of bytes, not just long
    // Then you can replace all write calls which are two bytes or more with this function

    // for (unsigned char i = 0; i < 8; i++)
    // {
    //     file_info.bitmap[write_pos + 8] = (input >> (i * 8) & 0xff);
    // }

    // fputc(value & 0xff, image);
    // fputc(value >> 8 & 0xff, image);
    // fputc(value >> 16 & 0xff, image);
    // fputc(value >> 24 & 0xff, image);
    // fputc(value >> 32 & 0xff, image);
    // fputc(value >> 40 & 0xff, image);
    // fputc(value >> 48 & 0xff, image);
    // fputc(value >> 56 & 0xff, image);
}
void color_pixel_24(unsigned long write_pos, unsigned long iteration)
{
    unsigned char r = 255.0f * ((float)iteration / (float)render_info.max_iterations);
    if (r > 255)
    {
        r = 255;
    }

    unsigned char g = (unsigned long)(255.0f * (float)iteration / ((float)render_info.max_iterations / 10.0f)) % 255;
    if (g > 255)
    {
        g = 255;
    }

    unsigned char b = (unsigned long)(255.0f * (float)iteration / ((float)render_info.max_iterations / 100.0f)) % 255;
    if (b > 255)
    {
        b = 255;
    }

    file_info.bitmap[write_pos] = r;
    file_info.bitmap[write_pos + 1] = g;
    file_info.bitmap[write_pos + 2] = b;
}

void color_pixel_8(unsigned long write_pos, unsigned long iteration)
{
    unsigned char color = ceil(255.0f * ((float)iteration / (float)render_info.max_iterations));

    file_info.bitmap[write_pos] = color;
}

// Multithread this
void escape_time(long double xmin, long double xmax, long double ymin, long double ymax)
{
    for (unsigned long pixel_y = render_info.y_start; pixel_y < render_info.y_end; pixel_y++)
    {
        for (unsigned long pixel_x = 0; pixel_x < render_info.width; pixel_x++)
        {
            const unsigned long pixel_index = ((pixel_y - render_info.y_start) * render_info.width + pixel_x);
            // const double difference = (float)pixel_index / (float)image_size;
            const unsigned long write_pos = 8 + pixel_index;

            long double x0 = xmin + (xmax - xmin) * ((long double)pixel_x / (long double)render_info.width);
            long double y0 = ymin + (ymax - ymin) * ((long double)pixel_y / (long double)render_info.height);
            long double x = 0;
            long double y = 0;

            unsigned long iteration = 0;

            while (x * x + y * y <= 4 && iteration < render_info.max_iterations)
            {
                long double xtemp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = xtemp;
                iteration++;
            }

            color_pixel_8(write_pos, iteration);
        }
    }
}

int main()
{
    printf("Please input the server adress ({protocol}://{ip}:{port})\n");
    fgets(server_adress, sizeof(server_adress), stdin);

    for (int i = 0; i < sizeof(server_adress); i++)
    {
        if (server_adress[i] == '\n')
        {
            server_adress[i] = 0;
            break;
        }
    }

    printf("Server adress: %s\n", server_adress);

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    handle = curl_easy_init();

    /* some servers do not like requests that are made without a user-agent
    field, so we provide one */
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* Do not timeout */
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 0);

    init();

    printf("---------------------\n");

    file_info.bitmap = malloc(file_info.file_size);
    while (1)
    {
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        // curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);

        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 0);

        if (next_fragment())
        {
            printf("Final fragment recieved.\n");
            break;
        }

        write_ulong_to_bitmap(0, render_info.current_fragment);

        clock_t time_render_start = clock();
        printf("Rendering...\n");
        escape_time(render_info.target_x - render_info.range / 2, render_info.target_x + render_info.range / 2, render_info.target_y - render_info.range / 2, render_info.target_y + render_info.range / 2);
        clock_t time_render_end = clock();
        printf("Render complete. Time: %f\n", ((time_render_end - time_render_start) / (float)CLOCKS_PER_SEC));

        send_fragment();

        curl_easy_reset(handle);
    }

    curl_easy_cleanup(handle);

    curl_global_cleanup();
}