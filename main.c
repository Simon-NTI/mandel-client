#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include <curl/curl.h>

#define print_progress 0

struct
{
    unsigned char total_header_size;
    unsigned char bits_per_pixel;
    unsigned char padding;
    unsigned long total_padding;
    unsigned long image_size;
    unsigned long file_size;

    unsigned char *bitmap;
} file_info = {
    .total_header_size = 54,
    .bits_per_pixel = 0,
    .padding = 0,
    .total_padding = 0,
    .image_size = 0,
    .file_size = 0};

struct
{
    unsigned long width;
    const long double target_x;
    const long double range_x;

    unsigned long height;
    const long double target_y;
    const long double range_y;

    unsigned long max_iterations;

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
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void calc_render_info()
{
    file_info.padding = 4 - (((file_info.bits_per_pixel / 8) * render_info.width) % 4);
    file_info.total_padding = file_info.padding * render_info.height;
    file_info.image_size = (file_info.bits_per_pixel / 8) * (render_info.width * render_info.height) + file_info.total_padding;
    file_info.file_size = file_info.total_header_size + file_info.image_size + file_info.total_padding;
}

void send_fragment()
{
    curl_easy_setopt(handle, CURLOPT_URL, "http://localhost:3031/fragment");
    // /* Now specify we want to POST data */
    // curl_easy_setopt(handle, CURLOPT_POST, 1L);
    // curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
    // curl_easy_setopt(handle, CURLOPT_READDATA, &upload);

    // /* Set the expected upload size. */
    // curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)upload.sizeleft);

    /* Send */

    FILE *test_ = fopen("data.bin", "wb");

    for (int i = 0; i < 50; i++)
    {
        fputc(97 + i, test_);
    }

    /* size of the POST data */
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, file_info.file_size);
    /* binary data */
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, file_info.bitmap);
    printf("Sending data...\n");
    res = curl_easy_perform(handle);

    /* check for errors */
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }
    else
    {
        printf("Success\n");
    }
}

void begin_compute()
{
    curl_easy_setopt(handle, CURLOPT_URL, "http://localhost:3031/begin");

    /* send all data to this function  */
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);

    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 0);

    printf("Awaiting response...\n");
    res = curl_easy_perform(handle);

    /* check for errors */
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }
    else
    {
        printf("Success\n");
    }
}

void write_ulong_to_bitmap(long write_pos, unsigned long input)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            bitmap[write_pos + i] |= ((input >> (i * 8)) & (1 << j));
        }
    }
}

void color_pixel(unsigned long write_pos, unsigned long iteration)
{
    unsigned char r = 255.0f * ((float)iteration / (float)max_iteration);
    if (r > 255)
    {
        r = 255;
    }

    unsigned char g = (unsigned long)(255.0f * (float)iteration / ((float)max_iteration / 10.0f)) % 255;
    if (g > 255)
    {
        g = 255;
    }

    unsigned char b = (unsigned long)(255.0f * (float)iteration / ((float)max_iteration / 100.0f)) % 255;
    if (b > 255)
    {
        b = 255;
    }

    bitmap[write_pos] = r;
    bitmap[write_pos + 1] = g;
    bitmap[write_pos + 2] = b;
}

void unoptimized_naive_escape_time(long double xmin, long double xmax, long double ymin, long double ymax)
{
    for (unsigned long pixel_y = 0; pixel_y < height; pixel_y++)
    {
        for (unsigned long pixel_x = 0; pixel_x < width; pixel_x++)
        {
            const unsigned long pixel_index = (pixel_y * width + pixel_x);
            // const double difference = (float)pixel_index / (float)image_size;
            const unsigned long write_pos = total_header_size + pixel_index * 3;

            long double x0 = xmin + (xmax - xmin) * ((double)pixel_x / (double)width);
            long double y0 = ymin + (ymax - ymin) * ((double)pixel_y / (double)height);
            long double x = 0;
            long double y = 0;

            unsigned long iteration = 0;
            // printf("Plotting pixel %lu at (%f, %f)\n", pixel_index, x0, y0);

            while (x * x + y * y <= 4 && iteration < max_iteration)
            {
                long double xtemp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = xtemp;
                iteration++;
            }

            color_pixel(write_pos, iteration);
        }

#ifdef print_progress

        if (pixel_y % (unsigned long)((float)height / 100.0f) == 0)
        {
            printf("%d%%\n", (unsigned int)((float)pixel_y / ((float)height / 100.0f)) + 1);
        }
#endif
    }
}

void optimized_naive_escape_time(long double xmin, long double xmax, long double ymin, long double ymax)
{
    for (unsigned long pixel_y = 0; pixel_y < height; pixel_y++)
    {
        for (unsigned long pixel_x = 0; pixel_x < width; pixel_x++)
        {
            const unsigned long pixel_index = (pixel_y * width + pixel_x);
            // const double difference = (float)pixel_index / (float)image_size;
            const unsigned long write_pos = total_header_size + pixel_index * 3;

            long double x0 = xmin + (xmax - xmin) * ((double)pixel_x / (double)width);
            long double x = 0;
            long double x2 = 0;

            long double y0 = ymin + (ymax - ymin) * ((double)pixel_y / (double)height);
            long double y = 0;
            long double y2 = 0;

            unsigned long iteration = 0;
            // printf("Plotting pixel %lu at (%f, %f)\n", pixel_index, x0, y0);

            while (x2 + y2 <= 4 && iteration < max_iteration)
            {
                y = 2 * x * y + y0;
                x = x2 - y2 + x0;
                x2 = x * x;
                y2 = y * y;
                iteration++;
            }

            color_pixel(write_pos, iteration);
        }

#ifdef print_progress
        // BUG, this line of code may result in pixel_y % 0 when height < 100
        if (pixel_y % (unsigned long)((float)height / 100.0f) == 0)
        {
            printf("%u%%\n", (unsigned int)((float)pixel_y / ((float)height / 100.0f)) + 1);
        }
#endif
    }
}

int main()
{
    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    handle = curl_easy_init();

    /* some servers do not like requests that are made without a user-agent
    field, so we provide one */
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // Debugging
    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);

    // clang-format off
    /* identifier */
    bitmap[0] = 0x42;
    bitmap[1] = 0x4D;

    /* File size in bytes */
    write_ulong_to_bitmap(2, file_size);
    
    // bitmap[6] = 66;
    // bitmap[7] = 0;
    // bitmap[8] = 0;
    // bitmap[9] = 0;

    /* Reserved field */
    bitmap[6] = 0;
    bitmap[7] = 0;
    bitmap[8] = 0;
    bitmap[9] = 0;

    /* Offset to image data, bytes */
    // sprintf(bitmap + 10, "%ld", total_header_size);
    bitmap[10] = 54;
    bitmap[11] = 0;
    bitmap[12] = 0;
    bitmap[13] = 0;

    /* Header size in bytes */
    bitmap[14] = 40;
    bitmap[15] = 0;
    bitmap[16] = 0;
    bitmap[17] = 0;

    /* Width of image */
    write_ulong_to_bitmap(18, width);

    /* Height of image */
    write_ulong_to_bitmap(22, height);
    
    /* Number of colour planes */
    bitmap[26] = 1;
    bitmap[27] = 0;

    /* Bits per pixel */
    bitmap[28] = 24;
    bitmap[29] = 0;

    /* Compression type */
    bitmap[30] = 0;
    bitmap[31] = 0;
    bitmap[32] = 0;
    bitmap[33] = 0;

    /* Image size in bytes */
    write_ulong_to_bitmap(34, image_size);

    /* Horizontal pixels per meter */
    bitmap[38] = 0;
    bitmap[39] = 0;
    bitmap[40] = 0;
    bitmap[41] = 0;

    /* Horizontal pixels per meter */
    bitmap[42] = 0;
    bitmap[43] = 0;
    bitmap[44] = 0;
    bitmap[45] = 0;

    /* Number of colours */
    bitmap[46] = 0;
    bitmap[47] = 0;
    bitmap[48] = 0;
    bitmap[49] = 0;

    /* Important colours */
    bitmap[50] = 0;
    bitmap[51] = 0;
    bitmap[52] = 0;
    bitmap[53] = 0;

    // clang-format on

    // {
    //     printf("Painting bitmap...\n");
    //     float difference = (float)image_size / 255.0f;
    //     // printf("%f\n", difference);

    //     for (long y = 0; y < height; y++)
    //     {
    //         for (long x = 0; x < width; x++)
    //         {
    //             unsigned long pixel_index = y * width + x;
    //             unsigned long write_pos = total_header_size + pixel_index * 3 + y * padding;
    //             unsigned char color = pixel_index % 2;

    //             bitmap[write_pos] = 255 * color;
    //             bitmap[write_pos + 1] = 255 * color;
    //             bitmap[write_pos + 2] = 255 * color;
    //         }
    //     }
    // }

    begin_compute();

    clock_t time_render_start = clock();
    printf("Rendering...\n");
    unoptimized_naive_escape_time(target_x - range / 2, target_x + range / 2, target_y - range / 2, target_y + range / 2);
    // optimized_naive_escape_time(target_x - range / 2, target_x + range / 2, target_y - range / 2, target_y + range / 2);
    clock_t time_render_end = clock();
    printf("Render complete. Time: %f\n", ((time_render_end - time_render_start) / (float)CLOCKS_PER_SEC));

    FILE *f_image = fopen("image.bmp", "wb");

    // printf("--- Full Bitmap info ---\n");

    clock_t time_write_start = clock();
    printf("Writing to file...");
    for (int i = 0; i < file_size; i++)
    {
        fputc(bitmap[i], f_image);
        // printf("Value at byte %d: %u\n", i, bitmap[i]);
    }
    clock_t time_write_end = clock();
    printf("Write complete. Time: %f\n", ((time_write_end - time_write_start) / (float)CLOCKS_PER_SEC));

    fclose(f_image);

    send_fragment();

    /* cleanup curl stuff */
    curl_easy_cleanup(handle);

    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
}